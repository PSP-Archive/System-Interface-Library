/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/resource.h: Internal header for resource management.
 */

#ifndef SIL_SRC_RESOURCE_H
#define SIL_SRC_RESOURCE_H

#include "SIL/resource.h"  // Include the public header.

/*************************************************************************/
/********************** Library-internal interface ***********************/
/*************************************************************************/

/**
 * resource_init:  Initialize resource management functionality.  Always
 * succeeds.
 */
extern void resource_init(void);

/**
 * resource_cleanup:  Shut down resource management functionality,
 * unregistering all package module instances.  Any resources still in
 * memory will leak.
 */
extern void resource_cleanup(void);

/**
 * resource_internal_open_file:  Return an open file handle for a resource.
 * If the resource itself is an ordinary file, this is equivalent to
 * sys_file_open().  If the resource is located in a package file, this
 * function returns a new file handle for the package file along with the
 * offset and size of the resource's data; if the resource is compressed,
 * however, this function fails.
 *
 * On success, the returned file handle's synchronous read position points
 * to the beginning of the resource's data.  On failure, the return
 * variables (*offset_ret and *size_ret) are unmodified.
 *
 * [Parameters]
 *     name: Resource name.
 *     offset_ret: Pointer to variable to receive the file offset, in bytes,
 *         of the resource.
 *     size_ret: Pointer to variable to receive the resource size, in bytes.
 * [Return value]
 *     Newly created file handle for the resource, or NULL on error.
 */
extern struct SysFile *resource_internal_open_file(
    const char *name, int64_t *offset_ret, int *size_ret);

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_resource_set_path_prefix:  Override the resource path prefix to the
 * given string.  Passing NULL restores the default behavior of using the
 * path prefix provided by sys_get_resource_path_prefix().
 *
 * The passed-in string pointer must remain valid until this function is
 * called with a different argument.
 *
 * [Parameters]
 *     prefix: Path prefix to use for loading resources from the host
 *         filesystem, or NULL to restore default behavior.
 */
extern void TEST_resource_set_path_prefix(const char *prefix);

/**
 * TEST_resource_block_load:  Enable or disable load blocking.  If enabled,
 * resource_sync() will act as though any pending read operations have not
 * yet completed.  This does not affect resource_wait(), except that if some
 * resources are waiting for asynchronous read request slots to become
 * available, load blocking will prevent resource_wait() from restarting the
 * read operations on such resources as earlier reads complete.
 *
 * [Parameters]
 *     enable: True to enable load blocking, false to disable.
 */
extern void TEST_resource_block_load(int enable);

/**
 * TEST_resource_use_silent_sync:  Enable or disable "silent sync" mode.
 * If enabled, resource_sync() will not call finish_load() on any
 * resources, and will just return whether the actual load operations have
 * completed.  This does not affect resource_wait().
 *
 * [Parameters]
 *     enable: True to enable silent sync mode, false to disable.
 */
extern void TEST_resource_use_silent_sync(int enable);

/**
 * TEST_resource_override_sync_order:  Override the default resource load
 * finalization order.
 *
 * [Parameters]
 *     enable: True to enable the override, false to disable.
 *     reverse: True to sync in reverse order, false to sync in forward
 *         order.  Ignored if enable is false.
 */
extern void TEST_resource_override_sync_order(int enable, int reverse);

/**
 * TEST_resource_set_mark:  Set the current mark value for a resource
 * manager to the given value.  The next call to resource_mark() for this
 * resource manager will return the next mark value after the given one.
 *
 * This function should not be called while any resources are loaded in
 * the resource manager.
 *
 * [Parameters]
 *     resmgr: Resource manager to modify.
 *     mark: Mark value to set.
 */
extern void TEST_resource_set_mark(ResourceManager *resmgr, int mark);

/**
 * TEST_resource_set_link_pointer:  Set the link pointer for a resource
 * so that id1->next == id2, without changing any other resource metadata.
 * This will naturally corrupt the resource manager state; it is intended
 * only for testing fail-soft behavior for resource links.
 *
 * [Parameters]
 *     resmgr: Resource manager.
 *     id1: ID of resource to modify.
 *     id2: ID of resource to set as the link target of id1.
 */
extern void TEST_resource_set_link_pointer(ResourceManager *resmgr,
                                           int id1, int id2);

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_RESOURCE_H
