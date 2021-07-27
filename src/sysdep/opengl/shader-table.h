/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader-table.h: OpenGL shader hash table management
 * header.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_SHADER_TABLE_H
#define SIL_SRC_SYSDEP_OPENGL_SHADER_TABLE_H

/*************************************************************************/
/*************************************************************************/

/**
 * ShaderInfo:  Data for a single shader.
 */
typedef struct ShaderInfo ShaderInfo;
struct ShaderInfo {
    GLuint program;
    GLint uniforms[UNIFORM__NUM];
    int num_user_uniforms;
    GLint *user_uniforms;
};

/**
 * opengl_shader_table_overflow_count:  Count of shader table overflows
 * caused by selecting an uncached shader into a full table.  Only defined
 * in debug mode.
 */
#ifdef DEBUG
extern int opengl_shader_table_overflow_count;
#endif

/*-----------------------------------------------------------------------*/

/**
 * shader_table_init:  Initialize the internal shader table.  All existing
 * shaders will be destroyed.
 *
 * If dynamic_resize is true, table_size may be zero; in this case,
 * memory for the shader table is allocated on the first lookup call.
 * If dynamic_resize is false, table_size must be greater than zero.
 *
 * [Parameters]
 *     table_size: Initial size of table.
 *     dynamic_resize: True to allow dynamic resizing of the table; false to
 *         flush old entries as new ones are added to a full table.
 * [Return value]
 *     True on success, false on error.
 */
extern int shader_table_init(int table_size, int dynamic_resize);

/**
 * shader_table_lookup:  Return a pointer to the ShaderInfo structure for
 * the given key.  If no shader has yet been created for this key, return
 * a pointer to an entry with ShaderInfo.program == 0 such that, if the
 * .program field is set to a nonzero value, the entry will be found on
 * subsequent lookups for the same key.
 *
 * [Parameters]
 *     key: Key to look up.
 *     invalidate_ret: Pointer to variable to receive true if shader data
 *         was moved around and cached pointers need to be invalidated,
 *         false if cached pointers can be kept.
 * [Return value]
 *     Pointer to key's ShaderInfo entry, or NULL if the shader table is
 *     empty and no memory can be allocated for it.
 */
extern ShaderInfo *shader_table_lookup(uint32_t key, int *invalidate_ret);

/**
 * shader_table_used:  Return the number of shaders stored in the shader
 * table.
 *
 * [Return value]
 *     Number of shaders stored in the shader table.
 */
extern int shader_table_used(void);

/**
 * shader_table_clear:  Clear the shader hash table, freeing associated GL
 * resources.  If dynamic table resizing is enabled, the table buffer is
 * also freed; a new buffer will be allocated on the next lookup call.
 */
extern void shader_table_clear(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_SHADER_TABLE_H
