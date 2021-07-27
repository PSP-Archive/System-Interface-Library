/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics.h: Internal header for graphics functionality.
 */

#ifndef SIL_SRC_GRAPHICS_H
#define SIL_SRC_GRAPHICS_H

#include "SIL/graphics.h"  // Include the public header.

/*************************************************************************/
/************************** Internal-use macros **************************/
/*************************************************************************/

/**
 * GRAPHICS_VERTEX_FORMAT_TYPE, GRAPHICS_VERTEX_FORMAT_OFFSET:  Return the
 * type or offset value from a vertex format entry constructed by the
 * GRAPHICS_VERTEX_FORMAT() macro.
 */
#define GRAPHICS_VERTEX_FORMAT_TYPE(format) \
    ((GraphicsVertexFormatType)((uint32_t)(format) >> 16))
#define GRAPHICS_VERTEX_FORMAT_OFFSET(format) \
    ((uint32_t)(format) & 0xFFFF)

/**
 * GRAPHICS_VERTEX_TYPE_IS_USER:  Return whether the given vertex format
 * type is a user attribute for a generated shader.
 */
#define GRAPHICS_VERTEX_TYPE_IS_USER(type) \
    (((type) & 0xF000) == GRAPHICS_VERTEX_USER(0))

/**
 * GRAPHICS_VERTEX_TYPE_IS_ATTRIB:  Return whether the given vertex format
 * type is an attribute for a shader object.
 */
#define GRAPHICS_VERTEX_TYPE_IS_ATTRIB(type)  (((type) & 0xC000) == 0xC000)

/**
 * GRAPHICS_VERTEX_ATTRIB_COUNT, GRAPHICS_VERTEX_ATTRIB_TYPE,
 * GRAPHICS_VERTEX_ATTRIB_INDEX:  Return the data count, data type, or
 * attribute index from a vertex format type.
 */
#define GRAPHICS_VERTEX_ATTRIB_COUNT(type)  (((type)>>12 & 3) + 1)
#define GRAPHICS_VERTEX_ATTRIB_TYPE(type) \
    ((GraphicsVertexDataType)((type)>>8 & 0xF))
#define GRAPHICS_VERTEX_ATTRIB_INDEX(type)  ((type) & 0xFF)

/*************************************************************************/
/************************** Internal interface ***************************/
/*************************************************************************/

/**
 * graphics_init:  Initialize the graphics/rendering subsystem.  This does
 * _not_ prepare the display itself; graphics_set_display_mode() must be
 * called before any rendering operations are performed.
 *
 * It is invalid to call any other graphics/rendering functions except
 * graphics_cleanup() without first successfully calling graphics_init().
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int graphics_init(void);

/**
 * graphics_cleanup:  Shut down the graphics/rendering subsystem, closing
 * the display if it is open.
 */
extern void graphics_cleanup(void);

/*************************************************************************/
/*************************** Test control data ***************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_graphics_sync_count:  Counter for the number of calls to
 * graphics_sync() (including via debug sync).  Reset to zero by
 * graphics_init().
 */
extern int TEST_graphics_sync_count;

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_GRAPHICS_H
