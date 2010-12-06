#!/usr/bin/env python
# vim: sts=4 sw=4 et
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

    Usage: gladevcp -g position -c compname -H halfile -x windowid myfile.glade
    compname is the name of the HAL component to be created.
    halfile contains hal commands to be executed with halcmd after the hal component is ready
    The name of the HAL pins associated with the VCP will begin with 'compname.'

    myfile.glade is an XML file which specifies the layout of the VCP.

    -g option allows setting of the inital position of the panel
"""
import sys, os, subprocess

import hal
from optparse import Option, OptionParser
import gtk
import gtk.glade
import gobject

import gladevcp.makepins

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-d', action='store_true', dest='debug'
                  , help="Enable debug output")
          , Option( '-g', dest='geometry', default="", help="""Set geometry WIDTHxHEIGHT+XOFFSET+YOFFSET.
Values are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen
use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position""")
          , Option( '-H', dest='halfile', metavar='FILE'
                  , help="execute hal statements from FILE with halcmd after the component is set up and ready")
          , Option( '-x', dest='parent', type=int, metavar='XID'
                  , help="Reparent gladevcp into an existing window XID instead of creating a new top level window")
          , Option( '-u', dest='usermod', action='append', default=[], metavar='FILE'
                  , help='Use FILEs as additional user defined modules with handlers')
          ]


def on_window_destroy(widget, data=None):
        gtk.main_quit()

class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

def main():
    """ creates a HAL component.
        parsees a glade XML file with gtk.builder or libglade
        calls gladevcp.makepins with the specified XML file
        to create pins and register callbacks.
        main window must be called "window1"
    """
    (progdir, progname) = os.path.split(sys.argv[0])

    usage = "usage: %prog [options] myfile.ui"
    parser = OptionParser(usage=usage)
    parser.disable_interspersed_args()
    parser.add_options(options)

    (opts, args) = parser.parse_args()

    if not args:
        parser.print_help()
        sys.exit(1)

    debug = opts.debug
    xmlname = args[0]

    #if there was no component name specified use the xml file name
    if opts.component is None:
        opts.component = os.path.splitext(os.path.basename(xmlname))[0]
    try:
        halcomp = hal.component(opts.component)
    except:
        print "*** GLADE VCP ERROR:    Asking for a HAL component using a name that already exists."
        sys.exit(0)
    #try loading as a libglade project
    try:
        builder = gtk.glade.XML(xmlname)
    except:
        try:
            # try loading as a gtk.builder project
            print "**** GLADE VCP INFO:    Not a libglade project, trying to load as a GTK builder project"
            builder = gtk.Builder()
            builder.add_from_file(xmlname)
        except:
            print "**** GLADE VCP ERROR:    With xml file: %s"% xmlname
            sys.exit(0)
    if not isinstance(builder, gtk.Builder):
            window = builder.get_widget("window1")
    else:
            window = builder.get_object("window1")

    window.connect("destroy", on_window_destroy)
    window.set_title(opts.component)

    handlers = {}

    for u in opts.usermod:
        (directory,filename) = os.path.split(u)
        (basename,extension) = os.path.splitext(filename)
        if directory == '':
            directory = '.'
        if directory not in sys.path:
            sys.path.insert(0,directory)
            if debug: print 'adding import dir %s' % directory

        try:
            m = __import__(basename)
        except ImportError,_msg:
            print "module '%s' skipped - import error: %s" %(basename,_msg)
	    continue
        if debug: print "module '%s' imported OK" % m.__name__
        try:
            for n in dir(m):
                f = getattr(m, n);
                if not hasattr(f, '__call__'):
                    continue
                if n in handlers:
                    handlers[n].append(f)
                else:
                    handlers[n] = [f]
        except Exception,msg:
            print "trouble looking for handlers in '%s': %s" %(basename,msg)

    # XXX: Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    if debug: print "connecting handlers: %s" % handlers.keys()
    if not isinstance(builder, gtk.Builder):
        builder.signal_autoconnect(handlers)
    else:
        builder.connect_signals(handlers)

    if opts.parent:
        plug = gtk.Plug(opts.parent)
        for c in window.get_children():
            window.remove(c)
            plug.add(c)
        window = plug

    window.show()

    if opts.parent:
        from Xlib import display
        from Xlib.xobject import drawable
        d = display.Display()
        w = drawable.Window(d.display, window.window.xid, 0)
        # Honor XEmbed spec
        atom = d.get_atom('_XEMBED_INFO')
        w.change_property(atom, atom, 32, [0, 1])
        w.reparent(opts.parent, 0, 0)
        w.map()
        d.sync()

    # for window resize and or position options
    if "+" in opts.geometry:
        try:
            j =  opts.geometry.partition("+")
            pos = j[2].partition("+")
            window.move( int(pos[0]), int(pos[2]) )
        except:
            print "**** GLADE VCP ERROR:    With window position data"
            parser.print_usage()
            sys.exit(1)
    if "x" in opts.geometry:
        try:
            if "+" in opts.geometry:
                j =  opts.geometry.partition("+")
                t = j[0].partition("x")
            else:
                t = window_geometry.partition("x")
            window.resize( int(t[0]), int(t[2]) )
        except:
            print "**** GLADE VCP ERROR:    With window resize data"
            parser.print_usage()
            sys.exit(1)
    panel = gladevcp.makepins.GladePanel( halcomp, xmlname, builder, None)
    halcomp.ready()
    
    if opts.halfile:
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
