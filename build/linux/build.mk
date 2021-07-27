#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/linux/build.mk: Build control file for Linux.
#

#
# Linux-specific notes:
#
# - Since Linux runs on a wide variety of architectures, the Linux build
#   rules do not support explicit target architecture selection using
#   $(TARGET_ARCH_ABI) as on other platforms; builds will always be for
#   the native architecture, as modified by the compiler and linker flags.
#   To simplify architecture selection, $(TARGET_FLAGS) will be added to
#   all compiler and command line invocations.  For the common case of
#   building for 32-bit x86 on a 64-bit x86-64 platform with GCC or Clang,
#   set TARGET_FLAGS=-m32 on the "make" command line.
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
# include SIL/build/linux/build.mk
#
# .PHONY: my-date.h  # Force regeneration on every build.
# my-date.h:
# 	date +'#define DATE "%Y-%m-%d %H:%M:%S"' >$@
#

###########################################################################
################ Defaults for common configuration options ################
###########################################################################

# Look for Valgrind installed in /usr, and use it if found.
_SIL_HAVE_VALGRIND := $(if $(wildcard /usr/include/valgrind/memcheck.h),1,0)
SIL_DEBUG_USE_VALGRIND ?= $(_SIL_HAVE_VALGRIND)

###########################################################################
################## Linux-specific configuration options ###################
###########################################################################

#------------------------- Build control options -------------------------#

# $(TARGET_FLAGS):  Flags to add to all compiler and linker invocations.
# Intended for choosing a target architeture, e.g. "-m32" on x86-64.

TARGET_FLAGS ?=

#----------------------- Program behavior options ------------------------#

# $(USE_FFMPEG):  DEPRECATED.  Setting this to 1 enables use of the FFmpeg
# libraries (libavcodec, libavformat, libavresample, and libavutil) for
# decoding movie data.  Note that WebM streams will always be decoded by
# the libvpx-based WebM decoder if enabled by SIL_MOVIE_INCLUDE_WEBM (as is
# the default).
#
# FFmpeg support is deprecated due to the difficulty of keeping up with the
# constantly changing API, and will be removed in SIL 0.6 or later.

USE_FFMPEG ?= 0
ifeq (1,$(USE_FFMPEG))
    $(warning *** Warning: FFmpeg support is deprecated; use WebM format instead.)
endif

###########################################################################
######################### Build environment setup #########################
###########################################################################

_THISDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(_THISDIR)/../common/base.mk
include $(_THISDIR)/../common/config.mk

CC ?= gcc

OBJECT_EXT  = .o
LIBRARY_EXT = .a

SYS_CFLAGS    = $(TARGET_FLAGS)
SYS_CXXFLAGS  = $(TARGET_FLAGS)
SYS_OBJCFLAGS = $(TARGET_FLAGS)
SYS_LDFLAGS   = $(TARGET_FLAGS)

BASE_LIBS = $(if $(filter 1,$(USE_FFMPEG)),-lavcodec -lavformat -lavresample -lavutil) \
            -lGL -lX11 -lasound -ldl -lm -lpthread -lrt

include $(_THISDIR)/../common/toolchain.mk

CC_LD = $(if $(filter 1,$(SIL_LINK_CXX_STL)),$(CXX),$(CC))

###########################################################################
################# Source file lists and associated flags ##################
###########################################################################

SIL_SYS_CONFIG_HEADER = linux.h

SIL_SYS_SOURCES = $(SIL_OPENGL_SOURCES) \
                  sysdep/linux/debug.c \
                  sysdep/linux/graphics.c \
                  sysdep/linux/input.c \
                  sysdep/linux/main.c \
                  sysdep/linux/meminfo.c \
                  sysdep/linux/misc.c \
                  sysdep/linux/movie.c \
                  sysdep/linux/sound.c \
                  sysdep/linux/sysfont.c \
                  sysdep/linux/thread.c \
                  sysdep/linux/userdata.c \
                  sysdep/misc/ioqueue.c \
                  sysdep/misc/joystick-db.c \
                  sysdep/misc/log-stdio.c \
                  sysdep/posix/condvar.c \
                  sysdep/posix/files.c \
                  sysdep/posix/fileutil.c \
                  sysdep/posix/misc.c \
                  sysdep/posix/mutex.c \
                  sysdep/posix/semaphore.c \
                  sysdep/posix/thread.c \
                  sysdep/posix/time.c \
                  sysdep/posix/userdata.c \
                  sysdep/posix/util.c \
                  $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                      test/sysdep/linux/graphics/fs-early.c \
                      test/sysdep/linux/graphics/fs-methods.c \
                      test/sysdep/linux/graphics/fs-minimize.c \
                      test/sysdep/linux/graphics/fs-mode.c \
                      test/sysdep/linux/graphics/internal.c \
                      test/sysdep/linux/graphics/modes.c \
                      test/sysdep/linux/graphics/window.c \
                      test/sysdep/linux/graphics/x11-base.c \
                      test/sysdep/linux/graphics/x11-events.c \
                      test/sysdep/linux/graphics/xinerama.c \
                      test/sysdep/linux/input.c \
                      test/sysdep/linux/main.c \
                      test/sysdep/linux/meminfo.c \
                      test/sysdep/linux/misc.c \
                      test/sysdep/linux/posix-fileutil.c \
                      test/sysdep/linux/sound.c \
                      test/sysdep/linux/sysfont.c \
                      test/sysdep/linux/userdata.c \
                      test/sysdep/linux/wrap-io.c \
                      test/sysdep/linux/wrap-x11.c \
                      test/sysdep/misc/ioqueue.c \
                      test/sysdep/misc/joystick-db.c \
                      test/sysdep/misc/log-stdio.c \
                      test/sysdep/posix/files.c \
                      test/sysdep/posix/fileutil.c \
                      test/sysdep/posix/internal.c \
                      test/sysdep/posix/misc.c \
                      test/sysdep/posix/thread.c \
                      test/sysdep/posix/time.c \
                      test/sysdep/posix/userdata.c \
                  )

# System-specific internal SIL flags.  We set 32-byte alignment for texture
# buffers to improve copy performance.
SIL_SYS_FLAGS = $(CFLAG_DEFINE)SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT=32 \
                $(call _if-define,USE_FFMPEG,SIL_PLATFORM_LINUX_USE_FFMPEG)

# Declare that we follow POSIX.
SIL_SYS_FLAGS += $(CFLAG_DEFINE)_POSIX_C_SOURCE=200809L

include $(_THISDIR)/../common/sources.mk

###########################################################################
############################### Build rules ###############################
###########################################################################

ifneq ($(EXECUTABLE_NAME),)
SIL_HELP_all           = 'build the executable ($(EXECUTABLE_NAME))'
endif
SIL_HELP_gen-coverage  = 'generate coverage.out from coverage data'
SIL_HELP_coverage-html = 'generate HTML coverage results from coverage.out'
SIL_HELP_clean         = 'remove all intermediate files and dependency data'
SIL_HELP_spotless      = 'remove all generated files, including the executable'

include $(_THISDIR)/../common/rules.mk

#-------------------------------------------------------------------------#

.PHONY: all clean spotless

ifneq ($(EXECUTABLE_NAME),)
all: $(EXECUTABLE_NAME) $(RESOURCES)
endif

clean:
	$(ECHO) 'Removing object files'
	$(Q)rm -rf obj
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing temporary files'
	$(Q)rm -rf .covtmp
endif
	$(SIL_RECIPE_clean)

spotless: clean
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing executable file'
	$(Q)rm -rf '$(EXECUTABLE_NAME)'
endif
	$(ECHO) 'Removing coverage data'
	$(Q)rm -rf coverage coverage.out
	$(_SIL_TOOLS_RECIPE_spotless)
	$(SIL_RECIPE_spotless)

#-------------------------------------------------------------------------#

ifneq ($(EXECUTABLE_NAME),)

# Sort objects in lexical order to make it easier to estimate build progress.
# The input pipe and --dynamic-list linker option are to allow dlsym() of the
# XF86VidMode and XRandR wrappers defined in src/test/sysdep/linux/wrap-x11.c.
$(EXECUTABLE_NAME): $(sort $(ALL_OBJECTS))
	$(ECHO) 'Linking $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(if $(filter 1,$(SIL_INCLUDE_TESTS)),echo '{X*;};' |) \
	    $(CC_LD) \
	    $(BASE_LDFLAGS) $(ALL_LDFLAGS) $(LDFLAGS) \
	    $(if $(filter 1,$(SIL_INCLUDE_TESTS)),-Wl$(,)--dynamic-list=/dev/stdin) \
	    $(ALL_OBJECTS) \
	    $(ALL_LIBS) $(BASE_LIBS) $(LIBS) \
	    $(CFLAG_LINK_OUTPUT)'$@'

endif

###########################################################################
###########################################################################
