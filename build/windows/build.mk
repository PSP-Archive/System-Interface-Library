#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/windows/build.mk: Build control file for Windows.
#

#
# Windows-specific notes:
#
# - $(PROGRAM_VERSION_CODE) is used as the version code in the application
#   manifest, and it must follow the Windows version format: four 16-bit
#   unsigned integer values separated by dots (e.g., 1.2.3.45678).  If the
#   version code syntax is incorrect, the program will fail start,
#   displaying an error along the lines of "the side-by-side configuration
#   is incorrect".
#
# - $(TARGET_ARCH_ABI) can be set to either x86 (32-bit) or amd64 (64-bit).
#   The default is x86.
#
# -------------------------------------------------------------------------
#
# To use this file, include it in your Makefile after defining values for
# configuration variables and source lists, but before defining any rules.
# For example:
#
# PROGRAM_NAME = MyProgram
# EXECUTABLE_NAME = my-program
#
# MY_SOURCES = my-program.c
# MY_OBJECTS_C = $(MY_SOURCES:$(TOPDIR)/%.c=$(OBJDIR)/%$(OBJECT_EXT))
# OBJECT_GROUPS += MY
# AUTOGEN_HEADERS += my-date.h
#
# include SIL/build/windows/build.mk
#
# .PHONY: my-date.h  # Force regeneration on every build.
# my-date.h:
# 	date +'#define DATE "%Y-%m-%d %H:%M:%S"' >$@
#

###########################################################################
################ Defaults for common configuration options ################
###########################################################################

# Make the default version code valid for Windows.
PROGRAM_VERSION_CODE ?= 1.0.0.0

# strtof() is missing in the Visual Studio runtime libraries (at least in
# VS.NET 2003), so always enable our own.
SIL_STRTOF_CUSTOM ?= 1

# Default to a 32-bit build.
TARGET_ARCH_ABI ?= x86

###########################################################################
################# Windows-specific configuration options ##################
###########################################################################

#----------------------- Program behavior options ------------------------#

# $(USE_PRECOMPILED_D3D_SHADERS):  If set to 1, precompiled bytecode for
# the Direct3D shaders used in the default rendering pipeline will be built
# into SIL.  This can help avoid delays when specific rendering patterns
# are used for the first time after program startup, but it adds about 300k
# of read-only data to the program size.

USE_PRECOMPILED_D3D_SHADERS ?= 0

#------------------------- Build control options -------------------------#

# $(LINK_MODE):  The program link mode, either "windows" or "console".
# "console" causes the program to open a command prompt window when started
# and allows debug log output to be sent to the console.  Note that this is
# not required to view log output if running the program under the MinGW
# console.

LINK_MODE ?= windows

#--------------------------- Packaging options ---------------------------#

# $(ICON_FILE):  The path of an ICO-format icon file to use as the main
# application icon for the program.  If not set, the program will not have
# a custom icon.

ICON_FILE ?=


# $(MANIFEST_TEMPLATE):  The path of an application manifest XML template
# to use to generate the application manifest.  Certain @-delimited strings
# are replaced at build time with the appropriate values from build
# settings; see the default template (manifest.xml.in) for details.

MANIFEST_TEMPLATE ?= $(_THISDIR)/manifest.xml.in

#-------------------------------------------------------------------------#

# Validate $(PROGRAM_VERSION_CODE) so the user doesn't have to wait for the
# build to complete before finding that the version code format is wrong.
ifeq (,$(shell echo '.$(PROGRAM_VERSION_CODE)' | grep -E '^(\.(0|[1-9][0-9]{0,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])){1,4}$$'))
    $(error Invalid syntax for PROGRAM_VERSION_CODE ($(PROGRAM_VERSION_CODE)))
endif

###########################################################################
######################### Build environment setup #########################
###########################################################################

_THISDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(_THISDIR)/../common/base.mk
include $(_THISDIR)/../common/config.mk


ifeq ($(TARGET_ARCH_ABI),x86)
    TOOLCHAIN_PREFIX = i686-w64-mingw32-
else ifeq ($(TARGET_ARCH_ABI),amd64)
    TOOLCHAIN_PREFIX = x86_64-w64-mingw32-
else
    $(error Invalid value for TARGET_ARCH_ABI ($(TARGET_ARCH_ABI)))
endif


ifneq (,$(filter undefined default,$(origin CC)))
CC = $(TOOLCHAIN_PREFIX)gcc
endif

OBJECT_EXT  = .o
LIBRARY_EXT = .a

SYS_FLAGS = -pie \
            $(CFLAG_DEFINE)WINVER=0x0501 \
            $(CFLAG_DEFINE)_WIN32_WINNT=0x0501 \
            $(CFLAG_DEFINE)NTDDI_VERSION=0x05010300
ifeq ($(TARGET_ARCH_ABI),x86)
    # Our preferred stack boundary is 2^4 == 16 bytes for SSE instructions,
    # but 32-bit Windows doesn't honor that, so tell GCC to expect 4-byte
    # alignment.
    SYS_FLAGS += $(if $(filter gcc clang,$(CC_TYPE)),-mincoming-stack-boundary=2)
endif

SYS_CFLAGS    = $(SYS_FLAGS)
SYS_CXXFLAGS  = $(SYS_FLAGS)
SYS_OBJCFLAGS = $(SYS_FLAGS)

SYS_LDFLAGS = \
    $(if $(filter console,$(LINK_MODE)),-mconsole,-mwindows) \
    -Wl,--enable-auto-import -Wl,--no-seh -Wl,--nxcompat \
    -pie -Wl,--dynamicbase \
    $(if $(filter x86,$(TARGET_ARCH_ABI)),-Wl$(,)--large-address-aware)
# Work around silly bugs and defaults in mingw-w64 ld as of binutils-2.27
# (see: https://sourceware.org/bugzilla/show_bug.cgi?id=19011)
SYS_LDFLAGS += \
    -Wl,--pic-executable \
    -Wl,-e$(if $(filter x86,$(TARGET_ARCH_ABI)),_)WinMainCRTStartup \
    -Wl,--disable-auto-image-base \
    $(if $(filter amd64,$(TARGET_ARCH_ABI)),-Wl$(,)--high-entropy-va) \
    $(if $(filter amd64,$(TARGET_ARCH_ABI)),-Wl$(,)--image-base$(,)0x140000000)
# This is needed for the hid.dll and xinput*.dll wrappers in
# src/test/sysdep/windows/input.c to be successfully looked up with
# GetProcAddress().  Note that this only works on 32-bit x86.
SYS_LDFLAGS += $(if $(and $(filter x86,$(TARGET_ARCH_ABI)),$(filter 1,$(SIL_INCLUDE_TESTS))),-Wl$(,)--kill-at)

BASE_LIBS = $(if $(filter 1,$(DEBUG)),-lpsapi) \
            -ldxguid -lwinmm -lgdi32 -lole32 -lshell32 -luser32

include $(_THISDIR)/../common/toolchain.mk

CC_LD = $(if $(filter 1,$(SIL_LINK_CXX_STL)),$(CXX),$(CC))


# GCC through version 4.8.2 has a bug which can destroy the first argument
# to a local function in certain cases, so we disallow those versions.
# See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56807 for details.
ifneq ($(and $(filter gcc,$(CC_TYPE)),$(call CC_VERSION-is-not-at-least,4.8.3)),)
    $(warning --- This version of GCC ($(CC_VERSION)) has a known bug which can cause)
    $(warning --- crashes in compiled programs.  Please use GCC 4.8.3 or later.)
    $(error GCC version too old)
endif


# Program paths used only in this file.
WINDRES ?= $(if $(filter $(TOOLCHAIN_PREFIX)%,$(CC)),$(TOOLCHAIN_PREFIX))windres

###########################################################################
################# Source file lists and associated flags ##################
###########################################################################

SIL_SYS_CONFIG_HEADER = windows.h

SIL_SYS_SOURCES = $(SIL_OPENGL_SOURCES) \
                  sysdep/misc/ioqueue.c \
                  sysdep/misc/joystick-db.c \
                  sysdep/misc/joystick-hid.c \
                  sysdep/misc/movie-none.c \
                  sysdep/misc/sysfont-none.c \
                  sysdep/windows/condvar.c \
                  sysdep/windows/d3d-base.c \
                  sysdep/windows/d3d-framebuffer.c \
                  sysdep/windows/d3d-inputlayout.c \
                  sysdep/windows/d3d-primitive.c \
                  sysdep/windows/d3d-shader.c \
                  sysdep/windows/d3d-state.c \
                  sysdep/windows/d3d-texture.c \
                  sysdep/windows/debug.c \
                  sysdep/windows/files.c \
                  sysdep/windows/graphics.c \
                  sysdep/windows/input.c \
                  sysdep/windows/log.c \
                  sysdep/windows/main.c \
                  sysdep/windows/misc.c \
                  sysdep/windows/mutex.c \
                  sysdep/windows/semaphore.c \
                  sysdep/windows/sound.c \
                  sysdep/windows/sound-wasapi.c \
                  sysdep/windows/sound-winmm.c \
                  sysdep/windows/thread.c \
                  sysdep/windows/time.c \
                  sysdep/windows/userdata.c \
                  sysdep/windows/utf8-wrappers.c \
                  sysdep/windows/util.c \
                  $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                      test/sysdep/misc/ioqueue.c \
                      test/sysdep/misc/joystick-db.c \
                      test/sysdep/misc/joystick-hid.c \
                      test/sysdep/windows/condvar.c \
                      test/sysdep/windows/d3d-core.c \
                      test/sysdep/windows/files.c \
                      test/sysdep/windows/graphics.c \
                      test/sysdep/windows/input.c \
                      test/sysdep/windows/main.c \
                      test/sysdep/windows/misc.c \
                      test/sysdep/windows/thread.c \
                      test/sysdep/windows/time.c \
                      test/sysdep/windows/userdata.c \
                      test/sysdep/windows/utf8-wrappers.c \
                      test/sysdep/windows/util.c \
                  )

SIL_SYS_FLAGS += $(if $(ICON_FILE),$(CFLAG_DEFINE)HAVE_DEFAULT_ICON) \
                 $(if $(filter 1,$(USE_PRECOMPILED_D3D_SHADERS)),$(CFLAG_DEFINE)SIL_PLATFORM_WINDOWS_PRECOMPILED_D3D_SHADERS)

include $(_THISDIR)/../common/sources.mk

###########################################################################
############################### Build rules ###############################
###########################################################################

ifneq ($(EXECUTABLE_NAME),)
SIL_HELP_all           = 'build the executable ($(EXECUTABLE_NAME).exe)'
endif
SIL_HELP_gen-coverage  = 'generate coverage.out from coverage data'
SIL_HELP_coverage-html = 'generate HTML coverage results from coverage.out'
SIL_HELP_clean         = 'remove all intermediate files and dependency data'
SIL_HELP_spotless      = 'remove all generated files, including the executable'

include $(_THISDIR)/../common/rules.mk

# The resource file is not an "object file" per se, but we include it on
# the link command line with everything else.
ALL_OBJECTS += $(EXECUTABLE_NAME).res

#-------------------------------------------------------------------------#

.PHONY: all clean spotless

all: $(if $(EXECUTABLE_NAME),$(EXECUTABLE_NAME).exe) $(RESOURCES)

clean:
	$(ECHO) 'Removing object files'
	$(Q)rm -rf obj
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing generated files'
	$(Q)rm -f '$(EXECUTABLE_NAME)'.exe.ico
	$(Q)rm -f '$(EXECUTABLE_NAME)'.exe.manifest
	$(Q)rm -f '$(EXECUTABLE_NAME)'.rc
	$(Q)rm -f '$(EXECUTABLE_NAME)'.res
	$(ECHO) 'Removing temporary files'
	$(Q)rm -rf .covtmp
endif
	$(SIL_RECIPE_clean)

spotless: clean
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing executable file'
	$(Q)rm -rf '$(EXECUTABLE_NAME).exe'
endif
	$(ECHO) 'Removing coverage data'
	$(Q)rm -rf coverage coverage.out
	$(_SIL_TOOLS_RECIPE_spotless)
	$(SIL_RECIPE_spotless)

#-------------------------------------------------------------------------#

ifneq ($(EXECUTABLE_NAME),)
# Sort objects in lexical order to make it easier to estimate build progress.
$(EXECUTABLE_NAME).exe: $(sort $(ALL_OBJECTS))
	$(ECHO) 'Linking $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(CC_LD) \
	    $(BASE_LDFLAGS) $(ALL_LDFLAGS) $(LDFLAGS) \
	    $(ALL_OBJECTS) \
	    $(ALL_LIBS) $(BASE_LIBS) $(LIBS) \
	    $(CFLAG_LINK_OUTPUT)'$@'
endif

#-------------------------------------------------------------------------#

ifneq ($(EXECUTABLE_NAME),)

# We can't use the source icon filename directly in the .rc file because
# (when cross-compiling from a POSIX-style environment) windres expects
# absolute pathnames to start with a drive letter, and $(ICON_FILE) may be
# a POSIX-style path starting with a "/".
LOCAL_ICON_FILE = $(if $(ICON_FILE),$(EXECUTABLE_NAME).exe.ico)

$(EXECUTABLE_NAME).res: $(EXECUTABLE_NAME).rc $(EXECUTABLE_NAME).exe.manifest \
                        $(LOCAL_ICON_FILE) $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)$(WINDRES) --use-temp-file -O COFF -i '$<' -o '$@'

$(EXECUTABLE_NAME).rc: $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)echo  >'$@' '#define APP_MANIFEST 1'
	$(Q)echo >>'$@' '#define RT_MANIFEST 24'
	$(Q)echo >>'$@' 'APP_MANIFEST RT_MANIFEST $(EXECUTABLE_NAME).exe.manifest'
	$(Q)$(if $(ICON_FILE),echo >>'$@' 'DefaultIcon ICON $(LOCAL_ICON_FILE)')

$(EXECUTABLE_NAME).exe.manifest: $(MANIFEST_TEMPLATE) $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)sed \
	        -e 's/@PACKAGE_NAME@/$(PACKAGE_NAME)/g' \
	        -e 's/@PROGRAM_VERSION_CODE@/$(PROGRAM_VERSION_CODE)/g' \
	        -e 's/@TARGET_ARCH_ABI@/$(TARGET_ARCH_ABI)/g' \
	        <'$<' >'$@'

ifneq ($(ICON_FILE),)
$(LOCAL_ICON_FILE): $(ICON_FILE) $(MAKEFILE_DEPS)
	$(ECHO) 'Copying $@ from $<'
	$(Q)cp -p '$<' '$@'
endif

endif  # ifneq ($(EXECUTABLE_NAME),)

###########################################################################
###########################################################################
