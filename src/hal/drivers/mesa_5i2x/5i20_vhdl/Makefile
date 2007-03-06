
help :
	@echo "This Makefile is NOT part of the EMC2 build system."
	@echo "It uses the zero-cost (non-free) Xilinx Webpack tools"
	@echo "to convert VHDL source into bitfiles for the FPGA."
	@echo "The completed bitfiles are distributed with EMC2, and"
	@echo "do not need to be recreated except to fix bugs or to"
	@echo "make a custom FPGA configuration."

%.prj : %.vhd
	@echo "making project file $@ from top level file $<"
	@echo "vhdl work $<" >$@
	@grep -e "---[[:space:]][[:space:]]*include" $< | while read i ; do set -- $$i ; echo "vhdl work $$3" ; done >>$@

%.scr : %.vhd
	@echo "making synthesis script file $@ from top level file $<"
	@echo "set -tmpdir ./tmp" >$@
	@echo "set -xsthdpdir ./work" >>$@
	@echo "run" >>$@
	@echo "-ifmt VHDL -ifn $*.prj" >>$@
	@set -- `grep -m 1 -e "---[[:space:]][[:space:]]*topentity" $<` ; echo "-top $$3" >>$@
	@echo "-ofmt NGC -ofn $*.ngc" >>$@
	@set -- `grep -m 1 -e "---[[:space:]][[:space:]]*device" $<` ; echo "-p $$3" >>$@

%.ngc : %.prj %.scr
	@echo "Synthesis: making $@"
	@xst -ifn $*.scr -ofn $*.log.synthesis

%.ngd : %.ngc
	@echo "NGDbuild: making $@ from $< with constraints from $(word 2,$^)"
	@ngdbuild -uc $(word 2,$^) $< $@
	@mv $*.bld $*.log.ngdbuild

%.ncd : %.ngd
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


.PRECIOUS : %.prj %.scr %.ngc %.ngd %.ncd %-placed.ncd %-routed.ncd %.bit

makefile.depends : *.vhd
	@echo "updating dependency file $@"
	@echo " " >$@
	@for FILE in `grep -l -e "---[[:space:]][[:space:]]*include" $^` ; do \
	    echo -n "$$FILE :" | sed s/\.vhd/\.ngc/ >>$@ ;\
	    grep -e "---[[:space:]][[:space:]]*include" $$FILE | while read i ; do set -- $$i ; echo -n " $$3" ; done >>$@ ;\
	    echo " " >>$@ ;\
	done
	@for FILE in `grep -l -e "---[[:space:]][[:space:]]*constraints" $^` ; do \
	    echo -n "$$FILE :" | sed s/\.vhd/\.ngd/ >>$@ ;\
	    set -- `grep -m 1 -e "---[[:space:]][[:space:]]*constraints" $$FILE` ; echo " $$3" >>$@ ;\
	done

ifeq ($(XILINX),)
    $(error "source ~/xilinx/settings.sh" to set env vars)
endif

include makefile.depends


