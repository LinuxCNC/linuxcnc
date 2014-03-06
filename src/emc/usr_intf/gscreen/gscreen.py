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
import sys,os,subprocess
from optparse import Option, OptionParser
import gladevcp.makepins
from gladevcp.gladebuilder import GladeBuilder
import pango
import traceback
import atexit
import vte
import time
from time import strftime,localtime
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
    _AUDIO_AVAILABLE = False
    import pygst
    pygst.require("0.10")
    import gst
    _AUDIO_AVAILABLE = True
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
SKINPATH = os.path.join(BASE, "share","gscreen","skins")
sys.path.insert(0, libdir)
themedir = "/usr/share/themes"

xmlname = os.path.join(datadir,"gscreen.glade")
xmlname2 = os.path.join(datadir,"gscreen2.glade")
ALERT_ICON = os.path.join(imagedir,"applet-critical.png")
INFO_ICON = os.path.join(imagedir,"std_info.gif")

# internationalization and localization
import locale, gettext


# path to TCL for external programs eg. halshow
TCLPATH = os.environ['LINUXCNC_TCL_DIR']
# path to the configuration the user requested
# used to see if the is local GLADE files to use
CONFIGPATH = os.environ['CONFIG_DIR']
import linuxcnc
from gscreen import emc_interface
from gscreen import mdi
from gscreen import preferences
from gscreen import keybindings

# this is for hiding the pointer when using a touch screen
pixmap = gtk.gdk.Pixmap(None, 1, 1, 1)
color = gtk.gdk.Color()
INVISABLE = gtk.gdk.Cursor(pixmap, pixmap, color, color, 0, 0)
# to help with debugging new screens
verbose_debug = False
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
_X = 0;_Y = 1;_Z = 2;_A = 3;_B = 4;_C = 5;_U = 6;_V = 7;_W = 8
_ABS = 0;_REL = 1;_DTG = 2
_SPINDLE_INPUT = 1;_PERCENT_INPUT = 2;_VELOCITY_INPUT = 3;_DEGREE_INPUT = 4

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
        # constants for mode idenity
        self._MAN = 0
        self._MDI = 1
        self._AUTO = 2
        self._JOG = 3
        self._MM = 1
        self._IMPERIAL = 0
        # paths included to give access to handler files
        self.SKINPATH = SKINPATH
        self.CONFIGPATH = CONFIGPATH
        self.BASEPATH = BASE

        self.audio_available = False
        self.use_screen2 = False
        self.theme_name = "Follow System Theme"
        self.abs_textcolor = ""
        self.rel_textcolor = ""
        self.dtg_textcolor = ""
        self.err_textcolor = ""
        self.window_geometry = ""
        self.window_max = ""
        self.axis_list = []
        self.rotary_joints = False
        self.active_axis_buttons = [(None,None)] # axis letter,axis number
        self.abs_color = (0, 65535, 0)
        self.rel_color = (65535, 0, 0)
        self.dtg_color = (0, 0, 65535)
        self.highlight_color = (65535,65535,65535)
        self.highlight_major = False
        self.display_order = (_REL,_DTG,_ABS)
        self.mode_order = (self._MAN,self._MDI,self._AUTO)
        self.mode_labels = ["Manual Mode","MDI Mode","Auto Mode"]
        self.IPR_mode = False
        self.plot_view = ("p","x","y","y2","z","z2")
        self.task_mode = 0
        self.active_gcodes = []
        self.active_mcodes = []
        for letter in ('x','y','z','a','b','c','u','v','w'):
            self['%s_abs'%letter] = 0.0
            self['%s_rel'%letter] = 0.0
            self['%s_dtg'%letter] = 0.0
            self['%s_is_homed'%letter] = False
        self.spindle_request_rpm = 0
        self.spindle_dir = 0
        self.spindle_speed = 0
        self.spindle_start_rpm = 300
        self.spindle_preset = 300
        self.active_spindle_command = "" # spindle command setting
        self.active_feed_command = "" # feed command setting
        self.system = 1
        self.estopped = True
        self.dro_units = self._IMPERIAL
        self.machine_units = self._IMPERIAL
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
        self.angular_jog_adjustment_flag = False
        self.angular_jog_increments = []
        self.angular_jog_rate = 1800
        self.angular_jog_rate_inc = 60
        self.angular_jog_rate_max = 7200
        self.current_angular_jogincr_index = 0
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
        self.plot_hidden = False
        self.file = ""
        self.file_lines = 0
        self.line = 0
        self.last_line = 0
        self.motion_line = 0
        self.id = 0
        self.dtg = 0.0
        self.show_dtg = False
        self.velocity = 0.0
        self.delay = 0.0
        self.preppedtool = None
        self.lathe_mode = False
        self.diameter_mode = True
        self.tooleditor = ""
        self.tooltable = ""
        self.alert_sound = "/usr/share/sounds/ubuntu/stereo/bell.ogg"         
        self.error_sound  = "/usr/share/sounds/ubuntu/stereo/dialog-question.ogg"
        self.ob = None
        self.index_tool_dialog = None
        self.keyboard_dialog = None
        self.preset_spindle_dialog = None
        self.spindle_control_dialog = None
        self.entry_dialog = None
        self.restart_dialog = None
        self.key_event_last = None,0

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
    mod = object = None
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
            print _('adding import dir %s' % directory)
        try:
            mod = __import__(basename)
        except ImportError,msg:
            print ("module '%s' skipped - import error: %s" %(basename,msg))
	    continue
        print _("module '%s' imported OK" % mod.__name__)
        try:
            # look for functions
            for temp in ("periodic","connect_signals","initialize_widgets"):
                h = getattr(mod,temp,None)
                if h and callable(h):
                    print ("module '%s' : '%s' function found" % (mod.__name__,temp))

            # look for 'get_handlers' function
            h = getattr(mod,hdl_func,None)
            if h and callable(h):
                print ("module '%s' : '%s' function found" % (mod.__name__,hdl_func))
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
            print ("**** GSCREEN ERROR: trouble looking for handlers in '%s': %s" %(basename, e))
            traceback.print_exc()

    # Wrap lists in Trampoline, unwrap single functions
    for n,v in list(handlers.items()):
        if len(v) == 1:
            handlers[n] = v[0]
        else:
            handlers[n] = Trampoline(v)

    return handlers,mod,object

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
        global verbose_debug
        self.skinname = "gscreen"
        self.inipath = sys.argv[2]
        (progdir, progname) = os.path.split(sys.argv[0])

        # linuxcnc adds -ini to display name and optparse
        # can't understand that, so we do it manually 
        for num,temp in enumerate(sys.argv):
            if temp == '-c':
                try:
                    print ("**** GSCREEN INFO: Skin name ="),sys.argv[num+1]
                    self.skinname = sys.argv[num+1]
                except:
                    pass
            if temp == '-d': gscreen_debug = True
            if temp == '-v': verbose_debug = True

        # check for a local translation folder
        locallocale = os.path.join(CONFIGPATH,"locale")
        if os.path.exists(locallocale):
            LOCALEDIR = locallocale
            domain = self.skinname
            print ("**** GSCREEN INFO: CUSTOM locale name =",LOCALEDIR,self.skinname)
        else:
            locallocale = os.path.join(SKINPATH,"%s/locale"%self.skinname)
            if os.path.exists(locallocale):
                LOCALEDIR = locallocale
                domain = self.skinname
                print ("**** GSCREEN INFO: SKIN locale name =",LOCALEDIR,self.skinname)
            else:
                LOCALEDIR = os.path.join(BASE, "share", "locale")
                domain = "linuxcnc"
        locale.setlocale(locale.LC_ALL, '')
        locale.bindtextdomain(domain, LOCALEDIR)
        gettext.install(domain, localedir=LOCALEDIR, unicode=True)
        gettext.bindtextdomain(domain, LOCALEDIR)

        # main screen
        localglade = os.path.join(CONFIGPATH,"%s.glade"%self.skinname)
        if os.path.exists(localglade):
            print _("\n**** GSCREEN INFO:  Using CUSTOM glade file from %s ****"% localglade)
            xmlname = localglade
        else:
            localglade = os.path.join(SKINPATH,"%s/%s.glade"%(self.skinname,self.skinname))
            if os.path.exists(localglade):
                print _("\n**** GSCREEN INFO:  Using SKIN glade file from %s ****"% localglade)
                xmlname = localglade
            else:
                print _("\n**** GSCREEN INFO:  using STOCK glade file from: %s ****"% xmlname2)
        try:
            self.xml = gtk.Builder()
            self.xml.set_translation_domain(domain) # for locale translations
            self.xml.add_from_file(xmlname)
        except:
            print _("**** Gscreen GLADE ERROR:    With main screen xml file: %s"% xmlname)
            sys.exit(0)
        # second screen
        localglade = os.path.join(CONFIGPATH,"%s2.glade"%self.skinname)
        if os.path.exists(localglade):
            print _("\n**** GSCREEN INFO:  Screen 2 -Using CUSTOM glade file from %s ****"% localglade)
            xmlname2 = localglade
            try:
                self.xml.add_from_file(xmlname2)
                self.screen2 = True
            except:
                print _("**** Gscreen GLADE ERROR:    With screen 2's xml file: %s"% xmlname)
        else:
            print _("\n**** GSCREEN INFO:  No Screen 2 glade file present") 
        self.screen2 = False
        self.widgets = Widgets(self.xml)
        self.data = Data()
        self.keylookup = keybindings.Keylookup()

        if _AUDIO_AVAILABLE:
            self.audio = Player()
            self.data.audio_available = True       

        # access to EMC control
        self.emc = emc_interface.emc_control(linuxcnc)
        # access to EMC status
        self.status = emc_interface.emc_status( self.data, linuxcnc)
        # access to MDI
        mdi_labels = mdi_eventboxes = []
        self.mdi_control = mdi.mdi_control(gtk, linuxcnc, mdi_labels, mdi_eventboxes)
        # pull info from the INI file
        self.inifile = self.emc.emc.ini(self.inipath)
        # change the display based on the requested axis
        temp = self.inifile.find("TRAJ","COORDINATES")
        if temp == None:
            self.add_alarm_entry("No coordinates entry found in [TRAJ] of INI file")
        self.data.axis_list = []
        for letter in temp:
            if letter.lower() in self.data.axis_list: continue
            if not letter.lower() in ["x","y","z","a","b","c","u","v","w"]: continue
            self.data.axis_list.append(letter.lower())
        # check for rotary joints
        for i in("a","b","c"):
            if i in self.data.axis_list:
                self.data.rotary_joints = True
                break
        # check the ini file if UNITS are set to mm"
        # first check the global settings
        units=self.inifile.find("TRAJ","LINEAR_UNITS")
        if units==None:
            # else then the X axis units
            units=self.inifile.find("AXIS_0","UNITS")
            if units==None:
                self.add_alarm_entry(_("No UNITS entry found in [TRAJ] or [AXIS_0] of INI file"))
        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            conversion=[25.4]*3+[1]*3+[25.4]*3
        self.status.set_machine_units(self.machine_units_mm,conversion)

        # set-up HAL component
        try:
            self.halcomp = hal.component("gscreen")
        except:
            print _("*** Gscreen ERROR:    Asking for a HAL component using a name that already exists.")
            sys.exit(0)
        panel = gladevcp.makepins.GladePanel( self.halcomp, xmlname, self.xml, None)
        # at this point, any glade HAL widgets and their pins are set up.

        # look for custom handler files:
        HANDLER_FN = "%s_handler.py"%self.skinname
        local_handler_path = os.path.join(CONFIGPATH,HANDLER_FN)
        skin_handler_path = os.path.join(SKINPATH,"%s/%s"%(self.skinname,HANDLER_FN))
        if os.path.exists(local_handler_path):
            temp = [local_handler_path]
        elif os.path.exists(skin_handler_path):
            temp = [skin_handler_path]
        else:
            temp = []
        dbg("**** GSCREEN INFO: handler file path: %s"%temp)
        handlers,self.handler_module,self.handler_instance = load_handlers(temp,self.halcomp,self.xml,[],self)
        self.xml.connect_signals(handlers)

        # Look for an optional preferece file path otherwise it uses ~/.gscreen_preferences
        # then initiate access to saved prefernces
        temp = self.inifile.find("DISPLAY","PREFERENCE_FILE_PATH")
        dbg("**** GSCREEN INFO: Preference file path: %s"%temp)
        self.prefs = preferences.preferences(temp)

        # Intialize prefereces either from the handler file or from Gscreen
        if "initialize_preferences" in dir(self.handler_instance):
            self.handler_instance.initialize_preferences()
        else:
            self.initialize_preferences()

        # check for ladder loaded
        self.data.is_ladder = hal.component_exists('classicladder_rt')

        # get the system wide theme
        settings = gtk.settings_get_default()
        settings.props.gtk_button_images = True
        self.data.system_theme = settings.get_property("gtk-theme-name")

        # jogging increments
        increments = self.inifile.find("DISPLAY", "INCREMENTS")
        if increments:
            if not "continuous" in increments:
                increments +=",continuous"
            if "," in increments:
                self.data.jog_increments = [i.strip() for i in increments.split(",")]
            else:
                self.data.jog_increments = increments.split()
        else:
            if self.machine_units_mm ==self.data._MM:
                self.data.jog_increments = [".001 mm",".01 mm",".1 mm","1 mm","continuous"]
            else:
                self.data.jog_increments = [".0001 in",".001 in",".01 in",".1 in","continuous"]
            self.add_alarm_entry(_("No default jog increments entry found in [DISPLAY] of INI file"))

        # angular jogging increments
        increments = self.inifile.find("DISPLAY", "ANGULAR_INCREMENTS")
        if increments:
            if not "continuous" in increments:
                increments +=",continuous"
            if "," in increments:
                self.data.angular_jog_increments = [i.strip() for i in increments.split(",")]
            else:
                self.data.angular_jog_increments = increments.split()
        else:
            self.data.angular_jog_increments = ["1","45","180","360","continuous"]
            self.add_alarm_entry(_("No default angular jog increments entry found in [DISPLAY] of INI file"))

        # set default jog rate
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("DISPLAY","DEFAULT_LINEAR_VELOCITY")
        if temp:
            temp = float(temp)*60
        else:
            temp = self.data.jog_rate
            self.add_alarm_entry(_("No DEFAULT_LINEAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
        self.data.jog_rate = float(temp)
        self.emc.continuous_jog_velocity(float(temp),None)

        # set max jog rate
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("DISPLAY","MAX_LINEAR_VELOCITY")
        if temp:
            temp = float(temp)*60
        else:
            temp = self.data.jog_rate_max
            self.add_alarm_entry(_("No MAX_LINEAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
        self.data.jog_rate_max = float(temp)

        # max velocity settings: more then one place to check
        # This is the maximum velocity of the machine
        temp = self.inifile.find("TRAJ","MAX_VELOCITY")
        if temp == None:
            self.add_alarm_entry(_("No MAX_VELOCITY found in [TRAJ] of the INI file"))
            temp = 1.0
        self.data._maxvelocity = float(temp)

        # look for angular defaults if there is angular axis
        if "a" in self.data.axis_list or "b" in self.data.axis_list or "c" in self.data.axis_list:
            # set default angular jog rate
            # must convert from INI's units per second to gscreen's units per minute
            temp = self.inifile.find("DISPLAY","DEFAULT_ANGULAR_VELOCITY")
            if temp:
                temp = float(temp)*60
            else:
                temp = self.data.angular_jog_rate
                self.add_alarm_entry(_("No DEFAULT_ANGULAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
            self.data.angular_jog_rate = float(temp)
            self.emc.continuous_jog_velocity(None,float(temp))

            # set default angular jog rate
            # must convert from INI's units per second to gscreen's units per minute
            temp = self.inifile.find("DISPLAY","MAX_ANGULAR_VELOCITY")
            if temp:
                temp = float(temp)*60
            else:
                temp = self.data.angular_jog_rate_max
                self.add_alarm_entry(_("No MAX_ANGULAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
            self.data.angular_jog_rate_max = float(temp)

        # check for override settings
        temp = self.inifile.find("DISPLAY","MAX_SPINDLE_OVERRIDE")
        if temp:
            self.data.spindle_override_max = float(temp)
        else:
            self.add_alarm_entry(_("No MAX_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file"))

        temp = self.inifile.find("DISPLAY","MIN_SPINDLE_OVERRIDE")
        if temp:
            self.data.spindle_override_min = float(temp)
        else:
            self.add_alarm_entry(_("No MIN_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file"))

        temp = self.inifile.find("DISPLAY","MAX_FEED_OVERRIDE")
        if temp:
            self.data.feed_override_max = float(temp)
        else:
            self.add_alarm_entry(_("No MAX_FEED_OVERRIDE entry found in [DISPLAY] of INI file"))

        # if it's a lathe config, set the tooleditor style 
        self.data.lathe_mode = bool(self.inifile.find("DISPLAY", "LATHE"))
        if self.data.lathe_mode:
            self.add_alarm_entry(_("This screen will be orientated for Lathe options"))

        # get the path to the tool table
        self.data.tooltable = self.inifile.find("EMCIO","TOOL_TABLE")

        # see if the user specified a tool editor
        self.data.tooleditor = self.inifile.find("DISPLAY","TOOL_EDITOR")

        # see if the user specified a tool editor
        self.data.varfile = self.inifile.find("RS274NGC","PARAMETER_FILE")

        if "initialize_keybindings" in dir(self.handler_instance):
            self.handler_instance.initialize_keybindings()
        else:
            self.initialize_keybindings()

        # TODO the user should be able to invoke this so they know what methods are available
        # and what handers are registered
        #print handlers
        if "initialize_pins" in dir(self.handler_instance):
            self.handler_instance.initialize_pins()
        else:
            self.initialize_pins()

        if "initialize_manual_toolchange" in dir(self.handler_instance):
            self.handler_instance.initialize_manual_toolchange()
        else:
            self.initialize_manual_toolchange()

        if "connect_signals" in dir(self.handler_instance):
            self.handler_instance.connect_signals(handlers)
        else:
            self.connect_signals(handlers)

        # Set up the widgets
        if "initialize_widgets" in dir(self.handler_instance):
            self.handler_instance.initialize_widgets()
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
        temp = self.inifile.find("DISPLAY","CYCLE_TIME")
        if not temp:
            self.add_alarm_entry(_("CYCLE_TIME in [DISPLAY] of INI file is missing: defaulting to 100ms"))
            temp = 100
        elif float(temp) < 50:
            self.add_alarm_entry(_("CYCLE_TIME in [DISPLAY] of INI file is too small: defaulting to 100ms"))
            temp = 100
        print _("timeout %d" % int(temp))
        if "timer_interrupt" in dir(self.handler_instance):
            gobject.timeout_add(int(temp), self.handler_instance.timer_interrupt)
        else:
            gobject.timeout_add(int(temp), self.timer_interrupt)

    def initialize_keybindings(self):
        self.widgets.window1.connect('key_press_event', self.on_key_event,1)
        self.widgets.window1.connect('key_release_event', self.on_key_event,0)

    def initialize_preferences(self):
        self.init_dro_pref()
        self.init_theme_pref()
        self.init_window_geometry_pref()
        self.init_keybinding_pref()
        self.init_general_pref()

    def init_dro_pref(self):
        self.data.abs_textcolor = self.prefs.getpref('abs_textcolor', '#0000FFFF0000', str)
        self.data.dtg_textcolor = self.prefs.getpref('dtg_textcolor', '#00000000FFFF', str)
        self.data.rel_textcolor = self.prefs.getpref('rel_textcolor', '#FFFF00000000', str)
        self.data.show_dtg = self.prefs.getpref('show_dtg', False, bool)

    def init_theme_pref(self):
        self.data.theme_name = self.prefs.getpref('gtk_theme', 'Redmond', str)

    def init_window_geometry_pref(self):
        self.data.fullscreen1 = self.prefs.getpref('fullscreen1', False, bool)
        self.data.use_screen2 = self.prefs.getpref('use_screen2', False, bool)
        self.data.window_geometry = self.prefs.getpref('window_geometry', 'default', str)
        self.data.window_max = self.prefs.getpref('window_force_max', False, bool)
        self.data.window2_geometry = self.prefs.getpref('window2_geometry', 'default', str)
        self.data.window2_max = self.prefs.getpref('window2_force_max', False, bool)

    def init_keybinding_pref(self):
        self.keylookup.add_binding('Right', self.prefs.getpref('Key_Right', 'XPOS', str,"KEYCODES"))
        self.keylookup.add_binding('Left', self.prefs.getpref('Key_Left', 'XNEG', str,"KEYCODES"))
        self.keylookup.add_binding('Up', self.prefs.getpref('Key_Up', 'YPOS', str,"KEYCODES"))
        self.keylookup.add_binding('Down', self.prefs.getpref('Key_Down', 'YNEG', str,"KEYCODES"))
        self.keylookup.add_binding('Page_Up', self.prefs.getpref('Key_Page_Up', 'ZPOS', str,"KEYCODES"))
        self.keylookup.add_binding('Page_Down', self.prefs.getpref('Key_Page_Down', 'ZNEG', str,"KEYCODES"))
        self.keylookup.add_binding('bracketleft', self.prefs.getpref('Key_bracketleft', 'APOS', str,"KEYCODES"))
        self.keylookup.add_binding('bracketright', self.prefs.getpref('Key_bracketright', 'ANEG', str,"KEYCODES"))
        self.keylookup.add_binding('i', self.prefs.getpref('Key_i', 'INCREMENTS', str,"KEYCODES"))
        self.keylookup.add_binding('I', self.prefs.getpref('Key_I', 'INCREMENTS', str,"KEYCODES"))
        self.keylookup.add_binding('F1', self.prefs.getpref('Key_F1', 'ESTOP', str,"KEYCODES"))
        self.keylookup.add_binding('F2', self.prefs.getpref('Key_F2', 'POWER', str,"KEYCODES"))
        self.keylookup.add_binding('Escape', self.prefs.getpref('Key_Escape', 'ABORT', str,"KEYCODES"))

    def init_general_pref(self):
        self.data.alert_sound = self.prefs.getpref('audio_alert', self.data.alert_sound, str)
        self.data.desktop_notify = self.prefs.getpref('desktop_notify', True, bool)
        self.data.diameter_mode = self.prefs.getpref('diameter_mode', False, bool)
        self.data.display_order = self.prefs.getpref('display_order', (0,1,2), repr)
        self.data.dro_units = self.prefs.getpref('dro_is_metric', False, bool)
        if self.data.dro_units: # set linuxcnc as well
            self.status.dro_mm(0)
        else:
            self.status.dro_inch(0)
        self.data.error_sound = self.prefs.getpref('audio_error', self.data.error_sound, str)
        self.data.error_font_name = self.prefs.getpref('error_font', 'Sans Bold 10', str)
        self.data.err_textcolor = self.prefs.getpref('err_textcolor', 'default', str)
        self.data.grid_size = self.prefs.getpref('grid_size', 1.0 , float)
        self.data.hide_cursor = self.prefs.getpref('hide_cursor', False, bool)
        self.data.plot_view = self.prefs.getpref('view', ("p","x","y","y2","z","z2"), repr)
        self.data.show_offsets = self.prefs.getpref('show_offsets', True, bool)
        self.data.spindle_start_rpm = self.prefs.getpref('spindle_start_rpm', 300 , float)
        self.data.unlock_code = self.prefs.getpref('unlock_code', '123', str)
        self.data.embedded_keyboard = self.prefs.getpref('embedded_keyboard', True, bool)
        if self.prefs.getpref('dro_actual', False, bool):
           self.status.dro_actual(0)
        else:
           self.status.dro_commanded(0)
        # toolsetting reference type
        if self.prefs.getpref('toolsetting_fixture', False, bool):
            self.g10l11 = 1
        else:
            self.g10l11 = 0





    # initialize default widgets
    def initialize_widgets(self):
        self.init_show_windows()
        self.init_dynamic_tabs()
        self.init_axis_frames()
        self.init_dro_colors()
        self.init_screen2()
        self.init_fullscreen1()
        self.init_gremlin()
        self.init_manual_spindle_controls()
        self.init_dro()
        self.init_audio()
        self.init_desktop_notify()
        self.init_statusbar()
        self.init_entry()
        self.init_tooleditor()
        self.init_offsetpage()
        self.init_embeded_terminal()
        self.init_themes()
        self.init_screen1_geometry()
        self.init_running_options()
        self.init_mode()
        self.init_sensitive_on_off()
        self.init_sensitive_run_idle()
        self.init_sensitive_all_homed()
        self.init_sensitive_edit_mode()
        self.init_sensitive_override_mode()
        self.init_sensitive_graphics_mode()
        self.init_sensitive_origin_mode()
        self.init_hide_cursor()

        self.init_state()

    def show_try_errors(self):
        global verbose_debug
        if verbose_debug:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            formatted_lines = traceback.format_exc().splitlines()
            print
            print "****Gscreen verbose debugging:",formatted_lines[0]
            traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
            print formatted_lines[-1]

    def verbosely_print(self,data):
        global verbose_debug
        if verbose_debug:
            print data

    def init_axis_frames(self):
        for letter in ('x','y','z','a','b','c','u','v','w'):
            try:
                frame_for_letter = eval("self.widgets." + 'frame_' + letter)
            except:
                self.show_try_errors()
                continue
            if letter in self.data.axis_list:
                frame_for_letter.show()
                # don't show image6 when axes other than xyz are present
                if letter in ('a','b','c','u','v','w'):
                    self.widgets.image6.hide() ;# make more room for axis display
            else:
                # hide unneeded frames for axes not in use
                frame_for_letter.hide() ;# frame not relevant
        if self.data.rotary_joints:
            try:
                self.widgets.button_select_rotary_adjust.show()
                self.widgets.angular_jog_increments.show()
                self.widgets.angular_jog_rate.show()
            except:
                self.show_try_errors()

    def init_dynamic_tabs(self):
        # dynamic tabs setup
        self._dynamic_childs = {}
        atexit.register(self.kill_dynamic_childs)
        self.set_dynamic_tabs()

    def init_dro_colors(self):
        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.data.abs_textcolor))
        self.set_abs_color()
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.data.rel_textcolor))
        self.set_rel_color()
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.data.dtg_textcolor))
        self.set_dtg_color()

    def init_screen2(self):
        self.widgets.use_screen2.set_active(self.data.use_screen2)

    def init_fullscreen1(self):
        self.widgets.fullscreen1.set_active(self.data.fullscreen1)

    def init_gremlin(self):
        self.widgets.show_offsets.set_active( self.data.show_offsets )
        self.widgets.gremlin.show_offsets = self.data.show_offsets
        self.widgets.grid_size.set_value(self.data.grid_size) 
        self.widgets.gremlin.grid_size = self.data.grid_size
        self.widgets.gremlin.set_property('view',self.data.plot_view[0])
        self.widgets.gremlin.set_property('metric_units',(self.data.dro_units == self.data._MM))

    def init_manual_spindle_controls(self):
        self.widgets.spindle_start_rpm.set_value(self.data.spindle_start_rpm)
        self.block("s_display_fwd")
        self.widgets.s_display_fwd.set_active(True)
        self.unblock("s_display_fwd")
        self.preset_spindle_speed(self.data.spindle_start_rpm)

    def init_dro(self):
        self.widgets.diameter_mode.set_active(self.data.diameter_mode)
        self.widgets.dro_units.set_active(self.data.dro_units)

    def init_audio(self):
        self.widgets.audio_alert_chooser.set_filename(self.data.alert_sound)
        self.widgets.audio_error_chooser.set_filename(self.data.error_sound)

    def init_desktop_notify(self):
        self.widgets.desktop_notify.set_active(self.data.desktop_notify)

    def init_statusbar(self):
        self.statusbar_id = self.widgets.statusbar1.get_context_id("Statusbar1")
        self.homed_status_message = self.widgets.statusbar1.push(1,"Ready For Homing")

    def init_entry(self):
        return

    # first we hide all the axis columns the unhide the ones we want
    # if it's a lathe config we show lathe related columns
    # and we load the tooltable data
    def init_tooleditor(self):
        self.widgets.tooledit1.set_visible("abcxyzuvwijq",False)
        for axis in self.data.axis_list:
            self.widgets.tooledit1.set_visible("%s"%axis,True)
        if self.data.lathe_mode:
            self.widgets.tooledit1.set_visible("ijq",True)
        path = os.path.join(CONFIGPATH,self.data.tooltable)
        self.widgets.tooledit1.set_filename(path)

    # Only show the rows of the axes we use
    # set the var path so offsetpage can fill in all the user system offsets
    def init_offsetpage(self):
        self.widgets.offsetpage1.set_col_visible('xyzabcuvw',False)
        temp =""
        for axis in self.data.axis_list:
            temp=temp+axis
        self.widgets.offsetpage1.set_col_visible(temp,True)
        path = os.path.join(CONFIGPATH,self.data.varfile)
        self.widgets.offsetpage1.set_filename(path)

    def init_embeded_terminal(self):
        # add terminal window
        self.widgets._terminal = vte.Terminal ()
        self.widgets._terminal.connect ("child-exited", lambda term: gtk.main_quit())
        self.widgets._terminal.fork_command()
        self.widgets._terminal.show()
        window = self.widgets.terminal_window.add(self.widgets._terminal)
        self.widgets.terminal_window.connect('delete-event', lambda window, event: gtk.main_quit())
        self.widgets.terminal_window.show()

    def init_themes(self):
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

    def init_screen1_geometry(self):
        # maximize window or set geometry and optionally maximize 
        if "max" in self.data.window_geometry:
		    self.widgets.window1.maximize()
        elif self.data.window_geometry == "default":
            self.show_try_errors()
        else:
            good = self.widgets.window1.parse_geometry(self.data.window_geometry)
            if self.data.window_max:
               self.widgets.window1.maximize()
            if not good:
                print _("**** WARNING GSCREEN: could not understand the window geometry info in hidden preference file")
        if self.widgets.fullscreen1.get_active():
            self.widgets.window1.fullscreen()

    def init_running_options(self):
        self.widgets.button_block_delete.set_active(self.prefs.getpref('blockdel', False))
        self.emc.blockdel(self.data.block_del)
        self.widgets.button_option_stop.set_active(self.prefs.getpref('opstop', False))
        self.emc.opstop(self.data.op_stop)

    def init_hide_cursor(self):
        self.widgets.hide_cursor.set_active(self.data.hide_cursor)
        # hide cursor if requested
        # that also sets the graphics to use touchscreen controls
        if self.data.hide_cursor:
            self.widgets.window1.window.set_cursor(INVISABLE)
            self.widgets.gremlin.set_property('use_default_controls',False)
        else:
            self.widgets.window1.window.set_cursor(None)
            self.widgets.gremlin.set_property('use_default_controls',True)

    def init_mode(self):
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        # set to 'manual mode' 
        self.mode_changed(self.data.mode_order[0])

    # buttons that need to be sensitive based on the machine being on or off
    def init_sensitive_on_off(self):
        self.data.sensitive_on_off = ["vmode0","mode0","mode1","button_homing","button_override","button_graphics","frame_s","button_mode","button_restart"]
        for axis in self.data.axis_list:
            self.data.sensitive_on_off.append("axis_%s"% axis)

    # buttons that need to be sensitive based on the interpeter runing or being idle
    def init_sensitive_run_idle(self):
        self.data.sensitive_run_idle = ["button_edit","button_load","button_mode","button_restart"]
        for axis in self.data.axis_list:
            self.data.sensitive_run_idle.append("axis_%s"% axis)

    def init_sensitive_all_homed(self):
        self.data.sensitive_all_homed = ["button_zero_origin","button_offset_origin","button_select_system","button_tool_set"]

    def init_sensitive_edit_mode(self):
        self.data.sensitive_edit_mode = ["button_mode","button_menu","button_graphics","button_override","button_restart","button_single_step","button_run",
            "ignore_limits"]

    def init_sensitive_override_mode(self):
        self.data.sensitive_override_mode = ["spindle_preset","spindle_control","spindle_increase","spindle_decrease","s_display_fwd",
            "s_display_rev","button_graphics","button_homing","button_mode","button_jog_mode","button_flood_coolant",
                "button_mist_coolant","button_tool_editor","button_tool_set"]
        for axis in self.data.axis_list:
            self.data.sensitive_override_mode.append("axis_%s"% axis)

    def init_sensitive_graphics_mode(self):
        self.data.sensitive_graphics_mode = ["button_override","button_homing","button_mode",
              "button_zero_origin","button_offset_origin","button_plus","button_minus","vmode0","button_tool_set"]
        for axis in self.data.axis_list:
            self.data.sensitive_graphics_mode.append("axis_%s"% axis)

    def init_sensitive_origin_mode(self):
        self.data.sensitive_origin_mode = ["button_override","button_graphics","button_homing","button_mode",
                "button_zero_origin","button_offset_origin","button_jog_mode","button_flood_coolant","button_mist_coolant","button_tool_editor","button_tool_set"]
        for axis in self.data.axis_list:
            self.data.sensitive_origin_mode.append("axis_%s"% axis)

    # this needs to be last as it causes methods to be called (eg to sensitize buttons)
    def init_state(self):
        for num,i in enumerate(self.data.jog_increments):
            if i == "continuous": break
        self.data.current_jogincr_index = num
        try:
            jogincr = self.data.jog_increments[self.data.current_jogincr_index]
            self.widgets.jog_increments.set_text(jogincr)
        except:
            self.show_try_errors()
        try:
            for num,i in enumerate(self.data.angular_jog_increments):
                if i == "continuous": break
            self.data.current_angular_jogincr_index = num
            jogincr = self.data.angular_jog_increments[self.data.current_angular_jogincr_index]
            self.widgets.angular_jog_increments.set_text(jogincr)
        except:
            self.show_try_errors()
        self.on_hal_status_state_off(None)
        try:
            self.widgets.search_box.hide()
        except:
            self.show_try_errors()
        self.add_alarm_entry(_("Control powered up and initialized"))

    def init_show_windows(self):
        # set title and display everything including the second screen if requested
        if self.skinname == "gscreen":
            title = "Gscreen"
        else:
            title = "Gscreen-%s"% self.skinname
        self.widgets.window1.set_title("%s for linuxcnc"% title)
        if self.screen2:
            self.widgets.window2.show()
            self.widgets.window2.move(0,0)
            if not self.data.use_screen2:
                self.widgets.window2.hide()
        self.widgets.window1.show()

    def init_unlock_code(self):
        print "unlock code #",int(self.data.unlock_code)
        self.widgets.unlock_number.set_value(int(self.data.unlock_code))

    # general call to initialize HAL pins
    # select this if you want all the default pins or select each call for 
    # which ones you want
    def initialize_pins(self):
        self.init_spindle_pins()
        self.init_coolant_pins()
        self.init_jog_pins()
        self.init_override_pins()
        self.init_control_pins()

    def init_spindle_pins(self):
        self.halcomp.newpin("spindle-readout-in", hal.HAL_FLOAT, hal.HAL_IN)

    def init_coolant_pins(self):
        self.halcomp.newpin("aux-coolant-m7-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("aux-coolant-m8-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("mist-coolant-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("flood-coolant-out", hal.HAL_BIT, hal.HAL_OUT)

    def init_jog_pins(self):
        for axis in self.data.axis_list:
            self.halcomp.newpin("jog-enable-%s-out"% (axis), hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("jog-enable-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("jog-increment-out", hal.HAL_FLOAT, hal.HAL_OUT)
        #self.data['jog-increment-in'] = hal_glib.GPin(self.halcomp.newpin('jog-increment-in', hal.HAL_S32, hal.HAL_IN))
        #self.data['jog-increment-in'].connect('value-changed', self.on_hal_jog_increments_changed)
        #self.data['jog-rate-in'] = hal_glib.GPin(self.halcomp.newpin('jog-rate-in', hal.HAL_S32, hal.HAL_IN))
        #self.data['jog-rate-in'].connect('value-changed', self.on_hal_jog_rate_changed)

    # pins used for selecting an encoder to adjust overrides
    def init_override_pins(self):
        self.halcomp.newpin("s-override-enable-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("f-override-enable-out", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("mv-override-enable-out", hal.HAL_BIT, hal.HAL_OUT)

    def init_control_pins(self):
        self.data['cycle_start'] = hal_glib.GPin(self.halcomp.newpin('cycle-start', hal.HAL_BIT, hal.HAL_IN))
        self.data['cycle_start'].connect('value-changed', self.on_cycle_start_changed)
        self.data['abort'] = hal_glib.GPin(self.halcomp.newpin('abort', hal.HAL_BIT, hal.HAL_IN))
        self.data['abort'].connect('value-changed', self.on_abort_changed)
        self.data['feed_hold'] = hal_glib.GPin(self.halcomp.newpin('feed-hold', hal.HAL_BIT, hal.HAL_IN))
        self.data['feed_hold'].connect('value-changed', self.on_feed_hold_changed)

    def initialize_manual_toolchange(self):
        # for manual tool change dialog
        self.halcomp.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)
        self.halcomp.newpin("tool-changed", hal.HAL_BIT, hal.HAL_OUT)
        self.data['change-tool'] = hal_glib.GPin(self.halcomp.newpin('change-tool', hal.HAL_BIT, hal.HAL_IN))
        # you can override manual tool change
        if "on_tool_change" in dir(self.handler_instance):
            self.data['change-tool'].connect('value-changed', self.handler_instance.on_tool_change)
        else:
            self.data['change-tool'].connect('value-changed', self.on_tool_change)

# *** GLADE callbacks ****

    def on_keycall_ABORT(self,state,SHIFT,CNTRL,ALT):
        if state: # only activate when pushed not when released
            self.widgets.hal_action_stop.emit("activate")
    def on_keycall_ESTOP(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.widgets.button_estop.emit('clicked')
    def on_keycall_POWER(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.widgets.button_machine_on.emit('clicked')
    def on_keycall_XPOS(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_X,1,state)
    def on_keycall_XNEG(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_X,0,state)
    def on_keycall_YPOS(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_Y,1,state)
    def on_keycall_YNEG(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_Y,0,state)
    def on_keycall_ZPOS(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_Z,1,state)
    def on_keycall_ZNEG(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_Z,0,state)
    def on_keycall_APOS(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_A,1,state)
    def on_keycall_ANEG(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            self.do_key_jog(_A,0,state)
    def on_keycall_INCREMENTS(self,state,SHIFT,CNTRL,ALT):
        if state: 
            if SHIFT:
                self.set_jog_increments(index_dir = -1)
            else:
                self.set_jog_increments(index_dir = 1)
    def on_keycall_(self,state,SHIFT,CNTRL,ALT):
        if self.data._MAN in self.check_mode(): # manual mode required
            pass

    def on_button_spindle_controls_clicked(self,widget):
        self.spindle_dialog()

    def on_button_select_rotary_adjust_clicked(self,widget):
        self.data.angular_jog_adjustment_flag = widget.get_active()
        print self.data.angular_jog_adjustment_flag

    def search_fwd(self,widget):
        self.widgets.gcode_view.text_search(direction=True,text=self.widgets.search_entry.get_text())

    def search_bwd(self,widget):
        self.widgets.gcode_view.text_search(direction=False,text=self.widgets.search_entry.get_text())

    def undo_edit(self,widget):
        self.widgets.gcode_view.undo()

    def redo_edit(self,widget):
        self.widgets.gcode_view.redo()

    def keypress(self,accelgroup, acceleratable, accel_key, accel_mods):
        print gtk.accelerator_name(accel_key,accel_mods),acceleratable,accel_mods,
        return True

    def on_key_event(self,widget, event,state):
        CNTRL = SHIFT = ALT = 0
        keyname = gtk.gdk.keyval_name(event.keyval)
        self.verbosely_print("Key %s (%d) was pressed state: %d last: %s" % (keyname, event.keyval,state, self.data.key_event_last))
        if event.state & gtk.gdk.CONTROL_MASK:
            CNTRL = 1
            self.verbosely_print("Control was being held down")
        if event.state & gtk.gdk.MOD1_MASK:
            ALT = 1
            self.verbosely_print("Alt was being held down")
        if event.state & gtk.gdk.SHIFT_MASK:
            SHIFT = 1
            self.verbosely_print("Shift was being held down")
        if keyname in( "Shift_L","Shift_R"): return True # ignore shift key press
        if self.data.key_event_last[0] == keyname and self.data.key_event_last[1] == state : return True
        self.data.key_event_last = keyname,state
        try:
            method = self.keylookup.convert(keyname)
            if method:
                try:
                    self.handler_instance[method](state,SHIFT,CNTRL,ALT)
                    return True
                except:
                    self[method](state,SHIFT,CNTRL,ALT) 
                    return True                             
        except:
            self.show_try_errors()

    def on_cycle_start_changed(self,hal_object):
        print "cycle start change"
        h = self.halcomp
        if not h["cycle-start"]: return
        if self.data.mode_order[0] == self.data._AUTO:
            self.add_alarm_entry(_("Cycle start pressed in AUTO mode"))
            self.widgets.hal_toggleaction_run.emit('activate')
        elif self.data.mode_order[0] == self.data._MDI:
            self.add_alarm_entry(_("Cycle start pressed in MDI mode"))
            self.widgets.hal_mdihistory.submit()

    def on_abort_changed(self,hal_object):
        print "abort change"
        h = self.halcomp
        if not h["abort"]: return
        self.widgets.hal_action_stop.emit("activate")

    def on_feed_hold_changed(self,hal_object):
        print "feed-hold change"
        h = self.halcomp
        self.widgets.hal_toggleaction_pause.set_active(h["feed-hold"])

    # Here we create a manual tool change dialog
    # This can be overridden in a handler file
    def on_tool_change(self,widget):
        h = self.halcomp
        c = h['change-tool']
        n = h['tool-number']
        cd = h['tool-changed']
        print "tool change",c,cd,n
        if c:
            message =  _("Please change to tool # %s, then click OK."% n)
            self.data.tool_message = self.notify(_("INFO:"),message,None)
            self.warning_dialog(message, True,pinname="TOOLCHANGE")
        else:
            h['tool-changed'] = False

    def on_spindle_speed_adjust(self,widget):
                # spindle increase /decrease controls
        if self.mdi_control.mdi_is_reading():
            self.notify(_("INFO:"),_("Can't start spindle manually while MDI busy"),INFO_ICON)
            return
        elif self.data.mode_order[0] == self.data._AUTO:
            self.notify(_("INFO:"),_("can't start spindle manually in Auto mode"),INFO_ICON)
            return
        if widget == self.widgets.spindle_increase:
            self.spindle_adjustment(True,True)
        elif widget == self.widgets.spindle_decrease:
            self.spindle_adjustment(False,True)

    # start the spindle according to preset rpm and direction buttons, unless interp is busy
    def on_spindle_control_clicked(self,*args):
        if self.mdi_control.mdi_is_reading():
            self.notify(_("INFO:"),_("Can't start spindle manually while MDI busy"),INFO_ICON)
            return
        elif self.data.mode_order[0] == self.data._AUTO:
            self.notify(_("INFO:"),_("can't start spindle manually in Auto mode"),INFO_ICON)
            return
        if not self.data.spindle_speed == 0:
            self.emc.spindle_off(1)
            return
        if not self.widgets.s_display_fwd.get_active() and not self.widgets.s_display_rev.get_active():
            self.notify(_("INFO:"),_("No direction selected for spindle"),INFO_ICON)
            return
        if self.widgets.s_display_fwd.get_active():
            self.adjust_spindle_rpm(self.data.spindle_preset,1)
        else:
            self.adjust_spindle_rpm(self.data.spindle_preset,-1)

    # dialog for setting the spindle preset speed
    def on_preset_spindle(self,*args):
        if self.data.preset_spindle_dialog: return
        label = gtk.Label(_("Spindle Speed Preset Entry"))
        label.modify_font(pango.FontDescription("sans 20"))
        self.data.preset_spindle_dialog = gtk.Dialog(_("Spindle Speed Preset Entry"),
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        calc = gladevcp.Calculator()
        self.data.preset_spindle_dialog.vbox.pack_start(label)
        self.data.preset_spindle_dialog.vbox.add(calc)
        calc.set_value("")
        calc.set_property("font","sans 20")
        calc.set_editable(True)
        calc.entry.connect("activate", lambda w : self.data.preset_spindle_dialog.emit('response',gtk.RESPONSE_ACCEPT))
        self.data.preset_spindle_dialog.parse_geometry("400x400")
        self.data.preset_spindle_dialog.set_decorated(False)
        self.data.preset_spindle_dialog.show_all()
        self.data.preset_spindle_dialog.connect("response", self.on_preset_spindle_return,calc)

    def on_preset_spindle_return(self,widget,result,calc):
        if result == gtk.RESPONSE_ACCEPT:
            data = calc.get_value()
            if data == None:
                return
            self.preset_spindle_speed(data)
        widget.destroy()
        self.data.preset_spindle_dialog = None

    # dialog for manually calling a tool
    def on_index_tool(self,*args):
        if self.data.index_tool_dialog: return
        self.data.index_tool_dialog = gtk.Dialog(_("Manual Tool Index Entry"),
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        label = gtk.Label(_("Manual Tool Index Entry"))
        label.modify_font(pango.FontDescription("sans 20"))
        self.data.index_tool_dialog.vbox.pack_start(label)
        calc = gladevcp.Calculator()
        self.data.index_tool_dialog.vbox.add(calc)
        calc.set_value("")
        calc.set_property("font","sans 20")
        calc.set_editable(True)
        calc.entry.connect("activate", lambda w : self.data.index_tool_dialog.emit('response',gtk.RESPONSE_ACCEPT))
        self.data.index_tool_dialog.parse_geometry("400x400")
        self.data.index_tool_dialog.show_all()
        calc.num_pad_only(True)
        calc.integer_entry_only(True)
        self.data.index_tool_dialog.connect("response", self.on_index_tool_return,calc)

    def on_index_tool_return(self,widget,result,calc):
        if result == gtk.RESPONSE_ACCEPT:
            raw = calc.get_value()
            try:
                tool = abs(int((raw)))
                self.mdi_control.index_tool(tool)
            except:
                return
        widget.destroy()
        self.data.index_tool_dialog = None

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
            if self.widgets.button_zoom.get_active():
                self.widgets.gremlin.start_continuous_zoom(event.y)
            elif self.widgets.button_rotate_v.get_active():
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
                if self.widgets.button_zoom.get_active():
                    self.widgets.gremlin.continuous_zoom(event.y)
                elif self.widgets.button_pan_v.get_active():
                    self.pan(event.x,event.y)
                elif self.widgets.button_pan_h.get_active():
                    self.pan(event.x,event.y)
                elif self.widgets.button_rotate_v.get_active():
                    self.rotate(event.x,event.y)
                elif self.widgets.button_rotate_h.get_active():
                    self.rotate(event.x,event.y)
            elif self.widgets.button_zoom.get_active() or self.widgets.button_pan_v.get_active():
                return
            elif self.widgets.button_pan_h.get_active() or self.widgets.button_rotate_v.get_active():
                return
            elif self.widgets.button_rotate_h.get_active():
                return
            else:
                self.widgets.gremlin.set_property('use_default_controls',True)
        else:
            self.widgets.gremlin.set_property('use_default_controls',not self.data.hide_cursor)

    # display calculator for input
    def launch_numerical_input(self,callback="on_numerical_entry_return",data=None,data2=None,title=_("Entry dialog")):
        if self.data.entry_dialog: return
        label = gtk.Label(title)
        label.modify_font(pango.FontDescription("sans 20"))
        self.data.entry_dialog = gtk.Dialog(title,
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        calc = gladevcp.Calculator()
        calc.set_editable(True)
        self.data.entry_dialog.vbox.pack_start(label)
        self.data.entry_dialog.vbox.add(calc)
        calc.set_value("")
        calc.set_property("font","sans 20")
        calc.entry.connect("activate", lambda w : self.data.entry_dialog.emit('response',gtk.RESPONSE_ACCEPT))
        self.data.entry_dialog.parse_geometry("400x400")
        #self.data.entry_dialog.set_decorated(False)
        if callback in dir(self.handler_instance):
            self.data.entry_dialog.connect("response", self.handler_instance[callback],calc,data,data2)
        else:
            self.data.entry_dialog.connect("response", self[callback],calc,data,data2)
        self.data.entry_dialog.show_all()

    def on_numerical_entry_return(self,widget,result,calc,userdata,userdata2):
        data = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            print "accept",data
            if data == None:
                data = 0
            self.widgets.statusbar1.push(1,"Last Calculation: %f"%data)
        widget.destroy()
        self.data.entry_dialog = None

    def on_offset_origin_entry_return(self,widget,result,calc,userdata,userdata2):
        value = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            if value == None:
                return
            # if an axis is selected then set it
            for axis in self.data.axis_list:
                if self.widgets["axis_%s"%axis].get_active():
                    print "set %s axis" %axis
                    if not axis == "s":
                        if axis in('a','b','c'):
                            pos = self.get_qualified_input(value,switch=_DEGREE_INPUT)
                        else:
                            pos = self.get_qualified_input(value)
                        self.mdi_control.set_axis(axis,pos)
                        self.reload_plot()
        widget.destroy()
        self.data.entry_dialog = None

    def on_tool_offset_entry_return(self,widget,result,calc,userdata,userdata2):
        value = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            if value == None:
                return
            # if an axis is selected then set it
            for axis in self.data.axis_list:
                if self.widgets["axis_%s"%axis].get_active():
                    print "tool %d, set in %s axis to- %f" %(self.data.tool_in_spindle,axis,value)
                    if axis in('a','b','c'):
                        pos = self.get_qualified_input(value,switch=_DEGREE_INPUT)
                    else:
                        pos = self.get_qualified_input(value)
                    self.mdi_control.touchoff(self.data.tool_in_spindle,axis,pos)
        widget.destroy()
        self.data.entry_dialog = None

    def on_adj_overrides_entry_return(self,widget,result,calc,userdata,userdata2):
        data = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            print "accept",data
            if data == None:
                return None
            self.adjustment_buttons(userdata,userdata2,data)
        widget.destroy()
        self.data.entry_dialog = None

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
        p = os.popen("tclsh %s/bin/halshow.tcl &" % (TCLPATH))

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

    # opens the halscope
    def on_halscope(self,*args):
        p = os.popen("halscope  > /dev/null &","w")

    def on_ladder(self,*args):
        if  hal.component_exists('classicladder_rt'):
            p = os.popen("classicladder  &","w")
        else:
            self.notify(_("INFO:"),_("Classicladder realtime component not detected"),INFO_ICON)
            self.add_alarm_entry(_("ladder not available - is the realtime component loaded?"))

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
            label = self.data.mode_labels
            self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
            self.mode_changed(self.data.mode_order[0])

    def on_button_show_offsets_clicked(self,widget):
        self.toggle_offset_view()

    # Horizontal buttons
    def on_button_home_all_clicked(self,widget):
        self.home_all()
    def on_button_unhome_all_clicked(self,widget):
        self.unhome_all()
    def on_button_home_axis_clicked(self,widget):
        self.home_selected()
    def on_button_unhome_axis_clicked(self,widget):
        self.unhome_selected()
    def on_button_toggle_readout_clicked(self,widget):
        self.dro_toggle()

    def on_button_jog_mode_clicked(self,widget):
        self.jog_mode()
    def on_button_select_system_clicked(self,widget):
        self.origin_system()
    def on_button_flood_coolant_clicked(self,widget):
        self.toggle_flood()
    def on_button_mist_coolant_clicked(self,widget):
        self.toggle_mist()
    def on_button_tool_editor_clicked(self,widget):
        self.reload_tooltable()

    def on_button_block_delete_clicked(self,widget):
        self.toggle_block_delete()
    def on_button_option_stop_clicked(self,widget):
        self.toggle_optional_stop()
    def on_button_next_tab_clicked(self,widget):
        self.next_tab()
    def on_button_overrides_clicked(self,widget,button):
        self.toggle_overrides(widget,button)

    def on_button_clear_view_clicked(self,widget):
        self.clear_plot()
    def on_graphic_overrides_clicked(self,widget,button):
        self.toggle_graphic_overrides(widget,button)

    # vertical buttons
    def on_button_plus_pressed(self,widget):
        self.adjustment_buttons(widget,True)
    def on_button_plus_released(self,widget):
        self.adjustment_buttons(widget,False)
    def on_button_minus_pressed(self,widget):
        self.adjustment_buttons(widget,True)
    def on_button_minus_released(self,widget):
        self.adjustment_buttons(widget,False)
    def on_offset_origin_clicked(self,widget):
        # adjust overrrides
        if self.widgets.button_override.get_active():
            self.launch_numerical_input("on_adj_overrides_entry_return",widget,True,title=_("Override Entry"))
        # offset origin
        else:
            self.set_axis_checks()
    def on_move_to_clicked(self,widget):
        # Move-to button
        # manual mode and jog mode active
        if self.data.mode_order[0] == self.data._MAN and self.widgets.button_jog_mode.get_active():
            self.launch_numerical_input("on_adj_overrides_entry_return",widget,True)

    def on_tool_touchoff_clicked(self,widget):
        print "touch"
        self.tool_touchoff_checks()

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
            self.add_alarm_entry(_("Machine powered on"))
        else:
            self.emc.machine_off(1)
            self.emc.estop(1)
            self.widgets.on_label.set_text("Machine Off")
            self.add_alarm_entry(_("Machine Estopped!"))

    def on_calc_clicked(self,widget):
        self.launch_numerical_input(title=_("Calculator"))

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

    def on_unlock_number_value_changed(self,widget):
        self.data.unlock_code = str(int(widget.get_value()))
        self.set_unlock_code()

    # True is for showing DTG
    def on_show_dtg_pressed(self, widget):
        self.set_show_dtg(widget.get_active())

    # True will use notify 
    def on_desktop_notify_toggled(self,widget):
        self.set_desktop_notify( widget.get_active())

    def on_pop_statusbar_clicked(self, *args):
        self.widgets.statusbar1.pop(self.statusbar_id)

    # This is part of the user message system
    # There is status that prints to the status bar
    # There is Okdialog that prints a dialog that the user must acknoledge
    # there is yes/no dialog where the user must choose between yes or no
    # you can combine status and dialog messages so they print to the status bar 
    # and pop a dialog
    def on_printmessage(self, pin, pinname,boldtext,text,type):
        if not pin.get(): return
        if boldtext == "NONE": boldtext = None
        if "status" in type:
            if boldtext:
                statustext = boldtext
            else:
                statustext = text
            self.notify(_("INFO:"),statustext,INFO_ICON)
        if "dialog" in type or "okdialog" in type:
            if pin.get():
                self.halcomp[pinname + "-waiting"] = True
            if "okdialog" in type:
                self.warning_dialog(boldtext,True,text,pinname)
            else:
                if pin.get():
                    self.halcomp[pinname + "-response"] = 0
                self.warning_dialog(boldtext,False,text,pinname)

    def toggle_overrides(self,widget,data):
        print "overrides - button_h_%s"% data
        list = ('jog_speed','jog_increments','feed_override','spindle_override','rapid_override')
        for i in (list):
            print i,data
            if i == data:continue
            button = "button_%s"% (i)
            print "block",button
            self.block(button)
            self.widgets[button].set_active(False)
            self.unblock(button)
        self.update_hal_override_pins()

    def toggle_graphic_overrides(self,widget,data):
        print "graphic overrides - button_%s"%data
        list = ('zoom','pan_v','pan_h','rotate_v','rotate_h')
        for i in (list):
            if i == data:continue
            button = "button_%s"% (i)
            self.block(button)
            self.widgets[button].set_active(False)
            self.unblock(button)

    def on_hal_status_interp_run(self,widget):
        print "run"
        self.sensitize_widgets(self.data.sensitive_run_idle,False)

    def on_hal_status_interp_idle(self,widget):
        print "idle"
        self.sensitize_widgets(self.data.sensitive_run_idle,True)
        state = self.data.all_homed
        self.sensitize_widgets(self.data.sensitive_all_homed,state)
        mode = self.emc.get_mode()
        print "mode",mode,self.data.mode_order[0]
        if self.data.mode_order[0] == self.data._MAN and not mode == 1:
            print "set to manual"
            self.emc.set_manual_mode()

    def on_hal_status_state_on(self,widget):
        print "on"
        self.sensitize_widgets(self.data.sensitive_on_off,True)
        state = self.data.all_homed
        self.sensitize_widgets(self.data.sensitive_all_homed,state)
        if not state:
            self.widgets.button_homing.emit("clicked")

    def on_hal_status_state_off(self,widget):
        print "off"
        self.sensitize_widgets(self.data.sensitive_on_off,False)

    def on_hal_status_axis_homed(self,widget,data):
        print "homed list",data
        temp=[]
        for letter in(self.data.axis_list):
            count = "xyzabcuvws".index(letter)
            if str(count) in data:
                temp.append(" %s"%letter.upper())
        self.add_alarm_entry(_("Axes %s are homed"%temp))

    def on_hal_status_all_homed(self,widget):
        print "all-homed"
        self.data.all_homed = True
        self.widgets.button_homing.set_active(False)
        self.widgets.statusbar1.remove_message(self.statusbar_id,self.homed_status_message)
        self.add_alarm_entry(_("All the axes have been homed"))

    def on_hal_status_not_all_homed(self,widget,data):
        print "not-all-homed",data
        self.data.all_homed = False
        temp =[]
        for letter in(self.data.axis_list):
            count = "xyzabcuvws".index(letter)
            if str(count) in data:
                temp.append(" %s"%letter.upper())
        self.add_alarm_entry(_("There are unhomed axes: %s"%temp))

    def on_hal_status_file_loaded(self,widget,filename):
        path,name = os.path.split(filename)
        self.widgets.gcode_tab.set_text(name)
        self.add_alarm_entry(_("Program loaded: %s"%filename))

    def on_toggle_keyboard(self,widget,args="",x="",y=""):
        if self.data.ob:
            self.kill_keyboard()
        else:
            self.launch_keyboard()
            if self.data.mode_order[0] == self.data._MDI:
                try:
                    self.widgets.hal_mdihistory.entry.grab_focus()
                except:
                    dbg("**** GSCREEN ERROR: can't set focus to hal_mdihistory when Onboard launched - Is this widget name in glade file?")
            elif self.data.mode_order[0] == self.data._AUTO:
                try:
                    self.widgets.gcode_view.grab_focus()
                except:
                    dbg("**** GSCREEN ERROR: can't set focus to gcode_view when Onboard launched - Is this widget name in glade file?")

    def on_hal_jog_increments_changed(self,halpin):
        print halpin.get()
        data = halpin.get()
        self.set_jog_increments(vector=data)

    def on_hal_jog_rate_changed(self,halpin):
        print halpin.get()
        data = halpin.get()
        self.set_jog_rate(absolute=data)

    # highlight the gcode down one line lower
    # used for run-at-line restart
    def restart_down(self,widget,calc):
        self.widgets.gcode_view.line_down()
        line = int(self.widgets.gcode_view.get_line_number())
        calc.set_value(line)
        self.update_restart_line(line)

    # highlight the gcode down one line higher
    # used for run-at-line restart
    def restart_up(self,widget,calc):
        self.widgets.gcode_view.line_up()
        line = int(self.widgets.gcode_view.get_line_number())
        calc.set_value(line)
        self.update_restart_line(line)

    # highlight the gcode line specified
    # used for run-at-line restart
    def restart_set_line(self,widget,calc):
        try:
            line = int(calc.get_value())
        except:
            calc.set_value("0.0")
            line = 0
        self.widgets.gcode_view.set_line_number(line)
        self.update_restart_line(line)

    # This is a method that toggles the DRO units
    # the preference unit button saves the state
    # for startup, This one just changes it for the session
    def on_metric_select_clicked(self,widget):
        data = (self.data.dro_units -1) * -1
        self.set_dro_units(data,False)

    def on_button_edit_clicked(self,widget):
        state = widget.get_active()
        if not state:
            self.edited_gcode_check()
        self.widgets.notebook_main.set_current_page(0)
        self.widgets.notebook_main.set_show_tabs(not (state))
        self.edit_mode(state)
        if not state and self.widgets.button_full_view.get_active():
            self.set_full_graphics_view(True)
        if state:
            self.widgets.search_box.show()
        else:
            self.widgets.search_box.hide()

    def on_button_change_view_clicked(self,widget):
        self.toggle_view()

    def on_button_full_view_clicked(self,widget):
        self.set_full_graphics_view(widget.get_active())

# ****** do stuff *****

    def check_mode(self):
        try:
            return self.handler_instance.check_mode()
        except:
            pass
        string=[]
        if self.data.mode_order[0] == self.data._MAN and self.widgets.notebook_main.get_current_page() == 0:
            string.append( self.data._MAN)
            if self.widgets.button_jog_mode.get_active(): # jog mode active
                string.append(self.data._JOG)
        return string

    def spindle_dialog(self):
        if not self.data.spindle_control_dialog:
            self.data.spindle_control_dialog = gtk.Dialog(_("Manual Spindle Control"),
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CLOSE, gtk.RESPONSE_REJECT))
            self.data.spindle_control_dialog.vbox.add(self.widgets.frame_s)
            self.data.spindle_control_dialog.parse_geometry("200x200")
            self.data.spindle_control_dialog.connect("delete_event", self.spindle_dialog_return)
            self.data.spindle_control_dialog.connect("response",  self.spindle_dialog_return)
        self.data.spindle_control_dialog.show_all()

    def spindle_dialog_return(self,widget,signal):
        self.data.spindle_control_dialog.hide()
        return True

    def update_restart_line(self,line):
        if "set_restart_line" in dir(self.handler_instance):
            self.handler_instance.set_restart_line(line)
        else:
            self.set_restart_line(line)

    def set_restart_line(self,line):
        self.widgets.hal_toggleaction_run.set_restart_line(line)

    def edited_gcode_check(self):
        if self.widgets.gcode_view.buf.get_modified():
                dialog = gtk.MessageDialog(self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,"You edited the File. save edits?\n Choosing No will erase the edits.")
                dialog.show_all()
                result = dialog.run()
                dialog.destroy()
                if result == gtk.RESPONSE_YES:
                    self.widgets.hal_action_saveas.emit("activate")
                else:
                    self.widgets.gcode_view.load_file()

    def set_desktop_notify(self,data):
        self.data.desktop_notify = data
        self.prefs.putpref('desktop_notify', data, bool)

    # shows 'Onboard' virtual keyboard if available
    # check for key_box widget - if there is, and embedded flag, embed Onboard in it.
    # else launch an independant Onboard inside a dialog so it works in fullscreen
    # (otherwise it hides when main screen is touched)
    # else error message
    def launch_keyboard(self,args="",x="",y=""):
        print args,x,y
        def dialog_keyboard():
            if self.data.keyboard_dialog:
                self.data.keyboard_dialog.show()
                self.data.ob = True
            else:
                self.data.ob = subprocess.Popen(["onboard","--xid",args,x,y],
                                       stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE,
                                       close_fds=True)
                sid = self.data.ob.stdout.readline()
                self.data.keyboard_dialog = gtk.Dialog(_("Keyboard"),
                           self.widgets.window1,
                           gtk.DIALOG_DESTROY_WITH_PARENT)
                self.data.keyboard_dialog.set_accept_focus(False)
                self.data.keyboard_dialog.set_deletable(False)
                socket = gtk.Socket()
                socket.show()
                self.data.keyboard_dialog.vbox.add(socket)
                socket.add_id(long(sid))
                self.data.keyboard_dialog.parse_geometry("800x200")
                self.data.keyboard_dialog.show_all()
                self.data.keyboard_dialog.connect("destroy", self.keyboard_return)

        try:
            if self.widgets.key_box and self.data.embedded_keyboard:
                self.widgets.rightside_box.show()
                self.widgets.key_box.show()
                self.data.ob = subprocess.Popen(["onboard","--xid",args,x,y],
                                       stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE,
                                       close_fds=True)
                sid = self.data.ob.stdout.readline()
                print"keyboard", sid # skip header line
                socket = gtk.Socket()
                socket.show()
                self.widgets.key_box.add(socket)
                socket.add_id(long(sid))
            else:
                dialog_keyboard()
        except:
            try:
                dialog_keyboard()
            except:
                print _("Error with launching 'Onboard' on-screen keyboard program")

    # seems the only way to trap the destroy signal
    def keyboard_return(self,widget):
        self.data.keyboard_dialog = None
        self.data.ob = None

    # if keyboard in dialog just hide it
    # else kill it and if needed hide the key_box
    def kill_keyboard(self):
        if not self.data.keyboard_dialog == None:
            self.data.keyboard_dialog.hide()
            self.data.ob = None
            return
        try:
            self.widgets.key_box.hide()
            self.data.ob.kill()
            self.data.ob.terminate()
            self.data.ob = None
        except:
            try:
                self.data.ob.kill()
                self.data.ob.terminate()
                self.data.ob = None
            except:
                self.show_try_errors()

    # this installs local signals unless overriden by custom handlers
    # HAL pin signal call-backs are covered in the HAL pin initilization functions
    def connect_signals(self, handlers):

        signal_list = [
                        ["","button_estop","clicked", "on_estop_clicked"],
                        ["","gremlin","motion-notify-event", "on_gremlin_motion"],
                        ["","button_mode","button_press_event", "on_mode_clicked"],
                        ["","button_menu","button_press_event", "on_mode_select_clicked"],
                        ["","button_plus","pressed", "on_button_plus_pressed"],
                        ["","button_plus","released", "on_button_plus_released"],
                        ["","button_minus","pressed", "on_button_minus_pressed"],
                        ["","button_minus","released", "on_button_minus_released"],
                        ["","button_zero_origin","clicked", "adjustment_buttons",True],
                        ["","button_offset_origin","clicked", "on_offset_origin_clicked"],

                        ["","button_move_to","clicked", "on_move_to_clicked"],
                        ["","button_tool_set","clicked", "on_tool_touchoff_clicked"],
                        ["","button_change_view","clicked", "on_button_change_view_clicked"],
                        ["","button_full_view","clicked", "on_button_full_view_clicked"],
                        ["","button_home_all","clicked", "on_button_home_all_clicked"],
                        ["","button_unhome_all","clicked", "on_button_unhome_all_clicked"],
                        ["","button_home_axis","clicked", "on_button_home_axis_clicked"],
                        ["","button_unhome_axis","clicked", "on_button_unhome_axis_clicked"],
                        ["","button_toggle_readout","clicked", "on_button_toggle_readout_clicked"],
                        ["","button_jog_mode","clicked", "on_button_jog_mode_clicked"],
                        ["","button_spindle_controls","clicked", "on_button_spindle_controls_clicked"],

                        ["","button_select_system","clicked", "on_button_select_system_clicked"],
                        ["","button_mist_coolant","clicked", "on_button_mist_coolant_clicked"],
                        ["","button_flood_coolant","clicked", "on_button_flood_coolant_clicked"],
                        ["","button_tool_editor","clicked", "on_button_tool_editor_clicked"],
                        ["","button_toggle_readout2","clicked", "on_button_toggle_readout_clicked"],
                        ["","button_show_offsets","clicked", "on_button_show_offsets_clicked"],
                        ["","button_block_delete","clicked", "on_button_block_delete_clicked"],
                        ["","button_option_stop","clicked", "on_button_option_stop_clicked"],
                        ["","button_next_tab","clicked", "on_button_next_tab_clicked"],
                        ["","button_calc","clicked", "on_calc_clicked"],
                ["block","button_jog_speed","clicked", "on_button_overrides_clicked","jog_speed"],
                        ["","button_select_rotary_adjust","clicked", "on_button_select_rotary_adjust_clicked"],

                ["block","button_jog_increments","clicked", "on_button_overrides_clicked","jog_increments"],
                ["block","button_feed_override","clicked", "on_button_overrides_clicked","feed_override"],
                ["block","button_spindle_override","clicked", "on_button_overrides_clicked","spindle_override"],
                ["block","button_rapid_override","clicked", "on_button_overrides_clicked","rapid_override"],
                ["block","button_zoom","clicked", "on_graphic_overrides_clicked","zoom"],
                ["block","button_pan_v","clicked", "on_graphic_overrides_clicked","pan_v"],
                ["block","button_pan_h","clicked", "on_graphic_overrides_clicked","pan_h"],
                ["block","button_rotate_v","clicked", "on_graphic_overrides_clicked","rotate_v"],
                ["block","button_rotate_h","clicked", "on_graphic_overrides_clicked","rotate_h"],
                        ["","button_clear_view","clicked", "on_button_clear_view_clicked"],

                        ["","theme_choice","changed", "on_theme_choice_changed"],
                        ["","use_screen2","clicked", "on_use_screen2_pressed"],
                        ["","dro_units","clicked", "on_dro_units_pressed"],
                        ["","diameter_mode","clicked", "on_diameter_mode_pressed"],
                        ["","button_edit","toggled", "on_button_edit_clicked"],
                        ["","show_offsets","clicked", "on_show_offsets_pressed"],
                        ["","show_dtg","clicked", "on_show_dtg_pressed"],
                        ["","fullscreen1","clicked", "on_fullscreen1_pressed"],
                        ["","shut_down","clicked", "on_window1_destroy"],
                        ["","shut_down",'released',"hack_leave"],

                        ["","run_halshow","clicked", "on_halshow"],
                        ["","run_calibration","clicked", "on_calibration"],
                        ["","run_status","clicked", "on_status"],
                        ["","run_halmeter","clicked", "on_halmeter"],
                        ["","run_halscope","clicked", "on_halscope"],
                        ["","run_ladder","clicked", "on_ladder"],
                        ["","hide_cursor","clicked", "on_hide_cursor"],
                        ["","button_homing","clicked", "homing"],
                        ["","button_override","clicked", "override"],
                        ["","button_graphics","clicked", "graphics"],

                        ["","desktop_notify","toggled", "on_desktop_notify_toggled"],
                        ["","grid_size","value_changed", "set_grid_size"],
                        ["","spindle_start_rpm","value_changed", "set_spindle_start_rpm"],
                        ["","spindle_control","clicked", "on_spindle_control_clicked"],
                        ["","spindle_preset","clicked", "on_preset_spindle"],
                        ["","spindle_increase","clicked", "on_spindle_speed_adjust"],
                        ["","spindle_decrease","clicked", "on_spindle_speed_adjust"],
                        ["","audio_error_chooser","update-preview", "update_preview"],
                        ["","audio_alert_chooser","update-preview", "update_preview"],
                        ["","hal_status","interp-idle", "on_hal_status_interp_idle"],

                        ["","hal_status","interp-run", "on_hal_status_interp_run"],
                        ["","hal_status","state-on", "on_hal_status_state_on"],
                        ["","hal_status","state-off", "on_hal_status_state_off"],
                        ["","hal_status","homed", "on_hal_status_axis_homed"],
                        ["","hal_status","all-homed", "on_hal_status_all_homed"],
                        ["","hal_status","not-all-homed", "on_hal_status_not_all_homed"],
                        ["","hal_status","file-loaded", "on_hal_status_file_loaded"],
                        ["","window1","destroy", "on_window1_destroy"],
                        ["","pop_statusbar","clicked", "on_pop_statusbar_clicked"],
                        ["","dtg_colorbutton","color-set", "on_dtg_colorbutton_color_set"],

                        ["","abs_colorbutton","color-set", "on_abs_colorbutton_color_set"],
                        ["","rel_colorbutton","color-set", "on_rel_colorbutton_color_set"],
                        ["","eventbox_gremlin","leave_notify_event", "on_eventbox_gremlin_leave_notify_event"],
                        ["","eventbox_gremlin","enter_notify_event", "on_eventbox_gremlin_enter_notify_event"],
                ["block","s_display_rev","toggled", "on_s_display_rev_toggled"],
                ["block","s_display_fwd","toggled", "on_s_display_fwd_toggled"],
                        ["","ignore_limits","clicked", "toggle_ignore_limits"],
                        ["","audio_error_chooser","selection_changed","change_sound","error"],
                        ["","audio_alert_chooser","selection_changed","change_sound","alert"],
                        ["","toggle_keyboard","clicked", "on_toggle_keyboard"],

                        ["","metric_select","clicked","on_metric_select_clicked"],
                        ["","button_restart","clicked", "launch_restart_dialog"],
                        ["","button_index_tool","clicked", "on_index_tool"],

                        ["","button_search_fwd","clicked", "search_fwd"],
                        ["","button_search_bwd","clicked", "search_bwd"],
                        ["","button_undo","clicked", "undo_edit"],
                        ["","button_redo","clicked", "redo_edit"],]

        # check to see if the calls in the signal list are in the custom handler's list of calls
        # if so skip the call in the signal list
        # else connect the signals based on how many arguments they have and if blockable
        for i in signal_list:
            if i[3] in handlers:
                print _("**** GSCREEN INFO: Overriding internal signal call to %s"% i[3])
                continue
            try:
                # add id # for blockable signals
                if  i[0] == "block":
                    j = "_sighandler_%s"% i[1]
                    if len(i) == 4:
                        self.data[j] = int(self.widgets[i[1]].connect(i[2], self[i[3]]))
                    if len(i) == 5:
                        self.data[j] = int(self.widgets[i[1]].connect(i[2], self[i[3]],i[4]))
                elif len(i) == 4:
                        self.widgets[i[1]].connect(i[2], self[i[3]])
                elif len(i) == 5:
                    self.widgets[i[1]].connect(i[2], self[i[3]],i[4])
            except:
                print ("**** GSCREEN WARNING: could not connect %s to %s"% (i[1],i[3]))

        # setup signals that can be blocked but not overriden 
        for axis in self.data.axis_list:
            cb = "axis_%s"% axis
            i = "_sighandler_axis_%s"% axis
            try:
                self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_selection_clicked))
            except:
                self.show_try_errors()

    def toggle_offset_view(self):
            data = self.data.plot_hidden
            data = (data * -1) +1
            self.widgets.dro_frame.set_visible(not data)
            self.widgets.gremlin.set_property('enable_dro', data)
            self.widgets.gremlin.set_property('show_program', not data)
            self.widgets.gremlin.set_property('show_limits', not data)
            self.widgets.gremlin.set_property('show_extents_option', not data)
            self.widgets.gremlin.set_property('show_live_plot', not data)
            self.widgets.gremlin.set_property('show_tool', not data)
            self.widgets.gremlin.show_offsets = data
            self.data.plot_hidden = data

    def preset_spindle_speed(self,rpm):
        self.data.spindle_preset = rpm
        self.widgets.spindle_preset.set_label(" S %d"% rpm)

    def sensitize_widgets(self, widgetlist, value):
        for name in widgetlist:
            try:
                self.widgets[name].set_sensitive(value)
            except:
                print "**** GSCREEN WARNING: No widget named: %s to sensitize"%name

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

    # This prints a message in the status bar, the system notifier (if available)
    # and makes a sound (if available)
    # It returns a statusbar message id reference so one can directly erase the message later.
    # Ubuntu screws with the system notification program so it does follow timeouts
    # There is a ppa on the net to fix this I suggest it.
    # https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
    def notify(self,title,message,icon="",timeout=2):
            messageid = None
            try:
                messageid = self.widgets.statusbar1.push(self.statusbar_id,message)
            except:
                self.show_try_errors()
            self.add_alarm_entry(message)
            if NOTIFY_AVAILABLE and self.data.desktop_notify:
                uri = ""
                if icon:
                    uri = "file://" + icon
                n = pynotify.Notification(title, message, uri)
                n.set_hint_string("x-canonical-append","True")
                n.set_urgency(pynotify.URGENCY_CRITICAL)
                n.set_timeout(int(timeout * 1000) )
                n.show()
            if _AUDIO_AVAILABLE:
                if icon == ALERT_ICON:
                    self.audio.set_sound(self.data.error_sound)
                else:
                    self.audio.set_sound(self.data.alert_sound)
                self.audio.run()
            return messageid

    def add_alarm_entry(self,message):
        try:
            textbuffer = self.widgets.alarm_history.get_buffer()
            textbuffer.insert_at_cursor(strftime("%a, %d %b %Y %H:%M:%S     -", localtime())+message+"\n" )
        except:
            self.show_try_errors()

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

    def set_jog_rate(self,step=None,absolute=None):
        if self.data.angular_jog_adjustment_flag:
            j_rate = "angular_jog_rate"
        else:
            j_rate = "jog_rate"
        # in units per minute
        print "jog rate =",step,absolute,self.data[j_rate]
        if not absolute == None:
            rate = absolute
        elif not step == None:
            rate = self.data[j_rate] + step
        else:return
        if rate < 0: rate = 0
        if rate > self.data[j_rate+"_max"]: rate = self.data[j_rate+"_max"]
        rate = round(rate,1)
        if self.data.angular_jog_adjustment_flag:
            self.emc.continuous_jog_velocity(None,rate)
        else:
            self.emc.continuous_jog_velocity(rate,None)
        self.data[j_rate] = rate

    # This sets the jog increments -there are three ways
    # ABSOLUTE:
    # set absolute to the absolute increment wanted
    # INDEX from INI:
    # self.data.jog_increments holds the increments from the INI file
    # do not set absolute variable
    # index_dir = 1 or -1 to set the rate higher or lower from the list
    def set_jog_increments(self,vector=None,index_dir=None,absolute=None):
        print "set jog incr"
        if self.data.angular_jog_adjustment_flag:
            incr = "angular_jog_increments"
            incr_index = "current_angular_jogincr_index"
        else:
            incr = "jog_increments"
            incr_index = "current_jogincr_index"

        if not absolute == None:
            distance = absolute
            self.widgets[incr].set_text("%f"%distance)
            self.halcomp["jog-increment-out"] = distance
            print "index jog increments",distance
            return
        elif not index_dir == None:
            next = self.data[incr_index] + index_dir
        elif not vector == None:
            next = vector
        else: return
        end = len(self.data[incr])-1
        if next < 0: next = 0
        if next > end: next = end
        self.data[incr_index] = next
        jogincr = self.data[incr][next]
        try:
            if 'angular' in incr and not jogincr == 'continuous':
                label = jogincr + ' Degs'
            else:
                label = jogincr
            self.widgets[incr].set_text(label)
        except:
            self.show_try_errors()
        if jogincr == ("continuous"):
            distance = 0
        else:
            distance = self.parse_increment(jogincr)
        print "index jog increments",jogincr,distance
        self.halcomp["jog-increment-out"] = distance

    def adjustment_buttons(self,widget,action,change=0):
        print "adjustment buttons"
        # is over ride adjustment selection active?
        if self.widgets.button_override.get_active():
            print "override"
            if widget == self.widgets.button_zero_origin:
                print "zero button",action
                change = 0
                absolute = True
            elif widget == self.widgets.button_offset_origin:
                print "set at button",action
                absolute = True
            elif widget == self.widgets.button_plus:
                print "up button",action
                change = 1
                absolute = False
            elif widget == self.widgets.button_minus:
                print "down button",action
                change = -1
                absolute = False
            else:return
            self.adjust_overrides(widget,action,change,absolute)

        # graphics adjustment
        elif self.widgets.button_graphics.get_active():
            inc = self.data.graphic_move_inc
            if widget == self.widgets.button_plus:
                print "up button",action
                change = 1
            elif widget == self.widgets.button_minus:
                print "down button",action
                change = -1
            if self.widgets.button_zoom.get_active() and action:
                print "zoom"
                if change == 1: self.zoom_in()
                else: self.zoom_out()
            elif self.widgets.button_pan_v.get_active() and action:
                print "pan vertical"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.pan(0,-inc)
                else: self.pan(0,inc)
            elif self.widgets.button_pan_h.get_active() and action:
                print "pan horizontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.pan(-inc,0)
                else: self.pan(inc,0)
            elif self.widgets.button_rotate_v.get_active() and action:
                print "rotate horiontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.rotate(-inc,0)
                else: self.rotate(inc,0)
            elif self.widgets.button_rotate_h.get_active() and action:
                print "rotate horiontal"
                self.widgets.gremlin.set_mouse_start(0,0)
                if change == 1: self.rotate(0,-inc)
                else: self.rotate(0,inc)

        # user coordinate system
        elif self.widgets.button_select_system.get_active():
            if widget == self.widgets.button_plus and action:
                print "up button",action
                change = 1
            elif widget == self.widgets.button_minus and action:
                print "down button",action
                change = -1
            else: return
            self.change_origin_system(None,change)
        # Jogging mode (This needs to be last)
        elif self.data.mode_order[0] == self.data._MAN and self.widgets.button_jog_mode.get_active(): # manual mode and jog mode active
            # what axis is set
            if widget == self.widgets.button_zero_origin:
                print "zero button",action
                self.zero_axis()
            elif widget == self.widgets.button_move_to:
                print "move to button",action
                self.move_to(change)
            elif widget == self.widgets.button_plus:
                print "up button",action
                self.do_jog(True,action)
            elif widget == self.widgets.button_minus:
                print "down button",action
                self.do_jog(False,action)
            elif widget == self.widgets.button_offset_origin:
                self.set_axis_checks()
        elif widget == self.widgets.button_zero_origin:
            print "zero buttons"
            self.zero_axis()
        elif widget == self.widgets.button_offset_origin:
            print "set axis buttons"
            self.set_axis_checks()

    def adjust_overrides(self,widget,action,number,absolute):
            print "adjust overrides",action,number,absolute
            # what override is selected
            if absolute:
                change = self.get_qualified_input(number,_PERCENT_INPUT)/100
            else:
                change = number
            if self.widgets.button_feed_override.get_active() and action:
                print "feed override"
                if absolute:
                    self.set_feed_override(change,absolute)
                else:
                    self.set_feed_override((change * self.data.feed_override_inc),absolute)
            elif self.widgets.button_spindle_override.get_active() and action:
                print "spindle override"
                if absolute:
                    self.set_spindle_override(change,absolute)
                else:
                    self.set_spindle_override((change * self.data.spindle_override_inc),absolute)
            elif self.widgets.button_rapid_override.get_active() and action:
                print "velocity override"
                if absolute:
                    self.set_velocity_override(change,absolute)
                else:
                    self.set_velocity_override((change * self.data.velocity_override_inc),absolute)
            elif self.widgets.button_jog_speed.get_active() and action:
                print "jog speed adjustment"
                if widget == self.widgets.button_offset_origin:
                    change = self.get_qualified_input(number)
                if absolute:
                    self.set_jog_rate(absolute = change)
                else:
                    if self.data.angular_jog_adjustment_flag:
                        self.set_jog_rate(step = (change * self.data.angular_jog_rate_inc))
                    else:
                        self.set_jog_rate(step = (change * self.data.jog_rate_inc))
            elif self.widgets.button_jog_increments.get_active() and action:
                print "jog increments adjustment"
                if widget == self.widgets.button_offset_origin:
                    change = self.get_qualified_input(number)
                if absolute:
                    self.set_jog_increments(absolute = change)
                else:
                    self.set_jog_increments(index_dir = change)

    def origin_system(self,*args):
        print "origin system button"
        value = self.widgets.button_select_system.get_active()
        self.sensitize_widgets(self.data.sensitive_origin_mode,not value)

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
            for i in ('zero_origin','offset_origin','plus','minus'):
                self.widgets["button_%s"% i].set_sensitive(False)
            self.widgets.mode0.hide()
            self.widgets.mode3.show()
            #self.widgets.button_mode.set_sensitive(False)
            self.widgets.button_override.set_sensitive(False)
            self.widgets.button_graphics.set_sensitive(False)
        else:
            for i in ('zero_origin','offset_origin','plus','minus'):
                self.widgets["button_%s"% i].set_sensitive(True)
            self.widgets.mode3.hide()
            self.widgets.mode0.show()
            state = self.data.all_homed
            self.sensitize_widgets(self.data.sensitive_all_homed,state)
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
            for name in (self.data.sensitive_graphics_mode):
                self._tempholder.append(self.widgets[name].get_sensitive())
                self.widgets[name].set_sensitive(False)
            self.widgets.vmode0.set_sensitive(True)
            self.widgets.button_plus.set_sensitive(True)
            self.widgets.button_minus.set_sensitive(True)
        else:
            self.widgets.mode5.hide()
            self.mode_changed(self.data.mode_order[0])
            for num,name in enumerate(self.data.sensitive_graphics_mode):
                if self.data.machine_on:
                    self.widgets[name].set_sensitive(True)
                else:
                    self.widgets[name].set_sensitive(self._tempholder[num])

    def override(self,*args):
        print "show/hide override buttons"
        value = self.widgets.button_override.get_active()
        self.sensitize_widgets(self.data.sensitive_override_mode,not value)
        if self.widgets.button_override.get_active():
            for i in range(0,3):
                self.widgets["mode%d"% i].hide()
            self.widgets.mode4.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.button_zero_origin.set_label("Zero\n ")
            self.widgets.button_offset_origin.set_label("Set At\n ")
        else:
            self.widgets.mode4.hide()
            self.mode_changed(self.data.mode_order[0])
            self.widgets.button_zero_origin.set_label(_(" Zero Origin"))
            self.widgets.button_offset_origin.set_label(_("Offset Origin"))

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
            print _("**** Gscreen ERROR:    Invalid message configuration (missing text or type) in INI File [DISPLAY] section")
        if len(m_text) != len(m_pinname):
            print _("**** Gscreen ERROR:    Invalid message configuration (missing pinname) in INI File [DISPLAY] section")
        if len(m_text) != len(m_boldtext):
            print _("**** Gscreen ERROR:    Invalid message configuration (missing boldtext) in INI File [DISPLAY] section")
        for bt,t,c ,name in zip(m_boldtext,m_text, m_type,m_pinname):
            #print bt,t,c,name
            if not ("status" in c) and not ("dialog" in c) and not ("okdialog" in c):
                print _("**** Gscreen ERROR:    invalid message type (%s)in INI File [DISPLAY] section"% c)
                continue
            if not name == None:
                # this is how we make a pin that can be connected to a callback 
                self.data['name'] = hal_glib.GPin(self.halcomp.newpin(name, hal.HAL_BIT, hal.HAL_IN))
                self.data['name'].connect('value-changed', self.on_printmessage,name,bt,t,c)
                if ("dialog" in c):
                    self.halcomp.newpin(name+"-waiting", hal.HAL_BIT, hal.HAL_OUT)
                    if not ("ok" in c):
                        self.halcomp.newpin(name+"-response", hal.HAL_BIT, hal.HAL_OUT)

    # display dialog
    def warning_dialog(self,message, displaytype, secondary=None,pinname=None):
        if displaytype:
            dialog = gtk.MessageDialog(self.widgets.window1,
                gtk.DIALOG_DESTROY_WITH_PARENT,
                gtk.MESSAGE_INFO, gtk.BUTTONS_OK,message)
        else:   
            dialog = gtk.MessageDialog(self.widgets.window1,
               gtk.DIALOG_DESTROY_WITH_PARENT,
               gtk.MESSAGE_QUESTION, gtk.BUTTONS_YES_NO,message)
        # if there is a secondary message then the first message text is bold
        if secondary:
            dialog.format_secondary_text(secondary)
        dialog.show_all()
        try:
            if "dialog_return" in dir(self.handler_instance):
                dialog.connect("response", self.handler_instance.dialog_return,self,displaytype,pinname)
            else:
                dialog.connect("response", self.dialog_return,displaytype,pinname)
        except:
            dialog.destroy()
            raise NameError (_('Dialog error - Is the dialog handler missing from the handler file?'))
        if pinname == "TOOLCHANGE":
            dialog.set_title(_("Manual Toolchange"))
        else:
            dialog.set_title(_("Operator Message"))

    # message dialog returns a response here
    # This includes the manual tool change dialog
    # We know this by the pinname being called 'TOOLCHANGE' 
    def dialog_return(self,widget,result,dialogtype,pinname):
        if pinname == "TOOLCHANGE":
            self.halcomp["tool-changed"] = True
            widget.destroy()
            try:
                self.widgets.statusbar1.remove_message(self.statusbar_id,self.data.tool_message)
            except:
                self.show_try_errors()
            return
        if not dialogtype: # yes/no dialog
            if result == gtk.RESPONSE_YES:result = True
            else: result = False
            if pinname:
                self.halcomp[pinname + "-response"] = result
        if pinname:
            self.halcomp[pinname + "-waiting"] = False
        widget.destroy()

    # dialog is used for choosing the run-at-line position
    def launch_restart_dialog(self,widget):
        self.restart_dialog()

    # dialog for manually calling a tool
    def restart_dialog(self):
        if self.data.restart_dialog: return
        if "restart_dialog_return" in dir(self.handler_instance):
            return_method = self.handler_instance.restart_dialog_return
        else:
            return_method = self.restart_dialog_return
        self.data.restart_dialog = gtk.Dialog(_("Restart Entry"),
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT))
        label = gtk.Label(_("Restart Entry"))
        label.modify_font(pango.FontDescription("sans 20"))
        self.data.restart_dialog.vbox.pack_start(label)
        calc = gladevcp.Calculator()
        self.data.restart_dialog.vbox.add(calc)
        calc.set_value("%d"%self.data.last_line)
        calc.set_property("font","sans 20")
        calc.set_editable(True)
        box = gtk.HButtonBox()
        upbutton = gtk.Button(label = _("Up"))
        box.add(upbutton)
        enterbutton = gtk.Button(label = _("Enter"))
        box.add(enterbutton)
        downbutton = gtk.Button(label = _("Down"))
        box.add(downbutton)
        calc.calc_box.pack_end(box, expand=False, fill=False, padding=0)
        upbutton.connect("clicked",self.restart_up,calc)
        downbutton.connect("clicked",self.restart_down,calc)
        enterbutton.connect("clicked",lambda w:calc.entry.emit('activate'))
        calc.entry.connect("activate",return_method,True,calc)
        self.data.restart_dialog.parse_geometry("400x400+0+0")
        self.data.restart_dialog.show_all()
        calc.num_pad_only(True)
        calc.integer_entry_only(True)
        self.data.restart_dialog.connect("response", return_method,calc)

    # either start the gcode at the line specified or cancel
    def restart_dialog_return(self,widget,result,calc):
        value = 0
        if not result == gtk.RESPONSE_REJECT:
            value = calc.get_value()
            if value == None:value = 0
        self.widgets.gcode_view.set_line_number(value)
        self.add_alarm_entry(_("Ready to Restart program from line %d"%value))
        self.update_restart_line(value)
        self.data.restart_dialog.destroy()
        self.data.restart_dialog = None

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
            print _("Invalid embeded tab configuration") # Complain somehow
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
        self.data.fullscreen1 = data
        if data:
            self.widgets.window1.fullscreen()
        else:
            self.widgets.window1.unfullscreen()

    def set_show_offsets(self, data):
        self.prefs.putpref('show_offsets', data, bool)
        self.data.show_offsets = data
        try:
            self.widgets.gremlin.show_offsets = data
        except:
            self.show_try_errors()

    def set_show_dtg(self, data):
        self.prefs.putpref('show_dtg', data, bool)
        try:
            self.widgets.gremlin.set_property('show_dtg',data)
        except:
            self.show_try_errors()

    def set_diameter_mode(self, data):
        print "toggle diameter mode"
        self.data.diameter_mode = data
        self.prefs.putpref('diameter_mode', data, bool)
        try:
            self.widgets.gremlin.set_property('show_lathe_radius',not data)
        except:
            self.show_try_errors()

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

    def set_unlock_code(self):
        self.prefs.putpref('unlock_code', self.data.unlock_code,str)

    # toggles gremlin's different views
    # if in lathe mode only P, Y and Y2 available
    def toggle_view(self):
        dist = self.widgets.gremlin.get_zoom_distance()
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
        self.widgets.gremlin.set_zoom_distance(dist)

    # toggle a large graphics view / gcode view
    def set_full_graphics_view(self,data):
        print "full view",data
        if data:
            print "enlarge"
            self.data.full_graphics = True
            self.widgets.notebook_mode.hide()
            self.widgets.dro_frame.hide()
            self.widgets.gremlin.set_property('enable_dro',True)
        else:
            print "shrink"
            self.data.full_graphics = False
            self.widgets.notebook_mode.show()
            self.widgets.dro_frame.show()
            self.widgets.gremlin.set_property('enable_dro',False)

    # enlargen the Gcode box while in edit mode
    def edit_mode(self,data):
        print "edit mode pressed",data
        self.sensitize_widgets(self.data.sensitive_edit_mode,not data)
        if data:
            self.widgets.mode2.hide()
            self.widgets.mode6.show()
            self.widgets.dro_frame.hide()
            self.widgets.gcode_view.set_sensitive(1)
            self.data.edit_mode = True
            self.widgets.show_box.hide()
            self.widgets.notebook_mode.show()
        else:
            self.widgets.mode6.hide()
            self.widgets.mode2.show()
            self.widgets.dro_frame.show()
            self.widgets.gcode_view.set_sensitive(0)
            self.data.edit_mode = False
            self.widgets.show_box.show()

    def set_dro_units(self, data, save=True):
        print "toggle dro units",self.data.dro_units,data
        if data == self.data._IMPERIAL:
            print "switch to imperial"
            self.status.dro_inch(1)
            self.widgets.gremlin.set_property('metric_units',False)
            try:
                self.widgets.offsetpage1.set_to_inch()
            except:
                self.show_try_errors()
        else:
            print "switch to mm"
            self.status.dro_mm(1)
            self.widgets.gremlin.set_property('metric_units',True)
            try:
                self.widgets.offsetpage1.set_to_mm()
            except:
                self.show_try_errors()
        self.data.dro_units = data
        if save:
            self.prefs.putpref('dro_is_metric', data, bool)

    def toggle_optional_stop(self):
        print "option stop"
        self.set_optional_stop(self.widgets.button_option_stop.get_active())

    def set_optional_stop(self,data):
        self.prefs.putpref('opstop', data, bool)
        self.data.op_stop = data
        self.emc.opstop(data)

    def toggle_block_delete(self):
        self.set_block_delete(self.widgets.button_block_delete.get_active())

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
            if self.widgets.button_jog_mode.get_active() or self.widgets.button_homing.get_active():
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
        # check and update jogging buttons
        self.jog_mode()

    # adjust sensitivity and labels of buttons
    def jog_mode(self):
        print "jog mode:",self.widgets.button_jog_mode.get_active()
        # if muliple axis selected - unselect all of them
        if len(self.data.active_axis_buttons) > 1 and self.widgets.button_jog_mode.get_active():
            for i in self.data.axis_list:
                self.widgets["axis_%s"%i].set_active(False)
        if self.widgets.button_jog_mode.get_active():
            self.widgets.button_move_to.set_label("Goto Position")
            self.emc.set_manual_mode()
        else:
            self.widgets.button_move_to.set_label("")
        self.update_hal_jog_pins()
        self.update_hal_override_pins()

    # do some checks then jog selected axis or start spindle
    def do_jog(self,direction,action):
        # if manual mode, if jogging
        # if only one axis button pressed
        # jog positive  at selected rate
        if self.data.mode_order[0] == self.data._MAN:
            if len(self.data.active_axis_buttons) > 1:
                self.notify(_("INFO:"),_("Can't jog multiple axis"),INFO_ICON)
                print self.data.active_axis_buttons
            elif self.data.active_axis_buttons[0][0] == None:
                self.notify(_("INFO:"),_("No axis selected to jog"),INFO_ICON)
            else:
                print "Jog axis %s" % self.data.active_axis_buttons[0][0]
                if not self.data.active_axis_buttons[0][0] == "s":
                    if not action: cmd = 0
                    elif direction: cmd = 1
                    else: cmd = -1
                    self.emc.jogging(1)
                    if self.data.active_axis_buttons[0][0] in('a','b','c'):
                        jogincr = self.data.angular_jog_increments[self.data.current_angular_jogincr_index]
                    else:
                        jogincr = self.data.jog_increments[self.data.current_jogincr_index]
                    print jogincr
                    if jogincr == ("continuous"): # continuous jog
                        print "active axis jog:",self.data.active_axis_buttons[0][1]
                        self.emc.continuous_jog(self.data.active_axis_buttons[0][1],cmd)
                    else:
                        print "jog incremental"
                        if cmd == 0: return # don't want release of button to stop jog
                        distance = self.parse_increment(jogincr)
                        self.emc.incremental_jog(self.data.active_axis_buttons[0][1],cmd,distance)

    def do_key_jog(self,axis,direction,action):
        if self.data._JOG in self.check_mode(): # jog mode active:
                    if not action: cmd = 0
                    elif direction: cmd = 1
                    else: cmd = -1
                    self.emc.jogging(1)
                    print self.data.jog_increments[self.data.current_jogincr_index]
                    if self.data.jog_increments[self.data.current_jogincr_index] == ("continuous"): # continuous jog
                        print "active axis jog:",axis
                        self.emc.continuous_jog(axis,cmd)
                    else:
                        print "jog incremental"
                        if cmd == 0: return # don't want release of button to stop jog
                        self.mdi_control.mdi.emcstat.poll()
                        if self.mdi_control.mdi.emcstat.state != 1: return
                        jogincr = self.data.jog_increments[self.data.current_jogincr_index]
                        distance = self.parse_increment(jogincr)
                        self.emc.incremental_jog(axis,cmd,distance)

    # spindle control
    def spindle_adjustment(self,direction,action):
        if action and not self.widgets.s_display_fwd.get_active() and not self.widgets.s_display_rev.get_active():
            self.notify(_("INFO:"),_("No direction selected for spindle"),INFO_ICON)
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
    def do_jog_to_position(self,data):
        if len(self.data.active_axis_buttons) > 1:
            self.notify(_("INFO:"),_("Can't jog multiple axis"),INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected to move"),INFO_ICON)
        else:
            if not self.data.active_axis_buttons[0][0] == "s":
                if self.data.active_axis_buttons[0][0] in('a','b','c'):
                    rate = self.data.angular_jog_rate
                    pos = self.get_qualified_input(data,switch=_DEGREE_INPUT)
                else:
                    rate = self.data.jog_rate
                    pos = self.get_qualified_input(data)
                self.mdi_control.go_to_position(self.data.active_axis_buttons[0][0],pos,rate)

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
    def get_qualified_input(self,raw = 0,switch = None):
        print "RAW input:",raw
        if switch in(_DEGREE_INPUT, _SPINDLE_INPUT):
            return raw
        elif switch == _PERCENT_INPUT:
            return round(raw,2)
        else:
            g21 = False
            if "G21" in self.data.active_gcodes: g21 = True

            # metric DRO - imperial mode
            if self.data.dro_units == self.data._MM:
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
            self.notify(_("INFO:"),_("Can't home multiple axis - select HOME ALL instead"),INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected to home"),INFO_ICON)
        else:
            print "home axis %s" % self.data.active_axis_buttons[0][0]
            self.emc.home_selected(self.data.active_axis_buttons[0][1])

    def unhome_selected(self):
        if len(self.data.active_axis_buttons) > 1:
            self.notify(_("INFO:"),_("Can't unhome multiple axis"),INFO_ICON)
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected to unhome"),INFO_ICON)
        else:
            print "unhome axis %s" % self.data.active_axis_buttons[0][0]
            self.emc.unhome_selected(self.data.active_axis_buttons[0][1])

    # Touchoff the axis zeroing it
    # reload the plot to update the display
    def zero_axis(self):
        if self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected for origin zeroing"),INFO_ICON)
        # if an axis is selected then set it
        for i in self.data.axis_list:
            if self.widgets["axis_%s"%i].get_active():
                print "zero %s axis" %i
                self.mdi_control.set_axis(i,0)
                self.reload_plot()

    # touchoff - setting the axis to the input
    def set_axis_checks(self):
        if self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected for origin touch-off"),INFO_ICON)
            return
        self.launch_numerical_input("on_offset_origin_entry_return")

    def tool_touchoff_checks(self):
        if len(self.data.active_axis_buttons) > 1:
            self.notify(_("INFO:"),_("Can't tool touch-off multiple axes"),INFO_ICON)
            return
        if self.data.active_axis_buttons[0][0] == None:
            self.notify(_("INFO:"),_("No axis selected for tool touch-off"),INFO_ICON)
            return
        self.launch_numerical_input("on_tool_offset_entry_return")

    # move axis to a position (while in manual mode)
    def move_to(self,data):
        if self.data.mode_order[0] == self.data._MAN:# if in manual mode
            if self.widgets.button_jog_mode.get_active(): # jog mode active
                print "jog to position"
                self.do_jog_to_position(data)

    def clear_plot(self):
        self.widgets.gremlin.clear_live_plotter()

    def pan(self,x,y):
        self.widgets.gremlin.pan(x,y)

    def rotate(self,x,y):
        self.widgets.gremlin.rotate_view(x,y)

    def reload_plot(self):
        print "reload plot"
        self.widgets.hal_action_reload.emit("activate")

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
                self.notify(_("Error Message"),_("Tool editor error - is the %s editor available?"% editor,ALERT_ICON,3))
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

    # adjust the screen as per each mode toggled 
    def mode_changed(self,mode):

        if mode == self.data._MAN: 
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
            self.widgets.hal_mdihistory.hide()
            self.widgets.button_homing.show()
            self.widgets.dro_frame.show()
            self.widgets.spare.hide()
        elif mode == self.data._MDI:
            if self.widgets.button_homing.get_active():
                self.widgets.button_homing.emit("clicked")
            if self.data.plot_hidden:
                self.toggle_offset_view()
            self.emc.set_mdi_mode()
            self.widgets.hal_mdihistory.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
        elif mode == self.data._AUTO:
            self.widgets.vmode0.hide()
            self.widgets.vmode1.show()
            if self.data.full_graphics:
                self.widgets.notebook_mode.hide()
            else:
                self.widgets.notebook_mode.show()
            self.widgets.hal_mdihistory.hide()
        if not mode == self.data._MAN:
            self.widgets.button_jog_mode.set_active(False)
            self.widgets.button_homing.set_active(False)
            self.widgets.button_homing.hide()
            self.widgets.spare.show()
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
    def timer_interrupt(self):
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
            print kind,text
            if "joint" in text:
                for letter in self.data.axis_list:
                    axnum = "xyzabcuvws".index(letter)
                    text = text.replace( "joint %d"%axnum,"Axis %s"%letter.upper() )
            if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                self.notify(_("Error Message"),text,ALERT_ICON,3)
            elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
                self.notify(_("Message"),text,INFO_ICON,3)
            elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
                self.notify(_("Message"),text,INFO_ICON,3)
        self.emc.unmask()
        if "periodic" in dir(self.handler_instance):
            self.handler_instance.periodic()
        else:
            self.update_position()
        return True

    # update the whole display
    def update_position(self,*args):
        self.update_mdi_spindle_button()
        self.update_spindle_bar()
        self.update_dro()
        self.update_active_gcodes()
        self.update_active_mcodes()
        self.update_aux_coolant_pins()
        self.update_feed_speed_label()
        self.update_tool_label()
        self.update_coolant_leds()
        self.update_estop_led()
        self.update_machine_on_led()
        self.update_limit_override()
        self.update_override_label()
        self.update_jog_rate_label()
        self.update_mode_label()
        self.update_units_button_label()

    # spindle controls
    def update_mdi_spindle_button(self):
        try:
            self.widgets.at_speed_label.set_label(_("%d RPM"%abs(self.data.spindle_speed)))
        except:
            pass
        label = self.widgets.spindle_control.get_label()
        speed = self.data.spindle_speed
        if speed == 0 and not label == _("Start"):
            temp = _("Start")
        elif speed and not label == _("Stop"):
            temp = _("Stop")
        else: return
        self.widgets.spindle_control.set_label(temp)

    def update_spindle_bar(self):
        self.widgets.s_display.set_value(abs(self.halcomp["spindle-readout-in"]))
        self.widgets.s_display.set_target_value(abs(self.data.spindle_speed))
        try:
            self.widgets.s_display2.set_value(abs(self.data.spindle_speed))
        except:
            self.show_try_errors()

    def update_dro(self):
        # DRO
        for i in self.data.axis_list:
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
                if self.data.dro_units == self.data._MM:
                    text = "%s% 10.3f"% (h,data)
                else:
                    text = "%s% 9.4f"% (h,data)
                self.widgets["%s_display_%d"%(i,j)].set_text(text)
                self.widgets["%s_display_%d"%(i,j)].set_alignment(0,.5)
                self.widgets["%s_display_%d_label"%(i,j)].set_alignment(1,.5)
                self.widgets["%s_display_%d_label"%(i,j)].set_text(label)

    def update_active_gcodes(self):
        # active codes
        active_g = " ".join(self.data.active_gcodes)
        self.widgets.active_gcodes_label.set_label("%s   "% active_g)

    def update_active_mcodes(self):
        self.widgets.active_mcodes_label.set_label(" ".join(self.data.active_mcodes))

    def update_aux_coolant_pins(self):
        # control aux_coolant  - For Dave Armstrong
        m7 = m8 = False
        self.halcomp["aux-coolant-m8-out"] = False
        self.halcomp["mist-coolant-out"] = False
        self.halcomp["aux-coolant-m7-out"] = False
        self.halcomp["flood-coolant-out"] = False
        if self.data.mist:
            if self.widgets.aux_coolant_m7.get_active():
               self.halcomp["aux-coolant-m7-out"] = True
            else:
                self.halcomp["mist-coolant-out"] = True
        if self.data.flood:
            if self.widgets.aux_coolant_m8.get_active():
                self.halcomp["aux-coolant-m8-out"] = True
            else:
                self.halcomp["flood-coolant-out"] = True

    def update_feed_speed_label(self):
        data = self.data.velocity
        if self.data.IPR_mode:
            try:
                data = data/abs(self.halcomp["spindle-readout-in"])
            except:
                data = 0
        if self.data.dro_units == self.data._MM:
            text = "%.2f"% (data)
        else:
            text = "%.3f"% (data)
        self.widgets.active_feed_speed_label.set_label("F%s    S%s   V%s"% (self.data.active_feed_command,
                            self.data.active_spindle_command,text))

    def update_tool_label(self):
        # corodinate system:
        systemlabel = (_("Machine"),"G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3")
        tool = str(self.data.tool_in_spindle)
        if tool == None: tool = "None"
        self.widgets.system.set_text(("Tool %s     %s"%(tool,systemlabel[self.data.system])))

    def update_coolant_leds(self):
        # coolant
        self.widgets.led_mist.set_active(self.data.mist)
        self.widgets.led_flood.set_active(self.data.flood)

    def update_estop_led(self):
        # estop
        self.widgets.led_estop.set_active(self.data.estopped)

    def update_machine_on_led(self):
        self.widgets.led_on.set_active(self.data.machine_on)

    def update_limit_override(self):
        # ignore limts led
        self.widgets.led_ignore_limits.set_active(self.data.or_limits)

    def update_override_label(self):
        # overrides
        self.widgets.fo.set_text("FO: %d%%"%(round(self.data.feed_override,2)*100))
        self.widgets.so.set_text("SO: %d%%"%(round(self.data.spindle_override,2)*100))
        self.widgets.mv.set_text("VO: %d%%"%(round((self.data.velocity_override),2) *100))

    # we need to check if the current units is in the basic machine units - convert if nesassary.
    # then set the display according to the current display units.
    def update_jog_rate_label(self):
        rate = round(self.status.convert_units(self.data.jog_rate),2)
        if self.data.dro_units == self.data._MM:
            text = "%4.2f mm/min"% (rate)
        else:
            text = "%3.2f IPM"% (rate)
        self.widgets.jog_rate.set_text(text)
        try:
            text = "%4.2f DPM"% (self.data.angular_jog_rate)
            self.widgets.angular_jog_rate.set_text(text)
        except:
            pass

    def update_mode_label(self):
        # Mode / view
        modenames = self.data.mode_labels
        time = strftime("%a, %d %b %Y  %I:%M:%S %P    ", localtime())
        self.widgets.mode_label.set_label( "%s   View -%s               %s"% (modenames[self.data.mode_order[0]],self.data.plot_view[0],time) )

    def update_units_button_label(self):
        label = self.widgets.metric_select.get_label()
        data = self.data.dro_units
        if data and not label == " mm ":
            temp = " mm "
        elif data == 0 and not label == "Inch":
            temp = "Inch"
        else: return
        self.widgets.metric_select.set_label(temp)

    def update_hal_jog_pins(self):
         for i in self.data.axis_list:
            if self.widgets.button_jog_mode.get_active() and self.widgets["axis_%s"%i].get_active():
                self.halcomp["jog-enable-%s-out"%i] = True
            else:
                self.halcomp["jog-enable-%s-out"%i] = False
            if self.widgets.button_jog_mode.get_active():
                self.halcomp["jog-enable-out"] = True
            else:
                self.halcomp["jog-enable-out"] = False
            try:
                self.widgets.led_jog_mode.set_active(self.halcomp["jog-enable-out"])
            except:
                pass

    # These pins set and unset enable pins for override adjustment
    # only true when the screen button is true and not in jog mode 
    # (because jog mode may use the encoder for jogging)
    def update_hal_override_pins(self):
        jogmode = not(self.widgets.button_jog_mode.get_active())
        fo = self.widgets.button_feed_override.get_active() and jogmode
        so = self.widgets.button_spindle_override.get_active() and jogmode
        mv = self.widgets.button_rapid_override.get_active() and jogmode
        self.halcomp["f-override-enable-out"] = fo
        self.halcomp["s-override-enable-out"] = so
        self.halcomp["mv-override-enable-out"] = mv

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

