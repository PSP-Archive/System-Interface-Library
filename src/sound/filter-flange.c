/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/filter-flange.c: Audio filter implementing a flanging effect.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/filter.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Private data structure. */

struct SoundFilterPrivate {
    /* Basic parameters. */
    uint32_t period;  // Flange period, in units of samples.
    uint32_t depth;   // Flange depth in samples, as a 16.16 fixed-point value.
    uint32_t phase;   // Current cosine phase (0...period-1).

    /* Delay buffer and current position.  The buffer is filled from the
     * end (highest index) moving forward (toward lower indices). */
    int16_t *buffer;  // Buffer of delayed audio samples.
    uint32_t buflen;  // Buffer size, in samples.
    uint32_t bufpos;  // Index into buffer[] at which to write the next sample.

    /* Lookup table for sample delays, of length period/256.  Each value
     * gives the time offset (number of samples, in 16.16 fixed point) at
     * which to pull the delayed sample for averaging with the incoming
     * sample. */
    uint32_t *delay_lut;
};

/*-----------------------------------------------------------------------*/

/* Method implementation declarations. */

static int filter_flange_filter(SoundFilterHandle *this,
                                int16_t *pcm_buffer, uint32_t pcm_len);
static void filter_flange_close(SoundFilterHandle *this);

/*************************************************************************/
/************************* Method implementation *************************/
/*************************************************************************/

SoundFilterHandle *sound_filter_open_flange(
    int stereo, uint32_t freq, float period, float depth)
{
    if (freq == 0 || period <= 0 || depth < 0) {
        DLOG("Invalid parameters: %d %u %g %g", stereo, freq, period, depth);
        goto error_return;
    }
    if (roundf(period * freq) >= 4294967296.0f) {
        DLOG("Period %g out of range (will cause integer overflow)", period);
        goto error_return;
    }
    if (roundf(depth * freq) >= 65536.0f) {
        DLOG("Depth %g out of range (will cause integer overflow)", depth);
        goto error_return;
    }

    SoundFilterHandle *this = mem_alloc(sizeof(*this), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!this)) {
        DLOG("No memory for SoundFilterHandle");
        goto error_return;
    }
    this->private = mem_alloc(sizeof(*this->private), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!this->private)) {
        DLOG("No memory for SoundFilterPrivate");
        goto error_free_this;
    }

    this->filter = filter_flange_filter;
    this->close  = filter_flange_close;
    this->stereo = stereo;
    this->freq   = freq;
    this->private->period = (uint32_t)roundf(period * freq);
    this->private->depth  = (uint32_t)roundf(depth * freq * 65536);
    this->private->phase  = 0;

    /* Allocate a buffer for the delayed PCM data.  We always use a
     * power-of-2 length for efficient wraparound. */
    int buffer_len = iceilf(depth * freq);
    int power_of_2 = 0;
    while (buffer_len > 1) {
        power_of_2++;
        buffer_len >>= 1;
    }
    this->private->buflen = 1 << (power_of_2 + 1);
    this->private->bufpos = this->private->buflen - 1;
    const unsigned int sample_size = stereo ? 4 : 2;
    this->private->buffer =
        mem_alloc(sample_size * this->private->buflen, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!this->private->buffer)) {
        DLOG("No memory for flange buffer (%u samples)",
             this->private->buflen);
        goto error_free_private;
    }
    /* Pre-clear these to avoid spurious Valgrind warnings (otherwise we'd
     * compute undefined*0 on the first sample). */
    this->private->buffer[0] = 0;
    this->private->buffer[1] = 0;

    /* Set up the delay lookup table.  To reduce memory use, we record
     * delay values at 256-sample intervals and linearly interpolate
     * between them as necessary. */
    const uint32_t delay_lut_len = (this->private->period + 255) / 256 + 1;
    this->private->delay_lut = mem_alloc(4 * delay_lut_len, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!this->private->delay_lut)) {
        DLOG("No memory for flange lookup table (%u entries)", delay_lut_len);
        goto error_free_buffer;
    }
    uint32_t * const delay_lut   = this->private->delay_lut;
    const uint32_t period_scaled = this->private->period;
    const uint32_t depth_scaled  = this->private->depth;
    for (uint32_t t = 0; t < delay_lut_len; t++) {
        const float k = (1 - cosf(2*M_PIf*(t*256)/(float)period_scaled)) / 2;
        delay_lut[t] = (uint32_t)roundf(depth_scaled * k);
    }

    return this;

  error_free_buffer:
    mem_free(this->private->buffer);
  error_free_private:
    mem_free(this->private);
  error_free_this:
    mem_free(this);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

static int filter_flange_filter(SoundFilterHandle *this, int16_t *pcm_buffer,
                                uint32_t pcm_len)
{
    /* Load parameters into local variables to reduce memory accesses. */
    const int       stereo           = this->stereo;
    const uint32_t  period           = this->private->period;
          uint32_t  phase            = this->private->phase;
    int16_t * const flange_buffer    = this->private->buffer;
    const uint32_t  buflen_mask      = this->private->buflen - 1;
          uint32_t  bufpos           = this->private->bufpos;
    const uint32_t * const delay_lut = this->private->delay_lut;

    for (uint32_t i = 0; i < pcm_len; i++) {
        /* Calculate the delay for this sample. */
        const uint32_t offset_phase0 = delay_lut[phase/256 + 0];
        const uint32_t offset_phase1 = delay_lut[phase/256 + 1];
        const uint32_t offset_weight1 = phase%256;
        const uint32_t offset_weight0 = 256 - offset_weight1;
        const uint32_t offset =
            (uint32_t)(((uint64_t)offset_phase0 * offset_weight0
                        + (uint64_t)offset_phase1 * offset_weight1
                        + 0x80) >> 8);

        if (!stereo) {

            /* Load the next input sample and store it in the delay buffer.
             * To avoid overflow when computing the output sample, we load
             * into an int32 instead of an int16. */
            const int32_t input_sample = pcm_buffer[i];
            flange_buffer[bufpos] = input_sample;

            /* Load the delayed sample to average with the input sample. */
            const uint32_t delaypos_0 = (bufpos + (offset>>16)) & buflen_mask;
            const uint32_t delaypos_1 = (delaypos_0 + 1) & buflen_mask;
            const int32_t weight_1 = offset & 0xFFFF;
            const int32_t weight_0 = 0x10000 - weight_1;
            const int32_t delayed_sample =
                (flange_buffer[delaypos_0]*weight_0
               + flange_buffer[delaypos_1]*weight_1 + 0x8000) >> 16;

            /* Compute the output sample and store it back to the PCM
             * buffer.  We use a fixed dry:wet ratio of 9:7. */
            pcm_buffer[i] = (input_sample*9 + delayed_sample*7 + 8) >> 4;

        } else {  // Stereo stream.

            const int32_t input_sample_l = pcm_buffer[i*2+0];
            const int32_t input_sample_r = pcm_buffer[i*2+1];
            flange_buffer[bufpos*2+0] = input_sample_l;
            flange_buffer[bufpos*2+1] = input_sample_r;

            const uint32_t delaypos_0 = (bufpos + (offset>>16)) & buflen_mask;
            const uint32_t delaypos_1 = (delaypos_0 + 1) & buflen_mask;
            const int32_t weight_1 = offset & 0xFFFF;
            const int32_t weight_0 = 0x10000 - weight_1;
            const int32_t delayed_sample_l =
                (flange_buffer[delaypos_0*2+0]*weight_0
                 + flange_buffer[delaypos_1*2+0]*weight_1 + 0x8000) >> 16;
            const int32_t delayed_sample_r =
                (flange_buffer[delaypos_0*2+1]*weight_0
                 + flange_buffer[delaypos_1*2+1]*weight_1 + 0x8000) >> 16;

            pcm_buffer[i*2+0] = (input_sample_l*9 + delayed_sample_l*7
                                 + 8) >> 4;
            pcm_buffer[i*2+1] = (input_sample_r*9 + delayed_sample_r*7
                                 + 8) >> 4;

        }

        /* Advance counters. */
        phase++;
        if (phase >= period) {
            phase = 0;
        }
        bufpos = (bufpos - 1) & buflen_mask;
    }

    /* Store modified fields back to the instance data. */
    this->private->phase = phase;
    this->private->bufpos = bufpos;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void filter_flange_close(SoundFilterHandle *this)
{
    mem_free(this->private->delay_lut);
    mem_free(this->private->buffer);
    mem_free(this->private);
}

/*************************************************************************/
/*************************************************************************/
