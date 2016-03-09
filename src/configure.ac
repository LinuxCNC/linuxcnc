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

AC_PREREQ(2.53)
AC_INIT([LinuxCNC],[m4_normalize(esyscmd(cat ../VERSION))],[emc-developers@lists.sourceforge.net])
AC_CONFIG_SRCDIR(emc/motion/motion.c)
if test "$srcdir" != "."; then
    AC_MSG_ERROR([Building outside of srcdir is not supported])
fi

## _PYTHON_SYSCONFIG(expression, [action-if-succeeded], [action-if-failed])
##
m4_define([_PYTHON_SYSCONFIG],
[dnl
py_val=`$PYTHON -c "dnl
[import sys; from distutils import sysconfig; sys.stdout.write]dnl
([sysconfig.]$1 or '')" 2>&AS_MESSAGE_LOG_FD`
AS_IF([test "[$]?" -eq 0 && test "x$py_val" != x], [$2], [$3])[]dnl
])

# Python module import test
#
# adapted from AX_BOOST_PYTHON_MURRAYC
# http://ftp.gnome.org/pub/GNOME/sources/glom/1.19/
m4_define([AX_BOOST_PYTHON_EMC],
[
AC_LANG_PUSH([C++])
saved_CPPFLAGS=$CPPFLAGS
saved_LIBS=$LIBS
# Note that this requires PYTHON_CPPFLAGS
# Note that this expects boost/ to be at some top-level such as /usr/include/
# We couldn't check for anything else anyway because there's no pkg-config file to tell us where it is
CPPFLAGS="$PYTHON_CPPFLAGS $saved_CPPFLAGS"
AC_CACHE_CHECK([whether the Boost::Python headers are available],
               [ac_cv_boost_python],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <boost/python/module.hpp>
using namespace boost::python;
BOOST_PYTHON_MODULE(test) { (void)0; }
]], [])],
    [ac_cv_boost_python=yes], [ac_cv_boost_python=no])])

AS_IF([test "x$ac_cv_boost_python" = xyes],
[
  BOOST_PYTHON_LIBS=
  ax_python_lib=boost_python
  AC_ARG_WITH([boost-python],
              [AS_HELP_STRING([--with-boost-python],
                              [specify the boost python shared library to use. For instance, --with-boost-python=boost_python-py25. Defaults to boost-python. If you use this then you should probably set PYTHON too, to avoid using multiple python versions.])],
  [if test "x$with_boost_python" != xno; then
    ax_python_lib=$with_boost_python
    ax_boost_python_lib=boost_python-$with_boost_python
  fi])
  AC_MSG_CHECKING([for boost::python shared library])
  for ax_lib in "$ax_python_lib" "$ax_boost_python_lib" boost_python
  do
    # Note that this requires LIBPYTHON from configure.ac
    LIBS="$saved_LIBS -l$LIBPYTHON -l$ax_lib"
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM([[#include <boost/python.hpp>]], [[boost::python::object test_object;]])],
      [BOOST_PYTHON_LIBS="-l$ax_lib"; break])
  done
  AS_IF([test "x$BOOST_PYTHON_LIBS" != x], [ax_result=$BOOST_PYTHON_LIBS], [ax_result=])
  AC_MSG_RESULT([$ax_result])
])
CPPFLAGS=$saved_CPPFLAGS
LIBS=$saved_LIBS
AC_LANG_POP([C++])
AS_IF([test "x$ac_cv_boost_python" = xyes && test "x$BOOST_PYTHON_LIBS" != x],
      [AC_DEFINE([HAVE_BOOST_PYTHON], [1],
                 [define if the Boost::Python headers and library are available])],
      [AC_MSG_ERROR([[boost::python is required to build $PACKAGE_NAME]])])
AC_SUBST([BOOST_PYTHON_LIBS])
])

# Cython Python Bindings generator
# 0.19.1 is the version currently in wheezy-backports
AC_PROG_CYTHON(0.19.0)
if test "$CYTHON" = "false"; then
    AC_MSG_ERROR([not found])
fi
AC_SUBST([CYTHON], [$CYTHON])


AC_MSG_CHECKING(build toplevel)
BUILD_TOPLEVEL="$(cd ..; pwd -P)"
AC_MSG_RESULT($BUILD_TOPLEVEL)

AC_CONFIG_HEADER(config.h)

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
PACKAGE="machinekit"
AC_SUBST([PACKAGE])

if test -f ../VERSION; then
    EMC2VERSION="$(cat ../VERSION)"
else
    EMC2VERSION="Undetermined"
fi
AC_SUBST([EMC2VERSION])

AC_PATH_PROG(GREP, grep)
AC_PATH_PROG(EGREP, egrep)
AC_PATH_PROG(ARCHCMD, arch)

AC_PROG_CC
AC_SUBST([CROSS_COMPILING],[$cross_compiling])
dnl Put a plausible default for CC_FOR_BUILD in Makefile.inc.in
if test "$cross_compiling" = yes; then
    AC_PATH_PROGS(CC_FOR_BUILD, gcc cc)
else
    AC_SUBST(CC_FOR_BUILD,$CC)
fi

AC_PROG_CPP()
AC_CHECK_TOOL(OBJCOPY, objcopy)
AC_CHECK_TOOL(LD, ld)

# tools for correct cross-building; this sets $host (the target
# system) to e.g. x86_64-unknown-linux-gnu
AC_CANONICAL_HOST  # $host, host_cpu, host_vendor, and host_os
AC_CANONICAL_BUILD  # $build, build_cpu, build_vendor, and build_os

# locate pkg-config, including cross-compiler variations
if test "$cross_compiling" = yes; then
   AC_CHECK_TOOL(PKG_CONFIG, pkg-config)
else
   # The multi-arch version can choke on non-multi-arch packages, since it
   # (intentionally) doesn't look in `/usr/lib/pkgconfig`.
   AC_CHECK_PROGS(PKG_CONFIG, pkg-config)
   dnl Avoid PKG_CHECK_MODULES re-checking
   ac_cv_env_PKG_CONFIG_set=set
fi
AC_CONFIG_MACRO_DIR(/usr/share/aclocal)


##############################################################################
# Section 2                                                                  #
# RealTime checking                                                          #
# Subsection 2.1 - init basic thread style params and declare m4 macros      #
# Subsection 2.2 - documentation of threads variables to be configured       #
# Subsection 2.3 - misc basic features:  platform, shmdrv, parport I/O, etc. #
# Subsection 2.4 - kernel detection and RT capability probing                #
# Subsection 2.5 - thread style configuration                                #
# Subsection 2.6 - userland PCI driver configuration                         #
# Subsection 2.7 - pass configuration into config.h, Makefile.inc.in, etc.   #
##############################################################################

##############################################################################
# Subsection 2.1                                                             #
# init basic thread style params                                             #
# - the [_cfg_flavor_params] m4 macro lists all flavors                      #
# - for each flavor, lists name, flavor_id, build_sys and threads_source     #
#                                                                            #
# define several m4 macros to help manipulate the list                       #
##############################################################################


dnl #--------------------------------------------------------#
dnl # list of all thread styles with FLAVOR_ID and BUILD_SYS #
dnl #                                                        #
dnl # each thread style needs a 4-item m4 tuple set here:    #
dnl # flavor-name, flavor-id, build-sys, threads-headers     #
dnl #                                                        #
dnl # flavor-name: lower-case with hyphens                   #
dnl # flavor-id: a unique integer                            #
dnl # build-sys: one of kbuild or user-dso                   #
dnl # threads-headers: header file from rtapi/*.h (w/o '.h') #
dnl # mod_ext:  RTAPI module extension, .so or .ko           #
dnl # so_ext:  ULAPI module extension, .so                   #
dnl # flags:  see rtapi.h; combine with '|' operator, or 0   #
dnl # extra_kmods:  list of extra kmodules to load           #
dnl #                                                        #
dnl # these are used in both Makefiles and in config.h,      #
dnl # and are the single authoritative source for these data #
dnl #--------------------------------------------------------#
AC_DEFUN([_cfg_flavor_params],
	[[[posix],[0],[user-dso],[rt-preempt],[.so],[.so],[0]],
	 [[rt-preempt],[1],[user-dso],[rt-preempt],[.so],[.so],[0]],
	 [[xenomai],[2],[user-dso],[xenomai],[.so],[.so],[FLAVOR_DOES_IO]],
	 [[xenomai-kernel],[3],[kbuild],[xenomai-kernel],[.ko],[.so],dnl
[FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD],[rtapi_math]],
	 [[rtai-kernel],[4],[kbuild],[rtai-kernel],[.ko],[.so],dnl
[FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD],dnl
[rtai_hal rtai_sched rtapi_math]]])


dnl #--------------------------------------------------------#
dnl # m4 convenience macros for accessing above data         #

dnl list of all thread styles to be used in macros
dnl   posix,rt-preempt,xenomai,xenomai-kernel,rtai
AC_DEFUN([_cfg_all_flavors],
	m4_map_sep([m4_car],[[,]],[_cfg_flavor_params]))

dnl _cfg_flavor_id(flavor):  return THREAD_FLAVOR_ID
dnl   _cfg_flavor_id(xenomai) => 2; _cfg_flavor_id(rtai) => 4
AC_DEFUN([_cfg_flavor_tuple_id],[$2])
AC_DEFUN([_cfg_flavor_id],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_id],[flav]))])])

dnl _cfg_flavor_build_sys(flavor):  return BUILD_SYS:  kbuild or user-dso
dnl   for use in Makefiles
dnl   _cfg_flavor_build_sys(xenomai) => user-dso;
dnl   _cfg_flavor_build_sys(xenomai-kernel) => kbuild;
AC_DEFUN([_cfg_flavor_tuple_build_sys],[$3])
AC_DEFUN([_cfg_flavor_build_sys],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_build_sys],[flav]))])])

dnl _cfg_flavor_threads_headers(flavor):  return THREADS_HEADERS
dnl   for use in config.h
dnl   _cfg_flavor_threads_headers(xenomai) => rtapi/xenomai.h;
dnl   _cfg_flavor_threads_headers(posix) => rtapi/rt-preempt.h;
AC_DEFUN([_cfg_flavor_tuple_threads_headers],[$4])
AC_DEFUN([_cfg_flavor_threads_headers],
	rtapi/[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_threads_headers],[flav]))])].h)

dnl _cfg_flavor_mod_ext(flavor):  return RT module extension:  .ko or .so
dnl   for use in rtapi.ini
dnl   _cfg_flavor_mod_ext(xenomai) => .so;
dnl   _cfg_flavor_mod_ext(xenomai-kernel) => .ko;
AC_DEFUN([_cfg_flavor_tuple_mod_ext],[$5])
AC_DEFUN([_cfg_flavor_mod_ext],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_mod_ext],[flav]))])])

dnl _cfg_flavor_so_ext(flavor):  return plugin filename extension:  .so
dnl   for use in rtapi.ini
dnl   _cfg_flavor_so_ext(xenomai) => .so
AC_DEFUN([_cfg_flavor_tuple_so_ext],[$6])
AC_DEFUN([_cfg_flavor_so_ext],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_so_ext],[flav]))])])

dnl _cfg_flavor_flags(flavor):  flavor flags:  integer expression
dnl   for use in rtapi.ini
dnl   _cfg_flavor_flags(rt-preempt) => 0
dnl   _cfg_flavor_flags(xenomai) => FLAVOR_DOES_IO
dnl   _cfg_flavor_flags(xenomai-kernel) => FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD
AC_DEFUN([_cfg_flavor_tuple_flags],[$7])
AC_DEFUN([_cfg_flavor_flags],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_flags],[flav]))])])

dnl _cfg_flavor_extra_kmods(flavor):  extra kernel modules to load for kthreads
dnl   for use in rtapi.ini
dnl   _cfg_flavor_extra_kmods(xenomai-kernel) => rtapi_math
AC_DEFUN([_cfg_flavor_tuple_extra_kmods],[$8])
AC_DEFUN([_cfg_flavor_extra_kmods],
	[m4_foreach([flav],[_cfg_flavor_params],
	    [m4_cond(m4_car(flav),$1,
	        m4_apply([_cfg_flavor_tuple_extra_kmods],[flav]))])])

dnl _cfg_flavor_upcase(flavor):  return uppercase, and dashes->underscores
dnl   used to generate C preprocessor macro names
dnl   _cfg_flavor_ucase(xenomai-kernel) => XENOMAI_KERNEL
AC_DEFUN([_cfg_flavor_upcase],[m4_translit([$*],[a-z-],[A-Z_])])

dnl _cfg_flavor_underscored(flavor):  return dashes->underscores
dnl   used to generate e.g. $with_xenomai_kernel
AC_DEFUN([_cfg_flavor_underscored],[m4_translit([$*],[-],[_])])

dnl _cfg_flavor_BUILD_SYS(flavor):  return BUILD_SYS_{KBUILD,USER_DSO}
dnl   for use in config.h
dnl   _cfg_flavor_BUILD_SYS(xenomai) => BUILD_SYS_USER_DSO;
dnl   _cfg_flavor_BUILD_SYS(xenomai-kernel) => BUILD_SYS_KBUILD;
AC_DEFUN([_cfg_flavor_BUILD_SYS],
	[BUILD_SYS_[]_cfg_flavor_upcase(_cfg_flavor_build_sys($1))])

dnl _cfg_kbuild_flavors:  list of kbuild RT thread flavors
AC_DEFUN([_cfg_flavor_tuple_name],[$1])
m4_foreach([flav],[_cfg_flavor_params],
	[m4_cond(_cfg_flavor_build_sys(flav),[kbuild],
	    [m4_append_uniq([_cfg_tmp],_cfg_flavor_tuple_name(flav),[[ ]])]
)])

dnl   _cfg_kbuild_flavors => xenomai-kernel,rtai
AC_DEFUN([_cfg_kbuild_flavors],m4_split(_cfg_tmp))


#--------------------------------------------------------#
# Universal build variables and macros
#
# new thread systems introduced here:
# http://wiki.linuxcnc.org/cgi-bin/wiki.pl?RealtimeConfiguratonProposal
#
# at the shell script/makefile level, we have:
#
# $(threads) - one of 'rtai-kernel', 'posix', 'xenomai', 'xenomai-kernel',
# 'rt-preempt' in shell/Makefile
#
# the Makefile will run '$(MAKE) threads=<flavor> modules' once for
# each flavor configured here.
#
# This is used to generate the upper-case, underscored $(THREADS),
# which is e.g. POSIX or RTAI_KERNEL.
#
# $(THREADS) in turn is used to select the correct configuration
# variables out of Makefile.inc.  For example, THREADS_RTFLAGS is set
# to RT_PREEMPT_THREADS_RTFLAGS for the rt-preempt flavor.


##############################################################################
# Subsection 2.2                                                             #
# Variable documentation                                                     #
#                                                                            #
# Here the various Makefile variables and CPP macros generated by this       #
# script are documented                                                      #
##############################################################################

#--------------------------------------------------------#
# Makefile variables                                     #

# RT system Makefile variables defined by configure:
#
# BUILD_THREAD_FLAVORS
#
# a list of all configured threads systems to build; the complete
# possible list is 'posix rt-preempt xenomai xenomai-kernel
# rtai-kernel'
#
# <THREADS_SYSTEM>_THREADS_RTS
#
# a script like /usr/realtime/foo-config, which can be used to extract
# c or ld flags specific for the thread system.
#
# HAVE_<THREADS_SYSTEM>_THREADS
#
# 'yes' or 'no' depending on whether the thread system was able to be
# configured
#
# <THREADS_SYSTEM>_THREADS_FLAVOR_ID
#
# 0..4, depending on the _cfg_flavor_params macro defined earlier;
# this is used in config.h to set flavor-specific macros, and set in
# cflags for any compile that #includes config.h
#
# <THREADS_SYSTEM>_THREADS_BUILD_SYS
#
# 'kbuild' for kernel threads, 'user-dso' for user threads
#
# <THREADS_SYSTEM>_THREADS_SOURCE
#
# the basename of the flavor's file in src/rtapi; this is usually the
# name of the flavor, except in the case of posix threads, which use
# the rt-preempt sources
#
# <THREADS_SYSTEM>_THREADS_RTFLAGS
#
# list of CFLAGS specific to the threads system; some flavors
# determine this with <THREADS_SYSTEM>_THREADS_RTS --cflags
#
# <THREADS_SYSTEM>_THREADS_LDFLAGS
#
# list of LDFLAGS specific to the threads system; also obtained partly
# from <THREADS_SYSTEM>_THREADS_RTS --ldflags
#
# <THREADS_SYSTEM>_THREADS_KERNEL_DIRS
#
# for kbuild flavors only (currently); list of directory paths for the
# flavor's configured kernel source trees
#
# paths may be explicitly set on the ./configure command line using
# '--with-xenomai-kernel-sources' or '--with-rtai-kernel-sources'
#
# otherwise kernel sources are automatically detected from standard
# locations, which may be extended using '--with-extra-kernel-sources'
#
# SHMDRV_KERNEL_DIRS
#
# similar to above, a list of directory paths for which to build the
# shmdrv.ko module
#
# as above, paths may be set using '--with-shmdrv-kernel-sources'
#
# --enable-shmdrv builds shmdrv.ko for all configured kernels,
# regardless of whether other kthreads modules will be built;
# shmdrv.ko is always built for kthreads modules
#
# <THREADS_SYSTEM>_THREADS_EXTRA_SYMBOLS
#
# path name to a Module.symvers file needed by kbuild
#
# <THREADS_SYSTEM>_THREADS_KERNEL_MATH_CFLAGS
#
# math-related CFLAGS used in RTAPI compiles; partly obtained by
# <THREADS_SYSTEM>_THREADS_RTS
#
# RTAI_KERNEL_THREADS_RTDIR
#
# RTAI only; the directory RTAI modules are contained in, such as
# /usr/realtime/modules, obtained by RTAI_KERNEL_THREADS_RTS --prefix

#--------------------------------------------------------#
# config.h macros                                        #
#
# $(THREADS) is used to select the RTAPI_FLAVOR_ID, passed in a CFLAG
# as -DRTAPI_FLAVOR_ID=$(RTAPI_FLAVOR_ID), then used to select the
# appropriate flavor macros from config.h.
#
# flavor macros from config.h, set appropriately for each '$(MAKE)
# threads=<flavor> modules' run:
#
# RTAPI_<FLAVOR>
#
# for example '#ifdef RTAPI_RTAI_KERNEL' to test if the current flavor
# being built is rtai-kernel

# THREAD_FLAVOR_NAME
#
# the name of the current flavor, quoted, e.g. "posix" or "rt-preempt"

# BUILD_SYS_USER_DSO or BUILD_SYS_KBUILD
#
# The former will be defined for userland threads, the latter for
# kernel threads
#
# THREADS_HEADERS
#
# Used by the rtapi system to #include the correct headers for the
# flavor currently being built, e.g. "rtapi/xenomai.h"

# RTAPI_<FLAVOR>_ID
#
# Always present for all flavors.  Equal to
# <FLAVOR>_THREADS_FLAVOR_ID.

# RTAPI_<FLAVOR>_NAME
#
# Always present for all flavors.  Equal to $(threads)

# GIT_VERSION
#
# a string containing the content of src/VERSION, git branch name,
# and shortened SHA, valid at the time when configure was run.
# generated by running scripts/get-version-from-git at configure time.
# Example:
#
# define GIT_VERSION "v2.6.0~pre~make-clean-check~0255c8c"

# GIT_CONFIG_SHA
#
# a string containing the shortened SHA, valid at the time when configure was run.
# generated by running scripts/get-git-sha at configure time.
# Example:
#
# GIT_CONFIG_SHA "0255c8c"
# the Makefile runs the same script, and adds -DGIT_BUILD_SHA=\"'$(GIT_BUILD_SHA)'\"
# to CFLAGS for logging or a runtime check

# ------------ drivers: build or dont build -------------
#
# BUILD_DRIVERS
#
# ./configure will attempt to configure driver building unless
# explicitly disabled with '--disable-drivers'.
#
# To reduce the surprise factor of a simulator config suddenly talking
# to ports, drivers will be disabled by default for posix threads and
# enabled for others in rtapi.ini.
#
# reflected in config.h as follows:
# /* build hardware drivers */
# #define BUILD_DRIVERS /**/
#
#
KSOURCE_LOCATION_HINTS=$(echo \
	/usr/src/kernels/* \
	/usr/src/linux-headers-* \
	/lib/modules/*/build \
	)
#
# Places to look for Xeno and RTAI kernel headers
#
# RedHat-derivatives: (/usr/src/kernels/`uname -r`),
# Debian-derivatives: (/usr/src/linux-headers-`uname -r`)
# Vanilla kernel install: (/lib/modules/`uname -r`/build)


# USE_PORTABLE_PARPORT_IO="no"
#
# Parport: on x86 PC platform, we can use either use
# direct inb/outb or the slower but portable ppdev ioctl
#
# reflected in config.h as follows:
# /* build hardware drivers */
# #define USE_PORTABLE_PARPORT_IO /**/

# PLATFORM
# TARGET_PLATFORM_PC
# TARGET_PLATFORM_BEAGLEBONE
# TARGET_PLATFORM_RASPBERRY
# TARGET_PLATFORM_ZEDBOARD
#
# Platforms. set by --with-platform-{pc,beaglebone,raspberry,zedboard}
# see Section 2.3 to enable other platforms, and 2.7 to AC_DEFINE it

ARCHITECTURE=`$ARCHCMD`
#
# Architecture. Use output of arch(1).
#
# FIXME this will break 32-bit builds on 64-bit systems.  Need an
# expert to figure out host/target arch, e.g. --host=i686-pc-linux-gnu
# even when --build=x86_64-pc-linux-gnu
#
# This will cause issues on build servers that build for multiple
# arches

# xenomai equivalent of pkg-config
XENO_CONFIG=

# note to mah: dont forget debian/configure FIXME


##############################################################################
# Subsection 2.3                                                             #
# Configure several basic features:                                          #
# - some etc/linuxcnc/machinekit.ini  values                                 #
# - the MKUUID instance UUID                                                 #
# - Platform (PC, Raspberry Pi, BeagleBone, Zedboard)                        #
# - Common shared memory driver                                              #
# - Parport I/O mode (portable or inb/outb)                                  #
# - emcweb UI                                                                #
##############################################################################

# Platform:
#
# By default, build all platforms applicable to the $host arch
#
# If any --with-platform-* provided, only build for that platform if
# $host arch applies
#
# If any --without-platform-* provided, build everything *except* that
# platform, if $host arch applies

platform_specified=false
# Status messages
arch_enab_msg="default enabled for this architecture"
arch_disab_msg="disabled for this architecture"
cmdl_enab_msg="enabled on command line"
cmdl_disab_msg="disabled on command line"
cmdl_notspec_msg="not specified on command line"

AC_ARG_WITH(platform-pc,
    [AS_HELP_STRING(
        [--with-platform-pc],
	[Build for PC platform (default for x86 arch)])],
    [
        case $with_platform_pc in
	    (y*)
	    	TARGET_PLATFORM_PC=true
		platform_specified=true
		platform_pc_reason="$cmdl_enab_msg"
		;;
	    (*)
	    	TARGET_PLATFORM_PC=false
		platform_pc_reason="$cmdl_disab_msg"
		;;
	esac
    ],
    [
        case $host_cpu in
	    (i?86 | x86_64)
	        TARGET_PLATFORM_PC=unk
		;;
	    (*)
	        TARGET_PLATFORM_PC=false
		platform_pc_reason="$arch_disab_msg"
		;;
	esac
    ])

AC_ARG_WITH(platform-beaglebone,
    [AS_HELP_STRING(
        [--with-platform-beaglebone],
	[Build for Beaglebone platform (default for ARM arch)])],
    [
        case $with_platform_beaglebone in
	    (y*)
	    	TARGET_PLATFORM_BEAGLEBONE=true
		platform_specified=true
		platform_beaglebone_reason="$cmdl_enab_msg"
		;;
	    (*)
	    	TARGET_PLATFORM_BEAGLEBONE=false
		platform_beaglebone_reason="$cmdl_disab_msg"
		;;
	esac
    ],
    [
        case $host_cpu in
	    (arm*)
	        TARGET_PLATFORM_BEAGLEBONE=unk
		;;
	    (*)
	        TARGET_PLATFORM_BEAGLEBONE=false
		platform_beaglebone_reason="$arch_disab_msg"
		;;
	esac
    ])

AC_ARG_WITH(platform-raspberry,
    [AS_HELP_STRING(
        [--with-platform-raspberry],
	[Build for Raspberry platform (default for ARM arch)])],
    [
        case $with_platform_raspberry in
	    (y*)
	    	TARGET_PLATFORM_RASPBERRY=true
		platform_specified=true
		platform_raspberry_reason="$cmdl_enab_msg"
		;;
	    (*)
	    	TARGET_PLATFORM_RASPBERRY=false
		platform_raspberry_reason="$cmdl_disab_msg"
		;;
	esac
    ],
    [
        case $host_cpu in
	    (arm*)
	        TARGET_PLATFORM_RASPBERRY=unk
		;;
	    (*)
	    	TARGET_PLATFORM_RASPBERRY=false
		platform_raspberry_reason="$arch_disab_msg"
		;;
	esac
    ])

AC_ARG_WITH(platform-zedboard,
    [AS_HELP_STRING(
        [--with-platform-zedboard],
	[Build for Zedboard platform])],
    [
        case $with_platform_zedboard in
	    (y*)
	    	TARGET_PLATFORM_ZEDBOARD=true
		platform_specified=true
		platform_zedboard_reason="$cmdl_enab_msg"
		;;
	    (*)
	    	TARGET_PLATFORM_ZEDBOARD=false
		platform_zedboard_reason="$cmdl_disab_msg"
		;;
	esac
    ],
    [
        case $host_cpu in
	    (arm*)
	        TARGET_PLATFORM_ZEDBOARD=unk
		;;
	    (*)
	    	TARGET_PLATFORM_ZEDBOARD=false
		platform_zedboard_reason="$arch_disab_msg"
		;;
	esac
    ])

if $platform_specified; then
    # Platform was explicitly specified using --with-platform-*;
    # disable anything not explicitly specified
    if test $TARGET_PLATFORM_PC = unk; then
	TARGET_PLATFORM_PC=false
	platform_pc_reason="$cmdl_notspec_msg"
    fi
    if test $TARGET_PLATFORM_BEAGLEBONE = unk; then
	TARGET_PLATFORM_BEAGLEBONE=false
	platform_beaglebone_reason="$cmdl_notspec_msg"
    fi
    if test $TARGET_PLATFORM_RASPBERRY = unk; then
	TARGET_PLATFORM_RASPBERRY=false
	platform_raspberry_reason="$cmdl_notspec_msg"
    fi
    if test $TARGET_PLATFORM_ZEDBOARD = unk; then
	TARGET_PLATFORM_ZEDBOARD=false
	platform_zedboard_reason="$cmdl_notspec_msg"
    fi
else
    # Platform was not explicitly specified; enable anything
    # applicable to this architecture and not explicitly disabled
    # (these tests performed earlier)
    if test $TARGET_PLATFORM_PC = unk; then
        TARGET_PLATFORM_PC=true
	platform_pc_reason="$arch_enab_msg"
    fi
    if test $TARGET_PLATFORM_BEAGLEBONE = unk; then
        TARGET_PLATFORM_BEAGLEBONE=true
	platform_beaglebone_reason="$arch_enab_msg"
    fi
    if test $TARGET_PLATFORM_RASPBERRY = unk; then
        TARGET_PLATFORM_RASPBERRY=true
	platform_raspberry_reason="$arch_enab_msg"
    fi
    if test $TARGET_PLATFORM_ZEDBOARD = unk; then
        TARGET_PLATFORM_ZEDBOARD=true
	platform_zedboard_reason="$arch_enab_msg"
    fi
fi

# Print messages about what platforms are to be enabled or disabled
AC_MSG_CHECKING(platform-pc)
AC_MSG_RESULT([$platform_pc_reason])

AC_MSG_CHECKING(platform-beaglebone)
AC_MSG_RESULT([$platform_beaglebone_reason])

AC_MSG_CHECKING(platform-raspberry)
AC_MSG_RESULT([$platform_raspberry_reason])

AC_MSG_CHECKING(platform-zedboard)
AC_MSG_RESULT([$platform_zedboard_reason])

# every Machinekit instance needs a unique ID, so generate here.
# Can be changed later in etc/linuxcnc/machinekit.ini
AC_PATH_PROG(UUIDGEN, uuidgen)
if test "x$UUIDGEN" = "x"; then
  AC_MSG_ERROR([ uuidgen not found - please install package uuid-runtime ])
  AC_MSG_RESULT(no)
else
  MKUUID=`uuidgen`
  AC_MSG_RESULT(setting unique Machinekit UUID to $MKUUID)
  AC_SUBST([MKUUID])
fi

AC_MSG_CHECKING(whether to enable remote operation)
AC_ARG_ENABLE(remote,
    [  --enable-remote      enable remote zeroMQ services],
    [
	REMOTE=1
        AC_MSG_RESULT([configuring enable remote zeroMQ services])
    ],
    [
      AC_MSG_RESULT([local operation on IPC sockets only, zeroconf not used])
      REMOTE=0
    ])
AC_SUBST([REMOTE])

# this should move to /var/run eventually
# it might mean a subdirectory needs to be created with proper permissions
AC_MSG_CHECKING(which directory to use for IPC sockets and other transient files)
AC_ARG_WITH(rundir,
    [  --with-rundir=<directory>  directory for IPC sockets and other transient files (default /tmp)],
    [
	RUNDIR="$withval"
        AC_MSG_RESULT([configuring to use $RUNDIR for transient files like IPC sockets])
    ],
    [
      RUNDIR=/tmp
      AC_MSG_RESULT([using /tmp for IPC sockets and other transient files])
    ])
AC_DEFINE_UNQUOTED([RUNDIR], "$RUNDIR", [directory for IPC sockets and other transient files])

AC_SUBST([RUNDIR])

AC_MSG_CHECKING(whether to build programming examples)
AC_ARG_ENABLE(examples,
    [  --enable-examples      build included programming examples],
    [
	BUILD_EXAMPLES=yes
        AC_MSG_RESULT([configuring to build example programs])
    ],
    [
      AC_MSG_RESULT([not building example programs])
      BUILD_EXAMPLES=no
    ])

# protobuf-to-Javascript generator from  https://github.com/dcodeIO/ProtoBuf.js/wiki
AC_PATH_PROG(PROTO2JS,proto2js, none)
AC_SUBST([PROTO2JS])

AC_MSG_CHECKING(whether to build protobuf Javascript bindings)
AC_ARG_ENABLE(proto-js,
    [  --enable-proto-js    build Javascript bindings based on https://github.com/dcodeIO/ProtoBuf.js/wiki],
    [
	if test "$PROTO2JS" = "none"; then
	   AC_MSG_ERROR([Please install proto2js before building Javascript bindings.])
	fi
	BUILD_PROTO_JS=yes
        AC_MSG_RESULT([configuring to to build protobuf Javascript bindings])
    ],
    [
      AC_MSG_RESULT([not protobuf Javascript bindings])
      BUILD_PROTO_JS=no
    ])


AC_MSG_CHECKING(whether to use the common shared memory driver)
AC_ARG_ENABLE(shmdrv,
    [  --enable-shmdrv      use the common shared memory driver kernel module],
    [
        case "$enableval" in
            Y*|y*)
		USE_SHMDRV=yes
                AC_MSG_RESULT([explicitly configuring for common shared memory driver kernel module])
            ;;
            N*|n*)
		USE_SHMDRV=no
                AC_MSG_RESULT([explicitly disabling shared memory driver kernel module])
            ;;
        esac
	if test -z "USE_SHMDRV"; then
	   AC_MSG_ERROR([ I dont understand $enableval, just yes or no ])
	fi
    ],
    [
      USE_SHMDRV=no
      AC_MSG_RESULT([default is no])
    ])

AC_MSG_CHECKING(whether to use inb/outb or ppdev ioctl on the x86 for parport I/O)
AC_ARG_ENABLE(portable-parport,
    [  --enable-portable-parport      Build hal_parport using the ppdev ioctl instead of inb/outb],
    [
        case "$enableval" in
            Y*|y*)
		USE_PORTABLE_PARPORT_IO=yes
                AC_MSG_RESULT([explicitly configuring for ppdev ioctl])
            ;;
            N*|n*)
		USE_PORTABLE_PARPORT_IO=no
                AC_MSG_RESULT([explicitly disabling ppdev ioctl and using inb/outb])
            ;;
        esac
	if test -z "$USE_PORTABLE_PARPORT_IO"; then
	   AC_MSG_ERROR([ I dont understand $enableval, just yes or no ])
	fi
    ],
    [
      USE_PORTABLE_PARPORT_IO=no
      AC_MSG_RESULT([using defaults: $USE_PORTABLE_PARPORT_IO])
    ])

AC_MSG_CHECKING(whether to build the emcweb interface)
AC_ARG_ENABLE(emcweb,
    [  --enable-emcweb      build the emcweb interface],
    [
	BUILD_EMCWEB=yes
        AC_MSG_RESULT([configuring to build emcweb interface])
    ],
    [
      AC_MSG_RESULT([not building the emcweb interface])
      BUILD_EMCWEB=no
    ])

AC_MSG_CHECKING(whether to build unstable development code)
AC_ARG_ENABLE(dev,
    [  --enable-dev       build unstable development code],
    [
	BUILD_DEV=yes
        AC_MSG_RESULT([configuring to build unstable development code])
    ],
    [
      AC_MSG_RESULT([not building  development code])
      BUILD_DEV=no
    ])

##############################################################################
# Subsection 2.3.1                                                           #
# protobuf, zmq, websockets, json, uriparse, avahi-client packages           #
# NML
##############################################################################

PKG_CHECK_MODULES([CZMQ], [libczmq > 2.0],
    [
        AC_DEFINE(HAVE_CZMQ, [], [zeroMQ czmq library available])
	USE_CZMQ=yes
    ],)

PKG_CHECK_MODULES([PROTOBUF], [protobuf > 2.4.0],
    [
        AC_DEFINE(HAVE_PROTOBUF, [], [zeroMQ czmq library available])
	USE_PROTOBUF=yes
    ],)

AC_PATH_PROG(PROTOC, protoc, "none")
if test $PROTOC = "none"
then
    AC_MSG_ERROR([protoc not found - protobuf-compiler package not installed?])
fi

AC_PYTHON_MODULE(google.protobuf.descriptor, failed to import google.protobuf.descriptor
 - python-protobuf package missing? )
AC_PYTHON_MODULE(google.protobuf.descriptor_pb2, failed to import google.protobuf.descriptor_pb2
 - python-protobuf package missing? )
AC_PYTHON_MODULE(google.protobuf.compiler.plugin_pb2, failed to import google.protobuf.compiler.plugin_pb2
 - python-protobuf package missing? )

PKG_CHECK_MODULES([JANSSON], [jansson > 2.4],
    [
        AC_DEFINE(HAVE_JANSSON, [], [jansson JSON C bindings available])
	USE_JANSSON=yes
    ],)

PKG_CHECK_MODULES([URIPARSER], [liburiparser > 0.7.4],
   [
        AC_DEFINE(HAVE_URIPARSER, [], [uriparser library available])
	USE_URIPARSER=yes
   ],)

PKG_CHECK_MODULES([LWS], libwebsockets,
   [
        AC_DEFINE(HAVE_LWS, [], [libwebsockets library available])
	USE_LWS=yes
   ],)

PKG_CHECK_MODULES([SSL], libssl,
   [
        AC_DEFINE(HAVE_SSL, [], [libssl library available])
	USE_SSL=yes
   ],)


PKG_CHECK_MODULES([UUID], uuid,
   [
        AC_DEFINE(HAVE_UUID, [], [uuid library available])
	USE_UUID=yes
   ],)

PKG_CHECK_MODULES([AVAHI], [avahi-client > 0.6],
    [
        AC_DEFINE(HAVE_AVAHI, [], [avahi mDNS client library available])
	USE_AVAHI=yes
    ],)

AC_MSG_CHECKING(whether to enable NML-using parts)
AC_ARG_ENABLE(nml,
    [  --enable-nml      enable NML-using parts],
    [
	BUILD_NMLDEP=yes
        AC_MSG_RESULT([configuring to build NML-using parts])
    ],
    [
      AC_MSG_RESULT([not building non-essential NML-using parts])
      BUILD_NMLDEP=no
    ])

##############################################################################
# Subsection 2.3.2                                                           #
# Dependencies for mkwrapper                                                 #
# pyftpdlib                                                                  #
##############################################################################

AC_PYTHON_MODULE(pyftpdlib.authorizers, failed to import pyftpdlib.authorizers
 - python-pyftpdlib package missing or too old? )
AC_PYTHON_MODULE(pyftpdlib.handlers, failed to import pyftpdlib.handlers
 - python-pyftpdlib package missing or too old? )
AC_PYTHON_MODULE(pyftpdlib.servers, failed to import pyftpdlib.servers
 - python-pyftpdlib package missing or too old? )


##############################################################################
# Subsection 2.4                                                             #
# Kernel detection                                                           #
# Find all kernels on system and assess realtime capabilities                #
##############################################################################

check_kernel_source_configured() {
    # Check if a path is a configured kernel source
    local ksrc_dir=${1}
    if test -z "$ksrc_dir"; then
        AC_MSG_ERROR([check_kernel_source_configured() given empty ksrc_dir])
    fi

    AC_MSG_CHECKING([for configured kernel source in $ksrc_dir])
    # if .config and version.h both exist, then assume it's configured
    if test -f "$ksrc_dir/.config" -a \
	    \(  -f "$ksrc_dir/include/linux/version.h" -o \
	    -f "$ksrc_dir/include/generated/uapi/linux/version.h"  \); then
	AC_MSG_RESULT([found])
	return 0
    else
	AC_MSG_RESULT([not found])
	return 1
    fi
}

check_kernel_source_version() {
    # get kernel version from version.h; set variable KVER
    # assume check_source_configured() already done

    # Unfortunately, many distributions (redhat, mandrake) have
    # #defines inside version.h, so a simple cat|grep|cut test won't
    # work... But then again, RH & Mandrake kernels are notorious for
    # their use of patches that break the RT patching - Both RTAI and
    # RTLinux strongly recommend using a virgin source from
    # kernel.org.

    local ksrc_dir=${1}
    if test -z "$ksrc_dir"; then
        AC_MSG_ERROR([check_kernel_source_version() given empty ksrc_dir])
    fi

    # Get the kernel version from version.h
    AC_MSG_CHECKING([kernel version string in $ksrc_dir])

    if test -e $ksrc_dir/include/linux/utsrelease.h; then
	VERSION_FILE=$ksrc_dir/include/linux/utsrelease.h
    elif test -e $ksrc_dir/include/generated/utsrelease.h; then
	VERSION_FILE=$ksrc_dir/include/generated/utsrelease.h
    else
	VERSION_FILE=$ksrc_dir/include/linux/version.h
    fi
    KVER=`$CC -E -dM ${VERSION_FILE} | grep UTS | cut -d '"' -f 2`

    if test -z "$KVER"; then
	AC_MSG_RESULT([not found])
    else
	AC_MSG_RESULT([$KVER])
    fi
}

check_xenomai_kernel_source() {
    # check if kernel source is a xenomai kernel source
    # assume check_source_configured() already done

    local ksrc_dir=${1}
    if test -z "$ksrc_dir"; then
        AC_MSG_ERROR([check_xenomai_kernel_source() given empty ksrc_dir])
    fi

    # look for CONFIG_XENOMAI variable
    AC_MSG_CHECKING(if $ksrc_dir is a xenomai kernel)
    if test -f "$ksrc_dir/.config" && \
	    grep -q '^CONFIG_XENOMAI=' "$ksrc_dir/.config"; then
	AC_MSG_RESULT([yes])
	return 0
    else
	AC_MSG_RESULT([no])
	return 1
    fi

}

check_rtai_kernel_source() {
    # check if kernel source is an rtai kernel source
    # assume check_source_configured() already done

    local ksrc_dir=${1}
    if test -z "$ksrc_dir"; then
        AC_MSG_ERROR([check_rtai_kernel_source() given empty ksrc_dir])
    fi


    # look for rt_daemonize (older RTAI) or rtai_irq_handler (at least
    # RTAI 4.0) or rtai_syscall_hook (RTAI 4.1) function definition
    AC_MSG_CHECKING(if $ksrc_dir is an RTAI kernel)
    if test -f "$ksrc_dir/Module.symvers" && \
            grep -q '\(rt_daemonize\|rtai_irq_handler\|rtai_syscall_hook\)' \
            "$ksrc_dir/Module.symvers"; then
        AC_MSG_RESULT([yes])
        return 0
    else
        AC_MSG_RESULT([no])
        return 1
    fi

}

# xenomai kernel modules will be built for these kernels if
# xenomai-kernel threads are enabled
xenomai_kernels=''
# RTAI kernel modules will be built for these kernels if
# rtai-kernel threads are enabled
rtai_kernels=''
# shmdrv kernel modules will be built for these kernels if
# shmdrv and at least one userspace threads are enabled
shmdrv_kernels=''
#
# list of kernel sources seen, so we don't check the same ones repeatedly
seen_kernels=' '
# ...and a function to check and add new
kernel_seen() {
    if test "${seen_kernels/ $1 /}" = "$seen_kernels"; then
        # not seen
	seen_kernels="$seen_kernels $1 "
	return 1
    else
        # seen
	return 0
    fi
}

# check any xenomai sources specified on command line now
# if any found, only build xenomai modules for these kernels
AC_ARG_WITH(xenomai-kernel-sources,
    [AS_HELP_STRING([--with-xenomai-kernel-sources="<ksrc_dir> <ksrc_dir>..."],
	[directory paths of xenomai kernel source(s), space-separated])],
	[# check directories now
	    if test $with_xenomai_kernel_sources = no; then
	        # ignore this
		with_xenomai_kernel_sources=''
	    else
		for ksrc_dir in $with_xenomai_kernel_sources; do
		    ksrc_dir=$(readlink -f $ksrc_dir)
		    # add only configured, confirmed Xenomai sources
		    if check_kernel_source_configured $ksrc_dir && \
			check_xenomai_kernel_source $ksrc_dir; then
			shmdrv_kernels="shmdrv_kernels $ksrc_dir"
			xenomai_kernels="$xenomai_kernels $ksrc_dir"
			# note this one's been seen
			kernel_seen $ksrc_dir
		    else
			AC_MSG_ERROR([invalid xenomai kernel source: $ksrc_dir])
		    fi
		done
	    fi
	],
	[with_xenomai_kernel_sources=''])

# check any RTAI sources specified on command line now
# if any found, only build RTAI modules for these kernels
AC_ARG_WITH(rtai-kernel-sources,
    [AS_HELP_STRING([--with-rtai-kernel-sources="<ksrc_dir> <ksrc_dir>..."],
	[directory paths of RTAI kernel source(s), space-separated])],
	[# check directories now
	    if test $with_rtai_kernel_sources = no; then
	        # ignore this
		with_rtai_kernel_sources=''
	    else
		for ksrc_dir in $with_rtai_kernel_sources; do
		    ksrc_dir=$(readlink -f $ksrc_dir)
		    # add only configured, confirmed RTAI sources
		    if check_kernel_source_configured $ksrc_dir && \
			check_rtai_kernel_source $ksrc_dir; then
			shmdrv_kernels="$shmdrv_kernels $ksrc_dir"
			rtai_kernels="$rtai_kernels $ksrc_dir"
			# note this one's been seen
			kernel_seen $ksrc_dir
		    else
			AC_MSG_ERROR([invalid RTAI kernel source:  $ksrc_dir])
		    fi
		done
	    fi
	],
	[with_rtai_kernel_sources=''])

# check any shmdrv sources specified on command line now
# if any found, build shmdrv modules for these kernels
AC_ARG_WITH(shmdrv-kernel-sources,
    [AS_HELP_STRING([--with-shmdrv-kernel-sources="<ksrc_dir> <ksrc_dir>..."],
	[directory paths of kernel source(s) to build shmdrv module for])],
	[# check directories now
	    if test $with_shmdrv_kernel_sources = no; then
	        # ignore --without-shmdrv-kernel-sources
		with_shmdrv_kernel_sources=''
	    else
		for ksrc_dir in $with_shmdrv_kernel_sources; do
		    ksrc_dir=$(readlink -f $ksrc_dir)
		    # add only new, configured sources
		    if ! kernel_seen $ksrc_dir && \
			    check_kernel_source_configured $ksrc_dir; then
			shmdrv_kernels="$shmdrv_kernels $ksrc_dir"
		    else
			AC_MSG_ERROR([invalid kernel source:  $ksrc_dir])
		    fi
		done
	    fi
	],
	[with_shmdrv_kernel_sources=''])

AC_ARG_WITH(extra_kernel_sources,
	[AS_HELP_STRING(
	    [--with-extra-kernel-sources="<dir> <dir>..."],
	    [list of extra kernel source directories to search])],
	[KSOURCE_LOCATION_HINTS="$KSOURCE_LOCATION_HINTS
	    $with_extra_kernel_sources"])


find_kernels() {
    # find all kernel directories from hints if not told on cmdline
    # check kernel source is a valid, configured tree
    # add all valid trees to $shmdrv_kernels
    # add all valid xenomai and rtai trees to $xenomai_kernels and
    #     $rtai_kernels, respectively
    location_list=''
    for location in $KSOURCE_LOCATION_HINTS; do
        location=$(readlink -f $location)

	# be sure $location is a directory not already in the list
	test -d "$location" || continue
	test "${location_list/$location/}" = "${location_list}" || continue

	# if kernel source has been seen, ignore it
	kernel_seen $location && continue

	# if kernel source unconfigured, ignore it
        check_kernel_source_configured $location || continue

	# if --with-shmdrv-kernel-sources not explicitly on cmdline, add it
	test -z "$with_shmdrv_kernel_sources" && \
	    shmdrv_kernels="$shmdrv_kernels $location"
	# if --with-xenomai-kernel-sources not explicitly on cmdline
	# and xenomai kernel detected, add it
	test -z "$with_xenomai_kernel_sources" && \
	        check_xenomai_kernel_source $location && \
	    xenomai_kernels="$xenomai_kernels $location"
	# if --with-rtai-kernel-sources not explicitly on cmdline and
	# RTAI kernel detected, add it
	test -z "$with_rtai_kernel_sources" && \
	        check_rtai_kernel_source $location && \
	    rtai_kernels="$rtai_kernels $location"
    done
}


find_kernels


##############################################################################
# Subsection 2.5                                                             #
# Flavor configuration                                                       #
# - Check whether each flavor should be configured                           #
# - Sanity checks and configuration generation                               #
##############################################################################

# By default, build all flavors.  If a --with-<flavor> arg is found,
# do not build all by default.
all_flavors_default=yes

# Set to 'yes' when either xenomai or rt-preempt will be built
with_userland_rt_threads=no

AC_PRESERVE_HELP_ORDER()
AC_ARG_WITH(posix,
    [AS_HELP_STRING([--with-posix],
        [build POSIX (simulator) non-realtime threads modules
        (enabled by default)])],
    [test $withval = yes && all_flavors_default=no],
    [with_posix=check])

AC_ARG_WITH(rt-preempt,
    [AS_HELP_STRING([--with-rt-preempt],
        [build RT_PREEMPT threads modules])],
    [test $withval = yes && all_flavors_default=no],
    [with_rt_preempt=check])

AC_ARG_WITH(xenomai,
    [AS_HELP_STRING([--with-xenomai],
        [build Xenomai userland threads])],
    [xenomai_from_cmdline=$withval
     test $withval = yes && all_flavors_default=no],
    [with_xenomai=check])

AC_ARG_WITH(xenomai-kernel,
    [AS_HELP_STRING([--with-xenomai-kernel],
        [build Xenomai kernel-space realtime threads (deprecated)])],
    [xenomai_kernel_from_cmdline=$withval
     test $withval = yes && all_flavors_default=no],
    [with_xenomai_kernel=check])
# XENOMAI_THREADS_RTS:  locate the 'xeno-config' executable
AC_ARG_WITH(xeno-config,
    [AS_HELP_STRING([--with-xeno-config=<path>],
    [location of the xeno-config executable])],
    [xeno_config_from_cmdline=yes],
    [
	with_xenomai_config=""
    ])

AC_ARG_WITH(rtai-kernel,
    [AS_HELP_STRING([--with-rtai-kernel],
        [build RTAI kernel-space realtime threads modules])],
    [test $withval = yes && all_flavors_default=no],
    [with_rtai_kernel=check])
# RTAI_KERNEL_THREADS_RTS:  locate the 'rtai-config' executable
AC_ARG_WITH(rtai-config,
    [AS_HELP_STRING([--with-rtai-config=<path>],
    [location of the rtai-config executable])],
    [rtai_config_from_cmdline=yes],
    [
	with_rtai_config=""
    ])


# If a --with-<flavor> flag was found, disable all flavors not
# explicitly enabled
if test $all_flavors_default = no; then
    for f in posix rt_preempt xenomai xenomai_kernel rtai_kernel; do
        if eval "test \$with_$f = check"; then
	    eval "with_$f=no"
	fi
    done
fi


##############################################################################
# POSIX and RT_PREEMPT flavor checks
if test "$with_rt_preempt" != no -o "$with_posix" != no; then
    LIBS_hold=$LIBS
    LIBS="$LIBS -lrt"

    AC_MSG_CHECKING(-lrt for RT_PREEMPT and POSIX)
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
	#include <time.h>
	#include <sched.h>
	#include <sys/mman.h>
    ]],
    [[
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
    ]])],
    [
        have_librt=yes
        AC_MSG_RESULT(ok)],
    [
        have_librt=no
        AC_MSG_RESULT(failed)
    ])
    LIBS=$LIBS_hold
fi

AC_MSG_CHECKING(whether to build POSIX threads)
test "$with_posix" = check && with_posix=yes
test "$have_librt" = no && with_posix=no
AC_MSG_RESULT($with_posix)
if test "$with_posix" = yes; then
   POSIX_THREADS_LDFLAGS="-lpthread -lrt"
   POSIX_THREADS_RTFLAGS="$POSIX_THREADS_RTFLAGS -pthread"
   BUILD_THREAD_FLAVORS="$BUILD_THREAD_FLAVORS posix"
fi

AC_MSG_CHECKING(whether to build RT_PREEMPT threads)
test "$with_rt_preempt" = check && with_rt_preempt=yes
test "$have_librt" = no && with_rt_preempt=no
AC_MSG_RESULT($with_rt_preempt)
if test "$with_rt_preempt" = yes; then
    RT_PREEMPT_THREADS_LDFLAGS="-lpthread -lrt"
    RT_PREEMPT_THREADS_RTFLAGS="$RT_PREEMPT_THREADS_RTFLAGS -pthread"
    BUILD_THREAD_FLAVORS="$BUILD_THREAD_FLAVORS rt-preempt"
    with_userland_rt_threads=yes
fi


##############################################################################
# Xenomai flavor checks, both userland and kthreads

if test "$with_xenomai" != no -o "$with_xenomai_kernel" != no; then
    # XENOMAI_THREADS_RTS:  look for the xeno-config executable
    XENOMAI_THREADS_RTS=''
    if test "$xeno_config_from_cmdline" = yes; then
        # user-supplied path
        XENOMAI_THREADS_RTS="$with_xeno_config"
    else
	# guess the path
	AC_PATH_PROGS([XENOMAI_THREADS_RTS],[xeno-config],0,$PATH:/usr/xenomai/bin)
    fi
    # check xeno-config path
    if test -n "$XENOMAI_THREADS_RTS"; then
        # path found, so check its validity: add '--skin=native' to
        # the variable; then --ldflags should return something
        # containing the string '-lnative'
	AC_MSG_CHECKING([usability of xenomai utility, $XENOMAI_THREADS_RTS])
	if test -x $XENOMAI_THREADS_RTS; then
	    # Check for Xenomai v.2 or v.3
	    case $(xeno-config --version) in
	        2.*)
		    XENOMAI_SKIN=native
		    XENOMAI_V2=true
		    AC_MSG_RESULT(yes (v2))
		    ;;
		3.*)
		    XENOMAI_SKIN=alchemy
		    XENOMAI_V2=false
		    with_xenomai_kernel=no  # No more xenomai kthreads in v3
		    AC_MSG_RESULT(yes (v3))
		    ;;
	    esac
	    XENOMAI_THREADS_RTS="$XENOMAI_THREADS_RTS --skin=$XENOMAI_SKIN"
	else
	    XENOMAI_THREADS_RTS=''
	    AC_MSG_RESULT(no)
	fi
    fi
    # deal with nothing found in the right way
    if test -z "$XENOMAI_THREADS_RTS"; then
	if test "$xeno_config_from_cmdline" = yes; then
	    # --with-xeno-config specified, but invalid path
	    AC_MSG_ERROR(--with-xeno-config:  "$with_xeno_config":  bad path)
        elif test "$with_xenomai_kernel" = yes -o "$with_xenomai" = yes; then
	    # --with-xenomai{,-kernel} specified but can't find xeno-config
            AC_MSG_ERROR("xeno-config" not found; use --with-xeno-config")
	else
	    with_xenomai_kernel=no
	    with_xenomai=no
	fi
    fi
    if test -x "$XENOMAI_THREADS_RTS"; then
        # found xeno-config; use same one for kernel threads for consistency
	XENOMAI_KERNEL_THREADS_RTS="$XENOMAI_THREADS_RTS"
    fi
fi

if test -n "$XENOMAI_THREADS_RTS"; then
    # XENOMAI_THREADS_RTFLAGS:  test runtime library & get flags
    LIBS_hold="$LIBS"
    CFLAGS_hold="$CFLAGS"
    CPPFLAGS_hold="$CPPFLAGS"
    LIBS="$LIBS $($XENOMAI_THREADS_RTS --ldflags)"
    CFLAGS="$CFLAGS $($XENOMAI_THREADS_RTS --cflags)"
    CPPFLAGS="$CPPFLAGS $($XENOMAI_THREADS_RTS --cflags)"
    AC_CHECK_LIB($XENOMAI_SKIN, rt_task_create,,
        with_xenomai=no; with_xenomai_kernel=no)
    AC_CHECK_HEADER($XENOMAI_SKIN/task.h,,
        with_xenomai=no; with_xenomai_kernel=no)
    LIBS="$LIBS_hold"
    CFLAGS="$CFLAGS_hold"
    CPPFLAGS="$CPPFLAGS_hold"
fi

if test "$with_xenomai" != no -o "$with_xenomai_kernel" != no; then
    flags="$($XENOMAI_THREADS_RTS --cflags)"
    # Xenomai docs recommend turning off CONFIG_CC_STACKPROTECTOR
    # on all arches but x86_64; this causes missing symbols without
    # this CFLAG
    if test "$ARCHITECTURE" != x86_64; then
        flags="$flags -fno-stack-protector"
    fi
    XENOMAI_THREADS_RTFLAGS="$XENOMAI_THREADS_RTFLAGS $flags"
    XENOMAI_KERNEL_THREADS_RTFLAGS="$XENOMAI_KERNEL_THREADS_RTFLAGS -g $flags"
    # xenomai kernel math options
    flags="-ffast-math -mhard-float"
    if test "$($XENOMAI_THREADS_RTS --arch)" = x86; then
        flags="$flags -msse -funsafe-math-optimizations"
        # When using SSE instructions, not normally allowed in the
        # kernel, the stack must be aligned at 16 bytes.  Later
        # kernels (3.8.13, but not 3.5.7) try to set
        # -mpreferred-stack-boundary=3 on x86_64 arch, so force it.
	flags="$flags -mpreferred-stack-boundary=4"
    fi
    XENOMAI_KERNEL_THREADS_KERNEL_MATH_CFLAGS="$flags"
    # ldflags
    flags="$($XENOMAI_THREADS_RTS --ldflags)"
    XENOMAI_THREADS_LDFLAGS="$XENOMAI_THREADS_LDFLAGS $flags"
    XENOMAI_KERNEL_THREADS_LDFLAGS="$XENOMAI_KERNEL_THREADS_LDFLAGS $flags"
fi

AC_MSG_CHECKING(whether to build Xenomai userland threads)
test "$with_xenomai" = check && with_xenomai=yes
test -z "$XENOMAI_THREADS_RTS" && with_xenomai=no
if test $with_xenomai = yes; then
    BUILD_THREAD_FLAVORS="$BUILD_THREAD_FLAVORS xenomai"
    with_userland_rt_threads=yes
elif test "$xenomai_from_cmdline" = yes; then
    AC_MSG_ERROR([Requested '--with-xenomai', but unable to configure])
fi
AC_MSG_RESULT($with_xenomai)


AC_MSG_CHECKING(whether to build Xenomai kernel threads)
if test "$with_xenomai_kernel" = check; then
   # xenomai-kernel threads are broken in many ways on ARM arch; see
   # #63
   $TARGET_PLATFORM_PC && \
       with_xenomai_kernel=yes || \
       with_xenomai_kernel=no
fi
test -z "$xenomai_kernels" && with_xenomai_kernel=no
test -z "$XENOMAI_THREADS_RTS" && with_xenomai_kernel=no
if test $with_xenomai_kernel = yes; then
     BUILD_THREAD_FLAVORS="$BUILD_THREAD_FLAVORS xenomai-kernel"
elif test "$xenomai_kernel_from_cmdline" = yes; then
    AC_MSG_ERROR([Requested '--with-xenomai-kernel', but unable to configure])
fi
AC_MSG_RESULT($with_xenomai_kernel)


##############################################################################
# RTAI flavor checks

if test "$with_rtai_kernel" != no; then
    # look for rtai-config path
    RTAI_KERNEL_THREADS_RTS=''
    if test "$rtai_config_from_cmdline" = yes; then
        # user-supplied path
	RTAI_KERNEL_THREADS_RTS=$with_rtai_config
    else
        # guess path from defaults
	rtai_config_path=''
	dirs="$(echo \
	   /usr/realtime-`uname -r` /usr/realtime* /usr /usr/src/rtai*)"
	for d in $dirs; do
	    rtai_config_path="$rtai_config_path:$d:$d/bin"
	done
	AC_PATH_PROGS([RTAI_KERNEL_THREADS_RTS],[rtai-config],
	    [],[$rtai_config_path])
    fi
    # check rtai-config path
    if test -n "$RTAI_KERNEL_THREADS_RTS"; then
        # path found, so check its validity: --linux-dir should return
        # a configured kernel source directory with .config file
	AC_MSG_CHECKING([usability of RTAI utility, $RTAI_KERNEL_THREADS_RTS])
	if test -x $RTAI_KERNEL_THREADS_RTS -a \
	     -f $($RTAI_KERNEL_THREADS_RTS -o --linux-dir 2>/dev/null)/.config
	then
	    # ShabbyX-style `rtai-config`
	    RTAI_KERNEL_THREADS_RTS="$RTAI_KERNEL_THREADS_RTS -o"
	    AC_MSG_RESULT(yes)
	elif test -x $RTAI_KERNEL_THREADS_RTS -a \
	     -f $($RTAI_KERNEL_THREADS_RTS --linux-dir 2>/dev/null)/.config
	then
	    AC_MSG_RESULT(yes)
	else
	    RTAI_KERNEL_THREADS_RTS=''
	    AC_MSG_RESULT(no)
	fi
    fi
    # deal with nothing found in the right way
    if test -z "$RTAI_KERNEL_THREADS_RTS"; then
	if test "$rtai_config_from_cmdline" = yes; then
	    # --with-rtai-config specified, but invalid path
	    AC_MSG_ERROR(--with-rtai-config:  "$with_rtai_config":  bad path)
        elif test "$with_rtai_kernel" = yes; then
	    # --with-rtai-kernel specified but can't find rtai-config
            AC_MSG_ERROR("rtai-config" not found; use --with-rtai-config")
	else
	    with_rtai_kernel=no
	fi
    fi
fi

# Generate RTFLAGS for RTAI
if test $with_rtai_kernel != no -a -n "$rtai_kernels"; then

    RTAI_KERNEL_THREADS_RTFLAGS="$($RTAI_KERNEL_THREADS_RTS --module-cflags) \
	-I$($RTAI_KERNEL_THREADS_RTS --prefix)/include"
    RTAI_KERNEL_THREADS_LDFLAGS="$($RTAI_KERNEL_THREADS_RTS --lxrt-ldflags) \
        -llxrt -Wl,-rpath,$($RTAI_KERNEL_THREADS_RTS --library-dir)"

    if test "$host_cpu" = x86_64; then
	flags="-msse -mpreferred-stack-boundary=4 -funsafe-math-optimizations"
    else
        flags="-fno-unsafe-math-optimizations"
    fi
    RTAI_KERNEL_THREADS_KERNEL_MATH_CFLAGS="$flags"

    RTAI_KERNEL_THREADS_RTDIR=$(readlink -f \
	$($RTAI_KERNEL_THREADS_RTS --module-dir))
fi
AC_MSG_CHECKING(whether to build RTAI threads)
test -z "$RTAI_KERNEL_THREADS_RTS" && with_rtai_kernel=no
test -z "$rtai_kernels" && with_rtai_kernel=no

# Check the RTAI Module.symvers file
if test $with_rtai_kernel != no; then
    RTAI_KERNEL_THREADS_EXTRA_SYMBOLS=`$RTAI_KERNEL_THREADS_RTS \
	--module-dir`/Module.symvers
    if ! test -f "${RTAI_KERNEL_THREADS_EXTRA_SYMBOLS}"; then
	if test "$with_rtai_kernel" = check; then
	    with_rtai_kernel=no
	else
	    AC_MSG_ERROR([RTAI build requested but no Module.symvers found])
	fi
    fi
fi
test "$with_rtai_kernel" = check && with_rtai_kernel=yes
if test $with_rtai_kernel = yes; then
    BUILD_THREAD_FLAVORS="$BUILD_THREAD_FLAVORS rtai-kernel"
fi
AC_MSG_RESULT($with_rtai_kernel)
if test $with_rtai_kernel = yes; then
    AC_MSG_CHECKING(RTAI version)
    if grep -q rt_free_timers $RTAI_KERNEL_THREADS_EXTRA_SYMBOLS; then
        RTAI_VERSION=401
	AC_MSG_RESULT(4.1 or greater)
    else
        RTAI_VERSION=400
	AC_MSG_RESULT(4.0 or earlier)
    fi
fi

##############################################################################
# Subsection 2.6                                                             #
# Hardware driver detection                                                  #
# - Configure usermode PCI drivers                                           #
# - Configure libudev                                                        #
##############################################################################

AC_MSG_CHECKING(whether to build hardware drivers)
AC_ARG_ENABLE(drivers,
    [AS_HELP_STRING([--enable-drivers],[Build hardware drivers])],
    [with_drivers=$enableval],
    [with_drivers=check])
if test "$with_drivers" = check; then
    with_drivers=yes
fi
AC_MSG_RESULT($with_drivers)


AC_ARG_ENABLE(usermode-pci,
    [AS_HELP_STRING([--enable-usermode-pci],
	[Build PCI drivers with usermode PCI support])],
    [with_usermode_pci=$enableval],
    [with_usermode_pci=check])
# record what was requested on the command line
if test $with_usermode_pci = yes; then
    usermode_pci_requested_cmdline=yes
else
    usermode_pci_requested_cmdline=no
fi
if test $with_drivers = yes -a $with_usermode_pci != no; then
    usermode_pci_disabled_cmdline=no
else
    usermode_pci_disabled_cmdline=yes
fi

# test usability of libudev, needed by userland drivers
with_libudev=no
if test "$with_drivers" = "yes" -a $with_usermode_pci != no; then
   PKG_CHECK_MODULES(LIBUDEV, libudev, with_libudev=yes, with_libudev=no)
fi

AC_MSG_CHECKING(whether to build usermode PCI hardware drivers)
if test "$usermode_pci_disabled_cmdline" = yes; then
    # user explicitly disabled either userland drivers or all drivers
    with_usermode_pci=no
    AC_MSG_RESULT([disabled by user])
elif test "$with_usermode_pci" = check -a $with_userland_rt_threads = no; then
    with_usermode_pci=no
    AC_MSG_RESULT([default disabled for no userland RT threads])
elif test $with_libudev = no; then
    with_usermode_pci=no
    AC_MSG_RESULT([no; depends on libudev])
elif test "$with_usermode_pci" = check -a $with_userland_rt_threads = yes; then
    with_usermode_pci=yes
    AC_MSG_RESULT([default enabled for userland RT threads])
else
    # catchall
    AC_MSG_RESULT($with_usermode_pci)
fi
# if --with-usermode-pci was specified on the cmdline but is
# unavailable, consider it an error
test $usermode_pci_requested_cmdline = yes -a $with_usermode_pci = no && \
    AC_MSG_ERROR([Usermode PCI drivers requested but unavailable])
# if xenomai or rt-preempt will be built and userland drivers were
# not explicitly disabled but will not be built, issue a warning
test $with_usermode_pci = no \
        -a $usermode_pci_disabled_cmdline = no \
        -a $with_userland_rt_threads = yes && \
        $TARGET_PLATFORM_PC && \
    AC_MSG_WARN([Usermode PCI drivers will not be available])

##############################################################################
# Subsection 2.7                                                             #
# Pass configuration out of script                                           #
# - Substitution variables (Makefile.inc.in, realtime.in, etc.               #
# - config.h macro definitions                                               #
##############################################################################

# RTDIR:  FIXME:  needs maintenance (elimination)
# latencyplot only works with RTAI and needs RTDIR
AC_SUBST([RTDIR],$RTAI_KERNEL_THREADS_RTDIR)


if test "$with_drivers" = yes; then
   AC_DEFINE(BUILD_DRIVERS, [], [build hardware drivers])
fi
AC_SUBST(BUILD_DRIVERS,[$with_drivers])

if test "$USE_SHMDRV" = yes; then
   AC_DEFINE(USE_SHMDRV, [], [use shm driver module])
fi
AC_SUBST(USE_SHMDRV)
AC_SUBST(SHMDRV_KERNEL_DIRS,[$shmdrv_kernels])

AC_SUBST(BUILD_EXAMPLES)
if test "$BUILD_EXAMPLES" = yes; then
   AC_DEFINE(BUILD_EXAMPLES, [], [build programming examples])
fi

AC_SUBST(PROTOC)

AC_SUBST(BUILD_EMCWEB)
if test "$BUILD_EMCWEB" = yes; then
   AC_DEFINE(BUILD_EMCWEB, [], [build emcweb UI])
fi

AC_SUBST(BUILD_DEV)
AC_SUBST(BUILD_PROTO_JS)


AC_SUBST(BUILD_NMLDEP)
if test "$BUILD_NMLDEP" = yes; then
   AC_DEFINE(BUILD_NMLDEP, [], [build NML dependent parts])
fi

AC_SUBST([CZMQ_CFLAGS])
AC_SUBST([CZMQ_LIBS])

AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])

AC_SUBST([AVAHI_CFLAGS])
AC_SUBST([AVAHI_LIBS])

AC_SUBST([JANSSON_CFLAGS])
AC_SUBST([JANSSON_LIBS])

AC_SUBST([URIPARSER_CFLAGS])
AC_SUBST([URIPARSER_LIBS])

AC_SUBST([LWS_CFLAGS])
AC_SUBST([LWS_LIBS])

AC_SUBST([SSL_CFLAGS])
AC_SUBST([SSL_LIBS])

AC_SUBST([UUID_CFLAGS])
AC_SUBST([UUID_LIBS])

AC_SUBST([USE_CZMQ])
AC_SUBST([USE_PROTOBUF])
AC_SUBST([USE_JANSSON])
AC_SUBST([USE_LWS])
AC_SUBST([USE_URIPARSER])
AC_SUBST([USE_AVAHI])
AC_SUBST([USE_SSL])
AC_SUBST([USE_UUID])

if test "$with_usermode_pci" = yes; then
   AC_DEFINE([USERMODE_PCI], [], [build PCI drivers with usermode PCI support])
fi
AC_SUBST([USERMODE_PCI],[$with_usermode_pci])
AC_SUBST(LIBUDEV_CFLAGS)
AC_SUBST(LIBUDEV_LIBS)
AC_DEFINE(HAVE_LIBUDEV, [], [libudev installed and available])

if test "$USE_PORTABLE_PARPORT_IO" = yes; then
   AC_DEFINE(USE_PORTABLE_PARPORT_IO, [], [use ppdev ioctl instead of inb/outb])
fi
AC_SUBST(USE_PORTABLE_PARPORT_IO)

AC_SUBST(ARCHITECTURE)

# Platform
if $TARGET_PLATFORM_PC; then
   AC_DEFINE(TARGET_PLATFORM_PC, [], [build for PC platform])
   target_platform_pc_val=yes
fi
AC_SUBST(TARGET_PLATFORM_PC,$target_platform_pc_val)

if $TARGET_PLATFORM_BEAGLEBONE; then
   AC_DEFINE(TARGET_PLATFORM_BEAGLEBONE, [], [build for BeagleBone platform])
   target_platform_beaglebone_val=yes
fi
AC_SUBST(TARGET_PLATFORM_BEAGLEBONE,$target_platform_beaglebone_val)

if $TARGET_PLATFORM_RASPBERRY; then
   AC_DEFINE(TARGET_PLATFORM_RASPBERRY, [], [build for Raspberry Pi platform])
   target_platform_raspberry_val=yes
fi
AC_SUBST(TARGET_PLATFORM_RASPBERRY,$target_platform_raspberry_val)

if $TARGET_PLATFORM_ZEDBOARD; then
   AC_DEFINE(TARGET_PLATFORM_ZEDBOARD, [], [build for Zedboard platform])
   target_platform_zedboard_val=yes
fi
AC_SUBST(TARGET_PLATFORM_ZEDBOARD,$target_platform_zedboard_val)

############################################################
# thread-specific -DDEFINE and Makefile.inc variables

# List of threads flavors
AC_SUBST([BUILD_THREAD_FLAVORS])


# POSIX settings
if test "$with_posix" = yes; then
    AC_DEFINE([HAVE_POSIX_THREADS],[],[Build POSIX threads])
fi
AC_SUBST([HAVE_POSIX_THREADS],[$with_posix])
AC_SUBST([POSIX_THREADS_FLAVOR_ID],_cfg_flavor_id(posix))
AC_SUBST([POSIX_THREADS_BUILD_SYS],_cfg_flavor_build_sys(posix))
AC_SUBST([POSIX_THREADS_SOURCE],[rt-preempt])
AC_SUBST([POSIX_THREADS_RTFLAGS])
AC_SUBST([POSIX_THREADS_LDFLAGS])
AC_SUBST([POSIX_THREADS_RTDIR],[])
AC_SUBST([POSIX_THREADS_RTS],[])
AC_SUBST([POSIX_THREADS_EXTRA_KMODS],[])

# RT_PREEMPT settings
if test "$with_rt_preempt" = yes; then
    AC_DEFINE([HAVE_RT_PREEMPT_THREADS],[],[Build RT_PREEMPT threads])
fi
AC_SUBST([HAVE_RT_PREEMPT_THREADS],[$with_rt_preempt])
AC_SUBST([RT_PREEMPT_THREADS_FLAVOR_ID],_cfg_flavor_id(rt-preempt))
AC_SUBST([RT_PREEMPT_THREADS_BUILD_SYS],_cfg_flavor_build_sys(rt-preempt))
AC_SUBST([RT_PREEMPT_THREADS_SOURCE],[rt-preempt])
AC_SUBST([RT_PREEMPT_THREADS_RTFLAGS])
AC_SUBST([RT_PREEMPT_THREADS_LDFLAGS])
AC_SUBST([RT_PREEMPT_THREADS_RTDIR],[])
AC_SUBST([RT_PREEMPT_THREADS_RTS],[])
AC_SUBST([RT_PREEMPT_THREADS_EXTRA_KMODS],[])

# Xenomai general settings
if test "$XENOMAI_V2" = true; then
    AC_DEFINE([XENOMAI_V2],[],[Xenomai v.2])
fi
AC_DEFINE_UNQUOTED([XENOMAI_SKIN],$XENOMAI_SKIN,
	[Xenomai skin ('native' for v2, 'alchemy' for v3)])

# Xenomai userland settings
if test "$with_xenomai" = yes; then
    AC_DEFINE([HAVE_XENOMAI_THREADS],[],[Build Xenomai userland threads])
fi
AC_SUBST([HAVE_XENOMAI_THREADS],[$with_xenomai])
AC_SUBST([XENOMAI_THREADS_FLAVOR_ID],_cfg_flavor_id(xenomai))
AC_SUBST([XENOMAI_THREADS_BUILD_SYS],_cfg_flavor_build_sys(xenomai))
AC_SUBST([XENOMAI_THREADS_SOURCE],[xenomai])
AC_SUBST([XENOMAI_THREADS_RTFLAGS])
AC_SUBST([XENOMAI_THREADS_LDFLAGS])
AC_SUBST([XENOMAI_THREADS_RTS])
AC_SUBST([XENOMAI_THREADS_RTDIR],[])
AC_SUBST([XENOMAI_THREADS_EXTRA_KMODS],[])
AC_SUBST([PRECISE_MULTILIB_BUG],[$PRECISE_MULTILIB_BUG])

# Xenomai kernel settings
if test "$with_xenomai_kernel" = yes; then
    AC_DEFINE([HAVE_XENOMAI_KERNEL_THREADS],[],[Build Xenomai kernel threads])
fi
AC_SUBST([HAVE_XENOMAI_KERNEL_THREADS],[$with_xenomai_kernel])
AC_SUBST([XENOMAI_KERNEL_THREADS_FLAVOR_ID],_cfg_flavor_id(xenomai-kernel))
AC_SUBST([XENOMAI_KERNEL_THREADS_BUILD_SYS],
	_cfg_flavor_build_sys(xenomai-kernel))
AC_SUBST([XENOMAI_KERNEL_THREADS_SOURCE],[xenomai-kernel])
# force hard-float for kernel modules
AC_SUBST([XENOMAI_KERNEL_THREADS_RTFLAGS])
AC_SUBST([XENOMAI_KERNEL_THREADS_LDFLAGS])
AC_SUBST([XENOMAI_KERNEL_THREADS_RTS])
AC_SUBST([XENOMAI_KERNEL_THREADS_KERNEL_DIRS],[$xenomai_kernels])
AC_SUBST([XENOMAI_KERNEL_THREADS_EXTRA_SYMBOLS])
# in-kernel math options
AC_SUBST([XENOMAI_KERNEL_THREADS_KERNEL_MATH_CFLAGS])
AC_SUBST([XENOMAI_KERNEL_THREADS_RTDIR],[])
AC_SUBST([XENOMAI_KERNEL_THREADS_EXTRA_KMODS],
	"_cfg_flavor_extra_kmods(xenomai-kernel)")

# RTAI settings
if test "$with_rtai_kernel" = yes; then
    AC_DEFINE([HAVE_RTAI_KERNEL_THREADS],[],[Build RTAI threads])
fi
AC_SUBST([HAVE_RTAI_KERNEL_THREADS],[$with_rtai_kernel])
AC_SUBST([RTAI_KERNEL_THREADS_FLAVOR_ID],_cfg_flavor_id(rtai-kernel))
AC_SUBST([RTAI_KERNEL_THREADS_BUILD_SYS],_cfg_flavor_build_sys(rtai-kernel))
AC_SUBST([RTAI_KERNEL_THREADS_SOURCE],[rtai-kernel])
AC_SUBST([RTAI_KERNEL_THREADS_RTFLAGS])
AC_SUBST([RTAI_KERNEL_THREADS_LDFLAGS])
AC_SUBST([RTAI_KERNEL_THREADS_RTS])
AC_SUBST([RTAI_KERNEL_THREADS_KERNEL_DIRS],[$rtai_kernels])
AC_SUBST([RTAI_KERNEL_THREADS_EXTRA_SYMBOLS])
AC_SUBST([RTAI_KERNEL_THREADS_KERNEL_MATH_CFLAGS])
# absolute path of rtai-config rtdir with symlinks dereferenced
AC_SUBST([RTAI_KERNEL_THREADS_RTDIR],[$RTAI_KERNEL_THREADS_RTDIR])
AC_SUBST([RTAI_KERNEL_THREADS_EXTRA_KMODS],
	"_cfg_flavor_extra_kmods(rtai-kernel)")
if test -n "$RTAI_VERSION"; then
    AC_DEFINE_UNQUOTED([RTAI_VERSION],$RTAI_VERSION,"RTAI version")
fi


# while we are at it, drop a git scent mark
GIT_VERSION=unknown
GIT_CONFIG_SHA=`../scripts/get-git-sha`

gitcmd=`which git`
if test -n "$gitcmd"; then
   version=`(cd ..;scripts/get-version-from-git)` 2>/dev/null
   if test -n "$version"; then
      GIT_VERSION=$version
   fi
else
   GIT_VERSION='git not installed at configure time'
fi

AC_SUBST(GIT_VERSION)
AC_SUBST(GIT_CONFIG_SHA)


EMC2_HOME=`(cd .. ; pwd)`
AC_SUBST([EMC2_HOME])


##############################################################################
# Section 3                                                                  #
# additional checking, ensure gcc > 4.4                                      #
# Subsection 3.1 - checking for a CC (we must use the same one used for RT)  #
# Subsection 3.2 - define default dirs                                       #
# Subsection 3.3 - check for GTK                                             #
# Subsection 3.4 - check for utility programs needed to build and run EMC    #
# Subsection 3.5 - check for programs needed to build documentation          #
##############################################################################

### ensure suficient adherence to C11 standard to allow forward declaration
### of typedefs etc - >= gcc-4.6

GCC_VERSION=$(gcc -dumpversion)
MAJ_VERSION=4
MIN_VERSION=6
MAJOR_VERSION=${GCC_VERSION:0:1}
MINOR_VERSION=${GCC_VERSION:2:1}

if (($MAJOR_VERSION == $MAJ_VERSION))
then
    if (($MINOR_VERSION < $MIN_VERSION))
    then
        AC_MSG_ERROR([gcc version = $GCC_VERSION, requires 4.6 or higher])
    fi
fi


##############################################################################
# Subsection 3.1 - checking for a CC (we must use the same one used for RT)  #
# Check CC version - (P.C.) We need to verify the compiler is the same one   #
# used for the realtime extensions... Hence the need to find the RT signature#
# first.                                                                     #
# $RTS --cc should return the C compiler used - Verify this is acceptable.   #
# if version is gcc-2.96 check for alternative                               #
##############################################################################

AC_MSG_CHECKING([for usability of rdtscll from asm/msr.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <asm/msr.h>
#include <stdio.h>
void use_rdtscll(void) { long long tsc; rdtscll(tsc); }
]], [[]])],[
        MSR_H_USABLE=yes
        AC_DEFINE([MSR_H_USABLE], [], [Define to 1 if asm/msr.h is usable and defines rdtscll])
    ],[MSR_H_USABLE=no])
AC_MSG_RESULT($MSR_H_USABLE)

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
# check for libmodbus3
#

AC_ARG_WITH(
    [libmodbus],
    [  --with-libmodbus                        Specify whether or not to build
                                          the drivers that use libmodbus
                                          (defaults to "yes")],
    [WITH_LIBMODBUS=$withval],
    [WITH_LIBMODBUS=check]
)

AS_IF(
    [test $WITH_LIBMODBUS = yes -o $WITH_LIBMODBUS = check ],
    [
	PKG_CHECK_MODULES(
	    LIBMODBUS,
	    libmodbus >= 3,
	    [ AC_SUBST(HAVE_LIBMODBUS3, yes) ],
	    [ AC_MSG_ERROR([libmodbus3 requested but not found]) ])
    ]
)


#
# check for libusb-1.0
#

AC_ARG_WITH(
    [libusb-1.0],
    [  --with-libusb-1.0                       Specify whether or not to build
                                          the drivers that use libusb-1.0
                                          (defaults to "yes")],
    [WITH_LIBUSB10=$withval],
    [WITH_LIBUSB10=yes]
)

AS_IF(
    [test "x$WITH_LIBUSB10" = "xyes"],
    [
	PKG_CHECK_MODULES(
	    LIBUSB10,
	    libusb-1.0,
	    [ AC_SUBST(HAVE_LIBUSB10, yes) ],
	    [ AC_MSG_ERROR([not found]) ])
    ]
)

#
# check for LTTNG
# the lttng-ust package should be enough for now
#
AC_ARG_WITH(
    [lttng],
    [  --with-lttng                       Specify whether or not to use
                                          the LTTNG tracing framework
                                          (defaults to "no")],
    [WITH_LTTNG=$withval],
    [WITH_LTTNG=no]
)

AS_IF(
    [test "x$WITH_LTTNG" = "xyes"],
    [
	PKG_CHECK_MODULES(LTTNG_UST, lttng-ust,
	    [
	      HAVE_LTTNG_UST=yes
	      AC_DEFINE([HAVE_LTTNG_UST], [], [if lttng-ust support is available])
	    ],
	    [ AC_MSG_ERROR([package lttng-ust not found]) ]
	    )
    ]
)
AC_SUBST(HAVE_LTTNG_UST)

##############################################################################
# Subsection 3.2 - define default dirs, these get used in a lot of places    #
# we decide based on RIP or install where stuff goes                         #
##############################################################################

if test "$RUN_IN_PLACE" = "yes"; then
    prefix=${BUILD_TOPLEVEL}
    DEFAULT_NMLFILE=${prefix}/configs/common/linuxcnc.nml
    DEFAULT_TOOLTABLE=${prefix}/configs/sim/sim.tbl
else
    RUN_IN_PLACE=no
    EMC2_HOME=$prefix
    DEFAULT_NMLFILE=${prefix}/share/linuxcnc/linuxcnc.nml
fi

####################################
# create dirs and links for re-brand
####################################

export USERPREFIX=$prefix

./rebrand

###################################

AC_SUBST([RUN_IN_PLACE])
AC_SUBST([DEFAULT_NMLFILE])
AC_DEFINE_UNQUOTED(
	[EMC2_DEFAULT_NMLFILE], "$DEFAULT_NMLFILE", [Default nml file])
AC_DEFINE_UNQUOTED(
	[EMC2_DEFAULT_TOOLTABLE], "$DEFAULT_TOOLTABLE", [Default tooltable file])

fe () {
    # Fully expand argument.  Example:
    # ${datadir} -> ${datarootdir} -> ${prefix}/share -> /usr/local
    case "$1" in
    *\$*) eval fe \""$1"\" ;;
    *) echo "$1" ;;
    esac
}

EMC2_SYSTEM_CONFIG_DIR=${sysconfdir}/linuxcnc

if test "xyes" = "x$RUN_IN_PLACE"; then
    EMC2_LATENCY_SCRIPT=$EMC2_HOME/scripts/latency-test
    EMC2_SCRIPT=$EMC2_HOME/scripts/emc
    EMC2_SUFFIX=" (run-in-place)"
    EMC2_ICON=$EMC2_HOME/machinekiticon.png
    EMC2_BIN_DIR=$EMC2_HOME/bin
    EMC2_LIBEXEC_DIR=$EMC2_HOME/libexec
    EMC2_TCL_DIR=$EMC2_HOME/tcl
    EMC2_TCL_LIB_DIR=$EMC2_HOME/tcl
    EMC2_LANG_DIR=$EMC2_HOME/src/objects
    EMC2_PO_DIR=$EMC2_HOME/share/locale
    EMC2_HELP_DIR=$EMC2_HOME/help
    EMC2_RTLIB_DIR=$EMC2_HOME/rtlib
    LINUXCNC_CONFIG_PATH="~/machinekit/configs:$EMC2_HOME/configs"
    EMC2_NCFILES_DIR=$EMC2_HOME/nc_files
    REALTIME=$EMC2_HOME/scripts/realtime
    EMC2_IMAGE_DIR=$EMC2_HOME
else
    EMC2_HOME=${prefix}
    EMC2_BIN_DIR=${prefix}/bin
    EMC2_LIBEXEC_DIR=${libexecdir}/linuxcnc
    EMC2_LATENCY_SCRIPT=$EMC2_BIN_DIR/latency-test
    EMC2_SCRIPT=$EMC2_BIN_DIR/emc
    EMC2_SUFFIX=""
    EMC2_ICON=machinekiticon
    EMC2_TCL_DIR=${prefix}/lib/tcltk/linuxcnc
    EMC2_TCL_LIB_DIR=${prefix}/lib/tcltk/linuxcnc
    EMC2_LANG_DIR=${prefix}/share/linuxcnc/tcl/msgs
    EMC2_PO_DIR=${prefix}/share/locale
    EMC2_HELP_DIR=${prefix}/share/doc/linuxcnc
    EMC2_RTLIB_DIR=${prefix}/lib/linuxcnc
    LINUXCNC_CONFIG_PATH="~/machinekit/configs:${sysconfdir}/linuxcnc/configs:$(fe "$datadir")/linuxcnc/examples/sample-configs"
    EMC2_NCFILES_DIR=${prefix}/share/linuxcnc/ncfiles
    REALTIME=${bindir}/realtime
    EMC2_IMAGE_DIR=$(fe "$datadir")/linuxcnc
fi

# Paths in config.h should be reduced to one:  EMC2_SYSTEM_CONFIG_DIR.
# This should contain the .ini file that points to everything else.

# do this here since substituted into files
# needed below after substitution
MACHINEKIT_INI="$EMC2_SYSTEM_CONFIG_DIR/machinekit.ini"
AC_SUBST([MACHINEKIT_INI])

AC_DEFINE_UNQUOTED([EMC2_HOME], "$EMC2_HOME", [Prefix or RIP directory])
AC_DEFINE_UNQUOTED([MACHINEKIT_INI], "$MACHINEKIT_INI", [path of global machinekit.ini file])
AC_DEFINE_UNQUOTED([EMC2_BIN_DIR], "$EMC2_BIN_DIR", [Directory for binaries])
AC_DEFINE_UNQUOTED([EMC2_RTLIB_DIR], "$EMC2_RTLIB_DIR", [Directory for ulapi shared objects])
AC_DEFINE_UNQUOTED([EMC2_PO_DIR], "$EMC2_PO_DIR", [Directory for po/mo translation files])
AC_DEFINE_UNQUOTED([EMC2_NCFILES_DIR], "$EMC2_NCFILES_DIR", [Directory for nc files])
AC_DEFINE_UNQUOTED([EMC2_IMAGE_DIR], "$EMC2_IMAGE_DIR", [Directory for images])
AC_DEFINE_UNQUOTED([GIT_VERSION], "$GIT_VERSION", [Git version tag])
AC_DEFINE_UNQUOTED([GIT_CONFIG_SHA], "$GIT_CONFIG_SHA", [SHA valid when configure was run])

AC_SUBST([EMC2_SYSTEM_CONFIG_DIR])
AC_SUBST([EMC2_BIN_DIR])
AC_SUBST([EMC2_LIBEXEC_DIR])
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
AC_SUBST([EMC2_LATENCYPLOT_SCRIPT])
AC_SUBST([EMC2_SUFFIX])
AC_SUBST([EMC2_ICON])


##############################################################################
# Subsection 3.3 - check for GTK                                             #
# FIXME: allow it to be enabled or disabled command line                     #
##############################################################################

PKG_CHECK_MODULES(GLIB, glib-2.0)

AC_ARG_ENABLE(gtk,
    [  --disable-gtk        Disable the parts of Machinekit that depend on GTK],
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
    PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.4.0,  [GTK_VER=`pkg-config gtk+-2.0 --modversion`],
        [ AC_MSG_ERROR([no gtk+-2.0 found; use --disable-gtk])])
    PKG_CHECK_MODULES(GNOMEPRINT, libgnomeprintui-2.2,
        [
	    GTK_CFLAGS="$GTK_CFLAGS $GNOMEPRINT_CFLAGS"
	    GTK_LIBS="$GTK_LIBS $GNOMEPRINT_LIBS"
	    HAVE_GNOMEPRINT=yes
	], :)
fi

AC_SUBST(HAVE_GNOMEPRINT)
AC_SUBST(GTK_VER)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

##############################################################################
# Subsection 3.4 - check for utility programs needed to build and run EMC    #
#                                                                            #
# Check for all the utility programs we need to build and install emc        #
# as well as those we use in the emc scripts...                              #
##############################################################################

SPATH=$PATH:/usr/local/sbin:/usr/sbin:/sbin
AC_PROG_CXX

# make this optional - it will likely only result in warnings
AX_CXX_COMPILE_STDCXX_11(noext,optional)

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

# MANDB empty is handled in doc/Submakefile
AC_PATH_PROG(MANDB, mandb, "")

AC_ARG_WITH(rmmod,
    [  --with-rmmod=</path/rmmod>              rmmod variant],
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
    [  --with-insmod=</path/insmod>            insmod variant],
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

##############################################################################
# Subsection 3.5 - check for programs needed to build documentation          #
#                                                                            #
# Check for programs we need to build and install docs.                      #
# (Optional, if not present, just don't build the docs.)                     #
##############################################################################

AC_ARG_WITH(python,
    [  --with-python=<path>                    Specify the Python interpreter],
    [
        PYTHON=$withval
    ],[
        PYTHON=python
    ])
AC_PATH_PROG(PYTHON,$PYTHON,"none")
if test $PYTHON = "none"
then
    AC_MSG_ERROR([python not found])
fi

AC_MSG_CHECKING([whether to build documentation])
AC_ARG_ENABLE(build-documentation,
    [  --enable-build-documentation[=format]   Build documentation.
                          Format may be "pdf" or "html" if only one format is desired.],
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
    if ( test "none" = "$PYTHON" ) ; then
        AC_MSG_ERROR([no python, documentation cannot be built])
    fi

    AC_PATH_PROG(ASCIIDOC,asciidoc,"none")
    if ( test "none" = "$ASCIIDOC" ) ; then
	AC_MSG_ERROR([no AsciiDoc, documentation cannot be built])
    fi

    AC_PATH_PROG(A2X,a2x,"none")
    if ( test "none" = "$A2X" ) ; then
	AC_MSG_ERROR([no a2x, documentation cannot be built])
    fi

    AC_MSG_CHECKING([asciidoc version])
    set -- `asciidoc --version`; ASCIIDOC_VER=$2
    set -- `echo $ASCIIDOC_VER | sed 's/\./ /g'`

    if test $1 -lt 8 -o \( $1 -eq 8 -a $2 -lt 5 \); then
        AC_MSG_ERROR([asciidoc version $ASCIIDOC_VER less than 8.5.
Documentation cannot be built.
On Lucid, install the regular asciidoc from Ubuntu by running "sudo apt-get install asciidoc".
On Hardy, install the backported asciidoc available from the linuxcnc.org debian archive.
On other systems, do what you need to install asciidoc version 8.5 or newer.])
    fi
    AC_MSG_RESULT([$ASCIIDOC_VER])

    AC_MSG_CHECKING([whether to specify latex.encoding])
    temp_asciidoc=`mktemp --suffix=.asciidoc`
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
    rm -f $temp_asciidoc ${temp_asciidoc%.asciidoc}.pdf

    AC_PATH_PROG(DBLATEX,dblatex,"none")
    if ( test "none" = "$DBLATEX" ) ; then
	AC_MSG_ERROR([no dblatex, documentation cannot be built])
    fi

    AC_MSG_CHECKING([dblatex version])
    set -- `dblatex --version`; DBLATEX_VER=$3
    set -- `echo $DBLATEX_VER | sed 's/[[.-]]/ /g'`

    if test $1 -eq 0 -a \( $2 -lt 2 -o \( $2 -eq 2 -a ${3:-0} -lt 12 \) \); then
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
    AC_PATH_PROG(DBLATEX,dblatex,"none")
    if ( test "none" = "$DBLATEX" ) ; then
	AC_MSG_ERROR([no dblatex, PDF documentation cannot be built])
    fi

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
fi
AC_SUBST(BUILD_DOCS)
AC_SUBST(BUILD_DOCS_PDF)
AC_SUBST(BUILD_DOCS_HTML)

##############################################################################
# Section 4 - Important headers, functions and gloabl defines.               #
#                                                                            #
##############################################################################

# This section gets inserted in to config.h.in when autoheader is run.
# Global and common defines should be here rather than duplicated in
# multiple places. Keep screwball constructs out of this, and it can
# be used in both kernel space and user space.

dnl _cfg_thread_flavor_macros():  define thread flavor IDs
dnl   and set RTAPI_<FLAVOR>, BUILD_SYS_<BUILDSYS> & THREADS_HEADERS macros
AC_DEFUN([_cfg_thread_flavor_macros],[dnl
m4_foreach([_cfg_flavor],[_cfg_all_flavors],[dnl
[#]define RTAPI_[]_cfg_flavor_upcase(_cfg_flavor)[]_ID dnl
_cfg_flavor_id(_cfg_flavor)
[#]define RTAPI_[]_cfg_flavor_upcase(_cfg_flavor)[]_NAME dnl
"_cfg_flavor"
])
#if ! defined(THREAD_FLAVOR_ID)
// this isn't allowed when building RTAPI
#ifdef RTAPI
#error RTAPI defined but THREAD_FLAVOR_ID not defined
#endif
m4_foreach([_cfg_flavor],[_cfg_all_flavors],[dnl
[#]elif THREAD_FLAVOR_ID == RTAPI_[]_cfg_flavor_upcase(_cfg_flavor)_ID
[#]  define RTAPI_[]_cfg_flavor_upcase(_cfg_flavor)
[#]  define THREAD_FLAVOR_NAME "_cfg_flavor"
[#]  define _cfg_flavor_BUILD_SYS(_cfg_flavor)
[#]  define THREADS_HEADERS "_cfg_flavor_threads_headers(_cfg_flavor)"
])dnl
#else
#error unknown value for THREAD_FLAVOR_ID
#endif
])


AH_TOP([/********************************************************************
* Description: config.h
*
*	Common defines used in many emc2 source files.
*       To add to this file, edit Section 4 in configure.ac
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

/* LINELEN is used throughout for buffer sizes, length of file name strings,
   etc. Let's just have one instead of a multitude of defines all the same. */
#define LINELEN 255
/* Used in a number of places for sprintf() buffers. */
#define BUFFERLEN 80

#define MM_PER_INCH 25.4
#define INCH_PER_MM (1.0/25.4)

/* RTAPI array sizes */
#define RTAPI_MAX_MODULES	64
#define RTAPI_MAX_TASKS		64
#define RTAPI_MAX_SHMEMS	32

#define MAX_INSTANCES           16

// default size of the HAL data segment
// can be overridden by the rtapi.so/ko module param hal_size=<n>
#define HAL_SIZE                262000

// minimum memlock limit
// see http://wiki.linuxcnc.org/cgi-bin/wiki.pl?LockedMemory
// edit /etc/security/limits.conf to read:
//  *       -    memlock 20480
#define RLIMIT_MEMLOCK_RECOMMENDED (20480*1024lu)

// default size of the thread stack size passed to rtapi_task_new()
// in hal_create_thread()
// can be overridden by an option to rtapi_msgd
#define HAL_STACKSIZE           32768

// a pseudo-flavor id to indicate ulapi<flavor>.so is not yet loaded
// not a build target, just a state indicator for ulapi_autoload.c
#define RTAPI_NOTLOADED_NAME  "not-yet-loaded"
#define RTAPI_NOTLOADED_ID    99

// format of zeroMQ IPC socket pathnames
// pass: RUNDIR, rtapi_instance, service name, uuid
// service name is: log, rtapi, halrcomp etc
#define ZMQIPC_FORMAT "ipc://%s/%d.%s.%s"

]
// flavor IDs for the various threads systems
// and conditional to set thread system and build_sys macros
// alter these in configure.ac Subsection 2.0
dnl this needs to be unquoted
_cfg_thread_flavor_macros
)

AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
])

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_FUNCS(semtimedop)
LIBS_hold=$LIBS
LIBS="$LIBS -lrt"

AC_CHECK_FUNCS(clock_getres)


AC_MSG_CHECKING([for sincos function])
sincos_ok=yes
AC_LINK_IFELSE([AC_LANG_PROGRAM([[double a, b;]], [[sincos(1.0, &a, &b);]])],[AC_DEFINE(
        HAVE_SINCOS,
        1,
        Define to 1 if sincos is available.
    )],[sincos_ok=no
])
AC_MSG_RESULT([$sincos_ok])

AC_CHECK_FUNCS(__sincos)
LIBS=$LIBS_hold

AH_BOTTOM(#endif
)

##############################################################################
# Section 5 - Generate rtapi.ini.sh.in                                       #
#                                                                            #
# Add per-flavor sections to the rtapi.ini generator                         #
##############################################################################

AC_MSG_NOTICE([Building rtapi.ini generator script])
cp ../scripts/gen-rtapi.ini.sh.in.tmpl ../scripts/gen-rtapi.ini.sh.in

cat >> ../scripts/gen-rtapi.ini.sh.in <<EOFOUTER
cat <<EOF
m4_foreach([_cfg_flavor],[_cfg_all_flavors],[dnl


#################################################
# _cfg_flavor flavor section
#
@<:@flavor_[]_cfg_flavor@:>@
# These values do not normally need to be changed.
rtapi_app=@EMC2_LIBEXEC_DIR@/rtapi_app_[]_cfg_flavor
RTS=@_cfg_flavor_upcase(_cfg_flavor)_THREADS_RTS@
RTDIR=@_cfg_flavor_upcase(_cfg_flavor)_THREADS_RTDIR@
MODULES=@_cfg_flavor_upcase(_cfg_flavor)_THREADS_EXTRA_KMODS@
])
EOF
EOFOUTER

##############################################################################
# Section 6 - Check for tcl/tk support                                       #
#                                                                            #
# Test for tcl/tk headers fails if they are not in /usr/include              #
# so testing is done by finding tclConfig.sh and tkConfig.sh                 #
# if the search fails the traditional check is done                          #
##############################################################################
SC_PATH_TCLCONFIG
SC_LOAD_TCLCONFIG
TCL_CFLAGS="$TCL_INCLUDE_SPEC -DUSE_TCL_STUBS"
TCL_LIBS="$TCL_STUB_LIB_SPEC"

SC_PATH_TKCONFIG
SC_LOAD_TKCONFIG
TK_CFLAGS="$TK_INCLUDE_SPEC $TK_XINCLUDES"
TK_LIBS="$TK_LIBS $TK_LIB_SPEC"

AC_PATH_PROGS(WISH,[wish${TCL_VERSION} wish], [wish],
     [path="$TCL_EXEC_PREFIX/bin:$PATH"])
AC_PATH_PROGS(TCLSH,[tclsh${TCL_VERSION} tclsh], [tclsh],
     [path="$TCL_EXEC_PREFIX/bin:$PATH"])

HAVE_WORKING_BLT=no
AC_MSG_CHECKING([for BLT using tclsh8.5])
if (test -f $TCL_EXEC_PREFIX/bin/tclsh8.5) && (echo ['if [catch {package require BLT}] { exit 1; } else { exit 0; }'] | tclsh8.5); then
    AC_MSG_RESULT([found])
    HAVE_WORKING_BLT=yes
else
    AC_MSG_RESULT([not found])
fi

AC_SUBST([TCL_DBGX])
AC_SUBST([TK_DBGX])
AC_SUBST([TCL_CFLAGS])
AC_SUBST([TK_CFLAGS])
AC_SUBST([TCL_LIBS])
AC_SUBST([TK_LIBS])
AC_SUBST([WISH])
AC_SUBST([TCLSH])
AC_SUBST([HAVE_WORKING_BLT])

##############################################################################
# Section 7 - Miscelaneous support                                           #
#                                                                            #
# Subsection 7.1. - provides DATE to output it to the generated files        #
# Subsection 7.2. - user option to convince make to be quiet during make     #
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

# check for Xaw, that is needed for emc2/src/emc/usr_intf/xemc.cc

if test "$have_x" = "yes"; then
CFLAGS_hold=$CFLAGS
LIBS_hold=$LIBS
CFLAGS="$CFLAGS $X_CFLAGS"
LIBS="$LIBS $X_PRE_LIBS $X_LIBS"

AC_CHECK_HEADERS(X11/Xaw/XawInit.h,
	    [HAVE_XAW=yes],
	    [HAVE_XAW=no],
	    [[#include <X11/Intrinsic.h>
	    ]])

AC_CHECK_LIB(Xaw, XawDialogGetValueString,
	    [],
	    [HAVE_XAW=no])
CFLAGS=$CFLAGS_hold
LIBS=$LIBS_hold
fi

if test "x$HAVE_XAW" = "xno"; then
    AC_MSG_WARN([Xaw lib missing, you won't be able to build/run xemc. try installing it with 'apt-get install libxaw7-dev'])
else
    XAW_LIBS="$X_PRE_LIBS $X_LIBS -lX11 -lXaw -lXt"
fi
AC_SUBST([HAVE_XAW])
AC_SUBST([XAW_LIBS])

#clean out LIBS, or else all future tests will check with -lXaw (which fails on odd systems)
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

if test "x$HAVE_READLINE" = "xyes"; then
    AC_DEFINE([HAVE_READLINE], [], [Define to 1 if you have the 'readline' library (-lreadline) and required headers])
else
    AC_MSG_ERROR([Required library 'readline' missing.])
fi
AC_SUBST([HAVE_READLINE])
AC_SUBST([READLINE_LIBS])


#check for ncurses.h needed for emc2/src/emc/usr_intf/keystick.cc

AC_CHECK_HEADERS(ncurses.h,
	    [HAVE_NCURSES=yes],
	    [HAVE_NCURSES=no])

AC_CHECK_LIB(ncurses, initscr,
	    [],
	    [HAVE_NCURSES=no])

if test "x$HAVE_NCURSES" = "xno"; then
    AC_MSG_WARN(ncurses lib missing, you won't be able to build/run keystick. try installing it with 'apt-get install libncurses-dev')
else
    NCURSES_LIBS="-lncurses"
fi
AC_SUBST([HAVE_NCURSES])
AC_SUBST([NCURSES_LIBS])

# libbacktrace library:
# with gcc 4.8 and later, libbacktrace is bundled with libgcc-dev-<gcc-version>
# the includes are just "backtrace.h" etc and -lbacktrace is used for linking

# for earlier gcc versions, libbacktrace can be built from
# https://github.com/mhaberler/libbacktrace, in which case the includes are named
# libbacktrace/backtrace.h etc, typically installed in /usr/local

# bundled version
AC_CHECK_HEADERS(backtrace.h)
# custom installed version
AC_CHECK_HEADERS(libbacktrace/backtrace.h)

# the library proper - if found, use -lbacktrace which should work for
# bundled and /usr/local/lib installs:
LIBBACKTRACE=""
AC_CHECK_LIB(backtrace, backtrace_simple,
	[
	    AC_DEFINE(HAVE_LIBBACKTRACE, [], [libbacktrace available])
	    LIBBACKTRACE="-lbacktrace"
	],[])

AC_SUBST([LIBBACKTRACE])

##############################################################################
# Section 8 - Language support                                               #
#                                                                            #
# i18n Language support:                                                     #
# see http://cvs.gnome.org/viewcvs/intltool/doc/I18N-HOWTO?rev=1.3           #
##############################################################################

# if you don't have nls tools, allow a way out!

AC_ARG_ENABLE(nls,
              [  --disable-nls           don't use NLS],
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
  [  --with-locale-dir=DIR   Location of the locale file(s)
                          [DATADIR/locale]],[
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
AC_PATH_PROG(MSGFMT,msgfmt,none)
if test "$MSGFMT" = none -o "$XGETTEXT" = none; then
   AC_MSG_ERROR(install gettext)
fi
AC_SUBST(XGETTEXT)
AC_SUBST(MSGFMT)

AC_MSG_CHECKING(for Python support)
AC_ARG_ENABLE(python,
    [  --disable-python        Disable the parts of Machinekit that depend on Python],
    [
	case "$enableval" in
	Y*|y*)
	    BUILD_PYTHON=yes ;;
	*)
	    BUILD_PYTHON=no ;;
	esac
    ],
    [BUILD_PYTHON=yes])

AC_MSG_RESULT($BUILD_PYTHON)

if test "$BUILD_PYTHON" = "yes"; then
    if test "$PYTHON" = "none"; then
	AC_MSG_ERROR([Python missing.  Install it or specify --disable-python to skip the parts of Machinekit that depend on Python])
    fi

    AC_MSG_CHECKING([python version])
    if ! $PYTHON -c 'import sys;raise SystemExit,sys.hexversion<0x2040000'
    then
	AC_MSG_ERROR(Python version too old (2.4 or newer required))
    fi
    AC_MSG_RESULT(OK)

    AC_MSG_CHECKING([version of python libraries])
    LIBPYTHON=`$PYTHON -c 'import distutils.sysconfig; print "python" + distutils.sysconfig.get_config_vars().get("VERSION")'`
    AC_MSG_RESULT($LIBPYTHON)

    AC_MSG_CHECKING([match between tk and Tkinter versions])

    PYTHON_TK_VERSION="`$PYTHON -c 'import _tkinter; print _tkinter.TK_VERSION'`"
    PYTHON_TCL_VERSION="`$PYTHON -c 'import _tkinter; print _tkinter.TCL_VERSION'`"

    if test "$PYTHON_TCL_VERSION" != "$TCL_VERSION"; then
	AC_MSG_RESULT([TCL mismatch: $TCL_VERSION vs $PYTHON_TCL_VERSION])
	AC_MSG_ERROR([Python requires use of Tcl $PYTHON_TCL_VERSION and Tk $PYTHON_TK_VERSION.
Install this version and specify --with-tcl and --with-tk if necessary])
    fi
    if test "$PYTHON_TK_VERSION" != "$TK_VERSION"; then
	AC_MSG_RESULT([Tk mismatch: $TK_VERSION vs $PYTHON_TK_VERSION])
	AC_MSG_ERROR(["Python requires use of Tk $TK_VERSION.  Install this version and specify --with-tk if necessary"])
    fi
    AC_MSG_RESULT([$PYTHON_TK_VERSION])

    AC_MSG_CHECKING(location of Python header files)
    INCLUDEPY=`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars().get("INCLUDEPY")'`
    AC_MSG_RESULT($INCLUDEPY)

    AC_MSG_CHECKING([for Python headers])
    AS_IF([test "x$PYTHON_CPPFLAGS" = x],
    [
	_PYTHON_SYSCONFIG([[get_python_inc()]], [PYTHON_CPPFLAGS="-I$py_val"])
  	_PYTHON_SYSCONFIG([[get_python_inc(True)]],
  	[test "x$PYTHON_CPPFLAGS" = "x-I$py_val" || PYTHON_CPPFLAGS="$PYTHON_CPPFLAGS -I$py_val"])
    ])
    AC_MSG_RESULT([$PYTHON_CPPFLAGS])
    AC_SUBST(PYTHON_CPPFLAGS)

    AC_MSG_CHECKING([for Python libraries])
    AS_IF([test "x$PYTHON_LIBS" = x],
    [
	_PYTHON_SYSCONFIG([[get_config_var('LIBS')]], [PYTHON_LIBS=$py_val])
    ])
    AC_MSG_RESULT([$PYTHON_LIBS])
    AC_SUBST(PYTHON_LIBS)

    # Get the CFLAGS and LIBS for boost::python.
    # This does an AC_SUBST() of BOOST_PYTHON_LIBS
    # For the CFLAGS we must assume that boost is at the top-level, for instance in /usr/include/:
    AX_BOOST_PYTHON_EMC

    AC_CHECK_HEADER($INCLUDEPY/Python.h,[],
	[AC_MSG_ERROR([Required header Python.h missing.  Install it, or specify --disable-python to skip the parts of Machinekit that depend on Python])])

    AC_MSG_CHECKING(for site-package location)
    SITEPY=`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_python_lib(1)'`
    AC_MSG_RESULT($SITEPY)

    AC_CHECK_HEADERS(GL/gl.h GL/glu.h,[],[AC_MSG_ERROR([Required OpenGL header missing.  Install it, or specify --disable-python to skip the parts of Machinekit that depend on Python])])

    AC_CHECK_LIB(GL, glBegin, [], [AC_MSG_ERROR([Required GL library missing.  Install it, or specify --disable-python to skip the parts of Machinekit that depend on Python])])

    AC_MSG_CHECKING(for working GLU quadrics)

    AC_TRY_COMPILE([
#include <GL/gl.h>
#include <GL/glu.h>
],
	[GLUquadric *q;],
	[AC_MSG_RESULT(yes)],[
		AC_MSG_ERROR([Required GLU library missing.  Install it or specify --disable-python to skip the parts of Machinekit that depend on Python])]

    )

    AC_MSG_CHECKING(for Xmu headers)
    AC_CHECK_HEADERS(X11/Xmu/Xmu.h,[],[AC_MSG_ERROR([Required Xmu header missing.  Install it, or specify --disable-python to skip the parts of Machinekit that depend on Python])])
fi

AC_SUBST([BUILD_PYTHON])
AC_SUBST([LIBPYTHON])
AC_SUBST([INCLUDEPY])
AC_SUBST([SITEPY])

if test "$BUILD_EMCWEB" = yes; then
   # these are needed for the emcweb interface only
   AX_BOOST_BASE(1.34, , [AC_MSG_ERROR([Boost 1.34.0 or newer required])])
   AX_BOOST_SERIALIZATION()
   AX_BOOST_THREAD()
   AX_BOOST_SYSTEM()
fi


##############################################################################
# files that get created by ./configure                                      #
##############################################################################
AC_CONFIG_FILES([../scripts/gen-rtapi.ini.sh])
AC_CONFIG_FILES([../scripts/check-build-vs-configure-sha], [chmod +x ../scripts/check-build-vs-configure-sha])
AC_CONFIG_FILES([../man/man1/linuxcnc.1])
AC_CONFIG_FILES([../scripts/linuxcnc], [chmod +x ../scripts/linuxcnc])
AC_CONFIG_FILES([../scripts/halrun], [chmod +x ../scripts/halrun])
AC_CONFIG_FILES([../scripts/rip-environment], [chmod +x ../scripts/rip-environment])
AC_CONFIG_FILES([../scripts/haltcl], [chmod +x ../scripts/haltcl])
AC_CONFIG_FILES([../scripts/realtime], [chmod +x ../scripts/realtime])
AC_CONFIG_FILES([../scripts/linuxcnc_var], [chmod +x ../scripts/linuxcnc_var])
AC_CONFIG_FILES(Makefile.inc)
AC_CONFIG_FILES(Makefile.modinc)
AC_CONFIG_FILES(../tcl/linuxcnc.tcl)
AC_CONFIG_FILES(../lib/python/nf.py)
AC_CONFIG_FILES([../scripts/linuxcncmkdesktop], [chmod +x ../scripts/linuxcncmkdesktop])
AC_CONFIG_FILES(../share/applications/linuxcnc-latency.desktop)
AC_CONFIG_FILES(../share/applications/linuxcnc.desktop)
AC_CONFIG_FILES(../share/desktop-directories/cnc.directory)
AC_CONFIG_FILES(../share/menus/CNC.menu)
AC_CONFIG_FILES(machinekitcfg.py-tmp)
AC_CONFIG_FILES(../scripts/machinekit.ini.tmpl)
AC_OUTPUT

# if there is no machinekit.ini file, use
# the autogenerated one; else dont touch it as the user might have
# adapted it already
AC_MSG_CHECKING(if $BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini exists )

if test -e "$BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini"; then
   AC_MSG_RESULT(not touching existing	$BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini file)
else
   AC_MSG_RESULT($BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini does not exist - using default version)
   mkdir -p  $BUILD_TOPLEVEL/etc/linuxcnc
   cp ../scripts/machinekit.ini.tmpl $BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini
   AC_MSG_RESULT(please review and edit $BUILD_TOPLEVEL/etc/linuxcnc/machinekit.ini as needed!)
fi

# clean up machinekitcfg.py:
# subsitute "yes" by True
# subsitute "no" by False
AC_MSG_RESULT(touching up $BUILD_TOPLEVEL/lib/python/machinekit/config.py)
mkdir -p $BUILD_TOPLEVEL/lib/python/machinekit
sed -e 's/"yes"/True/g' -e 's/"no"/False/g' \
    <machinekitcfg.py-tmp \
    >machinekitcfg.py-tmp2
# recursively subsitute variables
while : ; do
    sed -e 's/\"/\\"/g' \
        <machinekitcfg.py-tmp2 \
        >machinekitcfg.py-tmp3
    echo -e "$(eval "echo -e \"`<machinekitcfg.py-tmp3`\"")" \
        >machinekitcfg.py-tmp4
    cmp --silent machinekitcfg.py-tmp2 machinekitcfg.py-tmp4
    RESULT=$?
    rm machinekitcfg.py-tmp2 machinekitcfg.py-tmp3
    mv machinekitcfg.py-tmp4 machinekitcfg.py-tmp2
    [[ $RESULT -ne 0 ]] || break
done
cat machinekitcfg.py-tmp2 > $BUILD_TOPLEVEL/lib/python/machinekit/config.py
rm machinekitcfg.py-tmp2
AC_MSG_CHECKING(usability of $BUILD_TOPLEVEL/lib/python/machinekit/config.py)
python $BUILD_TOPLEVEL/lib/python/machinekit/config.py
AC_MSG_RESULT(done)

##############################################################################
# message to the user what to do next, after a succesfull ./configure        #
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
echo "#                Machinekit - Enhanced Machine Controller            #"
echo "######################################################################"
echo "#                                                                    #"
echo "#   Machinekit is a software system for computer control of machine  #"
echo "#   tools such as milling machines. Machinekit is released under the #"
echo "#   GPL.  Check out http://www.machinekit.io for more details.       #"
echo "#                                                                    #"
echo "#                                                                    #"
echo "#   It seems that ./configure completed successfully.                #"
echo "#   If things don't work check config.log for errors & warnings      #"
echo "#                                                                    #"

if test "xyes" = "x$RUN_IN_PLACE"; then
echo "#   Next compile by typing                                           #"
echo "#         make                                                       #"
if test $with_drivers = yes ; then
echo "#         sudo make setuid                                           #"
fi
echo "#                                                                    #"
echo "#   Before running the software, set the environment:                #"
echo "#         . (top dir)/scripts/rip-environment                        #"
else
bold
echo "#   warning: If you already have an installed Machinekit, this will  #"
echo "#         replace an existing installation.  If you have installed   #"
echo "#         a machinekit package, this will damage the package.        #"
offbold
echo "#   hint: To test a self-built version of machinekit without damaging#"
echo "#         the package version, don't specify a --prefix              #"
echo "#                                                                    #"
echo "#   Next compile by typing                                           #"
echo "#         make                                                       #"
echo "#   then install it by typing                                        #"
echo "#         sudo make install                                          #"
fi
echo "#                                                                    #"
echo "#   To run the software type                                         #"
echo "#         machinekit                                                 #"
echo "#                                                                    #"
echo "######################################################################"
echo ""
echo ""
