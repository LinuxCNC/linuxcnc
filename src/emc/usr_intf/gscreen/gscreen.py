#!/usr/bin/python2.4

#    Gscreen a GUI for linuxcnc cnc controller 
#    Chris Morley copyright 2012
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

# Gscreen is made for running linuxcnc CNC machines
# currently only machines with XYZA or less are useable.
# Gscreen was built with touchscreens in mind though a mouse works too.
# a keyboard is necessary for editting gcode
# Gscreen is, at it's heart, a gladevcp program though loaded in a non standard way.
# one can also use a second monitor to display a second glade panel
# this would probably be most useful for user's custom status widgets.
# you would need to calibrate your touchscreen to just work on a single screen
 
import pygtk
pygtk.require("2.0")
import gtk
import gtk.glade
import gobject
import hal
import sys,os
from optparse import Option, OptionParser
import gladevcp.makepins
from gladevcp.gladebuilder import GladeBuilder
import pango
import traceback
import atexit
import vte
import time
import hal_glib

# try to add a notify system so messages use the
# nice intergrated pop-ups
# Ubuntu kinda wrecks this be not following the
# standard - you can't set how long the message stays up for.
# I suggest fixing this with a PPA off the net
# https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
try:
    NOTIFY_AVAILABLE = False
    import pynotify
    if not pynotify.init("Gscreen"):
        print "**** GSCREEN INFO: There was a problem initializing the pynotify module"
    else:
        NOTIFY_AVAILABLE = True
except:
    print "**** GSCREEN INFO: You don't seem to have pynotify installed"

# try to add ability for audio feedback to user.
try:
    _AUDIO_AVAIALBLE = False
    import pygst
    pygst.require("0.10")
    import gst
    _AUDIO_AVAIALBLE = True
    print "**** GSCREEN INFO: audio available!"
except:
    print "**** GSCREEN INFO: no audio alerts available - PYGST libray not installed?"

# BASE is the absolute path to linuxcnc base
# libdir is the path to Gscreen python files
# datadir is where the standarad GLADE files are
# imagedir is for icons
# themesdir is path to system's GTK2 theme folder
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python")
datadir = os.path.join(BASE, "share", "linuxcnc")
imagedir = os.path.join(BASE, "share","gscreen","images")
sys.path.insert(0, libdir)
themedir = "/usr/share/themes"

xmlname = os.path.join(datadir,"gscreen.glade")
xmlname2 = os.path.join(datadir,"gscreen2.glade")
ALERT_ICON = os.path.join(imagedir,"applet-critical.png")
INFO_ICON = os.path.join(imagedir,"std_info.gif")

# internationalization and localization
import gettext
LOCALEDIR = os.path.join(BASE, "share", "locale")
gettext.install("linuxcnc", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("linuxcnc", LOCALEDIR)
gtk.glade.textdomain("linuxcnc")

# path to TCL for external programs eg. halshow
TCLPATH = os.environ['LINUXCNC_TCL_DIR']
# path to the configuration the user requested
# used to see if the is local GLADE files to use
CONFIGPATH = os.environ['CONFIG_DIR']
import linuxcnc
from gscreen import emc_interface
from gscreen import mdi
from gscreen import preferences

# this is for hiding the pointer when using a touch screen
pixmap = gtk.gdk.Pixmap(None, 1, 1, 1)
color = gtk.gdk.Color()
INVISABLE = gtk.gdk.Cursor(pixmap, pixmap, color, color, 0, 0)

# print debug messages if debug is true
gscreen_debug = False
def dbg(str):
    global gscreen_debug
    if not gscreen_debug: return
    print str

# Throws up a dialog with debug info when an error is encountered 
def excepthook(exc_type, exc_obj, exc_tb):
    try:
        w = app.widgets.window1
    except KeyboardInterrupt:
        sys.exit(0)
    except NameError:
        w = None
    lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
    m = gtk.MessageDialog(w,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_ERROR, gtk.BUTTONS_OK,
                ("Gscreen encountered an error.  The following "
                "information may be useful in troubleshooting:\n\n")
                + "".join(lines))
    m.show()
    m.run()
    m.destroy()
sys.excepthook = excepthook

# constants
X = 0;Y = 1;Z = 2;A = 3;B = 4;C = 5;U = 6;V = 7;W = 8
_ABS = 0;_REL = 1;_DTG = 2
_MAN = 0;_MDI = 1;_AUTO = 2
_MM = 1;_IMPERIAL = 0
_SPINDLE_INPUT = 1;_PERCENT_INPUT = 2;_VELOCITY_INPUT = 3

# the player class does the work of playing the audio hints
# http://pygstdocs.berlios.de/pygst-tutorial/introduction.html
class Player:
    def __init__(self):
        #Element playbin automatic plays any file
        self.player = gst.element_factory_make("playbin", "player")
        #Enable message bus to check for errors in the pipeline
        bus = self.player.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_message)
        self.loop = gobject.MainLoop()

    def run(self):
        self.player.set_state(gst.STATE_PLAYING)
        self.loop.run()

    def set_sound(self,file):
        #Set the uri to the file
        self.player.set_property("uri", "file://" + file)

    def on_message(self, bus, message):
        t = message.type
        if t == gst.MESSAGE_EOS:
            #file ended, stop
            self.player.set_state(gst.STATE_NULL)
            self.loop.quit()
        elif t == gst.MESSAGE_ERROR:
            #Error ocurred, print and stop
            self.player.set_state(gst.STATE_NULL)
            err, debug = message.parse_error()
            print "Error: %s" % err, debug
            self.loop.quit()

# a class for holding the glade widgets rather then searching for them each time
class Widgets:
    def __init__(self, xml):
        self._xml = xml
    def __getattr__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise AttributeError, "No widget %r" % attr
        return r
    def __getitem__(self, attr):
        r = self._xml.get_object(attr)
        if r is None: raise IndexError, "No widget %r" % attr
        return r

# a class for holding data
# here we intialize the data
class Data:
    def __init__(self):
        self.use_screen2 = False
        self.theme_name = "Follow System Theme"
        self.abs_textcolor = ""
        self.rel_textcolor = ""
        self.dtg_textcolor = ""
        self.err_textcolor = ""
        self.window_geometry = ""
        self.window_max = ""
        self.axis_list = []
        self.active_axis_buttons = [(None,None)] # axis letter,axis number
        self.abs_color = (0, 65535, 0)
        self.rel_color = (65535, 0, 0)
        self.dtg_color = (0, 0, 65535)
        self.highlight_color = (65535,65535,65535)
        self.highlight_major = False
        self.display_order = (_REL,_DTG,_ABS)
        self.mode_order = (_MAN,_MDI,_AUTO)
        self.plot_view = ("p","x","y","y2","z","z2")
        self.task_mode = 0
        self.active_gcodes = []
        self.active_mcodes = []
        self.x_abs = 0.0
        self.x_rel = 1.0
        self.x_dtg = -2.0
        self.y_abs = 0.0
        self.y_rel = 100.0
        self.y_dtg = 2.0
        self.z_abs = 0.0
        self.z_rel = 1.0
        self.z_dtg = 21.0
        self.a_abs = 0.0
        self.a_rel = 1.0
        self.a_dtg = 2.0
        self.x_is_homed = 0
        self.y_is_homed = 0
        self.z_is_homed = 0
        self.a_is_homed = 0
        self.spindle_request_rpm = 0
        self.spindle_dir = 0
        self.spindle_speed = 0
        self.spindle_start_rpm = 300
        self.spindle_preset = 300
        self.active_spindle_command = "" # spindle command setting
        self.active_feed_command = "" # feed command setting
        self.system = 1
        self.estopped = True
        self.dro_units = _IMPERIAL
        self.machine_units = _IMPERIAL
        self.tool_in_spindle = 0
        self.flood = False
        self.mist = False
        self.machine_on = False
        self.or_limits = False
        self.op_stop = False
        self.block_del = False
        self.all_homed = False
        self.jog_rate = 15
        self.jog_rate_inc = 1
        self.jog_rate_max = 60
        self.jog_increments = []
        self.current_jogincr_index = 0
        self.feed_override = 1.0
        self.feed_override_inc = .05
        self.feed_override_max = 2.0
        self.spindle_override = 1.0
        self.spindle_override_inc = .05
        self.spindle_override_max = 1.2
        self.spindle_override_min = .50
        self.maxvelocity = 1
        self.velocity_override = 1.0
        self.velocity_override_inc = .05
        self.edit_mode = False
        self.full_graphics = False
        self.graphic_move_inc = 20
        self.file = ""
        self.file_lines = 0
        self.line = 0
        self.id = 0
        self.dtg = 0.0
        self.velocity = 0.0
        self.delay = 0.0
        self.preppedtool = None
        self.lathe_mode = False
        self.diameter_mode = True
        self.tooleditor = ""
        self.tooltable = ""
        self.alert_sound = "/usr/share/sounds/ubuntu/stereo/bell.ogg"         
        self.error_sound  = "/usr/share/sounds/ubuntu/stereo/dialog-question.ogg"

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# trampoline and load_handlers are used for custom displays
class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

def load_handlers(usermod,halcomp,builder,useropts,gscreen):
    hdl_func = 'get_handlers'
    mod = None
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
            print ('adding import dir %s' % directory)

        try:
            mod = __import__(basename)
        except ImportError,msg:
            print "module '%s' skipped - import error: %s" %(basename,msg)
	    continue
        print("module '%s' imported OK" % mod.__name__)
        try:
            # look for functions
            for temp in ("periodic","connect_signals","initialize_widgets"):
                h = getattr(mod,temp,None)
                if h and callable(h):
                    print("module '%s' : '%s' function found" % (mod.__name__,temp))

            # look for 'get_handlers' function
            h = getattr(mod,hdl_func,None)
            if h and callable(h):
                print("module '%s' : '%s' function found" % (mod.__name__,hdl_func))
                objlist = h(halcomp,builder,useropts,gscreen)
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
                        dbg("Register callback '%s' in %s" % (method, basename))
                        add_handler(method, f)
        except Exception, e:
            print "**** GSCREEN ERROR: trouble looking for handlers in '%s': %s" %(basename, e)
            traceback.print_exc()

    # Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    return handlers,mod

# ok here is the Gscreen class
# there are also three other files:
# mdi.py for mdi commands (which include non-obvious mdi commands done in maual mode)
# preference.py for keeping track of stored user preferences
# emc_interface.py which does most of the commands and status of linuxcnc
# keep in mind some of the gladeVCP widgets send-commands-to/monitor linuxcnc also

class Gscreen: 

    def __init__(self):
        global xmlname
        global xmlname2
        global gscreen_debug
        skinname = "gscreen"
        self.inipath = sys.argv[2]
        (progdir, progname) = os.path.split(sys.argv[0])

        # linuxcnc adds -ini to display name and optparse
        # can't understand that, so we do it manually 
        for num,temp in enumerate(sys.argv):
            if temp == '-c':
                try:
                    print "component =",sys.argv[num+1]
                    skinname = sys.argv[num+1]
                except:
                    pass
            if temp == '-d': gscreen_debug = True

        # main screen
        localglade = os.path.join(CONFIGPATH,"%s.glade"%skinname)
        if os.path.exists(localglade):
            print "\n**** GSCREEN INFO:  Using LOCAL glade file from %s ****"% localglade
            xmlname = localglade
        else:
            print"\n**** GSCREEN INFO:  using STOCK glade file from: %s ****"% xmlname
        try:
            self.xml = gtk.Builder()
            self.xml.add_from_file(xmlname)
        except:
            print "**** Gscreen GLADE ERROR:    With main screen xml file: %s"% xmlname
            sys.exit(0)
        # second screen
        localglade = os.path.join(CONFIGPATH,"%s2.glade"%skinname)
        if os.path.exists(localglade):
            print "\n**** GSCREEN INFO:  Using LOCAL glade file from %s ****"% localglade
            xmlname2 = localglade
        else:
            print"\n**** GSCREEN INFO:  using STOCK glade file from: %s ****"% xmlname2
        try:
            self.xml.add_from_file(xmlname2)
            self.screen2 = True
        except:
            print "**** Gscreen GLADE ERROR:    With screen 2's xml file: %s"% xmlname
            self.screen2 = False
        self.widgets = Widgets(self.xml)
        self.data = Data()
        if _AUDIO_AVAIALBLE:
            self.audio = Player()         

        # access to saved prefernces
        self.prefs = preferences.preferences()
        # access to EMC control
        self.emc = emc_interface.emc_control(linuxcnc, self.widgets.statusbar1)
        # access to EMC status
        self.status = emc_interface.emc_status( self.data, linuxcnc)
        # access to MDI
        mdi_labels = mdi_eventboxes = []
        self.mdi_control = mdi.mdi_control(gtk, linuxcnc, mdi_labels, mdi_eventboxes)
        # pull info from the INI file
        self.inifile = self.emc.emc.ini(self.inipath)
        # change the display based on the requested axis
        temp = self.inifile.find("TRAJ","COORDINATES")
        self.data.axis_list = []
        for letter in temp:
            if letter.lower() in self.data.axis_list: continue
            if not letter.lower() in ["x","y","z","a","b","c","u","v","w"]: continue
            self.data.axis_list.append(letter.lower())

        #setup default stuff
        self.data.hide_cursor = self.prefs.getpref('hide_cursor', False, bool)

        self.data.theme_name = self.prefs.getpref('gtk_theme', 'Redmond', str)
        self.data.abs_textcolor = self.prefs.getpref('abs_textcolor', '#0000FFFF0000', str)

        self.data.rel_textcolor = self.prefs.getpref('rel_textcolor', '#FFFF00000000', str)

        self.data.dtg_textcolor = self.prefs.getpref('dtg_textcolor', '#00000000FFFF', str)

        self.data.err_textcolor = self.prefs.getpref('err_textcolor', 'default', str)
        self.data.error_font_name = self.prefs.getpref('error_font', 'Sans Bold 10', str)
        self.data.window_geometry = self.prefs.getpref('window_geometry', 'default', str)
        self.data.window_max = self.prefs.getpref('window_force_max', False, bool)
        self.data.window2_geometry = self.prefs.getpref('window2_geometry', 'default', str)
        self.data.window2_max = self.prefs.getpref('window2_force_max', False, bool)
        self.data.use_screen2 = self.prefs.getpref('use_screen2', False, bool)
        self.data.grid_size = self.prefs.getpref('grid_size', 1.0 , float)
        self.data.spindle_start_rpm = self.prefs.getpref('spindle_start_rpm', 300 , float)
        self.data.diameter_mode = self.prefs.getpref('diameter_mode', False, bool)

        self.data.dro_units = self.prefs.getpref('units', False, bool)

        self.data.display_order = self.prefs.getpref('display_order', (0,1,2), repr)
        self.data.plot_view = self.prefs.getpref('view', ("p","x","y","y2","z","z2"), repr) 
        self.data.alert_sound = self.prefs.getpref('audio_alert', self.data.alert_sound, str)

        self.data.error_sound = self.prefs.getpref('audio_error', self.data.error_sound, str)
        # get the system wide theme
        settings = gtk.settings_get_default()
        settings.props.gtk_button_images = True
        self.data.system_theme = settings.get_property("gtk-theme-name")

        # jogging increments
        increments = self.inifile.find("DISPLAY", "INCREMENTS")
        if increments:
            if "," in increments:
                self.data.jog_increments = [i.strip() for i in increments.split(",")]
            else:
                self.data.jog_increments = increments.split()
        self.data.jog_increments.insert(0,"Continuous")

        # max velocity settings: more then one place to check
        temp = self.inifile.find("TRAJ","MAX_LINEAR_VELOCITY")
        if temp == None:
            temp = self.inifile.find("TRAJ","MAX_VELOCITY")
            if temp == None:
                temp = 1.0
        self.data._maxvelocity = float(temp)

        # check for override settings
        temp = self.inifile.find("DISPLAY","MAX_SPINDLE_OVERRIDE")
        if temp:
            self.data.spindle_override_max = float(temp)
        temp = self.inifile.find("DISPLAY","MIN_SPINDLE_OVERRIDE")
        if temp:
            self.data.spindle_override_min = float(temp)
        temp = self.inifile.find("DISPLAY","MAX_FEED_OVERRIDE")
        if temp:
            self.data.feed_override_max = float(temp)

        # check the ini file if UNITS are set to mm"
        # first check the global settings
        units=self.inifile.find("TRAJ","LINEAR_UNITS")
        if units==None:
            # else then the X axis units
            units=self.inifile.find("AXIS_0","UNITS")
        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            conversion=[25.4]*3+[1]*3+[25.4]*3
        self.status.set_machine_units(self.machine_units_mm,conversion)

        # if it's a lathe config, set the tooleditor style 
        self.data.lathe_mode = bool(self.inifile.find("DISPLAY", "LATHE"))

        # get the path to the tool table
        self.data.tooltable = self.inifile.find("EMCIO","TOOL_TABLE")

        # see if the user specified a tool editor
        self.data.tooleditor = self.inifile.find("DISPLAY","TOOL_EDITOR")

        # toolsetting reference type
        if self.prefs.getpref('toolsetting_fixture', False):
            self.g10l11 = 1
        else:
            self.g10l11 = 0

        # set the display options from preference file
        if self.prefs.getpref('dro_mm', False):
            self.status.dro_mm(0)
        else:
            self.status.dro_inch(0)

        if self.prefs.getpref('dro_actual', False):
           self.status.dro_actual(0)
        else:
           self.status.dro_commanded(0)

        # set default jog rate
        self.emc.continuous_jog_velocity(self.data.jog_rate)

        # set-up HAL component
        try:
            self.halcomp = hal.component(skinname)
            self.halcomp.newpin("aux-coolant-m7.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("aux-coolant-m8.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("mist-coolant.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("flood-coolant.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("jog-enable.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("jog-increment.out", hal.HAL_FLOAT, hal.HAL_OUT)
            self.halcomp.newpin("spindle-readout.in", hal.HAL_FLOAT, hal.HAL_IN)
            for axis in self.data.axis_list:
                self.halcomp.newpin("jog-enable-%s.out"% (axis), hal.HAL_BIT, hal.HAL_OUT)
        except:
            print "*** Gscreen ERROR:    Asking for a HAL component using a name that already exists."
            sys.exit(0)
    
        panel = gladevcp.makepins.GladePanel( self.halcomp, xmlname, self.xml, None)
        # at this point, any glade HAL widgets and their pins are set up.

        # look for custom handler files:
        HANDLER_FN = "%s_handler.py"%skinname
        local_handler_path = os.path.join(CONFIGPATH,HANDLER_FN)
        if os.path.exists(local_handler_path):
            temp = [HANDLER_FN]
        else:
            temp = []
        handlers,module = load_handlers(temp,self.halcomp,self.xml,[],self)
        self.xml.connect_signals(handlers)
        self.custom_handler = module
        # TODO the user should be able to invoke this so they know what methods are available
        # and what handers are registered
        #print handlers
        if "connect_signals" in dir(self.custom_handler):
            self.custom_handler.connect_signals(self)
        else:
            self.install_signals(handlers)


        # dynamic tabs setup
        self._dynamic_childs = {}
        atexit.register(self.kill_dynamic_childs)
        self.set_dynamic_tabs()

        # set title and display everything including the second screen if requested
        if skinname == "gscreen":
            title = "Gscreen"
        else:
            title = "Gscreen-%s"% skinname
        self.widgets.window1.set_title("%s for linuxcnc"% title)
        if self.screen2:
            self.widgets.window2.show()
            self.widgets.window2.move(0,0)
            if not self.data.use_screen2:
                self.widgets.window2.hide()
        self.widgets.window1.show()

        # Set up the widgets
        if "initialize_widgets" in dir(self.custom_handler):
            self.custom_handler.initialize_widgets(self)
        else:
            self.initialize_widgets()

        # see if there are user messages in the ini file 
        self.message_setup()

        # ok everything that might make HAL pins should be done now - let HAL know that
        self.halcomp.ready()
        try:
            self.widgets._terminal.feed_child('halcmd show pin gscreen\n')
        except:
            pass
        # timers for display updates
        gobject.timeout_add(50, self.periodic_status)

# intitialize #
    def initialize_widgets(self):
        temp = self.data.axis_list
        if "a" in temp:
            self.widgets.frame3.show()
            self.widgets.image6.hide() # make more room for axis display
        if "y" in temp:
            self.widgets.frame2.show()
        self.widgets.hide_cursor.set_active(self.data.hide_cursor)

        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.data.abs_textcolor))
        self.set_abs_color()
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.data.rel_textcolor))
        self.set_rel_color()
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.data.dtg_textcolor))
        self.set_dtg_color()

        self.widgets.use_screen2.set_active(self.data.use_screen2)
        self.widgets.fullscreen1.set_active( self.prefs.getpref('fullscreen1', False, bool) )
        self.widgets.show_offsets.set_active( self.prefs.getpref('show_offsets', True, bool) )
        self.widgets.gremlin.show_offsets = self.widgets.show_offsets.get_active()
        self.widgets.grid_size.set_value(self.data.grid_size) 
        self.widgets.gremlin.grid_size = self.data.grid_size
        self.widgets.spindle_start_rpm.set_value(self.data.spindle_start_rpm)
        self.block("s_display_fwd")
        self.widgets.s_display_fwd.set_active(True)
        self.unblock("s_display_fwd")
        self.widgets.diameter_mode.set_active(self.data.diameter_mode)
        self.widgets.dro_units.set_active(self.data.dro_units)
        self.widgets.audio_alert_chooser.set_filename(self.data.alert_sound)
        self.widgets.audio_error_chooser.set_filename(self.data.error_sound)
        self.statusbar_id = self.widgets.statusbar1.get_context_id("Statusbar1")
        self.homed_status_message = self.widgets.statusbar1.push(1,"Ready For Homing")
        self.widgets.data_input.set_value(5.125)
        pangoFont = pango.FontDescription("Tahoma 18")
        self.widgets.data_input.modify_font(pangoFont)
        label = ["Manual","MDI","Auto"]
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.preset_spindle_speed(self.data.spindle_start_rpm)
        if self.data.lathe_mode:
            self.widgets.tooledit1.set_display(0)
        path = os.path.join(CONFIGPATH,self.data.tooltable)
        self.widgets.tooledit1.set_filename(path)
        # add terminal window
        self.widgets._terminal = vte.Terminal ()
        self.widgets._terminal.connect ("child-exited", lambda term: gtk.main_quit())
        self.widgets._terminal.fork_command()
        self.widgets._terminal.show()
        window = self.widgets.terminal_window.add(self.widgets._terminal)
        self.widgets.terminal_window.connect('delete-event', lambda window, event: gtk.main_quit())
        self.widgets.terminal_window.show()

        # If there are themes then add them to combo box
        if os.path.exists(themedir):
            model = self.widgets.theme_choice.get_model()
            model.clear()
            model.append(("Follow System Theme",))
            temp = 0
            names = os.listdir(themedir)
            names.sort()
            for search,dirs in enumerate(names):
                model.append((dirs,))
                if dirs  == self.data.theme_name:
                    temp = search+1
            self.widgets.theme_choice.set_active(temp)
        settings = gtk.settings_get_default()
        if not self.data.theme_name == "Follow System Theme":
            settings.set_string_property("gtk-theme-name", self.data.theme_name, "")
        # maximize window or set geometry and optionally maximize 
        if "max" in self.data.window_geometry:
		    self.widgets.window1.maximize()
        elif self.data.window_geometry == "default":
            pass
        else:
            good = self.widgets.window1.parse_geometry(self.data.window_geometry)
            if self.data.window_max:
               self.widgets.window1.maximize()
            if not good:
                print "**** WARNING GSCREEN: could not understand the window geometry info in hidden preference file"
        if self.widgets.fullscreen1.get_active():
            self.widgets.window1.fullscreen()


        self.widgets.button_h3_1.set_active(self.prefs.getpref('blockdel', False))
        self.emc.blockdel(self.data.block_del)

        self.widgets.button_h3_2.set_active(self.prefs.getpref('opstop', False))
        self.emc.opstop(self.data.op_stop)
        self.on_hal_status_state_off(None)
        # hide cursor if requested
        # that also sets the graphics to use touchscreen controls
        if self.data.hide_cursor:
            self.widgets.window1.window.set_cursor(INVISABLE)
            self.widgets.gremlin.set_property('use_default_controls',False)
        else:
            self.widgets.window1.window.set_cursor(None)
            self.widgets.gremlin.set_property('use_default_controls',True)
        self.update_position()

        self.widgets.gremlin.set_property('view',self.data.plot_view[0])
        self.widgets.gremlin.set_property('metric_units',(self.data.dro_units == _MM))

        # set to 'manual mode' 
        self.mode_changed(self.data.mode_order[0])

# *** GLADE callbacks ****

    def on_spindle_speed_adjust(self,widget):
                # spindle increase /decrease controls
        if self.mdi_control.mdi_is_reading():
            self.notify("INFO:","Can't start spindle manually while MDI busy ",INFO_ICON)
            return
        elif self.data.mode_order[0] == _AUTO:
            self.notify("INFO:","can't start spindle manually in Auto mode",INFO_ICON)
            return
        if widget == self.widgets.spindle_increase:
            self.spindle_adjustment(True,True)
        elif widget == self.widgets.spindle_decrease:
            self.spindle_adjustment(False,True)

    # start the spindle according to preset rpm and direction buttons, unless interp is busy
    def on_spindle_control_clicked(self,*args):
        if self.mdi_control.mdi_is_reading():
            self.notify("INFO:","Can't start spindle manually while MDI busy ",INFO_ICON)
            return
        elif self.data.mode_order[0] == _AUTO:
            self.notify("INFO:","can't start spindle manually in Auto mode",INFO_ICON)
            return
        if not self.data.spindle_speed == 0:
            self.emc.spindle_off(1)
            return
        if not self.widgets.s_display_fwd.get_active() and not self.widgets.s_display_rev.get_active():
            self.notify("INFO:","No direction selected for spindle",INFO_ICON)
            return
        if self.widgets.s_display_fwd.get_active():
            self.adjust_spindle_rpm(self.data.spindle_preset,1)
        else:
            self.adjust_spindle_rpm(self.data.spindle_preset,-1)

    def on_preset_spindle(self,*args):
        self.preset_spindle_speed(self.get_qualified_input(_SPINDLE_INPUT))

    def set_grid_size(self,widget):
        data = widget.get_value()
        self.widgets.gremlin.set_property('grid_size',data)
        self.prefs.putpref('grid_size', data,float)

    # from prefererence page
    def set_spindle_start_rpm(self,widget):
        data = widget.get_value()
        self.data.spindle_start_rpm = data
        self.prefs.putpref('spindle_start_rpm', data,float)
        self.preset_spindle_speed(data)

    def update_preview(self,widget):
        file = widget.get_filename()
        if file:
            try:
                test = Player()
                test.set_sound(file)
                test.run()
            except:pass

    def change_sound(self,widget,sound):
        file = widget.get_filename()
        if file:
            self.data[sound+"_sound"] = file
            temp = "audio_"+ sound
            self.prefs.putpref(temp, file, str)

    # manual spindle control
    def on_s_display_fwd_toggled(self,widget):
        if widget.get_active():
            if self.widgets.s_display_fwd.get_active():
                self.emc.spindle_off(1)
                self.block("s_display_rev")
                self.widgets.s_display_rev.set_active(False)
                self.unblock("s_display_rev")
        else:
            self.block("s_display_fwd")
            widget.set_active(True)
            self.unblock("s_display_fwd")
 
    # manual spindle control
    def on_s_display_rev_toggled(self,widget):
        if widget.get_active():
            if self.widgets.s_display_fwd.get_active():
                self.emc.spindle_off(1)
                self.block("s_display_fwd")
                self.widgets.s_display_fwd.set_active(False)
                self.unblock("s_display_fwd")
        else:
            self.block("s_display_rev")
            widget.set_active(True)
            self.unblock("s_display_rev")

    # for plot view controls with touchscreen
    def on_eventbox_gremlin_enter_notify_event(self,widget,event):
        if self.widgets.button_graphics.get_active():
            if self.widgets.button_h5_0.get_active():
                self.widgets.gremlin.start_continuous_zoom(event.y)
            elif self.widgets.button_h5_6.get_active():
                self.widgets.gremlin.select_prime(event.x,event.y)
            self.widgets.gremlin.set_mouse_start(event.x,event.y)

    # for plot view controls with touchscreen
    def on_eventbox_gremlin_leave_notify_event(self,widget,event):
        self.widgets.gremlin.select_fire(event.x,event.y)

    # for plot view controls with touchscreen or mouse
    # if using mouse and when in graphics adjustment mode,
    # we don't use mouse controls when we have selected button controls
    def on_gremlin_motion(self,widget,event):
        if self.widgets.button_graphics.get_active():
            self.widgets.gremlin.set_property('use_default_controls',False)
            if self.data.hide_cursor:
                if self.widgets.button_h5_0.get_active():
                    self.widgets.gremlin.continuous_zoom(event.y)
                elif self.widgets.button_h5_4.get_active():
                    self.pan(event.x,event.y)
                elif self.widgets.button_h5_5.get_active():
                    self.pan(event.x,event.y)
                elif self.widgets.button_h5_6.get_active():
                    self.rotate(event.x,event.y)
                elif self.widgets.button_h5_7.get_active():
                    self.rotate(event.x,event.y)
            elif self.widgets.button_h5_0.get_active() or self.widgets.button_h5_4.get_active():
                return
            elif self.widgets.button_h5_5.get_active() or self.widgets.button_h5_6.get_active():
                return
            elif self.widgets.button_h5_7.get_active():
                return
            else:
                self.widgets.gremlin.set_property('use_default_controls',True)
        else:
            self.widgets.gremlin.set_property('use_default_controls',not self.data.hide_cursor)

    # display calculator for input
    def launch_numerical_input(self,widget,event):
        if (event.type == gtk.gdk._2BUTTON_PRESS and not self.data.hide_cursor) or \
        (event.type == gtk.gdk.BUTTON_PRESS and self.data.hide_cursor):
            dialog = self.widgets.dialog_entry
            self.widgets.calc_entry.set_value(self.widgets.data_input.get_value())
            dialog.show_all()
            self.widgets.data_input.set_sensitive(False)

    # calculator input accepted
    def on_button_yes_clicked(self,widget):
        self.widgets.data_input.set_value(self.widgets.calc_entry.get_value())
        self.widgets.data_input.set_sensitive(True)
        self.widgets.dialog_entry.hide()

    # calculator input canceled
    def on_button_no_clicked(self,widget):
        self.widgets.data_input.set_sensitive(True)
        self.widgets.dialog_entry.hide()

    # shows the cursor and warps it to the origin before exiting
    def hack_leave(self,*args):
        if not self.data.hide_cursor: return
        w = self.widgets.window1.window
        d = w.get_display()
        s = w.get_screen()
        x, y = w.get_origin()
        d.warp_pointer(s, x, y)

    def on_hide_cursor(self,*args):
        print "hide cursor change"
        if self.widgets.hide_cursor.get_active():
            self.prefs.putpref('hide_cursor', True)
            self.data.hide_cursor = True
            self.widgets.window1.window.set_cursor(INVISABLE)
        else:
            self.prefs.putpref('hide_cursor', False)
            self.data.hide_cursor = False
            self.widgets.window1.window.set_cursor(None)

    # opens halshow
    def on_halshow(self,*args):
        print "halshow",TCLPATH
        p = os.popen("tclsh %s/bin/halshow.tcl -- -ini %s &" % (TCLPATH,self.inipath))

    # opens the calibration program
    def on_calibration(self,*args):
        print "calibration --%s"% self.inipath
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH,self.inipath),"w")

    # opens the linuxcnc status program
    def on_status(self,*args):
        p = os.popen("linuxcnctop  > /dev/null &","w")

    # opens a halmeter
    def on_halmeter(self,*args):
        print "halmeter"
        p = os.popen("halmeter &")

    # estop machine before closing
    def on_window1_destroy(self, widget, data=None):
        print "estopping / killing gscreen"
        self.emc.machine_off(1)
        self.emc.estop(1)
        time.sleep(2)
    	gtk.main_quit()

    def on_axis_selection_clicked(self,widget):
        self.update_active_axis_buttons(widget)

    def on_mode_clicked(self,widget,event):
        # only change machine modes on click
        if event.type == gtk.gdk.BUTTON_PRESS:
            a,b,c = self.data.mode_order
            self.data.mode_order = b,c,a
            label = ["Manual","MDI","Auto"]
            self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
            self.mode_changed(self.data.mode_order[0])

    def on_gremlin_clicked(self,widget,event):
        # only change machine modes on double click
        button1 = event.button == 1
        button2 = event.button == 2
        button3 = event.button == 3
        if button1 and event.type == gtk.gdk._2BUTTON_PRESS:
            temp = self.widgets.dro_frame.get_visible()
            temp = (temp * -1) +1
            self.widgets.dro_frame.set_visible(temp)
            self.widgets.gremlin.set_property('enable_dro',(not temp))

    def on_hbutton_clicked(self,widget,mode,number):
            if mode == 0:
                if number == 0: self.toggle_ignore_limits()
                elif number == 2: self.home_all()
                elif number == 3: self.home_selected()
                elif number == 4: self.unhome_all()
                elif number == 5: self.dro_toggle()
                else: print "hbutton %d_%d clicked but no function"% (mode,number)
            elif mode == 1:
                if number == 0: self.jog_mode()
                elif number == 1: self.origin_system()
                elif number == 2: self.toggle_mist()
                elif number == 3: self.toggle_flood()
                elif number == 4: self.reload_tooltable()
                elif number == 5: self.dro_toggle()
                else: print "hbutton %d_%d clicked but no function"% (mode,number)
            elif mode == 3:
                if number == 1: self.toggle_block_delete()
                elif number == 2: self.toggle_optional_stop()
                elif number == 7: self.next_tab()
                else: print "hbutton %d_%d clicked but no function"% (mode,number)
            elif mode == 4:
                self.toggle_overrides(widget,mode,number)
            elif mode == 5:
                if number == 1:pass
                elif number == 2:self.toggle_view()
                elif number == 3:self.clear_plot()
                else: self.toggle_graphic_overrides(widget,mode,number)
            else: print "hbutton %d_%d clicked but no function"% (mode,number)

    def on_vbutton_clicked(self,widget,mode,number):
        if mode == 0:
            if number == 0: self.adjustment_buttons(widget,True)
            elif number == 1: self.adjustment_buttons(widget,True)
            elif number == 2: pass # using press and release signals
            elif number == 3: pass # ditto
            elif number == 4: pass
            elif number == 5: self.adjustment_buttons(widget,True)
            else: print "Vbutton %d_%d clicked but no function"% (mode,number)
        elif mode == 1:
            if number == 0: pass
            elif number == 1: pass
            elif number == 2: pass
            elif number == 3: pass
            elif number == 4: pass
            elif number == 5: self.toggle_view()
            elif number == 6: self.full_graphics()
            elif number == 7: self.edit_mode()
            else: print "Vbutton %d_%d clicked but no function"% (mode,number)


    def on_button_v0_2_pressed(self,widget):
        self.adjustment_buttons(widget,True)
    def on_button_v0_2_released(self,widget):
        self.adjustment_buttons(widget,False)
    def on_button_v0_3_pressed(self,widget):
        self.adjustment_buttons(widget,True)
    def on_button_v0_3_released(self,widget):
        self.adjustment_buttons(widget,False)

    def on_mode_select_clicked(self,widget,event):
        maxpage = self.widgets.notebook_main.get_n_pages()
        page = self.widgets.notebook_main.get_current_page()
        nextpage = page + 1
        print "mode select",maxpage,page,nextpage
        if nextpage == maxpage:nextpage = 0
        self.widgets.notebook_main.set_current_page(nextpage)

    def on_estop_clicked(self,*args):
        if self.data.estopped:
            self.emc.estop_reset(1)
        elif not self.data.machine_on:
            self.emc.machine_on(1)
            self.widgets.on_label.set_text("Machine On")
        else:
            self.emc.machine_off(1)
            self.emc.estop(1)
            self.widgets.on_label.set_text("Machine Off")


    def on_theme_choice_changed(self, widget):
        self.change_theme(widget.get_active_text())

    # True is fullscreen
    def on_fullscreen1_pressed(self, widget):
        self.set_fullscreen1(widget.get_active())

    def on_use_screen2_pressed(self,*args):
        self.toggle_screen2()

    # True is metric
    def on_dro_units_pressed(self, widget):
        self.set_dro_units(widget.get_active())

    # True is diameter mode
    def on_diameter_mode_pressed(self, widget):
        self.set_diameter_mode(widget.get_active())

    def on_rel_colorbutton_color_set(self,*args):
        self.set_rel_color()

    def on_abs_colorbutton_color_set(self,*args):
        self.set_abs_color()

    def on_dtg_colorbutton_color_set(self,*args):
        self.set_dtg_color()

    # True for showing full offsets
    def on_show_offsets_pressed(self, widget):
        self.set_show_offsets(widget.get_active())

    # True is for showing DTG
    def on_show_dtg_pressed(self, widget):
        self.set_show_dtg(widget.get_active())

    def on_pop_statusbar_clicked(self, *args):
        self.widgets.statusbar1.pop(self.statusbar_id)
    
    def on_printmessage(self, pin, pinname,boldtext,text,type):
        if pin.get():
            if boldtext == "NONE": boldtext = None
            if "status" in type:
                if boldtext:
                    statustext = boldtext
                else:
                    statustext = text
                self.notify("INFO:",statustext,INFO_ICON)
            if "dialog" in type or "okdialog" in type:
                self.notify("INFO:","Dialog Responce Required",INFO_ICON)
                self.halcomp[pinname + "-waiting"] = True
                if "okdialog" in type:
                    self.warning_dialog(boldtext,True,text)
                else:
                    result = self.warning_dialog(boldtext,False,text)
                    self.halcomp[pinname + "-response"] = result
                self.halcomp[pinname + "-waiting"] = False
                self.widgets.statusbar1.pop(self.statusbar_id)

    def toggle_overrides(self,widget,mode,number):
        print "overrides - button_h_%d_%d"%(mode,number)
        for i in range(0,5):
            if i == number:continue
            button = "button_h%d_%d"% (mode,i)
            self.block(button)
            self.widgets[button].set_active(False)
            self.unblock(button)

    def toggle_graphic_overrides(self,widget,mode,number):
        print "graphic overrides - button_h_%d_%d"%(mode,number)
        for i in range(0,8):
            if i == number or i in(1,2,3):continue
            button = "button_h%d_%d"% (mode,i)
            self.block(button)
            self.widgets[button].set_active(False)
            self.unblock(button)

    def on_hal_status_interp_run(self,widget):
        print "run"
        temp = ["button_v1_7","button_h3_0","button_h3_4","button_h3_5","button_h3_6","button_mode"]
        for axis in self.data.axis_list:
            temp.append("axis_%s"% axis)
        self.sensitize_widgets(temp,False)

    def on_hal_status_interp_idle(self,widget):
        print "idle"
        temp = ["button_v1_7","button_h3_0","button_h3_4","button_h3_5","button_h3_6"]
        for axis in self.data.axis_list:
            temp.append("axis_%s"% axis)
        self.sensitize_widgets(temp,True)
        state = self.data.all_homed
        self.widgets.button_mode.set_sensitive(True)
        self.widgets.button_v0_0.set_sensitive(state)
        self.widgets.button_v0_1.set_sensitive(state)
        self.widgets.button_h1_1.set_sensitive(state)

    def on_hal_status_state_on(self,widget):
        print "on"
        temp = ["vmode0","mode0","mode1","button_homing","button_override","button_graphics","frame5"]
        for axis in self.data.axis_list:
            temp.append("axis_%s"% axis)
        self.sensitize_widgets(temp,True)
        state = self.data.all_homed
        self.widgets.button_mode.set_sensitive(True)
        self.widgets.button_v0_0.set_sensitive(state)
        self.widgets.button_v0_1.set_sensitive(state)
        self.widgets.button_h1_1.set_sensitive(state)
        if not state:
            self.widgets.button_homing.emit("clicked")

    def on_hal_status_state_off(self,widget):
        print "off"
        temp = ["vmode0","mode0","mode1","button_homing","button_override","button_mode","button_graphics","frame5"]
        for axis in self.data.axis_list:
            temp.append("axis_%s"% axis)
        self.sensitize_widgets(temp,False)

    def on_hal_status_all_homed(self,widget):
        print "all-homed"
        self.data.all_homed = True
        self.widgets.button_homing.set_active(False)
        self.widgets.statusbar1.remove_message(self.statusbar_id,self.homed_status_message)

    def on_hal_status_not_all_homed(self,widget):
        print "not-all-homed"
        self.data.all_homed = False

    def on_hal_status_file_loaded(self,widget,filename):
        path,name = os.path.split(filename)
        self.widgets.gcode_tab.set_text(name)

# ****** do stuff *****

    # this installs local signals unless overriden by custom handlers
    def install_signals(self, handlers):

        signal_list = [ ["unhome_axis","clicked", "unhome_selected"],
                        ["button_estop","clicked", "on_estop_clicked"],
                        ["gremlin","motion-notify-event", "on_gremlin_motion"],
                        ["gremlin","button_press_event", "on_gremlin_clicked"],
                        ["button_mode","button_press_event", "on_mode_clicked"],
                        ["button_menu","button_press_event", "on_mode_select_clicked"],
                        ["button_v0_2","pressed", "on_button_v0_2_pressed"],
                        ["button_v0_2","released", "on_button_v0_2_released"],
                        ["button_v0_3","pressed", "on_button_v0_3_pressed"],
                        ["button_v0_3","released", "on_button_v0_3_released"],
                        ["theme_choice","changed", "on_theme_choice_changed"],
                        ["use_screen2","clicked", "on_use_screen2_pressed"],
                        ["dro_units","clicked", "on_dro_units_pressed"],
                        ["diameter_mode","clicked", "on_diameter_mode_pressed"],
                        ["show_offsets","clicked", "on_show_offsets_pressed"],
                        ["show_dtg","clicked", "on_show_dtg_pressed"],
                        ["fullscreen1","clicked", "on_fullscreen1_pressed"],
                        ["shut_down","clicked", "on_window1_destroy"],
                        ["shut_down",'released',"hack_leave"],
                        ["run_halshow","clicked", "on_halshow"],
                        ["run_calibration","clicked", "on_calibration"],
                        ["run_status","clicked", "on_status"],
                        ["run_halmeter","clicked", "on_halmeter"],
                        ["hide_cursor","clicked", "on_hide_cursor"],
                        ["button_homing","clicked", "homing"],
                        ["button_override","clicked", "override"],
                        ["button_graphics","clicked", "graphics"],
                        ["data_input","button_press_event", "launch_numerical_input"],
                        ["grid_size","value_changed", "set_grid_size"],
                        ["spindle_start_rpm","value_changed", "set_spindle_start_rpm"],
                        ["spindle_control","clicked", "on_spindle_control_clicked"],
                        ["spindle_preset","clicked", "on_preset_spindle"],
                        ["spindle_increase","clicked", "on_spindle_speed_adjust"],
                        ["spindle_decrease","clicked", "on_spindle_speed_adjust"],
                        ["audio_error_chooser","update-preview", "update_preview"],
                        ["audio_alert_chooser","update-preview", "update_preview"],
                        ["hal_status","interp-idle", "on_hal_status_interp_idle"],
                        ["hal_status","interp-run", "on_hal_status_interp_run"],
                        ["hal_status","state-on", "on_hal_status_state_on"],
                        ["hal_status","state-off", "on_hal_status_state_off"],
                        ["hal_status","all-homed", "on_hal_status_all_homed"],
                        ["hal_status","not-all-homed", "on_hal_status_not_all_homed"],
                        ["hal_status","file-loaded", "on_hal_status_file_loaded"],
                        ["button_no","clicked", "on_button_no_clicked"],
                        ["button_yes","clicked", "on_button_yes_clicked"],
                        ["window1","destroy", "on_window1_destroy"],
                        ["pop_statusbar","clicked", "on_pop_statusbar_clicked"],
                        ["dtg_colorbutton","color-set", "on_dtg_colorbutton_color_set"],
                        ["abs_colorbutton","color-set", "on_abs_colorbutton_color_set"],
                        ["rel_colorbutton","color-set", "on_rel_colorbutton_color_set"],
                        ["eventbox_gremlin","leave_notify_event", "on_eventbox_gremlin_leave_notify_event"],
                        ["eventbox_gremlin","enter_notify_event", "on_eventbox_gremlin_enter_notify_event"],
                        ["s_display_rev","toggled", "on_s_display_rev_toggled"],
                        ["s_display_fwd","toggled", "on_s_display_fwd_toggled"],
                        ["ignore_limits","clicked", "toggle_ignore_limits"],
                        ["audio_error_chooser","selection_changed","change_sound","error"],
                        ["audio_alert_chooser","selection_changed","change_sound","alert"], ]

        # check to see if the calls in the signal list are in the custom handler's list of calls
        # if so skip the call in the signal list
        # else connect the signals based on how many arguments they have 
        for i in signal_list:
            if i[2] in handlers:
                print "**** GSCREEN INFO: Overriding internal signal call to %s"% i[2]
                continue
            try:
                if  i[0] in("s_display_rev","s_display_fwd"):
                    j = "_sighandler_%s"% i[0]
                    self.data[j] = int(self.widgets[i[0]].connect(i[1], self[i[2]]))
                elif len(i) == 3:
                        self.widgets[i[0]].connect(i[1], self[i[2]])
                elif len(i) == 4:
                    self.widgets[i[0]].connect(i[1], self[i[2]],i[3])
            except:
                print "**** GSCREEN WARNING: could not connect %s to %s"% (i[0],i[2])

        # setup signals that can be blocked but not overriden 
        for axis in self.data.axis_list:
            cb = "axis_%s"% axis
            i = "_sighandler_axis_%s"% axis
            try:
                self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_selection_clicked))
            except:
                pass

        for mode in range(0,6):
            for num in range(0,24):
                cb = "button_h%d_%d"% (mode,num)
                i = "_sighandler_button_h%d_%d"% (mode,num)
                try:
                    self.data[i] = int(self.widgets[cb].connect("clicked", self.on_hbutton_clicked,mode,num))
                except:
                    break
        for mode in range(0,2):
            for num in range(0,11):
                cb = "button_v%d_%d"% (mode,num)
                i = "_sighandler_button_v%d_%d"% (mode,num)
                try:
                    self.data[i] = int(self.widgets[cb].connect("clicked", self.on_vbutton_clicked,mode,num))
                except:
                    break
                self.widgets.s_display_fwd

    def preset_spindle_speed(self,rpm):
        self.data.spindle_preset = rpm
        self.widgets.spindle_preset.set_label(" S %d"% rpm)

    def sensitize_widgets(self, widgetlist, value):
        for name in widgetlist:
            self.widgets[name].set_sensitive(value)

    def from_internal_linear_unit(self,v, unit=None):
        if unit is None:
            unit = self.status.get_linear_units()
        lu = (unit or 1) * 25.4
        return v*lu

    def parse_increment(self,jogincr):
        if jogincr.endswith("mm"):
            scale = self.from_internal_linear_unit(1/25.4)
        elif jogincr.endswith("cm"):
            scale = self.from_internal_linear_unit(10/25.4)
        elif jogincr.endswith("um"):
            scale = self.from_internal_linear_unit(.001/25.4)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self.from_internal_linear_unit(1.)
        elif jogincr.endswith("mil"):
            scale = self.from_internal_linear_unit(.001)
        else:
            scale = 1
        jogincr = jogincr.rstrip(" inchmuil")
        if "/" in jogincr:
            p, q = jogincr.split("/")
            jogincr = float(p) / float(q)
        else:
            jogincr = float(jogincr)
        return jogincr * scale

    def notify(self,title,message,icon="",timeout=2):
            try:
                self.widgets.statusbar1.push(self.statusbar_id,message)
            except:
                pass
            if NOTIFY_AVAILABLE:
                uri = ""
                if icon:
                    uri = "file://" + icon
                n = pynotify.Notification(title, message, uri)
                n.set_hint_string("x-canonical-append","True")
                n.set_urgency(pynotify.URGENCY_CRITICAL)
                n.set_timeout(int(timeout * 1000) )
                n.show()
            if _AUDIO_AVAIALBLE:
                if icon == ALERT_ICON:
                    self.audio.set_sound(self.data.error_sound)
                else:
                    self.audio.set_sound(self.data.alert_sound)
                self.audio.run()

    def next_tab(self):
        maxpage = self.widgets.notebook_mode.get_n_pages()
        page = self.widgets.notebook_mode.get_current_page()
        nextpage = page + 1
        print "mode select",maxpage,page,nextpage
        if nextpage == maxpage:nextpage = 0
        self.widgets.notebook_mode.set_current_page(nextpage)

    def set_feed_override(self,percent_rate,absolute=False):
        if absolute:
            rate = percent_rate
        else:
            rate = self.data.feed_override + percent_rate
        if rate > self.data.feed_override_max: rate = self.data.feed_override_max
        self.emc.feed_override(rate)

    def set_spindle_override(self,percent_rate,absolute=False):
        if absolute:
            rate = percent_rate
        else:
            rate = self.data.spindle_override + percent_rate
        if rate > self.data.spindle_override_max: rate = self.data.spindle_override_max
        elif rate < self.data.spindle_override_min: rate = self.data.spindle_override_min
        self.emc.spindle_override(rate)

    def set_velocity_override(self,percent_rate,absolute=False):
        if absolute:
            rate = percent_rate
        else:
            rate = self.data.velocity_override + percent_rate
        if rate > 1.0: rate = 1.0
        self.emc.max_velocity(rate * self.data._maxvelocity)

    def set_jog_rate(self,begining_rate,absolute=False):
        # in units per minute
        print "jog rate =",begining_rate,self.data.jog_rate
        if absolute:
            rate = begining_rate
        else:
            rate = self.data.jog_rate + begining_rate
        if rate < 0: rate = 0
        if rate > self.data.jog_rate_max: rate = self.data.jog_rate_max
        rate = round(rate,1)
        self.emc.continuous_jog_velocity(rate)
        self.data.jog_rate = rate

    def set_jog_increments(self,index_dir,absolute=False):
        next = self.data.current_jogincr_index + index_dir
        end = len(self.data.jog_increments)-1
        if next < 0: next = end
        if next > end: next = 0
        self.data.current_jogincr_index = next
        jogincr = self.data.jog_increments[next]
        self.widgets.jog_increment.set_text(jogincr)
        if jogincr == ("Continuous"):
            distance = 0
        else:
            distance = self.parse_increment(jogincr)
        self.halcomp["jog-increment.out"] = distance
        print "index jog increments",jogincr,distance

    def adjustment_buttons(self,widget,action):
        # is over ride adjustment selection active?
        if self.widgets.button_override.get_active():
            if widget == self.widgets.button_v0_0:
                print "zero button",action
                change = 0
                absolute = True
            if widget == self.widgets.button_v0_1:
                print "set at button",action
                change = self.get_qualified_input(_PERCENT_INPUT)/100
                print "qualified=",change
                absolute = True
            if widget == self.widgets.button_v0_2:
                print "up button",action
                change = 1
                absolute = False
            if widget == self.widgets.button_v0_3:
                print "down button",action
                change = -1
                absolute = False
            # what override is selected
            if self.widgets.button_h4_0.get_active() and action:
                print "feed override"
                if absolute:
                    self.set_feed_override(change,absolute)
                else:
                    self.set_feed_override((change * self.data.feed_override_inc),absolute)
            elif self.widgets.button_h4_1.get_active() and action:
                print "spindle override"
                if absolute:
                    self.set_spindle_override(change,absolute)
                else:
                    self.set_spindle_override((change * self.data.spindle_override_inc),absolute)
            elif self.widgets.button_h4_2.get_active() and action:
                print "velocity override"
                if absolute:
                    self.set_velocity_override(change,absolute)
                else:
                    self.set_velocity_override((change * self.data.velocity_override_inc),absolute)
            elif self.widgets.button_h4_3.get_active() and action:
                print "jog speed adjustment"
                if widget == self.widgets.button_v0_1:
                    change = self.get_qualified_input()
                if absolute:
                    self.set_jog_rate(change,absolute)
                else:
                    self.set_jog_rate((change * self.data.jog_rate_inc),absolute)
            elif self.widgets.button_h4_4.get_active() and action:
                print "jog increments adjustment"
                if widget == self.widgets.button_v0_1:
                    return
                self.set_jog_increments(change,absolute)

        # graphics adjustment
        elif self.widgets.button_graphics.get_active():
            inc = self.data.graphic_move_inc
            if widget == self.widgets.button_v0_2:
                print "up button",action
                change = 1
            elif widget == self.widgets.button_v0_3:
                print "down button",action
                change = -1
            if self.widgets.button_h5_0.get_active() and action:
                print "zoom"
                if change == 1: self.zoom_in()
                else: self.zoom_out()
            elif self.widgets.button_h5_4.get_active() and action:
                print "pan vertical"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.pan(0,-inc)
                else: self.pan(0,inc)
            elif self.widgets.button_h5_5.get_active() and action:
                print "pan horizontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.pan(-inc,0)
                else: self.pan(inc,0)
            elif self.widgets.button_h5_6.get_active() and action:
                print "rotate horiontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.rotate(-inc,0)
                else: self.rotate(inc,0)
            elif self.widgets.button_h5_7.get_active() and action:
                print "rotate horiontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.rotate(0,-inc)
                else: self.rotate(0,inc)

        # user coordinate system
        elif self.widgets.button_h1_1.get_active():
            if widget == self.widgets.button_v0_2 and action:
                print "up button",action
                change = 1
            elif widget == self.widgets.button_v0_3 and action:
                print "down button",action
                change = -1
            else: return
            self.change_origin_system(None,change)
        # Jogging mode (This needs to be last)
        elif self.data.mode_order[0] == _MAN and self.widgets.button_h1_0.get_active(): # manual mode and jog mode active
            # what axis is set
            if widget == self.widgets.button_v0_0:
                print "zero button",action
                self.zero_axis()
            elif widget == self.widgets.button_v0_5:
                print "move to button",action
                self.move_to()
            elif widget == self.widgets.button_v0_2:
                print "up button",action
                self.do_jog(True,action)
            elif widget == self.widgets.button_v0_3:
                print "down button",action
                self.do_jog(False,action)
            elif widget == self.widgets.button_v0_1:
                self.set_axis_checks()
        elif widget == self.widgets.button_v0_0:
                self.zero_axis()
        elif widget == self.widgets.button_v0_1:
            self.set_axis_checks()

    def origin_system(self,*args):
        print "origin system button"
        value = self.widgets.button_h1_1.get_active()
        temp = ["button_override","button_graphics","button_homing","button_mode",
                "button_v0_0","button_v0_1","button_h1_0","button_h1_2","button_h1_3","button_h1_4"]
        for axis in self.data.axis_list:
            temp.append("axis_%s"% axis)
        self.sensitize_widgets(temp,not value)

    def change_origin_system(self,system,direction=None):
        print system,direction
        system_list = (0,54,55,56,57,58,59,59.1,59.2,59.3)
        current = system_list[self.data.system]
        if not system:
            if direction > 0 and not current == 59.3: self.mdi_control.set_user_system(system_list[self.data.system+1])
            elif direction < 0 and not current == 54: self.mdi_control.set_user_system(system_list[self.data.system-1])
            self.reload_plot()


    def homing(self,*args):
        print "show/hide homing buttons"
        if self.widgets.button_homing.get_active():
            if len(self.data.active_axis_buttons) > 1:
                for i in self.data.axis_list:
                    self.widgets["axis_%s"%i].set_active(False)
            for i in range(0,4):
                self.widgets["button_v0_%d"% i].set_sensitive(False)
            self.widgets.mode0.hide()
            self.widgets.mode3.show()
            #self.widgets.button_mode.set_sensitive(False)
            self.widgets.button_override.set_sensitive(False)
            self.widgets.button_graphics.set_sensitive(False)
        else:
            for i in range(0,4):
                self.widgets["button_v0_%d"% i].set_sensitive(True)
            self.widgets.mode3.hide()
            self.widgets.mode0.show()
            state = self.data.all_homed
            #self.widgets.button_mode.set_sensitive(state)
            self.widgets.button_v0_0.set_sensitive(state)
            self.widgets.button_v0_1.set_sensitive(state)
            self.widgets.button_h1_1.set_sensitive(state)
            self.widgets.button_override.set_sensitive(True)
            self.widgets.button_graphics.set_sensitive(True)

    def graphics(self,*args):
        print "show/hide graphics buttons"
        if self.widgets.button_graphics.get_active():
            for i in range(0,3):
                self.widgets["mode%d"% i].hide()
            self.widgets.mode5.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self._tempholder = []
            self._templist = ["button_override","button_homing","button_mode",
                                "button_v0_0","button_v0_1","button_v0_2","button_v0_3","vmode0"]
            for axis in self.data.axis_list:
                self._templist.append("axis_%s"% axis)
            for name in (self._templist):
                self._tempholder.append(self.widgets[name].get_sensitive())
                self.widgets[name].set_sensitive(False)
            self.widgets.vmode0.set_sensitive(True)
            self.widgets.button_v0_2.set_sensitive(True)
            self.widgets.button_v0_3.set_sensitive(True)

        else:
            self.widgets.mode5.hide()
            self.mode_changed(self.data.mode_order[0])
            for num,name in enumerate(self._templist):
                if self.data.machine_on:
                    self.widgets[name].set_sensitive(True)
                else:
                    self.widgets[name].set_sensitive(self._tempholder[num])

    def override(self,*args):
        print "show/hide override buttons"
        if self.widgets.button_override.get_active():
            for i in range(0,3):
                self.widgets["mode%d"% i].hide()
            self.widgets.mode4.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.dro_frame.set_sensitive(False)
            self.widgets.button_mode.set_sensitive(False)
            self.widgets.button_graphics.set_sensitive(False)
            self.widgets.button_homing.set_sensitive(False)
            self.widgets.button_v0_0.set_label("Zero")
            self.widgets.button_v0_1.set_label("Set At")
        else:
            self.widgets.mode4.hide()
            self.mode_changed(self.data.mode_order[0])
            self.widgets.dro_frame.set_sensitive(True)
            self.widgets.button_mode.set_sensitive(True)
            self.widgets.button_graphics.set_sensitive(True)
            self.widgets.button_homing.set_sensitive(True)
            self.widgets.button_v0_0.set_label("Zero Origin")
            self.widgets.button_v0_1.set_label("Offset Origin")

    # search for and set up user requested message system.
    # status displays on the statusbat and requires no acknowledge.
    # dialog displays a GTK dialog box with yes or no buttons
    # okdialog displays a GTK dialog box with an ok button
    # dialogs require an answer before focus is sent back to main screen
    def message_setup(self):
        if not self.inifile:
            return
        m_boldtext = self.inifile.findall("DISPLAY", "MESSAGE_BOLDTEXT")
        m_text = self.inifile.findall("DISPLAY", "MESSAGE_TEXT")
        m_type = self.inifile.findall("DISPLAY", "MESSAGE_TYPE")
        m_pinname = self.inifile.findall("DISPLAY", "MESSAGE_PINNAME")
        if len(m_text) != len(m_type):
            print "**** Gscreen ERROR:    Invalid message configuration (missing text or type) in INI File [DISPLAY] section"
        if len(m_text) != len(m_pinname):
            print "**** Gscreen ERROR:    Invalid message configuration (missing pinname) in INI File [DISPLAY] section"
        if len(m_text) != len(m_boldtext):
            print "**** Gscreen ERROR:    Invalid message configuration (missing boldtext) in INI File [DISPLAY] section"
        for bt,t,c ,name in zip(m_boldtext,m_text, m_type,m_pinname):
            #print bt,t,c,name
            if not ("status" in c) and not ("dialog" in c) and not ("okdialog" in c):
                print "**** Gscreen ERROR:    invalid message type (%s)in INI File [DISPLAY] section"% c
                continue
            if not name == None:
                # this is how we make a pin that can be connected to a callback 
                self.data['name'] = hal_glib.GPin(self.halcomp.newpin(name, hal.HAL_BIT, hal.HAL_IN))
                self.data['name'].connect('value-changed', self.on_printmessage,name,bt,t,c)
                if ("dialog" in c):
                    self.halcomp.newpin(name+"-waiting", hal.HAL_BIT, hal.HAL_OUT)
                    if not ("ok" in c):
                        self.halcomp.newpin(name+"-response", hal.HAL_BIT, hal.HAL_OUT)

    # display dialog and wait for an answer
    def warning_dialog(self,message, displaytype, secondary=None):
        if displaytype:
            dialog = gtk.MessageDialog(self.widgets.window1,
                gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_WARNING, gtk.BUTTONS_OK,message)
            # if there is a secondary message then the first message text is bold
            if secondary:
                dialog.format_secondary_text(secondary)
            dialog.show_all()
            result = dialog.run()
            dialog.destroy()
            return True
        else:   
            dialog = gtk.MessageDialog(self.widgets.window1,
               gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
               gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,message)
            if secondary:
                dialog.format_secondary_text(secondary)
            dialog.show_all()
            result = dialog.run()
            dialog.destroy()
            if result == gtk.RESPONSE_YES:
                return True
            else:
                return False

    # adds the embedded object to a notebook tab or box
    def _dynamic_tab(self, widget, text):
        s = gtk.Socket()
        try:
            widget.append_page(s, gtk.Label(" " + text + " "))
        except:
            try:
                widget.pack_end(s,True,True,0)
            except:
                return None
        return s.get_id()

    # Check INI file for embed commands
    # NAME is used as the tab label if a notebook is used
    # LOCATION is the widgets name from the gladefile.
    # COMMAND is the actual program command
    # if no location is specified the main notebook is used
    def set_dynamic_tabs(self):
        from subprocess import Popen

        if not self.inifile:
            return

        tab_names = self.inifile.findall("DISPLAY", "EMBED_TAB_NAME")
        tab_location = self.inifile.findall("DISPLAY", "EMBED_TAB_LOCATION")
        tab_cmd   = self.inifile.findall("DISPLAY", "EMBED_TAB_COMMAND")

        if len(tab_names) != len(tab_cmd):
            print "Invalid tab configuration" # Complain somehow
        if len(tab_location) != len(tab_names):
            for num,i in enumerate(tab_names):
                try:
                    if tab_location[num]:
                        continue
                except:
                    tab_location.append("notebook_mode")

        for t,c ,name in zip(tab_names, tab_cmd,tab_location):
            nb = self.widgets[name]
            xid = self._dynamic_tab(nb, t)
            if not xid: continue
            cmd = c.replace('{XID}', str(xid))
            child = Popen(cmd.split())
            self._dynamic_childs[xid] = child
            nb.show_all()

    # Gotta kill the embedded processes when gscreen closes
    def kill_dynamic_childs(self):
        for c in self._dynamic_childs.values():
            c.terminate()

    # finds the postgui file name and INI file path
    def postgui(self):
        postgui_halfile = self.inifile.find("HAL", "POSTGUI_HALFILE")
        return postgui_halfile,sys.argv[2]

    # zooms in a set amount (set deep in gremlin)
    def zoom_in(self,*args):
        self.widgets.gremlin.zoom_in()

    def zoom_out(self,*args):
        self.widgets.gremlin.zoom_out()

    def set_fullscreen1(self, data):
        self.prefs.putpref('fullscreen1', data, bool)
        if data:
            self.widgets.window1.fullscreen()
        else:
            self.widgets.window1.unfullscreen()

    def set_show_offsets(self, data):
        self.prefs.putpref('show_offsets', data, bool)
        try:
            self.widgets.gremlin.show_offsets = data
        except:
            pass

    def set_show_dtg(self, data):
        self.prefs.putpref('show_dtg', data, bool)
        try:
            self.widgets.gremlin.set_property('show_dtg',data)
        except:
            pass

    def set_diameter_mode(self, data):
        print "toggle diameter mode"
        self.data.diameter_mode = data
        self.prefs.putpref('diameter_mode', data, bool)
        try:
            self.widgets.gremlin.set_property('show_lathe_radius',not data)
        except:
            pass

    # returns the separate RGB color numbers from the color widget
    def convert_to_rgb(self,spec):
        color =  spec.to_string()
        temp = color.strip("#")
        r = temp[0:4]
        g = temp[4:8]
        b = temp[8:]
        return (int(r,16),int(g,16),int(b,16))

    def set_rel_color(self):
        self.data.rel_color = self.convert_to_rgb(self.widgets.rel_colorbutton.get_color())
        self.prefs.putpref('rel_textcolor', self.widgets.rel_colorbutton.get_color(),str)

    def set_abs_color(self):
        self.data.abs_color = self.convert_to_rgb(self.widgets.abs_colorbutton.get_color())
        self.prefs.putpref('abs_textcolor', self.widgets.abs_colorbutton.get_color(),str)

    def set_dtg_color(self):
        self.data.dtg_color = self.convert_to_rgb(self.widgets.dtg_colorbutton.get_color())
        self.prefs.putpref('dtg_textcolor', self.widgets.dtg_colorbutton.get_color(),str)

    # toggles gremlin's different views
    # if in lathe mode only P, Y and Y2 available
    def toggle_view(self):
        def shift():
            a = self.data.plot_view[0]
            b = self.data.plot_view[1]
            c = self.data.plot_view[2]
            d = self.data.plot_view[3]
            e = self.data.plot_view[4]
            f = self.data.plot_view[5]
            self.data.plot_view = (b,c,d,e,f,a)
        shift()
        if self.data.lathe_mode:
            while not self.data.plot_view[0].lower() in("p","y","y2"):
                shift()
        elif self.data.plot_view[0].lower() == "y2":
                shift()
        self.widgets.gremlin.set_property('view',self.data.plot_view[0])
        self.prefs.putpref('view', self.data.plot_view, tuple)

    # toggle a large graphics view
    def full_graphics(self):
        print "graphics mode"
        if self.data.full_graphics:
            print "shrink"
            self.data.full_graphics = False
            self.widgets.notebook_mode.show()
            self.widgets.dro_frame.show()
            self.widgets.gremlin.set_property('enable_dro',False)
        else:
            print "enlarge"
            self.data.full_graphics = True
            self.widgets.notebook_mode.hide()
            self.widgets.dro_frame.hide()
            self.widgets.gremlin.set_property('enable_dro',True)

    # enlargen the Gcode box while in edit mode
    def edit_mode(self):
        print "edit mode pressed"
        if self.data.edit_mode:
            self.widgets.gcode_view.set_sensitive(0)
            self.data.edit_mode = False
            self.widgets.show_box.show()
            self.widgets.hbuttonbox.set_sensitive(True)
            self.widgets.button_mode.set_sensitive(True)
        else:
            self.widgets.gcode_view.set_sensitive(1)
            self.data.edit_mode = True
            self.widgets.show_box.hide()
            self.widgets.notebook_mode.show()
            self.widgets.hbuttonbox.set_sensitive(False)
            self.widgets.button_mode.set_sensitive(False)

    def set_dro_units(self, data):
        print "toggle dro units",self.data.dro_units,data
        if data == _IMPERIAL:
            print "switch to imperial"
            self.status.dro_inch(1)
            self.widgets.gremlin.set_property('metric_units',False)
        else:
            print "switch to mm"
            self.status.dro_mm(1)
            self.widgets.gremlin.set_property('metric_units',True)
        self.data.dro_units = data

    def toggle_optional_stop(self):
        self.set_optional_stop(self.widgets.button_h3_2.get_active())

    def set_optional_stop(self,data):
        self.prefs.putpref('opstop', data, bool)
        self.data.op_stop = data
        self.emc.opstop(data)

    def toggle_block_delete(self):
        self.set_block_delete(self.widgets.button_h3_1.get_active())

    def set_block_delete(self,data):
        print "block delete"
        self.prefs.putpref('blockdel', data, bool)
        self.data.block_del = data
        self.emc.blockdel(data)

    def save_edit(self):
        print "edit"

    # helper method to block and unblock GTK widget signals
    def block(self,widget_name):
        self.widgets["%s"%(widget_name)].handler_block(self.data["_sighandler_%s"% (widget_name)])

    def unblock(self,widget_name):
         self.widgets["%s"%(widget_name)].handler_unblock(self.data["_sighandler_%s"% (widget_name)])

    # update the global variable of active axis buttons
    # if in jogging or homing mode, only one axis can be active at once
    # update the related axis HAL pins
    def update_active_axis_buttons(self,widget):
        count = 0;temp = []
        self.data.active_axis_buttons = []
        for i in self.data.axis_list:
            num = "xyzabcuvws".index(i)
            if self.widgets.button_h1_0.get_active() or self.widgets.button_homing.get_active():
                if not self.widgets["axis_%s"%i] == widget:
                    # unselect axis / HAL pin
                    self.block("axis_%s"%i)
                    self.widgets["axis_%s"%i].set_active(False)
                    self.unblock("axis_%s"%i)
                    continue
            if self.widgets["axis_%s"%i].get_active():
                count +=1
                axisnum = num
                self.data.active_axis_buttons.append((i,num))
        if count == 0: self.data.active_axis_buttons.append((None,None))
        self.update_hal_jog_pins()
        # check and update jogging buttons
        self.jog_mode()

    # adjust sensitivity and labels of buttons
    def jog_mode(self):
        print "jog mode:",self.widgets.button_h1_0.get_active()
        # if muliple axis selected - unselect all of them
        if len(self.data.active_axis_buttons) > 1 and self.widgets.button_h1_0.get_active():
            for i in self.data.axis_list:
                self.widgets["axis_%s"%i].set_active(False)
        if self.widgets.button_h1_0.get_active():
            self.widgets.button_v0_5.set_label("Move To")
        else:
            self.widgets.button_v0_5.set_label("")
        self.update_hal_jog_pins()

    # do some checks then jog selected axis or start spindle
    def do_jog(self,direction,action):
        # if manual mode, if jogging
        # if only one axis button pressed
        # jog positive  at selected rate
        if self.data.mode_order[0] == _MAN:
            if len(self.data.active_axis_buttons) > 1:
                self.notify("INFO:","Can't jog multiple axis",INFO_ICON)
                print self.data.active_axis_buttons
            elif self.data.active_axis_buttons[0][0] == None:
                self.notify("INFO:","No axis selected to jog",INFO_ICON)
            else:
                print "Jog axis %s" % self.data.active_axis_buttons[0][0]
                if not self.data.active_axis_buttons[0][0] == "s":
                    if not action: cmd = 0
                    elif direction: cmd = 1
                    else: cmd = -1
                    self.emc.jogging(1)
                    if self.data.current_jogincr_index == 0: # continuous jog
                        print "active axis jog:",self.data.active_axis_buttons[0][1]
                        self.emc.continuous_jog(self.data.active_axis_buttons[0][1],cmd)
                    else:
                        print "jog incremental"
                        if cmd == 0: return # don't want release of button to stop jog
                        jogincr = self.data.jog_increments[self.data.current_jogincr_index]
                        distance = self.parse_increment(jogincr)
                        self.emc.incremental_jog(self.data.active_axis_buttons[0][1],cmd,distance)

    # spindle control
    def spindle_adjustment(self,direction,action):
        if action and not self.widgets.s_display_fwd.get_active() and not self.widgets.s_display_rev.get_active():
            self.notify("INFO:","No direction selected for spindle",INFO_ICON)
            return
        if direction and action:
            if self.data.spindle_speed:
                self.emc.spindle_faster(1)
            elif self.widgets.s_display_fwd.get_active():
               self.emc.spindle_forward(1,self.data.spindle_start_rpm)
            else:
                self.emc.spindle_reverse(1,self.data.spindle_start_rpm)
            print direction,action
        elif not direction and action:
            if self.data.spindle_speed:
                if self.data.spindle_speed >100:
                    self.emc.spindle_slower(1)
                else:
                    self.emc.spindle_off(1)

    # feeds to a position (while in manual mode)
    def do_jog_to_position(self):
        if len(self.data.active_axis_buttons) > 1:
            self.notify("INFO:","Can't jog multiple axis",INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify("INFO:","No axis selected to move",INFO_ICON)
        else:
            if not self.data.active_axis_buttons[0][0] == "s":
                self.mdi_control.go_to_position(self.data.active_axis_buttons[0][0],self.get_qualified_input(),self.data.jog_rate)

    def adjust_spindle_rpm(self, rpm, direction=None):
            # spindle control
             if direction == None:
                direction = self.data.spindle_dir
             if direction > 0:
                print "forward"
                self.emc.spindle_forward(1, float(rpm))
             elif direction < 0:
                print "reverse"
                self.emc.spindle_reverse(1, float(rpm))
             else:
                self.emc.spindle_off(1)

    # shows the second glade file panel
    def toggle_screen2(self):
        self.data.use_screen2 = self.widgets.use_screen2.get_active()
        if self.screen2:
            if self.data.use_screen2:
                self.widgets.window2.show()
            else:
                self.widgets.window2.hide()
        self.prefs.putpref('use_screen2', self.data.use_screen2, bool)

    # This converts and qualifies the input
    # eg for diameter, metric or percentage
    # If the calculator is displayed use it for input instead of spinbox
    def get_qualified_input(self,switch = None):
        if self.widgets.dialog_entry.get_property("visible"):
            raw = self.widgets.calc_entry.get_value()
        else:
            raw = self.widgets.data_input.get_value()
        print "RAW input:",raw
        if switch == _SPINDLE_INPUT:
            return raw
        elif switch == _PERCENT_INPUT:
            return round(raw,2)
        else:
            g21 = False
            if "G21" in self.data.active_gcodes: g21 = True

            # metric DRO - imperial mode
            if self.data.dro_units == _MM:
                if not g21:
                    raw = raw / 25.4
            # imperial DRO - metric mode
            elif g21:
                raw = raw * 25.4

            if switch == "x" and self.data.diameter_mode:
                print "convert from diameter"
                raw = raw / 2.0
        print "Qualified input:",raw
        return raw

    def unhome_all(self):
        self.emc.unhome_all(1)

    def home_all(self):
        self.emc.home_all(1)

    # do some checks first the home the selected axis
    def home_selected(self):
        print "home selected"
        if len(self.data.active_axis_buttons) > 1:
            self.notify("INFO:","Can't home multiple axis - select HOME ALL instead",INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify("INFO:","No axis selected to home",INFO_ICON)
        else:
            print "home axis %s" % self.data.active_axis_buttons[0][0]
            self.emc.home_selected(self.data.active_axis_buttons[0][1])

    def unhome_selected(self,widget):
        if len(self.data.active_axis_buttons) > 1:
            self.notify("INFO:","Can't unhome multiple axis",INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify("INFO:","No axis selected to unhome",INFO_ICON)
        else:
            print "unhome axis %s" % self.data.active_axis_buttons[0][0]
            self.emc.unhome_selected(self.data.active_axis_buttons[0][1])

    # Touchoff the axis zeroing it
    # reload the plot to update the display
    def zero_axis(self):
        if self.data.active_axis_buttons[0][0] == None:
            self.notify("INFO:","No axis selected for origin zeroing",INFO_ICON)
        # if an axis is selected then set it
        for i in self.data.axis_list:
            if self.widgets["axis_%s"%i].get_active():
                print "zero %s axis" %i
                self.mdi_control.set_axis(i,0)
                self.reload_plot()

    # touchoff - setting the axis to the input
    def set_axis_checks(self):
        if self.data.active_axis_buttons[0][0] == None:
            self.notify("INFO:","No axis selected for origin touch-off",INFO_ICON)
        # if an axis is selected then set it
        for i in self.data.axis_list:
            if self.widgets["axis_%s"%i].get_active():
                print "set %s axis" %i
                if not i == "s":
                    self.mdi_control.set_axis(i,self.get_qualified_input(i))
                    self.reload_plot()

    # move axis to a position (while in manual mode)
    def move_to(self):
        if self.data.mode_order[0] == _MAN:# if in manual mode
            if self.widgets.button_h1_0.get_active(): # jog mode active
                print "jog to position"
                self.do_jog_to_position()

    def clear_plot(self):
        self.widgets.gremlin.clear_live_plotter()

    def pan(self,x,y):
        self.widgets.gremlin.pan(x,y)

    def rotate(self,x,y):
        self.widgets.gremlin.rotate_view(x,y)

    def reload_plot(self):
        print "reload plot"
        self.widgets.button_h3_6.emit("clicked")

    def toggle_mist(self):
        if self.data.mist:
            self.emc.mist_off(1)
        else:
            self.emc.mist_on(1)

    def toggle_flood(self):
        if self.data.flood:
            self.emc.flood_off(1)
        else:
            self.emc.flood_on(1)

    def toggle_ignore_limits(self,*args):
        print "over ride limits"
        self.emc.override_limits(1)

    # toggle the tool editor page forward
    # reload the page when doing this
    # If the user specified a tool editor spawn it. 
    def reload_tooltable(self):
        # show the tool table page or return to the main page
        if not self.widgets.notebook_main.get_current_page() == 3:
            self.widgets.notebook_main.set_current_page(3)
        else:
            self.widgets.notebook_main.set_current_page(0)
            return
        # set the tooltable path from the INI file and reload it
        path = os.path.join(CONFIGPATH,self.data.tooltable)
        print "tooltable:",path
        self.widgets.tooledit1.set_filename(path)
        # see if user requested an external editor and spawn it 
        editor = self.data.tooleditor
        if not editor == None:
            res = os.spawnvp(os.P_WAIT, editor, [editor, path])
            if res:
                self.notify("Error Message","Tool editor error - is the %s editor available?"% editor,ALERT_ICON,3)
        # tell linuxcnc that the tooltable may have changed
        self.emc.reload_tooltable(1)

    # toggle thru the DRO large display
    def dro_toggle(self):
        print "toggle axis display"
        a = self.data.display_order[0]
        b = self.data.display_order[1]
        c = self.data.display_order[2]
        self.data.display_order = (c,a,b)
        self.prefs.putpref('display_order', self.data.display_order, tuple)
        if self.data.display_order[2] == _ABS:
            self.widgets.gremlin.set_property('use_relative',False)
        else:
            self.widgets.gremlin.set_property('use_relative',True)
        self.update_position()

    # adjust the screen as per each mode toggled 
    def mode_changed(self,mode):

        if mode == _MAN: 
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
            self.widgets.hal_mdihistory.hide()
            self.widgets.button_homing.show()
            self.widgets.dro_frame.show()
        elif mode == _MDI:
            if self.widgets.button_homing.get_active():
                self.widgets.button_homing.emit("clicked")
            self.mdi_control.set_mdi_mode()
            self.widgets.hal_mdihistory.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
        elif mode == _AUTO:
            self.widgets.vmode0.hide()
            self.widgets.vmode1.show()
            if self.data.full_graphics:
                self.widgets.notebook_mode.hide()
            else:
                self.widgets.notebook_mode.show()
            self.widgets.hal_mdihistory.hide()
        if not mode == _MAN:
            self.widgets.button_h1_0.set_active(False)
            self.widgets.button_homing.set_active(False)
            self.widgets.button_homing.hide()
        for i in range(0,3):
            if i == mode:
                self.widgets["mode%d"% i].show()
            else:
                self.widgets["mode%d"% i].hide()

    def change_theme(self, theme):
        self.prefs.putpref('gtk_theme', theme, str)
        if theme == None:return
        if theme == "Follow System Theme":
            theme = self.data.system_theme
        settings = gtk.settings_get_default()
        settings.set_string_property("gtk-theme-name", theme, "")

    # check linuxcnc for status, error and then update the readout
    def periodic_status(self):
        self.emc.mask()
        self.emcstat = linuxcnc.stat()
        self.emcerror = linuxcnc.error_channel()
        self.emcstat.poll()
        self.data.task_mode = self.emcstat.task_mode 
        self.status.periodic()
        self.data.system = self.status.get_current_system()
        e = self.emcerror.poll()
        if e:
            kind, text = e
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                self.notify("Error Message",text,ALERT_ICON,3)
            else:
                self.notify("Error Message",text,INFO_ICON,3)
        self.emc.unmask()
        if "periodic" in dir(self.custom_handler):
            self.custom_handler.periodic(self)
        else:
            self.update_position()
        return True

    # update the whole display
    def update_position(self,*args):
        # spindle controls
        if self.data.spindle_speed == 0:
            temp = "Start"
        else:
            temp = "Stop"
        self.widgets.spindle_control.set_label(temp)
        self.widgets.s_display.set_value(abs(self.halcomp["spindle-readout.in"]))
        self.widgets.s_display.set_target_value(abs(self.data.spindle_speed))
        try:
            self.widgets.s_display2.set_value(abs(self.data.spindle_speed))
        except:
            pass
        # DRO
        for i in self.data.axis_list:
            if i in ('b','c','u','v','w'): continue
                
            for j in range (0,3):
                current = self.data.display_order[j]
                attr = pango.AttrList()
                if current == _ABS:
                    color = self.data.abs_color
                    data = self.data["%s_abs"%i]
                    #text = "%+ 10.4f"% self.data["%s_abs"%i]
                    label = "ABS"
                elif current == _REL:
                    color = self.data.rel_color
                    data = self.data["%s_rel"%i]
                    #text = "%+ 10.4f"% self.data["%s_rel"%i]
                    label= "REL"
                elif current == _DTG:
                    color = self.data.dtg_color
                    data = self.data["%s_dtg"%i]
                    #text = "%+ 10.4f"% self.data["%s_dtg"%i]
                    label = "DTG"
                if j == 2:
                    if self.data.highlight_major:
                        hlcolor = self.data.highlight_color
                        bg_color = pango.AttrBackground(hlcolor[0],hlcolor[1],hlcolor[2], 0, -1)
                        attr.insert(bg_color)
                    size = pango.AttrSize(30000, 0, -1)
                    attr.insert(size)
                    weight = pango.AttrWeight(600, 0, -1)
                    attr.insert(weight)
                fg_color = pango.AttrForeground(color[0],color[1],color[2], 0, 11)
                attr.insert(fg_color)
                self.widgets["%s_display_%d"%(i,j)].set_attributes(attr)
                h = " "
                if current == _ABS and self.data["%s_is_homed"% i]: h = "*"
                if self.data.diameter_mode and i == 'x': data = data * 2.0
                if self.data.dro_units == _MM:
                    text = "%s% 10.3f"% (h,data)
                else:
                    text = "%s% 9.4f"% (h,data)
                self.widgets["%s_display_%d"%(i,j)].set_text(text)
                self.widgets["%s_display_%d"%(i,j)].set_alignment(0,.5)
                self.widgets["%s_display_%d_label"%(i,j)].set_alignment(1,.5)
                self.widgets["%s_display_%d_label"%(i,j)].set_text(label)
        # corodinate system:
        systemlabel = ("Machine","G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3")
        # active codes
        active_g = " ".join(self.data.active_gcodes)
        self.widgets.active_gcodes_label.set_label("%s   "% active_g)
        self.widgets.active_mcodes_label.set_label(" ".join(self.data.active_mcodes))
        # control aux_coolant  - For Dave Armstrong
        m7 = m8 = False
        self.halcomp["aux-coolant-m8.out"] = False
        self.halcomp["mist-coolant.out"] = False
        self.halcomp["aux-coolant-m7.out"] = False
        self.halcomp["flood-coolant.out"] = False
        if self.data.mist:
            if self.widgets.aux_coolant_m7.get_active():
               self.halcomp["aux-coolant-m7.out"] = True
            else:
                self.halcomp["mist-coolant.out"] = True
        if self.data.flood:
            if self.widgets.aux_coolant_m8.get_active():
                self.halcomp["aux-coolant-m8.out"] = True
            else:
                self.halcomp["flood-coolant.out"] = True
        self.widgets.active_feed_speed_label.set_label("F%s    S%s"% (self.data.active_feed_command,self.data.active_spindle_command))
        tool = str(self.data.tool_in_spindle)
        if tool == None: tool = "None"
        self.widgets.system.set_text("Tool %s     %s"%(tool,systemlabel[self.data.system]))
        # coolant
        self.widgets.led_mist.set_active(self.data.mist)
        self.widgets.led_flood.set_active(self.data.flood)
        # estop
        self.widgets.led_estop.set_active(self.data.estopped)
        self.widgets.led_on.set_active(self.data.machine_on)
        # ignore limts led
        self.widgets.led_ignore_limits.set_active(self.data.or_limits)
        # overrides
        self.widgets.fo.set_text("FO: %d%%"%(round(self.data.feed_override,2)*100))
        self.widgets.so.set_text("SO: %d%%"%(round(self.data.spindle_override,2)*100))
        self.widgets.mv.set_text("VO: %d%%"%(round((self.data.velocity_override),2) *100))
        if self.data.dro_units == _MM:
            text = "Jog: %4.2f mm/min"% (round(self.data.jog_rate*25.4,2))
        else:
            text = "Jog: %3.2f IPM"% (round(self.data.jog_rate,2))
        self.widgets.jog_rate.set_text(text)
        # Mode / view
        modenames = ("Manual","MDI","Auto")
        self.widgets.mode_label.set_label( "%s Mode   View -%s"% (modenames[self.data.mode_order[0]],self.data.plot_view[0]) )

    def update_hal_jog_pins(self):
         for i in self.data.axis_list:
            if self.widgets.button_h1_0.get_active() and self.widgets["axis_%s"%i].get_active():
                self.halcomp["jog-enable-%s.out"%i] = True
            else:
                self.halcomp["jog-enable-%s.out"%i] = False
            if self.widgets.button_h1_0.get_active():
                self.halcomp["jog-enable.out"] = True
            else:
                self.halcomp["jog-enable.out"] = False

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# calls a postgui file if there is one.
# then starts Gscreen
if __name__ == "__main__":
    try:
        print "**** GSCREEN INFO ini:", sys.argv[2]
        app = Gscreen()
    except KeyboardInterrupt:
        sys.exit(0)
    postgui_halfile,inifile = Gscreen.postgui(app)
    print "**** GSCREEN INFO: postgui filename:",postgui_halfile
    if postgui_halfile:
        res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",inifile,"-f", postgui_halfile])
        if res: raise SystemExit, res
    gtk.main()

