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

BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python")
datadir = os.path.join(BASE, "share", "linuxcnc")
sys.path.insert(0, libdir)
themedir = "/usr/share/themes"
xmlname = os.path.join(datadir,"gscreen.glade")
xmlname2 = os.path.join(datadir,"gscreen2.glade")
import gettext
LOCALEDIR = os.path.join(BASE, "share", "locale")
gettext.install("linuxcnc", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("linuxcnc", LOCALEDIR)
gtk.glade.textdomain("linuxcnc")
TCLPATH = os.environ['LINUXCNC_TCL_DIR']

def set_active(w, s):
	if not w: return
	os = w.get_active()
	if os != s: w.set_active(s)

def set_label(w, l):
	if not w: return
	ol = w.get_label()
	if ol != l: w.set_label(l)

def set_text(w, t):
	if not w: return
	ot = w.get_label()
	if ot != t: w.set_label(t)

import linuxcnc
from gscreen import emc_interface
from gscreen import mdi
from gscreen import preferences


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
_START = 0;_MAN = 1;_MDI = 2;_AUTO = 3
_MM = 1;_IMPERIAL = 0
_SPINDLE_INPUT = 9

class Trampoline(object):
    def __init__(self,methods):
        self.methods = methods

    def __call__(self, *a, **kw):
        for m in self.methods:
            m(*a, **kw)

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
        self.axis_list = ("x","y","z","a","s")
        self.active_axis_buttons = [(None,None)]
        self.abs_color = (0, 65535, 0)
        self.rel_color = (65535, 0, 0)
        self.dtg_color = (0, 0, 65535)
        self.highlight_color = (65535,65535,65535)
        self.highlight_major = False
        self.display_order = (_REL,_DTG,_ABS)
        self.mode_order = (_START,_MAN,_MDI,_AUTO)
        self.plot_view = ("P","X","Y","Z","Z2")
        self.task_mode = 0
        self.active_gcodes = []
        self.active_mcodes = []
        self.maxvelocity = 1
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
        self.or_limits = 0
        self.feed_override = 1.0
        self.spindle_override = 1.0
        self.max_velocity = 1.0
        self.edit_mode = False
        self.full_graphics = False
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

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

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


class Gscreen: 

    def __init__(self,inipath):
        (progdir, progname) = os.path.split(sys.argv[0])
        print progdir,progname
        #usage = "usage: %prog [options] myfile.ui"
        #parser = OptionParser(usage=usage)
        #parser.disable_interspersed_args()
        #(opts, args) = parser.parse_args()
        #print args
        self.inipath = inipath
        try:
            # loading as a gtk.builder project
            self.xml = gtk.Builder()
            self.xml.add_from_file(xmlname)
        except:
            print "**** Gscreen GLADE ERROR:    With xml file: %s"% xmlname
            sys.exit(0)
        try:
            # loading as a gtk.builder project
            self.xml.add_from_file(xmlname2)
            self.screen2 = True
        except:
            print "**** Gscreen GLADE ERROR:    With xml file: %s"% xmlname
            self.screen2 = False
        self.widgets = Widgets(self.xml)
        self.data = Data()

        try:
            self.halcomp = hal.component("gscreen")
            self.halcomp.newpin("aux-coolant-m7.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("aux-coolant-m8.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("mist-coolant.out", hal.HAL_BIT, hal.HAL_OUT)
            self.halcomp.newpin("flood-coolant.out", hal.HAL_BIT, hal.HAL_OUT)
        except:
            print "*** Gscreen ERROR:    Asking for a HAL component using a name that already exists."
            sys.exit(0)
    
        panel = gladevcp.makepins.GladePanel( self.halcomp, xmlname, self.xml, None)
        self.halcomp.ready()
        # at this point, any glade HL widgets and their pins are set up.
        #handlers = load_handlers(None,self.halcomp,self.xml,None)
    
        #self.xml.connect_signals(handlers)

        self.xml.connect_signals( self )
        # access to saved prefernces
        self.prefs = preferences.preferences()

        #setup default stuff
        self.data.invisible_cursor = self.prefs.getpref('invisible_cursor', 'False', bool)
        self.data.theme_name = self.prefs.getpref('gtk_theme', 'Follow System Theme', str)
        self.data.abs_textcolor = self.prefs.getpref('abs_textcolor', '#0000FFFF0000', str)
        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.data.abs_textcolor))
        self.set_abs_color()
        self.data.rel_textcolor = self.prefs.getpref('rel_textcolor', '#FFFF00000000', str)
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.data.rel_textcolor))
        self.set_rel_color()
        self.data.dtg_textcolor = self.prefs.getpref('dtg_textcolor', '#00000000FFFF', str)
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.data.dtg_textcolor))
        self.set_dtg_color()
        self.data.err_textcolor = self.prefs.getpref('err_textcolor', 'default', str)
        self.data.error_font_name = self.prefs.getpref('error_font', 'Sans Bold 10', str)
        self.data.window_geometry = self.prefs.getpref('window_geometry', 'default', str)
        self.data.window_max = self.prefs.getpref('window_force_max', 'False', bool)
        self.data.window2_geometry = self.prefs.getpref('window2_geometry', 'default', str)
        self.data.window2_max = self.prefs.getpref('window2_force_max', 'False', bool)
        self.data.use_screen2 = self.prefs.getpref('use_screen2', 'True', bool)
        self.widgets.use_screen2.set_active(self.data.use_screen2)
        self.widgets.fullscreen1.set_active( self.prefs.getpref('fullscreen1', 'True', bool) )
        self.widgets.show_offsets.set_active( self.prefs.getpref('show_offsets', 'True', bool) )
        self.widgets.gremlin.show_offsets = self.widgets.show_offsets.get_active()
        self.data.diameter_mode = self.prefs.getpref('diameter_mode', 'True', bool)
        self.widgets.diameter_mode.set_active(self.data.diameter_mode)
        self.data.dro_units = self.prefs.getpref('units', 'False', bool)
        self.widgets.dro_units.set_active(self.data.dro_units)
        self.data.display_order = self.prefs.getpref('display_order', (0,1,2), repr)

        w = self.widgets.statusbar1
        #w.set_font_name(self.data.error_font_name)
        #self.error_font = pango.FontDescription(self.data.error_font_name)
        #w.modify_font(self.error_font)
        if not self.data.err_textcolor == "default":
            print"change error color",self.data.err_textcolor
            w.modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse(self.data.err_textcolor))
        self.widgets.statusbar1.push(1,"Status bar text                                                                       to here?")
        self.widgets.data_input.set_value(50.567)
        self.widgets.button_mode.set_label("Mode %d"% self.data.mode_order[0])
        # add terminal window
        v = vte.Terminal ()
        v.connect ("child-exited", lambda term: gtk.main_quit())
        v.fork_command()
        v.show()
        window = self.widgets.terminal_window.add(v)
        #window.add(v)
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
        # get the system wide theme
        settings = gtk.settings_get_default()
        self.data.system_theme = settings.get_property("gtk-theme-name")
        if not self.data.theme_name == "Follow System Theme":
            settings.set_string_property("gtk-theme-name", self.data.theme_name, "")
        # maximize window or set geometry and optionally maximize 
        if self.data.window_geometry == "default":
		    self.widgets.window1.maximize()
        else:
            self.widgets.window1.parse_geometry(self.data.window_geometry)
            if self.data.window_max:
                self.widgets.window1.maximize()
        if self.prefs.getpref('fullscreen1', 'True', bool):
            self.widgets.window1.fullscreen()

        # setup signals that can be blocked 
        cb = "axis_x"
        i = "_sighandler_axis_x"
        self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_x_clicked))
        cb = "axis_y"
        i = "_sighandler_axis_y"
        self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_y_clicked))
        cb = "axis_z"
        i = "_sighandler_axis_z"
        self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_z_clicked))
        cb = "axis_a"
        i = "_sighandler_axis_a"
        self.data[i] = int(self.widgets[cb].connect("clicked", self.on_axis_a_clicked))

        cb = "button_menu"
        i = "_sighandler_mode_select"
        self.data[i] = int(self.widgets[cb].connect("button_press_event", self.on_mode_select_clicked))

        cb = "button_mode"
        i = "_sighandler_mode"
        self.data[i] = int(self.widgets[cb].connect("button_press_event", self.on_mode_clicked))
        cb = "gremlin"
        i = "_sighandler_gremlin"
        self.data[i] = int(self.widgets[cb].connect("button_press_event", self.on_gremlin_clicked))
        cb = "button_estop"
        i = "_sighandler_estop"
        self.data[i] = int(self.widgets[cb].connect("clicked", self.on_estop_clicked))
        for mode in range(0,4):
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
        self.widgets.button_v0_2.connect("pressed", self.on_button_v0_2_pressed)
        self.widgets.button_v0_2.connect("released", self.on_button_v0_2_released)
        self.widgets.button_v0_3.connect("pressed", self.on_button_v0_3_pressed)
        self.widgets.button_v0_3.connect("released", self.on_button_v0_3_released)
        self.widgets.theme_choice.connect("changed", self.on_theme_choice_changed)
        self.widgets.use_screen2.connect("clicked", self.on_use_screen2_pressed)
        self.widgets.dro_units.connect("clicked", self.on_dro_units_pressed)
        self.widgets.diameter_mode.connect("clicked", self.on_diameter_mode_pressed)
        self.widgets.show_offsets.connect("clicked", self.on_show_offsets_pressed)
        self.widgets.show_dtg.connect("clicked", self.on_show_dtg_pressed)
        self.widgets.fullscreen1.connect("clicked", self.on_fullscreen1_pressed)
        self.widgets.shut_down.connect("clicked", self.on_window1_destroy)
        self.widgets.run_halshow.connect("clicked", self.on_halshow)
        self.widgets.run_calibration.connect("clicked", self.on_calibration)
        self.widgets.run_halmeter.connect("clicked", self.on_halmeter)

        # access to EMC control
        self.emc = emc_interface.emc_control(linuxcnc, self.widgets.statusbar1)
        # access to EMC status
        self.status = emc_interface.emc_status( self.data, linuxcnc)
        # access to MDI
        mdi_labels = mdi_eventboxes = []
        self.mdi_control = mdi.mdi_control(gtk, linuxcnc, mdi_labels, mdi_eventboxes)
        # set up EMC stuff

        # check the ini file if UNITS are set to mm"
        inifile=self.emc.emc.ini(self.inipath)
        # first check the global settings
        units=inifile.find("TRAJ","LINEAR_UNITS")
        if units==None:
            units=inifile.find("AXIS_0","UNITS")

        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            conversion=[25.4]*3+[1]*3+[25.4]*3
        self.data.lathe_mode = bool(inifile.find("DISPLAY", "LATHE"))
        self.status.set_machine_units(self.machine_units_mm,conversion)

        if self.prefs.getpref('toolsetting_fixture', 0):
            self.g10l11 = 1
        else:
            self.g10l11 = 0

        if self.prefs.getpref('dro_mm', 0):
            self.status.dro_mm(0)
        else:
            self.status.dro_inch(0)

        if self.prefs.getpref('dro_actual', 0):
           self.status.dro_actual(0)
        else:
           self.status.dro_commanded(0)

        if self.prefs.getpref('blockdel', 0):
           self.emc.blockdel_on(0)
        else:
           self.emc.blockdel_off(0)

        if self.prefs.getpref('opstop', 1):
           self.emc.opstop_on(0)
        else:
           self.emc.opstop_off(0)                   
        temp = inifile.find("TRAJ","MAX_LINEAR_VELOCITY")
        if temp == None:
            temp = inifile.find("TRAJ","MAX_VELOCITY")
            if temp == None:
                temp = 1.0
        self.data._maxvelocity = float(temp)
        print "max velocity",(float(self.data._maxvelocity) )
        # timers for updates
        gobject.timeout_add(50, self.periodic_status)
        #gobject.timeout_add(100, self.periodic_radiobuttons)

        # and display everything
        self.widgets.window1.set_title("Gscreen for linuxcnc")

        if self.screen2:
            self.widgets.window2.show()
            self.widgets.window2.move(0,0)
            if not self.data.use_screen2:
                self.widgets.window2.hide()
        self.widgets.window1.show()
        #self.widgets.window1.move(300,0)
        self.update_position()
        #self.widgets.gcode_view.set_sensitive(0)
        #self.widgets.hal_led1.set_shape(2)
        self.widgets.gremlin.set_property('view',self.data.plot_view[0])
        self.widgets.gremlin.set_property('metric_units',(self.data.dro_units == _MM))
        # set to 'start mode' 
        self.mode_changed(self.data.mode_order[0])

# *** GLADE callbacks ****

    def on_halshow(self,*args):
        print "halshow",TCLPATH
        p = os.popen("tclsh %s/bin/halshow.tcl -- -ini %s &" % (TCLPATH,self.inipath))

    def on_calibration(self,*args):
        print "calibration --%s"% self.inipath
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH,self.inipath),"w")

    def on_halmeter(self,*args):
        print "halmeter"
        p = os.popen("halmeter &")

    def on_window1_destroy(self, widget, data=None):
        print "kill"
    	gtk.main_quit()

    def on_axis_x_clicked(self,*args):
        def unclick():
            self.widgets.axis_x.handler_block(self.data._sighandler_axis_x)
            self.widgets.axis_x.set_active(False)
            self.widgets.axis_x.handler_unblock(self.data._sighandler_axis_x)

        if self.widgets.button_v0_1.get_active():
            self.set_axis_checks()
            unclick()
        elif self.widgets.button_v0_0.get_active():
            self.zero_axis()
            unclick()
        self.update_active_axis_buttons()

    def on_axis_y_clicked(self,*args):
        if self.widgets.button_v0_1.get_active():
         self.on_axis_set_clicked(None)
        elif self.widgets.button_v0_1.get_active():
         self.on_axis_zero_clicked(None)
        self.update_active_axis_buttons()

    def on_axis_z_clicked(self,*args):
        if self.widgets.button_v0_1.get_active():
         self.on_axis_set_clicked(None)
        if self.widgets.button_v0_0.get_active():
         self.on_axis_zero_clicked(None)
        self.update_active_axis_buttons()

    def on_axis_a_clicked(self,*args):
        if self.widgets.button_v0_1.get_active():
         self.on_axis_set_clicked(None)
        if self.widgets.button_v0_0.get_active():
         self.on_axis_zero_clicked(None)
        self.update_active_axis_buttons()

    def on_axis_s_clicked(self,*args):
        if self.widgets.button_v0_1.get_active():
         self.on_axis_set_clicked(None)
        if self.widgets.button_v0_0.get_active():
         self.on_axis_zero_clicked(None)
        self.update_active_axis_buttons()

    def on_mode_clicked(self,widget,event):
        # only change machine modes on click
        if event.type == gtk.gdk.BUTTON_PRESS:
            a,b,c,d = self.data.mode_order
            self.data.mode_order = b,c,d,a
            self.widgets.button_mode.set_label("Mode %d"% self.data.mode_order[0])
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
        try:
            if mode == 0:
                if number == 0: self.toggle_ignore_limits()
                elif number == 2: self.home_all()
                elif number == 4: self.unhome_all()
                elif number == 5: self.dro_toggle()
                else: raise nofunnction
            if mode == 1:
                if number == 0: self.jog_mode()
                elif number == 2: self.toggle_mist()
                elif number == 3: self.toggle_flood()
                elif number == 4: self.reload_tooltable()
                elif number == 5: self.dro_toggle()
                else: raise nofunnction
            if mode == 3:
                if number == 6: self.reload_plot()
                else: raise nofunnction
            else: raise nofunnction
        except :
         print "hbutton %d_%d clicked but no function"% (mode,number)

    def on_vbutton_clicked(self,widget,mode,number):
        #try:
            if mode == 0:
                if number == 0:
                    self.zero_axis()
                elif number == 1:
                    self.set_axis_checks()
                elif number == 2:
                    pass
                elif number == 3:
                    pass
                elif number == 4:
                    self.toggle_feed_hold()
                else: raise nofunnction
            elif mode == 1:
                if number == 3:
                    self.full_graphics()
                elif number == 2:
                    self.toggle_view()
                elif number == 4:
                    self.edit_mode()
                elif number == 7:
                    self.clear_plot()
                else: raise nofunnction
            else: raise nofunnction
        #except :
         #   print "Vbutton %d_%d clicked but no function"% (mode,number)

    def on_button_v0_2_pressed(self,*args):
        self.do_jog(1)
    def on_button_v0_2_released(self,*args):
        self.do_jog(0)
    def on_button_v0_3_pressed(self,*args):
        self.do_jog(-1)
    def on_button_v0_3_released(self,*args):
        self.do_jog(0)

    def on_mode_select_clicked(self,widget,event):
        # was it a multiple click?
        if event.type == gtk.gdk.BUTTON_PRESS:
            maxpage = self.widgets.notebook_mode.get_n_pages()
            page = self.widgets.notebook_mode.get_current_page()
            nextpage = page + 1
            print "mode select",maxpage,page,nextpage
            if nextpage == maxpage:nextpage = 0
            self.widgets.notebook_mode.set_current_page(nextpage)
        elif event.type == gtk.gdk._2BUTTON_PRESS:
            maxpage = self.widgets.notebook_main.get_n_pages()
            page = self.widgets.notebook_main.get_current_page()
            nextpage = page + 1
            print "double"
            print "mode select",maxpage,page,nextpage
            if nextpage == maxpage:nextpage = 0
            self.widgets.notebook_main.set_current_page(nextpage)

    def on_estop_clicked(self,*args):
        print "estop",self.data.estopped
        if self.data.estopped:
            self.emc.estop_reset(1)
            time.sleep(1)
            self.emc.machine_on(1)
        else:
            self.emc.estop(1)
            self.emc.machine_off(1)

    def on_theme_choice_changed(self,*args):
        self.change_theme()

    def on_fullscreen1_pressed(self,*args):
        self.toggle_fullscreen1()

    def on_use_screen2_pressed(self,*args):
        self.toggle_screen2()

    def on_dro_units_pressed(self,*args):
        self.toggle_dro_units()

    def on_diameter_mode_pressed(self, *args):
        self.toggle_diameter_mode()

    def on_rel_colorbutton_color_set(self,*args):
        self.set_rel_color()

    def on_abs_colorbutton_color_set(self,*args):
        self.set_abs_color()

    def on_dtg_colorbutton_color_set(self,*args):
        self.set_dtg_color()

    def on_show_offsets_pressed(self, *args):
        self.toggle_show_offsets()

    def on_show_dtg_pressed(self, *args):
        self.toggle_show_dtg()

# ****** do stuff *****

    def toggle_fullscreen1(self):
        print "toggle fullscreen"
        data = self.widgets.fullscreen1.get_active()
        print data
        if data:
            self.widgets.window1.fullscreen()
        else:
            self.widgets.window1.unfullscreen()
        self.prefs.putpref('fullscreen1', data, bool)

    def toggle_show_offsets(self):
        data = self.widgets.show_offsets.get_active()
        self.widgets.gremlin.show_offsets = data
        self.prefs.putpref('show_offsets', data, bool)

    def toggle_show_dtg(self):
        self.widgets.gremlin.set_property('show_dtg',self.widgets.show_dtg.get_active())

    def toggle_diameter_mode(self):
        print "toggle diameter mode"
        self.data.diameter_mode = self.widgets.diameter_mode.get_active()

    def convert_to_rgb(self,spec):
        color =  spec.to_string()
        temp = color.strip("#")
        r = temp[0:4]
        g = temp[4:8]
        b = temp[8:]
        return (int(r,16),int(g,16),int(b,16))

    def set_rel_color(self):
        print self.widgets.rel_colorbutton.get_color()
        self.data.rel_color = self.convert_to_rgb(self.widgets.rel_colorbutton.get_color())
        self.prefs.putpref('rel_textcolor', self.widgets.rel_colorbutton.get_color(),str)

    def set_abs_color(self):
        print self.widgets.abs_colorbutton.get_color()
        self.data.abs_color = self.convert_to_rgb(self.widgets.abs_colorbutton.get_color())
        self.prefs.putpref('abs_textcolor', self.widgets.abs_colorbutton.get_color(),str)

    def set_dtg_color(self):
        print self.widgets.dtg_colorbutton.get_color()
        self.data.dtg_color = self.convert_to_rgb(self.widgets.dtg_colorbutton.get_color())
        self.prefs.putpref('dtg_textcolor', self.widgets.dtg_colorbutton.get_color(),str)

    def toggle_view(self):
        def shift():
            a = self.data.plot_view[0]
            b = self.data.plot_view[1]
            c = self.data.plot_view[2]
            d = self.data.plot_view[3]
            e = self.data.plot_view[4]
            self.data.plot_view = (b,c,d,e,a)
        shift()
        if self.data.lathe_mode:
            while not self.data.plot_view[0] in("P","Y","p","y"):
                shift()
        self.widgets.gremlin.set_property('view',self.data.plot_view[0])

    def full_graphics(self):
        print "graphics mode"
        if self.data.full_graphics:
            print "shrink"
            self.data.full_graphics = False
            self.widgets.notebook_mode.show()
        else:
            print "enlarge"
            self.data.full_graphics = True
            self.widgets.notebook_mode.hide()

    def edit_mode(self):
        print "edit mode pressed"
        if self.data.edit_mode:
            self.widgets.gcode_view.set_sensitive(0)
            self.data.edit_mode = False
            self.widgets.eventbox_gremlin.show()
            self.widgets.hbuttonbox.set_sensitive(True)
            self.widgets.button_mode.set_sensitive(True)
        else:
            self.widgets.gcode_view.set_sensitive(1)
            self.data.edit_mode = True
            self.widgets.eventbox_gremlin.hide()
            self.widgets.hbuttonbox.set_sensitive(False)
            self.widgets.button_mode.set_sensitive(False)

    def toggle_dro_units(self):
        print "toggle dro units",self.data.dro_units
        if self.data.dro_units == _MM:
            print "switch to imperial"
            self.status.dro_inch(1)
            self.widgets.gremlin.set_property('metric_units',False)
        else:
            print "switch to mm"
            self.status.dro_mm(1)
            self.widgets.gremlin.set_property('metric_units',True)
        self.data.dro_units = self.widgets.dro_units.get_active()

    def save_edit(self):
        print "edit"

    def update_active_axis_buttons(self):
        count = 0;temp = []
        self.data.active_axis_buttons = []
        for num,i in enumerate(self.data.axis_list):
            if self.widgets["axis_%s"%i].get_active():
                count +=1
                axisnum = num
                self.data.active_axis_buttons.append((i,num))
        if count == 0: self.data.active_axis_buttons.append((None,None))

    def jog_mode(self):
        # if muliple axis selected - unselect all of them
        if len(self.data.active_axis_buttons) > 1 and self.widgets.button_h1_0.get_active():
            for i in self.data.axis_list:
                self.widgets["axis_%s"%i].set_active(False)

    def do_jog(self,direction):
        # if manual mode, if jogging
        # if only one axis button pressed
        # jog positive  at selected rate
        if self.data.mode_order[0] == 1:
            if len(self.data.active_axis_buttons) > 1:
                self.widgets.statusbar1.push(2,"Can't jog multiple axis")
                print self.data.active_axis_buttons
            elif self.data.active_axis_buttons[0][0] == None:
                self.widgets.statusbar1.push(1,"No axis selected to jog")
            else:
                print "Jog axis %s" % self.data.active_axis_buttons[0][0]
                if not self.data.active_axis_buttons[0][0] == "s":
                    self.emc.jogging(1)
                    self.emc.continuous_jog_velocity(20) # TODO make selectable
                    self.emc.continuous_jog(self.data.active_axis_buttons[0][1],direction)

    def do_jog_to_position(self):
        if len(self.data.active_axis_buttons) > 1:
            self.widgets.statusbar1.push(2,"Can't jog multiple axis")
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.widgets.statusbar1.push(1,"No axis selected to jog")
        else:
            print "Jog axis %s" % self.data.active_axis_buttons[0][0]
            if not self.data.active_axis_buttons[0][0] == "s":
                self.mdi_control.go_to_position(self.data.active_axis_buttons[0][0],self.get_qualified_input(),20) #TODO config jog rate

    def toggle_screen2(self):
        self.data.use_screen2 = self.widgets.use_screen2.get_active()
        if self.screen2:
            if self.data.use_screen2:
                self.widgets.window2.show()
            else:
                self.widgets.window2.hide()
        self.prefs.putpref('use_screen2', self.data.use_screen2, bool)

    def get_qualified_input(self,switch = None):
        raw = self.widgets.data_input.get_value()
        if switch == _SPINDLE_INPUT:
            return raw
        else:
            if self.data.dro_units != self.data.machine_units:
                print "conversion needed"
                if self.data.dro_units == _MM:
                    raw = raw / 25.4
                else:
                    raw = raw * 25.4
                print "converted to:",raw
            else:
                print "no conversion"
            if switch == "x" and self.data.diameter_mode:
                print "convert from diameter"
                raw = raw / 2.0
        return raw

    def unhome_all(self):
        self.emc.unhome_all(1)

    def home_all(self):
        self.emc.home_all(1)

    def unhome_selected(self,axis):
        if len(self.data.active_axis_buttons) > 1:
            self.widgets.statusbar1.push(2,"Can't unhome multiple axis")
            print self.data.active_axis_buttons
        elif self.data.active_axis_buttons[0][0] == None:
            self.widgets.statusbar1.push(1,"No axis selected to unhome")
        else:
            print "unhome axis %s" % self.data.active_axis_buttons[0][0]

    def zero_axis(self):
        # cancel set mode
        if self.widgets.button_v0_1.get_active():
            self.widgets.button_v0_1.set_active(False)
        # if an axis is selected then set it
        keep = True
        for i in self.data.axis_list:
            if self.widgets["axis_%s"%i].get_active():
                keep = False
                if i == "s":
                    self.emc.spindle_off(1)
                else:
                    print "zero %s axis" %i
                    self.mdi_control.set_axis(i,0)
                    self.reload_plot()
        if keep == False:
            self.widgets.button_v0_0.handler_block(self.data._sighandler_button_v0_0)
            self.widgets.button_v0_0.set_active(keep)
            self.widgets.button_v0_0.handler_unblock(self.data._sighandler_button_v0_0)

    def set_axis_checks(self):
        # cancel zero mode
        if self.widgets.button_v0_0.get_active():
            self.widgets.button_v0_0.set_active(False)
        if self.data.mode_order[0] in (_START,_MAN):
            # if in jog mode
            if self.widgets.button_h1_0.get_active():
                "print jog to position"
                # unpress toggle buttonn
                self.widgets.button_v0_1.handler_block(self.data._sighandler_button_v0_1)
                self.widgets.button_v0_1.set_active(False)
                self.widgets.button_v0_1.handler_unblock(self.data._sighandler_button_v0_1)
                self.do_jog_to_position()
            else:
                # if an axis is selected then set it
                keep = True
                for i in self.data.axis_list:
                    if self.widgets["axis_%s"%i].get_active():
                        keep = False
                        print "set %s axis" %i
                        if i == "s":
                            if self.widgets.s_display_fwd.get_active():
                                self.emc.spindle_forward(1)
                            elif self.widgets.s_display_rev.get_active():
                                self.emc.spindle_reverse(1)
                            else: continue
                            self.mdi_control.set_spindle_speed(self.get_qualified_input(_SPINDLE_INPUT))
                        else:
                            self.mdi_control.set_axis(i,self.get_qualified_input(i))
                            self.reload_plot()
                if keep == False:
                    self.widgets.button_v0_1.handler_block(self.data._sighandler_button_v0_1)
                    self.widgets.button_v0_1.set_active(keep)
                    self.widgets.button_v0_1.handler_unblock(self.data._sighandler_button_v0_1)

    def clear_plot(self):
        self.widgets.gremlin.clear_live_plotter()

    def reload_plot(self):
        print "reload plot"
        self.widgets.button_h3_7.emit("clicked")

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

    def toggle_feed_hold(self):
        print "toggle feed hold",self.data.feed_hold
        if self.data.feed_hold:
            self.emc.feed_hold(0)
        else:
            self.emc.feed_hold(1)

    def toggle_ignore_limits(self):
        print "over ride limits"
        self.emc.override_limits(1)

    def reload_tooltable(self):
        self.emc.reload_tooltable(1)

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

    def mode_changed(self,mode):
        for i in range(0,4):
            if i == mode:
                self.widgets["mode%d"% i].show()
            else:
                self.widgets["mode%d"% i].hide()
        if not mode == _START or not mode == _MAN:
            self.widgets.button_h1_0.set_active(False)
        if mode == _AUTO:
            self.widgets.vmode0.hide()
            self.widgets.vmode1.show()
            if self.data.full_graphics:
                self.widgets.notebook_mode.hide()
            else:
                self.widgets.notebook_mode.show()
            self.widgets.hal_mdihistory.hide()
        elif mode == _MDI:
            self.widgets.hal_mdihistory.show()
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
        else: 
            self.widgets.vmode0.show()
            self.widgets.vmode1.hide()
            self.widgets.notebook_mode.hide()
            self.widgets.hal_mdihistory.hide()

    def change_theme(self):
        theme = self.widgets.theme_choice.get_active_text()
        self.prefs.putpref('gtk_theme', theme, str)
        if theme == "Follow System Theme":
            theme = self.data.system_theme
        settings = gtk.settings_get_default()
        settings.set_string_property("gtk-theme-name", theme, "")

    def periodic_status(self):
        self.emc.mask()
        self.emcstat = linuxcnc.stat()
        self.emcerror = linuxcnc.error_channel()
        self.emcstat.poll()
        self.data.task_mode = self.emcstat.task_mode 
        self.status.periodic()
        self.data.system = self.status.get_current_system()
        #print self.status.data.x_abs
        #self.radiobutton_mask = 0
        e = self.emcerror.poll()
        if e:
            kind, text = e
            self.widgets.statusbar1.push(1,text)
        self.emc.unmask()
        #self.hal.periodic(self.tab == 1) # MDI tab?
        self.update_position()
        return True

    def update_position(self,*args):
        # DRO 
        for i in ("x","y","z","a","s"):
            if i == "s":
                self.widgets.s_display.set_value(abs(self.data.spindle_speed))
                self.widgets.s_display2.set_value(abs(self.data.spindle_speed))
            else:
                
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
        #tool = str(self.data.preppedtool)
        tool = str(self.data.tool_in_spindle)
        if tool == None: tool = "None"
        self.widgets.system.set_text("Tool %s     %s"%(tool,systemlabel[self.data.system]))
        # coolant
        self.widgets.led_mist.set_active(self.data.mist)
        self.widgets.led_flood.set_active(self.data.flood)
        # estop
        self.widgets.led_estop.set_active(not self.data.estopped)
        # overrides
        self.widgets.fo.set_text("FO: %d%%"%(self.data.feed_override*100))
        self.widgets.so.set_text("SO: %d%%"%(self.data.spindle_override*100))
        self.widgets.mv.set_text("VO: %d%%"%( int(self.data.max_velocity/self.data._maxvelocity *100)) )
        #print self.data.max_velocity,self.data._maxvelocity
        # Mode / view
        modenames = ("Startup","Manual","MDI","Auto")
        self.widgets.mode_label.set_label( "%s Mode   View -%s"% (modenames[self.data.mode_order[0]],self.data.plot_view[0]) )

if __name__ == "__main__":
    try:
        print "ini", sys.argv[2]
        app = Gscreen(sys.argv[2])
        gtk.main()
    except KeyboardInterrupt:
        print "linuxcnc closed down"
        sys.exit(0)

