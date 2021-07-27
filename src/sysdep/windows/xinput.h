#ifndef SIL_SRC_SYSDEP_WINDOWS_XINPUT_H
#define SIL_SRC_SYSDEP_WINDOWS_XINPUT_H

/* These declarations are normally found in the system header <XInput.h>,
 * but we include them here for the benefit of systems without that header
 * (such as some versions of MinGW). */

#define XINPUT_DEVTYPE_GAMEPAD  0
#define XINPUT_FLAG_GAMEPAD  (1 << XINPUT_DEVTYPE_GAMEPAD)

#define XINPUT_DEVSUBTYPE_GAMEPAD  1

#define XINPUT_GAMEPAD_DPAD_UP          (1 <<  0)
#define XINPUT_GAMEPAD_DPAD_DOWN        (1 <<  1)
#define XINPUT_GAMEPAD_DPAD_LEFT        (1 <<  2)
#define XINPUT_GAMEPAD_DPAD_RIGHT       (1 <<  3)
#define XINPUT_GAMEPAD_START            (1 <<  4)
#define XINPUT_GAMEPAD_BACK             (1 <<  5)
#define XINPUT_GAMEPAD_LEFT_THUMB       (1 <<  6)
#define XINPUT_GAMEPAD_RIGHT_THUMB      (1 <<  7)
#define XINPUT_GAMEPAD_LEFT_SHOULDER    (1 <<  8)
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   (1 <<  9)
#define XINPUT_GAMEPAD_A                (1 << 12)
#define XINPUT_GAMEPAD_B                (1 << 13)
#define XINPUT_GAMEPAD_X                (1 << 14)
#define XINPUT_GAMEPAD_Y                (1 << 15)

typedef struct XINPUT_GAMEPAD XINPUT_GAMEPAD;
struct XINPUT_GAMEPAD {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
};

typedef struct XINPUT_VIBRATION XINPUT_VIBRATION;
struct XINPUT_VIBRATION {
    WORD wLeftMotorSpeed;
    WORD wRightMotorSpeed;
};

typedef struct XINPUT_CAPABILITIES XINPUT_CAPABILITIES;
struct XINPUT_CAPABILITIES {
    BYTE Type;
    BYTE SubType;
    WORD Flags;
    XINPUT_GAMEPAD Gamepad;
    XINPUT_VIBRATION Vibration;
};

typedef struct XINPUT_STATE XINPUT_STATE;
struct XINPUT_STATE {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
};

extern DWORD WINAPI XInputGetCapabilities(
    DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities);
extern DWORD WINAPI XInputGetState(
    DWORD dwUserIndex, XINPUT_STATE *pState);
extern DWORD WINAPI XInputSetState(
    DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

#endif  // SIL_SRC_SYSDEP_WINDOWS_XINPUT_H
