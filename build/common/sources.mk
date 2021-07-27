#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/common/sources.mk: Source file definitions for building SIL.
#

#
# ======== Defining modules ========
#
# The SIL build system works on the basis of "modules": independent sets
# of source files which are typically (but not necessarily) located under
# the same directory and which are all built with the same set of compiler
# options, such as an engine component or library.
# This file defines variables describing the contents of each module used
# to build the program.  For the purposes of the SIL build system, a
# "module" is a set of source files all located in (or under) the same
# directory and all built with the same set of compiler options, such as
# an engine component or library.  This file defines only the SIL module
# itself (with variables named $(SIL_*)), but the system is designed to
# simplify the addition of other modules.  For each module named <module>,
# the following variables are required:
#
#    $(<module>_SOURCES): List of source files to be compiled; each file's
#       pathname must begin with $(TOPDIR).  The following filename
#       extensions are recognized:
#       - C: *.c
#       - C++: *.cc, *.cpp
#       - Objective C: *.m
#       - Objective C++: *.mm
#       - Assembly: *.s
#       - Assembly with C preprocessor directives: *.S
#    $(<module>_{C,CXX,OBJC,OBJCXX,AS}FLAGS): Module-specific compiler
#       flags for source files in each language.
#    $(<module>_OBJECTS): Additional object files for this module.  Each
#       file's pathname must begin with $(TOPDIR); however, the actual
#       path of the generated object will begin with "$(OBJDIR)/" (so that,
#       for example, "$(TOPDIR)/FILE.o" would map to "$(OBJDIR)/FILE.o").
#       Each object file must have a Makefile recipe to generate the file.
#    $(<module>_LDFLAGS): Additional linker flags for this module.
#    $(<module>_LIBS): Additional libraries for this module.
#    $(<module>_GENLIB): If not empty, specifies the filename of a library
#       (without filename extension) to be created from the module's object
#       files; the library will be linked into the executable in the order
#       specified by $(MODULES).  If empty or not defined, the module's
#       object files will be linked directly into the executable.  Note
#       that modules using this option must be listed in $(MODULES) in the
#       correct link order (each module must be listed before any other
#       modules it uses symbols from) or link errors will result;
#       consequently, this cannot be used with any modules that have
#       circular dependencies.
#
# The module is not actually built into the program unless the module
# name (<module>) is appended to the $(MODULES) variable.  See rules.mk
# for details.
#
# A module need not include any source files; for example, one could
# define a module whose only purpose is to add a library to the link
# command line, by setting $(<module>_LIBS) and leaving the other
# module-specific variables empty.
#
# Note that the name "ALL" is reserved and must not be used as a module
# name.
#
# ======== Modules defined by this file ========
#
# This file defines the "SIL" module, which encompasses all of the SIL
# source files.  The SIL module is automatically added to $(MODULES).
#
# The following optional variables will be incorporated into the SIL module
# flags if defined (they may be defined or modified after this file is
# included):
#
#    $(SIL_EXTRA_FLAGS): Extra flags to append to the compiler flags for
#       all source code languages when compiling SIL sources.  This can be
#       used to set configuration options in base.h which are not supported
#       natively in the build system, by adding a $(CFLAG_DEFINE) flag for
#       the option; for example: $(CFLAG_DEFINE)SIL_TEST_THREAD_PERFORMANCE
#    $(SIL_EXTRA_{C,CXX,OBJC,AS}FLAGS): Extra flags to append to the compiler
#       flags for specific source code languages when compiling SIL sources.
#
# This file also defines modules for each of the external libraries included
# with SIL.  Those modules are added to $(MODULES) if the appropriate
# configuration flags from config.mk are set.
#
#
# ======== Convenience symbols exported to the caller ========
#
# This file defines the following convenience symbols which may be used by
# the calling Makefile:
#
#    $(SIL_CLIENT_FLAGS): Compiler flags for source files which call SIL
#       code.  These flags allow SIL headers to be included with
#       "#include <SIL/file.h>" and define preprocessor symbols based on
#       the features enabled in the build configuration.
#
#
# ======== Internal details (library users can ignore this section) ========
#
# The following variables must be defined before this file is included:
#
#    $(TOPDIR): Absolute pathname of the top directory of the source code
#       tree for the program, without a trailing slash.  All source files
#       must be located under this directory.
#    $(SIL_SYS_CONFIG_HEADER): Path to the system's configuration header
#       (configs/*.h), relative to $(SIL_DIR)/include/SIL/configs.
#    $(SIL_SYS_SOURCES): List of additional source files to build as part
#       of the SIL module, relative to $(SIL_DIR)/src.
#    $(SIL_SYS_OBJECTS): List of additional object files to build as part
#       of the SIL module, relative to $(SIL_DIR)/src.
#    $(SIL_SYS_FLAGS): Additional compiler flags for SIL sources (not
#       exported to the calling Makefile).
#

###########################################################################
################### Convenience definitions (internal) ####################
###########################################################################

# $(call _if-define,FLAG[,SYMBOL]): Convenience macro to define SYMBOL
# (defaults to FLAG) if $(FLAG) is 1.  Uses the $(CFLAG_DEFINE) macro.
_if-define = $(if $(filter 1,$($1)),$(CFLAG_DEFINE)$(or $2,$1))

# _BUILD_*: Flags indicating whether each optional library should be built.
# A non-empty value is true, empty is false.
_BUILD_freetype = $(filter 11,$(SIL_FONT_INCLUDE_FREETYPE)$(SIL_LIBRARY_INTERNAL_FREETYPE))
_BUILD_libnogg = $(filter 11,$(SIL_SOUND_INCLUDE_OGG)$(SIL_LIBRARY_INTERNAL_LIBNOGG))
_BUILD_libpng = $(filter 11,$(SIL_UTILITY_INCLUDE_PNG)$(SIL_LIBRARY_INTERNAL_LIBPNG))
_BUILD_libvpx = $(filter 11,$(SIL_MOVIE_INCLUDE_WEBM)$(SIL_LIBRARY_INTERNAL_LIBVPX))
_BUILD_libwebmdec = $(filter 11,$(SIL_MOVIE_INCLUDE_WEBM)$(SIL_LIBRARY_INTERNAL_LIBWEBMDEC))
_BUILD_nestegg = $(filter 11,$(SIL_MOVIE_INCLUDE_WEBM)$(SIL_LIBRARY_INTERNAL_NESTEGG))
_BUILD_zlib = $(filter 11,$(SIL_UTILITY_INCLUDE_ZLIB)$(SIL_LIBRARY_INTERNAL_ZLIB))

# _SIL_HAS_NEON: True (nonempty) if the target platform is an ARM
# architecture supporting the NEON instructions.
_SIL_HAS_NEON = $(or $(filter arm64,$(CC_ARCH)),$(filter -mfpu=neon,$(BASE_CFLAGS)))

###########################################################################
############################### Module list ###############################
###########################################################################

# We could update $(MODULES) at each module's definition, but since we use
# GENLIB to generate static libraries for submodules, we have to make sure
# to list them in the proper linking order.

MODULES += SIL \
    $(if $(_BUILD_freetype),SIL_freetype) \
    $(if $(_BUILD_libwebmdec),SIL_libwebmdec) \
    $(if $(_BUILD_nestegg),SIL_nestegg) \
    $(if $(_BUILD_libwebmdec),SIL_libvpx) \
    $(if $(_BUILD_libnogg),SIL_libnogg) \
    $(if $(_BUILD_libpng),SIL_libpng) \
    $(if $(_BUILD_zlib),SIL_zlib) \

###########################################################################
########################### Module definitions ############################
###########################################################################

# The $(<module>_DIR) variable is not used by the build system directly;
# we define it as a convenience for listing source files.  $(SIL_DIR) in
# particular is also used by rules.mk and system-specific build.mk files.
#
# We assume that $(abspath ...) will give us a path beginning with
# $(TOPDIR); things may break if this is not the case (if SIL is not
# located under $(TOPDIR), for example), so we abort in such cases.

SIL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)
ifeq ($(filter $(TOPDIR)/%,$(SIL_DIR)/.),)
define _error
Error:
    Path configuration error: SIL is not located under $$(TOPDIR)!
        $$(TOPDIR): $(TOPDIR)
         SIL root: $(SIL_DIR)
*** Path configuration error, cannot continue
endef
$(error $(_error))
endif

SIL_SOURCES = $(SIL_DIR)/src/debug.c \
              $(SIL_DIR)/src/font/bitmap.c \
              $(SIL_DIR)/src/font/core.c \
              $(SIL_DIR)/src/font/freetype.c \
              $(SIL_DIR)/src/font/sysfont.c \
              $(SIL_DIR)/src/graphics/base.c \
              $(SIL_DIR)/src/graphics/framebuffer.c \
              $(SIL_DIR)/src/graphics/primitive.c \
              $(SIL_DIR)/src/graphics/shader.c \
              $(SIL_DIR)/src/graphics/state.c \
              $(SIL_DIR)/src/graphics/texture.c \
              $(SIL_DIR)/src/input.c \
              $(SIL_DIR)/src/main.c \
              $(SIL_DIR)/src/math/dtrig.c \
              $(SIL_DIR)/src/math/fpu.c \
              $(SIL_DIR)/src/math/matrix.c \
              $(SIL_DIR)/src/memory.c \
              $(SIL_DIR)/src/movie/core.c \
              $(SIL_DIR)/src/movie/webm.c \
              $(SIL_DIR)/src/random.c \
              $(SIL_DIR)/src/resource/core.c \
              $(SIL_DIR)/src/resource/package-pkg.c \
              $(SIL_DIR)/src/sound/core.c \
              $(SIL_DIR)/src/sound/decode.c \
              $(SIL_DIR)/src/sound/decode-ogg.c \
              $(SIL_DIR)/src/sound/decode-wav.c \
              $(SIL_DIR)/src/sound/filter.c \
              $(SIL_DIR)/src/sound/filter-flange.c \
              $(SIL_DIR)/src/sound/mixer.c \
              $(SIL_DIR)/src/test/test-harness.c \
              $(SIL_DIR)/src/thread.c \
              $(SIL_DIR)/src/time.c \
              $(SIL_DIR)/src/userdata.c \
              $(SIL_DIR)/src/utility/compress.c \
              $(SIL_DIR)/src/utility/dds.c \
              $(SIL_DIR)/src/utility/font-file.c \
              $(SIL_DIR)/src/utility/id-array.c \
              $(SIL_DIR)/src/utility/log.c \
              $(SIL_DIR)/src/utility/memory.c \
              $(SIL_DIR)/src/utility/misc.c \
              $(SIL_DIR)/src/utility/pixformat.c \
              $(SIL_DIR)/src/utility/png.c \
              $(SIL_DIR)/src/utility/strdup.c \
              $(SIL_DIR)/src/utility/strformat.c \
              $(SIL_DIR)/src/utility/stricmp.c \
              $(SIL_DIR)/src/utility/strtof.c \
              $(SIL_DIR)/src/utility/tex-file.c \
              $(SIL_DIR)/src/utility/tinflate.c \
              $(SIL_DIR)/src/utility/utf8.c \
              $(SIL_DIR)/src/utility/yuv2rgb.c \
              $(SIL_DIR)/src/utility/zlib.c \
              $(SIL_DIR)/src/workqueue.c \
              $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                  $(SIL_DIR)/src/sysdep/test/debug.c \
                  $(SIL_DIR)/src/sysdep/test/input.c \
                  $(SIL_DIR)/src/sysdep/test/misc.c \
                  $(SIL_DIR)/src/sysdep/test/sound.c \
                  $(SIL_DIR)/src/sysdep/test/time.c \
                  $(SIL_DIR)/src/sysdep/test/userdata.c \
                  $(SIL_DIR)/src/test/condvar.c \
                  $(SIL_DIR)/src/test/debug.c \
                  $(SIL_DIR)/src/test/endian.c \
                  $(SIL_DIR)/src/test/font/bitmap.c \
                  $(SIL_DIR)/src/test/font/core.c \
                  $(SIL_DIR)/src/test/font/freetype.c \
                  $(SIL_DIR)/src/test/font/internal.c \
                  $(SIL_DIR)/src/test/font/sysfont.c \
                  $(SIL_DIR)/src/test/graphics/base.c \
                  $(SIL_DIR)/src/test/graphics/clear-depth.c \
                  $(SIL_DIR)/src/test/graphics/clear-grab.c \
                  $(SIL_DIR)/src/test/graphics/framebuffer.c \
                  $(SIL_DIR)/src/test/graphics/internal.c \
                  $(SIL_DIR)/src/test/graphics/misc.c \
                  $(SIL_DIR)/src/test/graphics/primitive.c \
                  $(SIL_DIR)/src/test/graphics/shader-gen.c \
                  $(SIL_DIR)/src/test/graphics/shader-obj.c \
                  $(SIL_DIR)/src/test/graphics/state.c \
                  $(SIL_DIR)/src/test/graphics/texture.c \
                  $(SIL_DIR)/src/test/input.c \
                  $(SIL_DIR)/src/test/main.c \
                  $(SIL_DIR)/src/test/math/dtrig.c \
                  $(SIL_DIR)/src/test/math/internal.c \
                  $(SIL_DIR)/src/test/math/matrix.c \
                  $(SIL_DIR)/src/test/math/matrix-cxx.cc \
                  $(SIL_DIR)/src/test/math/rounding.c \
                  $(SIL_DIR)/src/test/math/vector.c \
                  $(SIL_DIR)/src/test/math/vector-cxx.cc \
                  $(SIL_DIR)/src/test/memory.c \
                  $(SIL_DIR)/src/test/movie/core.c \
                  $(SIL_DIR)/src/test/movie/internal.c \
                  $(SIL_DIR)/src/test/movie/webm.c \
                  $(SIL_DIR)/src/test/mutex.c \
                  $(SIL_DIR)/src/test/random.c \
                  $(SIL_DIR)/src/test/resource/core.c \
                  $(SIL_DIR)/src/test/resource/package-pkg.c \
                  $(SIL_DIR)/src/test/semaphore.c \
                  $(SIL_DIR)/src/test/sound/core.c \
                  $(SIL_DIR)/src/test/sound/decode.c \
                  $(SIL_DIR)/src/test/sound/decode-ogg.c \
                  $(SIL_DIR)/src/test/sound/decode-wav.c \
                  $(SIL_DIR)/src/test/sound/filter.c \
                  $(SIL_DIR)/src/test/sound/filter-flange.c \
                  $(SIL_DIR)/src/test/sound/mixer.c \
                  $(SIL_DIR)/src/test/sound/wavegen.c \
                  $(SIL_DIR)/src/test/sysdep/debug.c \
                  $(SIL_DIR)/src/test/sysdep/files.c \
                  $(SIL_DIR)/src/test/sysdep/log.c \
                  $(SIL_DIR)/src/test/test-logger.c \
                  $(SIL_DIR)/src/test/test-utils.c \
                  $(SIL_DIR)/src/test/thread.c \
                  $(SIL_DIR)/src/test/time.c \
                  $(SIL_DIR)/src/test/userdata.c \
                  $(SIL_DIR)/src/test/utility/compress.c \
                  $(SIL_DIR)/src/test/utility/dds.c \
                  $(SIL_DIR)/src/test/utility/font-file.c \
                  $(SIL_DIR)/src/test/utility/id-array.c \
                  $(SIL_DIR)/src/test/utility/log.c \
                  $(SIL_DIR)/src/test/utility/memory.c \
                  $(SIL_DIR)/src/test/utility/misc.c \
                  $(SIL_DIR)/src/test/utility/pixformat.c \
                  $(SIL_DIR)/src/test/utility/png.c \
                  $(SIL_DIR)/src/test/utility/strdup.c \
                  $(SIL_DIR)/src/test/utility/strformat.c \
                  $(SIL_DIR)/src/test/utility/stricmp.c \
                  $(SIL_DIR)/src/test/utility/strtof.c \
                  $(SIL_DIR)/src/test/utility/tex-file.c \
                  $(SIL_DIR)/src/test/utility/tinflate.c \
                  $(SIL_DIR)/src/test/utility/utf8.c \
                  $(SIL_DIR)/src/test/utility/yuv2rgb.c \
                  $(SIL_DIR)/src/test/utility/zlib.c \
                  $(SIL_DIR)/src/test/workqueue.c \
              ) \
              $(SIL_SYS_SOURCES:%=$(SIL_DIR)/src/%)

SIL_OBJECTS = $(SIL_SYS_OBJECTS:%=$(SIL_DIR)/src/%)

# Define the OpenGL sources here for convenience, since we use them in
# multiple build.mk files.  This is intended to be included in a
# $(SIL_SYS_SOURCES) definition.
SIL_OPENGL_SOURCES = sysdep/opengl/dsa.c \
                     sysdep/opengl/dyngl.c \
                     sysdep/opengl/framebuffer.c \
                     sysdep/opengl/graphics.c \
                     sysdep/opengl/primitive.c \
                     sysdep/opengl/shader.c \
                     sysdep/opengl/shader-common.c \
                     sysdep/opengl/shader-gen.c \
                     sysdep/opengl/shader-table.c \
                     sysdep/opengl/state.c \
                     sysdep/opengl/texture.c \
                     $(if $(filter 1,$(SIL_INCLUDE_TESTS)), \
                         test/sysdep/opengl/features.c \
                         test/sysdep/opengl/framebuffer.c \
                         test/sysdep/opengl/graphics.c \
                         test/sysdep/opengl/internal.c \
                         test/sysdep/opengl/primitive.c \
                         test/sysdep/opengl/shader.c \
                         test/sysdep/opengl/shader-gen.c \
                         test/sysdep/opengl/state.c \
                         test/sysdep/opengl/texture.c \
                         test/sysdep/opengl/version.c \
                     )

# Detect invalid combination of $(DEBUG) and $(SIL_INCLUDE_TESTS).
$(if $(and $(if $(filter 1,$(DEBUG)),,1),$(filter 1,$(SIL_INCLUDE_TESTS))), \
    $(error Cannot enable SIL_INCLUDE_TESTS without DEBUG))

SIL_CLIENT_FLAGS = $(CFLAG_INCLUDE_DIR)'$(SIL_DIR)/include' \
                   $(CFLAG_DEFINE)SIL_SYSTEM_CONFIG_HEADER='"SIL/configs/$(SIL_SYS_CONFIG_HEADER)"' \
                   $(call _if-define,DEBUG) \
                   $(if $(filter 1,$(DEBUG)),$(call _if-define,SIL_INCLUDE_TESTS)) \
                   $(call _if-define,SIL_MATH_ASSUME_ROUND_TO_NEAREST) \
                   $(call _if-define,SIL_MEMORY_FORBID_MALLOC) \
                   $(call _if-define,SIL_USE_STL_ALGORITHM) \
                   $(call _if-define,SIL_STRTOF_CUSTOM,SIL_UTILITY_INCLUDE_STRTOF) \
                   $(SIL_EXTRA_FLAGS)

SIL_FLAGS = $(SIL_CLIENT_FLAGS) \
            $(CFLAG_INCLUDE_DIR)'$(SIL_DIR)' \
            $(CFLAG_INCLUDE_DIR)'$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)' \
            $(call _if-define,SIL_DEBUG_USE_VALGRIND) \
            $(if $(filter 1,$(DEBUG)),$(CFLAG_DEFINE)SIL_DLOG_STRIP_PATH='"$(TOPDIR)/"') \
            $(call _if-define,SIL_FONT_INCLUDE_FREETYPE) \
            $(call _if-define,SIL_MEMORY_CHECK_POINTERS) \
            $(call _if-define,SIL_MEMORY_DEBUG_FILL_UNUSED) \
            $(call _if-define,SIL_MEMORY_LOG_ALLOCS) \
            $(call _if-define,SIL_MOVIE_INCLUDE_WEBM) \
            $(call _if-define,SIL_SOUND_INCLUDE_OGG) \
            $(call _if-define,SIL_STRFORMAT_USE_FLOATS) \
            $(call _if-define,SIL_TEST_VERBOSE_LOGGING) \
            $(call _if-define,SIL_UTILITY_INCLUDE_PNG) \
            $(call _if-define,SIL_UTILITY_INCLUDE_ZLIB) \
            $(call _if-define,SIL_UTILITY_NOISY_ERRORS) \
            $(if $(SIL_UTILITY_PNG_ALLOC_CHUNK),$(CFLAG_DEFINE)SIL_UTILITY_PNG_ALLOC_CHUNK=$(SIL_UTILITY_PNG_ALLOC_CHUNK)) \
            $(if $(SIL_UTILITY_PNG_COMPRESSION_LEVEL),$(CFLAG_DEFINE)SIL_UTILITY_PNG_COMPRESSION_LEVEL='($(SIL_UTILITY_PNG_COMPRESSION_LEVEL))') \
            $(if $(SIL_UTILITY_PNG_MAX_SIZE),$(CFLAG_DEFINE)SIL_UTILITY_PNG_MAX_SIZE=$(SIL_UTILITY_PNG_MAX_SIZE)) \
            $(if $(filter 1,$(SIL_STRTOF_OVERRIDE_LIBRARY)),,$(call _if-define,SIL_STRTOF_CUSTOM,SIL_UTILITY_RENAME_STRTOF)) \
            $(SIL_SYS_FLAGS) \
            $(if $(_BUILD_freetype),$(SIL_freetype_CLIENT_FLAGS)) \
            $(if $(_BUILD_libnogg),$(SIL_libnogg_CLIENT_FLAGS)) \
            $(if $(_BUILD_libpng),$(SIL_libpng_CLIENT_FLAGS)) \
            $(if $(_BUILD_libwebmdec),$(SIL_libwebmdec_CLIENT_FLAGS)) \
            $(if $(_BUILD_zlib),$(SIL_zlib_CLIENT_FLAGS)) \
            $(CFLAG_DEFINE)ZLIB_CONST

SIL_CFLAGS    = $(CFLAG_STD_C99)   $(SIL_FLAGS) $(WARNING_CFLAGS_STRICT) \
                $(SIL_EXTRA_CFLAGS)
SIL_CXXFLAGS  = $(CFLAG_STD_CXX11) $(SIL_FLAGS) $(WARNING_CXXFLAGS_STRICT) \
                $(SIL_EXTRA_CXXFLAGS)
SIL_OBJCFLAGS = $(CFLAG_STD_C99)   $(SIL_FLAGS) $(WARNING_OBJCFLAGS_STRICT) \
                $(SIL_EXTRA_OBJCFLAGS)
SIL_ASFLAGS   = $(SIL_FLAGS) $(SIL_EXTRA_ASFLAGS)

# Special case so isinf()/isnan() work correctly.  GCC allows us to disable
# just the inf/NaN part of -ffast-math with -fno-finite-math-only, but
# Clang either doesn't understand that option or doesn't implement it
# correctly, so we have to turn off -ffast-math altogether.
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/sysdep/%/userdata$(OBJECT_EXT) \
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/math/dtrig$(OBJECT_EXT) \
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/userdata$(OBJECT_EXT) \
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/utility/strformat$(OBJECT_EXT): \
    SIL_CFLAGS += $(if $(filter -ffast-math -ffinite-math-only,$(BASE_CFLAGS)),$(if $(filter gcc,$(CC_TYPE)),-fno-finite-math-only,-fno-fast-math))

#-------------------------------------------------------------------------#

SIL_freetype_DIR = $(SIL_DIR)/external/freetype
SIL_freetype_SOURCES = $(SIL_freetype_DIR)/src/autofit/autofit.c \
                       $(SIL_freetype_DIR)/src/base/ftbase.c \
                       $(SIL_freetype_DIR)/src/base/ftbitmap.c \
                       $(SIL_freetype_DIR)/src/base/ftbbox.c \
                       $(SIL_freetype_DIR)/src/base/ftdebug.c \
                       $(SIL_freetype_DIR)/src/base/ftglyph.c \
                       $(SIL_freetype_DIR)/src/base/ftinit.c \
                       $(SIL_freetype_DIR)/src/base/ftsystem.c \
                       $(SIL_freetype_DIR)/src/gzip/ftgzip.c \
                       $(SIL_freetype_DIR)/src/psnames/psnames.c \
                       $(SIL_freetype_DIR)/src/sfnt/sfnt.c \
                       $(SIL_freetype_DIR)/src/smooth/smooth.c \
                       $(SIL_freetype_DIR)/src/truetype/truetype.c
# Note include order: the OBJDIR path must come first to override ftmodule.h!
SIL_freetype_CLIENT_FLAGS = \
    $(CFLAG_INCLUDE_DIR)'$(SIL_freetype_DIR:$(TOPDIR)%=$(OBJDIR)%)/include' \
    $(CFLAG_INCLUDE_DIR)'$(SIL_freetype_DIR)/include' \
    $(if $(filter 1,$(DEBUG)),$(CFLAG_DEFINE)FT_DEBUG_MEMORY)
SIL_freetype_CFLAGS = $(CFLAG_STD_C99) \
                      $(SIL_freetype_CLIENT_FLAGS) \
                      $(SIL_zlib_CLIENT_FLAGS) \
                      $(CFLAG_DEFINE)FT2_BUILD_LIBRARY \
                      $(CFLAG_DEFINE)FT_CONFIG_OPTION_SYSTEM_ZLIB \
                      $(if $(filter -Wmissing-declarations,$(BASE_CFLAGS)),-Wno-missing-declarations) \
                      $(if $(filter -Wshadow,$(BASE_CFLAGS)),-Wno-shadow) \
                      $(if $(and $(filter gcc,$(CC_TYPE)),$(call CC_VERSION-is-at-least,4.6),$(filter -Wall -Wunused%,$(BASE_CFLAGS))),-Wno-unused-but-set-variable) \
                      $(if $(filter clang,$(CC_TYPE)),-Wno-unknown-warning-option -Wno-tautological-constant-compare)
SIL_freetype_GENLIB = libfreetype

AUTOGEN_HEADERS += $(if $(_BUILD_freetype), \
    $(SIL_freetype_DIR:$(TOPDIR)%=$(OBJDIR)%)/include/freetype/config/ftmodule.h)

#-------------------------------------------------------------------------#

SIL_libnogg_DIR = $(SIL_DIR)/external/libnogg

SIL_libnogg_SOURCES := $(wildcard $(SIL_libnogg_DIR)/src/*/*.c)

SIL_libnogg_CLIENT_FLAGS = $(CFLAG_INCLUDE_DIR)'$(SIL_libnogg_DIR)/include'
SIL_libnogg_CFLAGS = \
    $(CFLAG_STD_C99) \
    $(SIL_libnogg_CLIENT_FLAGS) \
    $(SIL_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_libnogg_DIR)' \
    $(if $(filter i%86 x86_64,$(CC_ARCH)),$(CFLAG_DEFINE)ENABLE_ASM_X86_SSE2) \
    $(if $(_SIL_HAS_NEON),$(CFLAG_DEFINE)ENABLE_ASM_ARM_NEON) \
    $(CFLAG_DEFINE)VERSION='"$(lastword $(subst -, ,$(SIL_libnogg_DIR)))"'
SIL_libnogg_GENLIB = libnogg

#-------------------------------------------------------------------------#

SIL_libpng_DIR = $(SIL_DIR)/external/libpng

SIL_libpng_SOURCES = $(SIL_libpng_DIR)/png.c \
                     $(SIL_libpng_DIR)/pngerror.c \
                     $(SIL_libpng_DIR)/pngget.c \
                     $(SIL_libpng_DIR)/pngmem.c \
                     $(SIL_libpng_DIR)/pngpread.c \
                     $(SIL_libpng_DIR)/pngread.c \
                     $(SIL_libpng_DIR)/pngrio.c \
                     $(SIL_libpng_DIR)/pngrtran.c \
                     $(SIL_libpng_DIR)/pngrutil.c \
                     $(SIL_libpng_DIR)/pngset.c \
                     $(SIL_libpng_DIR)/pngtrans.c \
                     $(SIL_libpng_DIR)/pngwio.c \
                     $(SIL_libpng_DIR)/pngwrite.c \
                     $(SIL_libpng_DIR)/pngwtran.c \
                     $(SIL_libpng_DIR)/pngwutil.c \
                     $(if $(_SIL_HAS_NEON), \
                         $(SIL_libpng_DIR)/arm/arm_init.c \
                         $(SIL_libpng_DIR)/arm/filter_neon_intrinsics.c \
                     )

SIL_libpng_CLIENT_FLAGS = \
    $(CFLAG_INCLUDE_DIR)'$(SIL_libpng_DIR:$(TOPDIR)%=$(OBJDIR)%)' \
    $(CFLAG_INCLUDE_DIR)'$(SIL_libpng_DIR)'
SIL_libpng_FLAGS = $(SIL_libpng_CLIENT_FLAGS) \
                   $(SIL_zlib_CLIENT_FLAGS) \
                   $(CFLAG_DEFINE)PNG_ARM_NEON_OPT=$(if $(_SIL_HAS_NEON),2,0)
SIL_libpng_CFLAGS = $(SIL_libpng_FLAGS) $(CFLAG_STD_C99)
SIL_libpng_ASFLAGS = $(SIL_libpng_FLAGS)
# Work around a shadowing warning on fmax in older versions of GCC.
SIL_libpng_CFLAGS += $(if $(and $(filter gcc,$(CC_TYPE)),$(not $(call CC_VERSION-is-at-least,5))),-Wno-shadow)
SIL_libpng_GENLIB = libpng

AUTOGEN_HEADERS += $(if $(_BUILD_libpng),$(SIL_libpng_DIR:$(TOPDIR)%=$(OBJDIR)%)/pnglibconf.h)

#-------------------------------------------------------------------------#

SIL_libvpx_DIR = $(SIL_DIR)/external/libvpx

# We split the sources into groups based on the organization of the source
# tree to simplify dealing with library updates.

_SIL_libvpx_SOURCES_vp8_common = \
    $(SIL_libvpx_DIR)/vp8/common/alloccommon.c \
    $(SIL_libvpx_DIR)/vp8/common/blockd.c \
    $(SIL_libvpx_DIR)/vp8/common/copy_c.c \
    $(SIL_libvpx_DIR)/vp8/common/dequantize.c \
    $(SIL_libvpx_DIR)/vp8/common/entropy.c \
    $(SIL_libvpx_DIR)/vp8/common/entropymode.c \
    $(SIL_libvpx_DIR)/vp8/common/entropymv.c \
    $(SIL_libvpx_DIR)/vp8/common/extend.c \
    $(SIL_libvpx_DIR)/vp8/common/filter.c \
    $(SIL_libvpx_DIR)/vp8/common/findnearmv.c \
    $(SIL_libvpx_DIR)/vp8/common/generic/systemdependent.c \
    $(SIL_libvpx_DIR)/vp8/common/idct_blk.c \
    $(SIL_libvpx_DIR)/vp8/common/idctllm.c \
    $(SIL_libvpx_DIR)/vp8/common/loopfilter_filters.c \
    $(SIL_libvpx_DIR)/vp8/common/mbpitch.c \
    $(SIL_libvpx_DIR)/vp8/common/modecont.c \
    $(SIL_libvpx_DIR)/vp8/common/quant_common.c \
    $(SIL_libvpx_DIR)/vp8/common/reconinter.c \
    $(SIL_libvpx_DIR)/vp8/common/reconintra.c \
    $(SIL_libvpx_DIR)/vp8/common/reconintra4x4.c \
    $(SIL_libvpx_DIR)/vp8/common/rtcd.c \
    $(SIL_libvpx_DIR)/vp8/common/setupintrarecon.c \
    $(SIL_libvpx_DIR)/vp8/common/swapyv12buffer.c \
    $(SIL_libvpx_DIR)/vp8/common/treecoder.c \
    $(SIL_libvpx_DIR)/vp8/common/vp8_loopfilter.c

_SIL_libvpx_SOURCES_vp8_common_ARCH_X86 = \
    $(SIL_libvpx_DIR)/vp8/common/x86/filter_x86.c \
    $(SIL_libvpx_DIR)/vp8/common/x86/loopfilter_x86.c \
    $(SIL_libvpx_DIR)/vp8/common/x86/vp8_asm_stubs.c

_SIL_libvpx_SOURCES_vp8_common_HAVE_MMX = \
    $(SIL_libvpx_DIR)/vp8/common/x86/dequantize_mmx.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/idct_blk_mmx.c \
    $(SIL_libvpx_DIR)/vp8/common/x86/idctllm_mmx.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/recon_mmx.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/subpixel_mmx.asm

_SIL_libvpx_SOURCES_vp8_common_HAVE_NEON = \
    $(SIL_libvpx_DIR)/vp8/common/arm/loopfilter_arm.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/bilinearpredict_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/copymem_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/dc_only_idct_add_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/dequant_idct_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/dequantizeb_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/idct_blk_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/idct_dequant_0_2x_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/idct_dequant_full_2x_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/iwalsh_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/loopfiltersimplehorizontaledge_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/loopfiltersimpleverticaledge_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/mbloopfilter_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/shortidct4x4llm_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/sixtappredict_neon.c \
    $(SIL_libvpx_DIR)/vp8/common/arm/neon/vp8_loopfilter_neon.c

_SIL_libvpx_SOURCES_vp8_common_HAVE_SSE2 = \
    $(SIL_libvpx_DIR)/vp8/common/x86/copy_sse2.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/idct_blk_sse2.c \
    $(SIL_libvpx_DIR)/vp8/common/x86/idctllm_sse2.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/iwalsh_sse2.asm \
    $(if $(filter x86_64,$(CC_ARCH)),$(SIL_libvpx_DIR)/vp8/common/x86/loopfilter_block_sse2_x86_64.asm) \
    $(SIL_libvpx_DIR)/vp8/common/x86/loopfilter_sse2.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/recon_sse2.asm \
    $(SIL_libvpx_DIR)/vp8/common/x86/subpixel_sse2.asm

_SIL_libvpx_SOURCES_vp8_common_HAVE_SSE3 = \
    $(SIL_libvpx_DIR)/vp8/common/x86/copy_sse3.asm

_SIL_libvpx_SOURCES_vp8_common_HAVE_SSSE3 = \
    $(SIL_libvpx_DIR)/vp8/common/x86/subpixel_ssse3.asm

_SIL_libvpx_SOURCES_vp8dx = \
    $(SIL_libvpx_DIR)/vp8/decoder/dboolhuff.c \
    $(SIL_libvpx_DIR)/vp8/decoder/decodeframe.c \
    $(SIL_libvpx_DIR)/vp8/decoder/decodemv.c \
    $(SIL_libvpx_DIR)/vp8/decoder/detokenize.c \
    $(SIL_libvpx_DIR)/vp8/decoder/onyxd_if.c \
    $(SIL_libvpx_DIR)/vp8/vp8_dx_iface.c

_SIL_libvpx_SOURCES_vp9_common = \
    $(SIL_libvpx_DIR)/vp9/common/vp9_alloccommon.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_blockd.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_common_data.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_entropy.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_entropymode.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_entropymv.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_filter.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_frame_buffers.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_idct.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_loopfilter.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_mvref_common.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_pred_common.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_quant_common.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_reconinter.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_reconintra.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_rtcd.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_scale.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_scan.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_seg_common.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_thread_common.c \
    $(SIL_libvpx_DIR)/vp9/common/vp9_tile_common.c

_SIL_libvpx_SOURCES_vp9_common_HAVE_NEON = \
    $(SIL_libvpx_DIR)/vp9/common/arm/neon/vp9_iht4x4_add_neon.c \
    $(SIL_libvpx_DIR)/vp9/common/arm/neon/vp9_iht8x8_add_neon.c

_SIL_libvpx_SOURCES_vp9_common_HAVE_SSE2 = \
    $(SIL_libvpx_DIR)/vp9/common/x86/vp9_idct_intrin_sse2.c

_SIL_libvpx_SOURCES_vp9dx = \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_decodeframe.c \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_decodemv.c \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_decoder.c \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_detokenize.c \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_dsubexp.c \
    $(SIL_libvpx_DIR)/vp9/decoder/vp9_dthread.c \
    $(SIL_libvpx_DIR)/vp9/vp9_dx_iface.c

_SIL_libvpx_SOURCES_vpx = \
    $(SIL_libvpx_DIR)/vpx/src/vpx_codec.c \
    $(SIL_libvpx_DIR)/vpx/src/vpx_decoder.c \
    $(SIL_libvpx_DIR)/vpx/src/vpx_image.c

_SIL_libvpx_SOURCES_vpx_dsp = \
    $(SIL_libvpx_DIR)/vpx_dsp/bitreader.c \
    $(SIL_libvpx_DIR)/vpx_dsp/bitreader_buffer.c \
    $(SIL_libvpx_DIR)/vpx_dsp/intrapred.c \
    $(SIL_libvpx_DIR)/vpx_dsp/inv_txfm.c \
    $(SIL_libvpx_DIR)/vpx_dsp/loopfilter.c \
    $(SIL_libvpx_DIR)/vpx_dsp/prob.c \
    $(SIL_libvpx_DIR)/vpx_dsp/vpx_convolve.c \
    $(SIL_libvpx_DIR)/vpx_dsp/vpx_dsp_rtcd.c

_SIL_libvpx_SOURCES_vpx_dsp_ARCH_X86 = \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_asm_stubs.c

_SIL_libvpx_SOURCES_vpx_dsp_HAVE_AVX2 = \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/loopfilter_avx2.c \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_8t_intrin_avx2.c

_SIL_libvpx_SOURCES_vpx_dsp_HAVE_NEON = \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct16x16_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct32x32_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct32x32_1_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct32x32_34_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct32x32_135_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct32x32_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct4x4_1_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct4x4_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct8x8_1_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/idct8x8_add_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/intrapred_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/loopfilter_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/vpx_convolve8_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/vpx_convolve_avg_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/vpx_convolve_copy_neon.c \
    $(SIL_libvpx_DIR)/vpx_dsp/arm/vpx_convolve_neon.c

_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSE2 = \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/intrapred_sse2.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/inv_txfm_sse2.c \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/inv_wht_sse2.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/loopfilter_sse2.c \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_convolve_copy_sse2.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_8t_sse2.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_bilinear_sse2.asm

_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSSE3 = \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/intrapred_ssse3.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_8t_intrin_ssse3.c \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_8t_ssse3.asm \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/vpx_subpixel_bilinear_ssse3.asm

_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSSE3_X86_64 = \
    $(SIL_libvpx_DIR)/vpx_dsp/x86/inv_txfm_ssse3_x86_64.asm

_SIL_libvpx_SOURCES_vpx_mem = \
    $(SIL_libvpx_DIR)/vpx_mem/vpx_mem.c

_SIL_libvpx_SOURCES_vpx_ports_ARCH_ARM = \
    $(SIL_libvpx_DIR)/vpx_ports/arm_cpudetect.c

_SIL_libvpx_SOURCES_vpx_ports_ARCH_X86 = \
    $(SIL_libvpx_DIR)/vpx_ports/emms.asm \
    $(SIL_libvpx_DIR)/vpx_ports/x86_abi_support.asm

_SIL_libvpx_SOURCES_vpx_scale = \
    $(SIL_libvpx_DIR)/vpx_scale/generic/yv12config.c \
    $(SIL_libvpx_DIR)/vpx_scale/generic/yv12extend.c \
    $(SIL_libvpx_DIR)/vpx_scale/vpx_scale_rtcd.c

_SIL_libvpx_SOURCES_vpx_util = \
    $(SIL_libvpx_DIR)/vpx_util/vpx_thread.c

# Note: The ARM VP9 decoder is broken since libvpx-1.4.0 (crash due to
# unaligned data access, confirmed on an iPad 3), so we disable it there.
# See also vpx.config and vp9_dx_stub$(OBJECT_EXT) rules in rules.mk.
SIL_libvpx_SOURCES = \
    $(_SIL_libvpx_SOURCES_vp8_common) \
    $(_SIL_libvpx_SOURCES_vp8dx) \
    $(if $(filter arm%,$(CC_ARCH)),, \
        $(_SIL_libvpx_SOURCES_vp9_common) \
        $(_SIL_libvpx_SOURCES_vp9dx) \
    ) \
    $(_SIL_libvpx_SOURCES_vpx) \
    $(_SIL_libvpx_SOURCES_vpx_dsp) \
    $(_SIL_libvpx_SOURCES_vpx_mem) \
    $(_SIL_libvpx_SOURCES_vpx_scale) \
    $(_SIL_libvpx_SOURCES_vpx_util) \
    $(if $(filter arm%,$(CC_ARCH)), \
        $(if $(_SIL_HAS_NEON), \
            $(_SIL_libvpx_SOURCES_vp8_common_HAVE_NEON) \
            $(_SIL_libvpx_SOURCES_vp9_common_HAVE_NEON) \
            $(_SIL_libvpx_SOURCES_vpx_dsp_HAVE_NEON) \
        ) \
        $(_SIL_libvpx_SOURCES_vpx_ports_ARCH_ARM) \
    ) \
    $(if $(filter x86%,$(CC_ARCH)), \
        $(_SIL_libvpx_SOURCES_vp8_common_ARCH_X86) \
        $(_SIL_libvpx_SOURCES_vpx_dsp_ARCH_X86) \
        $(if $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)), \
            $(_SIL_libvpx_SOURCES_vp8_common_HAVE_MMX) \
            $(_SIL_libvpx_SOURCES_vp8_common_HAVE_SSE2) \
            $(_SIL_libvpx_SOURCES_vp8_common_HAVE_SSE3) \
            $(_SIL_libvpx_SOURCES_vp8_common_HAVE_SSSE3) \
            $(_SIL_libvpx_SOURCES_vp9_common_HAVE_SSE2) \
            $(_SIL_libvpx_SOURCES_vpx_dsp_HAVE_AVX2) \
            $(_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSE2) \
            $(_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSSE3) \
            $(if $(filter x86_64,$(CC_ARCH)),$(_SIL_libvpx_SOURCES_vpx_dsp_HAVE_SSSE3_X86_64)) \
            $(_SIL_libvpx_SOURCES_vpx_ports_ARCH_X86) \
        ) \
    )

# *.asm sources aren't recognized by the SIL build system, so we need to
# write our own rules for them (see the libvpx section of rules.mk).
SIL_libvpx_OBJECTS += $(patsubst %.asm,%$(OBJECT_EXT),$(filter %.asm,$(SIL_libvpx_SOURCES)))

# On ARM, we still need a vpx_codec_vp9_dx() stub to keep libwebmdec happy.
# This is autogenerated in rules.mk.
SIL_libvpx_OBJECTS += $(if $(filter arm%,$(CC_ARCH)),$(SIL_libvpx_DIR)/vp9_dx_stub$(OBJECT_EXT))

SIL_libvpx_CLIENT_FLAGS = $(CFLAG_INCLUDE_DIR)'$(SIL_libvpx_DIR)'
SIL_libvpx_ARCH_FLAGS = \
    $(if $(filter arm,$(CC_ARCH)),-marm)
SIL_libvpx_CFLAGS = \
    $(SIL_libvpx_ARCH_FLAGS) \
    $(CFLAG_STD_C99) \
    $(SIL_libvpx_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)$(SIL_libvpx_DIR) \
    $(CFLAG_INCLUDE_DIR)$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%) \
    $(if $(filter clang,$(CC_TYPE)),-Wno-constant-conversion) \
    $(if $(filter -Wmissing-declarations,$(BASE_CFLAGS)),-Wno-missing-declarations) \
    $(if $(filter -Wshadow,$(BASE_CFLAGS)),-Wno-shadow) \
    $(if $(filter -Wstrict-prototypes,$(BASE_CFLAGS)),-Wno-strict-prototypes) \
    $(if $(filter -Wall -Wunused%,$(BASE_CFLAGS)),-Wno-unused) \
    $(if $(filter -Wwrite-strings,$(BASE_CFLAGS)),-Wno-write-strings) \
    $(SIL_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_DIR)' \
    $(CFLAG_INCLUDE_FILE)'$(SIL_DIR)/src/movie/webm-memory.h'
SIL_libvpx_ASFLAGS = \
    $(SIL_libvpx_ARCH_FLAGS) \
    $(CFLAG_INCLUDE_DIR)$(SIL_libvpx_DIR) \
    $(CFLAG_INCLUDE_DIR)$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)

# Special flags for source files with assembly intrinsics:
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_mmx$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -mmmx
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_sse2$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -msse2
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_sse3$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -msse3
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_ssse3$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -mssse3
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_sse4$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -msse4.1
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_avx$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -mavx
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_avx2$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += -mavx2
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%_neon$(OBJECT_EXT): SIL_libvpx_ARCH_FLAGS += $(if $(filter arm,$(CC_ARCH)),-mfpu=neon)

# Fix a build error when building without assembly sources:
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp8/%$(OBJECT_EXT) \
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9/%$(OBJECT_EXT): SIL_libvpx_CFLAGS += $(if $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),,$(CFLAG_DEFINE)'vpx_reset_mmx_state(...)=' $(if $(filter gcc clang,$(CC_TYPE)),-Wno-error))

SIL_libvpx_GENLIB = libvpx

AUTOGEN_HEADERS += $(if $(_BUILD_libvpx), \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp8_rtcd.h \
    $(if $(and $(filter arm,$(CC_TYPE)),$(not $(_SIL_HAS_NEON))),,$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9_rtcd.h) \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_config.h \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_dsp_rtcd.h \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_scale_rtcd.h \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_version.h \
    $(if $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)), \
        $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_config.asm \
    ) \
)

#-------------------------------------------------------------------------#

SIL_libwebmdec_DIR = $(SIL_DIR)/external/libwebmdec

# Assign immediately to avoid repeatedly shelling out to get the value,
# but only shell out if we'll actually need it.
ifneq ($(_BUILD_libwebmdec),)
    SIL_libwebmdec_VERSION := $(shell grep '^VERSION' '$(SIL_libwebmdec_DIR)/Makefile' | sed -e 's/^.*= *//')
endif

SIL_libwebmdec_SOURCES = \
    $(filter-out %/sample.c,$(wildcard $(SIL_libwebmdec_DIR)/src/*.c))

SIL_libwebmdec_CLIENT_FLAGS = \
    $(CFLAG_INCLUDE_DIR)'$(SIL_libwebmdec_DIR)/include'
SIL_libwebmdec_CFLAGS = \
    $(CFLAG_STD_C99) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_libwebmdec_DIR)' \
    $(SIL_nestegg_CLIENT_FLAGS) \
    $(SIL_libvpx_CLIENT_FLAGS) \
    $(if $(_BUILD_libnogg),$(SIL_libnogg_CLIENT_FLAGS)) \
    $(CFLAG_DEFINE)DECODE_AUDIO \
    $(CFLAG_DEFINE)DECODE_VIDEO \
    $(CFLAG_DEFINE)VERSION='"$(SIL_libwebmdec_VERSION)"' \
    $(SIL_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_DIR)' \
    $(CFLAG_INCLUDE_FILE)'$(SIL_DIR)/src/movie/webm-memory.h'

SIL_libwebmdec_GENLIB = libwebmdec

#-------------------------------------------------------------------------#

SIL_nestegg_DIR = $(SIL_DIR)/external/nestegg

SIL_nestegg_SOURCES = $(SIL_nestegg_DIR)/src/nestegg.c

SIL_nestegg_CLIENT_FLAGS = \
    $(CFLAG_INCLUDE_DIR)'$(SIL_nestegg_DIR)/include' \
    $(CFLAG_INCLUDE_DIR)'$(SIL_nestegg_DIR:$(TOPDIR)%=$(OBJDIR)%)'
SIL_nestegg_CFLAGS = \
    $(CFLAG_STD_C99) \
    $(SIL_nestegg_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_nestegg_DIR)/halloc' \
    $(if $(filter -Wwrite-strings,$(BASE_CFLAGS)),-Wno-write-strings) \
    $(SIL_CLIENT_FLAGS) \
    $(CFLAG_INCLUDE_DIR)'$(SIL_DIR)' \
    $(CFLAG_INCLUDE_FILE)'$(SIL_DIR)/src/movie/webm-memory.h'

SIL_nestegg_GENLIB = libnestegg

AUTOGEN_HEADERS += $(if $(_BUILD_nestegg), \
    $(SIL_nestegg_DIR:$(TOPDIR)%=$(OBJDIR)%)/nestegg/nestegg-stdint.h)

#-------------------------------------------------------------------------#

SIL_zlib_DIR = $(SIL_DIR)/external/zlib

SIL_zlib_SOURCES = $(SIL_zlib_DIR)/adler32.c \
                   $(SIL_zlib_DIR)/crc32.c \
                   $(SIL_zlib_DIR)/deflate.c \
                   $(SIL_zlib_DIR)/infback.c \
                   $(SIL_zlib_DIR)/inffast.c \
                   $(SIL_zlib_DIR)/inflate.c \
                   $(SIL_zlib_DIR)/inftrees.c \
                   $(SIL_zlib_DIR)/trees.c \
                   $(SIL_zlib_DIR)/zutil.c
SIL_zlib_CLIENT_FLAGS = $(CFLAG_INCLUDE_DIR)'$(SIL_zlib_DIR:$(TOPDIR)%=$(OBJDIR)%)' \
                        $(CFLAG_INCLUDE_DIR)'$(SIL_zlib_DIR)'
SIL_zlib_CFLAGS = $(CFLAG_STD_C99) \
                  $(SIL_zlib_CLIENT_FLAGS) \
                  $(CFLAG_DEFINE)ZLIB_CONST
SIL_zlib_GENLIB = libz

AUTOGEN_HEADERS += $(if $(_BUILD_zlib),$(SIL_zlib_DIR:$(TOPDIR)%=$(OBJDIR)%)/zconf.h)

###########################################################################
###########################################################################
