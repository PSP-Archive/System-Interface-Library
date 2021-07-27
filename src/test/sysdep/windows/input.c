/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/input.c: Tests for Windows input handling.
 */

#include "src/base.h"
#include "src/test/base.h"

#include "src/graphics.h"
#include "src/input.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/misc/joystick-hid.h"
#include "src/sysdep/test.h"
#include "src/sysdep/windows/internal.h"
#include "src/sysdep/windows/xinput.h"
#include "src/test/graphics/internal.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Buffer of events received from the Windows sys_input module. */
static InputEvent events[INPUT_MAX_JOYSTICK_BUTTONS];
static int num_events;

/* Enable diversion of rawinput/XInput functions? */
static uint8_t enable_input_diversion;

/* Module name to match for GetModuleHandle() wrapper.  If the name passed
 * to the function matches this name, the wrapper will return a handle for
 * the program's executable rather than a handle to the module. */
static const char *GetModuleHandle_module_to_divert;

/* Pointer type to return for GetPointerType() wrapper. */
static int GetPointerType_type_to_return;

/* Flag: Should the GetPointerType() wrapper return failure? */
static uint8_t GetPointerType_fail;

/*************************************************************************/
/***************** Library management function wrappers ******************/
/*************************************************************************/

HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName)
{
    HMODULE (WINAPI *p_GetModuleHandleA)(LPCSTR);
    /* Watch out for infinite recursion! */
    HMODULE kernel32 = GetModuleHandleW(
        (uint16_t[]){'k','e','r','n','e','l','3','2','.','d','l','l',0});
    ASSERT(kernel32);
    p_GetModuleHandleA = (void *)GetProcAddress(kernel32, "GetModuleHandleA");
    ASSERT(p_GetModuleHandleA);

    if (lpModuleName && GetModuleHandle_module_to_divert
     && strcmp(lpModuleName, GetModuleHandle_module_to_divert) == 0) {
        return (*p_GetModuleHandleA)(NULL);
    }
    return (*p_GetModuleHandleA)(lpModuleName);
}

/*-----------------------------------------------------------------------*/

HMODULE WINAPI LoadLibraryW(LPCWSTR lpFileName)
{
    HMODULE (WINAPI *p_LoadLibraryW)(LPCWSTR);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_LoadLibraryW = (void *)GetProcAddress(kernel32, "LoadLibraryW");
    ASSERT(p_LoadLibraryW);

    if (!enable_input_diversion) {
        return (*p_LoadLibraryW)(lpFileName);
    }

    static const int16_t hid_dll[] = {'h','i','d','.','d','l','l',0};
    static const int16_t xinput1_3_dll[] =
        {'x','i','n','p','u','t','1','_','3','.','d','l','l',0};
    static const int16_t xinput1_4_dll[] =
        {'x','i','n','p','u','t','1','_','4','.','d','l','l',0};
    if (memcmp(lpFileName, hid_dll, sizeof(hid_dll)) == 0
     || memcmp(lpFileName, xinput1_3_dll, sizeof(xinput1_3_dll)) == 0
     || memcmp(lpFileName, xinput1_4_dll, sizeof(xinput1_4_dll)) == 0) {
        HMODULE module = GetModuleHandle(NULL);
        ASSERT(module);
        return module;
    }

    return (*p_LoadLibraryW)(lpFileName);
}

/*-----------------------------------------------------------------------*/

BOOL WINAPI FreeLibrary(HMODULE hModule)
{
    BOOL (WINAPI *p_FreeLibrary)(HMODULE);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_FreeLibrary = (void *)GetProcAddress(kernel32, "FreeLibrary");
    ASSERT(p_FreeLibrary);

    /* Assume the code is sufficiently well-behaved that it does not
     * normally try to call FreeLibrary() on a handle returned by
     * GetModuleHandle() (which is invalid), so such calls must be
     * associated with diverted LoadLibrary() calls. */
    if (hModule == GetModuleHandle(NULL)) {
        return TRUE;
    }

    return (*p_FreeLibrary)(hModule);
}

/*************************************************************************/
/****************** Raw-input and HID library wrappers *******************/
/*************************************************************************/

/* Raw input device watches set by RegisterRawInputDevices(). */
static RAWINPUTDEVICE rawinput_watches[2];
static int rawinput_num_watches;

/* Should we simulate the Windows XP bug that returns "\??\" instead of
 * "\\?\" in device paths? */
static uint8_t rawinput_joystick_simulate_winxp_bug;

/* Which simulated raw-input joysticks are connected?  (The same data is
 * reported for all connected joysticks.) */
static uint8_t rawinput_joystick_connected[4];

/* Which simulated raw-input joysticks should be reported as XInput devices? */
static uint8_t rawinput_joystick_is_xinput[4];

/* Which raw-input joysticks' device files have been opened? */
static uint8_t rawinput_joystick_file_open[4];

/* Dummy raw-input handle values. */
#define HRAWINPUT_KEYBOARD     ((HANDLE)0x12345678)
#define HRAWINPUT_MOUSE        ((HANDLE)0x23456789)
#define HRAWINPUT_JOYSTICK(n)  ((HANDLE)((uintptr_t)0x34567890 | (n)))
#define HFILE_JOYSTICK(n)      ((HANDLE)((uintptr_t)0x45678900 | (n)))
#define RAWINPUT_JOYSTICK_INDEX(handle)  ((uintptr_t)(handle) & 0xF)
#define IS_HRAWINPUT_JOYSTICK(handle) \
    ((HANDLE)((uintptr_t)(handle) & ~0xF) == HRAWINPUT_JOYSTICK(0))
#define IS_HFILE_JOYSTICK(handle) \
    ((HANDLE)((uintptr_t)(handle) & ~0xF) == HFILE_JOYSTICK(0))

/* Path prefix for device paths returned by GetRawInputDeviceInfo(). */
#define GRIDI_PATH_PREFIX  "\\\\?\\SIL\\"
/* Same, but with the Windows XP "\??\" bug applied. */
#define GRIDI_PATH_PREFIX_WINXP_BUG  "\\??\\SIL\\"

/* Device information returned by GetRawInputDeviceInfo(). */
static RID_DEVICE_INFO rawinput_joystick_info = {
    .cbSize = sizeof(RID_DEVICE_INFO),
    .dwType = RIM_TYPEHID,
    {.hid = {
        .dwVendorId = 0,
        .dwProductId = 0,
        .dwVersionNumber = 0,
        .usUsagePage = 0,
        .usUsage = 0,
    }},
};

/* Product name and serial number returned by the HidD functions. */
static WCHAR rawinput_joystick_product[127];
static WCHAR rawinput_joystick_serial[127];

/* Number of buttons to report (numbered 1 through N on HID_PAGE_BUTTON). */
static int rawinput_joystick_num_buttons;
/* Number of HIDP_VALUE_CAP items to report. */
static int rawinput_joystick_num_values;
/* List of values to report. */
static HIDP_VALUE_CAPS rawinput_joystick_values[10];

/* Data structure used for passing input through the raw input API. */
typedef struct RawInputValueReport RawInputValueReport;
struct RawInputValueReport {
    uint16_t usage_page;
    uint16_t usage;
    ULONG value;
};

/* Data sent to device using WriteFile(). */
static uint8_t rawinput_joystick_write_buf[256];
static int rawinput_joystick_write_len;

/* Should we simulate the bug in Steam's injected DLL which causes GRIDL
 * calls to infinite-loop? */
static uint8_t rawinput_simulate_steam_gridl_bug;

/*-----------------------------------------------------------------------*/

BOOL WINAPI RegisterRawInputDevices(PCRAWINPUTDEVICE pRawInputDevices,
                                    UINT uiNumDevices, UINT cbSize)
{
    UINT (WINAPI *p_RRID)(PCRAWINPUTDEVICE, UINT, UINT);
    HMODULE user32 = GetModuleHandle("user32.dll");
    ASSERT(user32);
    p_RRID = (void *)GetProcAddress(user32, "GetRawInputDeviceList");
    ASSERT(p_RRID);

    if (!enable_input_diversion) {
        return (*p_RRID)(pRawInputDevices, uiNumDevices, cbSize);
    }

    if (cbSize != sizeof(*pRawInputDevices)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (UINT i = 0; i < uiNumDevices; i++) {
        if (pRawInputDevices[i].dwFlags & ~0x00003731U) {
            SetLastError(ERROR_INVALID_FLAGS);
            return FALSE;
        }
        if (!windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
            if (pRawInputDevices[i].dwFlags
                & (RIDEV_DEVNOTIFY | RIDEV_EXINPUTSINK))
            {
                /* These flags are unsupported under Windows XP. */
                SetLastError(ERROR_INVALID_FLAGS);
                return FALSE;
            }
        }
        if ((pRawInputDevices[i].dwFlags & RIDEV_APPKEYS)
         && !(pRawInputDevices[i].dwFlags & RIDEV_NOLEGACY)) {
            SetLastError(ERROR_INVALID_FLAGS);
            return FALSE;
        }
        if ((pRawInputDevices[i].dwFlags & RIDEV_INPUTSINK)
         && pRawInputDevices[i].hwndTarget != NULL) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        if ((pRawInputDevices[i].dwFlags & RIDEV_REMOVE)
         && pRawInputDevices[i].hwndTarget != NULL) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    for (UINT i = 0; i < uiNumDevices; i++) {
        if (pRawInputDevices[i].dwFlags & RIDEV_REMOVE) {
            for (int j = 0; j < rawinput_num_watches; j++) {
                if (rawinput_watches[j].usUsage == pRawInputDevices[i].usUsage
                 && rawinput_watches[j].usUsagePage == pRawInputDevices[i].usUsagePage) {
                    rawinput_num_watches--;
                    memmove(&rawinput_watches[j], &rawinput_watches[j+1],
                            sizeof(*rawinput_watches)
                                * (rawinput_num_watches - j));
                    j--;
                }
            }
        } else {  // not RIDEV_REMOVE
            for (int j = 0; j < rawinput_num_watches; j++) {
                ASSERT((rawinput_watches[j].usUsagePage
                        != pRawInputDevices[i].usUsagePage)
                    || (rawinput_watches[j].usUsage
                        != pRawInputDevices[i].usUsage));
            }
            ASSERT(rawinput_num_watches < lenof(rawinput_watches));
            rawinput_watches[rawinput_num_watches++] = pRawInputDevices[i];
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------*/

UINT WINAPI GetRawInputDeviceList(PRAWINPUTDEVICELIST pRawInputDeviceList,
                                  PUINT puiNumDevices, UINT cbSize)
{
    UINT (WINAPI *p_GRIDL)(PRAWINPUTDEVICELIST, PUINT, UINT);
    HMODULE user32 = GetModuleHandle("user32.dll");
    ASSERT(user32);
    p_GRIDL = (void *)GetProcAddress(user32, "GetRawInputDeviceList");
    ASSERT(p_GRIDL);

    if (!enable_input_diversion) {
        return (*p_GRIDL)(pRawInputDeviceList, puiNumDevices, cbSize);
    }

    if (cbSize != sizeof(*pRawInputDeviceList)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    UINT num_devices = 2;  // Include (dummy) keyboard and mouse entries.
    for (int i = 0; i < lenof(rawinput_joystick_connected); i++) {
        if (rawinput_joystick_connected[i]) {
            num_devices++;
        }
    }
    if (!pRawInputDeviceList) {
        *puiNumDevices = num_devices;
        return 0;
    } else if (*puiNumDevices < num_devices) {
        if (!rawinput_simulate_steam_gridl_bug) {
            *puiNumDevices = num_devices;
        }
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return -1;
    } else {
        mem_clear(pRawInputDeviceList,
                  sizeof(*pRawInputDeviceList) * num_devices);
        pRawInputDeviceList[0].hDevice = HRAWINPUT_KEYBOARD;
        pRawInputDeviceList[0].dwType = RIM_TYPEKEYBOARD;
        pRawInputDeviceList[1].hDevice = HRAWINPUT_MOUSE;
        pRawInputDeviceList[1].dwType = RIM_TYPEMOUSE;
        unsigned int n = 2;
        for (int i = 0; i < lenof(rawinput_joystick_connected); i++) {
            if (rawinput_joystick_connected[i]) {
                pRawInputDeviceList[n].hDevice = HRAWINPUT_JOYSTICK(i);
                pRawInputDeviceList[n].dwType = RIM_TYPEHID;
                n++;
            }
        }
        ASSERT(n == num_devices);
        return num_devices;
    }
}

/*-----------------------------------------------------------------------*/

UINT WINAPI GetRawInputDeviceInfoW(HANDLE hDevice, UINT uiCommand,
                                   LPVOID pData, PUINT pcbSize)
{
    UINT (WINAPI *p_GRIDI)(HANDLE, UINT, LPVOID, PUINT);
    HMODULE user32 = GetModuleHandle("user32.dll");
    ASSERT(user32);
    p_GRIDI = (void *)GetProcAddress(user32, "GetRawInputDeviceInfoW");
    ASSERT(p_GRIDI);

    if (!enable_input_diversion) {
        return (*p_GRIDI)(hDevice, uiCommand, pData, pcbSize);
    }

    if (hDevice == HRAWINPUT_KEYBOARD) {
        ASSERT(uiCommand == RIDI_DEVICEINFO);
        ASSERT(pData != NULL);
        ASSERT(*pcbSize == sizeof(RID_DEVICE_INFO));
        mem_clear(pData, sizeof(RID_DEVICE_INFO));
        ((RID_DEVICE_INFO *)pData)->cbSize = sizeof(RID_DEVICE_INFO);
        ((RID_DEVICE_INFO *)pData)->dwType = RIM_TYPEKEYBOARD;
        return sizeof(RID_DEVICE_INFO);
    } else if (hDevice == HRAWINPUT_MOUSE) {
        ASSERT(uiCommand == RIDI_DEVICEINFO);
        ASSERT(pData != NULL);
        ASSERT(*pcbSize == sizeof(RID_DEVICE_INFO));
        mem_clear(pData, sizeof(RID_DEVICE_INFO));
        ((RID_DEVICE_INFO *)pData)->cbSize = sizeof(RID_DEVICE_INFO);
        ((RID_DEVICE_INFO *)pData)->dwType = RIM_TYPEMOUSE;
        return sizeof(RID_DEVICE_INFO);
    } else if (!IS_HRAWINPUT_JOYSTICK(hDevice)) {
        ASSERT(!"should not be reached");
    }
    const int index = RAWINPUT_JOYSTICK_INDEX(hDevice);
    ASSERT(index >= 0 && index < lenof(rawinput_joystick_connected));

    if (!rawinput_joystick_connected[index]) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }

    const void *src;
    UINT size;
    UINT unit = 1; // Number of bytes per unit of "size" (for RIDI_DEVICENAME).
    WCHAR path16[20];  // For RIDI_DEVICENAME.
    switch (uiCommand) {
      case RIDI_DEVICENAME:
        src = path16;
        size = strlen(GRIDI_PATH_PREFIX)
             + (rawinput_joystick_is_xinput[index] ? 6 : 2);
        unit = sizeof(WCHAR);
        ASSERT(lenof(path16) >= size);
        ASSERT(strlen(GRIDI_PATH_PREFIX)
               == strlen(GRIDI_PATH_PREFIX_WINXP_BUG));
        UINT i;
        for (i = 0; i < strlen(GRIDI_PATH_PREFIX); i++) {
            if (rawinput_joystick_simulate_winxp_bug) {
                path16[i] = GRIDI_PATH_PREFIX_WINXP_BUG[i];
            } else {
                path16[i] = GRIDI_PATH_PREFIX[i];
            }
        }
        if (rawinput_joystick_is_xinput[index]) {
            path16[i++] = 'I';
            path16[i++] = 'G';
            path16[i++] = '_';
            path16[i++] = '0';
        }
        path16[i] = '0' + index;
        path16[i+1] = 0;
        break;
      case RIDI_DEVICEINFO:
        src = &rawinput_joystick_info;
        size = sizeof(rawinput_joystick_info);
        if (((RID_DEVICE_INFO *)pData)->cbSize != sizeof(RID_DEVICE_INFO)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return -1;
        }
        break;
      case RIDI_PREPARSEDDATA:
        src = &hDevice;
        size = sizeof(hDevice);
        break;
      default:
        ASSERT(!"should not be reached");
    }

    if (!pData) {
        *pcbSize = size;
        return 0;
    } else if (*pcbSize < size) {
        *pcbSize = size;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return -1;
    } else {
        memcpy(pData, src, size * unit);
        return size;
    }
}

/*-----------------------------------------------------------------------*/

UINT WINAPI GetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData,
                            PUINT pcbSize, UINT cbSizeHeader)
{
    UINT (WINAPI *p_GRID)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
    HMODULE user32 = GetModuleHandle("user32.dll");
    ASSERT(user32);
    p_GRID = (void *)GetProcAddress(user32, "GetRawInputData");
    ASSERT(p_GRID);

    if (!enable_input_diversion) {
        return (*p_GRID)(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    }

    if (cbSizeHeader != sizeof(RAWINPUTHEADER)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    const RAWINPUT *input = (const RAWINPUT *)hRawInput;
    UINT size;
    if (uiCommand == RID_HEADER) {
        size = sizeof(RAWINPUTHEADER);
    } else if (uiCommand == RID_INPUT) {
        size = input->header.dwSize;
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    if (pData) {
        size = ubound(size, *pcbSize);
        memcpy(pData, input, size);
        return size;
    } else {
        *pcbSize = size;
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

HANDLE WINAPI CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
                          DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                          DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    HANDLE (WINAPI *p_CreateFileW)(
        LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_CreateFileW = (void *)GetProcAddress(kernel32, "CreateFileW");
    ASSERT(p_CreateFileW);

    if (!enable_input_diversion) {
        return (*p_CreateFileW)(lpFileName, dwDesiredAccess, dwShareMode,
                                lpSecurityAttributes, dwCreationDisposition,
                                dwFlagsAndAttributes, hTemplateFile);
    }

    WCHAR rawinput_path16[20];
    int len = strlen(GRIDI_PATH_PREFIX);
    ASSERT(lenof(rawinput_path16) >= len);
    for (int i = 0; i < len; i++) {
        rawinput_path16[i] = GRIDI_PATH_PREFIX[i];
    }
    if (memcmp(lpFileName, rawinput_path16, len * sizeof(WCHAR)) != 0) {
        return (*p_CreateFileW)(lpFileName, dwDesiredAccess, dwShareMode,
                                lpSecurityAttributes, dwCreationDisposition,
                                dwFlagsAndAttributes, hTemplateFile);
    }

    if (lpFileName[len+0] == 'I'
     && lpFileName[len+1] == 'G'
     && lpFileName[len+2] == '_'
     && lpFileName[len+3] == '0') {
        len += 4;
    }
    const int index = lpFileName[len] - '0';
    ASSERT(index >= 0 && index < lenof(rawinput_joystick_connected));
    ASSERT(!rawinput_joystick_file_open[index]);
    rawinput_joystick_file_open[index] = 1;
    return HFILE_JOYSTICK(index);
}

/*-----------------------------------------------------------------------*/

BOOL WINAPI WriteFile(HANDLE hFile, LPCVOID lpBuffer,
                      DWORD nNumberOfBytesToWrite,
                      LPDWORD lpNumberOfBytesWritten,
                      LPOVERLAPPED lpOverlapped)
{
    BOOL (WINAPI *p_WriteFile)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_WriteFile = (void *)GetProcAddress(kernel32, "WriteFile");
    ASSERT(p_WriteFile);

    if (!enable_input_diversion) {
        return (*p_WriteFile)(hFile, lpBuffer, nNumberOfBytesToWrite,
                              lpNumberOfBytesWritten, lpOverlapped);
    }

    if (!IS_HFILE_JOYSTICK(hFile)) {
        return (*p_WriteFile)(hFile, lpBuffer, nNumberOfBytesToWrite,
                              lpNumberOfBytesWritten, lpOverlapped);
    }

    const int index = RAWINPUT_JOYSTICK_INDEX(hFile);
    ASSERT(rawinput_joystick_connected[index]);
    ASSERT(rawinput_joystick_file_open[index]);
    ASSERT(!lpOverlapped);
    ASSERT(lpNumberOfBytesWritten);
    ASSERT(nNumberOfBytesToWrite <= sizeof(rawinput_joystick_write_buf));
    memcpy(rawinput_joystick_write_buf, lpBuffer, nNumberOfBytesToWrite);
    rawinput_joystick_write_len = nNumberOfBytesToWrite;
    *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
    return TRUE;
}

/*-----------------------------------------------------------------------*/

BOOL WINAPI CloseHandle(HANDLE hObject)
{
    BOOL (WINAPI *p_CloseHandle)(HANDLE);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_CloseHandle = (void *)GetProcAddress(kernel32, "CloseHandle");
    ASSERT(p_CloseHandle);

    if (!enable_input_diversion) {
        return (*p_CloseHandle)(hObject);
    }

    if (!IS_HFILE_JOYSTICK(hObject)) {
        return (*p_CloseHandle)(hObject);
    }

    const int index = RAWINPUT_JOYSTICK_INDEX(hObject);
    ASSERT(rawinput_joystick_file_open[index]);
    rawinput_joystick_file_open[index] = 0;
    return TRUE;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) BOOLEAN __stdcall HidD_GetProductString(
    HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength)
{
    ASSERT(enable_input_diversion);

    if (!IS_HFILE_JOYSTICK(HidDeviceObject)) {
        return FALSE;
    }

    if (BufferLength >= sizeof(rawinput_joystick_product)) {
        memcpy(Buffer, rawinput_joystick_product,
               sizeof(rawinput_joystick_product));
        return TRUE;
    } else {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) BOOLEAN __stdcall HidD_GetSerialNumberString(
    HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength)
{
    ASSERT(enable_input_diversion);

    if (!IS_HFILE_JOYSTICK(HidDeviceObject)) {
        return FALSE;
    }

    if (BufferLength >= sizeof(rawinput_joystick_serial)) {
        memcpy(Buffer, rawinput_joystick_serial,
               sizeof(rawinput_joystick_serial));
        return TRUE;
    } else {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) NTSTATUS __stdcall HidP_GetCaps(
    PHIDP_PREPARSED_DATA PreparsedData, PHIDP_CAPS Capabilities)
{
    ASSERT(enable_input_diversion);

    HANDLE handle = *(HANDLE *)PreparsedData;
    if (!IS_HRAWINPUT_JOYSTICK(handle)) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }

    mem_clear(Capabilities, sizeof(*Capabilities));
    Capabilities->NumberInputButtonCaps =
        ubound(rawinput_joystick_num_buttons, 2); // See GetSpecificButtonCaps.
    Capabilities->NumberInputValueCaps = rawinput_joystick_num_values;
    return HIDP_STATUS_SUCCESS;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) NTSTATUS __stdcall HidP_GetSpecificButtonCaps(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PHIDP_BUTTON_CAPS ButtonCaps, PUSHORT ButtonCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData)
{
    ASSERT(enable_input_diversion);

    /* These are never used by the calling code. */
    ASSERT(!UsagePage);
    ASSERT(!LinkCollection);
    ASSERT(!Usage);

    HANDLE handle = *(HANDLE *)PreparsedData;
    if (!IS_HRAWINPUT_JOYSTICK(handle)) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (ReportType != HidP_Input) {
        return HIDP_STATUS_INVALID_REPORT_TYPE;
    }

    /* We report the first button as a single usage value and any
     * remaining buttons as a usage range to test handling of both types
     * of entry. */
    const int num_entries = ubound(rawinput_joystick_num_buttons, 2);
    *ButtonCapsLength = ubound(num_entries, (int)*ButtonCapsLength);
    mem_clear(ButtonCaps, sizeof(*ButtonCaps) * (*ButtonCapsLength));
    if (*ButtonCapsLength >= 1) {
        ButtonCaps[0].UsagePage = HID_PAGE_BUTTON;
        ButtonCaps[0].IsAbsolute = TRUE;
        ButtonCaps[0].NotRange.Usage = 1;
    }
    if (*ButtonCapsLength >= 2) {
        ButtonCaps[1].UsagePage = HID_PAGE_BUTTON;
        ButtonCaps[1].IsRange = TRUE;
        ButtonCaps[1].IsAbsolute = TRUE;
        ButtonCaps[1].Range.UsageMin = 2;
        ButtonCaps[1].Range.UsageMax = rawinput_joystick_num_buttons;
    }
    return HIDP_STATUS_SUCCESS;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) NTSTATUS __stdcall HidP_GetSpecificValueCaps(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PHIDP_VALUE_CAPS ValueCaps, PUSHORT ValueCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData)
{
    ASSERT(enable_input_diversion);

    /* These are never used by the calling code. */
    ASSERT(!UsagePage);
    ASSERT(!LinkCollection);
    ASSERT(!Usage);

    HANDLE handle = *(HANDLE *)PreparsedData;
    if (!IS_HRAWINPUT_JOYSTICK(handle)) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (ReportType != HidP_Input) {
        return HIDP_STATUS_INVALID_REPORT_TYPE;
    }

    *ValueCapsLength = ubound(rawinput_joystick_num_values,
                              (int)*ValueCapsLength);
    memcpy(ValueCaps, rawinput_joystick_values,
           sizeof(*ValueCaps) * (*ValueCapsLength));
    return HIDP_STATUS_SUCCESS;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) NTSTATUS __stdcall HidP_GetUsageValue(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PULONG UsageValue, PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report, ULONG ReportLength)
{
    ASSERT(enable_input_diversion);

    /* This is never used by the calling code. */
    ASSERT(!LinkCollection);

    HANDLE handle = *(HANDLE *)PreparsedData;
    if (!IS_HRAWINPUT_JOYSTICK(handle)) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (ReportType != HidP_Input) {
        return HIDP_STATUS_INVALID_REPORT_TYPE;
    }
    if (ReportLength % sizeof(RawInputValueReport) != 0) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    for (int i = 0; i < (int)(ReportLength/sizeof(RawInputValueReport)); i++) {
        const RawInputValueReport *report =
            &((const RawInputValueReport *)Report)[i];
        if (report->usage_page == UsagePage && report->usage == Usage) {
            *UsageValue = report->value;
            return HIDP_STATUS_SUCCESS;
        }
    }
    return HIDP_STATUS_USAGE_NOT_FOUND;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) NTSTATUS __stdcall HidP_GetUsagesEx(
    HIDP_REPORT_TYPE ReportType, USHORT LinkCollection,
    PUSAGE_AND_PAGE ButtonList, ULONG *UsageLength,
    PHIDP_PREPARSED_DATA PreparsedData, PCHAR Report, ULONG ReportLength)
{
    ASSERT(enable_input_diversion);

    /* This is never used by the calling code. */
    ASSERT(!LinkCollection);

    HANDLE handle = *(HANDLE *)PreparsedData;
    if (!IS_HRAWINPUT_JOYSTICK(handle)) {
        return HIDP_STATUS_INVALID_PREPARSED_DATA;
    }
    if (ReportType != HidP_Input) {
        return HIDP_STATUS_INVALID_REPORT_TYPE;
    }
    if (ReportLength % sizeof(RawInputValueReport) != 0) {
        return HIDP_STATUS_INVALID_REPORT_LENGTH;
    }

    const int num_entries = ReportLength / sizeof(RawInputValueReport);
    int num_buttons = 0;
    for (int i = 0; i < num_entries; i++) {
        const RawInputValueReport *report =
            &((const RawInputValueReport *)Report)[i];
        if (report->usage_page == HID_PAGE_BUTTON) {
            ASSERT(report->value == 1);
            num_buttons++;
        }
    }
    if ((int)*UsageLength < num_buttons) {
        *UsageLength = num_buttons;
        return HIDP_STATUS_BUFFER_TOO_SMALL;
    }

    *UsageLength = num_buttons;
    for (int i = 0, j = 0; i < num_entries && j < num_buttons; i++) {
        const RawInputValueReport *report =
            &((const RawInputValueReport *)Report)[i];
        if (report->usage_page == HID_PAGE_BUTTON) {
            ButtonList[j].UsagePage = report->usage_page;
            ButtonList[j].Usage = report->usage;
            j++;
        }
    }
    return HIDP_STATUS_SUCCESS;
}

/*************************************************************************/
/**************************** XInput wrappers ****************************/
/*************************************************************************/

/* Which simulated XInput joysticks are connected?  (The same data is
 * reported for all connected joysticks.) */
static uint8_t xinput_joystick_connected[4];

/* Simulated joystick capabilities and state. */
static XINPUT_CAPABILITIES xinput_joystick_caps;
static XINPUT_STATE xinput_joystick_state;
static XINPUT_VIBRATION xinput_joystick_vibration;

/* Capability definitions for various joystick types. */
static const XINPUT_CAPABILITIES xinput_simple_caps = {
    .Type = XINPUT_DEVTYPE_GAMEPAD,
    .SubType = XINPUT_DEVSUBTYPE_GAMEPAD,
    .Flags = 0,
    .Gamepad = {
        .wButtons = XINPUT_GAMEPAD_A,
        .bLeftTrigger = 0,
        .bRightTrigger = 0,
        .sThumbLX = -1,
        .sThumbLY = -1,
        .sThumbRX = 0,
        .sThumbRY = 0,
    },
    .Vibration = {
        .wLeftMotorSpeed = 0,
        .wRightMotorSpeed = 0,
    },
};
static const XINPUT_CAPABILITIES xinput_x360_caps = {
    .Type = XINPUT_DEVTYPE_GAMEPAD,
    .SubType = XINPUT_DEVSUBTYPE_GAMEPAD,
    .Flags = 0,
    .Gamepad = {
        .wButtons = XINPUT_GAMEPAD_DPAD_UP
                  | XINPUT_GAMEPAD_DPAD_DOWN
                  | XINPUT_GAMEPAD_DPAD_LEFT
                  | XINPUT_GAMEPAD_DPAD_RIGHT
                  | XINPUT_GAMEPAD_START
                  | XINPUT_GAMEPAD_BACK
                  | XINPUT_GAMEPAD_LEFT_THUMB
                  | XINPUT_GAMEPAD_RIGHT_THUMB
                  | XINPUT_GAMEPAD_LEFT_SHOULDER
                  | XINPUT_GAMEPAD_RIGHT_SHOULDER
                  | XINPUT_GAMEPAD_A
                  | XINPUT_GAMEPAD_B
                  | XINPUT_GAMEPAD_X
                  | XINPUT_GAMEPAD_Y,
        .bLeftTrigger = 255,
        .bRightTrigger = 255,
        .sThumbLX = -1,
        .sThumbLY = -1,
        .sThumbRX = -1,
        .sThumbRY = -1,
    },
    .Vibration = {
        .wLeftMotorSpeed = 65535,
        .wRightMotorSpeed = 65535,
    },
};

/*-----------------------------------------------------------------------*/

__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(
    DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities)
{
    ASSERT(enable_input_diversion);

    ASSERT(dwFlags == 0 || dwFlags == XINPUT_FLAG_GAMEPAD);
    if (dwUserIndex >= lenof(xinput_joystick_connected)
     || !xinput_joystick_connected[dwUserIndex]) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    *pCapabilities = xinput_joystick_caps;
    return ERROR_SUCCESS;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) DWORD WINAPI XInputGetState(
    DWORD dwUserIndex, XINPUT_STATE *pState)
{
    ASSERT(enable_input_diversion);

    if (dwUserIndex >= lenof(xinput_joystick_connected)
     || !xinput_joystick_connected[dwUserIndex]) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    *pState = xinput_joystick_state;
    return ERROR_SUCCESS;
}

/*-----------------------------------------------------------------------*/

__declspec(dllexport) DWORD WINAPI XInputSetState(
    DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
{
    ASSERT(enable_input_diversion);

    if (dwUserIndex >= lenof(xinput_joystick_connected)
     || !xinput_joystick_connected[dwUserIndex]) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    xinput_joystick_vibration = *pVibration;
    return ERROR_SUCCESS;
}

/*************************************************************************/
/**************************** Other wrappers *****************************/
/*************************************************************************/

__declspec(dllexport) BOOL WINAPI GetPointerType(
    UNUSED UINT32 pointerId, UNUSED POINTER_INPUT_TYPE *pointerType)
{
    if (GetPointerType_fail) {
        return FALSE;
    }
    *pointerType = GetPointerType_type_to_return;
    return TRUE;
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * event_callback:  Callback which receives input events from the sys_input
 * module.
 */
static void event_callback(const InputEvent *event)
{
    ASSERT(num_events < lenof(events));
    events[num_events++] = *event;
}

/*-----------------------------------------------------------------------*/

/**
 * get_windows_grab_state:  Return whether the Windows mouse pointer is
 * confined to the current window.
 *
 * [Return value]
 *     True if input is grabbed, false if not.
 */
static int get_windows_grab_state(void)
{
    RECT rect;
    GetClipCursor(&rect);
    return rect.right - rect.left == graphics_display_width()
        && rect.bottom - rect.top == graphics_display_height();
}

/*-----------------------------------------------------------------------*/

/**
 * post_message_sync:  Send a message to the current window's window
 * procedure and wait for it to be processed.  Similar to SendMessage(),
 * but does not bypass the message queue.
 */
static void post_message_sync(UINT msg, WPARAM wParam, LPARAM lParam)
{
    PRECOND(windows_window());
    PostMessage(windows_window(), msg, wParam, lParam);
    windows_flush_message_queue();
}

/*-----------------------------------------------------------------------*/

/**
 * send_raw_input:  Generate a raw-input joystick event.
 *
 * Calling this function more than 3 times without processing window
 * messages will cause earlier events to be overwritten.
 *
 * [Parameters]
 *     device: Device for which to generate event.
 *     data: Raw input data.
 *     size: Size of data, in bytes.
 */
static void send_raw_input(HANDLE device, const void *data, int size)
{
    static uint8_t buffers[3][1000];
    static int next_buffer = 0;

    const int total_size =
        offsetof(RAWINPUT,data) + offsetof(RAWHID,bRawData) + size;
    ASSERT(total_size <= (int)sizeof(buffers[0]));

    RAWINPUT *input = (RAWINPUT *)buffers[next_buffer];
    next_buffer = (next_buffer + 1) % lenof(buffers);

    mem_clear(input, total_size);
    input->header.dwType = RIM_TYPEHID;
    input->header.dwSize = total_size;
    input->header.hDevice = device;
    input->data.hid.dwSizeHid = size;
    input->data.hid.dwCount = 1;
    memcpy(input->data.hid.bRawData, data, size);
    post_message_sync(WM_INPUT, 0, (LPARAM)input);
}

/*-----------------------------------------------------------------------*/

/**
 * make_touch_lparam:  Return an lParam value containing the given window
 * coordinates for a touch (WM_POINTER*) message.
 */
UNUSED  // FIXME: see note about synthetic WM_POINTER messages below
static LPARAM make_touch_lparam(int x, int y)
{
    POINT p = {.x = x, .y = y};
    ASSERT(ClientToScreen(windows_window(), &p));
    return (p.y << 16) | (p.x & 0xFFFF);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_windows_input(void);
int test_windows_input(void)
{
    rawinput_num_watches = 0;
    enable_input_diversion = 1;

    const int result = run_tests_in_window(do_test_windows_input);

    enable_input_diversion = 0;
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_windows_input)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    time_init();
    GetModuleHandle_module_to_divert = NULL;
    GetPointerType_fail = 0;
    num_events = 0;
    rawinput_joystick_simulate_winxp_bug = 0;
    rawinput_simulate_steam_gridl_bug = 0;
    mem_clear(rawinput_joystick_connected, sizeof(rawinput_joystick_connected));
    mem_clear(rawinput_joystick_is_xinput, sizeof(rawinput_joystick_is_xinput));
    mem_clear(rawinput_joystick_file_open, sizeof(rawinput_joystick_file_open));
    mem_clear(&rawinput_joystick_info.hid, sizeof(rawinput_joystick_info.hid));
    mem_clear(rawinput_joystick_product, sizeof(rawinput_joystick_product));
    mem_clear(rawinput_joystick_serial, sizeof(rawinput_joystick_serial));
    rawinput_joystick_num_buttons = 0;
    rawinput_joystick_num_values = 0;
    mem_clear(rawinput_joystick_write_buf, sizeof(rawinput_joystick_write_buf));
    rawinput_joystick_write_len = 0;
    mem_clear(xinput_joystick_connected, sizeof(xinput_joystick_connected));
    mem_clear(&xinput_joystick_caps, sizeof(xinput_joystick_caps));
    mem_clear(&xinput_joystick_state, sizeof(xinput_joystick_state));
    mem_clear(&xinput_joystick_vibration, sizeof(xinput_joystick_vibration));
    SetEnvironmentVariable("SIL_WINDOWS_USE_RAWINPUT", NULL);
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", NULL);
    CHECK_TRUE(sys_input_init(event_callback));

    /* Ignore any real input events that may have come through since the
     * end of the last test. */
    windows_flush_message_queue();
    sys_input_update();
    num_events = 0;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_RAWINPUT", NULL);
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", NULL);
    GetModuleHandle_module_to_divert = NULL;

    for (int i = 0; i < lenof(rawinput_joystick_file_open); i++) {
        if (rawinput_joystick_file_open[i]) {
            FAIL("rawinput_joystick_file_open[%d] was not false as expected",
                 i);
        }
    }

    return 1;
}

/*************************************************************************/
/******************* Tests: Joystick input (rawinput) ********************/
/*************************************************************************/

TEST(test_joystick_rawinput_register)
{
    const DWORD expected_flags =
        (windows_version_is_at_least(WINDOWS_VERSION_VISTA)
         ? RIDEV_DEVNOTIFY : 0);

    graphics_cleanup();
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    ASSERT(graphics_init());
    graphics_set_display_attr("vsync", 0);
    graphics_show_mouse_pointer(1);
    ASSERT(open_window(TESTW, TESTH));
    CHECK_INTEQUAL(rawinput_num_watches, 2);
    CHECK_INTEQUAL(rawinput_watches[0].usUsagePage, HID_PAGE_GENERIC_DESKTOP);
    CHECK_INTEQUAL(rawinput_watches[0].usUsage, HID_USAGE_JOYSTICK);
    CHECK_INTEQUAL(rawinput_watches[0].dwFlags, expected_flags);
    CHECK_TRUE(rawinput_watches[0].hwndTarget);
    CHECK_INTEQUAL(rawinput_watches[1].usUsagePage, HID_PAGE_GENERIC_DESKTOP);
    CHECK_INTEQUAL(rawinput_watches[1].usUsage, HID_USAGE_GAMEPAD);
    CHECK_INTEQUAL(rawinput_watches[1].dwFlags, expected_flags);
    CHECK_TRUE(rawinput_watches[1].hwndTarget);

    graphics_cleanup();
    const int num_watches_after_cleanup = rawinput_num_watches;
    ASSERT(graphics_init());
    graphics_set_display_attr("vsync", 0);
    graphics_show_mouse_pointer(1);
    ASSERT(open_window(TESTW, TESTH));
    CHECK_INTEQUAL(num_watches_after_cleanup, 0);

    CHECK_INTEQUAL(rawinput_num_watches, 2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_initial)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_initial_memory_failure)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SysInputInfo info;

    /* We need to make sure to reinitialize the input subsystem before
     * returning failure because the test cleanup routine will call
     * sys_input_cleanup(), which we're not allowed to call twice in
     * succession without an intervening successful sys_input_init() call. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  ASSERT(sys_input_init(event_callback)); return 0
    CHECK_MEMORY_FAILURES((num_events = 0, sys_input_init(event_callback))
                          && ((sys_input_update(), sys_input_info(&info),
                               info.has_joystick)
                              || (sys_input_cleanup(), 0)));
    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0

    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_event)
{
    /* Windows XP doesn't generate device connected/removed events, but
     * the code is enabled unconditionally so we can still test it on XP. */

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_env_disabled)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_RAWINPUT", "0");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_env_enabled)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_RAWINPUT", "1");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_env_empty)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_RAWINPUT", "");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_gamepad)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_GAMEPAD;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_non_joystick_hid)
{
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = 0;  // HID_USAGE_UNDEFINED
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 0);

    /* Just for completeness. */
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));

    rawinput_joystick_info.hid.usUsagePage = 0;  // HID_PAGE_UNDEFINED
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_scan)
{
    if (windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
        SKIP("Raw input devices are only scanned in Windows XP.");
    }

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    DLOG("Waiting for connect detection (1.1 sec)...");
    Sleep(1100);

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_and_scan)
{
    if (windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
        SKIP("Raw input devices are only scanned in Windows XP.");
    }

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    DLOG("Waiting for scan (1.1 sec)...");
    Sleep(1100);

    sys_test_time_set_seconds(4.0);
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_disconnect)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    rawinput_joystick_connected[0] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_disconnect_scan)
{
    if (windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
        SKIP("Raw input devices are only scanned in Windows XP.");
    }

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    rawinput_joystick_connected[0] = 0;
    DLOG("Waiting for disconnect detection (1.1 sec)...");
    Sleep(1100);

    sys_test_time_set_seconds(3.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_xinput)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    rawinput_joystick_is_xinput[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_xinput_no_library)
{
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", "0");
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    rawinput_joystick_is_xinput[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_winxp_bug)
{
    rawinput_joystick_simulate_winxp_bug = 1;

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    rawinput_joystick_is_xinput[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_connect_overwrite)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    rawinput_joystick_connected[0] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    rawinput_joystick_info.hid.dwProductId = 2;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 0;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_copy_name)
{
    rawinput_joystick_product[0] = 'T';
    rawinput_joystick_product[1] = 'e';
    rawinput_joystick_product[2] = 's';
    rawinput_joystick_product[3] = 't';
    rawinput_joystick_product[4] = L'。';
    rawinput_joystick_product[5] = '\0';
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "Test。");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_copy_name_disconnected)
{
    rawinput_joystick_product[0] = 'T';
    rawinput_joystick_product[1] = 'e';
    rawinput_joystick_product[2] = 's';
    rawinput_joystick_product[3] = 't';
    rawinput_joystick_product[4] = L'。';
    rawinput_joystick_product[5] = '\0';
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    rawinput_joystick_connected[0] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_button_input)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 2;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    const RawInputValueReport inputs[] = {
        {.usage_page = HID_PAGE_BUTTON, .usage = 2, .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    send_raw_input(HRAWINPUT_JOYSTICK(0), NULL, 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_value_input)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 2;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_values[1].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[1].IsAbsolute = TRUE;
    rawinput_joystick_values[1].BitSize = 4;
    rawinput_joystick_values[1].ReportCount = 1;
    rawinput_joystick_values[1].LogicalMin = 1;
    rawinput_joystick_values[1].LogicalMax = -8;  // Will be treated as +8.
    rawinput_joystick_values[1].PhysicalMin = 1;
    rawinput_joystick_values[1].PhysicalMax = -8;
    rawinput_joystick_values[1].NotRange.Usage = HID_USAGE_HAT;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    const RawInputValueReport inputs[] = {
        {.usage_page = HID_PAGE_GENERIC_DESKTOP, .usage = HID_USAGE_X,
         .value = -32767},
        {.usage_page = HID_PAGE_GENERIC_DESKTOP, .usage = HID_USAGE_HAT,
         .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    /* The D-pad event comes first because the stick event isn't flushed
     * until after all input has been processed. */
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    /* Identical inputs should result in no events. */
    sys_test_time_set_seconds(3.0);
    num_events = 0;
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_button_input_overflow)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = INPUT_MAX_JOYSTICK_BUTTONS + 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    const RawInputValueReport inputs_1[] = {
        {.usage_page = HID_PAGE_BUTTON, .usage = 2, .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs_1, sizeof(inputs_1));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    RawInputValueReport inputs_2[INPUT_MAX_JOYSTICK_BUTTONS + 1];
    for (int i = 0; i < lenof(inputs_2); i++) {
        inputs_2[i].usage_page = HID_PAGE_BUTTON;
        inputs_2[i].usage = i + 1;
        inputs_2[i].value = 1;
    }
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs_2, sizeof(inputs_2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, INPUT_MAX_JOYSTICK_BUTTONS - 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    for (int i = 2; i < INPUT_MAX_JOYSTICK_BUTTONS; i++) {
        CHECK_INTEQUAL(events[i-1].type, INPUT_EVENT_JOYSTICK);
        CHECK_INTEQUAL(events[i-1].detail, INPUT_JOYSTICK_BUTTON_DOWN);
        CHECK_DOUBLEEQUAL(events[i-1].timestamp, 3.0);
        CHECK_INTEQUAL(events[i-1].joystick.device, 0);
        CHECK_INTEQUAL(events[i-1].joystick.index, i);
    }

    /* Make sure the state of the value inputs wasn't clobbered. */
    sys_test_time_set_seconds(4.0);
    num_events = 0;
    RawInputValueReport inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 3];
    memcpy(inputs_3, inputs_2, sizeof(inputs_2));
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 1].usage_page =
        HID_PAGE_GENERIC_DESKTOP;
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 1].usage = HID_USAGE_X;
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 1].value = 0;
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 2].usage_page =
        HID_PAGE_GENERIC_DESKTOP;
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 2].usage = HID_USAGE_Y;
    inputs_3[INPUT_MAX_JOYSTICK_BUTTONS + 2].value = 0;
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs_3, sizeof(inputs_3));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    sys_test_time_set_seconds(5.0);
    num_events = 0;
    const RawInputValueReport inputs_4[] = {
        {.usage_page = HID_PAGE_BUTTON, .usage = 1, .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs_4, sizeof(inputs_4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, INPUT_MAX_JOYSTICK_BUTTONS - 1);
    for (int i = 1; i < INPUT_MAX_JOYSTICK_BUTTONS; i++) {
        CHECK_INTEQUAL(events[i-1].type, INPUT_EVENT_JOYSTICK);
        CHECK_INTEQUAL(events[i-1].detail, INPUT_JOYSTICK_BUTTON_UP);
        CHECK_DOUBLEEQUAL(events[i-1].timestamp, 5.0);
        CHECK_INTEQUAL(events[i-1].joystick.device, 0);
        CHECK_INTEQUAL(events[i-1].joystick.index, i);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Rumble with raw-input devices is device-specific, so we need to test
 * with each known device. */
TEST(test_joystick_rawinput_rumble_ds4)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwVendorId = 0x054C;
    rawinput_joystick_info.hid.dwProductId = 0x05C4;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 14;
    rawinput_joystick_num_values = 3;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Z;
    rawinput_joystick_values[1].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[1].IsAbsolute = TRUE;
    rawinput_joystick_values[1].BitSize = 16;
    rawinput_joystick_values[1].ReportCount = 1;
    rawinput_joystick_values[1].LogicalMin = -32767;
    rawinput_joystick_values[1].LogicalMax = 32767;
    rawinput_joystick_values[1].PhysicalMin = -32767;
    rawinput_joystick_values[1].PhysicalMax = 32767;
    rawinput_joystick_values[1].NotRange.Usage = HID_USAGE_RZ;
    rawinput_joystick_values[2].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[2].IsAbsolute = TRUE;
    rawinput_joystick_values[2].BitSize = 4;
    rawinput_joystick_values[2].ReportCount = 1;
    rawinput_joystick_values[2].LogicalMin = 1;
    rawinput_joystick_values[2].LogicalMax = -8;
    rawinput_joystick_values[2].PhysicalMin = 1;
    rawinput_joystick_values[2].PhysicalMax = -8;
    rawinput_joystick_values[2].NotRange.Usage = HID_USAGE_HAT;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 14);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    /* Device initialization. */
    CHECK_INTEQUAL(rawinput_joystick_write_len, 32);
    CHECK_MEMEQUAL(rawinput_joystick_write_buf,
                   "\x05\xFF\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00", 32);

    sys_test_time_set_seconds(2.0);
    rawinput_joystick_write_len = 0;
    sys_input_joystick_rumble(0, 0.4, 0.2, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(rawinput_joystick_write_len, 32);
    CHECK_MEMEQUAL(rawinput_joystick_write_buf,
                   "\x05\xFF\x00\x00\x33\x66\x00\x00"  // Right, then left.
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00", 32);

    sys_test_time_set_seconds(2.5);
    rawinput_joystick_write_len = 0;
    sys_input_update();
    CHECK_INTEQUAL(rawinput_joystick_write_len, 0);  // Rumble still going.

    sys_test_time_set_seconds(3.0);
    rawinput_joystick_write_len = 0;
    sys_input_update();
    CHECK_INTEQUAL(rawinput_joystick_write_len, 32);
    CHECK_MEMEQUAL(rawinput_joystick_write_buf,
                   "\x05\xFF\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x00\x00\x00", 32);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_multiple_devices)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 2;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    rawinput_joystick_info.hid.dwProductId = 2;
    rawinput_joystick_connected[1] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(1));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 2);
    CHECK_TRUE(info.joysticks[1].connected);
    CHECK_INTEQUAL(info.joysticks[1].num_buttons, 2);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 1);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    const RawInputValueReport inputs[] = {
        {.usage_page = HID_PAGE_BUTTON, .usage = 2, .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(1), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    send_raw_input(HRAWINPUT_JOYSTICK(0), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_reconnect)
{
    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 2;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    rawinput_joystick_info.hid.dwProductId = 2;
    rawinput_joystick_connected[1] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(1));
    sys_input_update();
    rawinput_joystick_info.hid.dwVendorId = 1;
    rawinput_joystick_info.hid.dwProductId = 1;  // Same as the first device.
    rawinput_joystick_connected[2] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(2));
    sys_input_update();
    rawinput_joystick_info.hid.dwVendorId = 0;  // Same as the first device.
    rawinput_joystick_connected[3] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(3));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 4);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);
    CHECK_TRUE(info.joysticks[2].connected);
    CHECK_TRUE(info.joysticks[3].connected);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    rawinput_joystick_info.hid.dwProductId = 2;
    rawinput_joystick_connected[1] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(1));
    sys_input_update();
    rawinput_joystick_info.hid.dwVendorId = 1;
    rawinput_joystick_info.hid.dwProductId = 1;
    rawinput_joystick_connected[2] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(2));
    sys_input_update();
    rawinput_joystick_info.hid.dwVendorId = 0;
    rawinput_joystick_connected[3] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(3));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 4);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_FALSE(info.joysticks[2].connected);
    CHECK_FALSE(info.joysticks[3].connected);
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 2);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[2].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 2.0);
    CHECK_INTEQUAL(events[2].joystick.device, 3);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    /* rawinput_joystick_info still has VID/PID = 0000/0001, so this should
     * be reconnected as the fourth joystick (index 3). */
    rawinput_joystick_connected[1] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(1));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 4);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_FALSE(info.joysticks[2].connected);
    CHECK_TRUE(info.joysticks[3].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 3);

    sys_test_time_set_seconds(4.0);
    num_events = 0;
    const RawInputValueReport inputs[] = {
        {.usage_page = HID_PAGE_BUTTON, .usage = 1, .value = 1}};
    send_raw_input(HRAWINPUT_JOYSTICK(1), inputs, sizeof(inputs));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 3);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_steam_gridl_bug)
{
    rawinput_simulate_steam_gridl_bug = 1;

    sys_test_time_set_seconds(1.0);
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_num_values = 1;
    rawinput_joystick_values[0].UsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_values[0].IsRange = TRUE;
    rawinput_joystick_values[0].IsAbsolute = TRUE;
    rawinput_joystick_values[0].BitSize = 16;
    rawinput_joystick_values[0].ReportCount = 1;
    rawinput_joystick_values[0].LogicalMin = -32767;
    rawinput_joystick_values[0].LogicalMax = 32767;
    rawinput_joystick_values[0].PhysicalMin = -32767;
    rawinput_joystick_values[0].PhysicalMax = 32767;
    rawinput_joystick_values[0].Range.UsageMin = HID_USAGE_X;
    rawinput_joystick_values[0].Range.UsageMax = HID_USAGE_Y;
    rawinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    /* Without the workaround for the Steam bug, this call will block
     * indefinitely due to an infinite loop. */
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    /* The Steam bug workaround will cause all rawinput devices to be
     * ignored, so the joystick should not be detected. */
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*************************************************************************/
/******************** Tests: Joystick input (XInput) *********************/
/*************************************************************************/

TEST(test_joystick_xinput_connect)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    DLOG("Waiting for connect detection (1.1 sec)...");
    Sleep(1100);

    sys_test_time_set_seconds(2.0);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 12);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_HOME), -1);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_START), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_SELECT), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_UP), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_LEFT), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_RIGHT), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L1), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R1), 0, 9);
    /* These two will have known button numbers since they are translated
     * from analog inputs. */
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L2), 10);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R2), 11);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L_STICK), 0, 9);
    CHECK_INTRANGE(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R_STICK), 0, 9);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_connect_initial)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 12);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_connect_initial_memory_failures)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SysInputInfo info;

    /* As in the rawinput test. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  ASSERT(sys_input_init(event_callback)); return 0
    CHECK_MEMORY_FAILURES((num_events = 0, sys_input_init(event_callback))
                          && ((sys_input_update(), sys_input_info(&info),
                               info.has_joystick)
                              || (sys_input_cleanup(), 0)));
    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0

    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 12);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_env_disabled)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", "0");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_env_enabled)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", "1");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_env_empty)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", "");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(num_events, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_scan_after_connect)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 12);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    DLOG("Waiting for scan (1.1 sec)...");
    Sleep(1100);

    sys_test_time_set_seconds(3.0);
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_disconnect)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    xinput_joystick_connected[0] = 0;
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_copy_name)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    SetEnvironmentVariable("SIL_WINDOWS_USE_XINPUT", "1");
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "XInput");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_button_input)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(1.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN));

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons =
        XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_BACK;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_SELECT));

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_BACK;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_analog_trigger_input)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(1.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.bLeftTrigger = 255;
    xinput_joystick_state.Gamepad.bRightTrigger = 135;  // Just short of the midpoint + debounce.
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_L2));

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.bLeftTrigger = 120;  // Just beyond the midpoint - debounce.
    xinput_joystick_state.Gamepad.bRightTrigger = 136;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_R2));

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.bLeftTrigger = 119;
    xinput_joystick_state.Gamepad.bRightTrigger = 255;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_L2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_dpad_input)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(1.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_UP;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_UP
                                           | XINPUT_GAMEPAD_DPAD_RIGHT;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_DOWN
                                           | XINPUT_GAMEPAD_DPAD_LEFT;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    sys_test_time_set_seconds(4.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_UP
                                           | XINPUT_GAMEPAD_DPAD_DOWN
                                           | XINPUT_GAMEPAD_DPAD_LEFT
                                           | XINPUT_GAMEPAD_DPAD_RIGHT;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    sys_test_time_set_seconds(4.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = 0;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);  // No change, so no event is generated.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_stick_input)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(1.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.sThumbLX = -32768;
    xinput_joystick_state.Gamepad.sThumbLY = 0;
    xinput_joystick_state.Gamepad.sThumbRX = -16384;
    xinput_joystick_state.Gamepad.sThumbRY = 32767;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, -0.5);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    /* A change in either axis of a stick should trigger an event. */
    sys_test_time_set_seconds(2.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.sThumbLX = -32768;
    xinput_joystick_state.Gamepad.sThumbLY = -32768;
    xinput_joystick_state.Gamepad.sThumbRX = 0;
    xinput_joystick_state.Gamepad.sThumbRY = 32767;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_simple_joystick)
{
    sys_test_time_set_seconds(1.0);
    xinput_joystick_caps = xinput_simple_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 1);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    /* We deliberately set unused bits here to verify that they are ignored. */
    xinput_joystick_state.Gamepad.wButtons = ~(WORD)XINPUT_GAMEPAD_DPAD_UP;
    xinput_joystick_state.Gamepad.bLeftTrigger = 255;   // Should be ignored.
    xinput_joystick_state.Gamepad.bRightTrigger = 255;  // Should be ignored.
    xinput_joystick_state.Gamepad.sThumbLX = -16384;
    xinput_joystick_state.Gamepad.sThumbLY = 32767;
    xinput_joystick_state.Gamepad.sThumbRX = -32768;    // Should be ignored.
    xinput_joystick_state.Gamepad.sThumbRY = 0;         // Should be ignored.
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -0.5);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_rumble)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    sys_test_time_set_seconds(1.0);
    sys_input_joystick_rumble(0, 0.4, 0.2, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(xinput_joystick_vibration.wLeftMotorSpeed, 0x6666);
    CHECK_INTEQUAL(xinput_joystick_vibration.wRightMotorSpeed, 0x3333);

    sys_test_time_set_seconds(1.5);
    rawinput_joystick_write_len = 0;
    sys_input_update();
    /* Rumble should still be going. */
    CHECK_INTEQUAL(xinput_joystick_vibration.wLeftMotorSpeed, 0x6666);
    CHECK_INTEQUAL(xinput_joystick_vibration.wRightMotorSpeed, 0x3333);

    sys_test_time_set_seconds(2.0);
    rawinput_joystick_write_len = 0;
    sys_input_update();
    CHECK_INTEQUAL(xinput_joystick_vibration.wLeftMotorSpeed, 0);
    CHECK_INTEQUAL(xinput_joystick_vibration.wRightMotorSpeed, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_xinput_multiple_devices)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    xinput_joystick_connected[1] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);

    xinput_joystick_connected[0] = 0;
    xinput_joystick_connected[1] = 0;
    sys_input_update();
    sys_input_info(&info);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);

    xinput_joystick_connected[1] = 1;  // Should keep device index 1.
    DLOG("Waiting for scan (1.1 sec)...");
    Sleep(1100);
    sys_input_update();
    sys_input_info(&info);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);

    sys_test_time_set_seconds(1.0);
    num_events = 0;
    xinput_joystick_state.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(
                       1, INPUT_JOYBUTTON_FACE_DOWN));

    return 1;
}

/*************************************************************************/
/***************** Tests: Joystick input (miscellaneous) *****************/
/*************************************************************************/

TEST(test_joystick_xinput_overwrite_rawinput)
{
    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_product[0] = 'T';
    rawinput_joystick_product[1] = 'e';
    rawinput_joystick_product[2] = 's';
    rawinput_joystick_product[3] = 't';
    rawinput_joystick_product[4] = '\0';
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "Test");
    mem_free(name);

    rawinput_joystick_connected[0] = 0;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);

    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    DLOG("Waiting for connect detection (1.1 sec)...");
    Sleep(1100);
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "XInput");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rawinput_overwrite_xinput)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "XInput");
    mem_free(name);

    xinput_joystick_connected[0] = 0;
    DLOG("Waiting for disconnect detection (1.1 sec)...");
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);

    rawinput_joystick_info.hid.usUsagePage = HID_PAGE_GENERIC_DESKTOP;
    rawinput_joystick_info.hid.usUsage = HID_USAGE_JOYSTICK;
    rawinput_joystick_product[0] = 'T';
    rawinput_joystick_product[1] = 'e';
    rawinput_joystick_product[2] = 's';
    rawinput_joystick_product[3] = 't';
    rawinput_joystick_product[4] = '\0';
    rawinput_joystick_num_buttons = 1;
    rawinput_joystick_connected[0] = 1;
    post_message_sync(WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                (LPARAM)HRAWINPUT_JOYSTICK(0));
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "Test");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_mapping_on_disconnected_device)
{
    xinput_joystick_caps = xinput_x360_caps;
    xinput_joystick_connected[0] = 1;
    sys_input_cleanup();
    CHECK_TRUE(sys_input_init(event_callback));

    xinput_joystick_connected[0] = 0;
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);

    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), -1);

    return 1;
}

/*************************************************************************/
/********************** Tests: Keyboard/text input ***********************/
/*************************************************************************/

TEST(test_key)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, '1', 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, '1', 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, '1');
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[1].keyboard.system_key, '1');
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_left_right_mod)
{
    /* Check left versions of Shift/Ctrl/Alt/Windows modifiers, just for
     * completeness. */

    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, VK_SHIFT, 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, VK_SHIFT, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTSHIFT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_SHIFT);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_LEFTSHIFT);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_SHIFT);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_KEYDOWN, VK_CONTROL, 1);
    sys_input_update();
    sys_test_time_set_seconds(2.5);
    post_message_sync(WM_KEYUP, VK_CONTROL, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTCONTROL);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_CONTROL);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_LEFTCONTROL);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_CONTROL);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    post_message_sync(WM_KEYDOWN, VK_MENU, 1);
    sys_input_update();
    sys_test_time_set_seconds(3.5);
    post_message_sync(WM_KEYUP, VK_MENU, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTALT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_MENU);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_LEFTALT);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_MENU);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(4.0);
    post_message_sync(WM_KEYDOWN, VK_LWIN, 1);
    sys_input_update();
    sys_test_time_set_seconds(4.5);
    post_message_sync(WM_KEYUP, VK_LWIN, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTMETA);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_LWIN);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_LEFTMETA);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_LWIN);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    /* Check right versions of Shift/Ctrl/Alt/Windows modifiers.  (The
     * right Windows key doesn't use the KF_EXTENDED flag and thus doesn't
     * have a special case to test, but we test it anyway just for
     * completeness.) */

    num_events = 0;
    sys_test_time_set_seconds(5.0);
    post_message_sync(WM_KEYDOWN, VK_SHIFT, 1<<24 | 1);
    sys_input_update();
    sys_test_time_set_seconds(5.5);
    post_message_sync(WM_KEYUP, VK_SHIFT, 1<<31 | 1<<30 | 1<<24 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_RIGHTSHIFT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_SHIFT);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 5.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_RIGHTSHIFT);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_SHIFT);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(6.0);
    post_message_sync(WM_KEYDOWN, VK_CONTROL, 1<<24 | 1);
    sys_input_update();
    sys_test_time_set_seconds(6.5);
    post_message_sync(WM_KEYUP, VK_CONTROL, 1<<31 | 1<<30 | 1<<24 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 6.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_RIGHTCONTROL);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_CONTROL);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 6.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_RIGHTCONTROL);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_CONTROL);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(7.0);
    post_message_sync(WM_KEYDOWN, VK_MENU, 1<<24 | 1);
    sys_input_update();
    sys_test_time_set_seconds(7.5);
    post_message_sync(WM_KEYUP, VK_MENU, 1<<31 | 1<<30 | 1<<24 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_RIGHTALT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_MENU);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 7.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_RIGHTALT);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_MENU);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(8.0);
    post_message_sync(WM_KEYDOWN, VK_RWIN, 1);
    sys_input_update();
    sys_test_time_set_seconds(8.5);
    post_message_sync(WM_KEYUP, VK_RWIN, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 8.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_RIGHTMETA);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_RWIN);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 8.5);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_RIGHTMETA);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_RWIN);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_unsupported)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, VK_NONAME, 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, VK_NONAME, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, VK_NONAME);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.5);
    CHECK_INTEQUAL(events[1].keyboard.key, 0);
    CHECK_INTEQUAL(events[1].keyboard.system_key, VK_NONAME);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_out_of_range)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, 0x100, 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, 0x100, 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 0x100);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.5);
    CHECK_INTEQUAL(events[1].keyboard.key, 0);
    CHECK_INTEQUAL(events[1].keyboard.system_key, 0x100);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_repeat)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, '1', 1);
    sys_input_update();
    sys_test_time_set_seconds(1.25);
    post_message_sync(WM_KEYDOWN, '1', 1<<30 | 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, '1', 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, '1');
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.25);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[1].keyboard.system_key, '1');
    CHECK_TRUE(events[1].keyboard.is_repeat);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[2].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 1.5);
    CHECK_INTEQUAL(events[2].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[2].keyboard.system_key, '1');
    CHECK_FALSE(events[2].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_char)
{
    sys_input_text_set_state(1, NULL, NULL);

    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_KEYDOWN, '1', 1);
    sys_input_update();
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_KEYUP, '1', 1<<31 | 1<<30 | 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, '1');
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, '1');
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[2].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 1.5);
    CHECK_INTEQUAL(events[2].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[2].keyboard.system_key, '1');

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_utf16_surrogate)
{
    sys_input_text_set_state(1, NULL, NULL);

    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_CHAR, 0xD800 | (0x2345 >> 10), 0);
    post_message_sync(WM_CHAR, 0xDC00 | (0x2345 & 0x3FF), 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].text.ch, 0x12345);

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_utf16_surrogate_lone_high)
{
    sys_input_text_set_state(1, NULL, NULL);

    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_CHAR, 0xD800, 0);
    post_message_sync(WM_CHAR, '1', 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].text.ch, '1');

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_CHAR, 0xDBFF, 0);
    post_message_sync(WM_CHAR, '2', 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].text.ch, '2');

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_utf16_surrogate_lone_low)
{
    sys_input_text_set_state(1, NULL, NULL);

    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_CHAR, 0xDC00, 0);
    post_message_sync(WM_CHAR, '1', 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].text.ch, '1');

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_CHAR, 0xDFFF, 0);
    post_message_sync(WM_CHAR, '2', 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].text.ch, '2');

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_action)
{
    sys_input_text_set_state(1, NULL, NULL);

    static const int event_map[][4] = {
        {VK_BACK,   0, KEY_BACKSPACE,    INPUT_TEXT_BACKSPACE},
        {VK_DELETE, 0, KEY_DELETE,       INPUT_TEXT_DELETE},
        {VK_LEFT,   0, KEY_LEFT,         INPUT_TEXT_CURSOR_LEFT},
        {VK_RIGHT,  0, KEY_RIGHT,        INPUT_TEXT_CURSOR_RIGHT},
        {VK_HOME,   0, KEY_HOME,         INPUT_TEXT_CURSOR_HOME},
        {VK_END,    0, KEY_END,          INPUT_TEXT_CURSOR_END},
        {VK_ESCAPE, 0, KEY_ESCAPE,       INPUT_TEXT_CANCELLED},
        {VK_RETURN, 0, KEY_ENTER,        INPUT_TEXT_DONE},
        {VK_RETURN, 1, KEY_NUMPAD_ENTER, INPUT_TEXT_DONE},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        DLOG("Testing key %d", event_map[i][2]);
        num_events = 0;
        sys_test_time_set_seconds(time);
        post_message_sync(WM_KEYDOWN, event_map[i][0],
                    event_map[i][1]<<24 | 1);
        sys_input_update();
        sys_test_time_set_seconds(time + 0.5);
        post_message_sync(WM_KEYUP, event_map[i][0],
                    1<<31 | 1<<30 | event_map[i][1]<<24 | 1);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 3);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_INTEQUAL(events[0].keyboard.key, event_map[i][2]);
        CHECK_INTEQUAL(events[0].keyboard.system_key, event_map[i][0]);
        CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
        CHECK_INTEQUAL(events[1].detail, event_map[i][3]);
        CHECK_INTEQUAL(events[1].timestamp, time);
        CHECK_INTEQUAL(events[2].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[2].detail, INPUT_KEYBOARD_KEY_UP);
        CHECK_DOUBLEEQUAL(events[2].timestamp, time + 0.5);
        CHECK_INTEQUAL(events[2].keyboard.key, event_map[i][2]);
        CHECK_INTEQUAL(events[2].keyboard.system_key, event_map[i][0]);
    }

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*************************************************************************/
/*************************** Tests: Mouse input **************************/
/*************************************************************************/

TEST(test_mouse_position)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_MOUSEMOVE, 0,
                ((TESTH/2) << 16) | ((TESTW/4) & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_position_out_of_range)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_MOUSEMOVE, 0,
                ((TESTH*3/2) << 16) | ((TESTW*5/4) & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1) / (float)TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1) / (float)TESTH);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_MOUSEMOVE, 0,
                ((unsigned int)(-TESTH/2) << 16)
                    | (unsigned int)((-TESTW/4) & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_buttons)
{
    sys_input_text_set_state(1, NULL, NULL);

    static const int event_map[][3] = {
        {WM_LBUTTONDOWN, MK_LBUTTON, INPUT_MOUSE_LMB_DOWN},
        {WM_LBUTTONUP,   0,          INPUT_MOUSE_LMB_UP},
        {WM_MBUTTONDOWN, MK_MBUTTON, INPUT_MOUSE_MMB_DOWN},
        {WM_MBUTTONUP,   0,          INPUT_MOUSE_MMB_UP},
        {WM_RBUTTONDOWN, MK_RBUTTON, INPUT_MOUSE_RMB_DOWN},
        {WM_RBUTTONUP,   0,          INPUT_MOUSE_RMB_UP},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        num_events = 0;
        sys_test_time_set_seconds(time);
        post_message_sync(event_map[i][0], event_map[i][1],
                    ((TESTH/2) << 16) | ((TESTW/4) & 0xFFFF));
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, event_map[i][2]);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
        CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_scroll)
{
    POINT p;

    sys_test_time_set_seconds(1.0);
    p.x = TESTW/4;
    p.y = TESTH/2;
    ClientToScreen(windows_window(), &p);
    post_message_sync(WM_MOUSEHWHEEL, WHEEL_DELTA/2 << 16,
                (p.y << 16) | (p.x & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[0].mouse.scroll, -0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    p.x = TESTW/2;
    p.y = TESTH/4;
    ClientToScreen(windows_window(), &p);
    post_message_sync(WM_MOUSEHWHEEL,
                (unsigned int)(-2*WHEEL_DELTA) << 16,
                (p.y << 16) | (p.x & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);
    CHECK_INTEQUAL(events[0].mouse.scroll, -2);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    p.x = TESTW/4;
    p.y = TESTH/2;
    ClientToScreen(windows_window(), &p);
    post_message_sync(WM_MOUSEWHEEL,
                (unsigned int)(WHEEL_DELTA*-5/2) << 16,
                (p.y << 16) | (p.x & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[0].mouse.scroll, 2.5);

    num_events = 0;
    sys_test_time_set_seconds(4.0);
    p.x = TESTW/2;
    p.y = TESTH/4;
    ClientToScreen(windows_window(), &p);
    post_message_sync(WM_MOUSEWHEEL, (4*WHEEL_DELTA) << 16,
                (p.y << 16) | (p.x & 0xFFFF));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);
    CHECK_INTEQUAL(events[0].mouse.scroll, -4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_position)
{
    /* FIXME: We don't always get the input event, and if we do, sometimes
     * the reported position is wrong. */
    SKIP("Test is flaky, temporarily disabled.");

    /* This will set the real mouse pointer position, so save and restore
     * it to avoid interfering with whatever else the user may be doing. */
    POINT saved_position;
    GetCursorPos(&saved_position);

    /* Make sure the mouse pointer is not already at the target position. */
    {
        POINT point = {0,0};
        ClientToScreen(windows_window(), &point);
        SetCursorPos(point.x, point.y);
        /* It seems to take a little while for SetCursorPos() to send the
         * mouse movement message, so delay a little bit. */
        Sleep(10);
        windows_flush_message_queue();
        sys_input_update();
        num_events = 0;
    }

    sys_test_time_set_seconds(1.0);
    sys_input_mouse_set_position(0.5, 0.75);
    Sleep(10);  // As above.
    windows_flush_message_queue();
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    SetCursorPos(saved_position.x, saved_position.y);
    return 1;
}

/*************************************************************************/
/************************** Tests: Touch input ***************************/
/*************************************************************************/

/* FIXME: Disabled because Windows 8+ seem to ignore attempts to post
 * synthetic WM_POINTER messages. */
#if 0

TEST(test_touch_input)
{
    /* These tests all require the GetPointerType() function, which is
     * only present in Windows 8 and later. */
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_type_to_return = PT_TOUCH;

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW/2, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW*3/8, TESTH*3/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);

    num_events = 0;
    sys_test_time_set_seconds(4.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    sys_test_time_set_seconds(4.5);
    post_message_sync(WM_POINTERCAPTURECHANGED, 1,
                make_touch_lparam(TESTW/2, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].touch.id, 2);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_CANCEL);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.5);
    CHECK_INTEQUAL(events[0].touch.id, 2);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.25);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_multiple)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_type_to_return = PT_TOUCH;

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW*1/8, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.125);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_POINTERDOWN, 3,
                make_touch_lparam(TESTW*3/8, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 2);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    post_message_sync(WM_POINTERDOWN, 2,
                make_touch_lparam(TESTW*2/8, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 3);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(4.0);
    post_message_sync(WM_POINTERUPDATE, 2,
                make_touch_lparam(TESTW*6/8, TESTH/2));
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW*5/8, TESTH/2));
    post_message_sync(WM_POINTERUPDATE, 3,
                make_touch_lparam(TESTW*7/8, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].touch.id, 3);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.75);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.0);
    CHECK_INTEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.625);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.5);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[2].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 4.0);
    CHECK_INTEQUAL(events[2].touch.id, 2);
    CHECK_FLOATEQUAL(events[2].touch.x, 0.875);
    CHECK_FLOATEQUAL(events[2].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(5.0);
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW*3/8, TESTH*3/4));
    post_message_sync(WM_POINTERUP, 2,
                make_touch_lparam(TESTW*4/8, TESTH*3/4));
    post_message_sync(WM_POINTERUP, 3,
                make_touch_lparam(TESTW*5/8, TESTH*3/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 5.0);
    CHECK_INTEQUAL(events[1].touch.id, 3);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.75);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[2].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 5.0);
    CHECK_INTEQUAL(events[2].touch.id, 2);
    CHECK_FLOATEQUAL(events[2].touch.x, 0.625);
    CHECK_FLOATEQUAL(events[2].touch.y, 0.75);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_out_of_bounds)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_type_to_return = PT_TOUCH;

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(-TESTW/4, -TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0);
    CHECK_FLOATEQUAL(events[0].touch.y, 0);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW*5/4, TESTH*3/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, (float)(TESTW-1)/TESTW);
    CHECK_FLOATEQUAL(events[0].touch.y, (float)(TESTH-1)/TESTH);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_ignore_mouse)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override

    GetPointerType_type_to_return = PT_MOUSE;
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    GetPointerType_type_to_return = PT_TOUCHPAD;
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    post_message_sync(WM_POINTERCAPTURECHANGED, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_repeated_down_event)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_type_to_return = PT_TOUCH;

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(0, 0));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0);
    CHECK_FLOATEQUAL(events[0].touch.y, 0);

    num_events = 0;
    sys_test_time_set_seconds(1.5);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.5);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW/2, TESTH/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW*3/8, TESTH*3/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_table_full)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_type_to_return = PT_TOUCH;

    for (int i = 1; i <= INPUT_MAX_TOUCHES; i++) {
        num_events = 0;
        sys_test_time_set_seconds(i);
        post_message_sync(WM_POINTERDOWN, i,
                    make_touch_lparam(0, 0));
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
        CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
        CHECK_DOUBLEEQUAL(events[0].timestamp, i);
        CHECK_INTEQUAL(events[0].touch.id, i);
        CHECK_FLOATEQUAL(events[0].touch.x, 0);
        CHECK_FLOATEQUAL(events[0].touch.y, 0);
    }

    num_events = 0;
    sys_test_time_set_seconds(1000.0);
    post_message_sync(WM_POINTERDOWN, INPUT_MAX_TOUCHES+1,
                make_touch_lparam(TESTW/4, TESTH/2));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);  // Touch table is full.

    for (int i = 1; i <= INPUT_MAX_TOUCHES; i++) {
        num_events = 0;
        sys_test_time_set_seconds(1000+i);
        post_message_sync(WM_POINTERUP, i,
                    make_touch_lparam(0, 0));
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
        CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
        CHECK_DOUBLEEQUAL(events[0].timestamp, 1000+i);
        CHECK_INTEQUAL(events[0].touch.id, i);
        CHECK_FLOATEQUAL(events[0].touch.x, 0);
        CHECK_FLOATEQUAL(events[0].touch.y, 0);
    }

    num_events = 0;
    sys_test_time_set_seconds(2000.0);
    post_message_sync(WM_POINTERUPDATE, INPUT_MAX_TOUCHES+1,
                make_touch_lparam(TESTW/2, TESTH/4));
    post_message_sync(WM_POINTERUP, INPUT_MAX_TOUCHES+1,
                make_touch_lparam(TESTW*3/8, TESTH*3/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_GetPointerType_failure)
{
    if (!windows_version_is_at_least(WINDOWS_VERSION_8)) {
        SKIP("Touch events not supported on pre-Windows 8.");
    }

    GetModuleHandle_module_to_divert = "user32.dll"; // GetPointerType override
    GetPointerType_fail = 1;

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_POINTERDOWN, 1,
                make_touch_lparam(TESTW/4, TESTH/2));
    post_message_sync(WM_POINTERUPDATE, 1,
                make_touch_lparam(TESTW/2, TESTH/4));
    post_message_sync(WM_POINTERUP, 1,
                make_touch_lparam(TESTW*3/8, TESTH*3/4));
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    GetModuleHandle_module_to_divert = NULL;
    return 1;
}

#endif  // 0

/*************************************************************************/
/************************** Tests: Miscellaneous *************************/
/*************************************************************************/

TEST(init_memory_failure)
{
    SysInputInfo info;
    sys_input_info(&info);
    const int has_joystick = info.has_joystick;

    sys_input_cleanup();
    CHECK_MEMORY_FAILURES(
        sys_input_init(event_callback)
        && ((sys_input_info(&info), info.has_joystick == has_joystick)
            || (sys_input_cleanup(), 0)));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit_from_close)
{
    sys_test_time_set_seconds(1.0);
    post_message_sync(WM_CLOSE, 0, 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit_from_quit)
{
    sys_test_time_set_seconds(1.0);
    /* Deliberately SendMessage() so it doesn't confuse the system into
     * doing anything else (like force-quitting the program). */
    SendMessage(windows_window(), WM_QUIT, 0, 0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_suspend)
{
    /* We don't support suspend/resume on Windows yet, so just check that
     * the associated functions behave properly. */
    CHECK_FALSE(sys_input_is_suspend_requested());
    sys_input_acknowledge_suspend_request();  // Should do nothing.

    return 1;
}

/*-----------------------------------------------------------------------*/

/* For this test, we want to be sure to clean up on return even if the
 * test fails, so that we don't leave input in a grabbed state. */
#undef FAIL_ACTION
#define FAIL_ACTION  result = 0; goto out

TEST(test_grab)
{
    int running_under_wine = 0;
    char *envp = GetEnvironmentStrings();
    const char *s = envp;
    while (*s) {
        if (strncmp(s, "WINE", 4) == 0) {
            running_under_wine = 1;
            break;
        }
        s += strlen(s) + 1;
    }
    FreeEnvironmentStrings(envp);
    if (running_under_wine) {
        SKIP("This test sporadically fails under Wine.");
        /* The reason for the failure is unclear, but GetClipCursor()
         * fails to return the rectangle set by ClipCursor() in some
         * circumstances; it may be that Wine doesn't apply ClipCursor()
         * until it receives a real mouse input event (as opposed to
         * SetCursorPos() or SendInput()). */
    }

    /* This will set the real mouse pointer position, so save and restore
     * it to avoid interfering with whatever else the user may be doing. */
    POINT saved_position;
    GetCursorPos(&saved_position);

    int result = 1;

    /* Input should default to not-grabbed. */
    CHECK_FALSE(get_windows_grab_state());

    sys_input_grab(1);
    CHECK_TRUE(get_windows_grab_state());

    /* Make sure sys_input_grab() doesn't just blindly flip the grab state. */
    sys_input_grab(1);
    CHECK_TRUE(get_windows_grab_state());

    sys_input_grab(0);
    CHECK_FALSE(get_windows_grab_state());

  out:
    ClipCursor(NULL);
    SetCursorPos(saved_position.x, saved_position.y);
    return result;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*************************************************************************/
/*************************************************************************/
