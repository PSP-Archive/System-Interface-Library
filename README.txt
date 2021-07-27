SIL: System Interface Library for games (version 0.5e)
=======================================
Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
See the file "COPYING.txt" for conditions on use and redistribution.


Overview
--------
SIL provides a simple, fairly low-level, cross-platform interface
covering typical data operations performed by game software:
   - audio output
   - graphics rendering
   - input (joystick/gamepad, keyboard, mouse, touch)
   - persistent data storage (settings, game progress, achievements)
   - resource management
   - threads and synchronization

SIL is not a self-contained engine; rather, it is designed to serve as
the foundation for such an engine, allowing the engine to be ported more
easily to multiple platforms.  The API is intended to provide all
services needed by a client program, such that client code does not need
to make any system calls or use other platform-specific services.

SIL is written to the C99 standard (with a few bits in C++, Objective C
for Mac OS X and iOS, and Java for Android) and should compile with any
standards-compliant compiler.  The build system is written to work with
GNU Make 3.81 or later, though it is of course possible to use other
build systems as well, and the sample SIL program found in the "sample"
directory includes an Xcode project that can be used to build and run
the SIL test suite on an iOS device.

Note that SIL was designed primarily for 2D games.  While it is possible
to render simple 3D scenes, SIL does not currently have the
infrastructure to support complex 3D rendering as can be done with
full-fledged 3D engines such as Unreal or Unity.


Supported platforms
-------------------
SIL includes code for running on the following platforms; each platform
has particular build requirements, as documented below.  Where a
description indicates that "by default" programs will run on a specific
version of the platform, the variable TARGET_OS_VERSION can be set in
the project's Makefile to explicitly require a newer platform version;
see the documentation in build/*/build.mk for details.

For details about supported features and library usage on each platform,
see the platform-specific README file.

- Android: Requires the Android 6.0 SDK (SDK version 23) or later,
  version 19 or later of the platform-tools and build-tools packages,
  and version 14 or later of the NDK.  Joystick (including the Xperia
  Play built-in control pad), keyboard (external and on-screen), and
  touch input are supported.  By default, programs will run on any
  device with Android 2.3.3 or later which supports OpenGL ES 2.0; note
  that ARM devices require a CPU supporting at least the ARMv7a
  instruction set.

- iOS (and tvOS): Requires Xcode 4.5 or later and version 7.0 or later
  of the iOS SDK (note that the iOS 7 SDK was first distributed with
  Xcode 5.0).  Touch input, game controller input (for devices running
  iOS 7.0 or later), and on-screen keyboard input are supported.  Game
  Center support is also available, including isolation of player
  profiles (such as save files or configuration data) and uploading of
  achievements to the Game Center server.  By default, programs will run
  on any device with iOS 8.0 or later; SIL also supports iOS versions
  back to 5.1.1 by setting TARGET_OS_VERSION appropriately, but see
  README-ios.txt for caveats when targeting such versions.

- Linux: Requires X11 and OpenGL headers and libraries, including the
  X11 extensions XInput2, Xinerama, XRandR, and XF86VidMode (these
  extensions are optional at runtime, but the headers must be present
  when building).  Programs will run on any system with an X11 windowing
  interface and a supported OpenGL implementation (see below).  Touch
  input requires version 2.2 of the XInput2 extension, which is included
  in version 1.12 and later of the X.org X11 server.

- Mac OS X: Requires Xcode 4.5 or later and version 10.9 or later of the
  OS X SDK.  By default, programs will run on OS X version 10.7.3 or
  later.

  If you get errors like "stdio.h not found" when building tool programs
  on OS X 10.8 or later, try reinstalling Xcode.  If that doesn't help,
  you may need to run the following command:
     sudo ln -fs $(xcrun --show-sdk-path)/usr/include /usr/

- Sony PlayStation Portable (PSP): Requires the unofficial PSP SDK and
  development toolchain from ps2dev.org.  Gentoo Linux users can install
  the SDK and toolchain using a Portage overlay; see
  <http://achurch.org/gentoo.html>.

- Windows: Requires a POSIX shell environment (see README-windows.txt)
  and GNU Make; when building with GCC, requires GCC 4.8.3 or later.
  Programs will run on Windows XP SP3 and all later versions of Windows
  with a supported OpenGL implementation (see below) or DirectX 11 (see
  README-windows.txt for caveats), though horizontal-scroll mouse wheels
  are not supported by Windows XP and touch input is not supported
  before Windows 8.

On PC-type platforms (Linux, Mac, and Windows), SIL requires an OpenGL
implementation which supports OpenGL 2.0 or later or (on Windows only)
Direct3D 11 feature level 9_1 or later.  As of 2018, the number of
PC-type devices in current use (and on which SIL programs would be run
-- excluding servers, for example) which do _not_ support at least one
of these requirements is believed to be negligible.

Note that some older graphics hardware, reportedly including chipsets
made by ATI/AMD in the late 2000s, may claim to support OpenGL 2.0 but
fail to support non-power-of-two textures (for which support is mandated
by OpenGL 2.0); SIL programs which use NPOT textures will not work
correctly on systems using such graphics hardware.  SIL itself does not
require NPOT texture support, though some of the built-in tests (see
below) assume support for NPOT textures with no mipmaps and no texture
coordinate wrapping, as in OpenGL ES 2.0.

On platforms using the 32-bit x86 architecture, SIL assumes the presence
of at least SSE2 extensions.  SSE2 was introduced in 2001 with the
Pentium 4, so this requirement should not be a problem in practice.


Structure
---------
SIL................ Top-level directory.
+-- build.......... Build system for SIL (see "Building a SIL-based
|                      program" below).
+-- external....... Source code for independent libraries used by SIL.
|                      See external/README.txt for details.
+-- include/SIL.... Header files containing public definitions and
|   |                  declarations for use by SIL client code.
|   +-- configs.... Header files containing common definitions for each
|   |                  supported system.
|   +-- math....... Header files for vector and matrix data types.
|   +-- resource... Header files for less commonly used resource-related
|   |                  functions.
|   +-- sound...... Header files for less commonly used sound-related
|   |                  functions.
|   +-- sysdep..... Header files for specific systems.  The common.h
|   |                  header in each directory (if present) augments
|   |                  <SIL/base.h> with system-dependent definitions
|   |                  and is included automatically with <SIL/base.h>.
|   +-- utility.... Header files for utility functions.
+-- sample......... A simple example demonstrating how to build a
|                      SIL-based program.
+-- src............ Source code for SIL.  This directory includes source
|   |                  files for top-level functionality.
|   +-- font....... Font management and text rendering functionality.
|   +-- graphics... Graphics-related functionality, excluding text
|   |                  rendering and texture management.
|   +-- math....... Common mathematical functions.
|   +-- movie...... Movie playback functions.
|   +-- resource... Resource (data file) management, including package
|   |                  file support.
|   +-- sound...... Sound channel control and decoding/filtering
|   |                  logic, as well as a software mixer for systems
|   |                  without (or with insufficient) hardware mixing
|   |                  capabilities.
|   +-- sysdep..... System-dependent code.  Each subdirectory of this
|   |   |              directory includes code for a specific system
|   |   |              (or technology, such as OpenGL) which interacts
|   |   |              directly with the host environment.
|   |   +-- misc... Miscellaneous system-level sources shared by
|   |                  multiple systems.
|   +-- test....... Test routines (see "Built-in tests" below).
|   +-- utility.... Miscellaneous utility routines.
+-- testdata....... Data files for the test routines.
+-- tools.......... Tool programs useful for manipulating data files.


Building a SIL-based program
----------------------------
SIL includes a Makefile-based build system which is designed to simplify
the process of building a SIL-based program on multiple platforms.  All
of the logic for performing the build, as well as auxiliary files used
by each platform, can be found in the "build" directory; depending on
program complexity, the program's own build script may be as simple as
a Makefile listing the program's source files and including "build.mk"
from the appropriate platform's build subdirectory.

The build system includes a number of configurable options, some
applicable to multiple platforms and some specific to particular
platforms.  See build/common/config.mk and build/*/build.mk for details.

The build system presumes that the SIL source tree is located within the
overall project tree.  It is possible to use the SIL build system with a
copy of SIL located outside the project tree by setting the TOPDIR
variable to the system's root directory (or some other common parent
directory), but this may result in unnecessarily verbose filenames in
log messages and other pathname-related inconveniences when debugging.

SIL includes copies of several external data processing libraries:
FreeType, libnogg, libpng, libvpx, libwebmdec, nestegg, and zlib.  By
default, these copies of the libraries will be built directly into the
final executable if the associated functionality is enabled, but it is
possible to instead use an external version (such as a system-wide
shared library) by setting the appropriate SIL_LIBRARY_INTERNAL_*
configuration variable to 0 in the build configuration.  Note that these
external libraries have their own licensing terms, separate from SIL;
see the file "external/README.txt" for details about each library's
license and distribution source.


Built-in tests
--------------
SIL includes a number of self-test routines which are enabled by setting
the SIL_INCLUDE_TESTS Makefile symbol to 1.  The tests cover all code in the
library except:
   - Platform-specific I/O functionality, such as hardware sound output,
     which cannot be tested programmatically.  (However, the ALSA-based
     Linux sound driver is tested using the ALSA loopback plugin if that
     plugin is available on the system.)
   - Handling for system errors in platform-specific code.  (Error
     handling in platform-independent code is tested through failure
     injection.)
Current test coverage is approximately 83% by number of executable
lines, including 100% of platform-independent code, 94% of OpenGL code,
and 74% of other platform-specific code.  (Coverage statistics for code
executed on Android, iOS, and Mac OS X are inaccurate due to bugs in the
Clang toolchain; see note below.)

The tests can be run by building and executing the included sample
program (in the "sample" directory) as follows:

- On systems with command line support (Linux, Mac, Windows), build the
  program by running "make" in the sample/build directory, then pass the
  the command-line option "-test" to the program.  The program will exit
  with a successful status if all tests succeed, or an error status if
  any tests fail.  It is also possible to run just a subset of tests
  with the option "-test=name1,name2,..." (see the definition of tests[]
  in src/test/test-harness.c for a list of valid test names).

- The PSP build also uses a "-test" command-line option, and thus tests
  can only be run from the PSPlink debugging shell.  Use a command line
  like "program.prx -test" to run the tests.

- For Android, if SIL_INCLUDE_TESTS is defined at the Makefile level (as is
  the case for the sample program), the package will include an extra
  activity for running tests.  Running "make test" for an Android build
  will start this activity instead of the default activity (which runs
  the program normally).  There is currently no way to select only a
  subset of tests for running.

- For iOS, since there is no way to pass options at runtime, a separate
  preprocessor macro (#define RUN_TESTS) is used to control whether the
  tests are run.  If both SIL_INCLUDE_TESTS and RUN_TESTS are defined, the
  executable will run tests and exit instead of running the program
  normally.  RUN_TESTS can be enabled by passing "RUN_TESTS=1" on the
  "make" command line.  There is currently no way to select only a
  subset of tests for running.

The tests include many intentional errors, such as invalid calling
patterns and references to nonexistent files, which will generate
warning messages via the DLOG() debug logging facility.  A summary of
test results as well as any failure messages will be output using DLOG()
when the tests complete, so that you do not need to scroll back through
the log output to find test failures.  For example:

src/test/test-harness.c:520(show_results): ======== TEST RESULTS ========
src/test/test-harness.c:522(show_results): All tests passed.
src/test/test-harness.c:537(show_results): ==============================

or

src/test/test-harness.c:520(show_results): ======== TEST RESULTS ========
src/test/test-harness.c:528(show_results):      passed: condvar
[...]
src/test/test-harness.c:532(show_results): [*]  FAILED: resource_core
src/test/test-harness.c:530(show_results): [*] skipped: resource_pkg
[...]
src/test/test-harness.c:537(show_results): ==============================
src/test/test-harness.c:539(show_results): Failure log follows:
src/test/resource/core.c:123(test_foo): FAIL: this test failed

Additionally, if any tests fail (such as "resource_core" in the above
failure example), tests which depend on them (such as "resource_pkg" in
the above example) will not be run, in order to avoid reporting multiple
failures all caused by the same bug.  When running individual tests, the
dependencies for a test can be suppressed by prefixing an "=" to the
test name; for example, "-test==test1,=test2" will run the "test1" and
"test2" tests but not any tests on which they depend.

On PC-type platforms, some of the input tests can be affected by real
input such as mouse movements, and some mouse-related tests deliberately
change the position of the system mouse pointer.  The tests make an
effort to isolate themselves from the rest of the system, but if (for
example) you are actively moving the mouse during the platform-specific
input tests, some of the tests may see more input events than they
expected and thus fail even though the code itself is correct, or other
programs may react in unexpected ways to the synthetic mouse movements.


Coverage analysis
-----------------
The build control files (build/*/build.mk) also include rules to build
the program for coverage analysis.  These are primarily intended for
testing coverage of SIL itself using the included sample program, but
they can also be used to measure coverage of client code.  The process
for performing coverage analysis varies by system:

- Android: Use "make COVERAGE=1" to build for coverage analysis, then
  install ("make install") and run ("make test") as usual, or use
  "make all-test COVERAGE=1" to do all three steps at once.  After a
  run, use "make pull-coverage" (with the device connected) to retrieve
  the coverage data and write results to coverage.out in the build
  directory.  To ensure previous runs do not contaminate the results,
  run "adb shell rm -r /mnt/sdcard/coverage" before each run.  You can
  change the path to which coverage data is written by specifying
  GCOV_PREFIX=/absolute/path on the build command line.  Note that
  llvm-cov must be installed and in the shell's executable path on the
  build system, since it is no longer distributed as part of the NDK.

- iOS: Use "make COVERAGE=1 RUN_TESTS=1" to build for testing with
  coverage analysis, then install and run the program on a device.  When
  the program completes, use Xcode to download the application data
  directory; the coverage data files will be found under the
  "Library/Application Support" directory, using the same path structure
  as on the build system.  Find the top-level object directory (normally
  "obj" or "obj-<architecture>" under the build directory) in that
  directory tree, merge its contents into the build tree (using
  "cp -a SRC/build/obj DEST/build/", for example), then run
  "make gen-coverage" to generate a coverage.out file in the build
  directory containing the coverage results (append
  OBJDIR="obj-<architecture>" for a multi-architecture build).  Note
  that coverage analysis requires Xcode 7 or later.

- Linux: Use "make test-coverage" to build the program, run tests, and
  write coverage results to coverage.out in the build directory.  For
  the sample program, you can also run "make coverage" to automatically
  generate HTML output from the coverage results (see below).

- Mac OS X: Identical to Linux.  Note that coverage analysis requires
  Xcode 7 or later.

- PSP: Use "make COVERAGE=1" to build for coverage analysis, then run
  the executable on a device.  On exit, the executable will write
  coverage data (*.gcda) files to the same absolute pathnames on the
  filesystem device under which the executable is running: for example,
  if the absolute path to the program's build directory is
  "/home/user/project/build/psp" and the executable is run from the
  "host0:" device on the PSP, the *.gcda files will be written under
  "host0:/home/user/project/build/psp".  Move these files to the same
  pathnames on the build filesystem (so the *.gcda files end up in the
  same directories as the corresponding *.gcno files), then run "make
  gen-coverage" to generate coverage.out in the build directory.  To
  ensure previous runs do not contaminate the results, remove the
  directory tree containing the *.gcda files on the PSP filesystem
  before each run.

- Windows: Identical to Linux.

In all cases, coverage and non-coverage object files share the same
output directory, so be sure to run "make clean" before and after
coverage testing.

The coverage.out file generated by these build rules can be passed to
the tools/cov-html.pl script on that file to create a tree of HTML files
showing line and branch coverage for each source file.  By default, the
HTML file tree is written to a "coverage" subdirectory in the current
directory; use the --output-dir option to change the destination path.
On PC-type systems, running "make coverage" for the sample program will
execute this step automatically, writing the HTML tree to the directory
"sample/build/coverage".  The HTML generation step can also be run
separately with "make coverage-html" in the sample/build directory.

If you run the same program on two or more platforms, you can merge the
coverage.out files from each platform by passing them through the
tools/cov-merge.pl script and sending its output to tools/cov-html.pl.
The input files must all be for executables built from the same source
code; cov-merge.pl will abort with an error if it finds any differences.

Note that branch counts for code executed on multiple systems will be
inflated, since each branch path is counted once for each system.  In
particular, this implies that when a branch path can only be taken on
some systems and not on others, a combined analysis will never report
100% branch coverage for that branch even if all branch paths are in
fact covered.  Source lines are not overcounted in this way, and the
line counts will be accurate except in that different compilers may
assign different line numbers to the same piece of code -- for example,
one compiler may use the closing brace of a loop block for the loop
termination branch, while another compiler may use the loop statement
for the same branch.

Also note that the Clang compiler and associated LLVM tools currently
(as of version 3.5) do not seem to produce accurate output; for example,
they sometimes claim that comment lines were executed or missed.  This
particularly affects the iOS and Mac OS X platforms, for which Clang is
the only available compiler.


Tool programs
-------------
SIL includes several tool programs in the "tools" directory which can be
useful in creating or converting data files.  Notable among these are
the "pngtotex" program, which converts a PNG image to the optimized TEX
format used by SIL for textures, and the "build-pkg" program, which
generates a data package file from which resources can be efficiently
loaded.  Documentation for each program can be found in comments at the
top of the program's source code or by passing the "-h" command line
option to the program.

Some of the tool programs require external libraries to be installed on
the build system: "build-pkg" and "extract-pkg" require zlib, and
"pngtotex" requires libpng.  If you get link errors when trying to
compile the programs, consult your system's documentation for how to
install these libraries, or build and install them yourself from the
relevant library source code packages.  (SIL includes the source code
for these libraries under the "external" directory, but those copies are
intended only for building into SIL client programs and they have been
stripped of some build-related files, so they may not be suitable for
independent library builds.)

Additionally, when converting textures to compressed data formats, the
pngtotex tool requires external tools to be available on the build
system.  PVRTC format requires the PVRTexToolCLI tool (version 3.40 or
any later version with a compatible command-line interface) from the
PowerVR SDK distributed by ImgTec; S3TC (DXTn) format requires a custom
"dxtcomp" script as described in the pngtotex documentation (see the
pngtotex.c source file for details).
