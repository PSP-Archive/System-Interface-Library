/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/resource/package-pkg.c: Tests for PKG-format package file handling.
 */

#include "src/base.h"
#include "src/resource.h"
#include "src/resource/package.h"
#include "src/resource/package-pkg.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

static PackageModuleInfo *package;
DEFINE_STATIC_RESOURCEMANAGER(resmgr, 100);

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_resource_pkg)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    resource_init();

    /*
     * Open the PKG file we'll use for most of the tests below (this also
     * serves to check that a real PKG file can be successfully opened).
     * This package contains five files, stored in this order:
     *    - "P0.txt", contents "hello"
     *    - "P4.txt", contents "foobar"
     *    - "02.txt", contents "world" (has the same path hash as "P0.txt")
     *    - "02.4zt", corrupted compressed contents (has the same path hash
     *         as "P0.txt")
     *    - "DIR/square.dat", a 256-byte file where each byte is the low
     *         8 bits of the square of the byte's position: i.e.,
     *         uint8_t data[] = {0*0, 1*1, 2*2, 3*3, ..., 255*255}
     */
    CHECK_TRUE(package = pkg_create_instance("testdata/pkg/test.pkg", "pkg:"));
    CHECK_TRUE(resource_register_package(package));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    resource_destroy(resmgr);
    resource_cleanup();
    pkg_destroy_instance(package);
    sys_file_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_destroy_null_pointer)
{
    /* Just make sure it doesn't crash. */
    pkg_destroy_instance(NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_nonexistent_file)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/nonexistent.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_tiny_file)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/too-small.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_invalid_header)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/bad-magic.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/bad-header-size.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/bad-entry-size.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_truncated_index)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/short-index.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_truncated_pathnames)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/bad/short-pathdata.pkg", "pkg:"));
    CHECK_FALSE(resource_register_package(package2));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_exists)
{
    /* Check existence of pathnames for both original and different case. */
    CHECK_TRUE(resource_exists("pkg:P0.txt"));
    CHECK_TRUE(resource_exists("pkg:p0.Txt"));
    CHECK_TRUE(resource_exists("pkg:02.txt"));
    CHECK_TRUE(resource_exists("pkg:02.TXT"));
    CHECK_TRUE(resource_exists("pkg:DIR/square.dat"));
    CHECK_TRUE(resource_exists("pkg:dir/Square.DAT"));
    CHECK_FALSE(resource_exists("pkg:no.such.file"));
    CHECK_FALSE(resource_exists("pkg:no/such/file"));
    CHECK_FALSE(resource_exists("pkg:DIR"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_list_files)
{
    /* Files will be returned in hash order, and in lexical order for files
     * with the same hash. */
    ResourceFileListHandle *dir;

    CHECK_TRUE(dir = resource_list_files_start("pkg:", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "02.4zt");
    CHECK_STREQUAL(resource_list_files_next(dir), "02.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "P0.txt");
    CHECK_STREQUAL(resource_list_files_next(dir), "P4.txt");
    CHECK_FALSE(resource_list_files_next(dir));
    resource_list_files_end(dir);

    CHECK_TRUE(dir = resource_list_files_start("pkg:dir", 0));
    CHECK_STREQUAL(resource_list_files_next(dir), "square.dat");
    CHECK_FALSE(resource_list_files_next(dir));
    resource_list_files_end(dir);

    CHECK_FALSE(resource_list_files_start("P0.txt", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load)
{
    int resid, mark;
    uint8_t *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "pkg:p0.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "hello", 5);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_parallel)
{
    int resid, resid2, mark;
    uint8_t *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "pkg:02.txt", 0, 0));
    CHECK_TRUE(resid2 = resource_load_data(resmgr, "pkg:P4.txt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 5);
    CHECK_MEMEQUAL(data, "world", 5);
    resource_free(resmgr, resid);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid2, &size));
    CHECK_INTEQUAL(size, 6);
    CHECK_MEMEQUAL(data, "foobar", 6);
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_compressed)
{
    int resid, mark;
    uint8_t *data;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "pkg:dir/square.dat", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 256);
    for (int i = 0; i < 256; i++) {
        CHECK_INTEQUAL(data[i], (i*i) & 255);
    }
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_load_with_background_decompression)
{
    int resid, mark;
    uint8_t *data;
    int size;

    resource_set_background_decompression(1, 1, 1, 1);
    CHECK_TRUE(resid = resource_load_data(resmgr, "pkg:dir/square.dat", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    size = 0;
    CHECK_TRUE(data = resource_get_data(resmgr, resid, &size));
    CHECK_INTEQUAL(size, 256);
    for (int i = 0; i < 256; i++) {
        CHECK_INTEQUAL(data[i], (i*i) & 255);
    }
    resource_free(resmgr, resid);
    resource_set_background_decompression(0, 0, 0, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_corrupt_compressed_data)
{
    int resid, mark;
    int size;

    CHECK_TRUE(resid = resource_load_data(resmgr, "pkg:02.4zt", 0, 0));
    CHECK_TRUE(mark = resource_mark(resmgr));
    resource_wait(resmgr, mark);
    CHECK_FALSE(resource_get_data(resmgr, resid, &size));
    resource_free(resmgr, resid);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_out_of_memory)
{
    PackageModuleInfo *package2;

    CHECK_MEMORY_FAILURES(package2 = pkg_create_instance(
                   "testdata/pkg/test.pkg", "pkg:"));
    pkg_destroy_instance(package2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_out_of_memory)
{
    PackageModuleInfo *package2;

    CHECK_TRUE(package2 = pkg_create_instance(
                   "testdata/pkg/test.pkg", "pkg:"));
    CHECK_MEMORY_FAILURES(resource_register_package(package2));
    resource_unregister_package(package2);
    pkg_destroy_instance(package2);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
