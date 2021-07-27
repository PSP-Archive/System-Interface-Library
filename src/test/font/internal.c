/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/internal.c: Helper routines for font tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/font/internal.h"
#include "src/test/graphics/internal.h"  // Borrow the graphics test helpers.
#include "src/texture.h"

/*************************************************************************/
/*************************************************************************/

void render_setup(int flip_v)
{
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_parallel_projection(
        0, TESTW, (flip_v ? TESTH : 0), (flip_v ? 0 : TESTH), -1, 1);
    graphics_set_view_matrix(&mat4_identity);
    graphics_set_model_matrix(&mat4_identity);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    graphics_enable_alpha_test(0);
    graphics_enable_depth_test(0);
    graphics_set_fixed_color(&(Vector4f){1,1,1,1});
    graphics_enable_fog(0);
};

/*-----------------------------------------------------------------------*/

int check_render_result(int x0, int y0, int w, int h, int exact,
                        const char *data)
{
#if defined(SIL_PLATFORM_PSP)
    /* The PSP seems to have trouble putting pixels in exactly the right
     * place, so give it a bit of extra leeway. */
    const int range = 31;
#else
    /* Other platforms get reasonably close. */
    const int range = 8;
#endif

    int texture;
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));

    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        int value = 0;
        if (x >= x0 && x < x0+w && y >= y0 && y < y0+h) {
            const char ch = data[((h-1)-(y-y0))*w + (x-x0)];
            ASSERT(ch == ' ' || ch == '.' || ch == ':' || ch == '#');
            switch (ch) {
                case '.': value =  64; break;
                case ':': value = 128; break;
                case '#': value = 255; break;
            }
        }
        if (exact) {
            CHECK_PIXEL(&pixels[i], value,value,value,255, x, y);
        } else {
            CHECK_PIXEL_NEAR(&pixels[i], value,value,value,255, range, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
