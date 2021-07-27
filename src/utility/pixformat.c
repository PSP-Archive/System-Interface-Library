/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/pixformat.c: Utility routines for converting between
 * different pixel formats.
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/utility/pixformat.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void pixel_convert_bgra8888_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint32_t *src = (const uint32_t *)src_;
    uint32_t *dest = (uint32_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint32_t pixel = src[i];
#ifdef IS_LITTLE_ENDIAN
        dest[i] = (pixel & 0xFF00FF00)
                | (pixel>>16 & 0xFF)
                | (pixel & 0xFF) << 16;
#else
        dest[i] = (pixel & 0x00FF00FF)
                | (pixel>>16 & 0xFF00)
                | (pixel & 0xFF00) << 16;
#endif
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgb565_bgr565(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = pixel>>11 | (pixel & 0x07E0) | pixel<<11;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba5551_abgr1555(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = pixel >> 15
                | (pixel & 0x7C00) >> 9
                | (pixel & 0x03E0) << 1
                | pixel << 11;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_bgra5551_abgr1555(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = pixel>>15 | pixel<<1;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba5551_bgra5551(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = (pixel & 0x83E0)
                | (pixel & 0x7C00) >> 10
                | (pixel & 0x001F) << 10;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba4444_abgr4444(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = pixel >> 12
                | (pixel & 0x0F00) >> 4
                | (pixel & 0x00F0) << 4
                | pixel << 12;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_bgra4444_abgr4444(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = pixel>>12 | pixel<<4;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba4444_bgra4444(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint16_t *dest = (uint16_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        dest[i] = (pixel & 0xF0F0)
                | (pixel & 0x0F00) >> 8
                | (pixel & 0x000F) << 8;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgb565_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0x001F) >>  0;
        const int g = (pixel & 0x07E0) >>  5;
        const int b = (pixel & 0xF800) >> 11;
        dest[i*4+0] = r<<3 | r>>2;
        dest[i*4+1] = g<<2 | g>>4;
        dest[i*4+2] = b<<3 | b>>2;
        dest[i*4+3] = 255;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_bgr565_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0xF800) >> 11;
        const int g = (pixel & 0x07E0) >>  5;
        const int b = (pixel & 0x001F) >>  0;
        dest[i*4+0] = r<<3 | r>>2;
        dest[i*4+1] = g<<2 | g>>4;
        dest[i*4+2] = b<<3 | b>>2;
        dest[i*4+3] = 255;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba5551_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0x001F) >>  0;
        const int g = (pixel & 0x03E0) >>  5;
        const int b = (pixel & 0x7C00) >> 10;
        const int a = (pixel & 0x8000) >> 15;
        dest[i*4+0] = r<<3 | r>>2;
        dest[i*4+1] = g<<3 | g>>2;
        dest[i*4+2] = b<<3 | b>>2;
        dest[i*4+3] = a ? 255 : 0;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_bgra5551_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0x7C00) >> 10;
        const int g = (pixel & 0x03E0) >>  5;
        const int b = (pixel & 0x001F) >>  0;
        const int a = (pixel & 0x8000) >> 15;
        dest[i*4+0] = r<<3 | r>>2;
        dest[i*4+1] = g<<3 | g>>2;
        dest[i*4+2] = b<<3 | b>>2;
        dest[i*4+3] = a ? 255 : 0;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_rgba4444_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint32_t *dest = (uint32_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0x000F) >>  0;
        const int g = (pixel & 0x00F0) >>  4;
        const int b = (pixel & 0x0F00) >>  8;
        const int a = (pixel & 0xF000) >> 12;
#ifdef IS_LITTLE_ENDIAN
        const uint32_t out_pixel = r | g<<8 | b<<16 | a<<24;
#else
        const uint32_t out_pixel = r<<24 | g<<16 | b<<8 | a;
#endif
        dest[i] = out_pixel | out_pixel << 4;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_bgra4444_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint16_t *src = (const uint16_t *)src_;
    uint32_t *dest = (uint32_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint16_t pixel = src[i];
        const int r = (pixel & 0x0F00) >>  8;
        const int g = (pixel & 0x00F0) >>  4;
        const int b = (pixel & 0x000F) >>  0;
        const int a = (pixel & 0xF000) >> 12;
#ifdef IS_LITTLE_ENDIAN
        const uint32_t out_pixel = r | g<<8 | b<<16 | a<<24;
#else
        const uint32_t out_pixel = r<<24 | g<<16 | b<<8 | a;
#endif
        dest[i] = out_pixel | out_pixel << 4;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_l8_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint8_t *src = (const uint8_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint8_t pixel = src[i];
        dest[i*4+0] = pixel;
        dest[i*4+1] = pixel;
        dest[i*4+2] = pixel;
        dest[i*4+3] = 255;
    }
}

/*-----------------------------------------------------------------------*/

void pixel_convert_a8_rgba8888(void *dest_, const void *src_, size_t len)
{
    const uint8_t *src = (const uint8_t *)src_;
    uint8_t *dest = (uint8_t *)dest_;
    for (size_t i = 0; i < len; i++) {
        const uint8_t pixel = src[i];
        dest[i*4+0] = 255;
        dest[i*4+1] = 255;
        dest[i*4+2] = 255;
        dest[i*4+3] = pixel;
    }
}

/*************************************************************************/
/*************************************************************************/
