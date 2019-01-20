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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

""" Python / GLADE based Virtual Control Panel for EMC

    A virtual control panel (VCP) is used to display and control
    HAL pins.

    Usage: gladevcp -g position -c compname -H halfile -x windowid myfile.glade
    compname is the name of the HAL component to be created.
    halfile contains hal commands to be executed with halcmd after the hal component is ready
    The name of the HAL pins associated with the VCP will begin with 'compname.'

    myfile.glade is an XML file which specifies the layout of the VCP.

    -g option allows setting of the initial position of the panel
"""
import sys, os, subprocess
import traceback
import warnings

import hal
from optparse import Option, OptionParser
import gtk
import gtk.glade
import gobject
import signal

import gladevcp.makepins
from gladevcp.gladebuilder import GladeBuilder
from gladevcp import xembed
from hal_glib import GStat
GSTAT = GStat()

options = [ Option( '-c', dest='component', metavar='NAME'
                  , help="Set component name to NAME. Default is basename of UI file")
          , Option( '-d', action='store_true', dest='debug'
                  , help="Enable debug output")
          , Option( '-g', dest='geometry', default="", help="""Set geometry WIDTHxHEIGHT+XOFFSET+YOFFSET.
Values are in pixel units, XOFFSET/YOFFSET is referenced from top left of screen
use -g WIDTHxHEIGHT for just setting size or -g +XOFFSET+YOFFSET for just position""")
          , Option( '-H', dest='halfile', metavar='FILE'
                  , help="execute hal statements from FILE with halcmd after the component is set up and ready")
          , Option( '-m', dest='maximum', default=False, help="Force panel window to maxumize")
          , Option( '-r', dest='gtk_rc', default="",
                    help="read custom GTK rc file to set widget style")
          , Option( '-R', dest='gtk_workaround', action='store_false',default=True,
                    help="disable workaround for GTK bug to properly read ~/.gtkrc-2.0 gtkrc files")
          , Option( '-t', dest='theme', default="", help="Set gtk theme. Default is system theme")
          , Option( '-x', dest='parent', type=int, metavar='XID'
                  , help="Reparent gladevcp into an existing window XID instead of creating a new top level window")
          , Option( '--xid', action='store_true', dest='push_XID'
                  , help="reparent window into a plug add push the plug xid number to standardout")
          , Option( '-u', dest='usermod', action='append', default=[], metavar='FILE'
                  , help='Use FILEs as additional user defined modules with handlers')
          , Option( '-U', dest='useropts', action='append', metavar='USEROPT', default=[]
                  , help='pass USEROPTs to Python modules')
          , Option( '--always_above', action='store_true', dest='always_above_flag'
                  , help="Request the window To always be above other windows")
          ]

signal_func = 'on_unix_signal'

gladevcp_debug = 0
def dbg(string):
    global gladevcp_debug
    if not gladevcp_debug: return
    print string

def on_window_destroy(widget, data=None):
        gtk.main_quit()

class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

def load_handlers(usermod,halcomp,builder,useropts):
    hdl_func = 'get_handlers'

    def add_handler(method, f):
        if method in handlers:
            handlers[method].append(f)
        else:
            handlers[method] = [f]

    handlers = {}
    for u in usermod:
        (directory,filename) = os.path.split(u)
        (basename,extension) = os.path.splitext(filename)
        if directory == '':
            directory = '.'
        if directory not in sys.path:
            sys.path.insert(0,directory)
            dbg('adding import dir %s' % directory)

        try:
            mod = __import__(basename)
        except ImportError,msg:
            print "module '%s' skipped - import error: %s" %(basename,msg)
            continue
        dbg("module '%s' imported OK" % mod.__name__)
        try:
            # look for 'get_handlers' function
            h = getattr(mod,hdl_func,None)

            if h and callable(h):
                dbg("module '%s' : '%s' function found" % (mod.__name__,hdl_func))
                objlist = h(halcomp,builder,useropts)
            else:
                # the module has no get_handlers() callable.
                # in this case we permit any callable except class Objects in the module to register as handler
                dbg("module '%s': no '%s' function - registering only functions as callbacks" % (mod.__name__,hdl_func))
                objlist =  [mod]
            # extract callback candidates
            for object in objlist:
                dbg("Registering handlers in module %s object %s" % (mod.__name__, object))
                if isinstance(object, dict):
                    methods = dict.items()
                else:
                    methods = map(lambda n: (n, getattr(object, n, None)), dir(object))
                for method,f in methods:
                    if method.startswith('_'):
                        continue
                    if callable(f):
                        dbg("Register callback '%s' in %s" % (method, object))
                        add_handler(method, f)
        except Exception, e:
            print "gladevcp: trouble looking for handlers in '%s': %s" %(basename, e)
            traceback.print_exc()

    # Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    return handlers

def main():
    """ creates a HAL component.
        parsees a glade XML file with gtk.builder or libglade
        calls gladevcp.makepins with the specified XML file
        to create pins and register callbacks.
        main window must be called "window1"
    """
    global gladevcp_debug
    (progdir, progname) = os.path.split(sys.argv[0])

    usage = "usage: %prog [options] myfile.ui"
    parser = OptionParser(usage=usage)
    parser.disable_interspersed_args()
    parser.add_options(options)

    (opts, args) = parser.parse_args()
    if not args:
        parser.print_help()
        sys.exit(1)

    gladevcp_debug = debug = opts.debug
    xmlname = args[0]

    #if there was no component name specified use the xml file name
    if opts.component is None:
        opts.component = os.path.splitext(os.path.basename(xmlname))[0]

    #try loading as a libglade project
    try:
        builder = gtk.Builder()
        builder.add_from_file(xmlname)
    except:
        try:
            # try loading as a gtk.builder project
            dbg("**** GLADE VCP INFO:    Not a builder project, trying to load as a lib glade project")
            builder = gtk.glade.XML(xmlname)
            builder = GladeBuilder(builder)

        except Exception,e:
            print >> sys.stderr, "**** GLADE VCP ERROR:    With xml file: %s : %s" % (xmlname,e)
            sys.exit(0)

    window = builder.get_object("window1")

    window.set_title(opts.component)

    try:
        halcomp = hal.component(opts.component)
    except:
        print >> sys.stderr, "*** GLADE VCP ERROR:    Asking for a HAL component using a name that already exists."
        sys.exit(0)

    panel = gladevcp.makepins.GladePanel( halcomp, xmlname, builder, None)

    # at this point, any glade HL widgets and their pins are set up.
    handlers = load_handlers(opts.usermod,halcomp,builder,opts.useropts)

    builder.connect_signals(handlers)

    # This option puts the gladevcp panel into a plug and pushed the plug's
    # X window id number to standard output - so it can be reparented exterally
    # it also forwards events to qtvcp
    if opts.push_XID:
        if not opts.debug:
            # supress warnings when x window closes
            warnings.filterwarnings("ignore")
        # block X errors since gdk error handling silently exits the
        # program without even the atexit handler given a chance
        gtk.gdk.error_trap_push()

        window = xembed.add_plug(window)
        window.realize()
        gdkwin = window.get_window()
        w_id = gdkwin.xid
        print >> sys.stdout,w_id
        sys.stdout.flush()
        forward = os.environ.get('QTVCP_FORWARD_EVENTS_TO', None)
        if forward:
            xembed.keyboard_forward(window, forward)

    # This option reparents gladevcp in a given X window id.
    # it also forwards keyboard events from gladevcp to AXIS
    if opts.parent:
        if not opts.debug:
            # supress warnings when x window closes
            warnings.filterwarnings("ignore")
        # block X errors since gdk error handling silently exits the
        # program without even the atexit handler given a chance
        gtk.gdk.error_trap_push()

        window = xembed.reparent(window, opts.parent)

        forward = os.environ.get('AXIS_FORWARD_EVENTS_TO', None)
        if forward:
            xembed.keyboard_forward(window, forward)

    window.connect("destroy", on_window_destroy)
    window.show()

    # for window resize and or position options
    if "+" in opts.geometry:
        try:
            j =  opts.geometry.partition("+")
            pos = j[2].partition("+")
            window.move( int(pos[0]), int(pos[2]) )
        except:
            print >> sys.stderr, "**** GLADE VCP ERROR:    With window position data"
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
            print >> sys.stderr, "**** GLADE VCP ERROR:    With window resize data"
            parser.print_usage()
            sys.exit(1)

    if opts.gtk_workaround:
        # work around https://bugs.launchpad.net/ubuntu/+source/pygtk/+bug/507739
        # this makes widget and widget_class matches in gtkrc and theme files actually work
        dbg( "activating GTK bug workaround for gtkrc files")
        for o in builder.get_objects():
            if isinstance(o, gtk.Widget):
                # retrieving the name works only for GtkBuilder files, not for
                # libglade files, so be cautious about it
                name = gtk.Buildable.get_name(o)
                if name: o.set_name(name)

    if opts.gtk_rc:
        dbg( "**** GLADE VCP INFO: %s reading gtkrc file '%s'" %(opts.component,opts.gtk_rc))
        gtk.rc_add_default_file(opts.gtk_rc)
        gtk.rc_parse(opts.gtk_rc)

    if opts.theme:
        dbg("**** GLADE VCP INFO:    Switching %s to '%s' theme" %(opts.component,opts.theme))
        settings = gtk.settings_get_default()
        settings.set_string_property("gtk-theme-name", opts.theme, "")

    # This needs to be done after geometry moves so on dual screens the window maxumizes to the actual used screen size.
    if opts.maximum:
        window.window.maximize()
    if opts.always_above_flag:
        window.set_keep_above(True)
    if opts.halfile:
        if opts.halfile[-4:] == ".tcl":
            cmd = ["haltcl", opts.halfile]
        else:
            cmd = ["halcmd", "-f", opts.halfile]
        res = subprocess.call(cmd, stdout=sys.stdout, stderr=sys.stderr)
        if res:
            print >> sys.stderr, "'%s' exited with %d" %(' '.join(cmd), res)
            sys.exit(res)

    # User components are set up so report that we are ready
    halcomp.ready()
    GSTAT.forced_update()
    if handlers.has_key(signal_func):
        dbg("Register callback '%s' for SIGINT and SIGTERM" %(signal_func))
        signal.signal(signal.SIGTERM, handlers[signal_func])
        signal.signal(signal.SIGINT,  handlers[signal_func])

    try:
        gtk.main()
    except KeyboardInterrupt:
        sys.exit(0)
    finally:
        halcomp.exit()

    if opts.parent or opts.push_XID:
        gtk.gdk.flush()
        error = gtk.gdk.error_trap_pop()
        if error and opts.debug:
            print >> sys.stderr, "**** GLADE VCP ERROR:    X Protocol Error: %s" % str(error)


if __name__ == '__main__':
    main()
