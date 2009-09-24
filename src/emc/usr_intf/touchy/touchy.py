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

class touchy:
	def __init__(self):
		#Set the Glade file
		self.gladefile = os.path.join(libdir, "touchy.glade")
	        self.wTree = gtk.glade.XML(self.gladefile) 
		#self.window = self.wTree.get_widget("MainWindow")
                #self.style = gtk.Style().copy()

                self.num_mdi_labels = 11
                self.num_filechooser_labels = 11
                self.num_listing_labels = 20

                self.wheelxyz = 0
                self.wheelinc = 0
                self.wheel = "fo"
                self.radiobutton_mask = 0

                self.fo_val = 100
                self.so_val = 100
                self.mv_val = 100

                # initial screen setup
                # XXX read these fonts from preferences
                self.wTree.get_widget("controlfontbutton").set_font_name("Sans 13")
                self.control_font = pango.FontDescription("Sans 13")

                self.wTree.get_widget("drofontbutton").set_font_name("Courier 10 Pitch Bold 14")
                self.dro_font = pango.FontDescription("Courier 10 Pitch Bold 14")

                self.wTree.get_widget("errorfontbutton").set_font_name("Sans Bold 8")
                self.error_font = pango.FontDescription("Sans Bold 8")

                self.wTree.get_widget("listingfontbutton").set_font_name("Sans 8")
                self.listing_font = pango.FontDescription("Sans 8")

                self.setfont()

                # interactive mdi command builder and issuer
                mdi_labels = []
                mdi_eventboxes = []
                for i in range(self.num_mdi_labels):
                        mdi_labels.append(self.wTree.get_widget("mdi%d" % i))
                        mdi_eventboxes.append(self.wTree.get_widget("eventbox_mdi%d" % i))
                self.mdi_control = mdi.mdi_control(gtk, emc, mdi_labels, mdi_eventboxes)

                # emc interface
                self.emc = emc_interface.emc_control(emc)
                self.hal = hal_interface.hal_interface(self.emc)

                listing_labels = []
                listing_eventboxes = []
                for i in range(self.num_listing_labels):
                        listing_labels.append(self.wTree.get_widget("listing%d" % i))
                        listing_eventboxes.append(self.wTree.get_widget("eventbox_listing%d" % i))
                self.listing = listing.listing(gtk, emc, listing_labels, listing_eventboxes)

                # silly file chooser
                filechooser_labels = []
                filechooser_eventboxes = []
                for i in range(self.num_filechooser_labels):
                        filechooser_labels.append(self.wTree.get_widget("filechooser%d" % i))
                        filechooser_eventboxes.append(self.wTree.get_widget("eventbox_filechooser%d" % i))
                self.filechooser = filechooser.filechooser(gtk, emc, filechooser_labels, filechooser_eventboxes, self.listing)

                status_labels = {'xr' : self.wTree.get_widget("xr"),
                                 'yr' : self.wTree.get_widget("yr"),
                                 'zr' : self.wTree.get_widget("zr"),
                                 'xa' : self.wTree.get_widget("xa"),
                                 'ya' : self.wTree.get_widget("ya"),
                                 'za' : self.wTree.get_widget("za"),
                                 'xd' : self.wTree.get_widget("xd"),
                                 'yd' : self.wTree.get_widget("yd"),
                                 'zd' : self.wTree.get_widget("zd")}
                homes = {'x' : self.wTree.get_widget("home_x"),
                         'y' : self.wTree.get_widget("home_y"),
                         'z' : self.wTree.get_widget("home_z")}
                unhomes = {'x' : self.wTree.get_widget("unhome_x"),
                           'y' : self.wTree.get_widget("unhome_y"),
                           'z' : self.wTree.get_widget("unhome_z")}
                estops = {'estop_reset' : self.wTree.get_widget("estop_reset"),
                          'estop' : self.wTree.get_widget("estop")}
                machines = {'on' : self.wTree.get_widget("machine_on"),
                            'off' : self.wTree.get_widget("machine_off")}
                floods = {'on' : self.wTree.get_widget("flood_on"),
                            'off' : self.wTree.get_widget("flood_off")}
                mists = {'on' : self.wTree.get_widget("mist_on"),
                            'off' : self.wTree.get_widget("mist_off")}
                spindles = {'forward' : self.wTree.get_widget("spindle_forward"),
                            'off' : self.wTree.get_widget("spindle_off"),
                            'reverse' : self.wTree.get_widget("spindle_reverse")}
                stats = {'file' : self.wTree.get_widget("status_file"),
                         'line' : self.wTree.get_widget("status_line"),
                         'id' : self.wTree.get_widget("status_id"),
                         'dtg' : self.wTree.get_widget("status_dtg"),
                         'velocity' : self.wTree.get_widget("status_velocity"),
                         'delay' : self.wTree.get_widget("status_delay"),
                         'onlimit' : self.wTree.get_widget("status_onlimit"),
                         'spindledir' : self.wTree.get_widget("status_spindledir"),
                         'spindlespeed' : self.wTree.get_widget("status_spindlespeed"),
                         'loadedtool' : self.wTree.get_widget("status_loadedtool"),
                         'preppedtool' : self.wTree.get_widget("status_preppedtool")}
                prefs = {'actual' : self.wTree.get_widget("dro_actual"),
                         'commanded' : self.wTree.get_widget("dro_commanded"),
                         'inch' : self.wTree.get_widget("dro_inch"),
                         'mm' : self.wTree.get_widget("dro_mm")}
                self.status = emc_interface.emc_status(gtk, emc, status_labels,
                                                       self.wTree.get_widget("error"),
                                                       homes, unhomes,
                                                       estops, machines,
                                                       self.wTree.get_widget("override_limits"),
                                                       stats,
                                                       floods, mists, spindles, prefs)
                gobject.timeout_add(50, self.periodic_status)
                gobject.timeout_add(100, self.periodic_radiobuttons)

                # event bindings
                dic = {
                        "quit" : self.quit,
                        "on_controlfontbutton_font_set" : self.change_control_font,
                        "on_drofontbutton_font_set" : self.change_dro_font,
                        "on_dro_actual_clicked" : self.status.dro_actual,
                        "on_dro_commanded_clicked" : self.status.dro_commanded,
                        "on_dro_inch_clicked" : self.status.dro_inch,
                        "on_dro_mm_clicked" : self.status.dro_mm,
                        "on_errorfontbutton_font_set" : self.change_error_font,
                        "on_listingfontbutton_font_set" : self.change_listing_font,
                        "on_estop_clicked" : self.emc.estop,
                        "on_estop_reset_clicked" : self.emc.estop_reset,
                        "on_machine_off_clicked" : self.emc.machine_off,
                        "on_machine_on_clicked" : self.emc.machine_on,
                        "on_mdi_clear_clicked" : self.mdi_control.clear,
                        "on_mdi_back_clicked" : self.mdi_control.back,
                        "on_mdi_next_clicked" : self.mdi_control.next,
                        "on_mdi_ok_clicked" : self.mdi_ok,
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
                self.control_font = pango.FontDescription(fontbutton.get_font_name())
                self.setfont()

        def change_dro_font(self, fontbutton):
                self.dro_font = pango.FontDescription(fontbutton.get_font_name())
                self.setfont()

        def change_error_font(self, fontbutton):
                self.error_font = pango.FontDescription(fontbutton.get_font_name())
                self.setfont()

        def change_listing_font(self, fontbutton):
                self.listing_font = pango.FontDescription(fontbutton.get_font_name())
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
                          "dro_commanded", "dro_actual", "dro_inch", "dro_mm"]:
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
                for i in ["mdi", "startup", "manual", "auto", "preferences", "status"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.control_font)

                # dro
                for i in ["xa", "ya", "za", "xr", "yr", "zr", "xd", "yd", "zd", "relative", "absolute", "dtg"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.dro_font)

                # status bar
                for i in ["error"]:
                        w = self.wTree.get_widget(i)
                        w.modify_font(self.error_font)

                # XXX save font preferences

        def mdi_ok(self, b):
                if self.wheel == "jogging": self.wheel = "mv"
                self.jogsettings_activate(0)
                self.mdi_control.ok(b)

        def mdi_set_tool(self, b):
                self.mdi_control.set_tool(self.status.get_current_tool())

        def mdi_set_origin(self, b):
                self.mdi_control.set_origin(b)

        def fileselect(self, eb, e):
                if self.wheel == "jogging": self.wheel = "mv"
                self.jogsettings_activate(0)
                self.filechooser.select(eb, e)

        def periodic_status(self):
                self.emc.mask()
                self.status.periodic()
                self.emc.unmask()
                self.hal.periodic()
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
