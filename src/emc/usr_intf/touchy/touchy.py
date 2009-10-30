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
libdir = os.path.join(BASE, "lib", "python", "touchy")
sys.path.insert(0, libdir)
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

import emc
import emc_interface
import mdi
import hal_interface
import filechooser
import listing
import preferences

if gtk.gtk_version >= (2, 10, 0):
    gtk.rc_parse_string("""
	gtk_color_scheme = "bg_color:#dcdad5\nfg_color:#000"
	style "touchy-default-style" {
	    bg[PRELIGHT] = @bg_color
	    bg[NORMAL] = @bg_color
	    bg[ACTIVE] = @bg_color
	    bg[INSENSITIVE] = @bg_color
	    fg[PRELIGHT] = @fg_color
	    fg[NORMAL] = @fg_color
	    fg[ACTIVE] = @fg_color
	    fg[INSENSITIVE] = darker (@bg_color)
	}
	class "GtkWidget" style "touchy-default-style"
    """)
else:
    gtk.rc_parse_string("""
	style "touchy-default-style" {
	    bg[PRELIGHT] = "#dcdad5"
	    bg[NORMAL] = "#dcdad5"
	    bg[ACTIVE] = "#dcdad5"
	    bg[INSENSITIVE] = "#dcdad5"
	    fg[PRELIGHT] = "#000"
	    fg[NORMAL] = "#000"
	    fg[ACTIVE] = "#000"
	    fg[INSENSITIVE] = "#9a9895"
	}
	class "GtkWidget" style "touchy-default-style"
    """)

pix_data = """/* XPM */
static char * invisible_xpm[] = {
"1 1 1 1",
"	c None",
" "};"""

color = gtk.gdk.Color()
pix = gtk.gdk.pixmap_create_from_data(None, pix_data, 1, 1, 1, color, color)
invisible = gtk.gdk.Cursor(pix, pix, color, color, 0, 0)

class touchy:
	def __init__(self):
		#Set the Glade file
		self.gladefile = os.path.join(libdir, "touchy.glade")
	        self.wTree = gtk.glade.XML(self.gladefile) 
                
                self.num_mdi_labels = 11
                self.num_filechooser_labels = 11
                self.num_listing_labels = 20

                self.wheelxyz = 0
                self.wheelinc = 0
                self.wheel = "fo"
                self.radiobutton_mask = 0

                self.tab = 0

                self.fo_val = 100
                self.so_val = 100
                self.mv_val = 100

                self.prefs = preferences.preferences()
                self.control_font_name = self.prefs.getpref('control_font', 'Sans 18', str)
                self.dro_font_name = self.prefs.getpref('dro_font', 'Courier 10 Pitch Bold 16', str)
                self.error_font_name = self.prefs.getpref('error_font', 'Sans Bold 10', str)
                self.listing_font_name = self.prefs.getpref('listing_font', 'Sans 10', str)

                # initial screen setup
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

                self.setfont()

                # interactive mdi command builder and issuer
                mdi_labels = []
                mdi_eventboxes = []
                for i in range(self.num_mdi_labels):
                        mdi_labels.append(self.wTree.get_widget("mdi%d" % i))
                        mdi_eventboxes.append(self.wTree.get_widget("eventbox_mdi%d" % i))
                self.mdi_control = mdi.mdi_control(gtk, emc, mdi_labels, mdi_eventboxes)

                listing_labels = []
                listing_eventboxes = []
                for i in range(self.num_listing_labels):
                        listing_labels.append(self.wTree.get_widget("listing%d" % i))
                        listing_eventboxes.append(self.wTree.get_widget("eventbox_listing%d" % i))
                self.listing = listing.listing(gtk, emc, listing_labels, listing_eventboxes)

                # emc interface
                self.emc = emc_interface.emc_control(emc, self.listing, self.wTree.get_widget("error"))
                self.hal = hal_interface.hal_interface(self, self.emc, self.mdi_control)

                # silly file chooser
                filechooser_labels = []
                filechooser_eventboxes = []
                for i in range(self.num_filechooser_labels):
                        filechooser_labels.append(self.wTree.get_widget("filechooser%d" % i))
                        filechooser_eventboxes.append(self.wTree.get_widget("eventbox_filechooser%d" % i))
                self.filechooser = filechooser.filechooser(gtk, emc, filechooser_labels, filechooser_eventboxes, self.listing)

                status_labels = ['xr', 'yr', 'zr', 'xa', 'ya', 'za', 'xd', 'yd', 'zd']
                status_labels = dict((i, self.wTree.get_widget(i)) for i in status_labels)
                homes = ['x', 'y', 'z']
                homes = dict((i, self.wTree.get_widget("home_" + i)) for i in homes)
                unhomes = ['x', 'y', 'z']
                unhomes = dict((i, self.wTree.get_widget("unhome_" + i)) for i in unhomes)
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
                stats = ['file', 'line', 'id', 'dtg', 'velocity', 'delay', 'onlimit',
                         'spindledir', 'spindlespeed', 'loadedtool', 'preppedtool',
                         'xyrotation', 'tlo', 'activecodes']
                stats = dict((i, self.wTree.get_widget("status_" + i)) for i in stats)
                prefs = ['actual', 'commanded', 'inch', 'mm']
                prefs = dict((i, self.wTree.get_widget("dro_" + i)) for i in prefs)
                opstop = ['on', 'off']
                opstop = dict((i, self.wTree.get_widget("opstop_" + i)) for i in opstop)
                blockdel = ['on', 'off']
                blockdel = dict((i, self.wTree.get_widget("blockdel_" + i)) for i in blockdel)

                self.status = emc_interface.emc_status(gtk, emc, self.listing, status_labels,
                                                       self.wTree.get_widget("error"),
                                                       homes, unhomes,
                                                       estops, machines,
                                                       self.wTree.get_widget("override_limits"),
                                                       stats,
                                                       floods, mists, spindles, prefs,
                                                       opstop, blockdel)
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
                        "on_reload_tooltable_clicked" : self.emc.reload_tooltable,
                        "on_notebook1_switch_page" : self.tabselect,
                        "on_controlfontbutton_font_set" : self.change_control_font,
                        "on_drofontbutton_font_set" : self.change_dro_font,
                        "on_dro_actual_clicked" : self.dro_actual,
                        "on_dro_commanded_clicked" : self.dro_commanded,
                        "on_dro_inch_clicked" : self.dro_inch,
                        "on_dro_mm_clicked" : self.dro_mm,
                        "on_errorfontbutton_font_set" : self.change_error_font,
                        "on_listingfontbutton_font_set" : self.change_listing_font,
                        "on_estop_clicked" : self.emc.estop,
                        "on_estop_reset_clicked" : self.emc.estop_reset,
                        "on_machine_off_clicked" : self.emc.machine_off,
                        "on_machine_on_clicked" : self.emc.machine_on,
                        "on_mdi_clear_clicked" : self.mdi_control.clear,
                        "on_mdi_back_clicked" : self.mdi_control.back,
                        "on_mdi_next_clicked" : self.mdi_control.next,
                        "on_mdi_decimal_clicked" : self.mdi_control.decimal,
                        "on_mdi_minus_clicked" : self.mdi_control.minus,
                        "on_mdi_keypad_clicked" : self.mdi_control.keypad,
                        "on_mdi_g_clicked" : self.mdi_control.g,
                        "on_mdi_m_clicked" : self.mdi_control.m,
                        "on_mdi_t_clicked" : self.mdi_control.t,
                        "on_mdi_select" : self.mdi_control.select,
                        "on_mdi_set_tool_clicked" : self.mdi_set_tool,
                        "on_mdi_set_origin_clicked" : self.mdi_set_origin,
                        "on_filechooser_select" : self.fileselect,
                        "on_filechooser_up_clicked" : self.filechooser.up,
                        "on_filechooser_down_clicked" : self.filechooser.down,
                        "on_filechooser_reload_clicked" : self.filechooser.reload,
                        "on_listing_up_clicked" : self.listing.up,
                        "on_listing_down_clicked" : self.listing.down,
                        "on_listing_previous_clicked" : self.listing.previous,
                        "on_listing_next_clicked" : self.listing.next,
                        "on_mist_on_clicked" : self.emc.mist_on,
                        "on_mist_off_clicked" : self.emc.mist_off,
                        "on_flood_on_clicked" : self.emc.flood_on,
                        "on_flood_off_clicked" : self.emc.flood_off,
                        "on_home_all_clicked" : self.emc.home_all,
                        "on_unhome_all_clicked" : self.emc.unhome_all,
                        "on_home_x_clicked" : self.emc.home_x,
                        "on_home_y_clicked" : self.emc.home_y,
                        "on_home_z_clicked" : self.emc.home_z,
                        "on_unhome_x_clicked" : self.emc.unhome_x,
                        "on_unhome_y_clicked" : self.emc.unhome_y,
                        "on_unhome_z_clicked" : self.emc.unhome_z,
                        "on_fo_clicked" : self.fo,
                        "on_so_clicked" : self.so,
                        "on_mv_clicked" : self.mv,
                        "on_jogging_clicked" : self.jogging,
                        "on_wheelx_clicked" : self.wheelx,
                        "on_wheely_clicked" : self.wheely,
                        "on_wheelz_clicked" : self.wheelz,
                        "on_wheelinc1_clicked" : self.wheelinc1,
                        "on_wheelinc2_clicked" : self.wheelinc2,
                        "on_wheelinc3_clicked" : self.wheelinc3,
                        "on_override_limits_clicked" : self.emc.override_limits,
                        "on_spindle_forward_clicked" : self.emc.spindle_forward,
                        "on_spindle_off_clicked" : self.emc.spindle_off,
                        "on_spindle_reverse_clicked" : self.emc.spindle_reverse,
                        "on_spindle_slower_clicked" : self.emc.spindle_slower,
                        "on_spindle_faster_clicked" : self.emc.spindle_faster,
                        }
                self.wTree.signal_autoconnect(dic)

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
                self.emc.opstop_on(b)

        def opstop_off(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('opstop', 0)
                self.emc.opstop_off(b)

        def blockdel_on(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('blockdel', 1)
                self.emc.blockdel_on(b)

        def blockdel_off(self, b):
                if self.radiobutton_mask: return
                self.prefs.putpref('blockdel', 0)
                self.emc.blockdel_off(b)

        def wheelx(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 0

        def wheely(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 1

        def wheelz(self, b):
                if self.radiobutton_mask: return
                self.wheelxyz = 2

        def wheelinc1(self, b):
                if self.radiobutton_mask: return
                self.wheelinc = 0

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

        def jogging(self, b):
                if self.radiobutton_mask: return
                self.wheel = "jogging"
                self.emc.jogging(b)
                self.jogsettings_activate(1)

        def jogsettings_activate(self, active):
                for i in ["wheelx", "wheely", "wheelz",
                          "wheelinc1", "wheelinc2", "wheelinc3"]:
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

        def setfont(self):
                # buttons
                for i in ["1", "2", "3", "4", "5", "6", "7",
                          "8", "9", "0", "minus", "decimal",
                          "flood_on", "flood_off", "mist_on", "mist_off",
                          "g", "m", "t", "set_tool", "set_origin",
                          "estop", "estop_reset", "machine_off", "machine_on",
                          "home_all", "unhome_all", "home_x", "unhome_x",
                          "home_y", "unhome_y", "home_z", "unhome_z",
                          "fo", "so", "mv", "jogging", "wheelinc1", "wheelinc2", "wheelinc3",
                          "wheelx", "wheely", "wheelz", "override_limits",
                          "spindle_forward", "spindle_off", "spindle_reverse",
                          "spindle_faster", "spindle_slower",
                          "dro_commanded", "dro_actual", "dro_inch", "dro_mm",
                          "reload_tooltable", "opstop_on", "opstop_off",
                          "blockdel_on", "blockdel_off", "pointer_hide", "pointer_show"]:
                        w = self.wTree.get_widget(i).child
                        w.modify_font(self.control_font)

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
                          "relative", "absolute", "dtg"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.control_font)

                # dro
                for i in ["xa", "ya", "za", "xr", "yr", "zr", "xd", "yd", "zd"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.dro_font)

                # status bar
                for i in ["error"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.error_font)

        def mdi_set_tool(self, b):
                self.mdi_control.set_tool(self.status.get_current_tool())

        def mdi_set_origin(self, b):
                self.mdi_control.set_origin(self.status.get_current_system())

        def fileselect(self, eb, e):
                if self.wheel == "jogging": self.wheel = "mv"
                self.jogsettings_activate(0)
                self.filechooser.select(eb, e)
                self.listing.clear_startline()

        def periodic_status(self):
                self.emc.mask()
                self.radiobutton_mask = 1
                self.status.periodic()
                self.radiobutton_mask = 0
                self.emc.unmask()
                self.hal.periodic(self.tab == 1) # MDI tab?
                return True

        def periodic_radiobuttons(self):
                self.radiobutton_mask = 1
                self.wTree.get_widget("wheelx").set_active(self.wheelxyz == 0)
                self.wTree.get_widget("wheely").set_active(self.wheelxyz == 1)
                self.wTree.get_widget("wheelz").set_active(self.wheelxyz == 2)
                self.wTree.get_widget("wheelinc1").set_active(self.wheelinc == 0)
                self.wTree.get_widget("wheelinc2").set_active(self.wheelinc == 1)
                self.wTree.get_widget("wheelinc3").set_active(self.wheelinc == 2)
                self.wTree.get_widget("fo").set_active(self.wheel == "fo")
                self.wTree.get_widget("so").set_active(self.wheel == "so")
                self.wTree.get_widget("mv").set_active(self.wheel == "mv")
                self.wTree.get_widget("jogging").set_active(self.wheel == "jogging")
                self.wTree.get_widget("pointer_show").set_active(not self.invisible_cursor)
                self.wTree.get_widget("pointer_hide").set_active(self.invisible_cursor)
                self.radiobutton_mask = 0

                if self.wheel == "jogging":
                        self.hal.jogaxis(self.wheelxyz)
                else:
                        # disable all
                        self.hal.jogaxis(-1)
                self.hal.jogincrement(self.wheelinc)

                d = self.hal.wheel()
                if self.wheel == "fo":
                        self.fo_val += d
                        if self.fo_val < 0: self.fo_val = 0
                        if d != 0: self.emc.feed_override(self.fo_val)

                if self.wheel == "so":
                        self.so_val += d
                        if self.so_val < 0: self.so_val = 0
                        if d != 0: self.emc.spindle_override(self.so_val)

                if self.wheel == "mv":
                        self.mv_val += d
                        if self.mv_val < 0: self.mv_val = 0
                        if d != 0:
                                self.emc.max_velocity(self.mv_val)
                                self.emc.continuous_jog_velocity(self.mv_val)
                        

                self.wTree.get_widget("fo").child.set_label("FO: %d%%" % self.fo_val)
                self.wTree.get_widget("so").child.set_label("SO: %d%%" % self.so_val)
                self.wTree.get_widget("mv").child.set_label("MV: %d" % self.mv_val)

                        
                return True

if __name__ == "__main__":
	hwg = touchy()
	res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-f", "touchy.hal"])
	if res: raise SystemExit, res
	gtk.main()
