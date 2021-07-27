/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/graphics.c: Graphics and rendering functionality for
 * OpenGL-based platforms.
 */

/*
 * This file (along with the other sources in this directory) provides
 * the definitions for all sys_framebuffer_*(), sys_graphics_*(),
 * sys_shader_*(), and sys_texture_*() functions except for:
 *
 *    sys_graphics_init()
 *    sys_graphics_cleanup()
 *    sys_graphics_device_width()
 *    sys_graphics_device_height()
 *    sys_graphics_set_display_attr()
 *    sys_graphics_set_display_mode()
 *    sys_graphics_display_is_window()
 *    sys_graphics_set_window_title()
 *    sys_graphics_set_window_icon()
 *    sys_graphics_show_mouse_pointer()
 *    sys_graphics_get_mouse_pointer_state()
 *    sys_graphics_get_frame_period()
 *    sys_graphics_has_focus()
 *    sys_graphics_start_frame()
 *    sys_graphics_finish_frame()
 *    sys_graphics_sync()
 *
 * The sys_* functions and Sys* types defined by the OpenGL module are
 * renamed to opengl_sys_* and OpenGLSys* if SIL_OPENGL_NO_SYS_FUNCS is
 * defined; see the documentation of that setting in include/SIL/base.h
 * and the actual renaming macros in internal.h for details.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dsa.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-table.h"
#include "src/thread.h"
#include "src/time.h"

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
uint32_t TEST_opengl_force_feature_flags = 0;
uint32_t TEST_opengl_force_feature_mask = 0;
uint32_t TEST_opengl_force_format_flags = 0;
uint32_t TEST_opengl_force_format_mask = 0;
uint8_t TEST_opengl_always_wrap_dsa = 0;
#endif

/*************************************************************************/
/************** Exported data (local to the OpenGL library) **************/
/*************************************************************************/

unsigned int opengl_device_generation;
int opengl_window_width, opengl_window_height;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#ifdef DEBUG
/* Should we log OpenGL debug messages? */
static uint8_t enable_debug;
#endif

/* Have we wrapped the direct state access functions with dyngl_wrap_dsa()? */
static uint8_t wrapped_dsa;

/* Major and minor OpenGL and GLSL versions. */
static int major_version, minor_version;
static int glsl_major_version, glsl_minor_version;

/* Feature and supported-format flags for OpenGL extensions. */
static uint32_t features_avail;
static uint32_t formats_supported;

/* ID of the rendering thread (used by opengl_ensure_compile_context() to
 * check whether it's being called on a subthread). */
static int opengl_thread;
/* Callback for creating subthread contexts, or NULL if none. */
int (*shader_compile_context_callback)(void);

/* Data for and array of objects to delete in DELAYED_DELETE mode. */
typedef enum DeleteType DeleteType;
enum DeleteType {
    BUFFER = 1,
    FRAMEBUFFER,
    PROGRAM,
    PROGRAM_PIPELINE,
    RENDERBUFFER,
    SHADER,
    TEXTURE,
    VERTEX_ARRAY,
};
typedef struct DeleteInfo DeleteInfo;
struct DeleteInfo {
    DeleteType type;
    GLuint object;
};
static DeleteInfo *delete_info;        // Dynamically-allocated array.
static int delete_info_size;           // Array size (never shrinks).
static int delete_info_len;            // Number of actual entries.
static uint8_t delete_info_fixed_size; // Is the array buffer size fixed?

#ifdef SIL_OPENGL_LOG_CALLS
/* Buffer for logging OpenGL calls (see gl-headers.h). */
typedef struct CallLogEntry CallLogEntry;
struct CallLogEntry {
    CallLogEntry *next;
    const char *file;
    int line;
    unsigned int time_usec;
    char message[];  // Null-terminated.
};
typedef struct CallLogBuffer CallLogBuffer;
struct CallLogBuffer {
    CallLogBuffer *next;
    /* These are all "intptr_t" to ensure alignment of the pointers in the
     * CallLogEntry structure. */
    intptr_t *data_tail;  // Address of first free byte in data[].
    intptr_t *data_top;   // Pointer to one byte past the last byte in data[].
    intptr_t data[];  // A sequence of CallEntries.
};
static CallLogBuffer *log_head, *log_tail;
#endif

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * parse_gl_version:  Parse the OpenGL version string and set the
 * major_version and minor_version variables appropriately.  Helper for
 * opengl_init().
 *
 * [Return value]
 *     Version string returned by glGetString(GL_VERSION).  Always non-NULL.
 */
static const char *parse_gl_version(void);

/**
 * parse_glsl_version:  Parse the OpenGL Shading Language version string
 * and set the glsl_major_version and glsl_minor_version variables
 * appropriately.  Helper for opengl_init().
 *
 * [Return value]
 *     Version string returned by glGetString(GL_SHADING_LANGUAGE_VERSION).
 *     Always non-NULL.
 */
static const char *parse_glsl_version(void);

/**
 * set_features:  Set feature flags based on the OpenGL version and
 * supported extensions.  Helper for opengl_init().
 *
 * [Parameters]
 *     features: Feature flag set passed to opengl_init().
 */
static void set_features(uint32_t features);

/**
 * delayed_delete_one_resource:  Add the given GL resource to the array of
 * resources to delete at the next opengl_free_dead_resources() call.
 *
 * [Parameters]
 *     type: Resource type (as for DeleteInfo.type).
 *     object: GL object ID.
 */
static void delayed_delete_one_resource(DeleteType type, GLuint object);

/**
 * delete_one_resource:  Delete the given GL resource.
 *
 * [Parameters]
 *     type: Resources type (as for DeleteInfo.type).
 *     object: GL object ID.
 */
static void delete_one_resource(DeleteType type, GLuint object);

#ifdef DEBUG
/**
 * debug_callback:  Callback for receiving OpenGL debug messages.
 */
static void GLAPIENTRY debug_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, const void *userdata);
#endif

/*************************************************************************/
/********* Interface functions common to all OpenGL environments *********/
/*************************************************************************/

const char *sys_(graphics_renderer_info)(void)
{
    static char buf[256];
    strformat(buf, sizeof(buf), "OpenGL version %d.%d (GL_VERSION: %s)",
              major_version, minor_version,
              (const char *)glGetString(GL_VERSION));
    return buf;
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_clear)(const Vector4f *color, const float *depth,
                          unsigned int stencil)
{
#ifdef SIL_PLATFORM_WINDOWS
    /* Hack for broken OpenGL drivers (e.g. VMware) which can execute a
     * clear operation out-of-order with respect to pending draw operations. */
    glFlush();
#endif
    if (color) {
        glClearColor(color->x, color->y, color->z, color->w);
    }
    if (depth) {
#ifdef SIL_OPENGL_ES
        glClearDepthf(*depth);
#else
        glClearDepth(*depth);
#endif
        glClearStencil(stencil);
    }
    glClear((color ? GL_COLOR_BUFFER_BIT : 0)
            | (depth ? GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT : 0));
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_read_pixels)(int x, int y, int w, int h, int stride,
                               void *buffer)
{
    opengl_clear_error();

    /* Protect ourselves from broken GL implementations that choke on
     * out-of-bounds coordinates (for example, some Android implementations
     * completely ignore such calls or throw GL_INVALID_VALUE errors
     * instead of clipping to the framebuffer like they're supposed to). */
    int framebuffer_w, framebuffer_h;
    SysFramebuffer *framebuffer = opengl_current_framebuffer();
    if (framebuffer) {
        framebuffer_w = framebuffer->width;
        framebuffer_h = framebuffer->height;
    } else {
        framebuffer_w = opengl_window_width;
        framebuffer_h = opengl_window_height;
    }
    if (w > framebuffer_w - x) {
        w = framebuffer_w - x;
    }
    if (h > framebuffer_h - y) {
        h = framebuffer_h - y;
    }
    if (w <= 0 || h <= 0) {
        return 1;
    }

    if (stride == w) {
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    } else {
#ifdef SIL_OPENGL_ES
        uint32_t *tempbuf = mem_alloc(w*h*4, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!tempbuf)) {
            DLOG("No memory for temporary buffer (%dx%d)", w, h);
            return 0;
        }
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tempbuf);
        uint32_t *buffer32 = buffer;
        for (int j = 0; j < h; j++) {
            memcpy(buffer32 + j*stride, tempbuf + j*w, 4*w);
        }
        mem_free(tempbuf);
#else
        glPixelStorei(GL_PACK_ROW_LENGTH, stride);
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
#endif
    }
    const int error = glGetError();
    if (error != GL_NO_ERROR) {
        DLOG("Failed to read pixels: 0x%04X", error);
        return 0;
    }

    if (!framebuffer || framebuffer->texture.color_type == TEXCOLOR_RGB) {
        uint32_t *buffer32 = buffer;
        for (int j = 0; j < h; j++, buffer32 += stride) {
            for (int i = 0; i < w; i++) {
#ifdef IS_LITTLE_ENDIAN
                buffer32[i] |= 0xFF000000;
#else
                buffer32[i] |= 0x000000FF;
#endif
            }
        }
    }

    return 1;
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void opengl_lookup_functions(void *(*lookup_function)(const char *))
{
    if (wrapped_dsa) {
        dyngl_unwrap_dsa();
    }
    dyngl_init(lookup_function);
    if (wrapped_dsa) {
        dyngl_wrap_dsa();
    }
}

/*-----------------------------------------------------------------------*/

void opengl_get_version(void)
{
    parse_gl_version();
    parse_glsl_version();
}

/*-----------------------------------------------------------------------*/

void opengl_enable_debug(DEBUG_USED int enable)
{
#ifdef DEBUG
    /* This might be called before opengl_init(), so we need to set up the
     * saved version code ourself in that case (needed both for our own
     * version check and for opengl_has_extension() to call the correct
     * function). */
    int set_version = (major_version == 0);
    if (set_version) {
        parse_gl_version();
    }

    const int has_debug_output =
# ifdef SIL_OPENGL_ES
        opengl_has_extension("GL_KHR_debug");
# else
        opengl_version_is_at_least(4,3)
            || opengl_has_extension("GL_ARB_debug_output");
# endif

    if (!dyngl_has_debug_output() || !has_debug_output) {
        enable_debug = 0;
    } else {
        enable_debug = (enable != 0);

        opengl_clear_error();
        if (enable_debug) {
            glEnable(GL_DEBUG_OUTPUT);
            if (glGetError() == GL_NO_ERROR) {
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                                      0, NULL, GL_TRUE);
                glDebugMessageCallback(debug_callback, 0);
            }
        } else {
            glDisable(GL_DEBUG_OUTPUT);
            if (glGetError() == GL_NO_ERROR) {
                glDebugMessageCallback(NULL, NULL);
            }
        }
    }

    if (set_version) {
        major_version = minor_version = 0;
    }
#endif
}

/*-----------------------------------------------------------------------*/

int opengl_debug_is_enabled(void)
{
#ifdef DEBUG
    return enable_debug;
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int opengl_init(int width, int height, uint32_t features)
{
    opengl_window_width = width;
    opengl_window_height = height;

    DEBUG_USED const char *gl_version = parse_gl_version();
    DEBUG_USED const char *glsl_version = parse_glsl_version();

#ifdef DEBUG
    const char *gl_vendor = (const char *)glGetString(GL_VENDOR);
    ASSERT(gl_vendor, gl_vendor = "");
    const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
    ASSERT(gl_renderer, gl_renderer = "");
    DLOG("OpenGL%s version: %s",
# ifdef SIL_OPENGL_ES
         " ES",
# else
         "",
# endif
         *gl_version ? gl_version : "(unknown)");
    DLOG("GLSL version: %s", *glsl_version ? glsl_version : "(unknown)");
    DLOG("OpenGL vendor: %s", *gl_vendor ? gl_vendor : "(unknown)");
    DLOG("OpenGL renderer: %s", *gl_renderer ? gl_renderer : "(unknown)");
    if (major_version >= 3) {
        GLint num_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        for (GLint i = 0; i < num_extensions; i++) {
            DLOG("OpenGL extension %d: %s", i, glGetStringi(GL_EXTENSIONS, i));
        }
    } else {
        const char *gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
        DLOG("OpenGL extensions: %s",
             gl_extensions && *gl_extensions ? gl_extensions : "(none)");
    }
#endif  // DEBUG

    set_features(features);

#ifdef DEBUG
# define DEFINE_FLAG(flag) {flag, #flag}
    static const struct {int flag; const char *name;} flag_names[] = {
        DEFINE_FLAG(OPENGL_FEATURE_DELAYED_DELETE),
        DEFINE_FLAG(OPENGL_FEATURE_FAST_DYNAMIC_VBO),
        DEFINE_FLAG(OPENGL_FEATURE_FAST_STATIC_VBO),
        DEFINE_FLAG(OPENGL_FEATURE_GENERATEMIPMAP),
        DEFINE_FLAG(OPENGL_FEATURE_MANDATORY_VAO),
        DEFINE_FLAG(OPENGL_FEATURE_NATIVE_QUADS),
        DEFINE_FLAG(OPENGL_FEATURE_USE_STATIC_VAO),
        DEFINE_FLAG(OPENGL_FEATURE_NO_SEPARATE_SHADERS),
        DEFINE_FLAG(OPENGL_FEATURE_BROKEN_COPYTEXIMAGE),
        DEFINE_FLAG(OPENGL_FEATURE_BROKEN_ATTRIB_INT),
        DEFINE_FLAG(OPENGL_FEATURE_DISCARD_FRAMEBUFFER),
        DEFINE_FLAG(OPENGL_FEATURE_FRAMEBUFFERS),
        DEFINE_FLAG(OPENGL_FEATURE_GETTEXIMAGE),
        DEFINE_FLAG(OPENGL_FEATURE_SEPARATE_SHADERS),
        DEFINE_FLAG(OPENGL_FEATURE_SHADER_BINARIES),
        DEFINE_FLAG(OPENGL_FEATURE_TEXTURE_STORAGE),
        DEFINE_FLAG(OPENGL_FEATURE_VERTEX_ATTRIB_INT),
    }, format_names[] = {
        DEFINE_FLAG(OPENGL_FORMAT_BGRA),
        DEFINE_FLAG(OPENGL_FORMAT_BITREV),
        DEFINE_FLAG(OPENGL_FORMAT_INDEX32),
        DEFINE_FLAG(OPENGL_FORMAT_PVRTC),
        DEFINE_FLAG(OPENGL_FORMAT_RG),
        DEFINE_FLAG(OPENGL_FORMAT_S3TC),
    };
# undef DEFINE_FLAG
    DLOG("OpenGL features enabled: 0x%X", features_avail);
    for (int i = 0; i < lenof(flag_names); i++) {
        if (opengl_has_features(flag_names[i].flag)) {
            DLOG("   %s", flag_names[i].name);
        }
    }
    DLOG("OpenGL texture formats supported: 0x%X", formats_supported);
    for (int i = 0; i < lenof(format_names); i++) {
        if (opengl_has_formats(format_names[i].flag)) {
            DLOG("   %s", format_names[i].name);
        }
    }
#endif  // DEBUG

    /* Delay this check until after the log output to help debugging. */
#ifndef SIL_OPENGL_ES
    if (major_version < 2) {
        DLOG("OpenGL version (%d.%d) is too old!  Need at least OpenGL 2.0.",
             major_version, minor_version);
        return 0;
    }
#endif

#ifdef DEBUG
    opengl_enable_debug(enable_debug);
#endif

    wrapped_dsa = 0;
    const int has_dsa =
#ifdef SIL_OPENGL_ES
        0;
#else
        (opengl_version_is_at_least(4,5)
         || opengl_has_extension("GL_ARB_direct_state_access"));
#endif
    if (!(dyngl_has_dsa() && has_dsa)) {
        dyngl_wrap_dsa();
        wrapped_dsa = 1;
    }
#ifdef SIL_INCLUDE_TESTS
    if (!wrapped_dsa && TEST_opengl_always_wrap_dsa) {
        dyngl_wrap_dsa();
        wrapped_dsa = 1;
    }
#endif


    opengl_thread = thread_get_id();
    shader_compile_context_callback = NULL;

    opengl_state_init();
    sys_(framebuffer_bind)(NULL);
    opengl_shader_init();
    sys_(graphics_set_shader_generator)(NULL, NULL, NULL, 0, 0);
    glActiveTexture(GL_TEXTURE0);
    opengl_current_texture_unit = 0;
    sys_(texture_apply)(0, NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

void opengl_cleanup(void)
{
    opengl_primitive_cleanup();
    opengl_clear_generated_shaders();
    shader_table_init(0, 1);  // Ensure the table memory is also freed.
    opengl_free_dead_resources(1);
    opengl_sync();

    if (wrapped_dsa) {
        dyngl_unwrap_dsa();
        wrapped_dsa = 0;
    }

#ifdef DEBUG
    int save_enable_debug = enable_debug;
    opengl_enable_debug(0);
    enable_debug = save_enable_debug;
#endif

    major_version = minor_version = 0;
    glsl_major_version = glsl_minor_version = 0;

#ifdef SIL_OPENGL_LOG_CALLS
    DLOG("Dumping OpenGL call log...");
    for (CallLogBuffer *buffer = log_head, *next; buffer; buffer = next) {
        next = buffer->next;
        for (CallLogEntry *entry = (CallLogEntry *)buffer->data;
             entry->next; entry = entry->next)
        {
            DLOG("[%4d.%06d %s:%d] %s",
                 entry->time_usec / 1000000, entry->time_usec % 1000000,
                 entry->file, entry->line, entry->message);
        }
        mem_free(buffer);
    }
    log_head = log_tail = NULL;
    DLOG("OpenGL call log dump complete.");
#endif

    opengl_device_generation++;
}

/*-----------------------------------------------------------------------*/

int opengl_major_version(void)
{
    return major_version;
}

/*-----------------------------------------------------------------------*/

int opengl_minor_version(void)
{
    return minor_version;
}

/*-----------------------------------------------------------------------*/

int opengl_version_is_at_least(int major, int minor)
{
    return major_version > major
        || (major_version == major && minor_version >= minor);
}

/*-----------------------------------------------------------------------*/

int opengl_sl_major_version(void)
{
    return glsl_major_version;
}

/*-----------------------------------------------------------------------*/

int opengl_sl_minor_version(void)
{
    return glsl_minor_version;
}

/*-----------------------------------------------------------------------*/

int opengl_sl_version_is_at_least(int major, int minor)
{
    return glsl_major_version > major
        || (glsl_major_version == major && glsl_minor_version >= minor);
}

/*-----------------------------------------------------------------------*/

int opengl_has_extension(const char *name)
{
    PRECOND(name != NULL, return 0);

    if (!*name) {
        return 0;
    }
    if (strncmp(name, "GL_", 3) != 0) {
        DLOG("Invalid extension name (does not start with GL_): %s", name);
        return 0;
    }

    if (major_version >= 3) {
        GLint num_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        for (GLint i = 0; i < num_extensions; i++) {
            const char *extension =
                (const char *)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(extension, name) == 0) {
                return 1;
            }
        }
        return 0;
    } else {
        const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
        if (extensions) {
            const int namelen = strlen(name);
            const char *s = extensions;
            while ((s = strstr(s, name)) != NULL) {
                if ((s == extensions || s[-1] == ' ')
                 && (s[namelen] == 0 || s[namelen] == ' ')) {
                    return 1;
                }
                s += namelen;
                s += strcspn(s, " ");
            }
        }
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int opengl_has_features(uint32_t features)
{
    return (features_avail & features) == features;
}

/*-----------------------------------------------------------------------*/

int opengl_has_formats(uint32_t formats)
{
    return (formats_supported & formats) == formats;
}

/*-----------------------------------------------------------------------*/

int opengl_set_delete_buffer_size(int size)
{
    delete_info_fixed_size = 0;
    opengl_free_dead_resources(1);

    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE) && size > 0) {
        delete_info = mem_alloc(size * sizeof(*delete_info), 0, 0);
        if (UNLIKELY(!delete_info)) {
            DLOG("No memory for %d delete entries", size);
            return 0;
        }
        delete_info_size = size;
        delete_info_fixed_size = 1;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void opengl_set_compile_context_callback(int (*function)(void))
{
    shader_compile_context_callback = function;
}

/*-----------------------------------------------------------------------*/

void opengl_set_display_size(int width, int height)
{
    opengl_window_width = width;
    opengl_window_height = height;
}

/*-----------------------------------------------------------------------*/

void opengl_start_frame(void)
{
    opengl_primitive_reset_bindings();
}

/*-----------------------------------------------------------------------*/

void opengl_sync(void)
{
    glFinish();
}

/*-----------------------------------------------------------------------*/

void opengl_free_dead_resources(int also_array)
{
    for (int i = 0; i < delete_info_len; i++) {
        delete_one_resource(delete_info[i].type, delete_info[i].object);
    }
    delete_info_len = 0;
    if (also_array && !delete_info_fixed_size) {
        mem_free(delete_info);
        delete_info = NULL;
        delete_info_size = 0;
    }
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

int opengl_can_ensure_compile_context(void)
{
    return shader_compile_context_callback != NULL;
}

/*-----------------------------------------------------------------------*/

int opengl_ensure_compile_context(void)
{
    if (thread_get_id() == opengl_thread) {
        return 1;
    } else {
        return shader_compile_context_callback
            && (*shader_compile_context_callback)();
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_buffer(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(BUFFER, object);
    } else {
        delete_one_resource(BUFFER, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_framebuffer(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(FRAMEBUFFER, object);
    } else {
        delete_one_resource(FRAMEBUFFER, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_program(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(PROGRAM, object);
    } else {
        delete_one_resource(PROGRAM, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_program_pipeline(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(PROGRAM_PIPELINE, object);
    } else {
        delete_one_resource(PROGRAM_PIPELINE, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_renderbuffer(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(RENDERBUFFER, object);
    } else {
        delete_one_resource(RENDERBUFFER, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_shader(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(SHADER, object);
    } else {
        delete_one_resource(SHADER, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_texture(GLuint object)
{
    if (object == opengl_current_texture_id) {
        opengl_bind_texture(GL_TEXTURE_2D, 0);
    }
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(TEXTURE, object);
    } else {
        delete_one_resource(TEXTURE, object);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_delete_vertex_array(GLuint object)
{
    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        delayed_delete_one_resource(VERTEX_ARRAY, object);
    } else {
        delete_one_resource(VERTEX_ARRAY, object);
    }
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_OPENGL_LOG_CALLS

void _opengl_log(const char *s, int s_size, const char *file, int line)
{
    const int entry_size = (sizeof(CallLogEntry) + s_size
                            + (sizeof(intptr_t)-1)) / sizeof(intptr_t);

    CallLogBuffer *buffer = log_tail;
    if (LIKELY(buffer)) {
        /* >= here because we write a NULL to the the first word after
         * each entry to mark the end of the list, so we need at least
         * one free word beyond the entry we're writing. */
        if (UNLIKELY(buffer->data_tail + entry_size >= buffer->data_top)) {
            buffer = NULL;
        }
    }

    if (UNLIKELY(!buffer)) {
        const int data_size = 104857600 / sizeof(intptr_t);
        buffer =
            mem_alloc(sizeof(*buffer) + data_size * sizeof(intptr_t), 0, 0);
        if (UNLIKELY(!buffer)) {
            static uint8_t warned = 0;
            if (!warned) {
                warned = 1;
                DLOG("Out of memory allocating GL call log buffer");
            }
            return;
        }
        buffer->next = NULL;
        buffer->data_tail = buffer->data;
        buffer->data_top = buffer->data + data_size;
        buffer->data[0] = 0;
        if (log_tail) {
            log_tail->next = buffer;
        } else {
            log_head = buffer;
        }
        log_tail = buffer;
    }

    CallLogEntry *entry = (CallLogEntry *)buffer->data_tail;
    intptr_t *next = (intptr_t *)entry + entry_size;
    buffer->data_tail = next;
    *next = 0;
    entry->next = (CallLogEntry *)next;
    entry->file = file;
    entry->line = line;
    entry->time_usec = (unsigned int)(time_now() * 1000000.0);
    memcpy(entry->message, s, s_size);
}

#endif  // SIL_OPENGL_LOG_CALLS

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static const char *parse_gl_version(void)
{
    const char *gl_version = (const char *)glGetString(GL_VERSION);
    if (UNLIKELY(!gl_version)) {
        DLOG("WARNING: GL did not report a version");
        /* 1.0 will be rejected by opengl_init(), which is probably wise
         * for a GL that's so broken it can't even report its version. */
        gl_version = "1.0";
    }

#ifdef SIL_OPENGL_ES
    if (strncmp(gl_version, "OpenGL ES ", 10) == 0) {
        gl_version += 10;
    } else if (strncmp(gl_version, "OpenGL ES-CM ", 13) == 0
            || strncmp(gl_version, "OpenGL ES-CL ", 13) == 0) {
        gl_version += 13;
    } else {
        DLOG("WARNING: Invalid OpenGL ES version string: [%s]", gl_version);
        gl_version = "2.0";
    }
#endif

    const char *minor_str;
    if ((major_version = strtoul(gl_version, (char **)&minor_str, 10)) < 1
     || *minor_str != '.'
     || (minor_str[1] < '0' || minor_str[1] > '9')) {
        DLOG("WARNING: Invalid OpenGL version number: [%s]", gl_version);
        /* Assume the minimum supported version. */
        major_version = 2;
        minor_version = 0;
    } else {
        minor_version = strtoul(minor_str + 1, NULL, 10);
    }

    return gl_version;
}

/*-----------------------------------------------------------------------*/

static const char *parse_glsl_version(void)
{
    const char *glsl_version =
        (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (UNLIKELY(!glsl_version)) {
        /* Probably GL 1.x, but give it the benefit of the doubt for now. */
        DLOG("WARNING: GL did not report a GLSL version");
        glsl_version = "1.00";
    }

#ifdef SIL_OPENGL_ES
    if (strncmp(glsl_version, "OpenGL ES GLSL ES ", 18) == 0) {
        glsl_version += 18;
    } else if (glsl_version[0] < '0' || glsl_version[0] > '9') {
        DLOG("WARNING: Invalid OpenGL ES shader version string: [%s]",
             glsl_version);
        glsl_version = "1.00";
    }
#endif

    const char *minor_str;
    glsl_major_version = strtoul(glsl_version, (char **)&minor_str, 10);
    if (glsl_major_version < 1
     || *minor_str != '.'
     || (minor_str[1] < '0' || minor_str[1] > '9')) {
        DLOG("WARNING: Invalid GLSL version number: [%s]", glsl_version);
        glsl_major_version = 1;
        glsl_minor_version = 0;
    } else {
        glsl_minor_version = strtoul(minor_str + 1, NULL, 10);
    }

    return glsl_version;
}

/*-----------------------------------------------------------------------*/

static void set_features(uint32_t features)
{
    features_avail = features & ~OPENGL_AUTOCONFIG_FEATURE_MASK;

    const int has_framebuffers =
#ifdef SIL_OPENGL_ES
        1;  // Mandatory in OpenGL ES 2.0+, and we don't support GLES 1.x.
#else
        opengl_version_is_at_least(3,0)
            || opengl_has_extension("GL_EXT_framebuffer_object");
#endif
    if (dyngl_has_framebuffers() && has_framebuffers) {
        features_avail |= OPENGL_FEATURE_FRAMEBUFFERS;
    } else {
        features_avail &= ~OPENGL_FEATURE_GENERATEMIPMAP;
    }

    /* Separate shader objects are broken in various different ways on
     * most current (up to 3.1) implementations of OpenGL ES, so we just
     * suppress them unconditionally. */
#ifndef SIL_OPENGL_ES
    /* Separate shader objects are part of core OpenGL 4.1 or with the
     * ARB_separate_shader_objects extension.  If using the extension, we
     * additionally require at least GLSL 1.50 because we have to declare
     * outputs in a gl_PerVertex block, which isn't supported before that
     * version of GLSL.  (The extension does allow separate shader objects
     * in previous GLSL versions by way of an implied gl_PerVertex block if
     * any built-in variables are redeclared, but we don't worry about that
     * case because (1) separate shaders are only a convenience for us, not
     * a requirement, and (2) it's not worth the extra code to implement
     * given that most systems going forward should support at least GLSL
     * 1.50.) */
    const int has_separate_shaders =
        (opengl_version_is_at_least(4,1)
             || opengl_has_extension("GL_ARB_separate_shader_objects"))
        && opengl_sl_version_is_at_least(1,50);
    if (dyngl_has_separate_shaders() && has_separate_shaders
     && !(features_avail & OPENGL_FEATURE_NO_SEPARATE_SHADERS)) {
        features_avail |= OPENGL_FEATURE_SEPARATE_SHADERS;
    }
#endif

    const int has_shader_binaries =
#ifdef SIL_OPENGL_ES
        opengl_version_is_at_least(3,0)
            || opengl_has_extension("GL_OES_get_program_binary");
#else
        opengl_version_is_at_least(4,1)
            || opengl_has_extension("GL_ARB_get_program_binary");
#endif
    if (dyngl_has_shader_binaries() && has_shader_binaries) {
        GLint num_formats = -1;
        glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_formats);
        if (num_formats > 0) {
            int ok = 1;
#ifdef SIL_PLATFORM_ANDROID
            /* glProgramBinary() fails with GL_INVALID_VALUE on Mali
             * chipsets (e.g. Nexus 10) despite being passed valid data. */
            GLint *formats = mem_alloc(sizeof(*formats) * num_formats, 0,
                                       MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
            if (formats) {
                glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats);
                for (int i = 0; i < num_formats; i++) {
                    if (formats[i] == GL_MALI_PROGRAM_BINARY_ARM) {
                        DLOG("Disabling shader binary support for Android"
                             " Mali bug");
                        ok = 0;
                    }
                }
                mem_free(formats);
            }
#endif
            if (ok) {
                features_avail |= OPENGL_FEATURE_SHADER_BINARIES;
            }
        }
    }

    const int has_texture_storage =
#ifdef SIL_OPENGL_ES
        opengl_version_is_at_least(3,0);
#else
        opengl_version_is_at_least(4,2)
            || opengl_has_extension("GL_ARB_texture_storage");
#endif
    if (dyngl_has_texture_storage() && has_texture_storage) {
        features_avail |= OPENGL_FEATURE_TEXTURE_STORAGE;
    }

    if (!(features & OPENGL_FEATURE_BROKEN_ATTRIB_INT)) {
        const int has_vertex_attrib_int =
            opengl_version_is_at_least(3,0)
                || opengl_has_extension("GL_EXT_gpu_shader4");
        if (dyngl_has_vertex_attrib_int() && has_vertex_attrib_int) {
            features_avail |= OPENGL_FEATURE_VERTEX_ATTRIB_INT;
        }
    }

#ifdef SIL_OPENGL_ES

    if (opengl_version_is_at_least(3,0)) {
        features_avail |= OPENGL_FEATURE_MANDATORY_VAO;
    }

    if (opengl_has_extension("GL_EXT_discard_framebuffer")) {
        features_avail |= OPENGL_FEATURE_DISCARD_FRAMEBUFFER;
    }

    if (opengl_has_extension("GL_EXT_texture_format_BGRA8888")
        /* Apple apparently doesn't believe in following standards... */
     || opengl_has_extension("GL_APPLE_texture_format_BGRA8888")) {
        /* Some GLES implementations also advertise GL_EXT_bgra, but
         * apparently that extension name is only valid for non-ES OpenGL. */
        formats_supported |= OPENGL_FORMAT_BGRA;
    }
#ifdef SIL_PLATFORM_ANDROID
    /* The ARM Mali GLES 3.1 driver used in Android claims support for
     * GL_EXT_texture_format_BGRA8888 but returns GL_INVALID_ENUM when
     * trying to create a texture using the GL_BGRA_EXT format, so
     * suppress the BGRA format flag. */
    if (strcmp((const char *)glGetString(GL_VENDOR), "ARM") == 0
     && strncmp((const char *)glGetString(GL_RENDERER), "Mali", 4) == 0) {
        DLOG("Suppressing BGRA format for Android Mali bug");
        formats_supported &= ~OPENGL_FORMAT_BGRA;
    }
#endif

    if (opengl_has_extension("GL_OES_element_index_uint")) {
        formats_supported |= OPENGL_FORMAT_INDEX32;
    }

    if (opengl_version_is_at_least(3,0)
     || opengl_has_extension("GL_EXT_texture_rg")) {
        formats_supported |= OPENGL_FORMAT_RG;
    }

#else  // !SIL_OPENGL_ES

    features_avail |= OPENGL_FEATURE_GETTEXIMAGE;

    if (opengl_version_is_at_least(4,3)) {
        features_avail |= OPENGL_FEATURE_DISCARD_FRAMEBUFFER;
    }

    formats_supported = OPENGL_FORMAT_BITREV | OPENGL_FORMAT_INDEX32;

    if (opengl_version_is_at_least(3,2)
     || opengl_has_extension("GL_EXT_bgra")) {
        formats_supported |= OPENGL_FORMAT_BGRA;
    }

    if (opengl_version_is_at_least(3,0)
     || opengl_has_extension("GL_ARB_texture_rg")) {
        formats_supported |= OPENGL_FORMAT_RG;
    }

#endif

    if (opengl_has_extension("GL_EXT_texture_compression_s3tc")) {
        formats_supported |= OPENGL_FORMAT_S3TC;
    }
    if (opengl_has_extension("GL_IMG_texture_compression_pvrtc")) {
        formats_supported |= OPENGL_FORMAT_PVRTC;
    }

#ifdef SIL_INCLUDE_TESTS
    ASSERT((TEST_opengl_force_feature_flags
            & ~TEST_opengl_force_feature_mask) == 0);
    ASSERT(((features_avail & TEST_opengl_force_feature_mask)
            ^ TEST_opengl_force_feature_flags)
           == TEST_opengl_force_feature_mask);
    if (TEST_opengl_force_feature_mask) {
        DLOG("Flipping feature flags: 0x%X", TEST_opengl_force_feature_mask);
        features_avail = ((features_avail & ~TEST_opengl_force_feature_mask)
                          | TEST_opengl_force_feature_flags);
    }

    ASSERT((TEST_opengl_force_format_flags
            & ~TEST_opengl_force_format_mask) == 0);
    ASSERT(((formats_supported & TEST_opengl_force_format_mask)
            ^ TEST_opengl_force_format_flags)
           == TEST_opengl_force_format_mask);
    if (TEST_opengl_force_format_mask) {
        DLOG("Flipping format flags: 0x%X", TEST_opengl_force_format_mask);
        formats_supported =
            ((formats_supported & ~TEST_opengl_force_format_mask)
             | TEST_opengl_force_format_flags);
    }
#endif
}

/*-----------------------------------------------------------------------*/

static void delayed_delete_one_resource(DeleteType type, GLuint object)
{
    if (delete_info_len >= delete_info_size) {
        if (delete_info_fixed_size) {
            ASSERT(delete_info_size > 0,
                   delete_one_resource(type, object); return);
            DLOG("Delete array full for %d/%u, flushing objects",
                 type, object);
            opengl_free_dead_resources(0);
        } else {
            int new_size = delete_info_len + OPENGL_DELETE_INFO_EXPAND;
            DeleteInfo *new_array = mem_realloc(
                delete_info, new_size * sizeof(*delete_info), MEM_ALLOC_TOP);
            if (new_array) {
                delete_info = new_array;
                delete_info_size = new_size;
            } else if (delete_info_size > 0) {
                DLOG("Failed to expand array for %d/%u, flushing objects",
                     type, object);
                opengl_free_dead_resources(0);
                ASSERT(delete_info_len < delete_info_size,
                       delete_one_resource(type, object); return);
            } else {
                DLOG("Failed to allocate array for %d/%u, deleting"
                     " immediately", type, object);
                delete_one_resource(type, object);
                return;
            }
        }
    }

    delete_info[delete_info_len].type = type;
    delete_info[delete_info_len].object = object;
    delete_info_len++;
}

/*-----------------------------------------------------------------------*/

static void delete_one_resource(DeleteType type, GLuint object)
{
    switch (type) {
        case BUFFER:           glDeleteBuffers(1, &object);          break;
        case FRAMEBUFFER:      glDeleteFramebuffers(1, &object);     break;
        case PROGRAM:          glDeleteProgram(object);              break;
        case PROGRAM_PIPELINE: glDeleteProgramPipelines(1, &object); break;
        case RENDERBUFFER:     glDeleteRenderbuffers(1, &object);    break;
        case SHADER:           glDeleteShader(object);               break;
        case TEXTURE:          glDeleteTextures(1, &object);         break;
        case VERTEX_ARRAY:     glDeleteVertexArrays(1, &object);     break;
    }
}

/*-----------------------------------------------------------------------*/

#ifdef DEBUG

static void GLAPIENTRY debug_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    UNUSED GLsizei length, const GLchar *message, UNUSED const void *userdata)
{
    static const struct {GLenum value; const char *name;} enum_names[] = {
        {GL_DEBUG_SOURCE_API,             "API"},
        {GL_DEBUG_SOURCE_WINDOW_SYSTEM,   "window-system"},
        {GL_DEBUG_SOURCE_SHADER_COMPILER, "shader-compiler"},
        {GL_DEBUG_SOURCE_THIRD_PARTY,     "third-party"},
        {GL_DEBUG_SOURCE_APPLICATION,     "application"},
        {GL_DEBUG_SOURCE_OTHER,           "other-source"},

        {GL_DEBUG_TYPE_ERROR,               "error"},
        {GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "deprecated-behavior"},
        {GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR,  "undefined-behavior"},
        {GL_DEBUG_TYPE_PORTABILITY,         "portability"},
        {GL_DEBUG_TYPE_PERFORMANCE,         "performance"},
        {GL_DEBUG_TYPE_MARKER,              "marker"},
        {GL_DEBUG_TYPE_PUSH_GROUP,          "push-group"},
        {GL_DEBUG_TYPE_POP_GROUP,           "pop-group"},
        {GL_DEBUG_TYPE_OTHER,               "other-type"},

        {GL_DEBUG_SEVERITY_HIGH,         "high-severity"},
        {GL_DEBUG_SEVERITY_MEDIUM,       "medium-severity"},
        {GL_DEBUG_SEVERITY_LOW,          "low-severity"},
        {GL_DEBUG_SEVERITY_NOTIFICATION, "notification"},
    };

    const char *source_name = "unknown";
    const char *type_name = "unknown";
    const char *severity_name = "unknown";
    for (int i = 0; i < lenof(enum_names); i++) {
        if (enum_names[i].value == source) {
            source_name = enum_names[i].name;
        } else if (enum_names[i].value == type) {
            type_name = enum_names[i].name;
        } else if (enum_names[i].value == severity) {
            severity_name = enum_names[i].name;
        }
    }
    DLOG("GL message: [%s %s %u %s] %s", source_name, type_name, id,
         severity_name, message);
}

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/
