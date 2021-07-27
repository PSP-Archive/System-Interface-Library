/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/sound.c: Mac OS X audio output interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/thread.h"
#include "src/time.h"

#include "src/sysdep/macosx/osx-headers.h"
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * SOUND_BUFLEN:  Number of samples per internal buffer.
 */
#define SOUND_BUFLEN  512

/**
 * SOUND_BUFFERS:  Number of SOUND_BUFLEN-sized buffers to use for
 * buffering audio data.
 */
#define SOUND_BUFFERS  4

/**
 * MIXER_THREAD_PRIORITY:  Thread priority used for the mixer thread.
 */
#define MIXER_THREAD_PRIORITY  5

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Default output audio unit instance for playback.  Named "remoteio" to
 * keep the code as close as possible to the iOS code. */
static AudioUnit remoteio_instance;

/* Output sampling rate used by the hardware. */
static int output_rate;

/*-----------------------------------------------------------------------*/

/* Audio data buffer.  Samples from the software mixer are buffered here
 * before being sent to the hardware. */
static void *output_buffer_mem;
static void *output_buffers[SOUND_BUFFERS];

/* Semaphores used for tracking buffer availability.  The audio callback
 * waits on play_sem to get a buffer, then signals fill_sem when the buffer
 * has been consumed; conversely, the mixer thread waits on fill_sem before
 * filling a buffer, then signals play_sem when it has been filled. */
static SysSemaphoreID output_buffer_play_sem;
static SysSemaphoreID output_buffer_fill_sem;

/* Next buffer to send to the hardware (used by audio render callback). */
static int playback_buffer_index;
/* Offset within buffer of first sample to send to the hardware. */
static int playback_buffer_offset;

/* Thread ID of mixer thread, and flag used to tell it to stop. */
static int mixer_thread_id;
static uint8_t mixer_thread_stop;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * find_device:  Return the device ID associated with the named device.
 *
 * [Parameters]
 *     device_name: Name of device to look up.
 * [Return value]
 *     Device ID (an AudioObjectID), or 0 if the device was not found.
 */
static AudioObjectID find_device(const char *device_name);

/**
 * get_hardware_output_rate:  Return the default output sampling rate of
 * the given audio device.
 *
 * [Parameters]
 *     device_id: Device to use, or 0 for the default output device.
 * [Return value]
 *     Default output sampling rate in Hz, or 0 if unknown.
 */
static int get_hardware_output_rate(AudioObjectID device_id);

/**
 * audio_render_callback:  Callback called to generate audio data to be
 * sent to the hardware output device.  This function is called in an
 * independent, realtime-priority thread.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 *     flags: Operation flags (unused).
 *     timestamp: Stream timestamp (unused).
 *     bus: Bus index (unused).
 *     num_frames: Number of audio frames (samples) to generate.
 *     buffers: List of buffers into which to store data.
 */
static OSStatus audio_render_callback(void *userdata,
                                      AudioUnitRenderActionFlags *flags,
                                      const AudioTimeStamp *timestamp,
                                      UInt32 bus, UInt32 num_frames,
                                      AudioBufferList *buffers);

/**
 * mixer_thread:  Thread which buffers the output of the software mixer.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 * [Return value]
 *     0
 */
static int mixer_thread(void *userdata);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_sound_init(const char *device_name)
{
    PRECOND(device_name != NULL, goto error_return);

    /* Look up the requested device, if any. */

    OSType output_type;
    AudioObjectID device_id;
    if (*device_name) {
        output_type = kAudioUnitSubType_HALOutput;
        device_id = find_device(device_name);
        if (!device_id) {
            DLOG("Audio device \"%s\" not found", device_name);
            return 0;
        }
    } else {
        output_type = kAudioUnitSubType_DefaultOutput;
        device_id = 0;
    }

    /* Set up an output audio unit for playback. */

    AudioComponentDescription remoteio_desc;
    mem_clear(&remoteio_desc, sizeof(remoteio_desc));
    remoteio_desc.componentType         = kAudioUnitType_Output;
    remoteio_desc.componentSubType      = output_type;
    remoteio_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    remoteio_desc.componentFlags        = 0;
    remoteio_desc.componentFlagsMask    = 0;
    AudioComponent remoteio_ref = AudioComponentFindNext(NULL, &remoteio_desc);
    if (0 != AudioComponentInstanceNew(remoteio_ref, &remoteio_instance)) {
        DLOG("Failed to create Remote I/O audio unit");
        goto error_return;
    }

    if (device_id) {
        if (0 != AudioUnitSetProperty(remoteio_instance,
                                      kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global, 0, &device_id,
                                      sizeof(device_id))) {
            DLOG("Failed to select requested device \"%s\"", device_name);
            goto error_destroy_remoteio;
        }
    }

    output_rate = get_hardware_output_rate(device_id);
    if (output_rate) {
        DLOG("Audio output rate: %d Hz", output_rate);
    } else {
        DLOG("Warning: Could not get output rate, assuming 44.1 kHz");
        output_rate = 44100;
    }

    AudioStreamBasicDescription format;
    mem_clear(&format, sizeof(format));
    format.mFormatID         = kAudioFormatLinearPCM;
    format.mFormatFlags      = kAudioFormatFlagsNativeEndian
                             | kAudioFormatFlagIsSignedInteger
                             | kAudioFormatFlagIsPacked;
    format.mBytesPerPacket   = 4;
    format.mBytesPerFrame    = 4;
    format.mFramesPerPacket  = 1;
    format.mBitsPerChannel   = 16;
    format.mChannelsPerFrame = 2;
    format.mSampleRate       = output_rate;
    if (0 != AudioUnitSetProperty(remoteio_instance,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0,
                                  &format, sizeof(format))) {
        DLOG("Failed to set output stream format");
        goto error_destroy_remoteio;
    }

    AURenderCallbackStruct callback;
    callback.inputProc       = &audio_render_callback;
    callback.inputProcRefCon = NULL;
    if (0 != AudioUnitSetProperty(remoteio_instance,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global, 0,
                                  &callback, sizeof(callback))) {
        DLOG("Failed to install audio render callback");
        goto error_destroy_remoteio;
    }

    /* Set up output buffers for the software mixer. */

    output_buffer_mem = mem_alloc((4*SOUND_BUFLEN) * SOUND_BUFFERS, 0, 0);
    if (!output_buffer_mem) {
        DLOG("No memory for output buffers (%d bytes)",
             (4*SOUND_BUFLEN) * SOUND_BUFFERS);
        goto error_destroy_remoteio;
    }
    for (int i = 0; i < SOUND_BUFFERS; i++) {
        output_buffers[i] = (char *)output_buffer_mem + (4*SOUND_BUFLEN)*i;
    }
    output_buffer_play_sem = sys_semaphore_create(0, SOUND_BUFFERS);
    if (UNLIKELY(!output_buffer_play_sem)) {
        DLOG("Failed to create buffer playback semaphore");
        goto error_free_output_buffer_mem;
    }
    output_buffer_fill_sem = sys_semaphore_create(SOUND_BUFFERS, SOUND_BUFFERS);
    if (UNLIKELY(!output_buffer_fill_sem)) {
        DLOG("Failed to create buffer fill semaphore");
        goto error_destroy_play_sem;
    }

    /* Start playback. */

    playback_buffer_index = 0;
    playback_buffer_offset = 0;
    if (0 != AudioUnitInitialize(remoteio_instance)) {
        DLOG("Failed to initialize audio playback");
        goto error_destroy_fill_sem;
    }
    if (0 != AudioOutputUnitStart(remoteio_instance)) {
        DLOG("Failed to start audio playback");
        goto error_uninit_remoteio;
    }

    mixer_thread_stop = 0;
    mixer_thread_id = thread_create_with_priority(
        MIXER_THREAD_PRIORITY, mixer_thread, NULL);
    if (!mixer_thread_id) {
        DLOG("Failed to start mixer thread");
        goto error_stop_remoteio;
    }

    /* All done. */

    return 1;


  error_stop_remoteio:
    AudioOutputUnitStop(remoteio_instance);
  error_uninit_remoteio:
    AudioUnitUninitialize(remoteio_instance);
  error_destroy_fill_sem:
    sys_semaphore_destroy(output_buffer_fill_sem);
    output_buffer_fill_sem = 0;
  error_destroy_play_sem:
    sys_semaphore_destroy(output_buffer_play_sem);
    output_buffer_play_sem = 0;
  error_free_output_buffer_mem:
    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;
  error_destroy_remoteio:
    AudioComponentInstanceDispose(remoteio_instance);
    remoteio_instance = 0;
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

    const float mixer_buffer_time =
        ((float)(SOUND_BUFLEN * (SOUND_BUFFERS-1) + SOUND_BUFLEN/2)
         / (float)output_rate);
    float io_latency;
    AudioUnitGetProperty(remoteio_instance, kAudioUnitProperty_Latency,
                         kAudioUnitScope_Output, 0, &io_latency,
                         (UInt32[]){sizeof(io_latency)});
    return mixer_buffer_time + io_latency;
}

/*-----------------------------------------------------------------------*/

void sys_sound_enable_headphone_disconnect_check(UNUSED int enable)
{
    /* Not supported. */
}

/*-----------------------------------------------------------------------*/

int sys_sound_check_headphone_disconnect(void)
{
    return 0;  // Not supported.
}

/*-----------------------------------------------------------------------*/

void sys_sound_acknowledge_headphone_disconnect(void)
{
    /* Not supported. */
}

/*-----------------------------------------------------------------------*/

void sys_sound_cleanup(void)
{
    if (mixer_thread_id) {
        mixer_thread_stop = 1;
        thread_wait(mixer_thread_id);
        mixer_thread_id = 0;
    }

    if (remoteio_instance) {
        AudioOutputUnitStop(remoteio_instance);
        AudioUnitUninitialize(remoteio_instance);
        AudioComponentInstanceDispose(remoteio_instance);
        remoteio_instance = 0;
    }

    sys_semaphore_destroy(output_buffer_fill_sem);
    output_buffer_fill_sem = 0;
    sys_semaphore_destroy(output_buffer_play_sem);
    output_buffer_play_sem = 0;
    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static AudioObjectID find_device(const char *device_name)
{
    OSStatus result;

    UInt32 devices_size = 0;
    result = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject,
        &(AudioObjectPropertyAddress){
            .mSelector = kAudioHardwarePropertyDevices,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMaster},
        0, NULL, &devices_size);
    if (UNLIKELY(result != 0) || UNLIKELY(devices_size == 0)) {
        DLOG("Failed to get device list size");
        goto error_return;
    }

    AudioObjectID *devices = mem_alloc(devices_size, 0, MEM_ALLOC_TEMP);
    const int num_devices = devices_size / sizeof(*devices);
    if (UNLIKELY(!devices)) {
        DLOG("Failed to allocate device list (%d bytes)", (int)devices_size);
        goto error_return;
    }
    result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &(AudioObjectPropertyAddress){
            .mSelector = kAudioHardwarePropertyDevices,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMaster},
        0, NULL, &devices_size, devices);
    if (UNLIKELY(result != 0) || UNLIKELY(devices_size == 0)) {
        DLOG("Failed to get device list");
        goto error_free_devices;
    }

    AudioObjectID device_id = 0;
    for (int i = 0; i < num_devices; i++) {
        CFStringRef cf_name = 0;
        result = AudioObjectGetPropertyData(
            devices[i],
            &(AudioObjectPropertyAddress){
                .mSelector = kAudioObjectPropertyName,
                .mScope = kAudioObjectPropertyScopeOutput,
                .mElement = kAudioObjectPropertyElementMaster},
            0, NULL, (UInt32[]){sizeof(cf_name)}, &cf_name);
        if (UNLIKELY(result != 0) || UNLIKELY(!cf_name)) {
            DLOG("Failed to get name for device %d", i);
            continue;
        }
        char name[1000];  // Large enough for any reasonable name.
        const int ok = CFStringGetCString(cf_name, name, sizeof(name),
                                          kCFStringEncodingUTF8);
        CFRelease(cf_name);
        if (UNLIKELY(!ok)) {
            DLOG("Failed to get name string for device %d", i);
            continue;
        }
        if (strcmp(name, device_name) == 0) {
            /* Make sure it has an output channel. */
            int num_channels = 0;
            AudioBufferList buffer_list;
            result = AudioObjectGetPropertyData(
                devices[i],
                &(AudioObjectPropertyAddress){
                    .mSelector = kAudioDevicePropertyStreamConfiguration,
                    .mScope = kAudioObjectPropertyScopeOutput,
                    .mElement = kAudioObjectPropertyElementMaster},
                0, NULL, (UInt32[]){sizeof(buffer_list)}, &buffer_list);
            if (UNLIKELY(result != 0)) {
                DLOG("Failed to get stream configuration for device \"%s\"",
                     device_name);
            } else {
                for (int j = 0; j < (int)buffer_list.mNumberBuffers; j++) {
                    num_channels += buffer_list.mBuffers[j].mNumberChannels;
                }
            }
            if (num_channels > 0) {
                device_id = devices[i];
            } else {
                DLOG("Found device \"%s\", but it has no outputs!",
                     device_name);
            }
            break;
        }
    }

    mem_free(devices);
    return device_id;

  error_free_devices:
    mem_free(devices);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int get_hardware_output_rate(AudioObjectID device_id)
{
    if (!device_id) {
        static const AudioObjectPropertyAddress prop_addr = {
            .mSelector = kAudioHardwarePropertyDefaultSystemOutputDevice,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = 0,
        };
        const OSStatus result = AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &prop_addr, 0, NULL,
            (UInt32[]){sizeof(device_id)}, &device_id);
        if (UNLIKELY(result != 0)) {
            DLOG("Failed to get default audio output device: %d", result);
            return 0;
        }
    }

    Float64 output_rate_float = 0;
    static const AudioObjectPropertyAddress prop_addr = {
        .mSelector = kAudioDevicePropertyNominalSampleRate,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = 0,
    };
    const OSStatus result = AudioObjectGetPropertyData(
        device_id, &prop_addr, 0, NULL,
        (UInt32[]){sizeof(output_rate_float)}, &output_rate_float);
    if (UNLIKELY(result != 0)) {
        DLOG("Failed to get audio output rate: %d", result);
        return 0;
    }
    if (!(output_rate_float >= 1 && output_rate_float <= 1e9)) {
        DLOG("Ignoring invalid sample rate from system: %.3f",
             output_rate_float);
        return 0;
    }
    return iround(output_rate_float);
}

/*-----------------------------------------------------------------------*/

static OSStatus audio_render_callback(UNUSED void *userdata,
                                      AudioUnitRenderActionFlags *flags,
                                      UNUSED const AudioTimeStamp *timestamp,
                                      UNUSED UInt32 bus, UInt32 num_frames,
                                      AudioBufferList *buffers)
{
    if (UNLIKELY(num_frames == 0)) {
        return 0;
    }

    if (UNLIKELY(buffers->mNumberBuffers != 1)) {
        static int warned = 0;
        if (!warned) {
            DLOG("Invalid buffer count %d", (int)buffers->mNumberBuffers);
            warned = 1;
        }
        goto fill_with_silence;
    }

    AudioBuffer *buffer = &buffers->mBuffers[0];
    char *data = (char *)buffer->mData;
    if (UNLIKELY(buffer->mNumberChannels != 2)
     || UNLIKELY(buffer->mDataByteSize != 4 * num_frames)) {
        static int warned = 0;
        if (!warned) {
            DLOG("Invalid buffer format: channels=%d size=%d (num_frames=%d)",
                 (int)buffer->mNumberChannels, (int)buffer->mDataByteSize,
                 (int)num_frames);
            warned = 1;
        }
        goto fill_with_silence;
    }

    /* Avoid blocking the audio device for an excessively long time in
     * case the mixer thread gets stuck (e.g. on a filesystem read).
     * This also serves as a cheap workaround for a deadlock with
     * sys_sound_cleanup() if we consume all data produced by the mixer
     * thread after the thread exits but before the RemoteIO instance is
     * destroyed; with no limit, the wait here would indefinitely block
     * AudioOutputUnitStop() because the mixer would no longer be
     * producing output and signaling the semaphore. */
    const float max_wait = (float)(num_frames*2) / (float)output_rate;
    for (int samples_left = num_frames; samples_left > 0; ) {
        if (playback_buffer_offset == 0) {
            static uint8_t warned = 0;
            if (sys_semaphore_wait(output_buffer_play_sem, max_wait)) {
                warned = 0;
            } else {
                if (!warned) {
                    warned = 1;
                    /* Suppress the warning if we're shutting down, since
                     * in that case the "problem" is just that we lost a
                     * race with the main thread. */
                    if (!mixer_thread_stop) {
                        DLOG("Warning: audio mixing thread running too"
                             " slowly, inserting silence");
                    }
                }
                memset(data, 0, samples_left * 4);
                samples_left = 0;
                break;
            }
        }
        const int samples_to_copy =
            ubound(SOUND_BUFLEN - playback_buffer_offset, samples_left);
        memcpy(data, ((char *)output_buffers[playback_buffer_index]
                      + (playback_buffer_offset * 4)), samples_to_copy * 4);
        data += samples_to_copy * 4;
        samples_left -= samples_to_copy;
        playback_buffer_offset += samples_to_copy;
        if (playback_buffer_offset == SOUND_BUFLEN) {
            playback_buffer_index =
                (playback_buffer_index + 1) % SOUND_BUFFERS;
            playback_buffer_offset = 0;
            sys_semaphore_signal(output_buffer_fill_sem);
        }
    }

    return 0;

  fill_with_silence:;
    for (unsigned int i = 0; i < buffers->mNumberBuffers; i++) {
        mem_clear(buffers->mBuffers[i].mData,
                  buffers->mBuffers[i].mDataByteSize);
    }
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    return 0;
}

/*-----------------------------------------------------------------------*/

static int mixer_thread(UNUSED void *userdata)
{
    int next_buffer_to_fill = 0;

    while (!mixer_thread_stop) {
        /* Wake up occasionally to detect mixer_thread_stop in case the
         * audio callback stops being called. */
        if (!sys_semaphore_wait(output_buffer_fill_sem, 0.1)) {
            continue;
        }

        sound_mixer_get_pcm(output_buffers[next_buffer_to_fill], SOUND_BUFLEN);
        sys_semaphore_signal(output_buffer_play_sem);
        next_buffer_to_fill = (next_buffer_to_fill + 1) % SOUND_BUFFERS;
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
