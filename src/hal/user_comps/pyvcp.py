#!/usr/bin/env python
#    This is a component of emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
#
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


from pyvcp_widgets import *
import vcpparse
import hal


def main():
 
    try:
        filename=sys.argv[1]
    except:
        print "Error: No XML file specified!"
        sys.exit()
    pyvcp0 = Tk()
    vcpparse.filename=filename
    pycomp=vcpparse.create_vcp(pyvcp0)
    pyvcp0.mainloop()
    pycomp.ready()


main()
