#
# libnogg: a decoder library for Ogg Vorbis streams
# Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
#
# This software may be copied and redistributed under certain conditions;
# see the file "COPYING" in the source code distribution for details.
# NO WARRANTY is provided with this software.
#

#
# By default, all commands will be displayed in an abbreviated form, such
# as "Compiling foo.c", instead of the actual command lines executed.
# Pass V=1 on the make command line to display the actual commands instead.
#

###########################################################################
############################## Configuration ##############################
###########################################################################

# In addition to the options below, standard build environment variables
# (CC, CFLAGS, etc.) are also recognized.

#----------------------------- Build options -----------------------------#

# BUILD_SHARED:  If this variable is set to 1, the build process will
# create a shared library (typically "libnogg.so" on Unix-like systems).
# If the variable is set to 0, no shared library will not be built.
#
# The default is 1 (a shared library will be built).

BUILD_SHARED = 1


# BUILD_STATIC:  If this variable is set to 1, the build process will
# create a static library (typically "libnogg.a" on Unix-like systems).
# If the variable is set to 0, no static library will not be built.  Note
# that tests are linked against the static library, so "make test" will
# cause the static library to be built even if this variable is set to 0.
#
# It is possible, though mostly meaningless, to set both BUILD_SHARED and
# BUILD_STATIC to 0.  In this case, "make" will do nothing, and "make
# install" will install only the library header file.
#
# The default is 1 (a static library will be built).

BUILD_STATIC = 1


# BUILD_TOOLS:  If this variable is set to 1, the build process will also
# build the tool programs in the tools/ directory, creating tool
# executables in the top directory.  Note that at least one of BUILD_SHARED
# and BUILD_STATIC must be enabled for the tool programs to be built.
#
# The default is 0 (the tool programs will not be built).

BUILD_TOOLS = 1


# ENABLE_ASM_ARM_NEON:  If this variable is set to 1, optimized assembly
# code for the ARM platform using NEON instructions will be compiled into
# the library.
#
# The default is 0 (NEON assembly code will not be included).

ENABLE_ASM_ARM_NEON = 0


# ENABLE_ASM_X86_SSE2:  If this variable is set to 1, optimized assembly
# code for the x86 platform using SSE2 instructions will be compiled into
# the library.
#
# The default is 1 when building for an x86 platform, 0 otherwise.  (If the
# build target platform cannot be detected, the value will default to 0.)

ENABLE_ASM_X86_SSE2 = 0


# ENABLE_ASSERT:  If this variable is set to 1, additional assertion checks
# will be compiled into the code to guard against bugs in the library.
# This requires support for the assert() macro and <assert.h> header in the
# system's runtime libraries.
#
# The default is 0 (assertion checks will not be included).

ENABLE_ASSERT = 0


# INSTALL_PKGCONFIG:  If this variable is set to 1, the build process will
# install a control file for the "pkg-config" tool as
# "$(LIBDIR)/pkgconfig/nogg.pc".
#
# The default is 0 (nogg.pc will not be installed).

INSTALL_PKGCONFIG = 0


# TREMOR_SOURCE:  If this variable is set to the pathname of a directory
# containing the Tremor (http://svn.xiph.org/trunk/Tremor/) source code,
# the nogg-benchmark tool will include support for benchmarking against
# Tremor as well as libvorbis.
#
# The default is to not include Tremor benchmarking support.

TREMOR_SOURCE =


# USE_STDIO:  If this variable is set to 1, the library will include
# support for reading files from the filesystem using C stdio.  If the
# variable is set to 0, this support will be disabled, and the library
# will not reference any stdio functions.
#
# The default is 1 (stdio support will be included).

USE_STDIO = 1


# WARNINGS_AS_ERRORS:  If this variable is set to 1, the build will abort
# if the compiler emits any warnings.
#
# The default is 0 (warnings will not abort the build).

WARNINGS_AS_ERRORS = 0

#----------------------- Installation target paths -----------------------#

# BINDIR:  Sets the directory into which the tool programs (nogg-benchmark
# and nogg-decode) will be installed.  This path is not used if BUILD_TOOLS
# is set to 0.
#
# The default is "$(PREFIX)/bin".

BINDIR = $(PREFIX)/bin


# DESTDIR:  Sets the location of the installation root directory.  This
# string, if any, is prefixed to all target pathnames during installation,
# but it is not included in pathnames written to installed files, allowing
# the package to be installed to a non-root directory hierarchy.
#
# The default is the empty string.

DESTDIR =


# INCDIR:  Sets the directory into which the library's header file
# (nogg.h) will be installed.
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

###########################################################################
############################## Internal data ##############################
###########################################################################

#-------------------------------------------------------------------------#
# Note: You should never need to modify any variables below this point in #
# the file.                                                               #
#-------------------------------------------------------------------------#

# Package name:
PACKAGE = nogg

# Library version:
VERSION = 1.14
VERSION_MAJOR = $(firstword $(subst ., ,$(VERSION)))

# Library output filenames: (note that $(OSTYPE) is set below)
SHARED_LIB = lib$(PACKAGE).$(if $(filter darwin%,$(OSTYPE)),dylib,$(if $(filter mingw%,$(ARCH)),dll,so))
STATIC_LIB = lib$(PACKAGE).a

# Source and object filenames:
LIBRARY_SOURCES := $(wildcard src/*/*.c)
LIBRARY_OBJECTS := $(LIBRARY_SOURCES:%.c=%.o)
TEST_SOURCES := $(wildcard tests/*/*.c)
TEST_OBJECTS := $(TEST_SOURCES:%.c=%.o)
TEST_BINS := $(TEST_SOURCES:%.c=%)
TOOL_SOURCES := $(wildcard tools/nogg-*.c)
TOOL_BINS := $(TOOL_SOURCES:tools/%.c=%)

###########################################################################
#################### Helper functions and definitions #####################
###########################################################################

# if-true:  Return the second parameter if the variable named by the first
# parameter has the value 1, the third parameter (which may be omitted)
# otherwise.

if-true = $(if $(filter 1,$($1)),$2,$3)


# define-if-true:  Return an appropriate -D compiler option for the given
# variable name if its value is 1, the empty string otherwise.

define-if-true = $(call if-true,$1,-D$1)


# ECHO:  Expands to "@:" (a no-op) in verbose (V=1) mode, "@echo" in
# non-verbose mode.  Used for displaying abbreviated command text.

ECHO = $(call if-true,V,@:,@echo)


# Q:  Expands to the empty string in verbose (V=1) mode, "@" otherwise.
# Used to hide output of command lines in non-verbose mode.

Q = $(call if-true,V,,@)

###########################################################################
############################# Toolchain setup #############################
###########################################################################

# Default tool program names:

CC = cc
AR = ar
RANLIB = ranlib
GCOV = $(error gcov program unknown for this compiler)


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
        -pedantic -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wpointer-arith -Wshadow -Wwrite-strings \
        -Wundef -Wno-unused-parameter -Wvla \
        $(call if-true,ENABLE_ASM_ARM_NEON,-mfpu=neon) \
        $(call if-true,ENABLE_ASM_X86_SSE2,-msse -msse2)
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
    GCOV = llvm-cov
    GCOV_OPTS = -b -c
    GCOV_FILE_OPTS = \
        -gcno="`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.gcno|'`" \
        -gcda="`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.gcda|'`" \
        -o "`echo "$1" | sed -e 's|[/.]|-|g'`.out"
else ifeq ($(CC_TYPE),gcc)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wlogical-op -Wpointer-arith -Wshadow \
        -Wwrite-strings -Wundef -Wno-unused-parameter -Wvla \
        $(call if-true,ENABLE_ASM_ARM_NEON,-mfpu=neon) \
        $(call if-true,ENABLE_ASM_X86_SSE2,-msse -msse2)
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 -pedantic \
        -Wmissing-declarations -Wstrict-prototypes
    GCOV = gcov >/dev/null
    GCOV_OPTS = -b -c -l -p
    GCOV_FILE_OPTS = -o "`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.o|'`" '$1'
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


# Update build parameter defaults based on the compiler's target platform.

ifneq ($(filter clang gcc icc,$(CC_TYPE)),)
    TARGET := $(subst -, ,$(shell $(CC) -dumpmachine))
    ARCH := $(or $(firstword $(TARGET)),unknown)
    OSTYPE := $(or $(word 3,$(TARGET)),unknown)
else
    ARCH := unknown
    OSTYPE := unknown
endif

ifneq ($(filter i386 x86_64,$(ARCH)),)
    ENABLE_ASM_X86_SSE2 = 1
endif

ifneq ($(filter darwin%,$(OSTYPE)),)
    SHARED_LIB_FULLNAME = $(subst .dylib,.$(VERSION).dylib,$(SHARED_LIB))
    SHARED_LIB_LINKNAME = $(subst .dylib,.$(VERSION_MAJOR).dylib,$(SHARED_LIB))
    SHARED_LIB_LDFLAGS = \
        -dynamiclib \
        -install_name '$(LIBDIR)/$(SHARED_LIB_FULLNAME)' \
        -compatibility_version $(firstword $(subst ., ,$(VERSION))) \
        -current_version $(VERSION) \
        -Wl,-single_module
else ifneq ($(filter mingw%,$(ARCH)),)
    SHARED_LIB_FULLNAME = $(SHARED_LIB)
    SHARED_LIB_LDFLAGS = \
        -shared \
        -Wl,--enable-auto-image-base \
        -Xlinker --out-implib -Xlinker '$@.a'
else
    SHARED_LIB_FULLNAME = $(SHARED_LIB).$(VERSION)
    SHARED_LIB_LINKNAME = $(SHARED_LIB).$(VERSION_MAJOR)
    SHARED_LIB_CFLAGS = -fPIC
    SHARED_LIB_LDFLAGS = -shared -Wl,-soname=$(SHARED_LIB_LINKNAME)
endif


# Final flag set.  Note that the user-specified $(CFLAGS) reference comes
# last so the user can override any of our default flags.

ALL_DEFS = $(strip \
    $(call define-if-true,ENABLE_ASM_ARM_NEON) \
    $(call define-if-true,ENABLE_ASM_X86_SSE2) \
    $(call define-if-true,ENABLE_ASSERT) \
    $(call define-if-true,USE_STDIO) \
    -DVERSION=\"$(VERSION)\")

ALL_CFLAGS = $(BASE_CFLAGS) $(ALL_DEFS) $(CFLAGS)


# Libraries to use when linking tool and test programs.

LIBS = -lm

###########################################################################
############################### Build rules ###############################
###########################################################################

#----------------------------- Entry points ------------------------------#

.PHONY: all all-shared all-static all-tools
.PHONY: install install-headers install-pc install-shared install-static install-tools
.PHONY: test clean spotless


all: $(call if-true,BUILD_SHARED,all-shared) \
     $(call if-true,BUILD_STATIC,all-static) \
     $(call if-true,BUILD_TOOLS,all-tools)

all-shared: $(SHARED_LIB)

all-static: $(STATIC_LIB)

all-tools: $(TOOL_BINS)


install: $(call if-true,BUILD_SHARED,install-shared) \
         $(call if-true,BUILD_STATIC,install-static) \
         install-headers \
         $(call if-true,INSTALL_PKGCONFIG,install-pc) \
         $(call if-true,BUILD_TOOLS,install-tools)

install-headers:
	$(ECHO) 'Installing header files'
	$(Q)mkdir -p '$(DESTDIR)$(INCDIR)'
	$(Q)cp -pf include/nogg.h '$(DESTDIR)$(INCDIR)/'

install-pc:
	$(ECHO) 'Installing pkg-config control file'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)/pkgconfig'
	$(Q)sed \
	    -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@INCDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(INCDIR))|g' \
	    -e 's|@LIBDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(LIBDIR))|g' \
	    -e 's|@VERSION@|$(VERSION)|g'\
	    <$(PACKAGE).pc.in >'$(DESTDIR)$(LIBDIR)/pkgconfig/$(PACKAGE).pc'

install-shared: all-shared
	$(ECHO) 'Installing shared library'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)'
	$(Q)cp -pf $(SHARED_LIB) '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB_FULLNAME)'
	$(Q)$(if $(SHARED_LIB_LINKNAME),ln -sf '$(SHARED_LIB_FULLNAME)' '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB_LINKNAME)')
	$(Q)$(if $(filter-out $(SHARED_LIB),$(SHARED_LIB_FULLNAME)),ln -sf '$(SHARED_LIB_FULLNAME)' '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB)')

install-static: all-static
	$(ECHO) 'Installing static library'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)'
	$(Q)cp -pf $(STATIC_LIB) '$(DESTDIR)$(LIBDIR)/'

install-tools: all-tools
	$(ECHO) 'Installing tool programs'
	$(Q)mkdir -p '$(DESTDIR)$(BINDIR)'
	$(Q)cp -pf $(TOOL_BINS) '$(DESTDIR)$(BINDIR)/'


test: $(TEST_BINS)
	$(ECHO) 'Running tests'
	$(Q)ok=0 ng=0; \
	    for test in $^; do \
	        $(call if-true,V,echo "+ $${test}";) \
	        if $${test}; then \
	            ok=`expr $${ok} + 1`; \
	        else \
	            ng=`expr $${ng} + 1`; \
	        fi; \
	    done; \
	    if test $${ng} = 0; then \
	        echo 'All tests passed.'; \
	    else \
	        if test $${ok} = 1; then ok_s=''; else ok_s='s'; fi; \
	        if test $${ng} = 1; then ng_s=''; else ng_s='s'; fi; \
	        echo "$${ok} test$${ok_s} passed, $${ng} test$${ng_s} failed."; \
	        exit 1; \
	    fi

coverage: tests/coverage
	$(ECHO) 'Running tests'
	$(Q)find src -name \*.gcda -exec rm '{}' +
	$(Q)tests/coverage $(call if-true,V,-v)
	$(ECHO) 'Collecting coverage information'
	$(Q)rm -rf .covtmp
	$(Q)mkdir .covtmp
	$(Q)ln -s ../src .covtmp/
	$(Q)set -e; cd .covtmp && for f in $(LIBRARY_SOURCES); do \
	    $(call if-true,V,echo + $(GCOV) $(GCOV_OPTS) $(call GCOV_FILE_OPTS,$$f) $(GCOV_STDOUT);) \
	    $(GCOV) $(GCOV_OPTS) $(call GCOV_FILE_OPTS,$$f) $(GCOV_STDOUT); \
	done
	$(Q)rm -f .covtmp/src
	$(Q)cat .covtmp/* >coverage.txt
	$(Q)rm -rf .covtmp
	$(ECHO) 'Coverage results written to $(abspath coverage.txt)'


clean:
	$(ECHO) 'Removing object and dependency files'
	$(Q)find src tests tools \( -name '*.[do]' -o -name \*.d.tmp \) -exec rm '{}' +
	$(Q)rm -f tests/coverage-tests.h
	$(ECHO) 'Removing test executables'
	$(Q)rm -f $(TEST_BINS) tests/coverage
	$(ECHO) 'Removing coverage data files'
	$(Q)find src tests tools \( -name \*.gcda -o -name \*.gcno \) -exec rm '{}' +
	$(Q)rm -rf .covtmp

spotless: clean
	$(ECHO) 'Removing executable and library files'
	$(Q)rm -f $(SHARED_LIB) $(STATIC_LIB) $(TOOL_BINS)
	$(ECHO) 'Removing coverage output'
	$(Q)rm -f coverage.txt

#-------------------------- Library build rules --------------------------#

$(SHARED_LIB): $(LIBRARY_OBJECTS:%.o=%_so.o)
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(SHARED_LIB_LDFLAGS) -o '$@' $^

$(STATIC_LIB): $(LIBRARY_OBJECTS)
	$(ECHO) 'Archiving $@'
	$(Q)$(AR) rcu '$@' $^
	$(Q)$(RANLIB) '$@'

#--------------------------- Tool build rules ----------------------------#

ifneq ($(filter 1,$(BUILD_SHARED) $(BUILD_STATIC)),)

$(TOOL_BINS) : %: tools/%.o $(call if-true,BUILD_SHARED,$(SHARED_LIB),$(STATIC_LIB))
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(LDFLAGS) -o '$@' $^ $(TOOL_LIBS) $(LIBS)

else

$(TOOL_BINS):
	$(error Cannot build the tool programs without building the library)

endif

$(TOOL_BINS): BASE_CFLAGS += -Iinclude

nogg-benchmark: TOOL_LIBS += $(or \
    $(shell pkg-config --libs vorbisfile ogg 2>/dev/null), \
    -lvorbisfile -lvorbis -logg)
tools/nogg-benchmark.o: BASE_CFLAGS += \
    $(shell pkg-config --cflags vorbisfile 2>/dev/null) \
    $(if $(TREMOR_SOURCE),-DHAVE_TREMOR -I'$(TREMOR_SOURCE)')

ifneq ($(TREMOR_SOURCE),)
nogg-benchmark: tools/nogg-benchmark.o $(call if-true,BUILD_SHARED,$(SHARED_LIB),$(STATIC_LIB)) \
    $(patsubst $(TREMOR_SOURCE)/%.c,tools/tremor-%.o,$(filter-out %_example.c,$(wildcard $(TREMOR_SOURCE)/*.c)))
tools/tremor-%.o: $(TREMOR_SOURCE)/%.c $(wildcard $(TREMOR_SOURCE)/*.h) tools/tremor-wrapper.h
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) -o '$@' -c '$<'
tools/tremor-%.o: BASE_CFLAGS := \
    $(filter-out -W%,$(subst -std=c99,-std=gnu99,$(BASE_CFLAGS))) \
    -include tools/tremor-wrapper.h
endif

#--------------------------- Test build rules ----------------------------#

$(TEST_BINS) : %: %.o $(STATIC_LIB)
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(LDFLAGS) -o '$@' $^ $(LIBS)

tests/coverage: tests/coverage-main.o $(LIBRARY_OBJECTS:%.o=%_cov.o) $(TEST_SOURCES:%.c=%_cov.o)
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o '$@' $^ $(LIBS) --coverage

tests/coverage-main.o: tests/coverage-tests.h

tests/coverage-tests.h: $(TEST_SOURCES)
	$(ECHO) 'Generating $@'
	$(Q)( \
	    for file in $(TEST_SOURCES:%.c=%); do \
	        file_mangled=_`echo "$${file}" | sed -e 's/[^A-Za-z0-9]/_/g'`; \
	        echo "TEST($${file_mangled})"; \
	    done \
	) >'$@'

#----------------------- Common compilation rules ------------------------#

%.o: %.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) -MMD -MF '$(@:%.o=%.d.tmp)' -o '$@' -c '$<'
	$(call filter-deps,$@,$(@:%.o=%.d))

# We generate separate dependency files for shared objects even though the
# contents are the same as for static objects to avoid parallel builds
# colliding when writing the dependencies.
%_so.o: BASE_CFLAGS += $(SHARED_LIB_CFLAGS)
%_so.o: %.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) -MMD -MF '$(@:%.o=%.d.tmp)' -o '$@' -c '$<'
	$(call filter-deps,$@,$(@:%.o=%.d))

src/%_cov.o: BASE_CFLAGS += -O0
src/%_cov.o: src/%.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) --coverage -MMD -MF '$(@:%.o=%.d.tmp)' -o '$@' -c '$<'
	$(call filter-deps,$@,$(@:%.o=%.d))

tests/%_cov.o: tests/%.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)file_mangled=_`echo '$(<:%.c=%)' | sed -e 's/[^A-Za-z0-9]/_/g'`; \
	    $(CC) $(ALL_CFLAGS) "-Dmain=$${file_mangled}" \
	        $(if $(filter -Wmissing-declarations,$(ALL_CFLAGS)),-Wno-missing-declarations) \
	        -MMD -MF '$(@:%.o=%.d.tmp)' -o '$@' -c '$<'
	$(call filter-deps,$@,$(@:%.o=%.d))

#-------------------- Autogenerated dependency magic ---------------------#

define filter-deps
$(Q)test \! -f '$2.tmp' && rm -f '$2' || sed \
    -e ':1' \
    -e 's#\(\\ \|[^ /]\)\+/\.\./##' \
    -e 't1' \
    -e ':2' \
    -e 's#/\./#/#' \
    -e 't2' \
    -e 's#^\(\([^ 	]*[ 	]\)*\)$(subst .,\.,$(1:%$(OBJECT_EXT)=%.h))#\1#' \
    -e 's#$(subst .,\.,$1)#$1 $(1:%$(OBJECT_EXT)=%.h)#' \
    <'$2.tmp' >'$2~'
$(Q)rm -f '$2.tmp'
$(Q)mv '$2~' '$2'
endef

# Don't try to include dependency data if we're not actually building
# anything.  This is particularly important for "clean" and "spotless",
# since an out-of-date dependency file which referenced a nonexistent
# target (this can arise from switching versions in place using version
# control, for example) would otherwise block these targets from running
# and cleaning out that very dependency file!
ifneq ($(filter-out clean spotless,$(or $(MAKECMDGOALS),default)),)
include $(sort $(wildcard $(patsubst %.o,%.d,\
    $(LIBRARY_OBJECTS) \
    $(LIBRARY_OBJECTS:%.o=%_so.o) \
    $(LIBRARY_OBJECTS:%.o=%_cov.o) \
    $(TEST_OBJECTS) \
    $(TEST_OBJECTS:%.o=%_cov.o) \
)))
endif

###########################################################################
