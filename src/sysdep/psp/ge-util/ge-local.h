/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/ge-local.h: Header declaring internal variables
 * and functions for the GE utility library.
 */

#ifndef SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_LOCAL_H
#define SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_LOCAL_H

#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

/* Include relevant PSP system headers here. */

#include <pspuser.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>

/*************************************************************************/
/****************** Library-internal data declarations *******************/
/*************************************************************************/

/* Pointer to where the next GE instruction should be stored. */
extern uint32_t *gelist_ptr;

/* Top limit of the current list (list + lenof(list)). */
extern uint32_t *gelist_limit;

/* Saved pointer and limit for the main display list while constructing a
 * sublist (both NULL when the main list is active). */
extern uint32_t *saved_gelist_ptr, *saved_gelist_limit;

/* Pointer to next free address and limit for the vertex buffer. */
extern uint32_t *vertlist_ptr;
extern uint32_t *vertlist_limit;

/*-----------------------------------------------------------------------*/

/* Current bits/pixel for the display. */
extern int display_bpp;

/*-----------------------------------------------------------------------*/

/* Convenience macro for merging two 16-bit values into a 32-bit value. */
#define INT16_PAIR(first,second)  (((first) & 0xFFFF) | (second)<<16)

/* Convenience macro to return the raw bits for a floating-point value. */
#define FLOAT(val)  __extension__({                     \
    uint32_t i;                                         \
    __asm__("mfc1 %0, %1" : "=r" (i) : "f" ((val)));    \
    i;                                                  \
})

/*************************************************************************/
/***************** Library-internal macros and routines ******************/
/*************************************************************************/

/**
 * CHECK_GELIST, CHECK_VERTLIST:  Check whether enough space is available
 * in the GE display list or vertex buffer, and return from the current
 * function if not.
 *
 * [Parameters]
 *     required: Space required, in 32-bit words.
 */
#define CHECK_GELIST(required) do {                             \
    if (UNLIKELY(gelist_ptr + (required) > gelist_limit)) {     \
        DLOG("Command list full!");                             \
        return;                                                 \
    }                                                           \
} while (0)

#define CHECK_VERTLIST(required) do {                           \
    if (UNLIKELY(vertlist_ptr + (required) > vertlist_limit)) { \
        DLOG("Vertex list full!");                              \
        return;                                                 \
    }                                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * internal_add_command, internal_add_commandf:  Add an instruction to the
 * GE display list.  These functions do not check for buffer fullness or
 * clear the high 8 bits of the integer parameter.
 *
 * [Parameters]
 *     command: GE opcode (0-255).
 *     parameter: Instruction parameter (24-bit integer or floating-point).
 */
static inline void internal_add_command(GECommand command, uint32_t parameter)
{
    *gelist_ptr++ = (uint32_t)command<<24 | parameter;
}

static inline void internal_add_commandf(GECommand command, float parameter)
{
    uint32_t bits;
    __asm__(
        ".set push; .set noreorder\n"
        "mfc1 %[out], %[in]\n"
        "nop\n"
        "srl %[out], %[out], 8\n"
        ".set pop"
        : [out] "=r" (bits)
        : [in] "f" (parameter));
    *gelist_ptr++ = (uint32_t)command<<24 | bits;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_LOCAL_H
