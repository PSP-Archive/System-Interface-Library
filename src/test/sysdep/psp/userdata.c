/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/userdata.c: Tests for PSP implementation of the user
 * data access functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/resource.h"
#include "src/resource/package.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/texture.h"
#include "src/thread.h"
#include "src/userdata.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Parameters for the save file we operate on. */
#define SAVE_PROGRAM_NAME   "TEST00000SIL"
#define SAVE_PROGRAM_TITLE  "Userdata Test"
#define SAVE_NUM            1

/* Pathnames corresponding to the parameters above. */
#define PATH_SAVE_DIR_BASE  "ms0:/PSP/SAVEDATA/" SAVE_PROGRAM_NAME
#define PATH_SAVE_DIR       PATH_SAVE_DIR_BASE "_001"
#define PATH_SAVE_FILE      PATH_SAVE_DIR "/save.bin"
#define PATH_SETTINGS_DIR   PATH_SAVE_DIR_BASE "_Settings"
#define PATH_SETTINGS_FILE  PATH_SETTINGS_DIR "/settings.bin"
#define PATH_STATS_DIR      PATH_SAVE_DIR_BASE "_Stats"
#define PATH_STATS_FILE     PATH_STATS_DIR "/stats.bin"

/*************************************************************************/
/********************** Helper macros and functions **********************/
/*************************************************************************/

/**
 * CHECK_USERDATA_MEMORY_FAILURES:  Check that the given userdata
 * operation handles memory allocation failures properly.  Similar to the
 * CHECK_MEMORY_FAILURES() macro, except that this macro waits for the
 * operation to complete.
 *
 * Call like:
 *    CHECK_USERDATA_MEMORY_FAILURES(userdata_save_savefile(...))
 * (the parameter should return an operation ID).
 */
#define CHECK_USERDATA_MEMORY_FAILURES(operation) \
    CHECK_MEMORY_FAILURES( \
        (id = (operation)) && (userdata_wait(id), userdata_get_result(id)))

/*-----------------------------------------------------------------------*/

/**
 * create_savefile_with_image:  Create a save file with an associated image
 * of the given dimensions.
 *
 * [Parameters]
 *     index: Save file index.
 *     width: Width of image to create.
 *     height: Height of image to create.
 * [Return value]
 *     True if the save operation succeeded, false if not.
 */
static int create_savefile_with_image(int index, int width, int height)
{
    int texture;
    ASSERT(texture = texture_create(width, height, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < width*height; i++) {
        const int x = i % width;
        const int y = i / width;
        pixels[i*4+0] = x;
        pixels[i*4+1] = y;
        pixels[i*4+2] = x + y;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);
    int id;
    CHECK_TRUE(id = userdata_save_savefile(
                   index, "111", 3, "title", "desc", texture));
    userdata_wait(id);
    const int result = userdata_get_result(id);
    texture_destroy(texture);
    return result;
}

/*-----------------------------------------------------------------------*/

/**
 * path_exists:  Return whether the given path names an existing file or
 * directory.
 *
 * [Parameters]
 *     path: Pathname to check.
 * [Return value]
 *     True if the path names an existing file or directory, false if not.
 */
static int path_exists(const char *path)
{
    SceIoStat st;
    return sceIoGetstat(path, &st) == 0;
}

/*-----------------------------------------------------------------------*/

/**
 * remove_or_warn:  Attempt to remove the given file or directory, logging
 * a warning if the file or directory exists but cannot be removed.
 *
 * [Parameters]
 *     path: Pathname of file or directory to remove.
 *     is_dir: True if the item is a directory, false if a file.
 */
#define remove_or_warn(path,is_dir) \
    remove_or_warn_((path), (is_dir), __FILE__, __LINE__, __FUNCTION__)
static void remove_or_warn_(const char *path, int is_dir,
                            const char *file, int line, const char *function)
{
    uint32_t error;
    if (is_dir) {
        error = sceIoRmdir(path);
    } else {
        error = sceIoRemove(path);
    }
    if (error != 0 && error != PSP_ENOENT) {
        testlog_log(file, line, function, TESTLOG_WARN,
                    "WARN: Failed to remove%s %s: %s",
                    is_dir ? " directory" : "", path, psp_strerror(error));
    }
}

/*-----------------------------------------------------------------------*/

/**
 * spin_thread:  Thread routine that spins on an empty loop for about 2
 * seconds (at 222MHz) before returning.  Used to test low-priority mode.
 *
 * [Parameters]
 *     param: Thread parameter (unused).
 * [Return value]
 *     0
 */
static int spin_thread(UNUSED void *param)
{
    __asm__ volatile(".set push; .set noreorder; .set noat\n"
                     "li $at, 111000000\n"  // 4 cycles per loop iteration.
                     "0: bnez $at, 0b\n"
                     "addiu $at, $at, -1\n"  // Delay slot.
                     ".set pop"
                     : : : "at");
    return 0;
}

/*************************************************************************/
/******************* ICON0.PNG resource package module *******************/
/*************************************************************************/

/*
 * This module is used to implement a virtual resource at ICON0.PNG (the
 * actual file is located at testdata/psp/ICON0.PNG).
 */

/* Internal data for the package module. */
typedef struct IconPackagePrivate IconPackagePrivate;
struct IconPackagePrivate {
    SysFile *fh;  // Persistent file handle for icon file.
    /* Flag: force a read error on the file?  (Done by indicating a size
     * larger than the actual file in the data returned from file_info().) */
    uint8_t force_read_error;
};

/*-----------------------------------------------------------------------*/

static int icon0_init(PackageModuleInfo *module)
{
    IconPackagePrivate *private;
    ASSERT(private = mem_alloc(sizeof(*private), 0, 0));
    ASSERT(private->fh = sys_file_open("testdata/psp/ICON0.PNG"));
    private->force_read_error = 0;
    module->module_data = private;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void icon0_cleanup(PackageModuleInfo *module)
{
    IconPackagePrivate *private = (IconPackagePrivate *)module->module_data;
    sys_file_close(private->fh);
    mem_free(private);
}

/*-----------------------------------------------------------------------*/

static int icon0_file_info(PackageModuleInfo *module,
                             const char *path, SysFile **file_ret,
                             int64_t *pos_ret, int *len_ret,
                             int *comp_ret, int *size_ret)
{
    IconPackagePrivate *private = (IconPackagePrivate *)module->module_data;

    if (strcmp(path, "ICON0.PNG") != 0) {
        return 0;
    }

    *file_ret = private->fh;
    *pos_ret = 0;
    *len_ret =
        sys_file_size(private->fh) + (private->force_read_error ? 1 : 0);
    *comp_ret = 0;
    *size_ret = *len_ret;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int icon0_decompress(
    UNUSED PackageModuleInfo *module, UNUSED void *state,
    UNUSED const void *in, UNUSED int insize,
    UNUSED void *out, UNUSED int outsize)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

static PackageModuleInfo icon0_module = {
    .prefix     = "",
    .init       = icon0_init,
    .cleanup    = icon0_cleanup,
    .file_info  = icon0_file_info,
    .decompress = icon0_decompress,
};

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_psp_userdata(void);
int test_psp_userdata(void)
{
    /* Make sure the test files we're about to create don't already exist. */
    if (path_exists(PATH_SAVE_DIR)
     || path_exists(PATH_SETTINGS_DIR)
     || path_exists(PATH_STATS_DIR)
     || path_exists(PATH_SAVE_DIR_BASE "Override")) {
        FAIL("Test would overwrite existing save files!  Ensure that the"
             " following directories do not exist:"
             "\n    " PATH_SAVE_DIR
             "\n    " PATH_SETTINGS_DIR
             "\n    " PATH_STATS_DIR
             "\n    " PATH_SAVE_DIR_BASE "Override");
    }

    /* Run the tests. */
    DLOG("Running PSP userdata tests (slow)...");
    CHECK_TRUE(graphics_init());
    resource_init();
    sys_test_userdata_use_live_routines = 1;
    const int result = do_test_psp_userdata();
    sys_test_userdata_use_live_routines = 0;
    resource_cleanup();
    graphics_cleanup();

    /* Remove any leftover save files before returning. */
    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    remove_or_warn(PATH_SETTINGS_FILE, 0);
    remove_or_warn(PATH_SETTINGS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SETTINGS_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SETTINGS_DIR, 1);
    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_STATS_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override/FILE.DAT", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override", 1);

    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_psp_userdata)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    CHECK_TRUE(thread_init());
    CHECK_TRUE(userdata_init());
    userdata_set_program_name(SAVE_PROGRAM_NAME);
    userdata_set_program_title(SAVE_PROGRAM_TITLE);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    userdata_cleanup();
    thread_cleanup();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/**************************** Save data tests ****************************/
/*************************************************************************/

TEST(test_savefile)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "save_title", "Save Desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    /* We should not have an ICON0.PNG because we didn't pass in an image
     * and we don't have a default ICON0.PNG resource. */
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/PARAM.SFO", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    char buf[8192];
    int nread = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    CHECK_INTRANGE(nread, 0, sizeof(buf)-1);
    buf[nread] = '\0';
    int found_game = 0, found_title = 0, found_desc = 0;
    for (int i = 0; i < nread; i++) {
        found_game  |= (strcmp(&buf[i], SAVE_PROGRAM_TITLE) == 0);
        found_title |= (strcmp(&buf[i], "save_title") == 0);
        found_desc  |= (strcmp(&buf[i], "Save Desc") == 0);
    }
    CHECK_TRUE(found_game);
    CHECK_TRUE(found_title);
    CHECK_TRUE(found_desc);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_overlength_text)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    char game[sizeof(((PspUtilitySavedataSFOParam *)NULL)->title)+1];
    memset(game, '0', sizeof(game));
    memcpy(game, "Game Name", 9);
    game[sizeof(game)-2] = '1';
    game[sizeof(game)-1] = '\0';
    char title[sizeof(((PspUtilitySavedataSFOParam *)NULL)->savedataTitle)+1];
    memset(title, '0', sizeof(title));
    memcpy(title, "save_title", 10);
    title[sizeof(title)-2] = '1';
    title[sizeof(title)-1] = '\0';
    char desc[sizeof(((PspUtilitySavedataSFOParam *)NULL)->detail)+1];
    memset(desc, '0', sizeof(desc));
    memcpy(desc, "Save Desc", 9);
    desc[sizeof(desc)-2] = '1';
    desc[sizeof(desc)-1] = '\0';

    userdata_set_program_title(game);
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, title, desc, 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/PARAM.SFO", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    char buf[8192];
    int nread = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    CHECK_INTRANGE(nread, 0, sizeof(buf)-1);
    buf[nread] = '\0';
    int found_game = 0, found_title = 0, found_desc = 0;
    for (int i = 0; i < nread; i++) {
        if (strncmp(&buf[i], "Game Name", 9) == 0) {
            CHECK_INTEQUAL(buf[i+9], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_game = 1;
        }
        if (strncmp(&buf[i], "save_title", 10) == 0) {
            CHECK_INTEQUAL(buf[i+10], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_title = 1;
        }
        if (strncmp(&buf[i], "Save Desc", 9) == 0) {
            CHECK_INTEQUAL(buf[i+9], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_desc = 1;
        }
    }
    CHECK_TRUE(found_game);
    CHECK_TRUE(found_title);
    CHECK_TRUE(found_desc);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_write_error)
{
    int id;

    int fd = sceIoOpen(PATH_SAVE_DIR, PSP_O_WRONLY | PSP_O_CREAT, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoWrite(fd, "foo", 3), 3);
    /* Hold the file open so it can't be removed. */

    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    sceIoClose(fd);
    remove_or_warn(PATH_SAVE_DIR, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_missing)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM+1, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_corrupt)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_INTEQUAL(sceIoRemove(PATH_SAVE_DIR "/PARAM.SFO"), 0);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_directory_buffer_overflow)
{
    int id;

    /* Maximum length for the directory component is 19 bytes, excluding
     * the game ID. */
    userdata_set_program_name("TEST00000ABCDEFGHIJ1234567890");
    userdata_set_program_title("foo");
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Identical to the same-named test in src/test/userdata.c, with the
 * addition of removal of Memory Stick files. */
TEST(test_savefile_scan)
{
    int id;
    uint8_t *data;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(2, "2222", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(3, "33333", 5, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(0, 4, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_TRUE(data[0]);
    CHECK_FALSE(data[1]);
    CHECK_TRUE(data[2]);
    CHECK_TRUE(data[3]);
    mem_free(data);

    data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(1, 2, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_FALSE(data[0]);
    CHECK_TRUE(data[1]);
    mem_free(data);

    remove_or_warn(PATH_SAVE_DIR_BASE "_000/save.bin", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_000/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_000/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_000", 1);
    remove_or_warn(PATH_SAVE_DIR_BASE "_002/save.bin", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_002/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_002/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_002", 1);
    remove_or_warn(PATH_SAVE_DIR_BASE "_003/save.bin", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_003/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_003/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "_003", 1);
    return 1;
}

/*************************************************************************/
/************************* Save data image tests *************************/
/*************************************************************************/

TEST(test_savefile_image)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_TRUE(image);
    CHECK_INTEQUAL(texture_width(image), 144);
    CHECK_INTEQUAL(texture_height(image), 80);
    CHECK_FLOATEQUAL(texture_scale(image), 1);
    const uint8_t *image_pixels;
    CHECK_TRUE(image_pixels = (uint8_t *)texture_lock_readonly(image));
    for (int i = 0; i < 144*80; i++) {
        const int x = i%144;
        const int y = i/144;
        CHECK_INTEQUAL(image_pixels[i*4+0], x);
        CHECK_INTEQUAL(image_pixels[i*4+1], y);
        CHECK_INTEQUAL(image_pixels[i*4+2], x + y);
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(data);
    texture_destroy(image);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_image_wrong_size)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 143, 80));
    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));
    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 79));
    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));
    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_image_memory_failure)
{
    int texture;
    ASSERT(texture = texture_create(144, 80, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 144*80; i++) {
        const int x = i%144;
        const int y = i/144;
        pixels[i*4+0] = x;
        pixels[i*4+1] = y;
        pixels[i*4+2] = x + y;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_save_savefile(SAVE_NUM, "111", 3, "title", "desc", texture));
    texture_destroy(texture);

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);  // Should be missing due to memory allocation failure.
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_default_icon)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(resource_register_package(&icon0_module));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    resource_unregister_package(&icon0_module);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    /* We should get no image back even though we have a default icon
     * because the PSP userdata code only parses PNGs that it generated. */
    CHECK_FALSE(image);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    /* We should now get an ICON0.PNG identical to our test icon. */
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    void *expected_data;
    int expected_size;
    ResourceManager *resmgr = resource_create(2);
    int resid;
    ASSERT(resid = resource_load_data(resmgr, "testdata/psp/ICON0.PNG", 0, 0));
    resource_wait(resmgr, resource_mark(resmgr));
    ASSERT(data = resource_get_data(resmgr, resid, &expected_size));
    ASSERT(expected_data = mem_alloc(expected_size, 0, 0));
    memcpy(expected_data, data, expected_size);
    resource_destroy(resmgr);

    void *actual_data;
    int actual_size;
    SysFile *fh;
    ASSERT(fh = sys_file_open(PATH_SAVE_DIR "/ICON0.PNG"));
    actual_size = sys_file_size(fh);
    ASSERT(actual_data = mem_alloc(lbound(actual_size,1), 0, 0));
    ASSERT(sys_file_read(fh, actual_data, actual_size) == actual_size);
    sys_file_close(fh);

    CHECK_INTEQUAL(actual_size, expected_size);
    CHECK_MEMEQUAL(actual_data, expected_data, expected_size);
    mem_free(actual_data);
    mem_free(expected_data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_default_icon_read_error)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(resource_register_package(&icon0_module));
    ((IconPackagePrivate *)(icon0_module.module_data))->force_read_error = 1;
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    resource_unregister_package(&icon0_module);

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_default_icon_memory_failure)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(resource_register_package(&icon0_module));
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_save_savefile(SAVE_NUM, "111", 3, "title", "desc", 0));
    resource_unregister_package(&icon0_module);

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_too_short)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[33];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    sceIoClose(fd);
    fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_WRONLY | PSP_O_TRUNC,
                   0666);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoWrite(fd, buf, sizeof(buf)), sizeof(buf));
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_invalid_header)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_WRONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoWrite(fd, "x", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_wrong_format)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_WRONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoLseek(fd, 28, PSP_SEEK_SET), 28);
    CHECK_INTEQUAL(sceIoWrite(fd, "\1", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_wrong_width)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_WRONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoLseek(fd, 19, PSP_SEEK_SET), 19);
    CHECK_INTEQUAL(sceIoWrite(fd, "\1", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_wrong_height)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_WRONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    CHECK_INTEQUAL(sceIoLseek(fd, 23, PSP_SEEK_SET), 23);
    CHECK_INTEQUAL(sceIoWrite(fd, "\1", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_missing_IDAT)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-4; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs, PSP_SEEK_SET), idat_ofs);
    CHECK_INTEQUAL(sceIoWrite(fd, "J", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_IDAT_scan_past_end_of_file)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-4; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs-4, PSP_SEEK_SET), idat_ofs-4);
    CHECK_INTEQUAL(sceIoWrite(fd, "\x40\0\0\0\0", 5), 5);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_IDAT_scan_pointer_overflow)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-4; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs-4, PSP_SEEK_SET), idat_ofs-4);
    CHECK_INTEQUAL(sceIoWrite(fd, "\xFF\xFF\xFF\xF4\0", 5), 5);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_IDAT_too_long)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-4; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs-4, PSP_SEEK_SET), idat_ofs-4);
    CHECK_INTEQUAL(sceIoWrite(fd, "\x40\0\0\0\0", 4), 4);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_IDAT_pointer_overflow)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-4; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs-4, PSP_SEEK_SET), idat_ofs-4);
    CHECK_INTEQUAL(sceIoWrite(fd, "\xFF\xFF\xFF\xF4", 4), 4);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_bad_compression_signature)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-6; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    ASSERT(buf[idat_ofs+4] == 0x78);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs+4, PSP_SEEK_SET), idat_ofs+4);
    CHECK_INTEQUAL(sceIoWrite(fd, "\0", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_bad_final_block_flag)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-7; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    ASSERT(buf[idat_ofs+6] == 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs+6, PSP_SEEK_SET), idat_ofs+6);
    CHECK_INTEQUAL(sceIoWrite(fd, "\1", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_bad_block_size)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-9; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    ASSERT((buf[idat_ofs+7] | buf[idat_ofs+8]<<8) == 1+144*3);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs+7, PSP_SEEK_SET), idat_ofs+7);
    CHECK_INTEQUAL(sceIoWrite(fd, "\0\0", 2), 2);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_bad_inverted_block_size)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-11; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    ASSERT((buf[idat_ofs+9] | buf[idat_ofs+10]<<8) == (uint16_t)(~(1+144*3)));
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs+9, PSP_SEEK_SET), idat_ofs+9);
    CHECK_INTEQUAL(sceIoWrite(fd, "\0\0", 2), 2);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_unpack_icon0_bad_filter_type)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(create_savefile_with_image(SAVE_NUM, 144, 80));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SAVE_DIR "/ICON0.PNG", PSP_O_RDWR, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    uint8_t buf[4096];
    CHECK_INTEQUAL(sceIoRead(fd, buf, sizeof(buf)), sizeof(buf));
    int idat_ofs = -1;
    for (int i = 0; i <= (int)sizeof(buf)-12; i++) {
        if (memcmp(buf+i, "IDAT", 4) == 0) {
            idat_ofs = i;
            break;
        }
    }
    ASSERT(idat_ofs >= 0);
    ASSERT(buf[idat_ofs+11] == 0);
    CHECK_INTEQUAL(sceIoLseek(fd, idat_ofs+11, PSP_SEEK_SET), idat_ofs+11);
    CHECK_INTEQUAL(sceIoWrite(fd, "\1", 1), 1);
    sceIoClose(fd);

    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR "/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);
    return 1;
}

/*************************************************************************/
/************************** Path override tests **************************/
/*************************************************************************/

TEST(test_override_file_path)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "Override/FILE.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_FALSE(path_exists(PATH_SAVE_DIR));
    CHECK_FALSE(path_exists(PATH_SAVE_FILE));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR_BASE "Override"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR_BASE "Override/FILE.DAT"));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR_BASE "Override/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override/ICON0.PNG"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override/save.bin"));

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "Override/FILE.DAT"));
    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    remove_or_warn(PATH_SAVE_DIR_BASE "Override/FILE.DAT", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override/ICON0.PNG", 0);
    remove_or_warn(PATH_SAVE_DIR_BASE "Override", 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_override_file_path_wrong_format)
{
    int id;

    CHECK_TRUE(userdata_override_file_path(SAVE_PROGRAM_NAME "Override"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override"));

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "Override/FILE.DAT/foo"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_override_file_path_wrong_game_id_format)
{
    int id;

    CHECK_TRUE(userdata_override_file_path("TES_00000SILOverride/FILE.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists("ms0:/PSP/SAVEDATA/TES_00000SILOverride"));

    CHECK_TRUE(userdata_override_file_path("TEST0000XSILOverride/FILE.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists("ms0:/PSP/SAVEDATA/TEST0000XSILOverride"));

    CHECK_TRUE(userdata_override_file_path("TEST00000/FILE.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists("ms0:/PSP/SAVEDATA/TEST00000"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_override_file_path_directory_buffer_overflow)
{
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "Override123456789/FILE.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override123456789"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override12345678"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_override_file_path_filename_buffer_overflow)
{
    int id;

    /* Maximum length for the filename component is 12 bytes. */
    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "Override/123456789.DAT"));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR_BASE "Override"));

    return 1;
}

/*************************************************************************/
/************************** Settings data tests **************************/
/*************************************************************************/

TEST(test_settings)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_settings("2222", 4, "settings_title",
                                           "Settings Desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "2222", 4);
    mem_free(data);

    CHECK_TRUE(path_exists(PATH_SETTINGS_DIR));
    CHECK_TRUE(path_exists(PATH_SETTINGS_FILE));
    CHECK_TRUE(path_exists(PATH_SETTINGS_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SETTINGS_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_SETTINGS_DIR "/PARAM.SFO", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    char buf[8192];
    int nread = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    CHECK_INTRANGE(nread, 0, sizeof(buf)-1);
    buf[nread] = '\0';
    int found_game = 0, found_title = 0, found_desc = 0;
    for (int i = 0; i < nread; i++) {
        found_game  |= (strcmp(&buf[i], SAVE_PROGRAM_TITLE) == 0);
        found_title |= (strcmp(&buf[i], "settings_title") == 0);
        found_desc  |= (strcmp(&buf[i], "Settings Desc") == 0);
    }
    CHECK_TRUE(found_game);
    CHECK_TRUE(found_title);
    CHECK_TRUE(found_desc);

    remove_or_warn(PATH_SETTINGS_FILE, 0);
    remove_or_warn(PATH_SETTINGS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SETTINGS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_directory_buffer_overflow)
{
    int id;

    userdata_set_program_name("TEST00000ABCDEFGHIJ1234567890");
    userdata_set_program_title("foo");
    CHECK_TRUE(id = userdata_save_settings("2222", 4, "settings_title",
                                           "Settings Desc"));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*************************************************************************/
/************************* Statistics data tests *************************/
/*************************************************************************/

/* Identical to the same-named test in src/test/userdata.c, with the
 * addition of presence tests for and removal of Memory Stick files. */
TEST(test_stats)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
        {.id = 3, .type = USERDATA_STAT_DOUBLE},
        {.id = 5, .type = USERDATA_STAT_UINT32_MAX},
        {.id = 7, .type = USERDATA_STAT_DOUBLE_MAX},
    };
    int id;

    /* Register stats, and check that they're all initialized to zero. */
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Set values, and check that they can be read again. */
    CHECK_TRUE(userdata_set_stat(0, -999));  // Flag, should turn into 1.
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that attempts to reset flags or lower _MAX values fail. */
    CHECK_TRUE(userdata_set_stat(0, 0));
    CHECK_TRUE(userdata_set_stat(2, 10));
    CHECK_TRUE(userdata_set_stat(3, 1e5));
    CHECK_TRUE(userdata_set_stat(5, 10));
    CHECK_TRUE(userdata_set_stat(7, 1e5));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that setting a flag that is already set does nothing. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);

    /* Check that updating persistent storage succeeds and does not modify
     * any values. */
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that an update call when no changes have been made returns
     * false. */
    CHECK_FALSE(userdata_update_stats());

    /* Check that non-uint32 values are rejected for UINT32 stats. */
    CHECK_FALSE(userdata_set_stat(2, 123.4));
    CHECK_FALSE(userdata_set_stat(5, 1e10));
    CHECK_FALSE(userdata_set_stat(2, -1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that clear() clears everything, including flags and _MAX stats. */
    CHECK_TRUE(id = userdata_clear_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second clear() does nothing (and doesn't break). */
    CHECK_TRUE(id = userdata_clear_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second register succeeds and preserves values. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    CHECK_TRUE(path_exists(PATH_STATS_DIR));
    CHECK_TRUE(path_exists(PATH_STATS_FILE));
    CHECK_TRUE(path_exists(PATH_STATS_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_STATS_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Identical to the same-named test in src/test/userdata.c, with the
 * addition of presence tests for and removal of Memory Stick files. */
TEST(test_stats_memory_failure)
{
    /* Same as (portions of) test_stats(), except that we bracket all
     * userdata calls with CHECK_USERDATA_MEMORY_FAILURES(). */

    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
        {.id = 3, .type = USERDATA_STAT_DOUBLE},
        {.id = 5, .type = USERDATA_STAT_UINT32_MAX},
        {.id = 7, .type = USERDATA_STAT_DOUBLE_MAX},
    };
    int id;

    /* Register stats, and check that they're all initialized to zero. */
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that memory failures while updating don't cause the updates
     * to be lost from permanent storage. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_update_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check memory failures while clearing. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_clear_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second register succeeds and preserves values. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_update_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    CHECK_TRUE(path_exists(PATH_STATS_DIR));
    CHECK_TRUE(path_exists(PATH_STATS_FILE));
    CHECK_TRUE(path_exists(PATH_STATS_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_STATS_DIR "/ICON0.PNG"));

    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* These tests are identical to the same-named ones in
 * src/test/sysdep/posix/userdata.c, with file access operations modified
 * appropriately for the PSP. */

/*----------------------------------*/

TEST(test_stats_truncated_file_for_flag)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_FLAG},
    };
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "_Stats/stats.bin"));
    CHECK_TRUE(id = userdata_save_savefile(SAVE_NUM, "\0\0\0\x2A", 4,
                                           "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*----------------------------------*/

TEST(test_stats_corrupt_data_for_flag)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 1, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_FLAG},
    };
    struct UserStatValue values_out[] = {
        {.id = 0},
        {.id = 1},
        {.id = 2},
    };
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "_Stats/stats.bin"));
    CHECK_TRUE(id = userdata_save_savefile(SAVE_NUM, "\0\1\x2A", 3,
                                           "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 1);
    CHECK_DOUBLEEQUAL(values_out[2].value, 0);

    return 1;
}

/*----------------------------------*/

TEST(test_stats_truncated_file_for_uint32)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_UINT32},
    };
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "_Stats/stats.bin"));
    CHECK_TRUE(id = userdata_save_savefile(SAVE_NUM, "\0\0\0\x2A", 4,
                                           "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*----------------------------------*/

TEST(test_stats_truncated_file_for_double)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "_Stats/stats.bin"));
    CHECK_TRUE(id = userdata_save_savefile(SAVE_NUM, "\0\0\0\x2A", 4,
                                           "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_corrupt)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
    };
    int id;

    CHECK_TRUE(userdata_override_file_path(
                   SAVE_PROGRAM_NAME "_Stats/stats.bin"));
    CHECK_TRUE(id = userdata_save_savefile(SAVE_NUM, "\0\0\0\x2A", 4,
                                           "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_INTEQUAL(sceIoRemove(PATH_STATS_DIR "/PARAM.SFO"), 0);

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_stats_file_info)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
    };
    int id;

    psp_userdata_set_stats_file_info("stats_title", "Stats Desc");
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(path_exists(PATH_STATS_DIR));
    CHECK_TRUE(path_exists(PATH_STATS_FILE));
    CHECK_TRUE(path_exists(PATH_STATS_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_STATS_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_STATS_DIR "/PARAM.SFO", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    char buf[8192];
    int nread = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    CHECK_INTRANGE(nread, 0, sizeof(buf)-1);
    buf[nread] = '\0';
    int found_game = 0, found_title = 0, found_desc = 0;
    for (int i = 0; i < nread; i++) {
        found_game  |= (strcmp(&buf[i], SAVE_PROGRAM_TITLE) == 0);
        found_title |= (strcmp(&buf[i], "stats_title") == 0);
        found_desc  |= (strcmp(&buf[i], "Stats Desc") == 0);
    }
    CHECK_TRUE(found_game);
    CHECK_TRUE(found_title);
    CHECK_TRUE(found_desc);

    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_overlength_title_desc)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
    };
    int id;

    char game[sizeof(((PspUtilitySavedataSFOParam *)NULL)->title)+1];
    memset(game, '0', sizeof(game));
    memcpy(game, "Game Name", 9);
    game[sizeof(game)-2] = '1';
    game[sizeof(game)-1] = '\0';
    char title[sizeof(((PspUtilitySavedataSFOParam *)NULL)->savedataTitle)+1];
    memset(title, '0', sizeof(title));
    memcpy(title, "stats_title", 11);
    title[sizeof(title)-2] = '1';
    title[sizeof(title)-1] = '\0';
    char desc[sizeof(((PspUtilitySavedataSFOParam *)NULL)->detail)+1];
    memset(desc, '0', sizeof(desc));
    memcpy(desc, "Stats Desc", 10);
    desc[sizeof(desc)-2] = '1';
    desc[sizeof(desc)-1] = '\0';

    userdata_set_program_title(game);
    psp_userdata_set_stats_file_info(title, desc);
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(path_exists(PATH_STATS_DIR));
    CHECK_TRUE(path_exists(PATH_STATS_FILE));
    CHECK_TRUE(path_exists(PATH_STATS_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_STATS_DIR "/ICON0.PNG"));

    int fd = sceIoOpen(PATH_STATS_DIR "/PARAM.SFO", PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);
    char buf[8192];
    int nread = sceIoRead(fd, buf, sizeof(buf)-1);
    sceIoClose(fd);
    CHECK_INTRANGE(nread, 0, sizeof(buf)-1);
    buf[nread] = '\0';
    int found_game = 0, found_title = 0, found_desc = 0;
    for (int i = 0; i < nread; i++) {
        if (strncmp(&buf[i], "Game Name", 9) == 0) {
            CHECK_INTEQUAL(buf[i+9], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_game = 1;
        }
        if (strncmp(&buf[i], "stats_title", 11) == 0) {
            CHECK_INTEQUAL(buf[i+11], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_title = 1;
        }
        if (strncmp(&buf[i], "Stats Desc", 10) == 0) {
            CHECK_INTEQUAL(buf[i+10], '0');
            CHECK_FALSE(strchr(&buf[i], '1'));
            found_desc = 1;
        }
    }
    CHECK_TRUE(found_game);
    CHECK_TRUE(found_title);
    CHECK_TRUE(found_desc);

    remove_or_warn(PATH_STATS_FILE, 0);
    remove_or_warn(PATH_STATS_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_STATS_DIR, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_directory_buffer_overflow)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
    };
    int id;

    userdata_set_program_name("TEST00000ABCDEFGHIJ1234567890");
    userdata_set_program_title("foo");
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*************************************************************************/
/************************** Miscellaneous tests **************************/
/*************************************************************************/

TEST(test_bad_program_name)
{
    int id;

    userdata_set_program_name("TES_00000SIL");
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    userdata_set_program_name("TEST0000XSIL");
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    userdata_set_program_name("TEST00000");
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_unsupported_operations)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_delete_savefile(SAVE_NUM));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR));
    CHECK_TRUE(path_exists(PATH_SAVE_FILE));
    CHECK_TRUE(path_exists(PATH_SAVE_DIR "/PARAM.SFO"));
    CHECK_FALSE(path_exists(PATH_SAVE_DIR "/ICON0.PNG"));
    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);

    ASSERT(image = texture_create(144, 80, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(image));
    for (int i = 0; i < 144*80; i++) {
        const int x = i%144;
        const int y = i/144;
        pixels[i*4+0] = x;
        pixels[i*4+1] = y;
        pixels[i*4+2] = x + y;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(image);
    CHECK_TRUE(id = userdata_save_screenshot(image));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    texture_destroy(image);

    CHECK_TRUE(id = userdata_save_data("foo", "111", 3));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_delete_data("foo"));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_low_priority_mode)
{
    int id;
    void *data;
    uint32_t size;
    int image;
    int thread;

    DLOG("Testing low-priority mode (save should block for 2 seconds)");
    psp_userdata_set_low_priority_mode(1);
    ASSERT(thread = thread_create_with_priority(-1, spin_thread, NULL));
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    sceKernelDelayThread(1800*1000);
    /* The spinning thread should have blocked the savedata utility from
     * proceeding. */
    CHECK_FALSE(userdata_get_status(id));
    thread_wait(thread);
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);
    remove_or_warn(PATH_SAVE_FILE, 0);
    remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
    remove_or_warn(PATH_SAVE_DIR, 1);

    DLOG("Testing high-priority mode (save should happen immediately)");
    psp_userdata_set_low_priority_mode(0);
    CHECK_TRUE(id = userdata_save_savefile(
                   SAVE_NUM, "111", 3, "title", "desc", 0));
    ASSERT(thread = thread_create_with_priority(-1, spin_thread, NULL));
    const uint32_t start = sceKernelGetSystemTimeLow();
    while (!userdata_get_status(id)) {
        if (sceKernelGetSystemTimeLow() - start > 1800*1000) {
            thread_wait(thread);
            userdata_wait(id);
            remove_or_warn(PATH_SAVE_FILE, 0);
            remove_or_warn(PATH_SAVE_DIR "/PARAM.SFO", 0);
            remove_or_warn(PATH_SAVE_DIR, 1);
            FAIL("High-priority userdata operation did not complete within"
                 " 1.8 seconds");
        }
        sceKernelDelayThread(100*1000);
    }
    thread_wait(thread);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(SAVE_NUM, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
