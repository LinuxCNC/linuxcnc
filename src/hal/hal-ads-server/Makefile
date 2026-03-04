# Makefile for hal-ads-server Go HAL component
# Works with installed LinuxCNC or RIP (Run-In-Place) build

COMPONENT := hal-ads-server

# Detect LinuxCNC environment
ifdef EMC2_HOME
    # RIP (Run-In-Place) environment
    CGO_CFLAGS := -I$(EMC2_HOME)/include -I$(EMC2_HOME)/src/hal -I$(EMC2_HOME)/src/rtapi -DULAPI
    CGO_LDFLAGS := -L$(EMC2_HOME)/lib -Wl,-rpath,$(EMC2_HOME)/lib -llinuxcnchal
else
    # Installed LinuxCNC
    CGO_CFLAGS := -I/usr/include/linuxcnc -DULAPI
    CGO_LDFLAGS := -L/usr/lib/linuxcnc -Wl,-rpath,/usr/lib/linuxcnc -llinuxcnchal
endif

# Go settings
GO := go
GOFLAGS := -v

.PHONY: all clean install uninstall

all: $(COMPONENT)

GO_SOURCES := $(wildcard *.go) $(wildcard ads/*.go) go.mod
$(COMPONENT): $(GO_SOURCES)
	@echo "Building $(COMPONENT)..."
	CGO_ENABLED=1 \
	CGO_CFLAGS="$(CGO_CFLAGS)" \
	CGO_LDFLAGS="$(CGO_LDFLAGS)" \
	$(GO) build $(GOFLAGS) -o $(COMPONENT) .

clean:
	rm -f $(COMPONENT)

# Detect LinuxCNC bin directory
ifdef EMC2_HOME
    # RIP environment
    LINUXCNC_BINDIR := $(EMC2_HOME)/bin
else
    # Installed LinuxCNC
    HALCOMPILE_PATH := $(shell which halcompile 2>/dev/null)
    ifneq ($(HALCOMPILE_PATH),)
        LINUXCNC_BINDIR := $(patsubst %/,%,$(dir $(HALCOMPILE_PATH)))
    endif
endif

install: all
ifndef LINUXCNC_BINDIR
	$(error Cannot find LinuxCNC bin directory. Source scripts/rip-environment or ensure LinuxCNC is in your PATH)
endif
	@echo "Installing $(COMPONENT) to $(LINUXCNC_BINDIR)"
	install -m 755 $(COMPONENT) $(LINUXCNC_BINDIR)/$(COMPONENT)

uninstall:
ifndef LINUXCNC_BINDIR
	$(error Cannot find LinuxCNC bin directory)
endif
	@echo "Removing $(COMPONENT) from $(LINUXCNC_BINDIR)"
	rm -f $(LINUXCNC_BINDIR)/$(COMPONENT)
