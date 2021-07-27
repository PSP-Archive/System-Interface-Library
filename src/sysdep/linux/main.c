/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/main.c: Program entry point for Linux.
 */

#include "src/base.h"
#include "src/main.h"
#include "src/math/fpu.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/posix/path_max.h"
#include "src/utility/misc.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Pathname of executable's directory, or "." if unknown. */
static char executable_dir[PATH_MAX+1];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * term_signal_handler:  Signal handler for ordinary termination signals
 * (SIGTERM, SIGINT, and SIGHUP).  Sets the quit-requested flag and
 * discards the signal.
 *
 * [Parameters]
 *     signum: Signal number (unused).
 */
static void term_signal_handler(int signum);

/**
 * fatal_signal_handler:  Signal handler for fatal signals (SIGSEGV,
 * SIGQUIT, etc).  Attempts to reset the video mode if it was changed,
 * then terminates the program.
 *
 * [Parameters]
 *     signum: Signal number.
 */
static void fatal_signal_handler(int signum);

/*************************************************************************/
/************************** Program entry point **************************/
/*************************************************************************/

int main(int argc, char **argv_)
{
    const char **argv = (const char **)argv_;

    /* Install signal handlers to avoid being terminated with the display
     * in an unusable state.  We set SA_RESTART so we don't have to worry
     * about handling EINTR from system calls (but see inotify handling in
     * input.c for an exception). */
    struct sigaction term_sigaction, fatal_sigaction;
    mem_clear(&term_sigaction, sizeof(term_sigaction));
    term_sigaction.sa_handler = term_signal_handler;
    term_sigaction.sa_flags = SA_RESTART;
    mem_clear(&fatal_sigaction, sizeof(fatal_sigaction));
    fatal_sigaction.sa_handler = fatal_signal_handler;
    fatal_sigaction.sa_flags = SA_RESTART;
    sigaction(SIGHUP,    &term_sigaction, NULL);
    sigaction(SIGINT,    &term_sigaction, NULL);
    sigaction(SIGQUIT,   &fatal_sigaction, NULL);
    sigaction(SIGILL,    &fatal_sigaction, NULL);
    sigaction(SIGTRAP,   &fatal_sigaction, NULL);
    sigaction(SIGABRT,   &fatal_sigaction, NULL);
    sigaction(SIGBUS,    &fatal_sigaction, NULL);
    sigaction(SIGFPE,    &fatal_sigaction, NULL);
    signal   (SIGUSR1,   SIG_IGN);
    sigaction(SIGSEGV,   &fatal_sigaction, NULL);
    signal   (SIGUSR2,   SIG_IGN);
    signal   (SIGPIPE,   SIG_IGN);
    signal   (SIGALRM,   SIG_IGN);
    sigaction(SIGTERM,   &term_sigaction, NULL);
    sigaction(SIGSTKFLT, &fatal_sigaction, NULL);
    sigaction(SIGXCPU,   &fatal_sigaction, NULL);
    sigaction(SIGXFSZ,   &fatal_sigaction, NULL);
    signal   (SIGVTALRM, SIG_IGN);
    signal   (SIGIO,     SIG_IGN);
    sigaction(SIGSYS,    &fatal_sigaction, NULL);
    /* As a special case, if SIGPROF is not SIG_DFL (presumably because a
     * profiler is running), leave it alone. */
    struct sigaction sa_sigprof;
    if (sigaction(SIGPROF, NULL, &sa_sigprof) != 0
     || sa_sigprof.sa_handler == SIG_DFL) {
        signal(SIGPROF, SIG_IGN);
    }

    /* Other environmental setup. */
    fpu_configure();

    /* Open the display device.  We do this outside of sys_graphics_init()
     * so error windows can potentially be shown if needed (though we
     * don't actually have a need for that at the moment). */
    if (!linux_open_display()) {
        fprintf(stderr, "Error: Unable to open display device!\n"
                "Check that your DISPLAY environment variable is set"
                " correctly.\n");
        return 2;
    }

    /* Find the base directory for file access, either from an environment
     * variable or by looking up the directory containing our executable.
     * Note that readlink() returns the number of bytes stored, not the
     * actual length of the pathname, so we set a buffer size of PATH_MAX+2
     * (instead of +1) and assume that a return value of PATH_MAX+1
     * indicates a truncated name. */
    strcpy(executable_dir, ".");  // Safe.
    char pathbuf[PATH_MAX+2];
    memset(pathbuf, 0, sizeof(pathbuf));
    const ssize_t len = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf)-1);
    if (len < 0) {
        DLOG("readlink(/proc/self/exe) failed: %s", strerror(errno));
    } else if (len >= (ssize_t)(sizeof(pathbuf)-1)) {
        DLOG("Pathname too long, can't read executable directory");
    } else if (*pathbuf != '/') {
        DLOG("Executable pathname is not relative: %.*s", (int)len, pathbuf);
    } else {
        char *slash = strrchr(pathbuf, '/');
        ASSERT(slash != NULL, slash = pathbuf);
        const int dir_len = slash - pathbuf;
        ASSERT(strformat_check(executable_dir, sizeof(executable_dir), "%.*s",
                               dir_len, pathbuf));
    }

    /* Sanity-check program arguments. */
    const char *argv_dummy[2];
    if (argc == 0) {
        DLOG("argc is zero, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = "SIL";
        argv[1] = NULL;
    } else if (!argv[0]) {
        DLOG("argv[0] is null, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = "SIL";
        argv[1] = NULL;
    }

    /* Call the common SIL entry point. */
    const int exitcode = sil__main(argc, argv);

    /* Shut down the display and exit. */
    linux_close_display();
    return exitcode;
}

/*************************************************************************/
/******************* Linux-internal exported routines ********************/
/*************************************************************************/

const char *linux_executable_dir(void)
{
    return executable_dir;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void term_signal_handler(UNUSED int signum)
{
    linux_set_quit_requested();
}

/*-----------------------------------------------------------------------*/

static void fatal_signal_handler(int signum)
{
    linux_reset_video_mode();
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
}

/*************************************************************************/
/*************************************************************************/
