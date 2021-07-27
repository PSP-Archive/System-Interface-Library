SIL: System Interface Library for games
=======================================
Platform notes for iOS


Building for iOS versions prior to 8.0
--------------------------------------
As of Xcode 8, Apple has dropped official support for iOS versions prior
to iOS 8.0.  However, the toolchain itself is still capable of building
for earlier iOS versions (with caveats for iOS 5.1.1; see below), and at
least as of October 2017, Apple still accepts to the App Store programs
which have been built with the SIL build system to target iOS 5.1.1.

When targeting iOS 5.1.1 or 6.x, C++ programs must be built with the
GNU libstdc++ library rather than LLVM's libc++, since libc++ was not
included in iOS until version 7.0.  Apple removed libstdc++ headers and
libraries from iOS SDK 12 (included in Xcode 10), so C++ programs
targeting iOS 5.1.1 or 6.x cannot be built with this SDK.  For ad-hoc
distribution, it is sufficient to build with an earlier SDK; however,
Apple generally requires the latest SDK and Xcode toolchain when
submitting to the App Store.  In this case, copy the following
directories from iOS SDK 11 (Xcode 9) or earlier into the current SDK at
the same relative paths:
    usr/include/c++
    usr/lib/libstdc++*

When using libstdc++ (whether with iOS SDK 11 or earlier or by copying
the libstdc++ headers and libraries into a later SDK), the linker will
emit a warning similar to "libstdc++ is deprecated, move to libc++ with
a deployment target of 7.0 or later".  This can be safely ignored.

As of iOS SDK 10 (Xcode 8), the system object file "crt1.3.1.o" which is
automatically linked into the executable for iOS 5.1.1 is no longer
present.  In order to build for iOS 5.1.1, copy usr/lib/crt1.3.1.o from
iOS SDK 9 (Xcode 7) into the usr/lib directory of the current iOS SDK.
Earlier versions of the SDK should also work but have not been tested.


Building for the Apple TV
-------------------------
SIL treats the "tvOS" operating system used on Apple TVs starting in
2015 (4th generation and later) as a variant of iOS.  Programs
targeting the Apple TV should use the iOS build framework (PLATFORM=ios
in build scripts) and can use iOS-specific utility functions such as
ios_get_device().

When submitting apps targeting Apple TV to the App Store, Apple
requires embedded LLVM bitcode, so the EMBED_BITCODE build variable must
be set to 1.

Note also that SIL has not been tested on an Apple TV device.

Prior Apple TV generations (1st through 3rd generation) are not
supported.


Building for the iPad 1
-----------------------
The latest iOS version supported on the iPad 1 is iOS 5.1.1, so the
project must be explicitly configured to target that version of iOS.
See above for associated caveats.

The first-generation iPad does not support "fat" binaries with both
32-bit and 64-bit code, which is the default mode for the SIL build
system (and is also currently required for submitting apps to the App
Store; apparently Apple massages the uploaded app into something that
the iPad 1 can deal with).  In order to build a package which can be
installed on an iPad 1, explicitly set TARGET_ARCH_ABI=armv7 on the
"make" command line.


Building with Xcode 8.3 and later
---------------------------------
Xcode 8.3 and later omit the "PackageApplication" command-line tool used
to create package (.ipa) files, causing an error similar to "/bin/sh:
/Applications/Xcode9.app/Contents/Developer/Platforms/iPhoneOS.platform/
Developer/usr/bin/PackageApplication: No such file or directory" during
the final packaging step.  To build with Xcode 8.3 or later, copy the
PackageApplication binary from Xcode 8.2 to the same directory
(Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin).
PackageApplication binaries from earlier versions of Xcode will likely
also work but have not been tested.

Alternatively, you can set the PACKAGEAPPLICATION_PATH variable (on the
"make" command line or in your shell environment) to the path to the
PackageApplication program, and the build scripts will use that program
instead of looking for it inside the Xcode application bundle.


Device information
------------------
SIL exposes the iOS device type and iOS version through several
functions declared in the header <SIL/sysdep/ios/common.h>
(automatically included by <SIL/base.h>).  While the display size and
related device parameters are exposed through the platform-independent
interfaces, these functions can be useful to, for example, enable a
low-memory mode when running on a device that doesn't have much RAM.
That header also includes functions for controlling the audio session
category (see under "Sound" below) and system status bar, and a global
flag used to enable or disable auto-rotation of the display as the
device is (physically) rotated.


Game Center (GameKit) integration
---------------------------------
SIL supports integrating with Apple's Game Center using the GameKit
framework, to the extent of supporting independent sets of user data for
distinct Game Center players and exporting boolean-valued statistics as
achievements.  To include Game Center support, set the build variable
USE_GAMEKIT to 1 (see build/ios/build.mk) and include the header
<SIL/sysdep/ios/gamekit.h> in any source files which call functions
specific to Game Center.

If you support Game Center, you must also be aware of the Game Center
authorization status, which is reported by the ios_gamekit_auth_status()
function.  At startup, you should call ios_gamekit_authenticate() as
soon as possible, then loop on ios_gamekit_auth_status() until it
returns a value other than IOS_GAMEKIT_AUTH_PENDING.  (You can of course
use this time to do things like load resources, so as not to waste time
waiting for network communication to complete.)  Once authorization is
complete, you should periodically call ios_gamekit_auth_status(); if you
see a return value of IOS_GAMEKIT_AUTH_CHANGED, you should reset all
internal state and restart the program as though it had just been
started up with the new player ID.

In some cases, it is possible to get IOS_GAMEKIT_AUTH_CHANGED without an
actual change in the player ID.  To catch these cases and avoid
unnecessarily resetting the current program state, you can cache the
value returned from ios_current_player() and compare it to the value
returned after IOS_GAMEKIT_AUTH_CHANGED.

Rarely, GameKit may get stuck when attempting to contact the Game Center
servers.  If the authorization state remains PENDING for more than 10
seconds, you might consider giving the user an option to continue
without using Game Center, and behaving as though the authorization
state was IOS_GAMEKIT_AUTH_FAILED.  In this case you should be careful
not to call any other ios_gamekit_*() functions as long as the
authentication state remains PENDING.

Note that through iOS 7.1, GameKit always acted as though the user had
signed out if it could not connect to the Game Center server (for
example, if the user had no network connectivity).  To avoid users
having their data suddenly become unavailable in this case, SIL treats a
GameKit response of "no authenticated user" as if the most recently
authenticated user was still authenticated.


Graphics rendering
------------------
iOS's OpenGL implementation seems to draw an extra pixel at the end of a
line primitive depending on the exact coordinates of the endpoint.  This
can be seen, for example, in pixels sticking out from the corners of a
1-pixel-thick rectangle drawn using line primitives.

In at least some versions of iOS, glBindTexture() will crash rather than
reporting GL_INVALID_ENUM if zero is passed as the first parameter.
(SIL as distributed does not make such calls.)

iOS versions 8.0 through 8.2 have a bug in glCopyTexImage() which causes
incorrect alpha data to be stored to the target texture.  SIL includes a
workaround for the bug, but users with these iOS versions will see
reduced graphics performance if the program uses texture_grab() or
texture_grab_new() to copy from the framebuffer.  The bug is fixed in
iOS 8.3.


High-refresh-rate displays
--------------------------
SIL supports displays with refresh rates greater than 60 frames per
second, such as the "ProMotion" display used in the 10.5-inch iPad Pro,
by reporting two available graphics modes: one with a 60 Hz refresh rate
and one with the native refresh rate (such as 120 Hz for ProMotion
displays).  SIL defaults to 60 Hz (or the native rate, if that is lower)
on all devices, but programs can use a 120 Hz mode by explicitly
requesting that refresh rate with graphics_set_display_attr().

The function ios_get_native_refresh_rate() is also provided to return
the native refresh rate of the display, so client code with iOS-specific
logic can call that function directly instead of looking up display
modes.


Icon and launch images
----------------------
The SIL build system will automatically embed icon and launch screen
images into the application package if they are provided in the
ICON_IMAGES and LAUNCH_IMAGES build variables, respectively.

Note that the SIL build system does not support the smaller icon sizes
(57x57 for iPhone, 72x72 for iPad) used by iOS 5.1.1 and 6.x, even if
TARGET_OS_VERSION is set to such a version.  For these iOS versions, use
a custom Info.plist template (see the INFO_PLIST_SOURCE build varible
described in build/iOS/build.mk) with the appropriate property list keys
included, and add the relevant icon files to the resource list (the
RESOURCES variable) so they are included in the application package.
Also keep in mind that you may want to include the "UIPrerenderedIcon"
key in a custom Info.plist template to disable the "shine" effect
applied to icons in iOS 6.x and earlier.


iTunes file sharing
-------------------
SIL has an experimental feature, enabled by setting the USE_FILE_SHARING
build option to 1, which allows copies of user data to be stored in a
location from which iTunes can copy the data onto a PC.  This currently
carries a (small but nonzero) risk of data loss, especially when
combined with Game Center integration, so use with care!  See the
comments in build/ios/build.mk for details.


Memory management
-----------------
iOS provides a low-memory notification to all active programs when the
amount of available system memory decreases beyond a (hidden, internally
defined) limit.  This is exposed by the input event INPUT_MEMORY_LOW in
SIL.  If you receive this event, you may want to purge any persistent
caches to reduce the chance that the process will subsequently be killed
for using too much memory.

Note that iOS overcommits memory and will never fail a memory allocation
request.  Instead, the process will be killed if it attempts to write to
newly allocated memory and the system cannot find enough physical memory
for the memory block.  Because of this, you should be prepared for users
to report "crashes" which are in fact caused by the OS stealing memory
back from your program and leaving it without enough memory to run.  You
may want to use the low-memory notification to inform the user that
their device is running low on memory and they should close other apps
or reboot the device to resolve the problem.


Sound
-----
iOS supports the concept of "audio session categories", which are
intended to indicate the type (with respect to the user's interactions
with the device) of audio being played and thus how it should be mixed
with other programs which are also playing audio.  Unfortunately, the
implementation leaves much to be desired; notably, in some iOS versions,
selecting background mode may prevent any audio output even if no other
sound is playing.  For this reason, even when the default "background"
category would be appropriate, it is currently recommended to select the
"foreground mix" category by calling
ios_sound_set_category(IOS_SOUND_FOREGROUND_MIX) at program startup,
before calling sound_open_device().


Suspend and resume
------------------
On iOS, alert dialogs such as incoming call notifications and
low-battery messages, as well as window resizing (Split View) in iOS 9
and later, trigger a suspend operation for the active app.  If the
program takes more than a second or two to respond to a suspend request,
the user may notice the program seeming to freeze for a short time if
they immediately dismiss such a dialog or finish resizing the window,
since the program will still be handling the suspend request when the
dialog is closed and the system sends a resume notification.


User data management
--------------------
While not normally visible to users, the userdata_*() family of
functions stores data in the app sandbox's "Library/Application Support"
directory.  If Game Center integration is enabled and at least one
authenticated user has been seen, the Application Support directory
instead contains one directory for each known Game Center ID, along with
a file named "players.txt" listing all Game Center IDs that have been
seen, with the most recently seen ID at the top of the file.

If a user starts using a SIL program without logging into Game Center,
stores some user data, then later logs into Game Center, the previously
stored data is sutomatically and transparently migrated to the directory
for that Game Center ID.

The Apple TV does not provide any local storage for applications, so on
Apple TV devices, user data is stored using the iCloud interfaces under
the current iCloud user ID on the device.  If the user signs in to Game
Center and iCloud under different user IDs, user data may become
desynchronized.


Known quirks
------------
- On iOS 5.1.1, attempting to destroy shader objects which are bound to
  a shader pipeline will cause the program to crash due to a bug in the
  OpenGL libraries in that version of iOS.  If using shader objects (as
  opposed to the default rendering pipeline or shader generator
  functions), either ensure that shaders are not destroyed while bound
  to a pipeline or set the target OS version to 6.0 or later.
