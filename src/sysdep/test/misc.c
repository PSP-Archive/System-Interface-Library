/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/misc.c: Testing implementation of miscellaneous
 * system-level functions.
 */

/*
 * This file holds stubs for the functions typically implemented in misc.c
 * for other systems (except for sys_last_error(), sys_last_errstr(), and
 * sys_random_seed(), which are not diverted from the actual system
 * implementations).  As these generally return no information to the
 * caller, this file also provides several hooks for test routines to
 * retrieve information about calls made to the sys_*() functions.
 */

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Language values to be returned by sys_get_language(). */
static char languages[10][3];  // 10 languages should be plenty for testing.
static char dialects[10][3];
static int num_languages = 0;

/* Buffer to store the last strings sent to sys_console_vprintf() and
 * sys_display_error(). */
static char last_console_output[1000];
static char last_displayed_error[1000];

/* Last path passed to sys_open_file() or sys_open_url(). */
static char last_open_path[1000];

/* Flag set when sys_reset_idle_timer() is called; can be cleared with
 * sys_test_clear_idle_reset_flag(). */
static uint8_t idle_reset_flag = 0;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_console_vprintf(const char *format, va_list args)
{
    vstrformat(last_console_output, sizeof(last_console_output), format, args);
}

/*-----------------------------------------------------------------------*/

void sys_display_error(const char *message, va_list args)
{
    vstrformat(last_displayed_error, sizeof(last_displayed_error),
               message, args);
}

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    if (index < num_languages) {
        memcpy(language_ret, languages[index], 3);
        memcpy(dialect_ret, dialects[index], 3);
        return 1;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int sys_open_file(const char *path)
{
    if (path) {
        strformat(last_open_path, sizeof(last_open_path), "%s", path);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    if (url) {
        strformat(last_open_path, sizeof(last_open_path), "%s", url);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    idle_reset_flag = 1;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

void sys_test_set_language(int index, const char *language,
                           const char *dialect)
{
    PRECOND(index <= num_languages, return);  // Don't allow holes.
    PRECOND(language != NULL, return);
    PRECOND(strlen(language) == 2, return);
    PRECOND(language[0] >= 'a' && language[0] <= 'z', return);
    PRECOND(language[1] >= 'a' && language[1] <= 'z', return);
    PRECOND(strlen(dialect) == 0 || strlen(dialect) == 2, return);
    PRECOND(strlen(dialect) == 0 || (dialect[0] >= 'A' && dialect[0] <= 'Z'),
            return);
    PRECOND(strlen(dialect) == 0 || (dialect[1] >= 'A' && dialect[1] <= 'Z'),
            return);

    if (index >= num_languages) {
        num_languages = index+1;
    }
    strcpy(languages[index], language);  // Safe (length-checked above).
    strcpy(dialects[index], dialect);  // Safe (length-checked above).
}

/*-----------------------------------------------------------------------*/

void sys_test_clear_languages(void)
{
    num_languages = 0;
}

/*-----------------------------------------------------------------------*/

const char *sys_test_get_last_console_output(void)
{
    return last_console_output;
}

/*-----------------------------------------------------------------------*/

const char *sys_test_get_last_displayed_error(void)
{
    return last_displayed_error;
}

/*-----------------------------------------------------------------------*/

const char *sys_test_get_last_external_open_path(void)
{
    return last_open_path;
}

/*-----------------------------------------------------------------------*/

int sys_test_get_idle_reset_flag(void)
{
    return idle_reset_flag;
}

/*-----------------------------------------------------------------------*/

void sys_test_clear_idle_reset_flag(void)
{
    idle_reset_flag = 0;
}

/*************************************************************************/
/*************************************************************************/
