#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/ios/build.mk: Build control file for iOS.
#

#
# iOS-specific notes:
#
# - The final package file is named "$(EXECUTABLE_NAME).ipa", and the
#   unpacked program data is stored in the directory "$(EXECUTABLE_NAME).app".
#
# - Resource files or directories can be included in the application
#   package by adding their names to $(RESOURCES).  Each file or directory
#   will be staged in the build directory and then copied into the package
#   directory.  Rules for building resources should use the resource name
#   in the build directory (i.e., with no path prefix) as the rule target.
#
# - $(PROGRAM_VERSION_CODE) is used as the CFBundleVersion key value in
#   the Info.plist file, and it must match /[0-9]+(\.[0-9]+)*/.
#
# - $(SDK_VERSION) and $(TARGET_OS_VERSION), if set, should refer to an
#   iOS version number in "major.minor" form; for example:
#      SDK_VERSION = 6.0
#
# - $(TARGET_ARCH_ABI) can be any of:
#      all arm64 armv7
#   The special value "all" (which is the default) will build for all
#   architectures.
#
# - When collecting coverage data, it is necessary to copy the raw coverage
#   data files from the device before running "make gen-coverage".  The
#   data is located in "Library/Application Support/coverage" in the app
#   installation directory, under the same path structure as the absolute
#   path on the build system; for example, if the program's build directory
#   is "/Users/username/MyApp/build", the coverage data would be found in
#   "Library/Application Support/coverage/Users/username/MyApp/build/obj".
#   Copy all files from this "obj" directory on the device to the "obj"
#   directory on the build system, preserving relative pathnames.
#
#   Note that coverage data will only be output in debug mode.
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
# include SIL/build/ios/build.mk
#
# .PHONY: my-date.h  # Force regeneration on every build.
# my-date.h:
# 	date +'#define DATE "%Y-%m-%d %H:%M:%S"' >$@
#
#

###########################################################################
################### iOS-specific configuration options ####################
###########################################################################

#------------------------- Build control options -------------------------#

# $(EMBED_BITCODE):  Set this to 1 to embed LLVM bitcode within the
# compiled executable.  Apple claims to make use of embedded bitcode to
# dynamically recompile the executable for different target architectures.
#
# Enabling this option is not recommended because Apple's use of it for
# recompiling can introduce bugs (due to compiler flaws) which cannot be
# detected by the developer.  However, Apple currently requires bitcode
# for apps targeting the Apple Watch and Apple TV devices, so set this
# option to 1 if submitting such an app to Apple.
#
# Note that embedded bitcode is not supported in iOS versions prior to 6.0;
# attempting to build with this option enabled and $(TARGET_OS_VERSION) set
# to 5.1.1 will fail.

EMBED_BITCODE = 0

#---------------------- Build environment pathnames ----------------------#

# $(DEV_ROOT):  Root path of the iOS development tools.

DEV_ROOT ?= $(XCODE_ROOT)/Platforms/iPhoneOS.platform/Developer


# $(TOOLCHAIN_ROOT):  Root path of the compiler toolchain.
#
# Implementation note: the use of $(shell ls) instead of $(wildcard) is
# deliberate, since $(wildcard) breaks if there are spaces in the pathname.

TOOLCHAIN_ROOT ?= $(or $(shell ls -d '$(XCODE_ROOT)/Toolchains/XcodeDefault.xctoolchain' 2>/dev/null),$(DEV_ROOT))


# $(XCODE_ROOT):  Installation path for Xcode developer files, normally
# "Contents/Developer" inside the Xcode.app package.  This is only used to
# set the default values for $(DEV_ROOT) and $(TOOLCHAIN_ROOT), and it is
# not referenced (except in help text) otherwise.  If the xcode-select tool
# is available, the path reported by xcode-select -p is used as the default
# value.

XCODE_ROOT ?= $(or $(shell xcode-select -p 2>/dev/null),/Applications/Xcode.app/Contents/Developer)

#----------------------- Program behavior options ------------------------#

# $(RUN_TESTS):  Set this to 1 to run the test routines instead of the
# main program.

RUN_TESTS ?= 0


# $(USE_FILE_SHARING):  Set this to 1 to enable exporting of save files
# (and their associated screenshots) to the Documents directory, from which
# they can be copied off the device via iTunes file sharing.  Save files
# copied onto the device via iTunes will likewise be imported, overwriting
# any existing save in the game's internal storage.
#
# If GameKit is enabled (see below), the set of exported save files always
# reflects the currently signed-in Game Center player (or the most recently
# signed-in player, if no player is currently signed in).  When a new player
# is detected, the exported save files are replaced with those of the new
# player.  (As a corollary, if you want to copy save files to a new player,
# you have to start the game once with the new player signed in before you
# can copy the saves to the device -- otherwise your copied saves will be
# overwritten when the game detects the new player.)
#
# IMPORTANT:  This functionality should be considered _experimental_!
# The details of the file sharing implementation appear to be undefined
# by Apple documentation; SIL attempts to ensure that internal and
# external data copies are kept in sync without excessive load/save
# overhead, but there may still be a potential for data loss in either
# direction.  Use with care!

USE_FILE_SHARING ?= 0


# $(USE_GAMEKIT):  Set this to 1 to enable Game Center functionality via
# the GameKit framework.  If enabled, achievements will be sent to Game
# Center as well as stored locally; see sysdep/ios/userdata.c for details.
#
# This is generally useless unless you have an Apple developer account and
# associated app record with Game Center enabled.

USE_GAMEKIT ?= 0

#--------------------------- Packaging options ---------------------------#

# $(DEVICE_IPHONE), $(DEVICE_IPAD), $(DEVICE_TV):  Set these to 1 to enable
# the program to run on the respective device:
#     DEVICE_IPHONE: iPhone and iPod touch (all generations supporting iOS
#         5.1.1 or later)
#     DEVICE_IPAD: iPad (all generations)
#     DEVICE_TV: Apple TV (4th generation and later)
#
# The default is to support all devices except Apple TV.  Note that
# supporting Apple TV requires $(TARGET_OS_VERSION) to be set to 9.0 or
# later.

DEVICE_IPHONE ?= 1
DEVICE_IPAD ?= 1
DEVICE_TV ?= 0


# $(ICON_IMAGES):  Source image files for icons of various types and sizes.
#
# The SIL build system will automatically determine the appropriate icon
# type for each image based on its size; see the iOS SDK documentation for
# details of icon sizes (SIL is aware of all icon sizes supported through
# iOS SDK 12.1).  Note that icon sizes specific to iOS 5.x and 6.x, namely
# 57x57 for iPhone and 72x72 for iPad (along with their 2x resolution
# variants), are _not_ supported and must be packaged manually; see
# README-ios.txt for details.
#
# Since the 40x40 size is shared between both the notification icon (at 2x
# resolution) and the Spotlight search result icon (at 1x resolution), SIL
# will not generate Spotlight icon metadata unless an 80x80 icon is also
# present.  Similarly:
#    - 60x60 will only be treated as an iPhone icon at 1x resolution unless
#      a 20x20 or 40x40 icon is present.
#    - 120x120 will only be treated as an iPhone icon at 2x resolution
#      unless an 80x80 icon is also present.
#
# The App Store icon image (1024x1024) should also be included in the
# image list.

ICON_IMAGES ?=


# $(ICON_ITUNES):  Source image file for the 512x512 icon displayed in
# iTunes 12.6 and earlier.  If empty, no iTunes icon is embedded in the
# package.

ICON_ITUNES ?=


# $(INFO_PLIST_SOURCE):  Info.plist file into which build-specific values
# will be inserted.  If not set, a standard Info.plist will be used.

INFO_PLIST_SOURCE ?= $(_THISDIR)/Info.plist


# $(LAUNCH_IMAGES):  Source image files for launch screens at various
# resolutions.
#
# The SIL build system will automatically determine the appropriate launch
# image category (device type and orientation) for each image based on its
# size; see the iOS SDK documentation for details of icon sizes (SIL is
# aware of all launch image sizes supported through iOS SDK 12.1).

LAUNCH_IMAGES ?=


# $(PROFILE_NAME), $(PROFILE_FILE):  Use these to select the provisioning
# profile to build with.  $(PROFILE_NAME) specifies a name substring
# matched against the names of all existing profiles with valid
# certificates; $(PROFILE_FILE) can be used to specify a particular
# profile file (*.mobileprovision), and takes precedence if defined.
# If both are left empty, $(PROFILE_NAME) will default to "Team
# Provisioning Profile" for debug builds (when $(DEBUG) is 1), and to
# "Distribution" for non-debug builds.

PROFILE_NAME ?=
PROFILE_FILE ?=


# $(STRINGS_FILES):  List of *.strings (localization) files to include in
# the package.  The files from build/ios/*.lproj are always included, but
# may be overridden by files here.  Each entry in the list should be the
# full pathname of the file; the last two components of the pathname must
# match "*.lproj/*.strings".

STRINGS_FILES ?=


# $(VALIDATE):  If set to 1, the Xcode package validator will be run on
# the generated bundle after it is signed.  Note that validation is not
# supported under Xcode 7.0 and later.

VALIDATE ?= 0


# $(WHITE_POINT_STYLE):  Sets the value for the UIWhitePointAdaptivityStyle
# key in the app's Info.plist (for iOS 9.3+).  Should be one of:
#    Standard, Reading, Photo, Video, Game
# If left blank (the default), the UIWhitePointAdaptivityStyle key will be
# omitted.

WHITE_POINT_STYLE ?=

###########################################################################
##################### Common pathnames and filenames ######################
###########################################################################

# SDK root path and canonical version.  The pathname could potentially
# contain spaces, so use $(shell ls) instead of $(wildcard) to check for
# existence.
SDK_PREFIX = $(DEV_ROOT)/SDKs/iPhoneOS
ifeq ($(SDK_VERSION),)
override SDK_VERSION := $(shell ls -d '$(SDK_PREFIX)'* 2>/dev/null \
                                | sed -e 's,.*/,,; s/iPhoneOS//; s/\.sdk//' \
                                | sort -n \
                                | tail -n1)
endif
SDK_ROOT := $(SDK_PREFIX)$(SDK_VERSION).sdk

# If we're going to compile anything, make sure the various paths exist.
# Again, we use $(shell ls) instead of $(wildcard) to avoid problems if
# there are spaces in the pathname.

ifneq ($(filter-out help clean spotless,$(or $(MAKECMDGOALS),default)),)

ifeq ($(shell ls -d '$(DEV_ROOT)' 2>/dev/null),)
$(warning --- Failed to find Xcode iOS tools at $$(DEV_ROOT):)
$(warning ---     $(DEV_ROOT))
$(warning --- Rerun make with "XCODE_ROOT=/path/to/Xcode.app/Contents/Developer")
$(warning --- or "DEV_ROOT=/path/to/iPhoneOS.platform/Developer".)
$(error Xcode not found)
endif

ifeq ($(shell ls -d '$(TOOLCHAIN_ROOT)/usr/bin' 2>/dev/null),)
$(warning --- Failed to find Xcode toolchain at $$(TOOLCHAIN_ROOT):)
$(warning ---     $(TOOLCHAIN_ROOT))
$(warning --- Rerun make with "TOOLCHAIN_ROOT=/path/to/toolchain".)
$(error Xcode toolchain not found)
endif

ifeq ($(SDK_VERSION),)
$(warning --- Failed to find any iOS SDK under $$(DEV_ROOT):)
$(warning ---     $(DEV_ROOT))
$(warning --- Rerun make with "SDK_ROOT=/path/to/iPhoneOS.sdk".)
$(error iOS SDK not found)
endif

ifeq ($(shell ls -d '$(SDK_ROOT)' 2>/dev/null),)
$(warning --- Failed to find iOS SDK at $$(SDK_ROOT):)
$(warning ---     $(SDK_ROOT))
$(warning --- Rerun make with "SDK_ROOT=/path/to/iPhoneOS.sdk".)
$(error iOS SDK not found)
endif

ifneq ($(filter 1 2 3 4 5 6,$(firstword $(subst ., ,$(SDK_VERSION)))),)
$(warning --- iOS SDK (version $(SDK_VERSION)) is too old!)
$(warning --- Version 7.0 or later of the SDK is required.)
$(error iOS SDK too old)
endif

endif

# Default to building for all architectures if no target architecture is set.
TARGET_ARCH_ABI ?= all
# Avoid breaking if armv7 and arm64 are both explicitly requested.
# FIXME: Need a more general solution for explicit multi-arch lists.
ifeq (2,$(words $(filter armv7 arm64,$(TARGET_ARCH_ABI))))
    override TARGET_ARCH_ABI = all
endif
ifeq ($(filter all armv7 arm64,$(TARGET_ARCH_ABI)),)
    $(error Unsupported CPU architecture: $(TARGET_ARCH_ABI))
endif

# Target OS version (if not already defined).
TARGET_OS_VERSION ?= 8.0

# First component of target OS version.
TARGET_OS_VERSION_MAJOR = $(firstword $(subst ., ,$(TARGET_OS_VERSION)))

# Validate requested target version.
ifneq ($(filter 1 2 3 4,$(TARGET_OS_VERSION_MAJOR)),)
    $(error Target OS version $(TARGET_OS_VERSION) not supported (minimum 5.1.1))
else ifneq ($(and $(filter 1,$(DEVICE_TV)),$(filter 5 6 7 8,$(TARGET_OS_VERSION_MAJOR))),)
    $(error Target OS version $(TARGET_OS_VERSION) not supported for Apple TV (minimum 9.0))
endif
ifneq ($(and $(filter 1,$(EMBED_BITCODE)),$(filter 5,$(TARGET_OS_VERSION_MAJOR))),)
    $(error EMBED_BITCODE=1 requires a target OS version of 6.0 or newer)
endif

# iOS <7.0 require stdlibc++, which is no longer available in the iOS 12 SDK.
ifneq ($(filter 5 6,$(TARGET_OS_VERSION_MAJOR)),)
    ifeq ($(and $(wildcard $(SDK_ROOT)/usr/include/c++/4.2.1),$(wildcard $(SDK_ROOT)/usr/lib/libstdc++*)),)
        $(warning --- Headers/libraries for stdlibc++ not found.)
        $(warning --- Use Xcode 9 or earlier, or see README-ios.txt for workaround.)
        $(error Target OS version 5.x or 6.x requires stdlibc but stdlibc not found)
    endif
endif

# Profile selection cleanup.
ifneq ($(PROFILE_FILE),)
override PROFILE_NAME :=
else
ifeq ($(PROFILE_NAME),)
override PROFILE_NAME = $(if $(filter 1,$(DEBUG)),Team Provisioning Profile,Distribution)
endif
endif

#-------------------------------------------------------------------------#

# Output bundle (.app) directory name.
BUNDLE_DIR = $(EXECUTABLE_NAME).app

# The same thing, with spaces escaped for use in rules.
BUNDLE_DIR_ESCAPED = $(shell echo '$(BUNDLE_DIR)' | sed -e 's/ /\\ /g')

# Output package (.ipa) filename.
PACKAGE_FILE = $(EXECUTABLE_NAME).ipa

###########################################################################
######################### Build environment setup #########################
###########################################################################

_THISDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(_THISDIR)/../common/base.mk
include $(_THISDIR)/../common/config.mk


# Evaluate $(XCODE_ROOT) so we don't go shelling out on every reference.
XCODE_ROOT := $(XCODE_ROOT)

CC = $(TOOLCHAIN_ROOT)/usr/bin/clang

# Compiler ID to embed in the Info.plist file under the DTCompiler key.
# Doesn't seem to actually be necessary (at least Xcode 4.3.1 doesn't set it).
#CC_ID = com.apple.compilers.llvm.clang.1_0
CC_ID =

# Default flags used by Xcode (excluding those we already set in toolchain.mk).
PLATFORM_FLAGS = -arch $(TARGET_ARCH_ABI) \
                 -gdwarf-2 \
                 -isysroot $(SDK_ROOT) \
                 $(if $(filter armv7,$(TARGET_ARCH_ABI)),-mthumb) \
                 $(if $(filter 1,$(EMBED_BITCODE)),-fembed-bitcode-marker)
PLATFORM_CFLAGS = $(PLATFORM_FLAGS) \
                  $(if $(filter 1,$(DEBUG)),,-DNS_BLOCK_ASSERTIONS=1) \
                  -ffast-math \
                  -fmessage-length=0 \
                  -fvisibility=hidden \
                  -miphoneos-version-min=$(TARGET_OS_VERSION)
PLATFORM_CXXFLAGS = $(PLATFORM_CFLAGS) \
                    -fvisibility-inlines-hidden
PLATFORM_LDFLAGS = $(if $(filter 1,$(EMBED_BITCODE)),-fembed-bitcode-marker)

# SIL-specific modifications to default Xcode flags:
#     -mfpu=neon because all (SIL-supported) iOS devices support NEON.
PLATFORM_FLAGS += $(if $(filter armv7,$(TARGET_ARCH_ABI)),-mfpu=neon)

AR_RC  = $(TOOLCHAIN_ROOT)/usr/bin/ar rc
RANLIB = $(TOOLCHAIN_ROOT)/usr/bin/ranlib

# codesign_allocate moved from the SDK tree to the toolchain tree in Xcode 7.
CODESIGN_ALLOCATE := $(or $(wildcard $(TOOLCHAIN_ROOT)/usr/bin/codesign_allocate),$(DEV_ROOT)/usr/bin/codesign_allocate)

OBJECT_EXT  = .o
LIBRARY_EXT = .a

SYS_CFLAGS    = $(PLATFORM_CFLAGS)
SYS_CXXFLAGS  = $(PLATFORM_CXXFLAGS)
SYS_OBJCFLAGS = $(PLATFORM_CFLAGS)
SYS_ASFLAGS   = $(PLATFORM_FLAGS)
SYS_LDFLAGS   = $(PLATFORM_LDFLAGS)

# Put objects for each architecture in a separate directory, so we can
# build multiple architectures side-by-side.
OBJDIR_BASE := $(OBJDIR)
OBJDIR = $(OBJDIR_BASE)-$(TARGET_ARCH_ABI)

include $(_THISDIR)/../common/toolchain.mk

CC_LD = $(if $(filter 1,$(SIL_LINK_CXX_STL)),$(CXX),$(CC))


# Program paths and other definitions used only in this file.

WEAK_FRAMEWORKS = $(if $(filter 5 6,$(TARGET_OS_VERSION_MAJOR)),GameController)

FRAMEWORKS = AudioToolbox AVFoundation CoreGraphics CoreText Foundation \
             GameKit OpenGLES QuartzCore UIKit \
             $(filter-out $(WEAK_FRAMEWORKS),GameController) \
             $(EXTRA_FRAMEWORKS)

PACKAGEAPPLICATION_PATH ?= $(DEV_ROOT)/usr/bin/PackageApplication

###########################################################################
################# Source file lists and associated flags ##################
###########################################################################

SIL_SYS_CONFIG_HEADER = ios.h

SIL_SYS_SOURCES = $(SIL_OPENGL_SOURCES) \
                  sysdep/darwin/debug.c \
                  sysdep/darwin/meminfo.c \
                  sysdep/darwin/semaphore.c \
                  sysdep/darwin/time.c \
                  sysdep/ios/dialog.m \
                  sysdep/ios/gamekit.m \
                  sysdep/ios/graphics.m \
                  sysdep/ios/input.m \
                  sysdep/ios/log.m \
                  sysdep/ios/main.m \
                  sysdep/ios/misc.c \
                  sysdep/ios/osk.m \
                  sysdep/ios/sound.m \
                  sysdep/ios/sysfont.c \
                  sysdep/ios/thread.m \
                  sysdep/ios/userdata.c \
                  sysdep/ios/util.m \
                  sysdep/ios/view.m \
                  sysdep/misc/ioqueue.c \
                  sysdep/misc/movie-none.c \
                  sysdep/posix/condvar.c \
                  sysdep/posix/files.c \
                  sysdep/posix/fileutil.c \
                  sysdep/posix/misc.c \
                  sysdep/posix/mutex.c \
                  sysdep/posix/thread.c \
                  sysdep/posix/util.c \
                  $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                      test/sysdep/darwin/time.c \
                      test/sysdep/ios/graphics.c \
                      test/sysdep/ios/util.c \
                      test/sysdep/misc/ioqueue.c \
                      test/sysdep/posix/files.c \
                      test/sysdep/posix/fileutil.c \
                      test/sysdep/posix/internal.c \
                      test/sysdep/posix/misc.c \
                      test/sysdep/posix/thread.c \
                      test/sysdep/posix/userdata.c \
                  )

SIL_SYS_FLAGS = $(call _if-define,RUN_TESTS) \
                $(call _if-define,USE_FILE_SHARING,SIL_PLATFORM_IOS_USE_FILE_SHARING) \
                $(call _if-define,USE_GAMEKIT,SIL_PLATFORM_IOS_USE_GAMEKIT) \
                $(CFLAG_DEFINE)_POSIX_C_SOURCE=200809L \
                $(CFLAG_DEFINE)_DARWIN_C_SOURCE

# On 32-bit ARM, force ARM rather than Thumb instruction mode for SIL and
# client sources because it seems to provide better performance.
SIL_EXTRA_FLAGS += $(if $(filter armv7,$(TARGET_ARCH_ABI)),-mno-thumb)

include $(_THISDIR)/../common/sources.mk

# Special case for GameKit deprecation warnings which we ignore.
$(patsubst $(TOPDIR)%,$(OBJDIR)%,$(SIL_DIR)/src/sysdep/ios/gamekit$(OBJECT_EXT)): SIL_OBJCFLAGS += -Wno-deprecated-declarations

###########################################################################
############################### Build rules ###############################
###########################################################################

ifneq ($(EXECUTABLE_NAME),)
SIL_HELP_all           = 'build the application package ($(PACKAGE_FILE))'
SIL_HELP_binary        = 'build just the binary'
endif
SIL_HELP_gen-coverage  = 'generate coverage.out from coverage data'
SIL_HELP_coverage-html = 'generate HTML coverage results from coverage.out'
SIL_HELP_clean         = 'remove all intermediate files and dependency data'
SIL_HELP_spotless      = 'remove all generated files, including .ipa and bundle dir'
define SIL_HELPTRAILER
echo ''
echo 'Using Xcode installation path (XCODE_ROOT):  $(XCODE_ROOT)'
echo 'Building with SDK version (SDK_VERSION):     $(or $(SDK_VERSION),<not found>)'
echo 'Building for OS version (TARGET_OS_VERSION): $(or $(TARGET_OS_VERSION),<unknown>)'
echo ''
endef

include $(_THISDIR)/../common/rules.mk

#-------------------------------------------------------------------------#

.PHONY: all binary clean spotless

all: package

binary: $(BUNDLE_DIR_ESCAPED)/$(EXECUTABLE_NAME) $(BUNDLE_DIR_ESCAPED).dSYM

clean:
	$(ECHO) 'Removing object files'
	$(Q)rm -rf obj-* bin
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing generated files'
	$(Q)rm -rf Assets.xcassets Assets.plist $(EXECUTABLE_NAME).xcent
	$(ECHO) 'Removing temporary files'
	$(Q)rm -rf .covtmp
endif
	$(SIL_RECIPE_clean)

spotless: clean
ifneq ($(EXECUTABLE_NAME),)
	$(ECHO) 'Removing application files'
	$(Q)rm -rf '$(BUNDLE_DIR)' '$(BUNDLE_DIR).dSYM' '$(PACKAGE_FILE)'
endif
	$(ECHO) 'Removing coverage data'
	$(Q)rm -rf coverage coverage.out
	$(_SIL_TOOLS_RECIPE_spotless)
	$(SIL_RECIPE_spotless)

#-------------------------------------------------------------------------#

ifneq ($(EXECUTABLE_NAME),)  # Through the end of the file.

.PHONY: package
package: bundle
	$(ECHO) 'Packaging $(PACKAGE_FILE)'
	@# PackageApplication seems to need an absolute path for -o.
	@# In Xcode 8, PackageApplication spits out a warning to use xcodebuild
	@# instead (which we obviously can't), so suppress that unless V=1.
	$(Q)'$(PACKAGEAPPLICATION_PATH)' '$(BUNDLE_DIR)' -o '$(abspath .)/$(PACKAGE_FILE)' $(if $(filter 1,$(V)),,>/dev/null)

.PHONY: bundle
ifeq ($(VALIDATE),1)
bundle: validated-bundle
else
bundle: signed-bundle
endif

.PHONY: validated-bundle
validated-bundle: signed-bundle
	$(ECHO) 'Validating $(BUNDLE_DIR)'
	$(Q)'$(DEV_ROOT)/usr/bin/Validation' '$(BUNDLE_DIR)'

.PHONY: signed-bundle
signed-bundle: binary resources
	$(ECHO) 'Signing $(BUNDLE_DIR)'
	$(Q)rm -rf '$(BUNDLE_DIR)'/_CodeSignature '$(BUNDLE_DIR)'/CodeResources
	$(Q)IDENTITY=`'$(_THISDIR)/signing-helper.pl' \
	    $(if $(PROFILE_NAME),--profile='$(subst ','\'',$(PROFILE_NAME))') \
	    $(if $(PROFILE_FILE),--profile-file='$(subst ','\'',$(PROFILE_FILE))') \
	    $(if $(filter 1,$(V)),--verbose) \
	    '$(PACKAGE_NAME)' \
	    '$(BUNDLE_DIR)/embedded.mobileprovision' \
	    $(EXECUTABLE_NAME).xcent`; \
	test -n "$$IDENTITY" && \
	CODESIGN_ALLOCATE='$(CODESIGN_ALLOCATE)' /usr/bin/codesign \
	    --force \
	    --sign="$$IDENTITY" \
	    --entitlements=$(EXECUTABLE_NAME).xcent \
	    '$(BUNDLE_DIR)'

#-------------------------------------------------------------------------#

ifneq ($(filter all,$(TARGET_ARCH_ABI)),)

define _build-arch
bin/$$(EXECUTABLE_NAME)-$1: always-relink
	$$(Q)+$$(MAKE) 'bin/$$(EXECUTABLE_NAME)-$1' TARGET_ARCH_ABI='$1' SHOW_ARCH_IN_BUILD_MESSAGES=1
endef

ABIS-all = armv7 arm64
$(BUNDLE_DIR_ESCAPED)/$(EXECUTABLE_NAME): $(foreach arch,$(ABIS-$(TARGET_ARCH_ABI)),bin/$(EXECUTABLE_NAME)-$(arch))
	$(ECHO) 'Creating fat binary $@'
	@# As with the OSX build rules, we can't use $(@D) because it breaks
	@# if $(BUNDLE_DIR) contains spaces.
	$(Q)mkdir -p '$(BUNDLE_DIR)'
	$(Q)$(TOOLCHAIN_ROOT)/usr/bin/lipo -create $^ -output '$@'

$(foreach arch,$(ABIS-$(TARGET_ARCH_ABI)),$(eval $(call _build-arch,$(arch))))

else

$(BUNDLE_DIR_ESCAPED)/$(EXECUTABLE_NAME): bin/$(EXECUTABLE_NAME)-$(TARGET_ARCH_ABI)
	$(ECHO) 'Copying $< into package'
	$(Q)mkdir -p '$(BUNDLE_DIR)'
	$(Q)cp -p '$<' '$@'

endif

$(BUNDLE_DIR_ESCAPED).dSYM: $(BUNDLE_DIR_ESCAPED)/$(EXECUTABLE_NAME) \
                            $(BUNDLE_DIR_ESCAPED)/Info.plist
	$(ECHO) 'Generating symbol data in $(BUNDLE_DIR).dSYM'
	$(Q)$(TOOLCHAIN_ROOT)/usr/bin/dsymutil '$<' -o '$@'


bin/$(EXECUTABLE_NAME)-$(TARGET_ARCH_ABI): $(sort $(ALL_OBJECTS)) always-relink
	$(ECHO) 'Linking $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(CC_LD) \
	    -arch $(TARGET_ARCH_ABI) \
	    -isysroot $(SDK_ROOT) \
	    $(BASE_LDFLAGS) $(ALL_LDFLAGS) $(LDFLAGS) \
	    $(ALL_OBJECTS) \
	    -dead_strip \
	    -miphoneos-version-min='$(TARGET_OS_VERSION)' \
	    $(foreach I,$(FRAMEWORKS),-framework $I) \
	    $(foreach I,$(WEAK_FRAMEWORKS),-weak_framework $I) \
	    $(BASE_LIBS) $(ALL_LIBS) $(LIBS) \
	    $(CFLAG_LINK_OUTPUT)'$@'

# If we leave an existing binary in place because no objects have been
# updated, the codesign tool will theoretically rewrite the signature in
# the existing binary; however, experience has shown that iOS devices
# don't always accept such re-signed binaries, so we use this dummy rule
# to unconditionally force a relink.
.PHONY: always-relink
always-relink:

#-------------------------------------------------------------------------#

_SIL_STRINGS_FILES := $(shell ls '$(_THISDIR)'/*.lproj/*.strings 2>/dev/null)
_SIL_strings-name = $(lastword $(subst /, ,$(dir $1)))/$(notdir $1)
$(foreach i,$(_SIL_STRINGS_FILES) $(STRINGS_FILES),$(eval STRINGS_$(call _SIL_strings-name,$i) := $i))
STRINGS_LIST := $(foreach i,$(_SIL_STRINGS_FILES) $(STRINGS_FILES),$(call _SIL_strings-name,$i))

_SIL_XCASSETS_OS_VERSION := $(if $(filter 5.% 6.%,$(TARGET_OS_VERSION)),7.0,$(TARGET_OS_VERSION))

# _SIL_add-image(source, variable, outdir, source-var)
define _SIL_add-image
_ := $(if $(filter $(notdir $1),$($2)),$(error Duplicate image filename in $4: $1))
$2 += $(notdir $1)
$3/$(notdir $1): $1
	$$(ECHO) 'Copying $$< into asset catalog'
	$$(Q)mkdir -p '$$(@D)'
	$$(Q)cp '$$<' '$$@'

endef

_SIL_ICON_IMAGES_LOCAL :=
$(foreach i,$(ICON_IMAGES),$(eval $(call _SIL_add-image,$i,_SIL_ICON_IMAGES_LOCAL,Assets.xcassets/AppIcon.appiconset,ICON_IMAGES)))

_SIL_LAUNCH_IMAGES_LOCAL :=
$(foreach i,$(LAUNCH_IMAGES),$(eval $(call _SIL_add-image,$i,_SIL_LAUNCH_IMAGES_LOCAL,Assets.xcassets/LaunchImage.launchimage,LAUNCH_IMAGES)))

.PHONY: resources
resources: $(RESOURCES:%=$(BUNDLE_DIR_ESCAPED)/%) \
           $(BUNDLE_DIR_ESCAPED)/Assets.car \
           $(BUNDLE_DIR_ESCAPED)/Info.plist \
           $(BUNDLE_DIR_ESCAPED)/PkgInfo \
           $(if $(ICON_ITUNES),$(BUNDLE_DIR_ESCAPED)/iTunesArtwork) \
           $(STRINGS_LIST:%=$(BUNDLE_DIR_ESCAPED)/%) \
           $(if $(filter -DSIL_INCLUDE_TESTS,$(SIL_CFLAGS)),$(BUNDLE_DIR_ESCAPED)/testdata)

$(RESOURCES:%=$(BUNDLE_DIR_ESCAPED)/%) : $(BUNDLE_DIR_ESCAPED)/%: %
	$(ECHO) 'Copying $< into package'
	$(Q)mkdir -p '$(BUNDLE_DIR)'
	$(Q)cp -a '$<' '$(@D)/'
	$(Q)touch '$@'

$(BUNDLE_DIR_ESCAPED)/Assets.car: Assets.xcassets/Contents.json \
                                  Assets.xcassets/AppIcon.appiconset/Contents.json \
                                  Assets.xcassets/LaunchImage.launchimage/Contents.json \
                                  $(MAKEFILE_DEPS)
	$(ECHO) 'Building $@'
	$(Q)$(XCODE_ROOT)/usr/bin/actool \
	    --output-format human-readable-text \
	    --notices \
	    --warnings \
	    --app-icon AppIcon \
	    --launch-image LaunchImage \
	    --compress-pngs \
	    --enable-on-demand-resources YES \
	    $(if $(filter 1,$(DEVICE_IPHONE)),--target-device iphone) \
	    $(if $(filter 1,$(DEVICE_IPAD)),--target-device ipad) \
	    --minimum-deployment-target $(_SIL_XCASSETS_OS_VERSION) \
	    --platform iphoneos \
	    --product-type com.apple.product-type.application \
	    --output-partial-info-plist "$$(pwd)/Assets.plist" \
	    --compile '$(BUNDLE_DIR)' \
	    "$$(pwd)/Assets.xcassets" \
	    | perl \
	        -e '$$show = 0;' \
	        -e 'while (<>) {' \
	        -e '    if (m|^/\*|) {' \
	        -e '        $$show = !/compilation-results/;' \
	        -e '    } else {' \
	        -e '        print if $$show;' \
	        -e '    }' \
	        -e '}'

Assets.xcassets/Contents.json:
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)echo  >'$@~' '{'
	$(Q)echo >>'$@~' '  "info" : {'
	$(Q)echo >>'$@~' '    "version" : 1,'
	$(Q)echo >>'$@~' '    "author" : "xcode"'
	$(Q)echo >>'$@~' '  },'
	$(Q)echo >>'$@~' '  "properties" : {'
	$(Q)echo >>'$@~' '    "compression-type" : "lossless"'
	$(Q)echo >>'$@~' '  }'
	$(Q)echo >>'$@~' '}'
	$(Q)mv -f '$@~' '$@'

Assets.xcassets/AppIcon.appiconset/Contents.json: $(_SIL_ICON_IMAGES_LOCAL:%=Assets.xcassets/AppIcon.appiconset/%)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)echo  >'$@~' '{'
	$(Q)echo >>'$@~' '  "images" : ['
	$(Q)set $(if $(filter 1,$(V)),-x )-e; \
	    first=1; \
	    file_20x20=; \
	    file_40x40=; \
	    file_60x60=; \
	    file_80x80=; \
	    file_120x120=; \
	    for path in $^; do \
	        image=$$(basename "$$path"); \
	        size=$$(file -0 "$$path" | perl -pe 's/.*\0: // && s/^PNG image data, (\d+) x (\d+),.*/$$1 $$2/ or exit 1'); \
	        if test -z "$$size"; then \
	            echo >&2 "$$image: not found or not a PNG file"; \
	            exit 1; \
	        fi; \
	        w=$$(echo "$$size" | cut -d\  -f1); \
	        h=$$(echo "$$size" | cut -d\  -f2); \
	        if test $$w != $$h; then \
	            echo >&2 "$$image: icon is not square ($${w}x$${h})"; \
	            exit 1; \
	        fi; \
	        size=; \
	        scale=; \
	        idiom="iphone ipad"; \
	        if test $$w = 20; then size=20x20; scale=1x; file_20x20=$$image; fi; \
	        if test $$w = 29; then size=29x29; scale=1x; fi; \
	        if test $$w = 40; then size=20x20; scale=2x; file_40x40=$$image; fi; \
	        if test $$w = 58; then size=29x29; scale=2x; fi; \
	        if test $$w = 60; then size=60x60; scale=1x; idiom=iphone; file_60x60=$$image; fi; \
	        if test $$w = 76; then size=76x76; scale=1x; idiom=ipad; fi; \
	        if test $$w = 80; then size=40x40; scale=2x; file_80x80=$$image; fi; \
	        if test $$w = 87; then size=29x29; scale=3x; fi; \
	        if test $$w = 120; then size=60x60; scale=2x; idiom=iphone; file_120x120=$$image; fi; \
	        if test $$w = 152; then size=76x76; scale=2x; idiom=ipad; fi; \
	        if test $$w = 167; then size=83.5x83.5; scale=2x; idiom=ipad; fi; \
	        if test $$w = 180; then size=60x60; scale=3x; idiom=iphone; fi; \
	        if test $$w = 1024; then size=1024x1024; scale=1x; idiom=ios-marketing; fi; \
	        if test -z "$$size"; then \
	            echo >&2 "$$image: unknown size $${w}x$${h}"; \
	            exit 1; \
	        fi; \
	        for i in $$idiom; do \
	            if test -n "$$first"; then \
	                first=; \
	            else \
	                echo >>'$@~' '    },'; \
	            fi; \
	            echo >>'$@~' '    {'; \
	            echo >>'$@~' "      \"size\": \"$$size\","; \
	            echo >>'$@~' "      \"idiom\": \"$$i\","; \
	            echo >>'$@~' "      \"filename\": \"$$image\","; \
	            echo >>'$@~' "      \"minimum-system-version\": \"$(_SIL_XCASSETS_OS_VERSION)\","; \
	            echo >>'$@~' "      \"scale\": \"$$scale\""; \
	        done; \
	    done; \
	    if test -n "$$file_80x80" -a -n "$$file_40x40"; then \
	        for i in iphone ipad; do \
	            echo >>'$@~' '    },'; \
	            echo >>'$@~' '    {'; \
	            echo >>'$@~' "      \"size\": \"40x40\","; \
	            echo >>'$@~' "      \"idiom\": \"$$i\","; \
	            echo >>'$@~' "      \"filename\": \"$$file_40x40\","; \
	            echo >>'$@~' "      \"minimum-system-version\": \"$(_SIL_XCASSETS_OS_VERSION)\","; \
	            echo >>'$@~' "      \"scale\": \"1x\""; \
	        done; \
	    fi; \
	    if test -n "$$file_80x80" -a -n "$$file_120x120"; then \
	        for i in iphone ipad; do \
	            echo >>'$@~' '    },'; \
	            echo >>'$@~' '    {'; \
	            echo >>'$@~' "      \"size\": \"40x40\","; \
	            echo >>'$@~' "      \"idiom\": \"$$i\","; \
	            echo >>'$@~' "      \"filename\": \"$$file_120x120\","; \
	            echo >>'$@~' "      \"minimum-system-version\": \"$(_SIL_XCASSETS_OS_VERSION)\","; \
	            echo >>'$@~' "      \"scale\": \"3x\""; \
	        done; \
	    fi; \
	    if test \( -n "$$file_20x20" -o -n "$$file_40x40" \) -a -n "$$file_60x60"; then \
	        for i in iphone ipad; do \
	            echo >>'$@~' '    },'; \
	            echo >>'$@~' '    {'; \
	            echo >>'$@~' "      \"size\": \"20x20\","; \
	            echo >>'$@~' "      \"idiom\": \"$$i\","; \
	            echo >>'$@~' "      \"filename\": \"$$file_60x60\","; \
	            echo >>'$@~' "      \"minimum-system-version\": \"$(_SIL_XCASSETS_OS_VERSION)\","; \
	            echo >>'$@~' "      \"scale\": \"3x\""; \
	        done; \
	    fi; \
	    if test -z "$$first"; then \
	        echo >>'$@~' '    },'; \
	    fi
	$(Q)echo >>'$@~' '  ],'
	$(Q)echo >>'$@~' '  "info" : {'
	$(Q)echo >>'$@~' '    "version" : 1,'
	$(Q)echo >>'$@~' '    "author" : "xcode"'
	$(Q)echo >>'$@~' '  }'
	$(Q)echo >>'$@~' '}'
	$(Q)mv -f '$@~' '$@'

Assets.xcassets/LaunchImage.launchimage/Contents.json: $(_SIL_LAUNCH_IMAGES_LOCAL:%=Assets.xcassets/LaunchImage.launchimage/%)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)echo  >'$@~' '{'
	$(Q)echo >>'$@~' '  "images" : ['
	$(Q)set $(if $(filter 1,$(V)),-x )-e; \
	    first=1; \
	    for path in $^; do \
	        image=$$(basename "$$path"); \
	        size=$$(file -0 "$$path" | perl -pe 's/.*\0: // && s/^PNG image data, (\d+) x (\d+),.*/$$1 $$2/ or exit 1'); \
	        if test -z "$$size"; then \
	            echo >&2 "$$image: not found or not a PNG file"; \
	            exit 1; \
	        fi; \
	        w=$$(echo "$$size" | cut -d\  -f1); \
	        h=$$(echo "$$size" | cut -d\  -f2); \
		if test $$w -lt $$h; then \
	            orientation=portrait; \
	            s1=$$h; s2=$$w; \
	        else \
	            orientation=landscape; \
	            s1=$$w; s2=$$h; \
	        fi; \
	        idiom=; \
	        subtype=; \
	        scale=; \
	        if test $$s1 = 480 -a $$s2 = 320; then idiom=iphone; scale=1x; fi; \
	        if test $$s1 = 960 -a $$s2 = 640; then idiom=iphone; scale=2x; fi; \
	        if test $$s1 = 1136 -a $$s2 = 640; then idiom=iphone; subtype=retina4; scale=2x; fi; \
	        if test $$s1 = 1334 -a $$s2 = 750; then idiom=iphone; subtype=667h; scale=2x; fi; \
	        if test $$s1 = 1792 -a $$s2 = 828; then idiom=iphone; subtype=1792h; scale=2x; fi; \
	        if test $$s1 = 2208 -a $$s2 = 1242; then idiom=iphone; subtype=736h; scale=3x; fi; \
	        if test $$s1 = 2436 -a $$s2 = 1125; then idiom=iphone; subtype=2436h; scale=3x; fi; \
	        if test $$s1 = 2688 -a $$s2 = 1242; then idiom=iphone; subtype=2688h; scale=3x; fi; \
	        if test $$s1 = 1024 -a $$s2 = 768; then idiom=ipad; scale=1x; fi; \
	        if test $$s1 = 2048 -a $$s2 = 1536; then idiom=ipad; scale=2x; fi; \
	        if test $$s1 = 2224 -a $$s2 = 1668; then idiom=ipad; subtype=2224h; scale=2x; fi; \
	        if test $$s1 = 2388 -a $$s2 = 1668; then idiom=ipad; subtype=2388h; scale=2x; fi; \
	        if test $$s1 = 2732 -a $$s2 = 2048; then idiom=ipad; subtype=2732h; scale=2x; fi; \
	        if test -z "$$idiom"; then \
	            echo >&2 "$$image: unknown size $${w}x$${h}"; \
	            exit 1; \
	        fi; \
	        if test -n "$$first"; then \
	            first=; \
	            else \
	            echo >>'$@~' '    },'; \
	        fi; \
	        echo >>'$@~' '    {'; \
	        echo >>'$@~' "      \"orientation\": \"$$orientation\","; \
	        echo >>'$@~' "      \"idiom\": \"$$idiom\","; \
	        echo >>'$@~' "      \"filename\": \"$$image\","; \
	        echo >>'$@~' "      \"extent\": \"full-screen\","; \
	        if test -n "$$subtype"; then \
	            echo >>'$@~' "      \"subtype\": \"$$subtype\","; \
	        fi; \
	        echo >>'$@~' "      \"minimum-system-version\": \"$(_SIL_XCASSETS_OS_VERSION)\","; \
	        echo >>'$@~' "      \"scale\": \"$$scale\""; \
	    done; \
	    if test -z "$$first"; then \
	        echo >>'$@~' '    },'; \
	    fi
	$(Q)echo >>'$@~' '  ],'
	$(Q)echo >>'$@~' '  "info" : {'
	$(Q)echo >>'$@~' '    "version" : 1,'
	$(Q)echo >>'$@~' '    "author" : "xcode"'
	$(Q)echo >>'$@~' '  }'
	$(Q)echo >>'$@~' '}'
	$(Q)mv -f '$@~' '$@'

Assets.plist: $(BUNDLE_DIR_ESCAPED)/Assets.car

# Always regenerate Info.plist so we pick up the proper $(PROGRAM_VERSION_CODE)
# in case that's an autogenerated value.
.PHONY: $(BUNDLE_DIR_ESCAPED)/Info.plist
$(BUNDLE_DIR_ESCAPED)/Info.plist: $(INFO_PLIST_SOURCE) Assets.plist
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(BUNDLE_DIR)'
	$(Q)perl \
	    -e '$$xcode = `"$(XCODE_ROOT)/usr/bin/xcodebuild" -version`;' \
	    -e '$$xcode =~ /^Xcode (\d+)\.(\d+)(?:\.(\d+))?/m or die "Bad output from xcodebuild -version:\n$$xcode";' \
	    -e '$$xcode_version = sprintf("%02d%d%d", $$1, $$2, $$3);' \
	    -e '($$xcode_build) = ($$xcode =~ /^Build.*\s(\S+)/m);' \
	    -e '$$sdk = `"$(XCODE_ROOT)/usr/bin/xcodebuild" -version -sdk iphoneos$(SDK_VERSION)`;' \
	    -e '$$sdk =~ /^ProductBuildVersion:\s+(\S+)/m or die "Bad output from xcodebuild -version -sdk iphoneos$(SDK_VERSION):\n$$sdk";' \
	    -e '$$sdk_build = $$1;' \
	    -e '($$add{'BuildMachineOSBuild'}) = (`sw_vers -buildVersion` =~ /(.+)/);' \
	    -e '$$add{'DTCompiler'} = "$(CC_ID)";' \
	    -e '$$add{'DTPlatformBuild'} = "$$sdk_build";' \
	    -e '$$add{'DTPlatformName'} = "iphoneos";' \
	    -e '$$add{'DTPlatformVersion'} = "$(SDK_VERSION)";' \
	    -e '$$add{'DTSDKBuild'} = "$$sdk_build";' \
	    -e '$$add{'DTSDKName'} = "iphoneos$(SDK_VERSION)";' \
	    -e '$$add{'DTXcode'} = "$$xcode_version";' \
	    -e '$$add{'DTXcodeBuild'} = "$$xcode_build";' \
	    -e '$$add{'MinimumOSVersion'} = "$(TARGET_OS_VERSION)";' \
	    -e '$$add{'UIWhitePointAdaptivityStyle'} = "UIWhitePointAdaptivityStyle$(WHITE_POINT_STYLE)" if "$(WHITE_POINT_STYLE)";' \
	    -e '$$add_str = join("", map {sprintf("\t<key>%s</key>\n\t<string>%s</string>\n", $$_, $$add{$$_})} sort(keys(%add)));' \
	    -e '$$add_str .= "\t<key>UIFileSharingEnabled</key>\n\t<true/>\n" if "$(USE_FILE_SHARING)";' \
	    -e '$$add_str .= "\t<key>UIViewControllerBasedStatusBarAppearance</key>\n\t<false/>\n" if "$(firstword $(subst ., ,$(SDK_VERSION)))" >= 7;' \
	    -e '$$add_str .= "\t<key>CFBundleSupportedPlatforms</key>\n\t<array><string>iPhoneOS</string></array>\n";' \
	    -e '@devices = ();' \
	    -e 'push @devices, 1 if "$(DEVICE_IPHONE)";' \
	    -e 'push @devices, 2 if "$(DEVICE_IPAD)";' \
	    -e '$$add_str .= "\t<key>UIDeviceFamily</key>\n\t<array>\n"' \
	    -e '           . join("", map {"\t\t<integer>$$_</integer>\n"} @devices)' \
	    -e '           . "\t</array>\n";' \
	    -e 'open F, "<$$ARGV[1]" or die "$$ARGV[1]: $$!\n";' \
	    -e 'for (;;) {' \
	    -e '    $$_ = <F>;' \
	    -e '    defined($$_) or die "$<: <dict> not found\n";' \
	    -e '    last if /^<dict>/;' \
	    -e '}' \
	    -e 'for (;;) {' \
	    -e '    $$_ = <F>;' \
	    -e '    defined($$_) or die "$<: </dict> not found\n";' \
	    -e '    last if m|^</dict>|;' \
	    -e '    die "$<: duplicate root <dict>\n" if /^<dict>/;' \
	    -e '    $$add_str .= $$_;' \
	    -e '}' \
	    -e 'close F;' \
	    -e 'open F, "<$$ARGV[0]" or die "$$ARGV[0]: $!\n";' \
	    -e 'undef $$/;' \
	    -e '$$_ = <F>;' \
	    -e 'close F;' \
	    -e 's/\$$\{EXECUTABLE_NAME\}/$(EXECUTABLE_NAME)/g;' \
	    -e 's/\$$\{IPHONEOS_DEPLOYMENT_TARGET\}/$(TARGET_OS_VERSION)/g;' \
	    -e 's/\$$\{PRODUCT_NAME\}/$(PROGRAM_NAME)/g;' \
	    -e 's/(CFBundleIdentifier.*\n.*)<string>[^<]*/$$1<string>$(PACKAGE_NAME)/g;' \
	    -e 's/(CFBundleShortVersionString.*\n.*)<string>[^<]*/$$1<string>$(PROGRAM_VERSION)/g if "$(PROGRAM_VERSION)" ne "";' \
	    -e 's/(CFBundleVersion.*\n.*)<string>[^<]*/$$1<string>$(PROGRAM_VERSION_CODE)/g;' \
	    -e 's|\n(</dict>)|\n$${add_str}$$1|;' \
	    -e 'print;' \
	    '$(INFO_PLIST_SOURCE)' Assets.plist >'$@' \
	    && plutil -convert binary1 '$@' \
	    || rm -f '$@'

$(BUNDLE_DIR_ESCAPED)/PkgInfo: $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(BUNDLE_DIR)'
	$(Q)printf 'APPL????' >'$@'

$(BUNDLE_DIR_ESCAPED)/iTunesArtwork: $(ICON_ITUNES)
	$(ECHO) 'Copying $@ from $<'
	$(Q)mkdir -p '$(@D)'
	$(Q)cp -p '$<' '$@'

$(BUNDLE_DIR_ESCAPED)/testdata: $(SIL_DIR)/testdata $(MAKEFILE_DEPS)
	$(ECHO) 'Copying $@ from $<'
	@# Delete the old directory so removed files don't hang around.
	$(Q)rm -rf '$@'
	$(Q)mkdir -p '$@'
	$(Q)cp -a '$<'/* '$@/'

.SECONDEXPANSION:

$(STRINGS_LIST:%=$(BUNDLE_DIR_ESCAPED)/%) : \
$(BUNDLE_DIR_ESCAPED)/%: $$(STRINGS_$$(call _SIL_strings-name,$$(subst $$(preserve-space) ,_,$$@))) $$(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@ from $<'
	$(Q)mkdir -p '$(BUNDLE_DIR)/$(lastword $(subst /, ,$(@D)))'
	$(Q)$(_THISDIR)/../macosx/copystrings.sh \
	    --validate \
	    --inputencoding utf-8 \
	    --outputencoding binary \
	    --outdir '$(BUNDLE_DIR)/$(lastword $(subst /, ,$(@D)))' \
	    '$<'


endif  # ifneq ($(EXECUTABLE_NAME),)

###########################################################################
###########################################################################
