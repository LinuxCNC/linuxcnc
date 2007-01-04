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

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import vcpparse
import hal
from Tkinter import Tk
import getopt

def usage():
    print "Usage: pyvcp -c hal_component_name myfile.xml"

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "c:")
    except getopt.GetoptError, detail:
        print detail
        usage()
        sys.exit(1)

    
    #try:
    #    opts, args = getopt.getopt(sys.argv[1:], "c:")
    #except getopt.GetoptError:
    #    print "usage: pyvcp -c hal_component_name myfile.xml"
    #    sys.exit(0)

    #print opts
    #print args
    component_name = None
    for o, a in opts: 
        if o == "-c": 
            component_name = a
       
            

    if component_name is None:
        usage()
        sys.exit(1)

    
    try:
        filename=args[0]
    except:
        usage()
        sys.exit(1)

    #try:
    #    filename=sys.argv[1]
    #except:
    #    print "Error: No XML file specified!"
    #    sys.exit()

    pyvcp0 = Tk()
    pyvcp0.title(component_name)
    vcpparse.filename=filename
    pycomp=vcpparse.create_vcp(compname=component_name, master=pyvcp0)
    pycomp.ready()

    try: 
        pyvcp0.mainloop()
    except KeyboardInterrupt:
        sys.exit(0)


main()
