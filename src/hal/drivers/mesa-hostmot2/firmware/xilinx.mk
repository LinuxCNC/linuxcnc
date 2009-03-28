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

%.prj :
	@echo "making $@"
	@rm -f $@
	@for i in $(SOURCES); do echo $$i; done > $@

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

%.ngd %.log.ngd : %.ngc
	@echo "NGDbuild: making $@ from $< with constraints from $(CONSTRAINTS)"
	@ngdbuild -uc $(CONSTRAINTS) $< $@
	@mv $*.bld $*.log.ngd

# The extensions .nad and .nbd are actually .ncd files - the toolchain
# uses .ncd for unplaced, placed-but-not-routed, and placed-and-routed files
# The makefile uses distinct extensions to keep the dependencies straight,
# and to preserve the intermediate files.

%.nad : %.ngd
	@echo "Mapping: making $@ (unplaced) from $^"
	@map $<
	@mv $*.ncd $*.nad
	@mv $*.mrp $*.log.map

%.nbd : %.nad
	@echo "Placing: making $@ (unrouted) from $^"
	@cp $*.nad raw.ncd
	@par -nopad -r raw.ncd -w placed.ncd $*.pcf
	@rm raw.ncd
	@mv placed.ncd $*.nbd
	@rm placed.unroutes
	@rm placed.par
	@rm placed.xpi

%.ncd : %.nbd
	@rm -f $@
	@cp $*.nbd placed.ncd
	@echo "Routing: making $@ (finished) from $^"
	@par -p placed.ncd -w $*.ncd $*.pcf
	@rm placed.ncd

%.bit : %.ncd
	@echo "Bitgen: making $@ from $^"
	@bitgen -w $*.ncd $*.bit $*.pcf

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

%.svf: %.bit
	echo "setmode -bs" > $@.impact
	echo "setCable -port svf -file $@" >> $@.impact
	echo "addDevice -position 1 -file $<" >> $@.impact
	echo quit >> $@.impact
	impact -batch $@.impact
	rm -f $*.impact

# a .fpga file is a bitfile with config ram info added

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
	@rm -f *.pcf *.t *_usage.xml *~
