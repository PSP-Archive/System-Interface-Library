/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/dyngl.c: OpenGL dynamic loading support.
 */

#include "src/base.h"

#define SUPPRESS_GLLOG
#include "src/sysdep/opengl/gl-headers.h"

#include "src/sysdep/opengl/dsa.h"
#include "src/sysdep/opengl/dyngl.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Which extensions are available? */
static uint8_t has_debug_output;
static uint8_t has_dsa;
static uint8_t has_framebuffers;
static uint8_t has_separate_shaders;
static uint8_t has_shader_binaries;
static uint8_t has_texture_storage;
static uint8_t has_vertex_attrib_int;

/*-----------------------------------------------------------------------*/

/* Function pointers for all GL functions. */

#if defined(SIL_PLATFORM_ANDROID) && defined(__NDK_FPABI__)
# define GLAPI_DECL  __NDK_FPABI__ __attribute__((visibility("hidden")))
#elif defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_MACOSX)
# define GLAPI_DECL  __attribute__((visibility("hidden")))
#else
# define GLAPI_DECL  /*nothing*/
#endif

#define DYNGL_FUNC(rettype,name,altname,params,args,required,category) \
    static GLAPI_DECL rettype (GLAPIENTRY *p##name) params;
#include "src/sysdep/opengl/dyngl-funcs.h"
#undef DYNGL_FUNC

/*-----------------------------------------------------------------------*/

/* Saved function pointers for dyngl_{,un}wrap_dsa(). */

static GLAPI_DECL void (GLAPIENTRY *saved_glBindTextureUnit)(
    GLuint unit, GLuint texture);
static GLAPI_DECL GLenum (GLAPIENTRY *saved_glCheckNamedFramebufferStatus)(
     GLuint framebuffer, GLenum target);
static GLAPI_DECL void (GLAPIENTRY *saved_glCompressedTextureSubImage2D)(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void *data);
static GLAPI_DECL void (GLAPIENTRY *saved_glCopyTextureSubImage2D)(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset,
    GLint x, GLint y, GLsizei width, GLsizei height);
static GLAPI_DECL void (GLAPIENTRY *saved_glCreateFramebuffers)(
    GLsizei n, GLuint *ids);
static GLAPI_DECL void (GLAPIENTRY *saved_glCreateProgramPipelines)(
    GLsizei n, GLuint *ids);
static GLAPI_DECL void (GLAPIENTRY *saved_glCreateRenderbuffers)(
    GLsizei n, GLuint *ids);
static GLAPI_DECL void (GLAPIENTRY *saved_glCreateTextures)(
    GLenum target, GLsizei n, GLuint *ids);
static GLAPI_DECL void (GLAPIENTRY *saved_glGenerateTextureMipmap)(
    GLuint texture);
static GLAPI_DECL void (GLAPIENTRY *saved_glGetTextureImage)(
    GLuint texture, GLint level, GLenum format, GLenum type,
    GLsizei bufSize, void *pixels);
static GLAPI_DECL void (GLAPIENTRY *saved_glInvalidateNamedFramebufferData)(
    GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments);
static GLAPI_DECL void (GLAPIENTRY *saved_glNamedFramebufferRenderbuffer)(
    GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer);
static GLAPI_DECL void (GLAPIENTRY *saved_glNamedFramebufferTexture)(
    GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
static GLAPI_DECL void (GLAPIENTRY *saved_glNamedRenderbufferStorage)(
    GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
static GLAPI_DECL void (GLAPIENTRY *saved_glTextureParameteri)(
    GLuint texture, GLenum pname, GLint param);
static GLAPI_DECL void (GLAPIENTRY *saved_glTextureStorage2D)(
    GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
    GLsizei height);
static GLAPI_DECL void (GLAPIENTRY *saved_glTextureSubImage2D)(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void *pixels);

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/* Override for glGetString() set from TEST_dyngl_override_glGetString(). */
static const GLubyte *(*glGetString_override)(
    GLenum, GLAPI_DECL const GLubyte * (GLAPIENTRY *)(GLenum)) = NULL;

/* Original glGetString() function pointer when overridden. */
static GLAPI_DECL const GLubyte * (GLAPIENTRY *original_glGetString)(GLenum);

/* Wrapper for the override function which passes the original function
 * pointer. */
static GLAPI_DECL const GLubyte * GLAPIENTRY wrap_glGetString_override(
    GLenum name) {return (*glGetString_override)(name, original_glGetString);}

#endif

/*************************************************************************/
/************** Interface routine (called from graphics.c) ***************/
/*************************************************************************/

void dyngl_init(void *(*lookup_function)(const char *))
{
    has_debug_output = 1;
    has_dsa = 1;
    has_framebuffers = 1;
    has_separate_shaders = 1;
    has_shader_binaries = 1;
    has_texture_storage = 1;
    has_vertex_attrib_int = 1;

    int major = 0, minor = 0;
    pglGetString = (*lookup_function)("glGetString");
    if (UNLIKELY(!pglGetString)) {
        DLOG("glGetString() not found!");
    } else {
#ifdef SIL_INCLUDE_TESTS
        original_glGetString = pglGetString;
        TEST_dyngl_override_glGetString(glGetString_override);
#endif
        const char *gl_version = (const char *)glGetString(GL_VERSION);
        if (UNLIKELY(!gl_version || !*gl_version)) {
            DLOG("Failed to get GL version!");
        } else {
            gl_version += strcspn(gl_version, "0123456789");
            const char *gl_subversion;
            major = (int)strtoul(gl_version, (char **)&gl_subversion, 10);
            if (UNLIKELY(*gl_subversion != '.')) {
                DLOG("Failed to parse OpenGL version string!");
            } else {
                const char *extra;
                minor = strtoul(gl_subversion+1, (char **)&extra, 10);
#ifdef SIL_PLATFORM_WINDOWS
                /* Hack for broken libGL in VMware guest driver.  Mesa 8.x
                 * never had the ARB_debug_output extension; VMware (or
                 * somebody) added it in a way that lets callers of
                 * glDebugMessageInsert() arbitrarily overwrite memory
                 * (and coincidentally crashes our ARB_debug_output test). */
                extra += strspn(extra, " ");
                if (strncmp(extra, "Mesa 8.", 7) == 0) {
                    DLOG("Suppressing ARB_debug_output for broken Mesa GL"
                         " library");
                    has_debug_output = 0;
                }
#endif
            }
        }
    }

    UNUSED int has_;  // Avoid errors from an empty category on base functions.
#define DYNGL_FUNC(rettype,name,altname,params,args,required,category)  \
    p##name = (*lookup_function)(#name);                                \
    if (strcmp(#altname, #name) != 0 && !p##name) {                     \
        p##name = (*lookup_function)(#altname);                         \
    }                                                                   \
    if (!p##name) {                                                     \
        if (required) {                                                 \
            DLOG("Warning: Failed to get address of required function %s", \
                 #name);                                                \
        } else {                                                        \
            has_##category = 0;                                         \
        }                                                               \
    }
#include "src/sysdep/opengl/dyngl-funcs.h"
#undef DYNGL_FUNC

#ifdef SIL_INCLUDE_TESTS
    /* pglGetString got overwritten in the lookups above. */
    TEST_dyngl_override_glGetString(glGetString_override);
#endif
}

/*-----------------------------------------------------------------------*/

int dyngl_has_debug_output(void)
{
    return has_debug_output;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_dsa(void)
{
    return has_dsa;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_framebuffers(void)
{
    return has_framebuffers;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_separate_shaders(void)
{
    return has_separate_shaders;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_shader_binaries(void)
{
    return has_shader_binaries;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_texture_storage(void)
{
    return has_texture_storage;
}

/*-----------------------------------------------------------------------*/

int dyngl_has_vertex_attrib_int(void)
{
    return has_vertex_attrib_int;
}

/*-----------------------------------------------------------------------*/

void dyngl_wrap_dsa(void)
{
    saved_glBindTextureUnit = pglBindTextureUnit;
    saved_glCheckNamedFramebufferStatus = pglCheckNamedFramebufferStatus;
    saved_glCompressedTextureSubImage2D = pglCompressedTextureSubImage2D;
    saved_glCopyTextureSubImage2D = pglCopyTextureSubImage2D;
    saved_glCreateFramebuffers = pglCreateFramebuffers;
    saved_glCreateProgramPipelines = pglCreateProgramPipelines;
    saved_glCreateRenderbuffers = pglCreateRenderbuffers;
    saved_glCreateTextures = pglCreateTextures;
    saved_glGenerateTextureMipmap = pglGenerateTextureMipmap;
    saved_glGetTextureImage = pglGetTextureImage;
    saved_glInvalidateNamedFramebufferData = pglInvalidateNamedFramebufferData;
    saved_glNamedFramebufferRenderbuffer = pglNamedFramebufferRenderbuffer;
    saved_glNamedFramebufferTexture = pglNamedFramebufferTexture;
    saved_glNamedRenderbufferStorage = pglNamedRenderbufferStorage;
    saved_glTextureParameteri = pglTextureParameteri;
    saved_glTextureStorage2D = pglTextureStorage2D;
    saved_glTextureSubImage2D = pglTextureSubImage2D;

    pglBindTextureUnit = wrap_glBindTextureUnit;
    pglCheckNamedFramebufferStatus = wrap_glCheckNamedFramebufferStatus;
    pglCompressedTextureSubImage2D = wrap_glCompressedTextureSubImage2D;
    pglCopyTextureSubImage2D = wrap_glCopyTextureSubImage2D;
    pglCreateFramebuffers = wrap_glCreateFramebuffers;
    pglCreateProgramPipelines = wrap_glCreateProgramPipelines;
    pglCreateRenderbuffers = wrap_glCreateRenderbuffers;
    pglCreateTextures = wrap_glCreateTextures;
    pglGenerateTextureMipmap = wrap_glGenerateTextureMipmap;
    pglGetTextureImage = wrap_glGetTextureImage;
    pglInvalidateNamedFramebufferData = wrap_glInvalidateNamedFramebufferData;
    pglNamedFramebufferRenderbuffer = wrap_glNamedFramebufferRenderbuffer;
    pglNamedFramebufferTexture = wrap_glNamedFramebufferTexture;
    pglNamedRenderbufferStorage = wrap_glNamedRenderbufferStorage;
    pglTextureParameteri = wrap_glTextureParameteri;
    pglTextureStorage2D = wrap_glTextureStorage2D;
    pglTextureSubImage2D = wrap_glTextureSubImage2D;
}

/*-----------------------------------------------------------------------*/

void dyngl_unwrap_dsa(void)
{
    pglBindTextureUnit = saved_glBindTextureUnit;
    pglCheckNamedFramebufferStatus = saved_glCheckNamedFramebufferStatus;
    pglCompressedTextureSubImage2D = saved_glCompressedTextureSubImage2D;
    pglCopyTextureSubImage2D = saved_glCopyTextureSubImage2D;
    pglCreateFramebuffers = saved_glCreateFramebuffers;
    pglCreateProgramPipelines = saved_glCreateProgramPipelines;
    pglCreateRenderbuffers = saved_glCreateRenderbuffers;
    pglCreateTextures = saved_glCreateTextures;
    pglGenerateTextureMipmap = saved_glGenerateTextureMipmap;
    pglGetTextureImage = saved_glGetTextureImage;
    pglInvalidateNamedFramebufferData = saved_glInvalidateNamedFramebufferData;
    pglNamedFramebufferRenderbuffer = saved_glNamedFramebufferRenderbuffer;
    pglNamedFramebufferTexture = saved_glNamedFramebufferTexture;
    pglNamedRenderbufferStorage = saved_glNamedRenderbufferStorage;
    pglTextureParameteri = saved_glTextureParameteri;
    pglTextureStorage2D = saved_glTextureStorage2D;
    pglTextureSubImage2D = saved_glTextureSubImage2D;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

extern void TEST_dyngl_override_glGetString(
    const GLubyte *(*function)(
        GLenum name, TEST_glGetString_type *original_glGetString))
{
    glGetString_override = function;
    if (function) {
        pglGetString = wrap_glGetString_override;
    } else {
        pglGetString = original_glGetString;
    }
}

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/************************* OpenGL stub functions *************************/
/*************************************************************************/

#define DYNGL_FUNC(rettype,name,altname,params,args,required,category) \
    GLAPI_DECL rettype GLAPIENTRY name params {return (*p##name) args;}
#include "src/sysdep/opengl/dyngl-funcs.h"
#undef DYNGL_FUNC

/*************************************************************************/
/*************************************************************************/
