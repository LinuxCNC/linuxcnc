#!/bin/bash
#
# given a kernel source directory, extract the kernel version

# Unfortunately, many distributions (redhat, mandrake) have
# #defines inside version.h, so a simple cat|grep|cut test won't
# work... But then again, RH & Mandrake kernels are notorious for
# their use of patches that break the RT patching - Both RTAI and
# RTLinux strongly recommend using a virgin source from
# kernel.org.

# Get the kernel directory from cmdline
KERNELDIR=${1:?"Usage:  $0 <kernel-source-directory>"}

if test -e $KERNELDIR/include/linux/utsrelease.h; then
    VERSION_FILE=$KERNELDIR/include/linux/utsrelease.h
elif test -e $KERNELDIR/include/generated/utsrelease.h; then
    VERSION_FILE=$KERNELDIR/include/generated/utsrelease.h
else
    VERSION_FILE=$KERNELDIR/include/linux/version.h
fi
KERNEL_VERS=`gcc -E -dM ${VERSION_FILE} | grep UTS | cut -d '"' -f 2`

echo $KERNEL_VERS
