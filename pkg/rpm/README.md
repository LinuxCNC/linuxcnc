# MachineKit packaging for RPM

## Fedora {i686, x86_64, armv7}

### prepare a build machine

* i686 and x86_64 - no special recommendations, any amd or intel box with latest stable Fedora is okay
* armv7 - [Fedora 27](https://arm.fedoraproject.org) and [Raspberry Pi 3](https://www.raspberrypi.org) is a great choice  
  download and write to SD card (8 Gb minimum 10 class is recommended) Fedora Minimal image, boot machine and open terminal (ssh or physical)

### install prerequisites

under root permissions:

```bash
dnf install wget curl rpm-build rpmdevtools
wget https://github.com/machinekit/machinekit/raw/pkg/rpm/machinekit.spec
```

```bash
dnf builddep --spec machinekit.spec
```

or if you want to build machinekit-xenomai (requires xenomai, xenomai-libs and xenomai-devel packages):

```bash
dnf builddep --spec -D "with_xenomai 1" machinekit.spec
```

```bash
useradd mockbuild
```

### prepare build environment

```bash
su - mockbuild
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
echo '%_topdir %(echo $HOME)/rpmbuild' > ~/.rpmmacros
wget https://github.com/machinekit/machinekit/raw/pkg/rpm/machinekit.spec
spectool -g -C $HOME/rpmbuild/SOURCES machinekit.spec
```

### build rpms

under mockbuild user permissions:

```bash
rpmbuild -ba machinekit.spec
```

or if you want to build machinekit-xenomai

```bash
rpmbuild -ba --with xenomai machinekit.spec
```

The results will be in $HOME/rpmbuild/RPMS/armv7hl/

```bash
machinekit-0.1.0-2.armv7hl.rpm
machinekit-devel-0.1.0-2.armv7hl.rpm
machinekit-rt-preempt-0.1.0-2.armv7hl.rpm
machinekit-xenomai-0.1.0-2.armv7hl.rpm
```

### creating a repository on build machine (just as an example)

under root permissions:

__ARMv7:__

```bash
dnf install createrepo httpd
cd /var/www/html
mkdir -p fedora/armv7hl/machinekit
cp -f $(su - mockbuild -c "echo \$HOME")/rpmbuild/RPMS/armv7hl/*.rpm fedora/27/armv7hl/machinekit/
createrepo /var/www/html/fedora/27/armv7hl/machinekit
```

__x86_64:__

```bash
dnf install createrepo httpd
cd /var/www/html
mkdir -p fedora/x86_64/machinekit
cp -f $(su - mockbuild -c "echo \$HOME")/rpmbuild/RPMS/x86_64/*.rpm fedora/27/x86_64/machinekit/
createrepo /var/www/html/fedora/27/x86_64/machinekit
```

```bash
systemctl enable httpd.service
systemctl start httpd.service
firewall-cmd --add-service=http --permanent
firewall-cmd --add-service=https --permanent
firewall-cmd --reload
```

### client setup

create machinekit repo file /etc/yum.repos.d/machinekit.repo

```bash
[machinekit]
name=Machinekit Fedora $releasever - $basearch
failovermethod=priority
baseurl=http://[BUILD_MACHINE_HOSTNAME]/fedora/$releasever/$basearch/base
enabled=1
gpgcheck=0
```

Now, just do a

```bash
dnf install machinekit
```

and if required

```bash
dnf install machinekit-rt-preempt
```

### run machinekit on client

add user machinekit, for example:

```bash
useradd machinekit
```

run machinekit

```bash
su - machinekit
machinekit
```
