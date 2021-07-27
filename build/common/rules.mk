#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/common/rules.mk: Compilation and related rules for building SIL.
#

#
# This file defines common compilation and automatic dependency generation
# rules for source files.  The rules assume the following:
#
# - Modules have been defined as described in common/sources.mk and listed
#   in $(MODULES).  For each module <module> (with variables named
#   $(<module>_SOURCES), etc.), add the word <module> to $(MODULES).
#
# - Headers to be automatically generated at build time have been listed
#   in $(AUTOGEN_HEADERS).  The build rules will force these targets'
#   recipes to be executed at the start of each build.
#
# - Compilation commands are defined by $(CC) (for C, Objective-C, and
#   assembly sources) and $(CXX) (for C++ sources).
#
# - Common flags for all objects have been defined in
#   $(BASE_{C,CXX,OBJC,AS}FLAGS).
#
# - The following variables contain the associated compiler options:
#      $(CFLAG_PREPROCESS): Flag used to request output of preprocessed
#         source instead of compilation or linking.
#      $(CFLAG_COMPILE): Flag used to request compilation without linking.
#      $(CFLAG_WRITEDEPS): Flag used to request dependency data to be
#         written to the given file while compiling.  Used as
#         "$(CFLAG_WRITEDEPS)<path>".
#      $(CFLAG_OUTPUT): Flag used to specify the output file for a compile
#         operation.  Used as "$(CFLAG_OUTPUT)<path>".
#      $(CFLAG_LINK_OUTPUT): Flag used to specify the output file for a link
#         operation.  Used as "$(CFLAG_LINK_OUTPUT)<path>".
#      $(CFLAG_DEFINE): Flag used to define a preprocessor symbol.  Used as
#         "$(CFLAG_DEFINE)<var>[=<value>]".
#      $(CFLAG_INCLUDE_DIR): Flag used to add a directory to the #include
#         search path.  Used as "$(CFLAG_INCLUDE_DIR)<path>".
#      $(CFLAG_INCLUDE_FILE): Flag used to include a specific file before
#         processing each source file, as though the first line of each
#         source file was #include "<file>".  Used as
#         "$(CFLAG_INCLUDE_FILE)<path>".
#   Any options requiring a trailing space (for example, GCC's "-include"
#   option) should include the trailing space in the variable value, which
#   can be done by appending an empty variable reference (such as
#   $(preserve-trailing-space)) to the value.
#
# - $(AR_RC) gives the command name and options for creating an object
#   archive (i.e., library file), typically "ar rc" (without quotes) on a
#   Unix-like system, and $(ARFLAG_OUTPUT) gives the option name (if any)
#   for specifying the output pathname.  For example,
#   $(AR_RC) $(ARFLAG_OUTPUT)library.a object1.o object2.o should create an
#   archive "library.a" containing the files "object1.o" and "object2.o".
#
# - $(RANLIB) gives the command name required to prepare an object archive
#   for use as a library in a link command line, typically "ranlib" on a
#   Unix-like system.  If the system does not require object archives to be
#   prepared, this can be a no-op command such as ":".
#
# - $(GCOV) gives the name of the program used to extract coverage data
#   from *.gcda execution data files.  This is only used when generating a
#   coverage report.
#
# - $(OBJECT_EXT) and $(LIBRARY_EXT) give the filename extensions for
#   object and archive (library) files, respectively; these are typically
#   ".o" and ".a" on Unix-like systems.
#
# - The commands "mkdir", "rm", and "sed" are available and work as
#   specified by POSIX.
#
# =========================================================================
#
# This file exports the following variables for linking purposes:
#
# - $(ALL_OBJECTS): A list of all object files to be included in the final
#   executable, in the order the modules are listed in $(MODULES).
#
# - $(ALL_LDFLAGS): A list of all linker flags from modules, in the order
#   the modules are listed in $(MODULES).
#
# - $(ALL_LIBS): A list of all linker libraries from modules, in the order
#   the modules are listed in $(MODULES).
#
# This file also exports the variable $(MAKEFILE_DEPS), listing all
# make control files on which the project depends.  Custom build rules can
# use this variable to force rebuilds of custom-built files when a Makefile
# changes.
#
# =========================================================================
#
# This file does not define any build-related targets, since those tend to
# rely on system-specific behavior; see each system's "build.mk" for the
# specific targets available.  However, all systems define at least the
# following targets:
#
# - all: Build and (if appropriate for the system) package the program.
#   This is normally the default target.
#
# - clean: Remove all intermediate files, such as object files and
#   dependency data, but leave executables and other product files alone.
#
# - spotless: Remove all built files, including executables and other
#   product files, as well as "coverage.out" and other coverage data files.
#
# If you have custom build rules (to build data files, for example) and you
# want to remove those files in the default "clean" or "spotless" rules,
# define an appropriate recipe in the variable $(SIL_RECIPE_<rule>).  For
# example:
#    define SIL_RECIPE_spotless
#    $(ECHO) 'Removing generated data files'
#    $(Q)rm -f game.dat
#    endef
#
# In addition, this file defines the target "help" which lists all targets
# for which help messages are defined in $(SIL_HELP_<target>); for example,
#    SIL_HELP_all = 'Build the program'
# would cause "all" to be listed in the help text with the message "Build
# the program".  (The variable's value is passed directly to the "echo"
# command, so it should normally be enclosed in quotes.)  You can also
# define $(SIL_HELPTRAILER) for additional commands to execute after
# displaying the target list, for example:
#    define SIL_HELPTRAILER
#    echo ''
#    echo 'Source root: $(TOPDIR)'
#    echo ''
#    endef
#
# You can select the default target to be run by setting the
# $(SIL_DEFAULT_TARGET) variable.  If you do not define a default target
# before including this file, the default will be "all" if executable
# building is enabled ($(EXECUTABLE_NAME) is not empty), "help" otherwise.
#
# =========================================================================
#
# Separately from build targets, this file defines the target "gen-coverage"
# to generate a combined coverage file "coverage.out" from the data
# generated by running the program when built for coverage testing, and a
# "coverage-html" target to convert that combined coverage file to a set
# of HTML files showing coverage results in the subdirectory "coverage" of
# the build directory.
#
# The "coverage-html" rule supports merging multiple coverage reports from
# different platforms or architectures.  If there are multiple files whose
# names start with "coverage.out" in the build directory, the results in
# those files will be merged together to produce a single coverage report.
# Naturally, all coverage files should be generated by builds from
# identical source code or the report will not be generated correctly.
#
# Note that the "gen-coverage" rule does not currently work well with
# multi-architecture builds; when running gen-coverage manually, in many
# cases OBJDIR must be manually set to the appropriate value in the "make"
# command (e.g., "make gen-coverage OBJDIR=obj-armeabi-v7a" for Android
# 32-bit ARM).
#

###########################################################################
########################### Exported variables ############################
###########################################################################

# $(_SIL_BUILD):  Set to 1 to indicate that the SIL build system is in use.
# Referenced by tools/Makefile.

_SIL_BUILD = 1

###########################################################################
########################## Convenience variables ##########################
###########################################################################

# List of build control files.  All objects depend on these to ensure that
# changes to build rules are applied consistently throughout the project.

# Use := to evaluate it before we include all the *.d files (or else any
# future references to $(MAKEFILE_DEPS) would force rebuilding from scratch
# whenever any source file changed).
MAKEFILE_DEPS := $(MAKEFILE_LIST)

###########################################################################
######################### Default and help rules ##########################
###########################################################################

SIL_DEFAULT_TARGET ?= $(if $(EXECUTABLE_NAME),all,help)

.PHONY: default
default: $(or $(SIL_DEFAULT_TARGET),help)

.PHONY: help
help:
	@echo 'Available targets$(if $(filter-out help,$(SIL_DEFAULT_TARGET)), (default "$(SIL_DEFAULT_TARGET)")):'
	@maxwidth=1; \
	for i in $(patsubst SIL_HELP_%,%,$(filter SIL_HELP_%,$(.VARIABLES))); do \
	    if test $${#i} -gt $$maxwidth; then \
	        maxwidth=$${#i}; \
	    fi; \
	done \
	$(foreach i,$(sort $(filter SIL_HELP_%,$(.VARIABLES))),; printf '   %-*s -- %s\n' $$maxwidth ${i:SIL_HELP_%=%} ${$i})
	@$(SIL_HELPTRAILER)

###########################################################################
################## Object list and object-specific flags ##################
###########################################################################

ALL_OBJECTS =
ALL_LDFLAGS =
ALL_LIBS =

_SIL_ALL_OBJECTS_c =
_SIL_ALL_OBJECTS_cc =
_SIL_ALL_OBJECTS_cpp =
_SIL_ALL_OBJECTS_m =
_SIL_ALL_OBJECTS_mm =
_SIL_ALL_OBJECTS_S =
_SIL_ALL_OBJECTS_s =

# We use the COMPILER_FLAGS variable to hold the compiler flags for each
# set of objects.  This avoids the need to write extra copies of the build
# rules for each set.  In addition to object files, these flags are also
# used when generating test list headers for the generic testing framework
# (*.h files).
COMPILER_FLAGS = $(error Object flags not defined for $@)

# All the macros below are for internal use only -- do not call them from
# outside!

# $(call _SIL-add-source-set,<module>,<type>,<extension>):  Add sources from
# <module> of type <type> and filename extension <extension> to the build
# environment.  <type> is one of: C, CXX, OBJC, AS
define _SIL-add-source-set
    _SIL_$1_OBJECTS_$3 := $$(patsubst $$(TOPDIR)%,$(OBJDIR)%,$$(patsubst %.$3,%$$(OBJECT_EXT),$$(filter %.$3,$$($1_SOURCES))))
    ifneq ($$(_SIL_$1_OBJECTS_$3),)
        $1_OBJECTS += $$(_SIL_$1_OBJECTS_$3)
        _SIL_ALL_OBJECTS_$3 += $$(_SIL_$1_OBJECTS_$3)
        $$(_SIL_$1_OBJECTS_$3) $$(_SIL_$1_OBJECTS_$3:%$$(OBJECT_EXT)=%.h): \
            COMPILER_FLAGS = $$(BASE_$2FLAGS) \
                             $$(if $$(filter 1,$$(COVERAGE)), \
                                 $$(COVERAGE_$2FLAGS) $$(CFLAG_DEFINE)COVERAGE) \
                             $$($1_$2FLAGS) $$($2FLAGS)
    endif
endef

# $(eval $(call _SIL-add-module,<module>)):  Add <module> to the build
# environment.  <module> is the name of a module from $(MODULES).
define _SIL-add-module
    $(if $(filter ALL,$1),$(error Invalid module name "$1"))
    $1_OBJECTS := $$(patsubst $$(TOPDIR)%,$$(OBJDIR)%,$$($1_OBJECTS))
    $(call _SIL-add-source-set,$1,C,c)
    $(call _SIL-add-source-set,$1,CXX,cc)
    $(call _SIL-add-source-set,$1,CXX,cpp)
    $(call _SIL-add-source-set,$1,OBJC,m)
    $(call _SIL-add-source-set,$1,OBJCXX,mm)
    $(call _SIL-add-source-set,$1,AS,S)
    $(call _SIL-add-source-set,$1,AS,s)
    ALL_OBJECTS += $$(if $$($1_GENLIB),$$(OBJDIR)/$$($1_GENLIB)$$(LIBRARY_EXT),$$($1_OBJECTS))
    GENLIB_OBJECTS += $$(if $$($1_GENLIB),$$($1_OBJECTS))
    ALL_LDFLAGS += $$($1_LDFLAGS)
    ALL_LIBS += $$($1_LIBS)
endef

$(foreach module,$(MODULES),$(eval $(call _SIL-add-module,$(module))))

ALL_LIBS += $(if $(filter 1,$(COVERAGE)),$(COVERAGE_LIBS))

#-------------------------------------------------------------------------#

# Additional flag for test files, which need to include the generated
# header containing the list of tests for the generic test runner.
# Only enabled when actually building tests.

TEST_INCLUDE_FLAG =
ifeq ($(SIL_INCLUDE_TESTS),1)
_TEST_OBJECTS = $(patsubst $(TOPDIR)/%.c,$(OBJDIR)/%$(OBJECT_EXT),\
                  $(patsubst $(TOPDIR)/%.cc,$(OBJDIR)/%$(OBJECT_EXT),\
                    $(patsubst $(TOPDIR)/%.cpp,$(OBJDIR)/%$(OBJECT_EXT),\
                      $(patsubst $(TOPDIR)/%.m,$(OBJDIR)/%$(OBJECT_EXT),\
                        $(filter $(SIL_DIR)/src/test/%,$(SIL_SOURCES))))))
$(_TEST_OBJECTS): TEST_INCLUDE_FLAG = $(CFLAG_INCLUDE_FILE)$(@:%$(OBJECT_EXT)=%.h)
endif

###########################################################################
############################ Compilation rules ############################
###########################################################################

# Common dependencies for all objects.
$(ALL_OBJECTS) $(GENLIB_OBJECTS): $(AUTOGEN_HEADERS) $(MAKEFILE_DEPS)

# Test sources need to depend on each file's generated header as well
# (but only when actually building tests).
ifeq ($(SIL_INCLUDE_TESTS),1)
$(filter $(patsubst $(TOPDIR)%,$(OBJDIR)%,$(SIL_DIR)/src/test)/%,$(ALL_OBJECTS)) : \
%$(OBJECT_EXT): %.h
# Generated test headers may in turn need to depend on $(AUTOGEN_HEADERS)
# to avoid #include failures.
$(patsubst %$(OBJECT_EXT),%.h,$(filter $(patsubst $(TOPDIR)%,$(OBJDIR)%,$(SIL_DIR)/src/test)/%,$(ALL_OBJECTS))): $(AUTOGEN_HEADERS)
endif

# Add dependencies for all object files with sources on their respective
# source files.  This is needed to cause the build to fail with a useful
# error message for an object with a missing source file; without this, the
# failure is not detected until the final link, with somewhat misleading
# "obj/<file>.o: No such file or directory" errors.
# Note that the blank line in _SIL-source-dependency is needed to put each
# dependency on a separate line.
define _SIL-source-dependency
    $1: $(patsubst $(OBJDIR)/%$(OBJECT_EXT),$(TOPDIR)/%.$2,$1)

endef
define _SIL-gen-source-dependencies
    $(foreach object,$(_SIL_ALL_OBJECTS_$1),$(call _SIL-source-dependency,$(object),$1))
endef
$(eval $(call _SIL-gen-source-dependencies,c))
$(eval $(call _SIL-gen-source-dependencies,cc))
$(eval $(call _SIL-gen-source-dependencies,cpp))
$(eval $(call _SIL-gen-source-dependencies,m))
$(eval $(call _SIL-gen-source-dependencies,mm))
$(eval $(call _SIL-gen-source-dependencies,S))
$(eval $(call _SIL-gen-source-dependencies,s))

#-------------------------------------------------------------------------#

# Recipe fragment used to tweak a generated dependency file.
#
# The first sed expression (a 3-line loop) cleans .. path components out
# of each pathname so relative paths from different directories all end up
# as the same target name; this is important if the target has its own
# Makefile rule, since lexically distinct pathnames are treated as separate
# targets even if they refer to the same file.  The second expression is
# another 3-line loop that clears out . path components, for the same
# reason.  The third expression removes the generated header for test files
# from the dependency list, to avoid a circular dependency between the
# header and itself.  (The dependency is added separately in this file.)
# The last expression expands the target from *.o to *.[oh] (or as
# appropriate for $(OBJECT_EXT)), so the generated header shares the same
# dependency list.
#
# Note that we use two temporary files and a "mv" command to ensure that
# the final file is atomically created.  If we made the "sed" command
# output directly to the final file, an untimely interrupt signal could
# result in a partially-written dependency list which would cause
# subsequent build attempts to fail.
#
# System-specific build.mk files can define SIL_SYS_FILTER_DEPS_CMD to a
# filter command into which the output of this filter will be piped.  This
# can be used for systems which require additional filtering such as
# pathname translation.

define _SIL-filter-deps
$(Q)test \! -f '$2.tmp' && rm -f '$2~' || sed \
    -e ':1' \
    -e 's#\(\\ \|[^ /]\)\+/\.\./##' \
    -e 't1' \
    -e ':2' \
    -e 's#/\./#/#' \
    -e 't2' \
    -e 's#^\(\([^ 	]*[ 	]\)*\)$(subst .,\.,$(1:%$(OBJECT_EXT)=%.h))#\1#' \
    -e 's#$(subst .,\.,$1)#$1 $(1:%$(OBJECT_EXT)=%.h)#' \
    <'$2.tmp' $(if $(SIL_SYS_FILTER_DEPS_CMD),| $(SIL_SYS_FILTER_DEPS_CMD)) >'$2~'
$(Q)rm -f '$2.tmp'
$(Q)test \! -f '$2~' && rm -f '$2' || mv '$2~' '$2'
endef

#-------------------------------------------------------------------------#

# Compilation rules for each source file type.

# Call: _SIL-compile(tool,message[,no-dep-output])
define _SIL-compile
$(ECHO) '$2 $(<:$(TOPDIR)/%=%)$(if $(filter 1,$(SHOW_ARCH_IN_BUILD_MESSAGES)), ($(TARGET_ARCH_ABI)))'
$(Q)mkdir -p '$(@D)'
$(Q)$1 $(COMPILER_FLAGS) $(TEST_INCLUDE_FLAG) $(if $3,,$(CFLAG_WRITEDEPS)'$(@:%$(OBJECT_EXT)=%.d.tmp)') $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
$(if $3,,$(call _SIL-filter-deps,$@,$(@:%$(OBJECT_EXT)=%.d)))
endef

$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.c
	$(call _SIL-compile,$(CC),Compiling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.cc
	$(call _SIL-compile,$(CXX),Compiling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.cpp
	$(call _SIL-compile,$(CXX),Compiling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.m
	$(call _SIL-compile,$(CC),Compiling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.mm
	$(call _SIL-compile,$(CXX),Compiling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.S
	$(call _SIL-compile,$(CC),Assembling)
$(OBJDIR)/%$(OBJECT_EXT): $(TOPDIR)/%.s
	$(call _SIL-compile,$(CC),Assembling,1)

#-------------------------------------------------------------------------#

# Rules for building library files from modules.

define _SIL-gen-library
$$(OBJDIR)/$$($1_GENLIB)$$(LIBRARY_EXT): $$($1_OBJECTS)
	$$(ECHO) 'Archiving $$@$$(if $$(filter 1,$$(SHOW_ARCH_IN_BUILD_MESSAGES)), ($$(TARGET_ARCH_ABI)))'
	$$(Q)$$(AR_RC) $(ARFLAG_OUTPUT)'$$@' $$($1_OBJECTS)
	$$(Q)$$(RANLIB) '$$@'
endef

$(foreach module,$(MODULES),$(eval $(if $($(module)_GENLIB),$(call _SIL-gen-library,$(module)))))

#-------------------------------------------------------------------------#

# Rules for generating the per-source header used by the generic test
# framework.

define _SIL-gen-test-header
$(ECHO) 'Extracting tests from $(<:$(TOPDIR)/%=%)$(if $(filter 1,$(SHOW_ARCH_IN_BUILD_MESSAGES)), ($(TARGET_ARCH_ABI)))'
$(Q)mkdir -p '$(@D)'
$(Q)$1 $(CFLAG_DEFINE)'TEST(name)=--TEST_TEST--name--' \
       $(CFLAG_DEFINE)'TEST_INIT(name)=--TEST_INIT--name--' \
       $(CFLAG_DEFINE)'TEST_CLEANUP(name)=--TEST_CLEANUP--name--' \
       $(COMPILER_FLAGS) $(CFLAG_PREPROCESS) $(CFLAG_OUTPUT)'$@.tmp' '$<'
$(Q)(set -e; \
    cat /dev/null >'$@~'; \
    sed -n \
        -e '/^[ 	]*--TEST_[A-Z]*--/ {' \
        -e '    s/^[ 	]*--TEST_[A-Z]*--\([^-]*\).*/static int \1(void);/' \
        -e '    p' \
        -e '}' \
        <'$@.tmp' >>'$@~'; \
    echo >>'$@~' '#define _DEFINE_GENERIC_TESTS \'; \
    sed -n \
        -e '/^[ 	]*--TEST_[A-Z]*--/ {' \
        -e '    s/^[ 	]*--TEST\(_[A-Z]*\)--\([^-]*\).*/        {\1, "\2", \2}, \\/' \
        -e '    p' \
        -e '}' \
    <'$@.tmp' >>'$@~'; \
    echo >>'$@~' ''; \
)
$(Q)rm -f '$@.tmp'
$(Q)mv -f '$@~' '$@'
endef

$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/%.h: $(SIL_DIR)/src/test/%.c $(MAKEFILE_DEPS)
	$(call _SIL-gen-test-header,$(CC))
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/%.h: $(SIL_DIR)/src/test/%.cc $(MAKEFILE_DEPS)
	$(call _SIL-gen-test-header,$(CXX))
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/%.h: $(SIL_DIR)/src/test/%.cpp $(MAKEFILE_DEPS)
	$(call _SIL-gen-test-header,$(CXX))
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/%.h: $(SIL_DIR)/src/test/%.m $(MAKEFILE_DEPS)
	$(call _SIL-gen-test-header,$(CC))
$(SIL_DIR:$(TOPDIR)%=$(OBJDIR)%)/src/test/%.h: $(SIL_DIR)/src/test/%.mm $(MAKEFILE_DEPS)
	$(call _SIL-gen-test-header,$(CXX))

#-------------------------------------------------------------------------#

# Rule for generating coverage output.

_ALL_COVERAGE_SOURCES = $(subst $(TOPDIR)/,, \
                            $(SIL_SOURCES) \
                            $(wildcard $(SIL_DIR)/include/SIL/*.h) \
                            $(wildcard $(SIL_DIR)/include/SIL/*/*.h) \
                            $(wildcard $(SIL_DIR)/include/SIL/*/*/*.h)) \
                        $(COVERAGE_SOURCES)
_ALL_COVERAGE_EXCLUDE = $(subst $(TOPDIR)/,$(notdir $(TOPDIR))/,$(SIL_DIR)/src/test) $(patsubst %,$(notdir $(TOPDIR))/%,$(COVERAGE_EXCLUDE))
_ALL_COVERAGE_INCLUDE =
define _maybe-add-include
ifeq ($(filter $1,$(_ALL_COVERAGE_INCLUDE)),)
_ALL_COVERAGE_INCLUDE += $1
endif
endef
$(foreach file,$(_ALL_COVERAGE_SOURCES),$(eval \
    $(call _maybe-add-include,$(dir $(notdir $(TOPDIR))/$(file)))))

.PHONY: gen-coverage
gen-coverage: coverage.out

.PHONY: coverage.out
coverage.out:
	$(ECHO) 'Collecting coverage data'
	$(Q)rm -rf .covtmp
	$(Q)mkdir .covtmp
	$(Q)set -e $(if $(filter 1,$(V)),-x); cd .covtmp && for f in $(subst $(TOPDIR)/,,$(SIL_SOURCES) $(COVERAGE_SOURCES)); do \
	    $(GCOV) $(GCOV_OPTS) $(call GCOV_FILE_OPTS,$$f) $(GCOV_STDOUT) $(if $(filter 1,$(V)),,2>/dev/null); \
	done
	$(Q)perl '$(SIL_DIR)/tools/cov-merge.pl' \
	    --ignore-branch-regex='^\s+(PRECOND|ASSERT)' \
	    --strip='$(dir $(TOPDIR))' \
	    $(foreach path,$(_ALL_COVERAGE_INCLUDE),--include='$(path)') \
	    $(foreach path,$(_ALL_COVERAGE_EXCLUDE),--exclude='$(path)') \
	    .covtmp/* \
	    > '$@'
	$(Q)rm -rf .covtmp
	$(ECHO) 'Coverage data written to $(abspath $@)'

.PHONY: coverage-html
coverage-html:
	$(ECHO) 'Processing coverage data'
	$(Q)perl '$(SIL_DIR)/tools/cov-merge.pl' coverage.out* \
	    | perl '$(SIL_DIR)/tools/cov-html.pl'
	$(Q)rm -f coverage.out*
	$(ECHO) 'Coverage results can be found in $(abspath coverage)'

###########################################################################
################### Object- and library-specific rules ####################
###########################################################################

$(SIL_freetype_DIR:$(TOPDIR)%=$(OBJDIR)%)/include/freetype/config/ftmodule.h: \
    $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)rm -f '$@'
	$(Q)echo >>'$@' 'FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)'
	$(Q)echo >>'$@' 'FT_USE_MODULE(FT_Module_Class, sfnt_module_class)'
	$(Q)echo >>'$@' 'FT_USE_MODULE(FT_Module_Class, autofit_module_class)'
	$(Q)echo >>'$@' 'FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)'
	$(Q)echo >>'$@' 'FT_USE_MODULE(FT_Module_Class, psnames_module_class)'

#-------------------------------------------------------------------------#

$(SIL_libpng_DIR:$(TOPDIR)%=$(OBJDIR)%)/pnglibconf.h: \
    $(SIL_libpng_DIR)/scripts/pnglibconf.h.prebuilt $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)zlib_vernum="$$( (echo '#include "zlib.h"'; echo ZLIB_VERNUM) | $(CC) $(BASE_CFLAGS) $(SIL_libpng_CFLAGS) $(CFLAGS) -E - | tail -1)"; \
	    test -n "$$zlib_vernum" && \
	    sed \
	        -e '/STANDARD API DEFINITION/d' \
	        -e '/PNG_CONSOLE_IO_SUPPORTED/d' \
	        -e '/PNG_CONVERT_tIME_SUPPORTED/d' \
	        -e '/PNG_FORMAT_AFIRST_SUPPORTED/d' \
	        -e '/PNG_FORMAT_BGR_SUPPORTED/d' \
	        -e '/PNG_READ_ALPHA_MODE_SUPPORTED/d' \
	        -e '/PNG_READ_BACKGROUND_SUPPORTED/d' \
	        -e '/PNG_READ_GAMMA_SUPPORTED/d' \
	        -e '/PNG_READ_RGB_TO_GRAY_SUPPORTED/d' \
	        -e '/PNG_STDIO_SUPPORTED/d' \
	        -e '/PNG_SIMPLIFIED_READ.*_SUPPORTED/d' \
	        -e '/PNG_SIMPLIFIED_WRITE.*_SUPPORTED/d' \
	        -e '/PNG_TIME_RFC1123_SUPPORTED/d' \
	        -e '/PNG_.*iCCP_SUPPORTED/d' \
	        -e '/PNG_.*sRGB_SUPPORTED/d' \
	        -e 's/\(PNG_ZLIB_VERNUM\) 0.*/\1 '"$$zlib_vernum"'/' \
		<'$<' >'$@'
# Corresponding pngusr.dfa (for reference):
# chunk iCCP off
# chunk sRGB off
# option CONVERT_tIME off
# option SIMPLIFIED_READ off
# option SIMPLIFIED_WRITE off
# option STDIO off
# option TIME_RFC1123 off

#-------------------------------------------------------------------------#

# Handle libvpx's yasm-format x86 assembly sources.  We need an explicit
# source list to avoid also matching *.asm files in ARM directories.
$(patsubst $(TOPDIR)%.asm,$(OBJDIR)%$(OBJECT_EXT),$(filter %.asm,$(SIL_libvpx_SOURCES))) : \
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/%$(OBJECT_EXT): $(SIL_libvpx_DIR)/%.asm \
    $(MAKEFILE_DEPS)
	$(ECHO) 'Assembling $(<:$(TOPDIR)/%=%)$(if $(filter 1,$(SHOW_ARCH_IN_BUILD_MESSAGES)), ($(TARGET_ARCH_ABI)))'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(YASM) -f elf$(if $(filter x86_64,$(CC_ARCH)),64,32) -g dwarf2 \
	    -I'$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/' \
	    -I'$(SIL_libvpx_DIR)/' \
	    -o '$@' '$<'

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx.config: $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)echo  >'$@' CONFIG_DEPENDENCY_TRACKING=no
	$(Q)echo  >'$@' CONFIG_EXTERNAL_BUILD=no
	$(Q)echo >>'$@' CONFIG_INSTALL_DOCS=no
	$(Q)echo >>'$@' CONFIG_INSTALL_BINS=no
	$(Q)echo >>'$@' CONFIG_INSTALL_LIBS=no
	$(Q)echo >>'$@' CONFIG_INSTALL_SRCS=no
	$(Q)echo >>'$@' CONFIG_USE_X86INC=no
	$(Q)echo >>'$@' CONFIG_DEBUG=no
	$(Q)echo >>'$@' CONFIG_GPROF=no
	$(Q)echo >>'$@' CONFIG_GCOV=no
	$(Q)echo >>'$@' CONFIG_RVCT=no
	$(Q)echo >>'$@' CONFIG_GCC=$(if $(filter gcc clang,$(CC_TYPE)),yes,no)
	$(Q)echo >>'$@' CONFIG_MSVS=no
	$(Q)echo >>'$@' CONFIG_PIC=yes
	$(Q)echo >>'$@' CONFIG_BIG_ENDIAN=no
	$(Q)echo >>'$@' CONFIG_CODEC_SRCS=no
	$(Q)echo >>'$@' CONFIG_DEBUG_LIBS=no
	$(Q)echo >>'$@' CONFIG_DEQUANT_TOKENS=no
	$(Q)echo >>'$@' CONFIG_DC_RECON=no
	@# The ANDROID_NDK test here is to check whether we're building for Android.
	$(Q)echo >>'$@' CONFIG_RUNTIME_CPU_DETECT=$(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(or $(filter x86%,$(CC_ARCH)),$(and $(ANDROID_NDK),$(filter arm%,$(CC_ARCH))))),yes,no)
	$(Q)echo >>'$@' CONFIG_POSTPROC=no
	$(Q)echo >>'$@' CONFIG_VP9_POSTPROC=no
	$(Q)echo >>'$@' CONFIG_MULTITHREAD=no
	$(Q)echo >>'$@' CONFIG_INTERNAL_STATS=no
	$(Q)echo >>'$@' CONFIG_VP8_ENCODER=no
	$(Q)echo >>'$@' CONFIG_VP8_DECODER=yes
	$(Q)echo >>'$@' CONFIG_VP9_ENCODER=no
	@# The ARM VP9 decoder is broken since libvpx-1.4.0 (crash due to
	@# unaligned data access), so we disable it there; see also sources.mk.
	$(Q)echo >>'$@' CONFIG_VP9_DECODER=$(if $(filter arm%,$(CC_ARCH)),no,yes)
	$(Q)echo >>'$@' CONFIG_VP10_ENCODER=no
	$(Q)echo >>'$@' CONFIG_VP10_DECODER=no
	$(Q)echo >>'$@' CONFIG_VP8=yes
	$(Q)echo >>'$@' CONFIG_VP9=$(if $(filter arm%,$(CC_ARCH)),no,yes)
	$(Q)echo >>'$@' CONFIG_VP10=no
	$(Q)echo >>'$@' CONFIG_ENCODERS=no
	$(Q)echo >>'$@' CONFIG_DECODERS=yes
	$(Q)echo >>'$@' CONFIG_STATIC_MSVCRT=no
	$(Q)echo >>'$@' CONFIG_SPATIAL_RESAMPLING=no
	$(Q)echo >>'$@' CONFIG_REALTIME_ONLY=no
	$(Q)echo >>'$@' CONFIG_ONTHEFLY_BITPACKING=no
	$(Q)echo >>'$@' CONFIG_ERROR_CONCEALMENT=no
	$(Q)echo >>'$@' CONFIG_SHARED=no
	$(Q)echo >>'$@' CONFIG_STATIC=no
	$(Q)echo >>'$@' CONFIG_SMALL=no
	$(Q)echo >>'$@' CONFIG_POSTPROC_VISUALIZER=no
	$(Q)echo >>'$@' CONFIG_OS_SUPPORT=no
	$(Q)echo >>'$@' CONFIG_UNIT_TESTS=no
	$(Q)echo >>'$@' CONFIG_WEBM_IO=no
	$(Q)echo >>'$@' CONFIG_LIBYUV=no
	$(Q)echo >>'$@' CONFIG_DECODE_PERF_TESTS=no
	$(Q)echo >>'$@' CONFIG_ENCODE_PERF_TESTS=no
	$(Q)echo >>'$@' CONFIG_MULTI_RES_ENCODING=no
	$(Q)echo >>'$@' CONFIG_TEMPORAL_DENOISING=no
	$(Q)echo >>'$@' CONFIG_COEFFICIENT_RANGE_CHECKING=no
	$(Q)echo >>'$@' CONFIG_VP9_HIGHBITDEPTH=no
	$(Q)echo >>'$@' CONFIG_EXPERIMENTAL=no
	$(Q)echo >>'$@' CONFIG_SIZE_LIMIT=no
	$(Q)echo >>'$@' CONFIG_SPATIAL_SVC=no
	$(Q)echo >>'$@' CONFIG_FP_MB_STATS=no
	$(Q)echo >>'$@' CONFIG_EMULATE_HARDWARE=no
	$(Q)echo >>'$@' CONFIG_MISC_FIXES=no

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_config.h: \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx.config $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)echo  >'$@' '#define RESTRICT'
	$(Q)echo >>'$@' '#define INLINE $(if $(filter gcc clang,$(CC_TYPE)),__inline__ __attribute__((always_inline)))'
	$(Q)echo >>'$@' '#define ARCH_ARM $(if $(filter arm%,$(CC_ARCH)),1,0)'
	$(Q)echo >>'$@' '#define ARCH_MIPS $(if $(filter mips,$(CC_ARCH)),1,0)'
	$(Q)echo >>'$@' '#define ARCH_X86 $(if $(filter x86,$(CC_ARCH)),1,0)'
	$(Q)echo >>'$@' '#define ARCH_X86_64 $(if $(filter x86_64,$(CC_ARCH)),1,0)'
	$(Q)echo >>'$@' '#define HAVE_EDSP 0'
	$(Q)echo >>'$@' '#define HAVE_MEDIA 0'
	$(Q)echo >>'$@' '#define HAVE_NEON $(if $(or $(filter arm64,$(CC_ARCH)),$(and $(filter arm,$(CC_ARCH)),$(filter -mfpu=neon,$(BASE_CFLAGS)))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_NEON_ASM 0'
	$(Q)echo >>'$@' '#define HAVE_MIPS32 0'
	$(Q)echo >>'$@' '#define HAVE_DSPR2 0'
	$(Q)echo >>'$@' '#define HAVE_MSA 0'
	$(Q)echo >>'$@' '#define HAVE_MIPS64 0'
	$(Q)echo >>'$@' '#define HAVE_MMX $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_SSE $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_SSE2 $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_SSE3 $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_SSSE3 $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_SSE4_1 $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_AVX $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_AVX2 $(if $(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter x86%,$(CC_ARCH))),1,0)'
	$(Q)echo >>'$@' '#define HAVE_VPX_PORTS 1'
	$(Q)echo >>'$@' '#define HAVE_STDINT_H 1'
	$(Q)echo >>'$@' '#define HAVE_PTHREAD_H 0'
	$(Q)echo >>'$@' '#define HAVE_SYS_MMAN_H 0'
	$(Q)echo >>'$@' '#define HAVE_UNISTD_H 0'
	$(Q)sed \
	    -e 's/=yes/ 1/' \
	    -e 's/=no/ 0/' \
	    -e 's/^/#define /' \
	    <'$(@:%/vpx_config.h=%/vpx.config)' >>'$@'

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_config.asm: \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_config.h $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)grep -E '#define [A-Z0-9_]+ [01]' $< \
	    | awk '{print $$2" EQU "$$3}' \
	    > '$@'
	$(Q)echo >>'$@' ' END'

SIL_libvpx_RTCD_SOURCE-vp8_rtcd.h = vp8/common/rtcd_defs.pl
SIL_libvpx_RTCD_SOURCE-vp9_rtcd.h = vp9/common/vp9_rtcd_defs.pl
SIL_libvpx_RTCD_SOURCE-vpx_dsp_rtcd.h = vpx_dsp/vpx_dsp_rtcd_defs.pl
SIL_libvpx_RTCD_SOURCE-vpx_scale_rtcd.h = vpx_scale/vpx_scale_rtcd.pl

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp8_rtcd.h \
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9_rtcd.h \
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_dsp_rtcd.h \
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_scale_rtcd.h: \
    $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx.config $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(if $(filter 1,$(V)),set -x; )perl '$(SIL_libvpx_DIR)/build/make/rtcd.pl' \
	    --arch='$(if $(filter arm,$(CC_ARCH)),armv7,$(or $(filter arm64,$(CC_ARCH)),$(and $(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM)),$(filter arm64 x86%,$(CC_ARCH))),c))' \
	    --config='$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx.config' \
            $(if $(filter arm,$(CC_ARCH)),--disable-media --$(if $(filter -mfpu=neon,$(BASE_CFLAGS)),require,disable)-neon --disable-neon_asm) \
            $(if $(and $(filter x86%,$(CC_ARCH)),$(filter 1,$(SIL_LIBRARY_INTERNAL_LIBVPX_ASM))),--require-mmx --require-sse --require-sse2) \
	    --sym=$(@F:%.h=%) \
	    '$(SIL_libvpx_DIR)/$(SIL_libvpx_RTCD_SOURCE-$(@F))' \
	    >'$@' \
	    || (rm -f '$@'; false)

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vpx_version.h: $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)rm -f '$@'
	@# This is run in the target directory because it dumps garbage in cwd.
	$(Q)(cd '$(@D)'; sh -e '$(SIL_libvpx_DIR)/build/make/version.sh' '$(SIL_libvpx_DIR)' '$(@F)')
	$(Q)rm -f '$(@D)'/*.tmp

# VP9 stub for ARM:
$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9_dx_stub$(OBJECT_EXT): $(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9_dx_stub.c $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)$(CC) $(BASE_CFLAGS) $(SIL_libvpx_CFLAGS) $(CFLAGS) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'

$(SIL_libvpx_DIR:$(TOPDIR)%=$(OBJDIR)%)/vp9_dx_stub.c: $(MAKEFILE_DEPS)
	$(Q)mkdir -p '$(@D)'
	$(Q)echo >'$@' 'void *vpx_codec_vp9_dx(void) {return 0;}'

#-------------------------------------------------------------------------#

$(SIL_nestegg_DIR:$(TOPDIR)%=$(OBJDIR)%)/nestegg/nestegg-stdint.h: \
    $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)echo >'$@' '#include <stdint.h>'

#-------------------------------------------------------------------------#

$(SIL_zlib_DIR:$(TOPDIR)%=$(OBJDIR)%)/zconf.h: $(SIL_zlib_DIR)/zconf.h.cmakein \
    $(MAKEFILE_DEPS)
	$(ECHO) 'Generating $@'
	$(Q)mkdir -p '$(@D)'
	$(Q)sed \
		-e '/cmakedefine Z_PREFIX/d' \
		-e 's/cmakedefine Z_HAVE_UNISTD_H/define Z_HAVE_UNISTD_H 1/' \
		<'$<' >'$@'

###########################################################################
#################### Dependency data (auto-generated) #####################
###########################################################################

# Don't try to include dependency data if we're not actually building
# anything.  This is particularly important for "clean" and "spotless",
# since an out-of-date dependency file which referenced a nonexistent
# target (this can arise from switching versions in place using version
# control, for example) would otherwise block these targets from running
# and cleaning out that very dependency file!

ifneq ($(filter-out help clean spotless,$(or $(MAKECMDGOALS),default)),)
include $(sort $(wildcard $(patsubst %$(OBJECT_EXT),%.d,$(filter %$(OBJECT_EXT),$(ALL_OBJECTS))) $(GENLIB_OBJECTS:%$(OBJECT_EXT)=%.d)))
endif

###########################################################################
###########################################################################
