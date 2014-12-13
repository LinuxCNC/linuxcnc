DOCS_EN := $(patsubst %.asciidoc,%,$(wildcard *.asciidoc */*.asciidoc))
DOCS_ES := $(patsubst %.asciidoc,%,$(wildcard *_es.asciidoc */*_es.asciidoc))
DOCS_FR := $(patsubst %.asciidoc,%,$(wildcard *_fr.asciidoc */*_fr.asciidoc))
LARGE_EN := $(patsubst %.asciidoc,%,$(wildcard *.asciidoc))
LARGE_ES := $(patsubst %.asciidoc,%,$(wildcard *_es.asciidoc))
LARGE_FR := $(patsubst %.asciidoc,%,$(wildcard *_fr.asciidoc))
DOCS_EN := $(filter-out $(DOCS_ES), (filter-out $(DOCS_FR), $(DOCS_EN)) )
SMALL := $(filter-out $(LARGE_EN),$(DOCS_EN))
DOCS  := $(DOCS_EN) $(DOCS_ES) $(DOCS_FR)
LARGE := $(LARGE_EN) $(LARGE_ES) $(LARGE_FR)

#include $(patsubst %,%.dep,$(LARGE))

A2X = ./a2x --xsltproc-opts "--stringparam toc.section.depth 3 \
			   --stringparam toc.max.depth 2 \
			   --stringparam generate.section.toc.level 2 \
			   --stringparam generate.toc 'book toc,title chapter toc'" \
	  --asciidoc-opts "-f docbook.conf" \
	  --dblatex-opts "-P doc.publisher.show=0 -P latex.output.revhistory=0 -s ./emc2.sty"

all:
	$(MAKE) -C ../../src docs

docs htmldocs pdfdocs:
	$(MAKE) -C ../../src $@

docclean clean:
	$(MAKE) -C ../../src docclean

pdf: $(patsubst %,%.pdf,$(LARGE))
html: xref_en.links $(patsubst %,%.html,$(DOCS))
htmlclean:
	-rm -f $(patsubst %,%.html,$(DOCS))
dbclean:
	-rm -f $(patsubst %,%.dep,$(DOCS))
	-rm -f $(patsubst %,%.db,$(SMALL))
	-rm -f xref_en.links

xref_en.links: $(patsubst %,%.db,$(SMALL))
	echo $(SMALL)
	./links_db_gen.py $^ > $@

%.db: %.asciidoc
	asciidoc -d book -o- -b docbook $< | xsltproc links.xslt - > $@ || (X=$$?; rm $@; exit $$X)

%.dep: %.asciidoc
	./asciideps $< > $@.tmp
	mv $@.tmp $@

%.html: %.asciidoc
	asciidoc -a linksfile=xref_en.links -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(DOCS_ES)) :: %.html: %.asciidoc
#	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(DOCS_FR)) :: %.html: %.asciidoc
#	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

%.pdf: %.asciidoc
	$(A2X) -L -d book -vf pdf $< || (X=$$?; rm $@; exit $$X)

$(patsubst %,%.html,$(LARGE)) :: %.html: %.asciidoc
	$(A2X) --stylesheet=./linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(LARGE_ES)) :: %.html: %.asciidoc
#	$(A2X) --stylesheet=../linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(LARGE_FR)) :: %.html: %.asciidoc
#	$(A2X) --stylesheet=../linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

.PHONY: all docs htmldocs pdfdocs docclean clean
