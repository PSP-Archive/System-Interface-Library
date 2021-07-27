#
# libwebmdec: a decoder library for WebM audio/video streams
# Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
#
# This software may be copied and redistributed under certain conditions;
# see the file "COPYING" in the source code distribution for details.
# NO WARRANTY is provided with this software.
#

###########################################################################
############################## Configuration ##############################
###########################################################################

# In addition to the options below, standard build environment variables
# (CC, CFLAGS, etc.) are also recognized.

#----------------------------- Build options -----------------------------#

# BUILD_FRONTEND:  If this variable is set to 1, the build process will
# also build the sample frontend (tools/frontend.c), creating the
# executable "webmdec" in the top directory.  Note that at least one of
# BUILD_SHARED and BUILD_STATIC must be enabled for the frontend to be
# built.
#
# The default is 0 (the sample frontend will not be built).

BUILD_FRONTEND = 0


# BUILD_SHARED:  If this variable is set to 1, the build process will
# create a shared library (typically "libwebmdec.so" on Unix-like systems).
# If the variable is set to 0, no shared library will not be built.
#
# The default is 1 (a shared library will be built).

BUILD_SHARED = 1


# BUILD_STATIC:  If this variable is set to 1, the build process will
# create a static library (typically "libwebmdec.a" on Unix-like systems).
# If the variable is set to 0, no static library will not be built.
#
# It is possible, though mostly meaningless, to set both BUILD_SHARED and
# BUILD_STATIC to 0.  In this case, "make" will do nothing, and "make
# install" will install only the library header file.
#
# The default is 1 (a static library will be built).

BUILD_STATIC = 1


# DECODE_AUDIO:  If this variable is set to 1, the library will include
# support for decoding audio using libnogg.  If the variable is set to 0,
# audio decoding support will be disabled.
#
# The default is 1 (audio decoding support enabled).

DECODE_AUDIO = 1


# DECODE_VIDEO:  If this variable is set to 1, the library will include
# support for decoding video using libvpx.  If the variable is set to 0,
# video decoding support will be disabled.
#
# The default is 1 (video decoding support enabled).

DECODE_VIDEO = 1


# INSTALL_PKGCONFIG:  If this variable is set to 1, the build process will
# install a control file for the "pkg-config" tool as
# "$(LIBDIR)/pkgconfig/webmdec.pc".
#
# The default is 0 (webmdec.pc will not be installed).

INSTALL_PKGCONFIG = 0


# USE_STDIO:  If this variable is set to 1, the library will include
# support for reading files from the filesystem using C stdio.  If the
# variable is set to 0, this support will be disabled, and the library
# will not reference any stdio functions.
#
# The default is 1 (stdio support enabled).

USE_STDIO = 1


# WARNINGS_AS_ERRORS:  If this variable is set to 1, the build will abort
# if the compiler emits any warnings.
#
# The default is 0 (warnings will not abort the build).

WARNINGS_AS_ERRORS = 0

#----------------------- Installation target paths -----------------------#

# BINDIR:  Sets the directory into which the sample frontend (webmdec) will
# be installed.  This path is not used if BUILD_FRONTEND is set to 0.
#
# The default is "$(PREFIX)/bin".

BINDIR = $(PREFIX)/bin


# DESTDIR:  Sets the location of the installation root directory.  This
# string, if any, is prefixed to all target pathnames during installation,
# but it is not included in pathnames
# allowing files to be installed 
#
# The default is the empty string.

DESTDIR =


# INCDIR:  Sets the directory into which the library's header file
# (webmdec.h) will be installed.
#
# The default is "$(PREFIX)/include".

INCDIR = $(PREFIX)/include


# LIBDIR:  Sets the directory into which the shared and static library
# files will be installed.
#
# The default is "$(PREFIX)/lib".

LIBDIR = $(PREFIX)/lib


# PREFIX:  Sets the base directory for installation paths.  This is only
# used to set the default paths for BINDIR, INCDIR, and LIBDIR, and to set
# the "prefix" tag in the pkg-config control file if it is installed.
#
# The default is "/usr/local".

PREFIX = /usr/local

#----------------- Dependent library installation paths ------------------#

# There are three variables for each dependent library: <library>_PREFIX,
# <library>_INCLUDE, and <library>_LIB.  The <library>_PREFIX variable is
# only used in the default INCLUDE and LIB paths for each library if those
# variables are not individually set; if the INCLUDE and LIB paths are
# explicitly specified for a library, the PREFIX variable may be left unset.
#
# The header and library paths used for each library are determined as
# follows:
#
# - If the specific path (<library>_INCLUDE or <library>_LIB) is set to a
#   non-empty value, that path is used.
#
# - If the specific path is empty but the prefix (<library>_PREFIX) is set
#   to a non-empty value, the specific path is set to the "include" or "lib"
#   subdirectory of the prefix.
#
# - If both the specific path and the prefix are empty, then if the
#   pkg-config tool is available and a pkg-config control file is installed
#   for the library, pkg-config is used to look up the specific path.
#
# - Otherwise, the headers or libraries are assumed to be available on the
#   compiler's default search path.

LIBNESTEGG_PREFIX =
LIBNESTEGG_INCLUDE =
LIBNESTEGG_LIB =

LIBNOGG_PREFIX =
LIBNOGG_INCLUDE =
LIBNOGG_LIB =

LIBVPX_PREFIX =
LIBVPX_INCLUDE =
LIBVPX_LIB =

###########################################################################
############################## Internal data ##############################
###########################################################################

#-------------------------------------------------------------------------#
# Note: You should never need to modify any variables below this point in #
# the file.                                                               #
#-------------------------------------------------------------------------#

# Package name:
PACKAGE = webmdec

# Library version:
VERSION = 0.9

# Output filenames:
FRONTEND_BIN = $(PACKAGE)
SHARED_LIB = lib$(PACKAGE).so
STATIC_LIB = lib$(PACKAGE).a
TEST_BIN = $(PACKAGE)-test

# Library object filenames:
LIBRARY_OBJECTS := $(sort $(strip \
    $(patsubst %.c,%.o,$(wildcard src/*.c))))

###########################################################################
############################ Helper functions #############################
###########################################################################

# if-true:  Return the second parameter if the variable named by the first
# parameter has the value 1, the third parameter (which may be omitted)
# otherwise.

if-true = $(if $(filter 1,$($1)),$2,$3)


# define-if-true:  Return an appropriate -D compiler option for the given
# variable name if its value is 1, the empty string otherwise.

define-if-true = $(call if-true,$1,-D$1)


# pkg-config:  Try to run the pkg-config tool with the arguments in the
# first parameter, returning its output on success and the empty string
# otherwise.

pkg-config = $(shell pkg-config $1)


# select-incdirs:  Return the header directory options for the library
# whose variable name prefix is the first parameter and whose pkg-config
# tag is the second parameter.

select-incdirs = $(strip \
    $(if $($1_INCLUDE), -I'$($1_INCLUDE)', \
    $(if $($1_PREFIX), -I'$($1_PREFIX)/include', \
    $(if $2,$(call pkg-config,--cflags-only-I $2)))))


# select-libdirs:  Return the library directory options for the library
# whose variable name prefix is the first parameter and whose pkg-config
# tag is the second parameter.

select-libdirs = $(strip \
    $(if $($1_LIB), -I'$($1_LIB)', \
    $(if $($1_PREFIX), -I'$($1_PREFIX)/include', \
    $(if $2,$(call pkg-config,--libs-only-L $2)))))

###########################################################################
######################### Library path/flag setup #########################
###########################################################################

# For each library, we set up the following flags:
#     <library>_INCDIRS: Header directory (-I) options.
#     <library>_LIBDIRS: Library directory (-L) options.
#     <library>_LIBS: Library (-l) options, in link order.
# We also define LIBRARY_* variables which hold the concatenation of all
# libraries' options of each type, taking build settings into account.

LIBNESTEGG_INCDIRS = $(call select-incdirs,LIBNESTEGG,nestegg)
LIBNESTEGG_LIBDIRS = $(call select-libdirs,LIBNESTEGG,nestegg)
LIBNESTEGG_LIBS = -lnestegg

LIBNOGG_INCDIRS = $(call select-incdirs,LIBNOGG,nogg)
LIBNOGG_LIBDIRS = $(call select-libdirs,LIBNOGG,nogg)
LIBNOGG_LIBS = -lnogg

LIBVPX_INCDIRS = $(call select-incdirs,LIBVPX,vpx)
LIBVPX_LIBDIRS = $(call select-libdirs,LIBVPX,vpx)
LIBVPX_LIBS = -lvpx -lm

LIBRARY_INCDIRS = $(strip \
    $(LIBNESTEGG_INCDIRS) \
    $(call if-true,DECODE_AUDIO,$(LIBNOGG_INCDIRS)) \
    $(call if-true,DECODE_VIDEO,$(LIBVPX_INCDIRS)))
LIBRARY_LIBDIRS = $(strip \
    $(LIBNESTEGG_LIBDIRS) \
    $(call if-true,DECODE_AUDIO,$(LIBNOGG_LIBDIRS)) \
    $(call if-true,DECODE_VIDEO,$(LIBVPX_LIBDIRS)))
LIBRARY_LIBS = $(strip \
    $(LIBNESTEGG_LIBS) \
    $(call if-true,DECODE_AUDIO,$(LIBNOGG_LIBS)) \
    $(call if-true,DECODE_VIDEO,$(LIBVPX_LIBS)))

###########################################################################
############################# Toolchain setup #############################
###########################################################################

# Default tool program names:

CC ?= cc
AR ?= ar
RANLIB ?= ranlib


# Try and guess what sort of compiler we're using, so we can set
# appropriate default options.

CC_TYPE = unknown
ifneq ($(filter clang%,$(subst -, ,$(CC))),)
    CC_TYPE = clang
else ifneq ($(filter icc%,$(subst -, ,$(CC))),)
    CC_TYPE = icc
else ifneq ($(filter gcc%,$(subst -, ,$(CC))),)
    CC_TYPE = gcc
else
    CC_VERSION_TEXT := $(shell $(CC) --version 2>&1)
    ifneq (,$(filter clang LLVM,$(CC_VERSION_TEXT)))
        CC_TYPE = clang
    else ifneq (,$(filter gcc GCC,$(CC_VERSION_TEXT)))
        CC_TYPE = gcc
    else ifneq (__GNUC__,$(shell echo __GNUC__ | $(CC) -E - 2>/dev/null | tail -1))
        # GCC invoked as "cc" may not have any "gcc" in --version output,
        # so treat a compiler whose preprocessor recognizes __GNUC__ (and
        # thus translates it to something else) as GCC.
        CC_TYPE = gcc
    endif
endif

ifeq ($(CC_TYPE),clang)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wpointer-arith -Wshadow -Wwrite-strings \
        -Wundef -Wno-unused-parameter -Wvla
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else ifeq ($(CC_TYPE),gcc)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wlogical-op -Wpointer-arith -Wshadow \
        -Wwrite-strings -Wundef -Wno-unused-parameter -Wvla
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else ifeq ($(CC_TYPE),icc)
    BASE_FLAGS = -O2 -g -I. \
        $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wpointer-arith -Wreturn-type -Wshadow -Wuninitialized \
        -Wunknown-pragmas -Wunused-function -Wunused-variable -Wwrite-strings
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else
    $(warning *** Warning: Unknown compiler type.)
    $(warning *** Make sure your CFLAGS are set correctly!)
endif


# Final flag set.  Note that the user-specified $(CFLAGS) reference comes
# last so the user can override any of our default flags.

ALL_DEFS = $(strip \
    $(call define-if-true,DECODE_AUDIO) \
    $(call define-if-true,DECODE_VIDEO) \
    $(call define-if-true,USE_STDIO) \
    $(call define-if-true,USE_TREMOR) \
    -DVERSION=\"$(VERSION)\")

ALL_CFLAGS = $(BASE_CFLAGS) $(LIBRARY_INCDIRS) $(ALL_DEFS) $(CFLAGS)

###########################################################################
############################### Build rules ###############################
###########################################################################

#----------------------------- Entry points ------------------------------#

.PHONY: all all-frontend all-shared all-static install install-frontend
.PHONY: install-headers install-pc install-shared install-static
.PHONY: test clean spotless


all: $(call if-true,BUILD_SHARED,all-shared) \
     $(call if-true,BUILD_STATIC,all-static) \
     $(call if-true,BUILD_FRONTEND,all-frontend)

all-frontend: $(FRONTEND_BIN)

all-shared: $(SHARED_LIB)

all-static: $(STATIC_LIB)


install: $(call if-true,BUILD_SHARED,install-shared) \
         $(call if-true,BUILD_STATIC,install-static) \
         install-headers \
         $(call if-true,INSTALL_PKGCONFIG,install-pc) \
         $(call if-true,BUILD_FRONTEND,install-frontend)

install-frontend: all-frontend
	mkdir -p '$(DESTDIR)$(BINDIR)'
	cp -pf $(FRONTEND_BIN) '$(DESTDIR)$(BINDIR)/'

install-headers:
	mkdir -p '$(DESTDIR)$(INCDIR)'
	cp -pf include/webmdec.h '$(DESTDIR)$(INCDIR)/'

install-pc:
	mkdir -p '$(DESTDIR)$(LIBDIR)/pkgconfig'
	sed \
	    -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@INCDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(INCDIR))|g' \
	    -e 's|@LIBDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(LIBDIR))|g' \
	    -e 's|@REQUIRES__VORBIS@|$(call if-true,DECODE_AUDIO, vorbis)|g' \
	    -e 's|@REQUIRES__VPX@|$(call if-true,DECODE_VIDEO, vpx)|g' \
	    -e 's|@VERSION@|$(VERSION)|g'\
	    <$(PACKAGE).pc.in >'$(DESTDIR)$(LIBDIR)/pkgconfig/$(PACKAGE).pc'

install-shared: all-shared
	mkdir -p '$(DESTDIR)$(LIBDIR)'
	cp -pf $(SHARED_LIB) '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB).$(VERSION)'
	ln -s $(SHARED_LIB).$(VERSION) '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB).$(firstword $(subst ., ,$(VERSION)))'
	ln -s $(SHARED_LIB).$(VERSION) '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB)'

install-static: all-static
	mkdir -p '$(DESTDIR)$(LIBDIR)'
	cp -pf $(SHARED_LIB) '$(DESTDIR)$(LIBDIR)/'


test: $(TEST_BIN)
	./$(TEST_BIN)


clean:
	rm -f src/*.o test/*.o tools/*.o

spotless: clean
	rm -f $(SHARED_LIB) $(STATIC_LIB) $(FRONTEND_BIN) $(TEST_BIN)

#-------------------------- Library build rules --------------------------#

$(SHARED_LIB): $(LIBRARY_OBJECTS:%.o=%_so.o)
	$(CC) -o $@ $^ \
	    -shared \
	    -Wl,-soname=lib$(PACKAGE).so.$(firstword $(subst ., ,$(VERSION)))

$(STATIC_LIB): $(LIBRARY_OBJECTS)
	$(AR) rcu $@ $^
	$(RANLIB) $@

#------------------------- Frontend build rules --------------------------#

ifneq ($(filter 1,$(BUILD_SHARED) $(BUILD_STATIC)),)

$(FRONTEND_BIN): tools/frontend.o $(call if-true,BUILD_SHARED,$(SHARED_LIB),$(STATIC_LIB))
	$(CC) -o $@ $^ \
	    $(LDFLAGS) $(LIBRARY_LIBDIRS) $(LIBRARY_LIBS)

tools/frontend.o: tools/frontend.c
	$(CC) $(ALL_CFLAGS) -Iinclude -o $@ -c $<

else

$(FRONTEND_BIN):
	$(error Cannot build the frontend without building the library)

endif

#--------------------------- Test build rules ----------------------------#

$(TEST_BIN): $(LIBRARY_OBJECTS) $(patsubst %.c,%.o,$(wildcard test/*.c))
	$(CC) -o $@ $^ \
	    $(LDFLAGS) $(LIBRARY_LIBDIRS) $(LIBRARY_LIBS)

$(patsubst %.c,%.o,$(wildcard test/*.c)): $(wildcard test/*.h)

#----------------------- Common compilation rules ------------------------#

%.o: %.c $(wildcard include/*.h)
	$(CC) $(ALL_CFLAGS) -o $@ -c $<

%_so.o: %.c $(wildcard include/*.h)
	$(CC) $(ALL_CFLAGS) -fPIC -o $@ -c $<

###########################################################################
