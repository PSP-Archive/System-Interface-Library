#
# System Interface Library for games
# Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
# Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
# See the file COPYING.txt for details.
#
# build/common/base.mk: Common definitions for all Makefiles.
#

#
# This file defines some basic convenience variables useful in all
# Makefiles (not limited to the build rules for the main program).
#

###########################################################################
########################## Convenience variables ##########################
###########################################################################

# $(TOPDIR):  The top directory of the program's source tree.  By default,
# this is set to the top directory of the SIL source tree; to change it for
# your program, define $(TOPDIR) to a different value in your Makefile.

_SIL_TOPDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)
TOPDIR ?= $(_SIL_TOPDIR)

#-------------------------------------------------------------------------#

# $(Q), $(ECHO):  Variables used for abbreviated command output.  By
# writing commands like:
#     $(ECHO) 'Building target-file'
#     $(Q)command-1 target-file source-file
#     $(Q)command-2 target-file other-file
#     $(Q)...
# make will output only "Building target-file" normally, but it will instead
# output each command executed if "V=1" is given on the make command line.

ifneq ($(V),)
Q :=
ECHO := @:
else
Q := @
ECHO := @echo
endif

#-------------------------------------------------------------------------#

# $(,):  Defined to allow quoting commas in $(if ...) and similar
# expressions.  For example:
#     SYS_LDFLAGS = $(if $(filter 1,$(DEBUG)),-Wl$(,)--some-option)
# would expand to "-Wl,--some-option" if DEBUG is set to 1.  Note that
# without the quoted comma ("$(,)"), "-Wl" and "--some-option" would be
# treated as two separate arguments to $(if).  Also note that "$," without
# parentheses doesn't work (the comma is still treated as an argument
# separator in that case).

, := ,

###########################################################################
###########################################################################
