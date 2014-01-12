#!/bin/bash
#
# given a kernel source directory, extract the kernel version

# Unfortunately, many distributions (redhat, mandrake) have
# #defines inside version.h, so a simple cat|grep|cut test won't
# work... But then again, RH & Mandrake kernels are notorious for
# their use of patches that break the RT patching - Both RTAI and
# RTLinux strongly recommend using a virgin source from
# kernel.org.

#################################################
# Copyright (C) 2012, 2013  John Morris <john AT zultron DOT com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#################################################

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
