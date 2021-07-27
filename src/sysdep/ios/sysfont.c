/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/sysfont.c: System font rendering functionality for iOS.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/texture.h"
#include "src/utility/utf8.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreText/CTFont.h>
#include <CoreText/CTLine.h>
#include <CoreText/CTStringAttributes.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine declarations. */

/**
 * create_font:  Create the system UI font for the current language.
 *
 * [Parameters]
 *     size: Desired font size.
 * [Return value]
 *     System UI font (CTFont) for current language.
 */
static CTFontRef create_font(float size);

/**
 * create_line:  Create a CTLine object for the given text string and font
 * size.
 *
 * The return variables (*astr_ret and *font_ret) are only set on success.
 *
 * [Parameters]
 *     text: Text for the CTLine (UTF-8 encoded).
 *     size: Font size for rendering text.
 *     astr_ret: Pointer to variable to receive the CFAttributedString.
 *     font_ret: Pointer to variable to receive the created CTFont.
 * [Return value]
 *     Newly created CTLine object, or NULL on error.
 */
static CTLineRef create_line(const char *text, float size,
                             CFAttributedStringRef *astr_ret,
                             CTFontRef *font_ret);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysFont *sys_sysfont_create(UNUSED const char *name, UNUSED float size,
                            UNUSED int mem_flags)
{
    /* We currently create fonts on the fly as needed, so just return an
     * arbitrary non-NULL pointer to indicate success. */
    return (SysFont *)1;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_destroy(UNUSED SysFont *font)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

int sys_sysfont_native_size(UNUSED SysFont *font)
{
    return 0;  // Any size works fine.
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_metrics(
    UNUSED SysFont *font_, float size, float *height_ret, float *baseline_ret,
    float *ascent_ret, float *descent_ret)
{
    CTFontRef font = create_font(size);
    const float ascent  = CTFontGetAscent(font);
    const float descent = CTFontGetDescent(font);
    const float leading = CTFontGetLeading(font);
    CFRelease(font);

    if (height_ret) {
        *height_ret = ascent + descent + leading;
    }
    if (baseline_ret) {
        /* Round up to match render behavior, and tweak the value a bit
         * for better final results. */
        *baseline_ret = ceilf(ascent * 1.1f);
    }
    if (ascent_ret) {
        *ascent_ret = ascent;
    }
    if (descent_ret) {
        *descent_ret = descent;
    }
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_char_advance(UNUSED SysFont *font_, int32_t ch, float size)
{
    CTFontRef font = create_font(size);
    UniChar uni = ch;
    CGGlyph glyph;
    if (CTFontGetGlyphsForCharacters(font, &uni, &glyph, 1)) {
        const float advance =
            CTFontGetAdvancesForGlyphs(font, kCTFontHorizontalOrientation,
                                       &glyph, NULL, 1);
        CFRelease(font);
        return advance;
    } else {
        DLOG("Failed to get glyph for U+%04X, trying with CTLine", ch);
        CFRelease(font);
        uint8_t buf[7];
        if (ch < 0x80) {
            buf[0] = ch;
            buf[1] = 0;
        } else if (ch < 0x800) {
            buf[0] = 0xC0 |  (ch >>  6);
            buf[1] = 0x80 | ((ch >>  0) & 0x3F);
            buf[2] = 0;
        } else if (ch < 0x10000) {
            buf[0] = 0xE0 |  (ch >> 12);
            buf[1] = 0x80 | ((ch >>  6) & 0x3F);
            buf[2] = 0x80 | ((ch >>  0) & 0x3F);
            buf[3] = 0;
        } else if (ch < 0x200000) {
            buf[0] = 0xF0 |  (ch >> 18);
            buf[1] = 0x80 | ((ch >> 12) & 0x3F);
            buf[2] = 0x80 | ((ch >>  6) & 0x3F);
            buf[3] = 0x80 | ((ch >>  0) & 0x3F);
            buf[4] = 0;
        } else if (ch < 0x4000000) {
            buf[0] = 0xF8 |  (ch >> 24);
            buf[1] = 0x80 | ((ch >> 18) & 0x3F);
            buf[2] = 0x80 | ((ch >> 12) & 0x3F);
            buf[3] = 0x80 | ((ch >>  6) & 0x3F);
            buf[4] = 0x80 | ((ch >>  0) & 0x3F);
            buf[5] = 0;
        } else {
            buf[0] = 0xFC |  (ch >> 30);
            buf[1] = 0x80 | ((ch >> 24) & 0x3F);
            buf[2] = 0x80 | ((ch >> 18) & 0x3F);
            buf[3] = 0x80 | ((ch >> 12) & 0x3F);
            buf[4] = 0x80 | ((ch >>  6) & 0x3F);
            buf[5] = 0x80 | ((ch >>  0) & 0x3F);
            buf[6] = 0;
        }
        return sys_sysfont_text_advance(font_, (const char *)buf, size);
    }
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_text_advance(UNUSED SysFont *font_, const char *str,
                               float size)
{
    CFAttributedStringRef astr;
    CTFontRef font;
    CTLineRef line = create_line(str, size, &astr, &font);
    if (UNLIKELY(!line)) {
        DLOG("Failed to create CTLine for text: %s", str);
        return 0;
    }

    const float width =
        (float)CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    CFRelease(line);
    CFRelease(astr);
    CFRelease(font);
    return width;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_text_bounds(
    UNUSED SysFont *font_, const char *str, float size, float *left_ret,
    float *right_ret)
{
    CFAttributedStringRef astr;
    CTFontRef font;
    CTLineRef line = create_line(str, size, &astr, &font);
    if (UNLIKELY(!line)) {
        DLOG("Failed to create CTLine for text: %s", str);
        *left_ret = *right_ret = 0;
        return;
    }

    /* We won't actually render into this, but we need a CGContext for
     * CTLineGetImageBounds(). */
    CGContextRef context = CGBitmapContextCreate(
        NULL, 1, 1, 8, 1, NULL, kCGImageAlphaOnly);
    if (UNLIKELY(!context)) {
        DLOG("Failed to create CGContext for text: %s", str);
        CFRelease(line);
        CFRelease(astr);
        CFRelease(font);
        *left_ret = *right_ret = 0;
        return;
    }

    CGContextSetTextPosition(context, 0, 0);
    CGRect r = CTLineGetImageBounds(line, context);
    CFRelease(context);
    CFRelease(line);
    CFRelease(astr);
    CFRelease(font);
    *left_ret = r.origin.x;
    *right_ret = r.origin.x + r.size.width;
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_sysfont_render(
    UNUSED SysFont *font_, const char *str, float size, float *origin_x_ret,
    float *origin_y_ret, float *advance_ret, float *scale_ret)
{
    CFAttributedStringRef astr;
    CTFontRef font;
    CTLineRef line = create_line(str, size, &astr, &font);
    if (UNLIKELY(!line)) {
        DLOG("Failed to create CTLine for text: %s", str);
        return 0;
    }

    /* Use CTLineGetTypographicBounds() instead of CTLineGetImageBounds()
     * because the latter is very slow (~150us/call vs. ~1.5us/call for
     * GetTypographicBounds()). */
    CGFloat ascent, descent;
    const int width =
        iceil(CTLineGetTypographicBounds(line, &ascent, &descent, NULL));
    const float baseline = ceilf(ascent);
    /* Deliberately ceil(ascent)+ceil(descent) instead of ceil(ascent+descent),
     * because we put the baseline on an integral coordinate. */
    const int height = iceilf(ascent) + iceilf(descent);
    const int tex_width = align_up(width, 16);
    const int tex_height = height;
    const int origin_x = (tex_width - width) / 2;

    uint8_t *pixels = mem_alloc(tex_width * tex_height, 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!pixels)) {
        DLOG("Failed to allocate %dx%d pixels for text: %s",
             tex_width, tex_height, str);
        CFRelease(line);
        CFRelease(astr);
        CFRelease(font);
        return 0;
    }
    CGContextRef context = CGBitmapContextCreate(
        pixels + origin_x, width, height, 8, tex_width, NULL,
        kCGImageAlphaOnly);
    if (UNLIKELY(!context)) {
        DLOG("Failed to create CGContext for text: %s", str);
        mem_free(pixels);
        CFRelease(line);
        CFRelease(astr);
        CFRelease(font);
        return 0;
    }

    CGContextSetTextPosition(context, 0, height - baseline);
    CTLineDraw(line, context);
    CFRelease(context);
    CFRelease(line);
    CFRelease(astr);
    CFRelease(font);

    /* Need to flip the image upside down for the texture. */
    uint8_t *top, *bottom;
    for (top = pixels, bottom = top + ((tex_height - 1) * tex_width);
         top < bottom;
         top += tex_width, bottom -= tex_width)
    {
        uint8_t temp[256];
        for (int x = 0; x < tex_width; x += sizeof(temp)) {
            const int copy = ubound(tex_width - x, (int)sizeof(temp));
            memcpy(temp, &top[x], copy);
            memcpy(&top[x], &bottom[x], copy);
            memcpy(&bottom[x], temp, copy);
        }
    }

    SysTexture *texture = sys_texture_create(
        tex_width, tex_height, TEX_FORMAT_A8, 1, pixels, tex_width,
        (int32_t[]){0}, (int32_t[]){tex_width * tex_height}, 0, 0, 1);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to create %dx%d texture for text: %s",
             tex_width, tex_height, str);
        return 0;
    }

    *origin_x_ret = 0;
    *origin_y_ret = tex_height - ceilf(ascent);
    *advance_ret = width;
    *scale_ret = 1;
    return texture;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static CTFontRef create_font(float size)
{
    return CTFontCreateUIFontForLanguage(kCTFontSystemFontType, size, NULL);
}

/*-----------------------------------------------------------------------*/

static CTLineRef create_line(const char *text, float size,
                             CFAttributedStringRef *astr_ret,
                             CTFontRef *font_ret)
{
    PRECOND(astr_ret != NULL, return NULL);
    PRECOND(font_ret != NULL, return NULL);

    CFStringRef str = CFStringCreateWithCStringNoCopy(
        NULL, text, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (UNLIKELY(!str)) {
        DLOG("Failed to create CFString -- invalid UTF-8 in string?");
        return NULL;
    }
    CTFontRef font = create_font(size);
    CFDictionaryRef dict = CFDictionaryCreate(
        NULL, (CFTypeRef[]){kCTFontAttributeName}, (CFTypeRef[]){font}, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef astr = CFAttributedStringCreate(NULL, str, dict);
    CFRelease(str);
    CFRelease(dict);
    if (!astr) {
        DLOG("Failed to create CFAttributedStringRef for: %s", text);
        CFRelease(font);
        return 0;
    }

    CTLineRef line = CTLineCreateWithAttributedString(astr);
    if (line) {
        *astr_ret = astr;
        *font_ret = font;
    } else {
        CFRelease(astr);
        CFRelease(font);
    }
    return line;
}

/*************************************************************************/
/*************************************************************************/
