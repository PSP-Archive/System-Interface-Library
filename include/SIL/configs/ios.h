/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/ios.h: Configuration header for iOS.
 */

#ifndef SIL_CONFIGS_IOS_H
#define SIL_CONFIGS_IOS_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_IOS
#define SIL_SYSTEM_COMMON_HEADER  "sysdep/ios/common.h"

#define SIL_ARCH_ARM
#ifdef __arm64__
# define SIL_ARCH_ARM_64
#else
# define SIL_ARCH_ARM_32
#endif
#define SIL_ARCH_ARM_NEON
#define IS_LITTLE_ENDIAN

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define STRNICMP_SIZE_T  size_t

/* Apple recommends disabling glGetError() in release builds. */
#ifndef DEBUG
# define SIL_OPENGL_DISABLE_GETERROR
#endif

/* iOS devices use OpenGL ES. */
#define SIL_OPENGL_ES

/* Memory transposition seems to run fastest with small blocks. */
#define SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE  4

/*-----------------------------------------------------------------------*/

/*
 * The following iOS-specific configuration options are available and may
 * be enabled by defining the corresponding preprocessor symbol:
 *
 * - SIL_PLATFORM_IOS_USE_FILE_SHARING:  Define this to export copies of
 *   save files to the Documents directory, allowing users to copy the
 *   files off the device using iTunes.  When this is enabled, the program
 *   will also check when loading a save file whether a newer copy exists
 *   in the Documents directory; if so, that file is copied to the primary
 *   save directory (under Library/Application Support) before loading.
 *
 *   If GameKit is enabled, the entire set of exported files will be
 *   refreshed (copied or deleted, depending on whether the corresponding
 *   save file exists or not) when a player change is detected.
 *
 *   IMPORTANT:  This functionality should be considered _experimental_!
 *   The details of the file sharing implementation appear to be undefined
 *   by Apple documentation; SIL attempts to ensure that internal and
 *   external data copies are kept in sync without excessive load/save
 *   overhead, but there may still be a potential for data loss in either
 *   direction.  Use with care!
 *
 * - SIL_PLATFORM_IOS_USE_GAMEKIT:  Define this to enable GameKit-related
 *   code (Game Center player authentication and server-based achievements).
 *   If not defined, no GameKit classes will be referenced by the code, and
 *   the interface functions (ios_gamekit_*()) will always act as though no
 *   player is signed in to Game Center.
 */

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_IOS_H
