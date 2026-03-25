# tests/Makefile — build and run the PDO-count regression test
#
# Usage:
#   make            — build and run (reports PASS/FAIL/SKIP per driver)
#   make build      — build only
#   make run        — run an already-built binary
#   make clean      — remove build artefacts

CC      ?= gcc
SRCDIR  := ../src

# Stub headers shadow hal.h, ecrt.h, rtapi*.h so the build succeeds
# without LinuxCNC or EtherLab installed.
CFLAGS  := -Wall -Wno-unused-function \
           -I stubs \
           -I $(SRCDIR) \
           -std=gnu99 -g

# All device and class source files (compiled as separate translation units
# so that static symbols in each driver do not conflict with each other).
DEV_SRCS   := $(wildcard $(SRCDIR)/devices/*.c)
CLASS_SRCS := $(SRCDIR)/classes/class_ax5.c \
              $(SRCDIR)/classes/class_enc.c
TYPE_SRC   := $(SRCDIR)/typelist.c
TEST_SRC   := test_pdo_counts.c

ALL_SRCS := $(TEST_SRC) $(TYPE_SRC) $(DEV_SRCS) $(CLASS_SRCS)

.PHONY: all build run clean

all: build run

build: test_pdo_counts

test_pdo_counts: $(ALL_SRCS) $(wildcard stubs/*.h) \
                 $(wildcard $(SRCDIR)/*.h) \
                 $(wildcard $(SRCDIR)/devices/*.h) \
                 $(wildcard $(SRCDIR)/classes/*.h)
	$(CC) $(CFLAGS) -o $@ $(ALL_SRCS) -lm

run: test_pdo_counts
	./test_pdo_counts

clean:
	rm -f test_pdo_counts
