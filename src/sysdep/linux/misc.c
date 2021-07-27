/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/misc.c: Miscellaneous interface functions for Linux.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/posix/path_max.h"

#include <stdio.h>
#include <unistd.h>

#include <X11/Xlib.h>

/*************************************************************************/
/*************************************************************************/

void sys_display_error(const char *message, va_list args)
{
    fputs("Error: ", stderr);
    vfprintf(stderr, message, args);
    fputc('\n', stderr);
}

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    if (index == 0) {
        const char *env_var = "LC_ALL";
        const char *env_lang = getenv(env_var);
        if (!env_lang || !*env_lang) {
            env_var = "LC_MESSAGES";
            env_lang = getenv(env_var);
        }
        if (!env_lang || !*env_lang) {
            env_var = "LANG";
            env_lang = getenv(env_var);
        }
        if (env_lang && *env_lang) {
            if ((env_lang[0] >= 'a' && env_lang[0] <= 'z')
             && (env_lang[1] >= 'a' && env_lang[1] <= 'z')
             && (env_lang[2] == 0 || env_lang[2] == '_')) {
                language_ret[0] = env_lang[0];
                language_ret[1] = env_lang[1];
                language_ret[2] = 0;
                *dialect_ret = 0;
                if (env_lang[2] == '_') {
                    if ((env_lang[3] >= 'A' && env_lang[3] <= 'Z')
                     && (env_lang[4] >= 'A' && env_lang[4] <= 'Z')
                     && (env_lang[5] == 0 || env_lang[5] == '.')) {
                        dialect_ret[0] = env_lang[3];
                        dialect_ret[1] = env_lang[4];
                        dialect_ret[2] = 0;
                    } else {
                        DLOG("Ignoring invalid dialect code in $%s: %s",
                             env_var, env_lang);
                    }
                }
                return 1;
            } else if (strcmp(env_lang, "C") == 0
                    || strcmp(env_lang, "POSIX") == 0) {
                strcpy(language_ret, "en");  // Safe by contract.
                strcpy(dialect_ret, "US");  // Safe by contract.
                return 1;
            } else {
                static uint8_t warned = 0;
                if (!warned) {
                    DLOG("Ignoring invalid value for $%s: %s",
                         env_var, env_lang);
                    warned = 1;
                }
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_get_resource_path_prefix(char *prefix_buf, int bufsize)
{
    const char *path = linux_executable_dir();
#ifdef SIL_DATA_PATH_ENV_VAR
    const char *env_path = getenv(SIL_DATA_PATH_ENV_VAR);
    if (env_path && *env_path) {
        path = env_path;
    }
#endif
    return strformat(prefix_buf, bufsize, "%s/", path);
}

/*-----------------------------------------------------------------------*/

int sys_open_file(const char *path)
{
    /* xdg-open, used by sys_open_url(), can handle files as well, so just
     * pass the requested path to sys_open_url(); the "right thing" will
     * automagically happen. */
    return sys_open_url(path);
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    /* Make sure xdg-open can actually be found before we try executing it.
     * (While we're at it, save the path so exec() doesn't have to search
     * all over again.) */
    const char *xdg_open_path = NULL;
    char buf[PATH_MAX];
    const char *path = getenv("PATH");
    while (path && *path) {
        const char *s = path + strcspn(path, ":");
        if (!strformat_check(buf, sizeof(buf),
                             "%.*s/xdg-open", (int)(s-path), path)) {
            DLOG("Buffer overflow generating xdg-open path for dir %.*s",
                 (int)(s-path), path);
        } else {
            if (access(buf, X_OK) == 0) {
                xdg_open_path = buf;
                break;
            }
        }
        path = s;
        if (*path) {
            path++;
        }
    }
    if (!xdg_open_path) {
        DLOG("xdg-open not found in $PATH!");
        return 0;
    }

    if (!url) {
        return 1;
    }

    const char *argv[3];
    argv[0] = "xdg-open";
    argv[1] = url;
    argv[2] = NULL;

    switch (fork()) {
      case -1:
        DLOG("fork(): %s", strerror(errno));
        return 0;

      case 0: {
        /* Close all open files other than stdin/stdout/stderr. */
        int open_max;
#ifdef OPEN_MAX
        open_max = OPEN_MAX;
#else
        open_max = 1024;
#endif
#ifdef _SC_OPEN_MAX
        const int sc_open_max = sysconf(_SC_OPEN_MAX);
        if (sc_open_max > 0) {
            open_max = sc_open_max;
        }
#endif
        for (int fd = 3; fd < open_max; fd++) {
            close(fd);
        }
        /* Execute xdg-open with the target path.  Dunno why the execv()
         * parameter isn't "const char" -- maybe just to match the standard
         * declaration of argv in main()? */
        execv(xdg_open_path, (char * const *)argv);
        perror(xdg_open_path);
        exit(-1);
      }  // case 0

      default:
        return 1;
    }
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    XResetScreenSaver(linux_x11_display());
}

/*-----------------------------------------------------------------------*/

int sys_set_performance_level(int level)
{
    return level == 0;  // Alternate performance levels not supported.
}

/*************************************************************************/
/*************************************************************************/
