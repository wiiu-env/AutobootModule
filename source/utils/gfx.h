#pragma once
#include <gx2/context.h>
#include <gx2/shaders.h>
#include <gx2/texture.h>
#include <whb/gfx.h>
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL GfxInit();

void GfxShutdown();

void GfxBeginRender();

void GfxFinishRender();

void GfxBeginRenderDRC();

void GfxFinishRenderDRC();

void GfxBeginRenderTV();

void GfxFinishRenderTV();

BOOL GfxInitShaderAttribute(WHBGfxShaderGroup *group,
                            const char *name,
                            uint32_t buffer,
                            uint32_t offset,
                            GX2AttribFormat format);

BOOL GfxInitFetchShader(WHBGfxShaderGroup *group);

#ifdef __cplusplus
}
#endif

/** @} */
