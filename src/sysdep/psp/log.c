/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/log.c: Log writing routines for the PSP.
 */

#ifdef DEBUG  // To the end of the file.

#define IN_SYSDEP

#include "src/base.h"
#include "src/debug.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Debug message buffer and associated data. */
static char DLOG_buffer[16384];         // Buffer for message text.
static unsigned int DLOG_buffer_index;  // Offset for writing next message.
static struct {                         // Data for each line:
    uint16_t offset;                    //  - Offset of the start of this line.
    uint8_t length;                     //  - Length of line (in bytes).
    uint8_t indented;                   //  - Whether to indent this line.
} DLOG_lines[100];
static int DLOG_lines_index;            // DLOG_lines[] index for next line.

/* Debug message display parameter. */
#define DLOG_DISPLAY_X0      18
#define DLOG_DISPLAY_Y0      16
#define DLOG_DISPLAY_X1      (480-18)
#define DLOG_DISPLAY_Y1      (272-16)
#define DLOG_DISPLAY_BORDER  4
#define DLOG_DISPLAY_INDENT  10

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *sys_log_open(const char *name)
{
    char pathbuf[1000];
    if (strformat_check(pathbuf, sizeof(pathbuf), "%s/%s",
                        psp_executable_dir(), name)) {
        int fd = sceIoOpen(pathbuf, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC,
                           0666);
        if (fd >= 0) {
            return (void *)(intptr_t)(fd + 1);
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_log_write(void *logfile, const char *message, int len)
{
    const int fd = logfile ? (int)(intptr_t)logfile - 1 : 2;
    sceIoWrite(fd, message, len+1);

    if (logfile) {
        return;  // Only update the buffer for non-file writes.
    }

    const unsigned int start = DLOG_buffer_index;
    const int size = ubound(len+1, (int)sizeof(DLOG_buffer)-1);

    /* If the message would overflow the end of the buffer, jump back to
     * the beginning (but leave "start" alone so we can detect overwrites). */
    if (DLOG_buffer_index + size > sizeof(DLOG_buffer)) {
        DLOG_buffer_index = 0;
    }

    /* Record the starting point of this line (erasing the oldest line if
     * the line buffer is full). */
    if (DLOG_lines_index >= lenof(DLOG_lines)) {
        memmove(&DLOG_lines[0], &DLOG_lines[1],
                sizeof(DLOG_lines) - sizeof(DLOG_lines[0]));
        DLOG_lines_index = lenof(DLOG_lines) - 1;
    }
    unsigned int line = DLOG_lines_index++;
    DLOG_lines[line].offset = DLOG_buffer_index;
    DLOG_lines[line].length = 0;
    DLOG_lines[line].indented = 0;

    /* Store the message text in the text buffer. */
    memcpy(&DLOG_buffer[DLOG_buffer_index], message, size);
    DLOG_buffer_index += size;

    /* Delete any overwritten lines. */
    const unsigned int overwritten =
        ((DLOG_buffer_index + sizeof(DLOG_buffer)) - start)
        % sizeof(DLOG_buffer);
    for (int i = DLOG_lines_index-2; i >= 0; i--) {
        const unsigned int offset =
            ((DLOG_lines[i].offset + sizeof(DLOG_buffer)) - start)
            % sizeof(DLOG_buffer);
        if (offset < overwritten) {
            /* We're searching backward, so this is the newest overwritten
             * line; delete it and all older lines. */
            const int num_to_delete = i + 1;
            memmove(&DLOG_lines[0], &DLOG_lines[num_to_delete],
                    sizeof(DLOG_lines[0]) * (lenof(DLOG_lines)-num_to_delete));
            DLOG_lines_index -= num_to_delete;
            break;
        }
    }

    /* Break this message into display-width-sized lines. */
    uint32_t linestart = DLOG_lines[line].offset;
    int indented = 0;
    do {
        const int x = DLOG_DISPLAY_X0 + DLOG_DISPLAY_BORDER
                      + (indented ? DLOG_DISPLAY_INDENT : 0);
        const int w = (DLOG_DISPLAY_X1 - DLOG_DISPLAY_BORDER) - x;
        const char *this_line = &DLOG_buffer[linestart];
        const int left = strlen(this_line);
        int linelen = 1;
        while (linelen < left
               && linelen+1 <= len
               && debug_text_width(this_line, linelen+1) <= w)
        {
            linelen++;
        }
        DLOG_lines[line].offset = linestart;
        DLOG_lines[line].length = linelen;
        DLOG_lines[line].indented = indented;
        indented = 1;
        linestart += linelen;
        if (DLOG_buffer[linestart]) {  // Not yet at the end of the string.
            if (DLOG_lines_index < lenof(DLOG_lines)) {
                line = DLOG_lines_index++;
            } else {
                memmove(&DLOG_lines[0], &DLOG_lines[1],
                        sizeof(DLOG_lines[0]) * line);
            }
        }
    } while (linestart < DLOG_buffer_index);
}

/*-----------------------------------------------------------------------*/

void sys_log_close(void *logfile)
{
    sceIoClose((int)(intptr_t)logfile - 1);
}

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

void psp_debug_display_log(void)
{
    const int x0 = DLOG_DISPLAY_X0;
    const int y0 = DLOG_DISPLAY_Y0;
    const int x1 = DLOG_DISPLAY_X1;
    const int y1 = DLOG_DISPLAY_Y1;
    const int border = DLOG_DISPLAY_BORDER;
    const int indent = DLOG_DISPLAY_INDENT;
    static const Vector4f background_color = {0, 0, 0, 1.0/3.0};
    static const Vector4f text_color = {1, 1, 1, 1};

    const int fonth = 12;
    int y = y1 - border - fonth;
    int line = DLOG_lines_index - 1;

    debug_fill_box(x0, y+fonth, x1-x0, y1-(y+fonth), &background_color);
    while (y >= y0+border && line >= 0) {
        debug_fill_box(x0, y, x1-x0, fonth, &background_color);
        const int x = x0 + border + (DLOG_lines[line].indented ? indent : 0);
        debug_draw_text(x, y, 1, &text_color, "%.*s",
                        DLOG_lines[line].length,
                        &DLOG_buffer[DLOG_lines[line].offset]);
        y -= fonth;
        line--;
    }
    y += fonth;
    debug_fill_box(x0, y-border, x1-x0, border, &background_color);
}

/*-----------------------------------------------------------------------*/

void psp_debug_dump_log(const char *path)
{
    int fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (fd < 0) {
        DLOG("Failed to open %s: %s", path, psp_strerror(fd));
    } else {
        for (int line = 0; line < DLOG_lines_index; line++) {
            if (line > 0 && !DLOG_lines[line].indented) {
                sceIoWrite(fd, "\n", 1);
            }
            sceIoWrite(fd, &DLOG_buffer[DLOG_lines[line].offset],
                       DLOG_lines[line].length);
        }
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
