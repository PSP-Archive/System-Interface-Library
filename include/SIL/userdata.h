/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/userdata.h: User data management header.
 */

/*
 * This header declares functionality related to storing persistent data
 * for users, such as preferences and save files.  All data storage should
 * pass through these functions; in particular, programs should _not_
 * attempt to use the C stdio or C++ stream functions to create or write
 * to arbitrary files, since such actions are not permitted on some
 * platforms.
 *
 * All user data access functions operate asynchronously.  To perform an
 * access, the caller first calls the access function (such as
 * userdata_load_savefile()), then periodically polls userdata_get_status()
 * to learn when the operation has completed.  Alternatively, the caller
 * may call userdata_wait() to block until the current operation completes,
 * but there is no guarantee on how long the operation may take, so this
 * function should only be used in limited circumstances.  Operations may
 * be run in parallel (provided that no operation modifies data that
 * another operation is also accessing), but the underlying implementation
 * is not guaranteed to support simultaneous operations, and may instead
 * delay later operations until earlier ones have finished.
 *
 * The result of a completed operation (success or failure) can be obtained
 * via userdata_get_result().  This function serves the secondary purpose
 * of freeing internal resources, so the caller should always call this
 * function even if it does not need the return value.  (As a corollary,
 * the function may only be called once, so the caller should save the
 * value if it will be needed later.)
 *
 * The user data interface includes several different sets of functions
 * for loading and storing various kinds of user data, such as save files
 * or settings data.  This is because some platforms (notably gaming
 * consoles) have specific interfaces tailored to these kinds of data, and
 * the reading or writing of arbitrary data files may even be forbidden by
 * the system's API.  Callers should choose the most appropriate function
 * for accessing user data, and should only use the generic
 * userdata_{load,save}_data() when no reasonable alternative exists, or
 * when the call will only be performed on a platform known to support
 * such arbitrary data files.
 *
 * Interface functions are also provided for storing per-user gameplay
 * statistics, such as high scores or gameplay achievements.  Statistics
 * must first be registered by calling userdata_register_stats() with a
 * table of values to be stored, providing a unique numeric ID and data
 * type for each.  (All statistics have an initial value of zero.)
 * Statistics can be retrieved or modified at any time by calling
 * userdata_get_stat() or userdata_set_stat() respectively; these
 * functions always access the local copy of the data and therefore return
 * immediately.  Changed values can be stored to persistent storage
 * (including remote servers, such as Apple's Game Center, on relevant
 * platforms) by calling userdata_update_stats().
 *
 * Before calling any user data access functions, the program must call
 * userdata_set_program_name() and provide a program name to associate
 * with all user data.  On PC platforms, for example, this name is used in
 * constructing the directory path under which user data is stored.  When
 * running on platforms which associate metadata with save files
 * (currently only the PSP platform), the program should also call
 * userdata_set_program_title() to set a title string to associate a
 * common title string (such as a game title) with save data.
 */

#ifndef SIL_USERDATA_H
#define SIL_USERDATA_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * UserStatType:  Enumeration of permitted data types for per-user
 * statistic values.
 */
enum UserStatType {
    /* Ordinary statistic values. */
    USERDATA_STAT_FLAG = 1,   // Boolean value, such as a gameplay achievement.
    USERDATA_STAT_UINT32,     // 32-bit unsigned integer value.
    USERDATA_STAT_DOUBLE,     // Double-precision floating point value.

    /* Statistics which keep the maximum value seen to date. */
    USERDATA_STAT_UINT32_MAX, // 32-bit unsigned integer value.
    USERDATA_STAT_DOUBLE_MAX, // Double-precision floating point value.
};
typedef enum UserStatType UserStatType;  // Typedef has to come last for C++.

/**
 * UserStatInfo:  Type for defining a per-user persistent statistic.
 */
typedef struct UserStatInfo UserStatInfo;
struct UserStatInfo {
    /* Unique ID for this statistic. */
    int id;
    /* Data type for this statistic. */
    UserStatType type;
    /* Optional string ID associated with this statistic.  On platforms
     * with native support for user statistics (such as "achievements" or
     * "trophies"), storing a non-NULL value here will cause an update
     * operation to report the current value of this statistic to the
     * system using this string as the statistic ID.  The string may be set
     * to different values on different platforms, to accommodate different
     * naming schemes; SIL only uses the numeric ID (UserStatInfo.id) to
     * identify individual statistics.  The value of this field should be a
     * string literal or similar value which will be valid and constant for
     * the life of the program. */
    const char *sys_id;
};

/**
 * UserStatValue:  Type for setting the value of a per-user persistent
 * statistic.  The value is handled as follows:
 *
 * - If the value is a NaN, the function fails regardless of data type.
 *
 * - For type FLAG, nonzero values are treated as "true", and zero values
 *   are treated as "false".  Once a flag is set to true, it cannot be
 *   reset to false (except by calling userdata_clear_stats() to clear
 *   everything at once).  When retrieving values, true flags are
 *   indicated by the value 1, false flags by the value 0.
 *
 * - For type UINT32, any fractional part of the value is truncated,
 *   negative values are treated as zero, and values greater than the
 *   maximum 32-bit unsigned integer (4,294,967,295) are treated as that
 *   integer.
 *
 * - For type DOUBLE, the value is used as is.
 */
typedef struct UserStatValue UserStatValue;
struct UserStatValue {
    unsigned int id;  // ID of statistic to update.
    double value;     // Value to set.
};

/*************************************************************************/

/*------------------------ Global state control -------------------------*/

/**
 * userdata_set_program_name:  Set the program name with which all user
 * data should be associated, and the game title to be displayed to the
 * user (on platforms which allow metadata to be associated with user
 * data files).  This function must be called before any other user data
 * functions can be used; the function can also be called later to change
 * the program name being used.
 *
 * For some platforms, the program name string must be in a specific
 * format, such as including a leading product ID.  It is the caller's
 * responsibility to ensure that the string is in the proper format for
 * the platform on which the program is running.
 *
 * This function does not fail, but if the program name is not set
 * properly, subsequent user data function calls will fail.
 *
 * [Parameters]
 *     program_name: String identifying this program.
 *     game_title: Game title string to associate with user data files.
 */
extern void userdata_set_program_name(const char *program_name);

/**
 * userdata_set_program_title:  Set or change the title string to be
 * displayed to the user on platforms which allow metadata to be
 * associated with user data files.
 *
 * If this function is not called, the title associated with user data
 * files on such platforms will be set to the empty string.
 *
 * [Parameters]
 *     title: Title string to associate with user data files.
 */
extern void userdata_set_program_title(const char *title);

/**
 * userdata_set_flip_image_for_save:  Set whether images passed to
 * userdata_save_savefile() and userdata_save_screenshot() should be
 * flipped vertically before writing.  If disabled, images will be saved
 * such that texture coordinate (0,0) corresponds to the upper-left corner
 * of the saved image; if enabled, the image will be flipped vertically,
 * and texture coordinate (0,0) will correspond to the lower-left corner
 * of the saved image.  The default is disabled.
 *
 * Note that _loaded_ images are never flipped, regardless of this setting.
 *
 * [Parameters]
 *     flip: True to enable vertical flipping on save, false to disable.
 */
extern void userdata_set_flip_image_for_save(int flip);

/**
 * userdata_has_remote_storage:  Return whether any sort of remote storage
 * functionality (such as cloud saving) is available for user data.  This
 * function only checks whether the platform itself supports remote
 * storage; attempting to actually use remote storage may fail if, for
 * example, the user is not online or is not signed in to the appropriate
 * service.  See userdata_get_remote_storage_state() to determine whether
 * access to remote storage is available.
 *
 * [Return value]
 *     True if the platform supports remote storage of user data, false if not.
 */
extern int userdata_has_remote_storage(void);

/**
 * userdata_get_remote_storage_state:  Return whether remote storage is
 * currently accessible.  If the platform does not support remote storage,
 * this function always returns false.
 *
 * [Return value]
 *     True if remote user data storage is currently accessible, false if not.
 */
extern int userdata_get_remote_storage_state(void);

/**
 * userdata_get_data_path:  Return a pathname suitable for accessing
 * resources stored as user data, if the platform supports such accesses.
 * The returned string should be prefixed to resource names with no
 * intervening separator.
 *
 * [Return value]
 *     Path prefix for accessing user data resources, or NULL if not supported.
 */
extern const char *userdata_get_data_path(void);

/**
 * userdata_override_file_path:  Set the pathname to be used for the next
 * user data operation.  This pathname is used as-is, without regard to
 * the program name or any system-specific naming rules, and may cause
 * the next operation to fail if the path is not specified appropriately
 * for the current platform.  Passing NULL will revert to the default
 * behavior of using a system-dependent pathname derived from the program
 * name, operation type, and operation parameters.
 *
 * The pathname set by this function will be used only for the next call
 * to userdata_save_savefile() or similar, regardless of the success or
 * failure of that function.
 *
 * This function is intended for porting games from other engines which
 * use their own save data filenames, and should not be called from
 * programs designed for this library.
 *
 * [Parameters]
 *     path: Pathname to use for the next operation, or NULL to reset
 *         behavior to the default.
 * [Return value]
 *     True on success, false on error (out of memory).
 */
extern int userdata_override_file_path(const char *path);

/*---------------------- Data save/load operations ----------------------*/

/**
 * userdata_save_savefile:  Begin saving a save file.  The data buffer
 * passed to this function must remain valid and must not be modified until
 * the operation completes.  (The title and description string buffers and
 * the image texture may be destroyed once the function returns.)
 *
 * If the image passed to this function is invalid or unreadable, or if a
 * system error occurs while saving the image, the image is silently
 * discarded.
 *
 * Behavior is undefined if this function is called while another save
 * operation or a delete operation on the same save file index is in
 * progress.
 *
 * [Parameters]
 *     num: Save file index (an arbitrary nonnegative integer).
 *     data: Data buffer pointer.
 *     size: Data buffer size, in bytes.
 *     title: Title text to associate with the file (on platforms which
 *         support such a feature).
 *     desc: Descriptive text to associate with the file (on platforms
 *         which support such a feature).
 *     image: Texture ID of image to associate with the file (on platforms
 *         which support such a feature); may be zero.  If nonzero, the
 *         the texture must be unlocked.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_save_savefile(int num, const void *data, uint32_t size,
                                  const char *title, const char *desc,
                                  int image);

/**
 * userdata_load_savefile:  Begin loading a save file.  If the operation
 * completes successfully, *data_ret and *size_ret will be filled with the
 * pointer to and size of the loaded data, respectively; the data buffer
 * should be freed with mem_free() when no longer needed.
 *
 * Behavior is undefined if this function is called while a save or delete
 * operation on the same save file index is in progress or if such an
 * operation is started before this operation completes.
 *
 * [Parameters]
 *     num: Save file index (an arbitrary nonnegative integer).
 *     data_ret: Pointer to variable to receive allocated buffer pointer.
 *     size_ret: Pointer to variable to receive buffer size, in bytes.
 *     image_ret: Pointer to variable to receive texture ID of associated
 *         image, or zero if no image was found.  May be NULL to ignore
 *         any image associated with the file.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_load_savefile(int num, void **data_ret, uint32_t *size_ret,
                                  int *image_ret);

/**
 * userdata_delete_savefile:  Begin deleting a save file.
 *
 * A successful result from the operation indicates that the given save
 * file does not exist, either because it was deleted or because it did not
 * exist in the first place.  However, the inverse does _not_ hold: callers
 * should not draw any inference from a failure result, since (for example)
 * a system error may have occurred before existence of the save file could
 * be checked.
 *
 * Behavior is undefined if this function is called while a load or save
 * operation on the same save file index is in progress.
 *
 * [Parameters]
 *     num: Save file index (an arbitrary nonnegative integer).
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_delete_savefile(int num);

/**
 * userdata_scan_savefiles:  Begin scanning for existing save files.  If
 * the operation completes successfully, *data_ret will point to an array
 * of "count" bytes; the byte at index i will have value 1 if save file
 * index first+i exists, 0 if not.  The array should be freed with
 * mem_free() when no longer needed.
 *
 * Some systems may not have this capability; on such systems, the
 * operation will always fail (either with a failing return from this
 * function or a failing operation result).
 *
 * The data returned in *data_ret is undefined with respect to any save
 * file index for which a save or delete operation was in progress when
 * this function is called or which is started before this operation
 * completes.
 *
 * [Parameters]
 *     first: First save file index to check.
 *     count: Number of save files to check.
 *     data_ret: Pointer to variable to receive the allocated buffer pointer.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_scan_savefiles(int first, int count, uint8_t **data_ret);

/*----------------------------------*/

/**
 * userdata_save_settings:  Begin saving user settings/preference data.
 * The data buffer passed to this function must remain valid and must not
 * be modified until the operation completes.  (The title and description
 * string buffers may be destroyed once the function returns.)
 *
 * Behavior is undefined if this function is called while another
 * save-settings operation is in progress.
 *
 * [Parameters]
 *     data: Data buffer pointer.
 *     size: Data buffer size, in bytes.
 *     title: Title text to associate with the file (on platforms which
 *         support such a feature).
 *     desc: Descriptive text to associate with the file (on platforms
 *         which support such a feature).
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_save_settings(const void *data, uint32_t size,
                                  const char *title, const char *desc);

/**
 * userdata_load_settings:  Begin loading user settings/preference data.
 * If the operation completes successfully, *data_ret and *size_ret will be
 * filled with the pointer to and size of the loaded data, respectively;
 * the data buffer should be freed with mem_free() when no longer needed.
 *
 * Behavior is undefined if this function is called while a save-settings
 * operation is in progress or if a save-settings operation is started
 * before this operation completes.
 *
 * [Parameters]
 *     data_ret: Pointer to variable to receive allocated buffer pointer.
 *     size_ret: Pointer to variable to receive buffer size, in bytes.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_load_settings(void **data_ret, uint32_t *size_ret);

/*----------------------------------*/

/**
 * userdata_save_screenshot:  Begin saving a screenshot image.
 *
 * [Parameters]
 *     texture_id: ID of texture to save as a screenshot (must not be locked).
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_save_screenshot(int texture_id);

/*----------------------------------*/

/**
 * userdata_save_data:  Begin saving an arbitrary user data file.  The data
 * buffer passed to this function must remain valid and must not be
 * modified until the operation completes.
 *
 * Behavior is undefined if this function is called while a save or delete
 * operation on the same file is in progress.
 *
 * This functionality may not be available on some platforms.  If not
 * available, this function will always return failure.
 *
 * [Parameters]
 *     path: Data file path.
 *     data: Data buffer pointer.
 *     size: Data buffer size, in bytes).
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_save_data(const char *path,
                              const void *data, uint32_t size);

/**
 * userdata_load_data:  Begin loading an arbitrary user data file.  If
 * the operation completes successfully, *data_ret and *size_ret will be
 * filled with the pointer to and size of the loaded data, respectively;
 * the data buffer should be freed with mem_free() when no longer needed.
 *
 * This functionality may not be available on some platforms.  If not
 * available, this function will always return failure.
 *
 * Behavior is undefined if this function is called while a save or delete
 * operation on the same file is in progress or if such an operation is
 * started before this operation completes.
 *
 * [Parameters]
 *     path: Data file path.
 *     data_ret: Pointer to variable to receive allocated buffer pointer.
 *     size_ret: Pointer to variable to receive buffer size, in bytes.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_load_data(const char *path,
                              void **data_ret, uint32_t *size_ret);

/**
 * userdata_delete_data:  Begin deleting an arbitrary user data file.  As
 * with userdata_delete_save(), a successful return value indicates that
 * the file either was deleted or did not exist to begin with, but a
 * failure result does not imply anything about the existence of the file.
 *
 * Behavior is undefined if this function is called while a save or delete
 * operation on the same file is in progress.
 *
 * [Parameters]
 *     path: Data file path.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_delete_data(const char *path);

/*----------------------------------*/

/**
 * userdata_register_stats:  Register the set of per-user statistics to be
 * recorded for this game, and begin loading statistic values from
 * persistent storage.  If any statistics have previously been registered,
 * they are discarded.  This function must be called, and the operation
 * must complete successfully, before any other operations on per-user
 * statistics are performed.
 *
 * On systems which support multiple user profiles (such as iOS with
 * GameKit enabled), this function should be called when a profile change
 * is detected to load the stored statistic values for the new profile.
 *
 * Note that statistics must always be listed in the same order in the
 * stat_info[] array to ensure correct behavior when loading from
 * persistent storage.
 *
 * Behavior is undefined if this function is called while another
 * register operation is in progress.
 *
 * [Parameters]
 *     stat_info: Array of statistic definitions.
 *     num_stat_info: Number of elements in the stat_info[] array.
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred.
 */
extern int userdata_register_stats(const UserStatInfo *stats, int num_stats);

/**
 * userdata_get_stat:  Retrieve the value of a per-user statistic.  For
 * boolean values (type USERDATA_STAT_FLAG), a nonzero return value
 * indicates true and zero indicates false.
 *
 * [Parameters]
 *     id: ID of statistic to retrieve.
 * [Return value]
 *     Statistic value, or zero if the ID has not been registered.
 */
extern double userdata_get_stat(int id);

/**
 * userdata_set_stat:  Set the value of a per-user statistic.  For boolean
 * values (type USERDATA_STAT_FLAG), pass a nonzero value for true, zero
 * for false.
 *
 * Statistics of type USERDATA_STAT_FLAG can only be set, not cleared, by
 * this function.  Call userdata_clear_stats() to clear such values.
 *
 * For statistics of type USERDATA_STAT_*_MAX, the stored value will only
 * be set if the value passed to this function is greater than the stored
 * value.
 *
 * Passing a NaN value for any type, or an out-of-range value for UINT32
 * types, is treated as an error and does not change the stored value.
 *
 * [Parameters]
 *     id: ID of statistic to set.
 *     value: Value to set.
 * [Return value]
 *     True if the stored value of the statistic was changed, false otherwise.
 */
extern int userdata_set_stat(int id, double value);

/**
 * userdata_update_stats:  Begin storing per-user statistic values to
 * persistent storage.
 *
 * Behavior is undefined if this function is called while another update
 * operation or a clear operation is in progress.
 *
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if no update was required or if no statistics have been registered.
 */
extern int userdata_update_stats(void);

/**
 * userdata_clear_stats:  Begin clearing all per-user statistic values to
 * their defaults.
 *
 * Behavior is undefined if this function is called while another clear
 * operation or an update operation is in progress.
 *
 * [Return value]
 *     Operation ID (nonzero) if the operation was successfully started,
 *     zero if an error occurred or if no statistics have been registered.
 */
extern int userdata_clear_stats(void);

/*--------------------- Operation status retrieval ----------------------*/

/**
 * userdata_get_status:  Check whether the given user data operation has
 * completed.
 *
 * This function returns true (not false) on an invalid ID so that
 * callers can safely use a false return value as a wait condition; i.e.,
 * if this function returns false for a given ID, it is guaranteed (modulo
 * system errors) to return true for that ID at some future time.
 *
 * [Parameters]
 *     id: Operation ID.
 * [Return value]
 *     True if the operation has completed or the operation ID is invalid;
 *     false if the operation is in progress.
 */
extern int userdata_get_status(int id);

/**
 * userdata_wait:  Wait until the given operation completes.  Returns
 * immediately if the operation has already completed (or the ID is
 * invalid).
 *
 * [Parameters]
 *     id: Operation ID.
 */
extern void userdata_wait(int id);

/**
 * userdata_get_result:  Return the result of the given user data operation.
 * After calling this function, the operation ID is no longer valid.
 *
 * The behavior of this function is undefined if called while the
 * operation is in progress.
 *
 * [Parameters]
 *     id: Operation ID.
 * [Return value]
 *     True if the operation succeeded; false if the operation failed or
 *     the ID is invalid.
 */
extern int userdata_get_result(int id);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_USERDATA_H
