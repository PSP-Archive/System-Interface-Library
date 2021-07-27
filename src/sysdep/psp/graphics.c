/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/graphics.c: PSP graphics and rendering functionality.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/debug.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/shader.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/framebuffer.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-mp3.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Structure for storing primitive data. */

struct SysPrimitive {
    SysPrimitive *next_to_free; // Next primitive in the to-free list.
    uint32_t vertex_format;  // Vertex format code for ge_set_vertex_format().
    uint8_t type;            // Primitive type code (GE_PRIMITIVE_*).
    uint8_t is_quads:1,      // True if this is a QUADS primitive.
            is_quad_strip:1, // True if this is a QUAD_STRIP primitive.
            blit_capable:1,  // True if this primitive satisfies requirements
                             //    for fast blitting.
            rendered:1,      // True if this primitive has ever been rendered.
            tex_h_adjust:1;  // True if texture coordinates were adjusted for
                             //    use with an over-high texture.
    uint8_t vertex_size;     // Size of a single vertex, in bytes.
    uint8_t index_size;      // Size of a single index, in bytes.
    uint16_t num_vertices;   // Number of vertices to render.
    uint16_t render_count;   // Count to pass to render calls (either
                             //    num_vertices or index count).
    void *vertices;          // Vertex data, reformatted for use by the GE.
    void *orig_vertices;     // Reformatted vertex data with original colors
                             //    (for applying primitive coloring).  NULL
                             //    if the vertices had no color data.
    void *indices;           // Index data, or NULL if none.
    uint32_t vertex_buf[1];  // Vertex data buffer (extended as necessary to
                             //    accommodate the vertex and index data).
};

/*-----------------------------------------------------------------------*/

/* Graphics capability structure returned to high-level code.  The PSP
 * only supports one graphics mode, so we don't have to check anything at
 * runtime. */

static const GraphicsDisplayModeEntry display_modes[] = {
    {.device = 0, .device_name = NULL,
     .width = DISPLAY_WIDTH, .height = DISPLAY_HEIGHT,
     .refresh = 60.0f/1.001f}
};
static const SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 0,
    .num_devices = 1,
    .num_modes = lenof(display_modes),
    .modes = display_modes,
};

/*-----------------------------------------------------------------------*/

/* Flag: Have we been initialized? */
static uint8_t initted;

/* Current display mode (pixel format) and bits per pixel. */
static uint8_t display_mode;
static uint8_t display_bpp;

/* Has a stencil buffer been requested? */
static uint8_t stencil_enabled;

/* Flag: Are we currently drawing a frame? */
static uint8_t in_frame;

/* Flag: Is the GE potentially busy? */
static uint8_t ge_busy;

/* Surface pointers for display and rendering framebuffers. */
static void *surfaces[2];

/* Index into surfaces[] of the currently-displayed framebuffer. */
static uint8_t displayed_surface;

/* surfaces[] index of and surface pointer for the rendering framebuffer. */
static uint8_t work_surface;
static uint32_t *work_pixels;

/* Pointer to the depth buffer for 3D operations. */
static uint16_t *depth_buffer;

/* Pointer to the top of video EDRAM (last addressable byte plus one). */
static uint8_t *vram_top;

/* Handle for the current buffer flip thread (0 = none active). */
static SceUID buffer_flip_thread;

/*-----------------------------------------------------------------------*/

/* Spare EDRAM block list, used by psp_vram_{alloc,free}() to manage memory.
 * The block list itself is kept in system RAM. */

typedef struct VRAMBlock VRAMBlock;
struct VRAMBlock {
    void *ptr;
    uint32_t size:31;
    uint32_t free:1;
};

static VRAMBlock *vram_blocks;
static int vram_blocks_size = 0;
static int vram_blocks_len = 0;

/*-----------------------------------------------------------------------*/

/* Currently bound framebuffer, or NULL if writing to the display. */
static SysFramebuffer *current_framebuffer;

/* Current draw buffer size, in pixels. */
static int16_t draw_buffer_width, draw_buffer_height;

/* Current viewport position and size, in pixels.  The X and Y coordinates
 * are as passed to sys_graphics_set_viewport(), with the origin at the
 * bottom-left corner. */
static int16_t view_x, view_y, view_w, view_h;

/* Current depth buffer range. */
static uint16_t depth_near, depth_far;

/* Current coordinate transformation matrices (saved so they can be
 * reloaded at the beginning of each frame, and for reference by
 * get_matrix_param()). */
static Matrix4f projection_matrix;
static Matrix4f view_matrix;
static Matrix4f model_matrix;
/* Is the current projection matrix a parallel projection? */
static uint8_t projection_is_parallel;
/* Are the current view and model matrices equal to the identity matrix? */
static uint8_t view_is_identity, model_is_identity;

/* Current primitive color (in 0xAABBGGRR format). */
static uint32_t primitive_color;

/* Current texture offsets. */
static float texture_offset_u, texture_offset_v;

/* Primitive to-free list.  Primitives on this list are freed at the next
 * ge_sync(), when we know they've been drawn. */
static SysPrimitive *primitive_to_free;

/* Buffer for storing data used by immediate-mode primitives. */
static SysPrimitive immediate_primitive;

/* Current alpha test state. */
static uint8_t alpha_test, alpha_test_comparison, alpha_reference;

/* Current blend state. */
static uint8_t blend_enabled;
static uint8_t blend_op, blend_src, blend_dest;
/* _is_color > 0 => CONSTANT, _is_color < 0 => INV_CONSTANT */
static int8_t blend_src_is_color, blend_dest_is_color;
static uint32_t blend_srcval, blend_destval, blend_color;

/* Current clip state.  The coordinates are as passed to
 * sys_graphics_set_clip_region(), with the origin at the bottom-left
 * corner. */
static uint8_t clip_enabled;
static int16_t clip_x, clip_y, clip_w, clip_h;

/* Current color write bitmask. */
static uint32_t color_mask;

/* Current depth test/update state. */
static uint8_t depth_test, depth_test_comparison, depth_write;

/* Current face culling state. */
static uint8_t face_cull;
static uint8_t face_cull_cw;  // Cull clockwise faces? (false = cull CCW faces)

/* Current fog state. */
static uint8_t fog;
static float fog_start, fog_end;
static uint32_t fog_color;  // 0xRRGGBB (alpha bits ignored)

/* Current stencil parameters. */
static uint8_t stencil_test;
static uint8_t stencil_comparison;
static uint8_t stencil_reference, stencil_mask;
static uint8_t stencil_op_sfail, stencil_op_dfail, stencil_op_dpass;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * convert_comparison:  Return the GE comparison type code corresponding
 * to the given SIL comparison type.
 *
 * [Parameters]
 *     type: SIL comparison type constant (GRAPHICS_COMPARISON_*).
 * [Return value]
 *     Corresponding GE test type (GE_TEST_*).
 */
static int convert_comparison(GraphicsComparisonType type);

/**
 * convert_stencil_op:  Return the GE stencil operation code corresponding
 * to the given SIL stencil operation.
 *
 * [Parameters]
 *     operation: SIL stencil operation constant (GRAPHICS_STENCIL_*).
 * [Return value]
 *     Corresponding GE stencil operation (GE_STENCIL_*).
 */
static int convert_stencil_op(GraphicsStencilOp operation);

/**
 * color_to_pixel:  Convert a color specified by floating-point component
 * values to a 32-bit pixel value.  The component values are assumed to be
 * in the range 0.0 to 1.0 inclusive.
 *
 * [Parameters]
 *     r, g, b, a: Red, green, blue, and alpha components of color.
 * [Return value]
 *     32-bit pixel value.
 */
static inline uint32_t color_to_pixel(float r, float g, float b, float a);

/**
 * update_state_*:  Each of these functions issues GE commands to apply
 * the state indicated by the function name.  These are called by the
 * sys_graphics_set_*_param() functions when changing state and by
 * sys_graphics_start_frame() to apply any state changes since the end of
 * the last frame.
 *
 * These functions may only be called while drawing a frame.
 */
static void update_state_alpha_test(void);
static void update_state_alpha_test_params(void);
static void update_state_blend(void);
static void update_state_blend_mode(void);
static void update_state_color_mask(void);
static void update_state_depth_range(void);
static void update_state_depth_test(void);
static void update_state_depth_test_comparison(void);
static void update_state_depth_write(void);
static void update_state_face_cull(void);
static void update_state_fog(void);
static void update_state_fog_params(void);
static void update_state_stencil(void);
static void update_state_stencil_func(void);
static void update_state_stencil_op(void);
static void update_state_texture_offset(void);

/**
 * update_viewport_and_clip_region:  Update the GE viewport and clip region
 * parameters with the current viewport settings.  (These need to be done
 * together because the GE does not clip transformed coordinates to [-1,+1].)
 */
static void update_viewport_and_clip_region(void);

/**
 * depth_buffer_present:  Return whether the current render target (either
 * a user framebuffer or the display buffer) has a depth buffer.
 */
static int depth_buffer_present(void);

/**
 * do_buffer_flip:  Flip buffers so that the current rendering framebuffer
 * is displayed during the next vertical retrace (only if
 * SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH is defined), then wait for the next
 * vertical blank before returning.
 *
 * Normally, this function is called as a thread function at the end of a
 * frame; sys_graphics_start_frame() can then check whether the thread has
 * terminated to determine whether a vertical sync has occurred (and thus
 * the buffer flip has completed) since the end of the last frame.  If the
 * thread cannot be started for any reason, this function can also be
 * called directly.
 *
 * [Parameters]
 *     args: Thread argument size (always sizeof(void *)).
 *     argp: Thread argument pointer (points to a pointer to the render
 *         buffer).
 */
static void do_buffer_flip(SceSize args, void *argp);

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    PRECOND(!initted, return NULL);

    if (UNLIKELY(!ge_init())) {
        return NULL;
    }
    int32_t res = sceDisplaySetMode(0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (UNLIKELY(res < 0)) {
        DLOG("sceDisplaySetMode() failed: %s", psp_strerror(res));
        return NULL;
    }

    mem_clear(sceGeEdramGetAddr(), sceGeEdramGetSize());
    sceKernelDcacheWritebackAll();
    displayed_surface = 0;
    work_surface = 1;

    display_mode = PSP_DISPLAY_PIXEL_FORMAT_8888;
    display_bpp = 32;
    stencil_enabled = 0;

    uint8_t *vram_spare_ptr;  // Pointer to the unused portion of video EDRAM.

    uintptr_t vram_addr = (uintptr_t)sceGeEdramGetAddr();
    uint32_t vram_size = sceGeEdramGetSize();
    const uint32_t frame_size =
        DISPLAY_STRIDE * DISPLAY_HEIGHT * (display_bpp/8);
    for (int i = 0; i < lenof(surfaces); i++) {
        surfaces[i] = (void *)(vram_addr + i*frame_size);
    }
    depth_buffer = (uint16_t *)(vram_addr + lenof(surfaces)*frame_size);
    vram_spare_ptr = (uint8_t *)(depth_buffer + DISPLAY_STRIDE*DISPLAY_HEIGHT);
    vram_top = (uint8_t *)(vram_addr + vram_size);
    work_pixels = (uint32_t *)surfaces[work_surface];
    sceDisplaySetFrameBuf(surfaces[displayed_surface], DISPLAY_STRIDE,
                          display_mode, PSP_DISPLAY_SETBUF_IMMEDIATE);

    /* Set up the video EDRAM free block list, with spare memory as a single
     * free block and the depth buffer as an allocated block (so we can free
     * it if client code requests a display mode with no depth buffer).  Keep
     * the list at the top of memory to minimize potential fragmentation. */
    vram_blocks = mem_alloc(sizeof(*vram_blocks) * 2, 4, MEM_ALLOC_TOP);
    if (UNLIKELY(!vram_blocks)) {
        DLOG("No memory for free VRAM block list, psp_vram_alloc() will fail");
        vram_blocks_size = 0;
        vram_blocks_len = 0;
    } else {
        vram_blocks[0].ptr = depth_buffer;
        vram_blocks[0].size = vram_spare_ptr - (uint8_t *)depth_buffer;
        vram_blocks[0].free = 0;
        vram_blocks[1].ptr = vram_spare_ptr;
        vram_blocks[1].size = vram_top - vram_spare_ptr;
        vram_blocks[1].free = 1;
        vram_blocks_size = 2;
        vram_blocks_len = 2;
    }

    draw_buffer_width = DISPLAY_WIDTH;
    draw_buffer_height = DISPLAY_HEIGHT;
    view_x = 0;
    view_y = 0;
    view_w = DISPLAY_WIDTH;
    view_h = DISPLAY_HEIGHT;
    depth_near = 0;
    depth_far = 65535;

    projection_matrix = mat4_identity;
    view_matrix = mat4_identity;
    model_matrix = mat4_identity;
    projection_is_parallel = 1;
    view_is_identity = 1;
    model_is_identity = 1;

    primitive_color = 0xFFFFFFFF;
    primitive_to_free = NULL;

    alpha_test = 0;
    alpha_test_comparison = GE_TEST_GEQUAL;
    alpha_reference = 0;

    blend_enabled = 1;
    blend_op = GE_BLEND_ADD;
    blend_src = GE_BLEND_SRC_ALPHA;
    blend_dest = GE_BLEND_ONE_MINUS_SRC_ALPHA;
    blend_src_is_color = 0;
    blend_dest_is_color = 0;
    blend_srcval = 0;
    blend_destval = 0;
    blend_color = 0;

    clip_enabled = 0;

    color_mask = 0;

    depth_test = 0;
    depth_test_comparison = GE_TEST_LESS;
    depth_write = 1;

    face_cull = 0;

    fog = 0;
    fog_start = 0;
    fog_end = 1;
    fog_color = 0xFFFFFF;

    stencil_test = 0;
    stencil_comparison = GE_TEST_ALWAYS;
    stencil_reference = 0;
    stencil_mask = 0xFF;
    stencil_op_sfail = GE_STENCIL_KEEP;
    stencil_op_dfail = GE_STENCIL_KEEP;
    stencil_op_dpass = GE_STENCIL_KEEP;

    psp_texture_init();
    texture_offset_u = 0;
    texture_offset_v = 0;

    initted = 1;
    return &graphics_info;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);

    if (in_frame) {
        sys_graphics_finish_frame();
    }
    sys_graphics_sync(0);

    mem_free(vram_blocks);
    vram_blocks = NULL;
    vram_blocks_size = 0;
    vram_blocks_len = 0;

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    return DISPLAY_WIDTH;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    return DISPLAY_HEIGHT;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "center_window") == 0) {
        return 1;  // Meaningless on the PSP.
    }

    if (strcmp(name, "depth_bits") == 0) {
        const int depth_bits = va_arg(args, int);
        if (depth_bits < 0) {
            DLOG("Invalid value for attribute %s: %d", name, depth_bits);
            return 0;
        } else if (depth_bits <= 16) {
            if (depth_bits == 0) {
                if (depth_buffer) {
                    psp_vram_free(depth_buffer);
                    depth_buffer = NULL;
                }
            } else {
                if (!depth_buffer) {
                    depth_buffer = psp_vram_alloc(
                        2 * (DISPLAY_STRIDE * DISPLAY_HEIGHT), 64);
                    if (UNLIKELY(!depth_buffer)) {
                        DLOG("Failed to reallocate depth buffer");
                        return 0;
                    }
                }
            }
            return 1;
        } else {
            DLOG("PSP doesn't support depth_bits > 16 (%d requested)",
                 depth_bits);
            return 0;
        }
    }

    if (strcmp(name, "device") == 0) {
        const int value = va_arg(args, int);
        return (value == 0);
    }

    if (strcmp(name, "multisample") == 0) {
        const int samples = va_arg(args, int);
        if (samples == 1) {
            return 1;
        } else {
            DLOG("PSP doesn't support multisampling");
            return 0;
        }
    }

    if (strcmp(name, "refresh_rate") == 0) {
        const float value = (float)va_arg(args, double);
        if (!(value >= 0)) {
            DLOG("Invalid value for attribute %s: %g", name, value);
            return 0;
        }
        return 1;
    }

    if (strcmp(name, "stencil_bits") == 0) {
        const int stencil_bits = va_arg(args, int);
        if (stencil_bits < 0) {
            DLOG("Invalid value for attribute %s: %d", name, stencil_bits);
            return 0;
        } else if (stencil_bits <= 8) {
            stencil_enabled = (stencil_bits > 0);
            return 1;
        } else {
            DLOG("PSP doesn't support stencil_bits > 8 (%d requested)",
                 stencil_bits);
            return 0;
        }
    }

    if (strcmp(name, "vsync") == 0) {
        const int vsync = va_arg(args, int);
        return (vsync != 0);  // Vsync is required.
    }

    if (strcmp(name, "window") == 0) {
        const int window = va_arg(args, int);
        return (window == 0);  // No windows on the PSP.
    }

    if (strcmp(name, "window_resizable") == 0) {
        return 1;  // No windows on the PSP.
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    /* Nothing to actually set -- just return whether the size matches
     * the PSP's display size. */
    if (width == DISPLAY_WIDTH && height == DISPLAY_HEIGHT) {
        return GRAPHICS_ERROR_SUCCESS;
    } else {
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }
}

/*-----------------------------------------------------------------------*/

const char *sys_graphics_renderer_info(void)
{
    return "PSP GE";
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return 0;  // No windows on the PSP.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_title(UNUSED const char *title)
{
    /* Nothing to do for the PSP. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_icon(UNUSED SysTexture *texture)
{
    /* Nothing to do for the PSP. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_resize_limits(
    UNUSED int min_width, UNUSED int min_height,
    UNUSED int max_width, UNUSED int max_height,
    UNUSED int min_aspect_x, UNUSED int min_aspect_y,
    UNUSED int max_aspect_x, UNUSED int max_aspect_y)
{
    /* Nothing to do for the PSP. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_show_mouse_pointer(UNUSED int on)
{
    /* Nothing to do for the PSP. */
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return 0;  // No system mouse pointer.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    *numerator_ret = 1001;
    *denominator_ret = 60000;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    /* If the exit-game (HOME button) popup is visible, we don't have focus. */
    SceCtrlData pad_data;
    sceCtrlPeekBufferPositive(&pad_data, 1);
    return !(pad_data.Buttons & PSP_CTRL_HOME);
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    *width_ret = DISPLAY_WIDTH;
    *height_ret = DISPLAY_HEIGHT;

    /* Make sure the previous frame has finished rendering -- otherwise
     * the (current) rendering buffer might still be displayed when the
     * GE starts writing to it. */
    sys_graphics_sync(0);

    ge_start_frame(display_mode);
    in_frame = 1;
    ge_busy = 1;

    ge_set_draw_buffer(NULL, 0);
    ge_set_depth_buffer(NULL, 0);
    update_viewport_and_clip_region();

    ge_set_projection_matrix(&projection_matrix);
    ge_set_view_matrix(&view_matrix);
    ge_set_model_matrix(&model_matrix);

    ge_set_shade_mode(GE_SHADE_GOURAUD);  // We always keep this enabled.

    update_state_alpha_test();
    update_state_alpha_test_params();
    update_state_blend();
    update_state_blend_mode();
    update_state_color_mask();
    update_state_depth_range();
    update_state_depth_test();
    update_state_depth_test_comparison();
    update_state_depth_write();
    update_state_face_cull();
    update_state_fog();
    update_state_fog_params();
    update_state_stencil();
    update_state_stencil_func();
    update_state_stencil_op();
    update_state_texture_offset();

    psp_set_texture_state(1);

    ge_set_ambient_color(0xFFFFFFFF);
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    in_frame = 0;

#ifdef SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH
# ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_GPU_WAIT_START);
# endif
    ge_end_frame();
    ge_busy = 0;
# ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_GPU_WAIT_END);
# endif
    sceDisplaySetFrameBuf(work_pixels, DISPLAY_STRIDE, display_mode,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
#endif

    buffer_flip_thread = psp_start_thread("BufferFlipThread",
                                          (void *)do_buffer_flip,
                                          THREADPRI_MAIN, 1024,
                                          sizeof(work_pixels), &work_pixels);
    if (UNLIKELY(buffer_flip_thread < 0)) {
        DLOG("Failed to start buffer flip thread: %s",
             psp_strerror(buffer_flip_thread));
        buffer_flip_thread = 0;
        do_buffer_flip(sizeof(work_pixels), &work_pixels);
    }
    displayed_surface = work_surface;
    work_surface = (work_surface + 1) % lenof(surfaces);
    work_pixels = (uint32_t *)surfaces[work_surface];
}

/*-----------------------------------------------------------------------*/

void sys_graphics_sync(int flush)
{
    if (buffer_flip_thread) {
        SceUInt timeout = 5*1001000/60;  // Don't wait longer than 5 frames.
        if (UNLIKELY(sceKernelWaitThreadEnd(buffer_flip_thread,
                                            &timeout) < 0)) {
            sceKernelTerminateThread(buffer_flip_thread);
        }
        sceKernelDeleteThread(buffer_flip_thread);
        buffer_flip_thread = 0;
    } else if (in_frame) {
        ge_sync();
    }

    /* This is as good a place as any for periodic housekeeping. */
    psp_clean_mp3_garbage(0);

    /* Free everything on the primitive to-free list, since we know the
     * GE is done with all the data.  (We do this regardless of the
     * value of the "flush" parameter.) */
    SysPrimitive *primitive, *next;
    for (primitive = primitive_to_free; primitive; primitive = next) {
        next = primitive->next_to_free;
        mem_free(primitive);
    }
    primitive_to_free = NULL;

    /* Similarly, free all textures whose destruction was deferred. */
    psp_texture_flush_deferred_destroy_list();

    /* If an explicit flush was requested, also trim the VRAM free block
     * array buffer to its current length. */
    if (flush && vram_blocks_len < vram_blocks_size) {
        VRAMBlock *new_blocks = mem_realloc(
            vram_blocks, sizeof(*vram_blocks) * vram_blocks_len, MEM_ALLOC_TOP);
        if (new_blocks) {
            vram_blocks = new_blocks;
            vram_blocks_size = vram_blocks_len;
        }
    }
}

/*************************************************************************/
/***************** Interface: Render state manipulation ******************/
/*************************************************************************/

void sys_graphics_set_viewport(int left, int bottom, int width, int height)
{
    const int dispw = psp_framebuffer_width();
    const int disph = psp_framebuffer_height();

    view_x = bound(left, 0, dispw-1);
    view_y = bound(bottom, 0, disph-1);
    view_w = bound(width, 1, dispw - view_x);
    view_h = bound(height, 1, disph - view_y);
    update_viewport_and_clip_region();
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_clip_region(int left, int bottom, int width, int height)
{
    const int dispw = psp_framebuffer_width();
    const int disph = psp_framebuffer_height();

    clip_x = bound(left, 0, dispw);
    clip_y = bound(bottom, 0, disph);
    clip_w = bound(width, 0, dispw - clip_x);
    clip_h = bound(height, 0, disph - clip_y);
    update_viewport_and_clip_region();
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_depth_range(float near, float far)
{
    /* Round so that, for example, a value of 0.5 corresponds to 32767 for
     * near and 32768 for far (thus ensuring that adjacent ranges do not
     * overlap). */
    depth_near = ifloorf(near*65535);
    depth_far = iceilf(far*65535);
    if (in_frame) {
        update_state_depth_range();
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_blend(int operation, int src_factor, int dest_factor)
{
    int new_op = -1;  // Detect invalid input.
    switch ((GraphicsBlendOperation)operation) {
        case GRAPHICS_BLEND_ADD:  new_op = GE_BLEND_ADD;              break;
        case GRAPHICS_BLEND_SUB:  new_op = GE_BLEND_SUBTRACT;         break;
        case GRAPHICS_BLEND_RSUB: new_op = GE_BLEND_REVERSE_SUBTRACT; break;
    }
    ASSERT(new_op != -1, return 0);

    if ((new_op == GE_BLEND_ADD || new_op == GE_BLEND_SUBTRACT)
     && src_factor == 1 && dest_factor == 0) {

        blend_enabled = 0;
        blend_op = new_op;

    } else {

        int new_src = -1, new_dest = -1;
        uint32_t new_srcval = blend_srcval, new_destval = blend_destval;
        int new_src_is_color = 0, new_dest_is_color = 0;

        switch ((GraphicsBlendFactor)src_factor) {
          case GRAPHICS_BLEND_ZERO:
            new_src = GE_BLEND_FIX;
            new_srcval = 0x000000;
            break;
          case GRAPHICS_BLEND_ONE:
            new_src = GE_BLEND_FIX;
            new_srcval = 0xFFFFFF;
            break;
          case GRAPHICS_BLEND_SRC_COLOR:
            DLOG("SRC_COLOR not supported for source blend factor");
            return 0;
          case GRAPHICS_BLEND_SRC_ALPHA:
            new_src = GE_BLEND_SRC_ALPHA;
            break;
          case GRAPHICS_BLEND_INV_SRC_ALPHA:
            new_src = GE_BLEND_ONE_MINUS_SRC_ALPHA;
            break;
          case GRAPHICS_BLEND_DEST_COLOR:
            new_src = GE_BLEND_COLOR;
            break;
          case GRAPHICS_BLEND_DEST_ALPHA:
          case GRAPHICS_BLEND_INV_DEST_ALPHA:
            DLOG("DEST_ALPHA not supported");
            return 0;
          case GRAPHICS_BLEND_CONSTANT:
            new_src = GE_BLEND_FIX;
            new_srcval = blend_color;
            new_src_is_color = 1;
            break;
          case GRAPHICS_BLEND_INV_CONSTANT:
            new_src = GE_BLEND_FIX;
            new_srcval = blend_color ^ 0xFFFFFF;
            new_src_is_color = -1;
            break;
        }
        ASSERT(new_src != -1, return 0);

        switch (dest_factor) {
          case GRAPHICS_BLEND_ZERO:
            new_dest = GE_BLEND_FIX;
            new_destval = 0x000000;
            break;
          case GRAPHICS_BLEND_ONE:
            new_dest = GE_BLEND_FIX;
            new_destval = 0xFFFFFF;
            break;
          case GRAPHICS_BLEND_SRC_COLOR:
            new_dest = GE_BLEND_COLOR;
            break;
          case GRAPHICS_BLEND_SRC_ALPHA:
            new_dest = GE_BLEND_SRC_ALPHA;
            break;
          case GRAPHICS_BLEND_INV_SRC_ALPHA:
            new_dest = GE_BLEND_ONE_MINUS_SRC_ALPHA;
            break;
          case GRAPHICS_BLEND_DEST_COLOR:
            DLOG("DEST_COLOR not supported for destination blend factor");
            return 0;
          case GRAPHICS_BLEND_DEST_ALPHA:
          case GRAPHICS_BLEND_INV_DEST_ALPHA:
            DLOG("DEST_ALPHA not supported");
            return 0;
          case GRAPHICS_BLEND_CONSTANT:
            new_dest = GE_BLEND_FIX;
            new_destval = blend_color;
            new_dest_is_color = 1;
            break;
          case GRAPHICS_BLEND_INV_CONSTANT:
            new_dest = GE_BLEND_FIX;
            new_destval = blend_color ^ 0xFFFFFF;
            new_dest_is_color = -1;
            break;
        }
        ASSERT(new_dest != -1, return 0);

        blend_enabled = 1;
        blend_op = new_op;
        blend_src = new_src;
        blend_dest = new_dest;
        blend_srcval = new_srcval;
        blend_destval = new_destval;
        blend_src_is_color = new_src_is_color;
        blend_dest_is_color = new_dest_is_color;

    }  // if (blend enabled or disabled)

    if (in_frame) {
        update_state_blend();
        update_state_blend_mode();
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_blend_alpha(int enable, UNUSED int src_factor,
                                 UNUSED int dest_factor)
{
    /* The PSP doesn't support alpha framebuffers. */
    return enable == 0;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_int_param(SysGraphicsParam id, int value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
        alpha_test = (value != 0);
        if (in_frame) {
            update_state_alpha_test();
        }
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
        alpha_test_comparison = convert_comparison(value);
        if (in_frame) {
            update_state_alpha_test_params();
        }
        return;

      case SYS_GRAPHICS_PARAM_CLIP:
        clip_enabled = (value != 0);
        update_viewport_and_clip_region();
        return;

      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
        color_mask = ((value & 1<<0) ? 0 : 0xFF) <<  0
                   | ((value & 1<<1) ? 0 : 0xFF) <<  8
                   | ((value & 1<<2) ? 0 : 0xFF) << 16;
        if (in_frame) {
            update_state_color_mask();
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
        depth_test = (value != 0);
        if (in_frame) {
            update_state_depth_test();
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
        depth_test_comparison = convert_comparison(value);
        if (in_frame) {
            update_state_depth_test_comparison();
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
        depth_write = (value != 0);
        if (in_frame) {
            update_state_depth_write();
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL:
        face_cull = (value != 0);
        if (in_frame) {
            update_state_face_cull();
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
        face_cull_cw = (value != 0);
        if (in_frame) {
            update_state_face_cull();
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG:
        fog = (value != 0);
        if (in_frame) {
            update_state_fog();
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
        stencil_test = (value != 0);
        if (in_frame) {
            update_state_stencil();
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
        /* Like OpenGL, the GE has the relational comparisons backwards, so
         * fix them up. */
        switch (value) {
          case GRAPHICS_COMPARISON_LESS:
            stencil_comparison = GE_TEST_GREATER;
            break;
          case GRAPHICS_COMPARISON_LESS_EQUAL:
            stencil_comparison = GE_TEST_GEQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER_EQUAL:
            stencil_comparison = GE_TEST_LEQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER:
            stencil_comparison = GE_TEST_LESS;
            break;
          default:
            stencil_comparison = convert_comparison(value);
            break;
        }
        /* Value will be passed to the GE with STENCIL_MASK. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
        stencil_reference = (uint8_t)value;
        /* Value will be passed to the GE with STENCIL_MASK. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
        stencil_mask = (uint8_t)value;
        if (in_frame) {
            update_state_stencil_func();
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
        stencil_op_sfail = convert_stencil_op(value);
        /* Value will be passed to the GE with STENCIL_OP_DPASS. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
        stencil_op_dfail = convert_stencil_op(value);
        /* Value will be passed to the GE with STENCIL_OP_DPASS. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
        stencil_op_dpass = convert_stencil_op(value);
        if (in_frame) {
            update_state_stencil_op();
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_float_param(SysGraphicsParam id, float value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
        alpha_reference = bound(iroundf(value*255), 0, 255);
        if (in_frame) {
            update_state_alpha_test_params();
        }
        return;

      case SYS_GRAPHICS_PARAM_POINT_SIZE:
        if (value != 1) {
            DLOG("Warning: point size %.3f unsupported", value);
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_START:
        fog_start = value;
        if (in_frame) {
            update_state_fog_params();
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_END:
        fog_end = value;
        if (in_frame) {
            update_state_fog_params();
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_vec2_param(SysGraphicsParam id, const Vector2f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        texture_offset_u = value->x;
        texture_offset_v = value->y;
        if (in_frame) {
            update_state_texture_offset();
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_vec4_param(SysGraphicsParam id, const Vector4f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
        blend_color = bound(iroundf(value->x*255),0,255) <<  0
                    | bound(iroundf(value->y*255),0,255) <<  8
                    | bound(iroundf(value->z*255),0,255) << 16;
        if (blend_src_is_color || blend_dest_is_color) {
            if (blend_src_is_color > 0) {
                blend_srcval = blend_color;
            } else if (blend_src_is_color < 0) {
                blend_srcval = blend_color ^ 0xFFFFFF;
            }
            if (blend_dest_is_color > 0) {
                blend_destval = blend_color;
            } else if (blend_dest_is_color < 0) {
                blend_destval = blend_color ^ 0xFFFFFF;
            }
            if (in_frame) {
                update_state_blend_mode();
            }
        }
        return;

      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
        primitive_color = bound(iroundf(value->x*255),0,255) <<  0
                        | bound(iroundf(value->y*255),0,255) <<  8
                        | bound(iroundf(value->z*255),0,255) << 16
                        | bound(iroundf(value->w*255),0,255) << 24;
        return;

      case SYS_GRAPHICS_PARAM_FOG_COLOR:
        fog_color = bound(iroundf(value->x*255),0,255) <<  0
                  | bound(iroundf(value->y*255),0,255) <<  8
                  | bound(iroundf(value->z*255),0,255) << 16;
        if (in_frame) {
            update_state_fog_params();
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_matrix_param(SysGraphicsParam id, const Matrix4f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
        projection_matrix = *value;
        if (in_frame) {
            ge_set_projection_matrix(value);
            if (fog) {
                update_state_fog_params();
            }
        }
        projection_is_parallel = (value->_12 == 0
                               && value->_13 == 0
                               && value->_14 == 0
                               && value->_21 == 0
                               && value->_23 == 0
                               && value->_24 == 0
                               && value->_31 == 0
                               && value->_32 == 0
                               && value->_34 == 0
                               && value->_44 == 1);
        return;

      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
        view_matrix = *value;
        if (in_frame) {
            ge_set_view_matrix(value);
        }
        view_is_identity =
            (memcmp(value, &mat4_identity, sizeof(mat4_identity)) == 0);
        return;

      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
        model_matrix = *value;
        if (in_frame) {
            ge_set_model_matrix(value);
        }
        model_is_identity =
            (memcmp(value, &mat4_identity, sizeof(mat4_identity)) == 0);
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_matrix_param(SysGraphicsParam id, Matrix4f *value_ret)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
        *value_ret = projection_matrix;
        return;

      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
        *value_ret = view_matrix;
        return;

      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
        *value_ret = model_matrix;
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type");  // NOTREACHED
        return;  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

float sys_graphics_max_point_size(void)
{
    return 1;
}

/*************************************************************************/
/************** Interface: Primitive creation and rendering **************/
/*************************************************************************/

SysPrimitive *sys_graphics_create_primitive(
    GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count, const void *index_data, int index_size,
    int index_count, int immediate)
{
    if (!index_data) {
        if (count > 65535) {
            DLOG("Too many vertices (%u > 65535)", count);
            goto error_return;
        }
        index_size = 1;
        index_count = 0;
    } else {
        if (index_count > 65535) {
            DLOG("Too many indices (%u > 65535)", count);
            goto error_return;
        }
        if (index_size == 4) {
            DLOG("32-bit index data not supported");
            goto error_return;
        }
    }

    if (UNLIKELY(immediate && !in_frame)) {
        DLOG("Trying to create an immediate primitive outside a frame,"
             " ignoring immediate flag");
        immediate = 0;
    }

    uint32_t vertex_format = GE_VERTEXFMT_TRANSFORM_3D;
    int position_size = 0, texcoord_size = 0, color_size = 0;
    enum {NONE, UINT8, INT16, FLOAT32}
        position_type = NONE, texcoord_type = NONE, color_type = NONE;
    int position_offset = 0, texcoord_offset = 0, color_offset = 0;
    for (int i = 0; format[i] != 0; i++) {
        const int offset = GRAPHICS_VERTEX_FORMAT_OFFSET(format[i]);
        switch (GRAPHICS_VERTEX_FORMAT_TYPE(format[i])) {

          case GRAPHICS_VERTEX_POSITION_2S:
            vertex_format |= GE_VERTEXFMT_VERTEX_32BITF;
            position_size = 2;
            position_type = INT16;
            position_offset = offset;
            break;
          case GRAPHICS_VERTEX_POSITION_2F:
            vertex_format |= GE_VERTEXFMT_VERTEX_32BITF;
            position_size = 2;
            position_type = FLOAT32;
            position_offset = offset;
            break;
          case GRAPHICS_VERTEX_POSITION_3F:
            vertex_format |= GE_VERTEXFMT_VERTEX_32BITF;
            position_size = 3;
            position_type = FLOAT32;
            position_offset = offset;
            break;
          case GRAPHICS_VERTEX_POSITION_4F:
            DLOG("4-component positions not supported");
            goto error_return;

          case GRAPHICS_VERTEX_TEXCOORD_2F:
            vertex_format |= GE_VERTEXFMT_TEXTURE_32BITF;
            texcoord_size = 2;
            texcoord_type = FLOAT32;
            texcoord_offset = offset;
            break;

          case GRAPHICS_VERTEX_COLOR_4NUB:
            vertex_format |= GE_VERTEXFMT_COLOR_8888;
            color_size = 4;
            color_type = UINT8;
            color_offset = offset;
            break;
          case GRAPHICS_VERTEX_COLOR_4F:
            vertex_format |= GE_VERTEXFMT_COLOR_8888;
            color_size = 4;
            color_type = FLOAT32;
            color_offset = offset;
            break;

          default:
            DLOG("Unknown vertex data format 0x%08X", format[i]);
            goto error_return;

        }  // switch (GRAPHICS_VERTEX_FORMAT_TYPE(format[i]))
    }  // for (int i = 0; format[i] != 0; i++)

    if (!(vertex_format & GE_VERTEXFMT_VERTEX_MASK)) {
        DLOG("No position data in vertices, nothing to draw");
        goto error_return;
    }

    int out_size = 4*3;  // Size of converted data.
    if (vertex_format & GE_VERTEXFMT_TEXTURE_MASK) {
        out_size += 4*2;
    }
    if (vertex_format & GE_VERTEXFMT_COLOR_MASK) {
        out_size += 4;
    }
    ASSERT(out_size < 256, goto error_return);
    int alloc_count = count;  // Number of vertices to allocate.
    if ((vertex_format & GE_VERTEXFMT_COLOR_MASK) && !immediate) {
        alloc_count *= 2;  // Save a copy of the unmodified colors.
    }

    const uint32_t total_size = offsetof(SysPrimitive,vertex_buf)
                              + out_size * alloc_count
                              + index_size * index_count;

    /* If this is an immediate-mode primitive, we'll write directly into
     * the global display list and vertex buffer (if possible) to avoid
     * wasting time with memory allocation/release. */
    SysPrimitive *primitive;
    void *immediate_vertices = NULL, *immediate_indices = NULL;
    if (immediate) {
        immediate_vertices = ge_reserve_vertexbytes(out_size * count);
        if (UNLIKELY(!immediate_vertices)) {
            DLOG("No room in vertex buffer for immediate-mode vertices"
                 " (%d*%d bytes)", out_size, count);
            immediate = 0;
        }
    }
    if (immediate && index_data) {
        immediate_indices = ge_reserve_vertexbytes(index_size * index_count);
        if (UNLIKELY(!immediate_indices)) {
            DLOG("No room in vertex buffer for immediate-mode indices"
                 " (%d*%d bytes)", index_size, index_count);
            /* No way to free the allocated vertex memory, so we'll just
             * have to leak it. */
            immediate = 0;
        }
    }
    if (immediate) {
        primitive = &immediate_primitive;
        primitive->vertices = immediate_vertices;
        primitive->orig_vertices = NULL;
        primitive->indices = immediate_indices;
    } else {
        primitive = mem_alloc(total_size, 4, 0);
        if (UNLIKELY(!primitive)) {
            DLOG("No memory for primitive data (%u bytes)", total_size);
            goto error_return;
        }
        primitive->vertices = primitive->vertex_buf;
        if (vertex_format & GE_VERTEXFMT_COLOR_8888) {
            primitive->orig_vertices =
                (uint8_t *)primitive->vertices + (out_size * count);
        } else {
            primitive->orig_vertices = NULL;
        }
        if (index_data) {
            primitive->indices =
                (uint8_t *)primitive->vertices + (out_size * alloc_count);
        } else {
            primitive->indices = NULL;
        }
    }

    primitive->next_to_free  = NULL;
    primitive->is_quads      = 0;
    primitive->is_quad_strip = 0;
    primitive->vertex_format = vertex_format;
    primitive->rendered      = 0;
    primitive->tex_h_adjust  = 0;
    primitive->vertex_size   = out_size;
    primitive->index_size    = index_size;
    primitive->num_vertices  = count;

    primitive->type = 0xFF;  // Invalid value.
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
        primitive->type = GE_PRIMITIVE_POINTS;
        break;
      case GRAPHICS_PRIMITIVE_LINES:
        primitive->type = GE_PRIMITIVE_LINES;
        break;
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
        primitive->type = GE_PRIMITIVE_LINE_STRIP;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLES:
        primitive->type = GE_PRIMITIVE_TRIANGLES;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
        primitive->type = GE_PRIMITIVE_TRIANGLE_STRIP;
        break;
      case GRAPHICS_PRIMITIVE_QUADS:
        primitive->type = GE_PRIMITIVE_TRIANGLE_STRIP;
        primitive->is_quads = 1;
        break;
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        primitive->type = GE_PRIMITIVE_TRIANGLE_STRIP;
        primitive->is_quad_strip = 1;
        /* Make sure we don't draw half a quad if the vertex count is odd.
         * But avoid letting the count go to zero (to avoid potential
         * glitches). */
        if (index_data) {
            if (index_count >= 2) {
                index_count &= ~1;
            }
        } else {
            if (count >= 2) {
                count &= ~1;
            }
        }
        break;
    }
    ASSERT(primitive->type != 0xFF, return NULL);

    const void *src = data;
    uint8_t *dest_base = (uint8_t *)primitive->vertices;

    for (int i = 0; i < count; i++, src = (uint8_t *)src + size) {

        union {int32_t i; float f;} *dest;
        int index = i;
        if (primitive->is_quads && !index_data && (i & 2) == 2) {
            /* Swap the third and fourth vertices of quads so we can
             * render them as triangle strips.  If the primitive is
             * indexed, we instead swap the index values below. */
            index ^= 1;
        }
        dest = (void *)(dest_base + (index * out_size));

        if (vertex_format & GE_VERTEXFMT_TEXTURE_MASK) {
            ASSERT(texcoord_type == FLOAT32);
            if (UNLIKELY(((uintptr_t)src + texcoord_offset) % 4 != 0)) {
                DLOG("Vertex %d: misaligned texture coordinates (%p),"
                     " aborting primitive",
                     i, (uint8_t *)src + texcoord_offset);
                goto error_free_primitive;
            }
            /* We cast through uintptr_t instead of char * or uint8_t * to
             * avoid cast-align warnings. */
            const float *texcoord =
                (const float *)((uintptr_t)src + texcoord_offset);
            const float u = texcoord[0];
            const float v = texcoord[1];
            dest[0].f = u;
            dest[1].f = v;
            dest += 2;
        }

        if (vertex_format & GE_VERTEXFMT_COLOR_8888) {
            if (color_type == UINT8) {
                /* This should normally be aligned, but use an unaligned
                 * load just in case. */
                __asm__("ulw %0, %1"
                        : "=r" (dest[0].i)
                        : "m" (*(uint32_t *)((uintptr_t)src + color_offset)));
            } else {
                ASSERT(color_type == FLOAT32);
                if (UNLIKELY(((uintptr_t)src + color_offset) % 4 != 0)) {
                    DLOG("Vertex %d: misaligned color data (%p), aborting"
                         " primitive", i, (uint8_t *)src + color_offset);
                    goto error_free_primitive;
                }
                const float *color =
                    (const float *)((uintptr_t)src + color_offset);
                const int r = iroundf(color[0]*255);
                const int g = iroundf(color[1]*255);
                const int b = iroundf(color[2]*255);
                const int a = iroundf(color[3]*255);
                dest[0].i = bound(a,0,255) << 24
                          | bound(b,0,255) << 16
                          | bound(g,0,255) <<  8
                          | bound(r,0,255) <<  0;
            }
            dest++;
        }

        if (position_type == INT16) {
            if (UNLIKELY(((uintptr_t)src + position_offset) % 2 != 0)) {
                DLOG("Vertex %d: misaligned position data (%p), aborting"
                     " primitive", i, (uint8_t *)src + position_offset);
                goto error_free_primitive;
            }
            const int16_t *position =
                (const int16_t *)((uintptr_t)src + position_offset);
            const float x = position[0];
            const float y = position[1];
            const float z = 0;
            dest[0].f = x;
            dest[1].f = y;
            dest[2].f = z;
        } else {
            ASSERT(position_type == FLOAT32);
            if (UNLIKELY(((uintptr_t)src + position_offset) % 4 != 0)) {
                DLOG("Vertex %d: misaligned position data (%p), aborting"
                     " primitive", i, (uint8_t *)src + position_offset);
                goto error_free_primitive;
            }
            const float *position =
                (const float *)((uintptr_t)src + position_offset);
            const float x = position[0];
            const float y = position[1];
            const float z = (position_size == 3) ? position[2] : 0;
            dest[0].f = x;
            dest[1].f = y;
            dest[2].f = z;
        }
        dest += 3;

    }  // for (int i = 0; i < count; i++, src += size)

    if (primitive->orig_vertices) {
        memcpy(primitive->orig_vertices, primitive->vertices, out_size * count);
    }

    if (index_data) {
        memcpy(primitive->indices, index_data, index_size * index_count);
        primitive->vertex_format |= (index_size == 1
                                     ? GE_VERTEXFMT_INDEX_8BIT
                                     : GE_VERTEXFMT_INDEX_16BIT);
        primitive->render_count = index_count;
        if (primitive->is_quads) {
            if (index_size == 2) {
                uint16_t *indices16 = (uint16_t *)primitive->indices;
                for (int i = 0; i+4 <= index_count; i += 4) {
                    uint16_t temp  = indices16[i+2];
                    indices16[i+2] = indices16[i+3];
                    indices16[i+3] = temp;
                }
            } else {
                uint8_t *indices8 = (uint8_t *)primitive->indices;
                for (int i = 0; i+4 <= index_count; i += 4) {
                    uint8_t temp  = indices8[i+2];
                    indices8[i+2] = indices8[i+3];
                    indices8[i+3] = temp;
                }
            }
        }
    } else {
        primitive->render_count = count;
    }

    primitive->blit_capable = (
        primitive->is_quads && primitive->render_count == 4
        && (primitive->vertex_format == GE_VERTEXFMT_VERTEX_32BITF
            || primitive->vertex_format == (GE_VERTEXFMT_VERTEX_32BITF
                                            | GE_VERTEXFMT_TEXTURE_32BITF)));
    if (primitive->blit_capable) {
        const float *vertex0 = primitive->vertices;
        const float *vertex1 = vertex0 + (primitive->vertex_size / 4);
        const float *vertex2 = vertex1 + (primitive->vertex_size / 4);
        const float *vertex3 = vertex2 + (primitive->vertex_size / 4);
        const int has_texture =
            ((primitive->vertex_format & GE_VERTEXFMT_TEXTURE_MASK) != 0);
        const float x0 = vertex0[has_texture ? 2 : 0];
        const float y0 = vertex0[has_texture ? 3 : 1];
        const float x1 = vertex1[has_texture ? 2 : 0];
        const float y1 = vertex1[has_texture ? 3 : 1];
        const float x2 = vertex2[has_texture ? 2 : 0];
        const float y2 = vertex2[has_texture ? 3 : 1];
        const float x3 = vertex3[has_texture ? 2 : 0];
        const float y3 = vertex3[has_texture ? 3 : 1];
        /* We can't check here whether the vertex and texture coordinate
         * directions match since we don't know the orientation of the
         * projection matrix which will be used when the primitive is
         * rendered, so we just check that both quads are axis-aligned and
         * non-degenerate. */
        primitive->blit_capable = (
               x3 != x0
            && y3 != y0
            && ((x1 == x0 && y1 == y3 && x2 == x3 && y2 == y0)
             || (x1 == x3 && y1 == y0 && x2 == x0 && y2 == y3)));
        if (primitive->blit_capable
            && primitive->vertex_format == (GE_VERTEXFMT_VERTEX_32BITF
                                            | GE_VERTEXFMT_TEXTURE_32BITF)) {
            const float u0 = vertex0[0];
            const float v0 = vertex0[1];
            const float u3 = vertex3[0];
            const float v3 = vertex3[1];
            primitive->blit_capable = (
                   (u0 >= 0 && u0 <= 1)
                && (v0 >= 0 && v0 <= 1)
                && (u3 >= 0 && u3 <= 1)
                && (v3 >= 0 && v3 <= 1)
                && u3 != u0
                && v3 != v0
                && ((vertex1[0] == u0 && vertex1[1] == v3
                     && vertex2[0] == u3 && vertex2[1] == v0)
                 || (vertex1[0] == u3 && vertex1[1] == v0
                     && vertex2[0] == u0 && vertex2[1] == v3)));
        }
    }

    if (primitive != &immediate_primitive) {
        sceKernelDcacheWritebackRange(primitive, total_size);
    }
    return primitive;

  error_free_primitive:
    if (primitive != &immediate_primitive) {
        mem_free(primitive);
    }
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_draw_primitive(SysPrimitive *primitive, int start, int count)
{
    if (UNLIKELY(!in_frame)) {
        DLOG("Trying to draw a primitive outside a frame, ignoring");
        return;
    }

    if (start >= primitive->render_count) {
        return;
    }
    if (count < 0 || count > primitive->render_count - start) {
        count = primitive->render_count - start;
    }
    /* Make sure not to draw extra triangles for quad primitives which were
     * converted to TRIANGLE_STRIP. */
    if (primitive->is_quads) {
        count = (count/4) * 4;
    } else if (primitive->is_quad_strip) {
        count = (count/2) * 2;
    }
    if (count == 0) {
        return;
    }

    const SysTexture *texture = psp_current_texture();
    psp_set_texture_state(0);

    const int fb_width = psp_framebuffer_width();
    const int fb_height = psp_framebuffer_height();
    if (primitive->blit_capable
        && projection_is_parallel
        && view_is_identity
        && model_is_identity
        && !current_framebuffer
        && view_w == fb_width
        && view_h == fb_height
        && (primitive->vertex_format == GE_VERTEXFMT_VERTEX_32BITF
            || (primitive_color == 0xFFFFFFFFU
                && texture != NULL
                && (texture->format == GE_TEXFMT_8888
                    || texture->format == GE_TEXFMT_T8)
                && !texture->antialias
                && texture_offset_u == 0
                && texture_offset_v == 0)))
    {
        ASSERT(count == 4);  // render_count == 4 guaranteed by blit_capable.
        ASSERT(start == 0);  // Must be true if count == render_count.
        ASSERT(view_x == 0);  // Must be true if view_w == fb_width.
        ASSERT(view_y == 0);  // Must be true if view_h == fb_height.
        const int has_texture =
            ((primitive->vertex_format & GE_VERTEXFMT_TEXTURE_MASK) != 0);
        const float *vertex0 = primitive->vertices;
        const float *vertex3 = vertex0 + 3*(primitive->vertex_size / 4);
        const float x0f = (vertex0[has_texture ? 2 : 0]
                           * projection_matrix._11 + projection_matrix._41);
        const float y0f = (vertex0[has_texture ? 3 : 1]
                           * projection_matrix._22 + projection_matrix._42);
        const float x1f = (vertex3[has_texture ? 2 : 0]
                           * projection_matrix._11 + projection_matrix._41);
        const float y1f = (vertex3[has_texture ? 3 : 1]
                           * projection_matrix._22 + projection_matrix._42);
        int x0 = iroundf((x0f+1)/2 * fb_width);
        int y0 = iroundf((1-y0f)/2 * fb_height);
        int x1 = iroundf((x1f+1)/2 * fb_width);
        int y1 = iroundf((1-y1f)/2 * fb_height);
        int u0, v0, u1, v1;
        if (has_texture) {
            u0 = iroundf(vertex0[0] * texture->width);
            v0 = iroundf(vertex0[1] * texture->height);
            u1 = iroundf(vertex3[0] * texture->width);
            v1 = iroundf(vertex3[1] * texture->height);
        } else {
            /* Fill in dummy values so the code below works for both
             * textured and non-textured cases. */
            u0 = x0;
            v0 = y0;
            u1 = x1;
            v1 = y1;
        }
        if (u1-u0 == x1-x0 && v1-v0 == y1-y0) {
            if (x1 < x0) {
                int temp;
                temp = x0; x0 = x1; x1 = temp;
                temp = u0; u0 = u1; u1 = temp;
            }
            if (y1 < y0) {
                int temp;
                temp = y0; y0 = y1; y1 = temp;
                temp = v0; v0 = v1; v1 = temp;
            }
            int bound_left, bound_top, bound_w, bound_h;
            if (clip_enabled) {
                bound_left = clip_x;
                bound_top = fb_height - (clip_y + clip_h);
                bound_w = clip_w;
                bound_h = clip_h;
            } else {
                bound_left = 0;
                bound_top = 0;
                bound_w = fb_width;
                bound_h = fb_height;
            }
            if (x0 < bound_left) {
                u0 += bound_left - x0;
                x0 = bound_left;
            }
            if (y0 < bound_top) {
                v0 += bound_top - y0;
                y0 = bound_top;
            }
            x1 = ubound(x1, bound_left+bound_w);
            y1 = ubound(y1, bound_top+bound_h);
            const int width = x1-x0;
            const int height = y1-y0;
            if (width > 0 && height > 0) {
                ge_disable(GE_STATE_LIGHTING);
                ge_set_vertex_pointer(NULL);
                if (primitive->vertex_format == GE_VERTEXFMT_VERTEX_32BITF) {
                    if (texture) {
                        ge_disable(GE_STATE_TEXTURE);
                    }
                    ge_set_vertex_format(GE_VERTEXFMT_TRANSFORM_2D
                                       | GE_VERTEXFMT_COLOR_8888
                                       | GE_VERTEXFMT_VERTEX_16BIT);
                    ge_add_color_xy_vertex(primitive_color, x0, y0);
                    ge_add_color_xy_vertex(primitive_color,
                                           x0+width, y0+height);
                    ge_draw_primitive(GE_PRIMITIVE_SPRITES, 2);
                    if (texture) {
                        ge_enable(GE_STATE_TEXTURE);
                    }
                } else {
                    ge_set_vertex_format(GE_VERTEXFMT_TRANSFORM_2D
                                       | GE_VERTEXFMT_TEXTURE_16BIT
                                       | GE_VERTEXFMT_VERTEX_16BIT);
                    const int Bpp = (texture->format==GE_TEXFMT_T8) ? 1 : 4;
                    const int strip_width = 64/Bpp;
                    int nverts = 0;
                    for (int x = 0; x < width; x += strip_width, nverts += 2) {
                        const int thisw = ubound(width-x, strip_width);
                        ge_add_uv_xy_vertex(u0+x, v0, x0+x, y0);
                        ge_add_uv_xy_vertex(u0+x+thisw, v0+height,
                                            x0+x+thisw, y0+height);
                    }
                    ge_draw_primitive(GE_PRIMITIVE_SPRITES, nverts);
                }
                ge_commit();
            }
            primitive->rendered = 1;
            return;
        }
    }  // if (primitive->blit_capable && etc.)

    if (!primitive->rendered
     && (primitive->vertex_format & GE_VERTEXFMT_TEXTURE_MASK))
    {
        if (texture && texture->height > 512) {
            float *texcoord = (float *)primitive->vertices;
            const int vertex_size = primitive->vertex_size / 4;
            const int num_vertices = primitive->num_vertices;
            for (int i = 0; i < num_vertices; i++, texcoord += vertex_size) {
                const int v_pixel = iroundf(texcoord[1] * texture->height);
                texcoord[1] = v_pixel / 512.0f;
            }
            const int vertices_size =
                primitive->vertex_size * primitive->num_vertices;
            sceKernelDcacheWritebackRange(primitive->vertices, vertices_size);
            if (primitive->orig_vertices) {
                memcpy(primitive->orig_vertices, primitive->vertices,
                       vertices_size);
                sceKernelDcacheWritebackRange(primitive->orig_vertices,
                                              vertices_size);
            }
            primitive->tex_h_adjust = 1;
        }
    }

    void *vertices;
    if (primitive->orig_vertices) {
        if (primitive_color != 0xFFFFFFFF) {
            vertices = primitive->vertices;
            memcpy(vertices, primitive->orig_vertices,
                   primitive->vertex_size * primitive->num_vertices);
        } else {
            vertices = primitive->orig_vertices;
        }
    } else {
        vertices = primitive->vertices;
    }

    uint32_t color;
    if ((primitive->vertex_format & GE_VERTEXFMT_COLOR_MASK)
     && primitive_color != 0xFFFFFFFF) {
        /* Ambient light overrides vertex colors, so we have to modify the
         * vertex colors manually to get both working. */
        color = 0xFFFFFFFF;
        uint32_t *ptr = (uint32_t *)vertices;
        if (primitive->vertex_format & GE_VERTEXFMT_TEXTURE_MASK) {
            ptr += 2;
        }
        const int vertex_size = primitive->vertex_size / 4;
        const int num_vertices = primitive->num_vertices;
        const int R = primitive_color>> 0 & 0xFF;
        const int G = primitive_color>> 8 & 0xFF;
        const int B = primitive_color>>16 & 0xFF;
        const int A = primitive_color>>24 & 0xFF;
        for (int i = 0; i < num_vertices; i++, ptr += vertex_size) {
            const uint32_t vertex_color = *ptr;
            const int r = vertex_color>> 0 & 0xFF;
            const int g = vertex_color>> 8 & 0xFF;
            const int b = vertex_color>>16 & 0xFF;
            const int a = vertex_color>>24 & 0xFF;
            *ptr = ((r*R)/255) <<  0
                 | ((g*G)/255) <<  8
                 | ((b*B)/255) << 16
                 | ((a*A)/255) << 24;
        }
        sceKernelDcacheWritebackRange(
            primitive->vertices,
            primitive->vertex_size * primitive->num_vertices);
    } else {
        color = primitive_color;
    }

    if (color != 0xFFFFFFFF) {
        ge_enable(GE_STATE_LIGHTING);
        ge_set_ambient_light(color);
    } else {
        ge_disable(GE_STATE_LIGHTING);
    }
    ge_set_vertex_format(primitive->vertex_format);
    if (primitive->indices) {
        ge_set_vertex_pointer(vertices);
        ge_set_index_pointer((const uint8_t *)primitive->indices
                             + (start * primitive->index_size));
    } else {
        ge_set_vertex_pointer((const uint8_t *)vertices
                              + (start * primitive->vertex_size));
    }
    if (primitive->is_quads) {
        if (primitive->tex_h_adjust && texture != NULL) {
            const uint16_t * const indices = primitive->indices;
            const float *texcoord = (float *)primitive->vertices;
            const int vertex_size = primitive->vertex_size / 4;
            int current_subtexture = 0;
            for (int i = 0; i+4 <= count; i += 4) {
                int index;
                if (indices) {
                    if (primitive->index_size == 2) {
                        index = indices[start+i];
                    } else {
                        index = ((const uint8_t *)indices)[start+i];
                    }
                } else {
                    index = i;
                }
                const int subtexture =
                    ifloorf(texcoord[index*vertex_size + 1] + 0.5f/512.0f);
                if (subtexture != current_subtexture) {
                    current_subtexture = subtexture;
                    int width = texture->width, height = texture->height;
                    int stride = texture->stride;
                    const int format = texture->format;
                    const int bpp = (format==GE_TEXFMT_T8 ? 8 :
                                     format==GE_TEXFMT_8888 ? 32 : 16);
                    for (unsigned int level = 0; level <= texture->mipmaps;
                         level++)
                    {
                        const int subtex_offset =
                            current_subtexture * (512*stride) * (bpp/8);
                        ge_set_texture_data(
                            level, texture->pixels[level] + subtex_offset,
                            width, height, stride);
                        width  = lbound(width/2, 1);
                        /* height will always be >=2 here (unless the caller
                         * switches to a short texture, which itself will break
                         * things), but we lbound anyway for completeness. */
                        height = lbound(height/2, 1);
                        stride = align_up(stride/2, 128/bpp);
                    }
                    ge_flush_texture_cache();
                    ge_set_texture_offset(texture_offset_u,
                                          texture_offset_v - subtexture);
                }
                ge_draw_primitive(GE_PRIMITIVE_TRIANGLE_STRIP, 4);
            }
            if (current_subtexture != 0) {
                int width = texture->width, height = texture->height;
                int stride = texture->stride;
                const int format = texture->format;
                const int bpp = (format==GE_TEXFMT_T8 ? 8 :
                                 format==GE_TEXFMT_8888 ? 32 : 16);
                for (unsigned int level = 0; level <= texture->mipmaps;
                     level++)
                {
                    ge_set_texture_data(level, texture->pixels[level],
                                        width, height, stride);
                    width  = lbound(width/2, 1);
                    height = lbound(height/2, 1);
                    stride = align_up(stride/2, 128/bpp);
                }
                ge_flush_texture_cache();
                update_state_texture_offset();  // Restore proper values.
            }
        } else {
            for (int i = 0; i+4 <= count; i += 4) {
                ge_draw_primitive(GE_PRIMITIVE_TRIANGLE_STRIP, 4);
            }
        }
    } else {
        ge_draw_primitive(primitive->type, count);
    }

    ge_commit();

    primitive->rendered = 1;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_destroy_primitive(SysPrimitive *primitive)
{
    if (primitive != &immediate_primitive) {
        if (primitive->rendered) {
            /* We can't free immediately (since the GE may not have drawn
             * it yet), so tack it onto the to-free list to be freed at the
             * next sync. */
            primitive->next_to_free = primitive_to_free;
            primitive_to_free = primitive;
        } else {
            /* Primitive has never been rendered, so free immediately. */
            mem_free(primitive);
        }
    }
}

/*************************************************************************/
/****************** Interface: Shader generator control ******************/
/*************************************************************************/

int sys_graphics_set_shader_generator(
    UNUSED void *vertex_source_callback,
    UNUSED void *fragment_source_callback,
    UNUSED void *key_callback,
    UNUSED int hash_table_size, UNUSED int dynamic_resize)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_graphics_add_shader_uniform(UNUSED const char *name)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_graphics_add_shader_attribute(UNUSED const char *name, UNUSED int size)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_int(UNUSED int uniform, UNUSED int value) {}  // NOTREACHED
void sys_graphics_set_shader_uniform_float(UNUSED int uniform, UNUSED float value) {}  // NOTREACHED
void sys_graphics_set_shader_uniform_vec2(UNUSED int uniform, UNUSED const Vector2f *value) {}  // NOTREACHED
void sys_graphics_set_shader_uniform_vec3(UNUSED int uniform, UNUSED const Vector3f *value) {}  // NOTREACHED
void sys_graphics_set_shader_uniform_vec4(UNUSED int uniform, UNUSED const Vector4f *value) {}  // NOTREACHED
void sys_graphics_set_shader_uniform_mat4(UNUSED int uniform, UNUSED const Matrix4f *value) {}  // NOTREACHED

/*************************************************************************/
/****************** Interface: Shader object management ******************/
/*************************************************************************/

int sys_graphics_enable_shader_objects(void)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_graphics_disable_shader_objects(void)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_shader_background_compilation_supported(void)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

void sys_shader_enable_get_binary(UNUSED int enable)
{
    /* Not supported on the PSP. */
}

/*-----------------------------------------------------------------------*/

int sys_shader_max_attributes(void)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_shader_set_attribute(UNUSED int index, UNUSED const char *name) {return 0;}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void sys_shader_bind_standard_attribute(UNUSED ShaderAttribute attribute,
                                        UNUSED int index)
{
    /* Not supported on the PSP. */
}

/*-----------------------------------------------------------------------*/

void sys_shader_clear_attributes(void)
{
    /* Not supported on the PSP. */
}

/*-----------------------------------------------------------------------*/

SysShader *sys_shader_create(UNUSED ShaderType type, UNUSED const void *data,
                             UNUSED int size, UNUSED int is_binary)
{
    return NULL;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

void sys_shader_destroy(UNUSED SysShader *shader) {}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void *sys_shader_get_binary(UNUSED SysShader *shader, UNUSED int *size_ret) {return NULL;}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void *sys_shader_compile(UNUSED ShaderType type, UNUSED const char *source,
                         UNUSED int length, UNUSED int *size_ret)
{
    return NULL;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_shader_get_uniform_id(UNUSED SysShader *shader, UNUSED const char *name) {return 0;}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_int(UNUSED SysShader *shader, UNUSED int uniform, UNUSED int value) {}  // NOTREACHED
void sys_shader_set_uniform_float(UNUSED SysShader *shader, UNUSED int uniform, UNUSED float value) {}  // NOTREACHED
void sys_shader_set_uniform_vec2(UNUSED SysShader *shader, UNUSED int uniform, UNUSED const Vector2f *value) {}  // NOTREACHED
void sys_shader_set_uniform_vec3(UNUSED SysShader *shader, UNUSED int uniform, UNUSED const Vector3f *value) {}  // NOTREACHED
void sys_shader_set_uniform_vec4(UNUSED SysShader *shader, UNUSED int uniform, UNUSED const Vector4f *value) {}  // NOTREACHED
void sys_shader_set_uniform_mat4(UNUSED SysShader *shader, UNUSED int uniform, UNUSED const Matrix4f *value) {}  // NOTREACHED

/*************************************************************************/
/***************** Interface: Shader pipeline management *****************/
/*************************************************************************/

SysShaderPipeline *sys_shader_pipeline_create(UNUSED SysShader *vertex_shader, UNUSED SysShader *fragment_shader) {return NULL;}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void sys_shader_pipeline_destroy(UNUSED SysShaderPipeline *pipeline) {}  // NOTREACHED

/*-----------------------------------------------------------------------*/

void sys_shader_pipeline_apply(UNUSED SysShaderPipeline *pipeline) {}  // NOTREACHED

/*************************************************************************/
/***************** Interface: Other rendering operations *****************/
/*************************************************************************/

void sys_graphics_clear(const Vector4f *color, const float *depth,
                        unsigned int stencil)
{
    if (UNLIKELY(!in_frame)) {
        DLOG("Trying to clear outside a frame, ignoring");
    }

    ge_set_viewport(0, 0, psp_framebuffer_width(), psp_framebuffer_height());
    if (clip_enabled) {
        /* Computed as in update_viewport_and_clip_region(). */
        int clip_left = clip_x;
        int clip_right = clip_x + (clip_w-1);
        int clip_bottom, clip_top;
        if (current_framebuffer == NULL) {
            clip_top = psp_framebuffer_height() - (clip_y + clip_h);
            clip_bottom = clip_top + (clip_h-1);
        } else {
            clip_top = clip_y;
            clip_bottom = clip_y + (clip_h-1);
        }
        ge_set_clip_area(clip_left, clip_top, clip_right, clip_bottom);
    } else {
        ge_unset_clip_area();
    }
    ge_clear(color != NULL,
             depth && depth_buffer_present() && depth_write,
             depth && stencil_enabled,
             (color ? color_to_pixel(color->x,color->y,color->z,0) : 0)
                 | stencil<<24,
             depth ? (uint16_t)ifloorf(*depth * 65535) : 0,
             psp_framebuffer_width(), psp_framebuffer_height());
    update_viewport_and_clip_region();
}

/*-----------------------------------------------------------------------*/

int sys_graphics_read_pixels(int x, int y, int w, int h, int stride,
                             void *buffer)
{
    const int fb_w = psp_framebuffer_width();
    const int fb_h = psp_framebuffer_height();
    int src_stride = psp_framebuffer_stride();
    const int dest_stride = stride;  // Rename to reduce confusion.
    uint32_t *dest = buffer;
    if (x >= fb_w) {
        return 1;
    } else if (x+w > fb_w) {
        w = fb_w - x;
    }
    if (y >= fb_h) {
        return 1;
    } else if (y+h > fb_h) {
        h = fb_h - y;
    }

    if (current_framebuffer) {
        psp_sync_framebuffer(x, y, w, h);
    } else {
        src_stride = -src_stride; // Read bottom-to-top.
        y = fb_h - y - 1;  // Bottom line of the range to copy.
        psp_sync_framebuffer(x, y - (h-1), w, h);
    }

    if (current_framebuffer || display_bpp == 32) {
        const uint32_t *src = psp_fb_pixel_address(x, y);
        for (int line = 0; line < h;
             line++, src += src_stride, dest += dest_stride)
        {
            for (int pixel = 0; pixel < w; pixel++) {
                dest[pixel] = src[pixel] | 0xFF000000;
            }
        }
    } else {  // 16bpp
        /* We don't currently support 16bpp.  If and when we do, this
         * should be updated to perform the proper conversion depending on
         * framebuffer pixel format. */
        return 0;  // NOTREACHED
    }

    return 1;
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void *psp_vram_alloc(uint32_t size, uint32_t align)
{
    if (size == 0) {
        size = 1;
    }
    size = align_up(size, 64);
    if (align < 64) {
        align = 64;
    } else if ((align & (align - 1)) != 0) {
        DLOG("Invalid alignment (not a power of 2): %u", align);
        return NULL;
    }

    for (int i = 0; i < vram_blocks_len; i++) {
        if (vram_blocks[i].free) {
            const uintptr_t mem = (uintptr_t)vram_blocks[i].ptr;
            const uintptr_t mem_end = mem + vram_blocks[i].size;
            const uintptr_t alloc = align_up(mem, align);
            const uintptr_t alloc_end = alloc + size;
            if (alloc_end <= mem_end) {
                const int extra_low = (alloc > mem);
                const int extra_high = (alloc_end < mem_end);
                const int extra_blocks = extra_low + extra_high;
                const int new_blocks_len = vram_blocks_len + extra_blocks;
                if (extra_blocks) {
                    if (new_blocks_len > vram_blocks_size) {
                        VRAMBlock *new_blocks = mem_realloc(
                            vram_blocks,
                            sizeof(*vram_blocks) * (new_blocks_len+1),
                            MEM_ALLOC_TOP);
                        if (!new_blocks) {
                            DLOG("Failed to expand VRAM block array to %d"
                                 " entries", new_blocks_len);
                            return NULL;
                        }
                        vram_blocks = new_blocks;
                        vram_blocks_size = vram_blocks_len+1;
                    }
                    if (extra_low) {
                        vram_blocks[vram_blocks_len].ptr = (void *)mem;
                        vram_blocks[vram_blocks_len].size = alloc - mem;
                        vram_blocks[vram_blocks_len].free = 1;
                        vram_blocks_len++;
                    }
                    if (extra_high) {
                        vram_blocks[vram_blocks_len].ptr = (void *)alloc_end;
                        vram_blocks[vram_blocks_len].size = mem_end - alloc_end;
                        vram_blocks[vram_blocks_len].free = 1;
                        vram_blocks_len++;
                    }
                }
                vram_blocks[i].ptr = (void *)alloc;
                vram_blocks[i].size = size;
                vram_blocks[i].free = 0;
                return (void *)alloc;
            }
        }
    }

    return NULL;
}

/*-----------------------------------------------------------------------*/

void psp_vram_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    for (int i = 0; i < vram_blocks_len; i++) {
        if (vram_blocks[i].ptr == ptr) {
            if (UNLIKELY(vram_blocks[i].free)) {
                DLOG("Attempt to double-free pointer %p", ptr);
                return;
            }

            vram_blocks[i].free = 1;

            for (int j = 0; j < vram_blocks_len; j++) {
                if (vram_blocks[j].free
                 && (uint8_t *)ptr + vram_blocks[i].size == vram_blocks[j].ptr)
                {
                    vram_blocks[i].size += vram_blocks[j].size;
                    if (j+1 < vram_blocks_len) {
                        vram_blocks[j] = vram_blocks[vram_blocks_len-1];
                    }
                    vram_blocks_len--;
                    break;
                }
            }

            for (int j = 0; j < vram_blocks_len; j++) {
                if (vram_blocks[j].free
                 && (uint8_t *)vram_blocks[j].ptr + vram_blocks[j].size == ptr)
                {
                    vram_blocks[j].size += vram_blocks[i].size;
                    if (i+1 < vram_blocks_len) {
                        vram_blocks[i] = vram_blocks[vram_blocks_len-1];
                    }
                    vram_blocks_len--;
                    break;
                }
            }

            return;
        }
    }

    DLOG("Pointer %p not found in VRAM pool", ptr);
}

/*************************************************************************/
/************************* PSP-internal routines *************************/
/*************************************************************************/

int psp_is_ge_busy(void)
{
    return ge_busy;
}

/*-----------------------------------------------------------------------*/

uint32_t *psp_draw_buffer(void)
{
    return work_pixels;
}

/*-----------------------------------------------------------------------*/

uint16_t *psp_depth_buffer(void)
{
    return depth_buffer;
}

/*-----------------------------------------------------------------------*/

int psp_framebuffer_width(void)
{
    if (current_framebuffer) {
        return current_framebuffer->width;
    } else {
        return DISPLAY_WIDTH;
    }
}

/*-----------------------------------------------------------------------*/

int psp_framebuffer_height(void)
{
    if (current_framebuffer) {
        return current_framebuffer->height;
    } else {
        return DISPLAY_HEIGHT;
    }
}

/*-----------------------------------------------------------------------*/

int psp_framebuffer_stride(void)
{
    if (current_framebuffer) {
        return current_framebuffer->stride;
    } else {
        return DISPLAY_STRIDE;
    }
}

/*-----------------------------------------------------------------------*/

void *psp_fb_pixel_address(int x, int y)
{
    if (current_framebuffer) {
        return (uint32_t *)current_framebuffer->pixels
            + y*current_framebuffer->stride + x;
    } else if (display_bpp == 16) {  // Not currently supported.
        return (uint16_t *)work_pixels + y*DISPLAY_STRIDE + x;  // NOTREACHED
    } else {
        return (uint32_t *)work_pixels + y*DISPLAY_STRIDE + x;
    }
}

/*-----------------------------------------------------------------------*/

void psp_use_framebuffer(SysFramebuffer *framebuffer)
{
    if (in_frame) {
        if (framebuffer) {
            ge_set_draw_buffer(framebuffer->pixels, framebuffer->stride);
            if (framebuffer->depth) {
                ge_set_depth_buffer(framebuffer->depth, framebuffer->stride);
            }
        } else {
            ge_set_draw_buffer(NULL, 0);
            ge_set_depth_buffer(NULL, 0);
        }
    }
    current_framebuffer = framebuffer;
    update_state_depth_test();
    update_state_depth_write();
}

/*-----------------------------------------------------------------------*/

void psp_sync_framebuffer(int x, int y, int width, int height)
{
    const int stride = psp_framebuffer_stride();

    /* This looks backwards, but it's safe because ge_sync() doesn't touch
     * the framebuffer, and it lets the cache invalidation and GE
     * processing run in parallel.  (If there are any dirty cache lines,
     * we could still collide here, but that could happen regardless of
     * order; if you write to framebuffer VRAM from the CPU, all bets are
     * off.) */
    sceKernelDcacheWritebackInvalidateRange(
        psp_fb_pixel_address(x,y),
        ((height-1)*stride + width) * (display_bpp/8));
    if (in_frame) {
        ge_sync();
    }
}

/*-----------------------------------------------------------------------*/

SysFramebuffer *psp_current_framebuffer(void)
{
    return current_framebuffer;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int convert_comparison(GraphicsComparisonType type)
{
    switch (type) {
        case GRAPHICS_COMPARISON_TRUE:          return GE_TEST_ALWAYS;
        case GRAPHICS_COMPARISON_FALSE:         return GE_TEST_NEVER;
        case GRAPHICS_COMPARISON_EQUAL:         return GE_TEST_EQUAL;
        case GRAPHICS_COMPARISON_NOT_EQUAL:     return GE_TEST_NOTEQUAL;
        case GRAPHICS_COMPARISON_LESS:          return GE_TEST_LESS;
        case GRAPHICS_COMPARISON_LESS_EQUAL:    return GE_TEST_LEQUAL;
        case GRAPHICS_COMPARISON_GREATER_EQUAL: return GE_TEST_GEQUAL;
        case GRAPHICS_COMPARISON_GREATER:       return GE_TEST_GREATER;
    }
    ASSERT(!"Invalid comparison type", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static int convert_stencil_op(GraphicsStencilOp operation)
{
    switch (operation) {
        case GRAPHICS_STENCIL_KEEP:    return GE_STENCIL_KEEP;
        case GRAPHICS_STENCIL_CLEAR:   return GE_STENCIL_ZERO;
        case GRAPHICS_STENCIL_REPLACE: return GE_STENCIL_REPLACE;
        case GRAPHICS_STENCIL_INCR:    return GE_STENCIL_INCR;
        case GRAPHICS_STENCIL_DECR:    return GE_STENCIL_DECR;
        case GRAPHICS_STENCIL_INVERT:  return GE_STENCIL_INVERT;
    }
    ASSERT(!"Invalid stencil operation", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static inline uint32_t color_to_pixel(float r, float g, float b, float a)
{
    return (int)(r*255)<< 0
         | (int)(g*255)<< 8
         | (int)(b*255)<<16
         | (int)(a*255)<<24;
}

/*-----------------------------------------------------------------------*/

static void update_state_alpha_test(void)
{
    if (alpha_test) {
        ge_enable(GE_STATE_ALPHA_TEST);
    } else {
        ge_disable(GE_STATE_ALPHA_TEST);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_alpha_test_params(void)
{
    ge_set_alpha_test(alpha_test_comparison, alpha_reference);
}

/*-----------------------------------------------------------------------*/

static void update_state_blend(void)
{
    if (blend_enabled) {
        ge_enable(GE_STATE_BLEND);
    } else {
        ge_disable(GE_STATE_BLEND);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_blend_mode(void)
{
    ge_set_blend_mode(blend_op, blend_src, blend_dest,
                      blend_srcval, blend_destval);
}

/*-----------------------------------------------------------------------*/

static void update_state_color_mask(void)
{
    ge_set_color_mask(color_mask);
}

/*-----------------------------------------------------------------------*/

static void update_state_depth_range(void)
{
    ge_set_depth_range(depth_near, depth_far);
}

/*-----------------------------------------------------------------------*/

static void update_state_depth_test(void)
{
    if (depth_buffer_present() && depth_test) {
        ge_enable(GE_STATE_DEPTH_TEST);
    } else {
        ge_disable(GE_STATE_DEPTH_TEST);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_depth_test_comparison(void)
{
    ge_set_depth_test(depth_test_comparison);
}

/*-----------------------------------------------------------------------*/

static void update_state_depth_write(void)
{
    if (depth_buffer_present() && depth_write) {
        ge_enable(GE_STATE_DEPTH_WRITE);
    } else {
        ge_disable(GE_STATE_DEPTH_WRITE);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_face_cull(void)
{
    ge_set_cull_mode(face_cull ? (face_cull_cw ? GE_CULL_CW : GE_CULL_CCW)
                               : GE_CULL_NONE);
}

/*-----------------------------------------------------------------------*/

static void update_state_fog(void)
{
    if (fog) {
        ge_enable(GE_STATE_FOG);
    } else {
        ge_disable(GE_STATE_FOG);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_fog_params(void)
{
    ge_set_fog(fog_start, fog_end, projection_matrix._33 < 0 ? -1 : +1,
               fog_color);
}

/*-----------------------------------------------------------------------*/

static void update_state_stencil(void)
{
    if (stencil_enabled && stencil_test) {
        ge_enable(GE_STATE_STENCIL_TEST);
    } else {
        ge_disable(GE_STATE_STENCIL_TEST);
    }
}

/*-----------------------------------------------------------------------*/

static void update_state_stencil_func(void)
{
    ge_set_stencil_func(stencil_comparison, stencil_reference, stencil_mask);
}

/*-----------------------------------------------------------------------*/

static void update_state_stencil_op(void)
{
    ge_set_stencil_op(stencil_op_sfail, stencil_op_dfail, stencil_op_dpass);
}

/*-----------------------------------------------------------------------*/

static void update_state_texture_offset(void)
{
    ge_set_texture_offset(texture_offset_u, texture_offset_v);
}

/*-----------------------------------------------------------------------*/

static void update_viewport_and_clip_region(void)
{
    int x0 = view_x;
    int x1 = x0 + (view_w-1);
    int y0, y1;
    int clip_left = clip_x;
    int clip_right = clip_left + (clip_w-1);
    int clip_bottom, clip_top;

    if (current_framebuffer == NULL) {
        /* Convert to top-left-origin format. */
        y0 = psp_framebuffer_height() - (view_y + view_h);
        y1 = y0 + (view_h-1);
        clip_top = psp_framebuffer_height() - (clip_y + clip_h);
        clip_bottom = clip_top + (clip_h-1);
        if (in_frame) {
            ge_set_viewport(view_x, y0, view_w, view_h);
        }
    } else {
        /* Leave in bottom-left-origin format, so texturing works properly.
         * (Internally, the framebuffer will be stored upside down.) */
        y0 = view_y;
        y1 = y0 + (view_h-1);
        clip_top = clip_y;
        clip_bottom = clip_top + (clip_h-1);
        if (in_frame) {
            ge_set_viewport(view_x, current_framebuffer->height - y0,
                            view_w, -view_h);
        }
    }

    if (clip_enabled) {
        x0 = max(x0, clip_left);
        y0 = max(y0, clip_top);
        x1 = min(x1, clip_right);
        y1 = min(y1, clip_bottom);
    }
    if (in_frame) {
        ge_set_clip_area(x0, y0, x1, y1);
    }
}

/*-----------------------------------------------------------------------*/

static int depth_buffer_present(void)
{
    return (current_framebuffer ? current_framebuffer->depth : depth_buffer)
           != NULL;
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH
# define FLIP_UNUSED  UNUSED
#else
# define FLIP_UNUSED  /*nothing*/
#endif

static void do_buffer_flip(UNUSED SceSize args, FLIP_UNUSED void *argp)
{
#ifndef SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH
    void *my_work_pixels = *(void **)argp;

    ge_end_frame();
    sceDisplaySetFrameBuf(my_work_pixels, DISPLAY_STRIDE, display_mode,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    ge_busy = 0;
#endif

    sceDisplayWaitVblankStart();
}

#undef FLIP_UNUSED

/*************************************************************************/
/*************************************************************************/
