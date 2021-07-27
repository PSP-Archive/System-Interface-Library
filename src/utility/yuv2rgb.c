/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/yuv2rgb.c: YUV/RGB colorspace conversion functionality.
 */

/*
 * Performance data for x86_64 using an Intel Core i7 4770S: (times are in
 * millions of RDTSC ticks for a 640x480 video frame)
 *             | C version | SSE2 version
 * ------------+-----------+-------------
 *  Unsmoothed |    18.0   |     0.8
 *    Smoothed |    28.3   |     3.5
 *
 * Performance data for MIPS on a PSP running at 222 MHz:
 *    C version:        78285 used / 640x480 frame
 *    Assembly version: 58825 used / 640x480 frame
 */

#include "src/base.h"
#include "src/utility/yuv2rgb.h"

#ifdef SIL_ARCH_X86
# if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5
/* Work around GCC 5.x silently including <mm_malloc.h> which defines
 * inline functions which call malloc(). */
#  define _MM_MALLOC_H_INCLUDED
# endif
# include <emmintrin.h>
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/**
 * YUVConversionFunc:  Type of a function to convert a row of pixels from
 * YUV to RGBA format.
 *
 * [Parameters]
 *     srcY: Pointer to the start of the row in the Y plane of the source data.
 *     srcU: Pointer to the start of the row in the U plane of the source data.
 *     srcV: Pointer to the start of the row in the V plane of the source data.
 *     dest: Pointer to the start of the row in the destination buffer.
 *     width: Number of pixels to convert.
 */
typedef void YUVConversionFunc(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    uint8_t *dest, int width);

/**
 * YUVSmoothConversionFunc:  Type of a function to convert a row of pixels
 * from YUV to RGBA format using linear interpolation for U and V.
 *
 * [Parameters]
 *     srcY: Pointer to the start of the row in the Y plane of the source data.
 *     srcU: Pointer to the start of the row in the U plane of the source data.
 *     srcV: Pointer to the start of the row in the V plane of the source data.
 *     srcU2: Pointer to the start of the next nearest row in the U plane.
 *     srcV2: Pointer to the start of the next nearest row in the V plane.
 *     dest: Pointer to the start of the row in the destination buffer.
 *     width: Number of pixels to convert.
 */
typedef void YUVSmoothConversionFunc(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    const uint8_t *srcU2, const uint8_t *srcV2, uint8_t *dest, int width);


/* Conversion functions. */

static YUVConversionFunc convert_c;
static YUVSmoothConversionFunc convert_smooth_c;

#if defined(SIL_ARCH_MIPS_32) && defined(__GNUC__)
static YUVConversionFunc convert_mips32;
#endif

#ifdef SIL_ARCH_X86
static YUVConversionFunc convert_sse2;
static YUVSmoothConversionFunc convert_smooth_sse2;
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void yuv2rgb(const uint8_t **src, const int *src_stride,
             uint8_t *dest, const int dest_stride,
             const int width, const int height, int smooth_uv)
{
    /* Check that parameters are sane. */
    PRECOND(src != NULL, return);
    PRECOND(src[0] != NULL, return);
    PRECOND(src[1] != NULL, return);
    PRECOND(src[2] != NULL, return);
    PRECOND(src_stride != NULL, return);
    PRECOND(src_stride[0] > 0);
    PRECOND(src_stride[1] > 0);
    PRECOND(src_stride[2] > 0);
    PRECOND(dest != NULL, return);
    PRECOND(dest_stride > 0);
    PRECOND(width > 0);
    PRECOND(height > 0);

    /* Pick a conversion routine to use based on the availability of an
     * optimized routine for the current architecture and the alignment of
     * the parameter values. */
    YUVConversionFunc *convert = convert_c;
    YUVSmoothConversionFunc *convert_smooth = convert_smooth_c;
#if defined(SIL_ARCH_MIPS_32) && defined(__GNUC__)
    if (((intptr_t)dest & 3) == 0) {
        convert = convert_mips32;
    }
#endif
#ifdef SIL_ARCH_X86
    if (((intptr_t)src[0] & 15) == 0
     && src_stride[0] % 16 == 0
     && ((intptr_t)dest & 15) == 0
     && dest_stride % 4 == 0
     && width % 16 == 0) {
        convert = convert_sse2;
        convert_smooth = convert_smooth_sse2;
    }
#endif

    /* Run the conversion function on each line of the image. */
    int y;
    for (y = 0; y < height; y++, dest += dest_stride*4) {
        const uint8_t * const srcY = src[0] + y * src_stride[0];
        const uint8_t * const srcU = src[1] + (y/2) * src_stride[1];
        const uint8_t * const srcV = src[2] + (y/2) * src_stride[2];
        if (smooth_uv) {
            const uint8_t * const srcU2 =
                (y%2==0 ? (y==0 ? srcU : srcU - src_stride[1])
                        : (y==height-1 ? srcU : srcU + src_stride[1]));
            const uint8_t * const srcV2 =
                (y%2==0 ? (y==0 ? srcV : srcV - src_stride[2])
                        : (y==height-1 ? srcV : srcV + src_stride[2]));
            (*convert_smooth)(srcY, srcU, srcV, srcU2, srcV2, dest, width);
        } else {
            (*convert)(srcY, srcU, srcV, dest, width);
        }
    }
}

/*************************************************************************/
/******** Single-line conversion functions: Platform-agnostic C **********/
/*************************************************************************/

static void convert_c(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    uint8_t *dest, int width)
{
    int x;
    for (x = 0; x < width/2; x++) {
        int Y, U, V, R, G, B;
        Y = (srcY[x*2] - 0x10) * 9539;
        U = srcU[x] - 0x80;
        V = srcV[x] - 0x80;
        R = (Y           + 13075*V + (1<<12)) >> 13;
        G = (Y -  3209*U -  6660*V + (1<<12)) >> 13;
        B = (Y + 16525*U           + (1<<12)) >> 13;
        dest[x*8+0] = bound(R, 0, 255);
        dest[x*8+1] = bound(G, 0, 255);
        dest[x*8+2] = bound(B, 0, 255);
        dest[x*8+3] = 0xFF;
        Y = (srcY[x*2+1] - 0x10) * 9539;
        R = (Y           + 13075*V + (1<<12)) >> 13;
        G = (Y -  3209*U -  6660*V + (1<<12)) >> 13;
        B = (Y + 16525*U           + (1<<12)) >> 13;
        dest[x*8+4] = bound(R, 0, 255);
        dest[x*8+5] = bound(G, 0, 255);
        dest[x*8+6] = bound(B, 0, 255);
        dest[x*8+7] = 0xFF;
    }
}

/*-----------------------------------------------------------------------*/

static void convert_smooth_c(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    const uint8_t *srcU2, const uint8_t *srcV2, uint8_t *dest, int width)
{
    int x;
    for (x = 0; x < width/2; x++) {
        int Y, U, V, R, G, B;
        Y = (srcY[x*2] - 0x10) * 9539;
        U = srcU[x];
        V = srcV[x];
        int thisU = U;
        int thisU2 = srcU2[x];
        U = (thisU*3 + thisU2 + 2) / 4;
        int thisV = V;
        int thisV2 = srcV2[x];
        V = (thisV*3 + thisV2 + 2) / 4;
        U -= 0x80;
        V -= 0x80;
        R = (Y           + 13075*V + (1<<12)) >> 13;
        G = (Y -  3209*U -  6660*V + (1<<12)) >> 13;
        B = (Y + 16525*U           + (1<<12)) >> 13;
        dest[x*8+0] = bound(R, 0, 255);
        dest[x*8+1] = bound(G, 0, 255);
        dest[x*8+2] = bound(B, 0, 255);
        dest[x*8+3] = 0xFF;
        Y = (srcY[x*2+1] - 0x10) * 9539;
        int nextU = x==width/2-1 ? thisU : srcU[x+1];
        int nextU2 = x==width/2-1 ? thisU2 : srcU2[x+1];
        U = (thisU*3 + nextU*3 + thisU2 + nextU2 + 4) / 8;
        int nextV = x==width/2-1 ? thisV : srcV[x+1];
        int nextV2 = x==width/2-1 ? thisV2 : srcV2[x+1];
        V = (thisV*3 + nextV*3 + thisV2 + nextV2 + 4) / 8;
        U -= 0x80;
        V -= 0x80;
        R = (Y           + 13075*V + (1<<12)) >> 13;
        G = (Y -  3209*U -  6660*V + (1<<12)) >> 13;
        B = (Y + 16525*U           + (1<<12)) >> 13;
        dest[x*8+4] = bound(R, 0, 255);
        dest[x*8+5] = bound(G, 0, 255);
        dest[x*8+6] = bound(B, 0, 255);
        dest[x*8+7] = 0xFF;
    }
}

/*************************************************************************/
/********** Single-line conversion functions: Vectorized MIPS ************/
/*************************************************************************/

#if defined(SIL_ARCH_MIPS_32) && defined(__GNUC__)

static void convert_mips32(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    uint8_t *dest, int width)
{
    /* Register usage:
     *    $at: Temporary
     *    $v0: RGBA output pixel
     *    $t0: Y pixel
     *    $t1: U pixel (raw, then scaled for blue)
     *    $t2: V pixel scaled for red
     *    $t3: U+V pixels scaled for green
     *    $t4: V pixel (raw), then temporary
     *    $t5: Y constant
     *    $t6: V constant for red
     *    $t7: U constant for green
     *    $t8: V constant for green
     *    $t9: U constant for blue
     *    $s0: Second Y pixel
     *    $s1: Temporary (non-PSP only)
     *    $s2: Temporary (non-PSP only)
     *    $s3: 255 (for bounding pixel values and setting output alpha value)
     */

    /* MINMAX:  Bound the value in $at to the range [0,255].  Uses $s1 and
     * $s2 as temporaries on platforms without the MIN and MAX instructions. */
    #ifdef SIL_PLATFORM_PSP
        #define MINMAX \
            "min $at, $at, $s3\n"   \
            "max $at, $at, $zero\n"
    #else
       #define MINMAX \
            "slt $s1, $at, $s3\n"   \
            "slt $s2, $at, $zero\n" \
            "movz $at, $s3, $s1\n"  \
            "movn $at, $zero, $s2\n"
    #endif

    #if defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP)
        #define INS(n) \
            "ins $v0, $at, "#n", 8\n"
        #define INS_0 \
            "ins $v0, $at, 0, 8\n"
        #define CLEAR  /*nothing*/
    #else
        #define INS(n) \
            "sll $at, $at, "#n"\n" \
            "or $v0, $v0, $at\n"
        #define INS_0 \
            "or $v0, $v0, $at\n"
        #define CLEAR \
            "lui $v0, 0xFF00\n"
    #endif

    __asm__ volatile(
        /* Indentation here indicates separate calculation sequences which
         * are interleaved to improve instruction throughput. */
        ".set push; .set noreorder; .set noat   \n"
        "li $t5, 9539                           \n"
        "li $t6, 13075                          \n"
        "li $t7, -3209                          \n"
        "li $t8, -6660                          \n"
        "li $t9, 16525                          \n"
        "li $s3, 255                            \n"
        "lui $v0, 0xFF00                        \n"
        "0:                                     \n"
        "# Load, unbias, and scale              \n"
        "lbu $t4, 0(%[srcV])                    \n"
        "lbu $t0, 0(%[srcY])                    \n"
        "lbu $t1, 0(%[srcU])                    \n"
        "addiu $t4, $t4, -128                   \n"
        "mult $t4, $t6                          \n"
        "addiu %[srcU], %[srcU], 1              \n"
        "addiu %[srcV], %[srcV], 1              \n"
        "mflo $t2                               \n"
        "addiu $t0, $t0, -16                    \n"
        "lbu $s0, 1(%[srcY])                    \n"
        "mult $t0, $t5                          \n"
        "addiu %[width], %[width], -2           \n"
        "addiu %[srcY], %[srcY], 2              \n"
        "mflo $t0                               \n"
        "addiu $t1, $t1, -128                   \n"
        "        addu $at, $t0, $t2             \n"
        "mult $t1, $t7                          \n"
        "mflo $t3                               \n"
        "        addiu $at, $at, 4096           \n"
        "        sra $at, $at, 13               \n"
        "mult $t1, $t9                          \n"
        "        "MINMAX"                       \n"
        "        "CLEAR"                        \n"
        "        "INS_0"                        \n"
        "        or $v0, $v0, $at               \n"
        "mflo $t1                               \n"
        "        addu $at, $t0, $t1             \n"
        "        addiu $at, $at, 4096           \n"
        "mult $t4, $t8                          \n"
        "        sra $at, $at, 13               \n"
        "        "MINMAX"                       \n"
        "        "INS(16)"                      \n"
        "mflo $t4                               \n"
        "addu $t3, $t3, $t4                     \n"
        "addiu $s0, $s0, -16                    \n"
        "mult $s0, $t5                          \n"
        "        addu $at, $t0, $t3             \n"
        "        addiu $at, $at, 4096           \n"
        "        sra $at, $at, 13               \n"
        "        "MINMAX"                       \n"
        "        "INS(8)"                       \n"
        "        sw $v0, 0(%[dest])             \n"
        "                # Second output pixel  \n"
        "mflo $s0                               \n"
        "                addu $at, $s0, $t2     \n"
        "                addiu $at, $at, 4096   \n"
        "                sra $at, $at, 13       \n"
        "                "MINMAX"               \n"
        "                "CLEAR"                \n"
        "                "INS_0"                \n"
        "                addu $at, $s0, $t1     \n"
        "                addiu $at, $at, 4096   \n"
        "                sra $at, $at, 13       \n"
        "                "MINMAX"               \n"
        "                "INS(16)"              \n"
        "                addu $at, $s0, $t3     \n"
        "                addiu $at, $at, 4096   \n"
        "                sra $at, $at, 13       \n"
        "                "MINMAX"               \n"
        "                "INS(8)"               \n"
        "                sw $v0, 4(%[dest])     \n"
        "# Loop over remaining pixels           \n"
        "bnez %[width], 0b                      \n"
        "addiu %[dest], %[dest], 8              \n"
        ".set pop                               \n"
        : /* no outputs */
        : [srcY] "r" (srcY), [srcU] "r" (srcU), [srcV] "r" (srcV),
          [dest] "r" (dest), [width] "r" (width)
        : "at", "v0", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8",
          "t9", "s0",
#ifndef SIL_PLATFORM_PSP
          "s1", "s2",
#endif
          "s3"
    );
}

#endif  // SIL_ARCH_MIPS_32 && __GNUC__

/*************************************************************************/
/*********** Single-line conversion functions: Vectorized x86 ************/
/*************************************************************************/

#ifdef SIL_ARCH_X86

/* Convenience macro for declaring an 8x16bit constant value. */
#define EPI16_C(n)  _mm_set1_epi16(n)

/* Load/store wrappers to avoid needing to cast pointers between uint8_t
 * and __m128i (and the cast-align warnings that result). */
static inline __m128i mm_load_si128(const void *ptr) {
    return _mm_load_si128((const __m128i *)(uintptr_t)ptr);
}
static inline __m128i mm_loadl_epi64(const void *ptr) {
    return _mm_loadl_epi64((const __m128i *)(uintptr_t)ptr);
}
static inline void mm_store_si128(void *ptr, __m128i val) {
    _mm_store_si128((__m128i *)(uintptr_t)ptr, val);
}

/*-----------------------------------------------------------------------*/

static void convert_sse2(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    uint8_t *dest, int width)
{
    for (int i = width/2 - 8; i >= 0; i -= 8) {
        /* Load data, expand to 16 bits, unbias, and convert to 8.7 fixed
         * point. */
        const __m128i raw_Y = mm_load_si128(srcY + 2*i);
        const __m128i raw_U = mm_loadl_epi64(srcU + i);
        const __m128i raw_V = mm_loadl_epi64(srcV + i);
        __m128i Y_even = _mm_and_si128(raw_Y, EPI16_C(0x00FF));
        __m128i Y_odd = _mm_srli_epi16(raw_Y, 8);
        const __m128i U = _mm_unpacklo_epi8(raw_U, EPI16_C(0));
        const __m128i V = _mm_unpacklo_epi8(raw_V, EPI16_C(0));
        Y_even = _mm_slli_epi16(_mm_sub_epi16(Y_even, EPI16_C(16)), 7);
        Y_odd = _mm_slli_epi16(_mm_sub_epi16(Y_odd, EPI16_C(16)), 7);
        const __m128i U_even =
            _mm_slli_epi16(_mm_sub_epi16(U, EPI16_C(128)), 7);
        const __m128i U_odd = U_even;
        const __m128i V_even =
            _mm_slli_epi16(_mm_sub_epi16(V, EPI16_C(128)), 7);
        const __m128i V_odd = V_even;

        /* Multiply by constants. */
        const __m128i mult_Y = EPI16_C(9539);    // Common factor for Y
        const __m128i mult_rV = EPI16_C(13075);  // Red factor for V
        const __m128i mult_gU = EPI16_C(-3209);  // Green factor for U
        const __m128i mult_gV = EPI16_C(-6660);  // Green factor for V
        const __m128i mult_bU = EPI16_C(16525);  // Blue factor for U
        const __m128i cY_even = _mm_mulhi_epi16(Y_even, mult_Y);
        const __m128i gU_even = _mm_mulhi_epi16(U_even, mult_gU);
        const __m128i gV_even = _mm_mulhi_epi16(V_even, mult_gV);
        const __m128i cY_odd = _mm_mulhi_epi16(Y_odd, mult_Y);
        const __m128i gU_odd = _mm_mulhi_epi16(U_odd, mult_gU);
        const __m128i gV_odd = _mm_mulhi_epi16(V_odd, mult_gV);
        /* Intermediate red/green/blue sums. */
        const __m128i r_even = _mm_mulhi_epi16(V_even, mult_rV);
        const __m128i g_even = _mm_add_epi16(gU_even, gV_even);
        const __m128i b_even = _mm_mulhi_epi16(U_even, mult_bU);
        const __m128i r_odd = _mm_mulhi_epi16(V_odd, mult_rV);
        const __m128i g_odd = _mm_add_epi16(gU_odd, gV_odd);
        const __m128i b_odd = _mm_mulhi_epi16(U_odd, mult_bU);

        /* Add intermediate results and round/shift to get R/G/B values. */
        const __m128i rcY_even = _mm_add_epi16(cY_even, EPI16_C(8));  // cY+0.5
        const __m128i rcY_odd = _mm_add_epi16(cY_odd, EPI16_C(8));
        __m128i R_even = _mm_srai_epi16(_mm_add_epi16(r_even, rcY_even), 4);
        __m128i G_even = _mm_srai_epi16(_mm_add_epi16(g_even, rcY_even), 4);
        __m128i B_even = _mm_srai_epi16(_mm_add_epi16(b_even, rcY_even), 4);
        __m128i R_odd = _mm_srai_epi16(_mm_add_epi16(r_odd, rcY_odd), 4);
        __m128i G_odd = _mm_srai_epi16(_mm_add_epi16(g_odd, rcY_odd), 4);
        __m128i B_odd = _mm_srai_epi16(_mm_add_epi16(b_odd, rcY_odd), 4);

        /* Saturate to 0-255, pack into bytes, interleave, and store. */
        R_even = _mm_packus_epi16(R_even, R_even);
        G_even = _mm_packus_epi16(G_even, G_even);
        B_even = _mm_packus_epi16(B_even, B_even);
        R_odd = _mm_packus_epi16(R_odd, R_odd);
        G_odd = _mm_packus_epi16(G_odd, G_odd);
        B_odd = _mm_packus_epi16(B_odd, B_odd);
        const __m128i R = _mm_unpacklo_epi8(R_even, R_odd);
        const __m128i G = _mm_unpacklo_epi8(G_even, G_odd);
        const __m128i B = _mm_unpacklo_epi8(B_even, B_odd);
        const __m128i A = EPI16_C(-1);  // Set all alpha bytes to 0xFF.
        const __m128i RG_0 = _mm_unpacklo_epi8(R, G);
        const __m128i RG_8 = _mm_unpackhi_epi8(R, G);
        const __m128i BA_0 = _mm_unpacklo_epi8(B, A);
        const __m128i BA_8 = _mm_unpackhi_epi8(B, A);
        const __m128i RGBA_0 = _mm_unpacklo_epi16(RG_0, BA_0);
        const __m128i RGBA_4 = _mm_unpackhi_epi16(RG_0, BA_0);
        const __m128i RGBA_8 = _mm_unpacklo_epi16(RG_8, BA_8);
        const __m128i RGBA_12 = _mm_unpackhi_epi16(RG_8, BA_8);
        mm_store_si128(dest + 8*i, RGBA_0);
        mm_store_si128(dest + 8*i + 16, RGBA_4);
        mm_store_si128(dest + 8*i + 32, RGBA_8);
        mm_store_si128(dest + 8*i + 48, RGBA_12);
    }
}

/*-----------------------------------------------------------------------*/

static void convert_smooth_sse2(
    const uint8_t *srcY, const uint8_t *srcU, const uint8_t *srcV,
    const uint8_t *srcU2, const uint8_t *srcV2, uint8_t *dest, int width)
{
    int lastU = srcU[width/2-1];
    int lastV = srcV[width/2-1];
    int lastU2 = srcU2[width/2-1];
    int lastV2 = srcV2[width/2-1];

    for (int i = width/2 - 8; i >= 0; i -= 8) {
        /* Load data and expand to 16 bits.  Y values are unbiased and
         * shifted to 8.7 fixed point, while U and V are left as raw
         * (biased) 8-bit values in 16-bit slots for smoothing. */
        const __m128i raw_Y = mm_load_si128(srcY + 2*i);
        const __m128i raw_U = mm_loadl_epi64(srcU + i);
        const __m128i raw_V = mm_loadl_epi64(srcV + i);
        __m128i Y_even = _mm_and_si128(raw_Y, EPI16_C(0x00FF));
        __m128i Y_odd = _mm_srli_epi16(raw_Y, 8);
        const __m128i U = _mm_unpacklo_epi8(raw_U, EPI16_C(0));
        const __m128i V = _mm_unpacklo_epi8(raw_V, EPI16_C(0));
        Y_even = _mm_slli_epi16(_mm_sub_epi16(Y_even, EPI16_C(16)), 7);
        Y_odd = _mm_slli_epi16(_mm_sub_epi16(Y_odd, EPI16_C(16)), 7);

        /* Smooth U and V values. */

        /* Load U and V vectors shifted one pixel right.  (We convert from
         * right to left, so the extra pixel is the "last" pixel.) */
        const __m128i U_this = U;
        const __m128i U_last = _mm_insert_epi16(_mm_srli_si128(U_this, 2),
                                                lastU, 7);
        lastU = _mm_extract_epi16(U_this, 0);
        const __m128i U2_this = _mm_unpacklo_epi8(
            mm_loadl_epi64(srcU2 + i), EPI16_C(0));
        const __m128i U2_last = _mm_insert_epi16(_mm_srli_si128(U2_this, 2),
                                                lastU2, 7);
        lastU2 = _mm_extract_epi16(U2_this, 0);
        const __m128i V_this = V;
        const __m128i V_last = _mm_insert_epi16(_mm_srli_si128(V_this, 2),
                                                lastV, 7);
        lastV = _mm_extract_epi16(V_this, 0);
        const __m128i V2_this = _mm_unpacklo_epi8(
            mm_loadl_epi64(srcV2 + i), EPI16_C(0));
        const __m128i V2_last = _mm_insert_epi16(_mm_srli_si128(V2_this, 2),
                                                lastV2, 7);
        lastV2 = _mm_extract_epi16(V2_this, 0);

        /* U_even <- (U[0:7] * 3 + U2[0:7]) / 4 */
        __m128i U_even = _mm_add_epi16(
            _mm_mullo_epi16(U_this, EPI16_C(3)), U2_this);

        /* U_odd <- ((U[0:7]+U[1:8]) * 3 + (U2[0:7]+U2[1:8])) / 8 */
        __m128i U_odd = _mm_add_epi16(
            _mm_mullo_epi16(_mm_add_epi16(U_this, U_last), EPI16_C(3)),
            _mm_add_epi16(U2_this, U2_last));

        /* V_even <- (V[0:7] * 3 + V2[0:7]) / 4 */
        __m128i V_even = _mm_add_epi16(
            _mm_mullo_epi16(V_this, EPI16_C(3)), V2_this);

        /* V_odd <- ((V[0:7]+V[1:8]) * 3 + (V2[0:7]+V2[1:8])) / 8 */
        __m128i V_odd = _mm_add_epi16(
            _mm_mullo_epi16(_mm_add_epi16(V_this, V_last), EPI16_C(3)),
            _mm_add_epi16(V2_this, V2_last));

        /* Unbias and convert to 8.7 fixed point. */
        U_even = _mm_slli_epi16(_mm_sub_epi16(U_even, EPI16_C(0x80<<2)), 5);
        U_odd = _mm_slli_epi16(_mm_sub_epi16(U_odd, EPI16_C(0x80<<3)), 4);
        V_even = _mm_slli_epi16(_mm_sub_epi16(V_even, EPI16_C(0x80<<2)), 5);
        V_odd = _mm_slli_epi16(_mm_sub_epi16(V_odd, EPI16_C(0x80<<3)), 4);

        /* End of U/V smoothing. */

        /* Multiply by constants. */
        const __m128i mult_Y = EPI16_C(9539);    // Common factor for Y
        const __m128i mult_rV = EPI16_C(13075);  // Red factor for V
        const __m128i mult_gU = EPI16_C(-3209);  // Green factor for U
        const __m128i mult_gV = EPI16_C(-6660);  // Green factor for V
        const __m128i mult_bU = EPI16_C(16525);  // Blue factor for U
        const __m128i cY_even = _mm_mulhi_epi16(Y_even, mult_Y);
        const __m128i gU_even = _mm_mulhi_epi16(U_even, mult_gU);
        const __m128i gV_even = _mm_mulhi_epi16(V_even, mult_gV);
        const __m128i cY_odd = _mm_mulhi_epi16(Y_odd, mult_Y);
        const __m128i gU_odd = _mm_mulhi_epi16(U_odd, mult_gU);
        const __m128i gV_odd = _mm_mulhi_epi16(V_odd, mult_gV);
        /* Intermediate red/green/blue sums. */
        const __m128i r_even = _mm_mulhi_epi16(V_even, mult_rV);
        const __m128i g_even = _mm_add_epi16(gU_even, gV_even);
        const __m128i b_even = _mm_mulhi_epi16(U_even, mult_bU);
        const __m128i r_odd = _mm_mulhi_epi16(V_odd, mult_rV);
        const __m128i g_odd = _mm_add_epi16(gU_odd, gV_odd);
        const __m128i b_odd = _mm_mulhi_epi16(U_odd, mult_bU);

        /* Add intermediate results and round/shift to get R/G/B values. */
        const __m128i rcY_even = _mm_add_epi16(cY_even, EPI16_C(8));  // cY+0.5
        const __m128i rcY_odd = _mm_add_epi16(cY_odd, EPI16_C(8));
        __m128i R_even = _mm_srai_epi16(_mm_add_epi16(r_even, rcY_even), 4);
        __m128i G_even = _mm_srai_epi16(_mm_add_epi16(g_even, rcY_even), 4);
        __m128i B_even = _mm_srai_epi16(_mm_add_epi16(b_even, rcY_even), 4);
        __m128i R_odd = _mm_srai_epi16(_mm_add_epi16(r_odd, rcY_odd), 4);
        __m128i G_odd = _mm_srai_epi16(_mm_add_epi16(g_odd, rcY_odd), 4);
        __m128i B_odd = _mm_srai_epi16(_mm_add_epi16(b_odd, rcY_odd), 4);

        /* Saturate to 0-255, pack into bytes, interleave, and store. */
        R_even = _mm_packus_epi16(R_even, R_even);
        G_even = _mm_packus_epi16(G_even, G_even);
        B_even = _mm_packus_epi16(B_even, B_even);
        R_odd = _mm_packus_epi16(R_odd, R_odd);
        G_odd = _mm_packus_epi16(G_odd, G_odd);
        B_odd = _mm_packus_epi16(B_odd, B_odd);
        const __m128i R = _mm_unpacklo_epi8(R_even, R_odd);
        const __m128i G = _mm_unpacklo_epi8(G_even, G_odd);
        const __m128i B = _mm_unpacklo_epi8(B_even, B_odd);
        const __m128i A = EPI16_C(-1);  // Set all alpha bytes to 0xFF.
        const __m128i RG_0 = _mm_unpacklo_epi8(R, G);
        const __m128i RG_8 = _mm_unpackhi_epi8(R, G);
        const __m128i BA_0 = _mm_unpacklo_epi8(B, A);
        const __m128i BA_8 = _mm_unpackhi_epi8(B, A);
        const __m128i RGBA_0 = _mm_unpacklo_epi16(RG_0, BA_0);
        const __m128i RGBA_4 = _mm_unpackhi_epi16(RG_0, BA_0);
        const __m128i RGBA_8 = _mm_unpacklo_epi16(RG_8, BA_8);
        const __m128i RGBA_12 = _mm_unpackhi_epi16(RG_8, BA_8);
        mm_store_si128(dest + 8*i, RGBA_0);
        mm_store_si128(dest + 8*i + 16, RGBA_4);
        mm_store_si128(dest + 8*i + 32, RGBA_8);
        mm_store_si128(dest + 8*i + 48, RGBA_12);
    }
}

/*-----------------------------------------------------------------------*/

#undef EPI16_C

#endif  // SIL_ARCH_X86

/*************************************************************************/
/*************************************************************************/
