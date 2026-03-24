# Makefile for a Go HAL plugin (.so)
#
# Uses halcompile to locate LinuxCNC's Makefile.inc, which provides the
# exact Go binary, paths, and flags used to build LinuxCNC.  This guarantees
# the plugin is compiled with the same Go toolchain as the linuxcnc-launcher
# host binary (a hard requirement for Go plugins).
#
# Usage:
#   make            - build mygomodule.so in the current directory
#   make install    - install to EMC2_GOMOD_DIR (auto-detected via halcompile)
#   make clean      - remove build artifacts

PLUGIN_NAME ?= mygomodule
PLUGIN_SO   := $(PLUGIN_NAME).so
DESTDIR     ?=

# ---------------------------------------------------------------------------
# Locate Makefile.inc next to Makefile.modinc (works for both RIP and installed).
# ---------------------------------------------------------------------------
MODINC_PATH := $(shell halcompile --print-modinc 2>/dev/null)
ifneq ($(MODINC_PATH),)
MAKEFILE_INC := $(dir $(MODINC_PATH))Makefile.inc
-include $(MAKEFILE_INC)
endif

# Derive launcher source path from EMC2_HOME + RUN_IN_PLACE.
ifeq ($(RUN_IN_PLACE),yes)
LAUNCHER_DIR := $(EMC2_HOME)/src/launcher
else
LAUNCHER_DIR := $(datadir)/linuxcnc/launcher
endif

INSTALL_DIR  := $(EMC2_GOMOD_DIR)
CGO_LDFLAGS  ?= -Wl,-rpath,$(LIB_DIR)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all install clean check-env

all: check-env go.work $(PLUGIN_SO)

check-env:
	@if [ -z "$(MODINC_PATH)" ]; then \
		echo "Error: halcompile not found."; \
		echo "Source rip-environment or install LinuxCNC."; \
		exit 1; \
	fi
	@if [ ! -d "$(LAUNCHER_DIR)" ]; then \
		echo "Error: launcher not found at $(LAUNCHER_DIR)"; \
		exit 1; \
	fi
	@echo "GO:           $(GO)"
	@echo "launcher:     $(LAUNCHER_DIR)"
	@echo "RUN_IN_PLACE: $(RUN_IN_PLACE)"

go.work: go.mod
	@rm -f go.work go.work.sum
	GOTOOLCHAIN=local $(GO) work init
	GOTOOLCHAIN=local $(GO) work use .
	GOTOOLCHAIN=local $(GO) work edit \
		-replace github.com/sittner/linuxcnc/src/launcher=$(LAUNCHER_DIR)

$(PLUGIN_SO): main.go go.mod go.work
	CGO_ENABLED=1 \
	CGO_LDFLAGS="$(CGO_LDFLAGS)" \
	GOTOOLCHAIN=local \
	$(GO) build -buildmode=plugin -o $(PLUGIN_SO) .

install: $(PLUGIN_SO)
	install -d $(INSTALL_DIR)
	install -m 644 $(PLUGIN_SO) $(INSTALL_DIR)/$(PLUGIN_SO)

clean:
	rm -f $(PLUGIN_SO) go.work go.work.sum
