# top level Makefile
# -------------
#
# Notes on syntax:
# @ causes each command NOT to be echoed to the console.
# - allows the rule to continue after an error.
#
#
# Notes on targets:
#
# all - default target, makes the whole thing
# headers - copies public header files to INC_DIR (emc2/include)
# depend - generate dependency file(s)
# indent - formats source code
# install - right now this does nothing
# clean - cleans up temp files, backups, object files, binaries, etc.

# this rule handles most targets, except clean
# it simply changes to all the source sub-directories and calls make there
# note the order - low level code like rtapi is made first, before higher
# level code that might depend on it

all headers depend indent install :
	(cd src/rtapi; make -k $@)
	(cd src/hal; make -k $@)

# Include the Defines, Flags, etc..
include Makefile.inc

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.
clean :
	@ (cd src/rtapi; make -k $@)
	@ (cd src/hal; make -k $@)
	@ find . -name "*~" -exec rm -f {} \;
	@ find . -name "*.bak" -exec rm -f {} \;
	@ find . -name core -exec rm -f {} \;
	-@ (rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	@ (if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	@ (if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	@ (if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)


.PHONY : all headers depend indent install clean
