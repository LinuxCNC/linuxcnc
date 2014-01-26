#!/usr/bin/env python


# Touchy is Copyright (c) 2009  Chris Radek <chris@timeguy.com>
#
# Touchy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Touchy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
libdir = os.path.join(BASE, "lib", "python")
datadir = os.path.join(BASE, "share", "linuxcnc")
sys.path.insert(0, libdir)
themedir = "/usr/share/themes"
try:
        import pygtk
  	pygtk.require("2.0")
except:
  	pass
try:
	import gtk
  	import gtk.glade
        import gobject
        import pango
except:
	sys.exit(1)

import atexit
import tempfile

empty_program = tempfile.NamedTemporaryFile()
empty_program.write("%\n%\n")
empty_program.flush()

import gettext
LOCALEDIR = os.path.join(BASE, "share", "locale")
gettext.install("linuxcnc", localedir=LOCALEDIR, unicode=True)
gtk.glade.bindtextdomain("linuxcnc", LOCALEDIR)
gtk.glade.textdomain("linuxcnc")

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
from touchy import emc_interface
from touchy import mdi
from touchy import hal_interface
from touchy import filechooser
from touchy import listing
from touchy import preferences

pix_data = '''/* XPM */
static char * invisible_xpm[] = {
"1 1 1 1",
"	c None",
" "};'''


color = gtk.gdk.Color()
pix = gtk.gdk.pixmap_create_from_data(None, pix_data, 1, 1, 1, color, color)
invisible = gtk.gdk.Cursor(pix, pix, color, color, 0, 0)

class touchy:
        def __init__(self, inifile):
		# System default Glade file:
                self.gladefile = os.path.join(datadir, "touchy.glade")
                if inifile:
                        self.ini = linuxcnc.ini(inifile)
                        alternate_gladefile = self.ini.find("DISPLAY", "GLADEFILE")
                        if alternate_gladefile:
                                self.gladefile = alternate_gladefile
                else:
                        self.ini = None


	        self.wTree = gtk.glade.XML(self.gladefile) 

		for w in ['wheelinc1', 'wheelinc2', 'wheelinc3',
				'wheelx', 'wheely', 'wheelz',
				'wheela', 'wheelb', 'wheelc',
				'wheelu', 'wheelv', 'wheelw']:
			self.wTree.get_widget(w).get_child().set_property('width-chars', 6)

		for widget in self.wTree.get_widget_prefix(''):
			widget.unset_flags(gtk.CAN_FOCUS)
		self.wTree.get_widget('MainWindow').set_flags(gtk.CAN_FOCUS)
		self.wTree.get_widget('MainWindow').grab_focus()

                self.num_mdi_labels = 11
                self.num_filechooser_labels = 11
                self.num_listing_labels = 20

                self.wheelxyz = 0
                self.wheelinc = 0
                self.wheel = "fo"
                self.radiobutton_mask = 0
                self.resized_wheelbuttons = 0

                self.tab = 0

                self.fo_val = 100
                self.so_val = 100
                self.g10l11 = 0

                self.prefs = preferences.preferences()
                self.mv_val = self.prefs.getpref('maxvel', 100, int)
                self.control_font_name = self.prefs.getpref('control_font', 'Sans 18', str)
                self.dro_font_name = self.prefs.getpref('dro_font', 'Courier 10 Pitch Bold 16', str)
                self.error_font_name = self.prefs.getpref('error_font', 'Sans Bold 10', str)
                self.listing_font_name = self.prefs.getpref('listing_font', 'Sans 10', str)
                self.theme_name = self.prefs.getpref('gtk_theme', 'Follow System Theme', str)
                self.abs_textcolor = self.prefs.getpref('abs_textcolor', 'default', str)
                self.rel_textcolor = self.prefs.getpref('rel_textcolor', 'default', str)
                self.dtg_textcolor = self.prefs.getpref('dtg_textcolor', 'default', str)
                self.err_textcolor = self.prefs.getpref('err_textcolor', 'default', str)
                self.window_geometry = self.prefs.getpref('window_geometry', 'default', str)
                self.window_max = self.prefs.getpref('window_force_max', 'False', bool)

                # initial screen setup
                if os.path.exists(themedir):
                    model = self.wTree.get_widget("theme_choice").get_model()
                    model.clear()
                    model.append(("Follow System Theme",))
                    temp = 0
                    names = os.listdir(themedir)
                    names.sort()
                    for search,dirs in enumerate(names):
                        model.append((dirs,))
                        if dirs  == self.theme_name:
                            temp = search+1
                    self.wTree.get_widget("theme_choice").set_active(temp)

                if self.window_geometry == "default":
		            self.wTree.get_widget("MainWindow").window.maximize()
                else:
                    self.wTree.get_widget("MainWindow").parse_geometry(self.window_geometry)
                    if self.window_max:
                        self.wTree.get_widget("MainWindow").window.maximize()

                self.invisible_cursor = self.prefs.getpref('invisible_cursor', 0)
                if self.invisible_cursor:
                        self.wTree.get_widget("MainWindow").window.set_cursor(invisible)
                else:
                        self.wTree.get_widget("MainWindow").window.set_cursor(None)
                self.wTree.get_widget("controlfontbutton").set_font_name(self.control_font_name)
                self.control_font = pango.FontDescription(self.control_font_name)

                self.wTree.get_widget("drofontbutton").set_font_name(self.dro_font_name)
                self.dro_font = pango.FontDescription(self.dro_font_name)

                self.wTree.get_widget("errorfontbutton").set_font_name(self.error_font_name)
                self.error_font = pango.FontDescription(self.error_font_name)

                self.wTree.get_widget("listingfontbutton").set_font_name(self.listing_font_name)
                self.listing_font = pango.FontDescription(self.listing_font_name)

                settings = gtk.settings_get_default()
                self.system_theme = settings.get_property("gtk-theme-name")
                if not self.theme_name == "Follow System Theme":
                    settings.set_string_property("gtk-theme-name", self.theme_name, "")

                # interactive mdi command builder and issuer
                mdi_labels = []
                mdi_eventboxes = []
                for i in range(self.num_mdi_labels):
                        mdi_labels.append(self.wTree.get_widget("mdi%d" % i))
                        mdi_eventboxes.append(self.wTree.get_widget("eventbox_mdi%d" % i))
                self.mdi_control = mdi.mdi_control(gtk, linuxcnc, mdi_labels, mdi_eventboxes)

                if self.ini:
                    self.mdi_control.mdi.add_macros(
                        self.ini.findall("TOUCHY", "MACRO"))

                listing_labels = []
                listing_eventboxes = []
                for i in range(self.num_listing_labels):
                        listing_labels.append(self.wTree.get_widget("listing%d" % i))
                        listing_eventboxes.append(self.wTree.get_widget("eventbox_listing%d" % i))
                self.listing = listing.listing(gtk, linuxcnc, listing_labels, listing_eventboxes)

                # emc interface
                self.linuxcnc = emc_interface.emc_control(linuxcnc, self.listing, self.wTree.get_widget("error"))
                self.linuxcnc.continuous_jog_velocity(self.mv_val)
                self.hal = hal_interface.hal_interface(self, self.linuxcnc, self.mdi_control, linuxcnc)

                # silly file chooser
                filechooser_labels = []
                filechooser_eventboxes = []
                for i in range(self.num_filechooser_labels):
                        filechooser_labels.append(self.wTree.get_widget("filechooser%d" % i))
                        filechooser_eventboxes.append(self.wTree.get_widget("eventbox_filechooser%d" % i))
                self.filechooser = filechooser.filechooser(gtk, linuxcnc, filechooser_labels, filechooser_eventboxes, self.listing)

                relative = ['xr', 'yr', 'zr', 'ar', 'br', 'cr', 'ur', 'vr', 'wr']
                absolute = ['xa', 'ya', 'za', 'aa', 'ba', 'ca', 'ua', 'va', 'wa']
                distance = ['xd', 'yd', 'zd', 'ad', 'bd', 'cd', 'ud', 'vd', 'wd']
                relative = [self.wTree.get_widget(i) for i in relative]
                absolute = [self.wTree.get_widget(i) for i in absolute]
                distance = [self.wTree.get_widget(i) for i in distance]
                
                estops = ['estop_reset', 'estop']
                estops = dict((i, self.wTree.get_widget(i)) for i in estops)
                machines = ['on', 'off']
                machines = dict((i, self.wTree.get_widget("machine_" + i)) for i in machines)
                floods = ['on', 'off']
                floods = dict((i, self.wTree.get_widget("flood_" + i)) for i in floods)
                mists = ['on', 'off']
                mists = dict((i, self.wTree.get_widget("mist_" + i)) for i in mists)
                spindles = ['forward', 'off', 'reverse']
                spindles = dict((i, self.wTree.get_widget("spindle_" + i)) for i in spindles)
                stats = ['file', 'file_lines', 'line', 'id', 'dtg', 'velocity', 'delay', 'onlimit',
                         'spindledir', 'spindlespeed', 'loadedtool', 'preppedtool',
                         'xyrotation', 'tlo', 'activecodes', 'spindlespeed2',
                         'label_g5xoffset', 'g5xoffset', 'g92offset', 'tooltable']
                stats = dict((i, self.wTree.get_widget("status_" + i)) for i in stats)
                prefs = ['actual', 'commanded', 'inch', 'mm']
                prefs = dict((i, self.wTree.get_widget("dro_" + i)) for i in prefs)
                opstop = ['on', 'off']
                opstop = dict((i, self.wTree.get_widget("opstop_" + i)) for i in opstop)
                blockdel = ['on', 'off']
                blockdel = dict((i, self.wTree.get_widget("blockdel_" + i)) for i in blockdel)
                self.status = emc_interface.emc_status(gtk, linuxcnc, self.listing, relative, absolute, distance,
                                                       self.wTree.get_widget("dro_table"),
                                                       self.wTree.get_widget("error"),
                                                       estops, machines,
                                                       self.wTree.get_widget("override_limits"),
                                                       stats,
                                                       floods, mists, spindles, prefs,
                                                       opstop, blockdel)

                self.current_file = self.status.emcstat.file
                # check the ini file if UNITS are set to mm"
                # first check the global settings
                units=self.ini.find("TRAJ","LINEAR_UNITS")

                if units==None:
                        units=self.ini.find("AXIS_0","UNITS")

                if units=="mm" or units=="metric" or units == "1.0":
                        self.machine_units_mm=1
                        conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
                else:
                        self.machine_units_mm=0
                        conversion=[25.4]*3+[1]*3+[25.4]*3

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
                        self.linuxcnc.blockdel_on(0)
                else:
                        self.linuxcnc.blockdel_off(0)

                if self.prefs.getpref('opstop', 1):
                        self.linuxcnc.opstop_on(0)
                else:
                        self.linuxcnc.opstop_off(0)                        

		self.linuxcnc.emccommand.program_open(empty_program.name)

                self.linuxcnc.max_velocity(self.mv_val)
                                
                gobject.timeout_add(50, self.periodic_status)
                gobject.timeout_add(100, self.periodic_radiobuttons)

                # event bindings
                dic = {
                        "quit" : self.quit,
                        "on_pointer_show_clicked" : self.pointer_show,
                        "on_pointer_hide_clicked" : self.pointer_hide,
                        "on_opstop_on_clicked" : self.opstop_on,
                        "on_opstop_off_clicked" : self.opstop_off,
                        "on_blockdel_on_clicked" : self.blockdel_on,
                        "on_blockdel_off_clicked" : self.blockdel_off,
                        "on_reload_tooltable_clicked" : self.linuxcnc.reload_tooltable,
                        "on_notebook1_switch_page" : self.tabselect,
                        "on_controlfontbutton_font_set" : self.change_control_font,
                        "on_drofontbutton_font_set" : self.change_dro_font,
                        "on_dro_actual_clicked" : self.dro_actual,
                        "on_dro_commanded_clicked" : self.dro_commanded,
                        "on_dro_inch_clicked" : self.dro_inch,
                        "on_dro_mm_clicked" : self.dro_mm,
                        "on_errorfontbutton_font_set" : self.change_error_font,
                        "on_listingfontbutton_font_set" : self.change_listing_font,
                        "on_estop_clicked" : self.linuxcnc.estop,
                        "on_estop_reset_clicked" : self.linuxcnc.estop_reset,
                        "on_machine_off_clicked" : self.linuxcnc.machine_off,
                        "on_machine_on_clicked" : self.linuxcnc.machine_on,
                        "on_mdi_clear_clicked" : self.mdi_control.clear,
                        "on_mdi_back_clicked" : self.mdi_control.back,
                        "on_mdi_next_clicked" : self.mdi_control.next,
                        "on_mdi_decimal_clicked" : self.mdi_control.decimal,
                        "on_mdi_minus_clicked" : self.mdi_control.minus,
                        "on_mdi_keypad_clicked" : self.mdi_control.keypad,
                        "on_mdi_g_clicked" : self.mdi_control.g,
                        "on_mdi_gp_clicked" : self.mdi_control.gp,
                        "on_mdi_m_clicked" : self.mdi_control.m,
                        "on_mdi_t_clicked" : self.mdi_control.t,
                        "on_mdi_select" : self.mdi_control.select,
                        "on_mdi_set_tool_clicked" : self.mdi_set_tool,
                        "on_mdi_set_origin_clicked" : self.mdi_set_origin,
                        "on_mdi_macro_clicked" : self.mdi_macro,
                        "on_filechooser_select" : self.fileselect,
                        "on_filechooser_up_clicked" : self.filechooser.up,
                        "on_filechooser_down_clicked" : self.filechooser.down,
                        "on_filechooser_reload_clicked" : self.filechooser.reload,
                        "on_listing_up_clicked" : self.listing.up,
                        "on_listing_down_clicked" : self.listing.down,
                        "on_listing_previous_clicked" : self.listing.previous,
                        "on_listing_next_clicked" : self.listing.next,
                        "on_mist_on_clicked" : self.linuxcnc.mist_on,
                        "on_mist_off_clicked" : self.linuxcnc.mist_off,
                        "on_flood_on_clicked" : self.linuxcnc.flood_on,
                        "on_flood_off_clicked" : self.linuxcnc.flood_off,
                        "on_home_all_clicked" : self.linuxcnc.home_all,
                        "on_unhome_all_clicked" : self.linuxcnc.unhome_all,
                        "on_home_selected_clicked" : self.home_selected,
                        "on_unhome_selected_clicked" : self.unhome_selected,
                        "on_fo_clicked" : self.fo,
                        "on_so_clicked" : self.so,
                        "on_mv_clicked" : self.mv,
                        "on_jogging_clicked" : self.jogging,
                        "on_scrolling_clicked" : self.scrolling,
                        "on_wheelx_clicked" : self.wheelx,
                        "on_wheely_clicked" : self.wheely,
                        "on_wheelz_clicked" : self.wheelz,
                        "on_wheela_clicked" : self.wheela,
                        "on_wheelb_clicked" : self.wheelb,
                        "on_wheelc_clicked" : self.wheelc,
                        "on_wheelu_clicked" : self.wheelu,
                        "on_wheelv_clicked" : self.wheelv,
                        "on_wheelw_clicked" : self.wheelw,
                        "on_wheelinc1_clicked" : self.wheelinc1,
                        "on_wheelinc2_clicked" : self.wheelinc2,
                        "on_wheelinc3_clicked" : self.wheelinc3,
                        "on_override_limits_clicked" : self.linuxcnc.override_limits,
                        "on_spindle_forward_clicked" : self.linuxcnc.spindle_forward,
                        "on_spindle_off_clicked" : self.linuxcnc.spindle_off,
                        "on_spindle_reverse_clicked" : self.linuxcnc.spindle_reverse,
                        "on_spindle_slower_clicked" : self.linuxcnc.spindle_slower,
                        "on_spindle_faster_clicked" : self.linuxcnc.spindle_faster,
                        "on_toolset_fixture_clicked" : self.toolset_fixture,
                        "on_toolset_workpiece_clicked" : self.toolset_workpiece,
                        "on_changetheme_clicked" : self.change_theme,
                        }
                self.wTree.signal_autoconnect(dic)

		for widget in self.wTree.get_widget_prefix(''):
			if isinstance(widget, gtk.Button):
				widget.connect_after('released',self.hack_leave)

                self._dynamic_childs = {}
                atexit.register(self.kill_dynamic_childs)
                self.set_dynamic_tabs()

                atexit.register(self.save_maxvel_pref)

                self.setfont()

        def quit(self, unused):
                gtk.main_quit()

        def tabselect(self, notebook, b, tab):
                self.tab = tab

        def pointer_hide(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('invisible_cursor', 1)
                self.invisible_cursor = 1
                self.wTree.get_widget("MainWindow").window.set_cursor(invisible)

        def pointer_show(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('invisible_cursor', 0)
                self.invisible_cursor = 0
                self.wTree.get_widget("MainWindow").window.set_cursor(None)

        def dro_commanded(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('dro_actual', 0)
                self.status.dro_commanded(b)

        def dro_actual(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('dro_actual', 1)
                self.status.dro_actual(b)

        def dro_inch(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('dro_mm', 0)
                self.status.dro_inch(b)

        def dro_mm(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('dro_mm', 1)
                self.status.dro_mm(b)

        def opstop_on(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('opstop', 1)
                self.linuxcnc.opstop_on(b)

        def opstop_off(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('opstop', 0)
                self.linuxcnc.opstop_off(b)

        def blockdel_on(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('blockdel', 1)
                self.linuxcnc.blockdel_on(b)

        def blockdel_off(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('blockdel', 0)
                self.linuxcnc.blockdel_off(b)

        def wheelx(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 0

        def wheely(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 1

        def wheelz(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 2

        def wheela(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 3

        def wheelb(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 4

        def wheelc(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 5

        def wheelu(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 6

        def wheelv(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 7

        def wheelw(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 8

        def wheelinc1(self, b):
                if self.radiobutton_mask: return
                self.wheelinc = 0

        def home_selected(self, b):
                self.linuxcnc.home_selected(self.wheelxyz)

        def unhome_selected(self, b):
                self.linuxcnc.unhome_selected(self.wheelxyz)

        def wheelinc2(self, b):
                if self.radiobutton_mask: return
                self.wheelinc = 1

        def wheelinc3(self, b):
                if self.radiobutton_mask: return
                self.wheelinc = 2

        def fo(self, b):
                if self.radiobutton_mask: return
                self.wheel = "fo"
                self.jogsettings_activate(0)

        def so(self, b):
                if self.radiobutton_mask: return
                self.wheel = "so"
                self.jogsettings_activate(0)

        def mv(self, b):
                if self.radiobutton_mask: return
                self.wheel = "mv"
                self.jogsettings_activate(0)

        def scrolling(self, b):
                if self.radiobutton_mask: return
                self.wheel = "scrolling"
                self.jogsettings_activate(1)

        def jogging(self, b):
                if self.radiobutton_mask: return
                self.wheel = "jogging"
                self.linuxcnc.jogging(b)
                self.jogsettings_activate(1)

        def toolset_fixture(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('toolsetting_fixture', 1)
                self.g10l11 = 1

        def toolset_workpiece(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('toolsetting_fixture', 0)
                self.g10l11 = 0

        def jogsettings_activate(self, active):
                for i in ["wheelinc1", "wheelinc2", "wheelinc3"]:
                        w = self.wTree.get_widget(i)
                        w.set_sensitive(active)
                self.hal.jogactive(active)
        
        def change_control_font(self, fontbutton):
                self.control_font_name = fontbutton.get_font_name()
                self.prefs.putpref('control_font', self.control_font_name, str)
                self.control_font = pango.FontDescription(self.control_font_name)
                self.setfont()

        def change_dro_font(self, fontbutton):
                self.dro_font_name = fontbutton.get_font_name()
                self.prefs.putpref('dro_font', self.dro_font_name, str)
                self.dro_font = pango.FontDescription(self.dro_font_name)
                self.setfont()

        def change_error_font(self, fontbutton):
                self.error_font_name = fontbutton.get_font_name()
                self.prefs.putpref('error_font', self.error_font_name, str)
                self.error_font = pango.FontDescription(self.error_font_name)
                self.setfont()

        def change_listing_font(self, fontbutton):
                self.listing_font_name = fontbutton.get_font_name()
                self.prefs.putpref('listing_font', self.listing_font_name, str)
                self.listing_font = pango.FontDescription(self.listing_font_name)
                self.setfont()

        def change_theme(self,b):
            theme = self.wTree.get_widget("theme_choice").get_active_text()
            self.prefs.putpref('gtk_theme', theme, str)
            if theme == "Follow System Theme":
                theme = self.system_theme
            settings = gtk.settings_get_default()
            settings.set_string_property("gtk-theme-name", theme, "")

        def setfont(self):
                # buttons
                for i in ["1", "2", "3", "4", "5", "6", "7",
                          "8", "9", "0", "minus", "decimal",
                          "flood_on", "flood_off", "mist_on", "mist_off",
                          "g", "gp", "m", "t", "set_tool", "set_origin", "macro",
                          "estop", "estop_reset", "machine_off", "machine_on",
                          "home_all", "unhome_all", "home_selected", "unhome_selected",
                          "fo", "so", "mv", "jogging", "scrolling", "wheelinc1", "wheelinc2", "wheelinc3",
                          "wheelx", "wheely", "wheelz",
                          "wheela", "wheelb", "wheelc",
                          "wheelu", "wheelv", "wheelw",
                          "override_limits",
                          "spindle_forward", "spindle_off", "spindle_reverse",
                          "spindle_faster", "spindle_slower",
                          "dro_commanded", "dro_actual", "dro_inch", "dro_mm",
                          "reload_tooltable", "opstop_on", "opstop_off",
                          "blockdel_on", "blockdel_off", "pointer_hide", "pointer_show",
                          "toolset_workpiece", "toolset_fixture","change_theme"]:
                        w = self.wTree.get_widget(i)
                        if w:
                                w = w.child
                                w.modify_font(self.control_font)

		notebook = self.wTree.get_widget('notebook1')
		for i in range(notebook.get_n_pages()):
			w = notebook.get_nth_page(i)
			notebook.get_tab_label(w).modify_font(self.control_font)

                # labels
                for i in range(self.num_mdi_labels):
                        w = self.wTree.get_widget("mdi%d" % i)
                        w.modify_font(self.control_font)
                for i in range(self.num_filechooser_labels):
                        w = self.wTree.get_widget("filechooser%d" % i)
                        w.modify_font(self.control_font)
                for i in range(self.num_listing_labels):
                        w = self.wTree.get_widget("listing%d" % i)
                        w.modify_font(self.listing_font)
                for i in ["mdi", "startup", "manual", "auto", "preferences", "status",
                          "relative", "absolute", "dtg", "ss2label", "status_spindlespeed2"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.control_font)

                # dro
                for i in ['xr', 'yr', 'zr', 'ar', 'br', 'cr', 'ur', 'vr', 'wr',
                          'xa', 'ya', 'za', 'aa', 'ba', 'ca', 'ua', 'va', 'wa',
                          'xd', 'yd', 'zd', 'ad', 'bd', 'cd', 'ud', 'vd', 'wd']:
                        w = self.wTree.get_widget(i)
                        if w:
                            w.modify_font(self.dro_font)
                            if "r" in i and not self.rel_textcolor == "default":
                                w.modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse(self.rel_textcolor))
                            elif "a" in i and not self.abs_textcolor == "default":
                                w.modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse(self.abs_textcolor))
                            elif "d" in i and not self.dtg_textcolor == "default":
                                w.modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse(self.dtg_textcolor))

                # status bar
                for i in ["error"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.error_font)
                        if not self.err_textcolor == "default":
                            w.modify_fg(gtk.STATE_NORMAL,gtk.gdk.color_parse(self.err_textcolor))

        def mdi_set_tool(self, b):
                self.mdi_control.set_tool(self.status.get_current_tool(), self.g10l11)

        def mdi_set_origin(self, b):
                self.mdi_control.set_origin(self.status.get_current_system())

        def mdi_macro(self, b):
                self.mdi_control.o(b)

        def fileselect(self, eb, e):
                if self.wheel == "jogging": self.wheel = "mv"
                self.jogsettings_activate(1)
                self.current_file = self.filechooser.select(eb, e)
                self.listing.clear_startline()

        def periodic_status(self):
                self.linuxcnc.mask()
                self.radiobutton_mask = 1
                self.status.periodic()

                # check if current_file changed
                # perhaps by another gui or a gladevcp app
                if self.current_file != self.status.emcstat.file:
                    self.current_file = self.status.emcstat.file
                    self.filechooser.select_and_show(self.current_file)

                self.radiobutton_mask = 0
                self.linuxcnc.unmask()
                self.hal.periodic(self.tab == 1) # MDI tab?
                return True

        def periodic_radiobuttons(self):
                self.radiobutton_mask = 1
                s = linuxcnc.stat()
                s.poll()
                am = s.axis_mask
                if not self.resized_wheelbuttons:
                        at = self.wTree.get_widget("axis_table")
                        for i in range(9):
                                b = ["wheelx", "wheely", "wheelz",
                                     "wheela", "wheelb", "wheelc",
                                     "wheelu", "wheelv", "wheelw"][i]
                                w = self.wTree.get_widget(b)
                                if not (am & (1<<i)):
                                        at.remove(w)
                        if (am & 0700) == 0:
                                at.resize(3, 2)
                                if (am & 070) == 0:
                                        at.resize(3, 1)
                                        self.wTree.get_widget("wheel_hbox").set_homogeneous(1)
                        self.resized_wheelbuttons = 1
                
                self.wTree.get_widget("scrolling").set_sensitive(self.tab == 3)
                if self.tab != 3 and self.wheel == "scrolling":
                        self.jogsettings_activate(0)
                        self.wheel = "fo"

                set_active(self.wTree.get_widget("wheelx"), self.wheelxyz == 0)
                set_active(self.wTree.get_widget("wheely"), self.wheelxyz == 1)
                set_active(self.wTree.get_widget("wheelz"), self.wheelxyz == 2)
                set_active(self.wTree.get_widget("wheela"), self.wheelxyz == 3)
                set_active(self.wTree.get_widget("wheelb"), self.wheelxyz == 4)
                set_active(self.wTree.get_widget("wheelc"), self.wheelxyz == 5)
                set_active(self.wTree.get_widget("wheelu"), self.wheelxyz == 6)
                set_active(self.wTree.get_widget("wheelv"), self.wheelxyz == 7)
                set_active(self.wTree.get_widget("wheelw"), self.wheelxyz == 8)
                set_active(self.wTree.get_widget("wheelinc1"), self.wheelinc == 0)
                set_active(self.wTree.get_widget("wheelinc2"), self.wheelinc == 1)
                set_active(self.wTree.get_widget("wheelinc3"), self.wheelinc == 2)
                set_active(self.wTree.get_widget("fo"), self.wheel == "fo")
                set_active(self.wTree.get_widget("so"), self.wheel == "so")
                set_active(self.wTree.get_widget("mv"), self.wheel == "mv")
                set_active(self.wTree.get_widget("jogging"), self.wheel == "jogging")
                set_active(self.wTree.get_widget("scrolling"), self.wheel == "scrolling")
                set_active(self.wTree.get_widget("pointer_show"), not self.invisible_cursor)
                set_active(self.wTree.get_widget("pointer_hide"), self.invisible_cursor)
                set_active(self.wTree.get_widget("toolset_workpiece"), not self.g10l11)
                set_active(self.wTree.get_widget("toolset_fixture"), self.g10l11)
                self.radiobutton_mask = 0

                if self.wheel == "jogging":
                        self.hal.jogaxis(self.wheelxyz)
                else:
                        # disable all
                        self.hal.jogaxis(-1)

                if self.wheel == "scrolling":
                        incs = ["100", "10", "1"]
                elif self.wheelxyz == 3 or self.wheelxyz == 4 or self.wheelxyz == 5:
                        incs = ["1.0", "0.1", "0.01"]
                elif self.machine_units_mm:
                        incs = ["0.1", "0.01", "0.001"]
                else:
                        incs = ["0.01", "0.001", "0.0001"]

                set_label(self.wTree.get_widget("wheelinc1").child, incs[0])
                set_label(self.wTree.get_widget("wheelinc2").child, incs[1])
                set_label(self.wTree.get_widget("wheelinc3").child, incs[2])

                self.hal.jogincrement(self.wheelinc, map(float,incs))

                d = self.hal.wheel()
                if self.wheel == "fo":
                        self.fo_val += d
                        if self.fo_val < 0: self.fo_val = 0
                        if d != 0: self.linuxcnc.feed_override(self.fo_val)

                if self.wheel == "so":
                        self.so_val += d
                        if self.so_val < 0: self.so_val = 0
                        if d != 0: self.linuxcnc.spindle_override(self.so_val)

                if self.wheel == "mv":
                        if self.machine_units_mm:
                                self.mv_val += 20 * d
                        else:
                                self.mv_val += d
                        if self.mv_val < 0: self.mv_val = 0
                        if d != 0:
                                self.linuxcnc.max_velocity(self.mv_val)
                                self.linuxcnc.continuous_jog_velocity(self.mv_val)
                        
		if self.wheel == "scrolling":
			d0 = d * 10 ** (2-self.wheelinc)
			if d != 0: self.listing.next(None, d0)

                set_label(self.wTree.get_widget("fo").child, "FO: %d%%" % self.fo_val)
                set_label(self.wTree.get_widget("so").child, "SO: %d%%" % self.so_val)
                set_label(self.wTree.get_widget("mv").child, "MV: %d" % self.mv_val)

                        
                return True

	def hack_leave(self,w):
		if not self.invisible_cursor: return
		w = self.wTree.get_widget("MainWindow").window
		d = w.get_display()
		s = w.get_screen()
		x, y = w.get_origin()
		d.warp_pointer(s, x, y)

	def _dynamic_tab(self, notebook, text):
		s = gtk.Socket()
		notebook.append_page(s, gtk.Label(" " + text + " "))
		return s.get_id()

	def set_dynamic_tabs(self):
		from subprocess import Popen

		if not self.ini:
			return

		tab_names = self.ini.findall("DISPLAY", "EMBED_TAB_NAME")
		tab_cmd   = self.ini.findall("DISPLAY", "EMBED_TAB_COMMAND")

		if len(tab_names) != len(tab_cmd):
			print "Invalid tab configuration" # Complain somehow

		nb = self.wTree.get_widget('notebook1')
		for t,c in zip(tab_names, tab_cmd):
			xid = self._dynamic_tab(nb, t)
			if not xid: continue
			cmd = c.replace('{XID}', str(xid))
			child = Popen(cmd.split())
			self._dynamic_childs[xid] = child
		nb.show_all()

	def kill_dynamic_childs(self):
		for c in self._dynamic_childs.values():
			c.terminate()

        def save_maxvel_pref(self):
                self.prefs.putpref('maxvel', self.mv_val, int)

	def postgui(self):
		postgui_halfile = self.ini.find("HAL", "POSTGUI_HALFILE")
		return postgui_halfile,sys.argv[2]

if __name__ == "__main__":
        if len(sys.argv) > 2 and sys.argv[1] == '-ini':
            print "ini", sys.argv[2]
            hwg = touchy(sys.argv[2])
        else:
            hwg = touchy()
	res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-f", "touchy.hal"])
	if res: raise SystemExit, res
	# load a postgui file if one is present in the INI file
	postgui_halfile,inifile = touchy.postgui(hwg)
	print "TOUCHY postgui filename:",postgui_halfile
	if postgui_halfile:
		res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i",inifile,"-f", postgui_halfile])
		if res: raise SystemExit, res
	gtk.main()
