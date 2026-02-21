# Makefile for joint_ctrl — standalone single-joint HAL component
#
# Uses the standard LinuxCNC out-of-tree module build via Makefile.modinc.
#
# Build in a RIP (run-in-place) environment:
#   source /path/to/linuxcnc/scripts/rip-environment
#   make
#
# Build against an installed LinuxCNC:
#   make
#
# Install to the LinuxCNC rtlib directory:
#   make install

ifeq ($(MODINC),)
  MODINC := $(shell pkg-config --variable=modinc linuxcnc 2>/dev/null)
endif
ifeq ($(MODINC),)
  MODINC := $(shell linuxcnc-config --modinc 2>/dev/null)
endif
ifeq ($(MODINC),)
  $(error Cannot find Makefile.modinc. \
    Source the LinuxCNC rip-environment, or install LinuxCNC and retry.)
endif

# The module is named joint_ctrl.  Because Makefile.modinc would
# otherwise try to compile joint_ctrl.c (same stem as the module),
# the main translation unit is named joint_ctrl_main.c.
obj-m += joint_ctrl.o
joint_ctrl-objs := joint_ctrl_main.o joint_simple_tp.o joint_homing.o

EXTRA_CFLAGS += -I$(CURDIR)

include $(MODINC)
