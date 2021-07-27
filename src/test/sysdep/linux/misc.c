/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/misc.c: Tests for miscellaneous Linux-specific
 * functions.
 */

#include "src/base.h"
#include "src/memory.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-x11.h"
#include "src/test/sysdep/posix/internal.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Saved values of the $LANG, $LC_ALL, $LC_MESSAGES, and $PATH environment
 * variables. */
static char *saved_LANG = NULL;
static char *saved_LC_ALL = NULL;
static char *saved_LC_MESSAGES = NULL;
static char *saved_PATH = NULL;

/* Pathname of temporary directory, xdg_open script, and output file for
 * sys_open_file() and sys_open_url() testing. */
static char tempdir[PATH_MAX] = "";
static char xdg_open[PATH_MAX] = "";
static char output_path[PATH_MAX] = "";

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_misc)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    ASSERT(posix_create_temporary_dir("test-linux-misc",
                                      tempdir, sizeof(tempdir)));

    /* To test sys_open_file() and sys_open_url(), we create a dummy
     * xdg-open script in the temporary directory and modify $PATH so our
     * dummy script is run.  We use various values of $PATH and various
     * file modes for xdg-open in the tests, but the content of the script
     * is always the same, so we create it here. */
    ASSERT(strformat_check(xdg_open, sizeof(xdg_open), "%s/xdg-open", tempdir));
    ASSERT(strformat_check(output_path, sizeof(output_path),
                           "%s/output", tempdir));
    char script_buf[PATH_MAX+100];
    ASSERT(strformat_check(script_buf, sizeof(script_buf),
                           "#!/bin/sh\necho -n \"$*\" >\"%s\"\n",
                           output_path));
    ASSERT(posix_write_file(xdg_open, script_buf, strlen(script_buf), 0));

    const char *s;
    if ((s = getenv("LANG")) != NULL) {
        ASSERT(saved_LANG = mem_strdup(s, MEM_ALLOC_TEMP));
    }
    if ((s = getenv("LC_ALL")) != NULL) {
        ASSERT(saved_LC_ALL = mem_strdup(s, MEM_ALLOC_TEMP));
    }
    if ((s = getenv("LC_MESSAGES")) != NULL) {
        ASSERT(saved_LC_MESSAGES = mem_strdup(s, MEM_ALLOC_TEMP));
    }
    if ((s = getenv("PATH")) != NULL) {
        ASSERT(saved_PATH = mem_strdup(s, MEM_ALLOC_TEMP));
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    if (saved_LANG) {
        ASSERT(setenv("LANG", saved_LANG, 1) == 0);
        mem_free(saved_LANG);
        saved_LANG = NULL;
    }
    if (saved_LC_ALL) {
        ASSERT(setenv("LC_ALL", saved_LC_ALL, 1) == 0);
        mem_free(saved_LC_ALL);
        saved_LC_ALL = NULL;
    }
    if (saved_LC_MESSAGES) {
        ASSERT(setenv("LC_MESSAGES", saved_LC_MESSAGES, 1) == 0);
        mem_free(saved_LC_MESSAGES);
        saved_LC_MESSAGES = NULL;
    }
    if (saved_PATH) {
        ASSERT(setenv("PATH", saved_PATH, 1) == 0);
        mem_free(saved_PATH);
        saved_PATH = NULL;
    }

    if (!posix_rmdir_r(tempdir)) {
        FAIL("Failed to remove temporary directory %s", tempdir);
    }
    *tempdir = 0;
    *xdg_open = 0;

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_language)
{
    char language[3], dialect[3];

    /* Check a simple language-only case, using $LANG. */
    ASSERT(setenv("LANG", "en", 1) == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    /* Check that nothing is returned for indexes greater than zero. */
    CHECK_FALSE(sys_get_language(1, language, dialect));

    /* Check a language+dialect case, using $LC_ALL. */
    ASSERT(setenv("LC_ALL", "fr_FR", 1) == 0);
    ASSERT(unsetenv("LANG") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "fr");
    CHECK_STREQUAL(dialect, "FR");

    /* Check a language+dialect+charset case, using $LC_MESSAGES (charset
     * isn't returned, but shouldn't break anything else). */
    ASSERT(setenv("LC_MESSAGES", "ja_JP.UTF-8", 1) == 0);
    ASSERT(unsetenv("LANG") == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "ja");
    CHECK_STREQUAL(dialect, "JP");

    /* Check special cases. */
    ASSERT(setenv("LANG", "C", 1) == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "US");

    ASSERT(setenv("LANG", "POSIX", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "US");

    /* Check the case of no language setting at all. */
    ASSERT(unsetenv("LANG") == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "", 1) == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(unsetenv("LANG") == 0);
    ASSERT(setenv("LC_ALL", "", 1) == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(unsetenv("LANG") == 0);
    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(setenv("LC_MESSAGES", "", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_language_env_var_priority)
{
    char language[3], dialect[3];

    /* $LC_ALL should take precedence over the other two variables. */
    ASSERT(setenv("LANG", "en", 1) == 0);
    ASSERT(setenv("LC_ALL", "fr_FR", 1) == 0);
    ASSERT(setenv("LC_MESSAGES", "ja_JP.UTF-8", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "fr");
    CHECK_STREQUAL(dialect, "FR");

    /* If $LC_ALL is unset, $LC_MESSAGES should take precedence over $LANG. */
    ASSERT(unsetenv("LC_ALL") == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "ja");
    CHECK_STREQUAL(dialect, "JP");

    /* Present but empty values should not override non-empty values. */
    ASSERT(setenv("LC_ALL", "", 1) == 0);
    ASSERT(setenv("LC_MESSAGES", "", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_language_invalid_format)
{
    char language[3], dialect[3];

    ASSERT(unsetenv("LC_ALL") == 0);
    ASSERT(unsetenv("LC_MESSAGES") == 0);

    ASSERT(setenv("LANG", "foo", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "En_US", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "~n_US", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "eN_US", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "e~_US", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    ASSERT(setenv("LANG", "en-US", 1) == 0);
    CHECK_FALSE(sys_get_language(0, language, dialect));

    /* These succeed but ignore the (invalid) dialect string. */
    ASSERT(setenv("LANG", "en_uS", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    ASSERT(setenv("LANG", "en_Us", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    ASSERT(setenv("LANG", "en_1S", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    ASSERT(setenv("LANG", "en_U1", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    ASSERT(setenv("LANG", "en_US_US", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    ASSERT(setenv("LANG", "en_USUS", 1) == 0);
    strcpy(language, "??");  // Safe.
    strcpy(dialect, "??");  // Safe.
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url)
{
    ASSERT(setenv("PATH", tempdir, 1) == 0);
    ASSERT(chmod(xdg_open, 0755) == 0);

    /* Check that the functions report that file/URL opening is available. */
    CHECK_TRUE(sys_open_file(NULL));
    CHECK_TRUE(sys_open_url(NULL));

    int i;
    char *filedata;
    ssize_t filesize;

    /* Check that sys_open_file() passes the file parameter properly. */
    CHECK_TRUE(sys_open_file("xdg-open"));
    for (i = 100; i > 0; i--) {
        struct stat st;
        if (stat(output_path, &st) == 0 && st.st_size > 0) {
            break;
        }
        nanosleep(&(struct timespec){0, 10*1000*1000}, NULL);
    }
    if (i == 0) {
        FAIL("%s was not created", output_path);
    }
    filesize = 0;
    CHECK_TRUE(filedata = posix_read_file(output_path, &filesize, 0));
    CHECK_INTEQUAL(filesize, 8);
    CHECK_MEMEQUAL(filedata, "xdg-open", 8);
    mem_free(filedata);
    ASSERT(unlink(output_path) == 0);

    /* Check that sys_open_url() passes the URL parameter properly. */
    CHECK_TRUE(sys_open_url("http://example.com/"));
    for (i = 100; i > 0; i--) {
        struct stat st;
        if (stat(output_path, &st) == 0 && st.st_size > 0) {
            break;
        }
        nanosleep(&(struct timespec){0, 10*1000*1000}, NULL);
    }
    if (i == 0) {
        FAIL("%s was not created", output_path);
    }
    /* Wait a bit longer so we don't catch the file before it's been closed. */
    nanosleep(&(struct timespec){0, 10*1000*1000}, NULL);
    filesize = 0;
    CHECK_TRUE(filedata = posix_read_file(output_path, &filesize, 0));
    CHECK_INTEQUAL(filesize, 19);
    CHECK_MEMEQUAL(filedata, "http://example.com/", 19);
    mem_free(filedata);
    ASSERT(unlink(output_path) == 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_fds_closed)
{
    ASSERT(setenv("PATH", tempdir, 1) == 0);
    char script_buf[PATH_MAX];
    ASSERT(strformat_check(script_buf, sizeof(script_buf),
                           "#!/bin/sh\nPATH=/usr/bin:/bin\n"
                           "ls /proc/$$/fd | wc -l > \"%s\"\n",
                           output_path));
    ASSERT(posix_write_file(xdg_open, script_buf, strlen(script_buf), 0));
    ASSERT(chmod(xdg_open, 0755) == 0);

    int i;
    char *filedata;
    ssize_t filesize;

    /* Open a file descriptor so we can check for its closure in the
     * forked process. */
    int fd;
    ASSERT((fd = open(xdg_open, O_RDONLY)) >= 3);
    extern int system(const char *);  // Since base.h hides it.
    ASSERT(system(xdg_open) == 0);
    filesize = 0;
    CHECK_TRUE(filedata = posix_read_file(output_path, &filesize, 0));
    CHECK_TRUE(filesize >= 2);
    char *s;
    const int open_fds = strtol(filedata, &s, 10);
    CHECK_PTREQUAL(s, filedata + (filesize-1));
    CHECK_INTEQUAL(*s, '\n');
    mem_free(filedata);
    CHECK_TRUE(open_fds > 4);
    ASSERT(unlink(output_path) == 0);

    CHECK_TRUE(sys_open_file("xdg-open"));
    for (i = 100; i > 0; i--) {
        struct stat st;
        if (stat(output_path, &st) == 0 && st.st_size > 0) {
            break;
        }
        nanosleep(&(struct timespec){0, 10*1000*1000}, NULL);
    }
    if (i == 0) {
        FAIL("%s was not created", output_path);
    }
    filesize = 0;
    CHECK_TRUE(filedata = posix_read_file(output_path, &filesize, 0));
    CHECK_INTEQUAL(filesize, 2);
    CHECK_MEMEQUAL(filedata, "4\n", 2);
    mem_free(filedata);
    ASSERT(unlink(output_path) == 0);

    close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_no_path)
{
    ASSERT(unsetenv("PATH") == 0);

    CHECK_FALSE(sys_open_file(NULL));
    CHECK_FALSE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_no_xdg_open)
{
    ASSERT(setenv("PATH", tempdir, 1) == 0);
    ASSERT(unlink(xdg_open) == 0);

    CHECK_FALSE(sys_open_file(NULL));
    CHECK_FALSE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_xdg_open_not_executable)
{
    ASSERT(setenv("PATH", tempdir, 1) == 0);
    ASSERT(chmod(xdg_open, 0644) == 0);

    CHECK_FALSE(sys_open_file(NULL));
    CHECK_FALSE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_xdg_open_not_valid_program)
{
    ASSERT(setenv("PATH", tempdir, 1) == 0);
    char script_buf[PATH_MAX];
    ASSERT(strformat_check(script_buf, sizeof(script_buf), "#!%s/none\n",
                           tempdir));
    ASSERT(posix_write_file(xdg_open, script_buf, strlen(script_buf), 0));
    ASSERT(chmod(xdg_open, 0755) == 0);

    CHECK_TRUE(sys_open_file("xdg-open"));
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    CHECK_TRUE(access(output_path, F_OK) == -1);

    CHECK_TRUE(sys_open_url("http://example.com/"));
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    CHECK_TRUE(access(output_path, F_OK) == -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_multi_dir_path)
{
    char pathbuf[2*PATH_MAX+10];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf),
                           "%s/foo:%s", tempdir, tempdir));
    ASSERT(setenv("PATH", pathbuf, 1) == 0);
    ASSERT(chmod(xdg_open, 0755) == 0);

    CHECK_TRUE(sys_open_file(NULL));
    CHECK_TRUE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url_buffer_overflow_on_path_component)
{
    char pathbuf[2*PATH_MAX+10];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf),
                           "/%*s:%s", PATH_MAX, "", tempdir));
    ASSERT(setenv("PATH", pathbuf, 1) == 0);
    ASSERT(chmod(xdg_open, 0755) == 0);

    CHECK_TRUE(sys_open_file(NULL));
    CHECK_TRUE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reset_idle_timet)
{
    clear_x11_wrapper_variables();
    sys_reset_idle_timer();
    CHECK_INTEQUAL(called_XResetScreenSaver, 1);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
