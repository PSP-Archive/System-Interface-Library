/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/quantize.c: Routine to quantize 32-bit-per-pixel images into
 * 8-bit-per-pixel indexed-color images.
 */

#include "tool-common.h"

#include "quantize.h"
#ifdef USE_SSE2
# include "sse2.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Information about each color used in the image. */
struct colorinfo {
    uint32_t color;
    uint32_t count;
};
static struct colorinfo *colortable;  // Dynamically allocated.

/* Color box information for palette generation. */
struct colorbox {
    uint8_t rmin, rmax;
    uint8_t gmin, gmax;
    uint8_t bmin, bmax;
    uint8_t amin, amax;
    uint32_t ncolors;  // Number of colors in the box.
    uint32_t first;    // Index into colortable[] of the first color in
                       // the box.  Since new boxes are only created by
                       // splitting existing boxes, the set of colors in
                       // the box is always consecutive in colortable[].
};

/* Color component comparison order for compare_colors().  The value is
 * treated as four 8-bit fields, each of which specifies a component shift
 * count; the color components are compared starting from the component
 * specified by the low 8 bits of this value.  For example, if the value
 * of this variable was 0x10000818, compare_colors() would first compare
 * the 8-bit component value at color>>24 (i.e., alpha); if the alpha
 * components of the two colors under comparison were different, the
 * comparison would end there.  Otherwise the comparison would continue
 * with the 8-bit component value at color>>8 (i.e., green), then color>>0
 * and finally color>>16. */
static uint32_t color_compare_order;

/*-----------------------------------------------------------------------*/

/* Local function declarations. */

/**
 * compare_colors:  Compare two colortable[] entries, one color component
 * at a time.  Comparison function for qsort().
 *
 * [Parameters]
 *     a, b: Pointers to the colortable[] entries to compare.
 * [Return value]
 *     -1 if a <  b
 *      0 if a == b
 *     +1 if a >  b
 */
static int compare_colors(const void * const a, const void * const b);

/**
 * shrink_box:  Adjust the bounds of the given color box so they encompass
 * the minimum possible size given the colors they contain.  Helper function
 * for generate_palette().
 *
 * [Parameters]
 *     box: Color box to shrink.
 */
static void shrink_box(struct colorbox * const box);

/**
 * split_box:  Split a color box at its median color.  Helper function for
 * generate_palette().
 *
 * [Parameters]
 *     box: Color box to split.
 */
static void split_box(struct colorbox * const box,
                      struct colorbox * const newbox);

/**
 * compare_box:  Compare two color boxes by the number of colors they
 * contain.  Comparison function for qsort().
 *
 * [Parameters]
 *     a, b: Pointers to the color boxes to compare.
 * [Return value]
 *     -1 if a <  b
 *      0 if a == b
 *     +1 if a >  b
 */
static int compare_box(const void * const a, const void * const b);

/**
 * generate_colortable:  Find all colors used in an image and record the
 * color value and number of occurrences for each color in colortable[].
 * The colortable[] array must have been preallocated to hold width*height
 * entries.
 *
 * [Parameters]
 *     imageptr: Image data, in 0xAARRGGBB or 0xAABBGGRR format.
 *     width, height: Image size, in pixels.
 *     stride: Image line stride, in pixels.
 *     palette: Array buffer for generated palette colors.
 *     fixed_colors: Number of colors in palette[] which are preset.
 * [Return value]
 *     Number of colors in the image.
 */
static uint32_t generate_colortable(
    const uint32_t *imageptr, uint32_t width, uint32_t height, uint32_t stride,
    const uint32_t *palette, unsigned int fixed_colors, void (*callback)(void));

/**
 * colordiff_sq:  Return the squared difference between the given colors,
 * taking alpha into account.
 *
 * [Parameters]
 *     color1, color2: Color values, in 0xAARRGGBB format.
 * [Return value]
 *     Square of color difference (range 0 to approximately 0xFC000000).
 */
static inline uint32_t colordiff_sq(uint32_t color1, uint32_t color2);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int quantize_image(const uint32_t *src, const int32_t src_stride,
                   uint8_t *dest, const int32_t dest_stride,
                   const int32_t width, const int32_t height,
                   uint32_t *palette, const unsigned int fixed_colors)
{
    if (src == NULL || dest == NULL || width <= 0 || height <= 0) {
        return 0;
    }

    /* Generate palette colors, if necessary. */
    if (fixed_colors < 256) {
        memset(&palette[fixed_colors], 0, 4 * (256 - fixed_colors));
        generate_palette(src, width, height, src_stride, palette,
                         fixed_colors, NULL);
    }

    /* Convert the image data using the palette. */
    for (int y = 0; y < height; y++) {
        const uint32_t *srcrow = &src[y * src_stride];
        uint8_t *destrow = &dest[y * dest_stride];
        for (int x = 0; x < width; x++) {
            /* Find the closest color. */
            const uint32_t pixel = srcrow[x];
            uint32_t best = 0xFFFFFFFF;
            for (int i = 0; i < 256; i++) {
                const uint32_t diff = colordiff_sq(pixel, palette[i]);
                if (diff < best) {
                    destrow[x] = i;
                    if (diff == 0) {
                        break;
                    } else {
                        best = diff;
                    }
                }
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

void generate_palette(const uint32_t *imageptr, uint32_t width,
                      uint32_t height, uint32_t stride,
                      uint32_t *palette, unsigned int fixed_colors,
                      void (*callback)(void))
{
    PRECOND(imageptr != NULL, return);
    PRECOND(palette != NULL, return);

    uint32_t i;

    /* Find all colors used in the image. */
    colortable = calloc(width * height, sizeof(*colortable));
    if (!colortable) {
        fprintf(stderr, "Out of memory!\n");
        return;
    }
    const uint32_t ncolors =
        generate_colortable(imageptr, width, height, stride,
                            palette, fixed_colors, callback);

    /* If we have enough available entries in the palette for all colors,
     * we can just use them as is. */
    if (ncolors <= 256 - fixed_colors) {
        for (i = 0; i < ncolors; i++) {
            palette[fixed_colors+i] = colortable[i].color;
        }
        free(colortable);
        colortable = NULL;
        return;
    }

    /* Set up the initial color box, containing all colors. */
    struct colorbox box[256];
    box[0].rmin = 0; box[0].rmax = 255;
    box[0].gmin = 0; box[0].gmax = 255;
    box[0].bmin = 0; box[0].bmax = 255;
    box[0].amin = 0; box[0].amax = 255;
    box[0].ncolors = ncolors;
    box[0].first = 0;

    /* Repeatedly subdivide color boxes until we have enough colors. */
    for (i = 1; i < 256 - fixed_colors; i++) {
        /* We keep the boxes sorted in descending order by number of
         * contained colors (i.e., pixels with each color), so the first
         * box in the array is the one we want to split.  If that box has
         * only one color, then the image itself has few enough colors
         * that they can all fit in the palette, and we would have taken
         * the quick-out earlier in this function. */
        ASSERT(box[0].ncolors > 1, break);

        /* Shrink the box to the minimum size that encompasses all the
         * colors it contains. */
        shrink_box(&box[0]);

        /* Find the longest dimension of this box, and cut it in two at the
         * median value of the associated component. */
        split_box(&box[0], &box[i]);

        /* Re-sort the boxes in descending order by number of colors
         * (pixels) contained. */
        qsort(box, i+1, sizeof(*box), compare_box);
    }
    const unsigned int nboxes = i;

    /* Find the weighted average color of each box.  Also check whether the
     * image contains any transparent pixels. */
    int have_transparent_pixel = 0;
    for (i = 0; i < nboxes; i++) {
        if (box[i].amin == 0) {
            have_transparent_pixel = 1;
        }
        uint32_t atot = 0, rtot = 0, gtot = 0, btot = 0;
        uint32_t pixels = 0, alpha_pixels = 0;
        for (uint32_t j = box[i].first; j < box[i].first + box[i].ncolors; j++) {
            const uint32_t color = colortable[j].color;
            const uint32_t count = colortable[j].count;
            const uint32_t alpha_count =
                lbound(((color>>24 & 0xFF) * count) / 255, 1);
            atot   += (color>>24 & 0xFF) * count;
            rtot   += (color>>16 & 0xFF) * alpha_count;
            gtot   += (color>> 8 & 0xFF) * alpha_count;
            btot   += (color>> 0 & 0xFF) * alpha_count;
            pixels += count;
            alpha_pixels += alpha_count;
        }
        palette[fixed_colors + i] = ((atot + pixels/2) / pixels) << 24
            | ((rtot + alpha_pixels/2) / alpha_pixels) << 16
            | ((gtot + alpha_pixels/2) / alpha_pixels) <<  8
            | ((btot + alpha_pixels/2) / alpha_pixels) <<  0;
    }

    /* If the image has transparent pixels, ensure there is at least one
     * transparent color in the palette. */
    if (have_transparent_pixel) {
        int have_transparent_color = 0;
        for (i = 0; i < fixed_colors + nboxes; i++) {
            if (palette[i]>>24 == 0) {
                have_transparent_color = 1;
                break;
            }
        }
        if (!have_transparent_color) {
            /* Find the color with the lowest alpha value and force it to
             * transparent. */
            int best = fixed_colors;
            for (i = fixed_colors+1; i < fixed_colors + nboxes; i++) {
                if (palette[i]>>24 < palette[best]>>24) {
                    best = i;
                }
            }
            palette[best] &= 0x00FFFFFF;
        }
    }

    free(colortable);
    colortable = NULL;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int compare_colors(const void * const a, const void * const b)
{
    const uint32_t color1 = ((const struct colorinfo *)a)->color;
    const uint32_t color2 = ((const struct colorinfo *)b)->color;
    uint32_t order = color_compare_order;
    for (unsigned int i = 0; i < 4; i++, order >>= 8) {
        const unsigned int shift = order & 0xFF;
        const unsigned int component1 = (color1 >> shift) & 0xFF;
        const unsigned int component2 = (color2 >> shift) & 0xFF;
        if (component1 < component2) {
            return -1;
        } else if (component1 > component2) {
            return +1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static void shrink_box(struct colorbox * const box)
{
    PRECOND(box != NULL, return);

    uint8_t rmin = box->rmax, rmax = box->rmin;
    uint8_t gmin = box->gmax, gmax = box->gmin;
    uint8_t bmin = box->bmax, bmax = box->bmin;
    uint8_t amin = box->amax, amax = box->amin;
    for (uint32_t i = box->first; i < box->first + box->ncolors; i++) {
        const uint8_t R = colortable[i].color>>16 & 0xFF;
        const uint8_t G = colortable[i].color>> 8 & 0xFF;
        const uint8_t B = colortable[i].color>> 0 & 0xFF;
        const uint8_t A = colortable[i].color>>24 & 0xFF;
        rmin = min(rmin, R); rmax = max(rmax, R);
        gmin = min(gmin, G); gmax = max(gmax, G);
        bmin = min(bmin, B); bmax = max(bmax, B);
        amin = min(amin, A); amax = max(amax, A);
    }
    box->rmin = rmin; box->rmax = rmax;
    box->gmin = gmin; box->gmax = gmax;
    box->bmin = bmin; box->bmax = bmax;
    box->amin = amin; box->amax = amax;
}

/*-----------------------------------------------------------------------*/

static void split_box(struct colorbox * const box,
                      struct colorbox * const newbox)
{
    PRECOND(box != NULL, return);

    const uint8_t adiff = box->amax - box->amin;
    const uint8_t rdiff = box->rmax - box->rmin;
    const uint8_t gdiff = box->gmax - box->gmin;
    const uint8_t bdiff = box->bmax - box->bmin;
    uint8_t order[4] = {adiff, rdiff, gdiff, bdiff};
    uint8_t shift[4] = {24, 16, 8, 0};
    for (unsigned int i = 0; i < 3; i++) {
        unsigned int best = i;
        for (unsigned int j = i+1; j < 4; j++) {
            if (order[j] > order[best]) {
                best = j;
            }
        }
        if (best != i) {
            uint8_t temp;
            temp = order[i]; order[i] = order[best]; order[best] = temp;
            temp = shift[i]; shift[i] = shift[best]; shift[best] = temp;
        }
    }

    color_compare_order = shift[0] | shift[1]<<8 | shift[2]<<16 | shift[3]<<24;
    qsort(&colortable[box->first], box->ncolors, sizeof(*colortable),
          compare_colors);

    *newbox = *box;
    box->ncolors /= 2;
    newbox->first += box->ncolors;
    newbox->ncolors -= box->ncolors;
}

/*-----------------------------------------------------------------------*/

static int compare_box(const void * const a, const void * const b)
{
    const uint32_t count1 = ((const struct colorbox *)a)->ncolors;
    const uint32_t count2 = ((const struct colorbox *)b)->ncolors;
    return count1 > count2 ? -1 : count1 < count2 ? +1 : 0;  //Descending sort.
}

/*-----------------------------------------------------------------------*/

static uint32_t generate_colortable(
    const uint32_t *imageptr, uint32_t width, uint32_t height, uint32_t stride,
    const uint32_t *palette, unsigned int fixed_colors, void (*callback)(void))
{
    PRECOND(imageptr != NULL, return 0);
    PRECOND(colortable != NULL, return 0);
    PRECOND(palette != NULL, return 0);

    time_t last_callback = time(NULL);
    uint32_t total_pixels = 0;

    for (unsigned int y = 0; y < height; y++) {
        const uint32_t *ptr = &imageptr[y * stride];
        for (unsigned int x = 0; x < width; x++, total_pixels++) {
            uint32_t i;

            if (callback != NULL && total_pixels % 256 == 0
             && time(NULL) != last_callback) {
                (*callback)();
                last_callback = time(NULL);
            }

            /* Skip the pixel if it matches one of the preset colors. */
            for (i = 0; i < fixed_colors; i++) {
                if (ptr[x] == palette[i]) {
                    break;
                }
            }
            if (i < fixed_colors) {
                continue;
            }

            /* Count the color, and also bubble the entry up the table so
             * we can find it more quickly if it's a common color. */
            if (ptr[x] == colortable[0].color) {
                /* Already at the top of the table. */
                colortable[0].count++;
                continue;
            }
            for (i = 1; colortable[i].count != 0; i++) {
                if (ptr[x] == colortable[i].color) {
                    break;
                }
            }
            uint32_t save_color = colortable[i-1].color;
            uint32_t save_count = colortable[i-1].count;
            colortable[i-1].color = ptr[x];
            colortable[i-1].count = colortable[i].count + 1;
            colortable[i].color = save_color;
            colortable[i].count = save_count;
        }
    }

    unsigned int ncolors = 0;
    while (ncolors < width * height && colortable[ncolors].count != 0) {
        ncolors++;
    }
    return ncolors;
}

/*-----------------------------------------------------------------------*/

static inline uint32_t colordiff_sq(uint32_t color1, uint32_t color2)
{
#ifdef USE_SSE2
    uint32_t result;
    __asm__(
        "movd %[color1], %%xmm0\n"
        "movd %[color2], %%xmm1\n"
        "pxor %%xmm7, %%xmm7\n"
        "pcmpeqw %%xmm6, %%xmm6\n"
        "pxor %%xmm5, %%xmm5\n"
        "psubb %%xmm6, %%xmm5\n"
        "psrlw $8, %%xmm6\n"
        "psrlw $8, %%xmm5\n"              // XMM5 = {1,1,1,1,1,1,1,1}
        "pslldq $6, %%xmm6\n"             // XMM6 = {A=255, R=0, G=0, B=0}
        "punpcklbw %%xmm7, %%xmm0\n"      // XMM0 = {0,0,0,0,a1,r1,g1,b1}
        "punpcklbw %%xmm7, %%xmm1\n"      // XMM1 = {0,0,0,0,a2,r2,g2,b2}
        "pshuflw $0xFF, %%xmm0, %%xmm2\n" // XMM2 = {0,0,0,0,a1,a1,a1,a1}
        "pshuflw $0xFF, %%xmm1, %%xmm3\n" // XMM3 = {0,0,0,0,a2,a2,a2,a2}
        "por %%xmm6, %%xmm2\n"            // XMM2 = {0,0,0,0,255,a1,a1,a1}
        "por %%xmm6, %%xmm3\n"            // XMM3 = {0,0,0,0,255,a2,a2,a2}
        "psubw %%xmm1, %%xmm0\n"          // XMM0 = {0,0,0,0,Da,Dr,Dg,Db}
        "pmullw %%xmm3, %%xmm2\n"         // XMM2 = {0,0,0,0,255*255,a1*a2,...}
        "pmullw %%xmm0, %%xmm0\n"         // XMM0 = {0,0,0,0,Da*Da,Dr*Dr,...}
        "paddw %%xmm5, %%xmm2\n"          // XMM2 = {...,255*255+1,a1*a2+1,...}
        /* This is a little messy since there are no 16-bit unsigned
         * multiply or 32-bit-to-32-bit multiply instructions. */
        "punpcklwd %%xmm7, %%xmm2\n"      // XMM2 = {255*255+1,a1*a2+1,...}
        "punpcklwd %%xmm7, %%xmm0\n"      // XMM0 = {Da*Da,Dr*Dr,Dg*Dg,Db*Db}
        "movdqa %%xmm2, %%xmm3\n"         // XMM3 = {255*255,a1*a2,a1*a2,a1*a2}
        "movdqa %%xmm0, %%xmm1\n"         // XMM1 = {Da*Da,Dr*Dr,Dg*Dg,Db*Db}
        "psrldq $4, %%xmm3\n"             // XMM3 = {0,255*255,a1*a2,a1*a2}
        "psrldq $4, %%xmm1\n"             // XMM1 = {0,Da*Da,Dr*Dr,Dg*Dg}
        "pmuludq %%xmm2, %%xmm0\n"        // XMM0 = {0,Dr2,0,Db2}
        "pmuludq %%xmm3, %%xmm1\n"        // XMM1 = {0,Da2,0,Dg2}
        "psrld $2, %%xmm0\n"              // XMM0 = {0,Dr2/4,0,Db2/4}
        "psrld $2, %%xmm1\n"              // XMM1 = {0,Da2/4,0,Dg2/4}
        "paddd %%xmm1, %%xmm0\n"          // XMM0={0,Da2/4+Dr2/4,0,Db2/4+Dg2/4}
        "pshufd $0xAA, %%xmm0, %%xmm1\n"  // Add the last two and return.
        "paddd %%xmm1, %%xmm0\n"
        "movd %%xmm0, %[result]\n"
        : [result] "=r" (result)
        : [color1] "r" (color1), [color2] "r" (color2)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm5", "xmm6", "xmm7"
    );
    return result;
#else
    const uint32_t a1 = (color1>>24 & 0xFF);
    const uint32_t r1 = (color1>>16 & 0xFF);
    const uint32_t g1 = (color1>> 8 & 0xFF);
    const uint32_t b1 = (color1>> 0 & 0xFF);
    const uint32_t a2 = (color2>>24 & 0xFF);
    const uint32_t r2 = (color2>>16 & 0xFF);
    const uint32_t g2 = (color2>> 8 & 0xFF);
    const uint32_t b2 = (color2>> 0 & 0xFF);
    /* Add 1 to the alpha product multiplied with each color component's
     * difference, so we can tell colors apart even if they're transparent.
     * (The "color" of a transparent pixel is normally irrelevant, but
     * comes into play when interpolating with an adjacent non-transparent
     * pixel.) */
    return ((a2-a1)*(a2-a1) * (255*255+1)) / 4
         + ((r2-r1)*(r2-r1) * (a1*a2+1)) / 4
         + ((g2-g1)*(g2-g1) * (a1*a2+1)) / 4
         + ((b2-b1)*(b2-b1) * (a1*a2+1)) / 4;
#endif
}

/*************************************************************************/
/*************************************************************************/
