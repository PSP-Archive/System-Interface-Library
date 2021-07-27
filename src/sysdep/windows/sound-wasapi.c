/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/sound-wasapi.c: Windows audio output implementation
 * using the Windows Audio Session API (WASAPI).
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/thread.h"
#include "src/time.h"

#define COBJMACROS
/* We have to move the internal.h #include down here because apparently
 * some base Windows header #includes <propsys.h> on its own, and we need
 * to #define COBJMACROS before that. */
#include "src/sysdep/windows/internal.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <propsys.h>

/* These Windows 10-specific constants are currently (as of
 * mingw-runtime-5.0.3) missing from the MinGW headers. */
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
# define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM  0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
# define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY  0x08000000
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Various GUIDs used by the WASAPI interface.  We define these ourselves
 * to avoid linking with libraries not available on Windows XP. */
static const CLSID local_CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const IID local_IID_IAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}};
static const IID local_IID_IAudioClockAdjustment = {0xF6E4C0A0, 0x46D9, 0x4FB8, {0xBE,0x21,0x57,0xA3,0xEF,0x2B,0x62,0x6C}};
static const IID local_IID_IAudioRenderClient = {0xF294ACFC, 0x3146, 0x4483, {0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}};
static const IID local_IID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static const IID local_IID_IMMEndpoint = {0x1BE09788, 0x6894, 0x4089, {0x85,0x86,0x9A,0x2A,0x6C,0x26,0x5A,0xC5}};
static const IID local_IID_IMMNotificationClient = {0x7991EEC9, 0x7E89, 0x4D85, {0x83,0x90,0x6C,0x70,0x3C,0xEC,0x60,0xC0}};
static const GUID local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}};
static const GUID local_KSDATAFORMAT_SUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, {0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}};
static const PROPERTYKEY local_PKEY_Device_FriendlyName = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}}, 14};
#define CLSID_MMDeviceEnumerator  local_CLSID_MMDeviceEnumerator
#define IID_IAudioClient  local_IID_IAudioClient
#define IID_IAudioClockAdjustment  local_IID_IAudioClockAdjustment
#define IID_IAudioRenderClient  local_IID_IAudioRenderClient
#define IID_IMMDeviceEnumerator  local_IID_IMMDeviceEnumerator
#define IID_IMMEndpoint  local_IID_IMMEndpoint
#define IID_IMMNotificationClient  local_IID_IMMNotificationClient
#undef KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
#define KSDATAFORMAT_SUBTYPE_IEEE_FLOAT  local_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
#undef KSDATAFORMAT_SUBTYPE_PCM
#define KSDATAFORMAT_SUBTYPE_PCM  local_KSDATAFORMAT_SUBTYPE_PCM
#define PKEY_Device_FriendlyName  local_PKEY_Device_FriendlyName

/* Device role to use with WASAPI functions. */
#define WASAPI_ROLE  eConsole

/* Active device list. */
typedef struct DeviceInfo DeviceInfo;
struct DeviceInfo {
    DeviceInfo *next;
    uint16_t *id;
    char *name;  // "Friendly name" in Windows SDK terminology.
};
static DeviceInfo *device_list;
/* ID of default output device. */
static uint16_t *default_device_id;
/* Mutex protecting device_list and default_device_id. */
static SysMutexID device_list_mutex;

/* Has the default device changed since the last check?  (This is LONG so
 * we can use InterlockedExchange() on it.) */
static LONG default_device_changed;

/* ID of chosen output device, or NULL if using the default device.  (If
 * using the default device, we'll close and reopen on default device
 * change.) */
static uint16_t *chosen_device_id;

/* Audio device enumerator handle. */
static IMMDeviceEnumerator *enumerator;
/* IAudioClient instance for the current device. */
static IAudioClient *client;
/* IAudioRenderClient instance for the current device. */
static IAudioRenderClient *render_client;

/* Audio device sampling rate. */
static int sound_rate;
/* Number of channels per audio frame required by the audio device. */
static int sound_channels;
/* Number of audio frames in the WASAPI device buffer. */
static int wasapi_buffer_len;
/* Device-side audio data format. */
static enum {FORMAT_S16 = 1, FORMAT_S32, FORMAT_F32} wasapi_format;
/* Base device latency, in seconds. */
static float base_latency;

/* Temporary buffer for receiving data from the software mixer. */
static int16_t *mixer_buffer;
/* Number of audio frames in the mixer buffer. */
static int mixer_buffer_len;

/* Current buffer being filled by the playback thread, or NULL if none. */
static BYTE *current_buffer;

/* Event object used to wait for buffer playback completion. */
static HANDLE completion_event;

/* Is the device buffer known to be empty?  (Used to suppress extraneous
 * "buffer empty" warnings.) */
static uint8_t device_buffer_empty;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * enumerate_devices:  Enumerate all WASAPI audio devices.
 */
static void enumerate_devices(void);

/**
 * add_device:  Add a WASAPI device to the list of active devices.
 *
 * [Parameters]
 *     device_id: Device ID string.
 *     device: Device instance.
 */
static void add_device(LPCWSTR device_id, IMMDevice *device);

/**
 * remove_device:  Remove a WASAPI device from the list of active
 * devices.
 *
 * [Parameters]
 *     device_id: Device ID string.
 */
static void remove_device(LPCWSTR device_id);

/**
 * set_default_device:  Set the default WASAPI output device.
 *
 * [Parameters]
 *     device_id: Device ID string.
 */
static void set_default_device(LPCWSTR device_id);

/**
 * open_device:  Open the given device and start it in playback (render)
 * mode.
 *
 * If sound_rate is zero (as on initialization), it will be set to the
 * native rate of the audio device.  Otherwise, if the device's rate
 * differs from sound_rate, SIL will request resampling from WASAPI
 * (requires Windows 7 or better).
 *
 * The device instance is assumed to have been referenced by the caller;
 * this function will automatically release it regardless of success or
 * failure.
 *
 * [Parameters]
 *     device: Device to open.  Will be Release()d by this function.
 * [Return value]
 *     True on success, false on error.
 */
static int open_device(IMMDevice *device);

/**
 * close_device:  Close the currently open device.
 */
static void close_device(void);

/*************************************************************************/
/********************** WASAPI notification handler **********************/
/*************************************************************************/

typedef struct SILNotificationClient SILNotificationClient;
struct SILNotificationClient {
    const IMMNotificationClientVtbl *vtbl;
    LONG refcount;
};

/*-----------------------------------------------------------------------*/

/* IUnknown methods. */

static ULONG STDMETHODCALLTYPE snc_AddRef(IMMNotificationClient *this)
{
    return InterlockedExchangeAdd(
        &((SILNotificationClient *)this)->refcount, 1) + 1;
}

static ULONG STDMETHODCALLTYPE snc_Release(IMMNotificationClient *this)
{
    ASSERT(((SILNotificationClient *)this)->refcount > 0, return 0);
    /* This object is statically declared, so we don't need to free it
     * when the reference count hits zero. */
    return InterlockedExchangeAdd(
        &((SILNotificationClient *)this)->refcount, -1) - 1;
}

static HRESULT STDMETHODCALLTYPE snc_QueryInterface(
    IMMNotificationClient *this, REFIID iid, void **ppv)
{
    if (memcmp(iid, &IID_IUnknown, sizeof(*iid)) == 0
     || memcmp(iid, &IID_IMMNotificationClient, sizeof(*iid)) == 0) {
        *ppv = this;
        snc_AddRef(this);
        return S_OK;
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

/*-----------------------------------------------------------------------*/

/* IMMNotificationClient methods. */

static HRESULT STDMETHODCALLTYPE snc_OnDefaultDeviceChanged(
    UNUSED IMMNotificationClient *this, EDataFlow flow, ERole role,
    LPCWSTR pwstrDefaultDevice)
{
    if (role == WASAPI_ROLE && (flow == eRender || flow == eAll)) {
        IMMDevice *device;
        if (SUCCEEDED(IMMDeviceEnumerator_GetDevice(
                          enumerator, pwstrDefaultDevice, &device))) {
            set_default_device(pwstrDefaultDevice);
        }
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE snc_OnDeviceAdded(
    UNUSED IMMNotificationClient *this, UNUSED LPCWSTR pwstrDeviceId)
{
    /* We don't care about devices being added to or removed from the
     * system; we care whether they are active or not. */
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE snc_OnDeviceRemoved(
    UNUSED IMMNotificationClient *this, UNUSED LPCWSTR pwstrDeviceId)
{
    return S_OK;  // As for OnDeviceAdded().
}

static HRESULT STDMETHODCALLTYPE snc_OnDeviceStateChanged(
    UNUSED IMMNotificationClient *this, LPCWSTR pwstrDeviceId,
    DWORD dwNewState)
{
    IMMDevice *device;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDevice(
                      enumerator, pwstrDeviceId, &device))) {
        IMMEndpoint *endpoint;
        if (SUCCEEDED(IMMDevice_QueryInterface(
                          device, &IID_IMMEndpoint, (void **)&endpoint))) {
            EDataFlow flow;
            if (SUCCEEDED(IMMEndpoint_GetDataFlow(endpoint, &flow))
             && flow == eRender) {
                if (dwNewState == DEVICE_STATE_ACTIVE) {
                    add_device(pwstrDeviceId, device);
                } else {
                    remove_device(pwstrDeviceId);
                }
            }
        }
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE snc_OnPropertyValueChanged(
    UNUSED IMMNotificationClient *this, UNUSED LPCWSTR pwstrDeviceId,
    UNUSED const PROPERTYKEY key)
{
    /* We don't need to worry about any property changes. */
    return S_OK;
}

/*-----------------------------------------------------------------------*/

static SILNotificationClient notification_client = {
    .vtbl = &(const IMMNotificationClientVtbl){
        .QueryInterface = snc_QueryInterface,
        .AddRef = snc_AddRef,
        .Release = snc_Release,
        .OnDefaultDeviceChanged = snc_OnDefaultDeviceChanged,
        .OnDeviceAdded = snc_OnDeviceAdded,
        .OnDeviceRemoved = snc_OnDeviceRemoved,
        .OnDeviceStateChanged = snc_OnDeviceStateChanged,
        .OnPropertyValueChanged = snc_OnPropertyValueChanged,
    },
    .refcount = 0,
};

/*************************************************************************/
/*********************** Driver interface routines ***********************/
/*************************************************************************/

int windows_wasapi_init(void)
{
    if (enumerator) {
        return 1;
    }

    HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (UNLIKELY(FAILED(result))) {
        DLOG("CoInitializeEx(COINIT_APARTMENTTHREADED) failed: %s",
             windows_strerror(result));
        return 0;
    }

    result = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
        &IID_IMMDeviceEnumerator, (void *)&enumerator);
    if (FAILED(result)) {
        DLOG("CoCreateInstance(IMMDeviceEnumerator) failed: %s",
             windows_strerror(result));
        CoUninitialize();
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static int wasapi_open(const char *device_name)
{
    /* Create synchronization event object. */
    completion_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (UNLIKELY(!completion_event)) {
        DLOG("Failed to create completion event object: %s",
             windows_strerror(GetLastError()));
        goto error_return;
    }

    /* Initialize the active device list. */
    device_list_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!device_list_mutex)) {
        DLOG("Failed to create WASAPI device list mutex");
        goto error_destroy_completion_event;
    }

    /* Look up current audio device state. */
    enumerate_devices();
    IMMDevice *default_device;
    HRESULT result = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        enumerator, eRender, WASAPI_ROLE, &default_device);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to get default audio device: %s",
             windows_strerror(result));
        goto error_destroy_device_list_mutex;
    }
    LPWSTR default_id;
    result = IMMDevice_GetId(default_device, &default_id);
    if (UNLIKELY(FAILED(result))) {
        DLOG("IMMDevice::GetId() failed: %s", windows_strerror(result));
        IMMDevice_Release(default_device);
        goto error_free_device_list;
    }
    default_device_id = strdup_16(default_id);
    CoTaskMemFree(default_id);
    if (UNLIKELY(!default_device_id)) {
        DLOG("No memory for copy of default device ID");
        IMMDevice_Release(default_device);
        goto error_free_device_list;
    }

    /* Find the requested audio device. */
    IMMDevice *device = NULL;
    chosen_device_id = NULL;
    if (*device_name) {
        for (DeviceInfo *info = device_list; info; info = info->next) {
            if (info->name && strcmp(device_name, info->name) == 0) {
                chosen_device_id = strdup_16(info->id);
                if (UNLIKELY(!chosen_device_id)) {
                    DLOG("No memory for copy of device ID");
                    goto error_free_default_device_id;
                }
                result = IMMDeviceEnumerator_GetDevice(
                    enumerator, info->id, &device);
                if (UNLIKELY(FAILED(result))) {
                    DLOG("Failed to look up device %s: %s", info->name,
                         windows_strerror(result));
                    mem_free(chosen_device_id);
                    chosen_device_id = NULL;
                }
                break;
            }
        }
        if (chosen_device_id) {
            IMMDevice_Release(default_device);
        } else {
            DLOG("Requested device (%s) not found, using default"
                 " device instead", device_name);
        }
    }
    if (!device) {
        device = default_device;
        /* Make sure it's actually in the active device list. */
        device_name = NULL;
        for (DeviceInfo *info = device_list; info; info = info->next) {
            if (strcmp_16(default_device_id, info->id) == 0) {
                device_name = info->name;
                break;
            }
        }
        if (UNLIKELY(!device_name)) {
            DLOG("Default audio device not found in device list");
            IMMDevice_Release(default_device);
            goto error_free_default_device_id;
        }
    }

    /* Register to receive future device change events.  We do this here
     * to minimize the period during which we could miss events while
     * also not colliding on the device_list lock (which could potentially
     * lead to a deadlock within the API functions). */
    default_device_changed = 0;
    result = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(
        enumerator, (IMMNotificationClient *)&notification_client);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to register WASAPI notification callback (audio"
             " device changes will not be detected): %s",
             windows_strerror(result));
    }

    /* Open the selected device. */
    if (!open_device(device)) {
        goto error_free_default_device_id;
    }

    return sound_rate;

  error_free_default_device_id:
    mem_free(default_device_id);
  error_free_device_list:
    for (DeviceInfo *info = device_list, *next; info; info = next) {
        next = info->next;
        mem_free(info->id);
        mem_free(info->name);
        mem_free(info);
    }
    device_list = NULL;
  error_destroy_device_list_mutex:
    sys_mutex_destroy(device_list_mutex);
    device_list_mutex = 0;
  error_destroy_completion_event:
    CloseHandle(completion_event);
    completion_event = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void wasapi_close(void)
{
    close_device();

    IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(
        enumerator, (IMMNotificationClient *)&notification_client);
    IMMDeviceEnumerator_Release(enumerator);
    enumerator = NULL;
    CoUninitialize();

    mem_free(chosen_device_id);
    mem_free(default_device_id);
    for (DeviceInfo *info = device_list, *next; info; info = next) {
        next = info->next;
        mem_free(info->id);
        mem_free(info->name);
        mem_free(info);
    }
    device_list = NULL;
    sys_mutex_destroy(device_list_mutex);
    device_list_mutex = 0;
}

/*-----------------------------------------------------------------------*/

static float wasapi_get_latency(void)
{
    return base_latency + ((float)mixer_buffer_len / (float)sound_rate);
}

/*-----------------------------------------------------------------------*/

static void wasapi_set_latency(UNUSED float latency)
{
    // FIXME: not yet implemented
}

/*-----------------------------------------------------------------------*/

static int wasapi_get_buffer(float timeout, int16_t **buffer_ret,
                             int *size_ret)
{
    PRECOND(!current_buffer, return mixer_buffer_len);
    PRECOND(!client || render_client, return -1);

    if (InterlockedExchange(&default_device_changed, 0)) {
        if (!chosen_device_id) {
            DLOG("Default device changed, reopening...");
            IMMDevice *device;
            uint16_t *device_id;
            sys_mutex_lock(device_list_mutex, -1);
            device_id = strdup_16(default_device_id);
            sys_mutex_unlock(device_list_mutex);
            if (UNLIKELY(!device_id)) {
                DLOG("Out of memory copying device ID");
            } else {
                HRESULT result = IMMDeviceEnumerator_GetDevice(
                    enumerator, device_id, &device);
                if (UNLIKELY(FAILED(result))) {
                    DLOG("Failed to look up new default device: %s",
                         windows_strerror(result));
                } else {
                    if (client) {
                        close_device();
                    }
                    if (open_device(device)) {
                        DLOG("Reopen successful");
                        // FIXME: watch out for rate changes
                    } else {
                        DLOG("Failed to open new default device");
                    }
                }
            }
        }
    }

    if (!client) {
        return -1;
    }

    const int target_fill = wasapi_buffer_len - mixer_buffer_len;
    UINT32 fill;
    HRESULT result = IAudioClient_GetCurrentPadding(client, &fill);
    while (SUCCEEDED(result) && (int)fill > target_fill) {
        const double wait_start = time_now();
        const DWORD wait_result =
            WaitForSingleObject(completion_event, iceilf(timeout*1000));
        const double wait_end = time_now();
        if (wait_result == WAIT_TIMEOUT) {
            DLOG("Completion event wait timed out, retrying");
            return 0;
        } else if (wait_result != WAIT_OBJECT_0) {
            DLOG("Completion event wait failed: %s",
                 result==WAIT_ABANDONED ? "Wait abandoned"
                                        : windows_strerror(GetLastError()));
            return -1;
        }
        timeout -= wait_end - wait_start;
        if (timeout <= 0) {
            return 0;
        }
        result = IAudioClient_GetCurrentPadding(client, &fill);
    }
    if (FAILED(result)) {
        DLOG("Failed to get buffer fill level: %s", windows_strerror(result));
        if (result == AUDCLNT_E_DEVICE_INVALIDATED) {
            /* The device may have been removed, but it may also have just
             * had a configuration change which we can adapt to.  Try to
             * reopen it before giving up. */
            goto revalidate;
        } else {
            return -1;
        }
    }
    if (fill > 0) {
        device_buffer_empty = 0;
    } else if (!device_buffer_empty) {
        DLOG("Warning: device buffer empty, thread may be running too slowly");
        device_buffer_empty = 1;
    }

    result = IAudioRenderClient_GetBuffer(
        render_client, mixer_buffer_len, &current_buffer);
    if (FAILED(result)) {
        DLOG("Failed to get buffer for %d samples: %s", mixer_buffer_len,
             windows_strerror(result));
        if (result == AUDCLNT_E_DEVICE_INVALIDATED) {
            goto revalidate;
        } else {
            return -1;
        }
    }

    *buffer_ret = mixer_buffer;
    *size_ret = mixer_buffer_len;
    return 1;

  revalidate:
    DLOG("Attempting to revalidate device");
    close_device();
    IMMDevice *device;
    if (chosen_device_id) {
        result = IMMDeviceEnumerator_GetDevice(enumerator, chosen_device_id,
                                               &device);
    } else {
        uint16_t *device_id;
        sys_mutex_lock(device_list_mutex, -1);
        device_id = strdup_16(default_device_id);
        sys_mutex_unlock(device_list_mutex);
        if (UNLIKELY(!device_id)) {
            DLOG("Out of memory copying device ID");
            return -1;
        }
        result = IMMDeviceEnumerator_GetDevice(enumerator, device_id, &device);
        mem_free(device_id);
    }
    if (SUCCEEDED(result)) {
        if (open_device(device)) {
            return wasapi_get_buffer(timeout, buffer_ret, size_ret);
        } else {
            DLOG("Failed to reopen device for revalidation: %s",
                 windows_strerror(result));
        }
    } else {
        DLOG("Failed to look up device for revalidation: %s",
             windows_strerror(result));
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

static void wasapi_submit_buffer(void)
{
    PRECOND(current_buffer, return);

    switch (wasapi_format) {
      case FORMAT_S16: {
        int16_t *out = (int16_t *)current_buffer;
        if (sound_channels == 2) {
            memcpy(current_buffer, mixer_buffer, mixer_buffer_len * 4);
        } else if (sound_channels == 1) {
            for (int i = 0; i < mixer_buffer_len; i++) {
                out[i] = (int16_t)(((int32_t)mixer_buffer[i*2+0]
                                    + (int32_t)mixer_buffer[i*2+1]) / 2);
            }
        } else {
            for (int i = 0; i < mixer_buffer_len; i++, out += sound_channels) {
                out[0] = mixer_buffer[i*2+0];
                out[1] = mixer_buffer[i*2+1];
            }
        }
        break;
      }  // case FORMAT_S16

      case FORMAT_S32: {
        int32_t *out = (int32_t *)current_buffer;
        if (sound_channels == 2) {
            for (int i = 0; i < mixer_buffer_len * 2; i++) {
                out[i] = (int32_t)mixer_buffer[i] << 16;
            }
        } else if (sound_channels == 1) {
            for (int i = 0; i < mixer_buffer_len; i++) {
                out[i] = ((int32_t)mixer_buffer[i*2+0]
                          + (int32_t)mixer_buffer[i*2+1]) << (16-1);
            }
        } else {
            for (int i = 0; i < mixer_buffer_len; i++, out += sound_channels) {
                out[0] = (int32_t)mixer_buffer[i*2+0] << 16;
                out[1] = (int32_t)mixer_buffer[i*2+1] << 16;
            }
        }
        break;
      }  // case FORMAT_S32

      case FORMAT_F32: {
        float *out = (float *)current_buffer;
        if (sound_channels == 2) {
            for (int i = 0; i < mixer_buffer_len * 2; i++) {
                out[i] = (float)mixer_buffer[i] * (1/32768.0f);
            }
        } else if (sound_channels == 1) {
            for (int i = 0; i < mixer_buffer_len; i++) {
                out[i] = (float)((int32_t)mixer_buffer[i*2+0]
                                 + (int32_t)mixer_buffer[i*2+1]) * (1/65536.0f);
            }
        } else {
            for (int i = 0; i < mixer_buffer_len; i++, out += sound_channels) {
                out[0] = (float)mixer_buffer[i*2+0] * (1/32768.0f);
                out[1] = (float)mixer_buffer[i*2+1] * (1/32768.0f);
            }
        }
        break;
      }  // case FORMAT_F32
    }

    HRESULT result =
        IAudioRenderClient_ReleaseBuffer(render_client, mixer_buffer_len, 0);
    current_buffer = NULL;
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to write audio: %s", windows_strerror(result));
    }
}

/*-----------------------------------------------------------------------*/

AudioDriver windows_wasapi_driver = {
    .open = wasapi_open,
    .close = wasapi_close,
    .get_latency = wasapi_get_latency,
    .set_latency = wasapi_set_latency,
    .get_buffer = wasapi_get_buffer,
    .submit_buffer = wasapi_submit_buffer,
};

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void enumerate_devices(void)
{
    IMMDeviceCollection *collection;

    HRESULT result = IMMDeviceEnumerator_EnumAudioEndpoints(
        enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
    if (UNLIKELY(FAILED(result))) {
        DLOG("IMMDeviceEnumerator::EnumAudioEndpoints() failed: %s",
             windows_strerror(result));
        return;
    }

    UINT total;
    result = IMMDeviceCollection_GetCount(collection, &total);
    if (UNLIKELY(FAILED(result))) {
        DLOG("IMMDeviceCollection::GetCount() failed: %s",
             windows_strerror(result));
    } else {
        for (unsigned int i = 0; i < total; i++) {
            IMMDevice *device;
            result = IMMDeviceCollection_Item(collection, i, &device);
            if (UNLIKELY(FAILED(result))) {
                DLOG("IMMDeviceCollection::Item(%d) failed: %s", i,
                     windows_strerror(result));
            } else {
                LPWSTR device_id;
                result = IMMDevice_GetId(device, &device_id);
                if (UNLIKELY(FAILED(result))) {
                    DLOG("IMMDevice::GetId() failed: %s",
                         windows_strerror(result));
                } else {
                    add_device(device_id, device);
                    CoTaskMemFree(device_id);
                }
                IMMDevice_Release(device);
            }
        }
    }

    IMMDeviceCollection_Release(collection);
}

/*-----------------------------------------------------------------------*/

static void add_device(LPCWSTR device_id, IMMDevice *device)
{
    DeviceInfo *info = mem_alloc(sizeof(*info), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!info)) {
        DLOG("No memory for new WASAPI device");
        return;
    }
    info->next = NULL;
    info->id = strdup_16(device_id);
    if (UNLIKELY(!info->id)) {
        DLOG("No memory for WASAPI device ID copy");
        mem_free(info);
        return;
    }

    info->name = NULL;
    IPropertyStore *props;
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(device, STGM_READ, &props))) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName,
                                              &pv))) {
            info->name = strdup_16to8(pv.pwszVal);
            if (UNLIKELY(!info->name)) {
                DLOG("No memory for copy of friendly name");
                mem_free(info->id);
                mem_free(info);
                return;
            }
        }
        PropVariantClear(&pv);
        IPropertyStore_Release(props);
    }
    if (UNLIKELY(!info->name)) {
#ifdef DEBUG
        char *id_utf8 = strdup_16to8(device_id);
        DLOG("Unable to get friendly name for audio device %s", id_utf8);
        mem_free(id_utf8);
#endif
        info->name = mem_strdup("", 0);
        if (UNLIKELY(!info->name)) {
            DLOG("No memory for copy of empty name");
            mem_free(info->id);
            mem_free(info);
            return;
        }
    }

#ifdef DEBUG
    char *id_utf8 = strdup_16to8(device_id);
    DLOG("Audio output device added: %s (%s)", id_utf8, info->name);
    mem_free(id_utf8);
#endif

    sys_mutex_lock(device_list_mutex, -1);
    info->next = device_list;
    device_list = info;
    sys_mutex_unlock(device_list_mutex);
}

/*-----------------------------------------------------------------------*/

static void remove_device(LPCWSTR device_id)
{
    sys_mutex_lock(device_list_mutex, -1);
    for (DeviceInfo *info = device_list, **prev = &device_list;
         info; prev = &info->next, info = info->next)
    {
        if (strcmp_16(info->id, device_id) == 0) {
#ifdef DEBUG
            char *id_utf8 = strdup_16to8(device_id);
            DLOG("Audio output device removed: %s (%s)", id_utf8, info->name);
            mem_free(id_utf8);
#endif
            *prev = info->next;
            mem_free(info);
            break;
        }
    }
    sys_mutex_unlock(device_list_mutex);
}

/*-----------------------------------------------------------------------*/

static void set_default_device(LPCWSTR device_id)
{
    uint16_t *device_id_copy = strdup_16(device_id);
    if (UNLIKELY(!device_id_copy)) {
        DLOG("No memory for copy of device ID");
    }

#ifdef DEBUG
    sys_mutex_lock(device_list_mutex, -1);
    char *old_utf8 = strdup_16to8(default_device_id);
    sys_mutex_unlock(device_list_mutex);
    char *new_utf8 = strdup_16to8(device_id);
    DLOG("Default audio output device changed: %s -> %s", old_utf8, new_utf8);
    mem_free(new_utf8);
    mem_free(old_utf8);
#endif

    sys_mutex_lock(device_list_mutex, -1);
    const int changed =
        !default_device_id || strcmp_16(default_device_id, device_id) != 0;
    mem_free(default_device_id);
    default_device_id = device_id_copy;
    sys_mutex_unlock(device_list_mutex);

    /* Writes to 32-bit variables are atomic on all current Windows
     * platforms, but use InterlockedExchange() anyway for parallelism. */
    if (changed) {
        InterlockedExchange(&default_device_changed, 1);
    }
}

/*-----------------------------------------------------------------------*/

static int open_device(IMMDevice *device)
{
    PRECOND(!client, return 0);

#ifdef DEBUG
    char *device_name = NULL;
    IPropertyStore *props;
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(device, STGM_READ, &props))) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName,
                                              &pv))) {
            device_name = strdup_16to8(pv.pwszVal);
        }
        PropVariantClear(&pv);
        IPropertyStore_Release(props);
    }
#endif

    /* Activate the device, which lets us access its parameters. */
    HRESULT result = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL,
                                        NULL, (void **)&client);
    IMMDevice_Release(device);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to open WASAPI device: %s", windows_strerror(result));
        goto error_return;
    }

    /* Look up the audio format parameters used by the system mixer. */
    WAVEFORMATEX *mix_format;
    result = IAudioClient_GetMixFormat(client, &mix_format);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to get WASAPI device format: %s",
             windows_strerror(result));
        goto error_release_client;
    }
    DLOG("WASAPI reported format:"
         "\n      Format: 0x%X"
         "\n    Channels: %d"
         "\n        Rate: %d Hz (%d B/s)"
         "\n        Bits: %d",
         mix_format->wFormatTag,
         mix_format->nChannels,
         mix_format->nSamplesPerSec,
         mix_format->nAvgBytesPerSec,
         mix_format->wBitsPerSample);
    if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        DLOG("Extended format:"
             "\n     Sample info: %d"
             "\n    Channel mask: 0x%X"
             "\n     Format GUID: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             ((WAVEFORMATEXTENSIBLE *)mix_format)->Samples.wValidBitsPerSample,
             ((WAVEFORMATEXTENSIBLE *)mix_format)->dwChannelMask,
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data1,
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data2,
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data3,
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[0],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[1],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[2],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[3],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[4],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[5],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[6],
             ((WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat.Data4[7]);
    }
    const int mix_rate = mix_format->nSamplesPerSec;
    if (sound_rate == 0) {
        sound_rate = mix_rate;
    }
    sound_channels = mix_format->nChannels;
    wasapi_format = 0;  // Invalid value.
    if (mix_format->wFormatTag == WAVE_FORMAT_PCM) {
        if (mix_format->wBitsPerSample == 16) {
            wasapi_format = FORMAT_S16;
        } else if (mix_format->wBitsPerSample == 32) {
            wasapi_format = FORMAT_S32;
        }
    } else if (mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        if (mix_format->wBitsPerSample == 32) {
            wasapi_format = FORMAT_F32;
        }
    } else if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const GUID *ext_format =
            &((const WAVEFORMATEXTENSIBLE *)mix_format)->SubFormat;
        if (memcmp(ext_format, &KSDATAFORMAT_SUBTYPE_PCM,
                   sizeof(*ext_format)) == 0) {
            if (mix_format->wBitsPerSample == 16) {
                wasapi_format = FORMAT_S16;
            } else if (mix_format->wBitsPerSample == 32) {
                wasapi_format = FORMAT_S32;
            }
        } else if (memcmp(ext_format, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
                          sizeof(*ext_format)) == 0) {
            if (mix_format->wBitsPerSample == 32) {
                wasapi_format = FORMAT_F32;
            }
        }
    }
    if (!wasapi_format) {
        DLOG("WASAPI data format not supported");
        CoTaskMemFree(mix_format);
        goto error_release_client;
    }
    const int sample_bits = (wasapi_format==FORMAT_S16 ? 16 : 32);
    ASSERT(sample_bits == mix_format->wBitsPerSample);
    const int sample_size = sound_channels * sample_bits / 8;
    ASSERT(sample_size == mix_format->nBlockAlign);

    /* Look up the system mixer's processing period and choose our own
     * buffer period accordingly.  We use a minimum buffer period of 20ms
     * to avoid stutter, as with waveOut. */
    REFERENCE_TIME min_period = 0;
    IAudioClient_GetDevicePeriod(client, NULL, &min_period);
    base_latency = min_period * 1.0e-7f;
    const REFERENCE_TIME period = lbound(min_period, 20*1000*10);

    /* Initialize the device with our desired parameters.  We use the same
     * data format as provided by the system, so this should never fail
     * (except for rate changes on pre-Win7). */
    mix_format->nSamplesPerSec = sound_rate;
    mix_format->nAvgBytesPerSec = sound_rate * sample_size;
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (mix_rate != sound_rate) {
        if (windows_version_is_at_least(WINDOWS_VERSION_10)) {
            flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                  |  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        } else if (windows_version_is_at_least(WINDOWS_VERSION_7)) {
            flags |= AUDCLNT_STREAMFLAGS_RATEADJUST;
            mix_format->nSamplesPerSec = mix_rate;
            mix_format->nAvgBytesPerSec = mix_rate * sample_size;
        }  // Else just hope the driver is nice to us, but we'll probably fail.
    }
    result = IAudioClient_Initialize(
        client, AUDCLNT_SHAREMODE_SHARED, flags, period, 0, mix_format, NULL);
    CoTaskMemFree(mix_format);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to initialize WASAPI client: %s",
             windows_strerror(result));
        goto error_release_client;
    }
    if (flags & AUDCLNT_STREAMFLAGS_RATEADJUST) {
        IAudioClockAdjustment *clock_adj;
        result = IAudioClient_GetService(client, &IID_IAudioClockAdjustment,
                                         (void **)&clock_adj);
        if (UNLIKELY(FAILED(result))) {
            DLOG("Failed to get sample rate adjuster reference: %s",
                 windows_strerror(result));
            goto error_release_client;
        }
        result = IAudioClockAdjustment_SetSampleRate(clock_adj, sound_rate);
        IAudioClockAdjustment_Release(clock_adj);
        if (UNLIKELY(FAILED(result))) {
            DLOG("Failed to adjust sample rate from %d to %d: %s",
                 mix_rate, sound_rate, windows_strerror(result));
            goto error_release_client;
        }
    }

    /* Install our event handle so we can wait on processing-done
     * notifications from the system when the buffer is full. */
    result = IAudioClient_SetEventHandle(client, completion_event);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to initialize WASAPI client handle: %s",
             windows_strerror(result));
        goto error_release_client;
    }

    /* Retrieve the system's processing buffer size. */
    UINT32 buffer_len32;
    result = IAudioClient_GetBufferSize(client, &buffer_len32);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to get WASAPI buffer size: %s",
             windows_strerror(result));
        goto error_release_client;
    }
    wasapi_buffer_len = (int)buffer_len32;

    DLOG("Opened device %s: %d Hz, %d channels, format %s, buffer length %d\n",
         device_name ? device_name : "<unknown>", mix_rate, sound_channels,
         (wasapi_format==FORMAT_S16 ? "S16" :
          wasapi_format==FORMAT_S32 ? "S32" :
          wasapi_format==FORMAT_F32 ? "F32" : "???"),
         wasapi_buffer_len);
    if (mix_rate != sound_rate) {
        DLOG("Resampling enabled (%s): %d -> %d Hz",
             (flags & AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM) ? "AUTOCONVERTPCM" :
                 (flags & AUDCLNT_STREAMFLAGS_RATEADJUST) ? "RATEADJUST" :
                 "native",
             sound_rate, mix_rate);
    }

    /* Retrieve the IAudioRenderClient interface, which we need to access
     * the actual output buffer. */
    result = IAudioClient_GetService(client, &IID_IAudioRenderClient,
                                     (void **)&render_client);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to get WASAPI render client: %s",
             windows_strerror(result));
        goto error_release_client;
    }

    /* Allocate a buffer for receiving data from the software mixer (which
     * may not be in the same format as the device requires).  We use half
     * the device buffer size as our processing buffer size, so that we
     * don't have to wait for the device buffer to completely empty before
     * adding more data. */
    mixer_buffer_len = wasapi_buffer_len / 2;
    mixer_buffer = mem_alloc(mixer_buffer_len * 4, 4, 0);
    if (UNLIKELY(!mixer_buffer)) {
        DLOG("No memory for mixer buffer (%d frames)", mixer_buffer_len);
        goto error_release_render_client;
    }

    /* Start output on the audio device. */
    device_buffer_empty = 1;
    result = IAudioClient_Start(client);
    if (UNLIKELY(FAILED(result))) {
        DLOG("Failed to start WASAPI output: %s",
             windows_strerror(result));
        goto error_free_mixer_buffer;
    }

#ifdef DEBUG
    mem_free(device_name);
#endif
    return 1;

  error_free_mixer_buffer:
    mem_free(mixer_buffer);
    mixer_buffer = NULL;
    mixer_buffer_len = 0;
  error_release_render_client:
    IAudioRenderClient_Release(render_client);
    render_client = NULL;
  error_release_client:
    IAudioClient_Release(client);
    client = NULL;
  error_return:
#ifdef DEBUG
    mem_free(device_name);
#endif
    return 0;
}

/*-----------------------------------------------------------------------*/

static void close_device(void)
{
    PRECOND(client, return);

    mem_free(mixer_buffer);
    mixer_buffer = NULL;
    mixer_buffer_len = 0;
    IAudioRenderClient_Release(render_client);
    render_client = NULL;
    IAudioClient_Release(client);
    client = NULL;
}

/*************************************************************************/
/*************************************************************************/
