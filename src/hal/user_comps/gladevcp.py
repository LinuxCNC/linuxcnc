#!/usr/bin/env python
#    This is a component of EMC
#    gladevcp Copyright 2010 Chris Morley
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

""" Python / GLADE based Virtual Control Panel for EMC

    A virtual control panel (VCP) is used to display and control
    HAL pins.

    Usage: gladevcp -g position -c compname -h halfile -x windowid myfile.glade
    compname is the name of the HAL component to be created.
    halfile contains hal commands to be executed with halcmd after the hal component is ready 
    The name of the HAL pins associated with the VCP will begin with 'compname.'
    
    myfile.glade is an XML file which specifies the layout of the VCP.

    -g option allows setting of the inital position of the panel
"""
import sys, os, subprocess
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

import hal
import getopt
import gtk
import gtk.glade
import gobject

import gladevcp.makepins

global builder,buildertype,halcomp
GTKBUILDER = 1
LIBGLADE = 0

def usage():
    """ prints the usage message """
    print "usage: gladevcp [-g WIDTHxHEIGHT+XOFFSET+YOFFSET][-c hal_component_name] [-h hal command file] [-x windowid] myfile.glade"
    print "If the component name is not specified, the basename of the xml file is used."
    print "-g options are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen"
    print "use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position"
    print "use -h halfile to execute hal statements with halcmd after the component is set up and ready"
    print "use -x windowid to start gladevcp reparenting into an existing window instead of creating a new top level window"


def on_window_destroy(widget, data=None):
        gtk.main_quit()

def main():
    """ creates a HAL component.
        parsees a glade XML file with gtk.builder or libglade
        calls gladevcp.makepins with the specified XML file
        to create pins and register callbacks.
        main window must be called "window1"
    """

    try:
        opts, args = getopt.getopt(sys.argv[1:], "c:g:w:h:")
    except getopt.GetoptError, detail:
        print detail
        usage()
        sys.exit(1)
    window_geometry = ""
    component_name = None
    parent = None
    halfile = None
    for o, a in opts:
        print o,a
        if o == "-c":
            component_name = a
        if o == "-g": 
            window_geometry = a
        if o == "-w":
            parent = int(a, 0)
        if o == "-h":
            halfile = a
    try:
        xmlname = args[0]
    except:
        usage()
        sys.exit(1)
    #if there was no component name specified use the xml file name
    if component_name is None:
        component_name = os.path.splitext(os.path.basename(xmlname))[0]
    try:
        halcomp = hal.component(component_name)
    except:
        print "*** GLADE VCP ERROR:    Asking for a HAL component using a name that already exists."
        sys.exit(0)
    #try loading as a libglade project
    try:
        builder = gtk.glade.XML(xmlname)
        buildertype = LIBGLADE
    except:
        try:
            # try loading as a gtk.builder project
            print "**** GLADE VCP INFO:    Not a libglade project, trying to load as a GTK builder project"
            builder = gtk.Builder()
            builder.add_from_file(xmlname)
            buildertype = GTKBUILDER
        except:
            print "**** GLADE VCP ERROR:    With xml file: %s"% xmlname
            sys.exit(0)
    if buildertype == LIBGLADE:
            window = builder.get_widget("window1")
    else:
            window = builder.get_object("window1")

    window.connect("destroy", on_window_destroy)
    window.set_title(component_name)
    if buildertype == LIBGLADE:
        builder.signal_autoconnect(builder)
    else:
        builder.connect_signals(builder)
    if parent:
        plug = gtk.Plug(parent)
        for c in window.get_children():
            window.remove(c)
            plug.add(c)
        window = plug

    window.show()

    if parent:
        from Xlib import display
        from Xlib.xobject import drawable
        d = display.Display()
        w = drawable.Window(d.display, window.window.xid, 0)
        # Honor XEmbed spec
        atom = d.get_atom('_XEMBED_INFO')
        w.change_property(atom, atom, 32, [0, 1])
        w.reparent(parent, 0, 0)
        w.map()
        d.sync()

    # for window resize and or position options
    if "+" in window_geometry:
        try:
            j =  window_geometry.partition("+")
            pos = j[2].partition("+")
            window.move( int(pos[0]), int(pos[2]) )
        except:
            print "**** GLADE VCP ERROR:    With window position data"
            usage()
            sys.exit(1)
    if "x" in window_geometry:
        try:
            if "+" in window_geometry:
                j =  window_geometry.partition("+")
                t = j[0].partition("x")
            else:
                t = window_geometry.partition("x")
            window.resize( int(t[0]), int(t[2]) )
        except:
            print "**** GLADE VCP ERROR:    With window resize data"
            usage()
            sys.exit(1)
    panel = gladevcp.makepins.GladePanel( halcomp, xmlname, builder, buildertype)
    halcomp.ready()
    
    if halfile:
        res = subprocess.call(["halcmd", "-f", halfile])
        if res: raise SystemExit, res

    try:
        try:
            gtk.main()
        except KeyboardInterrupt:
            halcomp.exit()
            sys.exit(0)
    finally:
        halcomp.exit()

if __name__ == '__main__':
    main()
