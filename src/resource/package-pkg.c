/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/resource/package-pkg.c: Package access functions for PKG-format
 * package files (a custom format used by this library).
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/memory.h"
#include "src/resource.h"
#include "src/resource/package.h"
#include "src/resource/package-pkg.h"
#include "src/sysdep.h"
#include "src/utility/compress.h"

/*************************************************************************/
/********************* Package module implementation *********************/
/*************************************************************************/

/* Data structure for a single PKG-format package file. */
typedef struct PackageFile {
    char *pathname;         // Pathname of the package file.
    SysFile *fh;            // File handle for the package file.
    int64_t base_offset;    // Offset of the first byte of the package file
                            //    within its containing file, or zero if the
                            //    package file is its own system file.
    int package_size;       // Size of package file, in bytes.
    PKGIndexEntry *index;   // File index (sorted by hash, then pathname).
    int nfiles;             // Number of files.
    int list_pos;           // Current index for listing contained files.
    char *namebuf;          // Filename data buffer.
} PackageFile;

/* Macro to return the pathname of the file at the given index. */
#define NAME(i)  (info->namebuf + PKG_NAMEOFS(info->index[(i)].nameofs_flags))

/*-----------------------------------------------------------------------*/

static int package_pkg_init(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return 0);
    PRECOND(module->module_data != NULL, return 0);
    PackageFile *info = (PackageFile *)(module->module_data);
    PRECOND(info->pathname != NULL, return 0);

    /* Open the package file. */
    info->fh = resource_internal_open_file(info->pathname, &info->base_offset,
                                           &info->package_size);
    if (UNLIKELY(!info->fh)) {
        DLOG("open(%s): %s", info->pathname, sys_last_errstr());
        goto error_return;
    }

    /* Read in and verify the header data. */
    PKGHeader header;
    if (UNLIKELY(sys_file_read(info->fh, &header, 16) != 16)) {
        DLOG("EOF reading %s", info->pathname);
        goto error_close_file;
    }
    if (UNLIKELY(memcmp(header.magic, PKG_MAGIC, 4) != 0)) {
        DLOG("Bad magic number reading %s (got %02X%02X%02X%02X, expected"
             " %02X%02X%02X%02X)", info->pathname, header.magic[0],
             header.magic[1], header.magic[2], header.magic[3],
             PKG_MAGIC[0], PKG_MAGIC[1], PKG_MAGIC[2], PKG_MAGIC[3]);
        goto error_close_file;
    }
    PKG_HEADER_SWAP_BYTES(header);
    if (UNLIKELY(header.header_size != sizeof(PKGHeader))) {
        DLOG("Bad header size %d in %s", header.header_size, info->pathname);
        goto error_close_file;
    }
    if (UNLIKELY(header.entry_size != sizeof(PKGIndexEntry))) {
        DLOG("Bad index entry size %d in %s", header.entry_size,
             info->pathname);
        goto error_close_file;
    }

    /* Allocate memory for the index and pathname buffers. */
    info->nfiles = header.entry_count;
    const int index_size = sizeof(*info->index) * info->nfiles;
    info->index = mem_alloc(index_size, 4, 0);
    if (!info->index) {
        DLOG("No memory for %s directory (%d*%d)", info->pathname,
             info->nfiles, (int)sizeof(*info->index));
        goto error_close_file;
    }
    info->namebuf = mem_alloc(header.name_size, 1, 0);
    if (!info->namebuf) {
        DLOG("No memory for %s pathnames (%u bytes)", info->pathname,
             header.name_size);
        goto error_free_index;
    }

    /* Read in the file index. */
    if (UNLIKELY(sys_file_read(info->fh, info->index, index_size)
                 != index_size)) {
        DLOG("EOF reading %s directory", info->pathname);
        goto error_free_namebuf;
    }
    if (UNLIKELY(sys_file_read(info->fh, info->namebuf, header.name_size)
                 != (int)header.name_size)) {
        DLOG("EOF reading %s pathname table", info->pathname);
        goto error_free_namebuf;
    }
    PKG_INDEX_SWAP_BYTES(info->index, info->nfiles);

    /* Success! */
    return 1;

    /* Error handling. */
  error_free_namebuf:
    mem_free(info->namebuf);
    info->namebuf = NULL;
  error_free_index:
    mem_free(info->index);
    info->index = NULL;
  error_close_file:
    sys_file_close(info->fh);
    info->fh = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void package_pkg_cleanup(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return);
    PRECOND(module->module_data != NULL, return);
    PackageFile *info = (PackageFile *)(module->module_data);

    mem_free(info->namebuf);
    info->namebuf = NULL;
    mem_free(info->index);
    info->index = NULL;
    sys_file_close(info->fh);
    info->fh = NULL;
}

/*-----------------------------------------------------------------------*/

static void package_pkg_list_files_start(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return);
    PRECOND(module->module_data != NULL, return);
    PackageFile *info = (PackageFile *)(module->module_data);

    info->list_pos = 0;
}

/*-----------------------------------------------------------------------*/

static const char *package_pkg_list_files_next(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return NULL);
    PRECOND(module->module_data != NULL, return NULL);
    PackageFile *info = (PackageFile *)(module->module_data);

    if (info->list_pos < info->nfiles) {
        return NAME(info->list_pos++);
    } else {
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

static int package_pkg_file_info(PackageModuleInfo *module,
                                 const char *path, SysFile **file_ret,
                                 int64_t *pos_ret, int *len_ret,
                                 int *comp_ret, int *size_ret)
{
    PRECOND(module != NULL, return 0);
    PRECOND(module->module_data != NULL, return 0);
    PackageFile *info = (PackageFile *)(module->module_data);
    PRECOND(path != NULL, return 0);
    PRECOND(file_ret != NULL, return 0);
    PRECOND(pos_ret != NULL, return 0);
    PRECOND(len_ret != NULL, return 0);
    PRECOND(comp_ret != NULL, return 0);
    PRECOND(size_ret != NULL, return 0);
    PRECOND(info->fh != NULL, return 0);

    /* Look for the requested pathname in the index. */
    const uint32_t hash = pkg_hash(path);
    int low = 0, high = info->nfiles-1;
    int i = info->nfiles / 2;
    /* This test relies on the behavior of stricmp() treating alphabetic
     * characters as lowercase when comparing to non-alphabetic characters. */
    while (low <= high && stricmp(path, NAME(i)) != 0) {
        if (hash < info->index[i].hash) {
            high = i-1;
        } else if (hash > info->index[i].hash) {
            low = i+1;
        } else if (stricmp(path, NAME(i)) < 0) {
            high = i-1;
        } else {
            low = i+1;
        }
        i = (low + high + 1) / 2;
    }
    if (UNLIKELY(low > high)) {
        return 0;
    }

    /* Return the file's information. */
    *file_ret = info->fh;
    *pos_ret  = info->base_offset + info->index[i].offset;
    *len_ret  = ubound(info->index[i].datalen,
                       info->package_size - info->index[i].offset);
    *comp_ret = (info->index[i].nameofs_flags & PKGF_DEFLATED) ? 1 : 0;
    *size_ret = info->index[i].filesize;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int package_pkg_decompress_get_stack_size(
    UNUSED PackageModuleInfo *module)
{
    return 4096;  // Safe for both tinflate and zlib.
}

/*-----------------------------------------------------------------------*/

static void *package_pkg_decompress_init(UNUSED PackageModuleInfo *module)
{
    return decompress_create_state();
}

/*-----------------------------------------------------------------------*/

static int package_pkg_decompress(UNUSED PackageModuleInfo *module,
                                  void *state, const void *in, int insize,
                                  void *out, int outsize)
{
    PRECOND(in != NULL, return 0);
    PRECOND(out != NULL, return 0);

    if (state) {
        return decompress_partial(state, in, insize, out, outsize, NULL);
    } else {
        if (!decompress_to(in, insize, out, outsize, NULL)) {
            return 0;
        }
        return 1;
    }
}

/*-----------------------------------------------------------------------*/

static void package_pkg_decompress_finish(UNUSED PackageModuleInfo *module,
                                          void *state)
{
    decompress_destroy_state(state);
}

/*************************************************************************/
/******* Module instance creation/destruction routines (exported) ********/
/*************************************************************************/

PackageModuleInfo *pkg_create_instance(
    const char *package_path, const char *prefix)
{
    PRECOND(package_path != NULL, return NULL);
    PRECOND(prefix != NULL, return NULL);

    PackageModuleInfo *module = mem_alloc(sizeof(*module), 0, 0);
    PackageFile *pkg = mem_alloc(sizeof(*pkg), 0, 0);
    if (!module || !pkg) {
        DLOG("No memory for module data");
        goto error_free_structs;
    }

    module->prefix = mem_strdup(prefix, 0);
    pkg->pathname = mem_strdup(package_path, 0);
    if (!module->prefix || !pkg->pathname) {
        DLOG("No memory for pathnames: package_path[%s] prefix[%s]",
             package_path, prefix);
        goto error_free_pathnames;
    }

    module->init                      = package_pkg_init;
    module->cleanup                   = package_pkg_cleanup;
    module->list_files_start          = package_pkg_list_files_start;
    module->list_files_next           = package_pkg_list_files_next;
    module->file_info                 = package_pkg_file_info;
    module->decompress_get_stack_size = package_pkg_decompress_get_stack_size;
    module->decompress_init           = package_pkg_decompress_init;
    module->decompress                = package_pkg_decompress;
    module->decompress_finish         = package_pkg_decompress_finish;
    module->module_data               = pkg;
    return module;

  error_free_pathnames:
    mem_free(pkg->pathname);
    mem_free((char *)module->prefix);
  error_free_structs:
    mem_free(pkg);
    mem_free(module);
    return NULL;
}

/*-----------------------------------------------------------------------*/

void pkg_destroy_instance(PackageModuleInfo *module)
{
    if (module) {
        PackageFile *pkg = (PackageFile *)module->module_data;
        mem_free(pkg->pathname);
        mem_free(pkg);
        mem_free((char *)module->prefix);
        mem_free(module);
    }
}

/*************************************************************************/
/*************************************************************************/
