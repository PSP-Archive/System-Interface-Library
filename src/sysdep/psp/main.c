/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/main.c: Program entry point and related routines for the PSP.
 */

#include "src/base.h"
#include "src/main.h"
#include "src/math/fpu.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/file-read.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"
#include "src/time.h"

/*************************************************************************/
/************ Configuration option sanity checks and defaults ************/
/*************************************************************************/

#ifndef SIL_PLATFORM_PSP_MEMORY_POOL_SIZE
# error SIL_PLATFORM_PSP_MEMORY_POOL_SIZE must be defined!
#endif
#ifndef SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE
# error SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE must be defined!
#endif
#if SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE < 0
# error SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE cannot be negative!
#endif

#ifndef SIL_PLATFORM_PSP_MODULE_NAME
# define SIL_PLATFORM_PSP_MODULE_NAME  "user_module"
#endif

#ifndef SIL_PLATFORM_PSP_STACK_SIZE
# error SIL_PLATFORM_PSP_STACK_SIZE must be defined!
#endif
#if SIL_PLATFORM_PSP_STACK_SIZE <= 0
# error SIL_PLATFORM_PSP_STACK_SIZE must be positive!
#endif

/*************************************************************************/
/****************************** Global data ******************************/
/*************************************************************************/

/* PSP module information. */

PSP_MODULE_INFO(SIL_PLATFORM_PSP_MODULE_NAME, 0, 0, 1);
const PSP_MAIN_THREAD_PRIORITY(THREADPRI_MAIN);
const PSP_MAIN_THREAD_STACK_SIZE_KB((SIL_PLATFORM_PSP_STACK_SIZE+1023) / 1024);
const PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
const PSP_HEAP_SIZE_KB(0);

/*-----------------------------------------------------------------------*/

/* Suspend-requested flag.  Nonzero indicates that a system suspend
 * operation is pending. */
uint8_t psp_suspend;

/* Most recent system error code (used primarily by sys_file_*() functions). */
uint32_t psp_errno;

/* Suspend-ready semaphore.  Signalled by
 * sys_input_acknowledge_suspend_request() to allow a pending suspend
 * operation to complete. */
SceUID psp_suspend_ok_sema;

/* Resume semaphore.  Signalled when the system resumes from suspend, and
 * waited on by sys_input_acknowledge_suspend_request(). */
SceUID psp_resume_sema;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Main thread ID (referenced from the power callback). */
static SceUID main_thread;

/* Directory containing the program's executable file. */
static char executable_dir[256];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * init:  Perform program initialization.
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     True on success, false on failure.
 */
static int init(int argc, const char **argv);

/**
 * get_base_directory:  Extract the program's base directory from argv[0].
 *
 * [Parameters]
 *     argv0: Value of argv[0].
 *     buffer: Buffer for extracted pathname.
 *     bufsize: Size of buffer, in bytes.
 */
static void get_base_directory(const char *argv0, char *buffer, int bufsize);

/**
 * install_callbacks:  Install callbacks for exit requests and power events.
 *
 * [Return value]
 *     True on success, false on failure.
 */
static int install_callbacks(void);

/**
 * load_av_modules:  Load necessary audio/video decoder modules.
 *
 * [Return value]
 *     True on success, false on failure.
 */
static int load_av_modules(void);

/**
 * callback_thread:  Exit request / power callback monitoring thread.
 *
 * [Parameters]
 *     args: Argument size (unused).
 *     argp: Argument pointer (unused).
 * [Return value]
 *     Does not return.
 */
static int callback_thread(SceSize args, void *argp);

/**
 * exit_callback:  Exit request callback, called when the user requests
 * termination from the HOME button menu.
 *
 * [Parameters]
 *     arg1, arg2, userdata: Unused.
 * [Return value]
 *     Does not return.
 */
static int exit_callback(int arg1, int arg2, void *userdata);

/**
 * power_callback:  Power event callback.
 *
 * [Parameters]
 *     unknown: Unused.
 *     power_info: Power event flags.
 *     userdata: Unused.
 * [Return value]
 *     Always zero.
 */
static int power_callback(int unknown, int powerInfo, void *userdata);

/*************************************************************************/
/******************* Startup and shutdown entry points *******************/
/*************************************************************************/

/**
 * main:  Program entry point.  Performs PSP-specific initialization, then
 * calls out to sil__main().
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     Does not return.
 */
int main(int argc, char **argv_)
{
    const char **argv = (const char **)argv_;
    const char *argv_dummy[2];
    if (argc == 0) {
        DLOG("argc is zero, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = SIL_PLATFORM_PSP_MODULE_NAME;
        argv[1] = NULL;
    } else if (!argv[0]) {
        DLOG("argv[0] is null, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = SIL_PLATFORM_PSP_MODULE_NAME;
        argv[1] = NULL;
    }

    if (!init(argc, argv)) {
        exit(2);
    }

    /* Explicitly call exit() rather than simply returning, to ensure that
     * our exit() is called rather than the one defined in libc. */
    exit(sil__main(argc, argv));
}

/*-----------------------------------------------------------------------*/

/**
 * exit:  Terminate the program cleanly.  (Replaces the libc exit() function.)
 *
 * [Parameters]
 *     code: Exit code (zero = normal exit, nonzero = abnormal exit).
 * [Return value]
 *     Does not return.
 */
NORETURN void exit(int code)
{
    /* Ensure we don't try to run twice. */
    static int exiting = 0;
    psp_threads_lock();
    const int old_exiting = exiting;
    exiting = 1;
    psp_threads_unlock();
    if (old_exiting) {
        sceKernelExitThread(code);
        DLOG("sceKernelExitThread() returned!!");
        for (;;) {}
    }

    /* Ensure that any save data operation has finished (otherwise the
     * save data utility seems to crash the PSP on exit). */
    sys_userdata_perform(NULL);

#ifdef COVERAGE
    /* Flush coverage data before exiting.  (libgcov registers an atexit
     * handler, but we override libc's exit() so it won't get called.) */
    extern void __gcov_flush(void);
    __gcov_flush();
#endif

    /* Terminate the program.  This should never fail, but prevent the
     * program from wandering off into unknown territory if it does. */
    sceKernelExitGame();
    DLOG("sceKernelExitGame() failed!!");
    for (;;) {}
}

/*************************************************************************/
/******************** PSP-internal exported routines *********************/
/*************************************************************************/

const char *psp_executable_dir(void)
{
    return executable_dir;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int init(UNUSED int argc, const char **argv)
{
    /* Set up the FPU control register. */
    fpu_configure();

    /* Save the ID of this thread for use by the power callback. */
    main_thread = sceKernelGetThreadId();

    /* Install the exit request and power event callbacks. */
    if (!install_callbacks()) {
        DLOG("Failed to install system callbacks");
        return 0;
    }

    /* Find the program's base directory (always included in argv[0] by
     * the OS). */
    get_base_directory(argv[0], executable_dir, sizeof(executable_dir));

    /* Initialize the memory allocation subsystem. */
#ifdef SIL_PLATFORM_PSP_CXX_CONSTRUCTOR_HACK
    void *dummy = malloc(1);  // This will initialize things if necessary.
    if (!dummy) {
        DLOG("Failed to set up memory pools");
        return 0;
    }
    free(dummy);
#else
    if (!psp_mem_init()) {
        DLOG("Failed to set up memory pools");
        return 0;
    }
#endif

    /* Load audio/video decoding modules. */
    if (!load_av_modules()) {
        DLOG("Failed to load AV modules");
        return 0;
    }

    /* Set up suspend/resume semaphores. */
    psp_suspend_ok_sema = sceKernelCreateSema("SuspendOKSema", 0, 0, 1, NULL);
    if (UNLIKELY(psp_suspend_ok_sema < 0)) {
        DLOG("Failed to create suspend-OK semaphore: %s",
             psp_strerror(psp_suspend_ok_sema));
        return 0;
    }
    psp_resume_sema = sceKernelCreateSema("ResumeSema", 0, 0, 1, NULL);
    if (UNLIKELY(psp_resume_sema < 0)) {
        DLOG("Failed to create resume semaphore: %s",
             psp_strerror(psp_resume_sema));
        return 0;
    }

    /* Success! */
    return 1;
}

/*-----------------------------------------------------------------------*/

static void get_base_directory(const char *argv0, char *buffer, int bufsize)
{
    PRECOND(argv0 != NULL, return);
    PRECOND(buffer != NULL, return);
    PRECOND(bufsize > 0, return);

    const char *s = strrchr(argv0, '/');
    if (strncmp(argv0, "disc0:", 6) == 0) {
        strformat(buffer, bufsize, "disc0:/PSP_GAME/USRDIR");
    } else if (strncmp(argv0, "umd0:", 6) == 0) {
        strformat(buffer, bufsize, "umd0:/PSP_GAME/USRDIR");
    } else if (s != NULL) {
        int n = strformat(buffer, bufsize, "%.*s", s - argv0, argv0);
        if (UNLIKELY(n >= bufsize)) {
            DLOG("argv[0] too long! %s", argv0);
            *buffer = 0;
        }
    } else {
        DLOG("argv[0] has no directory: %s", argv0);
        *buffer = 0;
    }
}

/*-----------------------------------------------------------------------*/

static int install_callbacks(void)
{
    SceUID thid = psp_start_thread(
        "SysCallbackThread", callback_thread, THREADPRI_CALLBACK_WATCH,
        0x1000, 0, NULL);
    if (UNLIKELY(thid < 0)) {
        DLOG("psp_start_thread(callback_thread) failed: %s",
             psp_strerror(thid));
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static int load_av_modules(void)
{
    int res;

    res = sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
    if (UNLIKELY(res < 0)) {
        DLOG("sceUtilityLoadAvModule(AVCODEC): %s", psp_strerror(res));
        return 0;
    }
    res = sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);
    if (UNLIKELY(res < 0)) {
        DLOG("sceUtilityLoadAvModule(MPEGBASE): %s",psp_strerror(res));
        sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC);
        return 0;
    }

    return 1;
}

/*************************************************************************/
/*********************** System callback routines ************************/
/*************************************************************************/

static int callback_thread(UNUSED SceSize args, UNUSED void *argp)
{
    SceUID cbid;
    cbid = sceKernelCreateCallback("ExitCallback", exit_callback, NULL);
    if (UNLIKELY(cbid < 0)) {
        DLOG("sceKernelCreateCallback(exit_callback) failed: %s",
             psp_strerror(cbid));
        return 0;
    }
    sceKernelRegisterExitCallback(cbid);

    cbid = sceKernelCreateCallback("PowerCallback", power_callback, NULL);
    if (UNLIKELY(cbid < 0)) {
        DLOG("sceKernelCreateCallback(power_callback) failed: %s",
             psp_strerror(cbid));
        return 0;
    }
    const int slot = scePowerRegisterCallback(-1, cbid);
    if (UNLIKELY(slot < 0)) {
        DLOG("scePowerRegisterCallback(power_callback) failed: %s",
             psp_strerror(slot));
        return 0;
    }

    for (;;) {
        sceKernelSleepThreadCB();
    }
    UNREACHABLE;
}

/*-----------------------------------------------------------------------*/

static int exit_callback(UNUSED int arg1, UNUSED int arg2,
                         UNUSED void *userdata)
{
    exit(0);
}

/*-----------------------------------------------------------------------*/

static int power_callback(UNUSED int unknown, int power_info,
                          UNUSED void *userdata)
{
    if (power_info & (PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_STANDBY)) {
        /* We get PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_POWER_SWITCH when
         * the user hits the power switch to wake up the device, so don't
         * try to suspend again on that. */
        if (!psp_suspend) {
            /* Clear out any leftover value in the semaphores (should never
             * happen, but better safe than sorry). */
            SceUInt zero = 0;
            while (sceKernelWaitSema(psp_suspend_ok_sema, 1, &zero) == 0) {}
            while (sceKernelWaitSema(psp_resume_sema, 1, &zero) == 0) {}
            psp_suspend = 1;
            sceKernelWaitSema(psp_suspend_ok_sema, 1, NULL);
        }
    } else if (power_info & PSP_POWER_CB_RESUME_COMPLETE) {
        /* Presumably we should only see this once per resume, but who knows?
         * Let's play it safe. */
        if (psp_suspend) {
            psp_suspend = 0;
            sceKernelSignalSema(psp_resume_sema, 1);
        }
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
