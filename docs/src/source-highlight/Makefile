# the source-highlight language defs and lang.map dir
HL_DIR=/usr/share/source-highlight
DEPS=$(wildcard $(HL_DIR)/*)

LOC_HL_DIR=.

# languages for which we provide our own .lang files
# this includes overriding languages listed in HL_DIR
EMCLANGS=$(wildcard *.lang)

# grep arg to suppress user defined langs already in lang.map
GREPARG= $(patsubst %, -e %,$(EMCLANGS))

# examples
PDF_TARGETS := $(patsubst %.txt, %.pdf, $(wildcard *.txt))
HTML_TARGETS := $(patsubst %.txt, %.html ,$(wildcard *.txt))

# tests with HTML output without going through asciidoc
TEST_SRCS := hal-test.hal ini-test.ini ngc-test.ngc
TEST_TARGETS := hal-test.html ini-test.html ngc-test.html


TARGET=local/lang.map
ASCIIDOC_ARGS=-a source_highlight_dir=local -f emc-langs-source-highlight.conf

all:	$(TARGET)

$(TARGET): $(EMCLANGS) $(DEPS)
ifeq (,$(findstring lang.map,$(wildcard $(HL_DIR)/*)))#
	$(error $(HL_DIR)/lang.map does not exist - are you sure \
	the 'source-highlight'  package is installed?)
endif
	rm -rf local
	cp -r $(HL_DIR) local
	mv local/lang.map local/lang.map.dist
	cp *.lang local
	grep -v $(GREPARG) local/lang.map.dist >$(TARGET)
	for i in $(EMCLANGS); do \
             echo `basename $$i .lang` '=' $$i >>$(TARGET) ; \
        done

examples: $(TARGET) $(HTML_TARGETS) $(PDF_TARGETS) tests


tests: 	$(TARGET) $(TEST_TARGETS)


%.html:	%.ini
	source-highlight --data-dir=local --input $< --output $@

%.html:	%.ngc
	source-highlight --data-dir=local --input $< --output $@

%.html:	%.hal
	source-highlight --data-dir=local --input $< --output $@

clean:
	-rm -rf local $(HTML_TARGETS) $(PDF_TARGETS) $(TEST_TARGETS)

$(PDF_TARGETS):  %.pdf: %.txt
	a2x -v -f pdf $<

$(HTML_TARGETS): %.html: %.txt
	asciidoc $(ASCIIDOC_ARGS) -v  $<
