#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# tools/Makefile: Build control file for tool programs.
#

#
# This tools in this directory can be built either by calling this Makefile
# directly, in which case the executable files are placed in the directory
# specified by $(DESTDIR) (defaulting to the current directory), or by
# referencing an executable target in the tools/ subdirectory of a build
# directory which uses the SIL build system (for example, tools/pngtotex),
# in which case the referenced executable will be built in that location.
#
# Standalone builds use the CC variable to determine the compiler as usual.
# When building from within the SIL build system, SIL chooses the compiler
# for tool programs using the following heuristic:
#
# - If the HOST_CC variable is set, its value is used as the compiler.
#   In this case, HOST_CFLAG_DEFINE, HOST_CFLAG_INCLUDE_DIR, and
#   HOST_CFLAG_LINK_OUTPUT should also be set if the relevant flags are
#   different from the respective defaults "-D", "-I", and "-o".
#
# - Otherwise, if a test program compiled with the target compiler ($(CC))
#   can be executed in the host environment, the target compiler is used.
#
# - Otherwise, "cc" is used, with flags "-D", "-I", and "-o" as described
#   above.
#
# You can extend this Makefile by defining SIL_EXTRA_TOOLS before including
# this file (for a direct build) or build/*/build.mk (when using the SIL
# build system).  For each target (which must be a plain filename with no
# directory components), define the following variables, where <tool> is
# the name of the tool:
#
# - SIL_TOOL_<tool>_SOURCES: Source filenames for the tool.  Only C and
#   and assembly source files are supported.
#
# - SIL_TOOL_<tool>_DEPS: Dependencies (such as header files) for the
#   tool's source files.
#
# - SIL_TOOL_<tool>_EXTRA_OBJS: Additional object files required when
#   linking the tool.  These can be true object files with special build
#   rules, libraries which cannot be listed in SIL_TOOL_<tool>_LIBS for
#   some reason, or any other files required on the link command line.
#   Make sure to include a build rule for any such file which is generated.
#
# - SIL_TOOL_<tool>_CFLAGS: Additional command-line flags required when
#   compiling the tool's source files.
#
# - SIL_TOOL_<tool>_LIBS: Libraries required when linking the tool.  The
#   entries should be base library names, with no linker option or filename
#   prefix/suffix (for example, the "libm.a" or "libm.lib" library which
#   typically holds mathematical routines should be listead as just "m").
#
# - SIL_TOOL_<tool>_LDFLAGS: Additional command-line flags required when
#   linking the tool.  These are passed to the compiler frontend, so be
#   sure to format them appropriately (for example, "-Wl,-option" instead
#   of just "-option" for GCC-style frontends).
#

###########################################################################
###########################################################################

_SIL_TOOLS_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

include $(_SIL_TOOLS_DIR)/../build/common/base.mk

#-------------------------------------------------------------------------#

# Toolchain setup.

_SIL_TOOLS_OBJDIR = $(patsubst $(TOPDIR)%,$(OBJDIR)%,$(_SIL_TOOLS_DIR))

ifeq ($(_SIL_BUILD),1)

    ifneq (,$(HOST_CC))
        _SIL_TOOL_CC = $(HOST_CC)
        _SIL_TOOL_CFLAG_DEFINE = $(or $(HOST_CFLAG_DEFINE),-D)
        _SIL_TOOL_CFLAG_INCLUDE_DIR = $(or $(HOST_CFLAG_INCLUDE_DIR),-I)
        _SIL_TOOL_CFLAG_LINK_OUTPUT = $(or $(HOST_CFLAG_LINK_OUTPUT),-o)
    else
        _ := $(shell \
            set -e; \
            mkdir -p '$(_SIL_TOOLS_OBJDIR)'; \
            echo 'int main(void) {return 0;}' >'$(_SIL_TOOLS_OBJDIR)/tooltest.c'; \
            $(CC) '$(_SIL_TOOLS_OBJDIR)/tooltest.c' $(CFLAG_LINK_OUTPUT)'$(_SIL_TOOLS_OBJDIR)/tooltest'; \
            '$(_SIL_TOOLS_OBJDIR)/tooltest'; \
            echo ok)
        ifeq ($_,ok)
            _SIL_TOOL_CC = $(CC)
            _SIL_TOOL_CC_ARCH = $(CC_ARCH)
            _SIL_TOOL_CFLAG_DEFINE = $(CFLAG_DEFINE)
            _SIL_TOOL_CFLAG_INCLUDE_DIR = $(CFLAG_INCLUDE_DIR)
            _SIL_TOOL_CFLAG_LINK_OUTPUT = $(CFLAG_LINK_OUTPUT)
        else
            _SIL_TOOL_CC = cc
            _SIL_TOOL_CC_ARCH = unknown
            _SIL_TOOL_CFLAG_DEFINE = -D
            _SIL_TOOL_CFLAG_INCLUDE_DIR = -I
            _SIL_TOOL_CFLAG_LINK_OUTPUT = -o
        endif
        _ := $(shell rm -f '$(_SIL_TOOLS_OBJDIR)'/tooltest*; rmdir '$(_SIL_TOOLS_OBJDIR)' 2>/dev/null)
    endif

    _SIL_TOOL_CC_ARCH = unknown

else  # !_SIL_BUILD

    _SIL_TOOLS_OBJDIR = .
    CC ?= cc
    include $(_SIL_TOOLS_DIR)/../build/common/toolchain.mk
    _SIL_TOOL_CC = $(CC)
    _SIL_TOOL_CC_ARCH = $(CC_ARCH)
    _SIL_TOOL_CFLAG_DEFINE = $(CFLAG_DEFINE)
    _SIL_TOOL_CFLAG_INCLUDE_DIR = $(CFLAG_INCLUDE_DIR)
    _SIL_TOOL_CFLAG_LINK_OUTPUT = $(CFLAG_LINK_OUTPUT)

endif

_SIL_TOOL_EXEC_EXT =
_ := $(shell \
    set -e; \
    mkdir -p '$(_SIL_TOOLS_OBJDIR)'; \
    echo 'int main(void) {return 0;}' >'$(_SIL_TOOLS_OBJDIR)/tooltest.c'; \
    $(CC) '$(_SIL_TOOLS_OBJDIR)/tooltest.c' $(_SIL_TOOL_CFLAG_LINK_OUTPUT)'$(_SIL_TOOLS_OBJDIR)/tooltest'; \
    echo ok)
ifeq ($_,ok)
    ifneq (,$(wildcard $(_SIL_TOOLS_OBJDIR)/tooltest.exe))
        _SIL_TOOL_EXEC_EXT = .exe
    endif
endif
_ := $(shell rm -f '$(_SIL_TOOLS_OBJDIR)'/tooltest*; rmdir '$(_SIL_TOOLS_OBJDIR)' 2>/dev/null)

ifeq ($(_SIL_TOOL_CC_ARCH),unknown)
    _ := $(shell \
        set -e; \
        mkdir -p '$(_SIL_TOOLS_OBJDIR)'; \
        (echo '#if !defined(__i386__) && !defined(__amd64__)'; \
         echo '#error not x86'; \
         echo '#endif') >'$(_SIL_TOOLS_OBJDIR)/tooltest.c'; \
        $(_SIL_TOOL_CC) -c '$(_SIL_TOOLS_OBJDIR)/tooltest.c' >/dev/null 2>&1; \
        echo ok)
    ifeq ($_,ok)
        # We don't differentiate between x86 and x86-64 in this file.
        _SIL_TOOL_CC_ARCH = x86
    endif
    _ := $(shell rm -f '$(_SIL_TOOLS_OBJDIR)'/tooltest*; rmdir '$(_SIL_TOOLS_OBJDIR)' 2>/dev/null)
endif

_SIL_TOOL_COMMON_CFLAGS = $(BASE_CFLAGS) $(_SIL_TOOL_CFLAG_STD_C99) \
                          $(_SIL_TOOL_CFLAG_INCLUDE_DIR)'$(_SIL_TOOLS_DIR)/../include'

ifneq (,$(filter x86%,$(_SIL_TOOL_CC_ARCH)))
    _SIL_TOOL_COMMON_CFLAGS += \
        $(_SIL_TOOL_CFLAG_DEFINE)IS_LITTLE_ENDIAN \
        $(if $(filter gcc clang,$(_SIL_TOOL_CC_TYPE)),-msse2)
else
    _ := $(shell \
        set -e; \
        mkdir -p '$(_SIL_TOOLS_OBJDIR)'; \
        echo 'int main(void) {return *(const short *)"\1";}' >'$(_SIL_TOOLS_OBJDIR)/tooltest.c'; \
        $(CC) '$(_SIL_TOOLS_OBJDIR)/tooltest.c' $(_SIL_TOOL_CFLAG_LINK_OUTPUT)'$(_SIL_TOOLS_OBJDIR)/tooltest'; \
        echo ok)
    ifeq ($_,ok)
        _ := $(shell '$(_SIL_TOOLS_OBJDIR)/tooltest' && echo big)
        ifneq ($_,big)
            _SIL_TOOL_COMMON_CFLAGS += $(_SIL_TOOL_CFLAG_DEFINE)IS_LITTLE_ENDIAN
        endif
    else
        $(error Unable to compile an endianness test program)
    endif
    _ := $(shell rm -f '$(_SIL_TOOLS_OBJDIR)'/tooltest*; rmdir '$(_SIL_TOOLS_OBJDIR)' 2>/dev/null)
endif

# Library flags for various libraries.
LIBS_zlib   = -lz
LIBS_png    = -lpng $(LIBS_zlib)

###########################################################################

# Programs to build and their sources/flags.

_SIL_TOOL_PROGRAMS = $(sort build-pkg extract-pkg makefont pngtotex streamux \
                            $(SIL_EXTRA_TOOLS))

SIL_TOOL_build-pkg_SOURCES   = $(_SIL_TOOLS_DIR)/build-pkg.c
SIL_TOOL_build-pkg_DEPS      = $(_SIL_TOOLS_DIR)/tool-common.h \
                               $(_SIL_TOOLS_DIR)/../src/resource/package-pkg.h
SIL_TOOL_build-pkg_CFLAGS    = $(_SIL_TOOL_COMMON_CFLAGS)
SIL_TOOL_build-pkg_LDFLAGS   =
SIL_TOOL_build-pkg_LIBS      = $(LIBS) $(LIBS_zlib)

SIL_TOOL_extract-pkg_SOURCES = $(_SIL_TOOLS_DIR)/extract-pkg.c
SIL_TOOL_extract-pkg_DEPS    = $(_SIL_TOOLS_DIR)/tool-common.h \
                               $(_SIL_TOOLS_DIR)/../src/resource/package-pkg.h
SIL_TOOL_extract-pkg_CFLAGS  = $(_SIL_TOOL_COMMON_CFLAGS)
SIL_TOOL_extract-pkg_LDFLAGS =
SIL_TOOL_extract-pkg_LIBS    = $(LIBS) $(LIBS_zlib)

SIL_TOOL_makefont_SOURCES    = $(_SIL_TOOLS_DIR)/makefont.c \
                               $(_SIL_TOOLS_DIR)/util.c
SIL_TOOL_makefont_DEPS       = $(_SIL_TOOLS_DIR)/tool-common.h \
                               $(_SIL_TOOLS_DIR)/util.h \
                               $(_SIL_TOOLS_DIR)/../src/utility/font-file.h
SIL_TOOL_makefont_CFLAGS     = $(_SIL_TOOL_COMMON_CFLAGS)
SIL_TOOL_makefont_LDFLAGS    =
SIL_TOOL_makefont_LIBS       = $(LIBS) -lm

SIL_TOOL_pngtotex_SOURCES    = $(_SIL_TOOLS_DIR)/pngtotex.c \
                               $(_SIL_TOOLS_DIR)/quantize.c \
                               $(_SIL_TOOLS_DIR)/zoom.c
SIL_TOOL_pngtotex_DEPS       = $(_SIL_TOOLS_DIR)/tool-common.h \
                               $(_SIL_TOOLS_DIR)/quantize.h \
                               $(_SIL_TOOLS_DIR)/sse2.h \
                               $(_SIL_TOOLS_DIR)/zoom.h \
                               $(_SIL_TOOLS_DIR)/../src/endian.h \
                               $(_SIL_TOOLS_DIR)/../src/utility/tex-file.h
SIL_TOOL_pngtotex_CFLAGS     = $(_SIL_TOOL_COMMON_CFLAGS)
SIL_TOOL_pngtotex_LDFLAGS    =
SIL_TOOL_pngtotex_LIBS       = $(LIBS) $(LIBS_png) -lm

SIL_TOOL_streamux_SOURCES    = $(_SIL_TOOLS_DIR)/streamux.c \
                               $(_SIL_TOOLS_DIR)/util.c
SIL_TOOL_streamux_DEPS       = $(_SIL_TOOLS_DIR)/tool-common.h \
                               $(_SIL_TOOLS_DIR)/util.h
SIL_TOOL_streamux_CFLAGS     = $(_SIL_TOOL_COMMON_CFLAGS)
SIL_TOOL_streamux_LDFLAGS    =
SIL_TOOL_streamux_LIBS       = $(LIBS) -lm

###########################################################################

# Build rules.

ifeq ($(_SIL_BUILD),1)
    _sil_tool_exec_path = tools/$1$(_SIL_TOOL_EXEC_EXT)
else
    DESTDIR ?= .
    _sil_tool_exec_path = $(if $(filter .,$(DESTDIR)),,$(DESTDIR)/)$1$(_SIL_TOOL_EXEC_EXT)
endif

define _SIL_TOOLS_RECIPE_spotless
$(ECHO) 'Removing tool executable files'
$(Q)rm -rf $(if $(filter 1,$(_SIL_BUILD)),tools,$(foreach i,$(_SIL_TOOL_PROGRAMS),'$(call _sil_tool_exec_path,$i)') $(if $(findstring Darwin,$(shell uname -s 2>/dev/null)),$(foreach i,$(PROGRAMS),'$(DESTDIR)/$i.dSYM')))
endef
ifneq ($(_SIL_BUILD),1)
.PHONY: all clean spotless
all: $(foreach i,$(_SIL_TOOL_PROGRAMS),$(call _sil_tool_exec_path,$i))
clean:
	@# Nothing to do.
spotless: clean
	$(_SIL_TOOLS_RECIPE_spotless)
endif

ifeq ($(_SIL_BUILD),1)
    ifneq (,$(_SIL_TOOL_EXEC_EXT))
.PHONY: $(_SIL_TOOL_PROGRAMS)
$(foreach i,$(_SIL_TOOL_PROGRAMS),tools/$i)) : \
tools/%: tools/%$(_SIL_TOOL_EXEC_EXT)
    endif
else
    ifneq ($(call _sil_tool_exec_path,foo),foo)
.PHONY: $(_SIL_TOOL_PROGRAMS)
$(foreach i,$(_SIL_TOOL_PROGRAMS),$(call _sil_tool_exec_path,$i)) : \
%: $(call _sil_tool_exec_path,%)
    endif
endif

# Enable double expansion for the compile rule, so we don't need a
# separate rule for each program.
.SECONDEXPANSION:

$(foreach i,$(_SIL_TOOL_PROGRAMS),$(call _sil_tool_exec_path,$i)) : \
$(call _sil_tool_exec_path,%): \
    $$(SIL_TOOL_$$(patsubst $$(call _sil_tool_exec_path,%),%,$$@)_SOURCES) \
    $$(SIL_TOOL_$$(patsubst $$(call _sil_tool_exec_path,%),%,$$@)_DEPS) \
    $$(SIL_TOOL_$$(patsubst $$(call _sil_tool_exec_path,%),%,$$@)_EXTRA_OBJS) \
    $(MAKEFILE_LIST)
	$(ECHO) 'Building $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(_SIL_TOOL_CC) \
            $(SIL_TOOL_$(patsubst $(call _sil_tool_exec_path,%),%,$@)_CFLAGS) $(CFLAGS) \
	    $(SIL_TOOL_$(patsubst $(call _sil_tool_exec_path,%),%,$@)_SOURCES) \
	    $(SIL_TOOL_$(patsubst $(call _sil_tool_exec_path,%),%,$@)_EXTRA_OBJS) \
	    $(SIL_TOOL_$(patsubst $(call _sil_tool_exec_path,%),%,$@)_LDFLAGS) $(LDFLAGS) \
	    $(SIL_TOOL_$(patsubst $(call _sil_tool_exec_path,%),%,$@)_LIBS) \
	    $(_SIL_TOOL_CFLAG_LINK_OUTPUT)'$@'

###########################################################################
###########################################################################
