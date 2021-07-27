/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/gamekit.m: iOS GameKit interface routines.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/gamekit.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"


#ifndef SIL_PLATFORM_IOS_USE_GAMEKIT

/* Define stubs for the exported interface routines. */
void ios_gamekit_authenticate(void) {}
int ios_gamekit_auth_status(void) {return IOS_GAMEKIT_AUTH_FAILED;}
void ios_gamekit_acknowledge_auth_change(void) {}

#else  // SIL_PLATFORM_IOS_USE_GAMEKIT, to the end of the file.

#import <GameKit/GameKit.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Current authentication status (IOS_GAMEKIT_AUTH_*). */
static int auth_status = IOS_GAMEKIT_AUTH_NOTYET;

/* Current authenticated player ID (as a mem_alloc()ed string).  Non-NULL
 * if and only if there is an authenticated local player. */
static char *player_id;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * do_auth:  Perform the initial Game Center authentication.  Called as a
 * vertical sync function to ensure that it's run in the main thread.
 *
 * [Parameters]
 *     unused: V-sync callback parameter (unused).
 */
static void do_auth(void *unused);

/**
 * handle_auth_response:  Handle an authorization response from GameKit.
 *
 * [Parameters]
 *     error: Error object passed to the GameKit authentication callback.
 */
static void handle_auth_response(NSError *error);

/**
 * handle_auth_change:  Handle a change in authentication status.
 */
static void handle_auth_change(void);

/**
 * do_load_achievements:  Load achievements for the current local player
 * from the server.  Called as a vertical sync function.
 *
 * [Parameters]
 *     callback: V-sync callback parameter (load callback function).
 */
static void do_load_achievements(void *callback);

/**
 * do_update_achievements:  Update the given achievements for the current
 * local player on the server, and free the passed-in achievement array.
 * Called as a vertical sync function.
 *
 * [Parameters]
 *     achievements: V-sync callback parameter (NULL-terminated achievement
 *         array).
 */
static void do_update_achievements(void *unused);

/**
 * do_clear_achievements:  Clear all achievements for the current local
 * player from the server.  Called as a vertical sync function.
 *
 * [Parameters]
 *     unused: V-sync callback parameter (unused).
 */
static void do_clear_achievements(void *unused);

/*************************************************************************/
/********************** Exported interface routines **********************/
/*************************************************************************/

void ios_gamekit_authenticate(void)
{
    if (auth_status != IOS_GAMEKIT_AUTH_NOTYET) {
        return;
    }
    auth_status = IOS_GAMEKIT_AUTH_PENDING;
    ios_register_vsync_function(do_auth, NULL);
}

/*-----------------------------------------------------------------------*/

int ios_gamekit_auth_status(void)
{
    return auth_status;
}

/*-----------------------------------------------------------------------*/

void ios_gamekit_acknowledge_auth_change(void)
{
    if (player_id) {
        auth_status = IOS_GAMEKIT_AUTH_OK;
    } else {
        auth_status = IOS_GAMEKIT_AUTH_FAILED;
    }
}

/*************************************************************************/
/****************** Library-internal interface routines ******************/
/*************************************************************************/

const char *ios_gamekit_player_id(void)
{
    return auth_status==IOS_GAMEKIT_AUTH_OK ? player_id : NULL;
}

/*-----------------------------------------------------------------------*/

void ios_gamekit_load_achievements(iOSAchievementLoadCallback *callback)
{
    PRECOND(callback != NULL, return);
    if (!player_id) {
        DLOG("No authenticated local player");
        (*callback)(0, NULL);
        return;
    }

    ios_register_vsync_function(do_load_achievements, callback);
}

/*-----------------------------------------------------------------------*/

void ios_gamekit_update_achievements(int num_achievements,
                                     const iOSAchievement *achievements)
{
    PRECOND(num_achievements > 0, return);
    PRECOND(achievements != NULL, return);
    if (!player_id) {
        DLOG("No authenticated local player");
        return;
    }

    /* Make a local copy of the achievement data so the caller doesn't have
     * to keep it around.  do_update_achievements() will free it. */
    iOSAchievement *achievements_to_update =
        mem_alloc((num_achievements+1) * sizeof(*achievements_to_update), 0,
                  MEM_ALLOC_TEMP);
    memcpy(achievements_to_update, achievements,
           num_achievements * sizeof(*achievements_to_update));
    achievements_to_update[num_achievements].id[0] = 0;  // Fencepost.

    ios_register_vsync_function(do_update_achievements, achievements_to_update);
}

/*-----------------------------------------------------------------------*/

void ios_gamekit_clear_achievements(void)
{
    if (!player_id) {
        DLOG("No authenticated local player");
        return;
    }

    ios_register_vsync_function(do_clear_achievements, NULL);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

/* Dummy class used to hook into the authentication change notification. */

@interface AuthChangeHandler: NSObject
@end
@implementation AuthChangeHandler
- (void)call {handle_auth_change();}
- (void)register_ {
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call)
        name:GKPlayerAuthenticationDidChangeNotificationName object:nil];
}
@end

/*-----------------------------------------------------------------------*/

static void do_auth(UNUSED void *unused)
{
    if (!NSClassFromString(@"GKLocalPlayer")) {
        DLOG("GKLocalPlayer class not found, disabling Game Center support");
        auth_status = IOS_GAMEKIT_AUTH_FAILED;
        return;
    }

    [[GKLocalPlayer localPlayer] authenticateWithCompletionHandler:
        ^(NSError *error) {
            handle_auth_response(error);
        }
    ];
}

/*-----------------------------------------------------------------------*/

static void handle_auth_response(NSError *error)
{
    if (!error) {
        const char *player_id_src =
            [[GKLocalPlayer localPlayer].playerID UTF8String];
        if (!player_id_src) {
            DLOG("Player ID string is NULL!");
            auth_status = IOS_GAMEKIT_AUTH_FAILED;
        } else {
            player_id = mem_strdup(player_id_src, 0);
            if (UNLIKELY(!player_id)) {
                DLOG("mem_strdup() failed!");
                auth_status = IOS_GAMEKIT_AUTH_FAILED;
            } else {
                auth_status = IOS_GAMEKIT_AUTH_OK;
            }
        }
    } else {
        auth_status = IOS_GAMEKIT_AUTH_FAILED;
        DLOG("GameKit authorization failed: [%s/%ld] %s",
             [error.domain UTF8String], (long)error.code,
             [error.localizedDescription UTF8String]);
    }
    AuthChangeHandler *handler = [[AuthChangeHandler alloc] init];
    [handler register_];
}

/*-----------------------------------------------------------------------*/

static void handle_auth_change(void)
{
    auth_status = IOS_GAMEKIT_AUTH_CHANGED;
    mem_free(player_id);
    if ([GKLocalPlayer localPlayer].authenticated) {
        player_id =
            mem_strdup([[GKLocalPlayer localPlayer].playerID UTF8String], 0);
        if (UNLIKELY(!player_id)) {
            DLOG("mem_strdup() failed!");
        }
    } else {
        player_id = NULL;
    }
}

/*-----------------------------------------------------------------------*/

static void do_load_achievements(void *callback_)
{
    iOSAchievementLoadCallback *callback =
        (iOSAchievementLoadCallback *)callback_;
    DLOG("Loading achievements from Game Center...");
    [GKAchievement loadAchievementsWithCompletionHandler:
                       ^(NSArray *achievements, NSError *error) {
        if (error) {
            DLOG("Failed to load achievements: [%s/%ld] %s",
                 [error.domain UTF8String], (long)error.code,
                 [error.localizedDescription UTF8String]);
            (*callback)(0, NULL);
        } else if (achievements.count == 0) {
            /* No achievement data present. */
            DLOG("No achievements loaded.");
            (*callback)(0, NULL);
        } else {
            DLOG("%d achievements loaded.", (int)achievements.count);
            iOSAchievement *ach_array =
                mem_alloc(sizeof(*ach_array) * achievements.count, 0,
                          MEM_ALLOC_TEMP);
            if (!ach_array) {
                DLOG("Out of memory laoding achievements!");
                (*callback)(0, NULL);
            } else {
                int num_achievements = 0;
                for (unsigned int i = 0; i < achievements.count; i++) {
                    GKAchievement *ach_in = [achievements objectAtIndex:i];
                    iOSAchievement *ach_out = &ach_array[num_achievements];
                    if (!strformat_check(ach_out->id, sizeof(ach_out->id), "%s",
                                         [ach_in.identifier UTF8String])) {
                        DLOG("Buffer overflow on achievement ID: %s",
                             [ach_in.identifier UTF8String]);
                        continue;
                    }
                    ach_out->progress = ach_in.percentComplete / 100;
                    num_achievements++;
                }
                (*callback)(num_achievements, ach_array);
                mem_free(ach_array);
            }
        }
    }];
}

/*-----------------------------------------------------------------------*/

static void do_update_achievements(void *achievements_)
{
    iOSAchievement *achievements = (iOSAchievement *)achievements_;
    for (unsigned int i = 0; achievements[i].id[0] != 0; i++) {
        NSString *ach_id =
            [[NSString alloc] initWithUTF8String:achievements[i].id];
        GKAchievement *achievement =
            [[[GKAchievement alloc] initWithIdentifier:ach_id] autorelease];
        [ach_id release];
        achievement.percentComplete = achievements[i].progress * 100;
        DLOG("Updating achievement %s on Game Center...", achievements[i].id);
        [achievement reportAchievementWithCompletionHandler:^(NSError *error) {
            if (error) {
                DLOG("Failed to update achievement: [%s/%ld] %s",
                     [error.domain UTF8String], (long)error.code,
                     [error.localizedDescription UTF8String]);
            } else {
                DLOG("Successfully updated achievement.");
            }
        }];
    }
    mem_free(achievements);
}

/*-----------------------------------------------------------------------*/

static void do_clear_achievements(UNUSED void *unused)
{
    DLOG("Clearing achievements on Game Center...");
    [GKAchievement resetAchievementsWithCompletionHandler:^(NSError *error) {
        if (error) {
            DLOG("Failed to clear achievements: [%s/%ld] %s",
                 [error.domain UTF8String], (long)error.code,
                 [error.localizedDescription UTF8String]);
        } else {
            DLOG("Successfully cleared achievements.");
        }
    }];

#ifdef DEBUG
    /* Prevent the linker from removing the test function below. */
    extern void ios_gamekit_set_player_id(const char *);
    void *dummy;
    __asm__ volatile("": "=r" (dummy): "r" (ios_gamekit_set_player_id));
#endif
}

/*************************************************************************/
/*************************************************************************/

#ifdef DEBUG

/**
 * ios_gamekit_set_player_id:  Overwrite the current player ID with the
 * given string, simulating a GameKit authorization change.  Pass the empty
 * string (or NULL) to sign out the current player without changing to a
 * new one.
 *
 * THIS WILL CAUSE A DESYNC WITH GAMEKIT.  Use only for testing!
 *
 * [Parameters]
 *     new_id: New player ID to use.
 */
void ios_gamekit_set_player_id(const char *new_id) __attribute__((used));
void ios_gamekit_set_player_id(const char *new_id)
{
    auth_status = IOS_GAMEKIT_AUTH_CHANGED;
    mem_free(player_id);
    if (new_id && *new_id) {
        player_id = mem_strdup(new_id, 0);
        if (UNLIKELY(!player_id)) {
            DLOG("mem_strdup() failed!");
        }
    } else {
        player_id = NULL;
    }
}

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_PLATFORM_IOS_USE_GAMEKIT
