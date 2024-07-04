#include <gx2/draw.h>
#include <gx2/mem.h>
#include <malloc.h>
#include <span>

#include "TGATexture.h"
#include "logger.h"
#include <whb/log.h>

/*
 * Based on
 * https://github.com/Xpl0itU/savemii/blob/70e3b63db52113519230e1e39bd56876cef12dc8/src/tga_reader.cpp
 * and
 * https://github.com/Crementif/WiiU-GX2-Shader-Examples/blob/5a88f861043dcb7666d4d25a6bab6bd271e76d5f/include/TGATexture.h
 */

uint16_t inline _swapU16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

GX2Texture *TGA_LoadTexture(std::span<uint8_t> data) {
    TGA_HEADER *tgaHeader = (TGA_HEADER *) data.data();

    uint32_t width  = _swapU16(tgaHeader->width);
    uint32_t height = _swapU16(tgaHeader->height);

    if (tgaHeader->bits != 24) {
        DEBUG_FUNCTION_LINE_WARN("Only 24bit TGA images are supported");
        return nullptr;
    }
    if (tgaHeader->imagetype != 2 && tgaHeader->imagetype != 3) {
        DEBUG_FUNCTION_LINE_WARN("Only uncompressed TGA images are supported");
        return nullptr;
    }

    GX2Texture *texture = (GX2Texture *) malloc(sizeof(GX2Texture));
    *texture            = {};

    texture->surface.width     = width;
    texture->surface.height    = height;
    texture->surface.depth     = 1;
    texture->surface.mipLevels = 1;
    texture->surface.format    = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture->surface.aa        = GX2_AA_MODE1X;
    texture->surface.use       = GX2_SURFACE_USE_TEXTURE;
    texture->surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
    texture->surface.tileMode  = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture->surface.swizzle   = 0;
    texture->viewFirstMip      = 0;
    texture->viewNumMips       = 1;
    texture->viewFirstSlice    = 0;
    texture->viewNumSlices     = 1;
    texture->compMap           = 0x0010203;
    GX2CalcSurfaceSizeAndAlignment(&texture->surface);
    GX2InitTextureRegs(texture);

    if (texture->surface.imageSize == 0) {
        return nullptr;
    }

    texture->surface.image = memalign(texture->surface.alignment, texture->surface.imageSize);
    if (!texture->surface.image) {
        return nullptr;
    }

    for (uint32_t y = 0; y < height; y++) {
        uint32_t *out_data = (uint32_t *) texture->surface.image + (y * texture->surface.pitch);
        for (uint32_t x = 0; x < width; x++) {
            int index = sizeof(TGA_HEADER) + (3 * width * (height - 1 - y)) + (3 * x);

            int b = data[index + 0] & 0xFF;
            int g = data[index + 1] & 0xFF;
            int r = data[index + 2] & 0xFF;

            *out_data = r << 24 | g << 16 | b << 8 | 0xFF;
            out_data++;
        }
    }

    // todo: create texture with optimal tile format and use GX2CopySurface to convert from linear to tiled format
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE, texture->surface.image, texture->surface.imageSize);

    return texture;
}
