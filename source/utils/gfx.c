/*
 * Based on https://github.com/devkitPro/wut/blob/4933211d7ba86d4dd45c8525fc83747d799ecf31/libraries/libwhb/src/gfx.c
 */
#include "logger.h"
#include <avm/drc.h>
#include <avm/tv.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/memheap.h>
#include <coreinit/savedframe.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/utils.h>
#include <gx2r/buffer.h>
#include <gx2r/mem.h>
#include <proc_ui/procui.h>
#include <string.h>
#include <whb/gfx.h>
#include <whb/log.h>

#define WHB_GFX_COMMAND_BUFFER_POOL_SIZE (0x400000)

static void *sCommandBufferPool = NULL;
static GX2DrcRenderMode sDrcRenderMode;
static void *sDrcScanBuffer        = NULL;
static uint32_t sDrcScanBufferSize = 0;
static GX2SurfaceFormat sDrcSurfaceFormat;
static GX2TVRenderMode sTvRenderMode;
static void *sTvScanBuffer        = NULL;
static uint32_t sTvScanBufferSize = 0;
static GX2SurfaceFormat sTvSurfaceFormat;
static GX2SurfaceFormat sDrcSurfaceFormat;
static GX2ColorBuffer sTvColourBuffer    = {0};
static GX2ColorBuffer sDrcColourBuffer   = {0};
static GX2ContextState *sTvContextState  = NULL;
static GX2ContextState *sDrcContextState = NULL;
static BOOL sGpuTimedOut                 = FALSE;

static void *sGfxHeapForeground = NULL;

static void *AllocMEM2(uint32_t size, uint32_t alignment) {
    void *block;

    if (alignment < 4) {
        alignment = 4;
    }

    block = MEMAllocFromDefaultHeapEx(size, alignment);
    if (!block) {
        OSFatal("AutobootModule: Failed to allocate memory from MEM2");
    }
    return block;
}

static void FreeMEM2(void *block) {
    MEMFreeToDefaultHeap(block);
}

static void *AllocBucket(uint32_t size, uint32_t alignment) {
    void *block;

    if (!sGfxHeapForeground) {
        DEBUG_FUNCTION_LINE_ERR("sGfxHeapForeground was NULL");
        return NULL;
    }

    if (alignment < 4) {
        alignment = 4;
    }

    block = MEMAllocFromExpHeapEx(sGfxHeapForeground, size, alignment);
    if (!block) {
        DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory from bucket");
    }
    return block;
}

static void FreeBucket(void *block) {
    if (!sGfxHeapForeground) {
        DEBUG_FUNCTION_LINE_ERR("sGfxHeapForeground was NULL");
        return;
    }

    MEMFreeToExpHeap(sGfxHeapForeground, block);
}

static void *
GfxGX2RAlloc(GX2RResourceFlags flags, uint32_t size, uint32_t alignment) {
    return AllocMEM2(size, alignment);
}

static void
GfxGX2RFree(GX2RResourceFlags flags, void *block) {
    return FreeMEM2(block);
}

static void
GfxInitTvColourBuffer(GX2ColorBuffer *cb,
                      uint32_t width,
                      uint32_t height,
                      GX2SurfaceFormat format,
                      GX2AAMode aa) {
    memset(cb, 0, sizeof(GX2ColorBuffer));
    cb->surface.use       = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
    cb->surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
    cb->surface.width     = width;
    cb->surface.height    = height;
    cb->surface.depth     = 1;
    cb->surface.mipLevels = 1;
    cb->surface.format    = format;
    cb->surface.aa        = aa;
    cb->surface.tileMode  = GX2_TILE_MODE_DEFAULT;
    cb->viewNumSlices     = 1;
    GX2CalcSurfaceSizeAndAlignment(&cb->surface);
    GX2InitColorBufferRegs(cb);
}

static uint32_t InitMemory() {
    // Allocate TV scan buffer.
    sTvScanBuffer = AllocBucket(sTvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
    if (!sTvScanBuffer) {
        DEBUG_FUNCTION_LINE_INFO("%s: sTvScanBuffer = AllocBucket(0x%X, 0x%X) failed",
                                 __FUNCTION__,
                                 sTvScanBufferSize,
                                 GX2_SCAN_BUFFER_ALIGNMENT);
        goto error;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, sTvScanBuffer, sTvScanBufferSize);
    GX2SetTVBuffer(sTvScanBuffer, sTvScanBufferSize, sTvRenderMode, sTvSurfaceFormat, GX2_BUFFERING_MODE_SINGLE);

    // Allocate TV colour buffer.
    sTvColourBuffer.surface.image = AllocMEM2(sTvColourBuffer.surface.imageSize, sTvColourBuffer.surface.alignment);
    if (!sTvColourBuffer.surface.image) {
        DEBUG_FUNCTION_LINE_INFO("%s: sTvColourBuffer = AllocMEM2(0x%X, 0x%X) failed",
                                 __FUNCTION__,
                                 sTvColourBuffer.surface.imageSize,
                                 sTvColourBuffer.surface.alignment);
        goto error;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, sTvColourBuffer.surface.image, sTvColourBuffer.surface.imageSize);

    // Allocate DRC scan buffer.
    sDrcScanBuffer = AllocBucket(sDrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
    if (!sDrcScanBuffer) {
        DEBUG_FUNCTION_LINE_INFO("%s: sDrcScanBuffer = AllocBucket(0x%X, 0x%X) failed",
                                 __FUNCTION__,
                                 sDrcScanBufferSize,
                                 GX2_SCAN_BUFFER_ALIGNMENT);
        goto error;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, sDrcScanBuffer, sDrcScanBufferSize);
    GX2SetDRCBuffer(sDrcScanBuffer, sDrcScanBufferSize, sDrcRenderMode, sDrcSurfaceFormat, GX2_BUFFERING_MODE_SINGLE);

    // Allocate DRC colour buffer.
    sDrcColourBuffer.surface.image = AllocMEM2(sDrcColourBuffer.surface.imageSize, sDrcColourBuffer.surface.alignment);
    if (!sDrcColourBuffer.surface.image) {
        DEBUG_FUNCTION_LINE_INFO("%s: sDrcColourBuffer = AllocMEM2(0x%X, 0x%X) failed",
                                 __FUNCTION__,
                                 sDrcColourBuffer.surface.imageSize,
                                 sDrcColourBuffer.surface.alignment);
        goto error;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, sDrcColourBuffer.surface.image, sDrcColourBuffer.surface.imageSize);

    return 0;

error:
    return -1;
}

static uint32_t DeinitMemory() {
    if (sTvScanBuffer) {
        FreeBucket(sTvScanBuffer);
        sTvScanBuffer = NULL;
    }

    if (sTvColourBuffer.surface.image) {
        FreeMEM2(sTvColourBuffer.surface.image);
        sTvColourBuffer.surface.image = NULL;
    }

    if (sDrcScanBuffer) {
        FreeBucket(sDrcScanBuffer);
        sDrcScanBuffer = NULL;
    }

    if (sDrcColourBuffer.surface.image) {
        FreeMEM2(sDrcColourBuffer.surface.image);
        sDrcColourBuffer.surface.image = NULL;
    }

    return 0;
}

static BOOL initBucketHeap() {
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
    uint32_t size;
    void *base;

    size = MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
    if (!size) {
        DEBUG_FUNCTION_LINE_WARN("%s: MEMAllocFromFrmHeapEx(heap, 0x%X, 4)", __FUNCTION__, size);
        return FALSE;
    }

    base = MEMAllocFromFrmHeapEx(heap, size, 4);
    if (!base) {
        DEBUG_FUNCTION_LINE_WARN("%s: MEMGetAllocatableSizeForFrmHeapEx == 0", __FUNCTION__);
        return FALSE;
    }

    sGfxHeapForeground = MEMCreateExpHeapEx(base, size, 0);
    if (!sGfxHeapForeground) {
        DEBUG_FUNCTION_LINE_WARN("%s: MEMCreateExpHeapEx(0x%08X, 0x%X, 0)", __FUNCTION__, base, size);
        return FALSE;
    }

    return TRUE;
}

static BOOL deInitBucketHeap() {
    MEMHeapHandle foreground = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);

    if (sGfxHeapForeground) {
        MEMDestroyExpHeap(sGfxHeapForeground);
        sGfxHeapForeground = NULL;
    }

    MEMFreeToFrmHeap(foreground, MEM_FRM_HEAP_FREE_ALL);
    return TRUE;
}

BOOL GfxInit() {
    initBucketHeap();

    uint32_t drcWidth, drcHeight;
    uint32_t tvWidth, tvHeight;
    uint32_t unk;

    sCommandBufferPool = AllocMEM2(WHB_GFX_COMMAND_BUFFER_POOL_SIZE,
                                   GX2_COMMAND_BUFFER_ALIGNMENT);
    if (!sCommandBufferPool) {
        DEBUG_FUNCTION_LINE_INFO("%s: failed to allocate command buffer pool", __FUNCTION__);
        goto error;
    }

    uint32_t initAttribs[] = {
            GX2_INIT_CMD_BUF_BASE, (uintptr_t) sCommandBufferPool,
            GX2_INIT_CMD_BUF_POOL_SIZE, WHB_GFX_COMMAND_BUFFER_POOL_SIZE,
            GX2_INIT_ARGC, 0,
            GX2_INIT_ARGV, 0,
            GX2_INIT_END};
    GX2Init(initAttribs);

    // Clear frame information in saved foreground to avoid screen corruption
    __OSClearSavedFrame(OS_SAVED_FRAME_A, OS_SAVED_FRAME_SCREEN_TV);
    __OSClearSavedFrame(OS_SAVED_FRAME_A, OS_SAVED_FRAME_SCREEN_DRC);
    __OSClearSavedFrame(OS_SAVED_FRAME_B, OS_SAVED_FRAME_SCREEN_TV);
    __OSClearSavedFrame(OS_SAVED_FRAME_B, OS_SAVED_FRAME_SCREEN_DRC);

    // Disable output until we have rendered something
    GX2SetTVEnable(FALSE);
    GX2SetDRCEnable(FALSE);

    sDrcRenderMode    = GX2GetSystemDRCMode();
    sTvSurfaceFormat  = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    sDrcSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;

    switch (GX2GetSystemTVScanMode()) {
        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_480P;
            tvWidth       = 854;
            tvHeight      = 480;
            break;
        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_1080P;
            tvWidth       = 1920;
            tvHeight      = 1080;
            break;
        case GX2_TV_SCAN_MODE_720P:
        default:
            sTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
            tvWidth       = 1280;
            tvHeight      = 720;
            break;
    }

    drcWidth  = 854;
    drcHeight = 480;

    // Setup TV and DRC buffers - they will be allocated in GfxProcCallbackAcquired.
    GX2CalcTVSize(sTvRenderMode, sTvSurfaceFormat, GX2_BUFFERING_MODE_SINGLE, &sTvScanBufferSize, &unk);
    GfxInitTvColourBuffer(&sTvColourBuffer, tvWidth, tvHeight, sTvSurfaceFormat, GX2_AA_MODE1X);

    GX2CalcDRCSize(sDrcRenderMode, sDrcSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &sDrcScanBufferSize, &unk);
    GfxInitTvColourBuffer(&sDrcColourBuffer, drcWidth, drcHeight, sDrcSurfaceFormat, GX2_AA_MODE1X);

    GX2CalcDRCSize(sDrcRenderMode, sDrcSurfaceFormat, GX2_BUFFERING_MODE_SINGLE, &sDrcScanBufferSize, &unk);
    if (InitMemory() != 0) {
        DEBUG_FUNCTION_LINE_INFO("%s: GfxProcCallbackAcquired failed", __FUNCTION__);
        goto error;
    }

    GX2RSetAllocator(&GfxGX2RAlloc, &GfxGX2RFree);

    // Initialise TV context state.
    sTvContextState = AllocMEM2(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
    if (!sTvContextState) {
        DEBUG_FUNCTION_LINE_INFO("%s: failed to allocate sTvContextState", __FUNCTION__);
        goto error;
    }
    GX2SetupContextStateEx(sTvContextState, TRUE);
    GX2SetContextState(sTvContextState);
    GX2SetColorBuffer(&sTvColourBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, (float) sTvColourBuffer.surface.width, (float) sTvColourBuffer.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, (float) sTvColourBuffer.surface.width, (float) sTvColourBuffer.surface.height);
    GX2SetTVScale((float) sTvColourBuffer.surface.width, (float) sTvColourBuffer.surface.height);

    // Initialise DRC context state.
    sDrcContextState = AllocMEM2(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
    if (!sDrcContextState) {
        WHBLogPrintf("%s: failed to allocate sDrcContextState", __FUNCTION__);
        goto error;
    }
    GX2SetupContextStateEx(sDrcContextState, TRUE);
    GX2SetContextState(sDrcContextState);
    GX2SetColorBuffer(&sDrcColourBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, (float) sDrcColourBuffer.surface.width, (float) sDrcColourBuffer.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, (float) sDrcColourBuffer.surface.width, (float) sDrcColourBuffer.surface.height);
    GX2SetDRCScale((float) sDrcColourBuffer.surface.width, (float) sDrcColourBuffer.surface.height);

    // Set 60fps VSync
    GX2SetSwapInterval(1);

    return TRUE;

error:
    if (sCommandBufferPool) {
        FreeMEM2(sCommandBufferPool);
        sCommandBufferPool = NULL;
    }

    if (sTvScanBuffer) {
        FreeBucket(sTvScanBuffer);
        sTvScanBuffer = NULL;
    }

    if (sTvColourBuffer.surface.image) {
        FreeMEM2(sTvColourBuffer.surface.image);
        sTvColourBuffer.surface.image = NULL;
    }

    if (sTvContextState) {
        FreeMEM2(sTvContextState);
        sTvContextState = NULL;
    }

    if (sDrcContextState) {
        FreeMEM2(sDrcContextState);
        sDrcContextState = NULL;
    }

    if (sDrcScanBuffer) {
        FreeBucket(sDrcScanBuffer);
        sDrcScanBuffer = NULL;
    }

    if (sDrcColourBuffer.surface.image) {
        FreeMEM2(sDrcColourBuffer.surface.image);
        sDrcColourBuffer.surface.image = NULL;
    }

    return FALSE;
}

void GfxShutdown() {
    if (sGpuTimedOut) {
        GX2ResetGPU(0);
        sGpuTimedOut = FALSE;
    }

    GX2RSetAllocator(NULL, NULL);

    GX2Shutdown();

    DeinitMemory();

    if (sTvContextState) {
        FreeMEM2(sTvContextState);
        sTvContextState = NULL;
    }

    if (sDrcContextState) {
        FreeMEM2(sDrcContextState);
        sDrcContextState = NULL;
    }

    if (sCommandBufferPool) {
        FreeMEM2(sCommandBufferPool);
        sCommandBufferPool = NULL;
    }

    deInitBucketHeap();
}

void GfxBeginRender() {
    uint32_t swapCount, flipCount;
    OSTime lastFlip, lastVsync;
    uint32_t waitCount = 0;

    while (1) {
        GX2GetSwapStatus(&swapCount, &flipCount, &lastFlip, &lastVsync);

        if (flipCount >= swapCount) {
            break;
        }

        if (waitCount >= 10) {
            WHBLogPrint("WHBGfxBeginRender wait for swap timed out");
            sGpuTimedOut = TRUE;
            break;
        }

        waitCount++;
        GX2WaitForVsync();
    }
}

void GfxBeginRenderDRC() {
    GX2SetContextState(sDrcContextState);
}

void GfxFinishRenderDRC() {
    GX2CopyColorBufferToScanBuffer(&sDrcColourBuffer, GX2_SCAN_TARGET_DRC);
}


void GfxBeginRenderTV() {
    GX2SetContextState(sTvContextState);
}

void GfxFinishRenderTV() {
    GX2CopyColorBufferToScanBuffer(&sTvColourBuffer, GX2_SCAN_TARGET_TV);
}

void GfxFinishRender() {
    GX2SwapScanBuffers();
    GX2Flush();
    GX2DrawDone();
    GX2SetTVEnable(TRUE);
    GX2SetDRCEnable(TRUE);
}

BOOL GfxInitFetchShader(WHBGfxShaderGroup *group) {
    uint32_t size             = GX2CalcFetchShaderSizeEx(group->numAttributes,
                                                         GX2_FETCH_SHADER_TESSELLATION_NONE,
                                                         GX2_TESSELLATION_MODE_DISCRETE);
    group->fetchShaderProgram = AllocMEM2(size, GX2_SHADER_PROGRAM_ALIGNMENT);

    GX2InitFetchShaderEx(&group->fetchShader,
                         group->fetchShaderProgram,
                         group->numAttributes,
                         group->attributes,
                         GX2_FETCH_SHADER_TESSELLATION_NONE,
                         GX2_TESSELLATION_MODE_DISCRETE);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, group->fetchShaderProgram, size);
    return TRUE;
}

static uint32_t
GfxGetAttribFormatSel(GX2AttribFormat format) {
    switch (format) {
        case GX2_ATTRIB_FORMAT_UNORM_8:
        case GX2_ATTRIB_FORMAT_UINT_8:
        case GX2_ATTRIB_FORMAT_SNORM_8:
        case GX2_ATTRIB_FORMAT_SINT_8:
        case GX2_ATTRIB_FORMAT_FLOAT_32:
            return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
        case GX2_ATTRIB_FORMAT_UNORM_8_8:
        case GX2_ATTRIB_FORMAT_UINT_8_8:
        case GX2_ATTRIB_FORMAT_SNORM_8_8:
        case GX2_ATTRIB_FORMAT_SINT_8_8:
        case GX2_ATTRIB_FORMAT_FLOAT_32_32:
            return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
        case GX2_ATTRIB_FORMAT_FLOAT_32_32_32:
            return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_1);
        case GX2_ATTRIB_FORMAT_UNORM_8_8_8_8:
        case GX2_ATTRIB_FORMAT_UINT_8_8_8_8:
        case GX2_ATTRIB_FORMAT_SNORM_8_8_8_8:
        case GX2_ATTRIB_FORMAT_SINT_8_8_8_8:
        case GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32:
            return GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
            break;
        default:
            return GX2_SEL_MASK(GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    }
}

static int32_t
GfxGetVertexAttribVarLocation(const GX2VertexShader *shader,
                              const char *name) {
    uint32_t i;

    for (i = 0; i < shader->attribVarCount; ++i) {
        if (strcmp(shader->attribVars[i].name, name) == 0) {
            return shader->attribVars[i].location;
        }
    }

    return -1;
}

BOOL GfxInitShaderAttribute(WHBGfxShaderGroup *group,
                            const char *name,
                            uint32_t buffer,
                            uint32_t offset,
                            GX2AttribFormat format) {
    GX2AttribStream *attrib;
    int32_t location;

    location = GfxGetVertexAttribVarLocation(group->vertexShader, name);
    if (location == -1) {
        return FALSE;
    }

    attrib             = &group->attributes[group->numAttributes++];
    attrib->location   = location;
    attrib->buffer     = buffer;
    attrib->offset     = offset;
    attrib->format     = format;
    attrib->type       = GX2_ATTRIB_INDEX_PER_VERTEX;
    attrib->aluDivisor = 0;
    attrib->mask       = GfxGetAttribFormatSel(format);
    attrib->endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
    return TRUE;
}