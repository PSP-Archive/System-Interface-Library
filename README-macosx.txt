SIL: System Interface Library for games
=======================================
Platform notes for Mac OS X


Checking the OS version
-----------------------
SIL includes several OS X-specific functions for checking the OS
version; see the header <SIL/sysdep/macosx/common.h> (automatically
included by <SIL/base.h>) for details.  While SIL handles all OS version
differences relevant to SIL itself, these functions can be useful to
change UI style or behavior of client code based on the version of OS X
in use.


Fullscreen mode
---------------
When a window is created with resizing enabled (the "window_resizable"
attribute is set to true), SIL uses the OS X "Spaces" mechanism for
fullscreen windows.  This is integrated with the window system, which
causes the "maximize" button in the window title bar to change to a
"fullscreen" button.  SIL will track the fullscreen/windowed state and
report it via the graphics_display_is_window() function; programs should
be prepared for the windowed/fullscreen state to change without an
explicit call to graphics_set_display_mode().

Windows which are not marked resizable will not have a
maximize/fullscreen button and thus will not change between windowed and
fullscreen mode in this manner.

Windows which are explicitly created in fullscreen mode at a resolution
other than the default resolution (as returned by
graphics_device_{width,height}()) will also not use Spaces, since that
does not interact well with display mode changing.  Note in particular
that changing from such a fullscreen window to a non-fullscreen
resizable window (or vice versa) will cause the window to be recreated,
and accordingly all graphics context will be lost.  For this reason, it
is recommended to only use the default display resolution for fullscreen
when using resizable windows.


User data location
------------------
The userdata_*() family of functions reads and writes files in the
directory "<AppSupport>/<program-name>", where <AppSupport> is the
current user's "Application Support" directory and <program-name> is the
string passed as the first parameter to userdata_set_program_name().
The Application Support directory path is constructed as
"$HOME/Library/Application Support"; if the environment variable HOME is
not set, its value is taken to be "." (the current directory, which is
normally the user's home directory for programs launched from the OS X
GUI).
