/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/ios/gamekit.h: Declarations for the interface to the
 * iOS GameKit framework.
 */

#ifndef SIL_SYSDEP_IOS_GAMEKIT_H
#define SIL_SYSDEP_IOS_GAMEKIT_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* Constants returned from ios_gamekit_auth_status() indicating the current
 * authentication status. */

enum {
    /* ios_gamekit_authenticate() has not yet been called. */
    IOS_GAMEKIT_AUTH_NOTYET = 0,

    /* Authentication is currently in progress. */
    IOS_GAMEKIT_AUTH_PENDING,

    /* A local player has been successfully authenticated. */
    IOS_GAMEKIT_AUTH_OK,

    /* Authentication failed (or was cancelled by the user). */
    IOS_GAMEKIT_AUTH_FAILED,

    /* The current authentication state has changed: either the previous
     * player hsa logged out, or a new player has logged in.  This status
     * remains in effect (and ios_gamekit_player_id() returns NULL) until
     * ios_gamekit_acknowledge_auth_change() is called. */
    IOS_GAMEKIT_AUTH_CHANGED,
};

/*-----------------------------------------------------------------------*/

/**
 * ios_gamekit_authenticate:  Begin the Game Center authentication process.
 * This routine returns immediately, but touch input may be blocked for an
 * extended period of time.  Call ios_gamekit_auth_status() to check
 * whether authentication has completed.
 */
extern void ios_gamekit_authenticate(void);

/**
 * ios_gamekit_auth_status:  Return the current Game Center authentication
 * status.
 *
 * [Return value]
 *     Current authentication status (IOS_GAMEKIT_AUTH_*).
 */
extern int ios_gamekit_auth_status(void);

/**
 * ios_gamekit_acknowledge_auth_change:  Acknowledge a change in local
 * player authentication state.  After calling this function, the
 * authentication status will change to either IOS_GAMEKIT_AUTH_OK or
 * IOS_GAMEKIT_AUTH_FAILED, depending on whether a new player has signed
 * in.
 */
extern void ios_gamekit_acknowledge_auth_change(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_IOS_GAMEKIT_H
