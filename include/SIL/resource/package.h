/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/resource/package.h: Resource package file handler declarations.
 */

/*
 * This file defines the interface for the package modules used by the
 * resource management functionality to access resources stored inside
 * package files.  See ../resource.h for a detailed description of how
 * resource management functions interact with package modules.
 *
 * Except where otherwise stated in the method documentation, pointer
 * arguments to package methods (including the module instance pointer)
 * are guaranteed by the caller to be non-NULL.
 *
 * Normally, SIL client code should not call package module methods
 * directly; use the core resource functions instead.  If you do need to
 * call these methods, make sure to obey the preconditions listed in this
 * documentation; failure to do so may cause incorrect behavior, including
 * program crashes.
 */

#ifndef SIL_RESOURCE_PACKAGE_H
#define SIL_RESOURCE_PACKAGE_H

EXTERN_C_BEGIN

struct SysFile;

/*************************************************************************/
/*********************** Module instance data type ***********************/
/*************************************************************************/

/* The structure definition is located at the bottom of the file, since it
 * needs the method typedefs. */
typedef struct PackageModuleInfo PackageModuleInfo;

/*************************************************************************/
/********************** Package module method types **********************/
/*************************************************************************/

/**
 * PackageInitFunc:  Initialize a package module instance.  No other module
 * methods are called for a given instance if this method does not succeed.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     True on success, false on error.
 */
typedef int (*PackageInitFunc)(PackageModuleInfo *module);

/**
 * PackageCleanupFunc:  Clean up any resources used by this package module
 * instance.
 *
 * [Parameters]
 *     module: Package module instance.
 */
typedef void (*PackageCleanupFunc)(PackageModuleInfo *module);

/**
 * PackageListStartFunc:  Prepare for returning files via PackageListNextFunc.
 * For a single sequence of:
 *     PackageListStartFunc()
 *     PackageListNextFunc()
 *     ...
 *     PackageListNextFunc() (returning NULL)
 * the module should return each file in the package exactly once; the
 * order of the files is arbitrary (and need not be the same across
 * separate sequences of calls).
 *
 * [Parameters]
 *     module: Package module instance.
 */
typedef void (*PackageListStartFunc)(PackageModuleInfo *module);

/**
 * PackageListNextFunc:  Return the pathname of the next file in the package.
 * See PackageListStartFunc for details.
 *
 * The caller guarantees that the PackageListStartFunc function has been
 * called at least once on this module, and the caller will not call this
 * function again (without an intervening PackageListStartFunc call) after
 * this function returns NULL.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     File pathname, or NULL if no more files remain.
 */
typedef const char *(*PackageListNextFunc)(PackageModuleInfo *module);

/**
 * PackageFileInfoFunc:  Return information about the given file.  The
 * return parameters (*_ret) are only modified on success.
 *
 * [Parameters]
 *     module: Package module instance.
 *     path: Path to look up (without the module's path prefix).
 *     file_ret: Pointer to variable to receive the file handle for reading.
 *     pos_ret: Pointer to variable to receive the file position for reading.
 *     len_ret: Pointer to variable to receive the number of bytes to read.
 *     comp_ret: Pointer to variable to receive true if the file is
 *         compressed, false if not.
 *     size_ret: Pointer to receive the file size, in bytes, after any
 *         compression is undone.
 * [Return value]
 *     True on success, false on error (such as when the file does not exist).
 */
typedef int (*PackageFileInfoFunc)(PackageModuleInfo *module,
                                   const char *path, struct SysFile **file_ret,
                                   int64_t *pos_ret, int *len_ret,
                                   int *comp_ret, int *size_ret);

/**
 * PackageDecompressGetStackSizeFunc:  Return the minimum stack size
 * required for decompression of a data file.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     Minimum stack size for decompression, in bytes.  Must be positive.
 */
typedef int (*PackageDecompressGetStackSizeFunc)(PackageModuleInfo *module);

/**
 * PackageDecompressInitFunc:  Create a state block to use for block-by-block
 * decompression of a data file.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     Decompression state block, or NULL on error.
 */
typedef void *(*PackageDecompressInitFunc)(PackageModuleInfo *module);

/**
 * PackageDecompressFunc:  Decompress compressed data.
 *
 * Note that if state is NULL, partial success is treated as an error, so
 * the return value is never negative.
 *
 * [Parameters]
 *     module: Package module instance.
 *     state: Decompression state block, or NULL to decompress in one shot.
 *     in: Input (compressed) data.
 *     insize: Input data size, in bytes.
 *     out: Output (decompressed) data buffer.
 *     outsize: Output buffer size, in bytes.
 * [Return value]
 *     Positive if the decompression succeeded; zero if the decompression
 *     failed; negative if decompression succeeded but the stream is not
 *     yet complete.
 */
typedef int (*PackageDecompressFunc)(PackageModuleInfo *module, void *state,
                                     const void *in, int insize,
                                     void *out, int outsize);

/**
 * PackageDecompressFinishFunc:  Free a decompression state block allocated
 * with PackageDecompressInitFunc.
 *
 * [Parameters]
 *     module: Package module instance.
 *     state: Decompression state block.
 */
typedef void (*PackageDecompressFinishFunc)(PackageModuleInfo *module,
                                            void *state);

/*************************************************************************/
/***************** Module instance structure definition ******************/
/*************************************************************************/

struct PackageModuleInfo {

    /* Pathname prefix for this module.  The module will be accessed only
     * for resource file pathnames which begin with this string. */
    const char *prefix;

    /* Function pointers for each module method. */
    PackageInitFunc init;
    PackageCleanupFunc cleanup;
    PackageListStartFunc list_files_start;
    PackageListNextFunc list_files_next;
    PackageFileInfoFunc file_info;
    PackageDecompressGetStackSizeFunc decompress_get_stack_size;
    PackageDecompressInitFunc decompress_init;
    PackageDecompressFunc decompress;
    PackageDecompressFinishFunc decompress_finish;

    /* Opaque data pointer for use by the module. */
    void *module_data;

    /* Fields used by the resource management routines; modules must not
     * touch these fields. */
    PackageModuleInfo *next;
    size_t prefixlen;  // strlen(prefix), for convenience.

};

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_RESOURCE_PACKAGE_H
