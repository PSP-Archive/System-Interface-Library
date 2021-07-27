/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/userdata.c: User data management routines.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/thread.h"
#include "src/userdata.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local structure used to store operation parameters. */

typedef struct UserDataParams {
    /* Parameter block for sys_userdata_perform(). */
    SysUserDataParams sysparams;

    /* Result of the operation. */
    int result;
    /* Return data pointers for load and scan operations. */
    void **load_data_ret;
    uint32_t *load_size_ret;
    int *load_image_ret;

    /* ID of this operation. */
    int id;
    /* Thread on which this operation is running. */
    int thread;
} UserDataParams;

/*-----------------------------------------------------------------------*/

/* Program name and game title, as set by userdata_set_program_name().
 * Stored in a mem_alloc()ed buffer. */
static char *program_name, *game_title;

/* Vertical flip flag for writing save file images. */
static uint8_t flip_image_for_save;

/* Array of allocated SysUserDataParams parameter blocks. */
static IDArray param_blocks = ID_ARRAY_INITIALIZER(1);

/* User data path buffer for userdata_get_data_path(), allocated when the
 * program name is set.  Stored in a mem_alloc()ed buffer. */
static char *userdata_path;

/* Override pathname to use for next operation (stored in a mem_alloc()ed
 * buffer), or NULL if none. */
static char *next_override_path;

/* Table of registered per-user statistics and current values. */
static struct {
    UserStatInfo info;
    double value;
    uint8_t updated;  // Has this value been updated since last loaded/saved?
} *stats;
static int num_stats;

/* Mutex for accessing the stats array. */
static int stats_mutex;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * new_params:  Create and return a new UserDataParams structure.  All
 * fields are cleared, except for:
 *    - sysparams.operation, which is set to the passed-in operation ID;
 *    - sysparams.override_path, which is copied from next_override_path;
 *    - sysparams.program_name and sysparams.game_title, which are set to
 *         the corresponding global variables; and
 *    - index and id, which are initialized appropriately.
 *
 * next_override_path is always cleared on return, regardless of success
 * or failure.
 *
 * [Parameters]
 *     operation: Operation ID to store in the structure.
 * [Return value]
 *     Newly-created UserDataParams structure, or NULL on error.
 */
static UserDataParams *new_params(SysUserDataOp operation);

/**
 * id_to_params:  Return the UserDataParams structure corresponding to the
 * given ID value.
 *
 * [Parameters]
 *     id: ID value to look up.
 * [Return value]
 *     Corresponding SysUserDataParams structure, or NULL if the ID is invalid.
 */
static UserDataParams *id_to_params(int id);

/**
 * free_params:  Free the given parameter block and associated resources,
 * except for "loaded data" pointers (which are assumed to have been either
 * copied out or freed already).  params must be non-NULL.
 *
 * [Parameters]
 *     params: Parameter block to free.
 */
static void free_params(UserDataParams *params);

/**
 * start_operation:  Start the given operation.  If the operation cannot be
 * started, its result is set to 0 (failure).
 *
 * [Parameters]
 *     params: Parameter block.
 * [Return value]
 *     Operation ID.
 */
static int start_operation(UserDataParams *params);

/**
 * operation_thread:  Thread routine for running a user data operation.
 *
 * [Parameters]
 *     params: Parameter block (UserDataParams *).
 * [Return value]
 *     Result of the operation.
 */
static int operation_thread(void *params_);

/*************************************************************************/
/***************** Interface: Initialization and cleanup *****************/
/*************************************************************************/

int userdata_init(void)
{
    stats_mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (UNLIKELY(!stats_mutex)) {
        DLOG("Failed to create mutex for stats array");
        goto error_return;
    }
    if (!sys_userdata_init()) {
        goto error_destroy_stats_mutex;
    }
    return 1;

  error_destroy_stats_mutex:
    mutex_destroy(stats_mutex);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void userdata_cleanup(void)
{
    ASSERT(id_array_size(&param_blocks) == 0);

    sys_userdata_cleanup();

    mem_free(stats);
    stats = NULL;
    num_stats = 0;

    mutex_destroy(stats_mutex);
    stats_mutex = 0;

    mem_free(program_name);
    mem_free(game_title);
    mem_free(userdata_path);
    program_name = game_title = userdata_path = NULL;
}

/*************************************************************************/
/******************** Interface: Global state control ********************/
/*************************************************************************/

void userdata_set_program_name(const char *program_name_)
{
    if (UNLIKELY(!program_name_)) {
        DLOG("Invalid parameters: program_name == NULL");
        return;
    }

    mem_free(program_name);
    mem_free(userdata_path);
    program_name = userdata_path = NULL;

    program_name = mem_strdup(program_name_, 0);
    if (!program_name) {
        DLOG("Out of memory saving program name");
        return;
    }

    userdata_path = sys_userdata_get_data_path(program_name);
}

/*-----------------------------------------------------------------------*/

void userdata_set_program_title(const char *title)
{
    if (UNLIKELY(!title)) {
        DLOG("Invalid parameters: title == NULL");
        return;
    }

    mem_free(game_title);
    game_title = mem_strdup(title, 0);
    if (!game_title) {
        DLOG("Out of memory saving title string");
    }
}

/*-----------------------------------------------------------------------*/

void userdata_set_flip_image_for_save(int flip)
{
    flip_image_for_save = (flip != 0);
}

/*-----------------------------------------------------------------------*/

const char *userdata_get_data_path(void)
{
    return userdata_path;
}

/*-----------------------------------------------------------------------*/

int userdata_override_file_path(const char *path)
{
    if (path) {
        char *path_dup = mem_strdup(path, MEM_ALLOC_TEMP);
        if (!path_dup) {
            DLOG("Out of memory saving override path");
            return 0;
        }
        mem_free(next_override_path);
        next_override_path = path_dup;
    } else {
        mem_free(next_override_path);
        next_override_path = NULL;
    }

    return 1;
}

/*************************************************************************/
/***************** Interface: Data save/load operations ******************/
/*************************************************************************/

int userdata_save_savefile(int num, const void *data, uint32_t size,
                           const char *title, const char *desc, int image)
{
    if (UNLIKELY(num < 0)
     || UNLIKELY(!data)
     || UNLIKELY(!title)
     || UNLIKELY(!desc)) {
        DLOG("Invalid parameters: %d %p %u %p[%s] %p[%s] %d",
             num, data, size, title, title ? title : "",
             desc, desc ? desc : "", image);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_SAVE_SAVEFILE);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.savefile_num = num;
    params->sysparams.title        = mem_strdup(title, MEM_ALLOC_TEMP);
    params->sysparams.desc         = mem_strdup(desc, MEM_ALLOC_TEMP);
    params->sysparams.save_data    = data;
    params->sysparams.save_size    = size;
    params->sysparams.save_image   = NULL;
    if (UNLIKELY(!params->sysparams.title)
     || UNLIKELY(!params->sysparams.desc)) {
        DLOG("Failed to copy title or description string");
        free_params(params);
        return 0;
    }

    if (image) {
        const uint8_t *pixels = texture_lock_readonly(image);
        if (!pixels) {
            DLOG("Failed to lock texture");
        } else {
            const uint32_t width = texture_width(image);
            const uint32_t height = texture_height(image);
            const uint32_t row_size = width * 4;
            const uint32_t pixels_size = height * row_size;
            uint8_t *pixels_copy = mem_alloc(pixels_size, 0, MEM_ALLOC_TEMP);
            if (!pixels_copy) {
                DLOG("No memory for copy of image data (%u bytes)",
                     pixels_size);
            } else {
                if (flip_image_for_save) {
                    const uint8_t *src = pixels + (height - 1) * row_size;
                    uint8_t *dest = pixels_copy;
                    for (unsigned int y = 0; y < height;
                         y++, src -= row_size, dest += row_size)
                    {
                        memcpy(dest, src, row_size);
                    }
                } else {
                    memcpy(pixels_copy, pixels, height * row_size);
                }
                params->sysparams.save_image        = pixels_copy;
                params->sysparams.save_image_width  = width;
                params->sysparams.save_image_height = height;
            }
            texture_unlock(image);
        }
    }

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_load_savefile(int num, void **data_ret, uint32_t *size_ret,
                           int *image_ret)
{
    if (UNLIKELY(num < 0) || UNLIKELY(!data_ret) || UNLIKELY(!size_ret)) {
        DLOG("Invalid parameters: %d %p %p %p", num, data_ret, size_ret,
             image_ret);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_LOAD_SAVEFILE);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.savefile_num = num;
    params->load_data_ret          = data_ret;
    params->load_size_ret          = size_ret;
    params->load_image_ret         = image_ret;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_delete_savefile(int num)
{
    if (UNLIKELY(num < 0)) {
        DLOG("Invalid parameters: %d", num);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_DELETE_SAVEFILE);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.savefile_num = num;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_scan_savefiles(int first, int count, uint8_t **data_ret)
{
    if (UNLIKELY(first < 0)
     || UNLIKELY(count <= 0)
     || UNLIKELY(!data_ret)) {
        DLOG("Invalid parameters: %d %d %p", first, count, data_ret);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_SCAN_SAVEFILES);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.savefile_num = first;
    params->sysparams.scan_count   = count;
    params->sysparams.scan_buffer  = mem_alloc(count, 1, 0);
    if (UNLIKELY(!params->sysparams.scan_buffer)) {
        DLOG("Failed to allocate buffer for scanning %d saves", count);
        free_params(params);
        return 0;
    }

    params->load_data_ret = (void **)data_ret;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_save_settings(const void *data, uint32_t size,
                           const char *title, const char *desc)
{
    if (UNLIKELY(!data) || UNLIKELY(!title) || UNLIKELY(!desc)) {
        DLOG("Invalid parameters: %p %u %p[%s] %p[%s]", data, size,
             title, title ? title : "", desc, desc ? desc : "");
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_SAVE_SETTINGS);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.title     = mem_strdup(title, MEM_ALLOC_TEMP);
    params->sysparams.desc      = mem_strdup(desc, MEM_ALLOC_TEMP);
    params->sysparams.save_data = data;
    params->sysparams.save_size = size;
    if (UNLIKELY(!params->sysparams.title)
     || UNLIKELY(!params->sysparams.desc)) {
        DLOG("Failed to copy title or description string");
        free_params(params);
        return 0;
    }

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_load_settings(void **data_ret, uint32_t *size_ret)
{
    if (UNLIKELY(!data_ret) || UNLIKELY(!size_ret)) {
        DLOG("Invalid parameters: %p %p", data_ret, size_ret);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_LOAD_SETTINGS);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->load_data_ret = data_ret;
    params->load_size_ret = size_ret;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_save_screenshot(int texture_id)
{
    if (UNLIKELY(!texture_id)) {
        DLOG("Invalid parameters: %d", texture_id);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_SAVE_SCREENSHOT);
    if (UNLIKELY(!params)) {
        return 0;
    }

    const uint8_t *pixels = texture_lock_readonly(texture_id);
    if (!pixels) {
        DLOG("Failed to lock texture");
        free_params(params);
        return 0;
    }

    const uint32_t width = texture_width(texture_id);
    const uint32_t height = texture_height(texture_id);
    const uint32_t row_size = width * 4;
    const uint32_t pixels_size = row_size * height;
    uint8_t *pixels_copy = mem_alloc(pixels_size, 0, MEM_ALLOC_TEMP);
    if (!pixels_copy) {
        DLOG("No memory for copy of image data (%u bytes)", pixels_size);
        texture_unlock(texture_id);
        free_params(params);
        return 0;
    }

    if (flip_image_for_save) {
        const uint8_t *src = pixels + (height - 1) * row_size;
        uint8_t *dest = pixels_copy;
        for (unsigned int y = 0; y < height;
             y++, src -= row_size, dest += row_size)
        {
            memcpy(dest, src, row_size);
        }
    } else {
        memcpy(pixels_copy, pixels, height * row_size);
    }

    texture_unlock(texture_id);

    params->sysparams.save_image        = pixels_copy;
    params->sysparams.save_image_width  = width;
    params->sysparams.save_image_height = height;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_save_data(const char *path, const void *data, uint32_t size)
{
    if (UNLIKELY(!path) || UNLIKELY(!data)) {
        DLOG("Invalid parameters: %p[%s] %p %u", path, path ? path : "",
             data, size);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_SAVE_DATA);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.datafile_path = path;
    params->sysparams.save_data     = data;
    params->sysparams.save_size     = size;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_load_data(const char *path, void **data_ret, uint32_t *size_ret)
{
    if (UNLIKELY(!path) || UNLIKELY(!data_ret) || UNLIKELY(!size_ret)) {
        DLOG("Invalid parameters: %p[%s] %p %p", path, path ? path : "",
             data_ret, size_ret);
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_LOAD_DATA);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.datafile_path = path;
    params->load_data_ret           = data_ret;
    params->load_size_ret           = size_ret;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_delete_data(const char *path)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        mem_free(next_override_path);
        next_override_path = NULL;
        return 0;
    }

    UserDataParams *params = new_params(SYS_USERDATA_DELETE_DATA);
    if (UNLIKELY(!params)) {
        return 0;
    }

    params->sysparams.datafile_path = path;

    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_register_stats(const UserStatInfo *stat_info, int num_stat_info)
{
    if (UNLIKELY(!stat_info) || UNLIKELY(num_stat_info <= 0)) {
        DLOG("Invalid parameters: %p %d", stats, num_stats);
        mem_free(next_override_path);
        next_override_path = NULL;
        goto error_return;
    }

    UserDataParams *params = new_params(SYS_USERDATA_LOAD_STATS);
    if (UNLIKELY(!params)) {
        goto error_return;
    }

    mutex_lock(stats_mutex);

    if (stats) {
        mem_free(stats);
        stats = NULL;
        num_stats = 0;
    }
    stats = mem_alloc(sizeof(*stats) * num_stat_info, 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!stats)) {
        DLOG("No memory for %d statistic entries", num_stat_info);
        mutex_unlock(stats_mutex);
        goto error_free_params;
    }
    num_stats = num_stat_info;

    int failed = 0;
    for (int i = 0; i < num_stat_info; i++) {
        int j;
        for (j = 0; j < i; j++) {
            if (stats[j].info.id == stat_info[i].id) {
                break;
            }
        }
        if (j < i) {
            DLOG("Stats entry %d: duplicate ID %d", i, stat_info[i].id);
            failed = 1;
            continue;
        }

        /* Let the compiler help us make sure we don't miss any types. */
        int type_ok = 0;
        switch (stat_info[i].type) {
          case USERDATA_STAT_FLAG:
          case USERDATA_STAT_UINT32:
          case USERDATA_STAT_DOUBLE:
          case USERDATA_STAT_UINT32_MAX:
          case USERDATA_STAT_DOUBLE_MAX:
            type_ok = 1;
            break;
        }
        if (!type_ok) {
            DLOG("Stats entry %d (ID %d): invalid type %d",
                 i, stat_info[i].id, stat_info[i].type);
            failed = 1;
            continue;
        }

        stats[i].info = stat_info[i];
    }  // for (int i = 0; i < num_stat_info; i++)

    if (failed) {
        goto error_free_stats;
    }

    params->sysparams.stat_info =
        mem_alloc(sizeof(*params->sysparams.stat_info) * num_stats, 0,
                  MEM_ALLOC_TEMP);
    params->sysparams.stat_values =
        mem_alloc(sizeof(*params->sysparams.stat_values) * num_stats, 0,
                  MEM_ALLOC_TEMP);
    params->sysparams.stat_count = num_stats;
    if (!params->sysparams.stat_info || !params->sysparams.stat_values) {
        DLOG("No memory for %d stats", num_stats);
        goto error_free_stats;
    }
    for (int i = 0; i < num_stats; i++) {
        params->sysparams.stat_info[i] = stats[i].info;
    }

    mutex_unlock(stats_mutex);
    return start_operation(params);

  error_free_stats:
    mem_free(stats);
    stats = NULL;
    num_stats = 0;
    mutex_unlock(stats_mutex);
  error_free_params:
    free_params(params);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

double userdata_get_stat(int id)
{
    if (!stats) {
        DLOG("No stats registered");
        return 0;
    }

    mutex_lock(stats_mutex);
    int i;
    for (i = 0; i < num_stats; i++) {
        if (stats[i].info.id == id) {
            break;
        }
    }
    if (i >= num_stats) {
        DLOG("Stat %d not found", id);
        mutex_unlock(stats_mutex);
        return 0;
    }

    const double value = stats[i].value;
    mutex_unlock(stats_mutex);
    return value;
}

/*-----------------------------------------------------------------------*/

int userdata_set_stat(int id, double value)
{
    if (UNLIKELY(isnan(value))) {
        DLOG("Invalid parameters: %d nan", id);
        return 0;
    }

    if (!stats) {
        DLOG("No stats registered");
        return 0;
    }

    mutex_lock(stats_mutex);
    int i;
    for (i = 0; i < num_stats; i++) {
        if (stats[i].info.id == id) {
            break;
        }
    }
    if (i >= num_stats) {
        DLOG("Stat %d not found", id);
        mutex_unlock(stats_mutex);
        return 0;
    }

    if (stats[i].info.type == USERDATA_STAT_UINT32
     || stats[i].info.type == USERDATA_STAT_UINT32_MAX) {
        if (value < 0.0 || value >= 4294967296.0) {
            DLOG("Invalid value for stat %d (out of range): %.0f", id, value);
            mutex_unlock(stats_mutex);
            return 0;
        } else if (value != floor(value)) {
            DLOG("Invalid value for stat %d (not integral): %.6f", id, value);
            mutex_unlock(stats_mutex);
            return 0;
        }
    }

    switch (stats[i].info.type) {
      case USERDATA_STAT_FLAG:
        if (value) {
            if (!stats[i].value) {
                stats[i].value = 1;
                stats[i].updated = 1;
            }
        } else {
            if (stats[i].value) {
                DLOG("Ignoring attempt to clear flag %d", id);
            }
        }
        break;

      case USERDATA_STAT_UINT32:
      case USERDATA_STAT_DOUBLE:
        stats[i].value = value;
        stats[i].updated = 1;
        break;

      case USERDATA_STAT_UINT32_MAX:
      case USERDATA_STAT_DOUBLE_MAX:
        if (value > stats[i].value) {
            stats[i].value = value;
            stats[i].updated = 1;
        }
        break;
    }

    mutex_unlock(stats_mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

int userdata_update_stats(void)
{
    UserDataParams *params = new_params(SYS_USERDATA_SAVE_STATS);
    if (UNLIKELY(!params)) {
        return 0;
    }

    if (!stats) {
        DLOG("No stats registered");
        free_params(params);
        return 0;
    }

    mutex_lock(stats_mutex);

    int update_needed = 0;
    for (int i = 0; i < num_stats; i++) {
        if (stats[i].updated) {
            update_needed = 1;
            break;
        }
    }
    if (!update_needed) {
        mutex_unlock(stats_mutex);
        free_params(params);
        return 0;
    }

    params->sysparams.stat_info =
        mem_alloc(sizeof(*params->sysparams.stat_info) * num_stats, 0,
                  MEM_ALLOC_TEMP);
    params->sysparams.stat_values =
        mem_alloc(sizeof(*params->sysparams.stat_values) * num_stats, 0,
                  MEM_ALLOC_TEMP);
    params->sysparams.stat_updated =
        mem_alloc(sizeof(*params->sysparams.stat_updated) * num_stats, 0,
                  MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    params->sysparams.stat_count = num_stats;
    if (!params->sysparams.stat_info || !params->sysparams.stat_values
     || !params->sysparams.stat_updated) {
        DLOG("No memory for %d stats", num_stats);
        mutex_unlock(stats_mutex);
        free_params(params);
        return 0;
    }
    for (int i = 0; i < num_stats; i++) {
        params->sysparams.stat_info[i] = stats[i].info;
        params->sysparams.stat_values[i] = stats[i].value;
        if (stats[i].updated) {
            params->sysparams.stat_updated[i] = 1;
            stats[i].updated = 0;
        }
    }

    mutex_unlock(stats_mutex);
    return start_operation(params);
}

/*-----------------------------------------------------------------------*/

int userdata_clear_stats(void)
{
    UserDataParams *params = new_params(SYS_USERDATA_CLEAR_STATS);
    if (UNLIKELY(!params)) {
        return 0;
    }

    if (!stats) {
        DLOG("No stats registered");
        free_params(params);
        return 0;
    }

    mutex_lock(stats_mutex);

    params->sysparams.stat_info =
        mem_alloc(sizeof(*params->sysparams.stat_info) * num_stats, 0,
                  MEM_ALLOC_TEMP);
    params->sysparams.stat_count = num_stats;
    if (!params->sysparams.stat_info) {
        DLOG("No memory for %d stats", num_stats);
        mutex_unlock(stats_mutex);
        free_params(params);
        return 0;
    }
    for (int i = 0; i < num_stats; i++) {
        params->sysparams.stat_info[i] = stats[i].info;
        stats[i].value = 0;
        stats[i].updated = 0;
    }

    mutex_unlock(stats_mutex);
    return start_operation(params);
}

/*************************************************************************/
/***************** Interface: Operation status retrieval *****************/
/*************************************************************************/

int userdata_get_status(int id)
{
    UserDataParams *params = id_to_params(id);
    if (UNLIKELY(!params)) {
        DLOG("Invalid ID: %d", id);
        return 1;
    }

    if (!params->thread) {
        return 1;
    }
    if (thread_is_running(params->thread)) {
        return 0;
    }
    userdata_wait(id);
    return 1;
}

/*-----------------------------------------------------------------------*/

void userdata_wait(int id)
{
    UserDataParams *params = id_to_params(id);
    if (UNLIKELY(!params)) {
        DLOG("Invalid ID: %d", id);
        return;
    }

    if (!params->thread) {
        return;  // Already waited for this operation.
    }

    params->result = thread_wait(params->thread);
    params->thread = 0;
}

/*-----------------------------------------------------------------------*/

int userdata_get_result(int id)
{
    UserDataParams *params = id_to_params(id);
    if (UNLIKELY(!params)) {
        DLOG("Invalid ID: %d", id);
        return 0;
    }

    if (params->result) {

        if (params->load_data_ret) {
            if (params->sysparams.scan_buffer) {
                *params->load_data_ret = params->sysparams.scan_buffer;
                /* Make sure not to free the buffer in free_params(). */
                params->sysparams.scan_buffer = NULL;
            } else {
                *params->load_data_ret = params->sysparams.load_data;
            }
        } else {
            mem_free(params->sysparams.load_data);
            mem_free(params->sysparams.scan_buffer);
        }

        if (params->load_size_ret) {
            *params->load_size_ret = params->sysparams.load_size;
        }

        if (params->load_image_ret) {
            *params->load_image_ret = 0;
            if (params->sysparams.load_image) {
                const uint32_t width = params->sysparams.load_image_width;
                const uint32_t height = params->sysparams.load_image_height;
                int image = texture_create_with_data(
                    width, height, params->sysparams.load_image,
                    TEX_FORMAT_RGBA8888, width, 0, 0);
                if (!image) {
                    DLOG("Failed to create texture for loaded image");
                } else {
                    *params->load_image_ret = image;
                }
            }
        }
        mem_free(params->sysparams.load_image);

        if (params->sysparams.operation == SYS_USERDATA_LOAD_STATS) {
            mutex_lock(stats_mutex);
            for (int i = 0; i < num_stats; i++) {
                for (int j = 0; j < params->sysparams.stat_count; j++) {
                    if (stats[i].info.id == params->sysparams.stat_info[j].id) {
                        stats[i].value = params->sysparams.stat_values[j];
                        break;
                    }
                }
            }
            mutex_unlock(stats_mutex);
        }

    } else {  // !params->result

        if (params->sysparams.operation == SYS_USERDATA_LOAD_STATS) {
            mutex_lock(stats_mutex);
            mem_free(stats);
            stats = NULL;
            num_stats = 0;
            mutex_unlock(stats_mutex);
        } else if (params->sysparams.operation == SYS_USERDATA_SAVE_STATS) {
            mutex_lock(stats_mutex);
            for (int i = 0; i < num_stats; i++) {
                for (int j = 0; j < params->sysparams.stat_count; j++) {
                    if (stats[i].info.id == params->sysparams.stat_info[j].id) {
                        stats[i].updated |= params->sysparams.stat_updated[j];
                        break;
                    }
                }
            }
            mutex_unlock(stats_mutex);
        }

    }

    int result = params->result;
    free_params(params);
    return result;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static UserDataParams *new_params(SysUserDataOp operation)
{
    UserDataParams *params = mem_alloc(sizeof(*params), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!params)) {
        DLOG("Out of memory allocating parameter block");
        mem_free(next_override_path);
        next_override_path = NULL;
        return NULL;
    }

    params->id = id_array_register(&param_blocks, params);
    if (UNLIKELY(!params->id)) {
        DLOG("Failed to register parameter block");
        mem_free(params);
        mem_free(next_override_path);
        next_override_path = NULL;
        return NULL;
    }

    params->sysparams.operation         = operation;
    params->sysparams.override_path     = next_override_path;
    params->sysparams.program_name      = program_name;
    params->sysparams.game_title        = game_title ? game_title : "";
    params->sysparams.savefile_num      = 0;
    params->sysparams.datafile_path     = NULL;
    params->sysparams.title             = NULL;
    params->sysparams.desc              = NULL;
    params->sysparams.save_data         = NULL;
    params->sysparams.save_size         = 0;
    params->sysparams.save_image        = NULL;
    params->sysparams.save_image_width  = 0;
    params->sysparams.save_image_height = 0;
    params->sysparams.load_data         = NULL;
    params->sysparams.load_size         = 0;
    params->sysparams.load_image        = NULL;
    params->sysparams.load_image_width  = 0;
    params->sysparams.load_image_height = 0;
    params->sysparams.scan_buffer       = NULL;
    params->sysparams.scan_count        = 0;
    params->sysparams.stat_info         = NULL;
    params->sysparams.stat_values       = NULL;
    params->sysparams.stat_updated      = NULL;
    params->sysparams.stat_count        = 0;
    params->sysparams.private           = NULL;
    params->load_data_ret               = NULL;
    params->load_size_ret               = NULL;
    params->load_image_ret              = NULL;
    params->thread                      = 0;

    /* Don't free the string buffer yet -- it's needed until
     * sys_userdata_start() is called. */
    next_override_path = NULL;

    return params;
}

/*-----------------------------------------------------------------------*/

static UserDataParams *id_to_params(int id)
{
    return id_array_get(&param_blocks, id);
}

/*-----------------------------------------------------------------------*/

static void free_params(UserDataParams *params)
{
    ASSERT(params != NULL, return);
    ASSERT(!params->thread, thread_wait(params->thread));

    id_array_release(&param_blocks, params->id);
    mem_free((void *)(params->sysparams.override_path));
    mem_free((char *)(params->sysparams.title));
    mem_free((char *)(params->sysparams.desc));
    mem_free((void *)(params->sysparams.save_image));
    mem_free(params->sysparams.scan_buffer);
    mem_free(params->sysparams.stat_info);
    mem_free(params->sysparams.stat_values);
    mem_free(params->sysparams.stat_updated);
    mem_free(params);
}

/*-----------------------------------------------------------------------*/

static int start_operation(UserDataParams *params)
{
    params->thread = thread_create(operation_thread, params);
    if (!params->thread) {
        params->result = 0;
    }
    return params->id;
}

/*-----------------------------------------------------------------------*/

static int operation_thread(void *params_)
{
    UserDataParams *params = (UserDataParams *)params_;
    return sys_userdata_perform(&params->sysparams);
}

/*************************************************************************/
/*************************************************************************/
