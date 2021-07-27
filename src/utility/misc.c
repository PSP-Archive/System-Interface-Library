/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/misc.c: Miscellaneous utility functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/utility/misc.h"

/*************************************************************************/
/*************************************************************************/

int can_open_file(void)
{
    return sys_open_file(NULL);
}

/*-----------------------------------------------------------------------*/

int can_open_url(void)
{
    return sys_open_url(NULL);
}

/*-----------------------------------------------------------------------*/

void console_printf(const char *format, ...)
{
    PRECOND(format != NULL, return);

    va_list args;
    va_start(args, format);
    sys_console_vprintf(format, args);
    va_end(args);
}

/*-----------------------------------------------------------------------*/

void display_error(const char *message, ...)
{
    PRECOND(message != NULL, return);

    va_list args;
    va_start(args, message);
    sys_display_error(message, args);
    va_end(args);
}

/*-----------------------------------------------------------------------*/

int get_system_language(unsigned int index, const char **language_ret,
                        const char **dialect_ret)
{
    static char language[3], dialect[3];
    if (sys_get_language(index, language, dialect)) {
        language[2] = dialect[2] = '\0';  // Just in case.
        if (language_ret) {
            *language_ret = language;
        }
        if (dialect_ret) {
            *dialect_ret = dialect;
        }
        return 1;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

const char *default_dialect_for_language(const char *language)
{
    if (!language) {
        DLOG("language == NULL");
        return "";
    }

    static const struct {char language[2], dialect[3];} map[] = {
        {"xx", "XX"},
        {"af", "ZA"},
        {"am", "ET"},
        {"an", "ES"},
        {"be", "BY"},
        {"bg", "BG"},
        {"bn", "BD"},
        {"br", "FR"},
        {"bs", "BA"},
        {"ca", "ES"},
        {"cs", "CZ"},
        {"cv", "RU"},
        {"cy", "GB"},
        {"da", "DK"},
        {"de", "DE"},
        {"dv", "MV"},
        {"dz", "BT"},
        {"el", "GR"},
        {"en", "US"},
        {"es", "ES"},
        {"et", "EE"},
        {"eu", "ES"},
        {"fa", "IR"},
        {"ff", "SN"},
        {"fi", "FI"},
        {"fo", "FO"},
        {"fr", "FR"},
        {"ga", "IE"},
        {"gd", "GB"},
        {"gl", "ES"},
        {"gv", "GB"},
        {"ha", "NG"},
        {"he", "IL"},
        {"hi", "IN"},
        {"hr", "HR"},
        {"ht", "HT"},
        {"hu", "HU"},
        {"hy", "AM"},
        {"id", "ID"},
        {"ig", "NG"},
        {"ik", "CA"},
        {"is", "IS"},
        {"it", "IT"},
        {"iu", "CA"},
        {"iw", "IL"},
        {"ja", "JP"},
        {"ka", "GE"},
        {"kk", "KZ"},
        {"kl", "GL"},
        {"km", "KH"},
        {"kn", "IN"},
        {"ko", "KR"},
        {"ks", "IN"},
        {"ku", "TR"},
        {"kw", "GB"},
        {"ky", "KG"},
        {"lb", "LU"},
        {"lg", "UG"},
        {"lo", "LA"},
        {"lt", "LT"},
        {"lv", "LV"},
        {"mg", "MG"},
        {"mi", "NZ"},
        {"mk", "MK"},
        {"ml", "IN"},
        {"mn", "MN"},
        {"mr", "IN"},
        {"ms", "MY"},
        {"mt", "MT"},
        {"my", "MM"},
        {"nb", "NO"},
        {"ne", "NP"},
        {"nl", "NL"},
        {"nn", "NO"},
        {"nr", "ZA"},
        {"oc", "FR"},
        {"or", "IN"},
        {"os", "RU"},
        {"pa", "PK"},
        {"pl", "PL"},
        {"ps", "AF"},
        {"pt", "PT"},
        {"ro", "RO"},
        {"ru", "RU"},
        {"rw", "RW"},
        {"sa", "IN"},
        {"sc", "IT"},
        {"sd", "IN"},
        {"se", "NO"},
        {"si", "LK"},
        {"sk", "SK"},
        {"sl", "SI"},
        {"sq", "AL"},
        {"sr", "RS"},
        {"ss", "ZA"},
        {"st", "ZA"},
        {"sv", "SE"},
        {"ta", "IN"},
        {"te", "IN"},
        {"tg", "TJ"},
        {"th", "TH"},
        {"tk", "TM"},
        {"tl", "PH"},
        {"tn", "ZA"},
        {"tr", "TR"},
        {"ts", "ZA"},
        {"tt", "RU"},
        {"ug", "CN"},
        {"uk", "UA"},
        {"ur", "PK"},
        {"uz", "UZ"},
        {"ve", "ZA"},
        {"vi", "VN"},
        {"wa", "BE"},
        {"wo", "SN"},
        {"xh", "ZA"},
        {"yi", "US"},
        {"yo", "NG"},
        {"zh", "CN"},
    };

    for (int i = 0; i < lenof(map); i++) {
        if (language[0] == map[i].language[0]
         && language[1] == map[i].language[1]) {
            return map[i].dialect;
        }
    }
    return "";
}

/*-----------------------------------------------------------------------*/

int open_file(const char *path)
{
    PRECOND(path != NULL, return 0);
    return sys_open_file(path);
}

/*-----------------------------------------------------------------------*/

int open_url(const char *url)
{
    PRECOND(url != NULL, return 0);
    return sys_open_url(url);
}

/*-----------------------------------------------------------------------*/

void reset_idle_timer(void)
{
    sys_reset_idle_timer();
}

/*-----------------------------------------------------------------------*/

int set_performance_level(int level)
{
    if (UNLIKELY(level < PERFORMANCE_LEVEL_LOW)) {
        DLOG("Invalid parameter: %d", level);
        return 0;
    }
    return sys_set_performance_level(level);
}

/*-----------------------------------------------------------------------*/

int split_args(char *s, int insert_dummy, int *argc_ret, char ***argv_ret)
{
    if (!s || !argc_ret || !argv_ret) {
        DLOG("Invalid parameters: %p[%s] %d %p %p", s, s ? s : "",
             insert_dummy, argc_ret, argv_ret);
        return 0;
    }

    const char whitespace[] = " \t\r\n";

    int argc = 0;
    char **argv = NULL;

    #define EXTEND_ARGV()  do { \
        argc++;                 \
        char **new_argv = mem_realloc(argv, sizeof(*argv) * argc, 0); \
        if (!new_argv) {        \
            DLOG("Out of memory: can't extend argv to %d entries", argc); \
            mem_free(argv);     \
            return 0;           \
        }                       \
        argv = new_argv;        \
    } while (0)

    if (insert_dummy) {
        EXTEND_ARGV();
        argv[argc-1] = (char *)"";
    }

    while (s += strspn(s, whitespace), *s != '\0') {
        EXTEND_ARGV();
        argv[argc-1] = s;
        while (*s && !strchr(whitespace, *s)) {
            if (*s == '\'' || *s == '"') {
                const char quote = *s;
                memmove(s, s+1, strlen(s+1) + 1);
                for (; *s && *s != quote; s++) {
                    if (quote=='"' && *s=='\\' && strchr("$`\"\r\n", s[1])) {
                        memmove(s, s+1, strlen(s+1) + 1);
                    }
                }
                if (*s) {
                    memmove(s, s+1, strlen(s+1) + 1);
                }
            } else if (*s == '\\') {
                if (s[1] == '\r' && s[2] == '\n') {
                    memmove(s, s+3, strlen(s+3) + 1);
                } else if (s[1] == '\n') {
                    memmove(s, s+2, strlen(s+2) + 1);
                } else {
                    memmove(s, s+1, strlen(s+1) + 1);
                    s++;
                }
            } else {
                s++;
            }
        }
        if (*s) {
            *s++ = '\0';
        }
    }

    EXTEND_ARGV();
    argc--;  // Don't include the fencepost in the argument count.
    argv[argc] = NULL;

    #undef EXTEND_ARGV

    *argc_ret = argc;
    *argv_ret = argv;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
