SIL: System Interface Library for games
=======================================
Platform notes for Linux


Fullscreen mode and window focus
--------------------------------
When running in fullscreen mode, if the window loses focus (because the
user pressed Alt-Tab or a similar window manager shortcut to switch
windows, for example), SIL will by default minimize (iconify) the window
if the screen was set to a non-default video mode, so that other windows
can be used in the default video mode.  However, this may be undesirable
in multi-monitor setups in which the user has the SIL program running
fullscreen on one monitor and other applications running on another
monitor; conversely, the user may want a fullscreen window to minimize
on loss of focus even in the default video mode.

To control whether SIL auto-minimizes the window on loss of focus, set
the display attribute "fullscreen_minimize_on_focus_loss".  This
attribute takes a single argument of type int; a false (zero) value
prevents the window from being minimized on loss of focus, while a true
(nonzero) value causes a fullscreen window to always minimize on loss of
focus.  (If not in fullscreen mode, the window never auto-minimizes
regardless of this setting.)  Note that once the attribute has been set,
there is no way to restore the default behavior, and the client program
is responsible for controlling auto-minimization.

For compatibility with programs using SDL 2.x, SIL also recognizes the
environment variable SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS.  If the SIL
display attribute has not yet been set, the default behavior will be
taken from the value of this variable if it is present and non-empty:
a value of "0" or "false" (case-insensitive) will disable
auto-minimization, while any other value will enable it.


Input handling
--------------
SIL never grabs keyboard input, even when input_grab() is called, so
that window manager hotkeys still function as expected.  (This is in
contrast to some other libraries, such as SDL, which grab keyboard input
in addition to confining the mouse pointer to the window.)


Libc compatibility
------------------
The Linux-specific code in SIL is mostly POSIX-compliant, but there are
a few functions specific to glibc (at least pthread_setaffinity_np() and
pthread_get_affinity_np()) which are used due to the lack of a POSIX
equivalent.  SIL programs can potentially be compiled with an alternate
libc if replacements for these functions are provided.


Movie playback
--------------
SIL can be configured (via the USE_FFMPEG build setting; see
build/linux/build.mk) to use an external copy of the FFmpeg library to
decode movie files.  However, there are some caveats to this:

- The FFmpeg library API changes frequently; linking dynamically with a
particular version of FFmpeg typically requires users to have the same
minor version installed on their systems (for example, a program built
against FFmpeg 2.2.5 would require FFmpeg 2.2.x at runtime).

- FFmpeg is released under the LGPL license, with some parts under GPL.
If you will not be releasing your program under a compatible license,
you need to ensure that you build with an LGPL-only copy of FFmpeg and
link the library dynamically (not statically).


Resource files
--------------
Pathnames for resource and package files are taken to be relative to the
directory containing the program executable.  However, if the program
was built with the build setting SIL_DATA_PATH_ENV_VAR set to a
non-empty string, the program will check for the presence of the
environment variable named by that string and (if the variable is
present and non-empty) use its value instead of the current directory as
the base for relative pathnames.


Sound
-----
SIL uses the ALSA interface for audio output; a specific ALSA output
device can be rqeuested by passing the ALSA device name (such as
"hw:0,0" or "default") to sound_open_device().  For an example, see the
Linux sound tests in src/test/sysdep/linux/sound.c, which explicitly
open the ALSA loopback device to read back audio data written through
ALSA.

Some Linux distributions use the PulseAudio audio server for managing
audio output, which can introduce hidden output latency and has in some
cases been observed to cause audible errors such as stuttering (though
SIL attempts to work around such errors).  To resolve problems with
PulseAudio, users can start the program with the command
"pasuspender -- <program-path>", where "<program-path>" is the path to
the program's executable (or other script used to start the program).
This command could also be incorporated into a startup script, using
code such as the following:
    cmdline="./program"
    if type pasuspender >/dev/null 2>&1; then
        cmdline="pasuspender -- ${cmdline}"
    fi
    eval "exec ${cmdline}"

Another possible cause of audio stuttering on Linux is a failure to
raise the audio mixer thread's priority; see "Thread priorities" below
for the cause and a user-side workaround.


Thread priorities
-----------------
Current versions of Linux do not allow user programs to change thread
priorities with the standard POSIX thread priority functions.  Instead,
at least through Linux 4.15, the POSIX setpriority() function with the
PRIO_PROCESS argument (which is documented to affect the priority of the
entire process, but Linux usefully deviates from this requirement) can
be used for this purpose, with the caveat that in order to create a
thread with a higher priority than the current thread, the system must
be configured to allow non-root users to decrease the "nice" value
(increase the priority) of a thread.  On typical Linux systems, this can
be accomplished by editing the file /etc/security/limits.conf to add the
following two lines at the end of the file (the "*" is part of the text
to be added):

* hard nice -10
* soft nice -10

and then logging out and back in again.


User data location
------------------
SIL follows the XDG standard for user data storage; the userdata_*()
family of functions reads and writes files in the directory
"$XDG_DATA_HOME/<program-name>", where <program-name> is the string
passed as the first parameter to userdata_set_program_name().  If the
environment variable XDG_DATA_HOME is not set, its value is taken to be
"$HOME/.local/share", and if HOME is also not set, its value is taken to
be "." (the current directory, which is normally the directory from
which the program was started).


X11 issues
----------
SIL attempts to "do the right thing" with regard to X11 window managers,
but this is extraordinarily difficult due to the range of different
behaviors implemented by the multitude of window managers available for
Linux (see src/sysdep/linux/graphics.c for a fairly comprehensive list).
SIL has been tested to work correctly with the standard window managers
used by the GNOME and KDE desktop environments, but users who use other
window managers may experience undesired behavior, particularly when
setting fullscreen mode or using multiple monitors.

To help alleviate such problems, the user can set any of the following
environment variables to modify SIL's default behavior.  If a variable
is unset or set to the empty string, or if the variable's value is not
valid, the corresponding behavior is determined by checking the window
manager in use when the window is created and choosing the best behavior
given how that window manager is known to function.

- SIL_X11_CREATE_FULLSCREEN={0|1}
     Disables or enables creating windows in fullscreen mode.  If
     disabled, fullscreen windows will be created in windowed mode and
     then switched to fullscreen.  This can have the undesirable side
     effect of a window appearing for an instant before entering
     fullscreen mode, but some window managers do not properly handle
     windows which are set to fullscreen mode when first shown.

- SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE={0|1}
     Disables or enables moving the window to the target monitor before
     entering fullscreen mode.  Only affects multi-head, single-X11-
     screen ("Xinerama") configurations.  Useful for window managers
     that do not understand (or do not properly handle) the
     _NET_WM_FULLSCREEN_MONITORS message.

- SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER={0|1}
     Disables or enables resizing the window after entering fullscreen
     mode.  Useful for window managers that set the wrong window size
     for non-default screen resolutions.

- SIL_X11_FULLSCREEN_METHOD=<method>
     Sets the method for changing windows to fullscreen mode to
     <method>, which may be any of: XMOVEWINDOW, EWMH_FULLSCREEN

     XMOVEWINDOW enters fullscreen mode by manually moving the window to
     the upper-left corner of the screen and resizing it to fill the
     screen, then attempting to disable the window border (see also
     SIL_X11_USE_TRANSIENT_FOR_HINT below).  This works with many older
     window managers, but newer window managers may override some of the
     manually requested settings, leaving the window in an inconsistent
     state.

     EWMH_FULLSCREEN enters fullscreen mode by sending a fullscreen
     request to the window manager using the EWMH protocol.  If the
     window manager supports this protocol (modern window managers,
     including the default GNOME and KDE window managers, support it),
     this generally provides a cleaner experience than the XMOVEWINDOW
     method, but some window managers don't implement fullscreen
     support correctly.

- SIL_X11_USE_TRANSIENT_FOR_HINT={0|1}
     Sets whether to use XSetTransientForHint() to remove window borders
     when using the XMOVEWINDOW fullscreen method.  When set to 0, if
     the _MOTIF_WM_HINTS atom exists, SIL will set the Motif borderless
     hint to request the window manager to remove borders on the window;
     when set to 1, SIL will call the XSetTransientForHint() function
     for this purpose.  (If the _MOTIF_WM_HINTS atom does not exist, SIL
     will use XSetTransientForHint() unconditionally and this setting
     will have no effect.)

Additionally, the SIL_X11_VIDEO_MODE_INTERFACE variable can be set to
request a specific method for changing the screen resolution (video
mode), which may be any of: XRANDR, VIDMODE, NONE.  If the variable is
not set, XRANDR is used if possible, otherwise VIDMODE if possible,
otherwise NONE.

- XRANDR: Screen resolution is changed using the X11 XRandR extension.
  This requires a video driver which supports XRandR version 1.2 or
  later.  XRandR supports changing the resolution on all attached
  monitors, but some window managers may move windows or rearrange
  icons when the resolution is changed.  Also note that in some
  configurations, XRandR may report video modes which cannot actually
  be selected; make sure your code properly handles an error return
  from graphics_set_display_mode() even when passing in a resolution
  returned by graphics_list_display_modes().

- VIDMODE: Screen resolution is changed using the X11 XF86VidMode
  extension.  This method typically does not trigger window or icon
  rearrangement and thus can be superior to the XRandR method for
  single-head configurations, but XF86VidMode only supports changing
  the resolution of the first monitor in a multi-head, single-X11-
  screen ("Xinerama") configuration.

- NONE: Screen resolution changing is disabled; attempts to set a
  fullscreen display mode with a size different than the current
  resolution will fail.

Finally, if the SIL_X11_RESOURCE_CLASS environment variable is set to a
non-empty string, that variable will be used in place of the executable
file's name as the X11 resource class, allowing users to set a specific
resource class and configure their window manager to behave a certain
way with respect to that class.

In addition to the fullscreen issues listed above, the "center_window"
and "device" attributes may not be respected in some cases, since window
placement behavior is strongly dependent on the window manager.
Notably, in a Xinerama-type configuration in which multiple display
devices are combined to form a single X11 screen, the "device" attribute
will always be ignored if the "center_window" attribute is set to false,
and with some window managers, setting "device" to a non-default device
and "center_window" to true will result in a non-centered window on the
default device.  Additionally, while the default window managers used by
major Linux distributions are well-behaved with respect to windows
displayed in fullscreen mode, some uncommon window managers may forcibly
reposition windows after they are created, causing fullscreen windows at
a lower resolution than the display's default resolution to be displayed
outside the visible bounds of the screen.  See the source code
(src/sysdep/linux/graphics.c) for details.
