/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/memory.h: Memory manipulation utility function header.
 */

#ifndef SIL_SRC_UTILITY_MEMORY_H
#define SIL_SRC_UTILITY_MEMORY_H

/*************************************************************************/
/*************************************************************************/

/**
 * mem_transpose32:  Copy a region of memory, considering the data as a
 * matrix of 32-bit values and transposing rows and columns.
 *
 * [Parameters]
 *     dest: Destination memory pointer.
 *     src: Source memory pointer.
 *     src_width: Width of source region (height of destination region).
 *     src_height: Height of source region (width of destination region).
 *     src_stride: Row size (distance between starts of adjacent rows) of
 *         source region; may be negative.
 *     dest_stride: Row size of destination region; may be negative.
 */
extern void mem_transpose32(void *dest, const void *src,
                            unsigned int src_width, unsigned int src_height,
                            int src_stride, int dest_stride);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_MEMORY_H
