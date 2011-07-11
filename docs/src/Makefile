DOCS_EN := $(patsubst %.txt,%,$(wildcard *.txt */*.txt))
DOCS_DE := $(patsubst %.txt,%,$(wildcard *_de.txt */*_de.txt))
DOCS_ES := $(patsubst %.txt,%,$(wildcard *_es.txt */*_es.txt))
DOCS_FR := $(patsubst %.txt,%,$(wildcard *_fr.txt */*_fr.txt))
LARGE_EN := $(patsubst %.txt,%,$(wildcard *.txt))
LARGE_DE := $(patsubst %.txt,%,$(wildcard *_de.txt))
LARGE_ES := $(patsubst %.txt,%,$(wildcard *_es.txt))
LARGE_FR := $(patsubst %.txt,%,$(wildcard *_fr.txt))
DOCS_EN := $(filter-out $(DOCS_DE), (filter-out $(DOCS_ES), (filter-out $(DOCS_FR), $(DOCS_EN)) ) )
SMALL := $(filter-out $(LARGE_EN),$(DOCS_EN))
DOCS  := $(DOCS_EN) $(DOCS_DE) $(DOCS_ES) $(DOCS_FR)
LARGE := $(LARGE_EN) $(LARGE_DE) $(LARGE_ES) $(LARGE_FR)

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

%.db: %.txt
	asciidoc -d book -o- -b docbook $< | xsltproc links.xslt - > $@ || (X=$$?; rm $@; exit $$X)

%.dep: %.txt
	./asciideps $< > $@.tmp
	mv $@.tmp $@

%.html: %.txt
	asciidoc -a linksfile=xref_en.links -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(DOCS_DE)) :: %.html: %.txt
#	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(DOCS_ES)) :: %.html: %.txt
#	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(DOCS_FR)) :: %.html: %.txt
#	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$$?; rm $@; exit $$X)

%.pdf: %.txt
	$(A2X) -L -d book -vf pdf $< || (X=$$?; rm $@; exit $$X)

$(patsubst %,%.html,$(LARGE)) :: %.html: %.txt
	$(A2X) --stylesheet=./linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(LARGE_DE)) :: %.html: %.txt
#	$(A2X) --stylesheet=../linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(LARGE_ES)) :: %.html: %.txt
#	$(A2X) --stylesheet=../linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

#$(patsubst %,%.html,$(LARGE_FR)) :: %.html: %.txt
#	$(A2X) --stylesheet=../linuxcnc.css -L -d book -vf xhtml $< || (X=$$?; rm $@; exit $$X)

.PHONY: all docs htmldocs pdfdocs docclean clean

