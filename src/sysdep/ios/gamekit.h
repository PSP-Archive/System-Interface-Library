/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/gamekit.h: Internal header for iOS GameKit interface
 * routines.
 */

#ifndef SIL_SRC_SYSDEP_IOS_GAMEKIT_H
#define SIL_SRC_SYSDEP_IOS_GAMEKIT_H

#include "SIL/sysdep/ios/gamekit.h"  // Include the public header.

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* Structure holding data for a single achievement. */

typedef struct iOSAchievement iOSAchievement;
struct iOSAchievement {
    char id[100];
    float progress;  // 0.0 - 1.0 (_not_ 100)
};

/**
 * iOSAchievementLoadCallback:  Type of the callback function passed to
 * ios_gamekit_load_achievements().  The achievement data sent by the
 * server (if any) is passed to the function in an array.
 *
 * [Parameters]
 *     num_achievements: Number of achievements loaded, or zero if an error
 *         occurred (network failure, no local player, etc.).
 *     achievements: Array of achievements (NULL if num_achievements == 0).
 */
typedef void iOSAchievementLoadCallback(int num_achievements,
                                        const iOSAchievement *achievements);

/*-----------------------------------------------------------------------*/

#ifdef SIL_PLATFORM_IOS_USE_GAMEKIT

/*----------------------------------*/

/**
 * ios_gamekit_player_id:  Return the player ID string for the authenticated
 * local player.
 *
 * This function differs from userdata.c's ios_current_player() in that
 * this function returns NULL whenever there is no authenticated player,
 * while ios_current_player() stores the most recently seen player ID and
 * returns that ID if there is no authenticated player, only returning
 * NULL if the user has never signed in to Game Center.
 *
 * [Return value]
 *     Player ID string, or NULL if no player is authenticated.
 */
extern const char *ios_gamekit_player_id(void);

/**
 * ios_gamekit_load_achievements:  Load the current state (for the current
 * local player) of all achievements from the Game Center server, calling
 * the given callback function when done (or on error).  If no local player
 * is authenticated, the callback function is called immediately with an
 * error indication.
 *
 * Note that the callback may be called from a different thread, and thus
 * should perform locking on shared data when necessary.
 *
 * [Parameters]
 *     callback: Completion callback function.
 */
extern void ios_gamekit_load_achievements(iOSAchievementLoadCallback *callback);

/**
 * ios_gamekit_update_achievements:  Update the current state (for the
 * current local player) of the given achievements on the Game Center
 * server.  No error indication is provided.
 *
 * [Parameters]
 *     num_achievements: Number of achievements to update.
 *     achievements: Array of achievements to update.
 */
extern void ios_gamekit_update_achievements(int num_achievements,
                                            const iOSAchievement *achievements);


/**
 * ios_gamekit_clear_achievements:  Clear all achievements for the current
 * player on the Game Center server.  No error indication is provided.
 */
extern void ios_gamekit_clear_achievements(void);

/*----------------------------------*/

#else  // !SIL_PLATFORM_IOS_USE_GAMEKIT

/* Define the functions as stubs so callers don't need conditional
 * compilation. */

#define ios_gamekit_player_id()                     (NULL)
#define ios_gamekit_load_achievements(callback)     ((*callback)(0, NULL))
#define ios_gamekit_update_achievements(num,array)  /*nothing*/
#define ios_gamekit_clear_achievements()            /*nothing*/

#endif

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_SYSDEP_IOS_GAMEKIT_H
