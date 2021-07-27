/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/font.c: PSP-specific font tests.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/test/base.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Test font from src/test/font/bitmap.c, adjusted for proper alignment. */
static const uint8_t font_data[] = {
    'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  5,  0, 16,
      0,  0,  0,128,  0,  0,  0,192,

      0,  0,  0,' ',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  0,
      0,  0,  0,'A',  0,  0,  0,  0,  5,  7,  7,  0,  0,  0,  1,  0,
      0,  0,  0,'B',  0,  5,  0,  0,  6,  7,  7,  0,255,192,  0,128,
      0,  0,  0,'p',  0, 11,  0,  0,  5,  6,  5,  0,  0,  0,  1,  0,
      0,  0, 32, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 64,

    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // 64-byte alignment.

    'T','E','X', 10,  2,116,  0,  0,  0, 16,  0,  8,  0,  1,  0,  0,
      0,  0,  0, 64,  0,  0,  0,128,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,255,  0,  0, 64,255,255,255,128,  0,255,  0,255,255,  0,
      0,255,  0,255,  0,  0,255,  0,  0,255,128,255,255,  0,  0,255,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,  0,  0,255,
    255,255,255,255,255,  0,255,255,255,128,  0,255,255,  0,  0,255,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,255,255,  0,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,  0,  0,  0,
    255,  0,  0,  0,255, 64,255,255,255,128,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,
};

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_psp_font(void);
int test_psp_font(void)
{
    CHECK_TRUE(graphics_init());
    const int result = do_test_psp_font();
    graphics_cleanup();
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_psp_font)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_bitmap_font_reuse)
{
    void *font_copy;
    ASSERT(font_copy = mem_alloc(sizeof(font_data), SIL_TEXTURE_ALIGNMENT, 0));
    memcpy(font_copy, font_data, sizeof(font_data));

    int font;
    CHECK_TRUE(font = font_parse_bitmap(font_copy, sizeof(font_data), 0, 1));

    /* The font memory should have been repurposed as texture memory. */
    int8_t map[1];
    mem_debug_get_map(font_copy, 1, map, 1);
    CHECK_TRUE(map[0] == MEM_INFO_TEXTURE);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bitmap_font_reuse_unaligned)
{
    uint8_t *font_copy;
    /* Debug logic will ensure that the buffer is misaligned with
     * respect to SIL_TEXTURE_ALIGNMENT. */
    ASSERT(font_copy = mem_alloc(sizeof(font_data),
                                 SIL_TEXTURE_ALIGNMENT / 2, 0));
    ASSERT((uintptr_t)font_copy % SIL_TEXTURE_ALIGNMENT != 0);
    memcpy(font_copy, font_data, sizeof(font_data));

    /* Font creation should still succeed... */
    int font;
    CHECK_TRUE(font = font_parse_bitmap(font_copy, sizeof(font_data), 0, 1));

    /* ... but the texture should have been allocated separately. */
    int8_t map[1];
    mem_debug_get_map(font_copy, 1, map, 1);
    CHECK_TRUE(map[0] != MEM_INFO_TEXTURE);

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
