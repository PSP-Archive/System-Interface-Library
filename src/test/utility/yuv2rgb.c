/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/yuv2rgb.c: Tests for YUV->RGB conversion.
 */

#include "src/base.h"
#include "src/test/base.h"
#include "src/utility/yuv2rgb.h"

#if defined(SIL_ARCH_X86) || defined(SIL_PLATFORM_PSP)
# include "src/memory.h"
#endif

#ifdef SIL_PLATFORM_PSP
# include "src/sysdep.h"               // Needed by src/sysdep/psp/thread.h.
# include "src/sysdep/psp/internal.h"  // For syscall declarations.
# include "src/sysdep/psp/thread.h"    // For psp_threads_{lock,unlock}().
#endif

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_yuv2rgb)

/*-----------------------------------------------------------------------*/

TEST(test_Y)
{
    static const uint8_t Y[4] = {83,83,83,83};
    static const uint8_t U[1] = {128};
    static const uint8_t V[1] = {128};
    const int grey = 78;  // 78.016 => 78 for both truncation and rounding

    /* For these tests, we want to test the base C implementation, so we use
     * an unaligned destination buffer because that fails the conditions for
     * all assembly routines. */
    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;

    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_PIXEL(&rgb[i*4], grey,grey,grey,255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_Y_multiple)
{
    static const uint8_t Y[4] = {83,86,89,92};
    static const uint8_t U[1] = {128};
    static const uint8_t V[1] = {128};
    static const uint8_t grey[4] = {78,82,85,88};

    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_PIXEL(&rgb[i*4], grey[i],grey[i],grey[i],255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rounding)
{
    static const uint8_t Y[4] = {86,86,86,86};
    static const uint8_t U[1] = {128};
    static const uint8_t V[1] = {128};
    const int grey = 82;  // 81.510 => should round up to 82

    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_PIXEL(&rgb[i*4], grey,grey,grey,255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bound)
{
    static const uint8_t Y[4] = {1,1,254,254};
    static const uint8_t U[1] = {128};
    static const uint8_t V[1] = {128};

    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        const int grey = (i < 2) ? 0 : 255;
        CHECK_PIXEL(&rgb[i*4], grey,grey,grey,255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_U)
{
    static const uint8_t Y[4] = {86,86,86,86};
    static const uint8_t U[1] = {64};
    static const uint8_t V[1] = {128};

    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_PIXEL(&rgb[i*4], 82,107,0,255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_V)
{
    static const uint8_t Y[4] = {86,86,86,86};
    static const uint8_t U[1] = {128};
    static const uint8_t V[1] = {64};

    uint8_t rgb_base[1+4*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){2,1,1}, rgb, 2, 2, 2, 0);
    for (int i = 0; i < 4; i++) {
        CHECK_PIXEL(&rgb[i*4], 0,134,82,255, i%2, i/2);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_UV_multiple)
{
    static const uint8_t Y[16] =
        {95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95};
    static const uint8_t U[4] = {80,112,192,224};
    static const uint8_t V[4] = {160,128,64,32};
    static const uint8_t expect[16*3] = {
        143, 85,  0, 143, 85,  0,  92, 98, 60,  92, 98, 60,
        143, 85,  0, 143, 85,  0,  92, 98, 60,  92, 98, 60,
          0,119,221,   0,119,221,   0,132,255,   0,132,255,
          0,119,221,   0,119,221,   0,132,255,   0,132,255
    };

    uint8_t rgb_base[1+16*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){4,2,2}, rgb, 4, 4, 4, 0);
    for (int i = 0; i < 16; i++) {
        CHECK_PIXEL(&rgb[i*4],
                    expect[i*3+0], expect[i*3+1], expect[i*3+2], 255,
                    i%4, i/4);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_UV_multiple_smooth)
{
    static const uint8_t Y[16] =
        {95,95,95,95,95,95,95,95,95,95,95,95,95,95,95,95};
    static const uint8_t U[4] = {80,112,192,224};
    static const uint8_t V[4] = {160,128,64,32};
    static const uint8_t expect[16*3] = {
        143, 85,  0, 118, 92, 27,  92, 98, 60,  92, 98, 60,
        105, 93, 52,  79,100, 84,  54,107,116,  54,107,116,
         28,110,165,   3,117,197,   0,124,229,   0,124,229,
          0,119,221,   0,126,253,   0,132,255,   0,132,255
    };

    uint8_t rgb_base[1+16*4];
    uint8_t *rgb = rgb_base + 1;
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){4,2,2}, rgb, 4, 4, 4, 1);
    for (int i = 0; i < 16; i++) {
        CHECK_PIXEL(&rgb[i*4],
                    expect[i*3+0], expect[i*3+1], expect[i*3+2], 255,
                    i%4, i/4);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_ARCH_X86

TEST(test_asm_sse2)
{
    static const ALIGNED(16) uint8_t Y[64] =
        {50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
         52,62,72,82,92,102,112,122,132,142,152,162,172,182,192,202,
         54,64,74,84,94,104,114,124,134,144,154,164,174,184,194,204,
         56,66,76,86,96,106,116,126,136,146,156,166,176,186,196,206};
    /* These two are deliberately misaligned (the first byte is ignored)
     * to verify that alignment of this data is not required. */
    static const ALIGNED(8) uint8_t U[1+16] =
        {0,  8, 40, 72,104,136,168,200,232,
            20, 52, 84,116,148,180,212,244};
    static const ALIGNED(8) uint8_t V[1+16] =
        {0,248,216,184,152,120, 88, 56, 24,
           228,196,164,132,100, 68, 36,  4};

    /* Note that several values here are off by 1 from what the C routine
     * produces (see test_asm_sse2_conditions() below) due to reduced
     * precision in the SSE2 routine causing different rounding.  These
     * are marked with comments. */
    static const uint8_t expect[64*3] = {
        231,  0,  0, 243,  1,  0, 203, 26,  0, 215, 37,  0,
        176, 62, // Rounding: 63->62
                  0, 187, 74,  0, 148, 99, 61, 159,111, 73,
        120,136,149, 132,148,161,  92,173,237, 104,184, // Rounding: 185->184
                                                       248,
         64,210,255,  76,221,255,  37,246,255,  48,255,255,

        233,  0,  0, 245,  3,  0, 206, 28,  0, 217, 40,  0,
        178, 65,  0, 190, 76, // Rounding: 77->76
                               0, 150,102, 63, 162,113, 75,
        122,138,151, 134,150,163,  94, // Rounding: 95->94
                                      175,239, 106,187,251,
         67,212,255,  78,224,255,  39,249,255,  51,255,255,

        204,  5,  0, 215, 17,  0, 176, 42,  0, 188, 54,  0,
        148, 79,  2, 160, 90, 14, 120,115, // Rounding: 116->115
                                           90, 132,127,102,
         93,152,178, 104,164,189,  65,189,255,  77,201,255,
         37,226,255,  49,237, // Rounding: 238->237
                             255,   9,255,255,  21,255,255,

        206,  8,  0, 218, 19,  0, 178, 44,  0, 190, 56,  0,
        151, 81,  4, 162, 93, 16, 123,118, 92, 134,129, // Rounding: 130->129
                                                       104,
         95,155,180, 107,166,192,  67,191,255,  79,203,255,
         39,228,255,  51,240,255,  12,255,255,  23,255,255,
    };

    uint8_t rgb_base[64*4+15];
    uint8_t *rgb = (uint8_t *)align_up((uintptr_t)rgb_base, 16);
    yuv2rgb((const uint8_t *[]){Y,U+1,V+1}, (const int[]){16,8,8}, rgb, 16,
            16, 4, 0);
    for (int i = 0; i < 64; i++) {
        CHECK_PIXEL(&rgb[i*4],
                    expect[i*3+0], expect[i*3+1], expect[i*3+2], 255,
                    i%16, i/16);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_asm_sse2_smooth)
{
    static const ALIGNED(16) uint8_t Y[64] =
        {50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
         52,62,72,82,92,102,112,122,132,142,152,162,172,182,192,202,
         54,64,74,84,94,104,114,124,134,144,154,164,174,184,194,204,
         56,66,76,86,96,106,116,126,136,146,156,166,176,186,196,206};
    /* Again, these are deliberately misaligned. */
    static const ALIGNED(8) uint8_t U[1+16] =
        {0,  8, 40, 72,104,136,168,200,232,
            20, 52, 84,116,148,180,212,244};
    static const ALIGNED(8) uint8_t V[1+16] =
        {0,248,216,184,152,120, 88, 56, 24,
           228,196,164,132,100, 68, 36,  4};

    static const uint8_t expect[64*3] = {
        231,  0,  0, 217,  7,  0, 203, 26,  0, 189, 44,  0,
        176, 62, // Rounding: 63->62
                  0, 162, 81, 17, 148, 99, 61, 134,118,105,
        120,136,149, 106,154,193,  92,173,237,  78,191,255,
         64,210,255,  50, // Rounding: 51->50
                         228,255,  37,246,255,  48,255,255,

        225,  0,  0, 212, 13,  0, 198, 31,  0, 184, 49,  0,
        170, 68,  0, 156, 86, 25, // Rounding: 26->25
                                  142,104, // Rounding: 105->104
                                           69, 128,123,113,
        114,141,157, 100,160,201,  86, // Rounding: 87->86
                                      178,245,  73,196,255,
         59,215,255,  45,233,255,  31,252,255,  43,255,255,

        212,  2,  0, 198, 21,  0, 184, 39,  0, 170, 57, // Rounding: 58->57
                                                         0,
        156, 76,  0, 142, 94, 40, 128,113, 84, 115,131,128,
        101,149,172,  87,168,216,  73,186,255,  59,204, // Rounding: 205->204
                                                       255,
         45,223,255,  31,241,255,  17,255,255,  29,255,255,

        206,  8,  0, 192, 26,  0, 178, 44,  0, 164, // Rounding: 165->164
                                                    63,  0,
        151, 81,  4, 137, 99, // Rounding: 100->99
                              48, 123,118, 92, 109,136,136,
         95,155,180,  81,173,224,  67,191,255,  53,210,255,
         39,228,255,  26,246, // Rounding: 247->246
                             255,  12,255,255,  23,255,255,
    };

    uint8_t rgb_base[64*4+15];
    uint8_t *rgb = (uint8_t *)align_up((uintptr_t)rgb_base, 16);
    yuv2rgb((const uint8_t *[]){Y,U+1,V+1}, (const int[]){16,8,8}, rgb, 16,
            16, 4, 1);
    for (int i = 0; i < 32; i++) {
        CHECK_PIXEL(&rgb[i*4],
                    expect[i*3+0], expect[i*3+1], expect[i*3+2], 255,
                    i%16, i/16);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_asm_sse2_conditions)
{
    static const ALIGNED(16) uint8_t Y[32+33+32] =
        {50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
         55,65,75,85,95,105,115,125,135,145,155,165,175,185,195,205,
         50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
         0,
         55,65,75,85,95,105,115,125,135,145,155,165,175,185,195,205,
         50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
         55,65,75,85,95,105,115,125,135,145,155,165,175,185,195,205};
    static const ALIGNED(8) uint8_t U[1+8+1] =
        {0,  8, 40, 72,104,136,168,200,232,232};
    static const ALIGNED(8) uint8_t V[1+8+1] =
        {0,248,216,184,152,120, 88, 56, 24, 24};
    static const uint8_t expect[32*3] = {
        231,  0,  0, 243,  1,  0, 203, 26,  0, 215, 37,  0,
        176, 63,  0, 187, 74,  0, 148, 99, 61, 159,111, 73,
        120,136,149, 132,148,161,  92,173,237, 104,185,248,
         64,210,255,  76,221,255,  37,246,255,  48,255,255,
        237,  0,  0, 249,  7,  0, 209, 32,  0, 221, 43,  0,
        181, 68,  0, 193, 80,  0, 154,105, 67, 165,117, 79,
        126,142,155, 137,154,166,  98,179,243, 110,190,254,
         70,215,255,  82,227,255,  42,252,255,  54,255,255,
    };

    uint8_t rgb_base[(32*4+4)+15];
    uint8_t *rgb = (uint8_t *)align_up((uintptr_t)rgb_base, 16);

    /* We fail each condition for the SSE2 routine individually to ensure
     * that all are checked.  A missed check will cause the program to
     * crash due to a misaligned SSE register load or store. */
    struct {
        const uint8_t *yuv[3];
        int yuv_stride[3];
        uint8_t *rgb;
        int rgb_stride;
        int width;
    } argsets[] = {
        {{Y+65,U+1,V+1}, {16,8,8}, rgb, 16, 16},
        {{Y+32,U+1,V+1}, {17,8,8}, rgb, 16, 16},
        {{Y,U+1,V+1}, {16,8,8}, rgb+1, 16, 16},
        {{Y,U+1,V+1}, {16,8,8}, rgb, 17, 16},
        {{Y,U+1,V+1}, {16,8,8}, rgb, 16, 18},
    };

    for (int set = 0; set < lenof(argsets); set++) {
        DLOG("Testing set %d", set);
        yuv2rgb(argsets[set].yuv, argsets[set].yuv_stride,
                argsets[set].rgb, argsets[set].rgb_stride,
                argsets[set].width, 2, 0);
        for (int y = 0; y < 2; y++) {
            uint8_t *line = argsets[set].rgb + y*(argsets[set].rgb_stride*4);
            for (int x = 0; x < 16; x++) {
                CHECK_PIXEL(&line[x*4],
                            expect[(y*16+x)*3+0],
                            expect[(y*16+x)*3+1],
                            expect[(y*16+x)*3+2],
                            255,
                            x, y);
            }
        }
    }

    return 1;
}

#endif  // SIL_ARCH_X86

/*-----------------------------------------------------------------------*/

#ifdef SIL_ARCH_MIPS

TEST(test_asm_mips)
{
    static const uint8_t Y[1+32] =
        {0,50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
           55,65,75,85,95,105,115,125,135,145,155,165,175,185,195,205};
    static const ALIGNED(8) uint8_t U[1+8] =
        {0,  8, 40, 72,104,136,168,200,232};
    static const ALIGNED(8) uint8_t V[1+8] =
        {0,248,216,184,152,120, 88, 56, 24};
    static const uint8_t expect[32*3] = {
        231,  0,  0, 243,  1,  0, 203, 26,  0, 215, 37,  0,
        176, 63,  0, 187, 74,  0, 148, 99, 61, 159,111, 73,
        120,136,149, 132,148,161,  92,173,237, 104,185,248,
         64,210,255,  76,221,255,  37,246,255,  48,255,255,
        237,  0,  0, 249,  7,  0, 209, 32,  0, 221, 43,  0,
        181, 68,  0, 193, 80,  0, 154,105, 67, 165,117, 79,
        126,142,155, 137,154,166,  98,179,243, 110,190,254,
         70,215,255,  82,227,255,  42,252,255,  54,255,255,
    };

    uint32_t rgb_base[32];
    uint8_t *rgb = (uint8_t *)rgb_base;

    yuv2rgb((const uint8_t *[]){Y+1,U+1,V+1}, (const int[]){16,8,8}, rgb, 16,
            16, 2, 0);
    for (int y = 0; y < 2; y++) {
        uint8_t *line = rgb + y*(16*4);
        for (int x = 0; x < 16; x++) {
            CHECK_PIXEL(&line[x*4], expect[(y*16+x)*3+0],
                        expect[(y*16+x)*3+1], expect[(y*16+x)*3+2], 255, x, y);
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_asm_mips_conditions)
{
    static const uint8_t Y[1+32] =
        {0,50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,
           55,65,75,85,95,105,115,125,135,145,155,165,175,185,195,205};
    static const ALIGNED(8) uint8_t U[1+8] =
        {0,  8, 40, 72,104,136,168,200,232};
    static const ALIGNED(8) uint8_t V[1+8] =
        {0,248,216,184,152,120, 88, 56, 24};
    static const uint8_t expect[32*3] = {
        231,  0,  0, 243,  1,  0, 203, 26,  0, 215, 37,  0,
        176, 63,  0, 187, 74,  0, 148, 99, 61, 159,111, 73,
        120,136,149, 132,148,161,  92,173,237, 104,185,248,
         64,210,255,  76,221,255,  37,246,255,  48,255,255,
        237,  0,  0, 249,  7,  0, 209, 32,  0, 221, 43,  0,
        181, 68,  0, 193, 80,  0, 154,105, 67, 165,117, 79,
        126,142,155, 137,154,166,  98,179,243, 110,190,254,
         70,215,255,  82,227,255,  42,252,255,  54,255,255,
    };

    uint32_t rgb_base[32+1];
    uint8_t *rgb = (uint8_t *)rgb_base+1;

    yuv2rgb((const uint8_t *[]){Y+1,U+1,V+1}, (const int[]){16,8,8}, rgb, 16,
            16, 2, 0);
    for (int y = 0; y < 2; y++) {
        uint8_t *line = rgb + y*(16*4);
        for (int x = 0; x < 16; x++) {
            CHECK_PIXEL(&line[x*4], expect[(y*16+x)*3+0],
                        expect[(y*16+x)*3+1], expect[(y*16+x)*3+2], 255, x, y);
        }
    }

    return 1;
}

#endif  // SIL_ARCH_MIPS

/*************************************************************************/
/**************************** Timing routines ****************************/
/*************************************************************************/

/* These routines are not "tests" per se, but this is a convenient place
 * to check the timing of the C vs. assembly routines. */

/*-----------------------------------------------------------------------*/

#if defined(SIL_ARCH_X86) && defined(__GNUC__)

TEST(time_x86)
{
    uint8_t *Y;
    ASSERT(Y = mem_alloc(640*480*3/2, 16, 0));
    uint8_t *U = Y + 640*480;
    uint8_t *V = U + 320*240;
    unsigned int seed = 1;
    for (int i = 0; i < 640*480*3/2; i++) {
        Y[i] = seed>>23 & 0xFF;
        seed = seed*22695477 + 1;
    }
    uint8_t *rgb;
    ASSERT(rgb = mem_alloc((640*480*4)+1, 16, 0));

    /* We make a first call to prime the CPU's caches and branch predictors;
     * then we measure 7 consecutive calls with the same arguments, discard
     * the lowest and highest times, and take the mean of the remainder.
     * These arrays are used to store the low-order word of the RDTSC value
     * at the beginning and end of each call. */
    uint32_t start_sse2[8], end_sse2[8];
    uint32_t start_sse2_smooth[8], end_sse2_smooth[8];
    uint32_t start_c[8], end_c[8];
    uint32_t start_c_smooth[8], end_c_smooth[8];

    /* We use a CPUID instruction to serialize execution so we measure only
     * the time taken by yuv2rgb() itself.  This modifies the EBX register,
     * which may be used as a PIC register on x86-32, so we define a macro
     * here to cover the architectural differences. */
    #ifdef SIL_ARCH_X86_32
        #define CPUID()  __asm__ volatile("push %%ebx; cpuid; pop %%ebx" \
                                          : : : "eax", "ecx", "edx")
    #else
        #define CPUID()  __asm__ volatile("cpuid" \
                                          : : : "eax", "ebx", "ecx", "edx")
    #endif

    for (int i = 0; i < 8; i++) {
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (start_sse2[i]) : : "edx");
        yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
                rgb, 640, 640, 480, 0);
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (end_sse2[i]) : : "edx");
    }

    for (int i = 0; i < 8; i++) {
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (start_sse2_smooth[i]) : : "edx");
        yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
                rgb, 640, 640, 480, 1);
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (end_sse2_smooth[i]) : : "edx");
    }

    for (int i = 0; i < 8; i++) {
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (start_c[i]) : : "edx");
        yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
                rgb+1, 640, 640, 480, 0);
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (end_c[i]) : : "edx");
    }

    for (int i = 0; i < 8; i++) {
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (start_c_smooth[i]) : : "edx");
        yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
                rgb+1, 640, 640, 480, 1);
        CPUID();
        __asm__ volatile("rdtsc" : "=a" (end_c_smooth[i]) : : "edx");
    }

    #undef CPUID

    uint32_t time[7], low, high, total;

    for (int i = 0; i < 7; i++) {
        time[i] = end_sse2[i+1] - start_sse2[i+1];
        if (i == 0) {
            low = high = total = time[i];
        } else {
            total += time[i];
            if (time[i] < low) {
                low = time[i];
            } else if (time[i] > high) {
                high = time[i];
            }
        }
    }
    DLOG("SSE2 blocky: %u clocks (mean of 5/7)", (total+2)/5);

    for (int i = 0; i < 7; i++) {
        time[i] = end_c[i+1] - start_c[i+1];
        if (i == 0) {
            low = high = total = time[i];
        } else {
            total += time[i];
            if (time[i] < low) {
                low = time[i];
            } else if (time[i] > high) {
                high = time[i];
            }
        }
    }
    DLOG("C blocky: %u clocks (mean of 5/7)", (total+2)/5);

    for (int i = 0; i < 7; i++) {
        time[i] = end_sse2_smooth[i+1] - start_sse2_smooth[i+1];
        if (i == 0) {
            low = high = total = time[i];
        } else {
            total += time[i];
            if (time[i] < low) {
                low = time[i];
            } else if (time[i] > high) {
                high = time[i];
            }
        }
    }
    DLOG("SSE2 smooth: %u clocks (mean of 5/7)", (total+2)/5);

    for (int i = 0; i < 7; i++) {
        time[i] = end_c_smooth[i+1] - start_c_smooth[i+1];
        if (i == 0) {
            low = high = total = time[i];
        } else {
            total += time[i];
            if (time[i] < low) {
                low = time[i];
            } else if (time[i] > high) {
                high = time[i];
            }
        }
    }
    DLOG("C smooth: %u clocks (mean of 5/7)", (total+2)/5);

    mem_free(Y);
    mem_free(rgb);
    return 1;
}

#endif  // SIL_ARCH_X86

/*-----------------------------------------------------------------------*/

/* This test could run on any MIPS platform, but MIPS doesn't expose a
 * timing register like RDTSC at the user privilege level, so we need
 * platform-specific help which we can only get on the PSP. */
#ifdef SIL_PLATFORM_PSP

TEST(time_mips_psp)
{
    uint8_t *Y;
    ASSERT(Y = mem_alloc(640*480*3/2, 16, 0));
    uint8_t *U = Y + 640*480;
    uint8_t *V = U + 320*240;
    unsigned int seed = 1;
    for (int i = 0; i < 640*480*3/2; i++) {
        Y[i] = seed>>23 & 0xFF;
        seed = seed*22695477 + 1;
    }
    uint8_t *rgb;
    ASSERT(rgb = mem_alloc((640*480*4)+1, 4, 0));

    uint32_t start, time;

    /* Run the routine once to prime the instruction cache, then again to
     * actually time it.  Since we disable interrupts, we can get a good
     * result from a single run. */
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
            rgb, 640, 640, 480, 0);
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
            rgb, 640, 640, 480, 0);
    time = sceKernelGetSystemTimeLow() - start;
    psp_threads_unlock();
    DLOG("Assembly: %u usec", time);

    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
            rgb+1, 640, 640, 480, 0);
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    yuv2rgb((const uint8_t *[]){Y,U,V}, (const int[]){640,320,320},
            rgb+1, 640, 640, 480, 0);
    time = sceKernelGetSystemTimeLow() - start;
    psp_threads_unlock();
    DLOG("C: %u usec", time);

    mem_free(Y);
    mem_free(rgb);
    return 1;
}

#endif  // SIL_PLATFORM_PSP

/*************************************************************************/
/*************************************************************************/
