#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/macosx/build.mk: Build control file for Mac OS X.
#

#
# OSX-specific notes:
#
# - The final application package (directory) is named
#   "$(EXECUTABLE_NAME).app".
#
# - $(PROGRAM_VERSION_CODE) is used as the CFBundleVersion key value in
#   the Info.plist file, and it must match /[0-9]+(\.[0-9]+)*/.
#
# - $(SDK_VERSION) and $(TARGET_OS_VERSION), if set, should refer to an
#   OS X version number in "major.minor[.patch]" form; for example:
#      SDK_VERSION = 10.7
#
# - To add resources to the package, create a target for each resource file
#   or directory named "$(BUNDLE_DIR_ESCAPED)/Contents/Resources/<path>"
#   and make the "resources" target depend on that path.  For example:
#      resources: $(BUNDLE_DIR_ESCAPED)/Contents/Resources/data
#      $(BUNDLE_DIR_ESCAPED)/Contents/Resources/data: \
#         $(TOPDIR)/data $(MAKEFILE_DEPS)
#      <tab>rm -rf '$@'
#      <tab>cp -a '$<' '$@'
#
# Note that when debugging with GDB, it's necessary to have a set of
# object files corresponding to the executable.  Typically, the easiest
# way to do this is to build a new executable and immediately run GDB on
# that executable, since it will have correct paths to the object files.
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
# include SIL/build/macosx/build.mk
#
# .PHONY: my-date.h  # Force regeneration on every build.
# my-date.h:
# 	date +'#define DATE "%Y-%m-%d %H:%M:%S"' >$@
#
#

###########################################################################
################ Defaults for common configuration options ################
###########################################################################

# Look for Valgrind installed in /usr, and use it if found.
_SIL_HAVE_VALGRIND := $(if $(wildcard /usr/include/valgrind/memcheck.h),1,0)
SIL_DEBUG_USE_VALGRIND ?= $(_SIL_HAVE_VALGRIND)

###########################################################################
################### OSX-specific configuration options ####################
###########################################################################

#---------------------- Build environment pathnames ----------------------#

# $(DEV_ROOT):  Root path of the OSX development tools.

DEV_ROOT ?= $(XCODE_ROOT)/Platforms/MacOSX.platform/Developer


# $(TOOLCHAIN_ROOT):  Root path of the compiler toolchain.

TOOLCHAIN_ROOT ?= $(or $(shell ls -d '$(XCODE_ROOT)/Toolchains/XcodeDefault.xctoolchain' 2>/dev/null),$(DEV_ROOT))


# $(XCODE_ROOT):  Installation path for Xcode developer files, normally
# "Contents/Developer" inside the Xcode.app package.  This is only used to
# set the default values for $(DEV_ROOT) and $(TOOLCHAIN_ROOT), and it is
# not referenced (except in help text) otherwise.  If the xcode-select tool
# is available, the path reported by xcode-select -p is used as the default
# value.

XCODE_ROOT ?= $(or $(shell xcode-select -p 2>/dev/null),/Applications/Xcode.app/Contents/Developer)

#------------------------- Build control options -------------------------#

# $(STL_LIBRARY):  Which STL library to use for C++ code, either libc++ or
# libstdc++.  This option has no effect for non-C++ code or C++ code which
# does not use the STL.  The default is libc++.

STL_LIBRARY ?= libc++

#--------------------------- Packaging options ---------------------------#

# $(ICON_FILE):  Icon file (*.icns) to associate with the application.
# If not set, no icon will be included.

ICON_FILE ?=


# $(INFO_PLIST_SOURCE):  Info.plist file into which build-specific values
# will be inserted.  If not set, a standard Info.plist will be used.

INFO_PLIST_SOURCE ?= $(_THISDIR)/Info.plist


# $(PREFIX):  Installation directory for the "install" and "install-binary"
# targets, which use $(BUNDLE_DIR) under this directory.

PREFIX ?= $(HOME)/Applications


# $(STRINGS_FILES):  List of *.strings (localization) files to include in
# the package.  The files from build/macosx/*.lproj are always included,
# but may be overridden by files here.  Each entry in the list should be
# the full pathname of the file; the last two components of the pathname
# must match "*.lproj/*.strings".

STRINGS_FILES ?=

###########################################################################
##################### Common pathnames and filenames ######################
###########################################################################

# SDK root path and canonical version.
SDK_PREFIX = $(DEV_ROOT)/SDKs/MacOSX
ifeq ($(SDK_VERSION),)
override SDK_VERSION := $(shell ls -d $(SDK_PREFIX)* 2>/dev/null \
                                | sed -e 's,.*/,,; s/MacOSX//; s/\.sdk//' \
                                | sort -n \
                                | tail -n1)
endif
SDK_ROOT := $(SDK_PREFIX)$(SDK_VERSION).sdk

# If we're going to compile anything, make sure $(DEV_ROOT) exists.
# Again, we use $(shell ls) instead of $(wildcard) to avoid problems if
# there are spaces in the pathname.

ifneq ($(filter-out help clean spotless,$(or $(MAKECMDGOALS),default)),)

ifeq ($(shell ls -d '$(DEV_ROOT)' 2>/dev/null),)
$(warning --- Failed to find Xcode OSX tools at $$(DEV_ROOT):)
$(warning ---     $(DEV_ROOT))
$(warning --- Rerun make with "XCODE_ROOT=/path/to/Xcode.app/Contents/Developer")
$(warning --- or "DEV_ROOT=/path/to/MacOSX.platform/Developer".)
$(error Xcode not found)
endif

ifeq ($(shell ls -d '$(TOOLCHAIN_ROOT)/usr/bin' 2>/dev/null),)
$(warning --- Failed to find Xcode toolchain at $$(TOOLCHAIN_ROOT):)
$(warning ---     $(TOOLCHAIN_ROOT))
$(warning --- Rerun make with "TOOLCHAIN_ROOT=/path/to/toolchain".)
$(error Xcode toolchain not found)
endif

ifeq ($(SDK_VERSION),)
$(warning --- Failed to find any Mac OS X SDK under $$(DEV_ROOT):)
$(warning ---     $(DEV_ROOT))
$(warning --- Rerun make with "SDK_ROOT=/path/to/MacOSX.sdk".)
$(error Mac OS X SDK not found)
endif

ifeq ($(shell ls -d '$(SDK_ROOT)' 2>/dev/null),)
$(warning --- Failed to find Mac OS X SDK at $$(SDK_ROOT):)
$(warning ---     $(SDK_ROOT))
$(warning --- Rerun make with "SDK_ROOT=/path/to/MacOSX.sdk".)
$(error Mac OS X SDK not found)
endif

ifneq ($(or $(filter 1 2 3 4 5 6 7 8 9,$(firstword $(subst ., ,$(SDK_VERSION)))),$(filter 0 1 2 3 4 5 6,$(word 2,$(subst ., ,$(SDK_VERSION))))),)
$(warning --- Mac OS X SDK (version $(SDK_VERSION)) is too old!)
$(warning --- Version 10.7 or later of the SDK is required.)
$(error Mac OS X SDK too old)
endif

endif

# Target OS version (if not already defined).
TARGET_OS_VERSION ?= 10.7.3

#-------------------------------------------------------------------------#

# Output bundle (.app) directory name.
BUNDLE_DIR = $(EXECUTABLE_NAME).app

# The same thing, with spaces escaped for use in rules.
BUNDLE_DIR_ESCAPED = $(shell echo '$(BUNDLE_DIR)' | sed -e 's/ /\\ /g')

###########################################################################
######################### Build environment setup #########################
###########################################################################

_THISDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(_THISDIR)/../common/base.mk
include $(_THISDIR)/../common/config.mk


# Evaluate $(XCODE_ROOT) so we don't go shelling out on every reference.
XCODE_ROOT := $(XCODE_ROOT)

CC = $(TOOLCHAIN_ROOT)/usr/bin/clang

# Default flags used by Xcode (excluding those we already set in toolchain.mk).
PLATFORM_FLAGS = -arch x86_64 \
                 -gdwarf-2 \
                 -isysroot $(SDK_ROOT)
PLATFORM_CFLAGS = $(PLATFORM_FLAGS) \
                  -ffast-math \
                  -fmessage-length=0 \
                  -fvisibility=hidden \
                  -mmacosx-version-min=$(TARGET_OS_VERSION)
PLATFORM_CXXFLAGS = $(PLATFORM_CFLAGS) \
                    -stdlib=$(STL_LIBRARY) \
                    -fvisibility-inlines-hidden
PLATFORM_LDFLAGS = $(PLATFORM_FLAGS) \
                   -stdlib=$(STL_LIBRARY)

AR_RC  = $(TOOLCHAIN_ROOT)/usr/bin/ar rc
RANLIB = $(TOOLCHAIN_ROOT)/usr/bin/ranlib

OBJECT_EXT  = .o
LIBRARY_EXT = .a

SYS_CFLAGS    = $(PLATFORM_CFLAGS)
SYS_CXXFLAGS  = $(PLATFORM_CXXFLAGS)
SYS_OBJCFLAGS = $(PLATFORM_CFLAGS)
SYS_ASFLAGS   = $(PLATFORM_FLAGS)
SYS_LDFLAGS   = $(PLATFORM_LDFLAGS)

include $(_THISDIR)/../common/toolchain.mk

CC_LD = $(if $(filter 1,$(SIL_LINK_CXX_STL)),$(CXX),$(CC))


# Other definitions used only in this file.

SYSTEM_FRAMEWORKS = AudioUnit Cocoa CoreAudio ForceFeedback IOKit OpenGL \
                    QuartzCore \
                    $(if $(filter 10.7%,$(TARGET_OS_VERSION)),ApplicationServices,CoreGraphics)
LOCAL_FRAMEWORKS =
ALL_FRAMEWORKS = $(SYSTEM_FRAMEWORKS) $(LOCAL_FRAMEWORKS)

###########################################################################
################# Source file lists and associated flags ##################
###########################################################################

SIL_SYS_CONFIG_HEADER = macosx.h

SIL_SYS_SOURCES = $(SIL_OPENGL_SOURCES) \
                  sysdep/darwin/debug.c \
                  sysdep/darwin/meminfo.c \
                  sysdep/darwin/semaphore.c \
                  sysdep/darwin/time.c \
                  sysdep/macosx/dialog.m \
                  sysdep/macosx/graphics.m \
                  sysdep/macosx/input.m \
                  sysdep/macosx/main.m \
                  sysdep/macosx/misc.c \
                  sysdep/macosx/sound.c \
                  sysdep/macosx/thread.m \
                  sysdep/macosx/userdata.c \
                  sysdep/macosx/util.m \
                  sysdep/macosx/view.m \
                  sysdep/macosx/window.m \
                  sysdep/misc/ioqueue.c \
                  sysdep/misc/joystick-db.c \
                  sysdep/misc/joystick-hid.c \
                  sysdep/misc/log-stdio.c \
                  sysdep/misc/movie-none.c \
                  sysdep/misc/sysfont-none.c \
                  sysdep/posix/condvar.c \
                  sysdep/posix/files.c \
                  sysdep/posix/fileutil.c \
                  sysdep/posix/misc.c \
                  sysdep/posix/mutex.c \
                  sysdep/posix/thread.c \
                  sysdep/posix/userdata.c \
                  sysdep/posix/util.c \
                  $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                      test/sysdep/darwin/time.c \
                      test/sysdep/macosx/graphics.m \
                      test/sysdep/macosx/input.m \
                      test/sysdep/macosx/util.c \
                      test/sysdep/misc/ioqueue.c \
                      test/sysdep/misc/joystick-db.c \
                      test/sysdep/misc/joystick-hid.c \
                      test/sysdep/misc/log-stdio.c \
                      test/sysdep/posix/files.c \
                      test/sysdep/posix/fileutil.c \
                      test/sysdep/posix/internal.c \
                      test/sysdep/posix/misc.c \
                      test/sysdep/posix/thread.c \
                      test/sysdep/posix/userdata.c \
                  )

SIL_SYS_FLAGS = $(CFLAG_DEFINE)_POSIX_C_SOURCE=200809L \
                $(CFLAG_DEFINE)_DARWIN_C_SOURCE

include $(_THISDIR)/../common/sources.mk

###########################################################################
############################### Build rules ###############################
###########################################################################

ifneq ($(EXECUTABLE_NAME),)
SIL_HELP_all            = 'build application bundle ($(BUNDLE_DIR))'
SIL_HELP_binary         = 'build the executable ($(EXECUTABLE_NAME)) only'
SIL_HELP_install        = 'build application bundle and install to ~/Applications'
SIL_HELP_install-binary = 'build executable & install to ~/Applications/$(BUNDLE_DIR)'
endif
SIL_HELP_gen-coverage   = 'generate coverage.out from coverage data'
SIL_HELP_coverage-html  = 'generate HTML coverage results from coverage.out'
SIL_HELP_clean          = 'remove all intermediate files and dependency data'
SIL_HELP_spotless       = 'remove all generated files, including executable/bundle'
define SIL_HELPTRAILER
echo ''
echo 'Using Xcode installation path (XCODE_ROOT):  $(XCODE_ROOT)'
echo 'Building with SDK version (SDK_VERSION):     $(or $(SDK_VERSION),<not found>)'
echo 'Building for OS version (TARGET_OS_VERSION): $(or $(TARGET_OS_VERSION),<unknown>)'
echo ''
endef

include $(_THISDIR)/../common/rules.mk

#-------------------------------------------------------------------------#

.PHONY: all install binary install-binary clean spotless

ifneq ($(EXECUTABLE_NAME),)

all: binary-in-bundle resources

binary: $(EXECUTABLE_NAME)

install: all
	$(ECHO) 'Installing $(BUNDLE_DIR) to $(PREFIX)'
	$(Q)mkdir -p '$(PREFIX)/$(BUNDLE_DIR)'
	@# If there are any symlinks to directories, cp -a will complain
	@# that it "cannot overwrite directory with non-directory" even
	@# though both are symlinks, so we use a tar pipe instead.
	@#$(Q)cp -a '$(BUNDLE_DIR)'/* '$(PREFIX)/$(BUNDLE_DIR)/'
	$(Q)tar cf - '$(BUNDLE_DIR)' | tar Cxfp '$(PREFIX)' -

install-binary: binary
	$(ECHO) 'Installing $(EXECUTABLE_NAME) to $(PREFIX)/$(BUNDLE_DIR)/Contents/MacOS'
	$(Q)mkdir -p '$(PREFIX)/$(BUNDLE_DIR)/Contents/MacOS'
	$(Q)cp -p '$(EXECUTABLE_NAME)' '$(PREFIX)/$(BUNDLE_DIR)/Contents/MacOS/'

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
	$(ECHO) 'Removing application files'
	$(Q)rm -rf '$(EXECUTABLE_NAME)' '$(BUNDLE_DIR)'
endif
	$(ECHO) 'Removing coverage data'
	$(Q)rm -rf coverage coverage.out
	$(_SIL_TOOLS_RECIPE_spotless)
	$(SIL_RECIPE_spotless)

#-------------------------------------------------------------------------#

ifneq ($(EXECUTABLE_NAME),)  # Through the end of the file.

.PHONY: binary-in-bundle
binary-in-bundle: $(BUNDLE_DIR_ESCAPED)/Contents/MacOS/$(EXECUTABLE_NAME)

# Sort objects in lexical order to make it easier to estimate build progress.
$(BUNDLE_DIR_ESCAPED)/Contents/MacOS/$(EXECUTABLE_NAME) \
$(EXECUTABLE_NAME): $(sort $(ALL_OBJECTS))
	$(ECHO) 'Linking $@'
	@# Note that we can't use $(@D) as in the common rules, because that
	@# breaks if $@ contains spaces.  So we have to do things the hard way.
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents/MacOS'
	$(Q)MACOSX_DEPLOYMENT_TARGET='$(TARGET_OS_VERSION)' $(CC_LD) \
	    $(BASE_LDFLAGS) $(ALL_LDFLAGS) $(LDFLAGS) \
	    $(ALL_OBJECTS) \
	    $(foreach i,$(ALL_FRAMEWORKS),-framework $i) \
            $(BASE_LIBS) $(ALL_LIBS) $(LIBS) \
	    $(CFLAG_LINK_OUTPUT)'$@'

#-------------------------------------------------------------------------#

ICON_FILE_BASENAME := $(notdir $(ICON_FILE))
_SIL_STRINGS_FILES := $(shell ls '$(_THISDIR)'/*.lproj/*.strings 2>/dev/null)
_SIL_strings-name = $(lastword $(subst /, ,$(dir $1)))/$(notdir $1)
$(foreach i,$(_SIL_STRINGS_FILES) $(STRINGS_FILES),$(eval STRINGS_$(call _SIL_strings-name,$i) := $i))
STRINGS_LIST := $(foreach i,$(_SIL_STRINGS_FILES) $(STRINGS_FILES),$(call _SIL_strings-name,$i))

.PHONY: resources
resources: $(RESOURCES:%=$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%) \
           $(BUNDLE_DIR_ESCAPED)/Contents/Info.plist \
           $(BUNDLE_DIR_ESCAPED)/Contents/PkgInfo \
           $(if $(ICON_FILE),$(ICON_FILE_BASENAME:%=$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%)) \
           $(STRINGS_LIST:%=$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%) \
           $(if $(filter -DSIL_INCLUDE_TESTS,$(SIL_CFLAGS)),$(BUNDLE_DIR_ESCAPED)/Contents/Resources/testdata)

$(RESOURCES:%=$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%) : $(BUNDLE_DIR_ESCAPED)/Contents/Resources/%: %
	$(ECHO) 'Copying $< into package'
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents/Resources'
	$(Q)cp -a '$<' '$@'
	$(Q)touch '$@'

ifneq (,$(ICON_FILE))
$(BUNDLE_DIR_ESCAPED)/Contents/Resources/$(ICON_FILE_BASENAME): $(ICON_FILE)
	$(ECHO) 'Copying $< into package'
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents/Resources'
	$(Q)cp -a '$<' '$@'
	$(Q)touch '$@'
endif

$(BUNDLE_DIR_ESCAPED)/Contents/Info.plist: $(INFO_PLIST_SOURCE) $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents'
	$(Q)perl \
	    -e 'undef $$/; $$_ = <STDIN>;' \
	    -e 's/\$$\{EXECUTABLE_NAME\}/$(EXECUTABLE_NAME)/g;' \
	    -e 's/\$$\{ICON_FILE\}/$(ICON_FILE_BASENAME)/g;' \
	    -e 's/\$$\{PRODUCT_NAME\}/$(PROGRAM_NAME)/g;' \
	    -e 's/\$$\{MACOSX_DEPLOYMENT_TARGET\}/$(TARGET_OS_VERSION)/g;' \
	    -e 's/(CFBundleIdentifier.*\n.*)<string>[^<]*/$$1<string>$(PACKAGE_NAME)/g;' \
	    -e 's/(CFBundleShortVersionString.*\n.*)<string>[^<]*/$$1<string>$(PROGRAM_VERSION)/g;' \
	    -e 's/(CFBundleVersion.*\n.*)<string>[^<]*/$$1<string>$(PROGRAM_VERSION_CODE)/g;' \
	    -e '"$(ICON_FILE_BASENAME)" ne "" or s/.*<key>CFBundleIconFile<\/key>.*\n.*\n//;' \
	    -e 'print;' \
	    <'$<' >'$@' \
	    || rm -f '$@'

$(BUNDLE_DIR_ESCAPED)/Contents/PkgInfo: $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents'
	$(Q)echo -n 'APPL????' >'$@'

$(BUNDLE_DIR_ESCAPED)/Contents/Resources/testdata: $(SIL_DIR)/testdata
	$(ECHO) 'Copying $<'
	$(Q)mkdir -p '$@'
	$(Q)cp -a '$<'/* '$@/'

.SECONDEXPANSION:

$(STRINGS_LIST:%=$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%) : \
$(BUNDLE_DIR_ESCAPED)/Contents/Resources/%: $$(STRINGS_$$(call _SIL_strings-name,$$(subst $$(preserve-space) ,_,$$@))) $$(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@ from $<'
	$(Q)mkdir -p '$(BUNDLE_DIR)/Contents/Resources/$(lastword $(subst /, ,$(@D)))'
	$(Q)$(_THISDIR)/copystrings.sh \
	    --validate \
	    --inputencoding utf-8 \
	    --outputencoding binary \
	    --outdir '$(BUNDLE_DIR)/Contents/Resources/$(lastword $(subst /, ,$(@D)))' \
	    '$<'


endif  # ifneq ($(EXECUTABLE_NAME),)

###########################################################################
###########################################################################
