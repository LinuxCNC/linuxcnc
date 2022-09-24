asciidoc -f ../docs/src/xhtml11.conf \
         -f ../docs/src/asciidoc-dont-replace-arrows.conf \
         -f ../docs/src/source-highlight/emc-langs-source-highlight.conf \
         -a "source_highlight_dir=../docs/src/source-highlight/local" \
         -a linkcss \
         -a 'scriptsdir=../' -a 'stylesdir=../' \
         -a "scriptdir=../docs/src/" \
         -a "relindir=code" \
         -a "linksfile=objects/xref_en.links" \
         -a stylesheet=linuxcnc.css \
         -d book -a toc -a numbered -b xhtml11 -o temp.html $1

cp -f temp.html ../docs/html/code/temp.html