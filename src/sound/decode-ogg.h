/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/decode-ogg.h: Test control header for Ogg Vorbis decoding
 * routines.
 */

#ifndef SIL_SRC_SOUND_DECODE_OGG_H
#define SIL_SRC_SOUND_DECODE_OGG_H

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * sound_decode_ogg_test_fail_next_read:  Force the next read operation
 * requested by the Vorbis decoding library which would otherwise succeed
 * to fail.  Useful in testing error handling.
 */
extern void sound_decode_ogg_test_fail_next_read(void);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SOUND_DECODE_OGG_H
