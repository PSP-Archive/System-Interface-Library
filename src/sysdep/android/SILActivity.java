/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/SILActivity.java: Wrapper for the
 * Android NativeActivity class which uses a fullscreen window.  This
 * wrapper also provides an interface for passing arguments to native code
 * (like command-line arguments on a PC system), as well as stubs over
 * other Java interfaces to reduce the amount of JNI mess required in
 * native code.
 */

/* Replace "com.example.sil_app" with an appropriate package name for your
 * program.  (Normally, this will be done by the build script.) */
package com.example.sil_app;

import android.app.NativeActivity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Process;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseArray;
import android.view.Display;
import android.view.InputDevice;
import android.view.InputDevice.MotionRange;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import java.io.File;
import java.util.HashMap;
import java.util.Locale;
import java.util.concurrent.Semaphore;
import java.util.concurrent.locks.ReentrantLock;

public class SILActivity extends NativeActivity {

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Cached information about input devices. */
class InputDeviceInfo {
    boolean is_dpad;      // Used if no joystick is available.
    boolean is_joystick;  // Only for full joysticks or gamepads.
    boolean is_keyboard;  // Only for full (alphabetic) keyboards.
    boolean is_mouse;     // For both true mice and touchpads.
};
private SparseArray<InputDeviceInfo> input_device_info;

/* Presence flags for various input device types. */
private boolean has_joystick, has_keyboard, has_mouse;

/* Window width and height, set from the UI thread when the window first
 * gains focus. */
private int window_width, window_height;

/* Receiver object for headphones-removed events. */
private BroadcastReceiver audio_became_noisy_receiver;
/* Flag set when a headphones-removed event is received. */
private boolean audio_became_noisy;

/* Flag to store the system UI visibility state (since input dialogs can
 * clobber it). */
private boolean system_ui_visible;

/* Run lock for the UI thread. */
private ReentrantLock ui_thread_lock;

/* List of known granted/denied permissions. */
private HashMap<String,Boolean> requested_permissions;
/* Flag indicating whether a permission request is in progress. */
private boolean reqperm_waiting;

/*************************************************************************/
/*************************** Callback methods ****************************/
/*************************************************************************/

@Override
protected void onCreate(Bundle savedInstanceState)
{
    input_device_info = new SparseArray<InputDeviceInfo>(16);
    audio_became_noisy_receiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            audio_became_noisy = true;
        }
    };
    system_ui_visible = false;
    ui_thread_lock = new ReentrantLock();
    requested_permissions = new HashMap<String,Boolean>();

    try {
        Process.setThreadPriority(Process.THREAD_PRIORITY_FOREGROUND);
    } catch (SecurityException e) {}

    getWindow().requestFeature(Window.FEATURE_NO_TITLE);
    getWindow().setFlags(
        WindowManager.LayoutParams.FLAG_FULLSCREEN,
        (WindowManager.LayoutParams.FLAG_FULLSCREEN
         | WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN));
    super.onCreate(savedInstanceState);
}

/*-----------------------------------------------------------------------*/

@Override
protected void onPause()
{
    unregisterReceiver(audio_became_noisy_receiver);
    super.onPause();
}

/*-----------------------------------------------------------------------*/

@Override
protected void onResume()
{
    registerReceiver(
        audio_became_noisy_receiver,
        new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
    setSystemUiVisible(system_ui_visible);
    super.onResume();
}

/*-----------------------------------------------------------------------*/

/**
 * onKeyDown:  Event handler for keypresses.  We intercept KEYCODE_BACK to
 * prevent the app from being terminated by a Back button press.
 *
 * Note that as of SDK version 14, this function seems to no longer be called.
 */
@Override
public boolean onKeyDown(int keyCode, KeyEvent event)
{
    if (keyCode == KeyEvent.KEYCODE_BACK) {
        return true;
    }
    return super.onKeyDown(keyCode, event);
}

/*-----------------------------------------------------------------------*/

/**
 * onRequestPermissionsResult:  Event handler for requestPermissions() result.
 */
@Override
public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                       int[] grantResults)
{
    for (int i = 0; i < permissions.length; i++) {
        requested_permissions.put(
            permissions[i],
            grantResults[i] == PackageManager.PERMISSION_GRANTED);
    }
    reqperm_waiting = false;
}

/*-----------------------------------------------------------------------*/

/**
 * onWindowFocusChanged:  Event handler for window focus changes.  We use
 * this to set the window size if it hasn't yet been detected, since these
 * values may not be available at program startup.
 */
@Override
public void onWindowFocusChanged(boolean hasFocus)
{
    if (window_width == 0) {
        window_width = getContentView().getWidth();
    }
    if (window_height == 0) {
        window_height = getContentView().getHeight();
    }
}

/*************************************************************************/
/******************* General-purpose utility routines ********************/
/*************************************************************************/

/**
 * getClass:  Return the named class.  Intended for calling from native
 * code, since the JNI FindClass() function only works for base Java
 * classes.
 *
 * [Parameters]
 *     name: Class name.
 * [Return value]
 *     Class object, or null if the class is unknown.
 */
public Class<?> getClass(String name)
{
    try {
        return Class.forName(name);
    } catch (ClassNotFoundException e) {
        return null;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * getArgs:  Return a string containing arguments for the native program.
 * This implementation always returns an empty string; override it in a
 * subclass to pass different arguments.
 *
 * [Return value]
 *     Argument string.
 */
public String getArgs()
{
    return "";
}

/*-----------------------------------------------------------------------*/

/**
 * getAPILevel:  Return the Android API level (from Build.VERSION_CODES)
 * implemented by the runtime environment.
 *
 * [Return value]
 *     Runtime API level.
 */
public int getAPILevel()
{
    return Build.VERSION.SDK_INT;
}

/*-----------------------------------------------------------------------*/

/**
 * getBuildInfo:  Return a string from the android.os.Build class.
 *
 * The following ID values are recognized (passed as integers to simplify
 * calling from native code):
 *     1 = Build.BOARD
 *     2 = Build.BOOTLOADER
 *     3 = Build.BRAND
 *     4 = Build.CPU_ABI
 *     5 = Build.CPU_ABI2
 *     6 = Build.DEVICE
 *     7 = Build.DISPLAY
 *     8 = Build.FINGERPRINT
 *     9 = Build.HARDWARE
 *    10 = Build.HOST
 *    11 = Build.ID
 *    12 = Build.MANUFACTURER
 *    13 = Build.MODEL
 *    14 = Build.PRODUCT
 *    15 = Build.RADIO / Build.getRadioVersion()
 *    16 = Build.SERIAL
 *    17 = Build.TAGS
 *    18 = Build.TYPE
 *    19 = Build.USER
 *
 * [Parameters]
 *     id: String ID (see above).
 * [Return value]
 *     Corresponding string.
 */
@SuppressWarnings("deprecation")  // for Build.RADIO
public String getBuildInfo(int id)
{
    switch (id) {
        case  1: return Build.BOARD;
        case  2: return Build.BOOTLOADER;
        case  3: return Build.BRAND;
        case  4: return Build.CPU_ABI;
        case  5: return Build.CPU_ABI2;
        case  6: return Build.DEVICE;
        case  7: return Build.DISPLAY;
        case  8: return Build.FINGERPRINT;
        case  9: return Build.HARDWARE;
        case 10: return Build.HOST;
        case 11: return Build.ID;
        case 12: return Build.MANUFACTURER;
        case 13: return Build.MODEL;
        case 14: return Build.PRODUCT;
        case 15:
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH){
              return Build.getRadioVersion();
          } else {
              return Build.RADIO;
          }
        case 16: return Build.SERIAL;
        case 17: return Build.TAGS;
        case 18: return Build.TYPE;
        case 19: return Build.USER;
    }
    return "(invalid ID " + id + ")";
}

/*-----------------------------------------------------------------------*/

/**
 * getInternalDataPath, getExternalDataPath:  Return the internal or
 * external data directory path for the application.  These are shortcuts
 * for getFilesDir().getAbsolutePath() and
 * getExternalFilesDir(null).getAbsolutePath(), respectively, and are
 * provided mainly to simplify native code.
 *
 * [Return value]
 *     Absolute path of the data directory, or the empty string if not
 *     available.
 */
public String getInternalDataPath()
{
    File dir = getFilesDir();
    if (dir != null) {
        return dir.getAbsolutePath();
    } else {
        return "";
    }
}

public String getExternalDataPath()
{
    String state = Environment.getExternalStorageState();
    if (state.equals(Environment.MEDIA_MOUNTED)) {
        File dir = getExternalFilesDir(null);
        if (dir != null) {
            return dir.getAbsolutePath();
        } else {
            return "";
        }
    } else {
        return "";
    }
}

/*-----------------------------------------------------------------------*/

/**
 * getExpansionFilePath:  Return the path of an expansion file downloaded
 * separately from the application.
 *
 * [Parameters]
 *     index: File index.  For Google Play, 0 is the "main" file and 1 is
 *         the "patch" file.
 * [Return value]
 *     Absolute path of the expansion file, or null if no such file exists.
 */
public String getExpansionFilePath(int index)
{
    return Downloader.getFilePath(this, index);
}

/*-----------------------------------------------------------------------*/

/**
 * getResourceString:  Return a string from the Android string resources.
 * If a localized version of the string is available for the user's
 * preferred locale, that version is returned.
 *
 * [Parameters]
 *     name: String resource name.
 * [Return value]
 *     String text, or null if the resource is not found.
 */
public String getResourceString(String name)
{
    try {
        return getString(R.string.class.getField(name).getInt(null));
    } catch (NoSuchFieldException e) {
        return null;
    } catch (IllegalAccessException e) {
        return null;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * getUserLocale:  Return a locale string of the form "aa", "aa_BB", or
 * "aa_[BB]_VARIANT" (BB may be omitted) describing the user's preferred
 * locale.  "aa" is an ISO 639-1 language code, and "BB" is an ISO 3166-1
 * country code (both exactly two letters if present).
 *
 * [Return value]
 *     User's preferred locale.
 */
String getUserLocale()
{
    return Locale.getDefault().toString();
}

/*-----------------------------------------------------------------------*/

/**
 * keepScreenOn:  Enable or disable the FLAG_KEEP_SCREEN_ON window flag.
 * When enabled, the device will not go to sleep as long as the app is
 * displayed, regardless of the user's configured idle timeout.
 *
 * [Parameters]
 *     enable: True to keep the screen on, false to restore normal behavior.
 */
public void keepScreenOn(boolean enable)
{
    if (enable) {
        runOnUiThread(new Runnable() {public void run() {
            getWindow().addFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }});
    } else {
        runOnUiThread(new Runnable() {public void run() {
            getWindow().clearFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }});
    }
}

/*-----------------------------------------------------------------------*/

/**
 * lockUiThread, unlockUiThread:  Lock or unlock the UI thread so that
 * another thread can modify global UI state.  (Java calls must still be
 * performed on the UI thread itself.)
 */
public void lockUiThread()
{
    ui_thread_lock.lock();

    runOnUiThread(new Runnable() {public void run() {
        ui_thread_lock.lock();    // Blocks until unlockUiThread() is called.
        ui_thread_lock.unlock();
    }});
}

public void unlockUiThread()
{
    ui_thread_lock.unlock();
}

/*-----------------------------------------------------------------------*/

/**
 * openURL:  Open the given URL in the system web browser.
 *
 * [Parameters]
 *     url: URL to open.
 */
void openURL(String url)
{
    Intent intent = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url));
    startActivity(intent);
}

/*-----------------------------------------------------------------------*/

/**
 * Synchronously request a permission from the user.
 *
 * [Parameters]
 *     name: Name of permission to request (field name in Manifest.permission).
 * [Return value]
 *     1 if the permission was granted, 0 if it was denied, -1 if the
 */
int requestPermission(String name)
{
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
        /* Permission granting was done at install time. */
        return 1;
    }

    String permission;
    try {
        permission = (String)android.Manifest.permission.class.getDeclaredField(name).get(null);
    } catch (Exception e) {
        Log.w(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Failed to look up permission " + name);
        return 0;
    }
    Boolean status = requested_permissions.get(permission);
    if (status != null) {
        return status ? 1 : 0;
    }

    if (!reqperm_waiting) {
        reqperm_waiting = true;
        Log.d(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Requesting permission " + permission);
        requestPermissions(new String[]{permission}, 0);
    }

    status = requested_permissions.get(permission);
    if (status != null) {
        return status ? 1 : 0;
    } else {
        return -1;
    }
}

/*************************************************************************/
/******************* Display-related utility routines ********************/
/*************************************************************************/

/**
 * getContentView:  Return the content view for the window.  A counterpart
 * to setContentView() that is bizarrely missing from the API.
 *
 * [Return value]
 *     Content view, or null if none has been set.
 */
public View getContentView()
{
    View content_view = gcv_recurse(getWindow().getDecorView());
    if (content_view == null) {
        Log.w(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Failed to get content view!");
        Log.i(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Current view hierarchy:");
        gcv_print(getWindow().getDecorView(), "");
    }
    return content_view;
}

private View gcv_recurse(View view)
{
    /* Currently (SDK version 15), the view class is a nested class named
     * android.app.NativeActivity$NativeContentView.  We assume it will
     * always contain "ContentView" and test for that to find the view. */
    if (view.getClass().getName().contains("ContentView")) {
        return view;
    }
    if (view instanceof ViewGroup) {
        ViewGroup group = (ViewGroup)view;
        for (int i = 0; i < group.getChildCount(); i++) {
            View result = gcv_recurse(group.getChildAt(i));
            if (result != null) {
                return result;
            }
        }
    }
    return null;
}

private void gcv_print(View view, String indent)
{
    Log.i(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
          indent + view.getClass().getName());
    if (view instanceof ViewGroup) {
        ViewGroup group = (ViewGroup)view;
        for (int i = 0; i < group.getChildCount(); i++) {
            gcv_print(group.getChildAt(i), indent + "   ");
        }
    }
}

/*-----------------------------------------------------------------------*/

/**
 * getDisplayWidth, getDisplayHeight:  Return the size of the usable region
 * of the display (the region excluding the system navigation bar) in its
 * current orientation.
 *
 * [Return value]
 *     Display width or height, in pixels.
 */
public int getDisplayWidth()
{
    int tries = 0;
    while (window_width == 0) {
        tries++;
        if (tries >= 1000) {
            Log.e(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                  "Width was not set!");
            return 0;
        }
        try {Thread.sleep(1);} catch (InterruptedException e) {}
    }
    return window_width;
}

public int getDisplayHeight()
{
    int tries = 0;
    while (window_height == 0) {
        tries++;
        if (tries >= 1000) {
            Log.e(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                  "Width was not set!");
            return 0;
        }
        try {Thread.sleep(1);} catch (InterruptedException e) {}
    }
    return window_height;
}

/*-----------------------------------------------------------------------*/

/**
 * getDisplayFullWidth, getDisplayFullHeight:  Return the size of the
 * entire display in its current orientation.  This is intended only for
 * reference, and does not necessarily represent the size available for
 * rendering.
 *
 * [Return value]
 *     Display width or height, in pixels.
 */
@SuppressWarnings("deprecation")  // for getWidth()
public int getDisplayFullWidth()
{
    Display display = getWindowManager().getDefaultDisplay();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
        Point size = new Point();
        display.getRealSize(size);
        return size.x;
    } else if (Build.VERSION.SDK_INT>=Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
        Point size = new Point();
        display.getSize(size);
        return size.x;
    } else {
        return display.getWidth();
    }
}

@SuppressWarnings("deprecation")  // for getHeight()
public int getDisplayFullHeight()
{
    Display display = getWindowManager().getDefaultDisplay();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
        Point size = new Point();
        display.getRealSize(size);
        return size.y;
    } else if (Build.VERSION.SDK_INT>=Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
        Point size = new Point();
        display.getSize(size);
        return size.y;
    } else {
        return display.getHeight();
    }
}

/*-----------------------------------------------------------------------*/

/**
 * getDisplaySizeInches:  Return the approximate diagonal size of the
 * display, in inches.  The accuracy of this value depends on whether the
 * underlying drivers report the correct dots-per-inch values to the OS.
 *
 * [Return value]
 *     Approximate display size (diagonal), in inches.
 */
public float getDisplaySizeInches()
{
    DisplayMetrics metrics = new DisplayMetrics();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
    } else {
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
    }
    float width = metrics.widthPixels / metrics.xdpi;
    float height = metrics.heightPixels / metrics.ydpi;
    return (float)Math.sqrt(width*width + height*height);
}

/*************************************************************************/
/******************** Audio-related utility routines *********************/
/*************************************************************************/

/**
 * getAudioOutputRate:  Return the audio hardware's native output sampling
 * rate.
 *
 * [Return value]
 *     Hardware output sampling rate, or zero if unknown.
 */
public int getAudioOutputRate()
{
    return AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_SYSTEM);
}

/*-----------------------------------------------------------------------*/

/**
 * getAudioBecameNoisy:  Return whether a headphones-removed event has been
 * detected since the last call to clearAudioBecameNoisy().
 *
 * [Return value]
 *     True if a headphones-removed event has been detected, false if not.
 */
public boolean getAudioBecameNoisy()
{
    return audio_became_noisy;
}

/*-----------------------------------------------------------------------*/

/**
 * clearAudioBecameNoisy:  Clear the headphones-removed event flag.
 */
public void clearAudioBecameNoisy()
{
    audio_became_noisy = false;
}

/*************************************************************************/
/******************** Input-related utility routines *********************/
/*************************************************************************/

/**
 * scanInputDevices:  Scan for new or removed input devices, and update
 * internal data accordingly.
 *
 * This function must not be called from multiple threads simultaneously.
 *
 * [Return value]
 *     True if a change in the set of connected input devices was detected,
 *     false if not.
 */
public boolean scanInputDevices()
{
    int[] ids = InputDevice.getDeviceIds();
    if (ids == null && input_device_info == null) {
        return false;
    }
    if (input_device_info != null && ids.length == input_device_info.size()) {
        boolean match = true;
        for (int i = 0; i < ids.length; i++) {
            if (input_device_info.indexOfKey(ids[i]) < 0) {
                match = false;
                break;
            }
        }
        if (match) {
            return false;
        }
    }

    input_device_info.clear();
    has_joystick = false;
    has_keyboard = false;
    has_mouse = false;

    if (ids == null) {
        return true;
    }

    for (int i = 0; i < ids.length; i++) {
        int id = ids[i];
        InputDevice dev = InputDevice.getDevice(id);
        Log.d(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG, dev.toString());
        /* SOURCE_GAMEPAD and SOURCE_JOYSTICK were only added in ICS, so
         * provide a fallback for previous versions (even though they will
         * normally never match). */
        int SOURCE_GAMEPAD, SOURCE_JOYSTICK;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            SOURCE_GAMEPAD  = InputDevice.SOURCE_GAMEPAD;
            SOURCE_JOYSTICK = InputDevice.SOURCE_JOYSTICK;
        } else {
            SOURCE_GAMEPAD  = 0x00000401;
            SOURCE_JOYSTICK = 0x01000010;
        }
        int sources = dev.getSources();
        InputDeviceInfo info = new InputDeviceInfo();
        /* VIRTUAL_KEYBOARD is listed as being added in Honeycomb (API
         * level 11), but it's actually present in Gingerbread 2.3.3 (API
         * level 10), so we can use it with impunity.  That said, there
         * might be some 2.3.3 device out there that actually uses -1 as
         * a valid device ID, so we condition the check on HONEYCOMB anyway. */
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB
         && id == android.view.KeyCharacterMap.VIRTUAL_KEYBOARD) {
            /* Android 4.x (at least) report the virtual keyboard as also
             * being a D-pad, which isn't what we mean by "D-pad".  Force
             * appropriate info flags. */
            info.is_dpad = false;
            info.is_joystick = false;
            info.is_keyboard = true;
            info.is_mouse = false;
        } else {
            info.is_dpad =
                (sources & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
            info.is_joystick =
                (sources & SOURCE_GAMEPAD) == SOURCE_GAMEPAD
                || (sources & SOURCE_JOYSTICK) == SOURCE_JOYSTICK;
            info.is_keyboard =
                (sources & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD
                && dev.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC;
            info.is_mouse =
                (sources & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE
                || (sources & InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD;
        }
        input_device_info.put(id, info);
        if (info.is_joystick) {
            Log.d(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                  "Found joystick (device " + id + ")");
            has_joystick = true;
        }
        if (info.is_keyboard) {
            Log.d(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                  "Found keyboard (device " + id + ")");
            has_keyboard = true;
        }
        if (info.is_mouse) {
            Log.d(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                  "Found mouse/touchpad (device " + id + ")");
            has_mouse = true;
        }
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * hasJoystick, hasKeyboard, hasMouse:  Return whether the device has
 * joystick, keyboard, or mouse input, respectively.
 *
 * [Return value]
 *     True if the given type of input is available, false if not.
 */
public boolean hasJoystick()
{
    return has_joystick;
}

public boolean hasKeyboard()
{
    return has_keyboard;
}

public boolean hasMouse()
{
    return has_mouse;
}

/*-----------------------------------------------------------------------*/

/**
 * getDeviceName:  Return the name of the given input device.
 *
 * [Parameters]
 *     id: Input device ID.
 * [Return value]
 *     Device name.
 */
public String getDeviceName(int id)
{
    return InputDevice.getDevice(id).getName();
}

/*-----------------------------------------------------------------------*/

/**
 * isInputDeviceDpad, isInputDeviceJoystick, isInputDeviceKeyboard,
 * isInputDeviceMouse:  Return whether the given input device is a
 * non-joystick D-pad, full joystick (including gamepads), full alphabetic
 * keyboard, or mouse (including touchpads), respectively.
 *
 * [Parameters]
 *     id: Input device ID.
 * [Return value]
 *     True if the given device is of the given type, false if not.
 */
public boolean isInputDeviceDpad(int id)
{
    InputDeviceInfo info = input_device_info.get(id);
    return info != null && info.is_dpad;
}

public boolean isInputDeviceJoystick(int id)
{
    InputDeviceInfo info = input_device_info.get(id);
    return info != null && info.is_joystick;
}

public boolean isInputDeviceKeyboard(int id)
{
    InputDeviceInfo info = input_device_info.get(id);
    return info != null && info.is_keyboard;
}

public boolean isInputDeviceMouse(int id)
{
    InputDeviceInfo info = input_device_info.get(id);
    return info != null && info.is_mouse;
}

/*-----------------------------------------------------------------------*/

/**
 * getJoystickId:  Return the input device ID for the index-th joystick,
 * or zero if there is no such joystick.
 *
 * If there are no joystick- or gamepad-class devices connected, this
 * function instead returns the input device ID for the index-th D-pad,
 * or zero if there is none.  This is intended to support devices such as
 * the Xperia Play which have a built-in gamepad but run an older version
 * of Android that does not support the "gamepad" or "joystick" input type.
 *
 * The order of joysticks may change after a scanInputDevices() call that
 * returns true.
 *
 * [Parameters]
 *     index: Joystick index (zero-based).
 * [Return value]
 *     Input device ID, or zero if index is negative or >= number of joysticks.
 */
public int getJoystickId(int index)
{
    int joy_count = 0;
    for (int i = 0; i < input_device_info.size(); i++) {
        InputDeviceInfo info = input_device_info.valueAt(i);
        if (has_joystick ? info.is_joystick : info.is_dpad) {
            if (joy_count == index) {
                return input_device_info.keyAt(i);
            }
            joy_count++;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * getAxisThreshold:  Return the input threshold (dead-zone radius) for
 * the given joystick axis.
 *
 * This function always returns zero if running on a pre-Honeycomb MR1
 * version of Android.
 *
 * [Parameters]
 *     id: Input device ID.
 *     axis: Axis (MotionEvent.AXIS_*).
 * [Return value]
 *     Input threshold for the given axis, or 0.0 if the device is not a
 *     joystick or does not support the given axis.
 */
public float getAxisThreshold(int id, int axis)
{
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR1) {
        InputDeviceInfo info = input_device_info.get(id);
        if (info != null && info.is_joystick) {
            MotionRange range = InputDevice.getDevice(id).getMotionRange(axis);
            if (range != null) {
                return range.getFlat();
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * doesJoystickRumble:  Return whether the given input device is a joystick
 * with force feedback support.
 *
 * This function always returns false if running on a pre-Jellybean version
 * of Android.
 *
 * [Parameters]
 *     id: Input device ID.
 * [Return value]
 *     True if the device is a joystick with force feedback support,
 *     false otherwise.
 */
public boolean doesJoystickRumble(int id)
{
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
        InputDeviceInfo info = input_device_info.get(id);
        if (info != null && info.is_joystick) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null) {
                return device.getVibrator().hasVibrator();
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return false;
    }
}

/*************************************************************************/
/********************** UI-related utility routines **********************/
/*************************************************************************/

/**
 * setSystemUiVisible:  Set whether the system navigation bar (with the
 * Back/Home/Recent softkeys) should be displayed.  Meaningless on devices
 * with physical navigation buttons.
 *
 * The precise behavior of this function depends on the OS version:
 *
 * - In Android 4.4 (KitKat) and later, this enables immersive mode, in
 *   which the system UI is hidden and can be revealed by swiping in from
 *   the edge of the screen.
 *
 * - In Android 3.0 (Honeycomb) through 4.3 (Jelly Bean MR2), this enables
 *   "lights-out" mode, in which the softkeys are dimmed (though the
 *   navigation bar itself remains displayed).
 *
 * - In versions of Android before 3.0, this function has no effect.
 *   (These versions of Android only supported devices with physical
 *   navigation buttons.)
 *
 * [Parameters]
 *     visible: True to show the system navigation bar, false to hide it
 *         (enables "lights out" mode on Honeycomb and later API versions,
 *         but does not remove the bar itself).
 */
public void setSystemUiVisible(final boolean visible)
{
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
        return;
    }

    runOnUiThread(new Runnable() {public void run() {
        View view = getContentView();
        if (view == null) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            view.setSystemUiVisibility(
                visible ? View.SYSTEM_UI_FLAG_VISIBLE
                        : (View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                           | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY));
        } else if (Build.VERSION.SDK_INT
                   >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            view.setSystemUiVisibility(
                visible ? View.SYSTEM_UI_FLAG_VISIBLE
                        : View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else {  // Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB
            view.setSystemUiVisibility(
                visible ? 1    // View.STATUS_BAR_VISIBLE
                        : 0);  // View.STATUS_BAR_HIDDEN
        }
    }});
    system_ui_visible = visible;
}

/*-----------------------------------------------------------------------*/

/**
 * getSystemUiVisible:  Return whether the system navigation bar (with the
 * Back/Home/Recent softkeys) is displayed on Android 3.0+ devices without
 * physical navigation buttons.
 *
 * [Return value]
 *     True if the navigation bar is displayed, false if not.
 */
public boolean getSystemUiVisible()
{
    return system_ui_visible;
}

/*-----------------------------------------------------------------------*/

/**
 * showAlert:  Show an alert dialog with the given parameters, and wait for
 * the user to activate one of the buttons.
 *
 * This function must not be called from the UI thread, since doing so will
 * deadlock.
 *
 * [Parameters]
 *     title: Dialog title, or null for none.
 *     text: Dialog text, or null for none.
 *     button_yes: Text for the positive ("Yes"/"OK") button, or null to
 *         not show the button.
 *     button_no: Text for the negative ("No"/"Cancel") button, or null
 *         to not show the button.
 *     button_other: Text for the neutral button, or null to not show the
 *         button.
 * [Return value]
 *     1 if the positive button was activated; 0 if the negative button was
 *     activated; -1 if the neutral button was activated.
 */
int showAlert(String title, String text,
              String button_yes, String button_no, String button_other)
{
    int result =
        new Dialog(this, title, text, button_yes, button_no, button_other)
        .showAndWait();
    if (!system_ui_visible) {
        // UI visibility state may have been clobbered by the dialog.
        setSystemUiVisible(false);
    }
    return result;
}

/*-----------------------------------------------------------------------*/

/**
 * showInputDialog:  Show an input dialog with the given parameters.
 *
 * [Parameters]
 *     title: Dialog title, or null for none.
 *     text: Initial text, or null for none.
 * [Return value]
 *     New InputDialog object.
 */
InputDialog showInputDialog(String title, String text)
{
    InputDialog dialog = new InputDialog(this, title, text);
    dialog.show();
    return dialog;
}

/*-----------------------------------------------------------------------*/

/**
 * isInputDialogFinished:  Return whether the given input dialog has
 * received input from the user.
 *
 * [Parameters]
 *     dialog: InputDialog object.
 * [Return value]
 *     True if the dialog has closed (user pressed Enter or cancelled).
 */
boolean isInputDialogFinished(InputDialog dialog)
{
    return dialog.isFinished();
}

/*-----------------------------------------------------------------------*/

/**
 * getInputDialogText:  Return the text entered into the given input
 * dialog.  Returns the empty string if the dialog has not yet finished.
 *
 * [Parameters]
 *     dialog: InputDialog object.
 * [Return value]
 *     Text entered by user, null if the dialog was cancelled, or the
 *     empty string if the dialog is still active.
 */
String getInputDialogText(InputDialog dialog)
{
    if (dialog.isFinished()) {
        return dialog.getText();
    } else {
        return "";
    }
}

/*-----------------------------------------------------------------------*/

/**
 * dismissInputDialog:  Close the given input dialog.
 *
 * [Parameters]
 *     dialog: InputDialog object.
 */
void dismissInputDialog(InputDialog dialog)
{
    dialog.dismiss();
    if (!system_ui_visible) {
        // UI visibility state may have been clobbered by the dialog.
        setSystemUiVisible(false);
    }
}

/*************************************************************************/
/*************************************************************************/

}  // class SILActivity
