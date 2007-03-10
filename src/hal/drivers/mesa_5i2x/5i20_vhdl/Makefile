ifeq ($(XILINX),)
    $(error "source ~/xilinx/settings.sh" to set env vars)
endif

vpath %.vhd ../5i2x_vhdl

help :
	@echo "This Makefile is NOT part of the EMC2 build system."
	@echo "It uses the zero-cost (non-free) Xilinx Webpack tools"
	@echo "to convert VHDL source into bitfiles for the FPGA."
	@echo "The completed bitfiles are distributed with EMC2, and"
	@echo "do not need to be recreated except to fix bugs or to"
	@echo "make a custom FPGA configuration."

%.prj : %.vhd
	@echo "making project file $@ from top level file $<"
	@rm -f $@
	@for FILE in $^ ; do echo "vhdl work $$FILE" ; done >>$@

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


.PRECIOUS : %.prj %.scr %.ngc %.ngd %.ncd %-pl.ncd %-rt.ncd %.bit

makefile.depends : *.vhd
	@echo "updating dependency file $@"
	@rm -rf >$@
	@for FILE in `grep -l "use  *work\.[[:alnum:]_-]*_pkg" $^` ; do \
	    echo -n "$$FILE " | sed s/\.vhd/\.ngc/ >>$@ ;\
	    echo -n "$$FILE :" | sed s/\.vhd/\.prj/ >>$@ ;\
	    grep -o "use  *work\.[[:alnum:]_-]*_pkg" $$FILE | while read i ; do \
	        echo -n $$i | sed "s/use  *work\./ /" | sed "s/_pkg/.vhd/" ;\
	    done >>$@ ;\
	    echo " " >>$@ ;\
	done
	@for FILE in `grep -l -e "---[[:space:]][[:space:]]*constraints" $^` ; do \
	    echo -n "$$FILE :" | sed s/\.vhd/\.ngd/ >>$@ ;\
	    set -- `grep -m 1 -e "---[[:space:]][[:space:]]*constraints" $$FILE` ; echo " $$3" >>$@ ;\
	done


include makefile.depends


