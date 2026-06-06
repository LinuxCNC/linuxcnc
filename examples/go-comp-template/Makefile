# Makefile for a Go module compiled into gomc-server
#
# Uses modcompile to locate LinuxCNC paths and compile the module
# into the gomc-server binary.
#
# Usage:
#   make            - verify the module compiles locally
#   make install    - copy into gomc, register, and rebuild gomc-server
#   make uninstall  - remove from gomc and rebuild gomc-server
#   make clean      - remove local build artifacts

MODULE_NAME ?= mygomodule

# ---------------------------------------------------------------------------
# Get paths from modcompile
# ---------------------------------------------------------------------------
MODCOMPILE := $(shell command -v modcompile 2>/dev/null)
ifneq ($(MODCOMPILE),)
$(eval $(shell $(MODCOMPILE) --print-make-inc))
endif

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all compile install uninstall clean check-env

all: compile

check-env:
	@if [ -z "$(MODCOMPILE)" ]; then \
		echo "Error: modcompile not found in PATH."; \
		echo "Source rip-environment or install LinuxCNC."; \
		exit 1; \
	fi
	@echo "GO:       $(GOMC_GO)"
	@echo "gomc dir: $(GOMC_DIR)"

# Compile locally to verify the module builds before installing.
# Uses go.work to resolve the gomc module dependency from the local tree.
compile: check-env go.work
	@echo "Compiling $(MODULE_NAME)..."
	CGO_ENABLED=1 \
	CGO_LDFLAGS="-Wl,-rpath,$(GOMC_LIB_DIR)" \
	GOTOOLCHAIN=local \
	$(GOMC_GO) build ./...
	@echo "$(MODULE_NAME) compiles OK"

go.work: go.mod
	@rm -f go.work go.work.sum
	GOTOOLCHAIN=local $(GOMC_GO) work init
	GOTOOLCHAIN=local $(GOMC_GO) work use .
	GOTOOLCHAIN=local $(GOMC_GO) work edit \
		-replace github.com/sittner/linuxcnc/src/gomc=$(GOMC_DIR)

# Install: copy source into gomc/external/<name>/, register, and rebuild.
install: check-env compile
	$(MODCOMPILE) add-gomod .

# Uninstall: remove from registry and delete source copy.
# Uses the directory name, which is the registry key for add-gomod.
uninstall: check-env
	$(MODCOMPILE) rm-gomod $(notdir $(CURDIR))

clean:
	rm -f go.work go.work.sum
