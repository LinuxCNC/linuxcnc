# Makefile for RTAPI examples
#
# Note: this file makes nothing itself, it merely changes to the
# individual example directories and runs make there
#
all headers depend indent install clean:
	(cd extint; make $@)
	(cd shmem; make $@)
	(cd semaphore; make $@)


.PHONY : all headers depend indent install clean
