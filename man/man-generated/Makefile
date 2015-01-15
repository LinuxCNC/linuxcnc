#Makefile for generating .asciidoc from the manpages

MAN1_DIR = ../man/man1
MAN3_DIR = ../man/man3
MAN9_DIR = ../man/man9

MAN1_SOURCES = $(shell find $(MAN1_DIR) -type f -iname "*.1")
MAN3hal_SOURCES = $(shell find $(MAN3_DIR) -type f -iname "*.3hal")
MAN3hm2_SOURCES = $(shell find $(MAN3_DIR) -type f -iname "*.3hm2")
MAN3rtapi_SOURCES = $(shell find $(MAN3_DIR) -type f -iname "*.3rtapi")
MAN9_SOURCES = $(shell find $(MAN9_DIR) -type f -iname "*.9")
MAN9comp_SOURCES = $(shell find $(MAN9_DIR)-type f -iname "*.9comp")

MAN1_TARGETS = $(patsubst $(MAN1_DIR)/%,$(MAN1_DIR)/%.asciidoc,$(MAN1_SOURCES))
MAN3hal_TARGETS = $(patsubst $(MAN3_DIR)/%,$(MAN3_DIR)/%.asciidoc,$(MAN3hal_SOURCES))
MAN3hm2_TARGETS = $(patsubst $(MAN3_DIR)%,$(MAN3_DIR)/%.asciidoc,$(MAN3hm2_SOURCES))
MAN3rtapi_TARGETS = $(patsubst $(MAN3_DIR)/%,$(MAN3_DIR)/%.asciidoc,$(MAN3rtapi_SOURCES))
MAN9_TARGETS = $(patsubst $(MAN9_DIR)/%,$(MAN9_DIR)/%.asciidoc,$(MAN9_SOURCES))
MAN9comp_TARGETS = $(patsubst $(MAN9_DIR)/%,$(MAN9_DIR)/%.asciidoc,$(MAN9comp_SOURCES))

$(MAN1_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN3_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN3hal_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN3hm2_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN3rtapi_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN9_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

$(MAN9comp_TARGETS): %.asciidoc: %
	-python ../scripts/troff-to-asciidoc.py \
	$< $@

manpages: $(MAN1_TARGETS) $(MAN3_TARGETS) $(MAN3hal_TARGETS) \
	$(MAN3hm2_TARGETS) $(MAN3rtapi_TARGETS) $(MAN9comp_TARGETS) \
	$(MAN9_TARGETS)

clean:
	-rm -rf $(MAN1_TARGETS) $(MAN3_TARGETS) $(MAN3hal_TARGETS) \
	$(MAN3hm2_TARGETS) $(MAN3rtapi_TARGETS) $(MAN9comp_TARGETS) \
	$(MAN9_TARGETS)
