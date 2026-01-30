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
import getopt
import shutil

def usage():
    print("""Build and install Modbus components that use the Mesa PktUART
Usage:
  modcompile [opts] device.mod...  Compile and install a driver defined in
                                   device.mod
  modcompile [opts] all            Compile and install all .mod definition
                                   files in this directory.
Options:
  -h|--help       This message
  -k|--keep       Keep temporary files
  -n|--noinstall  Don't perform the install step
  -v|--verbose    Verbose compile
""")
    sys.exit(2)

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

def main():
    # Get the command-line options
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hklnv", ["help", "keep", "noinstall", "verbose"])
    except getopt.GetoptError as err:
        raise SystemExit(err)  # Something like "option -a not recognized"

    # Check the options
    verbose = ""
    install = " install"
    keep = False
    for o, a in opts:
        if o in ("-v", "--verbose"):
            verbose = " V=1"
        elif o in ("-n", "--noinstall"):
            install = ""
        elif o in ("-k", "--keep"):
            keep = True
        elif o in ("-h", "--help"):
            usage()
        else:
            raise SystemExit("Unhandled option: '%s'" % o);

    if len(args) < 1:
        raise SystemExit("Must have at least one 'file.mod' argument or 'all' for all.");

    if len(args) > 1 and "all" in args:
        raise SystemExit("Cannot do both 'all' modules and specific modules.");
    if args[0] == "all":
        args = [f for f in os.listdir(".") if f.endswith(".mod")]
    # 'args' now contains all modules to compile

    if len(args) < 1:
        raise SystemExit("No modules found (*.mod files) to compile.");

    # Create a temporary directory and copy the C template into it
    tempdir = tempfile.mkdtemp(prefix="modcompile")
    BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
    csrc = os.path.join(tempdir, "mesa_modbus.c")
    if verbose:
        print(csrc)
    shutil.copy(os.path.join(BASE, "share", "linuxcnc", "mesa_modbus.c.tmpl"), csrc)

    # For each module to compile...
    for f in args:
        modname = os.path.splitext(os.path.basename(f))[0]
        # The module definition is #included as mesa_modbus.h
        m = open(os.path.join(tempdir, "Makefile"), "w")
        print("obj-m += %s.o" % modname, file=m)
        print("%s-objs:=mesa_modbus.o" % modname, file=m)
        print("include %s" % find_modinc(), file=m)
        print("EXTRA_CFLAGS += -I%s" % tempdir, file=m)
        print("EXTRA_CFLAGS += -DMODFILE=%s" % os.path.abspath(f), file=m)
        print("EXTRA_CFLAGS += -D_COMP_NAME_=%s" % modname, file=m)
        m.close()
        result = os.system("cd %s && make -B -S modules %s%s" % (tempdir, install, verbose))
        if result != 0:
            raise SystemExit(os.WEXITSTATUS(result) or 1)

    # Cleanup step
    if keep:
        print("Sources and build files kept in: %s" % tempdir)
    else:
        shutil.rmtree(tempdir)

if __name__ == "__main__":
    main()
