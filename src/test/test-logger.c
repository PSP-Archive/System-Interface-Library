/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/test-logger.c: Utility routines for logging test failures and
 * warnings.
 */

#include "src/base.h"
#include "src/test/base.h"

/*************************************************************************/
/******************************* Local data ******************************/
/*************************************************************************/

/* Note that all logging data is stored in static buffers so it does not
 * interact with the memory allocation subsystem (which may be subject to
 * test). */

/* Number of log entries to keep. */
#define LOG_ENTRIES  1000

/* Log entry data. */
typedef struct LogEntry LogEntry;
struct LogEntry {
    char filename[100];
    int line;
    char function[100];
    TestLogType type;
    char message[500];
};
static LogEntry test_log[LOG_ENTRIES];
static int num_log_entries = 0;

/* Flag set if the log overflowed (so we can tell the user). */
static uint8_t log_overflowed = 0;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void testlog_log(const char *filename, int line, const char *function,
                 TestLogType type, const char *format, ...)
{
    PRECOND(filename != NULL, return);
    PRECOND(function != NULL, return);
    PRECOND(format != NULL, return);

    /* Slightly larger than LogEntry.message so we can detect overflow. */
    char message_buf[510];

    va_list args;
    va_start(args, format);
    vstrformat(message_buf, sizeof(message_buf), format, args);
    va_end(args);

    /* Send the message to the standard logging interface first so we can
     * see it in realtime. */
    do_DLOG(filename, line, function, "%s", message_buf);

    if (num_log_entries >= lenof(test_log)) {
        log_overflowed = 1;
        return;
    }

    LogEntry * const log_entry = &test_log[num_log_entries++];

    #define COPY_OR_TRUNCATE(buffer,string,truncate_before)  do {       \
        const char * const _truncator =                                 \
            truncate_before ? "[truncated] ..." : "... [truncated]";    \
        if (!strformat_check(buffer, sizeof(buffer), "%s", string)) {   \
            ASSERT(sizeof(buffer) > strlen(_truncator) + 1);            \
            char * const _dest = truncate_before ? buffer :             \
                buffer + strlen(buffer) - strlen(_truncator);           \
            memcpy(_dest, _truncator, strlen(_truncator));              \
        }                                                               \
    } while (0)

    COPY_OR_TRUNCATE(log_entry->filename, filename, 1);
    log_entry->line = line;
    COPY_OR_TRUNCATE(log_entry->function, function, 0);
    log_entry->type = type;
    COPY_OR_TRUNCATE(log_entry->message, message_buf, 0);

    #undef COPY_OR_TRUNCATE
}

/*-----------------------------------------------------------------------*/

int testlog_count_entries(TestLogType type)
{
    int count = 0;
    for (int i = 0; i < num_log_entries; i++) {
        if (test_log[i].type == type) {
            count++;
        }
    }
    return count;
}

/*-----------------------------------------------------------------------*/

void testlog_print(void)
{
    for (int i = 0; i < num_log_entries; i++) {
        if (test_log[i].type != TESTLOG_SKIP) {
            do_DLOG(test_log[i].filename, test_log[i].line,
                    test_log[i].function, "%s", test_log[i].message);
        }
    }

    if (log_overflowed) {
        DLOG("(following failures truncated)");
    }
}

/*-----------------------------------------------------------------------*/

const char *_memequal_failure_message(
    const uint8_t *value, const uint8_t *expected, int size)
{
    int pos;
    if (size <= 16) {
        pos = 0;
    } else {
        for (pos = 0; pos < size; pos++) {
            if (value[pos] != expected[pos]) {
                break;
            }
        }
        pos = bound(pos-4, 0, size-16);
    }

    char value_str[49], expected_str[49];
    for (int i = 0; i < 16 && pos+i < size; i++) {
        strformat(&value_str[i*3], 4, "%c%02X",
                  (value[pos+i] == expected[pos+i]) ? ' ' : '*', value[pos+i]);
        strformat(&expected_str[i*3], 4, " %02X", expected[pos+i]);
    }

    static char buf[200];
    ASSERT(strformat_check(buf, sizeof(buf),
                           "did not match expected data (%d bytes)"
                           "\n      Actual [@0x%X]: %s"
                           "\n    Expected [@0x%X]: %s",
                           size, pos, value_str, pos, expected_str));
    return buf;
}

/*************************************************************************/
/*************************************************************************/
