#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/common/config.mk: Build configuration settings.
#

#
# This file lists various settings which are used in the build process.
# If the caller (top-level Makefile or command line) sets values for any
# of these variables, those values will be used instead of the defaults
# given here.
#
# Note that while this file only provides variables for the most common
# configuration settings, the calling Makefile can define any other
# settings from base.h by adding appropriate flags to $(SIL_EXTRA_FLAGS)
# (see sources.mk).
#

###########################################################################
######################## Program behavior options #########################
###########################################################################

# $(DEBUG):  Set this to 1 to build in debugging mode, or 0 (or empty) to
# disable debugging features (which may slightly improve performance).

DEBUG ?= 1


# $(SIL_INCLUDE_TESTS):  Set this to 1 to include test routines when building
# in debugging mode (this is ignored if debugging is disabled).  For
# command-line systems, tests can typically be run by passing the argument
# "-test" on the command line; see the README for details.

SIL_INCLUDE_TESTS ?= 0


# $(SIL_DEBUG_USE_VALGRIND):  Set this to 1 to enable detection of the
# Valgrind tool when running in debugging mode.  This avoids hiding
# uninitialized memory accesses with the uninitialized-memory fill executed
# by mem_alloc() and mem_realloc() in debugging mode, and avoids a spurious
# utility_id_array test failure.  Valgrind's headers are assumed to be
# includable with:
#    #include <valgrind/valgrind.h>
#    #include <valgrind/memcheck.h>

SIL_DEBUG_USE_VALGRIND ?= 0


# $(SIL_ENABLE_CXX_EXCEPTIONS):  Set this to 1 to enable C++ exceptions.
# Not all systems support this.  SIL itself does not use exceptions, but
# this setting is useful if client code or external libraries need to use
# exceptions.  The default is 0 (exceptions disabled).

SIL_ENABLE_CXX_EXCEPTIONS ?= 0


# $(SIL_LINK_CXX_STL):  If set to 1, the C++ STL will be linked into the
# final executable.  The default is 0 (STL will not be linked in).

SIL_LINK_CXX_STL ?= 0


# $(SIL_MATH_ASSUME_ROUND_TO_NEAREST):  If set to 1, SIL will optimize
# certain functions on the assumption that floating-point instructions
# round inexact values to the nearest representable result.  This is the
# default behavior on all supported platforms, and there is no reason to
# disable this optimization unless client code manually changes the
# floating-point rounding mode.  The default is 1 (optimization will be
# performed).

SIL_MATH_ASSUME_ROUND_TO_NEAREST ?= 1


# $(SIL_MEMORY_CHECK_POINTERS):  If set to 1, pointers passed to
# mem_realloc() and mem_free() will be checked for validity by
# traversing the global allocation list.  This will naturally slow down
# such calls.

SIL_MEMORY_CHECK_POINTERS ?= 0


# $(SIL_MEMORY_DEBUG_FILL_UNUSED):  If set to 1 and debugging is enabled
# ($(DEBUG) is set to 1), newly allocated memory blocks will be filled
# with 0xBB (when MEM_ALLOC_CLEAR is not specified), and freed blocks will
# be filled with 0xDD.  If debugging is not enabled, this option has no
# effect.  Note that enabling this behavior can have a significant
# performance impact on slow systems.

SIL_MEMORY_DEBUG_FILL_UNUSED ?= 1


# $(SIL_MEMORY_FORBID_MALLOC):  If set to 1, the malloc() family of memory
# management functions (malloc(), calloc(), realloc(), strdup(), and
# free()) will be made unavailable to client code.  This can be useful
# with a custom memory allocator to ensure that all allocations go
# through the mem_alloc() family of functions.

SIL_MEMORY_FORBID_MALLOC ?= 0


# $(SIL_MEMORY_LOG_ALLOCS):  If set to 1, all memory allocation/free
# operations (whether successful or not) that go through the mem_*()
# interface will be logged via DLOG().

SIL_MEMORY_LOG_ALLOCS ?= 0


# $(SIL_STRFORMAT_USE_FLOATS):  If set to 1, the [v]strformat() functions
# will use single-precision instead of double-precision arithmetic when
# printing floating point values.  This can reduce code size and improve
# execution speed in environments without native double-precision support.

SIL_STRFORMAT_USE_FLOATS ?= 0


# $(SIL_STRTOF_CUSTOM):  If set to 1, a custom, optimized version of
# strtof() will be built into SIL and made available to client code.  This
# version uses single-precision floating point throughout, unlike many
# libraries which use double precision or simply alias strtof() to strtod(),
# and is significantly faster on hardware without double-precision floating
# point support.

SIL_STRTOF_CUSTOM ?= 0


# $(SIL_STRTOF_OVERRIDE_LIBRARY):  If set to 1, the library code will
# assume it can override the strtof() function provided by the standard
# system libraries with the custom, optimized version enabled by
# $(SIL_STRTOF_CUSTOM).  This allows external libraries which call
# strtof(), such as the C++ STL, to make use of the custom version.
#
# If you set this to 1 and attempt to build with a library that does not
# allow overriding, you'll get "multiple definition" errors during the
# final link.
#
# Setting this to 0 will cause the build to use the standard library's
# version of strtof() for external libraries.  SIL client code will still
# use the custom version, which will be renamed via a #define to avoid
# colliding with the system libraries.

SIL_STRTOF_OVERRIDE_LIBRARY ?= 0


# $(SIL_TEST_VERBOSE_LOGGING):  Log debug messages for each test routine
# (including initialization and cleanup routines) called by the generic
# test runner.  Can be useful in pinpointing memory errors, but generates
# huge amounts of DLOG() output.

SIL_TEST_VERBOSE_LOGGING ?= 0


# $(SIL_USE_STL_ALGORITHM):  If set to 1, the STL <algorithm> header will
# be automatically included for C++ sources, and STL's min() and max()
# functions will be used in place of SIL's own custom functions.  This
# setting has no effect on C sources.

SIL_USE_STL_ALGORITHM ?= 0


# $(SIL_UTILITY_NOISY_ERRORS):  Set this to 1 to display internal error
# messages using dialog boxes (if supported on the system).

SIL_UTILITY_NOISY_ERRORS ?= 1


# $(SIL_UTILITY_PNG_ALLOC_CHUNK):  Sets the block size for memory
# allocation operations performed by png_create().  Larger values reduce
# overhead from buffer reallocations, but png_create() may require up to
# this amount of extra memory over the final data size while creating the
# PNG data.  (The buffer returned by png_create() will be shrunk to exactly
# the required size, so any wasted space is freed before return.)  If not
# defined, defaults to 65536.

SIL_UTILITY_PNG_ALLOC_CHUNK ?=


# $(SIL_UTILITY_PNG_COMPRESSION_LEVEL):  Sets the default compression level
# for compressing PNG images, such as save file screenshots.  If not
# defined, defaults to zlib's default compression level.

SIL_UTILITY_PNG_COMPRESSION_LEVEL ?=


# $(SIL_UTILITY_PNG_MAX_SIZE):  Sets the maximum pixel size (width or
# height) accepted by the png_parse() function.  Images with a size larger
# than this value in either dimension will be rejected.  If not defined or
# empty, defaults to 16384.

SIL_UTILITY_PNG_MAX_SIZE ?=

###########################################################################
######################## External library options #########################
###########################################################################

# $(SIL_FONT_INCLUDE_FREETYPE):  Set this to 1 to include support for
# TrueType/OpenType font rendering using the FreeType library.

SIL_FONT_INCLUDE_FREETYPE ?= 1


# $(SIL_MOVIE_INCLUDE_WEBM):  Set this to 1 to include software-based WebM
# decoding support.

SIL_MOVIE_INCLUDE_WEBM ?= 1


# $(SIL_SOUND_INCLUDE_OGG):  Set this to 1 to include Ogg Vorbis audio
# support.

SIL_SOUND_INCLUDE_OGG ?= 1


# $(SIL_UTILITY_INCLUDE_PNG):  Set this to 1 to include PNG image support.
# Requires $(SIL_UTILITY_INCLUDE_ZLIB) to also be enabled.

SIL_UTILITY_INCLUDE_PNG ?= 1


# $(SIL_UTILITY_INCLUDE_ZLIB):  Set this to 1 to include deflate (RFC 1951)
# compression/decompression support via zlib.  If disabled, decompression
# is still available using the tinflate library built into SIL.

SIL_UTILITY_INCLUDE_ZLIB ?= 1

#-------------------------------------------------------------------------#

# $(SIL_LIBRARY_INTERNAL_FREETYPE):  Set this to 1 to build the internal
# copy of the FreeType library in the external/ directory.  If disabled
# (set to 0), the calling Makefile is responsible for linking the FreeType
# library into the executable if necessary.
#
# This setting has no effect if $(SIL_FONT_INCLUDE_FREETYPE) is disabled.

SIL_LIBRARY_INTERNAL_FREETYPE ?= 1


# $(SIL_LIBRARY_INTERNAL_LIBNOGG):  Set this to 1 to build the internal
# copy of the libnogg library in the external/ directory.  If disabled (set
# to 0), the calling Makefile is responsible for linking the libnogg
# library into the executable if necessary.
#
# This setting has no effect if $(SIL_SOUND_INCLUDE_OGG) is disabled.

SIL_LIBRARY_INTERNAL_LIBNOGG ?= 1


# $(SIL_LIBRARY_INTERNAL_LIBPNG):  Set this to 1 to build the internal copy
# of the libpng library in the external/ directory.  If disabled (set to 0),
# the calling Makefile is responsible for linking the libpng library into
# the executable if necessary.
#
# This setting has no effect if $(SIL_UTILITY_INCLUDE_PNG) is disabled.

SIL_LIBRARY_INTERNAL_LIBPNG ?= 1


# $(SIL_LIBRARY_INTERNAL_LIBVPX):  Set this to 1 to build the internal copy
# of the libvpx library in the external/ directory.  If disabled (set to 0),
# the calling Makefile is responsible for linking the libvpx library into
# the executable if necessary.
#
# This setting has no effect if $(SIL_UTILITY_INCLUDE_WEBM) is disabled.

SIL_LIBRARY_INTERNAL_LIBVPX ?= 1


# $(SIL_LIBRARY_INTERNAL_LIBVPX_ASM):  Set this to 1 to include assembly
# sources when building the internal copy of the libvpx library.  For x86
# architectures, this requires the yasm tool to be installed on the system.
# If disabled (set to 0), only the slower C-language routines will be
# compiled.  The default is to include assembly sources on non-x86 systems
# only.
#
# This setting has no effect if $(SIL_LIBRARY_INTERNAL_LIBVPX) is disabled.

SIL_LIBRARY_INTERNAL_LIBVPX_ASM ?= $(if $(filter x86%,$(CC_ARCH)),0,1)


# $(SIL_LIBRARY_INTERNAL_LIBWEBMDEC):  Set this to 1 to build the internal
# copy of the libwebmdec library in the external/ directory.  If disabled
# (set to 0), the calling Makefile is responsible for linking the
# libwebmdec library into the executable if necessary.
#
# This setting has no effect if $(SIL_UTILITY_INCLUDE_WEBM) is disabled.

SIL_LIBRARY_INTERNAL_LIBWEBMDEC ?= 1


# $(SIL_LIBRARY_INTERNAL_NESTEGG):  Set this to 1 to build the internal
# copy of the nestegg library in the external/ directory.  If disabled (set
# to 0), the calling Makefile is responsible for linking the nestegg
# library into the executable if necessary.
#
# This setting has no effect if $(SIL_UTILITY_INCLUDE_WEBM) is disabled.

SIL_LIBRARY_INTERNAL_NESTEGG ?= 1


# $(SIL_LIBRARY_INTERNAL_ZLIB):  Set this to 1 to build the internal copy
# of the zlib library in the external/ directory.  If disabled (set to 0),
# the calling Makefile is responsible for linking the zlib library into the
# executable if necessary.
#
# This setting has no effect if $(SIL_UTILITY_INCLUDE_ZLIB) is disabled.

SIL_LIBRARY_INTERNAL_ZLIB ?= 1

###########################################################################
########################## Build control options ##########################
###########################################################################

# $(COVERAGE):  If set to 1, the program will be built to record and save
# code coverage information.  Note that optimization will be disabled for
# a coverage-enabled build to ensure accurate reporting of covered lines
# and branches.

COVERAGE ?= 0


# $(COVERAGE_EXCLUDE):  List of path prefixes to exclude from coverage
# reporting.  Pathnames should be relative to $(TOPDIR).

COVERAGE_EXCLUDE ?=


# $(COVERAGE_SOURCES):  List of additional source files (outside of SIL)
# to analyze when performing coverage analysis.  Pathnames should be
# relative to $(TOPDIR).

COVERAGE_SOURCES ?=


# $(OBJDIR):  Pathname (relative to the build directory) of the directory
# in which object files and other intermediate files will be stored.

OBJDIR ?= obj


# $(SDK_VERSION):  SDK version with which to build, for platforms which
# have the concept of SDK versions.  If empty, a system-dependent default
# will be used.

SDK_VERSION ?=


# $(SHOW_ARCH_IN_BUILD_MESSAGES):  If set to 1, the current architecture
# set by $(TARGET_ARCH_ABI) will be included in the concise build messages
# displayed for compilation rules.  This can be useful on platforms which
# allow building for multiple architectures at once.

SHOW_ARCH_IN_BUILD_MESSAGES ?= 0


# $(TARGET_ARCH_ABI):  Target CPU architecture and/or ABI, for platforms
# which support multiple architectures or ABIs.  If empty, a
# system-dependent default will be used.

TARGET_ARCH_ABI ?=


# $(TARGET_OS_VERSION):  Minimum OS version which should be required to run
# the built program, for platforms which have such a concept.  If empty, a
# system-dependent default (typically the OS version corresponding to
# $(SDK_VERSION)) will be used.

TARGET_OS_VERSION ?=


# $(WARNINGS_AS_ERRORS):  If set to 1, the source code will be compiled
# with warnings treated as errors, to cause the build to abort if a warning
# is detected.  (When building with the GCC compiler, for example, this
# adds the -Werror option to the command line.)

WARNINGS_AS_ERRORS ?= 0


# $(YASM):  Pathname of the "yasm" program, for compiling x86 assembly
# sources in the libvpx library.

YASM ?= yasm

###########################################################################
############################ Packaging options ############################
###########################################################################

# $(EXECUTABLE_NAME):  Name of the executable file to be generated.  Some
# platforms may add an automatic extension (like .exe) to this name.  If
# empty, no rules for building an executable will be included, and the
# calling Makefile is responsible for defining any required build rules.

EXECUTABLE_NAME ?=


# $(PACKAGE_NAME):  OS-specific package name to be used by the program,
# if required (for example, the Java package name for Android, or the
# bundle identifier for iOS).

PACKAGE_NAME ?= com.example.$(EXECUTABLE_NAME)


# $(PROGRAM_NAME):  Name of the program, as it should be shown to users,
# for platforms which have such a concept.

PROGRAM_NAME ?= SIL App


# $(PROGRAM_VERSION):  User-readable version string for the program, for
# platforms which have such a concept.

PROGRAM_VERSION ?= 1.0


# $(PROGRAM_VERSION_CODE):  Internal version code for the program, for
# platforms which have such a concept.  When supported, this generally
# must take a specific format defined by the platform.

PROGRAM_VERSION_CODE ?= 1


# $(RESOURCES):  List of resource files to package with the executable,
# specified as the relative pathname by which the resource will be loaded.
#
# Each file specified here requires a corresponding rule whose target is
# that pathname.  For example:
#
# RESOURCES = file1.txt dir/file2.dat
# file1.txt: [...]
# dir/file2.dat: [...]

RESOURCES ?=

###########################################################################
###########################################################################
