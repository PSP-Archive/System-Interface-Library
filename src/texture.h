/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/texture.h: Internal header for texture manipulation routines.
 */

#ifndef SIL_SRC_TEXTURE_H
#define SIL_SRC_TEXTURE_H

#include "SIL/texture.h"  // Include the public header.

struct SysTexture;

/*************************************************************************/
/*************************************************************************/

/**
 * texture_import:  Assign a texture ID to the given system-level texture
 * (SysTexture object).  After a successful return from this function, the
 * SysTexture object belongs to the high-level texture manager and should
 * not be destroyed or otherwise manipulated by the caller.
 *
 * [Parameters]
 *     systex: System-level texture (SysTexture object) to import.
 *     mem_flags: Memory allocation flags for texture management data.
 * [Return value]
 *     Newly assigned texture ID (nonzero), or zero on error.
 */
extern int texture_import(struct SysTexture *systex, int mem_flags);

/**
 * texture_import_readonly:  Register the given SysTexture object as a
 * read-only texture.  texture_*() functions will reject attempts to
 * operate on such textures.
 *
 * [Parameters]
 *     systex: System-level texture (SysTexture object) to import.
 * [Return value]
 *     Newly assigned texture ID (nonzero), or zero on error.
 */
extern int texture_import_readonly(struct SysTexture *systex);

/**
 * texture_forget_readonly:  Unregister a texture which was imported with
 * texture_import_readonly().  The underlying SysTexture object is not
 * destroyed.
 *
 * This function does nothing if texture_id is zero.
 *
 * [Parameters]
 *     texture_id: ID of texture to forget.
 */
extern void texture_forget_readonly(int texture_id);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEXTURE_H
