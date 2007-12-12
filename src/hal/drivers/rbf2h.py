import sys, getopt

GPL=0
LGPL=0
EXTRA=[]
opts, args = getopt.getopt(sys.argv[1:], "glc:")
for k, v in opts:
    if k == "-g":
	GPL = 1
    elif k == "-l":
	LGPL = 1
    elif k == "-c":
	EXTRA.append("//   " + v)
    else:
	raise SystemExit, "Unknown argument: %r" % k

sys.stdout.write("\n".join(EXTRA))

print """
//    This is a generated file; the "corresponding source code" is the set of
//    files used by Altera's Quartus software to generate an .rbf-format
//    fpga firmware file.
"""

if GPL:
    print """\
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
"""

if LGPL:
    print """\
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Lesser General Public
//    License as published by the Free Software Foundation; either
//    version 2.1 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""

h = open(args[0]).read();

print "static unsigned char firmware[] = {"
for i, c in enumerate(h):
    print "%3d," % ord(c),
    if i % 16 == 15: print
print "};"
