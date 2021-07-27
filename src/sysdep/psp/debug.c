/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/debug.c: PSP-specific debugging utility functions.
 */

#ifdef DEBUG  // To the end of the file.

#define IN_SYSDEP

#include "src/base.h"
#include "src/debug.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/internal.h"
#include "src/texture.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_debug_get_memory_stats(
    int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret)
{
    /*
     * We return memory according to the following map:
     *     0x8800000
     *        ...    > system memory
     *     Start of program code/data
     *        ...    > process memory
     *     End of program code/data
     *     Start of memory pools
     *        ...    > process memory if used, available memory if free
     *     End of memory pools
     *        ...    > system memory (sceKernelTotalFreeMemSize() bytes)
     *     Start of thread stacks
     *        ...    > process memory
     *     End of main thread stack
     */
    int sys = 0, self = 0, avail = 0;

    extern char __executable_start;
    ASSERT((intptr_t)&__executable_start >= 0x8800000);
    ASSERT((intptr_t)&__executable_start <= 0x8900000);
    sys += (intptr_t)&__executable_start - 0x8800000;

    void *main_base, *temp_base;
    uint32_t main_size, temp_size;
    psp_mem_get_pool_info(&main_base, &main_size, &temp_base, &temp_size);
    self += (intptr_t)main_base - (intptr_t)&__executable_start;

    const int pool_avail = sys_mem_avail(0) + sys_mem_avail(MEM_ALLOC_TEMP);
    self += (main_size + temp_size) - pool_avail;
    avail += pool_avail;

    SceKernelThreadInfo thread_info;
    thread_info.size = sizeof(thread_info);
    ASSERT(sceKernelReferThreadStatus(sceKernelGetThreadId(),
                                      &thread_info) == 0);
    const int stack_top = (intptr_t)thread_info.stack + thread_info.stackSize;
    const int unused_size =
        (stack_top - (intptr_t)main_base) - (main_size + temp_size);
    const int sys_free = sceKernelTotalFreeMemSize();
    sys += sys_free;
    self += unused_size - sys_free;

    *total_ret = sys + avail + self;
    *avail_ret = avail;
    *self_ret = self;
    return 1;
}

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

void psp_debug_display_memory_map(void)
{
    static const Vector4f color_background = {0, 0, 0, 0.75f};
    static const Vector4f color_text       = {1, 1, 1, 1};
    static const uint32_t colors[] = {
        [0]                  = 0xFF404040,  // Component order: 0xAABBGGRR
        [MEM_INFO_UNKNOWN+1] = 0xFFFFFFFF,
        [MEM_INFO_FONT   +1] = 0xFFFF0000,
        [MEM_INFO_MANAGE +1] = 0xFF0080FF,
        [MEM_INFO_SOUND  +1] = 0xFF00FF00,
        [MEM_INFO_TEXTURE+1] = 0xFF0000FF,
    };
    void *main_base, *temp_base;
    uint32_t main_size, temp_size;
    psp_mem_get_pool_info(&main_base, &main_size, &temp_base, &temp_size);


    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);

    texture_apply(0, 0);
    graphics_set_fixed_color(&color_background);
    const int16_t box_vertices[][2] = {
        {0, 0},
        {480, 0},
        {480, 14+debug_text_height()},
        {0, 14+debug_text_height()},
    };
    static const uint32_t box_vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2S, 0),
        0
    };
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, box_vertices, box_vertex_format,
        sizeof(*box_vertices), lenof(box_vertices));

    int8_t main_map[480*4], temp_map[480*4];
    mem_debug_get_map(main_base, main_size, main_map, sizeof(main_map));
    if (temp_size > 0) {
        mem_debug_get_map(temp_base, temp_size, temp_map, sizeof(temp_map));
    } else {
        memset(temp_map, -1, sizeof(temp_map));
    }

    uint32_t bar_pixels[9][480];
    mem_clear(bar_pixels, sizeof(bar_pixels));
    for (int i = 0; i < lenof(main_map); i++) {
        const int x = i/4, y = i%4;
        bar_pixels[y][x] = colors[main_map[i]+1];
        bar_pixels[y+5][x] = colors[temp_map[i]+1];
    }
    int texture = texture_create_with_data(
        lenof(bar_pixels[0]), lenof(bar_pixels), bar_pixels,
        TEX_FORMAT_PSP_RGBA8888, lenof(bar_pixels[0]), MEM_ALLOC_TEMP, 0);
    texture_apply(0, texture);
    graphics_set_fixed_color(&(Vector4f){1,1,1,1});
    struct BarVertex {int16_t x, y; float u, v;};
    static const struct BarVertex vertices[] = {
        {  0,  1, 0, 0},
        {480,  1, 1, 0},
        {480, 10, 1, 1},
        {  0, 10, 0, 1},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2S, offsetof(struct BarVertex,x)),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, offsetof(struct BarVertex,u)),
        0
    };
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
        sizeof(*vertices), lenof(vertices));
    texture_destroy(texture);

    debug_draw_text(0, 12, 1, &color_text, "Main: %dk/%dk free",
                    (int)mem_avail(0) / 1024, main_size / 1024);
    debug_draw_text(480, 12, -1, &color_text, "Temp: %dk/%dk free",
                    (int)mem_avail(MEM_ALLOC_TEMP) / 1024, temp_size / 1024);
}

/*-----------------------------------------------------------------------*/

void psp_debug_display_ge_info(void)
{
    static const Vector4f bg_color = {0, 0, 0, 0.5};
    static const Vector4f text_color = {1, 1, 1, 1};

    int gelist_used, gelist_used_max, gelist_size;
    int vertlist_used, vertlist_used_max, vertlist_size;
    ge_get_debug_info(&gelist_used, &gelist_used_max, &gelist_size,
                      &vertlist_used, &vertlist_used_max, &vertlist_size);

    const int lineheight = debug_text_height();
    int y = (DISPLAY_HEIGHT - (2+debug_text_height())) - 2*lineheight;
    const int x0 = 0 + debug_text_width("VLIST: ", 0);
    const int x1 = x0 + debug_text_width("00000/", 0);
    const int x2 = x1 + debug_text_width("00000/", 0);
    const int x3 = x2 + debug_text_width("000000", 0);

    debug_fill_box(0, y, graphics_display_width(), 2*lineheight, &bg_color);

    debug_draw_text(x0, y, -1, &text_color, "DLIST: ");
    debug_draw_text(x1, y, -1, &text_color, "%d/", gelist_used);
    debug_draw_text(x2, y, -1, &text_color, "%d/", gelist_used_max);
    debug_draw_text(x3, y, -1, &text_color, "%d", gelist_size);

    y += lineheight;

    debug_draw_text(x0, y, -1, &text_color, "VLIST: ");
    debug_draw_text(x1, y, -1, &text_color, "%d/", vertlist_used);
    debug_draw_text(x2, y, -1, &text_color, "%d/", vertlist_used_max);
    debug_draw_text(x3, y, -1, &text_color, "%d", vertlist_size);
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
