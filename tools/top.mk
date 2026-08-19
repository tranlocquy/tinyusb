ifneq ($(lastword a b),b)
$(error This Makefile require make 3.81 or newer)
endif

# Detect whether shell style is windows or not
# https://stackoverflow.com/questions/714100/os-detecting-makefile/52062069#52062069
ifeq '$(findstring ;,$(PATH))' ';'
CMDEXE := 1
endif

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.

THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TOP := $(patsubst %/tools/top.mk,%,$(THIS_MAKEFILE))

ifeq ($(CMDEXE),1)
TOP := $(subst \,/,$(shell for %%i in ( $(TOP) ) do echo %%~fi))
else
TOP := $(shell realpath $(TOP))
endif
#$(info Top directory is $(TOP))

ifeq ($(CMDEXE),1)
CURRENT_PATH := $(subst $(TOP)/,,$(subst \,/,$(shell echo %CD%)))
else
# GNU realpath's --relative-to option is not available on BSD/macOS. CURDIR
# and TOP are already absolute here, so make can derive the in-tree path
# without invoking a host-specific utility.
CURRENT_PATH := $(patsubst $(TOP)/%,%,$(CURDIR))
endif
#$(info Path from top is $(CURRENT_PATH))
