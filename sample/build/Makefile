# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# sample/build/Makefile:  Build control file for the sample SIL client.


# Set the top directory for the project.  In this case, we use the root of
# the SIL package as the top directory.  Note the use of := to force the
# path to be evaluated immediately; while not strictly necessary in this
# particular case, the $(abspath) function may take time to look up
# pathnames, so we evaluate it here rather than reevaluating it on every
# reference (of which there will be many).

TOPDIR := $(abspath ../..)


# Set SIL build parameters for this project.

EXECUTABLE_NAME = sample
PROGRAM_NAME = SIL Sample App
PROGRAM_VERSION_CODE = $(shell (hg id -n 2>/dev/null || echo 1) | sed 's/[^0-9].*//')

SIL_INCLUDE_TESTS = $(if $(filter 1,$(DEBUG)),1,0)
SIL_MEMORY_FORBID_MALLOC = 1

WARNINGS_AS_ERRORS = 1


# Define the platform on which we're running, and set additional compiler
# and linker parameters.  These can be overridden from the command line to
# build for a different system.  In a more complex build environment, we
# might have separate build directories and Makefiles for each system,
# much like SIL itself has.

UNAME_S := $(shell uname -s | tr A-Z a-z)
ifneq (,$(findstring mingw,$(UNAME_S)))
    PLATFORM = windows
else ifneq (,$(findstring darwin,$(UNAME_S)))
    PLATFORM = macosx
else ifneq (,$(findstring -Microsoft,$(shell uname -r)))
    # Windows Subsystem for Linux
    PLATFORM = windows
else
    PLATFORM = linux
endif

ifeq ($(PLATFORM),android)
    SIGN = 1
    SIGN_KEYSTORE = android-debug.keystore
    SIGN_STOREPASS = storepass
    SIGN_KEYPASS = keypass
    SIGN_ALIAS = debug_key
else ifeq ($(PLATFORM),windows)
    PROGRAM_VERSION_CODE := 1.0.0.$(PROGRAM_VERSION_CODE)
    # Use console mode so we can see test output.
    LINK_MODE = console
endif


# Define a module for this program's source files.  Note the inclusion of
# $(SIL_CLIENT_FLAGS) in the module's custom CFLAGS; this pulls in the
# definition of SIL_SYSTEM_CONFIG_HEADER and other #defines needed by SIL
# headers.

sample_DIR = $(TOPDIR)/sample
sample_SOURCES = $(sample_DIR)/sample.c
sample_CFLAGS = $(SIL_CLIENT_FLAGS) $(CFLAG_STD_C99)
ifeq ($(PLATFORM),windows)
    # Optionally include a command-line argument "-dump-d3d-shaders=<path>"
    # to precompile all Direct3D shaders and write them to the given file.
    sample_CFLAGS += $(if $(filter 1,$(DUMP_D3D_SHADERS)),$(CFLAG_DEFINE)DUMP_D3D_SHADERS)
endif
MODULES += sample


# Define help text for our custom rules (see below) so "make help" will
# list them.

ifneq (,$(filter linux macosx windows,$(PLATFORM)))
    SIL_HELP_coverage = 'build/run tests and generate HTML coverage results'
    SIL_HELP_test-coverage = 'build and run tests, writing coverage data to coverage.out'
endif


# Include the SIL build control file to perform all the work.

include $(TOPDIR)/build/$(PLATFORM)/build.mk


# Add extra rules for coverage testing.

.PHONY: coverage test-coverage

# Set this to a non-empty string to run only those tests for coverage testing.
SIL_COVERAGE_TESTS ?=
# Set this to 1 to use the "wine" program to run the executable.  Useful
# when cross-compiling to Windows from Linux or Mac systems.
USE_WINE ?= $(if $(and $(filter windows,$(PLATFORM)),$(filter %Linux Darwin,$(shell uname -s 2>/dev/null))),1)

ifneq (,$(filter linux macosx windows,$(PLATFORM)))
coverage: test-coverage
	@# Make sure this is executed after the tests even in parallel builds.
	$(Q)$(MAKE) coverage-html
test-coverage:
	$(Q)$(MAKE) all COVERAGE=1
	$(ECHO) 'Running tests'
	$(Q)find obj -name \*.gcda -delete
	$(Q)$(if $(filter 1,$(USE_WINE)),wine )'./$(EXECUTABLE_NAME)$(if $(filter windows,$(PLATFORM)),.exe)' -test$(if $(SIL_COVERAGE_TESTS),=$(SIL_COVERAGE_TESTS))
	$(Q)$(MAKE) gen-coverage
else
coverage test-coverage:
	$(error Can't automate test coverage analysis on platform "$(PLATFORM)")
endif
