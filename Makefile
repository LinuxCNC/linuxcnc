# top level Makefile

all headers depend indent install clean_install :
	(cd src/rtapi; make -k $@)

clean :
	(cd src/rtapi; make -k $@)
	- \find . -name "*~" -exec \rm -f {} \;
	- \find . -name "*.bak" -exec \rm -f {} \;
	- \find . -name core -exec \rm -f {} \;
	- \rm -f include/* lib/* rtlib/* bin/*

include Makefile.inc

.PHONY : all headers depend indent install clean clean_install
