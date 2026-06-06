#!/usr/bin/env python3
#    This is a component of emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
#    
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


""" Python Virtual Control Panel for LinuxCNC

    A virtual control panel (VCP) is used to display and control
    HAL pins, which are either BIT or FLOAT valued.

    Usage: pyvcp [-g WxH+X+Y] [-c compname] [myfile.xml]

    compname is the name of the pyvcp panel instance on the server
    (default: "pyvcp").

    myfile.xml is accepted for backwards compatibility but ignored;
    the panel XML is fetched from gomc-server which loads the panel
    via "load pyvcp" in a HAL file.

    -g option allows setting of the initial size and/or position of the panel
"""

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import vcpparse

from tkinter import Tk
import getopt

def usage():
    """ prints the usage message """
    print("Usage: pyvcp [-g WIDTHxHEIGHT+XOFFSET+YOFFSET] [-c panel_name] [myfile.xml]")
    print("Connects to a pyvcp panel already loaded on gomc-server.")
    print("If the panel name is not specified, 'pyvcp' is used.")
    print("-g options are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen")
    print("use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position")

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "c:g:")
    except getopt.GetoptError as detail:
        print(detail)
        usage()
        sys.exit(1)
    window_geometry = None
    component_name = None
    for o, a in opts:
        if o == "-c":
            component_name = a
        if o == "-g":
            window_geometry = a

    # Accept XML filename for backwards compat but ignore it.
    # Derive component name from filename if -c not given.
    if args and component_name is None:
        component_name = os.path.splitext(os.path.basename(args[0]))[0]

    if component_name is None:
        component_name = "pyvcp"

    pyvcp0 = Tk()
    pyvcp0.title(component_name)
    if window_geometry:
        pyvcp0.geometry(window_geometry)

    pycomp = vcpparse.create_vcp_rest(master=pyvcp0, compname=component_name)

    try:
        try:
            pyvcp0.mainloop()
        except KeyboardInterrupt:
            sys.exit(0)
    finally:
        pycomp.stop()

if __name__ == '__main__':
    main()
