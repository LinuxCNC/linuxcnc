#ifeq ($(XILINX),)
#    $(error "source ~/xilinx/settings.sh" to set env vars)
#endif

ifeq ($(origin PYTHONPATH),undefined)
PYTHONPATH:=../../../../../lib/python
else
PYTHONPATH:=../../../../../lib/python:$(PYTHONPATH)
endif

vpath %.o work_sim

# This makefile doesn't build anything by default, you must explicitly
# give it a target to build - having 'help' as the first target prints
# this message if you don't specify a target.
help :
	@echo "This Makefile is NOT part of the EMC2 build system."
	@echo "It uses the zero-cost (non-free) Xilinx Webpack tools"
	@echo "to convert VHDL source into bitfiles for the FPGA."
	@echo "The completed bitfiles are distributed with EMC2, and"
	@echo "do not need to be recreated except to fix bugs or to"
	@echo "make a custom FPGA configuration."
	@echo "It also uses the Free Software programs GHDL and GtkWave"
	@echo "to allow simulation of the FPGA logic."

# The first group of targets are used for simulation.  Note that a top-
# level VHDL file suitable for simulation is not suitable for the FPGA
# toolchain, and vice versa.  A simulation VHDL file must provide a 
# 'testbench' and stimulus for the design under test.

# The .o target is an intermediate step.  There is one .o file for
# each .vhd file in the simulation.
%.o : %.vhd
	@echo "analysis: making $@"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	@ghdl -a --ieee=synopsys -fexplicit --workdir=./work_sim $<

# The .bin target is a binary which when executed runs the simulation.
%.bin : %.vhd %.o
	@echo "elaboration: making $@"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	@ghdl -e --ieee=synopsys -fexplicit --workdir=./work_sim -o $@\
	    `grep -m 1 "entity .* is" $< | \
	     sed -e 's/^.*entity  *//' -e 's/  *is.*$$//'`

# The .ghw target is the simulation output.  This target runs the sim
# and saves the results.
%.ghw : %.bin
	@echo "simulating: running $<"
	@if [ ! -d ./work_sim ] ; then mkdir work_sim; fi
	@rm -f $@
	./$< --wave=$@

# The .wave target invokes the waveform viewer and feeds it a waveform
# file.  Note that it does NOT generate a .wave file, hence if you command
# 'make foo.wave', it will always execute this target.  It will re-build
# and/or re-run the simulation only if the old results are out of date.
%.wave : %.ghw
	@gtkwave $< $*.sav &>/dev/null


# This is the beginning of the actual FPGA workflow (as opposed to
# the simulation workflow).
# It reads an FPGA spec and produces the top level VHDL file.
#
# Commented out because for some reason its being invoked even
# when a totally unrelated target is asked for.  work in progress
#
#%.vhd %.rspec : %.spec
#	@echo "making top level VHDL and ram spec from $<"
#	@python spec2vhdl.py $*

# The .t file for a .vhd file is a simple list of all .vhd files on
# which the specific .vhd file depends.  Strictly an intermediate
# file.
%.t : %.vhd
	@echo "making $@"
	@rm -f $@
	@for FILE in $^ ; do \
	    if [ ! $$FILE = $< ] ; then \
		cat $$FILE >>$@ ;\
	    fi ;\
	done
	@echo $< >>$@

# The .prj file is required by the xilinx tools.  among other things
# it tells the toolchain what individual vhd files are needed for the
# design.
%.prj : %.t
	@echo "making $@"
	@rm -f $@
	@touch $@
	@for F in `cat $<` ; do if ! grep -q $$F $@ ; then echo "vhdl work $$F" >>$@ ; fi ; done
	@echo "nosort" >>$@

# The .scr file is also required by the xilinx tools.  it is a sort
# of script which tells the first stage tool 'XST' what to do.
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
	@set -- `grep -m 1 -e "---[[:space:]][[:space:]]*device" $<` ; echo "-p $$3" >>$@

# The .ngc is the first target that is actually produced by the Xilinx
# toolchain.  It results from the 'synthesis' step, using the tool 'XST'.
%.ngc %.log.syn : %.prj %.scr
	@echo "Synthesis: making $@"
	@xst -ifn $*.scr -ofn $*.log.syn

# The .usage target is a short report predicting the number of CLBs, LUTs,
# flip-flops, etc that the design will use.  This is produced by parsing
# the log file from the synthesis step.
# The report is dependent on the output format of the Xilinx tools.
# If they change the text of their report, it will be necessary to change the
# search text, and possibly to use different strings for various versions.
%.usage : %.log.syn
	@echo "Usage report:"
	@cat $< | awk '/Selected Device/,/TIMING REPORT/' | head -n-3 >$@
	@cat $@

# The .ngd file is an intermediate step in the Xilinx chain, which adds the
# pin location and timing constraints to the design.
%.ngd %.log.ngd : %.ngc
	@echo "NGDbuild: making $@ from $< with constraints from $(word 2,$^)"
	@ngdbuild -uc $(word 2,$^) $< $@
	@mv $*.bld $*.log.ngd

# The extensions .nad and .nbd are actually .ncd files - the toolchain
# uses .ncd for unplaced, placed-but-not-routed, and placed-and-routed files
# The makefile uses distinct extensions to keep the dependencies straight,
# and to preserve the intermediate files.  'a' and 'b' are simply used to
# indicate the ordering in the process, 'a' comes first.

# The .nad file is mapped but not placed or routed.
%.nad : %.ngd
	@echo "Mapping: making $@ (unplaced) from $^"
	@map $<
	@mv $*.ncd $*.nad
	@mv $*.mrp $*.log.map

# The .nbd file is mapped and placed, but not routed.
%.nbd : %.nad
	@echo "Placing: making $@ (unrouted) from $^"
	@cp $*.nad raw.ncd
	@par -nopad -r raw.ncd -w placed.ncd $*.pcf
	@rm raw.ncd
	@mv placed.ncd $*.nbd
	@rm placed.unroutes
	@rm placed.par
	@rm placed.xpi

# The .ncd file is mapped, placed, and routed.
%.ncd : %.nbd
	@rm -f $@
	@cp $*.nbd placed.ncd
	@echo "Routing: making $@ (finished) from $^"
	@par -p placed.ncd -w $*.ncd $*.pcf
	@rm placed.ncd

# The .bit file is the actual bitstream, in Xilinx bitfile format.
%.bit : %.ncd
	@echo "Bitgen: making $@ from $^"
	@bitgen -w $*.ncd $*.bit $*.pcf

# The .fpga file is a bitfile with additional information that is
# used by our configuration tools and by the HAL driver.
%.fpga : %.rspec %.bit
	@echo "bit2fpga: Merging $^ into $@"
	@python bit2fpga.py $*

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

# 'makefile.vhd.dep' contains dependency information.  Individual VHDL
# files make use of others, but the mechanism used doesn't help make
# figure things out, so we do some grepping to capture the dependencies.
# In addition, individual top level designs depend on constraints files,
# and this file ensures that a change to a constraint file will rebuild
# things that depend on it.
makefile.vhd.dep : *.vhd
	@echo "Making dependency file $@"
	@touch $@
	@for FILE in $? ; do BASE=$${FILE/\.vhd/} ;\
	    grep -v "^$$BASE.o :" $@ | grep -v "^$$BASE.t :" | grep -v "^$$BASE.ngd :" >$@.tmp ;\
	    mv $@.tmp $@ ;\
	    echo -n "$$BASE.o :" >>$@ ;\
	    grep -o "use  *work\..*_pkg" $$FILE | while read i ; do \
	        echo -n $$i | sed "s/use  *work\./ /" | sed "s/_pkg/.o/" | sed "s/$$BASE.o//" >>$@ ;\
	    done ;\
	    echo " " >>$@ ;\
	    echo -n "$$BASE.t :" >>$@ ;\
	    grep -o "use  *work\..*_pkg" $$FILE | while read i ; do \
	        echo -n $$i | sed "s/use  *work\./ /" | sed "s/_pkg/.t/" | sed "s/$$BASE.t//" >>$@ ;\
	    done ;\
	    echo " " >>$@ ;\
	    if grep -q -e "--- *constraints" $$FILE ; then \
	        echo -n "$$BASE.ngd :" >>$@ ;\
	        set -- `grep -e "--- *constraints" $$FILE` ; echo " $$3" >>$@ ;\
	    fi ;\
	done

include makefile.vhd.dep

