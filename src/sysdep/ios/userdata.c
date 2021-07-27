/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/userdata.c: User data access routines for iOS.  Forked
 * from the POSIX routines to add GameKit and file sharing support.
 */

/*
 * The default pathnames for this implementation are as follows:
 *
 *    - Save files: <user-data-path>/save/save-NNNN.{bin,png}
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: <user-data-path>/settings.bin
 *    - Per-user statistics file: <user-data-path>/stats.bin
 *    - Arbitrary data files: <user-data-path>/<datafile-path>
 *
 * where <user-data-path> is "<Application_Home>/Library/Application
 * Support<player_subpath>", "<Application_Home>" is the application's home
 * directory on iOS, and "<player_subpath>" is "/players/<player_id>"
 * (where "<player_id>" is the player ID massaged to form a safe pathname)
 * if a Game Center player is authenticated (including local authentication
 * as described below) and the empty string otherwise.
 *
 * userdata_get_data_path() is supported, and returns
 * "<Application_Home>/Library/Application Support".  Note that even when
 * Game Center support is enabled, the same path is returned regardless of
 * player ID, so in a Game Center-enabled program, this path is only
 * suitable for things like debug logs.
 *
 * Save files are compressed using zlib when saving, and decompressed when
 * loading.
 *
 * If Game Center support is enabled via the SIL_PLATFORM_IOS_USE_GAMEKIT
 * preprocessor symbol, flag-value statistics will be sent to the Game
 * Center server as achievements with the ID string specified for each
 * statistic (UserStatInfo.sys_id); statistics with a NULL value for
 * sys_id are ignored for this purpose.  The game will also synchronize
 * achievements with the Game Center server at startup, taking the union
 * of achievements stored locally and achievements stored on the server as
 * the user's current state.
 *
 * In addition to regular user data management, this code also maintains a
 * list of Game Center accounts which have saved any data, as well as the
 * last such account to have been authenticated by Game Center.  If Game
 * Center fails to return an authenticated player -- which may be either
 * because the user cancelled the login window or because the device has
 * no network connectivity; through iOS 7.1, GameKit does not seem to
 * differentiate between the two cases -- then we look up the most recent
 * authenticated player and treat them as locally authenticated, only
 * defaulting to "no authenticated player" if no Game Center player has
 * ever been seen.  This avoids the case where a user starts playing the
 * game under a Game Center account, then goes somewhere without network
 * connectivity and loses access to their saved games because Game Center
 * refuses to authenticate them while offline.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/ios/dialog.h"
#include "src/sysdep/ios/gamekit.h"
#include "src/sysdep/ios/util.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/sysdep/posix/thread.h"
#include "src/sysdep/posix/userdata.h"  // Declarations used in testing.
#include "src/userdata.h"
#include "src/utility/png.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

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

/* Local copy of statistic list for receiving server achievements. */
static UserStatInfo *stats;
int num_stats;

/*-----------------------------------------------------------------------*/

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
 * generate_player_dir_path:  Generate the pathname for the current
 * player's data directory, including a trailing slash.
 *
 * [Parameters]
 *     buffer: Buffer into which to place generated pathname.
 *     bufsize: Size of buffer, in bytes.
 *     player_id: Player ID.
 * [Return value]
 *     Length of generated string, or zero on buffer overflow.
 */
static int generate_player_dir_path(char *buffer, int bufsize,
                                    const char *player_id);

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

/**
 * update_achievements_from_server:  Update the in-memory achievement flags
 * with the values retrieved from the server.  If there are any
 * discrepancies, update the local file or the server data as appropriate.
 *
 * [Parameters]
 *     num_achievements: Number of achievements loaded, or zero if an error
 *         occurred (network failure, no local player, etc.).
 *     achievements: Array of achievements (NULL if num_achievements == 0).
 */
static void update_achievements_from_server(int num_achievements,
                                            const iOSAchievement *achievements);

/*-------- Exported save helper functions --------*/

#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING

/**
 * generate_save_export_path:  Generate the pathname to use for the
 * exported copy of the given save file pathname.
 *
 * [Parameters]
 *     path: Save file pathname.
 *     buffer: Buffer into which to place generated pathname.
 *     bufsize: Size of buffer, in bytes.
 * [Return value]
 *     True if the pathname was successfully generated, false on buffer
 *     overflow or other error.
 */
static int generate_save_export_path(const char *path, char *buffer,
                                     int bufsize);

/**
 * refresh_exported_saves:  Refresh the set of save files exported to the
 * user via iTunes file sharing.
 */
static void refresh_exported_saves(void);

#endif  // SIL_PLATFORM_IOS_USE_FILE_SHARING

/*-------- Current player management --------*/

/**
 * get_current_player:  Return the player ID of the currently authenticated
 * player.  This is the player currently authenticated by Game Center if
 * one exists, otherwise the authenticated player most recently seen (as
 * set by set_last_seen_player()).
 *
 * The add_if_new parameter determines how to handle a previously-unseen
 * player ID authenticated by Game Center, and is passed directly to
 * set_last_seen_player().
 *
 * Before returning, this function may:
 *    - Move all user data into a new, player-specific directory (if this is
 *      the first time a player ID has been authenticated on this install).
 *    - Copy all save files and associated screenshots to the exported
 *      documents directory (if this is the first time the program has been
 *      run with exporting enabled, or the active player has changed since
 *      the last export).
 *
 * [Parameters]
 *     add_if_new: True to add this ID to the known list if it's a new ID.
 * [Return value]
 *     Current player ID, or NULL if none.
 */
static const char *get_current_player(int add_if_new);

/**
 * set_last_seen_player:  Set the last seen player ID, for use when Game
 * Center is unable to authenticate the user.
 *
 * If the player ID is already known, the last seen ID is updated
 * unconditionally.  Otherwise, the behavior depends on the value of the
 * add_if_new parameter:
 *
 * - If true, the ID is added to the list of known player IDs, and the
 *   last seen ID is updated.
 *
 * - If false, the last seen ID is not updated _unless_ this is the first
 *   player ID we've seen.
 *
 * [Parameters]
 *     id: Player ID, or NULL to just load the list of known IDs.
 *     add_if_new: True to add this ID to the known list if it's a new ID.
 */
static void set_last_seen_player(const char *id, int add_if_new);

/**
 * check_refresh_exported_saves:  Refresh the set of exported save files
 * if necessary, either because the program was previously run without
 * exporting enabled, or because the active player is different from the
 * player whose saves are currently exported.
 *
 * [Parameters]
 *     player_id: ID of the active player, or the empty string if none.
 */
static void check_refresh_exported_saves(const char *player_id);

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
    mem_free(stats);
    stats = NULL;
    num_stats = 0;
}

/*-----------------------------------------------------------------------*/

char *sys_userdata_get_data_path(UNUSED const char *program_name)
{
    const char *path = ios_get_application_support_path();
    const unsigned int len = strlen(path);
    char *path_copy = mem_alloc(len+2, 1, 0);
    if (!path_copy) {
        DLOG("Out of memory copying user data directory path (%s)", path);
        return NULL;
    }
    memcpy(path_copy, path, len);
    path_copy[len] = '/';
    path_copy[len+1] = 0;
    return path_copy;
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
            if (!strformat_check(pathbuf, sizeof(pathbuf), "%s%s",
                                 TEST_posix_userdata_path,
                                 params->override_path)) {
                DLOG("Buffer overflow on path: %s%s",
                     TEST_posix_userdata_path, params->override_path);
                return 0;
            }
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
/*********************** Exported utility routines ***********************/
/*************************************************************************/

const char *ios_current_player(void)
{
    return get_current_player(0);
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

    /* Retrieve the current player ID. */
    const int add_if_new = (params->operation == SYS_USERDATA_SAVE_SAVEFILE
                         || params->operation == SYS_USERDATA_SAVE_SETTINGS
                         || params->operation == SYS_USERDATA_SAVE_SCREENSHOT
                         || params->operation == SYS_USERDATA_SAVE_DATA
                         || params->operation == SYS_USERDATA_SAVE_STATS);
    const char *player_id = get_current_player(add_if_new);

    /* Generate the user data directory name. */
    if (TEST_posix_userdata_path) {
        path_len = strformat(buffer, bufsize, "%s", TEST_posix_userdata_path);
    } else {
        path_len = generate_player_dir_path(buffer, bufsize, player_id);
    }
    if (UNLIKELY(!path_len)) {
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

static int generate_player_dir_path(char *buffer, int bufsize,
                                    const char *player_id)
{
    int path_len = strformat(buffer, bufsize, "%s/",
                             ios_get_application_support_path());
    if (player_id) {
        char *safe_player_id = mem_alloc(strlen(player_id)*3+1, 1,
                                         MEM_ALLOC_TEMP);
        if (UNLIKELY(!safe_player_id)) {
            DLOG("Out of memory generating user data directory");
            return 0;
        }
        const char *in = player_id;
        char *out = safe_player_id;
        while (*in) {
            const char c = *in++;
            if ((c >= 'A' && c <= 'Z')
             || (c >= 'a' && c <= 'z')
             || (c >= '0' && c <= '9')
             || c == '-'
             || c == '.') {
                *out++ = c;
            } else {
                static const char hexdigits[16] = "0123456789ABCDEF";
                *out++ = '_';
                *out++ = hexdigits[c>>4 & 0xF];
                *out++ = hexdigits[c>>0 & 0xF];
            }
        }
        *out = 0;
        if (path_len < bufsize) {
            path_len += strformat(buffer+path_len, bufsize-path_len,
                                  "players/%s/", safe_player_id);
        }
        mem_free(safe_player_id);
    }
    if (UNLIKELY(path_len >= bufsize)) {
        DLOG("Path buffer overflow on user data directory");
        return 0;
    }
    return path_len;
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
#ifndef SIL_PLATFORM_IOS_USE_FILE_SHARING
    UNUSED
#endif
        int saved_screenshot = 0;
    char png_path[PATH_MAX];
    if (params->operation == SYS_USERDATA_SAVE_SAVEFILE) {
        if (generate_save_screenshot_path(path, png_path, sizeof(png_path))) {
            if (params->save_image) {
                saved_screenshot = do_save_image(params, png_path);
            } else {
                if (unlink(png_path) != 0 && errno != ENOENT) {
                    DLOG("Warning: unlink(%s) failed: %s", png_path,
                         strerror(errno));
                }
            }
        }
    }

    /* If this is a save file operation and file sharing is enabled, copy the
     * save file and screenshot to the user-visible Documents directory. */
#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING
    if (params->operation == SYS_USERDATA_SAVE_SAVEFILE) {
        char export_path[PATH_MAX];
        if (generate_save_export_path(path, export_path, sizeof(export_path))) {
            if (!posix_copy_file(path, export_path, 1, 65536)) {
                DLOG("Failed to copy %s to %s: %s", path, export_path,
                     strerror(errno));
                /* IMPORTANT: If copying failed, try to delete any existing
                 * file; otherwise it will be imported over the new save
                 * next time the player goes to load the game. */
                if (unlink(export_path) != 0 && errno != ENOENT) {
                    DLOG("Failed to remove %s for failsafe: %s",
                         export_path, strerror(errno));
                    /* As above, this will result in the user losing their
                     * save data, so treat this as a failure. */
                    goto fail;
                }
            }
        }
        if (generate_save_export_path(png_path, export_path,
                                      sizeof(export_path))) {
            if (saved_screenshot) {
                if (!posix_copy_file(png_path, export_path, 1, 65536)) {
                    DLOG("Failed to copy %s to %s: %s", png_path, export_path,
                         strerror(errno));
                }
            } else {
                if (unlink(export_path) != 0 && errno != ENOENT) {
                    DLOG("Failed to remove %s (no screenshot): %s",
                         export_path, strerror(errno));
                }
            }
        }
    }
#endif

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

    /* If this is a save file operation and file sharing is enabled, see if
     * there's a different copy of the file in the Documents directory, and
     * import it if so.  Since iTunes preserves timestamps when copying
     * files, we can't just check whether the Documents file is newer;
     * instead, we have to assume that any differing timestamp or size
     * indicates that the file has been changed. */
#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING
    if (params->operation == SYS_USERDATA_LOAD_SAVEFILE) {
        char export_path[PATH_MAX];
        if (generate_save_export_path(path, export_path, sizeof(export_path))) {
            struct stat st_local, st_export;
            if (stat(path, &st_local) < 0) {
                if (errno != ENOENT) {
                    DLOG("stat(%s) failed: %s", path, strerror(errno));
                }
                st_local.st_mtime = 0;
            }
            if (stat(export_path, &st_export) < 0) {
                if (errno != ENOENT) {
                    DLOG("stat(%s) failed: %s", export_path, strerror(errno));
                }
                st_export.st_mtime = 0;
            }
            if (st_export.st_mtime != st_local.st_mtime
             || st_export.st_size != st_local.st_size) {
                DLOG("Importing save %d: external (size %zu mtime %ld)"
                     " != internal (size %zu mtime %ld)", params->savefile_num,
                     (size_t)st_export.st_size, (long)st_export.st_mtime,
                     (size_t)st_local.st_size, (long)st_local.st_mtime);
                if (!posix_copy_file(export_path, path, 1, 65536)) {
                    DLOG("Failed to copy %s to %s: %s", export_path, path,
                         strerror(errno));
                }
                char png_path[PATH_MAX];
                if (generate_save_screenshot_path(path, png_path,
                                                  sizeof(png_path))) {
                    if (generate_save_export_path(png_path, export_path,
                                                  sizeof(export_path))) {
                        unlink(png_path);
                        if (!posix_copy_file(export_path, png_path, 1, 65536)
                         && errno != ENOENT) {
                            DLOG("Failed to copy %s to %s: %s", export_path,
                                 png_path, strerror(errno));
                        }
                    }
                }
            }
        }
    }
#endif

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

    /* Save a copy of the numeric-to-string ID mapping for all statistics.
     * We continue even if this fails; the only effect is that we won't be
     * able to update anything reported by the server (which is no
     * different than the effect of a network problem while loading
     * achievements). */
    mem_free(stats);
    stats = mem_alloc(sizeof(*stats) * params->stat_count, 0, 0);
    if (!stats) {
        DLOG("Failed to allocate %d stat IDs, server achievements will be"
             " ignored", params->stat_count);
        num_stats = 0;
    } else {
        num_stats = params->stat_count;
        memcpy(stats, params->stat_info, sizeof(*stats) * params->stat_count);
    }

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
        mem_free(stats);
        stats = NULL;
        num_stats = 0;
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

    /* Start loading the server's idea of the current achievements (if
     * there's a logged-in Game Center player).  We let this run in the
     * background and return success immediately, so as not to unduly
     * delay the caller; if and when the server comes back to use, we
     * silently update the stored values, rewriting one or the other as
     * appropriate. */
    if (ios_gamekit_auth_status() == IOS_GAMEKIT_AUTH_OK) {
        ios_gamekit_load_achievements(update_achievements_from_server);
    }

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

    /* Set up an array for sending achievements to Game Center. */
    iOSAchievement *achievements = mem_alloc(
        sizeof(*achievements) * params->stat_count, 0, MEM_ALLOC_TEMP);
    if (!achievements) {
        DLOG("Out of memory for achievement buffer");
        return 0;
    }
    int num_achievements = 0;

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
        mem_free(achievements);
        return 0;
    }
    char *data = save_buffer;
    for (int i = 0; i < params->stat_count; i++) {
        switch (params->stat_info[i].type) {
          case USERDATA_STAT_FLAG:
            *data++ = (params->stat_values[i] != 0) ? 1 : 0;
            if (params->stat_updated[i] && params->stat_info[i].sys_id) {
                iOSAchievement *ach = &achievements[num_achievements];
                if (!strformat_check(ach->id, sizeof(ach->id), "%s",
                                     params->stat_info[i].sys_id)) {
                    DLOG("Buffer overflow on achievement %d (%s)",
                         params->stat_info[i].id, params->stat_info[i].sys_id);
                } else {
                    ach->progress = params->stat_values[i] ? 1.0 : 0.0;
                    num_achievements++;
                }
            }
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
    }

    /* Also pass on any achievements to the Game Kit layer. */
    if (num_achievements > 0
     && ios_gamekit_auth_status() == IOS_GAMEKIT_AUTH_OK) {
        ios_gamekit_update_achievements(num_achievements, achievements);
    }
    mem_free(achievements);

    return result;
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

    if (ios_gamekit_auth_status() == IOS_GAMEKIT_AUTH_OK) {
        ios_gamekit_clear_achievements();
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void update_achievements_from_server(int num_achievements,
                                            const iOSAchievement *achievements)
{
    /* Run through the list of achievements passed in, merging any set
     * achievements into the local list.  (We could be missing some if
     * they were achieved on a different device or if the app was deleted
     * and reinstalled.) */
    for (int i = 0; i < num_achievements; i++) {
        if (achievements[i].progress != 1.0) {
            continue;  // We don't care about unset ones.
        }

        int j;
        for (j = 0; j < num_stats; j++) {
            if (stats[j].sys_id
             && strcmp(achievements[i].id, stats[j].sys_id) == 0) {
                break;
            }
        }
        if (j == num_stats) {
            DLOG("Achievement %s not registered!", achievements[i].id);
        } else if (stats[j].type != USERDATA_STAT_FLAG) {
            DLOG("Stat %d (%s) is not an achievement!",
                 stats[j].id, stats[j].sys_id);
        } else {
            userdata_set_stat(stats[j].id, 1);
        }
    }

    /* Run through our local list of achievements and push any new ones
     * to the server. */
    int num_new_achievements = 0;
    iOSAchievement *new_achievements =
        mem_alloc(sizeof(*new_achievements) * num_stats, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!new_achievements)) {
        DLOG("No memory for updating achievements on server");
        return;
    }

    for (int i = 0; i < num_stats; i++) {
        if (!stats[i].sys_id
         || stats[i].type != USERDATA_STAT_FLAG
         || !userdata_get_stat(stats[i].id)) {
            continue;
        }

        iOSAchievement *new_ach = &new_achievements[num_new_achievements];
        if (!strformat_check(new_ach->id, sizeof(new_ach->id), "%s",
                             stats[i].sys_id)) {
            DLOG("Buffer overflow on achievement %d (%s)",
                 stats[i].id, stats[i].sys_id);
            continue;
        }

        int j;
        for (j = 0; j < num_achievements; j++) {
            if (stricmp(achievements[j].id, new_ach->id) == 0) {
                break;
            }
        }
        if (j == num_achievements || achievements[j].progress != 1.0) {
            new_ach->progress = 1.0;
            num_new_achievements++;
        }
    }

    if (num_new_achievements > 0) {
        ios_gamekit_update_achievements(num_new_achievements, new_achievements);
    }

    mem_free(new_achievements);
}

/*************************************************************************/
/************ Local routines: Exported save helper functions *************/
/*************************************************************************/

#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING

/*-----------------------------------------------------------------------*/

static int generate_save_export_path(const char *path, char *buffer,
                                     int bufsize)
{
    PRECOND(path != NULL, return 0);
    PRECOND(buffer != NULL, return 0);
    PRECOND(bufsize > 0, return 0);

    const char *slash = strrchr(path, '/');
    const char *filename = slash ? slash+1 : path;

    if (!strformat_check(buffer, bufsize, "%s/%s",
                         ios_get_documents_path(), filename)) {
        DLOG("Buffer overflow on export pathname");
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void refresh_exported_saves(void)
{
    DIR *d;
    char path_buf[PATH_MAX];

    /* First delete all existing save files in the Documents directory. */
    d = opendir(ios_get_documents_path());
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_namlen == 13  // save-NNNN.bin
             && strnicmp(de->d_name, "save-", 5) == 0
             && strspn(de->d_name+5, "0123456789") == 4
             && (strcmp(de->d_name+9, ".bin") == 0
              || strcmp(de->d_name+9, ".png") == 0))
            {
                if (!strformat_check(path_buf, sizeof(path_buf),
                                     "%s/%s", ios_get_documents_path(),
                                     de->d_name)) {
                    DLOG("Path buffer overflow on %s", de->d_name);
                } else if (unlink(path_buf) != 0) {
                    DLOG("unlink(%s) failed: %s", path_buf, strerror(errno));
                } else {
                    DLOG("Removed %s", path_buf);
                }
            }
        }
        closedir(d);
    }

    /* Now scan the internal save directory for save files and copy them to
     * the export directory. */
    const unsigned int player_dir_len =
        generate_player_dir_path(path_buf, sizeof(path_buf)-18,
                                 get_current_player(0));
    if (!player_dir_len) {
        DLOG("Failed to get user data directory");
        return;
    }
    strcpy(path_buf + player_dir_len, "save/");  // Safe.
    const unsigned int save_dir_len = player_dir_len + 5;
    d = opendir(path_buf);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_namlen == 13  // save-NNNN.{bin,png}
             && strnicmp(de->d_name, "save-", 5) == 0
             && strspn(de->d_name+5, "0123456789") == 4
             && (strcmp(de->d_name+9, ".bin") == 0
              || strcmp(de->d_name+9, ".png") == 0))
            {
                strcpy(path_buf + save_dir_len, de->d_name);  // Safe.
                char to_path[PATH_MAX];
                if (!strformat_check(to_path, sizeof(to_path),
                                     "%s/%s", ios_get_documents_path(),
                                     de->d_name)) {
                    DLOG("Path buffer overflow on %s", de->d_name);
                } else if (!posix_copy_file(path_buf, to_path, 1, 65536)) {
                    DLOG("copy(%s, %s) failed: %s", path_buf, to_path,
                         strerror(errno));
                } else {
                    DLOG("Exported %s", path_buf);
                }
            }
        }
        closedir(d);
    }
}

/*-----------------------------------------------------------------------*/

#endif  // SIL_PLATFORM_IOS_USE_FILE_SHARING

/*************************************************************************/
/*************** Local routines: Current player management ***************/
/*************************************************************************/

/* To avoid unnecessary I/O, we load the known player list on the
 * first call, and keep it in memory for the life of the program. */
static char **known_players = NULL;
static unsigned int num_known_players = 0;
static int known_players_loaded = 0;

/*-----------------------------------------------------------------------*/

static const char *get_current_player(int add_if_new)
{
    const char *gamekit_id = ios_gamekit_player_id();
    const char *id;
    if (gamekit_id) {
        set_last_seen_player(gamekit_id, add_if_new);
        id = gamekit_id;
    } else {
        set_last_seen_player(NULL, 0);
        id = num_known_players > 0 ? known_players[0] : NULL;
    }
    check_refresh_exported_saves(id ? id : "");
    return id;
}

/*-----------------------------------------------------------------------*/

static void set_last_seen_player(const char *id, int add_if_new)
{
    /* Failure code passed via crash reports (see fail_with_dialog below). */
    uint32_t failure_code = 0xD1EFFFFF;

    char path[PATH_MAX];
    if (!strformat_check(path, sizeof(path), "%s/player-list.txt",
                         ios_get_application_support_path())) {
        DLOG("Buffer overflow on %s/player-list.txt",
             ios_get_application_support_path());
        /* If we fail to load the file, we could potentially cause data
         * loss due to our local authentication logic.  There's no easy
         * out from here, so fail hard. */
        *(volatile int *)0xD1ED1E01 = 0;  // Deliberate crash.
        exit(-1);  // Just in case.
    }

    if (!known_players_loaded) {
        ssize_t size;
        void *data = posix_read_file(path, &size, MEM_ALLOC_TEMP);
        if (data) {
            const char *s = data;
            const char *top = s + size;
            while (s < top) {
                const char *eol = s;
                while (eol < top && *eol != '\n') {
                    eol++;
                }
                /* Assume this will succeed (since we're probably being
                 * called shortly after program startup, and again there's
                 * no easy out from here). */
                known_players = mem_realloc(
                    known_players,
                    sizeof(*known_players) * (num_known_players+1), 0);
                if (UNLIKELY(!known_players)) {
                    *(volatile int *)0xD1ED1E03 = 0;
                    exit(-1);
                }
                known_players[num_known_players] = mem_alloc((eol-s)+1, 1, 0);
                if (UNLIKELY(!known_players[num_known_players])) {
                    *(volatile int *)0xD1ED1E05 = 0;
                    exit(-1);
                }
                memcpy(known_players[num_known_players], s, eol-s);
                known_players[num_known_players][eol-s] = 0;
                num_known_players++;
                if (eol < top) {
                    eol++;
                }
                s = eol;
            }
            mem_free(data);
        }  // if (data)
        known_players_loaded = 1;
    }  // if (!known_players_loaded)

    if (!id) {
        return;  // Nothing to update.
    }

    if (num_known_players > 0 && strcmp(id, known_players[0]) == 0) {
        return;  // Normal case -- already the current player.
    }

    unsigned int i;
    for (i = 1; i < num_known_players; i++) {
        if (strcmp(id, known_players[i]) == 0) {
            char *saved_ptr = known_players[i];
            for (; i > 0; i--) {
                known_players[i] = known_players[i-1];
            }
            known_players[0] = saved_ptr;
            break;
        }
    }

    if (i >= num_known_players) {
        if (num_known_players > 0 && !add_if_new) {
            return;
        }
        known_players = mem_realloc(
            known_players, sizeof(*known_players) * (num_known_players+1), 0);
        if (UNLIKELY(!known_players)) {
            DLOG("Failed to extend known_players to %u entries, can't add"
                 " new player %s", num_known_players+1, id);
            /* From here down, it's conceivable (though highly unlikely)
             * that we could fail on a real system, so give an alert to
             * the player. */
            failure_code = 0xD1ED1E07;
            goto fail_with_dialog;
        }
        for (i = num_known_players; i > 0; i--) {
            known_players[i] = known_players[i-1];
        }
        num_known_players++;
        known_players[0] = mem_strdup(id, 0);
        if (UNLIKELY(!known_players[0])) {
            DLOG("Failed to strdup new player %s", id);
            failure_code = 0xD1ED1E09;
            goto fail_with_dialog;
        }

        /* If this is the first ID to be added, move any existing data to
         * that player's directory, so if someone starts playing without
         * logging in to Game Center and later logs in, they don't
         * permanently lose access to the data they originally saved. */
        if (num_known_players == 1) {
            static const char * const names_to_move[] = {
                "save", "screenshots", "settings.bin", "stats.bin",
            };
            unsigned int max_name_len = 0;
            for (i = 0; i < lenof(names_to_move); i++) {
                max_name_len = max(max_name_len, strlen(names_to_move[i]));
            }
            char base_path[1000];
            const int base_path_len = generate_player_dir_path(
                base_path, sizeof(base_path) - max_name_len, NULL);
            char player_path[1000];
            const int player_path_len = generate_player_dir_path(
                player_path, sizeof(player_path) - max_name_len, id);
            if (UNLIKELY(!base_path_len)) {
                DLOG("Buffer overflow on path 1");
            } else if (UNLIKELY(!player_path_len)) {
                DLOG("Buffer overflow on path 2");
            } else {
                DLOG("Moving user data from %s to %s", base_path, player_path);

                /* First create the player directory so we have someplace to
                 * move all the data. */
                strcpy(base_path + base_path_len, "players");  // Safe.
                if (UNLIKELY(mkdir(base_path, 0777) != 0 && errno != EEXIST)) {
                    DLOG("mkdir(%s): %s", base_path, strerror(errno));
                }
                player_path[player_path_len-1] = 0;
                if (UNLIKELY(mkdir(player_path, 0777) != 0
                             && errno != EEXIST)) {
                    DLOG("mkdir(%s): %s", player_path, strerror(errno));
                }
                player_path[player_path_len-1] = '/';

                /* Now move all possible paths into the target directory. */
                for (i = 0; i < lenof(names_to_move); i++) {
                    strcpy(base_path + base_path_len,  // Safe.
                           names_to_move[i]);
                    strcpy(player_path + player_path_len,  // Safe.
                           names_to_move[i]);
                    if (rename(base_path, player_path) == 0) {
                        DLOG("Moved %s", base_path);
                    } else if (errno != ENOENT) {
                        DLOG("rename(%s, %s): %s", base_path, player_path,
                             strerror(errno));
                    }
                }
            }
        }
    }

    /* The known player array has been modified (rearranged or extended),
     * so write it back to permanent storage. */
    unsigned int size = 0;
    for (i = 0; i < num_known_players; i++) {
        size += strlen(known_players[i]) + 1;
    }
    char *filebuf = mem_alloc(size, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!filebuf)) {
        DLOG("Failed to allocate file buffer for updating recent player list"
             " (%u bytes)", size);
        failure_code = 0xD1E00001 | size<<1;
        goto fail_with_dialog;
    }
    char *s = filebuf;
    for (i = 0; i < num_known_players; i++) {
        strcpy(s, known_players[i]);  // Safe.
        s += strlen(s);
        *s++ = '\n';
    }
    int save_ok = posix_write_file(path, filebuf, size, 1);
    mem_free(filebuf);
    if (UNLIKELY(!save_ok)) {
        DLOG("Failed to save updated player list: %s", strerror(errno));
        failure_code = 0xD1ED1E0B;
        goto fail_with_dialog;
    }

    return;

  fail_with_dialog:;
    ios_show_dialog_formatted("IOS_FRIENDLY_ERROR_TITLE",
                              "IOS_PLAYERLIST_ERROR_TEXT",
                              ios_get_application_name());
    *(volatile int *)(uintptr_t)failure_code = 0;
    exit(-1);
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING
# define USED_FILE_SHARING  /*nothing*/
#else
# define USED_FILE_SHARING  UNUSED
#endif

static void check_refresh_exported_saves(
    USED_FILE_SHARING const char *player_id)
{
#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING
    /* Player ID of the player whose saves are currently exported, or the
     * empty string if there is no player registered and the saves from
     * the top-level directory are exported.  Never NULL except at program
     * start. */
    static char *exported_player;
#endif

    char path[PATH_MAX];
    if (!strformat_check(path, sizeof(path), "%s/exported-player.txt",
                         ios_get_application_support_path())) {
        DLOG("Buffer overflow on %s/player-list.txt",
             ios_get_application_support_path());
        /* Just exit -- don't potentially delete existing exported saves. */
        return;
    }

#ifdef SIL_PLATFORM_IOS_USE_FILE_SHARING

    if (!exported_player) {
        ssize_t size;
        exported_player = posix_read_file(path, &size, 0);
        if (exported_player) {
            if (size == 0) {
                exported_player = mem_realloc(exported_player, 1, 0);
                /* Assume success.  If we can't allocate 1 byte, we have
                 * bigger problems to worry about. */
                size = 1;
            }
            exported_player[size-1] = 0;  // Was \n.
        } else {
            goto do_refresh;  // First run with exporting enabled.
        }
    }

    if (strcmp(exported_player, player_id ? player_id : "") == 0) {
        return;  // No change.
    }

  do_refresh:;
    char *new_player = mem_strdup(player_id, 0);
    if (new_player) {
        const unsigned int len = strlen(new_player);
        new_player[len] = '\n';
        if (posix_write_file(path, new_player, len+1, 1)) {
            /* Don't export unless we successfully wrote the state file.
             * Also make sure to update exported_player first, since we'll
             * be called again when generating pathnames for files to copy. */
            new_player[len] = 0;
            mem_free(exported_player);
            exported_player = new_player;
            DLOG("Refreshing saves for new player ID [%s]", player_id);
            if (!posix_thread_create_detached(
                    (void (*)(void *))refresh_exported_saves, NULL))
            {
                DLOG("Failed to start thread, refreshing synchronously");
                refresh_exported_saves();
            }
        } else {
            DLOG("Skipped refresh: failed to write export state file %s: %s",
                 path, strerror(errno));
        }
    } else {
        DLOG("Skipped refresh: no memory for player ID [%s]", player_id);
    }

#else  // !SIL_PLATFORM_IOS_USE_FILE_SHARING

    /* Make sure the state file doesn't exist, so next time we run with
     * exporting enabled, the then-current save files are exported. */
    unlink(path);

#endif
}

/*************************************************************************/
/*************************************************************************/
