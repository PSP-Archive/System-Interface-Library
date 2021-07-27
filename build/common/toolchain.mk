#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/common/toolchain.mk: Toolchain configuration for the SIL build system.
#

#
# This file attempts to determine the toolchain in use and set up the
# compiler variables required by the rest of the build system.
#
# On input, the following variables should be set (typically by the
# system-specific build.mk file):
#
#    $(CC):  C/Objective-C/assembly compiler command.  This should be a
#       single word, the name of the program to execute.
#
#    $(CXX):  C++ compiler command.  This is optional; if not set, it will
#       be determined from $(CC).
#
#    $(AR_RC), $(RANLIB):  Commands for working with object archives, as
#       used by rules.mk.  These are optional; if not set, they weill be
#       determined from $(CC) assuming a Unix-like system.
#
#    $(SYS_CFLAGS), $(SYS_CXXFLAGS), $(SYS_OBJCFLAGS), $(SYS_ASFLAGS),
#    $(SYS_LDFLAGS):  System-specific flags for each compiler/linker mode
#       (respectively: C compiler, C++ compiler, Objective-C compiler,
#       assembler, linker). These will be appended to the end of the common
#       flag list on the compile or link command line.  These may be empty
#       if no system-specific flags are required.
#
#    $(OBJECT_EXT), $(LIBRARY_EXT):  These should be defined as required
#       for rules.mk, but they are not used directly by this file.
#
# On output, the following variables will be set:
#
#    $(CC_TYPE):  A single word indicating the compiler type.  This will
#       be one of:
#          clang (Clang compiler)
#          gcc   (GCC or compatible compilers)
#          icc   (Intel C++ compiler)
#       If the compiler type is not recognized, "gcc" will be assumed.
#
#    $(CC_VERSION):  A single word indicating the compiler version, or
#       nothing if the compiler version cannot be determined.
#
#    $(CC_VERSION_MAJOR), $(CC_VERSION_MINOR):  The first and second
#       dot-separated parts of $(CC_VERSION), if any.
#
#    $(CC_ARCH):  The target architecture of the compiler, if known.
#       Currently one of: arm arm64 mips mips64 x86 x86_64 unknown
#
#    $(CXX):  If not set on entry, this will be set to the C++ compiler
#       command corresponding to $(CC).
#
#    $(BASE_CFLAGS), $(BASE_CXXFLAGS), $(BASE_OBJCFLAGS), $(BASE_ASFLAGS),
#    $(BASE_LDFLAGS):  Base flag set for each compiler/linker mode.
#
#    $(CFLAG_PREPROCESS):  Compiler flag which selects preprocessing mode.
#       The input file(s) will be preprocessed but not compiled or linked.
#
#    $(CFLAG_COMPILE):  Compiler flag which selects preprocessing mode.
#       The input file(s) will be preprocessed and compiled but not linked.
#
#    $(CFLAG_WRITEDEPS):  Compiler flag which causes dependency information
#       for the source file being compiled to be written to a separate
#       file.  Usage: $(CFLAG_WRITEDEPS)<path>
#
#    $(CFLAG_OUTPUT):  Compiler flag which specifies the output file for
#       a preprocess or compile operation.  Usage: $(CFLAG_OUTPUT)<path>
#
#    $(CFLAG_LINK_OUTPUT):  Compiler flag which specifies the output file
#       for a link operation.  Usage: $(CFLAG_LINK_OUTPUT)<path>
#
#    $(CFLAG_DEFINE):  Compiler flag which defines a preprocessor macro.
#       Usage: $(CFLAG_DEFINE)<macro-name>[=<definition>]
#
#    $(CFLAG_INCLUDE_DIR):  Compiler flag which adds a directory to the
#       #include path list.  Usage: $(CFLAG_INCLUDE_DIR)<path>
#
#    $(CFLAG_INCLUDE_FILE):  Compiler flag which directly includes a file,
#       as if the file was #included at the top of the source file being
#       compiled.  Usage: $(CFLAG_INCLUDE_FILE)<path>
#
#    $(CFLAG_STD_C99):  Compiler flag which selects the C99 language
#       standard for compiling C and Objective-C sources.  This flag will
#       include support for C11-style anonymous unions.  All SIL sources
#       are built with this flag; you can also include it in the CFLAGS or
#       OBJCFLAGS settings for your own modules.
#
#    $(CFLAG_STD_CXX11):  Compiler flag which selects the C++11 language
#       standard for compiling C++ sources.  All SIL sources are built with
#       this flag; you can also include it in the CXXFLAGS setting for your
#       own modules.
#
#    $(WARNING_CFLAGS), $(WARNING_CXXFLAGS), $(WARNING_OBJCFLAGS):
#       Compiler flags enabling a standard set of warnings for C, C++, and
#       Objective-C respectively.  These are automatically included in the
#       base flag set for compilation.
#
#    $(WARNING_CFLAGS_STRICT), $(WARNING_CXXFLAGS_STRICT),
#       $(WARNING_OBJCFLAGS_STRICT):  Compiler flags enabling a stricter
#       set of warnings than the $(WARNING_*FLAGS) definitions.  These can
#       be added to module FLAGS variables as desired.
#
# This file also defines the convenience function CC_VERSION-is-at-least,
# which returns a nonempty (true) value if $(CC_VERSION) is at least the
# given version number.  For example:
#     BASE_CFLAGS += $(if $(call CC_VERSION-is-at-least,7.7),-fancy-new-option)
# Similarly, the convenience function CC_VERSION-is-not-at-least returns a
# true value if $(CC_VERSION) is equal to or less than the given version
# number (or if the compiler version is unknown).  Due to internal Make
# limitations, any component greater than 99 will be treated as 99.
#

###########################################################################
###########################################################################

CC_VERSION-is-at-least = $(call _SIL_version-greater-equal,$(subst ., ,$(CC_VERSION)),$(subst ., ,$1))
CC_VERSION-is-not-at-least = $(if $(call CC_VERSION-is-at-least,$1),,1)

# This is a bit unwieldy since Make doesn't have the concept of arithmetic.
# We manage it by turning numbers into sequences of "x" words; see
# http://www.cmcrossroads.com/article/learning-gnu-make-functions-arithmetic
# for a lengthier example.
_SIL_version-greater-equal = \
    $(or $(call _SIL_number-greater,$(firstword $1),$(firstword $2)), \
         $(and $(filter $(firstword $1),$(firstword $2)),             \
               $(or $(filter 1,$(words $2)),                          \
                    $(and $(filter-out 1,$(words $1)),                \
                          $(call _SIL_version-greater-equal,$(wordlist 2,$(words $1),$1),$(wordlist 2,$(words $2),$2))))))

_SIL_number-greater = \
    $(call _SIL_words-greater,$(call _SIL_encode-number,$1),$(call _SIL_encode-number,$2))

_SIL_words-greater = \
    $(filter-out $(words $2),$(words $(call _SIL_words-max,$1,$2)))

_SIL_words-max = \
    $(subst xx,x,$(join $1,$2))

_SIL_encode-number = $(wordlist 1,$1,$(_SIL_99))

_SIL_99 = $(foreach i,x x x,$(foreach i,x x x,x x x x x x x x x x x))

###########################################################################

# Extract the compiler directory and base command name.  If the base name
# contains any hyphens, take everything up to and including the last hyphen
# as a common prefix to be prepended to other command names.

_SIL_TOOLCHAIN_DIR = $(if $(filter ./%,$(CC)),$(dir $(CC)),$(if $(filter ./,$(dir $(CC))),,$(dir $(CC))))
_SIL_TOOLCHAIN_CC_BASENAME = $(notdir $(CC))
_SIL_TOOLCHAIN_CC_UNVERSIONED = $(shell echo 'x$(_SIL_TOOLCHAIN_CC_BASENAME)' | sed -e 's/^x//' -e 's/-[0-9.]*$$//')
_SIL_TOOLCHAIN_CC_BASE = $(lastword $(subst -,$(preserve-space) $(preserve-space),$(_SIL_TOOLCHAIN_CC_UNVERSIONED)))
_SIL_TOOLCHAIN_PREFIX = $(_SIL_TOOLCHAIN_DIR)$(patsubst %-$(_SIL_TOOLCHAIN_CC_BASE),%-,$(filter %-$(_SIL_TOOLCHAIN_CC_BASE),$(_SIL_TOOLCHAIN_CC_UNVERSIONED)))
_SIL_TOOLCHAIN_SUFFIX = $(patsubst $(_SIL_TOOLCHAIN_CC_BASE)-%,-%,$(filter $(_SIL_TOOLCHAIN_CC_BASE)-%,$(_SIL_TOOLCHAIN_CC_BASENAME)))


# Determine the compiler type.

ifeq ($(_SIL_TOOLCHAIN_CC_BASE),clang)
    CC_TYPE = clang
else ifeq ($(_SIL_TOOLCHAIN_CC_BASE),icc)
    CC_TYPE = icc
else ifeq ($(_SIL_TOOLCHAIN_CC_BASE),gcc)
    CC_TYPE = gcc
else
    _SIL_TOOLCHAIN_CC_VERSION_TEXT := $(shell $(CC) --version 2>&1)
    ifneq (,$(filter clang LLVM,$(_SIL_TOOLCHAIN_CC_VERSION_TEXT)))
        CC_TYPE = clang
    else ifneq (,$(filter gcc,$(_SIL_TOOLCHAIN_CC_VERSION_TEXT)))
        CC_TYPE = gcc
    else ifneq (__GNUC__,$(shell echo __GNUC__ | $(CC) -E - 2>/dev/null | tail -n1))
        # Some vendor distributions of GCC don't include "GCC" anywhere
        # in the --version text, so we have to try harder to guess.
        CC_TYPE = gcc
    else
        $(warning *** Can't determine compiler type, assuming GCC.)
        $(warning *** If this is incorrect, edit: $(lastword $(MAKEFILE_LIST)))
        CC_TYPE = gcc
    endif
endif


# Determine the compiler version.  In order for the CC_VERSION-is-at-least
# function to work, the version number must not contain any non-numeric
# characters, so we filter those out and fall back on a version number of
# 0 if necessary.

ifeq ($(CC_TYPE),clang)
    CC_VERSION := $(shell $(CC) --version 2>&1 | grep '^clang version ' | head -n1 | sed -e 's/^clang version \([^ ]*\).*/\1/' -e 's/[^0-9.]//g')
else ifeq ($(CC_TYPE),gcc)
    # Some vendor versions of GCC add flavor text to the version string in
    # random places, so do our best to get rid of it.  Since we use right
    # parens in the shell command, we need to use ${} instead of $() to
    # call the built-in functions.
    CC_VERSION := ${lastword ${shell $(CC) --version 2>&1 | head -n1 | sed -e 's/ *([^)]*)//g' -e 's/[^0-9. ]//g'}}
else ifeq ($(CC_TYPE),icc)
    CC_VERSION := $(lastword $(shell $(CC) -v 2>&1 | head -n1 | sed -e 's/[^0-9. ]//g'))
endif
CC_VERSION := $(or $(CC_VERSION),0)

CC_VERSION_MAJOR := $(word 1,$(subst ., ,$(CC_VERSION)))
CC_VERSION_MINOR := $(word 2,$(subst ., ,$(CC_VERSION)))


# Set tool pathnames.  We'd normally use ?= here to avoid overriding
# values set by the calling Makefile, but Make provides default values for
# some of these variables, so we have to explicitly check for those and
# override them.

ifeq ($(CC_TYPE),clang)
    _SIL_TOOLCHAIN_CXX = $(_SIL_TOOLCHAIN_PREFIX)clang++$(_SIL_TOOLCHAIN_SUFFIX)
else ifeq ($(CC_TYPE),icc)
    _SIL_TOOLCHAIN_CXX = $(_SIL_TOOLCHAIN_PREFIX)icpc$(_SIL_TOOLCHAIN_SUFFIX)
else ifeq ($(_SIL_TOOLCHAIN_CC_BASE),cc)
    _SIL_TOOLCHAIN_CXX = $(_SIL_TOOLCHAIN_PREFIX)c++$(_SIL_TOOLCHAIN_SUFFIX)
else
    _SIL_TOOLCHAIN_CXX = $(_SIL_TOOLCHAIN_PREFIX)$(patsubst %cc,%,$(_SIL_TOOLCHAIN_CC_BASE))++$(_SIL_TOOLCHAIN_SUFFIX)
endif

ifneq (,$(filter undefined default,$(origin CXX)))
    CXX = $(_SIL_TOOLCHAIN_CXX)
endif

ifneq (,$(filter undefined default,$(origin AR_RC)))
    AR_RC = $(_SIL_TOOLCHAIN_PREFIX)ar rc
endif

ifneq (,$(filter undefined default,$(origin RANLIB)))
    RANLIB = $(_SIL_TOOLCHAIN_PREFIX)ranlib
endif


# Set flag variables used by build rules.

CFLAG_PREPROCESS   = -E
CFLAG_COMPILE      = -c
CFLAG_WRITEDEPS    = -MMD -MF $(preserve-trailing-space)
CFLAG_OUTPUT       = -o
CFLAG_LINK_OUTPUT  = -o
CFLAG_DEFINE       = -D
CFLAG_INCLUDE_DIR  = -I
CFLAG_INCLUDE_FILE = -include $(preserve-trailing-space)
CFLAG_STD_C99      = -std=c99
CFLAG_STD_CXX11    = -std=c++11


# Determine the compiler's target architecture.

CC_ARCH := $(strip \
    $(if $(shell echo __arm__ | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),arm,\
    $(if $(shell echo __aarch64__ | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),arm64,\
    $(if $(shell echo __mips__ | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),mips,\
    $(if $(shell echo __mips64 | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),mips64,\
    $(if $(shell echo __i386__ | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),x86,\
    $(if $(shell echo __x86_64__ | $(CC) $(SYS_CFLAGS) $(CFLAGS) $(CFLAG_PREPROCESS) - 2>/dev/null | grep '^1'),x86_64,\
    unknown)))))))


# Set base compiler/linker flags for all compiler types.

BASE_FLAGS = -O3 -pipe -g \
             $(if $(filter 1,$(WARNINGS_AS_ERRORS)),-Werror)

BASE_CFLAGS = $(BASE_FLAGS) $(WARNING_CFLAGS)

BASE_CXXFLAGS = $(BASE_FLAGS) $(WARNING_CXXFLAGS) \
                -fsigned-char \
                $(if $(filter 1,$(SIL_ENABLE_CXX_EXCEPTIONS)),-fexceptions,-fno-exceptions)

BASE_OBJCFLAGS = $(BASE_FLAGS) $(WARNING_OBJCFLAGS)

BASE_ASFLAGS = # no flags

BASE_LDFLAGS = $(if $(filter 1,$(SIL_ENABLE_CXX_EXCEPTIONS)),-fexceptions,-fno-exceptions)

WARNING_FLAGS = -Wall -Wpointer-arith -Wshadow
WARNING_CFLAGS = $(WARNING_FLAGS) -Wmissing-declarations -Wstrict-prototypes \
                 -Wwrite-strings
WARNING_CXXFLAGS = $(WARNING_FLAGS)
WARNING_OBJCFLAGS = $(WARNING_CFLAGS)

WARNING_FLAGS_STRICT = -Wextra -Wcast-align -Winit-self -Wundef \
                       -Wunused-parameter
WARNING_CFLAGS_STRICT = $(WARNING_FLAGS_STRICT)
WARNING_CXXFLAGS_STRICT = $(WARNING_FLAGS_STRICT)
WARNING_OBJCFLAGS_STRICT = $(WARNING_CFLAGS_STRICT)


# Append compiler-specific flags.

ifeq ($(CC_TYPE),gcc)

    # Block known-bad versions (GCC 4.3 is the first version to support
    # C99 inline functions).
    ifeq ($(call CC_VERSION-is-at-least,4.3),)
        $(error GCC version ($(CC_VERSION)) too old; GCC 4.3 or later required)
    endif

    # When not building verbosely, suppress everything except the actual
    # diagnostic.
    BASE_FLAGS += $(if $(filter 1,$(V)),,-fno-diagnostics-show-option)

    BASE_FLAGS += -fno-strict-overflow
    WARNING_FLAGS_STRICT += -Wvla
    BASE_LDFLAGS += $(if $(filter 1,$(SIL_ENABLE_CXX_EXCEPTIONS)),,\
        -static-libgcc \
        $(if $(call CC_VERSION-is-at-least,4.5),-static-libstdc++))

    ifneq ($(call CC_VERSION-is-at-least,4.5),)
        WARNING_FLAGS_STRICT += -Wlogical-op
    endif

    ifneq ($(call CC_VERSION-is-at-least,4.7),)
        WARNING_FLAGS += -Wunused-local-typedefs
        # GCC 4.7 and later versions report numerous "may be uninitialized"
        # warnings.  These are widely[1][2][3] considered useless and ignored
        # due to a high false-positive rate.
        #    [1] http://lkml.org/lkml/2013/3/25/347
        #    [2] https://bugs.webkit.org/show_bug.cgi?id=119835
        #    [3] http://gcc.gnu.org/bugzilla/show_bug.cgi?id=55644
        # As with other projects, we treat these warnings as useless and
        # disable them.  The Clang compiler seems to do a better job of
        # accurately reporting uninitialized-data errors.
        BASE_FLAGS += -Wno-maybe-uninitialized
    else
        CFLAG_STD_CXX11 = -std=c++0x
        # Old versions of GCC require -std=gnu99 for C11-style anonymous
        # union support.
        CFLAG_STD_C99 = -std=gnu99
    endif

    ifneq ($(call CC_VERSION-is-at-least,4.8),)
        BASE_FLAGS += $(if $(filter 1,$(V)),,-fno-diagnostics-show-caret)
    endif

else ifeq ($(CC_TYPE),clang)

    BASE_FLAGS += $(if $(filter 1,$(V)),,-fno-diagnostics-show-option -fno-caret-diagnostics)
    WARNING_FLAGS_STRICT += -Wvla
    ifeq ($(call CC_VERSION-is-at-least,3.2),)
        CFLAG_STD_C99 = -std=gnu99
    endif

endif


# Append architecture-specific flags.

ifeq ($(CC_ARCH),x86)
    BASE_FLAGS += -msse -msse2 $(if $(filter clang,$(CC_TYPE)),,-mfpmath=sse)
endif


# Append system-specific flags.

BASE_CFLAGS    += $(SYS_CFLAGS)
BASE_CXXFLAGS  += $(SYS_CXXFLAGS)
BASE_OBJCFLAGS += $(SYS_OBJCFLAGS)
BASE_ASFLAGS   += $(SYS_ASFLAGS)
BASE_LDFLAGS   += $(SYS_LDFLAGS)


# Define flags and tools used for coverage analysis.

ifeq ($(CC_TYPE),gcc)
    COVERAGE_CFLAGS = -fprofile-arcs -ftest-coverage -O0
    COVERAGE_LIBS = -lgcov
    ifneq (,$(filter undefined default,$(origin GCOV)))
        GCOV = $(_SIL_TOOLCHAIN_PREFIX)gcov
        GCOV_OPTS = -b -c -l -p
        GCOV_FILE_OPTS = -o "../$(OBJDIR)/`dirname \"$1\"`" "$1"
        GCOV_STDOUT = >/dev/null
    endif

else ifeq ($(CC_TYPE),clang)
    COVERAGE_CFLAGS = -fprofile-arcs -ftest-coverage -O0
    # The library name for Clang varies with the architecture.
    COVERAGE_LIBS = --coverage
    ifneq (,$(filter undefined default,$(origin GCOV)))
        GCOV = $(_SIL_TOOLCHAIN_PREFIX)llvm-cov
        GCOV_OPTS = gcov -b -c -l -p
        GCOV_FILE_OPTS = \
            -gcno="../$(OBJDIR)/`echo \"$1\" | sed -e 's|\.[^./]*$$|.gcno|'`" \
            -gcda="../$(OBJDIR)/`echo \"$1\" | sed -e 's|\.[^./]*$$|.gcda|'`" \
            "$1"
        GCOV_STDOUT = >/dev/null
    endif

else
    COVERAGE_CFLAGS = $(warning Warning: Coverage flags unknown for this compiler.)
    COVERAGE_LIBS = $(warning Warning: Coverage flags unknown for this compiler.)
    GCOV = : $(error Coverage tool unknown for this compiler)
endif

COVERAGE_CXXFLAGS = $(COVERAGE_CFLAGS)
COVERAGE_OBJCFLAGS = $(COVERAGE_CFLAGS)
COVERAGE_ASFLAGS =

###########################################################################
###########################################################################
