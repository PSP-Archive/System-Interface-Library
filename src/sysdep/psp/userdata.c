/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/userdata.c: PSP user data manipulation interface.
 */

/*
 * All user data on the PSP is saved through the firmware's save data
 * utility (the sceUtilitySavedata*() system calls).  If a save file image
 * is not provided by the caller, save operations will attempt to open the
 * resource ICON0.PNG and use its data as the save data icon.
 *
 * The default pathnames for this implementation are as follows:
 *
 *    - Save files: <program-name>_NNN/save.bin
 *         (NNN is the save number, zero-padded to 3 digits)
 *    - Settings file: <program-name>_Settings/settings.bin
 *    - Per-user statistics file: <program-name>_Stats/stats.bin
 *
 * userdata_get_data_path() is not supported.
 *
 * Note that the program name must begin with a 9-character game ID, in
 * the form "GAME12345" (four uppercase letters followed by five digits).
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/resource.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/quickpng.h"
#include "src/userdata.h"

/*************************************************************************/
/********************* Configuration option defaults *********************/
/*************************************************************************/

#ifndef SIL_PLATFORM_PSP_MAX_USERDATA_FILE_SIZE
# define SIL_PLATFORM_PSP_MAX_USERDATA_FILE_SIZE        100000
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_SAVEFILE_DIR_FORMAT
# define SIL_PLATFORM_PSP_USERDATA_SAVEFILE_DIR_FORMAT  "%s_%03d"
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_SAVEFILE_FILENAME
# define SIL_PLATFORM_PSP_USERDATA_SAVEFILE_FILENAME    "save.bin"
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_SETTINGS_DIR_FORMAT
# define SIL_PLATFORM_PSP_USERDATA_SETTINGS_DIR_FORMAT  "%s_Settings"
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_SETTINGS_FILENAME
# define SIL_PLATFORM_PSP_USERDATA_SETTINGS_FILENAME    "settings.bin"
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_STATS_DIR_FORMAT
# define SIL_PLATFORM_PSP_USERDATA_STATS_DIR_FORMAT     "%s_Stats"
#endif
#ifndef SIL_PLATFORM_PSP_USERDATA_STATS_FILENAME
# define SIL_PLATFORM_PSP_USERDATA_STATS_FILENAME       "stats.bin"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Short names for the path format strings and filenames. */
#define PATH_SAVEFILE_DIR_FMT  SIL_PLATFORM_PSP_USERDATA_SAVEFILE_DIR_FORMAT
#define PATH_SAVEFILE_FILE     SIL_PLATFORM_PSP_USERDATA_SAVEFILE_FILENAME
#define PATH_SETTINGS_DIR_FMT  SIL_PLATFORM_PSP_USERDATA_SETTINGS_DIR_FORMAT
#define PATH_SETTINGS_FILE     SIL_PLATFORM_PSP_USERDATA_SETTINGS_FILENAME
#define PATH_STATS_DIR_FMT     SIL_PLATFORM_PSP_USERDATA_STATS_DIR_FORMAT
#define PATH_STATS_FILE        SIL_PLATFORM_PSP_USERDATA_STATS_FILENAME

/* Buffer size for reading in ICON0.PNG. */
#define ICON0_BUFSIZE  45000

/* PSP-specific data for save/load operations. */
struct SysUserDataParamsPrivate {
    /* Has this operation completed (successfully or otherwise)? */
    uint8_t finished;

    /* Local data buffer which should be freed on cleanup. */
    void *local_buffer;

    /* Parameter block passed to the system. */
    SceUtilitySavedataParam sys_params;

    /* Resource manager, ID, and sync mark for creating/loading ICON0.PNG. */
    ResourceManager *icon0_resmgr;
    int icon0_resid;
    int icon0_mark;
};

/*-----------------------------------------------------------------------*/

/* Base priority for savedata utility threads (either THREADPRI_UTILITY_BASE
 * or THREADPRI_UTILITY_LOW). */
static int base_priority = THREADPRI_UTILITY_BASE;

/* Mutex controlling access to the OS's save data utility library. */
static SysMutexID savedata_utility_mutex;

/* Title and descriptive text for statistics file. */
static char stats_title[sizeof(((PspUtilitySavedataSFOParam *)NULL)->savedataTitle)];
static char stats_desc[sizeof(((PspUtilitySavedataSFOParam *)NULL)->detail)];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/*-------- Operation-specific handling --------*/

/**
 * start_operation:  Start the save data operation specified by the
 * parameter block.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 * [Return value]
 *     True if the operation was sucessfully started, false if not.
 */
static int start_operation(SysUserDataParams *params);

/**
 * do_scan_savefiles:  Perform a SCAN_SAVEFILES operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 */
static int do_scan_savefiles(SysUserDataParams *params);

/**
 * finish_stats_load:  Complete a LOAD_STATS operation after the data has
 * been successfully loaded.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 */
static void finish_stats_load(SysUserDataParams *params);

/**
 * do_stats_save:  Perform a SAVE_STATS operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 */
static int do_stats_save(SysUserDataParams *params);

/**
 * do_stats_clear:  Perform a CLEAR_STATS operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 */
static int do_stats_clear(SysUserDataParams *params);

/*-------- Save data utility interface --------*/

/**
 * init_save_params:  Initialize the system parameter block for the given
 * operation.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 * [Return value]
 *     True on success, false on invalid parameters.
 */
static int init_save_params(SysUserDataParams *params);

/**
 * start_savedata_utility:  Start a save data utility operation for the
 * given parameter block.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 * [Return value]
 *     True if the operation was successfully started, false if an error
 *     occurred.
 */
static int start_savedata_utility(SysUserDataParams *params);

/**
 * poll_savedata_utility:  Check whether the save data utility call for
 * the given operation has completed, and perform appropriate finalization
 * if so.
 *
 * [Parameters]
 *     params: Parameter block for operation.
 *     result_ret: Pointer to variable to receive the operation result.
 * [Return value]
 *     True if the operation has completed or the parameter block is
 *     invalid, false if the operation is in progress.
 */
static int poll_savedata_utility(SysUserDataParams *params, int *result_ret);

/**
 * unpack_icon0:  Extract the image data from an ICON0.PNG image stored
 * with a PSP save file, and return it in RGBA format stored in a buffer
 * allocated with mem_alloc().
 *
 * [Parameters]
 *     icon0: Pointer to ICON0.PNG data.
 *     icon0_size: Size of ICON0.PNG data, in bytes.
 * [Return value]
 *     Pixel data, or NULL on error.
 */
static uint8_t *unpack_icon0(const uint8_t *icon0, uint32_t icon0_size);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_userdata_init(void)
{
    savedata_utility_mutex = sys_mutex_create(0, 0);
    if (!savedata_utility_mutex) {
        DLOG("Failed to create mutex for savedata utility");
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_userdata_cleanup(void)
{
    sys_mutex_destroy(savedata_utility_mutex);
    savedata_utility_mutex = 0;
}

/*-----------------------------------------------------------------------*/

char *sys_userdata_get_data_path(UNUSED const char *program_name)
{
    return NULL;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_userdata_perform(SysUserDataParams *params)
{
    /* We accept NULL as a special case: exit() calls us with NULL to
     * allow the current operation, if any, to complete. */
    if (!params) {
        /* Block until the pending operation (if any) completes. */
        sys_mutex_lock(savedata_utility_mutex, -1);
        /* Leave the mutex locked so nothing else can start. */
        return 0;
    }

    /* SCAN_SAVEFILES is handled specially, without going through the
     * savedata utility. */

    if (params->operation == SYS_USERDATA_SCAN_SAVEFILES) {
        return do_scan_savefiles(params);
    }

    /* Initialize the private data structure for this operation. */

    SysUserDataParamsPrivate private;
    params->private = &private;
    params->private->finished = 0;
    params->private->local_buffer = NULL;
    mem_clear(&params->private->icon0_resmgr,
              sizeof(params->private->icon0_resmgr));
    if (!(params->private->icon0_resmgr = resource_create(1))) {
        DLOG("resource_prepare(1) failed for ICON0.PNG");
        goto fail_clear_private;
    }
    params->private->icon0_resid = 0;

    /* Check parameters and set up the system parameter block. */

    if (!init_save_params(params)) {
        goto fail_delete_resmgr;
    }

    /* If this is a save operation and no image was provided, try to
     * load a default ICON0.PNG and use it instead. */

    if ((params->operation == SYS_USERDATA_SAVE_SAVEFILE && !params->save_image)
     || params->operation == SYS_USERDATA_SAVE_SETTINGS
     || params->operation == SYS_USERDATA_SAVE_STATS) {
        params->private->icon0_resid =
            resource_load_data(params->private->icon0_resmgr,
                               "ICON0.PNG", 0, RES_ALLOC_TEMP);
        if (!params->private->icon0_resid) {
            DLOG("resource_load_data() failed for ICON0.PNG");
        } else {
            resource_wait(params->private->icon0_resmgr,
                          resource_mark(params->private->icon0_resmgr));
            int size;
            void *data = resource_get_data(params->private->icon0_resmgr,
                                           params->private->icon0_resid, &size);
            if (!data) {
                DLOG("Failed to load ICON0.PNG, save file will have no icon");
            } else {
                params->private->sys_params.icon0FileData.buf     = data;
                params->private->sys_params.icon0FileData.bufSize = size;
                params->private->sys_params.icon0FileData.size    = size;
            }
        }
    }

    /* Perform the I/O operation. */

    int result;
    if (!start_operation(params)) {
        result = 0;
    } else {
        ASSERT(!params->private->finished);
        while (!poll_savedata_utility(params, &result)) {
            sceDisplayWaitVblankStart();
        }
    }

    /* Free local resources and return. */

    resource_destroy(params->private->icon0_resmgr);
    params->private->icon0_resmgr = NULL;
    mem_free(params->private->local_buffer);
    params->private = NULL;
    return result;

    /* Error handling. */

  fail_delete_resmgr:
    resource_destroy(params->private->icon0_resmgr);
    params->private->icon0_resmgr = NULL;
  fail_clear_private:
    params->private = NULL;
    return 0;
}

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

void psp_userdata_set_low_priority_mode(int on)
{
    base_priority = on ? THREADPRI_UTILITY_LOW : THREADPRI_UTILITY_BASE;
}

/*-----------------------------------------------------------------------*/

void psp_userdata_set_stats_file_info(const char *title, const char *desc)
{
    if (!strformat_check(stats_title, sizeof(stats_title), "%s", title)) {
        DLOG("WARNING: stats file title truncated");
    }
    if (!strformat_check(stats_desc, sizeof(stats_desc), "%s", desc)) {
        DLOG("WARNING: stats file description truncated");
    }
}

/*************************************************************************/
/************** Local routines: Operation-specific handling **************/
/*************************************************************************/

static int start_operation(SysUserDataParams *params)
{
    switch (params->operation) {
      case SYS_USERDATA_SAVE_SAVEFILE:
      case SYS_USERDATA_LOAD_SAVEFILE:
      case SYS_USERDATA_SAVE_SETTINGS:
      case SYS_USERDATA_LOAD_SETTINGS:
      case SYS_USERDATA_LOAD_STATS:
        return start_savedata_utility(params);

      case SYS_USERDATA_DELETE_SAVEFILE:
      case SYS_USERDATA_SCAN_SAVEFILES:
      case SYS_USERDATA_SAVE_SCREENSHOT:
      case SYS_USERDATA_SAVE_DATA:
      case SYS_USERDATA_LOAD_DATA:
      case SYS_USERDATA_DELETE_DATA:
        /* We should never get here, because init_save_params() will fail
         * for these operations. */
        ASSERT(!"impossible", return 0);  // NOTREACHED

      case SYS_USERDATA_SAVE_STATS:
        return do_stats_save(params);

      case SYS_USERDATA_CLEAR_STATS:
        return do_stats_clear(params);
    }

    ASSERT(!"Operation code was invalid.", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static int do_scan_savefiles(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);
    PRECOND(params->scan_buffer != NULL, return 0);
    PRECOND(params->scan_count > 0, return 0);

    int dir = sceIoDopen("ms0:/PSP/SAVEDATA");
    if (UNLIKELY(dir < 0)) {
        /* This directory should always exist, even on a newly-formatted
         * Memory Stick.  If we can't access it, assume something has gone
         * horribly wrong and return failure. */
        DLOG("Failed to open ms0:/PSP/SAVEDATA: %s", psp_strerror(dir));
        return 0;
    }

    const size_t program_name_len = strlen(params->program_name);
    mem_clear(params->scan_buffer, params->scan_count);

    struct SceIoDirent dirent;
    mem_clear(&dirent, sizeof(dirent));
    while (sceIoDread(dir, &dirent) > 0) {
        if (LIKELY(FIO_S_ISDIR(dirent.d_stat.st_mode))
         && strncmp(dirent.d_name, params->program_name, program_name_len) == 0
         && dirent.d_name[program_name_len] == '_'
         && dirent.d_name[program_name_len + 1
                          + strspn(dirent.d_name + program_name_len + 1,
                                   "0123456789")] == 0)
        {
            const int num = (int)strtol(dirent.d_name + program_name_len + 1,
                                        NULL, 10);
            if (num >= params->savefile_num) {
                const int offset = num - params->savefile_num;
                if (offset < params->scan_count) {
                    params->scan_buffer[offset] = 1;
                }
            }
        }
    }

    sceIoDclose(dir);
    return 1;
}

/*-----------------------------------------------------------------------*/

static void finish_stats_load(SysUserDataParams *params)
{
    unsigned char *data = params->private->sys_params.dataBuf;
    unsigned char * const top = data + params->private->sys_params.dataSize;

    /* Set things up so the buffer is automatically freed when we're done. */
    params->private->local_buffer = params->load_data;
    params->load_data = NULL;

    /* Set default values for all stats. */
    for (int i = 0; i < params->stat_count; i++) {
        params->stat_values[i] = 0;
    }

    /* Parse the loaded data. */
    for (int i = 0; i < params->stat_count; i++) {
        switch (params->stat_info[i].type) {

          case USERDATA_STAT_FLAG:
            if (data+1 > top) {
                DLOG("Missing data in statistics file");
                return;
            }
            if (*data != 0 && *data != 1) {
                DLOG("Invalid data in statistics file (ID %u)",
                     params->stat_info[i].id);
            } else {
                params->stat_values[i] = *data++;
            }
            break;

          case USERDATA_STAT_UINT32:
          case USERDATA_STAT_UINT32_MAX:
            if (data+4 > top) {
                DLOG("Missing data in statistics file");
                return;
            }
            /* IMPORTANT: The uint32_t cast on the first byte is required!
             * Without it, uint8_t gets promoted to (signed) int, so if
             * the high bit is set, the 32-bit value will be treated as a
             * negative number (technically, the result is implementation-
             * defined).  We cast the rest of the bytes as well for
             * parallelism. */
            params->stat_values[i] = (uint32_t)data[0]<<24
                                   | (uint32_t)data[1]<<16
                                   | (uint32_t)data[2]<< 8
                                   | (uint32_t)data[3]<< 0;
            data += 4;
            break;

          case USERDATA_STAT_DOUBLE:
          case USERDATA_STAT_DOUBLE_MAX:
            if (data+8 > top) {
                DLOG("Missing data in statistics file");
                return;
            }
            {
                union {uint64_t i; double d;} u;
                u.i = (uint64_t)data[0]<<56
                    | (uint64_t)data[1]<<48
                    | (uint64_t)data[2]<<40
                    | (uint64_t)data[3]<<32
                    | (uint64_t)data[4]<<24
                    | (uint64_t)data[5]<<16
                    | (uint64_t)data[6]<< 8
                    | (uint64_t)data[7]<< 0;
                params->stat_values[i] = u.d;
            }
            data += 8;
            break;

        }  // switch (params->stat_info[i].type)
    }  // for (int i = 0; i < params->stat_count; i++)
}

/*-----------------------------------------------------------------------*/

static int do_stats_save(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);

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

    /* Start up the save data utility. */
    params->private->local_buffer = save_buffer;
    params->private->sys_params.dataBuf = save_buffer;
    params->private->sys_params.dataBufSize = save_size;
    params->private->sys_params.dataSize = save_size;
    return start_savedata_utility(params);
}

/*-----------------------------------------------------------------------*/

static int do_stats_clear(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);

    /* There doesn't seem to be a sanctioned way to delete a save file,
     * so write a new file with all-zero data. */
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
    char *save_buffer =
        mem_alloc(save_size, 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (!save_buffer) {
        DLOG("Out of memory for statistics data (%d bytes)", save_size);
        return 0;
    }
    params->private->local_buffer = save_buffer;
    params->private->sys_params.dataBuf = save_buffer;
    params->private->sys_params.dataBufSize = save_size;
    params->private->sys_params.dataSize = save_size;
    return start_savedata_utility(params);
}

/*************************************************************************/
/************** Local routines: Save data utility interface **************/
/*************************************************************************/

static int init_save_params(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);
    PRECOND(params->program_name != NULL, return 0);
    PRECOND(params->game_title != NULL, return 0);
    PRECOND(params->private != NULL, return 0);

    SceUtilitySavedataParam *sys_params = &params->private->sys_params;

    /* Check the validity of the program name. */

    if (strspn(params->program_name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 4
     || strspn(params->program_name+4, "0123456789") != 5
     || params->program_name[9] == '\0') {
        DLOG("Invalid program_name: %s", params->program_name);
        return 0;
    }
    const char *game_id = params->program_name;
    const char *program_name = params->program_name + 9;

    /* Initialize basic parameters. */

    mem_clear(sys_params, sizeof(*sys_params));
    sys_params->base.size = sizeof(*sys_params);
    sys_params->base.graphicsThread = base_priority + 1;
    sys_params->base.accessThread   = base_priority + 3;
    sys_params->base.fontThread     = base_priority + 2;
    sys_params->base.soundThread    = base_priority;
    sys_params->overwrite = 1;

    /* Set the default file name. */

    ASSERT(strformat_check(sys_params->gameName, sizeof(sys_params->gameName),
                           "%.9s", game_id));

    switch (params->operation) {

      case SYS_USERDATA_SAVE_SAVEFILE:
      case SYS_USERDATA_LOAD_SAVEFILE:
        if (!strformat_check(sys_params->saveName,
                             sizeof(sys_params->saveName), PATH_SAVEFILE_DIR_FMT,
                             program_name, params->savefile_num)) {
            DLOG("Buffer overflow on save file directory");
            return 0;
        }
        ASSERT(strformat_check(sys_params->fileName,
                               sizeof(sys_params->fileName),
                               "%s", PATH_SAVEFILE_FILE), return 0);
        break;

      case SYS_USERDATA_SAVE_SETTINGS:
      case SYS_USERDATA_LOAD_SETTINGS:
        if (!strformat_check(sys_params->saveName,
                             sizeof(sys_params->saveName),
                             PATH_SETTINGS_DIR_FMT, program_name)) {
            DLOG("Buffer overflow on settings file directory");
            return 0;
        }
        ASSERT(strformat_check(sys_params->fileName,
                               sizeof(sys_params->fileName),
                               "%s", PATH_SETTINGS_FILE), return 0);
        break;

      case SYS_USERDATA_DELETE_SAVEFILE:
        DLOG("DELETE_SAVEFILE not supported");
        return 0;

      case SYS_USERDATA_SCAN_SAVEFILES:
        ASSERT(!"SCAN_SAVEFILES not supported in savedata utility", return 0);  // NOTREACHED

      case SYS_USERDATA_SAVE_SCREENSHOT:
        DLOG("SAVE_SCREENSHOT not supported");
        return 0;

      case SYS_USERDATA_SAVE_DATA:
      case SYS_USERDATA_LOAD_DATA:
      case SYS_USERDATA_DELETE_DATA:
        DLOG("SAVE/LOAD/DELETE_DATA not supported");
        return 0;

      case SYS_USERDATA_LOAD_STATS:
      case SYS_USERDATA_SAVE_STATS:
      case SYS_USERDATA_CLEAR_STATS:
        if (!strformat_check(sys_params->saveName,
                             sizeof(sys_params->saveName),
                             PATH_STATS_DIR_FMT, program_name)) {
            DLOG("Buffer overflow on statistics file directory");
            return 0;
        }
        ASSERT(strformat_check(sys_params->fileName,
                               sizeof(sys_params->fileName),
                               "%s", PATH_STATS_FILE), return 0);
        break;

    }  // switch (params->operation)

    /* Apply any override path, ensuring that the path is valid. */

    if (params->override_path) {

        const char *slash = strchr(params->override_path, '/');
        if (!slash || strchr(slash+1, '/')) {
            DLOG("Bad number of path components in override path: %s",
                 params->override_path);
            return 0;
        }
        if (strspn(params->override_path, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 4
         || strspn(params->override_path+4, "0123456789") != 5
         || params->override_path[9] == '/') {
            DLOG("Invalid game ID in override path: %s",
                 params->override_path);
            return 0;
        }
        memcpy(sys_params->gameName, params->override_path, 9);
        if (!strformat_check(sys_params->saveName,
                             sizeof(sys_params->saveName),
                             "%.*s", (int)(slash - params->override_path - 9),
                             params->override_path + 9)) {
            DLOG("Directory name of override path is too long: %s",
                 params->override_path);
            return 0;
        }
        if (!strformat_check(sys_params->fileName,
                             sizeof(sys_params->fileName), "%s", slash+1)) {
            DLOG("File name of override path is too long: %s",
                 params->override_path);
            return 0;
        }

    }  // if (params->override_path)

    /* Apply other operation-specific parameters. */

    switch (params->operation) {

      case SYS_USERDATA_SAVE_SAVEFILE:
        if (params->save_image) {
            if (params->save_image_width != PSP_SAVE_IMAGE_WIDTH
             || params->save_image_height != PSP_SAVE_IMAGE_HEIGHT) {
                DLOG("Image is wrong size (%dx%d, must be %dx%d), ignoring",
                     params->save_image_width, params->save_image_height,
                     PSP_SAVE_IMAGE_WIDTH, PSP_SAVE_IMAGE_HEIGHT);
            } else {
                /* Save the image uncompressed, both to save time and so
                 * we don't have to rely on libpng/zlib (which use extra
                 * memory) for saving and loading. */
                uint32_t pngsize = quickpng_rgb32_size(
                    PSP_SAVE_IMAGE_WIDTH, PSP_SAVE_IMAGE_HEIGHT, 0);
                params->private->icon0_resid = resource_new_data(
                    params->private->icon0_resmgr, pngsize, 0, RES_ALLOC_TEMP);
                void *pngbuf = resource_get_data(
                    params->private->icon0_resmgr,
                    params->private->icon0_resid, NULL);
                if (UNLIKELY(!pngbuf)) {
                    DLOG("Out of memory for icon generation (%u bytes)",
                         pngsize);
                } else {
                    quickpng_from_rgb32(
                        params->save_image,
                        PSP_SAVE_IMAGE_WIDTH, PSP_SAVE_IMAGE_HEIGHT,
                        PSP_SAVE_IMAGE_WIDTH, pngbuf, pngsize, 0, 0, 0);
                    sys_params->icon0FileData.buf     = pngbuf;
                    sys_params->icon0FileData.bufSize = pngsize;
                    sys_params->icon0FileData.size    = pngsize;
                }
            }
        }
        /* Fall through to common save setup code. */

      case SYS_USERDATA_SAVE_SETTINGS:
        sys_params->mode = PSP_UTILITY_SAVEDATA_AUTOSAVE;
        sys_params->dataBuf = (void *)params->save_data;
        sys_params->dataBufSize = params->save_size;
        sys_params->dataSize = params->save_size;
        if (!strformat_check(sys_params->sfoParam.title,
                             sizeof(sys_params->sfoParam.title),
                             "%s", params->game_title)) {
            DLOG("Buffer overflow on game title (continuing anyway)");
        }
        if (!strformat_check(sys_params->sfoParam.savedataTitle,
                             sizeof(sys_params->sfoParam.savedataTitle),
                             "%s", params->title)) {
            DLOG("Buffer overflow on file title (continuing anyway)");
        }
        if (!strformat_check(sys_params->sfoParam.detail,
                             sizeof(sys_params->sfoParam.detail),
                             "%s", params->desc)) {
            DLOG("Buffer overflow on file description (continuing anyway)");
        }
        break;

      case SYS_USERDATA_SAVE_STATS:
      case SYS_USERDATA_CLEAR_STATS:
        sys_params->mode = PSP_UTILITY_SAVEDATA_AUTOSAVE;
        if (!strformat_check(sys_params->sfoParam.title,
                             sizeof(sys_params->sfoParam.title),
                             "%s", params->game_title)) {
            DLOG("Buffer overflow on game title (continuing anyway)");
        }
        STATIC_ASSERT(sizeof(sys_params->sfoParam.savedataTitle)
                      == sizeof(stats_title), "Bad size for stats_title");
        memcpy(sys_params->sfoParam.savedataTitle, stats_title,
               sizeof(stats_title));
        STATIC_ASSERT(sizeof(sys_params->sfoParam.detail)
                      == sizeof(stats_desc), "Bad size for stats_desc");
        memcpy(sys_params->sfoParam.detail, stats_desc, sizeof(stats_desc));
        /* The data buffer will be set later on. */
        break;

      case SYS_USERDATA_LOAD_SAVEFILE:
      case SYS_USERDATA_LOAD_SETTINGS:
      case SYS_USERDATA_LOAD_STATS:
        sys_params->mode = PSP_UTILITY_SAVEDATA_AUTOLOAD;
        /* There doesn't seem to be any way to request the size of a file
         * through the savedata utility, so we allocate a fixed-size buffer. */
        sys_params->dataBufSize = SIL_PLATFORM_PSP_MAX_USERDATA_FILE_SIZE;
        sys_params->dataBuf =
            mem_alloc(sys_params->dataBufSize, 0, MEM_ALLOC_TEMP);
        if (!sys_params->dataBuf) {
            DLOG("No memory for load buffer (%u bytes)",
                 sys_params->dataBufSize);
            return 0;
        }
        params->private->icon0_resid =
            resource_new_data(params->private->icon0_resmgr,
                              ICON0_BUFSIZE, 0, RES_ALLOC_TEMP);
        sys_params->icon0FileData.buf =
            resource_get_data(params->private->icon0_resmgr,
                              params->private->icon0_resid, NULL);
        if (UNLIKELY(!sys_params->icon0FileData.buf)) {
            DLOG("No memory for icon0!");
        } else {
            sys_params->icon0FileData.bufSize = ICON0_BUFSIZE;
        }
        break;

      case SYS_USERDATA_DELETE_SAVEFILE:
      case SYS_USERDATA_SCAN_SAVEFILES:
      case SYS_USERDATA_SAVE_SCREENSHOT:
      case SYS_USERDATA_SAVE_DATA:
      case SYS_USERDATA_LOAD_DATA:
      case SYS_USERDATA_DELETE_DATA:
        /* Unreachable, but included to avoid a compiler warning. */
        break;  // NOTREACHED

    }  // switch (params->operation)

    return 1;
}

/*-----------------------------------------------------------------------*/

static int start_savedata_utility(SysUserDataParams *params)
{
    PRECOND(params != NULL, return 0);
    PRECOND(params->private != NULL, return 0);

    sys_mutex_lock(savedata_utility_mutex, -1);

    int res = sceUtilitySavedataInitStart(&params->private->sys_params);
    if (UNLIKELY(res < 0)) {
        DLOG("sceUtilitySavedataInitStart(): %s", psp_strerror(res));
        sys_mutex_unlock(savedata_utility_mutex);
        return 0;
    }

    params->private->finished = 0;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int poll_savedata_utility(SysUserDataParams *params, int *result_ret)
{
    PRECOND(params != NULL, *result_ret = 0; return 1);
    PRECOND(params->private != NULL, *result_ret = 0; return 1);
    PRECOND(!params->private->finished, *result_ret = 0; return 1);

    SceUtilitySavedataParam *sys_params = &params->private->sys_params;

    int res = sceUtilitySavedataGetStatus();
    switch (res) {
      case 1:
        return 0;
      case 2:
        sceUtilitySavedataUpdate(1);
        return 0;
      case 3:
        sceUtilitySavedataShutdownStart();
        return 0;
      case 0:
        res = sys_params->base.result;
        break;
    }

    if (sys_params->mode == PSP_UTILITY_SAVEDATA_AUTOLOAD) {

        if (res < 0) {
            if ((uint32_t)res != PSP_SAVEDATA_LOAD_NOT_FOUND) {
                DLOG("Save file read failed for %s%s: %s",
                     sys_params->gameName, sys_params->saveName,
                     psp_strerror(res));
            }
            if (params->operation == SYS_USERDATA_LOAD_STATS
             && (uint32_t)res == PSP_SAVEDATA_LOAD_NOT_FOUND) {
                /* Not an error -- just use default values. */
                for (int i = 0; i < params->stat_count; i++) {
                    params->stat_values[i] = 0;
                }
                *result_ret = 1;
            } else {
                *result_ret = 0;
            }
            mem_free(sys_params->dataBuf);
        } else {
            if (params->operation == SYS_USERDATA_LOAD_STATS) {
                finish_stats_load(params);
                mem_free(sys_params->dataBuf);
            } else {
                params->load_data = sys_params->dataBuf;
                params->load_size = sys_params->dataSize;
                if (sys_params->icon0FileData.buf
                 && sys_params->icon0FileData.size > 0) {
                    params->load_image =
                        unpack_icon0(sys_params->icon0FileData.buf,
                                     sys_params->icon0FileData.size);
                    params->load_image_width  = PSP_SAVE_IMAGE_WIDTH;
                    params->load_image_height = PSP_SAVE_IMAGE_HEIGHT;
                }
            }
            *result_ret = 1;
        }

    } else {  // PSP_UTILITY_SAVEDATA_AUTOSAVE

        if (res < 0) {
            DLOG("Save file write failed for %s%s: %s", sys_params->gameName,
                 sys_params->saveName, psp_strerror(res));
            *result_ret = 0;
        } else {
            *result_ret = 1;
        }

    }

    /* Apparently the savedata utility can get confused if we start a new
     * operation immediately after the old one finished, so insert a short
     * wait before releasing the mutex. */
    sceKernelDelayThread(25000);  // 25 msec

    sys_mutex_unlock(savedata_utility_mutex);
    params->private->finished = 1;
    mem_clear(&params->private->sys_params,
              sizeof(params->private->sys_params));

    return 1;
}

/*-----------------------------------------------------------------------*/

static uint8_t *unpack_icon0(const uint8_t *icon0, uint32_t icon0_size)
{
    PRECOND(icon0 != NULL, goto error_return);
    const uint8_t * const icon0_top = icon0 + icon0_size;

    uint32_t *pixels;

    /* Verify the PNG header and image size. */

    if (UNLIKELY(icon0_size < 33+12)
     || UNLIKELY(memcmp(icon0,
                        "\x89PNG\x0D\x0A\x1A\x0A\0\0\0\x0DIHDR", 16) != 0))
    {
        DLOG("Invalid PNG format");
        goto error_return;
    }
    if (UNLIKELY(memcmp(icon0+24, "\x08\x02\x00\x00\x00", 5) != 0)) {
        DLOG("Unsupported image format");
        goto error_return;
    }
    int32_t width  = icon0[16]<<24 | icon0[17]<<16 | icon0[18]<<8 | icon0[19];
    int32_t height = icon0[20]<<24 | icon0[21]<<16 | icon0[22]<<8 | icon0[23];
    if (UNLIKELY(width  != PSP_SAVE_IMAGE_WIDTH)
     || UNLIKELY(height != PSP_SAVE_IMAGE_HEIGHT)) {
        DLOG("Invalid width/height %dx%d", width, height);
        goto error_return;
    }

    /* Look for the image data inside the PNG file. */

    icon0 += 33;
    while (memcmp(icon0+4, "IDAT", 4) != 0) {
        const uint32_t chunksize =
            icon0[0]<<24 | icon0[1]<<16 | icon0[2]<<8 | icon0[3];
        if (chunksize > (uint32_t)(icon0_top - (icon0+12))) {
            DLOG("IDAT chunk not found");
            goto error_return;
        }
        icon0 += 12 + chunksize;
        if (UNLIKELY(icon0+12 > icon0_top)) {
            DLOG("IDAT chunk not found");
            goto error_return;
        }
    }
    const uint32_t idat_size =
        icon0[0]<<24 | icon0[1]<<16 | icon0[2]<<8 | icon0[3];
    if (idat_size > (uint32_t)(icon0_top - (icon0+12))) {
        DLOG("Image data truncated");
        goto error_return;
    }
    icon0 += 8;
    if (UNLIKELY(memcmp(icon0, "\x78\x01", 2) != 0)) {
        DLOG("Invalid compression signature 0x%02X 0x%02X",
             icon0[0], icon0[1]);
        goto error_return;
    }
    icon0 += 2;

    /* Allocate a buffer for the pixel data to be returned. */

    pixels = mem_alloc(width * height * 4, 64, MEM_ALLOC_TEMP);
    if (UNLIKELY(!pixels)) {
        DLOG("Failed to allocate %dx%d pixels", width, height);
        goto error_return;
    }

    /* Store the image data in the pixel buffer. */

    for (int y = 0; y < height; y++) {
        if (UNLIKELY(icon0[0] != (y==height-1 ? 0x01 : 0x00))) {
            DLOG("Row %d: invalid block header 0x%02X", y, icon0[0]);
            goto error_free_pixels;
        }
        if (UNLIKELY((icon0[1] | icon0[2]<<8) != 1+width*3)) {
            DLOG("Row %d: invalid block size %d (should be %d)",
                 y, icon0[1] | icon0[2]<<8, 1+width*3);
            goto error_free_pixels;
        }
        if (UNLIKELY((icon0[3] | icon0[4]<<8) != (uint16_t)~(1+width*3))) {
            DLOG("Row %d: inverted block size is wrong", y);
            goto error_free_pixels;
        }
        if (UNLIKELY(icon0[5] != 0)) {
            DLOG("Row %d: invalid filter type %d", y, icon0[5]);
            goto error_free_pixels;
        }
        icon0 += 6;
        uint32_t *dest = &pixels[y * width];
        for (int x = 0; x < width; x++, icon0 += 3, dest++) {
            *dest = icon0[0] | icon0[1]<<8 | icon0[2]<<16 | 0xFF000000;
        }
    }

    /* Done! */

    return (uint8_t *)pixels;

    /* Error handling. */

  error_free_pixels:
    mem_free(pixels);
  error_return:
    return NULL;
}

/*************************************************************************/
/*************************************************************************/
