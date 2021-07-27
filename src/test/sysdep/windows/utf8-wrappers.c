/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/utf8-wrappers.c: Tests for the UTF-8 wrappers
 * for Win32 Unicode functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

#include <stdio.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Environment variable used for environment access tests.  This variable
 * must not be present in the initial process environment. */
#define TEST_ENVVAR_NAME  "SIL_TEST_あいうえお"
/* Value to which the test variable is set. */
#define TEST_ENVVAR_VALUE  "test_かきくけこ"

/*************************************************************************/
/*************************** Helper functions ****************************/
/*************************************************************************/

/**
 * get_tempdir:  Return the pathname of the directory to use for temporary
 * files.
 *
 * [Return value]
 *     Temporary directory pathname, or NULL on error.
 */
static const char *get_tempdir(void)
{
    static char tempdir[(MAX_PATH+1)*3 + 1];
    const unsigned int templen = GetTempPath(sizeof(tempdir), tempdir);
    if (UNLIKELY(templen == 0)) {
        DLOG("GetTempPath() failed: %s", windows_strerror(GetLastError()));
        return NULL;
    }
    return tempdir;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_utf8_wrappers)

/*************************************************************************/
/************* Test routines: Environment variable handling **************/
/*************************************************************************/

TEST(test_GetEnvironmentVariable)
{
    int size;
    CHECK_TRUE(size = GetEnvironmentVariableU("PATH", NULL, 0));

    char *buf;
    ASSERT(buf = mem_alloc(size, 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR));
    /* On success, the return value should _not_ include the trailing null. */
    CHECK_INTEQUAL(GetEnvironmentVariableU("PATH", buf, size), size-1);
    CHECK_INTEQUAL(strlen(buf), size-1);

    mem_free(buf);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetEnvironmentVariable)
{
    ASSERT(!GetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL, 0));

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE));
    CHECK_INTEQUAL(GetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL, 0),
                   strlen(TEST_ENVVAR_VALUE) + 1);

    char buf[100];
    ASSERT(sizeof(buf) >= strlen(TEST_ENVVAR_VALUE) + 1);
    mem_clear(buf, sizeof(buf));
    CHECK_INTEQUAL(GetEnvironmentVariableU(TEST_ENVVAR_NAME, buf, sizeof(buf)),
                   strlen(TEST_ENVVAR_VALUE));
    CHECK_STREQUAL(buf, TEST_ENVVAR_VALUE);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    CHECK_FALSE(GetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetEnvironmentVariable_memory_failure)
{
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE));

    char buf[100];
    ASSERT(sizeof(buf) >= strlen(TEST_ENVVAR_VALUE) + 1);
    mem_clear(buf, sizeof(buf));
    int result;
    CHECK_MEMORY_FAILURES(result = GetEnvironmentVariableU(TEST_ENVVAR_NAME,
                                                           NULL, 0));
    CHECK_INTEQUAL(result, strlen(TEST_ENVVAR_VALUE) + 1);
    CHECK_MEMORY_FAILURES(result = GetEnvironmentVariableU(TEST_ENVVAR_NAME,
                                                           buf, sizeof(buf)));
    CHECK_INTEQUAL(result, strlen(TEST_ENVVAR_VALUE));
    CHECK_STREQUAL(buf, TEST_ENVVAR_VALUE);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetEnvironmentVariable_memory_failure)
{
    CHECK_MEMORY_FAILURES(SetEnvironmentVariableU(TEST_ENVVAR_NAME,
                                                  TEST_ENVVAR_VALUE));

    char buf[100];
    ASSERT(sizeof(buf) >= strlen(TEST_ENVVAR_VALUE) + 1);
    mem_clear(buf, sizeof(buf));
    CHECK_INTEQUAL(GetEnvironmentVariableU(TEST_ENVVAR_NAME, buf, sizeof(buf)),
                   strlen(TEST_ENVVAR_VALUE));
    CHECK_STREQUAL(buf, TEST_ENVVAR_VALUE);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetEnvironmentStrings)
{
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE));

    char *envp;
    CHECK_TRUE(envp = GetEnvironmentStringsU());

    int found = 0;
    const char *s = envp;
    while (*s) {
        found |= (strcmp(s, TEST_ENVVAR_NAME "=" TEST_ENVVAR_VALUE) == 0);
        s += strlen(s) + 1;
    }
    CHECK_TRUE(found);

    FreeEnvironmentStringsU(envp);
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetEnvironmentStrings_memory_failure)
{
    char *envp1, *envp2;
    CHECK_TRUE(envp1 = GetEnvironmentStringsU());
    CHECK_MEMORY_FAILURES_TO(1000, envp2 = GetEnvironmentStringsU());

    const char *s1 = envp1;
    const char *s2 = envp2;
    while (*s1) {
        CHECK_STREQUAL(s1, s2);
        s1 += strlen(s1) + 1;
        s2 += strlen(s2) + 1;
    }
    CHECK_INTEQUAL(*s2, '\0');

    FreeEnvironmentStringsU(envp1);
    FreeEnvironmentStringsU(envp2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Moved here from util.c to avoid a circular test dependency. */
TEST(test_windows_getenv)
{
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE));
    char *s;
    CHECK_TRUE(s = windows_getenv(TEST_ENVVAR_NAME));
    CHECK_STREQUAL(s, TEST_ENVVAR_VALUE);
    mem_free(s);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    CHECK_FALSE(windows_getenv(TEST_ENVVAR_NAME));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_getenv_empty_variable)
{
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, ""));
    char *s;
    CHECK_TRUE(s = windows_getenv(TEST_ENVVAR_NAME));
    CHECK_STREQUAL(s, "");
    mem_free(s);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_getenv_memory_failure)
{
    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE));
    char *s;
    CHECK_MEMORY_FAILURES(s = windows_getenv(TEST_ENVVAR_NAME));
    CHECK_STREQUAL(s, TEST_ENVVAR_VALUE);
    mem_free(s);

    CHECK_TRUE(SetEnvironmentVariableU(TEST_ENVVAR_NAME, NULL));
    return 1;
}

/*************************************************************************/
/********************* Test routines: File handling **********************/
/*************************************************************************/

TEST(test_CreateFile_DeleteFile)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));

    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    FILE *f;
    CHECK_TRUE(f = fopen(path, "r"));
    fclose(f);

    CHECK_TRUE(DeleteFileU(path));
    CHECK_FALSE(fopen(path, "r"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_CreateFile_DeleteFile_memory_failure)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));

    HANDLE file;
    CHECK_MEMORY_FAILURES((file = CreateFileU(
                               path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL))
                          != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    FILE *f;
    CHECK_TRUE(f = fopen(path, "r"));
    fclose(f);

    CHECK_MEMORY_FAILURES(DeleteFileU(path));
    CHECK_FALSE(fopen(path, "r"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetFileAttributes)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));

    CHECK_FALSE(GetFileAttributesU(path) != INVALID_FILE_ATTRIBUTES);

    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE(GetFileAttributesU(path) != INVALID_FILE_ATTRIBUTES);

    CHECK_TRUE(DeleteFileU(path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetFileAttributes_memory_failure)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));

    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_MEMORY_FAILURES(GetFileAttributesU(path) != INVALID_FILE_ATTRIBUTES);

    CHECK_TRUE(DeleteFileU(path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetFileAttributes)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    const DWORD attributes = GetFileAttributesU(path);
    CHECK_TRUE(attributes != INVALID_FILE_ATTRIBUTES);
    CHECK_FALSE(attributes & FILE_ATTRIBUTE_READONLY);

    CHECK_TRUE(SetFileAttributesU(path, attributes | FILE_ATTRIBUTE_READONLY));
    const DWORD attributes2 = GetFileAttributesU(path);
    CHECK_TRUE(attributes2 != INVALID_FILE_ATTRIBUTES);
    CHECK_TRUE(attributes2 & FILE_ATTRIBUTE_READONLY);

    CHECK_TRUE(SetFileAttributesU(path, attributes));
    CHECK_TRUE(DeleteFileU(path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetFileAttributes_memory_failure)
{
    char path[MAX_PATH*3];
    ASSERT(strformat_check(path, sizeof(path), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    const DWORD attributes = GetFileAttributesU(path);
    CHECK_TRUE(attributes != INVALID_FILE_ATTRIBUTES);
    CHECK_FALSE(attributes & FILE_ATTRIBUTE_READONLY);

    CHECK_MEMORY_FAILURES(SetFileAttributesU(
                              path, attributes | FILE_ATTRIBUTE_READONLY));
    const DWORD attributes2 = GetFileAttributesU(path);
    CHECK_TRUE(attributes2 != INVALID_FILE_ATTRIBUTES);
    CHECK_TRUE(attributes2 & FILE_ATTRIBUTE_READONLY);

    CHECK_TRUE(SetFileAttributesU(path, attributes));
    CHECK_TRUE(DeleteFileU(path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_MoveFileEx)
{
    char path1[MAX_PATH*3];
    ASSERT(strformat_check(path1, sizeof(path1), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char path2[MAX_PATH*3];
    ASSERT(strformat_check(path2, sizeof(path2), "%s\\SIL-%u.dat",
                           get_tempdir(), (int)GetCurrentProcessId()));

    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path1, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE(GetFileAttributesU(path1) != INVALID_FILE_ATTRIBUTES);
    CHECK_FALSE(GetFileAttributesU(path2) != INVALID_FILE_ATTRIBUTES);

    CHECK_TRUE(MoveFileExU(path1, path2, 0));
    CHECK_FALSE(GetFileAttributesU(path1) != INVALID_FILE_ATTRIBUTES);
    CHECK_TRUE(GetFileAttributesU(path2) != INVALID_FILE_ATTRIBUTES);

    CHECK_TRUE(DeleteFileU(path2));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_MoveFileEx_memory_failure)
{
    char path1[MAX_PATH*3];
    ASSERT(strformat_check(path1, sizeof(path1), "%s\\SIL-%u.txt",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char path2[MAX_PATH*3];
    ASSERT(strformat_check(path2, sizeof(path2), "%s\\SIL-%u.dat",
                           get_tempdir(), (int)GetCurrentProcessId()));

    HANDLE file;
    CHECK_TRUE((file = CreateFileU(path1, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE(GetFileAttributesU(path1) != INVALID_FILE_ATTRIBUTES);
    CHECK_FALSE(GetFileAttributesU(path2) != INVALID_FILE_ATTRIBUTES);

    CHECK_MEMORY_FAILURES(MoveFileExU(path1, path2, 0));
    CHECK_FALSE(GetFileAttributesU(path1) != INVALID_FILE_ATTRIBUTES);
    CHECK_TRUE(GetFileAttributesU(path2) != INVALID_FILE_ATTRIBUTES);

    CHECK_TRUE(DeleteFileU(path2));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_CreateDirectory_RemoveDirectory)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file_path[MAX_PATH*3];
    ASSERT(strformat_check(file_path, sizeof(file_path), "%s\\file.txt",
                           dir_path));

    CHECK_FALSE(CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                != INVALID_HANDLE_VALUE);

    CHECK_TRUE(CreateDirectoryU(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE(DeleteFileU(file_path));

    CHECK_TRUE(RemoveDirectoryU(dir_path));
    CHECK_FALSE(CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                != INVALID_HANDLE_VALUE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_CreateDirectory_RemoveDirectory_memory_failure)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file_path[MAX_PATH*3];
    ASSERT(strformat_check(file_path, sizeof(file_path), "%s\\file.txt",
                           dir_path));

    CHECK_FALSE(CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                != INVALID_HANDLE_VALUE);

    CHECK_MEMORY_FAILURES(CreateDirectory(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE(DeleteFileU(file_path));

    CHECK_MEMORY_FAILURES(RemoveDirectory(dir_path));
    CHECK_FALSE(CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                != INVALID_HANDLE_VALUE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetCurrentDirectory_SetCurrentDirectory)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file_path[MAX_PATH*3];
    ASSERT(strformat_check(file_path, sizeof(file_path), "%s\\file.txt",
                           dir_path));

    CHECK_TRUE(CreateDirectoryU(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    char cwd_buf[MAX_PATH*3];
    int len;
    CHECK_TRUE(len = GetCurrentDirectoryU(sizeof(cwd_buf), cwd_buf));
    CHECK_TRUE(len < (int)sizeof(cwd_buf));
    CHECK_INTEQUAL(len, (int)strlen(cwd_buf));

    CHECK_TRUE(SetCurrentDirectoryU(dir_path));
    CHECK_TRUE((file = CreateFileU("file.txt", GENERIC_READ, 0, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    CHECK_TRUE(SetCurrentDirectoryU(cwd_buf));
    CHECK_TRUE(DeleteFileU(file_path));
    CHECK_TRUE(RemoveDirectoryU(dir_path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetCurrentDirectory_null_buffer)
{
    char buf[MAX_PATH*3];
    int len;
    CHECK_TRUE(len = GetCurrentDirectoryU(sizeof(buf), buf));
    CHECK_TRUE(len < (int)sizeof(buf));
    CHECK_INTEQUAL(len, (int)strlen(buf));

    CHECK_INTEQUAL(GetCurrentDirectoryU(0, NULL), len+1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetCurrentDirectory_memory_failure)
{
    char buf[MAX_PATH*3];
    int len;
    CHECK_TRUE(len = GetCurrentDirectoryU(sizeof(buf), buf));
    CHECK_TRUE(len < (int)sizeof(buf));
    CHECK_INTEQUAL(len, (int)strlen(buf));

    char buf2[MAX_PATH*3];
    int len2;
    CHECK_MEMORY_FAILURES(len2 = GetCurrentDirectoryU(sizeof(buf2), buf2));
    CHECK_INTEQUAL(len2, len);
    CHECK_STREQUAL(buf2, buf);

    int len3;
    CHECK_MEMORY_FAILURES(len3 = GetCurrentDirectoryU(0, NULL));
    CHECK_INTEQUAL(len3, len+1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetCurrentDirectory_memory_failure)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file_path[MAX_PATH*3];
    ASSERT(strformat_check(file_path, sizeof(file_path), "%s\\file.txt",
                           dir_path));

    CHECK_TRUE(CreateDirectoryU(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    char cwd_buf[MAX_PATH*3];
    int len;
    CHECK_TRUE(len = GetCurrentDirectoryU(sizeof(cwd_buf), cwd_buf));
    CHECK_TRUE(len < (int)sizeof(cwd_buf));
    CHECK_INTEQUAL(len, (int)strlen(cwd_buf));

    CHECK_MEMORY_FAILURES(SetCurrentDirectoryU(dir_path));
    CHECK_TRUE((file = CreateFileU("file.txt", GENERIC_READ, 0, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    CHECK_TRUE(SetCurrentDirectoryU(cwd_buf));
    CHECK_TRUE(DeleteFileU(file_path));
    CHECK_TRUE(RemoveDirectoryU(dir_path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_FindFirstFile_FindNextFile)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file1_path[MAX_PATH*3];
    ASSERT(strformat_check(file1_path, sizeof(file1_path), "%s\\file1.txt",
                           dir_path));
    char file2_path[MAX_PATH*3];
    ASSERT(strformat_check(file2_path, sizeof(file2_path), "%s\\file2.txt",
                           dir_path));
    char pattern[MAX_PATH*3];
    ASSERT(strformat_check(pattern, sizeof(pattern), "%s\\*.*", dir_path));

    WIN32_FIND_DATAU find_data;
    CHECK_FALSE(FindFirstFileU(pattern, &find_data) != INVALID_HANDLE_VALUE);

    CHECK_TRUE(CreateDirectoryU(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file1_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE((file = CreateFileU(file2_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    int saw_file1 = 0, saw_file2 = 0;
    HANDLE find;
    CHECK_TRUE((find = FindFirstFileU(pattern, &find_data))
               != INVALID_HANDLE_VALUE);
    do {
        if (strcmp(find_data.cFileName, "file1.txt") == 0) {
            saw_file1 = 1;
        } else if (strcmp(find_data.cFileName, "file2.txt") == 0) {
            saw_file2 = 2;
        } else if (strncmp(find_data.cFileName, "..",
                           strlen(find_data.cFileName)) != 0) {
            FAIL("Find{First,Next}FileU() returned invalid filename: %s",
                 find_data.cFileName);
        }
    } while (FindNextFileU(find, &find_data));
    CHECK_INTEQUAL(GetLastError(), ERROR_NO_MORE_FILES);
    FindClose(find);
    CHECK_TRUE(saw_file1);
    CHECK_TRUE(saw_file2);

    CHECK_TRUE(DeleteFileU(file1_path));
    CHECK_TRUE(DeleteFileU(file2_path));
    CHECK_TRUE(RemoveDirectoryU(dir_path));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_FindFirstFile_memory_failure)
{
    char dir_path[MAX_PATH*3];
    ASSERT(strformat_check(dir_path, sizeof(dir_path), "%s\\SIL-%u",
                           get_tempdir(), (int)GetCurrentProcessId()));
    char file1_path[MAX_PATH*3];
    ASSERT(strformat_check(file1_path, sizeof(file1_path), "%s\\file1.txt",
                           dir_path));
    char file2_path[MAX_PATH*3];
    ASSERT(strformat_check(file2_path, sizeof(file2_path), "%s\\file2.txt",
                           dir_path));
    char pattern[MAX_PATH*3];
    ASSERT(strformat_check(pattern, sizeof(pattern), "%s\\*.*", dir_path));

    CHECK_TRUE(CreateDirectoryU(dir_path, NULL));
    HANDLE file;
    CHECK_TRUE((file = CreateFileU(file1_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);
    CHECK_TRUE((file = CreateFileU(file2_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
                != INVALID_HANDLE_VALUE);
    CloseHandle(file);

    int saw_file1 = 0, saw_file2 = 0;
    HANDLE find;
    WIN32_FIND_DATAU find_data;
    CHECK_MEMORY_FAILURES((find = FindFirstFileU(pattern, &find_data))
                          != INVALID_HANDLE_VALUE);
    do {
        if (strcmp(find_data.cFileName, "file1.txt") == 0) {
            saw_file1 = 1;
        } else if (strcmp(find_data.cFileName, "file2.txt") == 0) {
            saw_file2 = 2;
        } else if (strncmp(find_data.cFileName, "..",
                           strlen(find_data.cFileName)) != 0) {
            FAIL("Find{First,Next}FileU() returned invalid filename: %s",
                 find_data.cFileName);
        }
    } while (FindNextFileU(find, &find_data));
    CHECK_INTEQUAL(GetLastError(), ERROR_NO_MORE_FILES);
    FindClose(find);
    CHECK_TRUE(saw_file1);
    CHECK_TRUE(saw_file2);

    CHECK_TRUE(DeleteFileU(file1_path));
    CHECK_TRUE(DeleteFileU(file2_path));
    CHECK_TRUE(RemoveDirectoryU(dir_path));
    return 1;
}

/*************************************************************************/
/********************* Test routines: Miscellaneous **********************/
/*************************************************************************/

TEST(test_GetModuleFileName_zero_size)
{
    CHECK_FALSE(GetModuleFileNameU(NULL, NULL, 0));
    CHECK_INTEQUAL(GetLastError(), ERROR_INSUFFICIENT_BUFFER);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_GetModuleFileName_memory_failure)
{
    char buf1[4096], buf2[4096];
    CHECK_TRUE(GetModuleFileNameU(NULL, buf1, sizeof(buf1)));
    CHECK_MEMORY_FAILURES(GetModuleFileNameU(NULL, buf2, sizeof(buf2)));
    CHECK_STREQUAL(buf1, buf2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_LoadLibrary_memory_failure)
{
    HMODULE user32 = LoadLibraryU("user32.dll");
    HMODULE test;
    CHECK_MEMORY_FAILURES(test = LoadLibraryU("user32.dll"));
    CHECK_PTREQUAL(test, user32);
    FreeLibrary(test);
    FreeLibrary(user32);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_SetWindowText_null)
{
    #undef FAIL_ACTION
    #define FAIL_ACTION  graphics_cleanup(); return 0

    ASSERT(graphics_init());
    if (!open_window(64, 64)) {
        graphics_cleanup();
        FAIL("Unable to open window");
    }

    HWND window = windows_window();
    CHECK_TRUE(SetWindowTextU(window, NULL));
    uint16_t buf[2];
    memset(buf, -1, sizeof(buf));
    CHECK_INTEQUAL(GetWindowTextW(window, buf, 2), 0);
    CHECK_MEMEQUAL(buf, ((const uint16_t[]){'\0', -1}), 2*2);

    graphics_cleanup();
    return 1;

    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*-----------------------------------------------------------------------*/

TEST(test_SetWindowText_memory_failure)
{
    #undef FAIL_ACTION
    #define FAIL_ACTION  graphics_cleanup(); return 0

    ASSERT(graphics_init());
    if (!open_window(64, 64)) {
        graphics_cleanup();
        FAIL("Unable to open window");
    }

    HWND window = windows_window();
    CHECK_MEMORY_FAILURES(SetWindowTextU(window, "test。"));
    /* Windows XP (at least) has a bug in GetWindowTextW() such that you
     * need to pass a larger buffer size than the actual string length if
     * the string contains a mixture of ASCII and non-ASCII characters;
     * otherwise, the string will be truncated before the end of the buffer. */
    uint16_t buf[7];
    memset(buf, -1, sizeof(buf));
    CHECK_INTEQUAL(GetWindowTextW(window, buf, 7), 5);
    /* Depending on the current locale, Windows may translate U+3002 into a
     * question mark.  Don't treat that as a failure. */
    uint16_t expected[7] = {'t','e','s','t',L'。','\0', -1};
    if (buf[4] == '?') {
        expected[4] = '?';
    }
    CHECK_MEMEQUAL(buf, expected, sizeof(expected));

    graphics_cleanup();
    return 1;

    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*************************************************************************/
/*************************************************************************/
