# top level Makefile


all headers depend indent install clean_install :
	(cd src/rtapi; make -k $@)

# Include the Defines, Flags, etc..
include Makefile.inc

clean :
	@ (cd src/rtapi; make -k $@)
	@ find . -name "*~" -exec rm -f {} \;
	@ find . -name "*.bak" -exec rm -f {} \;
	@ find . -name core -exec rm -f {} \;
	-@ (rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	@ (if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	@ (if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)


.PHONY : all headers depend indent install clean clean_install
