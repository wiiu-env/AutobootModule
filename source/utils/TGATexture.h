#pragma once

#include <cstdint>
#include <gx2/texture.h>

struct WUT_PACKED TGA_HEADER {
    uint8_t identsize;     // size of ID field that follows 18 byte header (0 usually)
    uint8_t colourmaptype; // type of colour map 0=none, 1=has palette
    uint8_t imagetype;     // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

    uint8_t colourmapstart[2];  // first colour map entry in palette
    uint8_t colourmaplength[2]; // number of colours in palette
    uint8_t colourmapbits;      // number of bits per palette entry 15,16,24,32

    uint16_t xstart;    // image x origin
    uint16_t ystart;    // image y origin
    uint16_t width;     // image width in pixels
    uint16_t height;    // image height in pixels
    uint8_t bits;       // image bits per pixel 8,16,24,32
    uint8_t descriptor; // image descriptor bits (vh flip bits)
};

// quick and dirty 24-bit TGA loader
GX2Texture *TGA_LoadTexture(std::span<uint8_t> data);