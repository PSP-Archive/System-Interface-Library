/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/userdata.c: Testing implementation of the system-level
 * user data access functions.
 */

/*
 * When using path overrides with save files, the path override is ignored
 * for the save image in this implementation; thus, if a first operation
 * saves data A and image B with save number N, and a second operation
 * saves data C and image D with save number N but path override enabled,
 * attempting to load save number N will always return image D regardless
 * of whether path override is enabled for the load.  (The data itself is
 * properly segregated.)
 */

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/userdata.h"

/*************************************************************************/
/********************* Test control data (exported) **********************/
/*************************************************************************/

/* To enable the testing of the system's real userdata routines alongside
 * these test routines, we redo the sys_* stubs defined in sysdep.h to
 * switch on a userdata-specific flag. */

uint8_t sys_test_userdata_use_live_routines = 0;

#undef sys_userdata_init
#undef sys_userdata_cleanup
#undef sys_userdata_get_data_path
#undef sys_userdata_perform

#define DEFINE_STUB(type,name,params,args) \
extern type name params; \
static type TEST__##name params; \
type TEST_##name params { \
    return sys_test_userdata_use_live_routines \
        ? name args : TEST__##name args; \
}
DEFINE_STUB(int, sys_userdata_init, (void), ())
DEFINE_STUB(void, sys_userdata_cleanup, (void), ())
DEFINE_STUB(char *, sys_userdata_get_data_path, (const char *program_name), (program_name))
DEFINE_STUB(int, sys_userdata_perform, (SysUserDataParams *params), (params))
#undef DEFINE_STUB

#define sys_userdata_init          TEST__sys_userdata_init
#define sys_userdata_cleanup       TEST__sys_userdata_cleanup
#define sys_userdata_get_data_path TEST__sys_userdata_get_data_path
#define sys_userdata_perform       TEST__sys_userdata_perform

/*-----------------------------------------------------------------------*/

/* Flag controlling data writability. */
uint8_t sys_test_userdata_writable;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Stored data for save files. */
static struct {
    int num;
    void *data;
    int size;
    void *image;
    int width, height;
} *save_files;
static unsigned int num_save_files;

/* Stored settings data. */
static void *settings_data;
static int settings_size;

/* Stored data for generic data files. */
static struct {
    char *path;
    void *data;
    int size;
} *data_files;
static unsigned int num_data_files;

/* Stored screenshot data.  (Only the most recent screenshot is saved.) */
static void *screenshot_image;
static int screenshot_width, screenshot_height;

/* Table of registered per-user statistics and current values. */
static struct StatData {
    UserStatInfo info;
    double value;
} *stats;
static int num_stats;
/* Mutex for accessing stats[] and num_stats. */
static SysMutexID stats_mutex;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * store_datafile:  Store the given data as a generic data file.  The
 * data buffer must be a locally created copy, and its pointer will be
 * stored directly (thus it must not be freed by the caller).
 *
 * [Parameters]
 *     path: Data file path.
 *     data: File data.
 *     size: Size of file data, in bytes.
 * [Return value]
 *     True on success, false on error.
 */
static int store_datafile(const char *path, void *data, int size);

/**
 * get_datafile:  Return the data for the generic data file at the given
 * path, if any.
 *
 * [Parameters]
 *     path: Data file path.
 *     size_ret: Pointer to variable to receive size of file data, in bytes.
 * [Return value]
 *     Pointer to file data on success, NULL if the file does not exist.
 */
static const void *get_datafile(const char *path, int *size_ret);

/**
 * remove_datafile:  Remove the generic data file with the given path, if
 * one exists.
 *
 * [Parameters]
 *     path: Data file path.
 */
static void remove_datafile(const char *path);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_userdata_init(void)
{
    stats_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!stats_mutex)) {
        DLOG("Failed to create mutex for stats array");
        return 0;
    }

    sys_test_userdata_writable = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_userdata_cleanup(void)
{
    for (unsigned int i = 0; i < num_save_files; i++) {
        mem_free(save_files[i].data);
        mem_free(save_files[i].image);
    }
    mem_free(save_files);
    save_files = NULL;
    num_save_files = 0;

    mem_free(settings_data);
    settings_data = NULL;
    settings_size = 0;

    for (unsigned int i = 0; i < num_data_files; i++) {
        mem_free(data_files[i].path);
        mem_free(data_files[i].data);
    }
    mem_free(data_files);
    data_files = NULL;
    num_data_files = 0;

    mem_free(screenshot_image);
    screenshot_image = NULL;
    screenshot_width = screenshot_height = 0;

    mem_free(stats);
    stats = NULL;
    num_stats = 0;
    sys_mutex_destroy(stats_mutex);
    stats_mutex = 0;
}

/*-----------------------------------------------------------------------*/

char *sys_userdata_get_data_path(UNUSED const char *program_name)
{
    return NULL;  // Not supported.
}

/*-----------------------------------------------------------------------*/

int sys_userdata_perform(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);

    /* Support path override for only save, settings, and data files. */
    const char *datafile_path = params->datafile_path;
    if (params->override_path) {
        datafile_path = params->override_path;
        /* Treat *_SETTINGS as *_DATA for simplicity (we assume that any
         * pathname collision is intentional). */
        if (params->operation == SYS_USERDATA_SAVE_SETTINGS) {
            params->operation = SYS_USERDATA_SAVE_DATA;
        } else if (params->operation == SYS_USERDATA_LOAD_SETTINGS) {
            params->operation = SYS_USERDATA_LOAD_DATA;
        }
    }

    switch (params->operation) {

      case SYS_USERDATA_SAVE_SAVEFILE: {
        PRECOND(params->save_data != NULL, return 0);
        PRECOND(params->save_image == NULL || params->save_image_width > 0,
                return 0);
        PRECOND(params->save_image == NULL || params->save_image_height > 0,
                return 0);

        if (!sys_test_userdata_writable) {
            DLOG("sys_test_userdata_writable is false, failing");
            return 0;
        }

        void *data_copy = mem_alloc(lbound(params->save_size,1), 0, 0);
        const size_t image_size =
            params->save_image_width * params->save_image_height * 4;
        void *image_copy =
            params->save_image ? mem_alloc(image_size, 0, 0) : NULL;
        if (!data_copy || (params->save_image && !image_copy)) {
            DLOG("No memory for save/image data");
          save_savefile_fail:
            mem_free(data_copy);
            mem_free(image_copy);
            return 0;
        }
        memcpy(data_copy, params->save_data, params->save_size);
        if (params->save_image) {
            memcpy(image_copy, params->save_image, image_size);
        }

        unsigned int i;
        for (i = 0; i < num_save_files; i++) {
            if (save_files[i].num == params->savefile_num) {
                break;
            }
        }
        if (i == num_save_files) {
            void *save_files_new = mem_realloc(
                save_files, sizeof(*save_files) * (num_save_files+1), 0);
            if (!save_files_new) {
                DLOG("No memory to extend save file array");
                goto save_savefile_fail;
            }
            save_files = save_files_new;
            num_save_files++;
            save_files[i].num = params->savefile_num;
            save_files[i].data = NULL;
            save_files[i].image = NULL;
        }

        if (params->override_path) {
            if (!store_datafile(params->override_path, data_copy,
                                params->save_size)) {
                mem_free(data_copy);
                mem_free(image_copy);
                return 0;
            }
        } else {
            mem_free(save_files[i].data);
            save_files[i].data = data_copy;
            save_files[i].size = params->save_size;
        }
        mem_free(save_files[i].image);
        save_files[i].image = image_copy;
        save_files[i].width = params->save_image_width;
        save_files[i].height = params->save_image_height;
        return 1;
      }

      case SYS_USERDATA_SAVE_SETTINGS: {
        PRECOND(params->save_data != NULL, return 0);

        if (!sys_test_userdata_writable) {
            DLOG("sys_test_userdata_writable is false, failing");
            return 0;
        }

        settings_size = params->save_size;
        mem_free(settings_data);
        settings_data = mem_alloc(lbound(settings_size,1), 0, 0);
        if (!settings_data) {
            DLOG("No memory for save data (%u bytes)", settings_size);
            return 0;
        }
        memcpy(settings_data, params->save_data, settings_size);
        return 1;
      }

      case SYS_USERDATA_SAVE_DATA: {
        PRECOND(datafile_path != NULL, return 0);
        PRECOND(params->save_data != NULL, return 0);

        if (!sys_test_userdata_writable) {
            DLOG("sys_test_userdata_writable is false, failing");
            return 0;
        }

        void *data_copy = mem_alloc(lbound(params->save_size,1), 0, 0);
        if (!data_copy) {
            DLOG("No memory for data copy");
            return 0;
        }
        memcpy(data_copy, params->save_data, params->save_size);

        if (!store_datafile(datafile_path, data_copy, params->save_size)) {
            mem_free(data_copy);
            return 0;
        }

        return 1;
      }

      case SYS_USERDATA_LOAD_SAVEFILE: {
        unsigned int i;
        for (i = 0; i < num_save_files; i++) {
            if (save_files[i].num == params->savefile_num) {
                break;
            }
        }
        if (i == num_save_files) {
            DLOG("Save file %d not found", params->savefile_num);
            return 0;
        }

        const void *data;
        int size;
        if (params->override_path) {
            data = get_datafile(params->override_path, &size);
            if (!data) {
                DLOG("Save file %d not found at override path %s",
                     params->savefile_num, params->override_path);
                return 0;
            }
        } else {
            data = save_files[i].data;
            size = save_files[i].size;
            if (!data) {
                /* Must have been saved using an override path. */
                DLOG("No data for save file %d", params->savefile_num);
                return 0;
            }
        }

        params->load_size = size;
        params->load_image_width = save_files[i].width;
        params->load_image_height = save_files[i].height;
        params->load_data = mem_alloc(lbound(size,1), 0, 0);
        if (!params->load_data) {
            DLOG("No memory to return save data (%u bytes)", size);
            return 0;
        }
        memcpy(params->load_data, data, size);
        if (save_files[i].image) {
            params->load_image =
                mem_alloc(save_files[i].width * save_files[i].height * 4,
                          0, 0);
            if (!params->load_image) {
                DLOG("No memory to return save image (%dx%d)",
                     save_files[i].width, save_files[i].height);
                mem_free(params->load_data);
                params->load_data = NULL;
                return 0;
            }
            memcpy(params->load_image, save_files[i].image,
                   save_files[i].width * save_files[i].height * 4);
        } else {
            params->load_image = NULL;
        }
        return 1;
      }

      case SYS_USERDATA_LOAD_SETTINGS: {
        if (!settings_data) {
            DLOG("No settings data stored");
            return 0;
        }

        params->load_size = settings_size;
        params->load_data = mem_alloc(lbound(settings_size,1), 0, 0);
        if (!params->load_data) {
            DLOG("No memory to return settings data (%u bytes)",
                 settings_size);
            return 0;
        }
        memcpy(params->load_data, settings_data, settings_size);
        return 1;
      }

      case SYS_USERDATA_LOAD_DATA: {
        PRECOND(datafile_path != NULL, return 0);

        const void *data = get_datafile(datafile_path, &params->load_size);
        if (!data) {
            DLOG("Data file %s not found", datafile_path);
            return 0;
        }

        params->load_data = mem_alloc(lbound(params->load_size,1), 0, 0);
        if (!params->load_data) {
            DLOG("No memory to return file data (%u bytes)", params->load_size);
            return 0;
        }
        memcpy(params->load_data, data, params->load_size);
        return 1;
      }

      case SYS_USERDATA_DELETE_SAVEFILE: {
        for (unsigned int i = 0; i < num_save_files; i++) {
            if (save_files[i].num == params->savefile_num) {
                if (!sys_test_userdata_writable) {
                    DLOG("sys_test_userdata_writable is false, failing");
                    return 0;
                }
                if (params->override_path) {
                    remove_datafile(params->override_path);
                }
                mem_free(save_files[i].data);
                mem_free(save_files[i].image);
                num_save_files--;
                memmove(&save_files[i], &save_files[i+1],
                        (num_save_files - i) * sizeof(*save_files));
                break;
            }
        }
        return 1;
      }

      case SYS_USERDATA_DELETE_DATA: {
        PRECOND(datafile_path != NULL, return 0);

        if (!sys_test_userdata_writable) {
            for (unsigned int i = 0; i < num_data_files; i++) {
                if (stricmp(data_files[i].path, datafile_path) == 0) {
                    DLOG("sys_test_userdata_writable is false, failing");
                    return 0;
                }
            }
        }
        remove_datafile(datafile_path);
        return 1;
      }

      case SYS_USERDATA_SCAN_SAVEFILES: {
        PRECOND(params->scan_buffer != NULL, return 0);
        PRECOND(params->scan_count > 0, return 0);

        mem_clear(params->scan_buffer, params->scan_count);
        for (unsigned int i = 0; i < num_save_files; i++) {
            if (save_files[i].num >= params->savefile_num) {
                const int offset = save_files[i].num - params->savefile_num;
                if (offset < params->scan_count) {
                    params->scan_buffer[offset] = 1;
                }
            }
        }
        return 1;
      }

      case SYS_USERDATA_SAVE_SCREENSHOT: {
        PRECOND(params->save_image != NULL, return 0);
        PRECOND(params->save_image_width > 0, return 0);
        PRECOND(params->save_image_height > 0, return 0);

        if (!sys_test_userdata_writable) {
            DLOG("sys_test_userdata_writable is false, failing");
            return 0;
        }

        screenshot_width = params->save_image_width;
        screenshot_height = params->save_image_height;
        mem_free(screenshot_image);
        screenshot_image =
            mem_alloc(screenshot_width * screenshot_height * 4, 0, 0);
        if (!screenshot_image) {
            DLOG("No memory for screenshot copy (%dx%d)",
                 screenshot_width, screenshot_height);
            return 0;
        }
        memcpy(screenshot_image, params->save_image,
               screenshot_width * screenshot_height * 4);
        return 1;
      }

      case SYS_USERDATA_LOAD_STATS: {
        PRECOND(params->stat_info != NULL, return 0);
        PRECOND(params->stat_values != NULL, return 0);
        PRECOND(params->stat_count > 0, return 0);

        sys_mutex_lock(stats_mutex, -1);

        struct StatData * const old_stats = stats;
        const int old_num_stats = num_stats;

        stats = mem_alloc(sizeof(*stats) * params->stat_count, 0, 0);
        if (!stats) {
            DLOG("No memory for %d statistic entries", params->stat_count);
            mem_free(stats);
            stats = old_stats;
            num_stats = old_num_stats;
            sys_mutex_unlock(stats_mutex);
            return 0;
        }
        num_stats = params->stat_count;

        for (int i = 0; i < params->stat_count; i++) {
            stats[i].info = params->stat_info[i];
            stats[i].value = 0;
            for (int j = 0; j < old_num_stats; j++) {
                if (stats[i].info.id == old_stats[j].info.id) {
                    stats[i].value = old_stats[j].value;
                }
            }
            params->stat_values[i] = stats[i].value;
        }  // for (int i = 0; i < params->stat_count; i++)

        mem_free(old_stats);
        sys_mutex_unlock(stats_mutex);
        return 1;
      }

      case SYS_USERDATA_SAVE_STATS: {
        PRECOND(stats != NULL, return 0);
        PRECOND(params->stat_info != NULL, return 0);
        PRECOND(params->stat_values != NULL, return 0);
        PRECOND(params->stat_updated != NULL, return 0);
        PRECOND(params->stat_count == num_stats, return 0);

        sys_mutex_lock(stats_mutex, -1);
        for (int i = 0; i < params->stat_count; i++) {
            ASSERT(params->stat_info[i].id == stats[i].info.id);
            if (params->stat_updated[i]) {
                ASSERT(!isnan(params->stat_values[i]));
                stats[i].value = params->stat_values[i];
            }
        }
        sys_mutex_unlock(stats_mutex);

        return 1;
      }

      case SYS_USERDATA_CLEAR_STATS: {
        sys_mutex_lock(stats_mutex, -1);
        for (int i = 0; i < params->stat_count; i++) {
            ASSERT(params->stat_info[i].id == stats[i].info.id);
            stats[i].value = 0;
        }
        sys_mutex_unlock(stats_mutex);
        return 1;
      }

    }  // switch (params->operation)

    DLOG("Invalid operation code %d", params->operation);  // NOTREACHED
    ASSERT(0, return 0);  // Abort to help catch errors.  NOTREACHED
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

const void *sys_test_userdata_get_screenshot(int *width_ret, int *height_ret)
{
    PRECOND(width_ret != NULL, return NULL);
    PRECOND(height_ret != NULL, return NULL);

    *width_ret = screenshot_width;
    *height_ret = screenshot_height;
    return screenshot_image;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int store_datafile(const char *path, void *data, int size)
{
    unsigned int i;
    for (i = 0; i < num_data_files; i++) {
        if (stricmp(data_files[i].path, path) == 0) {
            break;
        }
    }
    if (i == num_data_files) {
        char *path_copy = mem_strdup(path, 0);
        if (!path_copy) {
            DLOG("No memory for copy of path (%s)", path);
            return 0;
        }
        void *data_files_new = mem_realloc(
            data_files, sizeof(*data_files) * (num_data_files+1), 0);
        if (!data_files_new) {
            DLOG("No memory to extend data file array");
            mem_free(path_copy);
            return 0;
        }
        data_files = data_files_new;
        data_files[i].path = path_copy;
        data_files[i].data = NULL;
        num_data_files++;
    }

    mem_free(data_files[i].data);
    data_files[i].data = data;
    data_files[i].size = size;
    return 1;
}

/*-----------------------------------------------------------------------*/

static const void *get_datafile(const char *path, int *size_ret)
{
    for (unsigned int i = 0; i < num_data_files; i++) {
        if (stricmp(data_files[i].path, path) == 0) {
            *size_ret = data_files[i].size;
            return data_files[i].data;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static void remove_datafile(const char *path)
{
    for (unsigned int i = 0; i < num_data_files; i++) {
        if (stricmp(data_files[i].path, path) == 0) {
            mem_free(data_files[i].data);
            mem_free(data_files[i].path);
            num_data_files--;
            memmove(&data_files[i], &data_files[i+1],
                    (num_data_files - i) * sizeof(*data_files));
            break;
        }
    }
}

/*************************************************************************/
/*************************************************************************/
