/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/wavegen.c: Simple wave generators for use in tests.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/sound/wavegen.h"

/*************************************************************************/
/*************************************************************************/

int square_gen(void *handle, void *pcm_buffer, int pcm_len)
{
    PRECOND(handle != NULL, return 0);
    PRECOND(pcm_buffer != NULL, return 0);
    SquareState *state = (SquareState *)handle;
    int16_t *out = (int16_t *)pcm_buffer;

    const int samples_to_generate = state->period * state->num_cycles;
    if (state->samples_out >= samples_to_generate) {
        return 0;
    }

    for (int i = 0; i < pcm_len; i++) {
        if (state->samples_out >= samples_to_generate) {
            mem_clear(&out[i], 2*(pcm_len - i));
            break;
        }
        if (state->samples_out % state->period < state->period/2) {
            out[i] = 10000;
        } else {
            out[i] = -10000;
        }
        state->samples_out++;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

int stereo_square_gen(void *handle, void *pcm_buffer, int pcm_len)
{
    PRECOND(handle != NULL, return 0);
    PRECOND(pcm_buffer != NULL, return 0);
    SquareState *state = (SquareState *)handle;
    int16_t *out = (int16_t *)pcm_buffer;

    const int samples_to_generate = state->period * state->num_cycles;
    if (state->samples_out >= samples_to_generate) {
        return 0;
    }

    for (int i = 0; i < pcm_len; i++) {
        if (state->samples_out >= samples_to_generate) {
            mem_clear(&out[i*2], 4*(pcm_len - i));
            break;
        }
        if (state->samples_out % state->period < state->period/2) {
            out[i*2+0] = 10000;
        } else {
            out[i*2+0] = -10000;
        }
        if (state->samples_out % (state->period*2) < state->period) {
            out[i*2+1] = 10000;
        } else {
            out[i*2+1] = -10000;
        }
        state->samples_out++;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

int sawtooth_gen(void *handle, void *pcm_buffer, int pcm_len)
{
    PRECOND(handle != NULL, return 0);
    PRECOND(pcm_buffer != NULL, return 0);
    int16_t *state = (int16_t *)handle;
    int16_t *out = (int16_t *)pcm_buffer;

    int16_t sample = *state;
    for (int i = 0; i < pcm_len; i++) {
        out[i] = sample++; // Assume two's-complement signed overflow behavior.
    }
    *state = sample;
    return 1;
}

/*-----------------------------------------------------------------------*/

int sawtooth_stereo_gen(void *handle, void *pcm_buffer, int pcm_len)
{
    PRECOND(handle != NULL, return 0);
    PRECOND(pcm_buffer != NULL, return 0);
    int16_t *state = (int16_t *)handle;
    int16_t *out = (int16_t *)pcm_buffer;

    int16_t sample = *state;
    for (int i = 0; i < pcm_len*2; i++) {
        out[i] = sample++;
    }
    *state = sample;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
