/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/main.h: Header for the SIL program driver.
 */

#ifndef SIL_SRC_MAIN_H
#define SIL_SRC_MAIN_H

/*************************************************************************/
/*************************************************************************/

/**
 * init_all:  Initialize all SIL subsystems.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int init_all(void);

/**
 * cleanup_all:  De-initialize all SIL subsystems.
 */
extern void cleanup_all(void);

/**
 * sil__main:  Program driver for SIL client code.  System-specific
 * program entry points should call this function after any necessary
 * system-specific initialization has been performed.
 *
 * On systems with POSIX-style command line arguments, the argument
 * parameters (argc and argv) can be passed directly to this function.
 * On other systems, an equivalent argument array should be created, with
 * the program name (or a suitable placeholder) in argv[0] and any program
 * arguments starting at argv[1], followed by a NULL value in argv[argc].
 *
 * The double underscore in the name is intentional, reflecting that this
 * is only a low-level wrapper for the core program's entry point (much
 * like _main() in the standard C library).
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 *     p_main: Pointer to sil_main() function.
 * [Return value]
 *     0 if sil_main() returned EXIT_SUCCESS.
 *     1 if sil_main() returned EXIT_FAILURE.
 *     2 if initialization failed before calling client code.
 */
extern int sil__main(int argc, const char **argv);

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_override_sil_main:  Set a function to be called from sil__main()
 * in place of sil_main().
 *
 * [Parameters]
 *     function: Override function, or NULL to restore default behavior.
 */
extern void TEST_override_sil_main(int (*function)(int, const char **));

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MAIN_H
