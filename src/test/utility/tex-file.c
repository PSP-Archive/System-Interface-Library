/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/tex-file.c: Tests for the texture file utility function
 * tex_parse_header().
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/texture.h"
#include "src/utility/tex-file.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_tex_file)

/*------------------------------------------------------------------------*/

/* Convenience macro for checking that a parse failure does not overwrite
 * the TexFileHeader return buffer. */

#define CHECK_PARSE_FAIL(data,size)  do {                       \
    TexFileHeader header;                                       \
    memset(&header, 0xDD, sizeof(header));                      \
    CHECK_FALSE(tex_parse_header((data), (size), &header));     \
    for (unsigned int i = 0; i < sizeof(header); i++) {         \
        const uint8_t byte = ((uint8_t *)&header)[i];           \
        if (byte != 0xDD) {                                     \
            FAIL("TexFileHeader was corrupted at byte %u"       \
                 " (0x%02X, should be 0xDD)", i, byte);         \
        }                                                       \
    }                                                           \
} while (0)

/*------------------------------------------------------------------------*/

TEST(test_v2)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    TexFileHeader header;
    CHECK_TRUE(tex_parse_header(data, sizeof(data), &header));
    CHECK_MEMEQUAL(header.magic, TEX_FILE_MAGIC, sizeof(header.magic));
    CHECK_INTEQUAL(header.version, TEX_FILE_VERSION);
    CHECK_INTEQUAL(header.format, TEX_FORMAT_RGBA8888);
    CHECK_INTEQUAL(header.mipmaps, 7);
    CHECK_INTEQUAL(header.opaque_bitmap, 1);
    CHECK_INTEQUAL(header.width, 4);
    CHECK_INTEQUAL(header.height, 5);
    CHECK_FLOATEQUAL(header.scale, 0.5);
    CHECK_INTEQUAL(header.pixels_offset, 32);
    CHECK_INTEQUAL(header.pixels_size, 3);
    CHECK_INTEQUAL(header.bitmap_offset, 35);
    CHECK_INTEQUAL(header.bitmap_size, 5);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_v2_formats)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };
    static const struct {uint8_t code; uint8_t format;} formats[] = {
        {0x00, TEX_FORMAT_RGBA8888},
        {0x01, TEX_FORMAT_RGB565},
        {0x02, TEX_FORMAT_RGBA5551},
        {0x03, TEX_FORMAT_RGBA4444},
        {0x08, TEX_FORMAT_BGRA8888},
        {0x09, TEX_FORMAT_BGR565},
        {0x0A, TEX_FORMAT_BGRA5551},
        {0x0B, TEX_FORMAT_BGRA4444},
        {0x40, TEX_FORMAT_A8},
        {0x70, TEX_FORMAT_PSP_RGBA8888},
        {0x71, TEX_FORMAT_PSP_RGB565},
        {0x72, TEX_FORMAT_PSP_RGBA5551},
        {0x73, TEX_FORMAT_PSP_RGBA4444},
        {0x74, TEX_FORMAT_PSP_A8},
        {0x75, TEX_FORMAT_PSP_PALETTE8_RGBA8888},
        {0x78, TEX_FORMAT_PSP_RGBA8888_SWIZZLED},
        {0x79, TEX_FORMAT_PSP_RGB565_SWIZZLED},
        {0x7A, TEX_FORMAT_PSP_RGBA5551_SWIZZLED},
        {0x7B, TEX_FORMAT_PSP_RGBA4444_SWIZZLED},
        {0x7C, TEX_FORMAT_PSP_A8_SWIZZLED},
        {0x7D, TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED},
        {0x80, TEX_FORMAT_PALETTE8_RGBA8888},
        {0x81, TEX_FORMAT_S3TC_DXT1},
        {0x82, TEX_FORMAT_S3TC_DXT3},
        {0x83, TEX_FORMAT_S3TC_DXT5},
        {0x84, TEX_FORMAT_PVRTC2_RGBA},
        {0x85, TEX_FORMAT_PVRTC4_RGBA},
        {0x86, TEX_FORMAT_PVRTC2_RGB},
        {0x87, TEX_FORMAT_PVRTC4_RGB},
    };

    char *buffer;
    ASSERT(buffer = mem_alloc(sizeof(data), 0, MEM_ALLOC_TEMP));
    memcpy(buffer, data, sizeof(data));
    for (int i = 0; i < lenof(formats); i++) {
        buffer[5] = formats[i].code;
        TexFileHeader header;
        if (!tex_parse_header(buffer, sizeof(data), &header)) {
            FAIL("tex_parse_header(data, sizeof(data), &header) was not true"
                 " as expected for format 0x%02X", formats[i].code);
        }
        if (header.format != formats[i].format) {
            FAIL("header.format was %u but should have been %u for format"
                 " 0x%02X", header.format, formats[i].format, formats[i].code);
        }
    }
    mem_free(buffer);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_v1)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   1,  0,  0,  0,   0,  4,  0,  5,   8,  0,  7,  1,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    TexFileHeader header;
    CHECK_TRUE(tex_parse_header(data, sizeof(data), &header));
    CHECK_MEMEQUAL(header.magic, TEX_FILE_MAGIC, sizeof(header.magic));
    CHECK_INTEQUAL(header.version, TEX_FILE_VERSION);
    CHECK_INTEQUAL(header.format, TEX_FORMAT_RGBA8888);
    CHECK_INTEQUAL(header.mipmaps, 7);
    CHECK_INTEQUAL(header.opaque_bitmap, 1);
    CHECK_INTEQUAL(header.width, 4);
    CHECK_INTEQUAL(header.height, 5);
    CHECK_FLOATEQUAL(header.scale, 0.5);
    CHECK_INTEQUAL(header.pixels_offset, 32);
    CHECK_INTEQUAL(header.pixels_size, 3);
    CHECK_INTEQUAL(header.bitmap_offset, 35);
    CHECK_INTEQUAL(header.bitmap_size, 5);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_v1_formats)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   1,  0,  0,  0,   0,  4,  0,  5,   8,  0,  7,  1,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };
    static const struct {uint8_t code; uint8_t format;} formats[] = {
        {0x00, TEX_FORMAT_RGBA8888},
        {0x01, TEX_FORMAT_PALETTE8_RGBA8888},
        {0x02, TEX_FORMAT_A8},
        {0x80, TEX_FORMAT_PVRTC2_RGB},
        {0x81, TEX_FORMAT_PVRTC4_RGB},
        {0x82, TEX_FORMAT_PVRTC2_RGBA},
        {0x83, TEX_FORMAT_PVRTC4_RGBA},
    };

    char *buffer;
    ASSERT(buffer = mem_alloc(sizeof(data), 0, MEM_ALLOC_TEMP));
    memcpy(buffer, data, sizeof(data));
    for (int i = 0; i < lenof(formats); i++) {
        buffer[13] = formats[i].code;
        TexFileHeader header;
        if (!tex_parse_header(buffer, sizeof(data), &header)) {
            FAIL("tex_parse_header(data, sizeof(data), &header) was not true"
                 " as expected for format 0x%02X", formats[i].code);
        }
        if (header.format != formats[i].format) {
            FAIL("header.format was %d but should have been %d for format"
                 " 0x%02X", header.format, formats[i].format, formats[i].code);
        }
    }
    mem_free(buffer);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_no_bitmap)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  0,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    TexFileHeader header;
    CHECK_TRUE(tex_parse_header(data, sizeof(data), &header));
    CHECK_MEMEQUAL(header.magic, TEX_FILE_MAGIC, sizeof(header.magic));
    CHECK_INTEQUAL(header.version, TEX_FILE_VERSION);
    CHECK_INTEQUAL(header.format, TEX_FORMAT_RGBA8888);
    CHECK_INTEQUAL(header.mipmaps, 7);
    CHECK_INTEQUAL(header.opaque_bitmap, 0);
    CHECK_INTEQUAL(header.width, 4);
    CHECK_INTEQUAL(header.height, 5);
    CHECK_FLOATEQUAL(header.scale, 0.5);
    CHECK_INTEQUAL(header.pixels_offset, 32);
    CHECK_INTEQUAL(header.pixels_size, 3);
    CHECK_INTEQUAL(header.bitmap_offset, 35);  // Should be present even though
    CHECK_INTEQUAL(header.bitmap_size, 5);     // the bitmap flag is clear.

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_short_magic)
{
    static const uint8_t data[] = {'T','E','X'};

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_short_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_magic)
{
    static const ALIGNED(4) uint8_t data[] = {
        't','e','x', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_version)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   0,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    char *buffer;
    ASSERT(buffer = mem_alloc(sizeof(data), 0, MEM_ALLOC_TEMP));
    memcpy(buffer, data, sizeof(data));
    buffer[4] = TEX_FILE_VERSION + 1;
    CHECK_PARSE_FAIL(buffer, sizeof(data));
    mem_free(buffer);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_pixels_bad_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 42,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_pixels_negative_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
        255,255,255,255,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_pixels_bad_end)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0, 13,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_pixels_end_overflow)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
         64,  0,  0, 32,  64,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bitmap_bad_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 45,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bitmap_negative_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3, 255,255,255,255,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bitmap_bad_end)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 39,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bitmap_end_overflow)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3, 127,255,255,255,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bitmap_bad_size)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  3,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_alignment)
{
    static const ALIGNED(4) uint8_t data[] = {
        0,  // Force misalignment.
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };

    CHECK_PARSE_FAIL(data+1, sizeof(data)-1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_invalid_params)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };
    TexFileHeader header;

    CHECK_FALSE(tex_parse_header(NULL, sizeof(data), &header));
    CHECK_FALSE(tex_parse_header(data, -1, &header));
    CHECK_FALSE(tex_parse_header(data, sizeof(data), NULL));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
