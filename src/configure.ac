#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Description: configure.ac
#   Autonconf produce a ./configure from this file
#   that's used to discover various programs/dependencies
#   usage: autoconf
#
# Authors: paul_c alex_joni jepler
# License: GPL Version 2
# System: Linux
#
# Copyright (c) 2004 All rights reserved.
#
# Last change:
# This file is released under GPL, refer to docs/README for details
#
# This file is adapted to replace the old ./configure script
# therefor it includes parts of that script

##############################################################################
# Section 1                                                                  #
# Autoconf initialisation                                                    #
##############################################################################

AC_PREREQ(2.69)
AC_INIT([LinuxCNC],[m4_normalize(esyscmd(cat ../VERSION))],[emc-developers@lists.sourceforge.net])
AC_CONFIG_SRCDIR(emc/motion/motion.c)
if test "$srcdir" != "."; then
    AC_MSG_ERROR([Building outside of srcdir is not supported])
fi

AC_PROG_CXX
AC_PROG_INSTALL

m4_include([m4/ax_cxx_compile_stdcxx_11.m4])
m4_include([m4/ax_python.m4])
m4_include([m4/ax_python_devel.m4])
m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_python.m4])
# make this optional - it will likely only result in warnings
AX_CXX_COMPILE_STDCXX_11(noext,optional)

AC_MSG_CHECKING(build toplevel)
BUILD_TOPLEVEL="$(cd ..; pwd -P)"
AC_MSG_RESULT($BUILD_TOPLEVEL)

AC_CONFIG_HEADERS([config.h])

# Support some of the standard configure options for directories
AC_PREFIX_DEFAULT(run-in-place)

AC_MSG_CHECKING(installation prefix)
if test "x$prefix" = "xNONE"; then
  RUN_IN_PLACE=yes
  AC_MSG_RESULT(run in place)
  prefix=${BUILD_TOPLEVEL}
else
  RUN_IN_PLACE=no
  AC_MSG_RESULT($prefix)
fi
AC_SUBST(AUTODIRS)

# internationalisation package name:
PACKAGE="linuxcnc"
AC_SUBST([PACKAGE])

if test -f ../VERSION; then
    EMC2VERSION="$(cat ../VERSION)"
else
    EMC2VERSION="Undetermined"
fi
AC_SUBST([EMC2VERSION])

AC_PATH_PROG(GREP, grep)
AC_PATH_PROG(EGREP, egrep)

#
# check for pkg-config
#
AC_PATH_PROG([PKG_CONFIG], [pkg-config])
if test -z "$PKG_CONFIG"; then
  AC_MSG_ERROR([pkg-config is required to build LinuxCNC])
fi

#
# check for rpc/rpc.h
#
AC_CHECK_HEADER([rpc/rpc.h],[], [
  if ! test -z "$PKG_CONFIG libtirpc"; then

    LIBTIRPC_CFLAGS=`pkg-config libtirpc --cflags`
    AC_SUBST([LIBTIRPC_CFLAGS])

    LIBTIRPC_LIBS=`pkg-config libtirpc --libs`
    AC_SUBST([LIBTIRPC_LIBS])

    CFLAGS+=" `pkg-config libtirpc --cflags` "
  fi

  # Update path
  AC_CHECK_LIB([tirpc], [get_myaddress], [], [
    AC_MSG_ERROR([Didn't find tirpc])
  ])

  # Clear the cached check
  $as_unset ac_cv_header_rpc_rpc_h
  AC_CHECK_HEADER([rpc/rpc.h],[],[
    AC_MSG_ERROR([glibc rpc.h missing, please install libtirpc])
  ])
])


##############################################################################
# Section 2                                                                  #
# RealTime checking (either RTAI, or RT-Linux)                               #
# Subsection 2.1 - arguments to ./configure (--with-realtime)                #
# Subsection 2.2 - if none specified above, RTDIR needs to be found by search#
# Subsection 2.3 - RTS(RT-script) is found, it gets queried for values       #
##############################################################################

# set up some default values
RTDIR=""
RTAI=""
RTPREFIX=""
KERNELDIR=""
RTFLAGS=""

NONDISTRIBUTABLE=false
AC_ARG_ENABLE(non-distributable,
    AS_HELP_STRING(
        [--enable-non-distributable],
        [To build LinuxCNC even if a license problem would render the resulting binaries non-distributable under the terms of the LinuxCNC license]),
    [
        case "$enableval" in
        (yes) NONDISTRIBUTABLE=true ;;
        esac
    ])

##############################################################################
# Subsection 2.1                                                             #
# Check for arguments to ./configure                                         #
# accept --with-realtime=<path> from commandline                             #
##############################################################################

RTDIR=""
AC_ARG_ENABLE(simulator,
    AS_HELP_STRING(
        [--enable-simulator],
        [Deprecated!  Use --with-realtime=uspace instead.]
    ),
    [
        case "$enableval" in
        Y*|y*)
            RTS=uspace
            AC_MSG_WARN([--enable-simulator is deprecated.  Use --with-realtime=uspace instead.])
        ;;
        esac
    ])

AC_ARG_WITH(realtime,
    AS_HELP_STRING(
        [--with-realtime=PATH],
        [Path where RTAI is installed, or "uspace" for POSIX userspace.]
    ),
    [   case "$withval" in
	"" | y | ye | yes | n | no)
	    AC_MSG_ERROR([You must supply a path or "uspace" for --with-realtime.]) ;;
	*)
            RTS="$withval" ;;
        esac
        ])
AC_SUBST([RTS])

TOOL_NML=no
AC_ARG_ENABLE(toolnml,
    AS_HELP_STRING(
        [--enable-toolnml],
        [Use nml for tooldata, default is mmap]
    ),
    [
        case "$enableval" in
        N*|n*)
            TOOL_NML=no ;;
        Y*|y*)
            TOOL_NML=yes ;;
        esac
    ])
##############################################################################
# Subsection 2.2                                                             #
# 1. If a RT has been specified by the user it needs to be checked for       #
# validity. That is done by checking default locations for a certain script  #
# (similar to 2)                                                             #
#                                                                            #
# 2. If no RT has been specified explicitly by the user, one has to be       #
# found by search. In order to do that the following code will search for a  #
# RT-specific script:                                                        #
# rtai-config for RTAI 3.x                                                   #
##############################################################################

#at this point if RTS is empty, we need to find RT ourselves
AC_MSG_CHECKING([for rtai-config])
if test -z "$RTDIR"; then
  DIRS="/usr/realtime-`uname -r` /usr/realtime /usr/realtime* /usr /usr/src/rtai*"
else
  DIRS="$RTS"
fi
  RTAI_CONFIG=none
  for location in $DIRS; do
    for subdir in . scripts bin; do
    for signature in rtai; do
      candidate=$location/$subdir/$signature-config
      if test "$RTAI_CONFIG" = "none" -a -x $candidate; then
        RTAI_CONFIG=$candidate
      fi
    done
  done
done

AC_MSG_RESULT([$RTAI_CONFIG])
AC_SUBST([RTAI_CONFIG])

AC_PATH_PROG([XENOMAI_CONFIG], [xeno-config], [none])
AC_SUBST([XENOMAI_CONFIG])

AC_MSG_CHECKING([for realtime API(s) to use])
if test "$RTS" = uspace || test -z "$RTS" -a "$RTAI_CONFIG" = "none" ; then
  RTS=uspace
  if test "$RTAI_CONFIG" = "none" ; then
    USPACE_RTAI=
    CONFIG_USPACE_RTAI=n
    RTAI_LXRT_CFLAGS=
    RTAI_LXRT_LDFLAGS=
  else
    USPACE_RTAI=+lxrt
    CONFIG_USPACE_RTAI=y
    RTAI_LXRT_CFLAGS=`$RTAI_CONFIG --lxrt-cflags`
    RTAI_LXRT_LDFLAGS=`$RTAI_CONFIG --lxrt-ldflags`
    AC_DEFINE([USPACE_RTAI], [], [Define if uspace realtime should optionally support RTAI LXRT])
  fi
  AC_SUBST([CONFIG_USPACE_RTAI])
  AC_SUBST([RTAI_LXRT_CFLAGS])
  AC_SUBST([RTAI_LXRT_LDFLAGS])

  if test "$XENOMAI_CONFIG" = "none" ; then
    USPACE_XENOMAI=
    CONFIG_USPACE_XENOMAI=n
    XENOMAI_CFLAGS=
    XENOMAI_LDFLAGS=
  else
    USPACE_XENOMAI=+xenomai
    CONFIG_USPACE_XENOMAI=y
    XENOMAI_CFLAGS=`$XENOMAI_CONFIG --skin posix --cflags`
    XENOMAI_LDFLAGS=`$XENOMAI_CONFIG --skin posix --ldflags`
    AC_DEFINE([USPACE_XENOMAI], [], [Define if uspace realtime should optionally support Xenomai])
  fi
  AC_SUBST([CONFIG_USPACE_XENOMAI])
  AC_SUBST([XENOMAI_CFLAGS])
  AC_SUBST([XENOMAI_LDFLAGS])

  AC_MSG_RESULT([$RTS$USPACE_RTAI$USPACE_XENOMAI])

  AC_MSG_CHECKING([whether to enable userspace PCI access])
  AC_ARG_ENABLE(userspace-pci,
      AS_HELP_STRING(
        [--disable-userspace-pci],
        [Disable userspace PCI access (use this if libudev is unavailable).],
      ),
      [
	case "$enableval" in
	(Y*|y*)
	    BUILD_UDEV=yes ;;
	(*)
	    BUILD_UDEV=no ;;
	esac
      ],
      [
        case "`uname -s`" in
        (*BSD*) BUILD_UDEV=no ;;
        (*) BUILD_UDEV=yes ;;
        esac
      ])
  AC_MSG_RESULT([$BUILD_UDEV])
  AC_SUBST([BUILD_UDEV])

  if test "$BUILD_UDEV" = "yes"; then
    AC_MSG_CHECKING([for libudev])
    if pkg-config libudev >/dev/null 2>&1; then
        LIBUDEV_VER=`pkg-config udev --modversion`
        AC_MSG_RESULT(yes - version [$LIBUDEV_VER])
        LIBUDEV_CFLAGS=`pkg-config libudev --cflags`
        LIBUDEV_LIBS=`pkg-config libudev --libs`
    else
      AC_MSG_ERROR([libudev-dev not found])
    fi
  else
    LIBUDEV_CFLAGS=-DNO_LIBUDEV
  fi
  AC_SUBST([LIBUDEV_CFLAGS])
  AC_SUBST([LIBUDEV_LIBS])
else
  RTS="$RTAI_CONFIG"
  AC_MSG_RESULT([$RTS])
fi


##############################################################################
# Subsection 2.3                                                             #
# we have the RT-script now, it's time to query it for some values           #
# it can provide us with default values such as CC, CFLAGS, etc              #
# see each case for more details                                             #
##############################################################################

RTARCH=`uname -m | sed s/i.86/i386/`
case $RTS in
*/rtai-config)
    RTPREFIX=rtai
    RTAI_VERSION=$($RTS --version)
    RTAI=$(echo $RTAI_VERSION | cut -f 1 -d .)
    RTDIR=`$RTS --prefix`
    RTDIR=$(cd $RTDIR ; pwd -P )
    RTFLAGS=`$RTS --module-cflags`
    RTFLAGS="$RTFLAGS -DRTAI=$RTAI"
    KERNELDIR=`$RTS --linux-dir`
    RTARCH=`$RTS --arch`
    ULAPI_CFLAGS=`$RTS --lxrt-cflags`
    ULAPI_LDFLAGS=`$RTS --lxrt-ldflags`
;;
uspace|uspace+*)
    RTPREFIX=uspace
    RTDIR=$(pwd -P)
    ULAPI_CFLAGS=""
    RTFLAGS="-DUSPACE"
    BUILD_SYS=uspace
    MODEXT=.so
    MODULE_DIR=${prefix}/lib/linuxcnc/modules
    ULAPI_LDFLAGS=""
esac

if test "$RTS" != uspace; then
    AC_MSG_CHECKING([for location of kernel headers])
    AC_ARG_WITH(kernel-headers,
        AS_HELP_STRING(
            [--with-kernel-headers=PATH],
            [Location for kernel headers.]
        ),
	[
	    KERNELDIR="$withval"
	    AC_MSG_RESULT(using specified value: $KERNELDIR)
	],[
	    AC_MSG_RESULT(using value from RTS: $KERNELDIR)
	])
fi

AC_SUBST([RTAI])
AC_SUBST([RTPREFIX])
AC_SUBST([KERNELDIR])
AC_SUBST([RTFLAGS])
AC_SUBST([RTDIR])
AC_SUBST([RTARCH])
AC_SUBST([ULAPI_CFLAGS])
AC_SUBST([ULAPI_LDFLAGS])

case "$RTPREFIX" in
uspace)
    AC_DEFINE(RTAPI_USPACE, [], [Realtime system is userspace]) ;;
rtai)
    AC_DEFINE(RTAPI_RTAI, [], [Realtime system is RTAI]) ;;
esac


EMC2_HOME=`(cd .. ; pwd)`
AC_SUBST([EMC2_HOME])

AC_DEFINE_UNQUOTED([RTDIR], "$RTDIR", [Directory of the installed realtime system])

##############################################################################
# Section 3                                                                  #
# additional checking                                                        #
# Subsection 3.1 - checking for a CC (we must use the same one used for RT)  #
# Subsection 3.2 - check for the kernel version, with RT patches             #
# Subsection 3.3 - check the moduledir where modules can get installed       #
# Subsection 3.4 - check for GTK                                             #
# Subsection 3.5 - check for utility programs needed to build and run EMC    #
##############################################################################


##############################################################################
# Subsection 3.1 - checking for a CC (we must use the same one used for RT)  #
##############################################################################
AC_MSG_CHECKING([for cc version])

if (test ${RTS##*/} = rtai-config ) ; then
  RTSCC=`$RTS --cc`
  AC_MSG_RESULT(found $RTSCC in rtai-config)
else
  AC_MSG_RESULT(not specified)
fi

AC_PROG_CC([$RTSCC gcc clang])
AC_PROG_CPP()

if test -z "$CC"; then
  AC_MSG_ERROR([no acceptable cc found])
elif ! test `$CC -dumpversion | cut -d '.' -f 1` -gt 2 ; then
  AC_MSG_ERROR([Compilers older than gcc 3.x are no longer supported])
fi

AC_MSG_CHECKING([for usability of linux/hidraw.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/ioctl.h>
#include <linux/hidraw.h>
long hidiocgrawinfo = HIDIOCGRAWINFO;
    ]], [[]])],[
	HIDRAW_H_USABLE=yes
        AC_DEFINE([HIDRAW_H_USABLE], [], [Define to 1 if linux/hidraw.h is usable and defines HIDIOCGRAWINFO])
],[HIDRAW_H_USABLE=no])
AC_MSG_RESULT($HIDRAW_H_USABLE)
AC_SUBST([HIDRAW_H_USABLE])

#
# Check for rpc/rpc.h
#

AC_MSG_CHECKING([for usability of rpc/rpc.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <rpc/rpc.h>
    ]], [[
struct sockaddr_in *foo;
get_myaddress(foo);
    ]])],[
        AC_MSG_RESULT(yes)
    ],[
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Unable to use rpc.h])
    
])

#
# check for libmodbus3
#

AC_ARG_WITH(
    [libmodbus],
    AS_HELP_STRING(
        [--with-libmodbus],
        [Specify whether or not to build the drivers that use libmodbus
        (defaults to "yes").]
    ),
    [WITH_LIBMODBUS=$withval],
    [WITH_LIBMODBUS=yes]
)

AS_IF(
    [test "x$WITH_LIBMODBUS" = "xyes"],
    [
        AC_MSG_CHECKING([for libmodbus3])
        if pkg-config libmodbus --atleast-version 3 >/dev/null 2>&1; then
            LIBMODBUS_VER=`pkg-config libmodbus --modversion`
            AC_MSG_RESULT(yes - version [$LIBMODBUS_VER])
            LIBMODBUS_CFLAGS=`pkg-config libmodbus --cflags`
            AC_SUBST([LIBMODBUS_CFLAGS])
            LIBMODBUS_LIBS=`pkg-config libmodbus --libs`
            AC_SUBST([LIBMODBUS_LIBS])
            AC_DEFINE(
                [HAVE_LIBMODBUS3],
                [yes],
                [define if the libmodbus3 headers and library are available]
            )
            AC_SUBST(HAVE_LIBMODBUS3, yes)
        else
          AC_MSG_RESULT(no)
          AC_MSG_ERROR([libmodbus3 not found!
install with "sudo apt-get install libmodbus-dev" or disable with
"configure --without-libmodbus"])
        fi
    ]
)


#
# check for libusb-1.0
#

AC_ARG_WITH(
    [libusb-1.0],
    AS_HELP_STRING(
        [--with-libusb-1.0],
        [Specify whether or not to build the drivers that use libusb-1.0
        (defaults to "yes").]
    ),
    [WITH_LIBUSB10=$withval],
    [WITH_LIBUSB10=yes]
)

AS_IF(
    [test "x$WITH_LIBUSB10" = "xyes"],
    [
        AC_MSG_CHECKING([for libusb-1.0])
        if pkg-config libusb-1.0 >/dev/null 2>&1; then
            AC_MSG_RESULT(yes)
            LIBUSB10_CFLAGS=`pkg-config libusb-1.0 --cflags`
            AC_SUBST([LIBUSB10_CFLAGS])
            LIBUSB10_LIBS=`pkg-config libusb-1.0 --libs`
            AC_SUBST([LIBUSB10_LIBS])
            AC_DEFINE(
                [HAVE_LIBUSB10],
                [yes],
                [define if the libusb-1.0 headers and library are available]
            )
            AC_SUBST(HAVE_LIBUSB10, yes)
        else
          AC_MSG_RESULT(no)
          AC_MSG_ERROR([libusb-1.0 not found!
install with "sudo apt-get install libusb-1.0-0-dev" or disable with
"configure --without-libusb-1.0"])
        fi
    ]
)


##############################################################################
# Subsection 3.2 - check for the kernel version, with RT patches             #
#----------------------------------------------------------------------------#
# Check the kernel headers to find the uts-kernel version.                   #
# This should be a more reliable way of determining the kernel that          #
# is being used to compile against - `uname -r` only returns the             #
# running kernel version which is may not be the one the code is             #
# compiled against.                                                          #
##############################################################################

if test $RTS '!=' uspace; then
AC_MSG_CHECKING([for kernel version string])

if test -e $KERNELDIR/include/linux/utsrelease.h; then
    VERSION_FILE=$KERNELDIR/include/linux/utsrelease.h
elif test -e $KERNELDIR/include/generated/utsrelease.h; then
    VERSION_FILE=$KERNELDIR/include/generated/utsrelease.h
else
    VERSION_FILE=$KERNELDIR/include/linux/version.h
fi
KERNEL_VERS=`$CC -E -dM ${VERSION_FILE} | grep UTS | cut -s -d '"' -f 2`

if test -z "$KERNEL_VERS"; then
    AC_MSG_ERROR(Kernel version string not found)
fi

AC_MSG_RESULT([$KERNEL_VERS])
AC_SUBST([KERNEL_VERS])
AC_DEFINE_UNQUOTED([RTAPI_KERNEL_VERSION], "$KERNEL_VERS", [Required kernel version])

# Do a quick test for module extension..
AC_MSG_CHECKING([for kernel module extension])
MODEXT=`$RTS --module-ext 2>/dev/null` || MODEXT=".o"
if test "x" = "x$MODEXT"; then
    MODEXT=".o"
fi
AC_MSG_RESULT([$MODEXT])

# This check should see if kbuild is or isn't used
AC_MSG_CHECKING([kernel build system])
# FIXME FIXME FIXME
# right now it will assume based on the kernel module extension
EXT_RTFLAGS=""
BUILD_SYS="normal"

if test "x$MODEXT" = "x.ko" ; then
    BUILD_SYS="kbuild"
else
    EXT_RTFLAGS="-DEXPORT_SYMTAB"
fi

AC_ARG_WITH(kbuild,
    AS_HELP_STRING(
        [--with-kbuild],
        [Specify if your kernel uses kbuild or not.]
    ),
    [
        case "$withval" in
        "" | y | ye | yes )
	    BUILD_SYS="kbuild"
          ;;
        n | no )
	    BUILD_SYS="normal"
          ;;
        esac
    ])
AC_MSG_RESULT([$BUILD_SYS])
fi
AC_SUBST([MODEXT])
AC_DEFINE_UNQUOTED([MODULE_EXT], "$MODEXT", [Extension for realtime modules])
AC_SUBST([BUILD_SYS])
AC_SUBST([EXT_RTFLAGS])

##############################################################################
# Subsection 3.3 - check the moduledir where modules can get installed       #
#                                                                            #
# Provides the user the possibility to specify (--with-module-dir=<path>)    #
# If not specified default inside /lib/modules/${KERNEL_VERS}/               #
# to the location where a RT-module gets found (mbuff.o or rtai_shed.o)     #
# (maybe the default should be /lib/modules/$KERNEL_VERS/extra if it exists? #
##############################################################################

# Create a module-dir option, similar to rtai's...
AC_MSG_CHECKING([for module installation directory])

AC_ARG_WITH(module-dir,
    AS_HELP_STRING(
        [--with-module-dir=PATH],
        [Installation directory for kernel modules.]
    ),
    [
        case "$withval" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply a path for --with-module-dir.])
          ;;
        esac
        MODULE_DIR="$withval"
    ])


# if not specified, ask the realtime config script where the directory is
# if the config script returns an error, set MODULE_DIR to nothing
if test "x" = "x$MODULE_DIR" ; then
  MODULE_DIR=`$RTS --module-dir 2>/dev/null`
  if ! test -d $MODULE_DIR ; then
    AC_MSG_ERROR([$RTS --module-dir did not specify an existing directory.  You must use specify it using --with-module-dir=...])
  fi
fi

if test "$RUN_IN_PLACE" = "yes"; then
    RIP_MODULE_DIR="${BUILD_TOPLEVEL}/rtlib"
    AC_MSG_RESULT([configuring for run-in-place])
    prefix=${BUILD_TOPLEVEL}
    if test "$TOOL_NML" = "yes"; then
       DEFAULT_NMLFILE=${prefix}/configs/common/linuxcnc_big.nml
    else
       DEFAULT_NMLFILE=${prefix}/configs/common/linuxcnc.nml
    fi
    DEFAULT_TOOLTABLE=${prefix}/configs/sim/sim.tbl
    AC_MSG_RESULT([$RIP_MODULE_DIR])
else
    RUN_IN_PLACE=no
    EMC2_HOME=$prefix
    if test "$TOOL_NML" = "yes"; then
       DEFAULT_NMLFILE=${prefix}/share/linuxcnc/linuxcnc_big.nml
    else
       DEFAULT_NMLFILE=${prefix}/share/linuxcnc/linuxcnc.nml
    fi
    DEFAULT_TOOLTABLE=${prefix}/share/doc/linuxcnc/examples/sample-configs/common/tool.tbl
    AC_MSG_RESULT([$MODULE_DIR])
fi

AC_SUBST([MODULE_DIR])
AC_SUBST([RIP_MODULE_DIR])
AC_SUBST([RUN_IN_PLACE])
AC_SUBST([DEFAULT_NMLFILE])
AC_SUBST([TOOL_NML])
AC_DEFINE_UNQUOTED([EMC2_DEFAULT_NMLFILE], "$DEFAULT_NMLFILE", [Default nml file])
AC_DEFINE_UNQUOTED([EMC2_DEFAULT_TOOLTABLE], "$DEFAULT_TOOLTABLE", [Default nml file])

##############################################################################
# Subsection 3.4 - define default dirs, these get used in a lot of places    #
# we decide based on RIP or install where stuff goes                         #
##############################################################################

fe () {
    # Fully expand argument.  Example:
    # ${datadir} -> ${datarootdir} -> ${prefix}/share -> /usr/local
    case "$1" in
    *\$*) eval fe \""$1"\" ;;
    *) echo "$1" ;;
    esac
}

if test "xyes" = "x$RUN_IN_PLACE"; then
    EMC2_LATENCY_SCRIPT=$EMC2_HOME/scripts/latency-test
    EMC2_SCRIPT=$EMC2_HOME/scripts/linuxcnc
    EMC2_SUFFIX=" (run-in-place)"
    EMC2_ICON=$EMC2_HOME/linuxcncicon.png
    EMC2_BIN_DIR=$EMC2_HOME/bin
    EMC2_TCL_DIR=$EMC2_HOME/tcl
    EMC2_TCL_LIB_DIR=$EMC2_HOME/tcl
    EMC2_LANG_DIR=$EMC2_HOME/src/objects
    EMC2_PO_DIR=$EMC2_HOME/share/locale
    EMC2_HELP_DIR=$EMC2_HOME/docs/help
    EMC2_RTLIB_DIR=$EMC2_HOME/rtlib
    LINUXCNC_CONFIG_PATH="~/linuxcnc/configs:$EMC2_HOME/configs"
    EMC2_NCFILES_DIR=$EMC2_HOME/nc_files
    REALTIME=$EMC2_HOME/scripts/realtime
    EMC2_IMAGE_DIR=$EMC2_HOME
    HALLIB_DIR=$EMC2_HOME/lib/hallib
else
    EMC2_HOME=${prefix}
    EMC2_BIN_DIR=${prefix}/bin
    EMC2_LATENCY_SCRIPT=$EMC2_BIN_DIR/latency-test
    EMC2_SCRIPT=$EMC2_BIN_DIR/linuxcnc
    EMC2_SUFFIX=""
    EMC2_ICON=linuxcncicon
    EMC2_TCL_DIR=${prefix}/lib/tcltk/linuxcnc
    EMC2_TCL_LIB_DIR=${prefix}/lib/tcltk/linuxcnc
    EMC2_LANG_DIR=${prefix}/lib/tcltk/linuxcnc/msgs
    EMC2_PO_DIR=${prefix}/share/locale
    EMC2_HELP_DIR=${prefix}/share/doc/linuxcnc
    case $MODULE_DIR in
        */linuxcnc*) EMC2_RTLIB_DIR=$MODULE_DIR ;;
	*) EMC2_RTLIB_DIR=$MODULE_DIR/linuxcnc
    esac
    LINUXCNC_CONFIG_PATH="~/linuxcnc/configs:/usr/local/etc/linuxcnc/configs:"$(eval echo $EMC2_HELP_DIR)"/examples/sample-configs"
    EMC2_NCFILES_DIR=${prefix}/share/linuxcnc/ncfiles
    REALTIME=${prefix}/lib/linuxcnc/realtime
    EMC2_IMAGE_DIR=$(fe "$datadir")/linuxcnc
    HALLIB_DIR=${prefix}/share/linuxcnc/hallib
fi

AC_DEFINE_UNQUOTED([EMC2_HOME], "$EMC2_HOME", [Prefix or RIP directory])
AC_DEFINE_UNQUOTED([EMC2_BIN_DIR], "$EMC2_BIN_DIR", [Directory for binaries])
AC_DEFINE_UNQUOTED([EMC2_TCL_DIR], "$EMC2_TCL_DIR", [Directory for tcl scripts])
AC_DEFINE_UNQUOTED([EMC2_HELP_DIR], "$EMC2_HELP_DIR", [Directory for help files])
AC_DEFINE_UNQUOTED([EMC2_RTLIB_DIR], "$EMC2_RTLIB_DIR", [Directory of realtime system])
AC_DEFINE_UNQUOTED([EMC2_LANG_DIR], "$EMC2_LANG_DIR", [Directory for tcl translation files])
AC_DEFINE_UNQUOTED([EMC2_PO_DIR], "$EMC2_PO_DIR", [Directory for po/mo translation files])
AC_DEFINE_UNQUOTED([EMC2_NCFILES_DIR], "$EMC2_NCFILES_DIR", [Directory for nc files])
AC_DEFINE_UNQUOTED([EMC2_IMAGE_DIR], "$EMC2_IMAGE_DIR", [Directory for images])

AC_SUBST([EMC2_BIN_DIR])
AC_SUBST([EMC2_TCL_DIR])
AC_SUBST([EMC2_TCL_LIB_DIR])
AC_SUBST([EMC2_HELP_DIR])
AC_SUBST([EMC2_RTLIB_DIR])
AC_SUBST([EMC2_LANG_DIR])
AC_SUBST([LINUXCNC_CONFIG_PATH])
AC_SUBST([EMC2_NCFILES_DIR])
AC_SUBST([REALTIME])
AC_SUBST([EMC2_IMAGE_DIR])
AC_SUBST([EMC2_SCRIPT])
AC_SUBST([EMC2_LATENCY_SCRIPT])
AC_SUBST([EMC2_SUFFIX])
AC_SUBST([EMC2_ICON])
AC_SUBST([HALLIB_DIR])

# coordinate installed auxiliary applications (not RIP or deb dependent):
LINUXCNC_AUX_GLADEVCP=/usr/share/linuxcnc/aux_gladevcp
LINUXCNC_AUX_EXAMPLES=/usr/share/linuxcnc/aux_examples
AC_SUBST([LINUXCNC_AUX_GLADEVCP])
AC_SUBST([LINUXCNC_AUX_EXAMPLES])

##############################################################################
# Subsection 3.5 - check for GTK                                             #
# FIXME: allow it to be enabled or disabled command line                     #
##############################################################################

AC_MSG_CHECKING([for glib])
if pkg-config glib-2.0 >/dev/null 2>&1; then
  GLIB_VER=`pkg-config glib-2.0 --modversion`
  AC_MSG_RESULT(yes - $GLIB_VER)
  GLIB_CFLAGS=`pkg-config glib-2.0 --cflags`
  GLIB_LIBS=`pkg-config glib-2.0 --libs`
else
  AC_MSG_ERROR(no -- required until somebody makes glib optional)
fi


AC_ARG_ENABLE(gtk,
    AS_HELP_STRING(
        [--disable-gtk],
        [Disable the parts of LinuxCNC that depend on GTK.]
    ),
    [
	case "$enableval" in
	Y*|y*)
	    BUILD_GTK=yes ;;
	*)
	    BUILD_GTK=no ;;
	esac
    ],
    [BUILD_GTK=yes])

if test "$BUILD_GTK" = "yes"; then
  AC_MSG_CHECKING([for GTK 3.22.4 or above])
  if pkg-config gtk+-3.0 --atleast-version 3.22.4 >/dev/null 2>&1; then
    GTK_VER=`pkg-config gtk+-3.0 --modversion`
    AC_MSG_RESULT(yes - $GTK_VER)
    GTK_CFLAGS=`pkg-config gtk+-3.0 --cflags`
    GTK_LIBS=`pkg-config gtk+-3.0 --libs`
  else
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([GTK3 missing.  Install with "sudo apt install libgtk-3-dev" or specify --disable-gtk to skip the parts of LinuxCNC that depend on GTK])
  fi
fi

AC_SUBST(HAVE_GNOMEPRINT)
AC_SUBST(GTK_VER)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

AC_ARG_ENABLE(gtk2,
    AS_HELP_STRING(
        [--disable-gtk2],
        [Disable the parts of LinuxCNC that depend on GTK2. Ónly needed for classicladder)]
    ),
    [
	case "$enableval" in
	Y*|y*)
	    BUILD_GTK2=yes ;;
	*)
	    BUILD_GTK2=no ;;
	esac
    ],
    [BUILD_GTK2=yes])

if test "$BUILD_GTK2" = "yes"; then
  AC_MSG_CHECKING([for GTK 2.4.0 or above])
  if pkg-config gtk+-2.0 --atleast-version 2.4.0 >/dev/null 2>&1; then
    GTK2_VER=`pkg-config gtk+-2.0 --modversion`
    AC_MSG_RESULT(yes - $GTK2_VER)
    GTK2_CFLAGS=`pkg-config gtk+-2.0 --cflags`
    GTK2_LIBS=`pkg-config gtk+-2.0 --libs`
    AC_MSG_CHECKING(for libgnomeprintui-2.2)
    if pkg-config --exists libgnomeprintui-2.2; then
      AC_MSG_RESULT(yes)
      GTK2_CFLAGS="$GTK2_CFLAGS `pkg-config --cflags libgnomeprintui-2.2`"
      GTK2_LIBS="$GTK2_LIBS `pkg-config --libs libgnomeprintui-2.2`"
      HAVE_GNOMEPRINT=yes
    else
      AC_MSG_RESULT(no -- printing from classicladder will not be possible)
      HAVE_GNOMEPRINT=
    fi
  else
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([GTK2 missing.  Install with "sudo apt install libgtk2.0-dev" or specify --disable-gtk2 to skip building classicladder])
  fi
fi

AC_SUBST(GTK2_VER)
AC_SUBST(GTK2_LIBS)
AC_SUBST(GTK2_CFLAGS)

##############################################################################
# Subsection 3.6 - check for utility programs needed to build and run EMC    #
#                                                                            #
# Check for all the utility programs we need to build and install emc        #
# as well as those we use in the emc scripts...                              #
##############################################################################

SPATH=$PATH:/usr/local/sbin:/usr/sbin:/sbin
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_PATH_PROG(INSTALL, install)

AC_PATH_PROG(SED, sed, "none")
if test $SED = "none"
then
    AC_MSG_ERROR([sed not found])
fi

AC_PATH_PROG(PS, ps, "none")
if test $PS = "none"
then
    AC_MSG_ERROR([ps not found])
fi

AC_PATH_PROG(KILL, kill, "none")
if test $KILL = "none"
then
    AC_MSG_ERROR([kill not found])
fi

AC_PATH_PROG(WHOAMI, whoami, "none")
if test $WHOAMI = "none"
then
    AC_MSG_ERROR([whoami not found])
fi

AC_PATH_PROG(AWK, awk, "none")
if test $AWK = "none"
then
    AC_MSG_ERROR([awk not found])
fi

if test "$RTS" != uspace; then
    # Module utilities not needed for uspace
    AC_PATH_PROG(INSMOD, insmod, "none", $SPATH)
    if test $INSMOD = "none"
    then
        AC_MSG_ERROR([insmod not found])
    fi

    AC_PATH_PROG(RMMOD, rmmod, "none", $SPATH)
    if test $RMMOD = "none"
    then
        AC_MSG_ERROR([rmmod not found])
    fi

    AC_PATH_PROG(LSMOD, lsmod, "none", $SPATH)
    if test $LSMOD = "none"
    then
        AC_MSG_ERROR([lsmod not found])
    fi
fi

AC_PATH_PROG(PIDOF, pidof, "none", $SPATH)
if test $PIDOF = "none"
then
    AC_MSG_ERROR([pidof not found])
fi

AC_PATH_PROG(IPCS, ipcs, "none")
if test $IPCS = "none"
then
    AC_MSG_ERROR([ipcs not found])
fi

AC_PATH_PROG(FUSER, fuser, "none", $SPATH)
if test $FUSER = "none"
then
    AC_MSG_ERROR([fuser not found])
fi

AC_PATH_PROGS(YAPPS, yapps yapps2, "none", $SPATH)
if test $YAPPS = "none"
then
    AC_MSG_ERROR([yapps/yapps2 and not found])
fi
AC_SUBST(YAPPS)

# MANDB empty is handled in doc/Submakefile
AC_PATH_PROG(MANDB, mandb, "")

AC_PATH_PROG(INTLTOOL_EXTRACT, intltool-extract, "none", $SPATH)
if test $INTLTOOL_EXTRACT = "none"
then
    AC_MSG_ERROR([intltool-extract not found])
fi

AC_ARG_WITH(rmmod,
    AS_HELP_STRING(
        [--with-rmmod=PATH],
        [Location of the rmmod program to use.]
    ),
    [
	case "$withval" in
	"" | y | ye | yes)
	    AC_MSG_ERROR([You must supply an argument to --with-rmmod.])
	  ;;
	n | no | none)
	    RMMOD=""
	  ;;
	*)
	    RMMOD="$withval"
	esac
    ],
    [
            RMMOD="$EMC2_BIN_DIR/linuxcnc_module_helper remove"
    ])

AC_ARG_WITH(insmod,
    AS_HELP_STRING(
        [--with-insmod=PATH],
        [Location of the insmod program to use.]
    ),
    [
	case "$withval" in
	"" | y | ye | yes)
	    AC_MSG_ERROR([You must supply an argument to --with-insmod.])
	  ;;
	n | no | none)
	    INSMOD=""
	  ;;
	*)
	    INSMOD="$withval"
	esac
    ],
    [
            INSMOD="$EMC2_BIN_DIR/linuxcnc_module_helper insert"
    ])

AC_PATH_PROGS(YAPPS, [yapps yapps2])
if test -z "$YAPPS"; then
  AC_MSG_ERROR([yapps is required to build LinuxCNC
install with "sudo apt-get install yapps2"])
fi
AC_SUBST(YAPPS)

##############################################################################
# Subsection 3.7 - check for programs needed to build documentation          #
#                                                                            #
# Check for LyX and other programs we need to build and install docs.        #
# (Optional, if not present, just don't build the docs.)                     #
##############################################################################

AX_BOOST_BASE(,,AC_MSG_ERROR([libboost not found]))
AX_PYTHON
AC_PATH_PROG(PYTHON,[$PYTHON_BIN])
AX_PYTHON_DEVEL(,,AC_MSG_ERROR([python-dev not found]))
AX_BOOST_PYTHON(,,AC_MSG_ERROR([libboost-python not found]))

AC_MSG_CHECKING([whether to build documentation])
AC_ARG_ENABLE(build-documentation,
    AS_HELP_STRING(
        [--enable-build-documentation[=FORMAT]],
        [Build documentation.  FORMAT may be "Y" for both or "pdf" or "html" if only one format is desired.]
    ),
    [
        case "$enableval" in
        Y*|y*|html,pdf|pdf,html)
            BUILD_DOCS=yes
            BUILD_DOCS_PDF=yes
            BUILD_DOCS_HTML=yes
            AC_MSG_RESULT([PDF and HTML requested])
        ;;
        html)
            BUILD_DOCS=yes
            BUILD_DOCS_PDF=no
            BUILD_DOCS_HTML=yes
            AC_MSG_RESULT([HTML requested])
	;;
        pdf)
            BUILD_DOCS=yes
            BUILD_DOCS_PDF=yes
            BUILD_DOCS_HTML=no
            AC_MSG_RESULT([PDF requested])
	;;
        *)
            BUILD_DOCS=no
            AC_MSG_RESULT([no])
        ;;
    esac
    ],
    [
        BUILD_DOCS=no
        AC_MSG_RESULT([no])
    ])

# Programs required for building all documentation
if ( test "$BUILD_DOCS" = "yes" ) ; then
    AC_PATH_PROG(ASCIIDOC,asciidoc,"none")
    if ( test "none" = "$ASCIIDOC" ) ; then
	AC_MSG_ERROR([no AsciiDoc, documentation cannot be built])
    fi

    AC_PATH_PROG(A2X,a2x,"none")
    if ( test "none" = "$A2X" ) ; then
	AC_MSG_ERROR([no a2x, documentation cannot be built])
    fi

    AC_MSG_CHECKING([whether to specify latex.encoding])
    temp_asciidoc=`mktemp --suffix=.txt`
    cat > $temp_asciidoc <<EOF
:lang: fr
:ascii-ids:

....
umläut nuß
....
EOF
    if $A2X --dblatex-opts "-P latex.encoding=utf8" $temp_asciidoc > /dev/null 2>&1;
    then
        A2X_LATEX_ENCODING="-P latex.encoding=utf8"
        AC_MSG_RESULT(yes)
    else
        A2X_LATEX_ENCODING=""
        AC_MSG_RESULT(no)
    fi
    AC_SUBST(A2X_LATEX_ENCODING)
    rm -f $temp_asciidoc ${temp_asciidoc%.txt}.pdf

    AC_PATH_PROG(DBLATEX,dblatex,"none")
    if ( test "none" = "$DBLATEX" ) ; then
	AC_MSG_ERROR([no dblatex, documentation cannot be built])
    fi

    AC_MSG_CHECKING([dblatex version])
    set -- `dblatex --version`; DBLATEX_VER=$3
    set -- `echo $DBLATEX_VER | sed 's/[[.-]]/ /g'`
    micro=`echo $3 | sed 's/\([[0-9]]*\).*/\1/g'`

    if test $1 -eq 0 -a \( $2 -lt 2 -o \( $2 -eq 2 -a ${micro:-0} -lt 12 \) \); then
        AC_MSG_ERROR([dblatex version $DBLATEX_VER less than 0.2.12.
Documentation cannot be built.])
    fi
    AC_MSG_RESULT([$DBLATEX_VER])


    AC_PATH_PROG(SOURCE_HIGHLIGHT,source-highlight,"none")
    if ( test "none" = "$SOURCE_HIGHLIGHT" ) ; then
	AC_MSG_ERROR([no source-highlight, documentation cannot be built])
    fi


    AC_PATH_PROG(CONVERT,convert,"none")
    if ( test "none" = "$CONVERT" ) ; then
	AC_MSG_ERROR([no convert, documentation cannot be built])
    fi

    AC_PATH_PROG(GS,gs,"none")
    if ( test "none" = "$GS" ) ; then
	AC_MSG_ERROR([no gs, documentation cannot be built])
    fi
fi

# Programs required only for building the PDF documentation
if ( test "$BUILD_DOCS_PDF" = "yes" ) ; then
    AC_PATH_PROG(PDFLATEX,pdflatex,"none")
    if ( test "none" = "$PDFLATEX" ) ; then
	AC_MSG_ERROR([no pdflatex, PDF documentation cannot be built])
    fi
fi

# Programs required only for building the HTML documentation
if ( test "$BUILD_DOCS_HTML" = "yes" ) ; then
    AC_PATH_PROG(XSLTPROC,xsltproc,"none")
    if ( test "none" = "$XSLTPROC" ) ; then
	AC_MSG_ERROR([no xsltproc, HTML documentation cannot be built])
    fi

    AC_PATH_PROG(DVIPNG,dvipng,"none")
    if ( test "none" = "$DVIPNG" ) ; then
	AC_MSG_ERROR([no dvipng, HTML documentation cannot be built])
    fi

    AC_MSG_CHECKING([for HTML support in groff])
    if ! groff -Thtml < /dev/null > /dev/null 2>&1 ; then
        AC_MSG_ERROR([no groff -Thtml, HTML documentation cannot be built])
    else
        AC_MSG_RESULT(yes)
    fi

    AC_PATH_PROG(CHECKLINK,checklink,"none")
    if ( test "none" = "$CHECKLINK" ) ; then
	AC_MSG_ERROR([no checklink, HTML documentation cannot be built
install with "sudo apt-get install w3c-linkchecker"])
    fi
fi
AC_SUBST(BUILD_DOCS)
AC_SUBST(BUILD_DOCS_PDF)
AC_SUBST(BUILD_DOCS_HTML)

if test "$RTS" '!=' uspace
then
for m in adeos rtai_hal rtai_ksched rtai_sched rtai_fifos rtai_sem rtai_math \
; do
    AC_MSG_CHECKING([for $m])
    TMP=$(find $MODULE_DIR -name $m$MODEXT 2>/dev/null)
    if test -z "$TMP"; then
        TMP=$(find /lib/modules/$KERNEL_VERS -name $m$MODEXT)
    fi
    eval MODPATH_$m=${TMP/$KERNEL_VERS/\\\$\\\(uname\\\ -r\\\)}
    AC_MSG_RESULT([${TMP:-not found}])
done

AC_SUBST(MODPATH_adeos)
AC_SUBST(MODPATH_rtai_hal)
AC_SUBST(MODPATH_rtai_ksched)
AC_SUBST(MODPATH_rtai_sched)
AC_SUBST(MODPATH_rtai_fifos)
AC_SUBST(MODPATH_rtai_sem)
AC_SUBST(MODPATH_rtai_math)
fi

# Checks for header files.

##############################################################################
# Section 4.1 - Important headers, functions and global defines.             #
#                                                                            #
##############################################################################

# This section gets inserted in to config.h.in when autoheader is run.
# Global and common defines should be here rather than duplicated in
# multiple places. Keep screwball constructs out of this, and it can
# be used in both kernel space and user space.
AH_TOP([/********************************************************************
* Description: config.h
*
*	Common defines used in many emc2 source files.
*       To add to this file, edit Section 4.1 in configure.ac
*       and then run autoheader.
*
* Author: Autogenerated by autoheader
* License: LGPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/
#ifndef EMC2_CONFIG_H
#define EMC2_CONFIG_H
])

AC_CHECK_HEADERS([sys/io.h])

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
])

AC_HEADER_SYS_WAIT
AC_CHECK_FUNCS(semtimedop)

AC_MSG_CHECKING([for optreset])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
extern int optreset;
    ]], [[ optreset = 1; ]])],[
	HAVE_OPTRESET=yes
        AC_DEFINE([HAVE_OPTRESET], [], [Define to 1 if getopt has the BSD 'optreset' extension])
],[HAVE_OPTRESET=no])
AC_MSG_RESULT($HAVE_OPTRESET)
AC_SUBST([HAVE_OPTRESET])



LIBS_hold=$LIBS
LIBS=
AC_SEARCH_LIBS(dlopen, [c dl], [
    case "$LIBS" in
    (-lc) LIBDL= ;;
    (*)   LIBDL="$LIBS"
    esac

    AC_DEFINE([LIBDL], [$LIBDL], [Define to the library that dlopen comes from, if it is not a part of libc])
], [AC_MSG_ERROR([not found])])
AC_SUBST([LIBDL])
LIBS=$LIBS_hold

AC_SEARCH_LIBS(clock_nanosleep, [rt], [
    AC_DEFINE([HAVE_CLOCK_NANOSLEEP], 1,
        [Define to 1 if clock_nanosleep is available (in -lc or -lrt)])
])

AH_BOTTOM(#endif
)

##############################################################################
# Section 5 - Check for tcl/tk support                                       #
#                                                                            #
# Test for tcl/tk headers fails if they are not in /usr/include              #
# so testing is done by finding tclConfig.sh and tkConfig.sh                 #
# if the search fails the traditional check is done                          #
##############################################################################
AC_MSG_CHECKING([for tcl])

AC_ARG_WITH(tclConfig,
    AS_HELP_STRING(
        [--with-tclConfig=PATH],
        [Specify the path to tclConfig.]
    ),
    [
        TCLCONFIG=$withval
    ],[
        TCLCONFIG=
    ])

# if not specified, search for tclConfig.sh
if (test "x$TCLCONFIG" = "x"); then
    TCLCONFIG=$(find /usr/lib* -maxdepth 2 -name tclConfig.sh | sort -r | head -n1)
    if (test "x$TCLCONFIG" = "x"); then
	TCLCONFIG=$(find /usr/local/lib* -maxdepth 2 -name tclConfig.sh | sort -r | head -n1)
    fi
fi

if (test "x$TCLCONFIG" = "x"); then
   AC_MSG_RESULT([tclConfig.sh not found, trying tcl.h and libs.
		  If it doesn't work try running ./configure --with-tclConfig=<path to tclConfig.sh>])
   AC_CHECK_HEADERS([tcl.h])
   AC_SEARCH_LIBS(Tcl_Init,[tcl tcl8.1 tcl8.2 tcl8.3 tcl8.4 tcl8.5],[],[AC_MSG_ERROR([tcl lib not found])])
else
# tclConfig.sh found,
# extracting vars
   AC_MSG_RESULT([$TCLCONFIG found])
   . "$TCLCONFIG"
   TCL_CFLAGS="$TCL_INCLUDE_SPEC -DUSE_TCL_STUBS"
   TCL_LIBS="$TCL_STUB_LIB_SPEC"
fi

AC_MSG_CHECKING([for tk])

AC_ARG_WITH(tkConfig,
    AS_HELP_STRING(
        [--with-tkConfig=PATH],
        [Specify the path to tkConfig.]
    ),
    [
        TKCONFIG=$withval
    ],[
        TKCONFIG=
    ])

# if not specified, search for tclConfig.sh
if test -z "$TKCONFIG"; then
    TKCONFIG=$(dirname $TCLCONFIG)/tkConfig.sh
    if ! test -f $TKCONFIG; then unset TKCONFIG; fi
fi
if (test "x$TKCONFIG" = "x"); then
    # search for tkConfig.sh
    TKCONFIG=$(find /usr/lib -maxdepth 2 -name tkConfig.sh | sort -r | head -n1)
    if (test "x$TKCONFIG" = "x"); then
	TKCONFIG=$(find /usr/local/lib -maxdepth 2 -name tkConfig.sh | head -n1)
    fi
fi

if (test "x$TKCONFIG" = "x"); then
   AC_MSG_RESULT([tkConfig.sh not found, trying tk.h and libs.
		  If it doesn't work try running ./configure --with-tkConfig=<path to tkConfig.sh>])
   AC_CHECK_HEADERS([tk.h])
   AC_SEARCH_LIBS(Tk_Init,[tk tk80 tk8.0 tk8.1 tk8.2 tk8.3 tk8.4 tk8.5],[],[AC_MSG_ERROR([tk lib not found])])
else
# tkConfig.sh found,
# extracting vars
   AC_MSG_RESULT([$TKCONFIG found])
   . "$TKCONFIG"
   TK_CFLAGS="$TK_INCLUDE_SPEC $TK_XINCLUDES"
   TK_LIBS="$TK_LIBS $TK_LIB_SPEC"
fi

if ! test -z "$TCL_VERSION" && ! test -z "$TK_VERSION" && ! test "$TCL_VERSION" = "$TK_VERSION" ; then
    AC_MSG_ERROR([Tcl and Tk versions must be the same, but configure found
Tcl $TCL_VERSION and Tk $TK_VERSION.  You can use --with-tkConfig= and --with-tclConfig=
to override the autodetected versions.])
fi

if test -f $TCL_EXEC_PREFIX/bin/wish$TCL_VERSION; then
    WISH=$TCL_EXEC_PREFIX/bin/wish$TCL_VERSION
elif test -f $TCL_EXEC_PREFIX/bin/wish; then
    WISH=$TCL_EXEC_PREFIX/bin/wish
else
    WISH=wish
fi

if test -f $TCL_EXEC_PREFIX/bin/tclsh$TCL_VERSION; then
    TCLSH=$TCL_EXEC_PREFIX/bin/tclsh$TCL_VERSION
elif test -f $TCL_EXEC_PREFIX/bin/tclsh; then
    TCLSH=$TCL_EXEC_PREFIX/bin/tclsh
else
    TCLSH=tclsh
fi

AC_ARG_ENABLE(check-runtime-deps,
    AS_HELP_STRING(
        [--disable-check-runtime-deps],
        [Skip checks for runtime requirements not needed during the
        build process.]
    ),
    [RUNTIME_CHECK=$enableval],
    [RUNTIME_CHECK=yes])
AC_MSG_CHECKING([whether to check for runtime dependencies])
AC_MSG_RESULT($RUNTIME_CHECK)

if test "$RUNTIME_CHECK" = "yes"; then
    AC_MSG_CHECKING([for BWidget using $TCLSH])
    if (unset DISPLAY; echo ["catch { package require bwidget }; exit [expr [lsearch [package names] BWidget] == -1]"] | $TCLSH); then
        AC_MSG_RESULT([found])
    else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([BWidget not found!
    install with "sudo apt-get install bwidget"])
    fi

    AC_MSG_CHECKING([for BLT using $TCLSH])
    if (unset DISPLAY; echo ['if [catch {package require BLT}] { exit 1; } else { exit 0; }'] | $TCLSH); then
        AC_MSG_RESULT([found])
    else
        AC_MSG_RESULT([not found])
    fi

    AC_MSG_CHECKING([for img::png using $TCLSH])
    if (unset DISPLAY; echo ["catch { package require img::png }; exit [expr [lsearch [package names] img::png] == -1]"] | $TCLSH); then
        AC_MSG_RESULT([found])
        HAVE_WORKING_BLT=yes
    else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Tk img::png not found!
    install with "sudo apt-get install libtk-img"])
    fi

    AC_MSG_CHECKING([for tclX using $TCLSH])
    if (unset DISPLAY; echo ["catch { package require Tclx }; exit [expr [lsearch [package names] Tclx] == -1]"] | $TCLSH); then
        AC_MSG_RESULT([found])
    else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Tclx not found!
    install with "sudo apt-get install tclx"])
    fi

    AC_MSG_CHECKING([for python pango module])
    if $PYTHON -c "import gi;gi.require_version('Pango', '1.0');from gi.repository import Pango"; then
        AC_MSG_RESULT([found])
    else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR(
[Python pango module not found!
install with "sudo apt-get install python3-gi"])
    fi
fi

AC_SUBST([TCL_DBGX])
AC_SUBST([TK_DBGX])
AC_SUBST([TCL_CFLAGS])
AC_SUBST([TK_CFLAGS])
AC_SUBST([TCL_LIBS])
AC_SUBST([TK_LIBS])
AC_SUBST([WISH])
AC_SUBST([TCLSH])

##############################################################################
# Section 6 - Miscellaneous support                                          #
#                                                                            #
# Subsection 6.1. - provides DATE to output it to the generated files        #
# Subsection 6.2. - user option to convince make to be quiet during make     #
##############################################################################

# provides DATE in order to output it to the generated files
DATE=$(date)
AC_SUBST([DATE])
AC_PATH_XTRA
#X_CFLAGS gets set
#X_LIBS and X_EXTRA_LIBS likewise

AC_SUBST([X_LIBS])
AC_SUBST([X_CFLAGS])

AC_CHECK_HEADERS(X11/extensions/Xinerama.h,
	    [HAVE_XINERAMA=yes],
	    [HAVE_XINERAMA=no])

AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
	    [],
	    [HAVE_XINERAMA=no])

if test "x$HAVE_XINERAMA" = "xno"; then
    AC_MSG_ERROR([Xinerama library or headers not found])
fi

#clean out LIBS
temp_LIBS=$LIBS
LIBS=

# check for readline.h and friends, optional for halcmd
AC_CHECK_HEADERS(readline/readline.h, [HAVE_READLINE=yes], [HAVE_READLINE=no])
AC_CHECK_HEADERS(readline/history.h, [], [HAVE_READLINE=no])

echo "Trying readline without -ltermcap"
AC_CHECK_LIB(readline, readline, [HAVE_READLINE_NOTERMCAP=yes], [])
unset ac_cv_lib_readline_readline
echo "Trying readline with -ltermcap"
AC_CHECK_LIB(readline, readline, [HAVE_READLINE_TERMCAP=yes], [], -ltermcap)
unset ac_cv_lib_readline_readline

if test "x$HAVE_READLINE_NOTERMCAP" = "xyes"; then
    echo "Readline does not require -ltermcap"
    READLINE_LIBS="-lreadline"
elif test "x$HAVE_READLINE_TERMCAP" = "xyes"; then
    echo "Readline requires -ltermcap"
    READLINE_LIBS="-lreadline -ltermcap"
else
    HAVE_READLINE=no
fi

AC_MSG_CHECKING([whether readline license is compatible with GPL-2])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #include <stdio.h>
        #include <readline/readline.h>
    ]], [[
        #if RL_VERSION_MAJOR > 5
        #error Readline version 6 and up are not compatible with GPL-2
        #endif
    ]])],[AC_MSG_RESULT(yes)],[
        AC_MSG_RESULT(no)
        AC_MSG_WARN(
[The LinuxCNC binary you are building may not be
distributable due to a license incompatibility with LinuxCNC (some portions
GPL-2 only) and Readline version 6 and greater (GPL-3 or later).])
        if $NONDISTRIBUTABLE; then
            AC_MSG_WARN(
[User requested to enable non-distributable builds.  Continuing.])
        else
            AC_MSG_ERROR(
[To configure LinuxCNC in this way, you must
invoke configure with "--enable-non-distributable=yes".  Note that on
Debian-based systems, you may be able to use libreadline-gplv2-dev instead.])
        fi
])

if test "x$HAVE_READLINE" = "xyes"; then
    AC_DEFINE([HAVE_READLINE], [], [Define to 1 if you have the 'readline' library (-lreadline) and required headers])
else
    AC_MSG_ERROR([Required library 'readline' missing.])
fi
AC_SUBST([HAVE_READLINE])
AC_SUBST([READLINE_LIBS])

##############################################################################
# Section 7 - Language support                                               #
#                                                                            #
# i18n Language support:                                                     #
# see http://cvs.gnome.org/viewcvs/intltool/doc/I18N-HOWTO?rev=1.3           #
##############################################################################

# if you don't have nls tools, allow a way out!

AC_ARG_ENABLE(nls,
    AS_HELP_STRING([--disable-nls], [Don't use NLS.]),
              USE_NLS=no, USE_NLS=yes)
AC_SUBST(USE_NLS)

if test "$USE_NLS" = "yes"; then
  AC_CHECK_HEADERS(locale.h)
  AC_CHECK_FUNCS(setlocale)
  AC_SUBST(HAVE_LOCALE_H)

# Add languages here when they are translated.
# Grep the LINGUAS file.
  LANGUAGES=""
  AC_SUBST(LANGUAGES)
  AC_CHECK_HEADERS(libintl.h)
  AC_CHECK_LIB(intl,gettext)
  AC_SUBST(HAVE_LIBINTL_H)
  AC_SUBST(HAVE_LIBINTL)

# The default locale directory is /usr/share/locale - This is generally fixed
# by the distribution....
  dnl Handle localedir
  LOCALEDIR='/usr/share/locale'
  AC_ARG_WITH(locale-dir,
  AS_HELP_STRING(
    [--with-locale-dir=DIR],
    [Location of the locale file(s) [DATADIR/locale].]
  ),[
    if test x$withval = xyes; then
      AC_MSG_WARN(Usage is: --with-locale-dir=basedir)
    else
      if test x$withval = xno; then
        AC_MSG_WARN(Usage is: --with-locale-dir=basedir)
      else
        LOCALEDIR=$withval
      fi
    fi
  ])
  AC_SUBST(LOCALEDIR)
fi

#restore LIBS
LIBS="$X_LIBS $temp_LIBS"

AC_PATH_PROG(XGETTEXT,xgettext,none)
XGETTEXT_TCL_WORKS=no
if test "$XGETTEXT" = "none"; then
   AC_MSG_ERROR(install gettext 0.14.5 or above)
fi
AC_SUBST(XGETTEXT)

AC_PATH_PROG(MSGFMT,msgfmt,none)
if test "$MSGFMT" = "none"; then
   AC_MSG_ERROR(install gettext 0.14.5 or above)
fi
AC_SUBST(MSGFMT)

if test "$PYTHON" = "none"; then
    AC_MSG_ERROR([Python missing.  Install it or specify --disable-python to skip the parts of LinuxCNC that depend on Python])
fi

AC_MSG_CHECKING([python version])
if ! $PYTHON -c 'import sys;raise SystemExit(sys.hexversion<0x2070000)'
then
    AC_MSG_ERROR(Python version too old (2.7 or newer required))
fi
AC_MSG_RESULT(OK)

AC_MSG_CHECKING([match between tk and Tkinter versions])
if $PYTHON -c 'import sys;raise SystemExit(sys.hexversion<0x3040000)'
then

	PYTHON_TK_VERSION="`$PYTHON -c 'import tkinter; print(tkinter.TkVersion)'`"
	PYTHON_TCL_VERSION="`$PYTHON -c 'import tkinter; print(tkinter.TclVersion)'`"

else

	PYTHON_TK_VERSION="`$PYTHON -c 'import _tkinter; print(_tkinter.TK_VERSION)'`"
	PYTHON_TCL_VERSION="`$PYTHON -c 'import _tkinter; print(_tkinter.TCL_VERSION)'`"

fi

if test "$PYTHON_TCL_VERSION" != "$TCL_VERSION"; then
    AC_MSG_RESULT([TCL mismatch: $TCL_VERSION vs $PYTHON_TCL_VERSION])
    AC_MSG_ERROR([Python requires use of Tcl $PYTHON_TCL_VERSION and Tk $PYTHON_TK_VERSION.
Install this version and specify --with-tclConfig and --with-tkConfig if necessary])
fi
if test "$PYTHON_TK_VERSION" != "$TK_VERSION"; then
    AC_MSG_RESULT([Tk mismatch: $TK_VERSION vs $PYTHON_TK_VERSION])
    AC_MSG_ERROR(["Python requires use of Tk $TK_VERSION.  Install this version and specify --with-tkConfig if necessary"])
fi
AC_MSG_RESULT([$PYTHON_TK_VERSION])

AC_MSG_CHECKING(for site-package location)
SITEPY=`$PYTHON -c 'import distutils.sysconfig; print(distutils.sysconfig.get_python_lib())'`
AC_MSG_RESULT($SITEPY)

AC_MSG_CHECKING(for working GLU quadrics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <GL/gl.h>
#include <GL/glu.h>
]], [[GLUquadric *q;]])],[AC_MSG_RESULT(yes)],[
            AC_MSG_ERROR([Required GLU library missing.  Install it or specify --disable-python to skip the parts of LinuxCNC that depend on Python])

])

AC_MSG_CHECKING(for Xmu headers)
AC_CHECK_HEADERS(X11/Xmu/Xmu.h,[],[AC_MSG_ERROR([Required Xmu header missing.  Install it, or specify --disable-python to skip the parts of LinuxCNC that depend on Python])])

AC_SUBST([LIBPYTHON])
AC_SUBST([SITEPY])

AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([CXXFLAGS])
AC_SUBST([LDFLAGS])

##############################################################################
# files that get created by ./configure                                      #
##############################################################################
#AC_CONFIG_FILES([../configs/rtapi.conf ../configs/emc.conf ../configs/hal.conf config.h])
AC_CONFIG_FILES([../scripts/rtapi.conf])
AC_CONFIG_FILES([../scripts/linuxcnc], [chmod +x ../scripts/linuxcnc])
AC_CONFIG_FILES([../scripts/linuxcnc_info], [chmod +x ../scripts/linuxcnc_info])
AC_CONFIG_FILES([../scripts/halrun], [chmod +x ../scripts/halrun])
AC_CONFIG_FILES([../scripts/rip-environment], [chmod +x ../scripts/rip-environment])
AC_CONFIG_FILES([../scripts/haltcl], [chmod +x ../scripts/haltcl])
AC_CONFIG_FILES([../scripts/halcmd_twopass], [chmod +x ../scripts/halcmd_twopass])
AC_CONFIG_FILES([../scripts/realtime], [chmod +x ../scripts/realtime])
AC_CONFIG_FILES([../scripts/runtests], [chmod +x ../scripts/runtests])
AC_CONFIG_FILES([../scripts/linuxcnc_var], [chmod +x ../scripts/linuxcnc_var])
AC_CONFIG_FILES([../scripts/linuxcnc-checklink], [chmod +x ../scripts/linuxcnc-checklink])
AC_CONFIG_FILES(Makefile.inc)
AC_CONFIG_FILES(Makefile.modinc)
AC_CONFIG_FILES(../tcl/linuxcnc.tcl)
AC_CONFIG_FILES(../lib/python/nf.py)
AC_CONFIG_FILES([../scripts/linuxcncmkdesktop], [chmod +x ../scripts/linuxcncmkdesktop])
AC_CONFIG_FILES(../share/applications/linuxcnc-latency.desktop)
AC_CONFIG_FILES(../share/applications/linuxcnc.desktop)
AC_CONFIG_FILES(../share/desktop-directories/linuxcnc-cnc.directory)
AC_CONFIG_FILES(../share/desktop-directories/linuxcnc-ref.directory)
AC_CONFIG_FILES(../share/desktop-directories/linuxcnc-doc.directory)
AC_CONFIG_FILES(../share/menus/CNC.menu)
AC_OUTPUT

##############################################################################
# message to the user what to do next, after a successful ./configure        #
##############################################################################
bold () {
    if tty > /dev/null 2>&1 && type -path tput > /dev/null 2>&1;
    then
        tput smso
    fi
}

offbold () {
    if tty > /dev/null 2>&1 && type -path tput > /dev/null 2>&1;
    then
        tput rmso
    fi
}

echo ""
echo ""
echo "######################################################################"
echo "#                LinuxCNC - Enhanced Machine Controller              #"
echo "######################################################################"
echo "#                                                                    #"
echo "#   LinuxCNC is a software system for computer control of machine    #"
echo "#   tools such as milling machines. LinuxCNC is released under the   #"
echo "#   GPL.  Check out http://www.linuxcnc.org/ for more details.       #"
echo "#                                                                    #"
echo "#                                                                    #"
echo "#   It seems that ./configure completed successfully.                #"
echo "#   This means that RT is properly installed                         #"
echo "#   If things don't work check config.log for errors & warnings      #"
echo "#                                                                    #"

if test "xyes" = "x$RUN_IN_PLACE"; then
echo "#   Next compile by typing                                           #"
echo "#         make                                                       #"
echo "#         sudo make setuid                                           #"
if test $RTS = uspace; then
echo "#          (if realtime behavior and hardware access are required)   #"
fi
echo "#                                                                    #"
echo "#   Before running the software, set the environment:                #"
echo "#         . (top dir)/scripts/rip-environment                        #"
else
bold
echo "#   warning: If you already have an installed linuxcnc, this will    #"
echo "#         replace an existing installation.  If you have installed   #"
echo "#         a linuxcnc package, this will damage the package.          #"
offbold
echo "#   hint: To test a self-built version of linuxcnc without damaging  #"
echo "#         the package version, don't specify a --prefix              #"
echo "#                                                                    #"
echo "#   Next compile by typing                                           #"
echo "#         make                                                       #"
echo "#   then install it by typing                                        #"
echo "#         sudo make install                                          #"
fi
echo "#                                                                    #"
echo "#   To run the software type                                         #"
echo "#         linuxcnc                                                   #"
echo "#                                                                    #"
echo "######################################################################"
echo ""
echo ""
