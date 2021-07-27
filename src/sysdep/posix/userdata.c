/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/userdata.c: User data access routines for
 * POSIX-compatible systems.
 */

/*
 * This file implements the common portion of user data access for systems
 * which use the standard POSIX filesystem interface for storing user data.
 * The system-specific code need only implement sys_userdata_get_data_path();
 * the routines in this file will call that function to retrieve the path
 * for accessing files (which may change over the life of the program,
 * though the value at sys_userdata_perform() time for a particular
 * operation will be used for that operation regardless of any concurrent
 * changes).
 *
 * The default file pathnames for this implementation are as follows, where
 * <path> is the path returned by sys_userdata_get_data_path():
 *
 *    - Save files: <path>save/save-NNNN.{bin,png}
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: <path>settings.bin
 *    - Per-user statistics file: <path>stats.bin
 *    - Arbitrary data files: <path><datafile-path>
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/sysdep/posix/userdata.h"
#include "src/userdata.h"
#include "src/utility/png.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

const char *
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    TEST_posix_userdata_path = NULL;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine declarations. */

/*-------- Helper functions --------*/

/**
 * generate_path:  Generate the pathname to use for the given operation
 * (assuming no override_path has been specified).
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     buffer: Buffer into which to place generated pathname.
 *     bufsize: Size of buffer, in bytes.
 * [Return value]
 *     True if the pathname was successfully generated, false on buffer
 *     overflow or other error.
 */
static int generate_path(const SysUserDataParams *params,
                         char *buffer, int bufsize);

/**
 * generate_save_screenshot_path:  Generate the pathname to use for the
 * screenshot associated with the given save file pathname.
 *
 * [Parameters]
 *     path: Save file pathname.
 *     buffer: Buffer into which to place generated pathname.
 *     bufsize: Size of buffer, in bytes.
 * [Return value]
 *     True if the pathname was successfully generated, false on buffer
 *     overflow or other error.
 */
static int generate_save_screenshot_path(const char *path,
                                         char *buffer, int bufsize);

/*-------- Operation-specific handling --------*/

/**
 * do_save:  Perform a generic save operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_save(SysUserDataParams *params, const char *path);

/**
 * do_load:  Perform a generic load operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_load(SysUserDataParams *params, const char *path);

/**
 * do_delete:  Perform a generic delete operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_delete(SysUserDataParams *params, const char *path);

/**
 * do_scan_savefiles:  Perform a SCAN_SAVEFILES operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of directory to scan.
 */
static int do_scan_savefiles(SysUserDataParams *params, const char *path);

/**
 * do_save_image:  Perform a save operation for an image file.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_save_image(SysUserDataParams *params, const char *path);

/**
 * do_load_image:  Perform a load operation for an image file.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_load_image(SysUserDataParams *params, const char *path);

/**
 * do_stats_load:  Perform a LOAD_STATS operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_stats_load(SysUserDataParams *params, const char *path);

/**
 * do_stats_save:  Perform a SAVE_STATS operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_stats_save(SysUserDataParams *params, const char *path);

/**
 * do_stats_clear:  Perform a CLEAR_STATS operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     path: Pathname of file to operate on.
 */
static int do_stats_clear(SysUserDataParams *params, const char *path);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_userdata_init(void)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_userdata_cleanup(void)
{
}

/*-----------------------------------------------------------------------*/

int sys_userdata_perform(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);

    /* Generate the pathname for this operation (unless an override path
     * was specified). */
    char pathbuf[PATH_MAX];
    if (params->override_path) {
        if (TEST_posix_userdata_path) {
// FIXME: remove this once GCC 8 is old
#if defined(__GNUC__) && __GNUC__ == 8  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87041
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat"
#endif
            if (!strformat_check(pathbuf, sizeof(pathbuf), "%s%s",
                                 TEST_posix_userdata_path,
                                 params->override_path)) {
                DLOG("Buffer overflow on path: %s%s",
                     TEST_posix_userdata_path, params->override_path);
                return 0;
            }
#if defined(__GNUC__) && __GNUC__ == 8
# pragma GCC diagnostic pop
#endif
        } else if (!strformat_check(pathbuf, sizeof(pathbuf), "%s",
                                    params->override_path)) {
            DLOG("Buffer overflow on path: %s", params->override_path);
            return 0;
        }
    } else {
        if (!generate_path(params, pathbuf, sizeof(pathbuf))) {
            return 0;
        }
    }

    /* Perform the operation. */
    int result = -1;
    switch (params->operation) {
      case SYS_USERDATA_SAVE_SAVEFILE:
      case SYS_USERDATA_SAVE_SETTINGS:
      case SYS_USERDATA_SAVE_DATA:
        result = do_save(params, pathbuf);
        break;

      case SYS_USERDATA_LOAD_SAVEFILE:
      case SYS_USERDATA_LOAD_SETTINGS:
      case SYS_USERDATA_LOAD_DATA:
        result = do_load(params, pathbuf);
        break;

      case SYS_USERDATA_DELETE_SAVEFILE:
      case SYS_USERDATA_DELETE_DATA:
        result = do_delete(params, pathbuf);
        break;

      case SYS_USERDATA_SCAN_SAVEFILES:
        result = do_scan_savefiles(params, pathbuf);
        break;

      case SYS_USERDATA_SAVE_SCREENSHOT:
        result = do_save_image(params, pathbuf);
        break;

      case SYS_USERDATA_LOAD_STATS:
        result = do_stats_load(params, pathbuf);
        break;

      case SYS_USERDATA_SAVE_STATS:
        result = do_stats_save(params, pathbuf);
        break;

      case SYS_USERDATA_CLEAR_STATS:
        result = do_stats_clear(params, pathbuf);
        break;
    }

    ASSERT(result >= 0 || !"Operation code was invalid.", result = 0);

    return result;
}

/*************************************************************************/
/******************* Local routines: Helper functions ********************/
/*************************************************************************/

static int generate_path(const SysUserDataParams *params,
                         char *buffer, int bufsize)
{
    PRECOND(params != NULL, return 0);
    PRECOND(buffer != NULL, return 0);
    PRECOND(bufsize > 0, return 0);

    int path_len;

    /* Retrieve the base user directory path from system-specific code. */
    char *sys_path = TEST_posix_userdata_path
        ? mem_strdup(TEST_posix_userdata_path, MEM_ALLOC_TEMP)
        : sys_userdata_get_data_path(params->program_name);
    if (!sys_path) {
        DLOG("Failed to get base path");
        return 0;
    }
    path_len = strformat(buffer, bufsize, "%s", sys_path);
    mem_free(sys_path);
    if (path_len >= bufsize) {
        DLOG("Path buffer overflow on user data directory");
        return 0;
    }

    /* Record whether the operation code is valid.  (We use this variable
     * instead of a "default" case in the switch() so the compiler can
     * detect any missing cases.) */
    int operation_valid = 0;

    /* Append the operation-specific directory and filename. */
    switch (params->operation) {

      case SYS_USERDATA_SAVE_SAVEFILE:
      case SYS_USERDATA_LOAD_SAVEFILE:
      case SYS_USERDATA_DELETE_SAVEFILE:
        path_len += strformat(buffer + path_len, bufsize - path_len,
                              "save/save-%04d.bin", params->savefile_num);
        operation_valid = 1;
        break;

      case SYS_USERDATA_SCAN_SAVEFILES:
        /* Pass back the path of the directory to read. */
        path_len += strformat(buffer + path_len, bufsize - path_len, "save");
        operation_valid = 1;
        break;

      case SYS_USERDATA_SAVE_SETTINGS:
      case SYS_USERDATA_LOAD_SETTINGS:
        path_len += strformat(buffer + path_len, bufsize - path_len,
                              "settings.bin");
        operation_valid = 1;
        break;

      case SYS_USERDATA_SAVE_SCREENSHOT:
      {
        const int base_path_len =
            path_len + strformat(buffer + path_len, bufsize - path_len,
                                 "screenshots/screen");
        int filenum = 0;
        do {
            path_len = base_path_len + strformat(buffer + base_path_len,
                                                 bufsize - base_path_len,
                                                 "%d.png", filenum);
            filenum++;
        } while (access(buffer, F_OK) == 0);
        operation_valid = 1;
        break;
      }

      case SYS_USERDATA_SAVE_DATA:
      case SYS_USERDATA_LOAD_DATA:
      case SYS_USERDATA_DELETE_DATA:
        ASSERT(params->datafile_path != NULL, return 0);
        path_len += strformat(buffer + path_len, bufsize - path_len,
                              "%s", params->datafile_path);
        operation_valid = 1;
        break;

      case SYS_USERDATA_LOAD_STATS:
      case SYS_USERDATA_SAVE_STATS:
      case SYS_USERDATA_CLEAR_STATS:
        path_len += strformat(buffer + path_len, bufsize - path_len,
                              "stats.bin");
        operation_valid = 1;
        break;

    }  // switch (params->operation)

    ASSERT(operation_valid, return 0);

    if (path_len >= bufsize) {
        DLOG("Path buffer overflow on user data file");
        return 0;
    }

    /* Pathname successfully generated. */
    return 1;
}

/*-----------------------------------------------------------------------*/

static int generate_save_screenshot_path(const char *path,
                                         char *buffer, int bufsize)
{
    PRECOND(path != NULL, return 0);
    PRECOND(buffer != NULL, return 0);
    PRECOND(bufsize > 0, return 0);

    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash)) {
        dot = path + strlen(path);
    }
    if (!strformat_check(buffer, bufsize, "%.*s.png", (int)(dot-path), path)) {
        DLOG("Buffer overflow on screenshot pathname");
        return 0;
    }

    return 1;
}

/*************************************************************************/
/************** Local routines: Operation-specific handling **************/
/*************************************************************************/

static int do_save(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    const char *data = params->save_data;
    uint32_t size = params->save_size;

    /* Write the file to disk. */
    int ok = posix_write_file(path, data, size, 1);
    if (!ok) {
        goto fail;
    }

    /* If this is a save file operation, save the screenshot separately,
     * or remove any existing screenshot if none was given with this
     * operation.  Note that we don't treat failure here as a failure of
     * the whole operation, since the save itself has already succeeded.
     * (Normally this will never fail, since posix_write_file() requires
     * write access to the directory, thus ensuring that a second write or
     * an unlink() call will succeed.  It could potentially fail if the
     * filesystem becomes full while writing the screenshot.) */
    if (params->operation == SYS_USERDATA_SAVE_SAVEFILE) {
        char png_path[PATH_MAX];
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            if (params->save_image) {
                do_save_image(params, png_path);
            } else {
                if (unlink(png_path) != 0 && errno != ENOENT) {
                    DLOG("Warning: unlink(%s) failed: %s", png_path,
                         strerror(errno));
                }
            }
        }
    }

    /* Success! */
    return 1;

    /* Error cases jump down here. */
  fail:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int do_load(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    char *buffer = NULL;
    uint32_t size = 0;

    /* Load the file into memory. */
    ssize_t size_temp;
    buffer = posix_read_file(path, &size_temp, 0);
    if (!buffer) {
        goto fail;
    }
#if SIZE_MAX > 0xFFFFFFFFUL
    if ((size_t)size_temp > 0xFFFFFFFFUL) {
        /* The user has probably given up waiting for the load long ago,
         * but let's avoid overflow anyway. */
        DLOG("%s: File too large", path);
        mem_free(buffer);
        goto fail;
    }
#endif
    size = (uint32_t)size_temp;

    /* If this is a save file operation, try to load the screenshot as well. */
    if (params->operation == SYS_USERDATA_LOAD_SAVEFILE) {
        char png_path[PATH_MAX];
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            const int result = do_load_image(params, png_path);
            if (!result) {
                params->load_image = NULL;
                params->load_image_width = 0;
                params->load_image_height = 0;
            }
        }
    }

    /* Success! */
    params->load_data = buffer;
    params->load_size = size;
    return 1;

    /* Error cases jump down here. */
  fail:
    mem_free(buffer);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int do_delete(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    if (unlink(path) != 0 && errno != ENOENT) {
        DLOG("unlink(%s) failed: %s", path, strerror(errno));
        return 0;
    }

    if (params->operation == SYS_USERDATA_DELETE_SAVEFILE) {
        char png_path[PATH_MAX];
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            if (unlink(png_path) != 0 && errno != ENOENT) {
                /* As with do_save(), we don't treat this as a failure. */
                DLOG("Warning: unlink(%s) failed: %s", png_path,
                     strerror(errno));
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static int do_scan_savefiles(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(params->scan_buffer != NULL, return 0);
    PRECOND(params->scan_count > 0, return 0);
    PRECOND(path != NULL, return 0);

    mem_clear(params->scan_buffer, params->scan_count);

    SysDir *dir = sys_dir_open(path);
    if (!dir) {
        /* If the directory is known not to exist, we can safely return
         * success with an array of zeroes.  Otherwise, return failure. */
        return (sys_last_error() == SYSERR_FILE_NOT_FOUND);
    }

    const char *filename;
    int is_subdir;
    while ((filename = sys_dir_read(dir, &is_subdir)) != NULL) {
        if (is_subdir || strncmp(filename, "save-", 5) != 0) {
            continue;
        }
        const int i = 5 + strspn(filename+5, "0123456789");
        if (strcmp(filename+i, ".bin") != 0) {
            continue;
        }
        const int num = atoi(filename+5);
        if (num >= params->savefile_num) {
            const int offset = num - params->savefile_num;
            if (offset < params->scan_count) {
                params->scan_buffer[offset] = 1;
            }
        }
    }

    sys_dir_close(dir);
    return 1;
}

/*-----------------------------------------------------------------------*/

static int do_save_image(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    uint32_t png_size;
    void *png_data = png_create(params->save_image,
                                params->save_image_width,
                                params->save_image_height,
                                0, -1, 0, MEM_ALLOC_TEMP, &png_size);
    if (!png_data) {
        DLOG("Failed to generate PNG file for screenshot");
        return 0;
    }
    const int result = posix_write_file(path, png_data, png_size, 0);
    mem_free(png_data);
    return result;
}

/*-----------------------------------------------------------------------*/

static int do_load_image(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    ssize_t png_size;
    void *png_data = posix_read_file(path, &png_size, MEM_ALLOC_TEMP);
    if (!png_data) {
        goto fail;
    }
#if SIZE_MAX > 0xFFFFFFFFUL
    if ((size_t)png_size > 0xFFFFFFFFU) {
        DLOG("%s: File too large", path);
        mem_free(png_data);
        goto fail;
    }
#endif

    params->load_image = png_parse(png_data, (uint32_t)png_size, 0,
                                   &params->load_image_width,
                                   &params->load_image_height);
    mem_free(png_data);
    if (!params->load_image) {
        DLOG("Failed to parse PNG file %s", path);
        goto fail;
    }

    return 1;

  fail:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int do_stats_load(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    /* Set default values for all stats. */
    for (int i = 0; i < params->stat_count; i++) {
        params->stat_values[i] = 0;
    }

    /* If the file doesn't exist, don't treat that as an error -- just
     * leave the default values in place. */
    if (access(path, F_OK) != 0) {
        return 1;
    }

    /* Load and parse the user's data. */
    ssize_t size;
    uint8_t * const data = posix_read_file(path, &size, MEM_ALLOC_TEMP);
    if (!data) {
        DLOG("Failed to load statistics file");
        return 0;
    }
    ssize_t pos = 0;
    for (int i = 0; i < params->stat_count; i++) {
        switch (params->stat_info[i].type) {

          case USERDATA_STAT_FLAG:
            if (pos+1 > size) {
                DLOG("Missing data in statistics file");
                goto exit;
            }
            if (data[pos] != 0 && data[pos] != 1) {
                DLOG("Invalid data in statistics file (ID %u)",
                     params->stat_info[i].id);
            } else {
                params->stat_values[i] = data[pos++];
            }
            break;

          case USERDATA_STAT_UINT32:
          case USERDATA_STAT_UINT32_MAX:
            if (pos+4 > size) {
                DLOG("Missing data in statistics file");
                goto exit;
            }
            /* IMPORTANT: The uint32_t cast on the first byte is required!
             * Without it, uint8_t gets promoted to (signed) int, so if
             * the high bit is set, the 32-bit value will be treated as a
             * negative number (technically, the result is implementation-
             * defined).  We cast the rest of the bytes as well for
             * parallelism. */
            params->stat_values[i] = (uint32_t)data[pos+0]<<24
                                   | (uint32_t)data[pos+1]<<16
                                   | (uint32_t)data[pos+2]<< 8
                                   | (uint32_t)data[pos+3]<< 0;
            pos += 4;
            break;

          case USERDATA_STAT_DOUBLE:
          case USERDATA_STAT_DOUBLE_MAX:
            if (pos+8 > size) {
                DLOG("Missing data in statistics file");
                goto exit;
            }
            {
                union {uint64_t i; double d;} u;
                u.i = (uint64_t)data[pos+0]<<56
                    | (uint64_t)data[pos+1]<<48
                    | (uint64_t)data[pos+2]<<40
                    | (uint64_t)data[pos+3]<<32
                    | (uint64_t)data[pos+4]<<24
                    | (uint64_t)data[pos+5]<<16
                    | (uint64_t)data[pos+6]<< 8
                    | (uint64_t)data[pos+7]<< 0;
                params->stat_values[i] = u.d;
            }
            pos += 8;
            break;

        }  // switch (params->stat_info[i].type)
    }  // for (int i = 0; i < params->stat_count; i++)

  exit:
    mem_free(data);
    params->load_data = NULL;
    params->load_size = 0;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int do_stats_save(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    /* Figure out how much buffer space we need. */
    int save_size = 0;
    for (int i = 0; i < params->stat_count; i++) {
        switch (params->stat_info[i].type) {
          case USERDATA_STAT_FLAG:
            save_size += 1;
            break;
          case USERDATA_STAT_UINT32:
          case USERDATA_STAT_UINT32_MAX:
            save_size += 4;
            break;
          case USERDATA_STAT_DOUBLE:
          case USERDATA_STAT_DOUBLE_MAX:
            save_size += 8;
            break;
        }
    }

    /* Create the file data in a memory buffer. */
    char *save_buffer = mem_alloc(save_size, 0, MEM_ALLOC_TEMP);
    if (!save_buffer) {
        DLOG("Out of memory for statistics data (%d bytes)", save_size);
        return 0;
    }
    char *data = save_buffer;
    for (int i = 0; i < params->stat_count; i++) {
        switch (params->stat_info[i].type) {
          case USERDATA_STAT_FLAG:
            *data++ = (params->stat_values[i] != 0) ? 1 : 0;
            break;
          case USERDATA_STAT_UINT32:
          case USERDATA_STAT_UINT32_MAX: {
            ASSERT(params->stat_values[i] >= 0.0, params->stat_values[i] = 0.0);
            ASSERT(params->stat_values[i] <= (double)0xFFFFFFFFU,
                   params->stat_values[i] = (double)0xFFFFFFFFU);
            uint32_t value = (uint32_t)params->stat_values[i];
            data[0] = value>>24 & 0xFF;
            data[1] = value>>16 & 0xFF;
            data[2] = value>> 8 & 0xFF;
            data[3] = value>> 0 & 0xFF;
            data += 4;
            break;
          }
          case USERDATA_STAT_DOUBLE:
          case USERDATA_STAT_DOUBLE_MAX: {
            union {uint64_t i; double d;} u;
            u.d = params->stat_values[i];
            data[0] = u.i>>56 & 0xFF;
            data[1] = u.i>>48 & 0xFF;
            data[2] = u.i>>40 & 0xFF;
            data[3] = u.i>>32 & 0xFF;
            data[4] = u.i>>24 & 0xFF;
            data[5] = u.i>>16 & 0xFF;
            data[6] = u.i>> 8 & 0xFF;
            data[7] = u.i>> 0 & 0xFF;
            data += 8;
            break;
          }
        }  // switch (params->stat_info[i].type)
    }  // for (int i = 0; i < params->stat_count; i++)

    /* Write out the data. */
    const int result = posix_write_file(path, save_buffer, save_size, 1);
    mem_free(save_buffer);
    if (!result) {
        DLOG("Failed to save statistics file");
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static int do_stats_clear(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    if (unlink(path) != 0 && errno != ENOENT) {
        DLOG("unlink(%s) failed: %s", path, strerror(errno));
        return 0;
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/
