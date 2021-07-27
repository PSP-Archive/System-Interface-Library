SIL: System Interface Library for games
=======================================
Platform notes for Windows


Build environment
-----------------
The Makefile-based SIL build system is designed to be platform-agnostic,
but it does require GNU Make <https://www.gnu.org/software/make/> and a
POSIX-compliant shell such as Bash <https://www.gnu.org/software/bash/>
along with the standard set of POSIX command line tools; Microsoft's
NMAKE tool and the CMD.EXE command-line interpreter will not suffice,
though the initial "make" command can be run from CMD.EXE if the other
prerequisites are satisfied.

If using Windows 10 (2017 Creators Update) or later, the Windows
Subsystem for Linux provides a convenient POSIX-compliant environment
for building SIL programs.  Install "make" with "sudo apt install make"
and the GCC compiler with "sudo apt install g++ g++-mingw-w64", then
just run "make" in the project build directory to build the program.
The tool programs included in SIL require some auxiliary libraries (see
the notes under "Tool programs" in README.txt); these can be installed
with "sudo apt install zlib1g-dev libpng-dev".  You may first need to
update the environment by running "sudo apt update && sudo apt upgrade".
See <https://msdn.microsoft.com/en-us/commandline/wsl/about> for more
information.

On a multi-core system, adding the "-jN" option (where "N" is the number
of cores) will let the build proceed in parallel, which can considerably
reduce the total build time.  For example, on a quad-core system with
2-thread-per-core hyperthreading, use the command "make -j8" instead of
just "make".  Depending on the speed of your storage device, it may even
help to increase N by 1 or 2 so that a process waiting for disk I/O
doesn't leave a core idle.

The very first time you compile in a Windows Subsystem for Linux
environment, the SIL build system may issue a warning saying "Can't
determine compiler type, assuming GCC".  This can be safely ignored.

The Microsoft Visual Studio compiler _cannot_ be used to build SIL
programs (at least as of Visual Studio 2017) because it does not support
enough of the C99 standard.


Build products and required DLLs
--------------------------------
On Windows 8.1 and later, the only file (other than resource data files
loaded using the resource_*() functions) needed to run the program is
the executable file "<program>.exe" created in the build directory.
However, to support game controllers which use the XInput interface,
Windows 7 and earlier require xinput1_3.dll from the DirectX
redistributable (see "Joystick (game controller) device support" below).
Typically, this is installed via the June 2010 DirectX end-user runtime
installer from Microsoft, which can be bundled with the application.

As part of the build process, an application manifest file is created
under the name "<program>.exe.manifest".  This is only a temporary file
and can be safely removed after a build completes ("make clean" will
remove it along with other temporary and intermediate files), but if the
file is present, Windows XP will refuse to start the executable.  If
testing on Windows XP, be sure to remove the manifest file or move the
executable to a different directory before running the program.


Fullscreen mode and window focus
--------------------------------
When running in fullscreen mode, SIL will by default minimize the window
if it loses focus (because the user pressed Alt-Tab to switch windows,
for example).  This is necessary in single-monitor environments to
ensure that the user can actually see other windows, but it may be
undesirable in multi-monitor setups in which the user has the SIL
program running fullscreen on one monitor and other applications running
on another monitor.

To control whether SIL auto-minimizes the window on loss of focus, set
the display attribute "fullscreen_minimize_on_focus_loss".  This
attribute takes a single argument of type int; a false (zero) value
prevents the window from being minimized on loss of focus, while a true
(nonzero) value restores the default behavior of auto-minimizing.  (If
not in fullscreen mode, the window never auto-minimizes regardless of
this setting.)

For compatibility with programs using SDL 2.x, SIL also recognizes the
environment variable SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS.  If the SIL
display attribute has not yet been set, the default behavior will be
taken from the value of this variable if it is present and non-empty:
a value of "0" or "false" (case-insensitive) will disable
auto-minimization, while any other value will enable it.


Graphics rendering
------------------
By default, SIL uses OpenGL for graphics rendering.  SIL also supports
rendering with Direct3D 11, which can be enabled by setting the Windows-
specific "backend_name" display attribute to the string "direct3d":
    graphics_set_display_attr("backend_name", "direct3d");
Similarly, if Direct3D is already in use, SIL can be switched back to
OpenGL by selecting the "opengl" backend:
    graphics_set_display_attr("backend_name", "opengl");
The new backend will be enabled on the next display mode change.  Note
that all graphics resources will be invalidated when the backend is
changed, so they must be reloaded as needed.

The Direct3D backend does not currently implement either the shader
generator API (graphics_set_shader_generator()) or custom shader objects
(shader_*()).

Note that the Direct3D operations corresponding to graphics primitive
creation in SIL (graphics_create_primitive(), graphics_draw_vertices(),
etc.) are expensive compared to OpenGL.  SIL tries to hide this overhead
as well as it can, but programs which make heavy use of immediate-mode
rendering may get better performance with OpenGL.

The Direct3D backend supports these additional display attributes:

- d3d_shader_opt_level: Accepts 1 int, a value from 0 through 3 giving
  the desired optimization level for shader compilation.  This is used
  for both autogenerated shaders and custom shader objects.  The default
  is 1.  Changes to this attribute take effect immediately.

- d3d_shader_debug_info: Accepts 1 int; a true (nonzero) value enables
  inclusion of debug information in compiled shaders, while false (0)
  disables debug information.  This can be useful in debugging shaders
  using the Microsoft Visual Studio debugging tools.  The default is
  false.  Changes to this attribute take effect immediately.

When using OpenGL, if graphics_set_display_mode() fails with a
BACKEND_TOO_OLD error, it usually indicates that Windows is attempting
to use the software renderer, which identifies itself as OpenGL 1.1.
This usually indicates that the graphics driver is too old for the
current version of Windows; updating the driver will often solve the
problem, but in some cases, the user's GPU or the associated OpenGL
driver may simply not be supported on the version of Windows in use.
(This has been observed, for example, with Intel HD Graphics 3000 and
4000 integrated GPUs on Windows 10 version 1709.)  In some cases,
DirectX may work even when OpenGL does not.

Some Windows graphics drivers deliberately deviate from the OpenGL or
Direct3D specification, ostensibly to improve graphics performance or
quality.  These deviations are generally intended for specific 3D
rendering patterns, and they can cause 2D rendering in particular to
break spectacularly.  Users should be advised to check their driver
settings and make sure that any "graphics quality" or similar options
are turned off, or set to "Use application settings" if such a choice is
present.

SIL normally spawns a separate thread to process window messages, which
improves Windows UI responsiveness (such as when moving the window); in
particular, when window resizing is enabled, this allows the program to
continue rendering while the window is being resized.  The multithreaded
logic is written to conform to the Windows API; however, some older
graphics drivers and third-party software have been reported to have
trouble with multithreaded programs.  In case a workaround is needed,
SIL supports a Windows-specific display attribute to control whether
multithreading is used for window management:

- window_thread: Accepts 1 int; a true (nonzero) value enables
  multithreading for window operations, while false (0) disables
  multithreading.  The default is true.

Note that disabling window multithreading will reduce Windows UI
responsiveness and prevent the program from updating the window while
it is being resized.


Joystick (game controller) device support
-----------------------------------------
SIL supports joystick devices through both the XInput interface, for
Xbox game controllers, and the WM_INPUT interface, for other USB or
Bluetooth controllers compatible with the HID standard.  XInput requires
either xinput1_3.dll from the DirectX redistributable or xinput1_4.dll
which is distributed with Windows 8 and later; xinput9_1_0.dll, included
with Windows Vista and 7, is not accepted due to deficiencies in the
library.  WM_INPUT requires the hid.dll library, which is standard in
all Windows distributions.  In either case, if the required library
cannot be loaded at runtime, the associated input interface will be
disabled.

If desired, the user can disable either joystick interface by setting
environment variables as follows (the quotes are not part of the value):
   - SIL_WINDOWS_USE_XINPUT="0": Disables the XInput interface.
   - SIL_WINDOWS_USE_RAWINPUT="0": Disables the WM_INPUT interface.
Setting either environment variable to "1" will cause SIL to display an
error message when the required library is unavailable (if the
SIL_UTILITY_NOISY_ERRORS build option is enabled), which can be used to
determine if a missing DLL is causing a joystick device to not be
recognized.

Typically, XInput devices are also exposed as HID devices.  If both
XInput and HID interfaces are enabled, SIL will use the XInput interface
for devices which report themselves as XInput-compatible.

The following devices are known to work "out of the box":

- Microsoft Xbox 360 wired controller (using the XInput interface).
  Note that Microsoft deliberately cripples the Xbox 360 controller when
  used with the WM_INPUT interface, so if a user reports that they can't
  use the LT and RT trigger buttons or rumble effects, that may be due
  to XInput not being available.

- Sony DualShock 4 (using the WM_INPUT interface).

The following devices are known to have problems:

- Sony SIXAXIS and DualShock 3: These controllers require a "secret
  handshake" from the host before they will start reporting events (see
  the Linux driver, linux/drivers/hid/hid-sony.c, for details).  This
  handshake is in the form of a feature request for a report not listed
  in the device's report descriptors, and the standard Windows HID
  driver rejects attempts to call HidD_GetFeature() for unlisted
  reports, so there is no way to activate the controller from userspace
  on Windows.  There are third-party drivers, albeit of unknown safety
  and stability, which support these controllers as XInput devices.

Note that at least as of January 2018, Valve Software's Steam client
injects a DLL which wraps the system function GetRawInputDeviceList() with
a buggy function that prevents SIL from scanning WM_INPUT devices.  SIL
will log a message to this effect if it detects the bug.  (Steam itself
appears to expose game controllers via the XInput interface, so this may
not cause any actual problems.)


Long pathname support
---------------------
The Windows API has traditionally limited pathnames to a maximum of 260
characters.  Generally speaking, this should not be a problem for SIL
programs; however, if a user installs the program in a location with a
particularly long pathname, or if you use userdata_override_file_path()
with a long pathname, you may encounter unexpected errors.

In Windows 10 version 1607 and later, applications can opt in to a
longer pathname limit (32,767 characters) with an application manifest
setting.  SIL does not enable this by default, to ensure that the
program does not unintentionally create files which other programs might
not be able to access.  However, if you need this behavior, you can copy
and modify the default application manifest template
(build/windows/manifest.xml.in) to include the <ws2:longPathAware> tag
in the <windowsSettings> block (make sure to also add the "xmlns:ws2"
attribute to <windowsSettings>):

<application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings xmlns:ws2="http://schemas.microsoft.com/SMI/2016/WindowsSettings">
        <ws2:longPathAware>true</ws2:longPathAware>
    </windowsSettings>
</application>

Set the "make" variable MANIFEST_TEMPLATE to the pathname of your
modified template file to apply it to the final executable.

Note that users can also enable long pathnames globally by setting a
registry key.  For more information, see the section "Maximum Path
Length Limitation" of the MSDN document "Naming Files, Paths, and
Namespaces (Windows)", which can (as of this writing) be found at:
https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx#maxpath


Synchronization primitives
--------------------------
SIL implements mutexes using Windows CRITICAL_SECTION objects.  These
provide significantly better performance than full-fledged Mutex
objects; the latter are only needed if mutexes must be shared between
processes (as opposed to threads), which is typically not the case for
SIL-based programs.  The one caveat to CRITICAL_SECTION objects is that
Windows does not include a "timed wait" operation on them, so timed
waits are implemented as a loop which polls the mutex and calls Sleep(1)
if acquisition fails.  Depending on system load and timer resolution,
this can have a fairly coarse resolution, so do not rely on
mutex_lock_timeout() for precise timing.

Windows will abort (with EXCEPTION_POSSIBLE_DEADLOCK) a program which
waits on a mutex for longer than the timeout given by the
"HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\
CriticalSectionTimeout" registry value.  The default value is 2592000
seconds (30 days), so this should not be a problem in practice, but it
may occur on systems in which CriticalSectionTimeout is set to an
abnormally low value.  (Notably, the WINE libraries for non-Windows
platforms use a timeout of just 65 seconds.)

Windows XP lacks native support for condition variables.  SIL includes a
substitute implementation, but it is fairly slow, so be aware that heavy
use of condition variables in code hotspots may impair performance on XP
systems.


Touch input
-----------
SIL uses the WM_POINTER* event interface, introduced in Windows 8, for
touch input.  Windows 7 also includes support for touch events, but this
support uses a completely different, more kludgey interface (WM_TOUCH),
and there are currently no plans to support this interface in SIL.
Accordingly, touch input is only supported for Windows 8 or later.


User data location
------------------
The userdata_*() family of functions reads and writes files in the
directory "<RoamingAppData>\<program-name>".  <RoamingAppData> is the
%APPDATA% folder for the current user; this is usually:
   - On Windows Vista and later, C:\Users\<username>\AppData\Roaming
   - On Windows XP, C:\Documents and Settings\<username>\Application Data
(The folder can also be opened by typing "%APPDATA%" in the Explorer
location bar.)  <program-name> is the string passed as the first
parameter to userdata_set_program_name().


Known quirks
------------
- The Windows OpenGL driver for Intel GPUs has two significant bugs
  affecting at least the HD Graphics 4000 and 4400, present through at
  least version 15.33.46.4885:
     * When a fullscreen window is minimized, the driver fails to stop
       drawing the window, so it hides all other windows even though
       some other window is in fact receiving input.
     * Specific to the HD Graphics 4000: Under unknown conditions,
       glFinish() (and thus graphics_sync()) can block for several
       seconds even though GPU rendering has completed.  In particular,
       enabling sync-on-frame-start with graphics_enable_debug_sync()
       appears to trigger this behavior fairly frequently.
  There are no known workarounds for either of these bugs, so it is
  recommended to use DirectX with this GPU.
