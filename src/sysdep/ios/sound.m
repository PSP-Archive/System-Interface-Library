/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/sound.m: iOS audio output interface.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/sound/mixer.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/sound.h"
#import "src/thread.h"
#import "src/time.h"

#import <AVFoundation/AVAudioSession.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>

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
/****************** Shared data (internal to iOS code) *******************/
/*************************************************************************/

/**
 * ios_ignore_audio_route_change_delay:  If nonzero, indicates the
 * time_now() timestamp until which audio route changes should be ignored.
 * This is a workaround for an iOS bug/misfeature (reported to Apple as bug
 * 9677380) which sends outdated route change events to the app when it
 * resumes from suspend.
 */
double ios_ignore_audio_route_change_until;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Current audio session category (IOS_SOUND_*). */
static iOSSoundCategory category = IOS_SOUND_BACKGROUND;

/* Remote I/O audio unit instance for playback. */
static AudioUnit remoteio_instance;

/* Output sampling rate used by the hardware. */
static int output_rate;

/* Flag: Has the audio session been initialized? */
static uint8_t session_initted;

/* Flag indicating whether to check for headphone disconnection. */
static uint8_t check_headphone_disconnect;

/* Flag indicating whether we have an unacknowledged headphone disconnect. */
static uint8_t got_headphone_disconnect;

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

/* Dummy object used to hook into the iOS 6+ audio notifications. */
@interface AudioNotificationHandler: NSObject
- (void)register_;
@end
static AudioNotificationHandler *ios6_notification_handler;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * is_ios_6:  Return whether the program is running on a device with iOS
 * 6.0 or later.  If the program is built to target only iOS 6.0 or later,
 * this will evaluate to a constant true value.
 *
 * [Return value]
 *     True if the device is running iOS 6.0 or later, false otherwise.
 */
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0
# define is_ios_6()  1
#else
# define is_ios_6()  ios_version_is_at_least("6.0")
#endif

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
 * handle_audio_interruption:  Handler for audio interruption events.
 *
 * [Parameters]
 *     begin: True if the event is the beginning of an audio interruption;
 *         false if the event is the end of an audio interruption.
 */
static void handle_audio_interruption(int begin);

/**
 * handle_headphone_disconnect:  Handler for headphone disconnect events.
 *
 * A headphone disconnect event is detected as a route change event where
 * the previous route included headphones and the new route does not.
 */
static void handle_headphone_disconnect(void);

/**
 * handle_reset_session:  Handler called when the audio session needs to
 * be reset due to loss of connection to the media server process.
 */
static void handle_reset_session(void);

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
/**
 * audio_interruption_callback_ios4:  Callback function called by iOS
 * 4.x/5.x when the audio session is interrupted or resumes from an
 * interruption.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 *     state: Interruption state code.
 */
static void audio_interruption_callback_ios4(void *userdata, UInt32 state);

/**
 * audio_route_change_callback_ios4:  Callback function called by iOS
 * 4.x/5.x when the current audio route changes (such as when headphones
 * are plugged in or unplugged).
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 *     property: Property ID (always kAudioSessionProperty_AudioRouteChange).
 *     size: Size of value, in bytes.
 *     value: New property value.
 */
static void audio_route_change_callback_ios4(void *userdata,
                                             AudioSessionPropertyID property,
                                             UInt32 size, const void *value);

/**
 * reset_session_callback_ios4:  Callback function called by iOS 4.x/5.x
 * when the audio session is interrupted or resumes from an interruption.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 *     property: Property ID (always kAudioSessionProperty_ServerDied).
 *     size: Size of value, in bytes.
 *     value: New property value.
 */
static void reset_session_callback_ios4(void *userdata,
                                        AudioSessionPropertyID property,
                                        UInt32 size, const void *value);
#endif

/**
 * start_audio_session:  Initialize the audio session at program startup
 * or after stopping the session on a media services reset.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int start_audio_session(void);

/**
 * start_audio_rendering:  Start audio output after initializing the audio
 * session.  The software mixer must have already been initialized.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int start_audio_rendering(void);

/**
 * stop_audio_rendering:  Stop audio rendering before shutting down the
 * audio session.  The software mixer state is not changed.
 */
static void stop_audio_rendering(void);

/**
 * stop_audio_session:  Shut down the audio session at program termination
 * or after a media services reset.
 */
static void stop_audio_session(void);

/**
 * do_set_category:  Set the audio session category, assuming the session
 * has already been initialized.   Implements ios_sound_set_category().
 *
 * [Parameters]
 *     new_category: New category to set (one of IOS_SOUND_*).
 * [Return value]
 *     True if the category was successfully set, false if not.
 */
static int do_set_category(iOSSoundCategory new_category);

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

int sys_sound_init(UNUSED const char *device_name)
{
    if (!start_audio_session()) {
        goto error_return;
    }

    output_buffer_mem = mem_alloc((4*SOUND_BUFLEN) * SOUND_BUFFERS, 0, 0);
    if (!output_buffer_mem) {
        DLOG("No memory for output buffers (%d bytes)",
             (4*SOUND_BUFLEN) * SOUND_BUFFERS);
        goto error_stop_audio_session;
    }
    for (int i = 0; i < SOUND_BUFFERS; i++) {
        output_buffers[i] = (char *)output_buffer_mem + (4*SOUND_BUFLEN)*i;
    }

    if (!start_audio_rendering()) {
        goto error_free_output_buffer_mem;
    }

    return 1;

  error_free_output_buffer_mem:
    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;
  error_stop_audio_session:
    stop_audio_session();
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
    float io_buffer_time;
    if (is_ios_6()) {
        io_buffer_time = ((AVAudioSession *)[AVAudioSession sharedInstance]).IOBufferDuration;
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        if (0 != AudioSessionGetProperty(
                     kAudioSessionProperty_CurrentHardwareIOBufferDuration,
                     (UInt32[]){sizeof(io_buffer_time)}, &io_buffer_time)) {
            DLOG("Could not get I/O buffer duration");
            io_buffer_time = 0;
        }
#endif
    }

    float actual_latency = mixer_buffer_time - io_buffer_time/2;
    if (is_ios_6()) {
        /* The iOS documentation doesn't say exactly what the outputLatency
         * value means, but since it seems to be small (e.g. 162 samples at
         * 44.1 kHz on an iPad 3 with iOS 6.1), we take it as the latency
         * between submitting a buffer to the OS and the first sample of
         * the buffer reaching the physical output device. */
        actual_latency +=
            ((AVAudioSession *)[AVAudioSession sharedInstance]).outputLatency;
    }
    return actual_latency;
}

/*-----------------------------------------------------------------------*/

void sys_sound_enable_headphone_disconnect_check(int enable)
{
    check_headphone_disconnect = (enable != 0);
}

/*-----------------------------------------------------------------------*/

int sys_sound_check_headphone_disconnect(void)
{
    return got_headphone_disconnect;
}

/*-----------------------------------------------------------------------*/

void sys_sound_acknowledge_headphone_disconnect(void)
{
    got_headphone_disconnect = 0;
}

/*-----------------------------------------------------------------------*/

void sys_sound_cleanup(void)
{
    stop_audio_rendering();

    mem_free(output_buffer_mem);
    output_buffer_mem = NULL;

    stop_audio_session();
}

/*-----------------------------------------------------------------------*/

/*
 * The remaining sys_sound_*() functions are defined in
 * src/sysdep/misc/sound-mixer.c.
 */

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

int ios_sound_set_category(iOSSoundCategory new_category)
{
    if (new_category == category) {
        return 1;  // No change.
    }

    if (!remoteio_instance) {
        /* Audio hasn't been started up yet, so just validate the category. */
        switch (new_category) {
          case IOS_SOUND_FOREGROUND:
          case IOS_SOUND_FOREGROUND_MIX:
          case IOS_SOUND_BACKGROUND:
            break;
          default:
            DLOG("Invalid category %d", new_category);
            return 0;
        }
    } else {
        if (!do_set_category(new_category)) {
            return 0;
        }
    }

    category = new_category;
    return 1;
}

/*************************************************************************/
/*********************** System callback routines ************************/
/*************************************************************************/

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

    for (int samples_left = num_frames; samples_left > 0; ) {
        if (playback_buffer_offset == 0) {
            sys_semaphore_wait(output_buffer_play_sem, -1);
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

    if (got_headphone_disconnect) {
        /* In case of a headphone disconnect, we process all the audio and
         * then silence it so that if client code is slow to respond to the
         * disconnect, audio events don't pile up behind the stopped mixer. */
        goto fill_with_silence;
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

static void handle_audio_interruption(int begin)
{
    if (begin) {
        if (remoteio_instance) {
            AudioOutputUnitStop(remoteio_instance);
        }
    } else {
        if (is_ios_6()) {
            NSError *error;
            if (![[AVAudioSession sharedInstance] setActive:YES error:&error]) {
                DLOG("setActive() failed: [%s/%ld] %s",
                     [error.domain UTF8String], (long)error.code,
                     [error.localizedDescription UTF8String]);
            }
        } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
            const OSStatus result = AudioSessionSetActive(1);
            if (result != kAudioSessionNoError) {
                DLOG("AudioSessionSetActive() failed: %d", (int)result);
            }
#endif
        }
        if (remoteio_instance) {
            AudioOutputUnitStart(remoteio_instance);
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_headphone_disconnect(void)
{
    if (ios_ignore_audio_route_change_until != 0) {
        if (time_now() < ios_ignore_audio_route_change_until) {
            return;
        } else {
            ios_ignore_audio_route_change_until = 0;
        }
    }

    if (check_headphone_disconnect) {
        got_headphone_disconnect = 1;
    }
}

/*-----------------------------------------------------------------------*/

static void handle_reset_session(void)
{
    stop_audio_rendering();
    stop_audio_session();
    session_initted = 0;

    if (UNLIKELY(!start_audio_session())) {
        DLOG("Failed to restart audio session");
        return;
    }
    if (UNLIKELY(!start_audio_rendering())) {
        DLOG("Failed to restart audio rendering");
        stop_audio_session();
        return;
    }
}

/*-----------------------------------------------------------------------*/

@implementation AudioNotificationHandler

- (void)call_interruption:(NSNotification *)notification {
    int type = [[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
    int begin;
    switch (type) {
        case AVAudioSessionInterruptionTypeBegan: begin = 1; break;
        case AVAudioSessionInterruptionTypeEnded: begin = 0; break;
        default: DLOG("Invalid type: %d", type); return;
    }
    handle_audio_interruption(begin);
}

- (void)call_route_change:(NSNotification *)notification {
    AVAudioSessionRouteDescription *old_route =
        [notification.userInfo valueForKey:AVAudioSessionRouteChangePreviousRouteKey];
    AVAudioSessionRouteDescription *new_route =
        ((AVAudioSession *)[AVAudioSession sharedInstance]).currentRoute;
    NSArray *old_route_array = old_route.outputs;
    NSArray *new_route_array = new_route.outputs;

    int old_has_headphones = 0, new_has_headphones = 0;
    for (int i = 0; i < (int)[old_route_array count]; i++) {
        AVAudioSessionPortDescription *route =
            [old_route_array objectAtIndex:i];
        if ([route.portType
                 compare:AVAudioSessionPortHeadphones] == NSOrderedSame) {
            old_has_headphones = 1;
            break;
        }
    }
    for (int i = 0; i < (int)[new_route_array count]; i++) {
        AVAudioSessionPortDescription *route =
            [new_route_array objectAtIndex:i];
        if ([route.portType
                 compare:AVAudioSessionPortHeadphones] == NSOrderedSame) {
            new_has_headphones = 1;
            break;
        }
    }

    if (old_has_headphones && !new_has_headphones) {
        handle_headphone_disconnect();
    }
}

- (void)call_reset_session:(NSNotification *) UNUSED notification {
    handle_reset_session();
}

- (void)register_ {
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call_interruption:)
        name:AVAudioSessionInterruptionNotification
        object:[AVAudioSession sharedInstance]];
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call_route_change:)
        name:AVAudioSessionRouteChangeNotification
        object:[AVAudioSession sharedInstance]];
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call_reset_session:)
        name:AVAudioSessionMediaServicesWereResetNotification
        object:[AVAudioSession sharedInstance]];
}

@end

/*-----------------------------------------------------------------------*/

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0

static void audio_interruption_callback_ios4(
    UNUSED void *userdata, UInt32 state)
{
    int begin;
    switch (state) {
        case kAudioSessionBeginInterruption: begin = 1; break;
        case kAudioSessionEndInterruption:   begin = 0; break;
        default: DLOG("Invalid state 0x%X", (int)state); return;
    }
    handle_audio_interruption(begin);
}

/*-----------------------------------------------------------------------*/

static void audio_route_change_callback_ios4(
    UNUSED void *userdata, AudioSessionPropertyID property,
    UNUSED UInt32 size, const void *value)
{
    ASSERT(property == kAudioSessionProperty_AudioRouteChange, return);

    CFDictionaryRef dict = (CFDictionaryRef)value;
    ASSERT(dict, return);

    CFDictionaryRef old_route = CFDictionaryGetValue(dict, kAudioSession_AudioRouteChangeKey_PreviousRouteDescription);
    ASSERT(old_route, return);
    CFDictionaryRef new_route = CFDictionaryGetValue(dict, kAudioSession_AudioRouteChangeKey_CurrentRouteDescription);
    ASSERT(new_route, return);
    CFArrayRef old_route_array = CFDictionaryGetValue(old_route, kAudioSession_AudioRouteKey_Outputs);
    ASSERT(old_route_array, return);
    CFArrayRef new_route_array = CFDictionaryGetValue(new_route, kAudioSession_AudioRouteKey_Outputs);
    ASSERT(new_route_array, return);

    int old_has_headphones = 0, new_has_headphones = 0;
    for (int i = 0; i < (int)CFArrayGetCount(old_route_array); i++) {
        CFDictionaryRef route = CFArrayGetValueAtIndex(old_route_array, i);
        ASSERT(route, return);
        CFStringRef type = CFDictionaryGetValue(route, kAudioSession_AudioRouteKey_Type);
        if (CFStringCompare(type, kAudioSessionOutputRoute_Headphones, 0) == kCFCompareEqualTo) {
            old_has_headphones = 1;
            break;
        }
    }
    for (int i = 0; i < (int)CFArrayGetCount(new_route_array); i++) {
        CFDictionaryRef route = CFArrayGetValueAtIndex(new_route_array, i);
        ASSERT(route, return);
        CFStringRef type = CFDictionaryGetValue(route, kAudioSession_AudioRouteKey_Type);
        if (CFStringCompare(type, kAudioSessionOutputRoute_Headphones, 0) == kCFCompareEqualTo) {
            new_has_headphones = 1;
            break;
        }
    }

    if (old_has_headphones && !new_has_headphones) {
        handle_headphone_disconnect();
    }
}

/*-----------------------------------------------------------------------*/

static void reset_session_callback_ios4(
    UNUSED void *userdata, AudioSessionPropertyID property,
    UInt32 size, const void *value)
{
    ASSERT(property == kAudioSessionProperty_ServerDied, return);
    ASSERT(size == 4, return);

    if (*(const uint32_t *)value) {
        handle_reset_session();
    }
}

#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0

/*************************************************************************/
/************************* Other local routines **************************/
/*************************************************************************/

static int start_audio_session(void)
{
    NSAutoreleasePool *pool;

    if (is_ios_6()) {
        pool = [[NSAutoreleasePool alloc] init];
    } else {
        pool = NULL;
    }

    /* Initialize the iOS audio session. */

    if (!session_initted) {
        if (is_ios_6()) {
            /* Nothing needed for iOS 6+. */
        } else {
            /* When targeting iOS 6+, is_ios_6() will ensure that this code
             * doesn't get compiled, but the compiler will still complain
             * about deprecation warnings so we explicitly #ifdef it out. */
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
            AudioSessionInitialize(NULL, NULL,
                                   audio_interruption_callback_ios4, NULL);
#endif
        }
        session_initted = 1;
    }

    /* Configure the audio session. */

    do_set_category(category);

    Float64 output_rate_float;
    if (is_ios_6()) {
        output_rate_float =
            ((AVAudioSession *)[AVAudioSession sharedInstance]).sampleRate;
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        output_rate_float = 0;
        AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate,
                                (UInt32[]){sizeof(output_rate_float)},
                                &output_rate_float);
#endif
    }
    output_rate = iround(output_rate_float);
    if (output_rate <= 0) {
        DLOG("WARNING: Could not get output rate, assuming 44.1kHz");
        output_rate = 44100;
    }

    Float32 io_buffer_duration = (Float32)SOUND_BUFLEN / (Float32)output_rate;
    if (is_ios_6()) {
        NSError *error;
        if (![[AVAudioSession sharedInstance]
                 setPreferredIOBufferDuration:io_buffer_duration
                 error:&error]) {
            DLOG("Could not set slice size: [%s/%ld] %s",
                 [error.domain UTF8String], (long)error.code,
                 [error.localizedDescription UTF8String]);
        } else {
            io_buffer_duration = ((AVAudioSession *)[AVAudioSession sharedInstance]).IOBufferDuration;
        }
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        if (0 != AudioSessionSetProperty(
                     kAudioSessionProperty_PreferredHardwareIOBufferDuration,
                     sizeof(io_buffer_duration), &io_buffer_duration)) {
            DLOG("Could not set slice size");
        } else {
            if (0 != AudioSessionGetProperty(
                         kAudioSessionProperty_CurrentHardwareIOBufferDuration,
                         (UInt32[]){sizeof(io_buffer_duration)},
                         &io_buffer_duration)) {
                DLOG("Could not get slice size");
            }
        }
#endif
    }
    const int slice_len = iroundf(io_buffer_duration * output_rate);
    if (slice_len != SOUND_BUFLEN) {
        DLOG("Note: requested slice size %d but got %d", SOUND_BUFLEN,
             slice_len);
    }

    /* Activate the audio session. */

    if (is_ios_6()) {
        NSError *error;
        if (![[AVAudioSession sharedInstance] setActive:YES error:&error]) {
            DLOG("setActive() failed: [%s/%ld] %s",
                 [error.domain UTF8String], (long)error.code,
                 [error.localizedDescription UTF8String]);
            goto error_clear_audio_session;
        }
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        const OSStatus result = AudioSessionSetActive(1);
        if (result != kAudioSessionNoError) {
            DLOG("AudioSessionSetActive() failed: %d", (int)result);
            goto error_clear_audio_session;
        }
#endif
    }

    /* Add a callback to catch audio route changes, such as headphones
     * being plugged in or unplugged.  (iPhone HIG: Audio playback should
     * pause when headphones are unplugged or wireless headphones go out
     * of range.)  We add the callback after setting the audio category
     * above to avoid an unnecessary trigger of the callback when the
     * category is set. */

    if (is_ios_6()) {
        ios6_notification_handler = [[AudioNotificationHandler alloc] init];
        [ios6_notification_handler register_];
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        if (0 != AudioSessionAddPropertyListener(
                     kAudioSessionProperty_AudioRouteChange,
                     audio_route_change_callback_ios4, NULL)) {
            DLOG("WARNING: Failed to add audio route change callback");
        }
        if (0 != AudioSessionAddPropertyListener(
                     kAudioSessionProperty_ServerDied,
                     reset_session_callback_ios4, NULL)) {
            DLOG("WARNING: Failed to add audio route change callback");
        }
#endif
    }

    if (pool) {
        [pool release];
        pool = NULL;
    }
    return 1;

  error_clear_audio_session:
    if (is_ios_6()) {
        [[AVAudioSession sharedInstance] setActive:NO error:nil];
    } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        AudioSessionSetActive(0);
#endif
    }
    if (pool) {
        [pool release];
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static int start_audio_rendering(void)
{
    /* Create semaphores for synchronization with the playback thread. */

    output_buffer_play_sem = sys_semaphore_create(0, SOUND_BUFFERS);
    if (UNLIKELY(!output_buffer_play_sem)) {
        DLOG("Failed to create buffer playback semaphore");
        goto error_return;
    }
    output_buffer_fill_sem = sys_semaphore_create(SOUND_BUFFERS, SOUND_BUFFERS);
    if (UNLIKELY(!output_buffer_fill_sem)) {
        DLOG("Failed to create buffer fill semaphore");
        goto error_destroy_play_sem;
    }

    /* Set up a Remote I/O audio unit for playback. */

    AudioComponentDescription remoteio_desc;
    mem_clear(&remoteio_desc, sizeof(remoteio_desc));
    remoteio_desc.componentType         = kAudioUnitType_Output;
    remoteio_desc.componentSubType      = kAudioUnitSubType_RemoteIO;
    remoteio_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    remoteio_desc.componentFlags        = 0;
    remoteio_desc.componentFlagsMask    = 0;
    AudioComponent remoteio_ref = AudioComponentFindNext(NULL, &remoteio_desc);
    if (0 != AudioComponentInstanceNew(remoteio_ref, &remoteio_instance)) {
        DLOG("Failed to create Remote I/O audio unit");
        goto error_destroy_fill_sem;
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

    /* Start playback. */

    playback_buffer_index = 0;
    playback_buffer_offset = 0;
    if (0 != AudioUnitInitialize(remoteio_instance)) {
        DLOG("Failed to initialize audio playback");
        goto error_destroy_remoteio;
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

    return 1;

  error_stop_remoteio:
    AudioOutputUnitStop(remoteio_instance);
  error_uninit_remoteio:
    AudioUnitUninitialize(remoteio_instance);
  error_destroy_remoteio:
    AudioComponentInstanceDispose(remoteio_instance);
    remoteio_instance = 0;
  error_destroy_fill_sem:
    sys_semaphore_destroy(output_buffer_fill_sem);
    output_buffer_fill_sem = 0;
  error_destroy_play_sem:
    sys_semaphore_destroy(output_buffer_play_sem);
    output_buffer_play_sem = 0;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void stop_audio_rendering(void)
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
}

/*-----------------------------------------------------------------------*/

static void stop_audio_session(void)
{
    if (session_initted) {
        if (is_ios_6()) {
            [[NSNotificationCenter defaultCenter]
                removeObserver:ios6_notification_handler];
            [ios6_notification_handler release];
            [[AVAudioSession sharedInstance] setActive:NO error:nil];
        } else {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
            AudioSessionSetActive(0);
#endif
        }
    }
}

/*-----------------------------------------------------------------------*/

static int do_set_category(iOSSoundCategory new_category)
{
    if (is_ios_6()) {

        NSString *category_str;
        AVAudioSessionCategoryOptions options;
        switch (new_category) {
          case IOS_SOUND_FOREGROUND:
            category_str = AVAudioSessionCategoryPlayback;
            options = 0;
            break;
          case IOS_SOUND_FOREGROUND_MIX:
            category_str = AVAudioSessionCategoryPlayback;
            options = AVAudioSessionCategoryOptionMixWithOthers;
            break;
          case IOS_SOUND_BACKGROUND:
            /* The iPhone HIG suggests that games should use Ambient mode
             * if other audio is playing when the app is started, otherwise
             * Solo Ambient.  However, this has the non-obvious result that
             * the program's future behavior will depend on whether audio
             * happened to be playing at the moment the category was set;
             * for example, if the user had nothing playing when the app
             * was first launched, subsequently suspending the app and
             * playing something in Music would cause the Music audio to be
             * interrupted when the app was resumed.  Users are unlikely to
             * recognize the true cause of the behavior and are much more
             * likely to interpret it as a bug in the app, so we never use
             * Solo Ambient mode. */
            category_str = AVAudioSessionCategoryAmbient;
            options = 0;
            break;
          default:
            DLOG("Invalid category %d", new_category);
            return 0;
        }
        NSError *error;
        if (![[AVAudioSession sharedInstance] setCategory:category_str
                                              withOptions:options
                                              error:&error]) {
            DLOG("WARNING: Failed to set audio session category: [%s/%ld] %s",
                 [error.domain UTF8String], (long)error.code,
                 [error.localizedDescription UTF8String]);
            return 0;
        }

    } else {  // iOS < 6.0

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0
        uint32_t category_flag;
        uint32_t override_mix;
        switch (new_category) {
          case IOS_SOUND_FOREGROUND:
            category_flag = kAudioSessionCategory_MediaPlayback;
            override_mix = 0;
            break;
          case IOS_SOUND_FOREGROUND_MIX:
            category_flag = kAudioSessionCategory_MediaPlayback;
            override_mix = 1;
            break;
          case IOS_SOUND_BACKGROUND:
            category_flag = kAudioSessionCategory_AmbientSound;
            override_mix = 0;
            break;
          default:
            DLOG("Invalid category %d", new_category);
            return 0;
        }
        OSStatus result;
        result = AudioSessionSetProperty(
            kAudioSessionProperty_AudioCategory,
            sizeof(category_flag), &category_flag);
        if (result != kAudioSessionNoError) {
            DLOG("Failed to set audio session category: %d", (int)result);
            return 0;
        }
        result = AudioSessionSetProperty(
            kAudioSessionProperty_OverrideCategoryMixWithOthers,
            sizeof(override_mix), &override_mix);
        if (result != kAudioSessionNoError) {
            DLOG("WARNING: Failed to set audio mix flag: %d", (int)result);
            /* Ignore failure since we did successfully set the session. */
        }
#endif

    }  // if (is_ios_6())

    return 1;
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
