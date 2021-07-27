SIL: System Interface Library for games
=======================================
Platform notes for Android


Build architectures
-------------------
Android runs on devices using several different CPU architectures:
currently ARM, MIPS, and x86, each of which has 32-bit and 64-bit
variants.  By default, SIL will build for all possible architectures,
which can take a significant amount of time.  Especially when
debugging, it can be useful to build for only the architecture of the
device on which the program is about to be run.  To do this, set the
build variable TARGET_ARCH_ABI to the appropriate architecture (see
build/android/build.mk for a list).

When building for 32-bit ARM, if the target devices are known to all
support the NEON instruction set (as all but the oldest devices do), you
can set the build variable USE_ARM_NEON to 1 to enable the use of NEON
instructions.  Note that this will cause the program to crash if run on
an ARM device which does not support NEON instructions.


Device information
------------------
SIL exposes the Android device type and coarse system version through
several functions declared in the header <SIL/sysdep/android/common.h>
(automatically included by <SIL/base.h>).  While the display size and
related device parameters are exposed through the platform-independent
interfaces, these functions can be useful to, for example, enable a
low-memory mode when running on a device that doesn't have much RAM.
That header also includes functions for toggling the system status bar
on or off and for retrieving the pathnames of expansion files
distributed through the Google Play store (see "Google Play expansion
files" below).


Device orientation
------------------
Currently, SIL is not able to support both portrait and landscape modes
at the same time; either portrait or landscape mode must be selected at
build time.  The default is landscape mode; to select portrait mode, set
the build variable SCREEN_ORIENTATION to "portrait" (without quotes).


Google Play expansion files
---------------------------
Currently, the Google Play store has a size limit of 100MB for APK
(application package) files.  If you intend to distribute through Google
Play but your APK is larger than that size, you need to instead make use
of Google Play's "expansion files" feature.  SIL provides the function
android_expansion_file_path() to return the path to where the file has
been downloaded on the local device; for example, if the expansion file
is a resource package file, the string returned from
android_expansion_file_path() can be passed to pkg_create_instance().

Expansion files are normally downloaded with the APK itself, but under
some circumstances the program may need to download the files on its
own.  To this end, SIL includes logic which checks on startup whether
any expansion files need to be downloaded and performs the download if
necessary, so that when the client program's entry point (sil_main()) is
called, all expansion files are available.  If desired, the text strings
used by the downloader activity can be translated into other languages;
copy build/android/downloader-strings.xml to a file named
downloader-strings-<lang>.xml in the same directory (where <lang> is the
two-letter ISO 639-1 language code, optionally followed by "-r" and an
ISO-3166-1-alpha-2 region code, for example "en-rUS"; see the Android
documentation on providing alternative resources, currently located at
https://developer.android.com/guide/topics/resources/providing-resources.html#AlternativeResources)
and edit the strings as appropriate.

Note that the build variable USE_DOWNLOADER must be set to 1 to enable
both detection and downloading of expansion files.  The build variables
DOWNLOADER_BASE64_PUBLIC_KEY and DOWNLOADER_SALT must also be set
appropriately; see build/android/build.mk and the Google Play
documentation for details.


Immersive mode
--------------
Android 4.4 "KitKat" and later support a display mode known as
"immersive mode", in which the navigation softkeys are hidden and the
program can draw to the full size of the display.  SIL always uses
immersive mode if it is available; client code can use the function
android_using_immersive_mode() to check whether immersive mode is in
use.


Memory management
-----------------
Android provides a low-memory notification to all active programs when
the amount of available system memory decreases beyond a (hidden,
internally defined) limit.  This is exposed by the input event
INPUT_MEMORY_LOW in SIL.  If you receive this event, you may want to
purge any persistent caches to reduce the chance that the process will
subsequently be killed for using too much memory.

Note that Android overcommits memory and will never fail a memory
allocation request.  Instead, the process will be killed if it attempts
to write to newly allocated memory and the system cannot find enough
physical memory for the memory block.  Because of this, you should be
prepared for users to report "crashes" which are in fact caused by the
OS stealing memory back from your program and leaving it without enough
memory to run.  You may want to use the low-memory notification to
inform the user that their device is running low on memory and they
should close other apps or reboot the device to resolve the problem.


Ouya support
------------
SIL includes basic support for building an application package that will
be recognized by the Ouya game console.  To enable this support, set the
build variable TARGET_ENVIRONMENT to the string "ouya" for an Ouya-only
package or "ouya-generic" for a package that will run on both Ouya and
regular Android devices, and set OUYA_ICON_PNG to the pathname of a
732x412 PNG image to be used as the icon in the Ouya launcher.


Packaging
---------
To set the application's icon, set the build variable ICON_PNG to the
pathname of a 72x72 PNG image to use as the icon.

All Android applications must be signed.  By default, SIL does not sign
built packages, leaving this to the client program's build system.
Signing can be enabled by setting the build variable SIGN to 1 and
setting appropriate values for the SIGN_KEYSTORE, SIGN_STOREPASS,
SIGN_KEYPASS, and SIGN_ALIAS variables; see build/android/build.mk for
details.  The SIL distribution includes a keystore
(build/android/debug.keystore) which can be used to sign applications
for debugging.

By default, SIL sets the "debuggable" attribute of a package to true,
meaning that devices will log crash reports to the system log if the
program crashes and users can attach to the process using a remote
debugger.  To disable this, set the build variable DEBUGGABLE to 0.

Additional tags can be inserted into the package's AndroidManifest.xml
file by setting the MANIFEST_GLOBAL_EXTRA (for top-level tags) or
MANIFEST_APPLICATION_EXTRA (for tags inside <application>) as
appropriate.


Permissions
-----------
By default, SIL does not request any special Android permissions, except
that on Android 4.3 (SDK level 18) and earlier, SIL requests the
WRITE_EXTERNAL_STORAGE permission in order to access its external data
directory (Android 4.4 allows an app to access its external data
directory without this permission).  If you need to read or write other
areas of external storage, such as scanning or opening the user's media
files, set either of the build variables READ_EXTERNAL_STORAGE or
WRITE_EXTERNAL_STORAGE to 1 as appropriate (WRITE_EXTERNAL_STORAGE
implies READ_EXTERNAL_STORAGE, so they do not both need to be set to
allow read-write access).

Note that as of Android 6.0, some permissions (including READ_ and
WRITE_EXTERNAL_STORAGE) are no longer automatically granted to the
program on install; instead, they must be explicitly requested at
runtime.  SIL provides the function android_request_permission() for
this purpose.  It is still necessary to declare the desired permissions
in the app manifest (for external storage access, this can be done by
setting the build variables described above).

If USE_DOWNLOADER is enabled, SIL programs request the following
permissions, which are needed for downloading expansion files:
ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE, CHECK_LICENSE, and INTERNET.


Resource files
--------------
Android does not allow direct (OS-level) file access to resource files
included in the application package (APK), instead wrapping them in an
"Asset" type.  The Asset interface does not provide a way to list
subdirectories of a directory, so setting the "recursive" parameter of
resource_list_files_start() to true will have no effect when listing
such resource files.  If you need to list resource files recursively,
use a package file instead of storing the resources directly in the APK.


"Unfortunately, <application> has stopped."
-------------------------------------------
On Android 4.x and older devices, applications may crash on startup with
the above error due to an ABI incompatibility introduced in the Android
NDK as part of the release of Android 5.0.  Use "adb logcat" to view the
device log, and look for a line like:

"E/AndroidRuntime(12345): java.lang.RuntimeException: Unable to start
activity ComponentInfo{com.example.foo/com.example.foo.SILActivity}:
java.lang.IllegalArgumentException: Unable to load native library:
/data/app-lib/com.example.foo/libnative.so"

If this line is present, it usually means that the program is attempting
to use a symbol which is not present in the shared libraries on the
device.  To determine which symbol is at issue, compile a simple program
like the following (use "make V=1" to view the command line used to
compile object files, then remove -M*, -c, and -o options to get a
command line usable for compiling executables):

    #include <stdio.h>
    #include <dlfcn.h>
    int main(int argc, char **argv) {
        void *p = dlopen(argv[1], 2);
        printf("%p %s\n", p, dlerror());
        return 0;
    }

Use "adb push" to push both the compiled program and the native library
(found in "libs/<ABI>" under the build directory) to the /data/local/tmp
directory on the device, then (assuming the executable is named "foo")
run the command:

    adb shell /data/local/tmp/foo /data/local/tmp/libnative.so

This should print a line similar to:

    0x0 dlopen failed: cannot locate symbol "rand" referenced by
    "libnative.so"...

Then look for the listed function (rand() in this case) in your code,
and either replace it with a call to different function (rand() and
srand(), for example, could be replaced with calls to functions from
<SIL/random.h>) or write your own version of the function and compile it
into the program.

Note that there may be multiple undefined symbols, so you may need to
repeat this process several times before the program starts up
successfully.


User data management
--------------------
Android provides two data storage directories for applications:
"internal" and "external".  Before Android 4.0 "Ice Cream Sandwich",
these terms referred to the physical location of the data, either on the
internal flash memory or on an external device such as an SD card, and
external storage was not guaranteed to be present (and could even
disappear while the program was running).  In modern Android, however,
all data is physically stored in internal flash memory, and the
distinction between the two directories is whether the user is allowed
to access the data (external storage) or not (internal storage).

SIL always uses the external directory for user data storage, so that
users are able to copy their data from or to the device.  This implies
that SIL programs will fail to start up on pre-4.0 Android devices which
do not have external storage available, but as of June 2016, over 99%
of Android devices use Android 4.0 or later[1], and this number is only
expected to increase over time.

The external data directory for an application can be found at
/Android/data/<package>/files on the device's external storage (as
viewed when connected to a PC), where <package> is the application
package identifier (for SIL, this is set in the PACKAGE_NAME build
variable).

[1] https://developer.android.com/about/dashboards/index.html


Known quirks
------------
- In some versions of Android (reported in CyanogenMod 10, so
  this may or may not affect official Android releases), OpenGL apps
  will fail to render to the screen when first launched.  Typically this
  is reported as "a black screen with sound".  The workaround is to
  return to the Android home screen by pressing the Home button, then
  resuming the app by tapping its icon again or by selecting it from the
  Recent Apps list.

- With some rendering patterns, taking a screenshot or viewing
  the app's thumbnail on the Recent Apps list results in part of the
  display not being shown in the screenshot or thumbnail.  This is a
  known bug in Android; for the current resolution status, see:
  <https://code.google.com/p/android/issues/detail?id=57407>

- On the Asus Padfone (a phone which can dock with a tablet and
  use the tablet's screen as its display), certain rendering patterns
  result in part of the display not being shown when run on the tablet
  screen, even though the app works fine when run on the phone screen.
  The trigger is believed to be the same as the previous issue, but the
  workaround is different: the user should turn on the option labeled
  "Disable HW overlays (always use GPU for screen compositing)" in the
  "Developer options" section of the Settings app.  Note that in Android
  4.2 and later, the "Developer options" section is hidden by default;
  to reveal it, go to the "About tablet" (or "About phone") section at
  the bottom of the Settings menu, scroll down to the "Build number"
  line at the bottom of that section, and tap "Build number" 7 times.

- On some devices (at least the Samsung SGH-T989D with
  CyanogenMod 10.0.0), touch-up events can be delayed by several tens of
  milliseconds, so a moving touch may seem to stop in place for a few
  video frames before the touch-up event arrives, even if the physical
  action was a continuous movement such as a flick.

- Rarely, on (at least) the 1st generation Nexus 7 with Android
  4.4.3 or earlier, audio may begin stuttering when the app resumes from
  being suspended.  This can be differentiated from simple CPU overload
  by a flood of system log lines like "W/audio_hw_primary(15866):
  out_write() limiting sleep time 24794721 to 23219", where the "sleep
  time" value counts down over time.  The problem resolves itself when
  the "sleep time" value reaches the second (limit) value, though cases
  have been observed in which this would take several hours.  Rebooting
  the device also fixes the problem.

- On ARM Mali devices (at least the Nexus 10), at least through Android
  6.0, if the very first graphics primitive drawn by the program is a
  point primitive (GRAPHICS_PRIMITIVE_POINTS), the primitive is not
  rendered to the screen.  As a workaround, make sure at least one
  non-point primitive (such as TRIANGLES or QUADS) is drawn before any
  point primitives.
