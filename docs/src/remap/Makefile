# rebuild svg and eps files from mscgen inputs
# see http://www.mcternan.me.uk/mscgen/

# include in docs like so:
#image::images/iocontrol-prepare.{basebackend-docbook!svg}{basebackend-docbook?eps}[]

MSCGEN=/usr/local/bin/mscgen

%.svg: ../%.msc
	$(MSCGEN) -T svg -i $< -o $@

%.eps: ../%.msc
	$(MSCGEN) -T eps  -i $< -o $@

all:	$(patsubst %.msc,images/%.svg,$(wildcard *.msc)) \
	$(patsubst %.msc,images/%.eps,$(wildcard *.msc))	





