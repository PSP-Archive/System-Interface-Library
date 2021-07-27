/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/font-file.c: Tests for the font file utility functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/utility/font-file.h"

/*************************************************************************/
/***************************** Helper macros *****************************/
/*************************************************************************/

/* Convenience macro for checking that a parse failure does not overwrite
 * the FontFileHeader return buffer. */

#define CHECK_PARSE_HEADER_FAIL(data,size)  do {                \
    FontFileHeader header;                                      \
    memset(&header, 0xDD, sizeof(header));                      \
    CHECK_FALSE(font_parse_header((data), (size), &header));    \
    for (unsigned int i = 0; i < sizeof(header); i++) {         \
        const uint8_t byte = ((uint8_t *)&header)[i];           \
        if (byte != 0xDD) {                                     \
            FAIL("FontFileHeader was corrupted at byte %u"      \
                 " (0x%02X, should be 0xDD)", i, byte);         \
        }                                                       \
    }                                                           \
} while (0)

#define CHECK_PARSE_CHARINFO_FAIL(data,count,version)  do {     \
    FontFileCharInfo *charinfo;                                 \
    const int _size = (count) * sizeof(*charinfo);              \
    ASSERT(charinfo = mem_alloc(_size, 0, MEM_ALLOC_TEMP));     \
    memset(charinfo, 0xDD, _size);                              \
    CHECK_FALSE(font_parse_charinfo((data), (count), (version), \
                                    charinfo));                 \
    for (int i = 0; i < _size; i++) {                           \
        const uint8_t byte = ((uint8_t *)charinfo)[i];          \
        if (byte != 0xDD) {                                     \
            FAIL("FontFileHeader was corrupted at byte %d"      \
                 " (0x%02X, should be 0xDD)", i, byte);         \
        }                                                       \
    }                                                           \
    mem_free(charinfo);                                         \
} while (0)

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_font_file)

/*************************************************************************/
/************************* Header parsing tests **************************/
/*************************************************************************/

TEST(test_v1_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    FontFileHeader header;
    CHECK_TRUE(font_parse_header(data, sizeof(data), &header));
    CHECK_MEMEQUAL(header.magic, FONT_FILE_MAGIC, sizeof(header.magic));
    CHECK_INTEQUAL(header.version, FONT_FILE_VERSION);
    CHECK_INTEQUAL(header.height, 10);
    CHECK_INTEQUAL(header.baseline, 8);
    CHECK_INTEQUAL(header.charinfo_offset, 24);
    CHECK_INTEQUAL(header.charinfo_count, 1);
    CHECK_INTEQUAL(header.charinfo_size, 16);
    CHECK_INTEQUAL(header.texture_offset, 40);
    CHECK_INTEQUAL(header.texture_size, 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_short_magic)
{
    static const ALIGNED(4) uint8_t data[] = {'F','O','N'};

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_short_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_magic)
{
    static const ALIGNED(4) uint8_t data[] = {
        'f','o','n','t',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_version_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  0, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    char *buffer;
    ASSERT(buffer = mem_alloc(sizeof(data), 0, MEM_ALLOC_TEMP));
    memcpy(buffer, data, sizeof(data));
    buffer[4] = FONT_FILE_VERSION + 1;
    CHECK_PARSE_HEADER_FAIL(buffer, sizeof(data));
    mem_free(buffer);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_charinfo_bad_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  1, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_charinfo_negative_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,255,255,255,252,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_charinfo_bad_end)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 25,  0,  2,  0, 16,
          0,  0,  0, 24,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_charinfo_end_overflow)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,127,255,240, 24,  1,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_charinfo_bad_size)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0,  8,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_texture_bad_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  1, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_texture_negative_offset)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
        255,255,255,252,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_texture_bad_end)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  1,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_texture_end_overflow)
{
    static const ALIGNED(4) uint8_t data[] = {
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
         64,  0,  0, 40, 64,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data, sizeof(data));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bad_alignment_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        0,  // Force misalignment.
        'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  1,  0, 16,
          0,  0,  0, 40,  0,  0,  0,  1,  0,  0,  0,' ',  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  4,  0,  0,
    };

    CHECK_PARSE_HEADER_FAIL(data+1, sizeof(data)-1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_invalid_params_header)
{
    static const ALIGNED(4) uint8_t data[] = {
        'T','E','X', 10,   2,  0,  7,  1,   0,  4,  0,  5,   0,  0,128,  0,
          0,  0,  0, 32,   0,  0,  0,  3,   0,  0,  0, 35,   0,  0,  0,  5,
         12, 23, 34, 45,  56, 67, 78, 89,
    };
    FontFileHeader header;

    CHECK_FALSE(font_parse_header(NULL, sizeof(data), &header));
    CHECK_FALSE(font_parse_header(data, -1, &header));
    CHECK_FALSE(font_parse_header(data, sizeof(data), NULL));

    return 1;
}

/*************************************************************************/
/********************* Character info parsing tests **********************/
/*************************************************************************/

TEST(test_v1_charinfo)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,120,119,118,117,244,243,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    FontFileCharInfo charinfo[2];
    ASSERT(lenof(charinfo) == charinfo_count);
    CHECK_TRUE(font_parse_charinfo(data, charinfo_count, 1, charinfo));
    CHECK_INTEQUAL(charinfo[0].ch, ' ');
    CHECK_INTEQUAL(charinfo[0].x, 1);
    CHECK_INTEQUAL(charinfo[0].y, 2);
    CHECK_INTEQUAL(charinfo[0].w, 3);
    CHECK_INTEQUAL(charinfo[0].h, 4);
    CHECK_INTEQUAL(charinfo[0].ascent, 5);
    CHECK_INTEQUAL(charinfo[0].prekern, 6);
    CHECK_INTEQUAL(charinfo[0].postkern, 7);
    CHECK_INTEQUAL(charinfo[1].ch, 0x10FFFF);
    CHECK_INTEQUAL(charinfo[1].x, 30839);
    CHECK_INTEQUAL(charinfo[1].y, 30325);
    CHECK_INTEQUAL(charinfo[1].w, 244);
    CHECK_INTEQUAL(charinfo[1].h, 243);
    CHECK_INTEQUAL(charinfo[1].ascent, -14);
    CHECK_INTEQUAL(charinfo[1].prekern, -15*256);
    CHECK_INTEQUAL(charinfo[1].postkern, -16*256);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_version_charinfo)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,120,119,118,117,116,115,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    CHECK_PARSE_CHARINFO_FAIL(data, charinfo_count, 0);
    CHECK_PARSE_CHARINFO_FAIL(data, charinfo_count, FONT_FILE_VERSION + 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_char_value)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
        255,255,255,255,120,119,118,117,116,115,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    CHECK_PARSE_CHARINFO_FAIL(data, charinfo_count, 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_negative_x)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,248,119,118,117,116,115,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    CHECK_PARSE_CHARINFO_FAIL(data, charinfo_count, 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_negative_y)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,120,119,246,117,116,115,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    CHECK_PARSE_CHARINFO_FAIL(data, charinfo_count, 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_bad_alignment_charinfo)
{
    static const ALIGNED(4) uint8_t data[] = {
        0,  // Force misalignment.
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,248,247,246,245,244,243,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = (sizeof(data)-1) / 16;

    CHECK_PARSE_CHARINFO_FAIL(data+1, charinfo_count, 1);

    return 1;
}

/*------------------------------------------------------------------------*/

TEST(test_invalid_params_charinfo)
{
    static const ALIGNED(4) uint8_t data[] = {
          0,  0,  0,' ',  0,  1,  0,  2,  3,  4,  5,  0,  0,  6,  0,  7,
          0, 16,255,255,248,247,246,245,244,243,242,  0,241,  0,240,  0,
    };
    const int charinfo_count = sizeof(data) / 16;

    FontFileCharInfo charinfo[2];
    ASSERT(lenof(charinfo) == charinfo_count);
    CHECK_FALSE(font_parse_charinfo(NULL, lenof(charinfo), 1, charinfo));
    CHECK_FALSE(font_parse_charinfo(data, lenof(charinfo), 1, NULL));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
