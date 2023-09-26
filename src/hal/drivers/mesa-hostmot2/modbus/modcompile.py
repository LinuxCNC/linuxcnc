#!/usr/bin/python3

#    Build Realtime Modbus modules to use the Mesa FPGA card PktUART

#    Based on parts of halcompile

#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.



import sys
import os
import tempfile


def usage(exitval=0):
    print("""Build and install Modbus components that use the Mesa PktUART

Usage:
          modcompile device.mod  Compile and install a driver defined in
                                 device.h
          modcompile all         Compile and install all definition files
                                 in this directory.

""")
    raise SystemExit(exitval)

modinc = None
def find_modinc():
    global modinc
    if modinc: return modinc
    d = os.path.abspath(os.path.dirname(os.path.dirname(sys.argv[0])))
    for e in ['src', 'etc/linuxcnc', '/etc/linuxcnc', 'share/linuxcnc']:
        e = os.path.join(d, e, 'Makefile.modinc')
        if os.path.exists(e):
            modinc = e
            return e
    raise SystemExit("Unable to locate Makefile.modinc")

if len(sys.argv) < 2:
    usage(0)

if sys.argv[1] == "all":
    names = [f for f in os.listdir(".") if f.endswith(".mod")]
else:
    names = [sys.argv[1]]

tempdir = tempfile.mkdtemp()
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
print(os.path.join(tempdir, "mesa_modbus.c"))
os.symlink(os.path.join(BASE, "share", "linuxcnc", "mesa_modbus.c.tmpl"), os.path.join(tempdir, "mesa_modbus.c"))
for f in names:
    b = os.path.splitext(os.path.basename(f))[0]
    # The module definition is #included as mesa_modbus.h
    m = open(os.path.join(tempdir, "Makefile"), "w")
    print("obj-m += %s.o" % b,file=m)
    print("%s-objs:=mesa_modbus.o" % b,file=m)
    print("include %s" % find_modinc(), file=m)
    print("EXTRA_CFLAGS += -I%s" % tempdir, file=m)
    print("EXTRA_CFLAGS += -DMODFILE=%s" % os.path.abspath(f), file=m)
    print("EXTRA_CFLAGS += -D_COMP_NAME_=%s" % b, file=m)
    m.close()
    os.system("touch mesa_modbus.c") # Force a recompile
    result = os.system("cd %s && make -S modules install" % tempdir)

    if result != 0:
        raise SystemExit(os.WEXITSTATUS(result) or 1)
