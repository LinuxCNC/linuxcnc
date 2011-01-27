DOCS = $(patsubst %.txt,%,$(shell find -name \*.txt))
LARGE = $(patsubst %.txt,%,$(shell ls ./*.txt))
SMALL = $(filter-out $(LARGE),$(DOCS))

all:
	$(MAKE) -C ../../src docs

docs htmldocs pdfdocs:
	$(MAKE) -C ../../src $@

docclean clean:
	$(MAKE) -C ../../src docclean

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
	asciidoc -o- -b docbook $< | xsltproc links.xslt - > $@

%.html: %.txt
	asciidoc -f xhtml11.conf -d book -a toc -a numbered -b xhtml11 $<

$(patsubst %,%.html,$(LARGE)) :: %.html: %.txt
	a2x -L -d book -vf xhtml $<

.PHONY: all docs htmldocs pdfdocs docclean clean
