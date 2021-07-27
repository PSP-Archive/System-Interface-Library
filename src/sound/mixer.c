/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/mixer.c: Software mixer implementation.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/sound.h"
#include "src/sound/mixer.h"
#include "src/time.h"  // For MIX_TIMING.

#ifdef SIL_ARCH_ARM_NEON
# include <arm_neon.h>
#endif

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * MIX_TIMING:  Define to report the audio processing speed via DLOG().
 */
// #define MIX_TIMING

/**
 * MIX_ACCUM_BUFLEN:  The size of the sample accumulation buffer used by
 * mix().  Consequently, this is the largest number of samples that can be
 * processed at once in sound_mixer_get_pcm(); larger buffer sizes will
 * require multiple calls to mix().
 */
#define MIX_ACCUM_BUFLEN  1024

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we been initialized? */
static int mixer_initted;

/* Total number of channels allocated. */
static int num_channels;

/* Output sampling rate. */
static int mix_rate;

/* Base volume multiplier. */
static float base_volume;

/*-----------------------------------------------------------------------*/

/* Mutual exclusion semaphore for [un]lock_mixer(). */
static int mixer_mutex;

/* Data for each sound channel. */
typedef struct MixerChannelInfo MixerChannelInfo;
struct MixerChannelInfo {
    SoundDecodeCallback decode_func;
    void *decode_handle;
    uint8_t stereo;        // True = stereo, false = monaural
    uint8_t playing;       // Flag: Are we playing?
    uint8_t fade_cut;      // True = stop playback when fade volume hits zero
    int32_t volume;        // sound_mixer_setvol(1.0) ⇒ 1 << VOLUME_BITS
    int32_t fade_rate;     // Amount to add to volume per sample played
    int32_t fade_target;   // Fade target volume (same units as .volume)
    int32_t pan;           // sound_mixer_setpan(-1.0) ⇒ 0
                           // sound_mixer_setpan(+1.0) ⇒ 1 << PAN_BITS
    int16_t stereo_pan_l;  // Left channel mult. for stereo pan (0...PAN_MULT)
    int16_t stereo_pan_r;  // Right channel mult. for stereo pan (0...PAN_MULT)
    int16_t *pcm_buffer;   // PCM data buffer
};
#define VOLUME_BITS  24
#define PAN_BITS     8
#define VOLUME_MULT  (1 << VOLUME_BITS)
#define PAN_MULT     (1 << PAN_BITS)

static MixerChannelInfo *mixer_channels;

/* Copy of mixer_channels[] used by mix() to avoid holding the lock for
 * long periods of time. */
static MixerChannelInfo *copy_channels;

/* Maximum volume (to avoid integer overflow). */
#define VOLUME_MAX   (0x7FFFFFFF >> VOLUME_BITS)

/* Shift amounts used in the mixer routine to obtain the final sample value
 * after multiplication by volume and pan factors. */

#define MONO_SHIFT  (VOLUME_BITS + (PAN_BITS-1))
#define STEREO_SHIFT  (VOLUME_BITS + PAN_BITS)

/*************************************************************************/

/* Local routine declarations. */

/**
 * lock_mixer:  Lock the sound system against state changes by other
 * threads.  Lock calls may _not_ be nested.
 */
static void lock_mixer(void);

/**
 * unlock_mixer:  Unlock the sound system after lock_mixer().
 */
static void unlock_mixer(void);

/**
 * mix:  Mix samples for all active channels into the given buffer.
 *
 * [Parameters]
 *     buffer: Buffer into which to mix.
 *     samples: Number of samples to mix.
 */
static void mix(void *buffer, int samples);

/*************************************************************************/
/***************** Interface: General control functions ******************/
/*************************************************************************/

int sound_mixer_init(int num_channels_, int mix_rate_)
{
    if (UNLIKELY(num_channels_ <= 0) || UNLIKELY(mix_rate_ <= 0)) {
        DLOG("Invalid parameters: %d %d", num_channels_, mix_rate_);
        goto error_return;
    }
    if (mixer_initted) {
        DLOG("Tried to init twice!");
        goto error_return;
    }

    num_channels = num_channels_;
    mix_rate = mix_rate_;
    base_volume = 1.0;

    mixer_channels = mem_alloc(num_channels * sizeof(*mixer_channels), 0, 0);
    if (UNLIKELY(!mixer_channels)) {
        DLOG("Failed to allocate %d channels", num_channels);
        goto error_return;
    }

    copy_channels = mem_alloc(num_channels * sizeof(*mixer_channels), 0, 0);
    if (UNLIKELY(!copy_channels)) {
        DLOG("Failed to allocate %d channels", num_channels);
        goto error_free_mixer_channels;
    }

    mixer_mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (UNLIKELY(!mixer_mutex)) {
        DLOG("Failed to create sound mutex");
        goto error_free_copy_channels;
    }

    int size = num_channels * (MIX_ACCUM_BUFLEN*4);
#ifdef SIL_ARCH_MIPS_32
    size += 4;  // Ensure mix() can overrun by 1 sample (see asm comments).
#endif
    int16_t *pcm_buffer = mem_alloc(size, 0, 0);
    if (UNLIKELY(!pcm_buffer)) {
        DLOG("Failed to allocate source PCM buffer");
        goto error_destroy_mixer_mutex;
    }

    for (int i = 0; i < num_channels; i++, pcm_buffer += MIX_ACCUM_BUFLEN*2) {
        mixer_channels[i].decode_func   = NULL;
        mixer_channels[i].decode_handle = NULL;
        mixer_channels[i].stereo        = 0;
        mixer_channels[i].playing       = 0;
        mixer_channels[i].fade_cut      = 0;
        mixer_channels[i].volume        = 1 << VOLUME_BITS;
        mixer_channels[i].fade_rate     = 0;
        mixer_channels[i].fade_target   = 0;
        mixer_channels[i].pan           = PAN_MULT/2;
        mixer_channels[i].stereo_pan_l  = PAN_MULT;
        mixer_channels[i].stereo_pan_r  = PAN_MULT;
        mixer_channels[i].pcm_buffer    = pcm_buffer;
    }
    mixer_initted = 1;
    return 1;

  error_destroy_mixer_mutex:
    mutex_destroy(mixer_mutex);
    mixer_mutex = 0;
  error_free_copy_channels:
    mem_free(copy_channels);
    copy_channels = NULL;
  error_free_mixer_channels:
    mem_free(mixer_channels);
    mixer_channels = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sound_mixer_set_base_volume(float volume)
{
    base_volume = bound(volume, 0, 15);
}

/*-----------------------------------------------------------------------*/

void sound_mixer_get_pcm(void *buffer, int samples)
{
    if (UNLIKELY(!buffer) || UNLIKELY(samples < 0)) {
        DLOG("Invalid parameters: %p %d", buffer, samples);
        return;
    }

    if (UNLIKELY(!mixer_initted)) {
        mem_clear(buffer, samples*4);
        return;
    }

    uint8_t *buffer8 = (uint8_t *)buffer;
    while (samples > 0) {
        const int tomix = ubound(samples, MIX_ACCUM_BUFLEN);
        mix(buffer8, tomix);
        buffer8 += tomix*4;
        samples -= tomix;
    }
}

/*-----------------------------------------------------------------------*/

void sound_mixer_cleanup(void)
{
    if (!mixer_initted) {
        return;
    }

    for (int i = 1; i <= num_channels; i++) {
        sound_mixer_reset(i);
    }

    mutex_destroy(mixer_mutex);
    mixer_mutex = 0;

    mem_free(mixer_channels[0].pcm_buffer);
    mem_free(mixer_channels);
    mem_free(copy_channels);
    mixer_channels = copy_channels = NULL;

    mixer_initted = 0;
}

/*************************************************************************/
/*********** Interface: Sound playback/manipulation functions ************/
/*************************************************************************/

int sound_mixer_setdata(int channel, SoundDecodeCallback decode_func,
                        void *handle, int is_stereo)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)
     || UNLIKELY(!decode_func)) {
        DLOG("Invalid parameters: %d %p %p %d", channel, decode_func,
             handle, is_stereo);
        return 0;
    }
    const int index = channel - 1;
    if (mixer_channels[index].decode_func) {
        DLOG("Channel %d is already in use", channel);
        return 0;
    }

    mixer_channels[index].decode_func = decode_func;
    mixer_channels[index].decode_handle = handle;
    mixer_channels[index].stereo = is_stereo ? 1 : 0;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sound_mixer_setvol(int channel, float volume)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d %g", channel, volume);
        return;
    }
    const int index = channel - 1;

    lock_mixer();
    if (volume < 0) {
        mixer_channels[index].volume = 0;
    } else if (volume >= VOLUME_MAX) {
        mixer_channels[index].volume = iroundf(VOLUME_MAX * VOLUME_MULT);
    } else {
        mixer_channels[index].volume = iroundf(volume * VOLUME_MULT);
    }
    mixer_channels[index].fade_rate = 0;
    mixer_channels[index].fade_cut = 0;
    unlock_mixer();
}

/*-----------------------------------------------------------------------*/

void sound_mixer_setpan(int channel, float pan)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d %g", channel, pan);
        return;
    }
    const int index = channel - 1;

    lock_mixer();
    if (pan < -1) {
        mixer_channels[index].pan = 0;
        mixer_channels[index].stereo_pan_l = PAN_MULT;
        mixer_channels[index].stereo_pan_r = 0;
    } else if (pan > 1) {
        mixer_channels[index].pan = PAN_MULT;
        mixer_channels[index].stereo_pan_l = 0;
        mixer_channels[index].stereo_pan_r = PAN_MULT;
    } else {
        mixer_channels[index].pan = iroundf(((pan+1)/2) * PAN_MULT);
        const float pan_l = 1 - pan;
        const float pan_r = 1 + pan;
        if (pan_l < pan_r) {
            mixer_channels[index].stereo_pan_l =
                (int16_t)iroundf((pan_l/pan_r) * PAN_MULT);
            mixer_channels[index].stereo_pan_r = PAN_MULT;
        } else {
            mixer_channels[index].stereo_pan_l = PAN_MULT;
            mixer_channels[index].stereo_pan_r =
                (int16_t)iroundf((pan_r/pan_l) * PAN_MULT);
        }
    }
    unlock_mixer();
}

/*-----------------------------------------------------------------------*/

void sound_mixer_setfade(int channel, float target, float length, int cut)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)
     || UNLIKELY(length < 0)) {
        DLOG("Invalid parameters: %d %g %g", channel, target, length);
        return;
    }
    const int index = channel - 1;
    if (!mixer_channels[index].decode_func) {
        DLOG("Channel %d has no data", channel);
        return;
    }

    lock_mixer();
    if (length == 0) {
        mixer_channels[index].fade_rate = 0;
        mixer_channels[index].fade_cut = 0;
    } else {
        target = bound(target, 0, VOLUME_MAX);
        const float delta_volume =
            target - ((float)mixer_channels[index].volume / VOLUME_MULT);
        const float samples = lbound(roundf(length * mix_rate), 1);
        mixer_channels[index].fade_rate =
            iroundf((delta_volume / samples) * VOLUME_MULT);
        mixer_channels[index].fade_target = iroundf(target * VOLUME_MULT);
        mixer_channels[index].fade_cut = (cut != 0);
    }
    unlock_mixer();
}

/*-----------------------------------------------------------------------*/

void sound_mixer_start(int channel)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    const int index = channel - 1;

    lock_mixer();
    if (mixer_channels[index].decode_func != NULL) {
        mixer_channels[index].playing = 1;
    }
    unlock_mixer();
}

/*-----------------------------------------------------------------------*/

void sound_mixer_stop(int channel)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    const int index = channel - 1;

    lock_mixer();
    mixer_channels[index].playing = 0;
    unlock_mixer();
}

/*-----------------------------------------------------------------------*/

void sound_mixer_reset(int channel)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    const int index = channel - 1;

    if (mixer_channels[index].decode_func) {
        lock_mixer();
        mixer_channels[index].playing       = 0;
        mixer_channels[index].decode_func   = NULL;
        mixer_channels[index].decode_handle = NULL;
        mixer_channels[index].stereo        = 0;
        mixer_channels[index].fade_cut      = 0;
        mixer_channels[index].volume        = VOLUME_MULT;
        mixer_channels[index].fade_rate     = 0;
        mixer_channels[index].pan           = PAN_MULT/2;
        mixer_channels[index].stereo_pan_l  = PAN_MULT;
        mixer_channels[index].stereo_pan_r  = PAN_MULT;
        unlock_mixer();
    }
}

/*-----------------------------------------------------------------------*/

int sound_mixer_status(int channel)
{
    if (UNLIKELY(channel < 1) || UNLIKELY(channel > num_channels)) {
        DLOG("Invalid parameters: %d", channel);
        return 0;
    }
    const int index = channel - 1;

    return mixer_channels[index].playing;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void lock_mixer(void)
{
    mutex_lock(mixer_mutex);
}

/*-----------------------------------------------------------------------*/

static void unlock_mixer(void)
{
    mutex_unlock(mixer_mutex);
}

/*-----------------------------------------------------------------------*/

#if defined(SIL_ARCH_ARM_32) && defined(__GNUC__) && !defined(__clang__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
/* This is needed to avoid running out of registers when building with -O0.
 * Since the heavy code is all in assembly, the reduction in optimization
 * level for optimized builds should be insignificant. */
__attribute__((optimize(1)))
#endif
static void mix(void *buffer, int samples)
{
    PRECOND(buffer != NULL, return);
    PRECOND(samples <= MIX_ACCUM_BUFLEN, return);

    /* Intermediate buffer for accumulating samples in 32 bits, so we can
     * clamp to [-0x8000,0x7FFF] instead of overflowing. */
    static int32_t accum_buffer[MIX_ACCUM_BUFLEN*2];

#ifdef MIX_TIMING
    static double timing_lock = 0;    // Total time spent waiting for the lock.
    static double timing_lock_max = 0;// Max time spent waiting for the lock.
    static double timing_decode = 0;  // Time spent decoding audio.
    static double timing_render = 0;  // Time spent rendering audio.
    static int timing_samples = 0;    // Total number of samples rendered.
    static int timing_channels = 0;   // Sum of channels active on each call.
    static int timing_count = 0;      // Number of calls to this function.
    const double time_lock = time_now();
#endif

    lock_mixer();
#ifdef MIX_TIMING
    const double time_decode = time_now();
#endif
    for (int ch = 0; ch < num_channels; ch++) {
        if (mixer_channels[ch].playing) {
            if (mixer_channels[ch].fade_rate) {
                const int32_t samples_left =
                    (mixer_channels[ch].fade_target - mixer_channels[ch].volume)
                    / mixer_channels[ch].fade_rate;
                if (samples >= samples_left) {
                    mixer_channels[ch].volume = mixer_channels[ch].fade_target;
                    mixer_channels[ch].fade_rate = 0;
                } else {
                    mixer_channels[ch].volume +=
                        mixer_channels[ch].fade_rate * samples;
                }
            }
            if (mixer_channels[ch].volume==0 && mixer_channels[ch].fade_cut) {
                mixer_channels[ch].playing = 0;
                continue;
            }
            const int got_data = (*mixer_channels[ch].decode_func)(
                mixer_channels[ch].decode_handle,
                mixer_channels[ch].pcm_buffer, samples);
            if (!got_data) {
                mixer_channels[ch].playing = 0;
            }
        }
    }
    for (int ch = 0; ch < num_channels; ch++) {
        if (mixer_channels[ch].playing) {
            copy_channels[ch] = mixer_channels[ch];
        } else {
            copy_channels[ch].playing = 0;
        }
    }
    unlock_mixer();

#ifdef MIX_TIMING
    const double time_render = time_now();
#endif

    mem_clear(accum_buffer, samples*8);

#if defined(SIL_ARCH_ARM_NEON) && defined(__GNUC__)

    MixerChannelInfo *channel = copy_channels;
    const unsigned long base_volume_scaled = iroundf(base_volume * (1<<27));
    for (int i = 0; i < num_channels; i++, channel++) {
        /* Skip out early if this channel isn't playing or is silent. */
        if (!channel->playing || channel->volume == 0) {
            continue;
        }
        const int64_t volume =
            ((uint64_t)channel->volume * base_volume_scaled) >> 27;
        const int16_t *src = channel->pcm_buffer;
        int32_t *dest = accum_buffer;
        int32_t * const dest_limit = dest + samples*2;

        if (channel->stereo) {
            const long volume_l =
                (long)((volume * channel->stereo_pan_l) >> PAN_BITS);
            const long volume_r =
                (long)((volume * channel->stereo_pan_r) >> PAN_BITS);
            /* This needs to be a literal constant for vrsraq below. */
            #define stereo_shift  (STEREO_SHIFT - PAN_BITS - 16)
            const int stereo_round = 1 << (stereo_shift - 1);
            /* If we use the obvious initializer here, Clang goes through
             * ridiculous contortions involving neg and mvn just to get a
             * loop counter it can increment instead of decrement.  The sub
             * instruction is presumably no slower than the add instruction
             * and certainly faster for <=3 loops than adding several extra
             * initialization instructions, so we hack around to force
             * Clang to Do The Right Thing. */
            //for (int j = samples & 3; j != 0; j--) {
            for (long j = samples & 3; j != 0;
                 __extension__({__asm__("sub %0, %0, #1"
                                        : "=r" (j) : "0" (j));}))
            {
                /* Clang wants to use multiple registers and recompute
                 * the value of src and dest on each iteration(!), so we
                 * explicitly use the more logical (and faster)
                 * post-indexed addressing mode. */
                //int32_t left = *src++;
                //int32_t right = *src++;
                long left, right;
                __asm__("ldrsh %0, [%1], #4"
                        : "=r" (left), "=r" (src) : "1" (src));
                __asm__("ldrsh %0, [%1, #-2]" : "=r" (right) : "r" (src));
#ifdef SIL_ARCH_ARM_64
                register long acc_left __asm__("x0") = dest[0];
                register long acc_right __asm__("x1") = dest[1];
#else
                long acc_left = dest[0];
                long acc_right = dest[1];
#endif
                left = (left * (int64_t)volume_l) >> 16;
                right = (right * (int64_t)volume_r) >> 16;
                acc_left += ((left + stereo_round) >> stereo_shift);
                acc_right += ((right + stereo_round) >> stereo_shift);
                //*dest++ = acc_left;
                //*dest++ = acc_right;
#ifdef SIL_ARCH_ARM_64
                __asm__("stp w0, w1, [%0], #8"
                        : "=r" (dest)
                        : "r" (acc_left), "r" (acc_right), "0" (dest));
#else
                __asm__("str %1, [%0], #8"
                        : "=r" (dest) : "r" (acc_left), "0" (dest));
                __asm__("str %1, [%0, #-4]"
                        : "=r" (dest) : "r" (acc_right), "0" (dest));
#endif
            }

            const int32x4_t volume_l_vec = vdupq_n_s32(volume_l);
            const int32x4_t volume_r_vec = vdupq_n_s32(volume_r);
            while (dest < dest_limit) {
#ifndef SIL_ARCH_ARM_64
                const int16x4x2_t sample_16 = vld2_s16(src);
                src += 8;
#else
                /* Clang doesn't seem to know about post-indexed addressing
                 * mode for {ld,st}[1234], so we need to help it out. */
                int16x4x2_t sample_16;
                __asm__("ld2 {%0.4h, %1.4h}, [%2], #16"
                        : "=w" (sample_16.val[0]), "=w" (sample_16.val[1]),
                          "=r" (src)
                        : "2" (src), "m" (*src));
#endif
                int32x4x2_t accum = vld2q_s32(dest);
                int32x4_t left = vshll_n_s16(sample_16.val[0], 15);
                int32x4_t right = vshll_n_s16(sample_16.val[1], 15);
                left = vqdmulhq_s32(left, volume_l_vec);
                right = vqdmulhq_s32(right, volume_r_vec);
                accum.val[0] =
                    vrsraq_n_s32(accum.val[0], left, stereo_shift);
                accum.val[1] =
                    vrsraq_n_s32(accum.val[1], right, stereo_shift);
#ifndef SIL_ARCH_ARM_64
                vst2q_s32(dest, accum);
                dest += 8;
#else
                __asm__("st2 {%2.4s, %3.4s}, [%0], #32"
                        : "=r" (dest), "=m" (*dest)
                        : "w" (accum.val[0]), "w" (accum.val[1]), "0" (dest));
#endif
            }
            #undef stereo_shift

        } else {  // !channel->stereo
            const long volume_l =
                (long)((volume * (PAN_MULT - channel->pan)) >> PAN_BITS);
            const long volume_r =
                (long)((volume * channel->pan) >> PAN_BITS);
            #define mono_shift  (MONO_SHIFT - PAN_BITS - 16)
            const int mono_round = 1 << (mono_shift - 1);
            /* See notes above for why all the inline assembly. */
            //for (int j = samples & 3; j != 0; j--) {
            for (long j = samples & 3; j != 0;
                 __extension__({__asm__("sub %0, %0, #1"
                                        : "=r" (j) : "0" (j));}))
            {
                //int16_t sample = *src++;
                long sample;
                __asm__("ldrsh %0, [%1], #2"
                        : "=r" (sample), "=r" (src) : "1" (src));
#ifdef SIL_ARCH_ARM_64
                register long acc_left __asm__("x0") = dest[0];
                register long acc_right __asm__("x1") = dest[1];
#else
                long acc_left = dest[0];
                long acc_right = dest[1];
#endif
                int32_t left = (sample * (int64_t)volume_l) >> 16;
                int32_t right = (sample * (int64_t)volume_r) >> 16;
                acc_left += ((left + mono_round) >> mono_shift);
                acc_right += ((right + mono_round) >> mono_shift);
                //*dest++ = acc_left;
                //*dest++ = acc_right;
#ifdef SIL_ARCH_ARM_64
                __asm__("stp w0, w1, [%0], #8"
                        : "=r" (dest)
                        : "r" (acc_left), "r" (acc_right), "0" (dest));
#else
                __asm__("str %1, [%0], #8"
                        : "=r" (dest) : "r" (acc_left), "0" (dest));
                __asm__("str %1, [%0, #-4]"
                        : "=r" (dest) : "r" (acc_right), "0" (dest));
#endif
            }

            const int32x4_t volume_l_vec = vdupq_n_s32(volume_l);
            const int32x4_t volume_r_vec = vdupq_n_s32(volume_r);
            while (dest < dest_limit) {
                /* This gets converted to an ldr instruction, with which
                 * Clang does know to use post-indexed addressing. */
                const int16x4_t sample_16 = vld1_s16(src);
                src += 4;
                int32x4x2_t accum = vld2q_s32(dest);
                const int32x4_t sample_32 = vshll_n_s16(sample_16, 15);
                const int32x4_t left = vqdmulhq_s32(sample_32, volume_l_vec);
                const int32x4_t right = vqdmulhq_s32(sample_32, volume_r_vec);
                accum.val[0] = vrsraq_n_s32(accum.val[0], left, mono_shift);
                accum.val[1] = vrsraq_n_s32(accum.val[1], right, mono_shift);
#ifndef SIL_ARCH_ARM_64
                vst2q_s32(dest, accum);
                dest += 8;
#else
                __asm__("st2 {%2.4s, %3.4s}, [%0], #32"
                        : "=r" (dest), "=m" (*dest)
                        : "w" (accum.val[0]), "w" (accum.val[1]), "0" (dest));
#endif
            }
            #undef mono_shift
        }
    }

    int32_t *src = accum_buffer;
    int16_t *dest = buffer;
    int16_t * const dest_limit = dest + samples*2;
    if (samples & 1) {
#ifdef SIL_ARCH_ARM_64
        __asm__("ldr d0, [%[src]], #8 \n"
                "sqxtn v0.4h, v0.4s   \n"
                "str s0, [%[dest]], #4\n"
                : [src] "=r" (src), [dest] "=r" (dest),
                  "=m" (*dest)
                : "0" (src), "1" (dest), "m" (*src)
                : "v0");
#else
        __asm__("ldmia %[src]!, {r0-r1}   \n"
                "ssat r0, #16, r0         \n"
                "ssat r1, #16, r1         \n"
                "pkhbt r0, r0, r1, lsl #16\n"
                "str r0, [%[dest]], #4      \n"
                : [src] "=r" (src), [dest] "=r" (dest), "=m" (*dest)
                : "0" (src), "1" (dest), "m" (*src)
                : "r0", "r1");
#endif
    }
    for (int count = samples & 6; count != 0; count -= 2) {
        register int32x4_t in = vld1q_s32(src);
        src += 4;
        register int16x4_t out = vqmovn_s32(in);
        vst1_s16(dest, out);
        dest += 4;
    }
    while (dest < dest_limit) {
        register int32x4_t in0 = vld1q_s32(src);
        register int32x4_t in1 = vld1q_s32(src+4);
        register int32x4_t in2 = vld1q_s32(src+8);
        register int32x4_t in3 = vld1q_s32(src+12);
        src += 16;
#ifdef SIL_ARCH_ARM_64
        register int16x8_t out0 = vqmovn_high_s32(vqmovn_s32(in0), in1);
        register int16x8_t out1 = vqmovn_high_s32(vqmovn_s32(in2), in3);
        vst1q_s16(dest, out0);
        vst1q_s16(dest+8, out1);
#else
        register int16x4_t out0 = vqmovn_s32(in0);
        register int16x4_t out1 = vqmovn_s32(in1);
        register int16x4_t out2 = vqmovn_s32(in2);
        register int16x4_t out3 = vqmovn_s32(in3);
        vst1_s16(dest, out0);
        vst1_s16(dest+4, out1);
        vst1_s16(dest+8, out2);
        vst1_s16(dest+12, out3);
#endif
        dest += 16;
    }

#elif defined(SIL_ARCH_ARM_32) && defined(__GNUC__)

    /* We modify some of the input registers, so we need to assign outputs
     * to those registers to ensure that the compiler doesn't try to reuse
     * the values from those registers (which it thinks are unmodified) or
     * put the values in registers that we clobber (since GCC-style asm()
     * assumes that all inputs are consumed before any clobbers occur). */
    void * volatile buf_dummy;
    void * volatile buf_limit_dummy;
    void * volatile channel_dummy;
    volatile int i_dummy;
    __asm__(
        "@ Register usage is as follows:                                \n"
        "@     r0: Temporary; left output sample                        \n"
        "@     r1: Temporary; right output sample                       \n"
        "@     r2: Sum of left samples                                  \n"
        "@     r3: Sum of right samples                                 \n"
        "@     r4: (Mono)   Left sample multiplier = volume * (1-pan)   \n"
        "@         (Stereo) Left sample multiplier = volume * pan_l     \n"
        "@     r5: (Mono)   Right sample multiplier = volume * pan      \n"
        "@         (Stereo) Right sample multiplier = volume * pan_r    \n"
        "@     r6: Input sample                                         \n"
        "@     r7: Unused (compiler might use r7 as a frame pointer)    \n"
        "@     r8: Channel PCM data pointer                             \n"
        "@ Additionally, the following registers are allocated by the   \n"
        "@ compiler:                                                    \n"
        "@          %[buf]: Output buffer pointer                       \n"
        "@    %[buf_limit]: Output buffer limit pointer                 \n"
        "@      %[channel]: Pointer to current channel's MixerChannelInfo\n"
        "@            %[i]: Channel loop counter (counts down to 0)     \n"
        "                                                               \n"
        "@ Save the base output pointer on the stack.                   \n"
        "push {%[buf]}                                                  \n"
        "                                                               \n"
        "@@@@@@@@ Process each channel in sequence. @@@@@@@@            \n"
        "                                                               \n"
        "1: @chanloop                                                   \n"
        "@ (Hmm, using an actual label seems to cause the iOS linker to \n"
        "@ randomly rearrange pieces of the routine...)                 \n"
        "                                                               \n"
        "@ Increment the channel pointer and index, and check whether   \n"
        "@ we've reached the end of the array.                          \n"
        "add %[channel], %[channel], %[sizeof_ch]                       \n"
        "subs %[i], %[i], #1                                            \n"
        "beq 9f @chanloop_end                                           \n"
        "@ Skip out early if this channel isn't playing or is silent.   \n"
        "ldrb r0, [%[channel], %[playing]]                              \n"
        "ldr r1, [%[channel], %[volume]]                                \n"
        "teq r0, #0                                                     \n"
        "beq 1b @chanloop                                               \n"
        "teq r1, #0                                                     \n"
        "beq 1b @chanloop                                               \n"
        "umull r0, r1, r1, %[base_volume_scaled]                        \n"
        "                                                               \n"
        "@@@@@@@@ Setup for monaural channels. @@@@@@@@                 \n"
        "                                                               \n"
        "@ Load this channel's data.                                    \n"
        "ldrb r0, [%[channel], %[stereo]]                               \n"
        "ldr %[buf], [sp]                                               \n"
        "ldr r8, [%[channel], %[pcm_buffer]]                            \n"
        "teq r0, #0                                                     \n"
        "bne 5f @stereo_setup                                           \n"
        "ldr r5, [%[channel], %[pan]]                                   \n"
        "@ The -5 here is because multiplying by base_volume_scaled     \n"
        "@ already cut 5 bits from the value.                           \n"
        "asr r1, r1, %[pan_bits] - 5                                    \n"
        "rsb r4, r5, %[pan_mult]                                        \n"
        "mul r5, r5, r1                                                 \n"
        "mul r4, r4, r1                                                 \n"
        "                                                               \n"
        "@@@@@@@@ Playback loop for monaural channels. @@@@@@@@         \n"
        "                                                               \n"
        "2: @monoloop                                                   \n"
        "@ Load the sample from the channel's input buffer.             \n"
        "ldrsh r6, [r8], #2                                             \n"
        "lsl r6, r6, #16                                                \n"
        "                                                               \n"
        "@ Add the sample to the left and right accumulated values.     \n"
        "smmul r0, r6, r4       @ Left output sample                    \n"
        "ldmia %[buf], {r2,r3}  @ Accumulated sample values             \n"
        "smmul r1, r6, r5       @ Right output sample                   \n"
        "add r0, r0, %[mono_round]                                      \n"
        "add r1, r1, %[mono_round]                                      \n"
        "add r2, r2, r0, asr %[mono_shift]                              \n"
        "add r3, r3, r1, asr %[mono_shift]                              \n"
        "stmia %[buf]!, {r2,r3}                                         \n"
        "                                                               \n"
        "@ Process the next sample (if any are left).                   \n"
        "cmp %[buf], %[buf_limit]                                       \n"
        "bne 2b @monoloop                                               \n"
        "                                                               \n"
        "@ Continue with the next channel.                              \n"
        "b 1b @chanloop                                                 \n"
        "                                                               \n"
        "@@@@@@@@ Setup for stereo channels. @@@@@@@@                   \n"
        "                                                               \n"
        "5: @stereo_setup                                               \n"
        "asr r1, r1, %[pan_bits] - 5  @ r1 holds channel volume from above.\n"
        "ldrsh r4, [%[channel], %[stereo_pan_l]]                        \n"
        "ldrsh r5, [%[channel], %[stereo_pan_r]]                        \n"
        "mul r4, r4, r1                                                 \n"
        "mul r5, r5, r1                                                 \n"
        "                                                               \n"
        "@@@@@@@@ Playback loop for stereo channels. @@@@@@@@           \n"
        "                                                               \n"
        "6: @stereoloop                                                 \n"
        "ldrsh r6, [r8], #4                                             \n"
        "lsl r6, r6, #16                                                \n"
        "smmul r0, r4, r6                                               \n"
        "ldrsh r6, [r8, #-2]                                            \n"
        "ldmia %[buf], {r2,r3}                                          \n"
        "lsl r6, r6, #16                                                \n"
        "smmul r1, r5, r6                                               \n"
        "add r0, r0, %[stereo_round]                                    \n"
        "add r1, r1, %[stereo_round]                                    \n"
        "add r2, r2, r0, asr %[stereo_shift]                            \n"
        "add r3, r3, r1, asr %[stereo_shift]                            \n"
        "stmia %[buf]!, {r2,r3}                                         \n"
        "                                                               \n"
        "cmp %[buf], %[buf_limit]                                       \n"
        "bne 6b @stereoloop                                             \n"
        "b 1b @chanloop                                                 \n"
        "                                                               \n"
        "@@@@@@@@ End of loops. @@@@@@@@                                \n"
        "                                                               \n"
        "9: @chanloop_end                                               \n"
        "pop {%[buf]}                                                   \n"
        : "=r" (buf_dummy), "=r" (buf_limit_dummy), "=r" (channel_dummy),
          "=r" (i_dummy)
          /* Technically, we need to declare accum_buffer as an output,
           * but if we do that GCC runs out of registers.  We get the
           * correct code even without the output declaration, so we just
           * drop it. */
          /* , "=m" (accum_buffer) */
        : [buf]          "0" (accum_buffer),
          [buf_limit]    "1" (accum_buffer + samples*2),
          /* These two are updated at the top of the loop; adjust so the
           * first loop iteration sees the first channel. */
          [channel]      "2" (copy_channels - 1),
          [i]            "3" (num_channels + 1),
          [base_volume_scaled] "r" (iroundf(base_volume * (1 << 27))),
          [pan_bits]     "I" (PAN_BITS),
          [pan_mult]     "I" (PAN_MULT),
          [mono_shift]   "I" (MONO_SHIFT - PAN_BITS - 16),
          [stereo_shift] "I" (STEREO_SHIFT - PAN_BITS - 16),
          [mono_round]   "I" (1 << (MONO_SHIFT - PAN_BITS - 17)),
          [stereo_round] "I" (1 << (STEREO_SHIFT - PAN_BITS - 17)),
          [stereo]       "I" (offsetof(MixerChannelInfo,stereo)),
          [volume]       "I" (offsetof(MixerChannelInfo,volume)),
          [pan]          "I" (offsetof(MixerChannelInfo,pan)),
          [stereo_pan_l] "I" (offsetof(MixerChannelInfo,stereo_pan_l)),
          [stereo_pan_r] "I" (offsetof(MixerChannelInfo,stereo_pan_r)),
          [playing]      "I" (offsetof(MixerChannelInfo,playing)),
          [pcm_buffer]   "I" (offsetof(MixerChannelInfo,pcm_buffer)),
          [sizeof_ch]    "I" (sizeof(copy_channels[0]))
        : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r8", "cc"
    );

    void * volatile mix_dummy;
    void * volatile accum_dummy;
    /* We use a bit of magic here to efficiently handle non-multiple-of-4
     * sample counts: we set up a sequence of three single sample copies
     * (the maximum possible), then jump (using an ADD PC, PC, Rn) over
     * the appropriate number of instructions so we copy exactly the
     * desired number of samples. */
    __asm__(
        "add r9, %[mix_buffer], %[count], lsl #2\n"
        "and r8, %[count], #3                   \n"
        "add r6, r8, r8, lsl #2  @ r6 = r8*5    \n"
        "@ 5*3-1: 5 insns * 3 samples - 1 because the ADD insn reads PC+8.\n"
        "rsb r5, r6, #(5*3-1)                   \n"
        "add pc, pc, r5, lsl #2                 \n"
        "3:                                     \n"
        "ldmia %[accum_buffer]!, {r0-r1}        \n"
        "ssat r0, #16, r0                       \n"
        "ssat r1, #16, r1                       \n"
        "pkhbt r0, r0, r1, lsl #16              \n"
        "stmia %[mix_buffer]!, {r0}             \n"
        "2:                                     \n"
        "ldmia %[accum_buffer]!, {r0-r1}        \n"
        "ssat r0, #16, r0                       \n"
        "ssat r1, #16, r1                       \n"
        "pkhbt r0, r0, r1, lsl #16              \n"
        "stmia %[mix_buffer]!, {r0}             \n"
        "1:                                     \n"
        "ldmia %[accum_buffer]!, {r0-r1}        \n"
        "ssat r0, #16, r0                       \n"
        "ssat r1, #16, r1                       \n"
        "pkhbt r0, r0, r1, lsl #16              \n"
        "stmia %[mix_buffer]!, {r0}             \n"
        "0:                                     \n"
        "cmp %[mix_buffer], r9                  \n"
        "bcs 9f                                 \n"
        "4:                                     \n"
        "ldmia %[accum_buffer]!, {r0-r6,r8}     \n"
        "ssat r0, #16, r0                       \n"
        "ssat r1, #16, r1                       \n"
        "ssat r2, #16, r2                       \n"
        "ssat r3, #16, r3                       \n"
        "ssat r4, #16, r4                       \n"
        "ssat r5, #16, r5                       \n"
        "ssat r6, #16, r6                       \n"
        "ssat r8, #16, r8                       \n"
        "pkhbt r0, r0, r1, lsl #16              \n"
        "pkhbt r1, r2, r3, lsl #16              \n"
        "pkhbt r2, r4, r5, lsl #16              \n"
        "pkhbt r3, r6, r8, lsl #16              \n"
        "stmia %[mix_buffer]!, {r0-r3}          \n"
        "cmp %[mix_buffer], r9                  \n"
        "bcc 0b                                 \n"
        "9:                                     \n"
        : "=r" (mix_dummy), "=r" (accum_dummy)
          /* Normally we also need  "=m" (buffer)  to indicate that the
           * contents of buffer are modified.  However, (1) buffer is a
           * "void *" here, so the extra specification doesn't actually
           * give the compiler any useful information, and (2) including
           * the specification causes the version of Clang included with
           * Xcode 4.5.2 to choke on register allocation.  So we drop
           * the 13-character specification and include this 8-line
           * explanation instead. */
        : [mix_buffer] "0" (buffer),
          [accum_buffer] "1" (accum_buffer),
          [count] "r" (samples),
          "m" (accum_buffer[0])
        : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r8", "r9", "cc"
    );

#elif defined(SIL_ARCH_MIPS_32) && defined(__GNUC__)

    __asm__(
        ".set push; .set noreorder\n"
        "# Register usage is as follows:                                \n"
        "#     t0-t3, t8-t9: Temporaries                                \n"
        "#     t4: (Mono)   Left sample multiplier = volume * (1-pan)   \n"
        "#         (Stereo) Channel volume * left channel pan           \n"
        "#     t5: (Mono)   Right sample multiplier = volume * pan      \n"
        "#         (Stereo) Channel volume * right channel pan          \n"
        "#     t6: (Mono)   Current sample value                        \n"
        "#         (Stereo) Current left sample value                   \n"
        "#     t7: (Mono)   Unused                                      \n"
        "#         (Stereo) Current right sample value                  \n"
        "#     s0: Sample accumulator buffer pointer                    \n"
        "#     s1: Sample accumulator buffer limit pointer              \n"
        "#     s2: Channel PCM data pointer                             \n"
        "#     s4: Base volume multiplier (5.27 fixed point)            \n"
        "#     s5: Number of channels                                   \n"
        "#     s6: Pointer to current channel's MixerChannelInfo structure\n"
        "#     s7: Index of current channel                             \n"
        "                                                               \n"
        "# Set up registers.                                            \n"
        "sll $t8, %[count], 3                                           \n"
        "addu $s1, %[buf], $t8                                          \n"
        "li $s7, -1  # $s6/$s7 are advanced at the top of the loop.     \n"
        "addiu $s6, %[copy_channels], -%[sizeof_ch]                     \n"
        "move $s5, %[num_channels]                                      \n"
        "round.w.s $f0, %[base_volume_scaled]                           \n"
        "mfc1 $s4, $f0                                                  \n"
        "                                                               \n"
        "# Clear the output buffer to zero.                             \n"
        "addiu $s0, %[buf], 32                                          \n"
     "0: sw $zero, -32($s0)                                             \n"
        "sw $zero, -28($s0)                                             \n"
        "sw $zero, -24($s0)                                             \n"
        "sw $zero, -20($s0)                                             \n"
        "sw $zero, -16($s0)                                             \n"
        "sw $zero, -12($s0)                                             \n"
        "sw $zero, -8($s0)                                              \n"
        "addiu $s0, $s0, 32                                             \n"
        "sltu $t9, $s1, $s0                                             \n"
        "beqz $t9, 0b                                                   \n"
        "sw $zero, -(4+32)($s0)                                         \n"
        "                                                               \n"
        "######## Process each channel in sequence. ########            \n"
        "                                                               \n"
"chanloop:                                                              \n"
        "# Increment the channel pointer and index, and check whether   \n"
        "# we've reached the end of the array.  Also load some values   \n"
        "# we'll use later; note that the overrun on the last channel   \n"
        "# is deliberate and safe.                                      \n"
        "lbu $t0, %[sizeof_ch]+%[playing]($s6)                          \n"
        "lw $t1, %[sizeof_ch]+%[volume]($s6)                            \n"
        "addiu $s7, $s7, 1                                              \n"
        "sltu $t8, $s7, $s5                                             \n"
        "multu $t1, $s4                                                 \n"
        "beqz $t8, chanloop_end                                         \n"
        "addiu $s6, $s6, %[sizeof_ch]                                   \n"
        "# Skip out early if this channel isn't playing or is silent.   \n"
        "beqz $t0, chanloop                                             \n"
        "lbu $t2, %[stereo]($s6)                                        \n"
        "beqz $t1, chanloop                                             \n"
        "# We deliberately pull out the scaled volume after the test to \n"
        "# leave an extra cycle for the ALU in case it needs it.  This  \n"
        "# should normally make no difference since there's little sense\n"
        "# in setting the base volume to zero.                          \n"
        "mfhi $t1                                                       \n"
        "# Scale the volume down into $t9 (so volume*pan fits in 32     \n"
        "# bits), and go to the appropriate loop (mono or stereo).  The \n"
        "# -5 in the shift count is because we already cut off 5 bits   \n"
        "# when scaling by the base volume.                             \n"
        "bnez $t2, stereo_setup                                         \n"
        "srl $t9, $t1, %[pan_bits] - 5                                  \n"
        "                                                               \n"
        "######## Setup for monaural channels. ########                 \n"
        "                                                               \n"
        "# Load this channel's data.                                    \n"
        "lw $t5, %[pan]($s6)                                            \n"
        "li $t4, 1<<%[pan_bits]                                         \n"
        "multu $t9, $t5                                                 \n"
        "subu $t4, $t4, $t5                                             \n"
        "mflo $t5                                                       \n"
        "move $s0, %[buf]                                               \n"
        "lw $s2, %[pcm_buffer]($s6)                                     \n"
        "multu $t9, $t4                                                 \n"
        "mflo $t4                                                       \n"
        "# Load the first sample.                                       \n"
        "lh $t6, 0($s2)                                                 \n"
        "                                                               \n"
        "######## Playback loop for monaural channels. ########         \n"
        "                                                               \n"
        "# Scale the sample up to 16.16 fixed point.                    \n"
        "sll $t6, $t6, 16                                               \n"
"monoloop:                                                              \n"
        "# Add the sample to the left and right accumulated values.     \n"
        "mult $t6, $t4           # Left output sample (to $t2)          \n"
        "lw $t0, 0($s0)          # $t0: Sum of left samples             \n"
        "lw $t1, 4($s0)          # $t1: Sum of right samples            \n"
        "addiu $s0, $s0, 8       # Advance the output pointer.          \n"
        "mfhi $t2                                                       \n"
        "addiu $t2, $t2, 1 << (%[mono_shift] - %[pan_bits] - 17)        \n"
        "sra $t2, $t2, %[mono_shift] - %[pan_bits] - 16                 \n"
        "mult $t6, $t5           # Right output sample (to $t3)         \n"
        "addu $t2, $t2, $t0                                             \n"
        "sw $t2, -8($s0)                                                \n"
        "mfhi $t3                                                       \n"
        "addiu $t3, $t3, 1 << (%[mono_shift] - %[pan_bits] - 17)        \n"
        "sra $t3, $t3, %[mono_shift] - %[pan_bits] - 16                 \n"
        "addu $t3, $t3, $t1                                             \n"
        "sw $t3, -4($s0)                                                \n"
        "                                                               \n"
        "# Load the next input sample.  This is a copy of the first     \n"
        "# instruction in the loop; we do the load here to avoid load   \n"
        "# delay stalls.  Note that this overruns the input buffer on   \n"
        "# the final sample; we allocate an extra stereo sample's worth \n"
        "# of space at init time to ensure this is safe.                \n"
        "lh $t6, 2($s2)                                                 \n"
        "# Advance the input pointer to the next sample.                \n"
        "addiu $s2, $s2, 2                                              \n"
        "# Process the next sample, if any are left.                    \n"
        "bne $s0, $s1, monoloop                                         \n"
        "sll $t6, $t6, 16        # First insn copied to the delay slot. \n"
        "b chanloop                                                     \n"
        "nop                                                            \n"
        "                                                               \n"
        "######## Setup for stereo channels. ########                   \n"
        "                                                               \n"
"stereo_setup:                                                          \n"
        "# Load this channel's data.                                    \n"
        "lh $t6, %[stereo_pan_l]($s6)                                   \n"
        "lh $t7, %[stereo_pan_r]($s6)                                   \n"
        "mult $t6, $t9                                                  \n"
        "mflo $t4                                                       \n"
        "lw $s2, %[pcm_buffer]($s6)                                     \n"
        "move $s0, %[buf]                                               \n"
        "mult $t7, $t9                                                  \n"
        "mflo $t5                                                       \n"
        "# Load the first sample for each audio channel.                \n"
        "lh $t6, 0($s2)                                                 \n"
        "lh $t7, 2($s2)                                                 \n"
        "                                                               \n"
        "######## Playback loop for stereo channels. ########           \n"
        "                                                               \n"
        "sll $t6, $t6, 16                                               \n"
"stereoloop:                                                            \n"
        "sll $t7, $t7, 16                                               \n"
        "mult $t6, $t4           # Left output sample (to $t2)          \n"
        "lw $t0, 0($s0)          # $t0: Sum of left samples             \n"
        "lw $t1, 4($s0)          # $t1: Sum of right samples            \n"
        "addiu $s0, $s0, 8       # Advance the output pointer.          \n"
        "mfhi $t2                                                       \n"
        "addiu $t2, $t2, 1 << (%[stereo_shift] - %[pan_bits] - 17)      \n"
        "sra $t2, $t2, %[stereo_shift] - %[pan_bits] - 16               \n"
        "mult $t7, $t5           # Right output sample (to $t3)         \n"
        "addu $t2, $t2, $t0                                             \n"
        "sw $t2, -8($s0)                                                \n"
        "mfhi $t3                                                       \n"
        "addiu $t3, $t3, 1 << (%[stereo_shift] - %[pan_bits] - 17)      \n"
        "sra $t3, $t3, %[stereo_shift] - %[pan_bits] - 16               \n"
        "addu $t3, $t3, $t1                                             \n"
        "sw $t3, -4($s0)                                                \n"
        "                                                               \n"
        "lh $t6, 4($s2)                                                 \n"
        "lh $t7, 6($s2)                                                 \n"
        "addiu $s2, $s2, 4                                              \n"
        "bne $s0, $s1, stereoloop                                       \n"
        "sll $t6, $t6, 16        # First insn copied to the delay slot. \n"
        "b chanloop                                                     \n"
        "nop                                                            \n"
        "                                                               \n"
        "######## End of loops. ########                                \n"
        "                                                               \n"
"chanloop_end:                                                          \n"
        ".set pop"
        : "=m" (accum_buffer)
        : [buf]          "r" (accum_buffer),
          [count]        "r" (samples),
          [copy_channels] "r" (copy_channels),
          [num_channels] "r" (num_channels),
          [base_volume_scaled] "f" (base_volume * (1 << 27)),
          [pan_bits]     "i" (PAN_BITS),
          [mono_shift]   "i" (MONO_SHIFT),
          [stereo_shift] "i" (STEREO_SHIFT),
          [stereo]       "I" (offsetof(MixerChannelInfo,stereo)),
          [volume]       "I" (offsetof(MixerChannelInfo,volume)),
          [pan]          "I" (offsetof(MixerChannelInfo,pan)),
          [stereo_pan_l] "I" (offsetof(MixerChannelInfo,stereo_pan_l)),
          [stereo_pan_r] "I" (offsetof(MixerChannelInfo,stereo_pan_r)),
          [playing]      "I" (offsetof(MixerChannelInfo,playing)),
          [pcm_buffer]   "I" (offsetof(MixerChannelInfo,pcm_buffer)),
          [sizeof_ch]    "I" (sizeof(copy_channels[0]))
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9",
          "s0", "s1", "s2", "s4", "s5", "s6", "s7", "$f0"
    );

    __asm__(
        ".set push; .set noreorder\n"
        "move $a0, %[accum_buffer]      \n"
        "move $a1, %[mix_buffer]        \n"
        "sll $t9, %[count], 3           \n"
        "addu $t9, $a0, $t9             \n"
        "lw $t0, 0($a0)                 \n"
        "andi $t3, %[count], 1          \n"
        "lw $t1, 4($a0)                 \n"
        "beqzl $t3, 1f                  \n"
        "lw $t2, 8($a0)                 \n"
#ifdef SIL_PLATFORM_PSP
        /* PSP prefers having RAW dependencies adjacent (EX->EX forwarding). */
        "min $t0, $t0, %[x7FFF]         \n"
        "max $t0, $t0, %[x8000]         \n"
        "min $t1, $t1, %[x7FFF]         \n"
        "max $t1, $t1, %[x8000]         \n"
#else
        "slt $t4, $t0, %[x7FFF]         \n"
        "slt $t5, $t1, %[x7FFF]         \n"
        "movz $t0, %[x7FFF], $t4        \n"
        "movz $t1, %[x7FFF], $t5        \n"
        "slt $t4, $t0, %[x8000]         \n"
        "slt $t5, $t1, %[x8000]         \n"
        "movn $t0, %[x8000], $t4        \n"
        "movn $t1, %[x8000], $t5        \n"
#endif
        "add $a0, $a0, 8                \n"
#if defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP)
        "ins $t0, $t1, 16, 16           \n"
#else
        "sll $t1, $t1, 16               \n"
        "andi $t0, $t0, 0xFFFF          \n"
        "or $t0, $t0, $t1               \n"
#endif
        "add $a1, $a1, 4                \n"
        "beq $a0, $t9, 9f               \n"
        "sw $t0, -4($a1)                \n"
        "lw $t0, 0($a0)                 \n"
     "0: lw $t1, 4($a0)                 \n"
        "lw $t2, 8($a0)                 \n"
     "1: lw $t3, 12($a0)                \n"
        "addiu $a0, $a0, 16             \n"
#ifdef SIL_PLATFORM_PSP
        "min $t0, $t0, %[x7FFF]         \n"
        "max $t0, $t0, %[x8000]         \n"
        "min $t1, $t1, %[x7FFF]         \n"
        "max $t1, $t1, %[x8000]         \n"
        "min $t2, $t2, %[x7FFF]         \n"
        "max $t2, $t2, %[x8000]         \n"
        "min $t3, $t3, %[x7FFF]         \n"
        "max $t3, $t3, %[x8000]         \n"
#else
        "slt $t4, $t0, %[x7FFF]         \n"
        "slt $t5, $t1, %[x7FFF]         \n"
        "movz $t0, %[x7FFF], $t4        \n"
        "movz $t1, %[x7FFF], $t5        \n"
        "slt $t4, $t2, %[x7FFF]         \n"
        "slt $t5, $t3, %[x7FFF]         \n"
        "movz $t2, %[x7FFF], $t4        \n"
        "movz $t3, %[x7FFF], $t5        \n"
        "slt $t4, $t0, %[x8000]         \n"
        "slt $t5, $t1, %[x8000]         \n"
        "movn $t0, %[x8000], $t4        \n"
        "movn $t1, %[x8000], $t5        \n"
        "slt $t4, $t2, %[x8000]         \n"
        "slt $t5, $t3, %[x8000]         \n"
        "movn $t2, %[x8000], $t4        \n"
        "movn $t3, %[x8000], $t5        \n"
#endif
#if defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP)
        "ins $t0, $t1, 16, 16           \n"
        "ins $t2, $t3, 16, 16           \n"
#else
        "sll $t1, $t1, 16               \n"
        "sll $t3, $t3, 16               \n"
        "andi $t0, $t0, 0xFFFF          \n"
        "andi $t2, $t2, 0xFFFF          \n"
        "or $t0, $t0, $t1               \n"
        "or $t2, $t2, $t3               \n"
#endif
        "sw $t0, 0($a1)                 \n"
        "addiu $a1, $a1, 8              \n"
        "sw $t2, -4($a1)                \n"
        "bnel $a0, $t9, 0b              \n"
        "lw $t0, 0($a0)                 \n"
    "9: .set pop"
        : "=m" (((int16_t *)buffer)[0])
        : [mix_buffer]   "r" (buffer),
          [accum_buffer] "r" (accum_buffer),
          [count]        "r" (samples),
          [x7FFF]        "r" (0x7FFF),
          [x8000]        "r" (-0x8000),
          "m" (accum_buffer[0])
        : "a0", "a1", "t0", "t1", "t2", "t3", "t9"
#ifndef SIL_PLATFORM_PSP
          , "t4", "t5"
#endif
    );

#else  // No assembly version available.

    for (int ch = 0; ch < num_channels; ch++) {
        if (!copy_channels[ch].playing || copy_channels[ch].volume == 0) {
            continue;
        }
        const int64_t volume = iroundf(copy_channels[ch].volume * base_volume);
        int32_t pan_l, pan_r;
        if (copy_channels[ch].stereo) {
            pan_l = copy_channels[ch].stereo_pan_l;
            pan_r = copy_channels[ch].stereo_pan_r;
        } else {
            pan_l = PAN_MULT - copy_channels[ch].pan;
            pan_r = copy_channels[ch].pan;
        }
        for (int i = 0; i < samples; i++) {
            int32_t left  = accum_buffer[i*2+0];
            int32_t right = accum_buffer[i*2+1];
            if (copy_channels[ch].stereo) {
                left += ((copy_channels[ch].pcm_buffer[i*2+0] * pan_l)
                         * volume + ((int64_t)1 << (STEREO_SHIFT-1)))
                    >> STEREO_SHIFT;
                right += ((copy_channels[ch].pcm_buffer[i*2+1] * pan_r)
                           * volume + ((int64_t)1 << (STEREO_SHIFT-1)))
                    >> STEREO_SHIFT;
            } else {
                const int32_t sample = copy_channels[ch].pcm_buffer[i];
                left += ((sample * pan_l) * volume
                         + ((int64_t)1 << (MONO_SHIFT-1))) >> MONO_SHIFT;
                right += ((sample * pan_r) * volume
                          + ((int64_t)1 << (MONO_SHIFT-1))) >> MONO_SHIFT;
            }
            accum_buffer[i*2+0] = left;
            accum_buffer[i*2+1] = right;
        }
    }  // for (int ch = 0 ... num_channels-1)

    int16_t *buffer16 = (int16_t *)buffer;
    for (int i = 0; i < samples*2; i++) {
        const int32_t sample = accum_buffer[i];
        if (UNLIKELY(sample > 0x7FFF)) {
            buffer16[i] = 0x7FFF;
        } else if (UNLIKELY(sample < -0x8000)) {
            buffer16[i] = -0x8000;
        } else {
            buffer16[i] = sample;
        }
    }

#endif  // SIL_ARCH_*

#ifdef MIX_TIMING
    const double time_end = time_now();
    timing_lock += time_decode - time_lock;
    timing_lock_max = max(timing_lock_max, time_decode - time_lock);
    timing_decode += time_render - time_decode;
    timing_render += time_end - time_render;
    timing_samples += samples;
    for (int ch = 0; ch < num_channels; ch++) {
        if (copy_channels[ch].playing && copy_channels[ch].volume != 0) {
            timing_channels++;
        }
    }
    timing_count++;
    if (timing_samples >= mix_rate) {
        const double total_time = (double)timing_samples / (double)mix_rate;
        DLOG("Mixer stats (%d calls):\n"
             "    Avg. channels: %.2f\n"
             "        Lock wait: %5.2fms/call (%.2f%% CPU), max %5.2fms\n"
             "     Audio decode: %5.2fms/call (%.2f%% CPU)\n"
             "     Audio render: %5.2fms/call (%.2f%% CPU), %.3fus/sample",
             timing_count,
             (float)timing_channels / (float)timing_count,
             (timing_lock / timing_count) * 1000,
             (timing_lock / total_time) * 100,
             timing_lock_max * 1000,
             (timing_decode / timing_count) * 1000,
             (timing_decode / total_time) * 100,
             (timing_render / timing_count) * 1000,
             (timing_render / total_time) * 100,
             (timing_render / timing_samples) * 1000000);
        timing_lock = 0;
        timing_lock_max = 0;
        timing_decode = 0;
        timing_render = 0;
        timing_samples = 0;
        timing_channels = 0;
        timing_count = 0;
    }
#endif
}

/*************************************************************************/
/*************************************************************************/
