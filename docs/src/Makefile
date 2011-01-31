DOCS_EN = $(patsubst %.txt,%,$(shell find -name \*.txt))
DOCS_FR = $(patsubst %.txt,%,$(shell find ../fr -name \*.txt))
LARGE_EN = $(patsubst %.txt,%,$(shell ls ./*.txt))
LARGE_FR = $(patsubst %.txt,%,$(shell ls ../fr/*.txt))
SMALL = $(filter-out $(LARGE_EN),$(DOCS_EN))
DOCS  = $(DOCS_EN) $(DOCS_FR)
LARGE = $(LARGE_EN) $(LARGE_FR)

A2X = ./a2x --xsltproc-opts "--stringparam toc.section.depth 3 \
			   --stringparam toc.max.depth 2 \
			   --stringparam generate.section.toc.level 2 \
			   --stringparam generate.toc 'book toc,title chapter toc'" \
	  --asciidoc-opts "-f docbook.conf" \
	  --dblatex-opts "-P doc.publisher.show=0 -P latex.output.revhistory=0"

all:
	$(MAKE) -C ../../src docs

docs htmldocs pdfdocs:
	$(MAKE) -C ../../src $@

docclean clean:
	$(MAKE) -C ../../src docclean

pdf: $(patsubst %,%.pdf,$(LARGE))
html: links_db.py $(patsubst %,%.html,$(DOCS))
htmlclean:
	-rm -f $(patsubst %,%.html,$(DOCS))
dbclean:
	-rm -f $(patsubst %,%.db,$(SMALL))
	-rm -f links_db.py

links_db.py: $(patsubst %,%.db,$(SMALL))
	echo $(SMALL)
	./links_db_gen.py $^ > $@

%.db: %.txt
	asciidoc -d book -o- -b docbook $< | xsltproc links.xslt - > $@ || (X=$?; rm $@; exit $X)

%.html: %.txt
	asciidoc -a stylesheet=$(shell pwd)/linuxcnc.css -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $< || (X=$?; rm $@; exit $X)

%.pdf: %.txt
	a2x --asciidoc-opts "-f docbook.conf" -L -d book -vf pdf $< || (X=$?; rm $@; exit $X)

$(patsubst %,%.html,$(LARGE_EN)) :: %.html: %.txt
	$(A2X) --stylesheet=$(shell pwd)/linuxcnc.css -L -d book -vf xhtml $< || (X=$?; rm $@; exit $X)

$(patsubst %,%.html,$(LARGE_FR)) :: %.html: %.txt
	$(A2X) --stylesheet=$(shell pwd)/linuxcnc.css -a imagesdir=../src -L -d book -vf xhtml $< || (X=$?; rm $@; exit $X)

.PHONY: all docs htmldocs pdfdocs docclean clean
