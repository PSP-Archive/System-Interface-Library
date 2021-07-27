/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/sysfont.c: System font rendering functionality for Linux.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/texture.h"
#include "src/utility/utf8.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for X11 fonts. */
struct SysFont {
    /* X11 display data (independent of the font). */
    Display *display;
    int screen;
    Window root;
    GC fill_gc, text_gc;
    unsigned long pixel_mask;
    int pixel_shift, pixel_bits;

    /* The selected font. */
    XFontStruct *xfont;
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * scale_for_size:  Return the scale factor for the given font at the
 * given size.
 *
 * [Parameters]
 *     font: SysFont object.
 *     size: Font size.
 * [Return value]
 *     Scale factor for rendering font.
 */
static inline float scale_for_size(const SysFont *font, float size);

/**
 * utf8_to_str2b:  Convert a UTF-8 string to an XChar2b string.
 *
 * [Parameters]
 *     str: UTF-8 encoded string.
 * [Return value]
 *     Newly allocated XChar2b string, or NULL on error (out of memory).
 */
static XChar2b *utf8_to_xchar2b(const char *str, int *num_chars_ret);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysFont *sys_sysfont_create(const char *name, float size, int mem_flags)
{
    SysFont *font = mem_alloc(sizeof(*font), 0, mem_flags);
    if (UNLIKELY(!font)) {
        DLOG("Failed to allocate SysFont structure");
        goto error_return;
    }

    font->display = linux_x11_display();
    XLockDisplay(font->display);
    font->screen = linux_x11_screen();
    font->root = RootWindow(font->display, font->screen);
    Visual *default_visual = DefaultVisual(font->display, font->screen);

    /* Use the green component to generate alpha values, since it will
     * generally have the highest precision. */
    unsigned long pixel_mask = default_visual->green_mask;
    ASSERT(pixel_mask != 0, pixel_mask = 0xFF);
    font->pixel_mask = pixel_mask;
    font->pixel_shift = 0;
    font->pixel_bits = 0;
    while (!(pixel_mask & 1)) {
        font->pixel_shift++;
        pixel_mask >>= 1;
    }
    while (pixel_mask) {
        font->pixel_bits++;
        pixel_mask >>= 1;
    }

    font->xfont = NULL;
    if (*name) {
        font->xfont = XLoadQueryFont(font->display, name);
        if (!font->xfont && *name != '-') {
            DLOG("Failed to load font \"%s\", trying expansion", name);
            char namebuf[1000];
            if (strformat_check(namebuf, sizeof(namebuf),
                                "-*-%s-*-r-*-*-%d-*-*-*-*-*-*-*",
                                name, iroundf(size))) {
                font->xfont = XLoadQueryFont(font->display, namebuf);
            } else {
                DLOG("Buffer overflow on font name, not expanding: %s", name);
            }
        }
        if (!font->xfont) {
            DLOG("Failed to load font \"%s\", trying \"fixed\"", name);
        }
    }
    if (!font->xfont) {
        font->xfont = XLoadQueryFont(font->display, "fixed");
        if (!font->xfont) {
            DLOG("Failed to load font \"fixed\"");
            goto error_unlock_x11;
        }
    }

    const unsigned long black = BlackPixel(font->display, font->screen);
    const unsigned long white = WhitePixel(font->display, font->screen);
    font->fill_gc = XCreateGC(font->display, font->root,
                              GCForeground | GCBackground,
                              &(XGCValues){
                                  .foreground = black,
                                  .background = black,
                              });
    if (!font->fill_gc) {
        DLOG("Failed to create background fill GC");
        goto error_free_font;
    }
    font->text_gc = XCreateGC(font->display, font->root,
                              GCForeground | GCBackground | GCFont,
                              &(XGCValues){
                                  .foreground = white,
                                  .background = black,
                                  .font = font->xfont->fid,
                              });
    if (!font->text_gc) {
        DLOG("Failed to create text rendering GC");
        goto error_free_fill_gc;
    }

    XUnlockDisplay(font->display);
    return font;

  error_free_fill_gc:
    XFreeGC(font->display, font->fill_gc);
  error_free_font:
    XFreeFont(font->display, font->xfont);
  error_unlock_x11:
    XUnlockDisplay(font->display);
    mem_free(font);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_destroy(SysFont *font)
{
    XLockDisplay(font->display);
    XFreeGC(font->display, font->text_gc);
    XFreeGC(font->display, font->fill_gc);
    XFreeFont(font->display, font->xfont);
    XUnlockDisplay(font->display);
    mem_free(font);
}

/*-----------------------------------------------------------------------*/

int sys_sysfont_native_size(SysFont *font)
{
    ASSERT(font->xfont != NULL, return 0);
    return font->xfont->ascent + font->xfont->descent;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_metrics(
    SysFont *font, float size, float *height_ret, float *baseline_ret,
    float *ascent_ret, float *descent_ret)
{
    ASSERT(font->xfont != NULL, return);
    const float scale = scale_for_size(font, size);

    *height_ret = size;
    *baseline_ret = scale * font->xfont->ascent;
    *ascent_ret = scale * font->xfont->ascent;
    *descent_ret = scale * font->xfont->descent;
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_char_advance(SysFont *font, int32_t ch, float size)
{
    ASSERT(font->xfont != NULL, return 0);
    const float scale = scale_for_size(font, size);

    if (ch >= 0x10000) {
        return 0;
    }
    XChar2b ch2 = {.byte1 = ch >> 8, .byte2 = ch & 0xFF};
    XCharStruct overall = {.width = 0};
    int dummy;
    XTextExtents16(font->xfont, &ch2, 1, &dummy, &dummy, &dummy, &overall);
    return overall.width * scale;
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_text_advance(SysFont *font, const char *str, float size)
{
    ASSERT(font->xfont != NULL, return 0);
    const float scale = scale_for_size(font, size);

    int num_chars;
    XChar2b *str2b = utf8_to_xchar2b(str, &num_chars);
    if (UNLIKELY(!str2b)) {
        return 0;
    }
    XCharStruct overall = {.width = 0};
    int dummy;
    XTextExtents16(font->xfont, str2b, num_chars, &dummy, &dummy, &dummy,
                   &overall);
    mem_free(str2b);
    return overall.width * scale;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_text_bounds(
    SysFont *font, const char *str, float size, float *left_ret,
    float *right_ret)
{
    ASSERT(font->xfont != NULL, return);
    const float scale = scale_for_size(font, size);

    int num_chars;
    XChar2b *str2b = utf8_to_xchar2b(str, &num_chars);
    if (UNLIKELY(!str2b)) {
        *left_ret = 0;
        *right_ret = 0;
        return;
    }
    XCharStruct overall = {.lbearing = 0, .rbearing = 0};
    int dummy;
    if (UNLIKELY(XTextExtents16(font->xfont, str2b, num_chars, &dummy, &dummy,
                                &dummy, &overall) != 0)) {
        mem_free(str2b);
        *left_ret = 0;
        *right_ret = 0;
        return;
    }
    mem_free(str2b);
    *left_ret = -overall.lbearing * scale;
    *right_ret = overall.rbearing * scale;
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_sysfont_render(
    SysFont *font, const char *str, float size, float *origin_x_ret,
    float *origin_y_ret, float *advance_ret, float *scale_ret)
{
    ASSERT(font->xfont != NULL, goto error_return);
    const float scale = scale_for_size(font, size);

    int num_chars;
    XChar2b *str2b = utf8_to_xchar2b(str, &num_chars);
    if (UNLIKELY(!str2b)) {
        goto error_return;
    }
    XCharStruct overall = {.width = 0, .lbearing = 0, .rbearing = 0};
    int dummy;
    XTextExtents16(font->xfont, str2b, num_chars, &dummy, &dummy, &dummy,
                   &overall);
    const int width = overall.rbearing - overall.lbearing;
    const int height = font->xfont->ascent + font->xfont->descent;

    uint8_t *pixbuf = mem_alloc(width * height, 0, 0);
    if (UNLIKELY(!pixbuf)) {
        goto error_free_str2b;
    }

    XLockDisplay(font->display);
    XImage *image;
    Pixmap pixmap = XCreatePixmap(font->display, font->root, width, height,
                                  DefaultDepth(font->display, font->screen));
    if (pixmap) {
        XFillRectangle(font->display, pixmap, font->fill_gc,
                       0, 0, width, height);
        XDrawString16(font->display, pixmap, font->text_gc,
                      -overall.lbearing, font->xfont->ascent,
                      str2b, num_chars);
        image = XGetImage(font->display, pixmap, 0, 0, width, height,
                          AllPlanes, ZPixmap);
        XFreePixmap(font->display, pixmap);
    }
    XUnlockDisplay(font->display);
    if (UNLIKELY(!pixmap)) {
        DLOG("Failed to create X11 pixmap for rendering");
        goto error_free_pixbuf;
    } else if (UNLIKELY(!image)) {
        DLOG("Failed to read rendered text image");
        goto error_free_pixbuf;
    }

    const uint8_t *src =
        (const uint8_t *)image->data + ((height-1) * image->bytes_per_line);
    uint8_t *dest = pixbuf;
    /* Pull these out to help optimization. */
    const int bpp = image->bits_per_pixel;
    const unsigned long pixel_mask = font->pixel_mask;
    const int pixel_shift = font->pixel_shift;
    const int pixel_bits = font->pixel_bits;
    for (int y = 0; y < height; y++, src-=image->bytes_per_line, dest+=width) {
        /* Use uint32_t here to avoid cast-align warnings. */
        const uint32_t *src_line = (const void *)src;
        for (int x = 0; x < width; x++) {
            unsigned long pixel;
            if (bpp == 32) {
                pixel = ((const uint32_t *)src_line)[x];
            } else if (bpp == 24) {
                pixel = ((const uint8_t *)src_line)[x*3+0] <<  0
                      | ((const uint8_t *)src_line)[x*3+1] <<  8
                      | ((const uint8_t *)src_line)[x*3+2] << 16;
            } else if (bpp == 16) {
                pixel = ((const uint16_t *)src_line)[x];
            } else {  // Assume 8bpp.
                pixel = ((const uint8_t *)src_line)[x];
            }
            pixel = (pixel & pixel_mask) >> pixel_shift;
            if (pixel_bits > 8) {
                pixel >>= pixel_bits - 8;
            } else if (pixel_bits < 8) {
                pixel <<= 8 - pixel_bits;
                pixel |= pixel >> pixel_bits;
            }
            dest[x] = (uint8_t)pixel;
        }
    }

    XDestroyImage(image);
    mem_free(str2b);

    *origin_x_ret = -overall.lbearing;
    *origin_y_ret = font->xfont->descent;
    *advance_ret = overall.width * scale;
    *scale_ret = scale;
    return sys_texture_create(
        width, height, TEX_FORMAT_A8, 1, pixbuf, width,
        (int32_t[]){0}, (int32_t[]){width * height}, 0, 0, 1);

  error_free_pixbuf:
    mem_free(pixbuf);
  error_free_str2b:
    mem_free(str2b);
  error_return:
    return NULL;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static inline float scale_for_size(const SysFont *font, float size)
{
    return size / (font->xfont->ascent + font->xfont->descent);
}

/*-----------------------------------------------------------------------*/

static XChar2b *utf8_to_xchar2b(const char *str, int *num_chars_ret)
{
    PRECOND(str != NULL, return NULL);
    PRECOND(num_chars_ret != NULL, return NULL);

    XChar2b *buf = mem_alloc(sizeof(*buf) * strlen(str), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!buf)) {
        DLOG("No memory for temporary string buffer");
        return NULL;
    }
    int num_chars = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1 || ch >= 0x10000) {
            continue;
        }
        buf[num_chars].byte1 = ch >> 8;
        buf[num_chars].byte2 = ch & 0xFF;
        num_chars++;
    }
    *num_chars_ret = num_chars;
    return buf;
}

/*************************************************************************/
/*************************************************************************/
