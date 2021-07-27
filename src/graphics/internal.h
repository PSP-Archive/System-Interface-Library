/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/internal.h: Common header for declarations internal to the
 * graphics subsystem.
 */

#ifndef SIL_SRC_GRAPHICS_INTERNAL_H
#define SIL_SRC_GRAPHICS_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/******** framebuffer.c ********/

/**
 * framebuffer_cleanup:  Clean up framebuffer resources.
 */
extern void framebuffer_cleanup(void);


/******** primitive.c ********/

/**
 * primitive_cleanup:  Clean up graphics primitive resources.
 */
extern void primitive_cleanup(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_GRAPHICS_INTERNAL_H
