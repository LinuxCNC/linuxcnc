Summary: Enhanced Machine Controller
Name: emc2 
%define version 0
Version: %{version}
Release: 1
Group: Applications/Engineering
URL: http://www.linuxcnc.org/
Packager: Jonathan Stark <emc@starks.org>
Copyright: GPL 
BuildRoot: %{_tmppath}/emc2-%{version}-root
Provides: emc2 
PreReq: chkconfig
Source0: emc2.tar.bz2

%define EMCDIR /usr/local/emc2/ 

%description
A free and powerful PC-based CNC machinery Controller


%prep
%setup -q -n %{name}-%{version} -c


%build

cd emc2
./configure --with-manpath=/usr/local/share/man/
make


%install
rm -rf $RPM_BUILD_ROOT

export EMC_ROOT=$RPM_BUILD_ROOT/%{EMCDIR}

cd emc2
make install DESTDIR=$RPM_BUILD_ROOT

# remove CVS directories
# This could be a bit dangerous, so be careful if you edit it;
# you are root when making rpm's, and this does rm -rf .....

#cd $EMC_ROOT ; tar -cf - ./ | tar -tf - | grep /CVS/ | xargs rm -rvf

#cd $RPM_BUILD_ROOT ; tar -cf - ./ | tar -tf - > /tmp/filelist

%post
chkconfig --add realtime

%preun
chkconfig --del realtime

%clean
#rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
#%doc CVS_CHECKOUT_DATE Documentation examples

/etc/
/usr/local/man/
/usr/local/bin/
/usr/local/etc/
/usr/local/lib/
/usr/realtime/modules/



%changelog
* Mon Jan  3 2005  Jonathan Stark <emc@starks.org>
- new version for emc2 rpm tag
* Sat Oct 16 2004  Jonathan Stark <emc@starks.org>
- EMC2 from cvs and rtai-3.1
* Tue Nov 11 2003  Jonathan Stark <emc@starks.org>
- R2 with many fixes
* Wed Oct 1 2003  Jonathan Stark <emc@starks.org>
- First packaging



