#ifeq ($(XILINX),)
#    $(error "source ~/xilinx/settings.sh" to set env vars)
#endif

vpath %.vhd ../5i2x_vhdl

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


%.t : %.vhd
	@echo "making $@"
	@rm -f $@
	@for FILE in $^ ; do \
	    if [ ! $$FILE = $< ] ; then \
		cat $$FILE >>$@ ;\
	    fi ;\
	done
	@echo $< >>$@

%.prj : %.t
	@echo "making $@"
	@rm -f $@
	@touch $@
	@for F in `cat $<` ; do if ! grep -q $$F $@ ; then echo "vhdl work $$F" >>$@ ; fi ; done
	@echo "nosort" >>$@

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

%.ngc %.log.syn : %.prj %.scr
	@echo "Synthesis: making $@"
	@xst -ifn $*.scr -ofn $*.log.syn

%.usage : %.log.syn
	@echo "Usage report:"
	@cat $< | awk '/Device utilization summary:/,/TIMING REPORT/' | head -n-3 | tail >$@
	@cat $@

%.ngd %.log.ngd : %.ngc
	@echo "NGDbuild: making $@ from $< with constraints from $(word 2,$^)"
	@ngdbuild -uc $(word 2,$^) $< $@
	@mv $*.bld $*.log.ngd

%.ncd %.log.map : %.ngd
	@echo "Mapping: making $@ from $^"
	@map $<
	@mv $*.mrp $*.log.map

%-pl.ncd : %.ncd
	@echo "Placing: making $@ from $^"
	@par -nopad -r $*.ncd -w $*-pl.ncd $*.pcf

%-rt.ncd : %-pl.ncd
	@rm -f $@
	@echo "Routing: making $@ from $^"
	@par -p $*-pl.ncd -w $*-rt.ncd $*.pcf

%.bit : %-rt.ncd
	@echo "Bitgen: making $@ from $^"
	@bitgen -w $*-rt.ncd $*.bit $*.pcf


.PRECIOUS : %.prj %.scr %.ngc %.ngd %.ncd %-pl.ncd %-rt.ncd %.bit %.0 %.bin %.ghw %.log.syn %.log.ngd %.log.map

makefile.depends : *.vhd
	@echo "Making $@"
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

include makefile.depends


