/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/filter.c: Tests for the audio filtering framework.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/filter.h"
#include "src/test/base.h"

/*************************************************************************/
/************************** Dummy filter module **************************/
/*************************************************************************/

/* Flag indicating whether filter_close() was called. */
static uint8_t filter_close_called;

/*-----------------------------------------------------------------------*/

static int filter_filter(UNUSED SoundFilterHandle *this, int16_t *pcm_buffer,
                         uint32_t pcm_len)
{
    for (uint32_t i = 0; i < pcm_len; i++) {
        pcm_buffer[i] = i;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static void filter_close(UNUSED SoundFilterHandle *this)
{
    filter_close_called = 1;
}

/*-----------------------------------------------------------------------*/

static SoundFilterHandle *filter_open(void)
{
    SoundFilterHandle *this;
    ASSERT(this = mem_alloc(sizeof(*this), 0, 0));
    this->filter = filter_filter;
    this->close  = filter_close;
    this->stereo = 0;
    this->freq   = 48000;
    return this;
}


/*************************************************************************/
/***************************** Test routine ******************************/
/*************************************************************************/

int test_sound_filter(void)
{
    SoundFilterHandle *filter;
    CHECK_TRUE(filter = filter_open());

    int16_t pcm[4];
    pcm[0] = pcm[1] = pcm[2] = pcm[3] = -1;
    CHECK_TRUE(sound_filter_filter(filter, pcm, lenof(pcm)));
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 1);
    CHECK_INTEQUAL(pcm[2], 2);
    CHECK_INTEQUAL(pcm[3], 3);

    filter_close_called = 0;
    sound_filter_close(filter);
    CHECK_TRUE(filter_close_called);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
