/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/dyngl.h: OpenGL dynamic loading support header.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_DYNGL_H
#define SIL_SRC_SYSDEP_OPENGL_DYNGL_H

#include "src/sysdep/opengl/gl-headers.h"

/*************************************************************************/
/*************************************************************************/

/**
 * dyngl_init:  Look up the addresses of all OpenGL symbols.
 *
 * [Parameters]
 *     lookup_function: Function to call to look up an OpenGL function.
 *         Takes the GL function name (e.g., "glGetIntegerv") and returns
 *         the pointer to that function or NULL if the function is not found.
 */
extern void dyngl_init(void *(*lookup_function)(const char *));

/**
 * dyngl_has_*:  Return whether all functions associated with the specified
 * OpenGL feature are available:
 *    - dyngl_has_debug_output()        ==> ARB_debug_output
 *    - dyngl_has_dsa()                 ==> ARB_direct_state_access
 *    - dyngl_has_framebuffers()        ==> EXT_framebuffer_object
 *    - dyngl_has_separate_shaders()    ==> ARB_separate_shader_objects
 *    - dyngl_has_shader_binaries()     ==> ARB_get_program_binary
 *    - dyngl_has_texture_storage()     ==> ARB_texture_storage
 *    - dyngl_has_vertex_attrib_int()   ==> EXT_gpu_shader4
 *
 * [Return value]
 *     True if all associated functions are available, false otherwise.
 */
extern PURE_FUNCTION int dyngl_has_debug_output(void);
extern PURE_FUNCTION int dyngl_has_dsa(void);
extern PURE_FUNCTION int dyngl_has_framebuffers(void);
extern PURE_FUNCTION int dyngl_has_separate_shaders(void);
extern PURE_FUNCTION int dyngl_has_shader_binaries(void);
extern PURE_FUNCTION int dyngl_has_texture_storage(void);
extern PURE_FUNCTION int dyngl_has_vertex_attrib_int(void);

/**
 * dyngl_wrap_dsa:  Replace direct state access function pointers with
 * pointers to the wrappers found in dsa.c.
 */
extern void dyngl_wrap_dsa(void);

/**
 * dyngl_unwrap_dsa:  Restore the original function pointers for the
 * direct state access functions.  Must be paired with dyngl_wrap_dsa().
 */
extern void dyngl_unwrap_dsa(void);

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/* Type of the glGetString() function pointer used below. */
typedef
#if defined(SIL_PLATFORM_ANDROID) && defined(__NDK_FPABI__)
    __NDK_FPABI__
#endif
    const GLubyte * GLAPIENTRY TEST_glGetString_type(GLenum);

/**
 * TEST_dyngl_override_glGetString:  Override the dynamically-loaded
 * glGetString() function with the given function.  Pass NULL to revert to
 * the standard function.
 *
 * [Parameters]
 *     function: Function to call in place of glGetString().  This function
 *         receives a pointer to the original glGetString() function in
 *         addition to the usual "name" parameter.
 */
extern void TEST_dyngl_override_glGetString(
    const GLubyte *(*function)(
        GLenum name, TEST_glGetString_type *original_glGetString));

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_DYNGL_H
