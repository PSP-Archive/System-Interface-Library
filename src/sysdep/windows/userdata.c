/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/userdata.c: User data access routines for Windows.
 */

/*
 * The default file pathnames for this implementation are as follows, where
 * <AppData> is the current Windows user's Application Data directory:
 *
 *    - Save files: <AppData>/<program-name>/save/save-NNNN.bin
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: <AppData>/<program-name>/settings.bin
 *    - Per-user statistics file: <AppData>/<program-name>/stats.bin
 *    - Arbitrary data files: <AppData>/<program-name>/<datafile-path>
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/sysdep/windows/userdata.h"
#include "src/userdata.h"
#include "src/utility/png.h"

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

const char *
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    TEST_windows_userdata_path = NULL;

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

char *sys_userdata_get_data_path(const char *program_name)
{
    PRECOND(program_name != NULL, return NULL);

    char appdata[MAX_PATH*3];
    const int result =
        SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 1 /*SHGFP_TYPE_DEFAULT*/,
                        appdata);
    if (UNLIKELY(result != S_OK)) {
        DLOG("SHGetFolderPath(CSIDL_APPDATA) failed: %s",
             windows_strerror(result));
        return NULL;
    }
    appdata[sizeof(appdata)-1] = '\0';  // Guard against Windows breakage.
    for (unsigned int i = 0; appdata[i]; i++) {
        if (appdata[i] == '\\') {
            appdata[i] = '/';
        }
    }

    const int path_size = strlen(appdata)      // <AppData>
                        + 1                    // "/"
                        + strlen(program_name) // program_name
                        + 2;                   // "/\0"
    char *path = mem_alloc(path_size, 0, 0);
    if (UNLIKELY(!path)) {
        DLOG("Out of memory generating user data directory path (%u bytes)",
             path_size);
        return NULL;
    }

    ASSERT(strformat(path, path_size, "%s/%s/", appdata, program_name),
           return NULL);
    return path;
}

/*-----------------------------------------------------------------------*/

int sys_userdata_perform(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);

    /* Generate the pathname for this operation (unless an override path
     * was specified). */
    char pathbuf[MAX_PATH*3];
    if (params->override_path) {
        if (TEST_windows_userdata_path) {
            if (!strformat_check(pathbuf, sizeof(pathbuf), "%s%s",
                                 TEST_windows_userdata_path,
                                 params->override_path)) {
                DLOG("Buffer overflow on path: %s%s",
                     TEST_windows_userdata_path, params->override_path);
                return 0;
            }
        } else if (!strformat_check(pathbuf, sizeof(pathbuf), "%s",
                                    params->override_path)) {
            DLOG("Buffer overflow on path: %s", params->override_path);
            return 0;
        }
        for (char *s = pathbuf; *s; s++) {
            if (*s == '/') {
                *s = '\\';
            }
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

    /* Generate the base pathname, and convert to Windows format. */
    char *sys_path = TEST_windows_userdata_path
        ? mem_strdup(TEST_windows_userdata_path, MEM_ALLOC_TEMP)
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
    for (unsigned int i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '/') {
            buffer[i] = '\\';
        }
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
                              "save\\save-%04d.bin", params->savefile_num);
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
                                 "screenshots\\screen");
        int filenum = 0;
        do {
            path_len = base_path_len + strformat(buffer + base_path_len,
                                                 bufsize - base_path_len,
                                                 "%d.png", filenum);
            filenum++;
        } while (GetFileAttributes(buffer) != INVALID_FILE_ATTRIBUTES);
        if (GetLastError() != ERROR_FILE_NOT_FOUND
         && GetLastError() != ERROR_PATH_NOT_FOUND) {
            DLOG("Error looking for an unused filename: %s",
                 windows_strerror(GetLastError()));
            return 0;
        }
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

    const char *slash = strrchr(path, '\\');
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

    /* Check whether the file has been marked unwritable.  (It seems that
     * MoveFileEx() rejects attempts to replace a READONLY file, but the
     * documentation for MoveFileEx() is silent on the subject, so we
     * check manually just to be safe.) */
    const DWORD attributes = GetFileAttributes(path);
    if (attributes != INVALID_FILE_ATTRIBUTES
     && (attributes & FILE_ATTRIBUTE_READONLY)) {
        DLOG("%s marked read-only, aborting write operation", path);
        goto fail;
    }

    /* Create any necessary parent directories. */
    char *dirbuf = mem_strdup(path, MEM_ALLOC_TEMP);
    if (UNLIKELY(!dirbuf)) {
        DLOG("No memory for temporary path copy, can't create parent"
             " directories for %s", path);
    } else {
        char *s = dirbuf;
        if (strncmp(s, "\\\\?\\", 4) == 0) {
            /* "Raw path" prefix.  The next component will be either a
             * drive identifier or a namespace such as "UNC", so skip it
             * as well, and skip the hostname if a UNC path. */
            s += 4;
            if (strnicmp(s, "UNC\\", 4) == 0) {
                s += 4;
            }
            s += strcspn(s, "\\");
        } else if (s[0] == '\\' && s[1] == '\\') {
            /* Skip over the hostname part of a non-raw UNC path. */
            s += 2;
            s += strcspn(s, "\\");
        } else if (dirbuf[1] == ':') {  // <drive>:...
            s = dirbuf+2;
        } else {
            s = dirbuf;
        }
        if (*s) {  // Should be pointing to a backslash.
            s++;
        }
        while (s += strcspn(s, "\\"), *s != 0) {
            *s = 0;
            const int result = CreateDirectory(dirbuf, NULL);
            if (!result && GetLastError() != ERROR_ALREADY_EXISTS) {
                DLOG("Failed to create parent directory %s of %s: %s",
                     dirbuf, path, windows_strerror(GetLastError()));
                mem_free(dirbuf);
                goto fail;
            }
            *s++ = '\\';
        }
        mem_free(dirbuf);
    }

    /* Generate a temporary filename to use for writing, so we don't
     * destroy the original if a write error occurs. */
    char temppath[MAX_PATH*3];
    const unsigned int temppath_len =
        strformat(temppath, sizeof(temppath), "%s~", path);
    if (temppath_len >= sizeof(temppath)) {
        DLOG("Buffer overflow generating temporary pathname for %s", path);
        goto fail;
    }

    /* Write the data to the temporary file. */
    const HANDLE fh = CreateFile(temppath, FILE_WRITE_DATA, 0, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (fh == INVALID_HANDLE_VALUE) {
        DLOG("CreateFile(%s) failed: %s", temppath,
             windows_strerror(GetLastError()));
        goto fail;
    }
    DWORD bytes_written = 0;
    BOOL result = WriteFile(fh, data, size, &bytes_written, NULL);
    /* MSDN docs say that WriteFile() doesn't return true when writing to
     * a regular file (as opposed to a pipe) unless all data has been
     * written, but check for short writes anyway just to be safe. */
    ASSERT(!result || bytes_written == size, SetLastError(ERROR_WRITE_FAULT));
    if (!result) {
        DLOG("WriteFile(%s) failed: %s", temppath,
             windows_strerror(GetLastError()));
        CloseHandle(fh);
        DeleteFile(temppath);
        goto fail;
    }

    /* Explicitly sync the data to persistent storage.  This reduces the
     * risk of a system crash (BSoD, power outage, etc.) leaving a file
     * containing only null data, which has been observed under some
     * circumstances. */
    result = FlushFileBuffers(fh);
    CloseHandle(fh);
    if (!result) {
        DLOG("FlushFileBuffers(%s) failed: %s", temppath,
             windows_strerror(GetLastError()));
        DeleteFile(temppath);
        goto fail;
    }

    /* Rename the temporary file to the final filename.  The Windows SDK
     * documentation doesn't say whether replacement of an existing file
     * is atomic, so we'll have to just hope and pray... */
    if (!MoveFileEx(temppath, path, MOVEFILE_REPLACE_EXISTING)) {
        DLOG("MoveFileEx(%s, %s) failed: %s", temppath, path,
             windows_strerror(GetLastError()));
        goto fail;
    }

    /* If this is a save file operation, save the screenshot separately,
     * or remove any existing screenshot if none was given with this
     * operation.  Note that we don't treat failure here as a failure of
     * the whole operation, since the save itself has already succeeded. */
    if (params->operation == SYS_USERDATA_SAVE_SAVEFILE) {
        char png_path[MAX_PATH*3];
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            if (params->save_image) {
                do_save_image(params, png_path);
            } else {
                if (!DeleteFile(png_path)) {
                    const DWORD delete_error = GetLastError();
                    if (delete_error != ERROR_FILE_NOT_FOUND
                     && delete_error != ERROR_PATH_NOT_FOUND) {
                        DLOG("Warning: DeleteFile(%s) failed: %s", png_path,
                             windows_strerror(delete_error));
                    }
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

    /* Open the file and get its size. */
    const HANDLE fh = CreateFile(path, FILE_READ_DATA, 0, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (fh == INVALID_HANDLE_VALUE) {
        DLOG("CreateFile(%s) failed: %s", path,
             windows_strerror(GetLastError()));
        goto fail;
    }

    DWORD size_high = 0;
    size = GetFileSize(fh, &size_high);
    if (size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        DLOG("GetFileSize(%s) failed: %s", path,
             windows_strerror(GetLastError()));
        goto fail_close_fh;
    }
    if (size_high != 0) {  // In case we get a bogus file.
        DLOG("%s: File too large", path);
        goto fail_close_fh;
    }

    /* Allocate a buffer for loading the file. */
    buffer = mem_alloc(size > 0 ? size : 1, 0, 0);
    if (!buffer) {
        DLOG("%s: Out of memory (unable to allocate %u bytes)", path, size);
        goto fail_close_fh;
    }

    /* Read the file contents into the buffer and close the file. */
    DWORD bytes_read = 0;
    if (!ReadFile(fh, buffer, size, &bytes_read, NULL) || bytes_read < size) {
        DLOG("ReadFile(%s) failed: %s", path,
             windows_strerror(GetLastError()));
        goto fail_close_fh;
    }
    CloseHandle(fh);

    /* If this is a save file operation, try to load the screenshot as well. */
    if (params->operation == SYS_USERDATA_LOAD_SAVEFILE) {
        char png_path[MAX_PATH*3];
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
  fail_close_fh:
    CloseHandle(fh);
  fail:
    mem_free(buffer);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int do_delete(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    if (!DeleteFile(path)) {
        const DWORD delete_error = GetLastError();
        if (delete_error != ERROR_FILE_NOT_FOUND
         && delete_error != ERROR_PATH_NOT_FOUND) {
            DLOG("DeleteFile(%s) failed: %s", path,
                 windows_strerror(delete_error));
            return 0;
        }
    }

    if (params->operation == SYS_USERDATA_DELETE_SAVEFILE) {
        char png_path[MAX_PATH*3];
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            if (!DeleteFile(png_path)) {
                const DWORD delete_error = GetLastError();
                if (delete_error != ERROR_FILE_NOT_FOUND
                 && delete_error != ERROR_PATH_NOT_FOUND) {
                    /* As with do_save(), we don't treat this as a failure. */
                    DLOG("Warning: DeleteFile(%s) failed: %s", png_path,
                         windows_strerror(delete_error));
                }
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

    /* Generate a PNG file from the image data.  (We strip out the alpha
     * channel since it's unnecessary for screenshot-type files.) */
    uint32_t png_size;
    void *png_data = png_create(params->save_image,
                                params->save_image_width,
                                params->save_image_height,
                                0, -1, 0, MEM_ALLOC_TEMP, &png_size);
    if (!png_data) {
        DLOG("Failed to generate PNG file for screenshot");
        return 0;
    }

    /* Create a sub-request so we can let do_save() do the actual I/O. */
    SysUserDataParams sub_params;
    sub_params.operation     = SYS_USERDATA_SAVE_DATA;
    sub_params.override_path = NULL;
    sub_params.program_name  = params->program_name;
    sub_params.game_title    = params->game_title;
    sub_params.datafile_path = "";  // Not used, but just for completeness.
    sub_params.title         = params->title;
    sub_params.desc          = params->desc;
    sub_params.save_data     = png_data;
    sub_params.save_size     = png_size;

    const int result = do_save(&sub_params, path);

    /* Free the PNG data and return the sub-request's result as our own. */
    mem_free(png_data);
    return result;
}

/*-----------------------------------------------------------------------*/

static int do_load_image(SysUserDataParams *params, const char *path)
{
    PRECOND(params != NULL, return 0);
    PRECOND(path != NULL, return 0);

    /* Create a sub-request so we can let do_load() do the actual I/O. */
    SysUserDataParams sub_params;
    sub_params.operation     = SYS_USERDATA_LOAD_DATA;
    sub_params.override_path = NULL;
    sub_params.program_name  = params->program_name;
    sub_params.game_title    = params->game_title;
    sub_params.datafile_path = "";  // Not used, but just for completeness.

    const int result = do_load(&sub_params, path);
    if (!result) {
        return 0;
    }

    /* Decode the PNG file into a pixel buffer and free the file data. */
    params->load_image = png_parse(sub_params.load_data, sub_params.load_size,
                                   0, &params->load_image_width,
                                   &params->load_image_height);
    mem_free(sub_params.load_data);
    if (!params->load_image) {
        DLOG("Failed to parse PNG file %s", path);
        return 0;
    }

    /* Success! */
    return 1;
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
    if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES
        && (GetLastError() == ERROR_FILE_NOT_FOUND
         || GetLastError() == ERROR_PATH_NOT_FOUND))
    {
        return 1;
    }

    /* Load and parse the user's data. */
    const int result = do_load(params, path);
    if (!result) {
        DLOG("Failed to load statistics file");
        return 0;
    }
    uint8_t * const data = params->load_data;
    const uint32_t size = params->load_size;
    uint32_t pos = 0;
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

    /* Call do_save() to write out the data. */
    SysUserDataParams sub_params;
    sub_params.operation     = SYS_USERDATA_SAVE_DATA;
    sub_params.override_path = NULL;
    sub_params.program_name  = "";  // Not used, but just for completeness.
    sub_params.game_title    = "";  // As above.
    sub_params.datafile_path = "";  // As above.
    sub_params.title         = "";  // As above.
    sub_params.desc          = "";  // As above.
    sub_params.save_data     = save_buffer;
    sub_params.save_size     = save_size;
    const int result = do_save(&sub_params, path);
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

    if (!DeleteFile(path) && GetLastError() != ERROR_FILE_NOT_FOUND) {
        DLOG("DeleteFile(%s) failed: %s", path,
             windows_strerror(GetLastError()));
        return 0;
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
