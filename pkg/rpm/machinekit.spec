# Default values are --without-xenomai --with-rtpreempt

%bcond_without rtpreempt
%bcond_with xenomai

%global commit master
%global gittag GIT-TAG
%global shortcommit master

Summary: Machine Kit
Vendor: MachineKit
Packager: MachineKit <support@machinekit.io>
Name:         machinekit
Version:      0.1.0
Release:      2
Epoch:        0
License: GNU
Group: Machine Control/Daemons
URL: http://www.machinekit.io

Source0: https://github.com/machinekit/%{name}/archive/%{commit}/%{name}-%{shortcommit}.tar.gz#/%{name}-%{version}.tar.gz

Conflicts: linuxcnc

Requires: rsyslog
Requires: libstdc++
Requires: python2 psmisc procps-ng bc
Requires: czmq zeromq python2-zmq jansson libwebsockets protobuf
Requires: bwidget tkimg
Requires: uriparser openssl libuuid libmodbus libusb glib2
Requires: gtk2 tcl tcl-tclreadline tk readline libXaw python2-tkinter mesa-libGLU
Requires: python2-simplejson python2-numpy python2-xlib pygtkglext python2-configobj python-avahi
Requires: pygtk2-libglade gnome-python2 python2-protobuf >= 2.4.1
Requires: python2-Cython python2-pyftpdlib boost-python python-xdot python2-pydot

BuildRequires: libstdc++-devel gcc gcc-c++
BuildRequires: pkgconf bwidget redhat-lsb autoconf automake
BuildRequires: libudev-devel protobuf-compiler protobuf-devel
BuildRequires: czmq-devel zeromq-devel jansson-devel libwebsockets-devel
BuildRequires: uriparser-devel openssl-devel libuuid-devel avahi-devel libmodbus-devel libusb-devel glib2-devel
BuildRequires: gtk2-devel tcl-devel tk-devel readline-devel libXaw-devel boost-devel mesa-libGLU-devel
BuildRequires: python2-zmq python2-Cython python2-pyftpdlib python2-protobuf python2-devel python2-tkinter
BuildRequires: python2-simplejson

AutoReq: no

%description
PC based motion controller for real-time Linux
 Machinekit is the next-generation Enhanced Machine Controller which
 provides motion control for CNC machine tools and robotic
 applications (milling, cutting, routing, etc.).
 .
 This package provides components and drivers that run on a non-realtime
 (Posix) system.

%if 0%{with rtpreempt}
%package rt-preempt
Summary: MachineKit components and drivers that run on an RT-Preempt system
Group: Machine Control/Daemons
Requires: %{name} = %{version}-%{release}
AutoReq: no

%description rt-preempt
PC based motion controller for real-time Linux
 Machinekit is the next-generation Enhanced Machine Controller which
 provides motion control for CNC machine tools and robotic
 applications (milling, cutting, routing, etc.).
 .
 This package provides components and drivers that run on an RT-Preempt system.
%endif

%if 0%{with xenomai}
%package xenomai
Summary: MachineKit components and drivers that run on a Xenomai system
Group: Machine Control/Daemons
Requires: %{name} = %{version}-%{release}
Requires: xenomai xenomai-libs
BuildRequires: xenomai-devel
AutoReq: no

%description xenomai
PC based motion controller for real-time Linux
 Machinekit is the next-generation Enhanced Machine Controller which
 provides motion control for CNC machine tools and robotic
 applications (milling, cutting, routing, etc.).
 .
 This package provides components and drivers that run on a Xenomai system.
%endif

%package devel
Summary: Development files for MachineKit
Group: Machine Control/Daemons
Requires: %{name} = %{version}-%{release}

%description devel
 This package contains libraries and header files needed for
 developing software that uses MachineKit.

%prep
%autosetup -n %{name}-%{commit}

%build
cd src
./autogen.sh
./configure --prefix=/usr \
%if %{with xenomai}
    --with-xenomai \
%endif
%if %{with rtpreempt}
    --with-rt-preempt \
%endif
    --with-posix \
    --enable-usermode-pci \
    --sysconfdir=/etc \
%ifarch x86_64
    --libdir=/usr/lib64 \
%endif
    --with-rundir=/var/run \
    --mandir=/usr/share/man \
    --enable-emcweb

# temporary patch - remove all "-o root"
sed -i "s/ -o root//g" Makefile
%ifarch x86_64
# fix destination path for emc2 tcl and rtl
sed -i "s/EMC2_TCL_DIR=\/usr\/lib\//EMC2_TCL_DIR=\/usr\/lib64\//" Makefile.inc
sed -i "s/EMC2_RTLIB_BASE_DIR=\/usr\/lib\//EMC2_RTLIB_BASE_DIR=\/usr\/lib64\//" Makefile.inc
sed -i "s/EMC2_TCL_LIB_DIR = \/usr\/lib\//EMC2_TCL_LIB_DIR = \/usr\/lib64\//" Makefile.inc
sed -i 's/tcldir = \${prefix}\/lib\//tcldir = \${prefix}\/lib64\//' Makefile.inc
%endif

make -j2
touch build-stamp

%install
# Clean up in case there is trash left from a previous build
rm -rf $RPM_BUILD_ROOT

cd src && make install DESTDIR=$RPM_BUILD_ROOT
# cd %{getenv:TESTBUILDPATH}/../src && make install DESTDIR=$RPM_BUILD_ROOT

# atm is not required
# install -m644 packages/rpm/machinekit.service $RPM_BUILD_ROOT%{_unitdir}/machinekit.service

#######################################################################
## Scriplets section                                                 ##
#######################################################################

%preun
rm -f %{_libdir}/linuxcnc/posix/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/posix/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/posix/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/posix/pru_decamux.dbg || :

%post
# ensure the links do not pre-exist, from previous installs.
# or user work-arounds,  which will produce error messages
rm -f %{_libdir}/linuxcnc/posix/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/posix/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/posix/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/posix/pru_decamux.dbg || :

# make symlinks to BBB pru_*.*
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.bin %{_libdir}/linuxcnc/posix/pru_generic.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.dbg %{_libdir}/linuxcnc/posix/pru_generic.dbg
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.bin %{_libdir}/linuxcnc/posix/pru_decamux.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.dbg %{_libdir}/linuxcnc/posix/pru_decamux.dbg

# setup emcweb var files
rm -rf %{_var}/cache/linuxcnc/www || :
mkdir -p %{_var}/cache/linuxcnc/www/data
chmod a+rw %{_var}/cache/linuxcnc/www/data

# add symbolic links to static files
cd %{_datarootdir}/linuxcnc/doc-root
find -type d -exec mkdir --parents -- %{_var}/cache/linuxcnc/www/{} \;
find -type f -exec ln -s -- %{_datarootdir}/linuxcnc/doc-root/{} %{_var}/cache/linuxcnc/www/{} \;

# if it doesnt exist, create /var/log/linuxcnc.log and make it publically readable
# touch /var/log/linuxcnc.log
# chmod ugo+r /var/log/linuxcnc.log

# Until rebranded completely solve corner cases like this                
if [ -f "%{_datarootdir}/linuxcnc" ]; then
    if [ ! -f "%{_datarootdir}/machinekit" ]; then
        ln -s %{_datarootdir}/linuxcnc %{_datarootdir}/machinekit || :
    fi
fi

/sbin/ldconfig > /dev/null 2>&1 || :

%postun
if [ $1 -eq 0 ] ; then
  rm -f %{_libdir}/linuxcnc/posix/pru_generic.bin || :
  rm -f %{_libdir}/linuxcnc/posix/pru_generic.dbg || :
  rm -f %{_libdir}/linuxcnc/posix/pru_decamux.bin || :
  rm -f %{_libdir}/linuxcnc/posix/pru_decamux.dbg || :

  if [ -L "%{_datarootdir}/machinekit" ]; then
    rm -f %{_datarootdir}/machinekit || :
  fi

  rm -rf %{_var}/cache/linuxcnc || :
fi

%if 0%{with rtpreempt}
%post rt-preempt
# ensure the links do not pre-exist, from previous installs
# or user work-arounds,  which will produce error messages
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.dbg || :

# make symlinks to BBB pru_*.*
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.bin %{_libdir}/linuxcnc/rt-preempt/pru_generic.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.dbg %{_libdir}/linuxcnc/rt-preempt/pru_generic.dbg
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.bin %{_libdir}/linuxcnc/rt-preempt/pru_decamux.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.dbg %{_libdir}/linuxcnc/rt-preempt/pru_decamux.dbg

%preun rt-preempt
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.dbg || :

%postun rt-preempt
if [ $1 -eq 0 ] ; then
  rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.bin || :
  rm -f %{_libdir}/linuxcnc/rt-preempt/pru_generic.dbg || :
  rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.bin || :
  rm -f %{_libdir}/linuxcnc/rt-preempt/pru_decamux.dbg || :
fi
%endif

%if 0%{with xenomai}
%post xenomai
# ensure the links do not pre-exist, from previous installs
# or user work-arounds,  which will produce error messages
rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.dbg || :

# make symlinks to BBB pru_*.*
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.bin %{_libdir}/linuxcnc/xenomai/pru_generic.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_generic.dbg %{_libdir}/linuxcnc/xenomai/pru_generic.dbg
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.bin %{_libdir}/linuxcnc/xenomai/pru_decamux.bin
ln -sf %{_libdir}/linuxcnc/prubin/pru_decamux.dbg %{_libdir}/linuxcnc/xenomai/pru_decamux.dbg

%preun xenomai
rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.bin || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.dbg || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.bin || :
rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.dbg || :

%postun xenomai
if [ $1 -eq 0 ] ; then
  rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.bin || :
  rm -f %{_libdir}/linuxcnc/xenomai/pru_generic.dbg || :
  rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.bin || :
  rm -f %{_libdir}/linuxcnc/xenomai/pru_decamux.dbg || :
fi
%endif

#######################################################################
## Files section                                                     ##
#######################################################################

%files
%defattr(-,root,root)

%dir %{_sysconfdir}/linuxcnc
%attr(0644,root,root) %config %{_sysconfdir}/linuxcnc/*
%attr(0644,root,root) %config %{_sysconfdir}/rsyslog.d/linuxcnc.conf
%attr(0644,root,root) %config %{_sysconfdir}/security/limits.d/machinekit.conf
%attr(0644,root,root) %config %{_sysconfdir}/udev/rules.d/50-shmdrv.rules
%attr(0644,root,root) %config %{_sysconfdir}/X11/app-defaults/*
%attr(0755,root,root) %{_bindir}/*

# libs
%attr(0755,root,root) %{_libdir}/compat.so
%attr(0755,root,root) %{_libdir}/hal.so
%attr(0755,root,root) %{_libdir}/rtapi.so
%attr(0755,root,root) %{_libdir}/shmcommon.so
%attr(0755,root,root) %{_libdir}/libcanterp.so.0
%attr(0755,root,root) %{_libdir}/liblinuxcnchal.so.0
%attr(0755,root,root) %{_libdir}/liblinuxcncini.so.0
%attr(0755,root,root) %{_libdir}/liblinuxcncshm.so.0
%attr(0755,root,root) %{_libdir}/liblinuxcnculapi.so.0
%attr(0755,root,root) %{_libdir}/libmachinetalk-npb.so.0
%attr(0755,root,root) %{_libdir}/libmachinetalk-pb2++.so.0
%attr(0755,root,root) %{_libdir}/libmtalk.so.0
%attr(0755,root,root) %{_libdir}/libnml.so.0
%attr(0755,root,root) %{_libdir}/libposemath.so.0
%attr(0755,root,root) %{_libdir}/libpyplugin.so.0
%attr(0755,root,root) %{_libdir}/librs274.so.0
%attr(0755,root,root) %{_libdir}/librtapi_math.so.0

%exclude %{_libdir}/libcanterp.so
%exclude %{_libdir}/liblinuxcnchal.so
%exclude %{_libdir}/liblinuxcncini.so
%exclude %{_libdir}/liblinuxcncshm.so
%exclude %{_libdir}/liblinuxcnculapi.so
%exclude %{_libdir}/libmachinetalk-npb.so
%exclude %{_libdir}/libmachinetalk-pb2++.so
%exclude %{_libdir}/libmtalk.so
%exclude %{_libdir}/librtapi_math.so
%exclude %{_libdir}/librs274.so
%exclude %{_libdir}/libposemath.so
%exclude %{_libdir}/libnml.so

%dir %{_libdir}/tcltk/linuxcnc
%dir %{_libdir}/tcltk/linuxcnc/bin
%dir %{_libdir}/tcltk/linuxcnc/msgs
%dir %{_libdir}/tcltk/linuxcnc/scripts

%attr(-,root,root) %{_libdir}/tcltk/linuxcnc/*.tcl
%attr(0755,root,root) %{_libdir}/tcltk/linuxcnc/hal.so
%attr(0755,root,root) %{_libdir}/tcltk/linuxcnc/linuxcnc.so
%attr(0755,root,root) %{_libdir}/tcltk/linuxcnc/bin/*
%attr(0644,root,root) %{_libdir}/tcltk/linuxcnc/msgs/*
%attr(-,root,root) %{_libdir}/tcltk/linuxcnc/scripts/*

%dir %{python2_sitearch}/drivers
%{python2_sitearch}/drivers/*.py*
%dir %{python2_sitearch}/fdm
%dir %{python2_sitearch}/fdm/config
%{python2_sitearch}/fdm/*.py*
%{python2_sitearch}/fdm/config/*.py*
%dir %{python2_sitearch}/gladevcp
%{python2_sitearch}/gladevcp/*.py*
%{python2_sitearch}/gladevcp/*.glade
%dir %{python2_sitearch}/gmoccapy
%{python2_sitearch}/gmoccapy/*.py*
%dir %{python2_sitearch}/gscreen
%{python2_sitearch}/gscreen/*.py*
%dir %{python2_sitearch}/machinekit
%dir %{python2_sitearch}/machinekit/nosetests
%{python2_sitearch}/machinekit/*.py*
%{python2_sitearch}/machinekit/nosetests/*.py*
# why these files there ?
%attr(0755,root,root) %{python2_sitearch}/machinekit/compat.so
%attr(0755,root,root) %{python2_sitearch}/machinekit/hal.so
%attr(0755,root,root) %{python2_sitearch}/machinekit/rtapi.so
%attr(0755,root,root) %{python2_sitearch}/machinekit/shmcommon.so
%dir %{python2_sitearch}/machinetalk
%dir %{python2_sitearch}/machinetalk/protobuf
%{python2_sitearch}/machinetalk/*.py*
%{python2_sitearch}/machinetalk/protobuf/*.py*
%dir %{python2_sitearch}/rs274
%{python2_sitearch}/rs274/*.py*
%dir %{python2_sitearch}/stepconf
%{python2_sitearch}/stepconf/*.py*
%dir %{python2_sitearch}/touchy
%{python2_sitearch}/touchy/*.py*
%{python2_sitearch}/*.py*
%attr(0755,root,root) %{python2_sitearch}/gcode.so
%attr(0755,root,root) %{python2_sitearch}/_hal.so
%attr(0755,root,root) %{python2_sitearch}/lineardeltakins.so
%attr(0755,root,root) %{python2_sitearch}/linuxcnc.so
%attr(0755,root,root) %{python2_sitearch}/minigl.so
%attr(0755,root,root) %{python2_sitearch}/preview.so
%attr(0755,root,root) %{python2_sitearch}/_togl.so

%dir %{_libexecdir}/linuxcnc
%attr(0755,root,root) %{_libexecdir}/linuxcnc/pci_read
%attr(0755,root,root) %{_libexecdir}/linuxcnc/pci_write
%attr(0755,root,root) %{_libexecdir}/linuxcnc/inivar
%attr(0755,root,root) %{_libexecdir}/linuxcnc/flavor
%attr(0755,root,root) %{_libexecdir}/linuxcnc/rtapi_msgd

%dir %{_datarootdir}/axis
%dir %{_datarootdir}/axis/images
%dir %{_datarootdir}/axis/tcl
%attr(0644,root,root) %{_datarootdir}/axis/images/*
%attr(0644,root,root) %{_datarootdir}/axis/tcl/*

%dir %{_datarootdir}/fdm
%dir %{_datarootdir}/fdm/thermistor_tables
%attr(0644,root,root) %{_datarootdir}/fdm/thermistor_tables/*

%dir %{_datarootdir}/glade3
%dir %{_datarootdir}/glade3/catalogs
%dir %{_datarootdir}/glade3/pixmaps
%attr(0644,root,root) %{_datarootdir}/glade3/catalogs/*
%attr(0644,root,root) %{_datarootdir}/glade3/pixmaps/*

%dir %{_datarootdir}/gmoccapy
%dir %{_datarootdir}/gmoccapy/images
%attr(0644,root,root) %{_datarootdir}/gmoccapy/*.glade
%attr(0644,root,root) %{_datarootdir}/gmoccapy/images/*

%dir %{_datarootdir}/gscreen
%dir %{_datarootdir}/gscreen/images
%dir %{_datarootdir}/gscreen/skins
%dir %{_datarootdir}/gscreen/skins/9_axis
%dir %{_datarootdir}/gscreen/skins/gaxis
%dir %{_datarootdir}/gscreen/skins/gaxis_no_plot
%dir %{_datarootdir}/gscreen/skins/industrial
%attr(0644,root,root) %{_datarootdir}/gscreen/images/*
%attr(0644,root,root) %{_datarootdir}/gscreen/skins/9_axis/*
%attr(0644,root,root) %{_datarootdir}/gscreen/skins/gaxis/*
%attr(0644,root,root) %{_datarootdir}/gscreen/skins/gaxis_no_plot/*
%attr(0644,root,root) %{_datarootdir}/gscreen/skins/industrial/*

%dir %{_datarootdir}/gtksourceview-2.0
%dir %{_datarootdir}/gtksourceview-2.0/language-specs
%attr(0644,root,root) %{_datarootdir}/gtksourceview-2.0/language-specs/*

%dir %{_datarootdir}/linuxcnc
%dir %{_datarootdir}/linuxcnc/doc-root
%dir %{_datarootdir}/linuxcnc/examples
%dir %{_datarootdir}/linuxcnc/pncconf
%dir %{_datarootdir}/linuxcnc/ncfiles
%dir %{_datarootdir}/linuxcnc/stepconf
%attr(-,root,root) %{_datarootdir}/linuxcnc/*.glade
%attr(-,root,root) %{_datarootdir}/linuxcnc/*.ui
%attr(-,root,root) %{_datarootdir}/linuxcnc/*.nml
%attr(-,root,root) %{_datarootdir}/linuxcnc/*.gif
%attr(-,root,root) %{_datarootdir}/linuxcnc/*.png
%attr(-,root,root) %{_datarootdir}/linuxcnc/doc-root/*
%attr(-,root,root) %{_datarootdir}/linuxcnc/examples/*
%attr(-,root,root) %{_datarootdir}/linuxcnc/pncconf/*
%attr(-,root,root) %{_datarootdir}/linuxcnc/ncfiles/*
%attr(-,root,root) %{_datarootdir}/linuxcnc/stepconf/*.glade

%attr(0644,root,root) %{_datarootdir}/locale/*/LC_MESSAGES/*.mo

%dir %{_docdir}/linuxcnc
%attr(0644,root,root) %{_docdir}/linuxcnc/axis_light_background
%attr(0644,root,root) %{_docdir}/linuxcnc/xlinuxcnc.asciidoc
%attr(0644,root,root) %{_docdir}/linuxcnc/tklinuxcnc.asciidoc
%attr(0644,root,root) %{_docdir}/linuxcnc/README.axis

# posix api
%dir %{_libdir}/linuxcnc/posix
%attr(0755,root,root) %{_libdir}/linuxcnc/posix/*.so
%attr(0755,root,root) %{_libdir}/linuxcnc/ulapi-posix.so
%attr(4711,root,root) %{_libexecdir}/linuxcnc/rtapi_app_posix

%if 0%{with rtpreempt}
%files rt-preempt
%defattr(-,root,root)

%dir %{_libdir}/linuxcnc/rt-preempt
%attr(0755,root,root) %{_libdir}/linuxcnc/rt-preempt/*.so
%attr(0755,root,root) %{_libdir}/linuxcnc/ulapi-rt-preempt.so
%attr(4711,root,root) %{_libexecdir}/linuxcnc/rtapi_app_rt-preempt
%endif

%if 0%{with xenomai}
%files xenomai
%defattr(-,root,root)

%dir %{_libdir}/linuxcnc/xenomai
%attr(0755,root,root) %{_libdir}/linuxcnc/xenomai/*.so
%attr(0755,root,root) %{_libdir}/linuxcnc/ulapi-xenomai.so
%attr(4711,root,root) %{_libexecdir}/linuxcnc/rtapi_app_xenomai
%endif

%files devel
%defattr(-,root,root)
%attr(0644,root,root) %{_includedir}/linuxcnc/*
%attr(0644,root,root) %{_libdir}/liblinuxcnc.a
%dir %{_datarootdir}/linuxcnc
%attr(0644,root,root) %{_datarootdir}/linuxcnc/Makefile.inc
%attr(0644,root,root) %{_datarootdir}/linuxcnc/Makefile.modinc
