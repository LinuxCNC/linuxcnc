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
# all - default target, makes the whole thing except examples
# examples - makes rtapi examples, run after "make all"
# headers - copies public header files to INC_DIR (emc2/include)
# depend - generate dependency file(s)
# indent - formats source code
# install - right now this does nothing
# clean - cleans up temp files, backups, object files, binaries, etc.

# this rule handles most targets, except clean and depend
# it simply changes to all the source sub-directories and calls make there
# note the order - low level code like rtapi is made first, before higher
# level code that might depend on it

all headers indent install :
	(cd src/rtapi; make $@)
	(cd src/hal; make $@)

# Include the Defines, Flags, etc..
include Makefile.inc

# this rule handles the depend target
# it first updates the headers target to ensure that the header
# files are installed, then runs the depend target in each directory

depend : headers
	(cd src/rtapi; make $@)
	(cd src/hal; make $@)

# this rule handles the examples target
# it only enters directories that have examples
examples :
	(cd src/rtapi; make $@)

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.
clean :
	@ (cd src/rtapi; make $@)
	@ (cd src/hal; make $@)
	@ find . -name "*~" -exec rm -f {} \;
	@ find . -name "*.bak" -exec rm -f {} \;
	@ find . -name core -exec rm -f {} \;
	-@ (rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	@ (if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	@ (if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	@ (if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)


.PHONY : all examples headers depend indent install clean
