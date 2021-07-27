/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/list.c: Display list management routines for the
 * GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

/* Base pointer for the current sublist. */
static uint32_t *sublist_base;

/*-----------------------------------------------------------------------*/

void ge_add_command(uint8_t command, uint32_t parameter)
{
    if (UNLIKELY(parameter & 0xFF000000)) {
        DLOG("Command %d: parameter 0x%08X has high bits set!",
             command, parameter);
        parameter &= 0x00FFFFFF;
    }
    if (UNLIKELY(gelist_ptr >= gelist_limit)) {
        DLOG("Command %d parameter 0x%06X: list full!", command, parameter);
        return;
    }

    internal_add_command(command, parameter);
}

/*-----------------------------------------------------------------------*/

void ge_add_commandf(uint8_t command, float parameter)
{
    if (UNLIKELY(gelist_ptr >= gelist_limit)) {
        DLOG("Command %d parameter %f: list full!", command, parameter);
        return;
    }

    internal_add_commandf(command, parameter);
}

/*-----------------------------------------------------------------------*/

int ge_start_sublist(uint32_t *list, int size)
{
    if (!list || size <= 0) {
        DLOG("Invalid parameters: %p %d", list, size);
        return 0;
    }
    if (saved_gelist_ptr) {
        DLOG("Already creating a sublist!");
        return 0;
    }

    saved_gelist_ptr   = gelist_ptr;
    saved_gelist_limit = gelist_limit;

    /* For sublists, since the instructions will not be executed immediately,
     * we use the ordinary cached address for writing to the list and flush
     * the data cache when the list is terminated. */
    sublist_base       = (uint32_t *)list;
    gelist_ptr         = sublist_base;
    gelist_limit       = gelist_ptr + size;

    return 1;
}

/*-----------------------------------------------------------------------*/

void ge_replace_sublist(uint32_t *list, int size)
{
    if (!list || size <= 0) {
        DLOG("Invalid parameters: %p %d", list, size);
        return;
    }
    if (!saved_gelist_ptr) {
        DLOG("Not currently creating a sublist!");
        return;
    }

    const uint32_t offset = gelist_ptr - sublist_base;
    sublist_base = (uint32_t *)list;
    gelist_ptr   = sublist_base + offset;
    gelist_limit = sublist_base + size;
}

/*-----------------------------------------------------------------------*/

uint32_t *ge_finish_sublist(void)
{
    if (!saved_gelist_ptr) {
        return NULL;
    }

    if (gelist_ptr >= gelist_limit) {
        DLOG("Sublist overflow at %p, dropping last insn", gelist_ptr);
        gelist_ptr = gelist_limit-1;
    }
    internal_add_command(GECMD_RETURN, 0);
    sceKernelDcacheWritebackRange(
        sublist_base, (uintptr_t)gelist_ptr - (uintptr_t)sublist_base);
    uint32_t *retval   = gelist_ptr;
    sublist_base       = NULL;
    gelist_ptr         = saved_gelist_ptr;
    gelist_limit       = saved_gelist_limit;
    saved_gelist_ptr   = NULL;
    saved_gelist_limit = NULL;
    return retval;
}

/*-----------------------------------------------------------------------*/

void ge_call_sublist(const uint32_t *list)
{
    CHECK_GELIST(2);
    internal_add_command(GECMD_ADDRESS_BASE, ((uint32_t)list & 0xFF000000)>>8);
    internal_add_command(GECMD_CALL, (uint32_t)list & 0x00FFFFFF);
}

/*-----------------------------------------------------------------------*/

uint32_t ge_sublist_free(void)
{
    if (!saved_gelist_ptr) {
        return 0;
    }

    return gelist_limit - gelist_ptr;
}

/*************************************************************************/
/*************************************************************************/
