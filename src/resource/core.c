/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/resource/core.c: Resource management functions.
 */

/*
 * Resources are managed using an array of ResourceInfo structures.  The
 * resource_create() function allocates the private data for the instance
 * itself (ResourceManagerPrivate) and the ResourceInfo array; resource
 * loading/creation functions (resource_load_*(), resource_new_*()) then
 * register new resources in this array.  Management of the array itself
 * is handled by the helper functions add_resource() and del_resource().
 *
 * Static resource managers defined with DEFINE_STATIC_RESOURCEMANAGER()
 * include a static buffer for the ResourceManagerPrivate structure and
 * ResourceInfo array.  The VALIDATE_RESMGR() macro used by all interface
 * functions to validate the ResourceManager argument detects the case of
 * an uninitialized static resource manager and sets up private data
 * structures appropriately, so that the caller does not need to make an
 * explicit initialization call.
 *
 * The resource ID used to identify resources is in fact just 1 added to
 * the index of the resource in the ResourceInfo array.  However, for the
 * sake of readability and forward compatibility, the functions
 * resource_to_id() and id_to_resource() are used to convert between
 * ResourceInfo pointers and resource IDs.
 *
 * Creation and deletion of resources is accomplished simply by modifying
 * the "type" field in the ResourceInfo structure; resources are never
 * moved around in the array.  This is required in order to preserve links
 * between resources (which store a ResourceInfo pointer in the
 * ResourceInfo.link_next field).
 *
 * If no ResourceInfo entries are available when adding a resource, the
 * array is lengthened using mem_realloc(), and one of the newly-added
 * entries is used.  Since the reallocated array may live for a long time,
 * the reallocation is done with the MEM_ALLOC_TEMP and MEM_ALLOC_TOP flags
 * set, to try and minimize the risk of memory fragmentation (on systems
 * where it matters).
 */

#include "src/base.h"
#include "src/font.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/resource.h"
#include "src/resource/package.h"
#include "src/sound.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/thread.h"
#include "src/workqueue.h"

/* Disable the memory debugging macros from the bottom of resource.h, along
 * with similar macros in other headers.  Also define convenience macros
 * for declaring and passing debug parameters, to avoid unneeded messiness
 * in the actual code. */
#ifdef DEBUG
# undef resource_create
# undef resource_destroy
# undef resource_load_data
# undef resource_load_texture
# undef resource_load_bitmap_font
# undef resource_load_freetype_font
# undef resource_load_sound
# undef resource_sync
# undef resource_wait
# undef resource_new_data
# undef resource_copy_data
# undef resource_strdup
# undef resource_new_texture
# undef resource_new_texture_from_display
# undef resource_get_new_data
# undef resource_get_copy_data
# undef resource_get_strdup
# undef resource_get_new_texture
# undef resource_get_new_texture_from_display
# undef resource_open_file
# undef resource_open_sound
# undef resource_open_sound_from_file
# undef resource_take_data
# undef resource_take_texture
# undef resource_take_sound
# undef resource_link
# undef resource_link_weak
# undef resource_free
# undef resource_free_all
# undef sound_create
# undef sound_create_stream
# undef sound_destroy
# define __DEBUG_PARAMS  , const char *file, int line
# define __DEBUG_ARGS    , file, line
#else
# define __DEBUG_PARAMS  /*nothing*/
# define __DEBUG_ARGS    /*nothing*/
#endif

/*************************************************************************/
/****** Private resource management data and structure definitions *******/
/*************************************************************************/

/* Forward declarations for structured types defined below. */
typedef struct LoadInfo LoadInfo;
typedef struct ResourceInfo ResourceInfo;

/*-----------------------------------------------------------------------*/

/* Linked list of all registered package module instances. */
static PackageModuleInfo *packages = NULL;

/* Background decompression settings. */
static int bgdecomp_on = 0;  // Default to off.
static int bgdecomp_threshold;
static int bgdecomp_buffer_size;

/* Work queue for background decompression. */
int bgdecomp_workqueue;

/*-----------------------------------------------------------------------*/

/* Resource types. */

typedef enum ResourceType {
    RES_UNUSED = 0,     // Unused ResourceInfo entry.
    RES_UNKNOWN,        // Unknown resource type (used temporarily for links).
    RES_DATA,           // Data resource.
    RES_TEXTURE,        // Texture resource.
    RES_FONT,           // Font resource.
    RES_SOUND,          // Sound resource.
    RES_FILE,           // Raw data file resource (from resource_open_file()).
} ResourceType;

/*----------------------------------*/

/* Data used only while loading a resource (freed when the load completes). */

struct LoadInfo {

    /* Pointer to the buffer into which data is to be read.  For background
     * decompression, this is used to hold the decompressed data. */
    void *file_data;

    /* Background decompression work unit ID (0 if not doing background
     * decompression). */
    int decomp_wu;

    /* Decompression state buffer. */
    void *decomp_state;

    /* Read buffers and size (in bytes) for background decompression.
     * (The size is stored in case the global background decompression
     * settings are changed while a load is in progress.) */
    void *decomp_read_buffer[2];
    int decomp_buffer_size;

    /* Does the data need to be decompressed after loading?  (This will be
     * false if the data is decompressed during load, i.e. when background
     * decompression is enabled and applied to this resource.) */
    uint8_t compressed;

    /* Flags used during loading. */
    uint8_t need_close;     // True if the file should be closed after loading.
    uint8_t need_finish;    // True if all data has been read in or if a
                            //    read operation failed.
    uint8_t read_failed;    // True if a read operation failed.
    uint8_t decomp_failed;  // True if background decompression failed.
    uint8_t decomp_abort;   // True to force background decompression to abort.

    /* Auto-mipmap flag for textures. */
    uint8_t texture_mipmaps;

    /* Memory alignment and allocation flags. */
    int mem_align;
    int mem_flags;

#ifdef DEBUG
    /* Memory type (to pass to debug_mem_*()). */
    int mem_type;
#endif

    /* Compressed and uncompressed data sizes (both in bytes). */
    int compressed_size;
    int data_size;

    /* Current asynchronous read request ID and number of bytes expected to
     * be read. */
    int read_request;
    int read_expected;

    /* Package module instance from which this resource is being loaded. */
    PackageModuleInfo *pkginfo;

    /* File handle and base offset for reading data. */
    SysFile *fp;
    int64_t data_offset;

    /* Parser function for font data (only used for font resources). */
    int (*font_parser)(void *data, int len, int mem_flags, int reuse);

#ifdef DEBUG
    /* Resource pathname (for debug messages). */
    char debug_path[100];
#endif
};

/*----------------------------------*/

/* Data for a single resource.  We force 8-byte alignment on this structure
 * so the structure size is constant across 32-bit platforms regardless of
 * whether int64_t requires 8-byte alignment (this is needed for the
 * static buffer sizing macros in resource.h to work correctly). */

struct ResourceInfo {
    /* Resource type (RES_*, defined above). */
    ALIGNED(8) ResourceType type;

    /* Pointer to the (private data of the) ResourceManager instance which
     * owns this resource. */
    ResourceManagerPrivate *owner;

    /* Circular linked list pointer for managing resource links. */
    ResourceInfo *link_next;

    /* Resource data buffer and size. */
    union {
        void *data;
        int texture;
        int font;
        Sound *sound;
        SysFile *fp;
    };
    int64_t size;  // Only used for data and file resources.
    int64_t offset;  // Only used for file resources.

    /* Synchronization mark (the value of ResourceManagerPrivate.mark when
     * the relevant load() or new() function was called). */
    int32_t mark;

    /* Weak and stale link flags. */
    uint8_t is_weak_link;
    uint8_t is_stale_link;

    /* Data used while loading (non-NULL indicates that the resource has
     * not yet finished loading).  Shared among all links to the same
     * resource. */
    LoadInfo *loadinfo;
};

/*----------------------------------*/

/* Private data for a ResourceManager instance (pointed to by
 * ResourceManager.private). */

struct ResourceManagerPrivate {
    /* Array and size (in elements) of the ResourceInfo array used to
     * manage resources. */
    ResourceInfo *resources;
    int32_t resources_size;

    /* Flag indicating whether this ResourceManager was allocated by
     * resource_create(). */
    uint8_t self_allocated;

    /* Flags indicating whether internal data is stored in the static
     * buffer (ResourceManager.static_buffer) associated with this instance. */
    uint8_t private_is_static;    // This structure is in the static buffer.
    uint8_t resources_is_static;  // resources[] is in the static buffer.

    /* Current mark value for synchronization.  resource_mark() adds 1 to
     * this value on every call, and resource_{sync,wait}() check for
     * resources with a mark value less than the passed-in value.  To
     * avoid bugs with wraparound, "less than" is checked not with a
     * literal less-than operation, but by looking at the difference
     * between the two mark values. */
    int32_t mark;

#ifdef DEBUG
    /* Source file/line which called resource_create() or used the
     * DEFINE_STATIC_RESOURCEMANAGER() macro for this instance (used in
     * debug messages). */
    char owner[128];
#endif
};

/*----------------------------------*/

/* Structure used for file list handles. */

struct ResourceFileListHandle {
    /* Directory pathname being looked up. */
    char *path;

    /* Is this a recursive list operation? */
    uint8_t recursive;

    /* Pathname buffer for most recently returned pathname. */
    int returned_file_size;
    char *returned_file;

    /* Prefix to prepend to returned filenames, or NULL if none.  Used for
     * host filesystem recursion. */
    char *return_prefix;

    /* Associated package module instance (NULL if none). */
    PackageModuleInfo *package;

    /* Directory handle for sys_dir_read(). */
    SysDir *dir;
    /* Recursive nesting level, for host filesystem infinite-loop detection. */
    int recursion_level;
    /* Pointer to next level's ResourceFileListHandle if currently in a
     * host-filesystem subdirectory (else NULL). */
    ResourceFileListHandle *subdir_handle;
};

/*----------------------------------*/

/* Ensure that the size macros used for static buffers really are the
 * right size. */
STATIC_ASSERT(_SIL_RESOURCE_SIZE1 * sizeof(void *)
                  == sizeof(ResourceManagerPrivate),
              "_SIL_RESOURCE_SIZE1 definition is wrong");
STATIC_ASSERT(_SIL_RESOURCE_SIZE2 * sizeof(void *) == sizeof(ResourceInfo),
              "_SIL_RESOURCE_SIZE2 definition is wrong");

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* These variables are only used to implement test-specific behaviors. */

/* Path prefix to use for generating resource file pathnames, or NULL for
 * the default behavior. */
static const char *
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    TEST_override_path_prefix = NULL;

/* Should resource_sync() act as though all read operations would block? */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_block_load = 0;

/* Should resource_sync() return without calling finish_load()? */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_silent_sync = 0;

/* Force forward or reverse order for resource load finalization regardless
 * of SIL_RESOURCE_SYNC_IN_REVERSE (-1 = no override). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    int8_t TEST_sync_order = -1;

/*************************************************************************/
/*********** Convenience macros and local routine declarations ***********/
/*************************************************************************/

/**
 * VALIDATE_RESMGR:  Check that the given ResourceManager instance is
 * valid (non-NULL and initialized).  If valid, a pointer to the associated
 * ResourceManagerPrivate structure will be stored in the variable named by
 * private_var; otherwise the code given in error_return will be executed
 * (this may be one or more statements, but must end in a return).
 */
#define VALIDATE_RESMGR(resmgr,private_var,error_return) do { \
    if (UNLIKELY(!(resmgr))) {                      \
        DLOG(#resmgr " == NULL");                   \
        error_return;                               \
    }                                               \
    private_var = get_private((resmgr));            \
    if (UNLIKELY(!private_var)) {                   \
        DLOG("resmgr at %p is corrupt", (resmgr));  \
        error_return;                               \
    }                                               \
} while (0)

/**
 * VALIDATE_CONST_RESMGR:  Check that the given ResourceManager instance is
 * valid.  Behaves like VALIDATE_RESMGR(), except that an uninitialized
 * static instance will be silently treated as an empty instance rather
 * than being initialized.  private_var must be a const pointer.
 */
#define VALIDATE_CONST_RESMGR(resmgr,private_var,error_return) do { \
    if (UNLIKELY(!(resmgr))) {                      \
        DLOG(#resmgr " == NULL");                   \
        error_return;                               \
    }                                               \
    private_var = get_private_noinit((resmgr));     \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * compare_marks:  Compare two synchronization mark values, taking
 * wraparound into account.  The return value follows the pattern of
 * strcmp().
 *
 * [Parameters]
 *     a, b: Mark values to compare.
 * [Return value]
 *     <0: a < b
 *      0: a == b
 *     >0: a > b
 */
static CONST_FUNCTION int compare_marks(int a, int b);

/**
 * convert_mem_flags:  Convert RES_ALLOC_* flags to the corresponding
 * MEM_ALLOC_* flags.
 *
 * [Parameters]
 *     res_flags: RES_ALLOC_* flag set.
 * [Return value]
 *     Corresponding MEM_ALLOC_* flag set.
 */
static CONST_FUNCTION int convert_mem_flags(int res_flags);

/**
 * list_files_format_path:  Prepend the path prefix for the given list
 * handle to the given local pathname and store the result in the handle's
 * returned_file buffer, expanding it as necessary.  Helper for
 * resource_list_files_next().
 *
 * [Parameters]
 *     handle: List handle.
 *     name: Name of file to format.
 * [Return value]
 *     True on success, false if out of memory.
 */
static int list_files_format_path(ResourceFileListHandle *handle,
                                  const char *name);

/**
 * generate_path:  Generate the host filesystem path corresponding to the
 * given resource name.  The returned value may point either into the name
 * string passed to the function or into the provided buffer.  (The buffer
 * is only used when prepending the common path prefix to the pathname;
 * otherwise the returned value points into the passed-in string.)
 *
 * See the documentation in include/SIL/resource.h for details about the
 * algorithm used to generate pathnames.
 *
 * [Parameters]
 *     name: Resource name for which to generate a pathname.
 *     buffer: Buffer to use for pathname generation.
 *     bufsize: Size of buffer, in bytes.
 * [Return value]
 *     Pathname corresponding to the given resource name, or NULL if a
 *     buffer overflow condition was detected.
 */
static const char *generate_path(const char *name, char *buffer, int bufsize);

/**
 * find_file:  Return the file handle and associated data for the given
 * resource name.
 *
 * On success, the file handle returned in *fh_ret belongs to the caller
 * if and only if *pkginfo_ret == NULL.
 *
 * [Parameters]
 *     name: Resource name to look up.
 *     pkginfo_ret: Pointer to variable to receive the module pointer for
 *         the package module handling the resource or NULL if none.  May
 *         be NULL if this value is not needed, but it is an error to pass
 *         NULL when fh_ret is not also NULL.
 *     fh_ret: Pointer to variable to receive the file handle.  May be
 *         NULL if the file handle is not needed (such as when testing for
 *         existence).
 *     offset_ret: Pointer to variable to receive the offset in bytes from
 *         the beginning of the file to the data for the given resource.
 *         May be NULL if this value is not needed.
 *     length_ret: Pointer to variable to receive the length in bytes of
 *         the resource's data as stored in the file (after compression,
 *         if applicable).  May be NULL if this value is not needed.
 *     compressed_ret: Pointer to variable to receive a Boolean value
 *         indicating whether the resource data is compressed.  May be
 *         NULL if this value is not needed.
 *     size_ret: Pointer to variable to receive the size in bytes of the
 *         resource data after any decompression has been applied.  May be
 *         NULL if this value is not needed.
 * [Return value]
 *     True if the resource was found, false if not.
 */
static int find_file(const char *name, PackageModuleInfo **pkginfo_ret,
                     SysFile **fh_ret, int64_t *offset_ret, int *length_ret,
                     int *compressed_ret, int *size_ret);

/*-----------------------------------------------------------------------*/

/**
 * get_private:  Return the ResourceManagerPrivate structure pointer for
 * the given ResourceManager instance.  If the instance is a static
 * ResourceManager which has not yet been initialized, the instance is
 * initialized to empty.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 * [Return value]
 *     ResourceManagerPrivate structure pointer, or NULL on error.
 */
static ResourceManagerPrivate *get_private(ResourceManager *resmgr);

/**
 * get_private_noinit:  Return the ResourceManagerPrivate structure pointer
 * for the given ResourceManager instance.  If the instance is a static
 * ResourceManager which has not yet been initialized, the instance is
 * _not_ initialized; instead, a pointer to an empty ResourceManagerPrivate
 * structure (unrelated to the passed-in ResourceManager) is returned.
 *
 * Note that unlike get_private(), this function always returns a valid
 * ResourceManagerPrivate pointer.
 *
 * [Parameters]
 *     resmgr: ResourceManager instance.
 * [Return value]
 *     ResourceManagerPrivate structure pointer (always non-NULL).
 */
static const ResourceManagerPrivate *get_private_noinit(
    const ResourceManager *resmgr);

/**
 * resource_to_id:  Return the resource ID corresponding to the given
 * ResourceInfo structure pointer.
 *
 * [Parameters]
 *     private: ResourceManagerPrivate structure for the owning
 *         ResourceManager.
 *     resinfo: ResourceInfo structure.
 * [Return value]
 *     Resource ID.
 */
static int resource_to_id(const ResourceManagerPrivate *private,
                          ResourceInfo *resinfo);

/**
 * id_to_resource:  Return a pointer to the ResourceInfo structure
 * corresponding to the given resource ID.
 *
 * [Parameters]
 *     private: ResourceManagerPrivate structure for the owning
 *         ResourceManager.
 *     id: Resource ID.
 * [Return value]
 *     ResourceInfo structure, or NULL if the ID is invalid.
 */
static ResourceInfo *id_to_resource(const ResourceManagerPrivate *private,
                                    int id);

/**
 * add_resource:  Add a new resource to a ResourceManager, extending the
 * resources[] array if necessary.
 *
 * [Parameters]
 *     private: ResourceManagerPrivate structure for the owning
 *         ResourceManager.
 *     type: Resource type (RES_*).
 * [Return value]
 *     Pointer to the newly-added ResourceInfo structure, or NULL on error.
 */
static ResourceInfo *add_resource(ResourceManagerPrivate *private,
                                  ResourceType type __DEBUG_PARAMS);

/**
 * del_resource:  Delete a resource from a ResourceManager.
 *
 * [Parameters]
 *     resinfo: ResourceInfo structure for the resource to delete.
 */
static void del_resource(ResourceInfo *resinfo);

/**
 * load_resource:  Add a new resource to a ResourceManager, and start
 * loading data into it.
 *
 * [Parameters]
 *     private: ResourceManagerPrivate structure for the owning
 *         ResourceManager.
 *     type: Resource type (RES_*).
 *     name: Resource name.
 *     align: Alignment value to pass to mem_alloc().
 *     flags: Resource allocation flags (RES_ALLOC_*).
 * [Return value]
 *     Pointer to the newly-added ResourceInfo structure, or NULL on error.
 */
static ResourceInfo *load_resource(
    ResourceManagerPrivate *private, ResourceType type,
    const char *name, uint16_t align, int flags __DEBUG_PARAMS);

/**
 * wait_resource:  Wait for a resource to finish loading.  This function
 * handles avoiding deadlock on background decompression when starved for
 * asynchronous read slots (see the note in the resource_wait() function
 * documentation for details), so this function should be called instead
 * of directly calling loadinfo_sync(loadinfo,1,0).
 *
 * [Parameters]
 *     private: ResourceManagerPrivate structure for the owning
 *         ResourceManager.
 *     index: Index in private->resources[] of the resource to wait for.
 */
static void wait_resource(ResourceManagerPrivate *private, int index);

/**
 * free_resource:  Free the data associated with a resource (but not the
 * resource itself).  If there are other strong links to the resource, the
 * specified link is removed, but the resource itself is left alone.
 *
 * If the resource is being loaded, this function may block until the load
 * completes.
 *
 * [Parameters]
 *     resinfo: ResourceInfo structure for the resource to free.
 */
static void free_resource(ResourceInfo *resinfo __DEBUG_PARAMS);

/*-----------------------------------------------------------------------*/

/**
 * load_data:  Start loading the data for the named resource.  The
 * ResourceInfo structure must be initialized (except for the pkginfo and
 * loading fields).
 *
 * [Parameters]
 *     resinfo: ResourceInfo structure for the resource to load.
 *     name: Resource name.
 * [Return value]
 *     True on success, false on error.
 */
static int load_data(ResourceInfo *resinfo, const char *path __DEBUG_PARAMS);

/**
 * start_async_read:  Start the asynchronous read operation for an
 * uncompressed or foreground-decompressed LoadInfo structure.
 *
 * [Parameters]
 *     loadinfo: LoadInfo structure for which to start the read operation.
 * [Return value]
 *     True on success, false on error.
 */
static int start_async_read(LoadInfo *loadinfo);

/**
 * start_background_decompress_locked:  Start background decompression for
 * the given LoadInfo structure.
 *
 * [Parameters]
 *     loadinfo: LoadInfo structure for which to start decompression.
 * [Return value]
 *     True on success, false on error.
 */
static int start_background_decompress(LoadInfo *loadinfo);

/**
 * start_fallback_decompress:  Start foreground decompression for the given
 * LoadInfo structure, which is assumed to have failed to start background
 * decompression.
 *
 * [Parameters]
 *     loadinfo: LoadInfo structure for which to start decompression.
 *     file, line: Source file and line number for debug_mem_alloc().
 * [Return value]
 *     True on success, false on error.
 */
/* Omit the file and line number parameters when not building in debug mode. */
#ifndef DEBUG
# define start_fallback_decompress(loadinfo,file,line) \
    start_fallback_decompress(loadinfo)
#endif
static int start_fallback_decompress(
    LoadInfo *loadinfo, const char *file, int line);

/**
 * loadinfo_sync:  Check the load state of the given LoadInfo structure,
 * and update it as necessary.
 *
 * [Parameters]
 *     loadinfo: LoadInfo structure to check.
 *     do_wait: True to wait for read completion, false to poll only.
 *     do_abort: True to abort the load.
 * [Return value]
 *     True if all read operations have completed, false otherwise.
 */
static int loadinfo_sync(LoadInfo *loadinfo, int do_wait, int do_abort);

/**
 * finish_load:  Perform all remaining actions for loading a resource after
 * the data has been completely read in (and background decompression has
 * completed, if appropriate).
 *
 * [Parameters]
 *     resinfo: ResourceInfo structure.
 */
static void finish_load(ResourceInfo *resinfo __DEBUG_PARAMS);

/*-----------------------------------------------------------------------*/

/**
 * decompress_thread:  Thread routine for background decompression.  The
 * read buffers and decompression state buffer must be allocated before
 * starting this thread.
 *
 * [Parameters]
 *     resinfo: LoadInfo structure.
 * [Return value]
 *     0
 */
static int decompress_thread(void *loadinfo_);

/*************************************************************************/
/******************* Interface: Initialization/cleanup *******************/
/*************************************************************************/

void resource_init(void)
{
    packages = NULL;
    bgdecomp_on = 0;
    bgdecomp_workqueue = 0;
}

/*-----------------------------------------------------------------------*/

void resource_cleanup(void)
{
    workqueue_destroy(bgdecomp_workqueue);
    while (packages) {
        (*packages->cleanup)(packages);
        packages = packages->next;
    }
}

/*-----------------------------------------------------------------------*/

int resource_register_package(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return 0);
    PRECOND(module->prefix != NULL, return 0);
    PRECOND(module->init != NULL, return 0);
    PRECOND(module->cleanup != NULL, return 0);
    PRECOND(module->file_info != NULL, return 0);
    PRECOND(module->decompress != NULL, return 0);
    const int prefixlen = strlen(module->prefix);
    PRECOND(prefixlen <= 255, return 0);

    for (PackageModuleInfo *i = packages; i; i = i->next) {
        if (i == module) {
            return 0;
        }
    }

    if (!(*module->init)(module)) {
        return 0;
    }
    module->prefixlen = prefixlen;
    module->next = packages;
    packages = module;
    return 1;
}

/*-----------------------------------------------------------------------*/

void resource_unregister_package(PackageModuleInfo *module)
{
    if (module) {
        PackageModuleInfo **next_ptr;
        for (next_ptr = &packages; *next_ptr; next_ptr = &(*next_ptr)->next) {
            if (*next_ptr == module) {
                *next_ptr = module->next;
                (*module->cleanup)(module);
                return;
            }
        }
        DLOG("Package module %p not found in registered module list, not"
             " calling cleanup routine %p", module, module->cleanup);
    }
}

/*-----------------------------------------------------------------------*/

void resource_set_background_decompression(
    int on, int threshold, int buffer_size, int num_threads)
{
    if (UNLIKELY(on && buffer_size == 0)) {
        DLOG("Invalid parameters: %d %u %u, setting OFF",
             on, threshold, buffer_size);
        bgdecomp_on = 0;
        return;
    }
    bgdecomp_on          = on;
    bgdecomp_threshold   = lbound(threshold, buffer_size+1);
    bgdecomp_buffer_size = buffer_size;
    if (on && !bgdecomp_workqueue) {
        bgdecomp_workqueue = workqueue_create(num_threads);
        if (UNLIKELY(!bgdecomp_workqueue)) {
            DLOG("Failed to create background decompression work queue,"
                 " reverting to foreground decompression");
            bgdecomp_on = 0;
        }
    }
}

/*************************************************************************/
/******* Interface: ResourceManager instance creation/destruction ********/
/*************************************************************************/

ResourceManager *resource_create(int num_resources __DEBUG_PARAMS)
{
    if (UNLIKELY(num_resources < 0)) {
        DLOG("Invalid num_resources: %d", num_resources);
        return NULL;
    } else if (num_resources == 0) {
        num_resources = 100;  // Should be reasonable.
    }

    ResourceManager *resmgr = debug_mem_alloc(
        sizeof(*resmgr), 0, MEM_ALLOC_CLEAR, file, line, MEM_INFO_MANAGE);
    if (UNLIKELY(!resmgr)) {
        return NULL;
    }

    ResourceManagerPrivate *private = debug_mem_alloc(
        sizeof(*private), 0, MEM_ALLOC_CLEAR, file, line, MEM_INFO_MANAGE);
    if (UNLIKELY(!private)) {
        DLOG("Out of memory for resmgr->private");
        mem_free(resmgr);
        return NULL;
    }
    const int resources_size = sizeof(ResourceInfo) * num_resources;
    private->resources = debug_mem_alloc(
        resources_size, 0, MEM_ALLOC_CLEAR, file, line, MEM_INFO_MANAGE);
    if (UNLIKELY(!private->resources)) {
        DLOG("Out of memory for %d ResourceInfos", num_resources);
        mem_free(private);
        mem_free(resmgr);
        return NULL;
    }

    private->resources_size = num_resources;
    private->self_allocated = 1;
    private->mark = 1;
#ifdef DEBUG
    const char *s = strrchr(file, '/');
    if (s) {
        while (s > file && s[-1] != '/') {
            s--;
        }
        file = s;
    }
    strformat(private->owner, sizeof(private->owner), "%s:%d", file, line);
#endif
    resmgr->private = private;
    return resmgr;
}

/*-----------------------------------------------------------------------*/

void resource_destroy(ResourceManager *resmgr __DEBUG_PARAMS)
{
    if (!resmgr) {
        return;
    }

    if (UNLIKELY(!resmgr->private)) {
        /* Not an error in this function, so don't use the VALIDATE macro. */
        return;
    }
    resource_free_all(resmgr __DEBUG_ARGS);
    ResourceManagerPrivate *private = resmgr->private;
    const int self_allocated = private->self_allocated;
    if (!private->resources_is_static) {
        debug_mem_free(private->resources, file, line);
    }
    if (!private->private_is_static) {
        debug_mem_free(private, file, line);
    }
    if (self_allocated) {
        mem_free(resmgr);
    } else {
        resmgr->private = NULL;
    }
}

/*************************************************************************/
/***************** Interface: File/directory information *****************/
/*************************************************************************/

int resource_exists(const char *name)
{
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    return find_file(name, NULL, NULL, NULL, NULL, NULL, NULL);
}

/*-----------------------------------------------------------------------*/

ResourceFileListHandle *resource_list_files_start(
    const char *dir, int recursive)
{
    if (UNLIKELY(!dir)) {
        DLOG("dir == NULL");
        return NULL;
    }

    ResourceFileListHandle *handle =
        mem_alloc(sizeof(*handle), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!handle)) {
        DLOG("Out of memory");
        return NULL;
    }
    handle->recursive = recursive;
    handle->returned_file_size = 0;
    handle->returned_file = NULL;
    handle->return_prefix = NULL;
    handle->dir = NULL;
    handle->subdir_handle = NULL;

    handle->path = mem_strdup(dir, MEM_ALLOC_TEMP);
    if (UNLIKELY(!handle->path)) {
        DLOG("Out of memory for path: %s", dir);
        mem_free(handle);
        return NULL;
    }
    char *s = handle->path + strlen(handle->path);
    while (s > handle->path && s[-1] == '/') {  // Drop the trailing slash.
        *--s = 0;
    }

    handle->package = NULL;
    for (PackageModuleInfo *module = packages; module; module = module->next) {
        ASSERT(module->prefix != NULL, continue);
        /* This test is slightly complicated because we want to accept a
         * pathname "path/name" to refer to a package whose prefix is
         * "path/name/", but we don't want to treat "path/name2" as also
         * belonging to that package. */
        if (strnicmp(handle->path, module->prefix, module->prefixlen) == 0
            || (/* Note that module->prefixlen must be nonzero here. */
                module->prefix[module->prefixlen - 1] == '/'
                && strlen(handle->path) == module->prefixlen - 1
                && strnicmp(handle->path, module->prefix,
                            module->prefixlen - 1) == 0))
        {
            /* Check whether the given directory actually exists in the
             * package. */
            (*module->list_files_start)(module);
            int found = 0;
            const char *package_path = handle->path
                + ubound(module->prefixlen, strlen(handle->path));
            const int pathlen = strlen(package_path);
            const char *file;
            while ((file = (*module->list_files_next)(module))
                   != NULL) {
                if (pathlen == 0 || (strnicmp(file, package_path, pathlen) == 0
                                     && file[pathlen] == '/')) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                handle->package = module;
                break;
            }

            goto error_not_found;
        }
    }

    if (handle->package) {
        (*handle->package->list_files_start)(handle->package);
        handle->dir = NULL;
    } else {
        char pathbuf[4096];  // See include/SIL/resource.h note re buffer size.
        const char *path =
            generate_path(handle->path, pathbuf, sizeof(pathbuf));
        if (!path) {
            goto error_not_found;
        }
        handle->dir = sys_dir_open(path);
        if (!handle->dir) {
            goto error_not_found;
        }
        handle->recursion_level = 0;
        handle->subdir_handle = NULL;
    }

    return handle;

  error_not_found:
    mem_free(handle->path);
    mem_free(handle);
    return NULL;
}

/*-----------------------------------------------------------------------*/

const char *resource_list_files_next(ResourceFileListHandle *handle)
{
    if (UNLIKELY(!handle)) {
        DLOG("handle == NULL");
        return NULL;
    }

    const char *rel_path;

    if (handle->package) {

        const char *path = handle->path
            + ubound(handle->package->prefixlen, strlen(handle->path));
        const int pathlen = strlen(path);
        const char *file;
        while ((file = (*handle->package->list_files_next)(handle->package))
               != NULL) {
            if (pathlen == 0 || (strnicmp(file, path, pathlen) == 0
                                 && file[pathlen] == '/')) {
                if (pathlen > 0) {
                    file += pathlen + 1;  // Skip over the directory name.
                }
                if (!handle->recursive && strchr(file, '/')) {
                    continue;  // Skip files in subdirs if not requested.
                }
                break;  // Valid file found.
            }
        }
        if (!file) {
            handle->package = NULL;
        }
        rel_path = file;

    } else if (handle->dir) {

        for (;;) {
            if (handle->subdir_handle) {
                const char *file =
                    resource_list_files_next(handle->subdir_handle);
                if (file) {
                    return file;
                }
                resource_list_files_end(handle->subdir_handle);
                handle->subdir_handle = NULL;
            }

            const char *file;
            int is_subdir;
            for (;;) {
                file = sys_dir_read(handle->dir, &is_subdir);
                ASSERT(!file || *file, file = NULL);
                if (!file) {
                    sys_dir_close(handle->dir);
                    handle->dir = NULL;
                    return NULL;
                }
                if (is_subdir && !handle->recursive) {
                    continue;
                }
                break;
            }

            if (is_subdir) {
                if (handle->recursion_level >= 15) {
                    DLOG("Skipping subdirectory due to recursion limit: %s/%s",
                         handle->path, file);
                } else if (UNLIKELY(!list_files_format_path(handle, file))) {
                    /* Note that we don't use this path directly, but
                     * list_files_format_path() conveniently gives us the
                     * string we need for the subdirectory's return_prefix,
                     * so we generate it here and then pass the buffer off
                     * to the sub-handle if it is successfully created. */
                    DLOG("Skipping subdirectory due to out-of-memory: %s/%s",
                         handle->path, file);
                } else {
                    const int full_path_size =
                        strlen(handle->path) + strlen(file) + 2;
                    char *full_path =
                        mem_alloc(full_path_size, 1, MEM_ALLOC_TEMP);
                    if (UNLIKELY(!full_path)) {
                        DLOG("Skipping subdirectory due to out-of-memory:"
                             " %s/%s", handle->path, file);
                    } else {
                        ASSERT(strformat_check(full_path, full_path_size,
                                               "%s/%s", handle->path, file));
                        ResourceFileListHandle *subdir_handle =
                            resource_list_files_start(full_path, 1);
                        if (UNLIKELY(!subdir_handle)) {
                            DLOG("Skipping subdirectory due to open error:"
                                 " %s/%s", handle->path, file);
                        } else {
                            handle->subdir_handle = subdir_handle;
                            subdir_handle->recursion_level =
                                handle->recursion_level + 1;
                            /* See note above regarding buffer passing. */
                            subdir_handle->return_prefix =
                                handle->returned_file;
                            handle->returned_file = NULL;
                            handle->returned_file_size = 0;
                        }
                        mem_free(full_path);
                    }
                }
            } else {  // !is_subdir
                rel_path = file;
                break;
            }
        }

    } else {

        /* Already hit the end of the list. */
        rel_path = NULL;

    }

    if (rel_path && handle->return_prefix) {
        if (UNLIKELY(!list_files_format_path(handle, rel_path))) {
            DLOG("Out of memory formatting path: %s/%s",
                 handle->return_prefix, rel_path);
            return NULL;
        }
        return handle->returned_file;
    } else {
        return rel_path;
    }
}

/*-----------------------------------------------------------------------*/

void resource_list_files_end(ResourceFileListHandle *handle)
{
    if (handle) {
        resource_list_files_end(handle->subdir_handle);
        sys_dir_close(handle->dir);
        mem_free(handle->return_prefix);
        mem_free(handle->returned_file);
        mem_free(handle->path);
        mem_free(handle);
    }
}

/*************************************************************************/
/********************** Interface: Resource loading **********************/
/*************************************************************************/

int resource_load_data(ResourceManager *resmgr, const char *name,
                       int align, int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceInfo *resinfo = load_resource(
        private, RES_DATA, name, align, flags __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_load_texture(ResourceManager *resmgr, const char *name,
                          int flags, int mipmaps __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceInfo *resinfo = load_resource(
        private, RES_TEXTURE, name, SIL_TEXTURE_ALIGNMENT, flags __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }
    resinfo->loadinfo->texture_mipmaps = (mipmaps != 0);

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_load_bitmap_font(ResourceManager *resmgr, const char *name,
                              int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceInfo *resinfo = load_resource(
        private, RES_FONT, name, SIL_TEXTURE_ALIGNMENT, flags __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }
    ASSERT(resinfo->loadinfo != NULL, return 0);
    resinfo->loadinfo->font_parser = font_parse_bitmap;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_load_freetype_font(ResourceManager *resmgr, const char *name,
                                int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceInfo *resinfo = load_resource(
        private, RES_FONT, name, 0, flags __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }
    ASSERT(resinfo->loadinfo != NULL, return 0);
    resinfo->loadinfo->font_parser = font_parse_freetype;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_load_sound(ResourceManager *resmgr, const char *name,
                        int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceInfo *resinfo = load_resource(
        private, RES_SOUND, name, 0, flags __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    return resource_to_id(private, resinfo);
}

/*************************************************************************/
/******************** Interface: Load synchronization ********************/
/*************************************************************************/

int resource_mark(ResourceManager *resmgr)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    private->mark++;
    if (private->mark == 0) {
        private->mark++;
    }
    return private->mark;
}

/*-----------------------------------------------------------------------*/

int resource_sync(ResourceManager *resmgr, int mark __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 1);
    if (mark == 0) {
        DLOG("Invalid mark: %d", mark);
        return 1;
    }

    /* See whether there are any resources still loading.  We iterate
     * over the entire array regardless of mark value to give pending
     * asynchronous reads a chance to complete. */
    int still_waiting = 0;
    for (int index = 0; index < private->resources_size; index++) {
        if (private->resources[index].type != RES_UNUSED) {
            LoadInfo *loadinfo = private->resources[index].loadinfo;
            if (loadinfo) {
                if (TEST_block_load) {
                    if (compare_marks(private->resources[index].mark,
                                      mark) < 0) {
                        return 0;
                    } else {
                        continue;
                    }
                }
                if (!loadinfo_sync(loadinfo, 0, 0)
                 && compare_marks(private->resources[index].mark, mark) < 0) {
                    still_waiting = 1;
                }
            }
        }
    }
    if (still_waiting) {
        return 0;
    }

    if (TEST_silent_sync) {
        return 1;
    }

    /* All the requested resources are done loading, so finish the load
     * operations.  We explicitly do _not_ process any resources loaded
     * later than the requested mark value, so as not to spend too much
     * time finishing loads in a single call. */
    const int reverse_order = (TEST_sync_order >= 0) ? TEST_sync_order :
#ifdef SIL_RESOURCE_SYNC_IN_REVERSE
        1
#else
        0
#endif
        ;
    for (int index = reverse_order ? private->resources_size-1 : 0;
         reverse_order ? (index >= 0) : (index < private->resources_size);
         reverse_order ? index-- : index++)
    {
        if (private->resources[index].type != RES_UNUSED
         && private->resources[index].loadinfo
         && private->resources[index].loadinfo->need_finish
         && compare_marks(private->resources[index].mark, mark) < 0) {
            finish_load(&private->resources[index] __DEBUG_ARGS);
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

void resource_wait(ResourceManager *resmgr, int mark __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return);
    if (mark == 0) {
        DLOG("Invalid mark: %d", mark);
        return;
    }

    const int reverse_order = (TEST_sync_order >= 0) ? TEST_sync_order :
#ifdef SIL_RESOURCE_SYNC_IN_REVERSE
        1
#else
        0
#endif
        ;

    if (reverse_order) {
        for (int index = private->resources_size-1; index >= 0; index--) {
            if (private->resources[index].type != RES_UNUSED
             && compare_marks(private->resources[index].mark, mark) < 0) {
                if (private->resources[index].loadinfo) {
                    wait_resource(private, index);
                    finish_load(&private->resources[index] __DEBUG_ARGS);
                }
            }
        }
    } else {
        /* When syncing forward, each resource we finish loading may free
         * up an async read request, so we periodically run down the
         * resource list and kick any resources which haven't started
         * reading yet. */
        int kick_counter = 0;
        for (int index = 0; index < private->resources_size; index++) {
            if (private->resources[index].type != RES_UNUSED
             && compare_marks(private->resources[index].mark, mark) < 0) {
                LoadInfo *loadinfo = private->resources[index].loadinfo;
                if (loadinfo) {
                    wait_resource(private, index);
                    finish_load(&private->resources[index] __DEBUG_ARGS);
                    if (!TEST_block_load) {
                        kick_counter++;
                    }
                    if (kick_counter >= MAX_ASYNC_READS / 2) {
                        for (int j = 0; j < private->resources_size; j++) {
                            LoadInfo *loadinfo2 =
                                private->resources[j].loadinfo;
                            if (loadinfo2) {
                                loadinfo_sync(loadinfo2, 0, 0);
                            }
                        }
                        kick_counter = 0;
                    }
                }
            }
        }
    }
}

/*************************************************************************/
/********************* Interface: Resource creation **********************/
/*************************************************************************/

int resource_new_data(ResourceManager *resmgr, int size, int align, int flags
                      __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = add_resource(private, RES_DATA __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->data = debug_mem_alloc(lbound(size,1), align,
                                    convert_mem_flags(flags),
                                    file, line, MEM_INFO_UNKNOWN);
    if (UNLIKELY(!resinfo->data)) {
        del_resource(resinfo);
        return 0;
    }
    resinfo->size = size;
#ifdef DEBUG
    if (!(flags & RES_ALLOC_CLEAR)) {
        memset(resinfo->data, 0xBB, resinfo->size);
    }
#endif

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_copy_data(ResourceManager *resmgr, const void *data,
                       int size, int align, int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!data)) {
        DLOG("data == NULL");
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_DATA __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->data = debug_mem_alloc(lbound(size,1), align,
                                    convert_mem_flags(flags),
                                    file, line, MEM_INFO_UNKNOWN);
    if (UNLIKELY(!resinfo->data)) {
        del_resource(resinfo);
        return 0;
    }
    resinfo->size = size;
    memcpy(resinfo->data, data, size);

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_strdup(ResourceManager *resmgr, const char *str, int flags
                    __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!str)) {
        DLOG("str == NULL");
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_DATA __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->data = debug_mem_strdup(str, convert_mem_flags(flags),
                                     file, line, MEM_INFO_UNKNOWN);
    if (UNLIKELY(!resinfo->data)) {
        del_resource(resinfo);
        return 0;
    }
    resinfo->size = strlen(resinfo->data) + 1;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_new_texture(ResourceManager *resmgr, int width, int height,
                         int flags, int mipmaps __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = add_resource(private, RES_TEXTURE __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    const int id =
        texture_create(width, height, convert_mem_flags(flags), mipmaps);
    if (UNLIKELY(!id)) {
        del_resource(resinfo);
        return 0;
    }
    resinfo->texture = id;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_new_texture_from_display(
    ResourceManager *resmgr, int x, int y, int w, int h,
    int readable, int flags, int mipmaps __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = add_resource(private, RES_TEXTURE __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    const int id = texture_create_from_display(
        x, y, w, h, readable, convert_mem_flags(flags), mipmaps);
    if (UNLIKELY(!id)) {
        del_resource(resinfo);
        return 0;
    }
    resinfo->texture = id;

    return resource_to_id(private, resinfo);
}

/*************************************************************************/
/****************** Interface: Resource data retrieval *******************/
/*************************************************************************/

void *resource_get_data(const ResourceManager *resmgr, int id, int *size_ret)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return NULL);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return NULL;
    } else if (resinfo->type != RES_DATA) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a data"
             " resource", id, resmgr, private->owner);
        return NULL;
    }

    if (size_ret) {
        *size_ret = resinfo->size;
    }
    return resinfo->data;
}

/*-----------------------------------------------------------------------*/

void *resource_get_new_data(ResourceManager *resmgr, int size, int align,
                            int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return NULL);

    const int id = resource_new_data(resmgr, size, align, flags __DEBUG_ARGS);
    if (!id) {
        return NULL;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    ASSERT(resinfo != NULL, resource_free(resmgr, id __DEBUG_ARGS); return 0);
    return resinfo->data;
}

/*-----------------------------------------------------------------------*/

void *resource_get_copy_data(ResourceManager *resmgr, const void *data,
                             int size, int align, int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return NULL);

    const int id = resource_copy_data(resmgr, data, size, align, flags
                                      __DEBUG_ARGS);
    if (!id) {
        return NULL;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    ASSERT(resinfo != NULL, resource_free(resmgr, id __DEBUG_ARGS); return 0);
    return resinfo->data;
}

/*-----------------------------------------------------------------------*/

char *resource_get_strdup(ResourceManager *resmgr, const char *str,
                          int flags __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return NULL);

    const int id = resource_strdup(resmgr, str, flags __DEBUG_ARGS);
    if (!id) {
        return NULL;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    ASSERT(resinfo != NULL, resource_free(resmgr, id __DEBUG_ARGS); return 0);
    return (char *)resinfo->data;
}

/*-----------------------------------------------------------------------*/

int resource_get_texture(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return 0;
    } else if (resinfo->type != RES_TEXTURE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a texture"
             " resource", id, resmgr, private->owner);
        return 0;
    }

    return resinfo->texture;
}

/*-----------------------------------------------------------------------*/

int resource_get_new_texture(ResourceManager *resmgr, int width, int height,
                             int flags, int mipmaps __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    const int id = resource_new_texture(resmgr, width, height, flags,
                                        mipmaps __DEBUG_ARGS);
    if (!id) {
        return 0;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    ASSERT(resinfo != NULL, resource_free(resmgr, id __DEBUG_ARGS); return 0);
    return resinfo->texture;
}

/*-----------------------------------------------------------------------*/

int resource_get_new_texture_from_display(
    ResourceManager *resmgr, int x, int y, int w, int h,
    int readable, int flags, int mipmaps __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    const int id = resource_new_texture_from_display(
        resmgr, x, y, w, h, readable, flags, mipmaps __DEBUG_ARGS);
    if (!id) {
        return 0;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    ASSERT(resinfo != NULL, resource_free(resmgr, id __DEBUG_ARGS); return 0);
    return resinfo->texture;
}

/*-----------------------------------------------------------------------*/

int resource_get_font(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return 0;
    } else if (resinfo->type != RES_FONT) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a font"
             " resource", id, resmgr, private->owner);
        return 0;
    }

    return resinfo->font;
}

/*-----------------------------------------------------------------------*/

Sound *resource_get_sound(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return NULL);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return NULL;
    } else if (resinfo->type != RES_SOUND) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a sound"
             " resource", id, resmgr, private->owner);
        return NULL;
    }

    return resinfo->sound;
}

/*************************************************************************/
/******************** Interface: Raw data file access ********************/
/*************************************************************************/

int resource_open_file(ResourceManager *resmgr, const char *name
                       __DEBUG_PARAMS)
{
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);

    SysFile *fp;
    int64_t offset;
    int size;
    fp = resource_internal_open_file(name, &offset, &size);
    if (!fp) {
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_FILE __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        sys_file_close(fp);
        return 0;
    }
    resinfo->fp = fp;
    resinfo->size = size;
    resinfo->offset = offset;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int64_t resource_get_file_size(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return 0;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return 0;
    }

    return resinfo->size;
}

/*-----------------------------------------------------------------------*/

void resource_set_file_position(const ResourceManager *resmgr, int id,
                                int64_t pos)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return;
    }

    pos = bound(pos, 0, resinfo->size);
    ASSERT(sys_file_seek(resinfo->fp, resinfo->offset + pos, FILE_SEEK_SET));
}

/*-----------------------------------------------------------------------*/

int64_t resource_get_file_position(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return 0;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return 0;
    }

    int64_t pos = sys_file_tell(resinfo->fp);
    ASSERT(pos >= resinfo->offset, pos = resinfo->offset);
    pos -= resinfo->offset;
    ASSERT(pos <= resinfo->size, pos = resinfo->size);
    return pos;
}

/*-----------------------------------------------------------------------*/

int resource_read_file(const ResourceManager *resmgr, int id, void *buf,
                       int len)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return -1);
    if (UNLIKELY(!buf) || UNLIKELY(len < 0)) {
        DLOG("Invalid parameters: %p %d %p %d", resmgr, id, buf, len);
        return -1;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return -1;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return -1;
    }

    const int64_t pos = sys_file_tell(resinfo->fp) - resinfo->offset;
    ASSERT(pos >= 0 && pos <= resinfo->size, return -1);
    len = ubound(len, resinfo->size - pos);
    return sys_file_read(resinfo->fp, buf, len);
}

/*-----------------------------------------------------------------------*/

int resource_read_file_at(const ResourceManager *resmgr, int id, void *buf,
                          int len, int64_t pos)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return -1);
    if (UNLIKELY(!buf) || UNLIKELY(len < 0) || UNLIKELY(pos < 0)) {
        DLOG("Invalid parameters: %p %d %p %d %lld", resmgr, id, buf, len,
             (long long)pos);
        return -1;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return -1;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return -1;
    }

    if (pos >= resinfo->size) {
        return 0;
    }
    return sys_file_read_at(resinfo->fp, buf, len, resinfo->offset + pos);
}

/*-----------------------------------------------------------------------*/

struct SysFile *resource_get_file_handle(const ResourceManager *resmgr,
                                         int id, int64_t *offset_ret)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!offset_ret)) {
        DLOG("offset_ret == NULL");
        return 0;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (!resinfo) {
        return 0;
    } else if (resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", id, resmgr, private->owner);
        return 0;
    }

    *offset_ret = resinfo->offset;
    return resinfo->fp;
}

/*************************************************************************/
/***************** Interface: Other resource operations ******************/
/*************************************************************************/

int resource_open_sound(ResourceManager *resmgr, const char *name
                        __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }

    int64_t offset;
    int size;
    SysFile *fh = resource_internal_open_file(name, &offset, &size);
    if (!fh) {
        return 0;
    }

    Sound *sound = sound_create_stream(
        fh, offset, size, SOUND_FORMAT_AUTODETECT __DEBUG_ARGS);
    if (UNLIKELY(!sound)) {
        DLOG("Failed to create Sound object for %s", name);
        sys_file_close(fh);
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_SOUND __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        sound_destroy(sound __DEBUG_ARGS);
        return 0;
    }
    resinfo->sound = sound;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_open_sound_from_file(
    ResourceManager *resmgr, ResourceManager *file_resmgr, int file_id,
    int64_t offset, int size __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    const ResourceManagerPrivate *file_private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    VALIDATE_CONST_RESMGR(file_resmgr, file_private, return 0);
    if (UNLIKELY(offset < 0) || UNLIKELY(size <= 0)) {
        DLOG("Invalid parameters: %p %p %d %lld %d",
             resmgr, file_resmgr, file_id, (long long)offset, size);
        return 0;
    }

    ResourceInfo *file_resinfo = id_to_resource(file_private, file_id);
    if (!file_resinfo) {
        return 0;
    } else if (file_resinfo->type != RES_FILE) {
        DLOG("Resource ID %d in resource manager %p (%s) is not a file"
             " resource", file_id, file_resmgr, file_private->owner);
        return 0;
    }
    if (UNLIKELY(offset + size > file_resinfo->size)) {
        DLOG("Byte range %lld+%d exceeds file size %lld",
             (long long)offset, size, (long long)file_resinfo->size);
        return 0;
    }
    offset += file_resinfo->offset;

    SysFile *fh = sys_file_dup(file_resinfo->fp);
    if (UNLIKELY(!fh)) {
        DLOG("Failed to dup file handle");
        return 0;
    }

    Sound *sound = sound_create_stream(
        fh, offset, size, SOUND_FORMAT_AUTODETECT __DEBUG_ARGS);
    if (UNLIKELY(!sound)) {
        DLOG("Failed to create Sound object");
        sys_file_close(fh);
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_SOUND __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        sound_destroy(sound __DEBUG_ARGS);
        return 0;
    }
    resinfo->sound = sound;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_take_data(ResourceManager *resmgr, void *data, int size
                       __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!data)) {
        DLOG("data == NULL");
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_DATA __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->data = data;
    resinfo->size = size;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_take_texture(ResourceManager *resmgr, int texture_id
                          __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!texture_id)) {
        DLOG("texture_id == 0");
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_TEXTURE __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->texture = texture_id;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_take_sound(ResourceManager *resmgr, Sound *sound __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    if (UNLIKELY(!sound)) {
        DLOG("sound == NULL");
        return 0;
    }

    ResourceInfo *resinfo = add_resource(private, RES_SOUND __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    resinfo->sound = sound;

    return resource_to_id(private, resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_link(ResourceManager *resmgr,
                  const ResourceManager *old_resmgr, int old_id __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    const ResourceManagerPrivate *old_private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    VALIDATE_CONST_RESMGR(old_resmgr, old_private, return 0);

    /* Set up the new ResourceInfo structure (we'll fill in the type later). */
    ResourceInfo *new_resinfo = add_resource(private, RES_UNKNOWN
                                             __DEBUG_ARGS);
    if (UNLIKELY(!new_resinfo)) {
        return 0;
    }
    if (resmgr == old_resmgr
     && resource_to_id(private, new_resinfo) == old_id) {
        /* If the new ResourceInfo has the same ID as old_id, it must not
         * have been allocated before this function was called, and is
         * therefore invalid. */
        DLOG("Resource ID %d invalid", old_id);
        del_resource(new_resinfo);
        return 0;
    }
    /* Don't try to get this pointer until after the new ResourceInfo has
     * been added.  If resmgr == old_resmgr and the add operation moved the
     * ResourceInfo array in memory, the pointer could change under us. */
    ResourceInfo *old_resinfo = id_to_resource(old_private, old_id);
    if (UNLIKELY(!old_resinfo)) {
        del_resource(new_resinfo);
        return 0;
    }
    if (old_resinfo->is_stale_link) {
        DLOG("Resource ID %d is a stale link", old_id);
        del_resource(new_resinfo);
        return 0;
    }
    new_resinfo->type     = old_resinfo->type;
    new_resinfo->data     = old_resinfo->data;
    new_resinfo->size     = old_resinfo->size;
    new_resinfo->loadinfo = old_resinfo->loadinfo;
    /* Give the link its own sync mark, since the owners of the original
     * resource and the link may have different sync priorities, and if
     * the original resource is in a different ResourceManager, its mark
     * value is meaningless in the target ResourceManager anyway. */
    new_resinfo->mark     = private->mark;

    /* Add the new ResourceInfo to the resource's link list. */
    ResourceInfo *prev = old_resinfo;
    int tries = 10000;
    while (prev->link_next != old_resinfo) {
        prev = prev->link_next;
        tries--;
        if (UNLIKELY(tries <= 0)) {
            DLOG("BUG: endless linked list on resource %p in resmgr"
                 " %p (%s)", old_resinfo, old_private, old_private->owner);
            del_resource(new_resinfo);
            return 0;
        }
    }
    prev->link_next = new_resinfo;
    new_resinfo->link_next = old_resinfo;

    return resource_to_id(private, new_resinfo);
}

/*-----------------------------------------------------------------------*/

int resource_link_weak(ResourceManager *resmgr,
                       const ResourceManager *old_resmgr, int old_id
                       __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    const ResourceManagerPrivate *old_private;
    VALIDATE_RESMGR(resmgr, private, return 0);
    VALIDATE_CONST_RESMGR(old_resmgr, old_private, return 0);
    (void) old_private;  // Avoid an unused-variable warning.

    const int new_id = resource_link(resmgr, old_resmgr, old_id __DEBUG_ARGS);
    if (!new_id) {
        return 0;
    }

    ResourceInfo *new_resinfo = id_to_resource(private, new_id);
    ASSERT(new_resinfo != NULL,
           resource_free(resmgr, new_id __DEBUG_ARGS); return 0);
    new_resinfo->is_weak_link = 1;

    return new_id;
}

/*-----------------------------------------------------------------------*/

int resource_is_stale(const ResourceManager *resmgr, int id)
{
    const ResourceManagerPrivate *private;
    VALIDATE_CONST_RESMGR(resmgr, private, return 0);

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (UNLIKELY(!resinfo)) {
        return 0;
    }

    return resinfo->is_stale_link;
}

/*-----------------------------------------------------------------------*/

void resource_free(ResourceManager *resmgr, int id __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return);
    if (UNLIKELY(!id)) {
        return;
    }

    ResourceInfo *resinfo = id_to_resource(private, id);
    if (UNLIKELY(!resinfo)) {
        return;
    }
    free_resource(resinfo __DEBUG_ARGS);
    del_resource(resinfo);
}

/*-----------------------------------------------------------------------*/

void resource_free_all(ResourceManager *resmgr __DEBUG_PARAMS)
{
    ResourceManagerPrivate *private;
    VALIDATE_RESMGR(resmgr, private, return);

    /* Abort all pending loads before doing anything else.  If we free each
     * resource as we abort it, we potentially give the next resource's
     * load operation time to start up, making us wait even longer to
     * finish aborting all the loads. */
    for (int index = 0; index < private->resources_size; index++) {
        if (private->resources[index].type != RES_UNUSED
         && private->resources[index].loadinfo) {
            loadinfo_sync(private->resources[index].loadinfo, 0, 1);
        }
    }

    for (int index = 0; index < private->resources_size; index++) {
        if (private->resources[index].type != RES_UNUSED) {
            free_resource(&private->resources[index] __DEBUG_ARGS);
            del_resource(&private->resources[index]);
        }
    }
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

SysFile *resource_internal_open_file(
    const char *name, int64_t *offset_ret, int *size_ret)
{
    PRECOND(name != NULL, return NULL);
    PRECOND(offset_ret != NULL, return NULL);
    PRECOND(size_ret != NULL, return NULL);

    PackageModuleInfo *pkginfo;
    SysFile *fh;
    int compressed;
    if (!find_file(name, &pkginfo, &fh, offset_ret, NULL, &compressed,
                   size_ret)) {
        return NULL;
    }
    if (pkginfo) {
        SysFile *dup = sys_file_dup(fh);
        if (!dup) {
            DLOG("Failed to dup file handle");
            return 0;
        }
        fh = dup;
    }
    if (compressed) {
        sys_file_close(fh);
        return NULL;
    }
    ASSERT(sys_file_seek(fh, *offset_ret, FILE_SEEK_SET));
    return fh;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

void TEST_resource_set_path_prefix(const char *prefix)
{
    TEST_override_path_prefix = prefix;
}

/*-----------------------------------------------------------------------*/

void TEST_resource_block_load(int enable)
{
    TEST_block_load = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_resource_use_silent_sync(int enable)
{
    TEST_silent_sync = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_resource_override_sync_order(int enable, int reverse)
{
    TEST_sync_order = enable ? (reverse != 0) : -1;
}

/*-----------------------------------------------------------------------*/

void TEST_resource_set_mark(ResourceManager *resmgr, int mark)
{
    PRECOND(resmgr != NULL, return);
    ResourceManagerPrivate *private = get_private(resmgr);
    PRECOND(private != NULL, return);

    resmgr->private->mark = mark;
}

/*-----------------------------------------------------------------------*/

void TEST_resource_set_link_pointer(ResourceManager *resmgr, int id1, int id2)
{
    PRECOND(resmgr != NULL, return);
    ResourceManagerPrivate *private = get_private(resmgr);
    PRECOND(private != NULL, return);

    ResourceInfo *resinfo1, *resinfo2;
    ASSERT(resinfo1 = id_to_resource(resmgr->private, id1));
    ASSERT(resinfo2 = id_to_resource(resmgr->private, id2));
    resinfo1->link_next = resinfo2;
}

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/***************** Local routines: Convenience functions *****************/
/*************************************************************************/

static CONST_FUNCTION int compare_marks(int a, int b)
{
    return a - b;
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int convert_mem_flags(int res_flags)
{
    /* Ensure that none of the MEM_ALLOC flags are being used. */
    ASSERT((res_flags & (MEM_ALLOC_TOP|MEM_ALLOC_TEMP|MEM_ALLOC_CLEAR)) == 0);

    return (res_flags & RES_ALLOC_TOP   ? MEM_ALLOC_TOP   : 0)
         | (res_flags & RES_ALLOC_TEMP  ? MEM_ALLOC_TEMP  : 0)
         | (res_flags & RES_ALLOC_CLEAR ? MEM_ALLOC_CLEAR : 0);
}

/*-----------------------------------------------------------------------*/

static int list_files_format_path(ResourceFileListHandle *handle,
                                  const char *name)
{
    PRECOND(handle != NULL, return 0);
    PRECOND(name != NULL, return 0);

    const char *prefix = handle->return_prefix;
    const int full_path_size =
        (prefix ? strlen(prefix)+1 : 0) + strlen(name) + 1;
    if (full_path_size > handle->returned_file_size) {
        char *buf = mem_realloc(handle->returned_file, full_path_size,
                                MEM_ALLOC_TEMP);
        if (UNLIKELY(!buf)) {
            return 0;
        }
        handle->returned_file = buf;
        handle->returned_file_size = full_path_size;
    }
    ASSERT(strformat_check(handle->returned_file, handle->returned_file_size,
                           "%s%s%s", prefix ? prefix : "", prefix ? "/" : "",
                           name));
    return 1;
}

/*-----------------------------------------------------------------------*/

static const char *generate_path(const char *name, char *buffer, int bufsize)
{
    PRECOND(name != NULL, return NULL);
    PRECOND(buffer != NULL, return NULL);
    PRECOND(bufsize > 0, return NULL);

    if (strncmp(name, "host:", 5) == 0) {
        return name+5;
    } else if (*name == '/') {
        return name;
    } else {
        int len;
// FIXME: remove this once GCC 8 is old
#if defined(__GNUC__) && __GNUC__ == 8  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87041
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat"
#endif
        if (TEST_override_path_prefix) {
            len = strformat(buffer, bufsize, "%s", TEST_override_path_prefix);
        } else {
            len = sys_get_resource_path_prefix(buffer, bufsize);
        }
#if defined(__GNUC__) && __GNUC__ == 8
# pragma GCC diagnostic pop
#endif
        if (len >= bufsize) {
            DLOG("Buffer overflow on resource path prefix");
            return NULL;
        }
        if (!strformat_check(buffer + len, bufsize - len, "%s", name)) {
            DLOG("Buffer overflow appending name %s to resource path prefix"
                 " %.*s", name, len, buffer);
            return NULL;
        }
        return buffer;
    }
}

/*-----------------------------------------------------------------------*/

static int find_file(const char *name, PackageModuleInfo **pkginfo_ret,
                     SysFile **fh_ret, int64_t *offset_ret, int *length_ret,
                     int *compressed_ret, int *size_ret)
{
    PRECOND(name != NULL, return 0);
    PRECOND(!(fh_ret && !pkginfo_ret), return 0);

    for (PackageModuleInfo *module = packages; module; module = module->next) {
        ASSERT(module->prefix != NULL, continue);
        if (strnicmp(name, module->prefix, module->prefixlen) == 0) {
            const char *package_name = name + strlen(module->prefix);
            SysFile *fh;
            int64_t pos;
            int len, size;
            int compressed;
            if ((*module->file_info)(module, package_name, &fh, &pos, &len,
                                     &compressed, &size)) {
                if (fh_ret) {
                    *fh_ret = fh;
                }
                if (pkginfo_ret) {
                    *pkginfo_ret = module;
                }
                if (offset_ret) {
                    *offset_ret = pos;
                }
                if (length_ret) {
                    *length_ret = len;
                }
                if (compressed_ret) {
                    *compressed_ret = compressed;
                }
                if (size_ret) {
                    *size_ret = size;
                }
                return 1;
            }
            return 0;
        }
    }

    char pathbuf[4096];  // See include/SIL/resource.h note about buffer size.
    const char *path = generate_path(name, pathbuf, sizeof(pathbuf));
    if (!path) {
        return 0;
    }

    SysFile *fh = sys_file_open(path);
    if (!fh) {
        return 0;
    }

    if (pkginfo_ret) {
        *pkginfo_ret = NULL;
    }
    if (fh_ret) {
        *fh_ret = fh;
    } else {
        sys_file_close(fh);
    }
    if (offset_ret) {
        *offset_ret = 0;
    }
    if (length_ret) {
        *length_ret = sys_file_size(fh);
    }
    if (compressed_ret) {
        *compressed_ret = 0;
    }
    if (size_ret) {
        *size_ret = sys_file_size(fh);
    }
    return 1;
}

/*************************************************************************/
/*********** Local routines: Private data structure management ***********/
/*************************************************************************/

static ResourceManagerPrivate *get_private(ResourceManager *resmgr)
{
    PRECOND(resmgr != NULL, return NULL);

    if (LIKELY(resmgr->private)) {
        return resmgr->private;
    }

    if (UNLIKELY(!resmgr->static_buffer)) {
        DLOG("%p: missing static buffer", resmgr);
        return NULL;
    }
    if (UNLIKELY((uintptr_t)resmgr->static_buffer % sizeof(uintptr_t) != 0)) {
        DLOG("%p: static_buffer %p is not %d-byte aligned!",
             resmgr, resmgr->static_buffer, (int)sizeof(uintptr_t));
        return NULL;
    }
    if (UNLIKELY(resmgr->static_count < 0)) {
        DLOG("%p: static_count %d is invalid!", resmgr, resmgr->static_count);
        return NULL;
    }
    const int resinfo_size = sizeof(ResourceInfo) * resmgr->static_count;
    const int private_size = sizeof(ResourceManagerPrivate);
    if (UNLIKELY(resmgr->static_size != resinfo_size + private_size)) {
        DLOG("%p: static_size %d is wrong for static_count %d (should be %d)!",
             resmgr, resmgr->static_size, resmgr->static_count,
             private_size + resinfo_size);
        return NULL;
    }

    ResourceInfo *resources = (ResourceInfo *)resmgr->static_buffer;
    mem_clear(resources, sizeof(*resources) * resmgr->static_count);

    ResourceManagerPrivate *private =
        (ResourceManagerPrivate *)&resources[resmgr->static_count];
    mem_clear(private, sizeof(*private));
    private->resources = resources;
    private->resources_size = resmgr->static_count;
    private->private_is_static = 1;
    private->resources_is_static = 1;
    private->mark = 1;
#ifdef DEBUG
    const char *file = resmgr->static_file;
    const char *s = strrchr(file, '/');
    if (s) {
        while (s > file && s[-1] != '/') {
            s--;
        }
        file = s;
    }
    strformat(private->owner, sizeof(private->owner), "%s:%d",
              file, resmgr->static_line);
#endif

    resmgr->private = private;
    return private;
}

/*-----------------------------------------------------------------------*/

static const ResourceManagerPrivate *get_private_noinit(
    const ResourceManager *resmgr)
{
    static ResourceInfo dummy_resinfo = {
        .type = RES_UNUSED,
    };
    static ResourceManagerPrivate dummy_private = {
        .resources = &dummy_resinfo,
        .resources_size = 1,
        .self_allocated = 0,
        .private_is_static = 1,
        .resources_is_static = 1,
        .mark = 1,
#ifdef DEBUG
        .owner = "__internal__:0",
#endif
    };

    PRECOND(resmgr != NULL, return &dummy_private);

    if (LIKELY(resmgr->private)) {
        return resmgr->private;
    }

    return &dummy_private;
}

/*-----------------------------------------------------------------------*/

static int resource_to_id(const ResourceManagerPrivate *private,
                          ResourceInfo *resinfo)
{
    PRECOND(private != NULL, return 0);
    PRECOND(resinfo != NULL, return 0);

    int index = (int)(resinfo - private->resources);
    ASSERT(index >= 0, return 0);
    ASSERT(index < private->resources_size, return 0);
    ASSERT(resinfo == &private->resources[index], return 0);
    return index+1;
}

/*-----------------------------------------------------------------------*/

static ResourceInfo *id_to_resource(const ResourceManagerPrivate *private,
                                    int id)
{
    PRECOND(private != NULL, return NULL);

    if (UNLIKELY(id <= 0) || UNLIKELY(id > private->resources_size)) {
        DLOG("Resource ID %d invalid or out of range", id);
        return NULL;
    }

    ResourceInfo *resinfo = &private->resources[id-1];
    if (UNLIKELY(resinfo->type == RES_UNUSED)) {
        DLOG("Resource ID %d is unused", id);
        return NULL;
    }

    return resinfo;
}

/*-----------------------------------------------------------------------*/

static ResourceInfo *add_resource(ResourceManagerPrivate *private,
                                  ResourceType type __DEBUG_PARAMS)
{
    PRECOND(private != NULL, return NULL);
    PRECOND(private->resources != NULL, return NULL);

    int index;
    for (index = 0; index < private->resources_size; index++) {
        if (private->resources[index].type == RES_UNUSED) {
            break;
        }
    }

    if (UNLIKELY(index >= private->resources_size)) {

        /* No room left in the array, so expand it. */
        const int new_num = private->resources_size + 100;
        DLOG("%p (%s): Expanding resource array to %d entries (called from"
             " %s:%d)", private, private->owner, new_num, file, line);
        ResourceInfo *new_resources;
        if (private->resources_is_static) {
            new_resources = debug_mem_alloc(
                sizeof(ResourceInfo) * new_num, 0,
                MEM_ALLOC_TEMP | MEM_ALLOC_TOP,  // Avoid fragmentation.
                file, line, MEM_INFO_MANAGE);
        } else {
            new_resources = debug_mem_realloc(
                private->resources, sizeof(ResourceInfo) * new_num,
                MEM_ALLOC_TEMP | MEM_ALLOC_TOP, file, line, MEM_INFO_MANAGE);
        }
        if (UNLIKELY(!new_resources)) {
            DLOG("... failed to realloc resource list!");
            return NULL;
        }
        if (private->resources_is_static) {
            memcpy(new_resources, private->resources,
                   sizeof(*new_resources) * private->resources_size);
        }
        mem_clear(&new_resources[private->resources_size],
                  sizeof(*new_resources) * (new_num - private->resources_size));

        /* Update ResourceInfo pointers stored in link_next fields.  We
         * first fix up all resources managed by this ResourceManager,
         * then take care of links back from other ResourceManager
         * instances. */
        for (int i = 0; i < private->resources_size; i++) {
            ResourceInfo *ptr = &new_resources[i];
            if (ptr->link_next >= private->resources
             && ptr->link_next < private->resources + private->resources_size)
            {
                ptr->link_next = (ResourceInfo *)
                    ((uintptr_t)ptr->link_next
                     - (uintptr_t)private->resources
                     + (uintptr_t)new_resources);
            }
        }
        for (int i = 0; i < private->resources_size; i++) {
            ResourceInfo *ptr = &new_resources[i];
            int tries = 10000;
            while (ptr->link_next != &new_resources[i]) {
                if (ptr->link_next == &private->resources[i]) {
                    ptr->link_next = &new_resources[i];
                    break;
                }
                ptr = ptr->link_next;
                tries--;
                if (UNLIKELY(tries <= 0)) {
                    DLOG("BUG: endless linked list on resource %p in resmgr"
                         " %p (%s)", &private->resources[i], private,
                         private->owner);
                    /* Kill it so it doesn't end up as a dangling pointer. */
                    new_resources[i].type = RES_UNUSED;
                    new_resources[i].data = NULL;
                    new_resources[i].size = 0;
                    break;
                }
            }
        }

        /* Update the ResourceManagerPrivate structure with the new values. */
        private->resources = new_resources;
        private->resources_size = new_num;
        private->resources_is_static = 0;
    }

    mem_clear(&private->resources[index], sizeof(private->resources[index]));
    private->resources[index].owner         = private;
    private->resources[index].link_next     = &private->resources[index];
    private->resources[index].type          = type;
    private->resources[index].is_weak_link  = 0;
    private->resources[index].is_stale_link = 0;

    return &private->resources[index];
}

/*-----------------------------------------------------------------------*/

static void del_resource(ResourceInfo *resinfo)
{
    PRECOND(resinfo != NULL, return);
    resinfo->type = RES_UNUSED;
}

/*-----------------------------------------------------------------------*/

static ResourceInfo *load_resource(
    ResourceManagerPrivate *private, ResourceType type,
    const char *path, uint16_t align, int flags __DEBUG_PARAMS)
{
    PRECOND(private != NULL, return NULL);
    PRECOND(path != NULL, return NULL);

    ResourceInfo *resinfo = add_resource(private, type __DEBUG_ARGS);
    if (UNLIKELY(!resinfo)) {
        return NULL;
    }

    resinfo->loadinfo = mem_alloc(sizeof(*resinfo->loadinfo), sizeof(void *),
                                   MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!resinfo->loadinfo)) {
        DLOG("%p (%s): Out of memory for load info", resinfo, path);
        del_resource(resinfo);
        return NULL;
    }
    resinfo->loadinfo->mem_align = align;
    resinfo->loadinfo->mem_flags = convert_mem_flags(flags);
#ifdef DEBUG
    resinfo->loadinfo->mem_type =
        (resinfo->type==RES_TEXTURE ? MEM_INFO_TEXTURE :
         resinfo->type==RES_SOUND   ? MEM_INFO_SOUND   :
         resinfo->type==RES_FONT    ? MEM_INFO_FONT    : MEM_INFO_UNKNOWN);
    strformat(resinfo->loadinfo->debug_path,
              sizeof(resinfo->loadinfo->debug_path), "%s", path);
#endif

    resinfo->mark = resinfo->owner->mark;
    if (load_data(resinfo, path __DEBUG_ARGS)) {
        return resinfo;
    }

    DLOG("%s: Resource not found", path);
    mem_free(resinfo->loadinfo);
    resinfo->loadinfo = NULL;
    resinfo->mark = 0;
    del_resource(resinfo);
    return NULL;
}

/*-----------------------------------------------------------------------*/

static void wait_resource(ResourceManagerPrivate *private, int index)
{
    PRECOND(private != NULL, return);
    PRECOND(index >= 0 && index < private->resources_size, return);
    PRECOND(private->resources[index].loadinfo != NULL, return);

    LoadInfo *loadinfo = private->resources[index].loadinfo;

    if (loadinfo->decomp_wu) {
        while (!loadinfo_sync(loadinfo, 0, 0)) {
            int have_pending_io = 0;
            for (int i = 0; i < private->resources_size; i++) {
                if (i != index
                 && private->resources[i].type != RES_UNUSED
                 && private->resources[i].loadinfo
                 && !private->resources[i].loadinfo->decomp_wu
                 && private->resources[i].loadinfo->read_request) {
                    have_pending_io = 1;
                    loadinfo_sync(private->resources[i].loadinfo, 0, 0);
                }
            }
            if (have_pending_io) {
                thread_yield();
            } else {
                break;  // No reason to poll any longer.
            }
        }
    }

    loadinfo_sync(loadinfo, 1, 0);
}

/*-----------------------------------------------------------------------*/

static void free_resource(ResourceInfo *resinfo __DEBUG_PARAMS)
{
    PRECOND(resinfo != NULL, return);

    void * const data = resinfo->data;
    resinfo->data = NULL;
    resinfo->size = 0;

    ASSERT(resinfo->type != RES_UNUSED, return);
    ASSERT(resinfo->link_next != NULL, resinfo->link_next = resinfo);

    if (resinfo->link_next != resinfo) {
        ResourceInfo *prev = resinfo->link_next;
        int tries = 10000;
        while (prev->link_next != resinfo) {
            prev = prev->link_next;
            tries--;
            if (UNLIKELY(tries <= 0)) {
                DLOG("BUG: endless linked list on resource %p", resinfo);
                /* Just delink this one and get out. */
                resinfo->link_next = resinfo;
                resinfo->loadinfo = NULL;  // Since it was presumably shared.
                return;
            }
        }
        prev->link_next = resinfo->link_next;

        int has_strong_link = 0;
        ResourceInfo *i = prev;
        do {
            has_strong_link = !i->is_weak_link;
            i = i->link_next;
        } while (!has_strong_link && i != prev);
        if (has_strong_link) {
            resinfo->loadinfo = NULL;
            return;
        }
        /* Only weak links are left, so make them all stale. */
        i = prev;
        do {
            ResourceInfo *next = i->link_next;
            i->link_next = i;
            i->is_stale_link = 1;
            i->data = NULL;
            i->size = 0;
            i->loadinfo = NULL;
            i = next;
        } while (i != prev);
    }

    if (resinfo->loadinfo) {
        LoadInfo *loadinfo = resinfo->loadinfo;
        loadinfo_sync(loadinfo, 1, 1);
        /* We free the data immediately, so no need to call finish_load(). */
        if (loadinfo->need_close) {
            sys_file_close(loadinfo->fp);
        }
        mem_free(loadinfo->file_data);
        mem_free(loadinfo);
        resinfo->loadinfo = NULL;
    } else {
        ASSERT(resinfo->type != RES_UNUSED, return);
        ASSERT(resinfo->type != RES_UNKNOWN, return);
        switch (resinfo->type) {
          case RES_UNUSED:   // Impossible (included to avoid a warning).
          case RES_UNKNOWN:  // Impossible (included to avoid a warning).
          case RES_DATA:
            debug_mem_free(data, file, line);
            break;
          case RES_TEXTURE:
            texture_destroy((int)(intptr_t)data);
            break;
          case RES_FONT:
            font_destroy((int)(intptr_t)data);
            break;
          case RES_SOUND:
            sound_destroy((Sound *)data __DEBUG_ARGS);
            break;
          case RES_FILE:
            sys_file_close((SysFile *)data);
            break;
        }
    }
}

/*************************************************************************/
/********************* Local routines: Data loading **********************/
/*************************************************************************/

static int load_data(ResourceInfo *resinfo, const char *name __DEBUG_PARAMS)
{
    PRECOND(resinfo != NULL, goto error_return);
    PRECOND(resinfo->loadinfo != NULL, goto error_return);
    PRECOND(name != NULL, goto error_return);
    LoadInfo *loadinfo = resinfo->loadinfo;

    PackageModuleInfo *pkginfo;
    SysFile *fh;
    int64_t offset;
    int length, size;
    int compressed;
    if (!find_file(name, &pkginfo, &fh, &offset, &length, &compressed, &size)){
        goto error_return;
    }
    if (!compressed) {
        ASSERT(length == size, length = size);
    }

    const int use_bgdecomp =
        compressed && bgdecomp_on && length >= bgdecomp_threshold;

    int alloc_size = lbound(use_bgdecomp ? size : length, 1);
    void *data;
    if (compressed && !use_bgdecomp) {
        /* Flip the state of the MEM_ALLOC_TOP bit so the compressed-data
         * and decompressed-data buffers are allocated at opposite ends of
         * the memory pool, reducing memory fragmentation (on systems where
         * it makes a difference).  Also don't pass the debug parameters to
         * debug_mem_alloc() for this allocation, since it's a temporary
         * buffer which properly belongs to this source file. */
        data = mem_alloc(alloc_size, 0, loadinfo->mem_flags ^ MEM_ALLOC_TOP);
    } else {
        data = debug_mem_alloc(alloc_size, loadinfo->mem_align,
                               loadinfo->mem_flags,
                               file, line, resinfo->loadinfo->mem_type);
    }
    if (UNLIKELY(!data)) {
        DLOG("%s: Out of memory", name);
        goto error_close_fh;
    }

    loadinfo->compressed      = (compressed && !use_bgdecomp);
    loadinfo->compressed_size = length;
    loadinfo->data_size       = size;
    loadinfo->file_data       = data;
    loadinfo->fp              = fh;
    loadinfo->data_offset     = offset;
    loadinfo->pkginfo         = pkginfo;
    loadinfo->need_close      = (pkginfo == NULL);

    if (use_bgdecomp) {
        if (LIKELY(start_background_decompress(loadinfo))) {
            return 1;
        }
        /* Failed to start the thread, so revert to regular decompression. */
        if (UNLIKELY(!start_fallback_decompress(loadinfo, file, line))) {
            goto error_free_file_data;
        }
    }

    if (UNLIKELY(!start_async_read(loadinfo))) {
        goto error_free_file_data;
    }

    return 1;

  error_free_file_data:
    debug_mem_free(loadinfo->file_data, file, line);
  error_close_fh:
    if (!pkginfo) {
        sys_file_close(fh);
    }
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int start_async_read(LoadInfo *loadinfo)
{
    loadinfo->read_expected = loadinfo->compressed_size;
    loadinfo->read_request = sys_file_read_async(
        loadinfo->fp, loadinfo->file_data, loadinfo->compressed_size,
        loadinfo->data_offset, -1);
    if (UNLIKELY(!loadinfo->read_request)
     && sys_last_error() != SYSERR_TRANSIENT_FAILURE
     && sys_last_error() != SYSERR_FILE_ASYNC_FULL) {
        DLOG("%s: Failed to read %d bytes from file offset %lld",
             loadinfo->debug_path, loadinfo->compressed_size,
             (long long)loadinfo->data_offset);
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static int start_background_decompress(LoadInfo *loadinfo)
{
    PRECOND(loadinfo->pkginfo != NULL, return 0);

    for (int i = 0; i < lenof(loadinfo->decomp_read_buffer); i++) {
        loadinfo->decomp_read_buffer[i] =
            mem_alloc(bgdecomp_buffer_size, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!loadinfo->decomp_read_buffer[i])) {
            DLOG("%s: Failed to allocate read buffer %u",
                 loadinfo->debug_path, i);
            goto error_free_buffers;
        }
    }
    loadinfo->decomp_buffer_size = bgdecomp_buffer_size;
    loadinfo->decomp_state =
        (*loadinfo->pkginfo->decompress_init)(loadinfo->pkginfo);
    if (UNLIKELY(!loadinfo->decomp_state)) {
        DLOG("%s: Failed to create decompressor state", loadinfo->debug_path);
        goto error_free_buffers;
    }
    int stack_size = (*loadinfo->pkginfo->decompress_get_stack_size)(
                          loadinfo->pkginfo) + SIL_DLOG_MAX_SIZE;
    ASSERT(stack_size > 0, stack_size = 0);
    loadinfo->decomp_wu = workqueue_submit(
        bgdecomp_workqueue, decompress_thread, loadinfo);
    if (UNLIKELY(!loadinfo->decomp_wu)) {
        DLOG("%s: Failed to submit decompression work unit",
             loadinfo->debug_path);
        goto error_free_state;
    }
    return 1;

  error_free_state:
    (*loadinfo->pkginfo->decompress_finish)(
        loadinfo->pkginfo, loadinfo->decomp_state);
    loadinfo->decomp_state = NULL;
  error_free_buffers:
    for (int i = 0; i < lenof(loadinfo->decomp_read_buffer); i++) {
        mem_free(loadinfo->decomp_read_buffer[i]);
        loadinfo->decomp_read_buffer[i] = NULL;
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static int start_fallback_decompress(
    LoadInfo *loadinfo, const char *file, int line)
{
    const int alloc_size = lbound(loadinfo->compressed_size, 1);
    void *newdata = debug_mem_realloc(
        loadinfo->file_data, alloc_size, loadinfo->mem_flags,
        file, line, loadinfo->mem_type);
    if (UNLIKELY(!newdata)) {
        DLOG("%s: Failed to reallocate read buffer to %d bytes",
             loadinfo->debug_path, alloc_size);
        return 0;
    }
    loadinfo->file_data = newdata;
    loadinfo->compressed = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int loadinfo_sync(LoadInfo *loadinfo, int do_wait, int do_abort)
{
    if (loadinfo->need_finish) {
        goto out;  // Nothing left to load.
    }

    if (loadinfo->decomp_wu) {
        if (do_abort) {
            loadinfo->decomp_abort = 1;
            BARRIER();
        }
        if (!do_wait
         && !workqueue_poll(bgdecomp_workqueue, loadinfo->decomp_wu)) {
            goto out;
        }
        workqueue_wait(bgdecomp_workqueue, loadinfo->decomp_wu);
        (*loadinfo->pkginfo->decompress_finish)(loadinfo->pkginfo,
                                                loadinfo->decomp_state);
        loadinfo->decomp_state = NULL;
        for (int i = 0; i < lenof(loadinfo->decomp_read_buffer); i++) {
            mem_free(loadinfo->decomp_read_buffer[i]);
            loadinfo->decomp_read_buffer[i] = NULL;
        }
        loadinfo->decomp_wu = 0;
        loadinfo->need_finish = 1;
        goto out;
    }  // if (loadinfo->decomp_wu)

    if (do_abort) {
        if (loadinfo->read_request) {
            sys_file_abort_async(loadinfo->read_request);
            sys_file_wait_async(loadinfo->read_request);
            loadinfo->read_request = 0;
        }
        goto out;
    }

    while (!loadinfo->read_request) {
        loadinfo->read_expected = loadinfo->compressed_size;
        loadinfo->read_request = sys_file_read_async(
            loadinfo->fp, loadinfo->file_data,
            loadinfo->compressed_size, loadinfo->data_offset, -1);
        if (!loadinfo->read_request) {
            if (sys_last_error() == SYSERR_FILE_ASYNC_FULL) {
                if (do_wait) {
                    int res = sys_file_read_at(
                        loadinfo->fp, loadinfo->file_data,
                        loadinfo->compressed_size, loadinfo->data_offset);
                    if (res != loadinfo->read_expected) {
                        DLOG("%s: Read failed (expected %d bytes, got %d)",
                             loadinfo->debug_path, loadinfo->read_expected,
                             res);
                        loadinfo->read_failed = 1;
                        loadinfo->need_finish = 1;
                    }
                }
                goto out;
            } else if (sys_last_error() == SYSERR_TRANSIENT_FAILURE) {
                if (do_wait) {
                    thread_yield();
                    /* This is the only case that continues. */
                } else {
                    goto out;
                }
            } else {
                DLOG("%s: Failed to read %u bytes from %lld: %s",
                     loadinfo->debug_path, loadinfo->compressed_size,
                     (long long)loadinfo->data_offset, sys_last_errstr());
                loadinfo->read_failed = 1;
                loadinfo->need_finish = 1;
                goto out;
            }
        }
    }  // while (!loadinfo->read_request)

    if (!do_wait && !sys_file_poll_async(loadinfo->read_request)) {
        goto out;
    }

    int res = sys_file_wait_async(loadinfo->read_request);
    loadinfo->read_request = 0;
    if (res != loadinfo->read_expected) {
        DLOG("%s: Read failed (expected %d bytes, got %d)",
             loadinfo->debug_path, loadinfo->read_expected, res);
        loadinfo->read_failed = 1;
    }

    loadinfo->need_finish = 1;

  out:
    return loadinfo->need_finish;
}

/*-----------------------------------------------------------------------*/

static void finish_load(ResourceInfo *resinfo __DEBUG_PARAMS)
{
    PRECOND(resinfo != NULL, return);
    PRECOND(resinfo->loadinfo != NULL, return);
    LoadInfo *loadinfo = resinfo->loadinfo;
    PackageModuleInfo *pkginfo = loadinfo->pkginfo;

    /* Close the file if necessary. */
    if (loadinfo->need_close) {
        sys_file_close(loadinfo->fp);
        loadinfo->fp = NULL;
        loadinfo->need_close = 0;
    }

    /* If there was a read error, we have nothing else to do. */
    if (loadinfo->read_failed) {
        mem_free(loadinfo->file_data);
        goto free_and_return;
    }

    /* If the data is compressed, decompress it (if we didn't do so in the
     * background). */
    if (loadinfo->decomp_failed) {
        DLOG("%s: Background decompression failed", loadinfo->debug_path);
        mem_free(loadinfo->file_data);
        goto free_and_return;
    } else if (loadinfo->compressed) {
        ASSERT(pkginfo != NULL, goto skip_decomp);
        void *newdata = debug_mem_alloc(
            lbound(loadinfo->data_size,1), loadinfo->mem_align,
            loadinfo->mem_flags, file, line, resinfo->loadinfo->mem_type);
        if (UNLIKELY(!newdata)) {
            DLOG("%s: Out of memory for final buffer", loadinfo->debug_path);
            mem_free(loadinfo->file_data);
            goto free_and_return;
        }
        if (UNLIKELY(!(*pkginfo->decompress)(
                         pkginfo, NULL,
                         loadinfo->file_data, loadinfo->compressed_size,
                         newdata, loadinfo->data_size))) {
            DLOG("%s: Decompression failed", loadinfo->debug_path);
            mem_free(newdata);
            mem_free(loadinfo->file_data);
            goto free_and_return;
        }
        mem_free(loadinfo->file_data);
        loadinfo->file_data = newdata;
      skip_decomp:;
    }

    /* Perform appropriate parsing, and save the data pointer/ID in the
     * ResourceInfo structure.*/
    switch (resinfo->type) {
      case RES_TEXTURE:
        resinfo->texture = texture_parse(
            loadinfo->file_data, loadinfo->data_size, loadinfo->mem_flags,
            loadinfo->texture_mipmaps, 1);
        if (UNLIKELY(!resinfo->texture)) {
            DLOG("%s: Texture parse failed", loadinfo->debug_path);
            goto free_and_return;
        }
        break;

      case RES_FONT:
        ASSERT(loadinfo->font_parser != NULL, goto free_and_return);
        resinfo->font = (*(loadinfo->font_parser))(
            loadinfo->file_data, loadinfo->data_size, loadinfo->mem_flags, 1);
        if (UNLIKELY(!resinfo->font)) {
            DLOG("%s: Font parse failed", loadinfo->debug_path);
            goto free_and_return;
        }
        break;

      case RES_SOUND:
        resinfo->sound = sound_create(
            loadinfo->file_data, loadinfo->data_size, SOUND_FORMAT_AUTODETECT,
            1 __DEBUG_ARGS);
        if (UNLIKELY(!resinfo->sound)) {
            DLOG("%s: Sound creation failed", loadinfo->debug_path);
            goto free_and_return;
        }
        break;

      default:
        resinfo->data = loadinfo->file_data;
        resinfo->size = loadinfo->data_size;
        break;
    }  // switch (resinfo->type)

    /* Copy the pointer/ID and size to links as well. */
    int tries = 10000;
    for (ResourceInfo *i = resinfo->link_next; i != resinfo; i = i->link_next){
        tries--;
        if (UNLIKELY(tries <= 0)) {
            DLOG("BUG: endless linked list on resource %p (%s)",
                 resinfo, loadinfo->debug_path);
            break;
        }
        switch (resinfo->type) {
          case RES_TEXTURE:
            i->texture = resinfo->texture;
            break;
          case RES_FONT:
            i->font = resinfo->font;
            break;
          case RES_SOUND:
            i->sound = resinfo->sound;
            break;
          default:
            i->data = resinfo->data;
            i->size = resinfo->size;
            break;
        }
        i->loadinfo = NULL;
    }

  free_and_return:
    mem_free(resinfo->loadinfo);
    resinfo->loadinfo = NULL;
}

/*************************************************************************/
/************ Local routines: Background decompression thread ************/
/*************************************************************************/

static int decompress_thread(void *loadinfo_)
{
    LoadInfo *loadinfo = (LoadInfo *)loadinfo_;
    PackageModuleInfo *pkginfo = loadinfo->pkginfo;

    int next_read = 0;
    int next_decompress = 0;
    int bytes_read = 0;
    int last_async_id = 0;
    int last_read_size = 0;
    int next_async_id = 0;

    while (bytes_read < loadinfo->compressed_size) {
        BARRIER();
        if (loadinfo->decomp_abort) {
            goto fail;
        }

        /* Send out the next asynchronous read request before we wait for
         * the current one.  This helps avoid interruptions in data transfer
         * if decompression completes faster than read calls. */
        const int next_read_size =
            ubound(loadinfo->compressed_size - bytes_read,
                   loadinfo->decomp_buffer_size);
        next_async_id = sys_file_read_async(
            loadinfo->fp, loadinfo->decomp_read_buffer[next_read],
            next_read_size, loadinfo->data_offset + bytes_read, -1);
        if (next_async_id) {
            bytes_read += next_read_size;
            next_read = (next_read + 1) % lenof(loadinfo->decomp_read_buffer);
        } else {
            if (sys_last_error() != SYSERR_FILE_ASYNC_FULL
             && sys_last_error() != SYSERR_TRANSIENT_FAILURE) {
                DLOG("%s: Failed to start async read at %u+%u",
                     loadinfo->debug_path, bytes_read, next_read_size);
                goto fail;
            }
        }

        if (!last_async_id) {
            if (!next_async_id) {
                /* We're currently stuck unable to read; avoid busy-waiting
                 * within this thread. */
                thread_yield();
            }
        } else {
            const int last_read_result = sys_file_wait_async(last_async_id);
            last_async_id = 0;
            if (UNLIKELY(last_read_result != last_read_size)) {
                DLOG("%s: Failed to read data at %u+%u: %s",
                     loadinfo->debug_path, bytes_read - last_read_size,
                     last_read_size,
                     last_read_result < 0 ? "Read error" : "Short read");
                goto fail;
            }
            const int decompress_result = (*pkginfo->decompress)(
                pkginfo, loadinfo->decomp_state,
                loadinfo->decomp_read_buffer[next_decompress], last_read_size,
                loadinfo->file_data, loadinfo->data_size);
            if (UNLIKELY(decompress_result >= 0)) {
                if (decompress_result > 0) {
                    if (next_async_id) {
                        sys_file_abort_async(next_async_id);
                        sys_file_wait_async(next_async_id);
                        next_async_id = 0;
                    }
                    break;
                } else {
                    DLOG("%s: Decompression error at %u", loadinfo->debug_path,
                         bytes_read - last_read_size);
                }
                goto fail;
            }
            next_decompress =
                (next_decompress + 1) % lenof(loadinfo->decomp_read_buffer);
        }

        last_async_id = next_async_id;
        last_read_size = next_read_size;
        next_async_id = 0;
    }

    if (last_async_id) {
        const int last_read_result = sys_file_wait_async(last_async_id);
        if (UNLIKELY(last_read_result != last_read_size)) {
            DLOG("%s: Failed to read data at %u+%u (got %d)",
                 loadinfo->debug_path, bytes_read - last_read_size,
                 last_read_size, last_read_result);
            goto fail;
        }
        const int decompress_result = (*pkginfo->decompress)(
            pkginfo, loadinfo->decomp_state,
            loadinfo->decomp_read_buffer[next_decompress], last_read_size,
            loadinfo->file_data, loadinfo->data_size);
        if (UNLIKELY(decompress_result <= 0)) {
            if (decompress_result < 0) {
                DLOG("%s: Premature end of file", loadinfo->debug_path);
            } else {
                DLOG("%s: Decompression error at %u", loadinfo->debug_path,
                     bytes_read - last_read_size);
            }
            goto fail;
        }
    }

    return 0;

  fail:
    if (last_async_id != 0) {
        sys_file_wait_async(last_async_id);
    }
    if (next_async_id != 0) {
        sys_file_wait_async(next_async_id);
    }
    loadinfo->decomp_failed = 1;
    return 0;
}

/*************************************************************************/
/*************************************************************************/
