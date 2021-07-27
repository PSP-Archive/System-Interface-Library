/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/resource.h: Resource management header.
 */

/*
 * This file declares the ResourceManager data type and associated
 * functions, which can be used to manage resources such as images
 * (textures), sounds, or raw data buffers. ResourceManager alleviates the
 * need for calling code to separately manage data pointers for each
 * resource, handles reference counting when a resource is used by multiple
 * callers, and allows related resources to be freed as a group to help
 * avoid memory leaks and fragmentation.
 *
 * ======== Managing resources ========
 *
 * ResourceManager instances can be created one of two ways: by calling
 * resource_create() to dynamically allocate and initialize a new instance,
 * or by defining a static instance with the DEFINE_STATIC_RESOURCEMANAGER()
 * macro.  Once created, a ResourceManager can manage an arbitrary number
 * of resources, limited only by available memory.  However, if the number
 * of resources to be managed is known ahead of time, memory use and
 * fragmentation can be reduced by using a static instance initialized with
 * that number of resource slots.
 *
 * When a ResourceManager instance is no longer needed, it can be destroyed
 * by calling resource_destroy(), which will free all managed resources as
 * well as memory used by the ResourceManager instance itself.  It is also
 * possible to free just the resources and return the ResourceManager to an
 * empty state, by calling resource_free_all().
 *
 * To load a resource from a data file, call the resource_load_*() function
 * appropriate to the resource type, such as resource_load_data() for raw
 * data files or resource_load_texture() for textures.  These functions
 * return a "resource ID", which can subsequently be passed to the
 * appropriate resource_get_*() function to retrieve the resource itself,
 * or to resource_free() to free the resource when it is no longer needed.
 *
 * Resource loading is performed in the background, so even if a call to
 * one of the resource_load_*() functions succeeds, the resource cannot be
 * used immediately.  Before attempting to use a resource, the caller must
 * either check with resource_sync() that the resource has been loaded, or
 * call resource_wait() to explicitly wait for the resource.  Note that
 * these functions do not take resource IDs; instead, they accept a
 * "synchronization mark" value returned by resource_mark().  When
 * resource_sync() returns true for a given mark value, or after calling
 * resource_wait() for a given mark value, all resources loaded via a
 * resource_load_*() function prior to the associated resource_mark() call
 * are guaranteed to have either finished loading or failed to load.
 *
 * In addition to loading resources from files, it is also possible to
 * create new resources which will be managed by the ResourceManager, using
 * the appropriate resource_new_*() or resource_strdup() functions.  As
 * with resource_load_*(), these functions return a resource ID which can
 * be passed to resource_get_*() to retrieve the actual resource data;
 * unlike loaded resources, newly-created resources are immediately
 * available when created and need not be waited for.
 *
 * There are also shortcut functions named resource_get_new_*() which
 * return the resource data directly; for example, resource_get_new_data()
 * is similar to mem_alloc(), except that the returned data block is
 * managed.  Since these functions do not return the resource ID, it is
 * not possible to free these resources individually; they can only be
 * freed as a group, by calling resource_free_all().  These functions are
 * mainly of use in self-contained environments where a ResourceManager is
 * tightly coupled with the data it manages.
 *
 * Finally, if you already have a resource (such as a texture returned by
 * another function) and want to manage it using a ResourceManager, the
 * resource_take_*() functions will accept the resource and manage it like
 * any other managed resource.  Note that attempting to register a resource
 * in this manner when the resource is already managed by a different
 * ResourceManager will appear to succeed, but is likely to break in
 * spectacular fashion when one ResourceManager frees the resource and the
 * other tries to access it.  Use resource_link() instead for such cases
 * (described below).
 *
 * Sometimes it may be necessary to use the same resource in separate parts
 * of the program for different periods of time.  To avoid requiring
 * explicit cooperation between resource users, the resource_link()
 * function can be used to create a new resource ID, possibly managed by a
 * different ResourceManager instance, which references the same resource.
 * Once such a link has been created, both resource IDs are essentially
 * equivalent; using resource_get_*() on either ID will return the same
 * resource, and modifications made to the resource using one ID will be
 * visible using the other ID.  However, the resource itself will not be
 * deallocated until both resource IDs are freed.  (This behavior is
 * analagous to the behavior of hard links on a Unix filesystem.)  Note
 * that the use of links can lead to memory fragmentation if a link to a
 * resource is still live when related resources are freed.
 *
 * To alleviate both the risk of memory fragmentation described above and
 * unnecessary complexity in managing linked resources (such as with local
 * caches of resource IDs), ResourceManager also provides the
 * resource_link_weak() function to create a "weak link" to a resource.
 * As long as at least one "strong link" -- the original resource ID for
 * the resource, or a link created with resource_link() -- exists, weak
 * links behave exactly like strong links.  However, as soon as the last
 * strong link is removed, the resource itself is freed, and any remaining
 * weak links become "stale links" which no longer reference any resource;
 * calling resource_get_*() on such a resource will return an error, and
 * the only valid operation on the stale link is to free it with
 * resource_free() (or resource_free_all()).  The staleness of a weak link
 * can be checked with the resource_is_stale() function.
 *
 * ======== Using package files ========
 *
 * In addition to loading resource files directly from the host filesystem,
 * ResourceManager also has the capability to load from "package files"
 * containing embedded resource data.  Such package files are implemented
 * through "package modules" registered with the ResourceManager core code.
 * A package module includes methods for initialization and cleanup,
 * retrieving information about an individual file stored in the package,
 * and decompressing compressed data loaded from the package; see
 * resource/package.h for the full specification.
 *
 * To make use of a package file, register a package module instance using
 * resource_register_package().  Any subsequent load operations will first
 * check whether the requested file exists in the package, and will only
 * attempt to load from the host filesystem if the file is not found in the
 * package.  Package module instances can be unregistered using
 * resource_unregister_package(), which restores the original behavior for
 * load operations.  In both cases, resources already loaded are not
 * affected, though unregistering a package file while resources are being
 * loaded from it may cause the program to crash.
 *
 * Multiple package modules can be registered at once.  If two or more
 * packages expose a file with the same name, the one in the most recently
 * registered package will take precedence.  This can be useful when
 * registering a "patch" package which overlays an existing package file:
 * load operations will first search the patch package, and if the file is
 * not found there, the operation will fall back to the original package.
 *
 * To avoid name conflicts when they are not desired, each package module
 * instance can define a prefix at which its filenames will be rooted.
 * For example, if a package uses the prefix "package:", then the resource
 * name "package:file.dat" will look for the file named "file.dat" in that
 * package.
 *
 * If a resource in the package file is compressed, compression will be
 * done synchronously by default, at the time the load is detected to be
 * complete (which is checked in resource_sync() and resource_wait()).
 * However, it is also possible to enable background decompression, by
 * calling resource_set_background_decompression().  When enabled, each
 * compressed resource will be loaded by a background thread which reads
 * data one block at a time, decompressing that block and reading the next
 * one until the entire file has been loaded.  Background compression can
 * be disabled again by calling resource_set_background_decompression()
 * with all parameters set to zero.
 *
 * In addition to potentially saving time through parallel processing,
 * background decompression also avoids an instantaneous memory spike at
 * synchronous decompression time when both the entire compressed stream
 * and the uncompressed data are resident at once.  On the flip side,
 * since reads are performed a block at a time, I/O overhead may lead to
 * decreased read performance.  Because of this, the parameters for
 * background decompression should be chosen carefully, and may need to be
 * dynamically adjusted for best performance based on the actual set of
 * resources being loaded.
 *
 * SIL includes one predefined package module in resource/package-pkg.c.
 * This module handles package files of a custom format, which uses a
 * simple hashed index for quick access to resource data and also allows
 * individual data files to be compressed using the "deflate" (gzip)
 * scheme.  The tools/build-pkg.c program can be used to create such
 * package files.  At runtime, use pkg_create_instance() and
 * pkg_destroy_instance() (declared in resource/package-pkg.h) to create
 * or destroy instances of the package module.
 *
 * ======== Other functionality ========
 *
 * For convenience, SIL also provides functions to check for the existence
 * of a file (resource_exists()) or list all files under a given directory
 * (resource_list_files_*()).  The latter is a set of three functions
 * intended to be used in an open-read-close loop, for example:
 *     ResourceFileListHandle *dir = resource_list_files_start("...", 0);
 *     const char *name;
 *     while ((name = resource_list_files_next(dir)) != NULL) {...}
 *     resource_list_files_end(dir);
 * All four of these functions support package files as well as direct
 * access to the host filesystem, just like the resource loading functions.
 *
 * ======== Resource pathname resolution ========
 *
 * Functions which access data files, including both resource loading
 * functions and direct-access functions such as resource_exists(), use
 * the following algorithm for resolving pathnames:
 *
 * 1) If the resource name passed to the function begins with a prefix
 *    associated with a package module, that package is used to access the
 *    data.  If the name could match two or more package prefixes, the
 *    most recently registered package is used.
 *
 * 2) Otherwise, if the resource name begins with "host:", the "host:" is
 *    stripped and the remaining part of the name is used as the pathname.
 *    If the resulting pathname is relative, it will be relative to the
 *    host environment's current working directory.  This implies that
 *    resources cannot be given a name starting with "host:", and that
 *    this functionality will be unavailable if a package is registered
 *    with a prefix that is an initial substring of "host:" (including the
 *    empty string).
 *
 * 3) Otherwise, if the resource name begins with "/", the name is used
 *    unchanged as the pathname.  (This logic is platform-independent; for
 *    example, a path like "c:/windows" is _not_ recognized under this
 *    rule even on Windows systems, and an explicit "host:" prefix is
 *    required to access that path on the host filesystem.)  As with the
 *    "host:" prefix, a package whose prefix is the empty string or "/"
 *    will mask this functionality.
 *
 * 4) Otherwise, the pathname is constructed by appending the resource name
 *    to the system-dependent resource path prefix:
 *       - On Linux and Windows, this is the directory containing the
 *         executable program.  On Linux, if a symbolic link was used to
 *         invoke the program, the link is dereferenced and the directory
 *         of the target (non-symlink) file is used.
 *       - On other platforms, this is the appropriate directory in the
 *         application package.
 *    On Linux, Mac OS X, and Windows, if the preprocessor symbol
 *    SIL_DATA_PATH_ENV_VAR is defined when building and the environment
 *    variable named by that symbol is defined at runtime, the resource
 *    path prefix is overridden with the directory specified by that
 *    environment variable.
 *
 * Note that when generating a host filesystem path in step 4, SIL has an
 * internal limit of 4095 bytes on the final UTF-8 pathname length.  This
 * is not expected to cause any problems on real-world systems as long as
 * resource names are kept reasonably short (under 1024 bytes).
 */

#ifndef SIL_RESOURCE_H
#define SIL_RESOURCE_H

EXTERN_C_BEGIN
#ifdef __cplusplus
# define private private_  // Avoid errors when included from C++ source.
#endif

/* External structure declarations. */
struct PackageModuleInfo;
struct Sound;
struct SysFile;

/*************************************************************************/
/****************** Data types and related declarations ******************/
/*************************************************************************/

/* Data type for ResourceManager instances.  The majority of the structure
 * definition is private; two fields are exposed here for the benefit of
 * the DEFINE_STATIC_RESOURCEMANAGER macro, and the remainder is
 * encapsulated within the ResourceManagerPrivate structure. */

typedef struct ResourceManager ResourceManager;
typedef struct ResourceManagerPrivate ResourceManagerPrivate;
struct ResourceManager {
    /* Pointer to and size (in bytes and in resource records) of the static
     * buffer reserved for this instance.  These are only declared here to
     * allow setting with DEFINE_STATIC_RESOURCEMANAGER, and they must not
     * be set or modified externally. */
    void * const static_buffer;
    const int static_size;
    const int static_count;
#ifdef DEBUG
    /* File and line at which this resource manager was defined. */
    const char * const static_file;
    const int static_line;
#endif
    /* Internal data. */
    ResourceManagerPrivate *private;
};

/**
 * DEFINE_STATIC_RESOURCEMANAGER:  Define a ResourceManager instance as a
 * static object, and reserve a buffer for internal data large enough to
 * hold the given number of resources.  A ResourceManager defined in this
 * way can be used immediately without any initialization.  If the number
 * of resources to be managed exceeds the number passed to this macro, a
 * dynamically-allocated internal buffer will still be allocated as usual.
 *
 * Use this macro as follows:
 *     DEFINE_STATIC_RESOURCEMANAGER(resmgr, num);
 * where "resmgr" is the identifier to use for the object, and "num" is the
 * number of resources which the static internal buffer should be able to
 * hold.  For example:
 *     DEFINE_STATIC_RESOURCEMANAGER(my_resmgr, 5);
 *     static int data_resource;
 *     void my_init(void) {
 *         data_resource = resource_load_data(my_resmgr, ...);
 *     }
 */
/* Note that we don't use designated initializers here because C++ still(!)
 * doesn't support them, and the macro needs to work from C++ code. */
#define DEFINE_STATIC_RESOURCEMANAGER(resmgr,num)                       \
static void *resmgr##_static_buffer[                                    \
    _SIL_RESOURCE_SIZE1 + _SIL_RESOURCE_SIZE2*(num)];                   \
static ResourceManager resmgr##_static_instance =                       \
     {resmgr##_static_buffer, sizeof(resmgr##_static_buffer), (num),    \
      _SIL_RESOURCE_DEBUG_FILE_LINE_ NULL};                             \
static ResourceManager * const resmgr = &resmgr##_static_instance

/* Size constants used by the DEFINE_STATIC_RESOURCEMANAGER() macro. */
#ifdef DEBUG
# define _SIL_RESOURCE_SIZE1 (1 + ((140 + sizeof(void *)-1) / sizeof(void *)))
#else
# define _SIL_RESOURCE_SIZE1 (1 + (( 12 + sizeof(void *)-1) / sizeof(void *)))
#endif
#define _SIL_RESOURCE_SIZE2 (4 + (( 24 + sizeof(void *)-1) / sizeof(void *))  \
                           + 1 + /* align up to 64 bits because of int64_t */ \
                                 ((8 - sizeof(void *)) / sizeof(void *)))

/* Macro to include the source file and line in a static ResourceManager
 * definition, but only when debugging. */
#ifdef DEBUG
# define _SIL_RESOURCE_DEBUG_FILE_LINE_  __FILE__, __LINE__,
#else
# define _SIL_RESOURCE_DEBUG_FILE_LINE_  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/* Handle type used with the resource_list_files_*() functions.  The
 * structure definition is private. */

typedef struct ResourceFileListHandle ResourceFileListHandle;

/*-----------------------------------------------------------------------*/

/* Allocation flags for use with resource loading/creation functions.
 * These follow the same pattern and have the same meanings as the
 * corresponding MEM_ALLOC_* flags from memory.h, but callers must use
 * these flags (not MEM_ALLOC_*) when calling ResourceManager functions. */

/* Deliberately skip the MEM_ALLOC_* values (1<<0, 1<<1, 1<<2). */
#define RES_ALLOC_TOP    (1<<3)  // Allocate from the top of the memory pool.
#define RES_ALLOC_TEMP   (1<<4)  // Allocate from the temporary pool.
#define RES_ALLOC_CLEAR  (1<<5)  // Zero allocated memory (new_data() only).

/*************************************************************************/
/******************************* Interface *******************************/
/*************************************************************************/

/*----------------------- Initialization/cleanup ------------------------*/

/**
 * resource_register_package:  Register a package file from which resources
 * can be loaded.  The registration is valid until the package is
 * unregistered with resource_unregister_package() or resource_cleanup().
 *
 * Depending on the particular module implementing access to the package
 * file, this function may block.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     True on success, false on error.
 */
extern int resource_register_package(struct PackageModuleInfo *module);

/**
 * resource_unregister_package:  Unregister a previously registered package
 * file.  This function does nothing if module is NULL or if the given
 * module instance was not registered.
 *
 * [Parameters]
 *     module: Package module instance.
 * [Return value]
 *     True on success, false on error.
 */
extern void resource_unregister_package(struct PackageModuleInfo *module);

/**
 * resource_set_background_decompression:  Enable or disable background
 * decompression of compressed resources loaded from a package file, and
 * configure associated parameters.  The new settings are applied from
 * the next load operation.
 *
 * When "on" is true, each time a compressed resource whose compressed data
 * size is at least "threshold" bytes is loaded, a background thread will
 * be started to read and decompress the data "buffer_size" bytes at a
 * time.  It naturally makes no sense to specify a threshold value less
 * than or equal to buffer_size; in that case, the threshold will be set to
 * buffer_size + 1.  (Thus, threshold == 0 is equivalent to saying "all
 * compressed files larger than buffer_size".)
 *
 * The number-of-threads setting only takes effect the first time background
 * decompression is enabled; attempts to change the number of threads are
 * ignored.
 *
 * By default, background decompression is disabled.
 *
 * IMPORTANT: The use of background decompression can cause resource_wait()
 * to block indefinitely under certain conditions.  See the resource_wait()
 * function documentation for details.
 *
 * [Parameters]
 *     on: True to enable background decompression, false to disable.
 *     threshold: Minimum compressed data size for background decompression,
 *         in bytes.
 *     buffer_size: Read buffer size, in bytes.
 *     num_threads: Number of decompression threads to use (must be >= 1).
 */
extern void resource_set_background_decompression(
    int on, int threshold, int buffer_size, int num_threads);

/*------------ ResourceManager instance creation/destruction ------------*/

/**
 * resource_create:  Create a new, empty ResourceManager instance.
 *
 * num_resources is used to set the initial size for internal data
 * structures used to record information about managed resources.  The data
 * structures will be expanded as necessary, so the value chosen is not
 * critical; however, if the number of resources to be managed is known in
 * advance, using an accurate value can help reduce memory waste.
 *
 * [Parameters]
 *     num_resources: Initial number of resources to reserve space for.
 *         If zero, a reasonable default value will be used.
 * [Return value]
 *     Newly created ResourceManager instance, or NULL on error.
 */
extern ResourceManager *resource_create(int num_resources
#ifdef DEBUG
                                        , const char *file, int line
#endif
);

/**
 * resource_destroy:  Free all resources and internal data associated with
 * the given ResourceManager instance.  If the instance was created with
 * resource_create(), the instance itself is freed, and the instance
 * pointer is no longer valid after this function returns.  If the instance
 * is a static instance defined with DEFINE_STATIC_RESOURCEMANAGER(), any
 * dynamically allocated internal buffers are freed, and the instance is
 * reset to its initial (empty) state.  If resmgr == NULL, the function
 * does nothing.
 *
 * This function may block if any resources are being loaded when the
 * function is called.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance to destroy (may be NULL).
 */
extern void resource_destroy(ResourceManager *resmgr
#ifdef DEBUG
                            , const char *file, int line
#endif
);

/*--------------------- File/directory information ----------------------*/

/**
 * resource_exists:  Return whether a resource with the given name exists.
 * This is generally faster than attempting to load the resource with
 * resource_load_*() and checking the result.
 *
 * [Parameters]
 *     name: Resource name.
 * [Return value]
 *     True if the resource exists, false if not.
 */
extern int resource_exists(const char *name);

/**
 * resource_list_files_start():  Start a directory-list operation on the
 * given directory path.  If the directory belongs to a package file, the
 * list will include all files within that package file under the given
 * directory; otherwise, it will include all such files on the host
 * filesystem.  (The list will never include files from more than one
 * package file or from both a package file and the host filesystem.)
 *
 * If "recursive" is true, the directory list will include all files in
 * subdirectories as well as in the given directory; otherwise, it will
 * only include files in the given directory, omitting subdirectories.
 * Note that when reading directly from the host filesystem, this function
 * imposes a nesting limit of 15 subdirectories in order to avoid the risk
 * of infinite recursion (such as can result from symbolic links).
 *
 * [Parameters]
 *     dir: Directory path for which to obtain a file list.
 *     recursive: True to recursively list files in subdirectories; false
 *         to only list files in the given directory.
 * [Return value]
 *     File list handle, or NULL on error.
 */
extern ResourceFileListHandle *resource_list_files_start(
    const char *dir, int recursive);

/**
 * resource_list_files_next():  Return the next filename for the given
 * directory list operation.  The order of returned files is unspecified.
 *
 * The returned pathname is relative to the directory path passed to
 * resource_list_files_start().
 *
 * The returned string is only valid until the next call to
 * resource_list_files_next() or resource_list_files_end() on the same
 * file list handle.
 *
 * [Parameters]
 *     handle: File list handle.
 * [Return value]
 *     Pathname of the next file in the list, or NULL if all files have
 *     been returned.
 */
extern const char *resource_list_files_next(ResourceFileListHandle *handle);

/**
 * resource_list_files_end():  Close the given file list handle.  This
 * function does nothing if handle == NULL.
 *
 * [Parameters]
 *     handle: File list handle.
 */
extern void resource_list_files_end(ResourceFileListHandle *handle);

/*-------------------------- Resource loading ---------------------------*/

/**
 * resource_load_data:  Reserve memory for loading a data resource, and
 * start the load operation.  The resource cannot be used until it has been
 * loaded and synced with resource_sync() or resource_wait().
 *
 * This function fails if the given resource does not exist.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 *     align: Memory alignment, in bytes.  If zero, the memory is aligned
 *         suitably for any basic data type.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_load_data(ResourceManager *resmgr, const char *name,
                              int align, int flags
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_load_texture:  Reserve memory for loading a texture resource,
 * and start the load operation.  The resource cannot be used until it has
 * been loaded and synced with resource_sync() or resource_wait().
 *
 * This function fails if the given resource does not exist.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 *     mipmaps: True to autogenerate mipmaps.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_load_texture(ResourceManager *resmgr, const char *name,
                                 int flags, int mipmaps
#ifdef DEBUG
                                 , const char *file, int line
#endif
);

/**
 * resource_load_bitmap_font:  Reserve memory for loading a bitmap font
 * resource, and start the load operation.  The resource cannot be used
 * until it has been loaded and synced with resource_sync() or
 * resource_wait().
 *
 * This function fails if the given resource does not exist.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_load_bitmap_font(ResourceManager *resmgr, const char *name,
                                     int flags
#ifdef DEBUG
                                     , const char *file, int line
#endif
);

/**
 * resource_load_freetype_font:  Reserve memory for loading a
 * FreeType-rendered font resource, and start the load operation.  The
 * resource cannot be used until it has been loaded and synced with
 * resource_sync() or resource_wait().
 *
 * This function fails if the given resource does not exist.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_load_freetype_font(ResourceManager *resmgr, const char *name,
                                       int flags
#ifdef DEBUG
                                       , const char *file, int line
#endif
);

/**
 * resource_load_sound:  Reserve memory for loading a sound resource, and
 * start the load operation.  The resource cannot be used until it has been
 * loaded and synced with resource_sync() or resource_wait().
 *
 * This function fails if the given resource does not exist.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource file name.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_load_sound(ResourceManager *resmgr, const char *name,
                               int flags
#ifdef DEBUG
                               , const char *file, int line
#endif
);

/**
 * resource_mark:  Register a synchronization mark for use with
 * resource_sync() or resource_wait().  This function never fails, but its
 * behavior is undefined if called more than 10,000 times without a
 * successful call to resource_sync() or resource_wait().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 * [Return value]
 *     Synchronization mark value (nonzero).
 */
extern int resource_mark(ResourceManager *resmgr);

/**
 * resource_sync:  Return the synchronization status of the given mark
 * value.  If this function returns true, all resources whose loads were
 * started before the associated call to resource_mark() have completed
 * loading.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     mark: Synchronization mark value.
 * [Return value]
 *     True if all previous resources have completed loading, false if not.
 */
extern int resource_sync(ResourceManager *resmgr, int mark
#ifdef DEBUG
                         , const char *file, int line
#endif
);

/**
 * resource_wait:  Wait for synchronization at the given mark value.  When
 * this function returns, all resources whose loads were started before the
 * associated call to resource_mark() have completed loading.
 *
 * IMPORTANT: When background decompression is enabled, this function can
 * block indefinitely if a resource is being decompressed in the background
 * and the system's asynchronous operation table becomes full due to an
 * outside cause, such as loading numerous uncompressed resources in a
 * separate ResourceManager instance.  Decompression will resume once the
 * decompressor is able to start new read operations, but if there is no
 * separate thread to clear out old read operations, the program will
 * deadlock.
 *
 * The above problem does _not_ occur if the asynchronous operations
 * originate from the same ResourceManager instance, for example when
 * loading a mix of compressed and uncompressed resources into a single
 * ResourceManager.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     mark: Synchronization mark value.
 */
extern void resource_wait(ResourceManager *resmgr, int mark
#ifdef DEBUG
                          , const char *file, int line
#endif
);

/*-------------------------- Resource creation --------------------------*/

/**
 * resource_new_data:  Create a new data resource.  If size == 0, this
 * function succeeds, but the pointer returned from resource_get_data()
 * may not be indirected through (though it will be non-NULL).
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     size: Data size, in bytes.
 *     align: Memory alignment, in bytes.  If zero, the memory is aligned
 *         suitably for any basic data type.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_new_data(ResourceManager *resmgr, int size, int align,
                             int flags
#ifdef DEBUG
                             , const char *file, int line
#endif
);

/**
 * resource_copy_data:  Create a new data resource as a copy of an
 * existing data buffer. If size == 0, this function behaves like
 * resource_new_data().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     data: Data buffer to copy.
 *     size: Data size, in bytes.
 *     align: Memory alignment, in bytes.  If zero, the memory is aligned
 *         suitably for any basic data type.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_copy_data(ResourceManager *resmgr, const void *data,
                              int size, int align, int flags
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_strdup:  Create a new data resource as a copy of a string,
 * like strdup().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     str: String to copy.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_strdup(ResourceManager *resmgr, const char *str,
                           int flags
#ifdef DEBUG
                           , const char *file, int line
#endif
);

/**
 * resource_new_texture:  Create a new texture resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     width: Width of texture to create, in pixels.
 *     height: Height of texture to create, in pixels.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 *     mipmaps: True to autogenerate mipmaps.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_new_texture(ResourceManager *resmgr, int width, int height,
                                int flags, int mipmaps
#ifdef DEBUG
                                , const char *file, int line
#endif
);

/**
 * resource_new_texture_from_display:  Create a new texture resource
 * containing a copy of data from the display (or currently bound
 * framebuffer), as for texture_create_from_display().
 *
 * As with texture_create_from_display(), some OpenGL ES systems may be
 * unable to return texture data even if readable is set to true due to
 * platform-specific constraints.  See the documentation of that function
 * for details.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     x, y: Base display coordinates of region to copy, in pixels.
 *     w, h: Size of region to copy, in pixels.
 *     readable: False if the texture is not required to be readable (this
 *         may improve performance if the pixel data will never be read out).
 *     flags: Memory allocation flags (RES_ALLOC_*).
 *     mipmaps: True to autogenerate mipmaps.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_new_texture_from_display(
    ResourceManager *resmgr, int x, int y, int w, int h,
    int readable, int flags, int mipmaps
#ifdef DEBUG
    , const char *file, int line
#endif
);

/*----------------------- Resource data retrieval -----------------------*/

/**
 * resource_get_data:  Return the data pointer and size for the given data
 * resource.  This function fails if the given resource is not a data
 * resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 *     size_ret: Pointer to variable to receive the data size, in bytes.
 *         May be NULL if this value is not needed.
 * [Return value]
 *     Data pointer, or NULL on error.
 */
extern void *resource_get_data(const ResourceManager *resmgr, int id,
                               int *size_ret);

/**
 * resource_get_new_data:  Create a new data resource, and return its data
 * pointer.  Since the resource ID is not returned, the resource can only
 * be freed with resource_free_all() or resource_destroy().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     size: Data size, in bytes.
 *     align: Memory alignment, in bytes.  If zero, the memory is aligned
 *         suitably for any basic data type.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Data pointer, or NULL on error.
 */
extern void *resource_get_new_data(ResourceManager *resmgr, int size,
                                   int align, int flags
#ifdef DEBUG
                                   , const char *file, int line
#endif
);

/**
 * resource_get_copy_data:  Create a new data resource as a copy of a data
 * buffer, and return the data pointer.  Since the resource ID is not
 * returned, the resource can only be freed with resource_free_all() or
 * resource_destroy().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     data: Data buffer to copy.
 *     size: Data size, in bytes.
 *     align: Memory alignment, in bytes.  If zero, the memory is aligned
 *         suitably for any basic data type.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Data pointer, or NULL on error.
 */
extern void * resource_get_copy_data(ResourceManager *resmgr, const void *data,
                                     int size, int align, int flags
#ifdef DEBUG
                                     , const char *file, int line
#endif
);

/**
 * resource_get_strdup:  Create a new data resource as a copy of a string,
 * and return the data pointer.  Since the resource ID is not returned, the
 * resource can only be freed with resource_free_all() or resource_destroy().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     str: String to copy.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 * [Return value]
 *     String pointer, or NULL on error.
 */
extern char *resource_get_strdup(ResourceManager *resmgr, const char *str,
                                 int flags
#ifdef DEBUG
                                 , const char *file, int line
#endif
);

/**
 * resource_get_texture:  Return the texture ID for the given texture
 * resource.  This function fails if the given resource is not a texture
 * resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     Texture ID, or zero on error.
 */
extern int resource_get_texture(const ResourceManager *resmgr, int id);

/**
 * resource_get_new_texture:  Create a new texture resource, and return its
 * texture ID.  Since the resource ID is not returned, the resource can
 * only be freed with resource_free_all() or resource_destroy().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     width: Width of texture to create, in pixels.
 *     height: Height of texture to create, in pixels.
 *     flags: Memory allocation flags (RES_ALLOC_*).
 *     mipmaps: True to autogenerate mipmaps.
 * [Return value]
 *     Texture ID, or zero on error.
 */
extern int resource_get_new_texture(ResourceManager *resmgr, int width,
                                    int height, int flags, int mipmaps
#ifdef DEBUG
                                    , const char *file, int line
#endif
);

/**
 * resource_get_new_texture_from_display:  Create a new texture resource
 * containing a copy of data from the display, and return its texture ID.
 * Since the resource ID is not returned, the resource can only be freed
 * with resource_free_all() or resource_destroy().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     x, y: Base display coordinates of region to copy, in pixels.
 *     w, h: Size of region to copy, in pixels.
 *     readable: False if the texture is not required to be readable (this
 *         may improve performance if the pixel data will never be read out).
 *     flags: Memory allocation flags (RES_ALLOC_*).
 *     mipmaps: True to autogenerate mipmaps.
 * [Return value]
 *     Texture ID, or zero on error.
 */
extern int resource_get_new_texture_from_display(
    ResourceManager *resmgr, int x, int y, int w, int h,
    int readable, int flags, int mipmaps
#ifdef DEBUG
    , const char *file, int line
#endif
);

/**
 * resource_get_font:  Return the font ID for the given font resource.
 * This function fails if the given resource is not a font resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     Font ID, or zero on error.
 */
extern int resource_get_font(const ResourceManager *resmgr, int id);

/**
 * resource_get_sound:  Return the Sound object pointer for the given sound
 * resource.  This function fails if the given resource is not a sound
 * resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     Sound object, or NULL on error.
 */
extern struct Sound *resource_get_sound(const ResourceManager *resmgr, int id);

/*------------------------ Raw data file access -------------------------*/

/**
 * resource_open_file:  Open a data resource for random access without
 * loading it into memory.  On success, the resource can be immediately
 * used with resource_read_file() or other raw data file access functions
 * (there is no need to sync the resource).
 *
 * This function fails if the given resource does not exist, or if the
 * resource is stored compressed in a package file.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_open_file(ResourceManager *resmgr, const char *name
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_get_file_size:  Return the size of the given data file
 * resource, which must have been opened with resource_open_file().
 *
 * This function always succeeds when given a valid resource ID.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     File size, or zero if a parameter is invalid.
 */
extern int64_t resource_get_file_size(const ResourceManager *resmgr, int id);

/**
 * resource_set_file_position:  Set the data offset into the given data
 * file resource from which the next call to resource_read_file() will read
 * data.  The resource must have been opened with resource_open_file().
 *
 * This function always succeeds when given a valid resource ID.  If the
 * requested position is less than zero, it is taken as zero; if greater
 * than the size of the resource, it is taken as the size of the resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 *     pos: New read position.
 */
extern void resource_set_file_position(const ResourceManager *resmgr, int id,
                                       int64_t pos);

/**
 * resource_get_file_position:  Return the current read offset for the
 * given data file resource, which must have been opened with
 * resource_open_file().
 *
 * This function always succeeds when given a valid resource ID.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     File size, or zero if a parameter is invalid.
 */
extern int64_t resource_get_file_position(const ResourceManager *resmgr,
                                          int id);

/**
 * resource_read_file:  Read data from the given data file resource, which
 * must have been opened with resource_open_file().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 *     buf: Buffer into which to read data.
 *     len: Number of bytes to read.
 * [Return value]
 *     Number of bytes read, or a negative value on error.
 */
extern int resource_read_file(const ResourceManager *resmgr, int id,
                              void *buf, int len);

/**
 * resource_read_file_at:  Read data from a specified position in the given
 * data file resource, which must have been opened with resource_open_file().
 * Calling this function does not change the file position used for reading
 * with resource_read_file().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 *     buf: Buffer into which to read data.
 *     len: Number of bytes to read.
 *     pos: File position from which to read.
 * [Return value]
 *     Number of bytes read, or a negative value on error.
 */
extern int resource_read_file_at(const ResourceManager *resmgr, int id,
                                 void *buf, int len, int64_t pos);

/**
 * resource_get_file_handle:  Return a low-level file handle for the given
 * data file resource, which must have been opened with resource_open_file().
 * The returned file handle is suitable for returning from a custom package
 * format's PackageFileInfoFunc implementation in the file_ret parameter;
 * it should not be used for any other purpose.
 *
 * This function always succeeds when given a valid resource ID.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 *     offset_ret: Pointer to variable to receive the resource's data offset
 *         within the low-level file.
 * [Return value]
 *     Low-level file handle, or NULL if a parameter is invalid.
 */
extern struct SysFile *resource_get_file_handle(const ResourceManager *resmgr,
                                                int id, int64_t *offset_ret);

/*---------------------- Other resource operations ----------------------*/

/**
 * resource_open_sound:  Open a streaming sound resource.  On success, the
 * Sound object can be immediately retrieved with resource_get_sound()
 * (there is no need to sync the resource).
 *
 * This function fails if the given resource does not exist, or if the
 * resource is stored compressed in a package file (this only refers to
 * package file compression, not audio data compression such as MP3).
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     name: Resource name.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_open_sound(ResourceManager *resmgr, const char *name
#ifdef DEBUG
                               , const char *file, int line
#endif
);

/**
 * resource_open_sound_from_file:  Open a streaming sound resource
 * embedded in a data file.  On success, the Sound object can be
 * immediately retrieved with resource_get_sound() (there is no need to
 * sync the resource).
 *
 * The data file resource must have been opened with resource_open_file().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance in which to create new resource.
 *     file_resmgr: ResourceManager instance containing data file resource.
 *     file_id: ID of data file resource.
 *     offset: Byte offset within data file at which sound resource starts.
 *     size: Size of sound resource data, in bytes.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_open_sound_from_file(
    ResourceManager *resmgr, ResourceManager *file_resmgr, int file_id,
    int64_t offset, int size
#ifdef DEBUG
    , const char *file, int line
#endif
);

/**
 * resource_take_data:  Take ownership of the given data buffer, treating
 * it as a data resource.  The data buffer must have been allocated using
 * mem_alloc().
 *
 * The value passed in for "size" is only used for returning via
 * resource_get_data(), and can be set to zero if (for example) the caller
 * does not know the buffer size, without any ill effects.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     data: Data buffer to take ownership of.
 *     size: Data size, in bytes.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_take_data(ResourceManager *resmgr, void *data, int size
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_take_texture:  Take ownership of the given texture, treating it
 * as a texture resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     texture_id: ID of texture to take ownership of.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_take_texture(ResourceManager *resmgr, int texture_id
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_take_sound:  Take ownership of the given sound, treating it as
 * a sound resource.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     sound: Sound object to take ownership of.
 * [Return value]
 *     Resource ID (nonzero), or zero on error.
 */
extern int resource_take_sound(ResourceManager *resmgr, struct Sound *sound
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_link:  Create a link to the given resource, which may be owned
 * by a different ResourceManager instance.  The resource will not be
 * destroyed until all links, including the ID under which it was
 * originally loaded, have been freed with resource_free() or
 * resource_free_all().
 *
 * [Parameters]
 *     resmgr: ResourceManager instance in which to create the link.
 *     old_resmgr: ResourceManager instance which owns the existing resource.
 *     old_id: Resource ID of the existing resource.
 * [Return value]
 *     Resource ID of the link (nonzero), or zero on error.
 */
extern int resource_link(ResourceManager *resmgr,
                         const ResourceManager *old_resmgr, int old_id
#ifdef DEBUG
                         , const char *file, int line
#endif
);

/**
 * resource_link_weak:  Create a weak link to the given resource.  Unlike
 * ordinary ("strong") links, weak links do not pin a resource in memory.
 * When the last strong link to a resource is freed, the resource is
 * destroyed; any remaining weak links to that resource become stale, and
 * attempting to retrieve the resource's data will result in an error.
 * This error case can be differentiated from other errors by calling
 * resource_is_stale() on the weak link.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance in which to create the link.
 *     old_resmgr: ResourceManager instance which owns the existing resource.
 *     old_id: Resource ID of the existing resource.
 * [Return value]
 *     Resource ID of the link (nonzero), or zero on error.
 */
extern int resource_link_weak(ResourceManager *resmgr,
                              const ResourceManager *old_resmgr, int old_id
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * resource_is_stale:  Return whether the given resource ID is a stale
 * link.  This function returns false for any resource ID which is not a
 * weak link, including invalid ID values.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 * [Return value]
 *     True if the resource is a stale link, false otherwise.
 */
extern int resource_is_stale(const ResourceManager *resmgr, int id);

/**
 * resource_free:  Free the given resource.  The resource data itself is
 * not destroyed if there are any strong links to the resource remaining.
 * This function does nothing if id == 0.
 *
 * If the given resource is currently being loaded, this function may block
 * until the load completes.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 *     id: Resource ID.
 */
extern void resource_free(ResourceManager *resmgr, int id
#ifdef DEBUG
                          , const char *file, int line
#endif
);

/**
 * resource_free_all:  Free all resources managed by the given
 * ResourceManager instance.
 *
 * If any resources are currently being loaded, this function may block
 * until those loads complete.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 */
extern void resource_free_all(ResourceManager *resmgr
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/*----------------- Package module creation/destruction -----------------*/

/**
 * pkg_create_instance:  Create a new package module instance for a
 * PKG-format package file.
 *
 * [Parameters]
 *     package_path: Pathname of the package file.  This pathname is
 *         resolved in the same manner as names for resource data files.
 *     prefix: Resource pathname prefix to apply to files in this package.
 *         For example, a prefix of "pkg:" would expose a resource named
 *         "file.dat" as "pkg:file.dat".
 * [Return value]
 *     Newly created module instance, or NULL on error.
 */
extern struct PackageModuleInfo *pkg_create_instance(
    const char *package_path, const char *prefix);

/**
 * pkg_destroy_instance:  Destroy a package module instance for a
 * PKG-format package file.  The instance must not be registered with the
 * resource management routines.
 *
 * This function does nothing if module == NULL.
 *
 * [Parameters]
 *     module: Module instance to destroy.
 */
extern void pkg_destroy_instance(struct PackageModuleInfo *module);

/*************************************************************************/
/************************** Debugging wrappers ***************************/
/*************************************************************************/

/*
 * When debugging is enabled, we wrap allocating calls with these macros
 * which pass down the source file and line at which the call was made.
 * Without these macros, every resource-related memory allocation would
 * appear to have been made by resource/core.c, significantly hampering
 * debugging of resource-related memory issues.
 */

#ifdef DEBUG
# define resource_create(num_resources) \
    resource_create((num_resources), __FILE__, __LINE__)
# define resource_destroy(resmgr) \
    resource_destroy((resmgr), __FILE__, __LINE__)
# define resource_load_data(resmgr,name,align,flags) \
    resource_load_data((resmgr), (name), (align), (flags), __FILE__, __LINE__)
# define resource_load_texture(resmgr,name,flags,mipmaps) \
    resource_load_texture((resmgr), (name), (flags), (mipmaps), \
                          __FILE__, __LINE__)
# define resource_load_bitmap_font(resmgr,name,flags) \
    resource_load_bitmap_font((resmgr), (name), (flags), __FILE__, __LINE__)
# define resource_load_freetype_font(resmgr,name,flags) \
    resource_load_freetype_font((resmgr), (name), (flags), __FILE__, __LINE__)
# define resource_load_sound(resmgr,name,flags) \
    resource_load_sound((resmgr), (name), (flags), __FILE__, __LINE__)
# define resource_sync(resmgr,mark) \
    resource_sync((resmgr), (mark), __FILE__, __LINE__)
# define resource_wait(resmgr,mark) \
    resource_wait((resmgr), (mark), __FILE__, __LINE__)
# define resource_new_data(resmgr,size,align,flags) \
    resource_new_data((resmgr), (size), (align), (flags), __FILE__, __LINE__)
# define resource_copy_data(resmgr,data,size,align,flags) \
    resource_copy_data((resmgr), (data), (size), (align), (flags), \
                       __FILE__, __LINE__)
# define resource_strdup(resmgr,str,flags) \
    resource_strdup((resmgr), (str), (flags), __FILE__, __LINE__)
# define resource_new_texture(resmgr,width,height,flags,mipmaps) \
    resource_new_texture((resmgr), (width), (height), (flags), (mipmaps), \
                         __FILE__, __LINE__)
# define resource_new_texture_from_display(resmgr,x,y,w,h,readable,flags,mipmaps) \
    resource_new_texture_from_display( \
        (resmgr), (x), (y), (w), (h), (readable), (flags), (mipmaps), \
        __FILE__, __LINE__)
# define resource_get_new_data(resmgr,size,align,flags) \
    resource_get_new_data((resmgr), (size), (align), (flags), __FILE__, \
                          __LINE__)
# define resource_get_copy_data(resmgr,data,size,align,flags) \
    resource_get_copy_data((resmgr), (data), (size), (align), (flags), \
                           __FILE__, __LINE__)
# define resource_get_strdup(resmgr,str,flags) \
    resource_get_strdup((resmgr), (str), (flags), __FILE__, __LINE__)
# define resource_get_new_texture(resmgr,width,height,flags,mipmaps) \
    resource_get_new_texture((resmgr), (width), (height), (flags), (mipmaps), \
                             __FILE__, __LINE__)
# define resource_get_new_texture_from_display(resmgr,x,y,w,h,readable,flags,mipmaps) \
    resource_get_new_texture_from_display( \
        (resmgr), (x), (y), (w), (h), (readable), (flags), (mipmaps), \
        __FILE__, __LINE__)
# define resource_open_file(resmgr,name) \
    resource_open_file((resmgr), (name), __FILE__, __LINE__)
# define resource_open_sound(resmgr,name) \
    resource_open_sound((resmgr), (name), __FILE__, __LINE__)
# define resource_open_sound_from_file(resmgr,file_resmgr,file_id,offset,size) \
    resource_open_sound_from_file((resmgr), (file_resmgr), (file_id), \
                                  (offset), (size), __FILE__, __LINE__)
# define resource_take_data(resmgr,data,size) \
    resource_take_data((resmgr), (data), (size), __FILE__, __LINE__)
# define resource_take_texture(resmgr,texture_id) \
    resource_take_texture((resmgr), (texture_id), __FILE__, __LINE__)
# define resource_take_sound(resmgr,sound) \
    resource_take_sound((resmgr), (sound), __FILE__, __LINE__)
# define resource_link(resmgr,old_resmgr,old_id) \
    resource_link((resmgr), (old_resmgr), (old_id), __FILE__, __LINE__)
# define resource_link_weak(resmgr,old_resmgr,old_id) \
    resource_link_weak((resmgr), (old_resmgr), (old_id), __FILE__, __LINE__)
# define resource_free(resmgr,id) \
    resource_free((resmgr), (id), __FILE__, __LINE__)
# define resource_free_all(resmgr) \
    resource_free_all((resmgr), __FILE__, __LINE__)
#endif

/*************************************************************************/
/*************************************************************************/

#ifdef __cplusplus
# undef private
#endif
EXTERN_C_END

#endif  // SIL_RESOURCE_H
