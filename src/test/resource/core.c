/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/resource/core.c: Tests for resource management functionality.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/resource.h"
#include "src/resource/package.h"
#include "src/semaphore.h"
#include "src/sound.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"  // Borrow the graphics test helpers.
#include "src/texture.h"
#include "src/thread.h"
#include "src/utility/log.h"

#if !defined(SIL_PLATFORM_PSP)
/* Pull in the ioqueue test control functions so we can have finer control
 * over when reads occur. */
# define USING_IOQUEUE
# include "src/sysdep/misc/ioqueue.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Resource managers shared by all test routines. */
DEFINE_STATIC_RESOURCEMANAGER(resmgr, 100);
DEFINE_STATIC_RESOURCEMANAGER(resmgr2, 100);
/* This instance has only one resource slot, so the second resource added
 * will cause the internal ResourceInfo buffer to be expanded. */
DEFINE_STATIC_RESOURCEMANAGER(resmgr_single, 1);
/* This instance is invalid and will cause calls to fail. */
static ResourceManager resmgr_uninit_buf, *resmgr_uninit = &resmgr_uninit_buf;

/* Thread pool size to use for background decompression. */
#define THREAD_POOL_SIZE  4

/*-----------------------------------------------------------------------*/

/* Package module definition (functions are at the bottom of the file). */
static int package_init(PackageModuleInfo *module);
static void package_cleanup(PackageModuleInfo *module);
static void package_list_files_start(PackageModuleInfo *module);
static const char *package_list_files_next(PackageModuleInfo *module);
static int package_file_info(PackageModuleInfo *module,
                             const char *path, SysFile **file_ret,
                             int64_t *pos_ret, int *len_ret,
                             int *comp_ret, int *size_ret);
static int package_decompress_get_stack_size(PackageModuleInfo *module);
static void *package_decompress_init(PackageModuleInfo *module);
static int package_decompress(PackageModuleInfo *module, void *state_,
                              const void *in, int insize,
                              void *out, int outsize);
static void package_decompress_finish(PackageModuleInfo *module, void *state);
static PackageModuleInfo package_module = {
    .prefix                    = "testdata/package/",
    .init                      = package_init,
    .cleanup                   = package_cleanup,
    .list_files_start          = package_list_files_start,
    .list_files_next           = package_list_files_next,
    .file_info                 = package_file_info,
    .decompress_get_stack_size = package_decompress_get_stack_size,
    .decompress_init           = package_decompress_init,
    .decompress                = package_decompress,
    .decompress_finish         = package_decompress_finish,
};
/* A second module for testing registration of two modules at once. */
static PackageModuleInfo second_module = {
    .prefix                    = "testdata/second/",
    .init                      = package_init,
    .cleanup                   = package_cleanup,
    .list_files_start          = package_list_files_start,
    .list_files_next           = package_list_files_next,
    .file_info                 = package_file_info,
    .decompress_get_stack_size = package_decompress_get_stack_size,
    .decompress_init           = package_decompress_init,
    .decompress                = package_decompress,
    .decompress_finish         = package_decompress_finish,
};

/* Flag for triggering an error from package_decompress_init(). */
static uint8_t package_error_from_decompress_init = 0;

/* Flag for causing package_decompress() to block, and associated
 * semaphores.  If package_block_decompress is nonzero, package_decompress()
 * will signal package_decompress_blocked_sema on entry, then wait on
 * package_decompress_unblock_sema before proceeding. */
static uint8_t package_block_decompress;
static int package_decompress_blocked_sema;
static int package_decompress_unblock_sema;

/*-----------------------------------------------------------------------*/

/**
 * CHECK_RESOURCE_MEMORY_FAILURES:  Variant of CHECK_MEMORY_FAILURES which
 * sets up the "resmgr_single" resource manager with one full resource slot
 * before evaluating the expression, so we can observe behavior in response
 * to memory failures while trying to expand the resource array.  This macro
 * also calls graphics_flush_resources() on each failure to ensure that any
 * memory used by resources which have been destroyed is actually freed.
 */
#define CHECK_RESOURCE_MEMORY_FAILURES(expr)  do {                      \
    resource_destroy(resmgr_single);                                    \
    for (int _i = 0; ; _i++) {                                          \
        if (_i >= 100) {                                                \
            FAIL("%s did not succeed after 100 iterations", #expr);     \
            break;                                                      \
        }                                                               \
        const int64_t _used_before = mem_debug_bytes_allocated();       \
        ASSERT(resource_strdup(resmgr_single, "test", 0));              \
        TEST_mem_fail_after(_i, 1, 0);                                  \
        if ((expr)) {                                                   \
            TEST_mem_fail_after(-1, 0, 0);                              \
            if (_i == 0) {                                              \
                FAIL("%s did not fail on a memory allocation failure",  \
                     #expr);                                            \
            }                                                           \
            break;                                                      \
        }                                                               \
        TEST_mem_fail_after(-1, 0, 0);                                  \
        resource_destroy(resmgr_single);                                \
        graphics_flush_resources();                                     \
        const int64_t _used_after = mem_debug_bytes_allocated();        \
        if (_used_after > _used_before) {                               \
            testlog_log(                                                \
                __FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,         \
                "FAIL: %s leaked memory on failure for iteration %d"    \
                " (%lld bytes)", #expr, _i+1,                           \
                (long long)(_used_after - _used_before));               \
            mem_debug_report_allocs();                                  \
            DLOG("End of leak report for %s", #expr);                   \
            FAIL_ACTION;                                                \
        }                                                               \
    }                                                                   \
} while (0)

/**
 * CHECK_LOAD_MEMORY_FAILURES:  Variant of CHECK_RESOURCE_MEMORY_FAILURES
 * which allows both a "load" expression and a "get" expression to be
 * specified.  fail_on_shrink indicates whether shrinking reallocate
 * operations should fail (like CHECK_MEMORY_FAILURES_SHRINK) or succeed
 * (like CHECK_MEMORY_FAILURES).
 */
#define CHECK_LOAD_MEMORY_FAILURES(fail_on_shrink,load_expr,get_expr)  do { \
    resource_destroy(resmgr_single);                                    \
    for (int _i = 0; ; _i++) {                                          \
        if (_i >= 100) {                                                \
            FAIL("%s did not succeed after 100 iterations", #load_expr);\
            break;                                                      \
        }                                                               \
        const int64_t _used_before = mem_debug_bytes_allocated();       \
        ASSERT(resource_strdup(resmgr_single, "test", 0));              \
        TEST_mem_fail_after(_i, 1, 0);                                  \
        if ((load_expr)) {                                              \
            int _mark;                                                  \
            ASSERT(_mark = resource_mark(resmgr_single));               \
            resource_wait(resmgr_single, _mark);                        \
            if ((get_expr)) {                                           \
                TEST_mem_fail_after(-1, 0, 0);                          \
                if (_i == 0) {                                          \
                    FAIL("%s did not fail on a memory allocation"       \
                         " failure", #load_expr);                       \
                }                                                       \
                break;                                                  \
            }                                                           \
        }                                                               \
        TEST_mem_fail_after(-1, 0, 0);                                  \
        resource_destroy(resmgr_single);                                \
        graphics_flush_resources();                                     \
        const int64_t _used_after = mem_debug_bytes_allocated();        \
        if (_used_after > _used_before) {                               \
            testlog_log(                                                \
                __FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,         \
                "FAIL: %s leaked memory on failure for iteration %d"    \
                " (%lld bytes)", #load_expr, _i+1,                      \
                (long long)(_used_after - _used_before));               \
            mem_debug_report_allocs();                                  \
            DLOG("End of leak report for %s", #load_expr);              \
            FAIL_ACTION;                                                \
        }                                                               \
    }                                                                   \
} while (0)

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * wrap_sys_file_open:  Call sys_file_open(), converting the given path to
 * an absolute path by prepending the resource path prefix.
 */
static SysFile *wrap_sys_file_open(const char *path)
{
    char abs_path[10000];
    ASSERT(sys_get_resource_path_prefix(abs_path, sizeof(abs_path))
           < (int)sizeof(abs_path));
    ASSERT(strformat_check(abs_path+strlen(abs_path),
                           sizeof(abs_path)-strlen(abs_path), "%s", path));
    return sys_file_open(abs_path);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

/* These have to be run with the graphics engine initialized because we
 * make use of textures. */

static int do_test_resource_core(void);
int test_resource_core(void)
{
    return run_tests_in_window(do_test_resource_core);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_resource_core)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    resource_init();
    package_module.module_data = NULL;
    CHECK_TRUE(resource_register_package(&package_module));

    /* Prime any statically-allocated low-level resources (e.g. ioqueue
     * for POSIX) so CHECK_MEMORY_FAILURES doesn't report spurious leaks. */
    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int req;
    ASSERT(req = sys_file_read_async(fh, buf, 1, 0, -1));
    ASSERT(sys_file_wait_async(req) == 1);
    sys_file_close(fh);

    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();

    TEST_resource_block_load(0);
    TEST_resource_use_silent_sync(0);
    TEST_resource_override_sync_order(0, 0);
    resource_set_background_decompression(0, 0, 0, 0);
    resource_destroy(resmgr);
    resource_destroy(resmgr2);
    resource_destroy(resmgr_single);
    resource_cleanup();
    sound_cleanup();
    graphics_flush_resources();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/********************* Basic resource manager tests **********************/
/*************************************************************************/

TEST(test_manager_basic)
{
    ResourceManager *test_resmgr;
    int resid, resid2;
    void *data;
    int size;

    /* Creation.  (Assume resource_strdup() works for this.) */
    CHECK_TRUE(test_resmgr = resource_create(1));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(test_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");
    /* Expansion of resource array. */
    CHECK_TRUE(resid2 = resource_strdup(test_resmgr, "test2", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(test_resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_STREQUAL(data, "test2");
    /* Destruction. */
    resource_destroy(test_resmgr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_manager_create_default_size)
{
    ResourceManager *test_resmgr;
    int resid;
    void *data;
    int size;

    CHECK_TRUE(test_resmgr = resource_create(0));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(test_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");
    resource_destroy(test_resmgr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_manager_create_memory_failure)
{
    ResourceManager *test_resmgr;
    int resid, resid2;
    void *data;
    int size;

    CHECK_MEMORY_FAILURES(test_resmgr = resource_create(1));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(test_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");

    CHECK_TRUE(resid2 = resource_strdup(test_resmgr, "test2", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(test_resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_STREQUAL(data, "test2");

    resource_destroy(test_resmgr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_manager_create_invalid)
{
    CHECK_FALSE(resource_create(-1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_static_manager)
{
    DEFINE_STATIC_RESOURCEMANAGER(static_resmgr, 1);
    int resid;
    void *data;
    int size;

    /* Check that we can allocate a resource into a static instance. */
    CHECK_TRUE(resid = resource_strdup(static_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");
    /* And make sure we can do it again. */
    resource_destroy(static_resmgr);
    CHECK_TRUE(resid = resource_strdup(static_resmgr, "test2", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_STREQUAL(data, "test2");
    /* Double destruction should not crash. */
    resource_destroy(static_resmgr);
    resource_destroy(static_resmgr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_static_manager_expand)
{
    DEFINE_STATIC_RESOURCEMANAGER(static_resmgr, 1);
    int resid, resid2;
    void *data;
    int size;

    /* Resource array expansion with a static resource buffer. */
    CHECK_TRUE(resid = resource_strdup(static_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");
    CHECK_TRUE(resid2 = resource_strdup(static_resmgr, "test2", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_STREQUAL(data, "test2");
    resource_destroy(static_resmgr);
    /* And make sure we can do it again. */
    CHECK_TRUE(resid = resource_strdup(static_resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");
    CHECK_TRUE(resid2 = resource_strdup(static_resmgr, "test2", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(static_resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_STREQUAL(data, "test2");
    resource_destroy(static_resmgr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_manager_expand_interlinked)
{
    DEFINE_STATIC_RESOURCEMANAGER(static_resmgr, 1);
    DEFINE_STATIC_RESOURCEMANAGER(static_resmgr2, 2);
    int resid, resid2;

    /* Expansion of resource arrays with interlinked resources. */
    CHECK_TRUE(resid = resource_strdup(static_resmgr2, "testing", 0));
    CHECK_TRUE(resource_link(static_resmgr, static_resmgr2, resid));
    CHECK_TRUE(resid2 = resource_strdup(static_resmgr, "test2", 0));
    CHECK_TRUE(resource_link(static_resmgr2, static_resmgr, resid2));
    CHECK_TRUE(resource_link(static_resmgr2, static_resmgr2, resid));
    resource_destroy(static_resmgr);
    resource_destroy(static_resmgr2);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Temporarily disable the debug wrapper so we can pass file and line
 * arguments directly to resource_create(). */
#undef resource_create

TEST(test_manager_debug_owner)
{
    static void *dummy_buf[_SIL_RESOURCE_SIZE1 + _SIL_RESOURCE_SIZE2];
    static ResourceManager test_resmgr_no_dirs = {
        .static_buffer = dummy_buf,
        .static_size = sizeof(dummy_buf),
        .static_count = 1,
        .static_file = "file.c",
        .static_line = 0,
    };
    static ResourceManager test_resmgr_one_dir = {
        .static_buffer = dummy_buf,
        .static_size = sizeof(dummy_buf),
        .static_count = 1,
        .static_file = "dir1/file.c",
        .static_line = 0,
    };
    static ResourceManager test_resmgr_two_dirs = {
        .static_buffer = dummy_buf,
        .static_size = sizeof(dummy_buf),
        .static_count = 1,
        .static_file = "dir1/dir2/file.c",
        .static_line = 0,
    };
    ResourceManager *test_resmgr;
    int resid;

    CHECK_TRUE(test_resmgr = resource_create(1, "file.c", 0));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);
    test_resmgr = &test_resmgr_no_dirs;
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);

    CHECK_TRUE(test_resmgr = resource_create(1, "dir1/file.c", 0));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (dir1/file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);
    test_resmgr = &test_resmgr_one_dir;
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (dir1/file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);

    CHECK_TRUE(test_resmgr = resource_create(1, "dir1/dir2/file.c", 0));
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (dir2/file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);
    test_resmgr = &test_resmgr_two_dirs;
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "test", 0));
    CHECK_TRUE(!resource_get_texture(test_resmgr, resid));
    CHECK_DLOG_TEXT("Resource ID %d in resource manager %p (dir2/file.c:0) is"
                    " not a texture resource", resid, test_resmgr);
    resource_destroy(test_resmgr);

    return 1;
}

/* Restore the debug wrapper we disabled above. */
#define resource_create(num_resources) \
    resource_create((num_resources), __FILE__, __LINE__)

/*-----------------------------------------------------------------------*/

TEST(test_manager_invalid)
{
    CHECK_FALSE(resource_create(-1));
    resource_destroy(NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_static_manager_corrupt_data)
{
    static void *dummy_buf[_SIL_RESOURCE_SIZE1 + _SIL_RESOURCE_SIZE2];
    static ResourceManager test_resmgr_empty = {
        .static_buffer = NULL,
        .static_size = 0,
        .static_count = 0,
        .static_file = __FILE__,
        .static_line = __LINE__,
    };
    static ResourceManager test_resmgr_negative_count = {
        .static_buffer = dummy_buf,
        .static_size = INT_MAX,
        .static_count = -1,
        .static_file = __FILE__,
        .static_line = __LINE__,
    };
    static ResourceManager test_resmgr_buffer_too_small = {
        .static_buffer = dummy_buf,
        .static_size = 1,
        .static_count = 1,
        .static_file = __FILE__,
        .static_line = __LINE__,
    };
    static ResourceManager test_resmgr_buffer_too_large = {
        .static_buffer = dummy_buf,
        .static_size = sizeof(dummy_buf) + 1,
        .static_count = 1,
        .static_file = __FILE__,
        .static_line = __LINE__,
    };
    static ResourceManager test_resmgr_buffer_not_aligned = {
        .static_buffer = (char *)dummy_buf + 1,
        .static_size = sizeof(dummy_buf),
        .static_count = 1,
        .static_file = __FILE__,
        .static_line = __LINE__,
    };

    CHECK_FALSE(resource_strdup(&test_resmgr_empty, "test", 0));
    CHECK_FALSE(resource_strdup(&test_resmgr_negative_count, "test", 0));
    CHECK_FALSE(resource_strdup(&test_resmgr_buffer_too_small, "test", 0));
    CHECK_FALSE(resource_strdup(&test_resmgr_buffer_too_large, "test", 0));
    CHECK_FALSE(resource_strdup(&test_resmgr_buffer_not_aligned, "test", 0));

    return 1;
}

/*************************************************************************/
/*************************** Path lookup tests ***************************/
/*************************************************************************/

TEST(test_exists)
{
    CHECK_TRUE(resource_exists("testdata/test.txt"));
    CHECK_TRUE(resource_exists("TestData/TEST.TXT"));
    CHECK_FALSE(resource_exists("testdata/test"));
    CHECK_FALSE(resource_exists("testdata/test.txtt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists_host_prefix)
{
    char buf[5+4096+20];
    memcpy(buf, "host:", 5);
    const size_t prefix_len =
        5 + sys_get_resource_path_prefix(buf+5, sizeof(buf)-25);
    ASSERT(prefix_len < sizeof(buf)-25);

    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test.txt"));
    CHECK_TRUE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "TestData/TEST.TXT"));
    CHECK_TRUE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test"));
    CHECK_FALSE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test.txtt"));
    CHECK_FALSE(resource_exists(buf));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists_absolute_path)
{
    char buf[4096+20];
    const size_t prefix_len =
        sys_get_resource_path_prefix(buf, sizeof(buf)-20);
    ASSERT(prefix_len < sizeof(buf)-20);
    if (*buf != '/') {
        SKIP("System resource path does not begin with '/'.");
    }

    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test.txt"));
    CHECK_TRUE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "TestData/TEST.TXT"));
    CHECK_TRUE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test"));
    CHECK_FALSE(resource_exists(buf));
    ASSERT(strformat_check(buf+prefix_len, sizeof(buf)-prefix_len,
                           "testdata/test.txtt"));
    CHECK_FALSE(resource_exists(buf));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists_buffer_overflow_on_prefix)
{
    char buf[8192];
    for (size_t i = 0; i < 4096; i += 2) {
        buf[i] = '.';
        buf[i+1] = '/';
    }
    ASSERT(sys_get_resource_path_prefix(buf+4096, sizeof(buf)-4096)
           < (int)sizeof(buf)-4096);

    TEST_resource_set_path_prefix(buf);
    const int exists = resource_exists("testdata/test.txt");
    /* Make sure the prefix gets reset even if the test fails. */
    TEST_resource_set_path_prefix(NULL);
    CHECK_FALSE(exists);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists_buffer_overflow_on_name)
{
    char buf[4096+20];
    for (size_t i = 0; i < sizeof(buf)-20; i += 2) {
        buf[i] = '.';
        buf[i+1] = '/';
    }
    ASSERT(strformat_check(buf+sizeof(buf)-20, 20, "testdata/test.txt"));
    CHECK_FALSE(resource_exists(buf));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists_invalid)
{
    CHECK_FALSE(resource_exists(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir1/dir2", 0));
    /* Note that this should not return the file "testdata/dir1/dir2.txt". */
    CHECK_STREQUAL(resource_list_files_next(dir), "File.Txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    /* Make sure it keeps returning NULL on subsequent calls. */
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_prefix_match)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir3", 0));
    /* This should not return the files "testdata/DIR3.txt" or
     * "testdata/dir3.txt2". */
    CHECK_STREQUAL(resource_list_files_next(dir), "file.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_trailing_slash)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir1/dir2/", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "File.Txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_subdirs_only)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir1", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_recursive)
{
#ifdef SIL_PLATFORM_ANDROID
    SKIP("Not supported on Android.");
#endif

    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir1", 1));
    CHECK_STREQUAL(resource_list_files_next(dir), "dir2/File.Txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_recursive_multiple)
{
#ifdef SIL_PLATFORM_ANDROID
    SKIP("Not supported on Android.");
#endif

    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/dir4", 1));
    /* The files could be returned in either order, so we have to check for
     * both possibilities. */
    const char *path = resource_list_files_next(dir);
    if (path && strcmp(path, "dir5/a.txt") == 0) {
        CHECK_STREQUAL(resource_list_files_next(dir), "dir5/b.txt");
    } else {
        CHECK_STREQUAL(path, "dir5/b.txt");
        CHECK_STREQUAL(resource_list_files_next(dir), "dir5/a.txt");
    }
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_recursive_memory_errors)
{
#ifdef SIL_PLATFORM_ANDROID
    SKIP("Not supported on Android.");
#endif

    /* We have to wrap the entire start/next/end sequence in a single
     * CHECK_MEMORY_FAILURES() call because any failure will cause the
     * subdirectory to be skipped, and we can't retry the lookup after
     * sys_dir_read() has already returned the entry. */
    ResourceFileListHandle *dir;
    const char *path;
    char *path_copy;
    CHECK_MEMORY_FAILURES((dir = resource_list_files_start("testdata/dir1", 1))
                          && ((path = resource_list_files_next(dir))
                              || (resource_list_files_end(dir), 0))
                          && (path_copy = mem_strdup(path, 0),
                              resource_list_files_end(dir), path_copy));
    CHECK_STREQUAL(path_copy, "dir2/File.Txt");
    mem_free(path_copy);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_recursive_limit)
{
#ifdef SIL_PLATFORM_ANDROID
    SKIP("Not supported on Android.");
#endif

    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/recurse", 1));
    CHECK_STREQUAL(resource_list_files_next(dir),
                   "1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/test.txt");
    /* .../16/test.txt should be skipped due to the recursion limit. */
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_recursive_close)
{
#ifdef SIL_PLATFORM_ANDROID
    SKIP("Not supported on Android.");
#endif

    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/recurse", 1));
    CHECK_STREQUAL(resource_list_files_next(dir),
                   "1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/test.txt");
    /* Closing the handle here should not leave subdir handles dangling. */
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_slash_only)
{
    ResourceFileListHandle *dir;

    /* Make sure trailing-slash handling doesn't wander off the beginning
     * of the string.  We can't predict what list_files("/") will do, so
     * just check that the call doesn't crash. */
    dir = resource_list_files_start("/", 0);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_nonexistent_path)
{
    CHECK_FALSE(resource_list_files_start("testdata/dir2", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_buffer_overflow)
{
    char buf[4096+20];
    for (size_t i = 0; i < sizeof(buf)-20; i += 2) {
        buf[i] = '.';
        buf[i+1] = '/';
    }
    ASSERT(strformat_check(buf+sizeof(buf)-20, 20, "testdata/dir1/dir2"));
    CHECK_FALSE(resource_list_files_start(buf, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_memory_failure)
{
    ResourceFileListHandle *dir;

    CHECK_MEMORY_FAILURES(
        dir = resource_list_files_start("testdata/dir1/dir2", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "File.Txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files_invalid)
{
    CHECK_FALSE(resource_list_files_start(NULL, 0));
    CHECK_FALSE(resource_list_files_next(NULL));
    resource_list_files_end(NULL);  // No return value, but shouldn't crash.

    return 1;
}

/*************************************************************************/
/*********************** Basic data loading tests ************************/
/*************************************************************************/

TEST(test_load_get_data)
{
    int resid, mark;
    void *data, *data2;
    int size;

    /* Normal load. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    /* Make sure a NULL size_ret is allowed, as documented. */
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid, NULL));
    /* Make sure it's not returned as another resource type. */
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_FALSE(resource_get_font(resmgr, resid));
    CHECK_FALSE(resource_get_sound(resmgr, resid));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_data_memory_failure)
{
    int resid;
    void *data;
    int size;

    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_data(resmgr_single, "testdata/test.txt", 0, 0),
        data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_load_data_read_call_failure)
{
    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_FALSE(resource_load_data(resmgr, "testdata/test.txt", 0, 0));

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_load_data_transient_read_call_failure)
{
    int resid, mark;
    void *data;
    int size;

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_load_data_read_failure)
{
    int resid, mark;

    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_load_data_async_overload)
{
    int resid[MAX_ASYNC_READS*2+1], mark;
    void *data;
    int size;

    /* Check that requests which can't start an async read the first time
     * around are properly started by resource_wait(). */
    for (int i = 0; i < lenof(resid); i++) {
        resid[i] = resource_load_data(resmgr, "testdata/test.txt", 0, 0);
        if (!resid[i]) {
            FAIL("resource_load_data(resmgr, \"testdata/test.txt\", 0, 0)"
                 " failed for iteration %d/%d", i, lenof(resid));
        }
    }
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_FALSE(resource_sync(resmgr, mark));
    TEST_misc_ioqueue_block_io_thread(0);
    resource_wait(resmgr, mark);
    for (int i = 0; i < lenof(resid); i++) {
        size = 0;
        data = resource_get_data(resmgr, resid[i], &size);
        if (!data) {
            FAIL("resource_get_data(resmgr, resid[i], &size) failed for"
                 " iteration %d/%d", i, lenof(resid));
        }
        if (size != 5) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " size (%d, should be 5) for iteration %d/%d",
                 size, i, lenof(resid));
        }
        if (memcmp(data, "hello", 5) != 0) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " data for iteration %d/%d: %02X %02X %02X %02X %02X",
                 i, lenof(resid), ((uint8_t *)data)[0], ((uint8_t *)data)[1],
                 ((uint8_t *)data)[2], ((uint8_t *)data)[3],
                 ((uint8_t *)data)[4]);
        }
        resource_free(resmgr, resid[i]);
    }

    return 1;
}
#endif  // USING_IOQUEUE

/*-----------------------------------------------------------------------*/

TEST(test_load_data_empty_file)
{
    int resid, mark;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/sound/bad/empty-file.wav", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 0);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_data_nonexistent_file)
{
    CHECK_FALSE(resource_load_data(resmgr, "testdata/no_such_file", 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_data_invalid)
{
    int resid, mark;
    int size;

    CHECK_FALSE(resource_load_data(NULL, "testdata/test.txt", 0, 0));
    CHECK_FALSE(resource_load_data(resmgr_uninit, "testdata/test.txt", 0, 0));
    CHECK_FALSE(resource_load_data(resmgr, NULL, 0, 0));
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(NULL, resid, &size));
    CHECK_FALSE(resource_get_data(resmgr_uninit, resid, &size));
    CHECK_FALSE(resource_get_data(resmgr, 0, &size));
    CHECK_FALSE(resource_get_data(resmgr, INT_MAX, &size));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mark_wraparound)
{
    int resid, mark;
    void *data;
    int size;

    TEST_resource_set_mark(resmgr, -1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_INTEQUAL(mark, 1);
    resource_wait(resmgr, mark);
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mark_invalid)
{
    CHECK_FALSE(resource_mark(NULL));
    CHECK_FALSE(resource_mark(resmgr_uninit));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sync)
{
    int resid, mark;
    void *data;
    int size;

    /* Check that resource_sync() returns false before a file has been
     * loaded. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_FALSE(resource_sync(resmgr, mark));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    TEST_resource_block_load(0);
    resource_wait(resmgr, mark);
    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

#ifdef USING_IOQUEUE
    /* The same, using low-level blocking. */
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_FALSE(resource_sync(resmgr, mark));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    TEST_misc_ioqueue_block_io_thread(0);
    resource_wait(resmgr, mark);
    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_sync_read_call_failure)
{
    int resid, mark;

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_misc_ioqueue_permfail_next_read(1);
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_sync_transient_read_call_failure)
{
    int resid, mark;
    void *data;
    int size;

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_misc_ioqueue_tempfail_next_read(1);
    resource_sync(resmgr, mark);
    TEST_misc_ioqueue_tempfail_next_read(1);
    resource_wait(resmgr, mark);
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_sync_async_overload)
{
    int resid, mark;
    void *data;
    int size;

    /* Common async-full behavior (kicking pending resources) was checked
     * by the test_load_data_async_overload test; here, we fill up the
     * async-read table ourselves and just run a single resource through
     * the pipeline. */

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int i;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (i = 1; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_FALSE(resource_sync(resmgr, mark));
    ASSERT(sys_file_wait_async(reqlist[--i]) == 1);
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    resource_free(resmgr, resid);
    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }
    sys_file_close(fh);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_sync_multiple_files)
{
    int resid, resid2, mark, mark2;
    void *data;
    int size;

    /* Check that resource_sync() returns true for a resource even if a
     * later resource is still being loaded. */

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));

    TEST_resource_block_load(1);
    resource_wait(resmgr, mark);
    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    CHECK_FALSE(resource_sync(resmgr, mark2));
    CHECK_FALSE(resource_get_data(resmgr, resid2, NULL));

    TEST_resource_block_load(0);
    resource_wait(resmgr, mark2);
    CHECK_TRUE(resource_sync(resmgr, mark2));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_sync_multiple_files_ioqueue)
{
    int resid, resid2, mark, mark2;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);

    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));

    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    CHECK_FALSE(resource_sync(resmgr, mark2));
    CHECK_FALSE(resource_get_data(resmgr, resid2, NULL));

    TEST_misc_ioqueue_block_io_thread(0);
    resource_wait(resmgr, mark2);
    CHECK_TRUE(resource_sync(resmgr, mark2));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_sync_delayed_finish)
{
    int resid, resid2, resid3, mark, mark2, mark3;
    void *data;
    int size;

    /* Check that resource_sync() and resource_wait() do not perform load
     * completion actions (i.e., call the internal function finish_load())
     * for a resource when syncing to an earlier resource mark. */

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));
    CHECK_TRUE(resid3 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark3 = resource_mark(resmgr));

    TEST_resource_use_silent_sync(1);
    while (!resource_sync(resmgr, mark3)) {
        thread_yield();
    }
    TEST_resource_use_silent_sync(0);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_data(resmgr, resid2, NULL));
    CHECK_FALSE(resource_get_data(resmgr, resid3, NULL));

    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    CHECK_FALSE(resource_get_data(resmgr, resid2, NULL));
    CHECK_FALSE(resource_get_data(resmgr, resid3, NULL));

    resource_wait(resmgr, mark2);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    CHECK_FALSE(resource_get_data(resmgr, resid3, NULL));

    CHECK_TRUE(resource_sync(resmgr, mark3));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sync_freed_resource)
{
    int resid, mark;

    /* Check that resource_sync() and resource_wait() don't break if the
     * resource is freed before loading completes. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_resource_use_silent_sync(1);
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    TEST_resource_use_silent_sync(0);
    resource_free(resmgr, resid);
    CHECK_TRUE(resource_sync(resmgr, mark));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_resource_use_silent_sync(1);
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    TEST_resource_use_silent_sync(0);
    resource_free(resmgr, resid);
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

#ifdef USING_IOQUEUE
    /* Also check freeing before I/O completes. */
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_FALSE(resource_sync(resmgr, mark));
    TEST_misc_ioqueue_block_io_thread(0);
    resource_free(resmgr, resid);
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sync_invalid)
{
    CHECK_TRUE(resource_sync(NULL, resource_mark(resmgr)));
    CHECK_TRUE(resource_sync(resmgr_uninit, resource_mark(resmgr)));
    CHECK_TRUE(resource_sync(resmgr, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_wait_async_overload)
{
    int resid, mark;
    void *data;
    int size;

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int i;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (i = 1; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    /* This should succeed even with no async handles available. */
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    resource_free(resmgr, resid);
    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }
    sys_file_close(fh);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#if defined(USING_IOQUEUE) && !defined(SIL_PLATFORM_WINDOWS)
/* Windows doesn't use ioqueue for synchronous reads, so this test won't
 * work. */
TEST(test_wait_async_overload_read_failure)
{
    int resid, mark;

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int i;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (i = 1; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }
    while (!sys_file_poll_async(reqlist[i-1])) {
        thread_yield();
    }

    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    resource_free(resmgr, resid);
    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }
    sys_file_close(fh);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_wait_invalid)
{
    /* Make sure these don't crash or block forever. */
    resource_wait(NULL, resource_mark(resmgr));
    resource_wait(resmgr_uninit, resource_mark(resmgr));
    resource_wait(resmgr, 0);

    return 1;
}

/*************************************************************************/
/******************* Load/get tests for all data types *******************/
/*************************************************************************/

TEST(test_load_get_texture)
{
    int resid, mark;
    int texture;
    const uint8_t *pixels;

    /* Normal load. */
    CHECK_TRUE(resid = resource_load_texture(
                   resmgr, "testdata/texture/4x4-rgba.tex", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(texture = resource_get_texture(resmgr, resid));
    /* Make sure it's not returned as another resource type. */
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_font(resmgr, resid));
    CHECK_FALSE(resource_get_sound(resmgr, resid));

    /* Check the actual texture data. */
    static const uint8_t rgba_4x4[] = {
          0,  0,  0,  0,  4,  0,  4, 16,  8,  0,  8, 32, 12,  0, 12, 48,
          0, 64, 64, 64,  4, 64, 68, 80,  8, 64, 72, 96, 12, 64, 76,112,
          0,128,128,128,  4,128,132,144,  8,128,136,160, 12,128,140,176,
          0,192,192,192,  4,192,196,208,  8,192,200,224, 12,192,204,240,
    };
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 4);
    CHECK_FLOATEQUAL(texture_scale(texture), 1);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    CHECK_MEMEQUAL(pixels, rgba_4x4, sizeof(rgba_4x4));
    texture_unlock(texture);

    /* Check that freeing the resource also frees the texture. */
    resource_free(resmgr, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_texture_memory_failure)
{
    int resid;
    int texture;
    const uint8_t *pixels;
    static const uint8_t rgba_4x4[] = {
          0,  0,  0,  0,  4,  0,  4, 16,  8,  0,  8, 32, 12,  0, 12, 48,
          0, 64, 64, 64,  4, 64, 68, 80,  8, 64, 72, 96, 12, 64, 76,112,
          0,128,128,128,  4,128,132,144,  8,128,136,160, 12,128,140,176,
          0,192,192,192,  4,192,196,208,  8,192,200,224, 12,192,204,240,
    };

    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_texture(
            resmgr_single, "testdata/texture/4x4-rgba.tex", 0, 0),
        texture = resource_get_texture(resmgr_single, resid));
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 4);
    CHECK_FLOATEQUAL(texture_scale(texture), 1);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    CHECK_MEMEQUAL(pixels, rgba_4x4, sizeof(rgba_4x4));
    texture_unlock(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_texture_nonexistent_file)
{
    CHECK_FALSE(resource_load_texture(resmgr, "testdata/no_such_file", 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_texture_bad_data)
{
    int resid, mark;

    CHECK_TRUE(resid = resource_load_texture(
                   resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_texture(resmgr, resid));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_texture_invalid)
{
    int resid, mark;

    CHECK_FALSE(resource_load_texture(
                    NULL, "testdata/texture/4x4-rgba.tex", 0, 0));
    CHECK_FALSE(resource_load_texture(
                    resmgr_uninit, "testdata/texture/4x4-rgba.tex", 0, 0));
    CHECK_FALSE(resource_load_texture(resmgr, NULL, 0, 0));
    CHECK_TRUE(resid = resource_load_texture(
                   resmgr, "testdata/texture/4x4-rgba.tex", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_texture(NULL, resid));
    CHECK_FALSE(resource_get_texture(resmgr_uninit, resid));
    CHECK_FALSE(resource_get_texture(resmgr, 0));
    CHECK_FALSE(resource_get_texture(resmgr, INT_MAX));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_get_bitmap_font)
{
    int resid, mark;
    int font;

    CHECK_TRUE(resid = resource_load_bitmap_font(
                   resmgr, "testdata/font/test.font", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(font = resource_get_font(resmgr, resid));
    /* Make sure it's not returned as another resource type. */
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_FALSE(resource_get_sound(resmgr, resid));
    /* Make sure we got the expected font data. */
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_text_width(font, "AC p\xE2\x80\x8A""B", 10), 22);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_get_freetype_font)
{
#ifdef SIL_FONT_INCLUDE_FREETYPE
    int resid, mark;
    int font;

    CHECK_TRUE(resid = resource_load_freetype_font(
                   resmgr, "testdata/font/SILTestFont.ttf", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(font = resource_get_font(resmgr, resid));
    /* Make sure it's not returned as another resource type. */
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_FALSE(resource_get_sound(resmgr, resid));
    /* Make sure we got the expected font data. */
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_char_advance(font, 'L', 10), 6);
    CHECK_FLOATEQUAL(font_text_width(font, "L-\xC2\xA0j", 10), 17);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_font_memory_failure)
{
    int resid;
    int font;

    /* Create and destroy a font once to prime the ID arrays. */
    CHECK_TRUE(resid = resource_load_bitmap_font(
                   resmgr, "testdata/font/test.font", 0));
    resource_wait(resmgr, resource_mark(resmgr));
    CHECK_TRUE(font = resource_get_font(resmgr, resid));
    resource_free(resmgr, resid);

    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_bitmap_font(
            resmgr_single, "testdata/font/test.font", 0),
        font = resource_get_font(resmgr_single, resid));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_text_width(font, "AC p\xE2\x80\x8A""B", 10), 22);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_font_nonexistent_file)
{
    CHECK_FALSE(resource_load_bitmap_font(resmgr, "testdata/no_such_file", 0));
    CHECK_FALSE(resource_load_freetype_font(
                    resmgr, "testdata/no_such_file", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_font_bad_data)
{
    int resid, mark;

    CHECK_TRUE(resid = resource_load_bitmap_font(
                   resmgr, "testdata/test.txt", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_font(resmgr, resid));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_font_invalid)
{
    int resid, mark;

    CHECK_FALSE(resource_load_bitmap_font(NULL, "testdata/font/test.font", 0));
    CHECK_FALSE(resource_load_freetype_font(
                    NULL, "testdata/font/test.font", 0));

    CHECK_FALSE(resource_load_bitmap_font(
                    resmgr_uninit, "testdata/font/test.font", 0));
    CHECK_FALSE(resource_load_freetype_font(
                    resmgr_uninit, "testdata/font/test.font", 0));

    CHECK_FALSE(resource_load_bitmap_font(resmgr, NULL, 0));
    CHECK_FALSE(resource_load_freetype_font(resmgr, NULL, 0));

    CHECK_TRUE(resid = resource_load_bitmap_font(
                   resmgr, "testdata/font/test.font", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_font(NULL, resid));
    CHECK_FALSE(resource_get_font(resmgr_uninit, resid));
    CHECK_FALSE(resource_get_font(resmgr, 0));
    CHECK_FALSE(resource_get_font(resmgr, INT_MAX));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_get_sound)
{
    int resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(resid = resource_load_sound(
                   resmgr, "testdata/sound/square.wav", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    /* Make sure it's not returned as another resource type. */
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_FALSE(resource_get_font(resmgr, resid));
    /* Make sure we can actually play the sound. */
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_sound_memory_failure)
{
    int resid;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_sound(
            resmgr_single, "testdata/sound/square.wav", 0),
        sound = resource_get_sound(resmgr_single, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_sound_nonexistent_file)
{
    CHECK_FALSE(resource_load_sound(resmgr, "testdata/no_such_file", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_sound_bad_data)
{
    int resid, mark;

    CHECK_TRUE(resid = resource_load_sound(resmgr, "testdata/test.txt", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_sound(resmgr, resid));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_sound_invalid)
{
    int resid, mark;

    CHECK_FALSE(resource_load_sound(NULL, "testdata/sound/square.wav", 0));
    CHECK_FALSE(resource_load_sound(resmgr_uninit,
                                    "testdata/sound/square.wav", 0));
    CHECK_FALSE(resource_load_sound(resmgr, NULL, 0));
    CHECK_TRUE(resid = resource_load_sound(
                   resmgr, "testdata/sound/square.wav", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_sound(NULL, resid));
    CHECK_FALSE(resource_get_sound(resmgr_uninit, resid));
    CHECK_FALSE(resource_get_sound(resmgr, 0));
    CHECK_FALSE(resource_get_sound(resmgr, INT_MAX));

    return 1;
}

/*************************************************************************/
/************************* Streaming sound tests *************************/
/*************************************************************************/

TEST(test_open_sound)
{
    int resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(resid = resource_open_sound(
                   resmgr, "testdata/sound/square.wav"));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_memory_failure)
{
    int resid;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_open_sound(
            resmgr_single, "testdata/sound/square.wav"),
        sound = resource_get_sound(resmgr_single, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_nonexistent_file)
{
    CHECK_FALSE(resource_open_sound(resmgr, "testdata/no_such_file"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_bad_data)
{
    CHECK_FALSE(resource_open_sound(resmgr, "testdata/test.txt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_invalid)
{
    CHECK_FALSE(resource_open_sound(NULL, "testdata/sound/square.wav"));
    CHECK_FALSE(resource_open_sound(resmgr_uninit,
                                    "testdata/sound/square.wav"));
    CHECK_FALSE(resource_open_sound(resmgr, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file)
{
    int file, resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/sound/square.wav"));
    CHECK_TRUE(resid = resource_open_sound_from_file(
                   resmgr, resmgr, file, 0,
                   resource_get_file_size(resmgr,file)));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file_different_resmgr)
{
    int file, resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/sound/square.wav"));
    CHECK_TRUE(resid = resource_open_sound_from_file(
                   resmgr2, resmgr, file, 0,
                   resource_get_file_size(resmgr,file)));
    CHECK_TRUE(mark = resource_mark(resmgr2));
    resource_wait(resmgr2, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr2, resid));
    CHECK_FALSE(resource_get_data(resmgr2, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr2, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file_memory_failure)
{
    int file, resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/sound/square.wav"));
    CHECK_RESOURCE_MEMORY_FAILURES(resid = resource_open_sound_from_file(
                                       resmgr_single, resmgr, file, 0,
                                       resource_get_file_size(resmgr,file)));
    CHECK_TRUE(mark = resource_mark(resmgr_single));
    resource_wait(resmgr_single, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr_single, resid));
    CHECK_FALSE(resource_get_data(resmgr_single, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr_single, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file_nonzero_offset)
{
    int file, resid, mark;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/package.dat"));
    CHECK_TRUE(resid = resource_open_sound_from_file(
                   resmgr, resmgr, file, 28, 124));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file_bad_data)
{
    int file;
    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/test.txt"));
    const int64_t size = resource_get_file_size(resmgr, file);
    CHECK_FALSE(resource_open_sound_from_file(resmgr, resmgr, file, 0, size));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_sound_from_file_invalid)
{
    int file;
    CHECK_TRUE(file = resource_open_file(resmgr, "testdata/sound/square.wav"));
    const int64_t size = resource_get_file_size(resmgr, file);

    CHECK_FALSE(resource_open_sound_from_file(
                    NULL, resmgr, file, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr_uninit, resmgr, file, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, NULL, file, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr_uninit, file, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, 0, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, INT_MAX, 0, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, file, -1, size));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, file, 0, 0));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, file, 0, -1));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, file, 0, size+1));
    CHECK_FALSE(resource_open_sound_from_file(
                    resmgr, resmgr, file, 1, size));

    int str;
    CHECK_TRUE(str = resource_strdup(resmgr, "test", 0));
    CHECK_FALSE(resource_open_sound_from_file(resmgr, resmgr, str, 0, 1));

    return 1;
}

/*************************************************************************/
/********************** New resource creation tests **********************/
/*************************************************************************/

TEST(test_new_data)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_new_data(resmgr, 4, 4, 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 4);
    /* Value filled in for non-clear allocs by resource.c when debugging. */
    CHECK_INTEQUAL(*(uint32_t *)data, 0xBBBBBBBBU);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_data_clear)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_new_data(
                   resmgr, 4, 4,
                   RES_ALLOC_CLEAR | RES_ALLOC_TOP | RES_ALLOC_TEMP));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 4);
    CHECK_INTEQUAL(*(uint32_t *)data, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_data_zero_size)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_new_data(resmgr, 0, 0, 0));
    size = 1;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_data_memory_failure)
{
    int resid;
    void *data;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_new_data(resmgr_single, 4, 4, 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 4);
    /* Value filled in for non-clear allocs by resource.c when debugging. */
    CHECK_INTEQUAL(*(uint32_t *)data, 0xBBBBBBBBU);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_data_invalid)
{
    CHECK_FALSE(resource_new_data(NULL, 4, 4, 0));
    CHECK_FALSE(resource_new_data(resmgr_uninit, 4, 4, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_data)
{
    void *data, *data2;
    int size;

    CHECK_TRUE(data = resource_get_new_data(resmgr, 4, 4, 0));
    /* Value filled in for non-clear allocs by resource.c when debugging. */
    CHECK_INTEQUAL(*(uint32_t *)data, 0xBBBBBBBBU);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, 1, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_data_clear)
{
    void *data, *data2;
    int size;

    CHECK_TRUE(data = resource_get_new_data(resmgr, 4, 4, RES_ALLOC_CLEAR));
    CHECK_INTEQUAL(*(uint32_t *)data, 0);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, 1, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_data_zero_size)
{
    void *data, *data2;
    int size;

    CHECK_TRUE(data = resource_get_new_data(resmgr, 0, 0, 0));
    size = 1;
    CHECK_TRUE(data2 = resource_get_data(resmgr, 1, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_data_memory_failure)
{
    void *data, *data2;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        data = resource_get_new_data(resmgr_single, 4, 4, 0));
    /* Value filled in for non-clear allocs by resource.c when debugging. */
    CHECK_INTEQUAL(*(uint32_t *)data, 0xBBBBBBBBU);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr_single, 2, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_data_invalid)
{
    CHECK_FALSE(resource_get_new_data(NULL, 4, 4, 0));
    CHECK_FALSE(resource_get_new_data(resmgr_uninit, 4, 4, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_data)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_copy_data(resmgr, "testing", 6, 1, 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "testin", 6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_data_zero_size)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_copy_data(resmgr, "testing", 0, 1, 0));
    size = 1;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_data_memory_failure)
{
    int resid;
    void *data;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_copy_data(resmgr_single, "testing", 6, 1, 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "testin", 6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_data_invalid)
{
    CHECK_FALSE(resource_copy_data(NULL, "testing", 6, 1, 0));
    CHECK_FALSE(resource_copy_data(resmgr_uninit, "testing", 6, 1, 0));
    CHECK_FALSE(resource_copy_data(resmgr, NULL, 6, 1, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_copy_data)
{
    char *data;
    void *data2;
    int size;

    CHECK_TRUE(data = resource_get_copy_data(resmgr, "testing", 6, 1, 0));
    CHECK_MEMEQUAL(data, "testin", 6);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, 1, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_copy_data_zero_size)
{
    char *data;
    void *data2;
    int size;

    CHECK_TRUE(data = resource_get_copy_data(resmgr, "testing", 0, 1, 0));
    size = 1;
    CHECK_TRUE(data2 = resource_get_data(resmgr, 1, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_copy_data_memory_failure)
{
    char *data;
    void *data2;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        data = resource_get_copy_data(resmgr_single, "testing", 6, 1, 0));
    CHECK_MEMEQUAL(data, "testin", 6);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr_single, 2, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_copy_data_invalid)
{
    CHECK_FALSE(resource_get_copy_data(NULL, "testing", 6, 1, 0));
    CHECK_FALSE(resource_get_copy_data(resmgr_uninit, "testing", 6, 1, 0));
    CHECK_FALSE(resource_get_copy_data(resmgr, NULL, 6, 1, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup)
{
    int resid;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_strdup(resmgr, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_memory_failure)
{
    int resid;
    void *data;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_strdup(resmgr_single, "testing", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_STREQUAL(data, "testing");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_invalid)
{
    CHECK_FALSE(resource_strdup(NULL, "testing", 0));
    CHECK_FALSE(resource_strdup(resmgr_uninit, "testing", 0));
    CHECK_FALSE(resource_strdup(resmgr, NULL, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_strdup)
{
    char *data;
    void *data2;
    int size;

    CHECK_RESOURCE_MEMORY_FAILURES(
        data = resource_get_strdup(resmgr_single, "testing", 0));
    CHECK_STREQUAL(data, "testing");
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr_single, 2, &size));
    CHECK_PTREQUAL(data, data2);
    CHECK_INTEQUAL(size, 8);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_strdup_invalid)
{
    CHECK_FALSE(resource_get_strdup(NULL, "testing", 0));
    CHECK_FALSE(resource_get_strdup(resmgr_uninit, "testing", 0));
    CHECK_FALSE(resource_get_strdup(resmgr, NULL, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_data)
{
    int resid;
    void *data;
    int size;

    ASSERT(data = mem_strdup("world", 0));
    CHECK_TRUE(resid = resource_take_data(resmgr, data, 5));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "world", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_data_memory_failure)
{
    int resid;
    void *data;
    int size;

    ASSERT(data = mem_strdup("world", 0));
    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_take_data(resmgr_single, data, 5));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "world", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_data_invalid)
{
    void *data;

    ASSERT(data = mem_strdup("world", 0));
    CHECK_FALSE(resource_take_data(NULL, data, 5));
    CHECK_FALSE(resource_take_data(resmgr_uninit, data, 5));
    mem_free(data);
    CHECK_FALSE(resource_take_data(resmgr, NULL, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture)
{
    int resid;
    int texture;
    const uint8_t *pixels;

    CHECK_TRUE(resid = resource_new_texture(resmgr, 4, 2, RES_ALLOC_CLEAR, 0));
    CHECK_TRUE(texture = resource_get_texture(resmgr, resid));
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 4*2*4; i++) {
        CHECK_INTEQUAL(pixels[i], 0);
    }
    texture_unlock(texture);
    resource_free(resmgr, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture_memory_failure)
{
    int resid;
    int texture;
    const uint8_t *pixels;

    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_new_texture(resmgr_single, 4, 2, RES_ALLOC_CLEAR, 0));
    CHECK_TRUE(texture = resource_get_texture(resmgr_single, resid));
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 4*2*4; i++) {
        CHECK_INTEQUAL(pixels[i], 0);
    }
    texture_unlock(texture);
    resource_free(resmgr_single, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture_invalid)
{
    CHECK_FALSE(resource_new_texture(NULL, 4, 2, 0, 0));
    CHECK_FALSE(resource_new_texture(resmgr_uninit, 4, 2, 0, 0));
    CHECK_FALSE(resource_new_texture(resmgr, 0, 2, 0, 0));
    CHECK_FALSE(resource_new_texture(resmgr, 4, 0, 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture)
{
    int texture;
    const uint8_t *pixels;

    CHECK_TRUE(
        texture = resource_get_new_texture(resmgr, 4, 2, RES_ALLOC_CLEAR, 0));
    CHECK_INTEQUAL(resource_get_texture(resmgr, 1), texture);
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 4*2*4; i++) {
        CHECK_INTEQUAL(pixels[i], 0);
    }
    texture_unlock(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture_memory_failure)
{
    int texture;
    const uint8_t *pixels;

    CHECK_RESOURCE_MEMORY_FAILURES(
        texture = resource_get_new_texture(
            resmgr_single, 4, 2, RES_ALLOC_CLEAR, 0));
    CHECK_INTEQUAL(resource_get_texture(resmgr_single, 2), texture);
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 4*2*4; i++) {
        CHECK_INTEQUAL(pixels[i], 0);
    }
    texture_unlock(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture_invalid)
{
    CHECK_FALSE(resource_get_new_texture(NULL, 4, 2, 0, 0));
    CHECK_FALSE(resource_get_new_texture(resmgr_uninit, 4, 2, 0, 0));
    CHECK_FALSE(resource_get_new_texture(resmgr, 0, 2, 0, 0));
    CHECK_FALSE(resource_get_new_texture(resmgr, 4, 0, 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture_from_display)
{
    int resid;
    int texture;
    const uint8_t *pixels;

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_TRUE(resid = resource_new_texture_from_display(
                   resmgr, 0, 0, 32, 16, 1, RES_ALLOC_CLEAR, 0));
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(texture = resource_get_texture(resmgr, resid));
    CHECK_INTEQUAL(texture_width(texture), 32);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 32*16*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4)%4,(i/4)/4);
    }
    texture_unlock(texture);
    resource_free(resmgr, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture_from_display_memory_failure)
{
    int resid;
    int texture;
    const uint8_t *pixels;

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_new_texture_from_display(
            resmgr_single, 0, 0, 32, 16, 1, RES_ALLOC_CLEAR, 0));
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(texture = resource_get_texture(resmgr_single, resid));
    CHECK_INTEQUAL(texture_width(texture), 32);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 32*16*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4)%4,(i/4)/4);
    }
    texture_unlock(texture);
    resource_free(resmgr_single, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_new_texture_from_display_invalid)
{
    CHECK_FALSE(resource_new_texture_from_display(
                    NULL, 0, 0, 32, 16, 0, 0, 0));
    CHECK_FALSE(resource_new_texture_from_display(
                    resmgr_uninit, 0, 0, 32, 16, 0, 0, 0));
    CHECK_FALSE(resource_new_texture_from_display(
                    resmgr, 0, 0, 0, 16, 0, 0, 0));
    CHECK_FALSE(resource_new_texture_from_display(
                    resmgr, 0, 0, 32, 0, 0, 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture_from_display)
{
    int texture;
    const uint8_t *pixels;

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_TRUE(texture = resource_get_new_texture_from_display(
                   resmgr, 0, 0, 32, 16, 1, RES_ALLOC_CLEAR, 0));
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(resource_get_texture(resmgr, 1), texture);
    CHECK_INTEQUAL(texture_width(texture), 32);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 32*16*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4)%4,(i/4)/4);
    }
    texture_unlock(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture_from_display_memory_failure)
{
    int texture;
    const uint8_t *pixels;

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_RESOURCE_MEMORY_FAILURES(
        texture = resource_get_new_texture_from_display(
            resmgr_single, 0, 0, 32, 16, 1, RES_ALLOC_CLEAR, 0));
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(resource_get_texture(resmgr_single, 2), texture);
    CHECK_INTEQUAL(texture_width(texture), 32);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 32*16*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4)%4,(i/4)/4);
    }
    texture_unlock(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_new_texture_from_display_invalid)
{
    CHECK_FALSE(resource_get_new_texture_from_display(
                    NULL, 0, 0, 32, 16, 0, 0, 0));
    CHECK_FALSE(resource_get_new_texture_from_display(
                    resmgr_uninit, 0, 0, 32, 16, 0, 0, 0));
    CHECK_FALSE(resource_get_new_texture_from_display(
                    resmgr, 0, 0, 0, 16, 0, 0, 0));
    CHECK_FALSE(resource_get_new_texture_from_display(
                    resmgr, 0, 0, 32, 0, 0, 0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_texture)
{
    int resid;
    int texture;

    ASSERT(texture = texture_create(4, 2, 0, 0));
    CHECK_TRUE(resid = resource_take_texture(resmgr, texture));
    CHECK_INTEQUAL(resource_get_texture(resmgr, resid), texture);
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(texture_lock_readonly(texture));
    texture_unlock(texture);
    resource_free(resmgr, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_texture_memory_failure)
{
    int resid;
    int texture;

    ASSERT(texture = texture_create(4, 2, 0, 0));
    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_take_texture(resmgr_single, texture));
    CHECK_INTEQUAL(resource_get_texture(resmgr_single, resid), texture);
    CHECK_INTEQUAL(texture_width(texture), 4);
    CHECK_INTEQUAL(texture_height(texture), 2);
    CHECK_TRUE(texture_lock_readonly(texture));
    texture_unlock(texture);
    resource_free(resmgr_single, resid);
    CHECK_FALSE(texture_lock_readonly(texture));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_texture_invalid)
{
    int texture;

    CHECK_TRUE(texture = texture_create(4, 2, 0, 0));
    CHECK_FALSE(resource_take_texture(NULL, texture));
    CHECK_FALSE(resource_take_texture(resmgr_uninit, texture));
    texture_destroy(texture);
    CHECK_FALSE(resource_take_texture(resmgr, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_sound)
{
    int resid;
    SysFile *fh;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    ASSERT(fh = wrap_sys_file_open("testdata/sound/square.wav"));
    ASSERT(sound = sound_create_stream(fh, 0, sys_file_size(fh), 0));
    CHECK_TRUE(resid = resource_take_sound(resmgr, sound));
    CHECK_TRUE(resource_get_sound(resmgr, resid) == sound);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_sound_memory_failure)
{
    int resid;
    SysFile *fh;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    ASSERT(fh = wrap_sys_file_open("testdata/sound/square.wav"));
    ASSERT(sound = sound_create_stream(fh, 0, sys_file_size(fh), 0));
    CHECK_RESOURCE_MEMORY_FAILURES(
        resid = resource_take_sound(resmgr_single, sound));
    CHECK_TRUE(resource_get_sound(resmgr_single, resid) == sound);
    CHECK_FALSE(resource_get_data(resmgr_single, resid, NULL));
    CHECK_FALSE(resource_get_texture(resmgr_single, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    int16_t pcm[10];
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_take_sound_invalid)
{
    SysFile *fh;
    Sound *sound;

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    ASSERT(fh = wrap_sys_file_open("testdata/sound/square.wav"));
    ASSERT(sound = sound_create_stream(fh, 0, sys_file_size(fh), 0));
    CHECK_FALSE(resource_take_sound(NULL, sound));
    CHECK_FALSE(resource_take_sound(resmgr_uninit, sound));
    sound_destroy(sound);
    CHECK_FALSE(resource_take_sound(resmgr, 0));

    return 1;
}

/*************************************************************************/
/************************* Raw file access tests *************************/
/*************************************************************************/

TEST(test_open_read_file)
{
    int resid;
    char buf[5];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    CHECK_INTEQUAL(resource_get_file_size(resmgr, resid), 5);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_memory_failure)
{
    int resid;
    char buf[5];

    CHECK_RESOURCE_MEMORY_FAILURES(resid = resource_open_file(
                              resmgr_single, "testdata/test.txt"));
    CHECK_INTEQUAL(resource_get_file_size(resmgr_single, resid), 5);
    CHECK_INTEQUAL(resource_read_file(resmgr_single, resid, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_invalid)
{
    CHECK_FALSE(resource_open_file(NULL, "testdata/test.txt"));
    CHECK_FALSE(resource_open_file(resmgr_uninit, "testdata/test.txt"));
    CHECK_FALSE(resource_open_file(resmgr, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_file_size_invalid)
{
    int resid, resid2;

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    CHECK_FALSE(resource_get_file_size(NULL, resid));
    CHECK_FALSE(resource_get_file_size(resmgr_uninit, resid));
    CHECK_FALSE(resource_get_file_size(resmgr, 0));
    CHECK_FALSE(resource_get_file_size(resmgr, INT_MAX));

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    CHECK_FALSE(resource_get_file_size(resmgr, resid2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_file_position)
{
    int resid;
    char buf[3];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    resource_set_file_position(resmgr, resid, 2);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "llo", 3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_file_position_invalid)
{
    int resid, resid2;
    char buf[1];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    resource_set_file_position(NULL, resid, 2);
    resource_set_file_position(resmgr_uninit, resid, 2);
    resource_set_file_position(resmgr, 0, 2);
    resource_set_file_position(resmgr, INT_MAX, 2);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 1), 1);
    CHECK_INTEQUAL(buf[0], 'h');

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    resource_set_file_position(resmgr, resid2, 4);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 1), 1);
    CHECK_INTEQUAL(buf[0], 'e');

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_file_position)
{
    int resid;
    char buf[3];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 0);

    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "hel", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 3);

    resource_set_file_position(resmgr, resid, 2);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 2);

    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "llo", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_file_position_invalid)
{
    int resid, resid2;

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    resource_set_file_position(resmgr, resid, 2);
    CHECK_FALSE(resource_get_file_position(NULL, resid));
    CHECK_FALSE(resource_get_file_position(resmgr_uninit, resid));
    CHECK_FALSE(resource_get_file_position(resmgr, 0));
    CHECK_FALSE(resource_get_file_position(resmgr, INT_MAX));

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    CHECK_FALSE(resource_get_file_position(resmgr, resid2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_invalid)
{
    int resid, resid2;
    char buf[1];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    buf[0] = 1;
    CHECK_INTEQUAL(resource_read_file(NULL, resid, buf, 1), -1);
    CHECK_INTEQUAL(resource_read_file(resmgr_uninit, resid, buf, 1), -1);
    CHECK_INTEQUAL(resource_read_file(resmgr, 0, buf, 1), -1);
    CHECK_INTEQUAL(resource_read_file(resmgr, INT_MAX, buf, 1), -1);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, NULL, 1), -1);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, -1), -1);
    CHECK_INTEQUAL(buf[0], 1);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 1), 1);
    CHECK_INTEQUAL(buf[0], 'h');

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    buf[0] = 1;
    CHECK_INTEQUAL(resource_read_file(resmgr, resid2, buf, 1), -1);
    CHECK_INTEQUAL(buf[0], 1);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 1), 1);
    CHECK_INTEQUAL(buf[0], 'e');

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_at)
{
    int resid;
    char buf[4];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));

    memset(buf, 1, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 3, 1), 3);
    CHECK_MEMEQUAL(buf, "ell\1", 4);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 0);

    memset(buf, 2, sizeof(buf));
    resource_set_file_position(resmgr, resid, 1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 4, 5), 0);
    CHECK_MEMEQUAL(buf, "\2\2\2\2", 4);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 1);

    memset(buf, 3, sizeof(buf));
    resource_set_file_position(resmgr, resid, 2);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 1), 1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf+1, 3, 1), 3);
    CHECK_MEMEQUAL(buf, "lell", 4);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_at_invalid)
{
    int resid, resid2;
    char buf[1];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    buf[0] = 1;
    CHECK_INTEQUAL(resource_read_file_at(NULL, resid, buf, 1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr_uninit, resid, buf, 1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, 0, buf, 1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, INT_MAX, buf, 1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, NULL, 1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, -1, 1), -1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 1, -1), -1);
    CHECK_INTEQUAL(buf[0], 1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 1, 1), 1);
    CHECK_INTEQUAL(buf[0], 'e');

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    buf[0] = 1;
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid2, buf, 1, 2), -1);
    CHECK_INTEQUAL(buf[0], 1);
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 1, 2), 1);
    CHECK_INTEQUAL(buf[0], 'l');

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_file_handle)
{
    int resid;
    SysFile *fh;
    int64_t offset = -1;
    char buf[5];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    CHECK_TRUE(fh = resource_get_file_handle(resmgr, resid, &offset));
    CHECK_INTEQUAL(offset, 0);
    CHECK_INTEQUAL(sys_file_size(fh), 5);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_file_handle_invalid)
{
    int resid, resid2;
    int64_t offset;

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));
    CHECK_FALSE(resource_get_file_handle(NULL, resid, &offset));
    CHECK_FALSE(resource_get_file_handle(resmgr_uninit, resid, &offset));
    CHECK_FALSE(resource_get_file_handle(resmgr, 0, &offset));
    CHECK_FALSE(resource_get_file_handle(resmgr, INT_MAX, &offset));
    CHECK_FALSE(resource_get_file_handle(resmgr, resid, NULL));

    CHECK_TRUE(resid2 = resource_strdup(resmgr, "foobar", 0));
    CHECK_FALSE(resource_get_file_handle(resmgr, resid2, &offset));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_past_end)
{
    int resid;
    char buf[6];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));

    memset(buf, 1, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 6), 5);
    CHECK_MEMEQUAL(buf, "hello\1", 6);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);

    memset(buf, 2, sizeof(buf));
    resource_set_file_position(resmgr, resid, 2);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 6), 3);
    CHECK_MEMEQUAL(buf, "llo\2\2\2", 6);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);

    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file_at(resmgr, resid, buf, 6, 1), 4);
    CHECK_MEMEQUAL(buf, "ello\3\3", 6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_file_position_out_of_range)
{
    int resid;
    char buf[3];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/test.txt"));

    resource_set_file_position(resmgr, resid, 6);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);
    memset(buf, 1, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 0);
    CHECK_MEMEQUAL(buf, "\1\1\1", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);

    resource_set_file_position(resmgr, resid, -1);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 0);
    memset(buf, 2, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "hel", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 3);

    return 1;
}

/*************************************************************************/
/************************ Resource linking tests *************************/
/*************************************************************************/

TEST(test_link_data)
{
    int resid, resid2, resid3, mark;
    void *data, *data2, *data3;
    int size, size2;

    /* Set up a data resource for testing. */
    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    CHECK_TRUE(data = resource_get_data(resmgr, resid, NULL));

    /* Create a link within the same resource manager and ensure that
     * it points to the same data (i.e., not reallocated), and that it
     * persists after the first resource is freed. */
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 7);
    if (data2 != data) {
        FAIL("resource_get_data(resid2) != resource_get_data(resid)");
    }
    resource_free(resmgr, resid);
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 7);
    CHECK_STREQUAL(data2, "foobar");

    /* Check that the link is not reported as stale. */
    CHECK_FALSE(resource_is_stale(resmgr, resid2));

    /* Create two links to the same resource and ensure they are managed
     * correctly. */
    CHECK_TRUE(resid = resource_link(resmgr, resmgr, resid2));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 7);
    if (data != data2) {
        FAIL("resource_get_data(resid) != resource_get_data(resid2)");
    }
    CHECK_TRUE(resid3 = resource_link(resmgr, resmgr, resid2));
    if (resid3 == resid2) {
        FAIL("resource_link(resmgr, resmgr, resid2) == resid2");
    }
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    if (data3 != data2) {
        FAIL("resource_get_data(resid3) != resource_get_data(resid2)");
    }
    resource_free(resmgr, resid);
    resource_free(resmgr, resid3);

    /* Create a link from a different resource manager and ensure that
     * it also works properly. */
    CHECK_TRUE(resid3 = resource_link(resmgr2, resmgr, resid2));
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr2, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    if (data3 != data2) {
        FAIL("resource_get_data(resid3) != resource_get_data(resid2)");
    }
    /* Create a link to a link as well. */
    CHECK_TRUE(resid = resource_link(resmgr, resmgr2, resid3));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 7);
    if (data != data2) {
        FAIL("resource_get_data(resid) != resource_get_data(resid2)");
    }
    resource_free(resmgr, resid2);
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr2, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    CHECK_STREQUAL(data3, "foobar");
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 7);
    if (data != data3) {
        FAIL("resource_get_data(resid) != resource_get_data(resid3)");
    }
    resource_free(resmgr2, resid3);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 7);
    CHECK_STREQUAL(data, "foobar");
    resource_free(resmgr2, resid);

    /* Check that a link to a load-in-progress resource can be created. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    CHECK_FALSE(resource_get_data(resmgr, resid2, NULL));
    /* Check that data can be retrieved from the link after resource_wait()
     * on the original resource. */
    resource_wait(resmgr, mark);
    size = size2 = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid2, &size2));
    CHECK_INTEQUAL(size2, size);
    CHECK_MEMEQUAL(data2, data, size);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_texture)
{
    int resid, resid2, mark;
    int texture, texture2;

    /* Check that the texture is accessible if the link is made while the
     * load is in progress. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_texture(
               resmgr, "testdata/texture/4x4-rgba.tex", 0, 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    TEST_resource_block_load(0);
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(texture = resource_get_texture(resmgr, resid));
    CHECK_TRUE(texture2 = resource_get_texture(resmgr, resid2));
    CHECK_INTEQUAL(texture2, texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_font)
{
    int resid, resid2, mark;
    int font, font2;

    /* Check that the font is accessible if the link is made while the
     * load is in progress. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_bitmap_font(
               resmgr, "testdata/font/test.font", 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    TEST_resource_block_load(0);
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(font = resource_get_font(resmgr, resid));
    CHECK_TRUE(font2 = resource_get_font(resmgr, resid2));
    CHECK_INTEQUAL(font2, font);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_sound)
{
    int resid, resid2, mark;
    Sound *sound, *sound2;

    /* Check that the sound is accessible if the link is made while the
     * load is in progress. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_sound(
               resmgr, "testdata/sound/square.wav", 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    TEST_resource_block_load(0);
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_TRUE(sound2 = resource_get_sound(resmgr, resid2));
    CHECK_PTREQUAL(sound2, sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_pointer_update_on_expand)
{
    DEFINE_STATIC_RESOURCEMANAGER(test_resmgr, 2);
    int resid, resid2, resid3;

    /* Fill the 2 resource slots with a linked resource. */
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "foobar", 0));
    CHECK_TRUE(resid2 = resource_link(test_resmgr, test_resmgr, resid));

    /* Create a third link to the resource, forcing the ResourceInfo array
     * to be reallocated. */
    CHECK_TRUE(resid3 = resource_link(test_resmgr, test_resmgr, resid));

    /* Free each resource to make sure the linked list is properly updated. */
    resource_free(test_resmgr, resid);
    resource_free(test_resmgr, resid2);
    resource_free(test_resmgr, resid3);

    resource_destroy(test_resmgr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_pointer_update_on_expand_other_resmgr)
{
    ResourceManager *test_resmgr, *resmgr_low, *resmgr_high;
    int resid, resid2, resid3, resid4, resid5;

    /* Check that link pointers are updated properly when the ResourceInfo
     * list is expanded.  Since there are two cases to cover (pointer below
     * and pointer above), we create two additional ResourceManagers and
     * carefully poke inside them to ensure that their ResourceInfo arrays
     * bracket that of the ResourceManager under test. */

    /* Create a ResourceManager with 4 slots: 2 as in the update_on_expand()
     * test, plus 1 each for linking from the other two ResourceManagers.
     * We create these ResourceManagers dynamically so we can replace the
     * pointers below. */
    CHECK_TRUE(test_resmgr = resource_create(4));

    /* Create two more ResourceManagers, each with 1 slot. */
    CHECK_TRUE(resmgr_low = resource_create(1));
    CHECK_TRUE(resmgr_high = resource_create(1));

    /* Allocate three ResourceInfo buffers of 4 slots each and replace the
     * existing ResourceManager array buffers so that resmgr is bracketed
     * by resmgr_low and resmgr_high.  Note that the private->resources
     * field is located at the top of the ResourceManagerPrivate structure. */
    const int resinfo_size = _SIL_RESOURCE_SIZE2 * sizeof(void *);
    void *resinfo_low = mem_alloc(resinfo_size * 4, 0, 0);
    void *resinfo_mid = mem_alloc(resinfo_size * 4, 0, 0);
    void *resinfo_high = mem_alloc(resinfo_size * 4, 0, 0);
    if (resinfo_low > resinfo_mid) {
        void *temp = resinfo_low;
        resinfo_low = resinfo_mid;
        resinfo_mid = temp;
    }
    if (resinfo_low > resinfo_high) {
        void *temp = resinfo_low;
        resinfo_low = resinfo_high;
        resinfo_high = temp;
    }
    if (resinfo_mid > resinfo_high) {
        void *temp = resinfo_mid;
        resinfo_mid = resinfo_high;
        resinfo_high = temp;
    }
    ASSERT(resinfo_low < resinfo_mid);
    ASSERT(resinfo_mid < resinfo_high);
    void **resources_ptr_low = (void **)(resmgr_low->private);
    void **resources_ptr_mid = (void **)(test_resmgr->private);
    void **resources_ptr_high = (void **)(resmgr_high->private);
    memcpy(resinfo_low, *resources_ptr_low, resinfo_size * 1);
    memcpy(resinfo_mid, *resources_ptr_mid, resinfo_size * 4);
    memcpy(resinfo_high, *resources_ptr_high, resinfo_size * 1);
    mem_free(*resources_ptr_low);
    mem_free(*resources_ptr_mid);
    mem_free(*resources_ptr_high);
    *resources_ptr_low = resinfo_low;
    *resources_ptr_mid = resinfo_mid;
    *resources_ptr_high = resinfo_high;

    /* Fill the ResourceInfo slots with resources and links. */
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "foobar", 0));
    CHECK_TRUE(resid2 = resource_link(test_resmgr, test_resmgr, resid));
    CHECK_TRUE(resid3 = resource_strdup(resmgr_low, "foo", 0));
    CHECK_TRUE(resid3 = resource_link(test_resmgr, resmgr_low, resid3));
    CHECK_TRUE(resid4 = resource_strdup(resmgr_high, "bar", 0));
    CHECK_TRUE(resid4 = resource_link(test_resmgr, resmgr_high, resid4));

    /* Create another link to the local resource, forcing the ResourceInfo
     * array to be reallocated. */
    CHECK_TRUE(resid5 = resource_link(test_resmgr, test_resmgr, resid));

    /* Free each resource to make sure linked lists are properly updated. */
    resource_destroy(resmgr_low);
    resource_destroy(resmgr_high);
    resource_free(test_resmgr, resid);
    resource_free(test_resmgr, resid2);
    resource_free(test_resmgr, resid3);
    resource_free(test_resmgr, resid4);
    resource_free(test_resmgr, resid5);

    resource_destroy(test_resmgr);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that resource_sync() returns false for a link to a file that has
 * not yet been loaded. */
TEST(test_link_sync)
{
    int resid, resid2, mark;
    void *data;
    int size;

    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_link(resmgr2, resmgr, resid));
    CHECK_TRUE(mark = resource_mark(resmgr2));
    CHECK_FALSE(resource_sync(resmgr2, mark));
    CHECK_FALSE(resource_get_data(resmgr2, resid2, NULL));
    TEST_resource_block_load(0);
    resource_wait(resmgr2, mark);
    CHECK_TRUE(resource_sync(resmgr2, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr2, resid2, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a linked resource gets its own sync mark. */
TEST(test_link_no_copy_mark)
{
    int resid, resid2, resid3, mark, mark2, mark3;
    void *data;
    int size;

    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(resid2 = resource_load_data(resmgr2, "testdata/DIR3.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr2));
    CHECK_TRUE(resid3 = resource_link(resmgr2, resmgr, resid));
    CHECK_TRUE(mark3 = resource_mark(resmgr2));
    CHECK_INTEQUAL(mark2, mark);
    /* This should sync only DIR3.txt and not the link or its source. */
    resource_wait(resmgr2, mark2);
    CHECK_FALSE(resource_sync(resmgr, mark));
    CHECK_FALSE(resource_sync(resmgr2, mark3));
    TEST_resource_block_load(0);
    resource_wait(resmgr2, mark3);
    CHECK_TRUE(resource_sync(resmgr, mark));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr2, resid3, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_memory_failure)
{
    int resid, resid2;
    void *data, *data2;
    int size, size2;

    CHECK_TRUE(resid = resource_strdup(resmgr_single, "test", 0));
    CHECK_RESOURCE_MEMORY_FAILURES(
        resid2 = resource_link(resmgr_single, resmgr_single, 1));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr_single, resmgr_single, resid) == resid");
    }
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr_single, resid, &size));
    CHECK_TRUE(data2 = resource_get_data(resmgr_single, resid2, &size2));
    CHECK_INTEQUAL(size2, size);
    if (data2 != data) {
        FAIL("resource_get_data(resid2) != resource_get_data(resid)");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_infinite_loop)
{
    int resid, resid2, resid3;

    /* Set up a data resource for testing. */
    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));

    /* Create two links to the resource. */
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    CHECK_TRUE(resid3 = resource_link(resmgr, resmgr, resid));

    /* Point the two links at each other, so iteration from the original
     * resource will fall into an infinite loop. */
    TEST_resource_set_link_pointer(resmgr, resid2, resid3);
    TEST_resource_set_link_pointer(resmgr, resid3, resid2);

    /* Attempt to create a new link to the original resource.  This should
     * detect the infinite loop and fail. */
    CHECK_FALSE(resource_link(resmgr, resmgr, resid));

    /* Attempt to delete the original resource.  This should detect the
     * infinite loop and cut the resource out of the list. */
    resource_free(resmgr, resid);

    /* It should now be possible to add a link to the resource, since the
     * two remaining links for a proper circular list. */
    CHECK_TRUE(resid = resource_link(resmgr, resmgr, resid2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_infinite_loop_on_load)
{
    int resid, resid2, resid3;
    void *data;
    int size;

    /* Start loading a data resource for testing. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));

    /* Create two links to the resource. */
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    CHECK_TRUE(resid3 = resource_link(resmgr, resmgr, resid));

    /* Point the two links at each other, so iteration from the original
     * resource (which will be the first one processed by resource_wait())
     * will fall into an infinite loop. */
    TEST_resource_set_link_pointer(resmgr, resid2, resid3);
    TEST_resource_set_link_pointer(resmgr, resid3, resid2);

    /* Attempt to wait on the resources.  This should detect the
     * infinite loop and break out of the link-update loop. */
    resource_wait(resmgr, resource_mark(resmgr));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_infinite_loop_on_expand)
{
    DEFINE_STATIC_RESOURCEMANAGER(test_resmgr, 3);
    int resid, resid2, resid3;

    /* Fill the 3 resource slots with a linked resource. */
    CHECK_TRUE(resid = resource_strdup(test_resmgr, "foobar", 0));
    CHECK_TRUE(resid2 = resource_link(test_resmgr, test_resmgr, resid));
    CHECK_TRUE(resid3 = resource_link(test_resmgr, test_resmgr, resid));

    /* Point the two links at each other, so iteration from the original
     * resource will fall into an infinite loop. */
    TEST_resource_set_link_pointer(test_resmgr, resid2, resid3);
    TEST_resource_set_link_pointer(test_resmgr, resid3, resid2);

    /* Attempt to create another link to the resource.  This should detect
     * the infinite loop during array expansion, kill resid, and ultimately
     * fail. */
    CHECK_FALSE(resource_link(test_resmgr, test_resmgr, resid));

    resource_destroy(test_resmgr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_invalid)
{
    int resid;

    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    CHECK_FALSE(resource_link(NULL, resmgr, resid));
    CHECK_FALSE(resource_link(resmgr_uninit, resmgr, resid));
    CHECK_FALSE(resource_link(resmgr, NULL, resid));
    CHECK_FALSE(resource_link(resmgr, resmgr_uninit, resid));
    CHECK_FALSE(resource_link(resmgr, resmgr, 0));
    CHECK_FALSE(resource_link(resmgr, resmgr, INT_MAX));
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_link(resmgr, resmgr, resid));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_weak)
{
    int resid, resid2, resid3;
    void *data, *data2, *data3;
    int size;

    /* Set up a data resource for testing. */
    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    CHECK_TRUE(data = resource_get_data(resmgr, resid, NULL));

    /* Create a weak link within the same resource manager and ensure that
     * it points to the same data. */
    CHECK_TRUE(resid2 = resource_link_weak(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 7);
    if (data2 != data) {
        FAIL("resource_get_data(resid2) != resource_get_data(resid)");
    }

    /* Check that the link becomes stale when the source data is freed. */
    resource_free(resmgr, resid);
    CHECK_TRUE(resource_is_stale(resmgr, resid2));
    size = 1;
    CHECK_FALSE(resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 0);

    /* Check that freeing the stale link works. */
    resource_free(resmgr, resid2);

    /* Check that multiple weak links all go stale when the source data is
     * freed. */
    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    CHECK_TRUE(resid2 = resource_link_weak(resmgr, resmgr, resid));
    CHECK_TRUE(resid3 = resource_link_weak(resmgr, resmgr, resid));
    resource_free(resmgr, resid);
    CHECK_TRUE(resource_is_stale(resmgr, resid2));
    CHECK_TRUE(resource_is_stale(resmgr, resid3));
    size = 1;
    CHECK_FALSE(resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 0);
    size = 1;
    CHECK_FALSE(resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 0);

    /* Create regular and weak links to the same resource and ensure they
     * are managed correctly. */
    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 7);
    CHECK_STREQUAL(data, "foobar");
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    if (resid2 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    size = 0;
    CHECK_TRUE(data2 = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 7);
    if (data2 != data) {
        FAIL("resource_get_data(resid2) != resource_get_data(resid)");
    }
    CHECK_TRUE(resid3 = resource_link_weak(resmgr, resmgr, resid));
    if (resid3 == resid) {
        FAIL("resource_link(resmgr, resmgr, resid) == resid");
    }
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    if (data3 != data2) {
        FAIL("resource_get_data(resid3) != resource_get_data(resid2)");
    }
    /* Freeing the resource should _not_ destroy the weak link (since a
     * strong link remains). */
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_is_stale(resmgr, resid3));
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    if (data3 != data2) {
        FAIL("resource_get_data(resid3) != resource_get_data(resid2)");
    }
    /* If we create a second strong link to the resource and then destroy
     * the first one, the weak link should still be live. */
    CHECK_TRUE(resid = resource_link(resmgr, resmgr, resid2));
    if (resid == resid2) {
        FAIL("resource_link(resmgr, resmgr, resid2) == resid2");
    }
    resource_free(resmgr, resid2);
    CHECK_FALSE(resource_is_stale(resmgr, resid3));
    size = 0;
    CHECK_TRUE(data3 = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 7);
    if (data3 != data2) {
        FAIL("resource_get_data(resid3) != resource_get_data(resid2)");
    }
    /* Destroying the link should make the weak link stale. */
    resource_free(resmgr, resid);
    CHECK_TRUE(resource_is_stale(resmgr, resid3));

    /* Ensure that no links (strong or weak) can be made to a stale link. */
    CHECK_FALSE(resource_link(resmgr, resmgr, resid3));
    CHECK_FALSE(resource_link_weak(resmgr, resmgr, resid3));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_link_weak_invalid)
{
    int resid;

    CHECK_TRUE(resid = resource_strdup(resmgr, "foobar", 0));
    CHECK_FALSE(resource_link_weak(NULL, resmgr, resid));
    CHECK_FALSE(resource_link_weak(resmgr_uninit, resmgr, resid));
    CHECK_FALSE(resource_link_weak(resmgr, NULL, resid));
    CHECK_FALSE(resource_link_weak(resmgr, resmgr_uninit, resid));
    CHECK_FALSE(resource_link_weak(resmgr, resmgr, 0));
    CHECK_FALSE(resource_link_weak(resmgr, resmgr, INT_MAX));
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_link_weak(resmgr, resmgr, resid));
    CHECK_FALSE(resource_is_stale(NULL, resid));
    CHECK_FALSE(resource_is_stale(resmgr_uninit, resid));
    CHECK_FALSE(resource_is_stale(resmgr, 0));
    CHECK_FALSE(resource_is_stale(resmgr, INT_MAX));
    CHECK_FALSE(resource_is_stale(resmgr, resid));

    return 1;
}

/*************************************************************************/
/************************ Resource freeing tests *************************/
/*************************************************************************/

TEST(test_free)
{
    int resid, resid2, resid3, mark;
    void *data;
    int size;

    /* Free of a single resource should invalidate the resource ID. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_get_data(resmgr, resid, &size));

    /* Free-all should invalidate all resource IDs. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid != resid2);
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free_all(resmgr);
    CHECK_FALSE(resource_get_data(resmgr, resid, &size));
    CHECK_FALSE(resource_get_data(resmgr, resid2, &size));

    /* Free (and free-all) should abort pending load operations. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_get_data(resmgr, resid, &size));
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    resource_free_all(resmgr);
    CHECK_FALSE(resource_get_data(resmgr, resid, &size));
    CHECK_FALSE(resource_get_data(resmgr, resid2, &size));
    TEST_resource_block_load(0);

    /* Free should not abort pending loads if another link exists. */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(!resource_sync(resmgr, mark));
    resource_free(resmgr, resid);
    CHECK_FALSE(resource_sync(resmgr, mark));
    CHECK_FALSE(resource_get_data(resmgr, resid2, &size));
    TEST_resource_block_load(0);
    resource_wait(resmgr, mark);
    CHECK_TRUE(resource_get_data(resmgr, resid2, &size));
    resource_free(resmgr, resid2);

    /* Make sure free and free-all don't try to free the same LoadInfo
     * structure multiple times for links.  (We can't detect this
     * directly, but resource.c includes code to invalidate pointers on
     * free when running in debug mode, so we rely on the program to
     * crash in such cases.  Running under a memory checker like Valgrind
     * should also detect this problem.) */
    TEST_resource_block_load(1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    CHECK_TRUE(resid3 = resource_link_weak(resmgr, resmgr, resid));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_TRUE(!resource_sync(resmgr, mark));
    resource_free(resmgr, resid);
    resource_free(resmgr, resid2);
    CHECK_TRUE(resource_sync(resmgr, mark));
    resource_free(resmgr, resid3);  // Stale link.
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_link(resmgr, resmgr, resid));
    CHECK_TRUE(resid3 = resource_link_weak(resmgr, resmgr, resid));
    CHECK_TRUE(mark = resource_mark(resmgr));
    CHECK_FALSE(resource_sync(resmgr, mark));
    resource_free_all(resmgr);
    CHECK_TRUE(resource_sync(resmgr, mark));
    TEST_resource_block_load(0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_free_bad_type)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));

    /* Set the resource type to an invalid value, which will cause
     * resource_free() to ignore it.  (This is just to cover the final
     * branch on the switch(resinfo->type) block; this path should never
     * be taken outside tests.) */
    struct _ResourceInfo {int type;};
    struct _ResourceManagerPrivate {struct _ResourceInfo *resources;};
    struct _ResourceInfo *resources =
        ((struct _ResourceManagerPrivate *)(resmgr->private))->resources;
    CHECK_INTEQUAL(resources[0].type, 2);  // RES_DATA
    resources[0].type = -1;

    resource_free(resmgr, resid);
    CHECK_INTEQUAL(resources[0].type, 0);  // RES_UNUSED

    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_free_invalid)
{
    int resid, mark;

    /* Since free operations don't return values, we just check that the
     * calls don't crash. */
    CHECK_TRUE(resid = resource_load_data(resmgr, "testdata/test.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    resource_free(NULL, resid);
    resource_free(resmgr_uninit, resid);
    resource_free(resmgr, 0);
    resource_free(resmgr, INT_MAX);
    resource_free(resmgr, resid);
    resource_free(resmgr, resid);  // No crash on double-free.
    resource_free_all(NULL);
    resource_free_all(resmgr_uninit);

    return 1;
}

/*************************************************************************/
/************************** Basic package tests **************************/
/*************************************************************************/

TEST(test_package)
{
    /* Make sure unregistering the NULL module doesn't crash (this is
     * documented as a no-op). */
    resource_unregister_package(NULL);

    /* Test that the package module is properly initialized and closed,
     * and that the same module can't be registered twice.  Note that the
     * package has already been registered via init(). */
    CHECK_TRUE(package_module.module_data);
    CHECK_FALSE(resource_register_package(&package_module));
    resource_unregister_package(&package_module);
    CHECK_FALSE(package_module.module_data);

    /* Test that attempting to remove an unregistered package is handled
     * properly (doesn't crash). */
    CHECK_TRUE(resource_register_package(&package_module));
    resource_unregister_package(&second_module);
    resource_unregister_package(&package_module);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_register_multiple)
{
    //CHECK_TRUE(resource_register_package(&package_module));  // Already done.
    CHECK_TRUE(package_module.module_data);
    CHECK_TRUE(resource_register_package(&second_module));
    CHECK_TRUE(second_module.module_data);
    resource_unregister_package(&package_module);
    resource_unregister_package(&second_module);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_register_init_failure)
{
    second_module.module_data = (void *)1;  // Force failure.
    CHECK_FALSE(resource_register_package(&second_module));
    second_module.module_data = NULL;
    resource_unregister_package(&package_module);
    /* Make sure this doesn't crash. */
    resource_unregister_package(&second_module);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_exists)
{
    CHECK_TRUE(resource_exists("testdata/package/top.txt"));
    CHECK_TRUE(resource_exists("testdata/package/Top.Txt"));
    CHECK_TRUE(resource_exists("testdata/package/data/1.txt"));
    CHECK_TRUE(resource_exists("testdata/package/DATA/2.TXT"));
    CHECK_TRUE(resource_exists("Testdata/Package/Data/Copy/2.Txt"));
    CHECK_FALSE(resource_exists("testdata/package/data/3.txt"));
    CHECK_FALSE(resource_exists("testdata/package/data/1.txtt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_list_files)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/package", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "top.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "data.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    CHECK_TRUE(dir = resource_list_files_start("testdata/package/data", 0));
    /* This should not return the package file "data.txt". */
    CHECK_STREQUAL(resource_list_files_next(dir), "1.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "2.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    /* Check special handling for trailing slash in package prefix. */
    CHECK_TRUE(dir = resource_list_files_start("testdata/packag3", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "test.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    CHECK_FALSE(resource_list_files_start("testdata/package/data/1.txt", 0));
    CHECK_FALSE(resource_list_files_start("testdata/package/othe", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_list_files_recursive)
{
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/package/data", 1));
    CHECK_STREQUAL(resource_list_files_next(dir), "1.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "2.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "copy/2.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_list_files_no_slash_on_prefix)
{
    /* This is static so we don't have to worry about ensuring that the
     * module is removed before we return. */
    static PackageModuleInfo no_slash_module;
    no_slash_module = second_module;
    no_slash_module.prefix = "testdata/packag";
    CHECK_TRUE(resource_register_package(&no_slash_module));

    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("testdata/packag", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "top.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "data.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    CHECK_FALSE(resource_list_files_start("testdata/packag3", 0));

    CHECK_TRUE(dir = resource_list_files_start("testdata/second", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "file.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_list_files_non_package)
{
    ResourceFileListHandle *dir;

    /* Check that pathname lookup doesn't fail for non-package files. */
    CHECK_TRUE(resource_exists("testdata/package.dat"));
    CHECK_FALSE(resource_exists("testdata/package/testdata/package.dat"));
    CHECK_TRUE(dir = resource_list_files_start("testdata/dir1/dir2", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "File.Txt");
    CHECK_STREQUAL(resource_list_files_next(dir), NULL);
    resource_list_files_end(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_path_overlap)
{
    /* Check handling of paths which overlap with the package prefix. */
    CHECK_FALSE(resource_exists("testdata/package/testdata/package.dat"));
    CHECK_FALSE(resource_list_files_start("testdata/package/file", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file)
{
    int resid;
    char buf[3];

    CHECK_TRUE(resid = resource_open_file(resmgr, "testdata/package/top.txt"));
    CHECK_INTEQUAL(resource_get_file_size(resmgr, resid), 3);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "top", 3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file_not_at_beginning)
{
    int resid;
    char buf[5];

    CHECK_TRUE(resid = resource_open_file(
                   resmgr, "testdata/package/data/2.txt"));
    CHECK_INTEQUAL(resource_get_file_size(resmgr, resid), 5);
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "world", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file_memory_failure)
{
    int resid;
    char buf[5];

    CHECK_RESOURCE_MEMORY_FAILURES(resid = resource_open_file(
                              resmgr_single, "testdata/package/data/2.txt"));
    CHECK_INTEQUAL(resource_get_file_size(resmgr_single, resid), 5);
    CHECK_INTEQUAL(resource_read_file(resmgr_single, resid, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "world", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file_compressed)
{
    CHECK_FALSE(resource_open_file(resmgr, "testdata/package/other/0.txt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file_not_in_package)
{
    CHECK_FALSE(resource_open_file(resmgr, "testdata/package/file/test.txt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_file_path_overlap)
{
    CHECK_FALSE(resource_open_file(resmgr, "testdata/package/data/test.txt"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_set_file_position_out_of_range)
{
    int resid;
    char buf[3];

    CHECK_TRUE(resid = resource_open_file(
                   resmgr, "testdata/package/data/2.txt"));

    resource_set_file_position(resmgr, resid, 6);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);
    memset(buf, 1, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 0);
    CHECK_MEMEQUAL(buf, "\1\1\1", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 5);

    resource_set_file_position(resmgr, resid, -1);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 0);
    memset(buf, 2, sizeof(buf));
    CHECK_INTEQUAL(resource_read_file(resmgr, resid, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "wor", 3);
    CHECK_INTEQUAL(resource_get_file_position(resmgr, resid), 3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_get_file_handle_not_at_beginning)
{
    int resid;
    SysFile *fh;
    int64_t offset = -1;
    char buf[5];

    CHECK_TRUE(resid = resource_open_file(
                   resmgr, "testdata/package/data/2.txt"));
    CHECK_TRUE(fh = resource_get_file_handle(resmgr, resid, &offset));
    CHECK_INTEQUAL(offset, 8);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "world", 5);

    return 1;
}

/*************************************************************************/
/************************* Package loading tests *************************/
/*************************************************************************/

TEST(test_package_data_load)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/data/1.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_data_load_overlap)
{
    /* This file exists in the directory tree, but lookup should fail
     * because the package overlays testdata/package/. */
    CHECK_FALSE(resource_load_data(
                    resmgr, "testdata/package/file/test.txt", 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_data_load_memory_failure)
{
    int resid;
    void *data;
    int size;

    size = 0;
    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_data(
            resmgr_single, "testdata/package/data/1.txt", 0, 0),
        data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_data_load_read_call_failure)
{
    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_FALSE(resource_load_data(
                    resmgr, "testdata/package/data/1.txt", 0, 0));

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_data_load_transient_read_call_failure)
{
    int resid, mark;
    void *data;
    int size;

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/data/1.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_data_load_read_failure)
{
    int resid, mark;

    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/data/1.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_short_read)
{
    int resid, mark;

    /* Check that short read errors on load are handled properly.  This is
     * not a package-specific test, but we use a package file entry with
     * invalid data to force a read failure in a platform-independent way. */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/error/shortread.dat", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    /* Also check using resource_sync() instead of resource_wait(). */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/error/shortread.dat", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_data_load_async_overload)
{
    int resid, mark;
    void *data;
    int size;

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int i;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (i = 1; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/data/1.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    ASSERT(sys_file_wait_async(reqlist[--i]) == 1);
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);

    resource_free(resmgr, resid);
    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }
    sys_file_close(fh);
    return 1;
}

/*************************************************************************/
/************************** Decompression tests **************************/
/*************************************************************************/

TEST(test_package_decompress)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    /* Check resource_sync() behavior with background decompression as well. */
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_size_limit)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 10000, 10000, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_change_size_limit)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 10000, 10000, THREAD_POOL_SIZE);
    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    /* There should be no visible difference between setting the
     * decompression parameters once or multiple times; just make sure
     * there's no memory leak at the end. */

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_trailing_junk)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/x.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "barfoo", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_trailing_junk)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/x.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "barfoo", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_memory_failure)
{
    int resid;
    void *data;
    int size;

    size = 0;
    CHECK_LOAD_MEMORY_FAILURES(0,
        resid = resource_load_data(
            resmgr_single, "testdata/package/other/0.txt", 0, 0),
        data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_init_failure)
{
    int resid, mark;
    void *data;
    int size;

    TEST_mem_fail_after(0, 1, 0);
    /* This call will fail. */
    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);
    TEST_mem_fail_after(-1, 0, 0);

    /* Decompression should still work (it'll just take place in the
     * foreground). */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_memory_failure)
{
    int resid;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    size = 0;
    /* For this test, we also force failure of shrinking mem_realloc()
     * calls to test failure to resize the read buffer from uncompressed
     * to compressed size after thread creation failure.  Normally, this
     * can only happen if the compressed data is larger than the
     * uncompressed data, which itself will typically never occur (the
     * data would be stored uncompressed in that case). */
    CHECK_LOAD_MEMORY_FAILURES(1,
        resid = resource_load_data(
            resmgr_single, "testdata/package/other/0.txt", 0, 0),
        data = resource_get_data(resmgr_single, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_bad_data)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/y.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_bad_data)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/y.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    /* Different code path. */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/q.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    /* Yet another code path. */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/z.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_short_read)
{
    int resid, mark;
    void *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/error/shortread.z", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_short_read)
{
    int resid, mark;
    void *data;
    int size;

    /* The file has 3 bytes available out of 4 needed, so set up to get a
     * partial read on the second read operation. */
    resource_set_background_decompression(1, 2, 2, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/error/shortread.z", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    /* Another code path (short read on the first read operation). */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/error/shortread2.z", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_FALSE(data = resource_get_data(resmgr, resid, &size));

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_decompress_background_read_call_failure)
{
    int resid, mark;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_decompress_background_transient_read_call_failure)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_package_decompress_background_read_failure)
{
    int resid, mark;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
    resource_free(resmgr, resid);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_thread_pool_full)
{
    int resid[THREAD_POOL_SIZE+2], mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    for (int i = 0; i < lenof(resid); i++) {
        resid[i] = resource_load_data(
            resmgr, "testdata/package/other/0.txt", 0, 0);
        if (!resid[i]) {
            FAIL("resource_load_data(resmgr, \"testdata/package/other/0.txt\","
                 " 0, 0) failed for iteration %d/%d", i, lenof(resid));
        }
    }
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    for (int i = 0; i < lenof(resid); i++) {
        size = 0;
        data = resource_get_data(resmgr, resid[i], &size);
        if (!data) {
            FAIL("resource_get_data(resmgr, resid[i], &size) failed for"
                 " iteration %d/%d", i, lenof(resid));
        }
        if (size != 6) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " size (%d, should be 6) for iteration %d/%d",
                 size, i, lenof(resid));
        }
        if (memcmp(data, "foobar", 6) != 0) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " data for iteration %d/%d: %02X %02X %02X %02X %02X %02X",
                 i, lenof(resid), ((uint8_t *)data)[0], ((uint8_t *)data)[1],
                 ((uint8_t *)data)[2], ((uint8_t *)data)[3],
                 ((uint8_t *)data)[4], ((uint8_t *)data)[5]);
        }
        resource_free(resmgr, resid[i]);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_thread_pool_full_memory_failure)
{
    int resid[THREAD_POOL_SIZE+1], mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    /* Prime asynchronous read tables so CHECK_MEMORY_FAILURES doesn't
     * affect I/O.  Note that each background decompression makes two
     * parallel read requests (the last request will be decompressed in
     * the foreground so it only needs one request). */
    {
        SysFile *fh;
        ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
        char buf[1];
        int req[THREAD_POOL_SIZE*2+1];
        for (int i = 0; i < lenof(req); i++) {
            ASSERT(req[i] = sys_file_read_async(fh, buf, 1, 0, -1));
        }
        for (int i = 0; i < lenof(req); i++) {
            ASSERT(sys_file_wait_async(req[i]) == 1);
        }
        sys_file_close(fh);
    }

    for (int i = 0; i < lenof(resid)-1; i++) {
        resid[i] = resource_load_data(
            resmgr, "testdata/package/other/0.txt", 0, 0);
        if (!resid[i]) {
            FAIL("resource_load_data(resmgr, \"testdata/package/other/0.txt\","
                 " 0, 0) failed for iteration %d/%d", i, lenof(resid)-1);
        }
    }
    /* For this test, we want to fail on the shrinking realloc that occurs
     * when falling back from background to foreground decompression so we
     * cover all out-of-memory paths. */
    CHECK_MEMORY_FAILURES_SHRINK(
        resid[lenof(resid)-1] = resource_load_data(
            resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    for (int i = 0; i < lenof(resid); i++) {
        size = 0;
        data = resource_get_data(resmgr, resid[i], &size);
        if (!data) {
            FAIL("resource_get_data(resmgr, resid[i], &size) failed for"
                 " iteration %d/%d", i, lenof(resid));
        }
        if (size != 6) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " size (%d, should be 6) for iteration %d/%d",
                 size, i, lenof(resid));
        }
        if (memcmp(data, "foobar", 6) != 0) {
            FAIL("resource_get_data(resmgr, resid[i], &size) returned wrong"
                 " data for iteration %d/%d: %02X %02X %02X %02X %02X %02X",
                 i, lenof(resid), ((uint8_t *)data)[0], ((uint8_t *)data)[1],
                 ((uint8_t *)data)[2], ((uint8_t *)data)[3],
                 ((uint8_t *)data)[4], ((uint8_t *)data)[5]);
        }
        resource_free(resmgr, resid[i]);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_free)
{
    int resid, mark;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    /* Make sure freeing a resource while background decompression is in
     * progress doesn't break things.  Check both resource_sync() and
     * resource_wait() for completeness. */
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_free(resmgr, resid);
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_free(resmgr, resid);
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

#ifdef USING_IOQUEUE
    /* The same thing with low-level I/O blocking. */
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_misc_ioqueue_block_io_thread(0);
    resource_free(resmgr, resid);
    while (!resource_sync(resmgr, mark)) {
        thread_yield();
    }
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));

    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    TEST_misc_ioqueue_block_io_thread(0);
    resource_free(resmgr, resid);
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, NULL));
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_async_overload)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int reqnum;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (reqnum = 1; reqnum < lenof(reqlist); reqnum++) {
        if (!(reqlist[reqnum] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (reqnum >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    /* We can't confirm directly that the background decompression has
     * started, so just spin a bit and rely on coverage data to confirm
     * that the branch has been taken. */
    for (int i = 0; i < 100; i++) {
        CHECK_FALSE(resource_sync(resmgr, mark));
        thread_yield();
    }

    ASSERT(sys_file_wait_async(reqlist[--reqnum]) == 1);
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);

    resource_free(resmgr, resid);
    for (reqnum--; reqnum >= 0; reqnum--) {
        ASSERT(sys_file_wait_async(reqlist[reqnum]) == 1);
    }
    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

/*
 * This test checks that resource loading does not deadlock in the
 * following case:
 *    - A resource "R" is being loaded with background decompression.
 *    - At least one resource has been scheduled for loading after "R",
 *      without background decompression.
 *    - The asynchronous read table becomes full, preventing background
 *      decompression from reading any additional data.
 * In this case, a resource_wait() operation on "R" should allow any
 * submitted asynchronous reads to complete even if they were loaded after
 * the load operation for "R".  (Failure to do so will cause this test to
 * deadlock.)
 */
TEST(test_package_decompress_background_async_overload_2)
{
    int resid, resid2, mark, mark2;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);
    package_block_decompress = 1;
    ASSERT(package_decompress_blocked_sema = semaphore_create(0, 1));
    ASSERT(package_decompress_unblock_sema = semaphore_create(0, 1));

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/h.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));

    semaphore_wait(package_decompress_blocked_sema);
    /* The decompress thread now has one pending read operation, on the
     * second byte of the compressed data. */

    CHECK_TRUE(resid2 = resource_load_data(
                   resmgr, "testdata/package/top.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));

    SysFile *fh;
    ASSERT(fh = wrap_sys_file_open("testdata/test.txt"));
    char buf[1];
    int reqlist[1000];
    int reqnum;
    ASSERT(reqlist[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    for (reqnum = 1; reqnum < lenof(reqlist)-1; reqnum++) {
        if (!(reqlist[reqnum] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (reqnum >= lenof(reqlist)-1) {
        package_block_decompress = 0;
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    semaphore_signal(package_decompress_unblock_sema);
    semaphore_wait(package_decompress_blocked_sema);
    /* The decompress thread now has no pending read operations, and there
     * should be exactly one free read operation slot. */
    ASSERT(reqlist[reqnum++] = sys_file_read_async(fh, buf, 1, 0, -1));
    ASSERT(!sys_file_read_async(fh, buf, 1, 0, -1));

    /* Check that resource_wait() does not deadlock on the background
     * decompression. */
    package_block_decompress = 0;
    semaphore_signal(package_decompress_unblock_sema);
    resource_wait(resmgr, mark);

    /* Check that all data was loaded correctly.  (This exercises the code
     * path for background decompression without background readahead.) */
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 8);
    CHECK_MEMEQUAL(data, "hogepiyo", 8);
    resource_wait(resmgr, mark2);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "top", 3);

    resource_free(resmgr, resid);
    resource_free(resmgr, resid2);
    for (reqnum--; reqnum >= 0; reqnum--) {
        ASSERT(sys_file_wait_async(reqlist[reqnum]) == 1);
    }
    sys_file_close(fh);

    semaphore_destroy(package_decompress_blocked_sema);
    semaphore_destroy(package_decompress_unblock_sema);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_background_failure)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 1, 1, THREAD_POOL_SIZE);

    /* Make sure failure to create background decompression state doesn't
     * prevent the data from being loaded. */
    package_error_from_decompress_init = 1;
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);
    package_error_from_decompress_init = 0;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_decompress_invalid_background_params)
{
    int resid, mark;
    void *data;
    int size;

    resource_set_background_decompression(1, 0, 0, THREAD_POOL_SIZE);

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*************************************************************************/
/********************* Sound-specific package tests **********************/
/*************************************************************************/

TEST(test_package_load_compressed_sound)
{
    int resid, mark;
    Sound *sound;
    int16_t pcm[10];

    sys_test_sound_set_output_rate(256);
    sound_init();
    ASSERT(sound_open_device("", 3));

    /* Load of a compressed file (should succeed). */
    CHECK_TRUE(resid = resource_load_sound(
                   resmgr, "testdata/package/sound/c.wav", 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    sound_mixer_get_pcm(pcm, 2);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 256);
    CHECK_INTEQUAL(pcm[1], 256);
    CHECK_INTEQUAL(pcm[2], 512);
    CHECK_INTEQUAL(pcm[3], 512);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_sound)
{
    int resid, mark;
    Sound *sound;
    int16_t pcm[10];

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    /* Streaming open of an uncompressed file. */
    CHECK_TRUE(resid = resource_open_sound(
                   resmgr, "testdata/package/sound/s.wav"));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_sound_memory_failure)
{
    int resid, mark;
    Sound *sound;
    int16_t pcm[10];

    sys_test_sound_set_output_rate(4000);
    sound_init();
    ASSERT(sound_open_device("", 3));

    /* Streaming open of an uncompressed file. */
    CHECK_MEMORY_FAILURES(resid = resource_open_sound(
                              resmgr, "testdata/package/sound/s.wav"));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_TRUE(sound = resource_get_sound(resmgr, resid));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    sound_mixer_get_pcm(pcm, 5);
    sound_update();
    sound_cleanup();
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_sound_overlay)
{
    CHECK_FALSE(resource_open_sound(
                    resmgr, "testdata/package/sound/square.wav"));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_package_open_sound_compressed)
{
    /* Streaming open of a compressed file (should fail). */
    CHECK_FALSE(resource_open_sound(resmgr, "testdata/package/sound/c.wav"));

    return 1;
}

/*************************************************************************/
/********************* Resource sync direction tests *********************/
/*************************************************************************/

/*
 * For these tests, we use compressed files from the test package in order
 * to detect finalization order (by checking the data pointers used for
 * uncompressed data).
 */

/*-----------------------------------------------------------------------*/

TEST(test_sync_forward)
{
    int resid, resid2, resid3, mark2, mark3;
    void *data, *buf;
    int size;

    TEST_resource_override_sync_order(1, 0);
    ASSERT(buf = mem_alloc(6, 0, 0));

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));
    CHECK_TRUE(resid3 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark3 = resource_mark(resmgr));
    TEST_mem_use(buf);
    while (!resource_sync(resmgr, mark2)) {
        thread_yield();
    }
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_PTREQUAL(data, buf);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);
    CHECK_FALSE(resource_get_data(resmgr, resid3, &size));

    while (!resource_sync(resmgr, mark3)) {
        thread_yield();
    }
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sync_reverse)
{
    int resid, resid2, resid3, mark2, mark3;
    void *data, *buf;
    int size;

    TEST_resource_override_sync_order(1, 1);
    ASSERT(buf = mem_alloc(6, 0, 0));

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));
    CHECK_TRUE(resid3 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark3 = resource_mark(resmgr));
    TEST_mem_use(buf);
    while (!resource_sync(resmgr, mark2)) {
        thread_yield();
    }
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_PTREQUAL(data, buf);
    CHECK_FALSE(resource_get_data(resmgr, resid3, &size));

    while (!resource_sync(resmgr, mark3)) {
        thread_yield();
    }
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wait_forward)
{
    int resid, resid2, resid3, mark2, mark3;
    void *data, *buf;
    int size;

    TEST_resource_override_sync_order(1, 0);
    ASSERT(buf = mem_alloc(6, 0, 0));

    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));
    CHECK_TRUE(resid3 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark3 = resource_mark(resmgr));
    TEST_mem_use(buf);
    resource_wait(resmgr, mark2);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_PTREQUAL(data, buf);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);
    CHECK_FALSE(resource_get_data(resmgr, resid3, &size));

    resource_wait(resmgr, mark3);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wait_reverse)
{
    int resid, resid2, resid3, mark2, mark3;
    void *data, *buf;
    int size;

    TEST_resource_override_sync_order(1, 1);
    ASSERT(buf = mem_alloc(6, 0, 0));
    CHECK_TRUE(resid = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark2 = resource_mark(resmgr));
    CHECK_TRUE(resid3 = resource_load_data(
                   resmgr, "testdata/package/other/0.txt", 0, 0));
    CHECK_TRUE(mark3 = resource_mark(resmgr));

    TEST_mem_use(buf);
    resource_wait(resmgr, mark2);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_PTREQUAL(data, buf);
    CHECK_FALSE(resource_get_data(resmgr, resid3, &size));

    resource_wait(resmgr, mark3);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid3, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    CHECK_FALSE(data == buf);

    return 1;
}

/*************************************************************************/
/******************** Package module used for testing ********************/
/*************************************************************************/

/*
 * This module implements a package file rooted at "testdata/package/",
 * with these embedded files:
 *    - testdata/package/top.txt, contents "top"
 *    - testdata/package/data.txt, contents "ophe" (overlaps adjacent files)
 *    - testdata/package/data/1.txt, contents "hello"
 *    - testdata/package/data/2.txt, contents "world"
 *    - testdata/package/other/0.txt, contents "foobar" (compressed)
 *    - testdata/package/other/x.txt, contents "barfoo" (compressed) + junk
 *    - testdata/package/other/y.txt, corrupt compressed contents
 *    - testdata/package/other/q.txt, truncated compressed contents
 *    - testdata/package/other/z.txt, corrupt contents (but first byte valid)
 *    - testdata/package/sound/c.wav, compressed WAV file (mono, 256Hz, len=2)
 *    - testdata/package/sound/s.wav, same data as testdata/sound/square.wav
 *    - testdata/package/error/shortread.dat, extends beyond end of phys. file
 *    - testdata/package/error/shortread.z, compressed and extends beyond end
 *    - testdata/package/error/shortread2.z, compressed and extends beyond end
 * The package data is located at "testdata/package.dat".
 */

/* Data file list. */
static struct {
    const char *path;
    int offset, length;
    int compressed;
    int uncompressed_size;
} package_files[] = {
    {"top.txt",               0,   3, 0,  0},
    {"data.txt",              1,   4, 0,  0},
    {"data/1.txt",            3,   5, 0,  0},
    {"data/2.txt",            8,   5, 0,  0},
    {"data/copy/2.txt",       8,   5, 0,  0},
    {"other/0.txt",          13,   2, 1,  6},
    {"other/x.txt",          15,   3, 1,  6},
    {"other/h.txt",          18,   5, 1,  8},
    {"other/y.txt",          22,   2, 1,  6},
    {"other/q.txt",          24,   2, 1,  9},
    {"other/z.txt",          26,   2, 1,  6},
    {"sound/c.wav",          28,   1, 1, 48},
    {"sound/s.wav",          28, 124, 0,  0},
    {"error/shortread.z",   152,   4, 1, 12},
    {"error/shortread2.z",  154,   4, 1, 12},
    {"error/shortread.dat", 154,   2, 0,  0},
};

/* Internal data for the package module. */
typedef struct TestPackagePrivate TestPackagePrivate;
struct TestPackagePrivate {
    SysFile *fh;     // Persistent file handle for package data.
    int list_index;  // Index of next file for list_files_next().
};

/* Decompressor data for the package module. */
typedef struct TestPackageDecompressState TestPackageDecompressState;
struct TestPackageDecompressState {
    int size;        // Number of input bytes to expect.
    int bytes_read;  // Number of input bytes read.
};

/*-----------------------------------------------------------------------*/

static int package_init(PackageModuleInfo *module)
{
    if (module->module_data) {
        DLOG("module_data is already non-NULL, failing");
        return 0;
    }

    TestPackagePrivate *private = mem_alloc(sizeof(*private), 0, 0);
    if (!private) {
        DLOG("Out of memory");
        return 0;
    }

    private->fh = wrap_sys_file_open("testdata/package.dat");
    if (!private->fh) {
        DLOG("Failed to open test package file");
        mem_free(private);
        return 0;
    }
    private->list_index = -1;

    module->module_data = private;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void package_cleanup(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return);
    PRECOND(module->module_data != NULL, return);
    TestPackagePrivate *private = (TestPackagePrivate *)module->module_data;

    sys_file_close(private->fh);
    mem_free(private);
    module->module_data = NULL;  // Checked to ensure cleanup was performed.
}

/*-----------------------------------------------------------------------*/

static void package_list_files_start(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return);
    PRECOND(module->module_data != NULL, return);
    TestPackagePrivate *private = (TestPackagePrivate *)module->module_data;

    private->list_index = 0;
}

/*-----------------------------------------------------------------------*/

static const char *package_list_files_next(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return NULL);
    PRECOND(module->module_data != NULL, return NULL);
    TestPackagePrivate *private = (TestPackagePrivate *)module->module_data;

    if (private->list_index < 0) {
        DLOG("package_list_files_start() was never called!");
        return NULL;
    }
    if (private->list_index < lenof(package_files)) {
        return package_files[private->list_index++].path;
    } else {
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

static int package_file_info(PackageModuleInfo *module,
                             const char *path, SysFile **file_ret,
                             int64_t *pos_ret, int *len_ret,
                             int *comp_ret, int *size_ret)
{
    PRECOND(module != NULL, return 0);
    PRECOND(module->module_data != NULL, return 0);
    TestPackagePrivate *private = (TestPackagePrivate *)module->module_data;

    PRECOND(path != NULL, return 0);
    PRECOND(file_ret != NULL, return 0);
    PRECOND(pos_ret != NULL, return 0);
    PRECOND(len_ret != NULL, return 0);
    PRECOND(comp_ret != NULL, return 0);
    PRECOND(size_ret != NULL, return 0);

    for (int i = 0; i < lenof(package_files); i++) {
        if (stricmp(path, package_files[i].path) == 0) {
            *file_ret = private->fh;
            *pos_ret = package_files[i].offset;
            *len_ret = package_files[i].length;
            *comp_ret = package_files[i].compressed;
            *size_ret = (package_files[i].compressed
                         ? package_files[i].uncompressed_size
                         : package_files[i].length);
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static int package_decompress_get_stack_size(PackageModuleInfo *module)
{
    PRECOND(module != NULL);
    PRECOND(module->module_data != NULL);

    return 4096;
}

/*-----------------------------------------------------------------------*/

static void *package_decompress_init(PackageModuleInfo *module)
{
    PRECOND(module != NULL, return NULL);
    PRECOND(module->module_data != NULL, return NULL);

    if (package_error_from_decompress_init) {
        return NULL;
    }

    TestPackageDecompressState *state = mem_alloc(sizeof(*state), 0, 0);
    if (!state) {
        DLOG("Out of memory");
        return NULL;
    }
    state->bytes_read = 0;
    return state;
}

/*-----------------------------------------------------------------------*/

static int package_decompress(PackageModuleInfo *module, void *state_,
                              const void *in, int insize,
                              void *out, int outsize)
{
    PRECOND(module != NULL, return 0);
    PRECOND(module->module_data != NULL, return 0);

    PRECOND(in != NULL, return 0);
    PRECOND(out != NULL, return 0);

    if (package_block_decompress) {
        semaphore_signal(package_decompress_blocked_sema);
        semaphore_wait(package_decompress_unblock_sema);
    }

    TestPackageDecompressState *state = (TestPackageDecompressState *)state_;
    TestPackageDecompressState dummy_state;
    if (!state) {
        state = &dummy_state;
        state->bytes_read = 0;
    }

    if (state->bytes_read == 0) {
        if (insize > 0) {
            const char ch0 = ((const uint8_t *)in)[0];
            state->size = (ch0=='h' ? 4 : ch0=='q' ? 3 : ch0=='R' ? 1 : 2);
        } else {
            state->size = 2;  // Just avoid failing out below.
        }
    }
    if (state->bytes_read >= state->size
     || insize > state->size - state->bytes_read) {
        insize = state->size - state->bytes_read;
    }
    if (outsize < (state->size==1 ? 48 : state->size==4 ? 8 : 3*insize)) {
        DLOG("Not enough output space (outsize=%u but need %u bytes)",
             outsize, (state->size==1 ? 48 : 3*insize));
        return 0;
    }

    out = (void *)((uint8_t *)out
                   + (state->bytes_read * (state->size==4 ? 2 : 3)));
    for (int i = 0; i < insize; i++, state->bytes_read++) {
        const uint8_t byte = ((const uint8_t *)in)[i];
        switch (byte) {
          case 'f':
            memcpy(&((uint8_t *)out)[i*3], "foo", 3);
            break;
          case 'b':
            memcpy(&((uint8_t *)out)[i*3], "bar", 3);
            break;
          case 'q':
            memcpy(&((uint8_t *)out)[i*3], "qux", 3);
            break;
          case 'h':
            memcpy(&((uint8_t *)out)[i*2], "ho", 2);
            break;
          case 'g':
            memcpy(&((uint8_t *)out)[i*2], "ge", 2);
            break;
          case 'p':
            memcpy(&((uint8_t *)out)[i*2], "pi", 2);
            break;
          case 'y':
            memcpy(&((uint8_t *)out)[i*2], "yo", 2);
            break;
          case 'R':
            memcpy(out, ("RIFF\x28\0\0\0WAVE"
                         "fmt \x10\0\0\0\1\0\1\0\0\1\0\0\0\2\0\0\2\0\x10\0"
                         "data\4\0\0\0\0\1\0\2"), 48);
            break;
          default:
            DLOG("Invalid byte 0x%02X at offset %u", byte, state->bytes_read);
            return 0;
        }
    }

    if (state->bytes_read == state->size) {
        return 1;
    } else if (state == &dummy_state) {
        return 0;
    } else {
        return -1;
    }
}

/*-----------------------------------------------------------------------*/

static void package_decompress_finish(PackageModuleInfo *module, void *state)
{
    PRECOND(module != NULL, return);
    PRECOND(module->module_data != NULL, return);

    PRECOND(state != NULL, return);

    mem_free(state);
}

/*************************************************************************/
/*************************************************************************/
