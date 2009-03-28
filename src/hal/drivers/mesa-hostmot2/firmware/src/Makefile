#ifeq ($(XILINX),)
#    $(error "source ~/xilinx/settings.sh" to set env vars)
#endif

vpath %.o work_sim

help :
	@echo "This Makefile is NOT part of the EMC2 build system."
	@echo "It uses the zero-cost (non-free) Xilinx Webpack tools"
	@echo "to convert VHDL source into bitfiles for the FPGA."
	@echo "The completed bitfiles are distributed with EMC2, and"
	@echo "do not need to be recreated except to fix bugs or to"
	@echo "make a custom FPGA configuration."
	@echo "It also uses the Free Software programs GHDL and GtkWave"
	@echo "to allow simulation of the FPGA logic."

# The following four targets are not part of the process of making a bitfile
# They are for simulation only, and will probably not work with the same VHDL
# source that can be used to make a bitfile

%.o : %.vhd
	@echo "analysis: making $@"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	@ghdl -a --ieee=synopsys -fexplicit --workdir=./work_sim $<

%.bin : %.vhd %.o
	@echo "elaboration: making $@"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	@ghdl -e --ieee=synopsys -fexplicit --workdir=./work_sim -o $@\
	    `grep -m 1 "entity .* is" $< | \
	     sed -e 's/^.*entity  *//' -e 's/  *is.*$$//'`

%.ghw : %.bin
	@echo "simulating: running $<"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	./$< --wave=$@

%.wave : %.ghw
	@gtkwave $< $*.sav &>/dev/null

# The bitfile build process starts here

# *.dep files are used for dependency tracking.  File foo.dep contains
# the names of every lower level .dep file that foo.vhd needs, followed
# by 'foo.vhd', the name of the upper level file.  Since foo.dep depends
# on foo.vhd, it is re-made when foo.vhd changes.  In addition, foo.dep
# depends on the lower level *.dep files (by way of makefile.vhd.dep),
# so it is remade if any of the lower level *.vhd files change.

# Note that a .dep file may contain repeated mentions of the same .vhd
# file, if that file is needed by more than one lower-level file.  The
# .vhd files listed in a .dep file are in order from the lowest level up
# to the top, so if compiled in order, all dependencies will be met.

%.dep : %.vhd
	@echo "Making $@"
	@rm -f $@
	@for FILE in $^ ; do \
	    if [ ! $$FILE = $< ] ; then \
		cat $$FILE >>$@ ;\
	    fi ;\
	done
	@echo $< >>$@

# the .prj file for a top-level .vhd file consists of a list of all
# the .vhd files that are needed to build that top-level.  A .prj file
# is only created for a top-level .vhd when a build of that .vhd is
# attempted.  Since the .prj file depends on the top-level .dep file,
# and that is the top of a pyramid of .dep dependencies, any change to
# any of the lower level .vhd files results in a rebuild.

# The 'grep -q' below ensures that duplicate .vhd filenames from the top-
# level .dep file are removed while preserving the order of the files.

%.prj : %.dep
	@echo "making $@"
	@rm -f $@
	@touch $@
	@for F in `cat $<` ; do if ! grep -q $$F $@ ; then echo "vhdl work $$F" >>$@ ; fi ; done
	@echo "nosort" >>$@

# synthesis requires a script with various info, including items
# that aren't in the VHDL source, like the device type

%.scr : %.vhd
	@echo "making $@"
	@if [ ! -d ./tmp_syn ] ; then mkdir tmp_syn; fi
	@echo "set -tmpdir ./tmp_syn" >$@
	@if [ ! -d ./work_syn ] ; then mkdir work_syn; fi
	@echo "set -xsthdpdir ./work_syn" >>$@
	@echo "run" >>$@
	@echo "-ifmt VHDL -ifn $*.prj" >>$@
	@echo "-top `grep -m 1 "entity .* is" $< | sed -e 's/^.*entity  *//' -e 's/  *is.*$$//'`" >>$@
	@echo "-ofmt NGC -ofn $*.ngc" >>$@
	@echo "-p $(DEVICE)" >>$@

# this step uses the script generated above to actually do the synthesis

%.ngc %.log.syn : %.prj %.scr
	@echo "Synthesis: making $@"
	@xst -ifn $*.scr -ofn $*.log.syn

# The .usage target is dependent on the output format of the Xilinx tools.
# If they change the text of their report, it will be necessary to change the
# search text, and possibly to use different strings for various versions.
%.usage : %.log.syn
	@echo "Usage report:"
	@cat $< | awk '/Selected Device/,/TIMING REPORT/' | head -n-3 >$@
	@cat $@

# this is the point at which the design constraints (mostly pinout) are merged

%.ngd %.log.ngd : %.ngc
	@echo "NGDbuild: making $@ from $< with constraints from $(CONSTRAINTS)"
	@ngdbuild -uc $(CONSTRAINTS) $< $@
	@mv $*.bld $*.log.ngd

# The extensions .nad and .nbd are actually .ncd files - the toolchain
# uses .ncd for unplaced, placed-but-not-routed, and placed-and-routed files
# The makefile uses distinct extensions to keep the dependencies straight,
# and to preserve the intermediate files.

# .nad is the design with everything in it, but nothing placed or routed

%.nad : %.ngd
	@echo "Mapping: making $@ (unplaced) from $^"
	@map $<
	@mv $*.ncd $*.nad
	@mv $*.mrp $*.log.map

# .nbd is the placed design - this step usually takes a while

%.nbd : %.nad
	@echo "Placing: making $@ (unrouted) from $^"
	@cp $*.nad raw.ncd
	@par -nopad -r raw.ncd -w placed.ncd $*.pcf
	@rm raw.ncd
	@mv placed.ncd $*.nbd
	@rm placed.unroutes
	@rm placed.par
	@rm placed.xpi

# .ncd is the routed design - this step takes even longer

%.ncd : %.nbd
	@rm -f $@
	@cp $*.nbd placed.ncd
	@echo "Routing: making $@ (finished) from $^"
	@par -p placed.ncd -w $*.ncd $*.pcf
	@rm placed.ncd

# .bit is the bitfile to be loaded into the FPGA

%.bit : %.ncd
	@echo "Bitgen: making $@ from $^"
	@bitgen -w $*.ncd $*.bit $*.pcf

# FIXME - dunno what a .mcs file is, document it

%.mcs: %.bit
	echo "setmode -promfile" > $@.impact
	echo "setsubmode -pffserial" >> $@.impact
	echo "addPromDevice -p 1 -name xcf02s" >> $@.impact
	echo "addDesign -version 0 -name 0" >> $@.impact
	echo "addDeviceChain -index 0" >> $@.impact
	echo "addDevice -position 1 -file $<" >> $@.impact
	echo "generate -format mcs -fillvalue FF -output $@" >> $@.impact
	echo quit >> $@.impact
	impact -batch $@.impact
	rm -f $@.impact

# FIXME - dunno what a .scf file is, document it

%.svf: %.bit
	echo "setmode -bs" > $@.impact
	echo "setCable -port svf -file $@" >> $@.impact
	echo "addDevice -position 1 -file $<" >> $@.impact
	echo quit >> $@.impact
	impact -batch $@.impact
	rm -f $*.impact

.PRECIOUS : %.vhd %.rspec %.prj %.scr %.ngc %.ngd %.nad %.nbd %.ncd %.bit %.o %.bin %.ghw %.log.syn %.log.ngd %.log.map

.PHONY : clean clean_all

# because placed and routed .fpga files might represent many hours
# of work, 'make clean' doesn't remove them or their precursers,
# it only removes incidental files that are generated along the way
# if you truly want to remove everything, use 'make clean_all'
# if you want to redo only a single FPGA file, touch the
# corresponding .spec file
clean :
	@rm -f *.scr *.o *.bin
	@rm -f *.pad *_pad.csv *_pad.txt *.par *.unroutes
	@rm -f *.ghw *.log.syn *.log.ngd *.log.map *.bgn *.drc *.ngm *.usage
	@rm -f *_usage.xml *~

clean_all :
	@rm -f *.prj *.scr *.ngc *.ngd *.nad *.nbd *.ncd *.o *.bin *.bit
	@rm -f *.pad *_pad.csv *_pad.txt *.par *.unroutes *.xpi *.rspec *.fpga
	@rm -f *.ghw *.log.syn *.log.ngd *.log.map *.bgn *.drc *.ngm *.usage
	@rm -f *.pcf *.dep *_usage.xml *~

# dependency tracking

makefile.vhd.dep : *.vhd mk_vhdl_deps.py
	@echo "Making $@"
	@rm -f $@
	@./mk_vhdl_deps.py

include makefile.vhd.dep
