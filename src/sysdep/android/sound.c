/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/sound.c: Android audio output interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/thread.h"
#include "src/time.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * SOUND_BUFLEN:  Number of samples to send to the hardware in a single
 * output call.  Should be the same as the mixer buffer length (in
 * sound/mixer.c) for best performance.
 */
#define SOUND_BUFLEN  1024

/**
 * SOUND_MIXER_BUFFERS:  Number of SOUND_BUFLEN-sized buffers to use for
 * buffering audio data.
 */
#define SOUND_MIXER_BUFFERS  5

/**
 * SOUND_HW_BUFFERS:  Number of SOUND_BUFLEN-sized buffers to allocate
 * for the audio driver.
 */
#define SOUND_HW_BUFFERS  4

/**
 * MIXER_THREAD_PRIORITY:  Thread priority used for the mixer thread,
 * relative to the main thread.
 */
#define MIXER_THREAD_PRIORITY \
    (-(THREAD_PRIORITY_AUDIO - THREAD_PRIORITY_FOREGROUND))

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Cached Java method IDs. */
static jmethodID getAudioBecameNoisy, clearAudioBecameNoisy;

/* Output sampling rate used by the hardware. */
static int output_rate;

/* Various OpenSL handles. */
static SLObjectItf engine_Object;
static SLEngineItf engine_Engine;
static SLObjectItf mixer_Object;
static SLObjectItf player_Object;

/* Quick macro to run Query(excuse-me-I-mean-Get)Interface on an object. */
#define QI(from,iid,to_ptr)  ((*(from))->GetInterface((from), (iid), (to_ptr)))

/* Zero-filled buffer used when we want to send out silence. */
static const uint16_t silence_buffer[2*SOUND_BUFLEN];

/*-----------------------------------------------------------------------*/

/* Audio data buffer.  Samples from the software mixer are buffered here
 * before being sent to the hardware. */
static void *output_buffer_mem;
static struct {
    uint8_t full;  // Is this buffer ready to be played?
    void *data;
} output_buffers[SOUND_MIXER_BUFFERS];

/* Buffer currently being played by the hardware, or -1 if none. */
static int buffer_playing;

/* Next buffer to send to the hardware (used by audio render callback). */
static unsigned int next_buffer_to_play;

/* Thread ID of mixer thread, and flag used to tell it to stop. */
static int mixer_thread_id;
static uint8_t mixer_thread_stop;

/* Flag indicating whether we should act on headphone disconnect events. */
static uint8_t check_headphone_disconnect;

/*-----------------------------------------------------------------------*/

/* Convenience macro for error-checking system calls, which calls a
 * function and logs a message if an error occurs, returning the success
 * or failure of the call as a boolean value.  The macro takes a function
 * call expression as its parameter. */

#define CHECK(call)  (check_error((call), #call, __LINE__))

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * audio_callback:  Buffer queue callback called by OpenSL when a buffer
 * finishes playing.
 *
 * [Parameters]
 *     queue: Buffer queue.
 *     context: User data pointer (unused).
 */
static void audio_callback(SLAndroidSimpleBufferQueueItf queue, void *context);

/**
 * mixer_thread:  Thread which buffers the output of the software mixer.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 * [Return value]
 *     0
 */
static int mixer_thread(void *userdata);

/**
 * check_error:  If the given error code indicates an error, log a debug
 * message and return false; otherwise return true.
 *
 * [Parameters]
 *     error_code: OpenSL error code.
 *     call_text: Text describing the OpenSL call (for debug messages).
 *     line: Source line where the error occurred.
 * [Return value]
 *     True if the error code was SL_RESULT_SUCCESS, indicating a
 *     successful call; false otherwise.
 */
static int check_error(SLresult error_code, const char *call_text, int line);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_sound_init(const char *device_name)
{
    PRECOND(device_name != NULL);

    /* Cache various Java method IDs we'll use later. */

    jmethodID getAudioOutputRate;
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    getAudioOutputRate = get_method(0, "getAudioOutputRate", "()I");
    getAudioBecameNoisy = get_method(0, "getAudioBecameNoisy", "()Z");
    clearAudioBecameNoisy = get_method(0, "clearAudioBecameNoisy", "()V");
    ASSERT(getAudioOutputRate != 0, goto error_return);
    ASSERT(getAudioBecameNoisy != 0, goto error_return);
    ASSERT(clearAudioBecameNoisy != 0, goto error_return);

    /* Set up the Android audio output chain. */

    output_rate = (*env)->CallIntMethod(env, activity_obj, getAudioOutputRate);
    ASSERT(!clear_exceptions(env), output_rate = 48000);
    if (output_rate < 8000) {
        if (output_rate > 0) {
            DLOG("Bizarre audio output rate %d, using 48000", output_rate);
        } else {
            DLOG("Couldn't get audio output rate, using 48000");
        }
        output_rate = 48000;
    }

    if (!CHECK(slCreateEngine(&engine_Object, 0, NULL, 0, NULL, NULL))) {
        goto error_return;
    }
    if (!CHECK((*engine_Object)->Realize(engine_Object, 0))) {
        goto error_destroy_engine;
    }
    if (!CHECK(QI(engine_Object, SL_IID_ENGINE, &engine_Engine))) {
        goto error_destroy_engine;
    }

    if (!CHECK((*engine_Engine)->CreateOutputMix(engine_Engine, &mixer_Object,
                                                 0, NULL, NULL))) {
        goto error_destroy_engine;
    }
    if (!CHECK((*mixer_Object)->Realize(mixer_Object, 0))) {
        goto error_destroy_mixer;
    }

    /* This can't be static because SL_IID_* aren't constants, but external
     * (structured!) symbols in the sound library.  Seriously, Google, this
     * is stupid. */
    const SLInterfaceID player_iid[] = {SL_IID_BUFFERQUEUE};
    static const SLboolean player_req[] = {SL_BOOLEAN_TRUE};
    if (!CHECK((*engine_Engine)->CreateAudioPlayer(
        engine_Engine, &player_Object,
        &(SLDataSource){
            &(SLDataLocator_AndroidSimpleBufferQueue){
                SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, SOUND_HW_BUFFERS},
            &(SLDataFormat_PCM){SL_DATAFORMAT_PCM, 2, output_rate*1000, 16, 16,
                SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                /* How can you not have a NATIVEENDIAN flag??? */
#ifdef IS_LITTLE_ENDIAN
                SL_BYTEORDER_LITTLEENDIAN
#else
                SL_BYTEORDER_BIGENDIAN
#endif
            }
        },
        &(SLDataSink){
            &(SLDataLocator_OutputMix){SL_DATALOCATOR_OUTPUTMIX, mixer_Object},
            NULL
        },
        lenof(player_iid), player_iid, player_req)))
    {
        goto error_destroy_mixer;
    }
    if (!CHECK((*player_Object)->Realize(player_Object, 0))) {
        goto error_destroy_player;
    }
    SLPlayItf player_Play;
    if (!CHECK(QI(player_Object, SL_IID_PLAY, &player_Play))) {
        goto error_destroy_player;
    }
    SLAndroidSimpleBufferQueueItf player_Queue;
    if (!CHECK(QI(player_Object, SL_IID_BUFFERQUEUE, &player_Queue))) {
        goto error_destroy_player;
    }
    (*player_Queue)->RegisterCallback(player_Queue, audio_callback, NULL);

    /* Set up output buffers for the software mixer. */

    output_buffer_mem =
        mem_alloc((4*SOUND_BUFLEN) * SOUND_MIXER_BUFFERS, 0, 0);
    if (!output_buffer_mem) {
        DLOG("No memory for output buffers (%d bytes)",
             (4*SOUND_BUFLEN) * SOUND_MIXER_BUFFERS);
        goto error_destroy_player;
    }
    for (int i = 0; i < SOUND_MIXER_BUFFERS; i++) {
        output_buffers[i].full = 0;
        output_buffers[i].data =
            (uint8_t *)output_buffer_mem + (4*SOUND_BUFLEN)*i;
    }

    /* Start playback. */

    buffer_playing = -1;
    next_buffer_to_play = 0;
    mixer_thread_stop = 0;
    mixer_thread_id = thread_create_with_priority(
        MIXER_THREAD_PRIORITY - THREAD_PRIORITY_FOREGROUND,
        mixer_thread, NULL);
    if (!mixer_thread_id) {
        DLOG("Failed to start mixer thread");
        goto error_free_output_buffer_mem;
    }

    if (!CHECK((*player_Play)->SetPlayState(player_Play,
                                            SL_PLAYSTATE_PLAYING))) {
        goto error_stop_mixer_thread;
    }
    /* Prime playback with empty buffers. */
    for (int i = 0; i < SOUND_HW_BUFFERS; i++) {
        if (!CHECK((*player_Queue)->Enqueue(player_Queue, silence_buffer,
                                            sizeof(silence_buffer)))) {
            goto error_stop_mixer_thread;
        }
    }

    /* All done. */

    return 1;


  error_stop_mixer_thread:
    mixer_thread_stop = 1;
    thread_wait(mixer_thread_id);
    mixer_thread_id = 0;
    mixer_thread_stop = 0;
  error_free_output_buffer_mem:
    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;
  error_destroy_player:
    (*player_Object)->Destroy(player_Object);
    player_Object = NULL;
  error_destroy_mixer:
    (*mixer_Object)->Destroy(mixer_Object);
    mixer_Object = NULL;
  error_destroy_engine:
    (*engine_Object)->Destroy(engine_Object);
    engine_Object = NULL;
    engine_Engine = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_sound_playback_rate(void)
{
    return output_rate;
}

/*-----------------------------------------------------------------------*/

float sys_sound_set_latency(UNUSED float latency)
{
    /* We don't support changing the latency. */
    return ((float)(SOUND_BUFLEN * (SOUND_HW_BUFFERS-1) + SOUND_BUFLEN/2)
            / (float)output_rate);
}

/*-----------------------------------------------------------------------*/

void sys_sound_enable_headphone_disconnect_check(int enable)
{
    check_headphone_disconnect = (enable != 0);
}

/*-----------------------------------------------------------------------*/

int sys_sound_check_headphone_disconnect(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    const int became_noisy =
        (*env)->CallBooleanMethod(env, activity_obj, getAudioBecameNoisy);
    ASSERT(!clear_exceptions(env), return 0);
    return became_noisy;
}

/*-----------------------------------------------------------------------*/

void sys_sound_acknowledge_headphone_disconnect(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    (*env)->CallVoidMethod(env, activity_obj, clearAudioBecameNoisy);
    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

void sys_sound_cleanup(void)
{
    (*player_Object)->Destroy(player_Object);
    player_Object = NULL;
    (*mixer_Object)->Destroy(mixer_Object);
    mixer_Object = NULL;
    (*engine_Object)->Destroy(engine_Object);
    engine_Object = NULL;
    engine_Engine = NULL;

    mixer_thread_stop = 1;
    thread_wait(mixer_thread_id);
    mixer_thread_id = 0;

    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;
}

/*************************************************************************/
/*********************** System callback routines ************************/
/*************************************************************************/

static void audio_callback(SLAndroidSimpleBufferQueueItf queue,
                           UNUSED void *context)
{
    const void *buffer;

    if (buffer_playing >= 0) {
        output_buffers[buffer_playing].full = 0;
    }
    BARRIER();
    if (output_buffers[next_buffer_to_play].full) {
        buffer = output_buffers[next_buffer_to_play].data;
        buffer_playing = next_buffer_to_play;
        next_buffer_to_play = (next_buffer_to_play + 1) % SOUND_MIXER_BUFFERS;
        if (check_headphone_disconnect) {
            JavaVM *vm = android_activity->vm;
            JNIEnv *env = NULL;
            (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
            if (!env) {
                (*vm)->AttachCurrentThread(vm, &env, NULL);
                ASSERT(env);
            }
            if (sys_sound_check_headphone_disconnect()) {
                buffer = silence_buffer;
            }
        }
    } else {
        buffer = silence_buffer;
        buffer_playing = -1;
    }
    CHECK((*queue)->Enqueue(queue, buffer, 4*SOUND_BUFLEN));
}

/*************************************************************************/
/************************* Other local routines **************************/
/*************************************************************************/

static int mixer_thread(UNUSED void *userdata)
{
    const float buffer_time = (float)SOUND_BUFLEN / (float)output_rate;

    unsigned int next_buffer_to_fill = 0;
    do {
        BARRIER();
        /*
         * In theory, it's enough to check whether the next buffer to fill
         * is empty or not.  However, if we ever happen to get out of sync
         * with the hardware output thread, we'd end up playing buffers in
         * a different order from how they were generated:
         *
         *     to_fill  to_play  Buffer 0  Buffer 1  Action taken
         *     -------  -------  --------  --------  -------------
         *        0        1       empty     empty   Fill 0 with A
         *        1        1         A       empty   Fill 1 with B
         *        0        1         A         B     Play B
         *        0        0         A       empty   Play A
         *        0        1       empty     empty   Fill 0 with C
         *        1        1         C       empty   Fill 1 with D
         *        0        1         C         D     Play D
         *        0        0         C       empty   Play C
         *        0        1       empty     empty   ...
         *
         * So to be safe, if the next buffer to fill is already full, we
         * look for the next non-empty buffer and fill it instead of just
         * waiting for the expected buffer to become empty:
         *
         *     to_fill  to_play  Buffer 0  Buffer 1  Action taken
         *     -------  -------  --------  --------  -------------
         *        0        1       empty     empty   Fill 0 with A
         *        1        1         A       empty   Fill 1 with B
         *        0        1         A         B     Play B
         *        0        0         A       empty   Fill 1 with C
         *        0        0         A         C     Play A
         *        0        1       empty       C     Fill 0 with D
         *        1        1         D         C     Play C
         *        1        0         D       empty   Fill 1 with E
         *        0        0         D         E     Play D
         *        0        1       empty       E     ...
         *
         * This relies on the assumption that the mixer will generally
         * run faster than the playback thread -- more specifically, that
         * this loop will execute more frequently than the audio output
         * callback is called by the system.
         */
        const unsigned int old_to_fill = next_buffer_to_fill;
        if (output_buffers[next_buffer_to_fill].full) {
            do {
                next_buffer_to_fill =
                    (next_buffer_to_fill + 1) % SOUND_MIXER_BUFFERS;
            } while (next_buffer_to_fill != old_to_fill
                     && output_buffers[next_buffer_to_fill].full);
        }
        if (!output_buffers[next_buffer_to_fill].full) {
            sound_mixer_get_pcm(output_buffers[next_buffer_to_fill].data,
                                SOUND_BUFLEN);
            output_buffers[next_buffer_to_fill].full = 1;
            next_buffer_to_fill =
                (next_buffer_to_fill + 1) % SOUND_MIXER_BUFFERS;
        } else {
            time_delay(buffer_time/2);  // Must be < buffer_time (see above).
        }
    } while (!mixer_thread_stop);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int check_error(SLresult error_code, DEBUG_USED const char *call_text,
                       DEBUG_USED int line)
{
    if (error_code == SL_RESULT_SUCCESS) {

        return 1;

    } else {

        #define LOG_ERROR(message) \
            DLOG("Line %d: %s: %s", line, call_text, message);

        if (error_code == SL_RESULT_PRECONDITIONS_VIOLATED) {
            LOG_ERROR("Preconditions violated");
        } else if (error_code == SL_RESULT_PARAMETER_INVALID) {
            LOG_ERROR("Parameter invalid");
        } else if (error_code == SL_RESULT_MEMORY_FAILURE) {
            LOG_ERROR("Memory failure");
        } else if (error_code == SL_RESULT_RESOURCE_ERROR) {
            LOG_ERROR("Resource error");
        } else if (error_code == SL_RESULT_RESOURCE_LOST) {
            LOG_ERROR("Resource lost");
        } else if (error_code == SL_RESULT_IO_ERROR) {
            LOG_ERROR("I/O error");
        } else if (error_code == SL_RESULT_BUFFER_INSUFFICIENT) {
            LOG_ERROR("Buffer insufficient");
        } else if (error_code == SL_RESULT_CONTENT_CORRUPTED) {
            LOG_ERROR("Content corrupted");
        } else if (error_code == SL_RESULT_CONTENT_UNSUPPORTED) {
            LOG_ERROR("Content unsupported");
        } else if (error_code == SL_RESULT_CONTENT_NOT_FOUND) {
            LOG_ERROR("Content not found");
        } else if (error_code == SL_RESULT_PERMISSION_DENIED) {
            LOG_ERROR("Permission denied");
        } else if (error_code == SL_RESULT_FEATURE_UNSUPPORTED) {
            LOG_ERROR("Feature unsupported");
        } else if (error_code == SL_RESULT_INTERNAL_ERROR) {
            LOG_ERROR("Internal error");
        } else if (error_code == SL_RESULT_UNKNOWN_ERROR) {
            LOG_ERROR("Unknown error");
        } else if (error_code == SL_RESULT_OPERATION_ABORTED) {
            LOG_ERROR("Operation aborted");
        } else if (error_code == SL_RESULT_CONTROL_LOST) {
            LOG_ERROR("Control lost");
        } else {
            DLOG("Line %d: %s: Unknown error 0x%X",
                 line, call_text, error_code);
        }

        return 0;

    }
}

/*************************************************************************/
/*************************************************************************/
