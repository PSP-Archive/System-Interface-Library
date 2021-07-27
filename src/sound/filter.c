/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/filter.c: Audio data filtering interface.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/filter.h"

/*************************************************************************/
/************************** Interface functions **************************/
/*************************************************************************/

int sound_filter_filter(SoundFilterHandle *this, int16_t *pcm_buffer,
                        uint32_t pcm_len)
{
    PRECOND(this != NULL, return 0);
    return this->filter(this, pcm_buffer, pcm_len);
}

/*-----------------------------------------------------------------------*/

void sound_filter_close(SoundFilterHandle *this)
{
    PRECOND(this != NULL, return);
    this->close(this);
    mem_free(this);
}

/*************************************************************************/
/*************************************************************************/
