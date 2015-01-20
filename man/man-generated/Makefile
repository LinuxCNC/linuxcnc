all: manpages

MAN_SECTIONS = 1 3hal 3hm2 3rtapi 9 9comp

define MAN_SECTION_GENERATE
MAN$(1)_DIR = ../man/man$(shell echo $(1) | awk '{ print substr($$0,1,1) }')
MAN$(1)_asciidoc_DIR = ../man/man-generated/man$(1)
MAN$(1)_SOURCES = $$(wildcard $$(MAN$(1)_DIR)/*.$(1))
MAN$(1)_TARGETS = \
	$$(patsubst $$(MAN$(1)_DIR)/%,$$(MAN$(1)_asciidoc_DIR)/%.asciidoc,\
	$$(MAN$(1)_SOURCES))

$$(MAN$(1)_TARGETS): \
$$(MAN$(1)_asciidoc_DIR)/%.asciidoc: $$(MAN$(1)_DIR)/%
	@mkdir -p $$(dir $$@)
	python ../scripts/troff-to-asciidoc.py \
		$$< $$@

endef
$(foreach sect,$(MAN_SECTIONS),$(eval $(call MAN_SECTION_GENERATE,$(sect))))


# To debug above functions, use e.g. 'make MAN_DEBUG=3hm2'
ifdef MAN_DEBUG
$(info MAN$(MAN_DEBUG)_DIR = $(MAN$(MAN_DEBUG)_DIR))
$(info MAN$(MAN_DEBUG)_asciidoc_DIR = $(MAN$(MAN_DEBUG)_asciidoc_DIR))
$(info MAN$(MAN_DEBUG)_SOURCES = $(MAN$(MAN_DEBUG)_SOURCES))
$(info MAN$(MAN_DEBUG)_TARGETS = $(MAN$(MAN_DEBUG)_TARGETS))
$(info --)
$(info $(call MAN_SECTION_GENERATE,$(MAN_DEBUG)))
endif

MAN_TARGETS = $(foreach sect,$(MAN_SECTIONS),$(MAN$(sect)_TARGETS))

manpages: $(MAN_TARGETS)

manclean:
	-rm -rf $(MAN_TARGETS)
