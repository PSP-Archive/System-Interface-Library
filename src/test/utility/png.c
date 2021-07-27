/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/png.c: Tests for png interface functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/utility/png.h"

#ifndef SIL_UTILITY_INCLUDE_PNG
int test_utility_png(void) {
    DLOG("PNG support disabled, nothing to test.");
    return 1;
}
#else  // To the end of the file.

#define PNG_USER_MEM_SUPPORTED
#include <zlib.h>
#include <png.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Test data for png_parse(). */

static const uint8_t test_grey8_raw[8][8][4] = {
    {{  0,  0,  0,255},{ 34, 34, 34,255},{ 68, 68, 68,255},{102,102,102,255},{136,136,136,255},{170,170,170,255},{204,204,204,255},{238,238,238,255}},
    {{ 16, 16, 16,255},{ 50, 50, 50,255},{ 84, 84, 84,255},{118,118,118,255},{152,152,152,255},{186,186,186,255},{220,220,220,255},{254,254,254,255}},
    {{ 32, 32, 32,255},{ 66, 66, 66,255},{100,100,100,255},{134,134,134,255},{168,168,168,255},{202,202,202,255},{236,236,236,255},{255,255,255,255}},
    {{ 48, 48, 48,255},{ 82, 82, 82,255},{116,116,116,255},{150,150,150,255},{184,184,184,255},{218,218,218,255},{252,252,252,255},{255,255,255,255}},
    {{ 64, 64, 64,255},{ 98, 98, 98,255},{132,132,132,255},{166,166,166,255},{200,200,200,255},{234,234,234,255},{255,255,255,255},{255,255,255,255}},
    {{ 80, 80, 80,255},{114,114,114,255},{148,148,148,255},{182,182,182,255},{216,216,216,255},{250,250,250,255},{255,255,255,255},{255,255,255,255}},
    {{ 96, 96, 96,255},{130,130,130,255},{164,164,164,255},{198,198,198,255},{232,232,232,255},{255,255,255,255},{255,255,255,255},{255,255,255,255}},
    {{112,112,112,255},{146,146,146,255},{180,180,180,255},{214,214,214,255},{248,248,248,255},{255,255,255,255},{255,255,255,255},{255,255,255,255}},
};
static const uint8_t test_grey8_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  8,  0,  0,  0,  8,  8,  0,  0,  0,  0,225,100,225,
     87,  0,  0,  0, 34, 73, 68, 65, 84,  8,215, 99,100, 80,130,  0,
     38,  1, 40,128, 49, 24, 97, 12,  6, 40,131, 25,198, 96,128, 50,
     88, 97, 12,  6,  6,  6,  0,162, 51,  3,199, 56,207,244,167,  0,
      0,  0,  0, 73, 69, 78, 68,174, 66, 96,130,
};

static const uint8_t test_rgb8_raw[8][6][4] = {
    {{  0,  0,  0,255},{ 17, 17,  0,255},{  0, 68,  0,255},{  0, 51, 51,255},{  0,  0,136,255},{ 85,  0, 85,255}},
    {{  8,  0,  8,255},{ 50,  0,  0,255},{ 42, 42,  0,255},{  0,118,  0,255},{  0, 76, 76,255},{  0,  0,186,255}},
    {{  0,  0, 32,255},{ 33,  0, 33,255},{100,  0,  0,255},{ 67, 67,  0,255},{  0,168,  0,255},{  0,101,101,255}},
    {{  0, 24, 24,255},{  0,  0, 82,255},{ 58,  0, 58,255},{150,  0,  0,255},{ 92, 92,  0,255},{  0,218,  0,255}},
    {{  0, 64,  0,255},{  0, 49, 49,255},{  0,  0,132,255},{ 83,  0, 83,255},{200,  0,  0,255},{117,117,  0,255}},
    {{ 40, 40,  0,255},{  0,114,  0,255},{  0, 74, 74,255},{  0,  0,182,255},{108,  0,108,255},{250,  0,  0,255}},
    {{ 96,  0,  0,255},{ 65, 65,  0,255},{  0,164,  0,255},{  0, 99, 99,255},{  0,  0,232,255},{127,  0,127,255}},
    {{ 56,  0, 56,255},{146,  0,  0,255},{ 90, 90,  0,255},{  0,214,  0,255},{  0,124,124,255},{  0,  0,255,255}},
};
static const uint8_t test_rgb8_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  6,  0,  0,  0,  8,  8,  2,  0,  0,  0, 85,164, 25,
    111,  0,  0,  0,145, 73, 68, 65, 84,  8,215,  5,193, 49, 11,  1,
    113,  0,198,225, 95,254,203, 91,166,119, 51,152,101, 39,221,160,
    108, 50, 40,147,146, 69, 54, 89,100,213, 45, 86,163, 15,101,184,
    129, 50, 80,119,  3,101, 56,117,  6,195, 13,  6,197,243,  4,192,
    230,211,  5,183,200, 87, 99, 28,132, 26,159,175, 69, 49,200,177,
     46,233, 50,124,169, 11,247,138,183,197,109,158, 98,  5,106,122,
    151, 29,225,201,237, 97,113,222, 28,  3,237, 18,235,241, 28,  9,
    175,207, 87,139,208, 20,175,254, 19,235,154, 45,132,119,135, 36,
     12, 95, 88,220,103, 25, 86,114,218, 86,168, 18, 17,237, 97, 58,
    133, 11,196, 49,252,254,238, 58, 44,184,135, 79,108, 46,  0,  0,
      0,  0, 73, 69, 78, 68,174, 66, 96,130,
};
static const uint8_t test_rgb8_indexed_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  6,  0,  0,  0,  8,  8,  3,  0,  0,  0,237, 24,126,
     10,  0,  0,  0,144, 80, 76, 84, 69,  0,  0,  0,  0,  0,132,  0,
    124,124,  0,  0, 32, 42, 42,  0,  0, 51, 51,250,  0,  0, 96,  0,
      0, 90, 90,  0,117,117,  0,  0, 24, 24,150,  0,  0,  0, 76, 76,
     50,  0,  0,  0,  0,136,  0,  0,182, 17, 17,  0,  0, 49, 49,  0,
      0, 82, 65, 65,  0, 92, 92,  0,  0,101,101,  0, 74, 74,  0,218,
      0,  0,214,  0,127,  0,127,146,  0,  0,108,  0,108,  0,168,  0,
      0,164,  0,200,  0,  0, 85,  0, 85, 83,  0, 83,  0,118,  0,  0,
    114,  0, 58,  0, 58, 56,  0, 56,  0,  0,186,100,  0,  0,  0, 68,
      0, 40, 40,  0, 67, 67,  0,  0, 64,  0, 33,  0, 33,  0, 99, 99,
      0,  0,232,  8,  0,  8,  0,  0,255,160,  9,206, 77,  0,  0,  0,
     61, 73, 68, 65, 84,  8,153,  5,193,131,  1,192,  0,  0,192,176,
    206,182,109,235,255,239,150,128,191, 41,110,199,235,200,131,189,
     32, 93,235, 81, 39,152,225,100,197, 25,103, 32,244,173,193, 62,
    166, 94,165,162, 69,205,253, 20,204,165,158,139,223, 15,119, 51,
      4,105, 47,157,140, 44,  0,  0,  0,  0, 73, 69, 78, 68,174, 66,
     96,130,
};

static const uint8_t test_rgb8_tRNS_raw[8][6][4] = {
    {{  0,  0,  0,255},{ 17, 17,  0,255},{  0, 68,  0,255},{  0, 51, 51,255},{  0,  0,136,255},{ 85,  0, 85,255}},
    {{  8,  0,  8,255},{ 50,  0,  0,255},{ 42, 42,  0,255},{  0,118,  0,255},{  0, 76, 76,255},{  0,  0,186,255}},
    {{  0,  0, 32,255},{ 33,  0, 33,255},{100,  0,  0,255},{ 67, 67,  0,255},{  0,168,  0,255},{  0,101,101,255}},
    {{  0, 24, 24,255},{  0,  0, 82,255},{ 58,  0, 58,255},{150,  0,  0,255},{ 92, 92,  0,255},{  0,218,  0,255}},
    {{  0, 64,  0,255},{  0, 49, 49,255},{  0,  0,132,255},{ 83,  0, 83,255},{200,  0,  0,255},{117,117,  0,255}},
    {{ 40, 40,  0,255},{  0,114,  0,255},{  0, 74, 74,255},{  0,  0,182,255},{108,  0,108,255},{250,  0,  0,255}},
    {{ 96,  0,  0,255},{ 65, 65,  0,255},{  0,164,  0,255},{  0, 99, 99,255},{  0,  0,232,255},{127,  0,127,255}},
    {{ 56,  0, 56,255},{146,  0,  0,255},{ 90, 90,  0,255},{  0,214,  0,255},{  0,124,124,255},{  0,  0,255,  0}},
};
static const uint8_t test_rgb8_tRNS_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  6,  0,  0,  0,  8,  8,  2,  0,  0,  0, 85,164, 25,
    111,  0,  0,  0,  6,116, 82, 78, 83,  0,  0,  0,  0,  0,255, 67,
    164,232, 28,  0,  0,  0,145, 73, 68, 65, 84,  8,215,  5,193, 49,
     11,  1,113,  0,198,225, 95,254,203, 91,166,119, 51,152,101, 39,
    221,160,108, 50, 40,147,146, 69, 54, 89,100,213, 45, 86,163, 15,
    101,184,129, 50, 80,119,  3,101, 56,117,  6,195, 13,  6,197,243,
      4,192,230,211,  5,183,200, 87, 99, 28,132, 26,159,175, 69, 49,
    200,177, 46,233, 50,124,169, 11,247,138,183,197,109,158, 98,  5,
    106,122,151, 29,225,201,237, 97,113,222, 28,  3,237, 18,235,241,
     28,  9,175,207, 87,139,208, 20,175,254, 19,235,154, 45,132,119,
    135, 36, 12, 95, 88,220,103, 25, 86,114,218, 86,168, 18, 17,237,
     97, 58,133, 11,196, 49,252,254,238, 58, 44,184,135, 79,108, 46,
      0,  0,  0,  0, 73, 69, 78, 68,174, 66, 96,130,
};

static const uint8_t test_grey_a8_raw[8][8][4] = {
    {{  0,  0,  0, 42},{ 34, 34, 34, 76},{ 68, 68, 68,110},{102,102,102,144},{136,136,136,178},{170,170,170,212},{204,204,204,246},{238,238,238, 24}},
    {{ 16, 16, 16, 58},{ 50, 50, 50, 92},{ 84, 84, 84,126},{118,118,118,160},{152,152,152,194},{186,186,186,228},{220,220,220,  6},{254,254,254, 40}},
    {{ 32, 32, 32, 74},{ 66, 66, 66,108},{100,100,100,142},{134,134,134,176},{168,168,168,210},{202,202,202,244},{236,236,236, 22},{255,255,255, 41}},
    {{ 48, 48, 48, 90},{ 82, 82, 82,124},{116,116,116,158},{150,150,150,192},{184,184,184,226},{218,218,218,  4},{252,252,252, 38},{255,255,255, 41}},
    {{ 64, 64, 64,106},{ 98, 98, 98,140},{132,132,132,174},{166,166,166,208},{200,200,200,242},{234,234,234, 20},{255,255,255, 41},{255,255,255, 41}},
    {{ 80, 80, 80,122},{114,114,114,156},{148,148,148,190},{182,182,182,224},{216,216,216,  2},{250,250,250, 36},{255,255,255, 41},{255,255,255, 41}},
    {{ 96, 96, 96,138},{130,130,130,172},{164,164,164,206},{198,198,198,240},{232,232,232, 18},{255,255,255, 41},{255,255,255, 41},{255,255,255, 41}},
    {{112,112,112,154},{146,146,146,188},{180,180,180,222},{214,214,214,  0},{248,248,248, 34},{255,255,255, 41},{255,255,255, 41},{255,255,255, 41}},
};
static const uint8_t test_grey_a8_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  8,  0,  0,  0,  8,  8,  4,  0,  0,  0,110,  6,118,
      0,  0,  0,  0, 43, 73, 68, 65, 84,  8,215, 93,200,177, 17,  0,
     48,  8,  3,177,119, 56,122,102, 72,201,254,  3, 82,219, 42, 37,
    246,155, 55, 33, 66,138,  0,139,170,  8,176,232,142,  0,128,  3,
    118,109,  7,168,  8, 89, 74,211,  0,  0,  0,  0, 73, 69, 78, 68,
    174, 66, 96,130,
};

static const uint8_t test_rgba8_raw[8][6][4] = {
    {{  0,  0,  0, 42},{ 17, 17,  0, 76},{  0, 68,  0,110},{  0, 51, 51,144},{  0,  0,136,178},{ 85,  0, 85,212}},
    {{  8,  0,  8,246},{ 50,  0,  0, 24},{ 42, 42,  0, 58},{  0,118,  0, 92},{  0, 76, 76,126},{  0,  0,186,160}},
    {{  0,  0, 32,194},{ 33,  0, 33,228},{100,  0,  0,  6},{ 67, 67,  0, 40},{  0,168,  0, 74},{  0,101,101,108}},
    {{  0, 24, 24,142},{  0,  0, 82,176},{ 58,  0, 58,210},{150,  0,  0,244},{ 92, 92,  0, 22},{  0,218,  0, 41}},
    {{  0, 64,  0, 90},{  0, 49, 49,124},{  0,  0,132,158},{ 83,  0, 83,192},{200,  0,  0,226},{117,117,  0,  4}},
    {{ 40, 40,  0, 38},{  0,114,  0, 41},{  0, 74, 74,106},{  0,  0,182,140},{108,  0,108,174},{250,  0,  0,208}},
    {{ 96,  0,  0,242},{ 65, 65,  0, 20},{  0,164,  0, 41},{  0, 99, 99, 41},{  0,  0,232,122},{127,  0,127,156}},
    {{ 56,  0, 56,190},{146,  0,  0,224},{ 90, 90,  0,  2},{  0,214,  0, 36},{  0,124,124, 41},{  0,  0,255, 41}},
};
static const uint8_t test_rgba8_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  6,  0,  0,  0,  8,  8,  6,  0,  0,  0,218,198,142,
     56,  0,  0,  0,202, 73, 68, 65, 84,  8,215,  5,193, 33,107,130,
     81, 20,128,225,119,222,176,  3,  6, 57,  8,195,176,116,145,225,
    103,117, 65, 44, 54,183, 32,  8,130, 69,132, 97,147, 53,211, 45,
    194, 88, 26, 24, 22, 22,246, 59,246, 11, 22, 13,130,  8,  6,149,
    207,160, 99, 65, 81, 65,131,112, 54, 16,182,231,185,  0,242,170,
    248, 67,  1,207,161,224, 25, 53,124,131,145,119,130,124,221,252,
    156,131, 10,182,187, 95, 27, 42, 54,141, 31,205,157,185,190, 19,
     52,148,119,199,160,130, 45,219,177,161, 98,144,201,188, 66,253,
    163, 68,105,242, 14,167, 86,139, 43, 98, 34,199,237, 41,141,138,
    125,111,106, 38,168,117, 39,179,160,194,147,203,  9,191,251,202,
    102,142,202,229,108,209, 49, 65,173, 63, 24,  6, 87,221,179, 84,
    225,101,245,176,216,162, 50, 30,142,159, 83,  9,146, 70,145,226,
    231, 27,172,154, 77, 18, 76,201,210,235, 69,240, 23,253,  3,252,
    125, 70, 80,180,187, 66,162,  0,  0,  0,  0, 73, 69, 78, 68,174,
     66, 96,130,
};
static const uint8_t test_rgba16_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  6,  0,  0,  0,  8, 16,  6,  0,  0,  0,138, 86, 82,
    123,  0,  0,  1, 50, 73, 68, 65, 84, 24,211, 69,205, 49,203, 65,
     81,  0,128,225,247,115,  7,167, 12, 58, 41, 25, 76, 39,201,181,
     50,200, 98,195,160,148,178, 72,201, 38,155,233, 46, 74, 38,101,
     48, 24,252, 14,191,192,104, 80, 82,119,184,116, 13,200, 64, 40,
      6,117, 40,229, 27, 36, 63,224,121,223, 63,  0,136,199,165,148,
     18,148,186, 94, 19,  9, 80, 10,174,215, 68, 66, 41,152,207,203,
    101,165,202,101,152,207,149, 50,132,  0, 33,118,187,104,244,241,
    120,189, 44, 75, 74, 33, 64,235,243, 57,159, 63, 28,180,  6, 41,
    133,208,218,113, 92,183,217,212,218,120,189, 32, 28,206,229,132,
      0, 41, 45, 43,147, 57,159,111,183, 31,220,108,234,117,215,253,
     65, 32, 20, 10,133,  6,  3,128, 82,105, 60, 78,167, 33,157,182,
    237,209,  8,224,126,175, 86,171, 85,  8,  6,193,117,193, 52, 13,
     72, 38,239,247, 64,224, 91,216,239,143,199, 98, 81,235,207, 81,
    235, 86,203,182,151,203,239,177,211, 49, 98, 49, 33,224,249,188,
     92,178,217,227,113,181,250, 64,175,119,185, 92,175, 27,141, 31,
    236,247,167,211,217,204,178,140, 66,225,114,129,205,230, 83,232,
    245,182,219, 90,109,189, 62,157, 62,112,177,152,205, 22,139,110,
    215,239,247,120,192,231,211,154, 84, 10, 82,169,201,100, 56,  4,
    216,110, 43,149, 74,  5, 60, 30,112, 28,136, 68,160,221,110,183,
     77, 19,224,253, 54,205,127,180,150,140,143,224,132,188,106,  0,
      0,  0,  0, 73, 69, 78, 68,174, 66, 96,130,
};

static const uint8_t test_1bpp_bw_raw[4][8][4] = {
    {{255,255,255,255}, {0,0,0,255}, {255,255,255,255}, {0,0,0,255},
     {255,255,255,255}, {0,0,0,255}, {255,255,255,255}, {0,0,0,255}},
    {{255,255,255,255}, {255,255,255,255}, {0,0,0,255}, {0,0,0,255},
     {255,255,255,255}, {255,255,255,255}, {0,0,0,255}, {0,0,0,255}},
    {{255,255,255,255}, {255,255,255,255}, {255,255,255,255},
     {255,255,255,255}, {0,0,0,255}, {0,0,0,255}, {0,0,0,255}, {0,0,0,255}},
    {{255,255,255,255}, {255,255,255,255}, {255,255,255,255},
     {255,255,255,255}, {255,255,255,255}, {255,255,255,255},
     {255,255,255,255}, {255,255,255,255}},
};
static const uint8_t test_1bpp_bw_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  8,  0,  0,  0,  4,  1,  0,  0,  0,  0,155,182, 67,
     93,  0,  0,  0, 16, 73, 68, 65, 84,  8,215, 99, 88,197,112,134,
    225,  3,195,127,  0, 12,121,  3,102,102,127,183,101,  0,  0,  0,
      0, 73, 69, 78, 68,174, 66, 96,130,
};

static const uint8_t test_2bpp_palette_raw[4][4][4] = {
    {{255,0,0,255}, {255,0,0,255}, {255,0,0,255}, {255,0,0,255}},
    {{255,0,0,255}, {0,255,0,255}, {0,255,0,255}, {0,255,0,255}},
    {{0,255,0,255}, {0,255,0,255}, {0,0,255,255}, {0,0,255,255}},
    {{0,0,255,255}, {0,0,255,255}, {0,0,255,255}, {255,255,255,255}},
};
static const uint8_t test_2bpp_palette_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  4,  0,  0,  0,  4,  2,  3,  0,  0,  0,212,159,118,
    237,  0,  0,  0, 12, 80, 76, 84, 69,  0,  0,255,255,255,255,  0,
    255,  0,255,  0,  0, 93, 96, 83,126,  0,  0,  0, 16, 73, 68, 65,
     84,  8,215, 99,248,207,240,138, 97,  1,  3, 35,  0, 13,116,  2,
    139, 22,100,239,112,  0,  0,  0,  0, 73, 69, 78, 68,174, 66, 96,
    130,
};

static const uint8_t test_interlaced_png[] = {
    137, 80, 78, 71, 13, 10, 26, 10,  0,  0,  0, 13, 73, 72, 68, 82,
      0,  0,  0,  8,  0,  0,  0,  8,  8,  2,  0,  0,  1, 60,106, 25,
     74,  0,  0,  0, 46, 73, 68, 65, 84,  8,215,165,139,177, 13,  0,
     32, 12,195, 76,255,255,217, 29, 64,130, 14,153,200, 18, 89, 78,
     80, 81,151, 10,156, 42,224,210, 78,241, 36,155, 12,227, 19, 87,
    127,162,  1, 64,111, 21,  1,249,  2,217,167,  0,  0,  0,  0, 73,
     69, 78, 68,174, 66, 96,130,
};

/*-----------------------------------------------------------------------*/

/* Macros for testing the PNG utility functions on a static data buffer. */

#define TEST_PARSE(png,raw)  do {                                   \
    if (!test_parse_one(test_##png##_png, sizeof(test_##png##_png), \
                        &test_##raw##_raw[0][0][0],                 \
                        lenof(test_##raw##_raw[0]),                 \
                        lenof(test_##raw##_raw))) {                 \
        FAIL("png_parse() failed for %s", #png);                    \
    }                                                               \
} while (0)

#define TEST_PARSE_FAIL(png)  do {                                      \
    int _width, _height;                                                \
    void *image = png_parse(test_##png##_png, sizeof(test_##png##_png), \
                            0, &_width, &_height);                      \
    if (image) {                                                        \
        mem_free(image);                                                \
        FAIL("png_parse succeeded for %s (should have failed)", #png);  \
    }                                                                   \
} while (0)

#define TEST_CREATE(raw)  do {                          \
    if (!test_create_one(&test_##raw##_raw[0][0][0],    \
                         lenof(test_##raw##_raw[0]),    \
                         lenof(test_##raw##_raw))) {    \
        FAIL("png_create() failed for %s", #raw);       \
    }                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * test_parse_one:  Test parsing of a single PNG file using png_parse().
 *
 * [Parameters]
 *     png, png_size: Input PNG file data and size, in bytes.
 *     expected_data: Expected pixel data, in RGBA format.
 *     expected_width: Expected width of image, in pixels.
 *     expected_height: Expected height of image, in pixels.
 * [Return value]
 *     True on success, false on failure.
 */
static int test_parse_one(const uint8_t * const png,
                          const uint32_t png_size,
                          const uint8_t * const expected_data,
                          const int expected_width,
                          const int expected_height);

/**
 * test_compress_one:  Test compression of a single image using png_create()
 * both with and without alpha by parsing the result and verifying that it
 * matches the original data.
 *
 * [Parameters]
 *     data: Image data to compress, in RGBA format.
 *     width: Image width, in pixels.
 *     height: Image height, in pixels.
 * [Return value]
 *     True on success, false on failure.
 */
static int test_create_one(const uint8_t * const data, const int width,
                           const int height);

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_png)

/*-----------------------------------------------------------------------*/

TEST(test_png_parse_grey8)
{
    TEST_PARSE(grey8, grey8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_png_parse_rgb8)
{
    TEST_PARSE(rgb8, rgb8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_png_parse_rgb8_indexed)
{
    TEST_PARSE(rgb8_indexed, rgb8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_rgb8_tRNS)
{
    TEST_PARSE(rgb8_tRNS, rgb8_tRNS);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_grey_a8)
{
    TEST_PARSE(grey_a8, grey_a8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_rgba8)
{
    TEST_PARSE(rgba8, rgba8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_rgba16)
{
    TEST_PARSE(rgba16, rgba8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_1bpp_bw)
{
    TEST_PARSE(1bpp_bw, 1bpp_bw);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_2bpp_palette)
{
    TEST_PARSE(2bpp_palette, 2bpp_palette);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_fail_interlaced)
{
    TEST_PARSE_FAIL(interlaced);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_fail_truncated_data)
{
    for (unsigned int size = 0; size < sizeof(test_rgb8_tRNS_png); size++) {
        int width, height;
        void *image = png_parse(test_rgb8_tRNS_png, size, 0, &width, &height);
        if (image) {
            mem_free(image);
            FAIL("png_parse succeeded for rgb8_tRNS truncated to %u bytes"
                 " (should have failed)", size);
        }
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_memory_failure)
{
    int width, height;
    void *image;
    CHECK_MEMORY_FAILURES(
        image = png_parse(test_rgb8_tRNS_png, sizeof(test_rgb8_tRNS_png),
                          0, &width, &height));
    CHECK_INTEQUAL(width, lenof(test_rgb8_tRNS_raw[0]));
    CHECK_INTEQUAL(height, lenof(test_rgb8_tRNS_raw));
    mem_free(image);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_grey8)
{
    TEST_CREATE(grey8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_rgb8)
{
    TEST_CREATE(rgb8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_grey_a8)
{
    TEST_CREATE(grey_a8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_rgba8)
{
    TEST_CREATE(rgba8);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_memory_failure)
{
    uint32_t png_size;
    uint8_t *png;
    CHECK_MEMORY_FAILURES(
        png = png_create(&test_rgb8_tRNS_raw[0][0][0],
                         lenof(test_rgb8_tRNS_raw[0]),
                         lenof(test_rgb8_tRNS_raw), 1, 1, 0, 0, &png_size));

    int width, height;
    void *image;
    CHECK_TRUE(image = png_parse(png, png_size, 0, &width, &height));
    CHECK_INTEQUAL(width, lenof(test_rgb8_tRNS_raw[0]));
    CHECK_INTEQUAL(height, lenof(test_rgb8_tRNS_raw));

    mem_free(image);
    mem_free(png);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_flush)
{
    uint32_t png_size;
    uint8_t *png;
    CHECK_TRUE(png = png_create(&test_rgb8_tRNS_raw[0][0][0],
                                lenof(test_rgb8_tRNS_raw[0]),
                                lenof(test_rgb8_tRNS_raw),
                                1, 1, 1, 0, &png_size));

    int width, height;
    void *image;
    CHECK_TRUE(image = png_parse(png, png_size, 0, &width, &height));
    CHECK_INTEQUAL(width, lenof(test_rgb8_tRNS_raw[0]));
    CHECK_INTEQUAL(height, lenof(test_rgb8_tRNS_raw));

    mem_free(image);
    mem_free(png);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_num_allocs)
{
    const int width = 640;
    const int height = 480;
    void *data;
    ASSERT(data = mem_alloc(width * height * 4, 0, MEM_ALLOC_CLEAR));

    uint32_t png_size;
    uint8_t *png;
    CHECK_TRUE(png = png_create(data, width, height, 0, 6, 0, 0, &png_size));
    mem_free(data);

    const int expected_allocs = ((png_size + (SIL_UTILITY_PNG_ALLOC_CHUNK - 1))
                                 / SIL_UTILITY_PNG_ALLOC_CHUNK);
    CHECK_INTEQUAL(TEST_png_create_num_allocs, expected_allocs);

    mem_free(png);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_too_wide)
{
    uint8_t *data;
    ASSERT(data = mem_alloc((SIL_UTILITY_PNG_MAX_SIZE + 1) * 4, 0,
                            MEM_ALLOC_CLEAR));

    uint32_t png_size;
    uint8_t *png;
    CHECK_TRUE(png = png_create(data, SIL_UTILITY_PNG_MAX_SIZE + 1, 1,
                                1, 1, 0, 0, &png_size));

    CHECK_FALSE(png_parse(png, png_size, 0, (int[1]){0}, (int[1]){0}));

    mem_free(png);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_too_tall)
{
    uint8_t *data;
    ASSERT(data = mem_alloc((SIL_UTILITY_PNG_MAX_SIZE + 1) * 4, 0,
                            MEM_ALLOC_CLEAR));

    uint32_t png_size;
    uint8_t *png;
    CHECK_TRUE(png = png_create(data, 1, SIL_UTILITY_PNG_MAX_SIZE + 1,
                                1, 1, 0, 0, &png_size));

    CHECK_FALSE(png_parse(png, png_size, 0, (int[1]){0}, (int[1]){0}));

    mem_free(png);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Data structure and libpng callbacks for test_parse_max_size(). */
typedef struct pngWFILE pngWFILE;
struct pngWFILE {
    uint8_t *data;
    uint32_t size, len;
};
static void png_write(png_structp png, png_bytep data, png_size_t length) {
    pngWFILE *f = (pngWFILE *)png_get_io_ptr(png);
    if (f->len + length > f->size) {
        while (f->size < f->len + length) {
            f->size += 1048576;
        }
        ASSERT(f->data = mem_realloc(f->data, f->size, 0));
    }
    memcpy(f->data + f->len, data, length);
    f->len += length;
}
static void png_flush(UNUSED png_structp png) { /* Nothing to do. */ }
static png_voidp png_memalloc(UNUSED png_structp png, png_alloc_size_t size) {
    return mem_alloc(size, 0, MEM_ALLOC_TEMP);
}
static void png_memfree(UNUSED png_structp png, png_voidp ptr) {
    mem_free(ptr);
}
static void png_warning_callback(UNUSED png_structp png, const char *message) {
    DLOG("libpng warning: %s", message);
}
static void png_error_callback(png_structp png, const char *message) {
    DLOG("libpng error: %s", message);
    longjmp(*(jmp_buf *)png_get_error_ptr(png), 1);
}

TEST(test_parse_max_size)
{
#if SIL_UTILITY_PNG_MAX_SIZE >= 0x40000000
    SKIP("Buffer size would overflow int64_t, skipping test.");
#else

    const char *skip = testutil_getenv("SIL_TEST_SKIP_PNG_MAX_SIZE");
    if (skip && strcmp(skip, "1") == 0) {
        SKIP("Skipped due to user request (SIL_TEST_SKIP_PNG_MAX_SIZE).");
    }

    /*
     * We create a 16-bit-depth source image for three reasons:
     * - To exercise the worst-case scenario for memory usage.
     * - To ensure that png_parse() will be able to allocate memory for the
     *   decoded image.  (This is trivially true, since png_parse() returns
     *   an 8-bit-per-pixel image.  It would theoretically still be true if
     *   we used 8-bit depth on the source image, but memory or address
     *   space fragmentation could potentially cause a failure in that case.)
     * - In the specific case of the default MAX_SIZE value 16384, to
     *   ensure that the source image data size (16384*16384*8 = 2147483648)
     *   does not cause overflow anywhere.
     */
    void *data;
    /* Use an int64_t variable in the size calculation to avoid overflow
     * from multiplying in the default (int) type. */
    const int64_t max_size = SIL_UTILITY_PNG_MAX_SIZE;
    const int64_t image_size = max_size * max_size * 8;
    const int64_t rowptr_size = max_size * sizeof(void *);
    if (!(data = mem_alloc(rowptr_size + image_size, 0, 0))) {
        SKIP("Not enough memory for test.");
    }

    const int width = SIL_UTILITY_PNG_MAX_SIZE;
    const int height = SIL_UTILITY_PNG_MAX_SIZE;
    DLOG("Creating %dx%d image, this may take a while...", width, height);
    uint8_t **row_pointers = data;
    uint16_t *ptr = (uint16_t *)&row_pointers[SIL_UTILITY_PNG_MAX_SIZE];
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (uint8_t *)ptr;
        const int y_bit = (y >> (y%32)) & 1;
        for (int x = 0; x < width; x++, ptr += 4) {
            const int x_bit = (x >> (x%32)) & 1;
            ptr[0] = x_bit ? 0xFFFF : 0;
            ptr[1] = y_bit ? 0xFFFF : 0;
            ptr[2] = 0;
            ptr[3] = 0xFFFF;
        }
    }

    /* png_create() only handles 8bpp images, so we need to call directly
     * to libpng.  See png_create() for logic comments. */
    jmp_buf png_jmpbuf;
    png_structp volatile png_volatile = NULL;
    png_infop volatile info_volatile = NULL;
    uint8_t ** volatile data_ptr_volatile = NULL;
    if (setjmp(png_jmpbuf) != 0) {
        mem_free(*data_ptr_volatile);
        png_structp png = png_volatile;
        png_infop info = info_volatile;
        png_destroy_write_struct(&png, &info);
        mem_free(data);
        FAIL("Failed to create %dx%d PNG image", width, height);
    }
    png_structp png = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        &png_jmpbuf, png_error_callback, png_warning_callback,
        NULL, png_memalloc, png_memfree);
    png_volatile = png;
    png_infop info = png_create_info_struct(png);
    info_volatile = info;
    pngWFILE out = {.data = NULL, .size = 0, .len = 0};
    data_ptr_volatile = &out.data;
    png_set_write_fn(png, &out, png_write, png_flush);
    png_set_IHDR(png, info, width, height, 16, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, 1);
    png_set_compression_mem_level(png, 9);
    png_set_compression_window_bits(png, 15);
    png_set_rows(png, info, row_pointers);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&png, &info);
    void *png_data = out.data;
    const uint32_t png_size = out.len;
    CHECK_TRUE(png_data);
    CHECK_TRUE(png_size > 0);
    mem_free(data);

    void *image;
    int out_w, out_h;
    CHECK_TRUE(image = png_parse(png_data, png_size, 0, &out_w, &out_h));
    mem_free(png_data);
    CHECK_INTEQUAL(out_w, width);
    CHECK_INTEQUAL(out_h, height);
    const uint8_t *ptr8 = image;
    for (int y = 0; y < height; y++) {
        const int y_bit = (y >> (y%32)) & 1;
        for (int x = 0; x < width; x++, ptr8 += 4) {
            const int x_bit = (x >> (x%32)) & 1;
            CHECK_PIXEL(ptr8, x_bit ? 255 : 0, y_bit ? 255 : 0, 0, 255, x, y);
        }
    }

    mem_free(image);
    return 1;

#endif  // SIL_UTILITY_PNG_MAX_SIZE >= 0x40000000
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

static int test_parse_one(const uint8_t * const png,
                          const uint32_t png_size,
                          const uint8_t * const expected_data,
                          const int expected_width,
                          const int expected_height)
{
    PRECOND(png != NULL, return 0);
    PRECOND(expected_data != NULL, return 0);

    int image_width, image_height;
    uint8_t *image = png_parse(png, png_size, 0, &image_width, &image_height);
    if (!image) {
        FAIL("png_parse() failed");
    }
    if (image_width != expected_width || image_height != expected_height) {
        mem_free(image);
        FAIL("Wrong size returned from png_parse() (got %dx%d, expected"
             " %dx%d)", image_width, image_height, expected_width,
             expected_height);
    }
    int offset = 0;
    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++, offset += 4) {
            if (image[offset+0] != expected_data[offset+0]
             || image[offset+1] != expected_data[offset+1]
             || image[offset+2] != expected_data[offset+2]
             || image[offset+3] != expected_data[offset+3]) {
                const uint8_t image0 = image[offset+0];
                const uint8_t image1 = image[offset+1];
                const uint8_t image2 = image[offset+2];
                const uint8_t image3 = image[offset+3];
                mem_free(image);
                FAIL("Incorrect pixel data at %d,%d (got RGBA"
                     " %02X%02X%02X%02X, expected %02X%02X%02X%02X)", x, y,
                     image0, image1, image2, image3,
                     expected_data[offset+0], expected_data[offset+1],
                     expected_data[offset+2], expected_data[offset+3]);
            }
        }
    }

    mem_free(image);
    return 1;
}

/*-----------------------------------------------------------------------*/

static int test_create_one(const uint8_t * const data, const int width,
                           const int height)
{
    PRECOND(data != NULL, return 0);
    PRECOND(width > 0, return 0);
    PRECOND(height > 0, return 0);

    for (int use_alpha = 1; use_alpha >= 0; use_alpha--) {
        for (int compression = 1; compression >= -1; compression--) {
            uint32_t png_size;
            uint8_t *png = png_create(data, width, height, use_alpha,
                                      compression, 0, 0, &png_size);
            if (!png) {
                FAIL("png_create() failed with alpha %s",
                     use_alpha ? "enabled" : "disabled");
            }

            int image_width, image_height;
            uint8_t *image = png_parse(png, png_size, 0, &image_width,
                                       &image_height);
            if (!image) {
                mem_free(png);
                FAIL("png_parse() failed with alpha %s",
                     use_alpha ? "enabled" : "disabled");
            }
            if (image_width != width || image_height != height) {
                mem_free(png);
                mem_free(image);
                FAIL("Wrong size returned from png_parse() (got %dx%d,"
                     " expected %dx%d)", image_width, image_height,
                     width, height);
            }
            int offset = 0;
            for (int y = 0; y < image_height; y++) {
                for (int x = 0; x < image_width; x++, offset += 4) {
                    if (image[offset+0] != data[offset+0]
                     || image[offset+1] != data[offset+1]
                     || image[offset+2] != data[offset+2]
                     || image[offset+3] != (use_alpha ? data[offset+3] : 255))
                    {
                        const uint8_t image0 = image[offset+0];
                        const uint8_t image1 = image[offset+1];
                        const uint8_t image2 = image[offset+2];
                        const uint8_t image3 = image[offset+3];
                        mem_free(png);
                        mem_free(image);
                        FAIL("Incorrect pixel data at %d,%d (got RGBA"
                             " %02X%02X%02X%02X, expected %02X%02X%02X%02X)"
                             " with alpha %s", x, y,
                             image0, image1, image2, image3,
                             data[offset+0], data[offset+1],
                             data[offset+2], (use_alpha ? data[offset+3] : 255),
                             use_alpha ? "enabled" : "disabled");
                    }
                }
            }

            mem_free(png);
            mem_free(image);
        }
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_UTILITY_INCLUDE_PNG
