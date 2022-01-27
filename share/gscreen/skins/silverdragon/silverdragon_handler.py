# This is a handler file for using Gscreen's infrastructure
# to load a completely custom glade screen.
# The only thing that really matters is that it's saved as a GTK builder project,
# the toplevel window is called window1 (The default name) and you connect a destroy
# window signal otherwise you can't close down linuxcnc 

import gi
from gi.repository import Gtk as gtk
from gi.repository import GObject as gobject
from gi.repository import Pango as pango
import hal
import hal_glib
import linuxcnc
from time import strftime

# constants
_RELEASE = " 1.1"
_INCH = 0
_MM = 1
# Mode Notebook Tabs
_MODE_MANUAL = 1
_MODE_MDI = 0
_MODE_AUTO = 1
# Main Notebook Tabs
_NB_PREVIEW = 0
_NB_OFFSET = 1
_NB_TOOL = 2
_NB_ALARMS = 3
_NB_SETUP = 4

NOTIFY_AVAILABLE = False
ALERT_ICON = "/home/jim/linuxcnc/configs/silverdragon/images/applet-critical.png"
INFO_ICON = "/home/jim/linuxcnc/configs/silverdragon/images/std_info.gif"

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]

class HandlerClass:

    # This will be pretty standard to gain access to everything
    # command is for control and status of linuxcnc
    # data is important data from gscreen and linuxcnc
    # widgets is all the widgets from the glade files
    # gscreen is for access to gscreens methods

    def __init__(self, halcomp,builder,useropts,gscreen):
            self.command = linuxcnc.command()
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen
            self.stat = linuxcnc.stat()
            self.halcomp = self.gscreen.halcomp
            self.error_channel = linuxcnc.error_channel()

# global variables
            self.initialized = False
            self.factor = 1.0
            self.slow_jog = 0
            self.fast_jog = 0
            self.slow_jog_factor = 10
            self.metric_units = True
            self.start_system = "G54"
            self.system_list = ("0", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
            self.units = ["", "IN", "MM", "CM"]
            self.must_be_auto = "Must be in AUTO mode"
            self.program_length = 0
            self.program_progress = 0.0
            self.current_line = 1
            self.distance = 0
            self.jog_increments = []
            self.tool_diameter = 0.0
            self.load_tool = False
            self.tool_change = False
            self.use_auto_zref = False
            self.tool_sensor = False
            self.tool_sensor_x = 0.0
            self.tool_sensor_y = 0.0
            self.tool_sensor_z = 0.0
            self.laser_x = 0.0
            self.laser_y = 0.0
            self.home_x = 0.0
            self.home_y = 0.0
            self.home_z = 0.0
            self.axis_to_ref = []
            self.xpos = 0
            self.ypos = 0
            self.width = 0
            self.height = 0
            self.file_to_load = ""
            gobject.timeout_add(1000, self.clock)
            self.default_theme = gtk.settings_get_default().get_property("gtk-theme-name")
            self.label_home_x = self.widgets.btn_home_x.get_children()[0]
            self.label_home_y = self.widgets.btn_home_y.get_children()[0]
            self.label_home_z = self.widgets.btn_home_z.get_children()[0]
            self.init_file_to_load()

    def __getitem__(self, item):
        return getattr(self, item)

    # This connects signals without using glade's autoconnect method
    # in this case to destroy the window
    # it calls the method in gscreen: gscreen.on_window_destroy()
    # Signals in this list are connected to gscreen methods
    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],
                        ["btn_show_hal", "clicked", "on_halshow"],
                        ["btn_hal_meter", "clicked", "on_halmeter"],
                        ["btn_hal_scope", "clicked", "on_halscope"],
                        ["btn_classicladder", "clicked", "on_ladder"],
                        ["btn_status", "clicked", "on_status"],
                        ["btn_calibration", "clicked", "on_calibration"],
                        ["desktop_notify", "toggled", "on_desktop_notify_toggled"],
                        ["theme_choice", "changed", "on_theme_choice_changed"],
                        ["btn_pop_statusbar", "clicked", "on_pop_statusbar_clicked"]]
        for i in signal_list:
            if len(i) == 3:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]])
            elif len(i) == 4:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]],i[3])
        for i in('x','y','z'):
            self.widgets[i+'neg'].connect("pressed", self['jog_'+i],0,True)
            self.widgets[i+'neg'].connect("released", self['jog_'+i],0,False)
            self.widgets[i+'pos'].connect("pressed", self['jog_'+i],1,True)
            self.widgets[i+'pos'].connect("released", self['jog_'+i],1,False)
        self.widgets.jog_speed.connect("value_changed",self.jog_speed_changed)
        self.widgets.full_window.connect("pressed", self.on_fullscreen_pressed)
        self.widgets.max_window.connect("pressed", self.on_max_window_pressed)
        self.widgets.default_window.connect("pressed", self.on_default_window_pressed)

    # We don't want Gscreen to initialize it's regular widgets because this custom
    # screen doesn't have most of them. So we add this function call.
    # Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
    # We don't add much to this function, but we do want the window to display.
    # init_show_window will do this
    def initialize_widgets(self):
        self.gscreen.init_show_windows()
        self.gscreen.init_gremlin()
        self.gscreen.init_audio()
        self.gscreen.init_desktop_notify()
        self.gscreen.init_statusbar()
        self.gscreen.init_tooleditor()
        self.gscreen.init_themes()
        self.init_home()
        self.init_jog_increments()
        self.init_tool_measurement()
        self.init_button_colors()
        self.init_offsetpage()
        self.init_sensitive_on_off()
        self.init_sensitive_run_idle()
        self.init_sensitive_all_homed()
        self.init_unit_labels()
        self.widgets.chk_reload_tool.set_active(self.gscreen.prefs.getpref("reload_tool", True, bool))
        self.widgets.rbt_abs.emit("toggled")
        self.widgets.rbt_rel.set_label(self.start_system)
        opt_blocks = self.gscreen.prefs.getpref("blockdel", False, bool)
        self.widgets.tbtn_optional_blocks.set_active(opt_blocks)
        self.command.set_block_delete(opt_blocks)
        optional_stops = self.gscreen.prefs.getpref( "opstop", False, bool )
        self.widgets.tbtn_optional_stops.set_active( optional_stops )
        self.command.set_optional_stop( optional_stops )
        self.widgets.chk_show_dro.set_active(self.gscreen.prefs.getpref("enable_dro", False, bool))
        self.widgets.show_offsets.set_active(self.gscreen.prefs.getpref("show_offsets", False, bool))
        self.widgets.chk_show_dtg.set_active(self.gscreen.prefs.getpref("show_dtg", False, bool))
        self.widgets.show_offsets.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.chk_show_dtg.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.cmb_mouse_button_mode.set_active(self.gscreen.prefs.getpref("mouse_btn_mode", 4, int))
        self.widgets.tbtn_view_tool_path.set_active(self.gscreen.prefs.getpref("view_tool_path", True, bool))
        self.widgets.tbtn_view_dimension.set_active(self.gscreen.prefs.getpref("view_dimension", True, bool))
        self.widgets.rbt_view_p.emit("toggled")
        self.widgets.tbtn_units.set_label("MM")

        if self.gscreen.prefs.getpref("run_from_line", "no_run", str) == "no_run":
            self.widgets.chk_run_from_line.set_active(False)
            self.widgets.btn_from_line.set_sensitive(False)
        else:
            self.widgets.chk_run_from_line.set_active(True)
            self.widgets.btn_from_line.set_sensitive(True)
        if self.gscreen.prefs.getpref("show_preview_on_offset", False, bool):
            self.widgets.rbtn_show_preview.set_active(True)
        else:
            self.widgets.rbtn_show_offsets.set_active(True)
        self.widgets.chk_use_kb_shortcuts.set_active(self.gscreen.prefs.getpref("use_keyboard_shortcuts", False, bool))
        self.widgets.adj_start_spindle_RPM.set_value(self.spindle_start_rpm)
        self.widgets.gcode_view.set_sensitive(False)
        self.widgets.ntb_main.set_current_page(_NB_PREVIEW)
        self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def initialize_preferences(self):
        self.gscreen.init_general_pref()
        self.gscreen.init_theme_pref()
        self.gscreen.init_window_geometry_pref()
        self.no_force_homing = self.gscreen.inifile.find("TRAJ", "NO_FORCE_HOMING")
        if self.no_force_homing:
            self.widgets.chk_reload_tool.set_sensitive(False)
            self.widgets.chk_reload_tool.set_active(False)
            self.widgets.lbl_reload_tool.set_visible(True)
        default_spindle_speed = self.gscreen.inifile.find("DISPLAY", "DEFAULT_SPINDLE_SPEED")
        self.spindle_start_rpm = self.gscreen.prefs.getpref( 'spindle_start_rpm', default_spindle_speed, float )
        # get the values for the sliders
        default_jog_vel = float(self.gscreen.inifile.find("TRAJ", "DEFAULT_LINEAR_VELOCITY"))
        self.fast_jog = default_jog_vel
        self.slow_jog = default_jog_vel / self.slow_jog_factor
        self.jog_rate_max = self.gscreen.inifile.find("TRAJ", "MAX_LINEAR_VELOCITY")
        self.data.spindle_override_max = self.gscreen.inifile.find("DISPLAY", "MAX_SPINDLE_OVERRIDE")
        self.data.spindle_override_min = self.gscreen.inifile.find("DISPLAY", "MIN_SPINDLE_OVERRIDE")
        self.data.feed_override_max = self.gscreen.inifile.find("DISPLAY", "MAX_FEED_OVERRIDE")
        self.data.rapid_override_max = self.gscreen.inifile.find("DISPLAY", "MAX_RAPID_OVERRIDE")
        self.data.dro_actual = self.gscreen.inifile.find("DISPLAY", "POSITION_FEEDBACK")
        # set the slider limits
        self.widgets.jog_speed.set_range(100, float(self.jog_rate_max) * 60)
        self.widgets.jog_speed.set_value(default_jog_vel * 60)
        self.widgets.jog_speed.set_digits(0)
        self.widgets.spc_spindle.set_range(float(self.data.spindle_override_min) * 100, float(self.data.spindle_override_max) * 100)
        self.widgets.spc_spindle.set_value(100)
        self.widgets.spc_rapid.set_range(1, float(self.data.rapid_override_max) * 100)
        self.widgets.spc_rapid.set_value(100)
        self.widgets.spc_feed.set_range(1, float(self.data.feed_override_max) * 100)
        self.widgets.spc_feed.set_value(100)
        self.max_velocity = self.stat.max_velocity

    def initialize_keybindings(self):
        self.widgets.window1.connect("key_press_event", self.gscreen.on_key_event, 1)
        self.widgets.window1.connect("key_release_event", self.gscreen.on_key_event, 0)

    def init_home(self):
        self.home_x = self.gscreen.inifile.find("JOINT_0", "HOME")
        self.home_y = self.gscreen.inifile.find("JOINT_1", "HOME")
        self.home_z = self.gscreen.inifile.find("JOINT_2", "HOME")

    def init_tool_measurement(self):
        # set up auto zref
        xpos = self.gscreen.inifile.find("TOOLSENSOR", "X")
        ypos = self.gscreen.inifile.find("TOOLSENSOR", "Y")
        zpos = self.gscreen.inifile.find("TOOLSENSOR", "Z")
        sensor_height = self.gscreen.inifile.find("TOOLSENSOR", "SENSOR_HEIGHT")
        maxprobe = self.gscreen.inifile.find("TOOLSENSOR", "MAXPROBE")
        search_vel = self.gscreen.inifile.find("TOOLSENSOR", "SEARCH_VEL")
        probe_vel = self.gscreen.inifile.find("TOOLSENSOR", "PROBE_VEL")
        self.halcomp["probe_vel"] = probe_vel
        self.halcomp["search_vel"] = search_vel
        self.halcomp["sensor_height"] = sensor_height
        self.halcomp["maxprobe"] = maxprobe
        if not xpos or not ypos or not zpos or not maxprobe:
            self.widgets.chk_use_auto_zref.set_active(False)
            self.widgets.chk_use_auto_zref.set_sensitive(False)
            self.widgets.btn_g30.set_sensitive(False)
            self.widgets.btn_blockheight.set_sensitive(False)
            self.widgets.lbl_blockheight.hide()
            self.widgets.chk_use_auto_zref.set_sensitive(False)
            print(_("No valid tool sensor location"))
            print(_("Block Height - Disabled"))
        else:
            self.widgets.lbl_tool_measurement.hide()
            self.widgets.lbl_xpos.set_label(str(xpos))
            self.widgets.lbl_ypos.set_label(str(ypos))
            self.widgets.lbl_zpos.set_label(str(zpos))
            self.widgets.lbl_max_probe.set_label(str(maxprobe))
            self.widgets.lbl_sensor_height.set_label(str(sensor_height))
            self.widgets.lbl_search_vel.set_label(str(search_vel))
            self.widgets.lbl_probe_vel.set_label(str(probe_vel))
            self.use_auto_zref = self.gscreen.prefs.getpref("use_autozref", False, bool)
            self.tool_sensor = True
            self.tool_sensor_x = xpos
            self.tool_sensor_y = ypos
            self.tool_sensor_z = zpos
            print(_("Block Height - Enabled"))
        self.widgets.chk_use_auto_zref.emit("toggled")
        # set up laser crosshair offsets
        xpos = self.gscreen.inifile.find("LASER", "X")
        ypos = self.gscreen.inifile.find("LASER", "Y")
        if not xpos or not ypos:
            self.widgets.btn_laser_zero.set_sensitive(False)
            self.widgets.tbtn_laser.set_sensitive(False)
        else:
            self.laser_x = xpos
            self.laser_y = ypos
            
    def init_button_colors(self):
        # set the button background colors and digits of the DRO
        self.homed_textcolor = self.gscreen.prefs.getpref("homed_textcolor", "#00FF00", str)     # default green
        self.unhomed_textcolor = self.gscreen.prefs.getpref("unhomed_textcolor", "#FF0000", str) # default red
        self.widgets.homed_colorbtn.set_color(gdk.color_parse(self.homed_textcolor))
        self.widgets.unhomed_colorbtn.set_color(gdk.color_parse(self.unhomed_textcolor))
        self.homed_color = self.gscreen.convert_to_rgb(self.widgets.homed_colorbtn.get_color())
        self.unhomed_color = self.gscreen.convert_to_rgb(self.widgets.unhomed_colorbtn.get_color())
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_NORMAL, gdk.color_parse("#00FF00"))
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_ACTIVE, gdk.color_parse("#FF0000"))
        self.widgets.tbtn_on.modify_bg(gtk.STATE_NORMAL, gdk.color_parse("#FF0000"))
        self.widgets.tbtn_on.modify_bg(gtk.STATE_ACTIVE, gdk.color_parse("#00FF00"))
        self.label_home_x.modify_fg(gtk.STATE_NORMAL, gdk.color_parse("#FF0000"))
        self.label_home_y.modify_fg(gtk.STATE_NORMAL, gdk.color_parse("#FF0000"))
        self.label_home_z.modify_fg(gtk.STATE_NORMAL, gdk.color_parse("#FF0000"))
        # set the active colours of togglebuttons and radiobuttons
        blue_list = ["tbtn_mist", "tbtn_flood", "tbtn_laser", "tbtn_spare",
                     "tbtn_units", "tbtn_pause",
                     "tbtn_optional_stops", "tbtn_optional_blocks",
                     "rbt_abs", "rbt_rel", "rbt_dtg",
                     "rbt_reverse", "rbt_stop", "rbt_forward"]
        green_list = ["rbt_manual", "rbt_mdi", "rbt_auto"]
        other_list = ["rbt_view_p", "rbt_view_x", "rbt_view_y", "rbt_view_z",
                      "tbtn_view_dimension", "tbtn_view_tool_path"]
        for btn in blue_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gdk.color_parse("#44A2CF"))
        for btn in green_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gdk.color_parse("#A2E592"))
        for btn in other_list:
            self.widgets["{0}".format(btn)].modify_bg(gtk.STATE_ACTIVE, gdk.color_parse("#BB81B5"))

    def init_sensitive_on_off(self):
        self.data.sensitive_on_off = ["table_run", "tbl_dro", "vbox_overrides",
                                      "vbox_tool", "hbox_spindle_ctl"]
        
    def init_sensitive_run_idle(self):
        self.data.sensitive_run_idle = ["rbt_manual", "rbt_mdi", "rbt_auto",
                                        "tbtn_optional_blocks", "tbtn_optional_stops",
                                        "btn_run", "rbt_reverse", "rbt_stop", "rbt_forward"]

    def init_sensitive_all_homed(self):
        self.data.sensitive_all_homed = ["btn_zero_x", "btn_zero_y", "btn_zero_z",
                                         "btn_ref_x", "btn_ref_y", "btn_ref_z"]

    def initialize_pins(self):
        # pins for block height
        self.halcomp.newpin("sensor_height", hal.HAL_FLOAT, hal.HAL_OUT)
        pin = self.halcomp.newpin("block_height", hal.HAL_FLOAT, hal.HAL_OUT)
        hal_glib.GPin(pin).connect("value_changed", self.on_blockheight_changed)
        self.halcomp["block_height"] = self.gscreen.prefs.getpref("blockheight", 0.0, float)
        # pins for probing
        self.halcomp.newpin("search_vel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.halcomp.newpin("probe_vel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.halcomp.newpin("maxprobe", hal.HAL_FLOAT, hal.HAL_OUT)
        # pins for clear_mdi button
        self.halcomp.newpin("clear_mdi", hal.HAL_BIT, hal.HAL_OUT)
        pin = self.halcomp.newpin("clear_mdi-waiting", hal.HAL_BIT, hal.HAL_OUT)
        pin = self.halcomp.newpin("clear_mdi-response", hal.HAL_BIT, hal.HAL_OUT)
        hal_glib.GPin(pin).connect("value_changed", self.on_clear_mdi_changed)
        
    # every 100 milli seconds this gets called
    # add pass so gscreen doesn't try to update it's regular widgets or
    # add the individual function names that you would like to call.
    def periodic(self):
        # put the poll command in a try, so if the linuxcnc pid is killed
        # from an external command, also quit the GUI
        try:
            self.stat.poll()
        except:
            raise SystemExit, "SilverDragon cannot poll linuxcnc status any more"
        error = self.error_channel.poll()
        if error:
            self.gscreen.notify(_("ERROR"), _(error), ALERT_ICON)
        if self.data.active_gcodes != self.stat.gcodes:
            self.gscreen.update_active_gcodes()
        if self.data.active_mcodes != self.stat.mcodes:
            self.gscreen.update_active_mcodes()
        if self.stat.task_mode == linuxcnc.MODE_MANUAL \
            and self.stat.interp_state == linuxcnc.INTERP_IDLE \
            and self.widgets.ntb_main.get_current_page() == _NB_PREVIEW:
            self.widgets.hal_led_jog.set_active(True)
        else:
            self.widgets.hal_led_jog.set_active(False)
        self._update_vel()
        self._update_coolant()
        self._update_spindle()
        self._update_vc()
        return True

    def clock(self):
        self.update_progress()
        self.update_dro()
        self.widgets.rbt_rel.set_label(self.system_list[self.stat.g5x_index])
        self.widgets.entry_clock.set_text(strftime("%H:%M:%S"))
        return True
    
# ========================
# Start of widget handlers
# ========================
    def on_window1_show(self, widget, data=None):
        self.stat.poll()
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.widgets.tbtn_estop.set_active(True)
            self.widgets.tbtn_on.set_sensitive(False)
        else:
            self.widgets.tbtn_estop.set_active(False)
            self.widgets.tbtn_on.set_sensitive(True)

        if self.file_to_load != "":
            self.widgets.hal_action_open.load_file(self.file_to_load)
        start_as = self.gscreen.prefs.getpref("window_geometry", "default", str)
        if start_as == "fullscreen":
            self.widgets.window1.fullscreen()
        elif start_as == "max":
            self.widgets.window1.maximize()
        else:
            self.xpos = int(self.gscreen.prefs.getpref("x_pos", 10, float))
            self.ypos = int(self.gscreen.prefs.getpref("y_pos", 10, float))
            self.width = int(self.gscreen.prefs.getpref("width", 1440, float))
            self.height = int(self.gscreen.prefs.getpref("height", 900, float))
            # set the adjustments according to Window position and size
            self.widgets.adj_x_pos.set_value(self.xpos)
            self.widgets.adj_y_pos.set_value(self.ypos)
            self.widgets.adj_width.set_value(self.width)
            self.widgets.adj_height.set_value(self.height)
            # move and resize the window
            self.widgets.window1.move(self.xpos, self.ypos)
            self.widgets.window1.resize(self.width, self.height)
        self.initialized = True

    # estop machine before closing
    def on_window1_destroy(self, widget, data=None):
        print "estopping / killing silverdragon"
        self.command.state(linuxcnc.STATE_OFF)
        self.command.state(linuxcnc.STATE_ESTOP)
        gtk.main_quit()

    def on_btn_exit_clicked(self, widget, data=None):
        self.widgets.tbtn_estop.emit("toggled")
        self.widgets.window1.destroy()
        return

    def on_rbt_manual_clicked(self, widget):
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            return
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_rbt_mdi_clicked(self, widget):
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            self.widgets.rbt_manual.set_active(True)
            self.widgets.rbt_mdi.set_active(False)
        else:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()

    def on_rbt_auto_clicked(self, widget):
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            self.widgets.rbt_manual.set_active(True)
            self.widgets.rbt_auto.set_active(False)
        else:
            self.command.mode(linuxcnc.MODE_AUTO)
            self.command.wait_complete()

    def on_ntb_main_switch_page(self, widget, tab, index):
        if self.stat.task_mode == linuxcnc.MODE_AUTO:
            return
        if index == _NB_OFFSET:
            names = self.widgets.offsetpage1.get_names()
            for system, name in names:
                system_name = "system_name_{0}".format(system)
                self.gscreen.prefs.putpref(system_name, name, str)
            self.widgets.offsetpage1.mark_active((self.system_list[self.stat.g5x_index]).lower())
        elif index == _NB_TOOL:
            self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)
        elif index == _NB_ALARMS:
            return
        elif index == _NB_SETUP:
            self.widgets.rbt_manual.emit("clicked")
        else:
            return

    def on_rbt_forward_clicked(self, widget, data=None):
        self._set_spindle("forward")

    def on_rbt_reverse_clicked(self, widget, data=None):
        self._set_spindle("reverse")

    def on_rbt_stop_clicked(self, widget, data=None):
        self._set_spindle("stop")

    def on_tbtn_units_toggled(self, widget, data=None):
        if widget.get_active():
            self.metric_units = False
            widget.set_label("IN")
        else:
            self.metric_units = True
            widget.set_label("MM")
        for axis in self.data.axis_list:
            self.widgets["hal_dro_{0}".format(axis)].set_property("display_units_mm", self.metric_units)
        self.gscreen.set_dro_units(not widget.get_active(), 1)

    def on_rbt_abs_toggled(self, widget, data=None):
        for axis in self.data.axis_list:
            self.widgets["hal_dro_{0}".format(axis)].set_property("reference_type", 0)

    def on_rbt_rel_toggled(self, widget, data=None):
        for axis in self.data.axis_list:
            self.widgets["hal_dro_{0}".format(axis)].set_property("reference_type", 1)

    def on_rbt_dtg_toggled(self, widget, data=None):
        for axis in self.data.axis_list:
            self.widgets["hal_dro_{0}".format(axis)].set_property("reference_type", 2)

    def on_zero_axis_clicked(self, widget):
        if not self.data.all_homed:
            self.gscreen.notify(_("INFO"), _("Must be all homed to perform this operation"), INFO_ICON)
            return
        if self.stat.interp_state != linuxcnc.INTERP_IDLE:
            self.gscreen.notify(_("INFO"), _("Interpreter is not IDLE"), INFO_ICON)
            return
        if widget == self.widgets.btn_zero_x:
            self.axis_to_ref = "x"
        elif widget == self.widgets.btn_zero_y:
            self.axis_to_ref = "y"
        elif widget == self.widgets.btn_zero_z:
            self.axis_to_ref = "z"
        else:
            self.gscreen.notify(_("ERROR"), _("Unknown axis selected"), ALERT_ICON)
            return
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        command = "G10 L20 P0 {0}0".format(self.axis_to_ref)
        self.command.mdi(command)
        self.command.wait_complete()
        self.gscreen.reload_plot()
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        self.gscreen.prefs.putpref("offset_axis_{0}".format(self.axis_to_ref), 0, float)

    def on_ref_axis_clicked(self, widget):
        if not self.data.all_homed:
            self.gscreen.notify(_("INFO"), _("Must be all homed to perform this operation"), INFO_ICON)
            return
        if self.stat.interp_state != linuxcnc.INTERP_IDLE:
            self.gscreen.notify(_("INFO"), _("Interpreter is not IDLE"), INFO_ICON)
            return
        if widget == self.widgets.btn_ref_x:
            self.axis_to_ref = "x"
        elif widget == self.widgets.btn_ref_y:
            self.axis_to_ref = "y"
        elif widget == self.widgets.btn_ref_z:
            self.axis_to_ref = "z"
        else:
            self.gscreen.notify(_("ERROR"), _("Unknown axis selected"), ALERT_ICON)
            return
        callback = "on_offset_axis_return"
        title = "Enter Offset for Axis %s"%self.axis_to_ref
        self.gscreen.launch_numerical_input(callback, 0, 0, title)
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()

    def on_tbtn_flood_toggled(self, widget, data=None):
        if self.stat.flood == widget.get_active():
            return
        if widget.get_active():
            self.command.flood(linuxcnc.FLOOD_ON)
        else:
            self.command.flood(linuxcnc.FLOOD_OFF)

    def on_tbtn_mist_toggled(self, widget, data=None):
        if self.stat.mist == widget.get_active():
            return
        if widget.get_active():
            self.command.mist(linuxcnc.MIST_ON)
        else:
            self.command.mist(linuxcnc.MIST_OFF)

    def on_tbtn_optional_blocks_toggled(self, widget, data=None):
        opt_blocks = widget.get_active()
        self.command.set_block_delete(opt_blocks)
        self.gscreen.prefs.putpref("blockdel", opt_blocks)

    def on_tbtn_optional_stops_toggled(self, widget, data=None):
        opt_stops = widget.get_active()
        self.command.set_optional_stop(opt_stops)
        self.gscreen.prefs.putpref("opstop", opt_stops)

    def on_tbtn_pause_toggled(self, widget, data=None):
        widgetlist = ["rbt_forward", "rbt_reverse", "rbt_stop"]
        paused = widget.get_active()
        self.gscreen.sensitize_widgets(widgetlist, paused)

    def on_btn_stop_clicked(self, widget, data=None):
        self.command.abort()
        self.set_restart_line(1)
        self.widgets.gcode_view.set_line_number(1)

    def on_btn_run_clicked(self, widget, data=None):
        if self.stat.task_mode != linuxcnc.MODE_AUTO:
            self.gscreen.notify(_("INFO"), self.must_be_auto, INFO_ICON)
            return
        self.widgets.ntb_main.set_current_page(_NB_PREVIEW)
        self.program_progress = 0.0
        self.gscreen.notify(_("INFO"), _("Cycle start pressed in AUTO mode"), INFO_ICON)
        self.widgets.hal_toggleaction_run.emit('activate')

    def on_btn_from_line_clicked(self, widget, data=None):
        if self.stat.task_mode != linuxcnc.MODE_AUTO:
            self.gscreen.notify(_("INFO"), self.must_be_auto, INFO_ICON)
            return
        self.gscreen.launch_restart_dialog(self)

    def on_spc_spindle_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        # this is in a try except, because on initializing the window,
        # the values are still zero, so we would get a divide by zero error
        value = widget.get_value()
        try:
            if not abs(self.stat.settings[2]):
                if self.widgets.rbt_forward.get_active() or self.widgets.rbt_reverse.get_active():
                    speed = self.stat.spindle[0]['speed']
                else:
                    speed = 0
            else:
                speed = abs(self.stat.spindle[0]['speed'])
            spindle_override = value / 100
            self.command.spindleoverride(spindle_override)
        except:
            pass

    def on_spc_feed_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = widget.get_value() / 100
        self.data.feed_override = value
        self.command.feedrate(value)

    def on_spc_rapid_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = widget.get_value() / 100
        self.data.rapid_override = value
        self.command.rapidrate(value)

    def on_btn_home_clicked(self, widget, data=None):
        command = "G90 G53 G0 Z{}".format(self.home_z)
        self.command.mdi(command)
        self.command.wait_complete()
        command = "G53 G0 X{} Y{}".format(self.home_x, self.home_y)
        self.command.mdi(command)
        self.command.wait_complete()
            
    def on_btn_laser_zero_clicked(self, widget, data=None):
        if self.stat.task_mode != linuxcnc.MODE_MDI:
            self.gscreen.notify(_("INFO"), _("Must be in MDI mode"), INFO_ICON)
            return
        command = "G10 L20 P0 X{} Y{}".format(self.laser_x, self.laser_y)
        self.command.mdi(command)
        self.command.wait_complete()
        self.gscreen.reload_plot()

    def on_btn_M6_clicked(self, widget, data=None):
        tool = str(self.widgets.enter_tool.get_text())
        if not tool.isdigit():
            self.gscreen.notify(_("ERROR"), _("Invalid tool specified"), ALERT_ICON)
            self.widgets.enter_tool.set_text(str(self.stat.tool_in_spindle))
        else:
            command = "T{} M6".format(tool)
            self.command.mdi(command)
            self.command.wait_complete()

    def on_btn_g30_clicked(self, widget, data=None):
        if not self.data.all_homed:
            self.gscreen.notify(_("ERROR"), _("Machine is not homed"), ALERT_ICON)
            return
        self.command.mdi("G53 G0 Z0")
        self.command.wait_complete()
        command = "G53 G0 X{} Y{}".format(self.tool_sensor_x, self.tool_sensor_y)
        self.command.mdi(command)
        self.command.wait_complete()
        
    def on_btn_blockheight_clicked(self, widget, data=None):
        title = "Enter Block Height"
        callback = "on_blockheight_return"
        self.gscreen.launch_numerical_input(callback, 0, 0, title)

    def on_btn_touchoff_clicked(self, widget, data=None):
        if self.stat.tool_in_spindle == 0:
            self.gscreen.notify(_("INFO"), _("Cannot probe with no tool loaded"), INFO_ICON)
            return
        self.widgets.gremlin.clear_live_plotter()
        if self.use_auto_zref is True:
            command = "o< tool_sensor > call"
        else:
            command = "o< touch_plate > call"
        self.command.mdi(command)

    def on_btn_rapid_100_clicked(self, widget, data=None):
        self.widgets.spc_rapid.set_value(100)

    def on_btn_feed_100_clicked(self, widget, data=None):
        self.widgets.spc_feed.set_value(100)

    def on_btn_spindle_100_clicked(self, widget, data=None):
        self.widgets.spc_spindle.set_value(100)

    def jog_x(self,widget,direction,state):
        self.gscreen.do_key_jog(0,direction,state)
        
    def jog_y(self,widget,direction,state):
        self.gscreen.do_key_jog(1,direction,state)
        
    def jog_z(self,widget,direction,state):
        self.gscreen.do_key_jog(2,direction,state)
        
    def jog_speed_changed(self,widget):
        value = widget.get_value()
        self.gscreen.set_jog_rate(absolute = value)
        
    def on_cmb_increments_changed(self, widget, data=None):
        index = widget.get_active()
        if index == 0:
            self.data.current_jogincr_index = len(self.data.jog_increments) - 1
        else:
            self.data.current_jogincr_index = index - 1

    def on_rbt_view_p_toggled(self, widget, data=None):
        if self.widgets.rbt_view_p.get_active():
            self.widgets.gremlin.set_property("view", "p")

    def on_rbt_view_x_toggled(self, widget, data=None):
        if self.widgets.rbt_view_x.get_active():
            self.widgets.gremlin.set_property("view", "x")

    def on_rbt_view_y_toggled(self, widget, data=None):
        if self.widgets.rbt_view_y.get_active():
            self.widgets.gremlin.set_property("view", "y")

    def on_rbt_view_z_toggled(self, widget, data=None):
        if self.widgets.rbt_view_z.get_active():
            self.widgets.gremlin.set_property("view", "z")

    def on_btn_zoom_in_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_in()

    def on_btn_zoom_out_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_out()

    def on_btn_delete_view_clicked(self, widget, data=None):
        self.widgets.gremlin.clear_live_plotter()

    def on_tbtn_view_dimension_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_extents_option", widget.get_active())
        self.gscreen.prefs.putpref("view_dimension", self.widgets.tbtn_view_dimension.get_active())

    def on_tbtn_view_tool_path_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_live_plot", widget.get_active())
        self.gscreen.prefs.putpref("view_tool_path", self.widgets.tbtn_view_tool_path.get_active())

    def on_tbtn_jog_speed_toggled(self, widget, data=None):
        # due to imperial and metric options we have to get first the values of the widget
        max = self.widgets.adj_scale_jog_vel.get_upper()
        min = self.widgets.adj_scale_jog_vel.get_lower()
        value = self.widgets.jog_speed.get_value()
        if widget.get_active():
            self.fast_jog = value
            widget.set_label("SLOW")
            self.widgets.jog_speed.set_range(min / self.slow_jog_factor, max / self.slow_jog_factor)
            self.widgets.jog_speed.set_value(self.fast_jog / self.slow_jog_factor)
        else:
            self.slow_jog = value
            widget.set_label("FAST")
            self.widgets.jog_speed.set_range(min * self.slow_jog_factor, max * self.slow_jog_factor)
            self.widgets.jog_speed.set_value(self.fast_jog)

    def on_rbtn_show_preview_toggled(self, widget, data=None):
        self.gscreen.prefs.putpref("show_preview_on_offset", widget.get_active())

    def on_fullscreen_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "fullscreen", str)
        self.widgets.window1.fullscreen()

    def on_max_window_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "max", str)
        self.widgets.window1.unfullscreen()
        self.widgets.window1.maximize()

    def on_default_window_pressed(self, widget):
        self.gscreen.prefs.putpref("window_geometry", "default", str)
        self.widgets.window1.unfullscreen()
        self.widgets.window1.unmaximize()
        self.widgets.spbtn_x_pos.set_sensitive(True)
        self.widgets.spbtn_y_pos.set_sensitive(True)
        self.widgets.spbtn_width.set_sensitive(True)
        self.widgets.spbtn_height.set_sensitive(True)
        self.widgets.window1.move(self.xpos, self.ypos)
        self.widgets.window1.resize(self.width, self.height)

    def on_btn_zero_g92_clicked(self, widget, data=None):
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        self.command.mdi("G92.1")
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()
        self.widgets.rbt_offset.emit("toggled")

    def on_tbtn_edit_offsets_toggled(self, widget, data=None):
        state = widget.get_active()
        self.widgets.offsetpage.edit_button.set_active(state)
        widgetlist = ["btn_set_selected", "btn_zero_g92"]
        self.gscreen.sensitize_widgets(widgetlist, not state)
        if not state:  # must switch back to manual mode, otherwise jogging is not possible
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    def on_chk_ignore_limits_toggled(self, widget, data=None):
        if self.widgets.chk_ignore_limits.get_active():
            if not self._check_limits():
                self.gscreen.notify(_("ERROR"), _("No limit switch is active, ignore limits will not be set."), ALERT_ICON)
                return
            self.command.override_limits()

    def on_chk_use_auto_zref_toggled(self, widget, data=None):
        state = widget.get_active()
        self.gscreen.prefs.putpref("use_autozref", state, bool)
        self.widgets.btn_blockheight.set_sensitive(state)
        self.widgets.frm_probe_pos.set_sensitive(state)
        self.use_auto_zref = state
        if state is True:
            self.widgets.lbl_blockheight.show()
        else:
            self.widgets.lbl_blockheight.hide()

    def on_chk_reload_tool_toggled(self, widget, data=None):
        state = widget.get_active()
        self.reload_tool_enabled = state
        self.gscreen.prefs.putpref("reload_tool", state)

    def on_chk_run_from_line_toggled(self, widget, data=None):
        if widget.get_active():
            self.gscreen.prefs.putpref("run_from_line", "run")
            self.widgets.btn_from_line.set_sensitive(True)
        else:
            self.gscreen.prefs.putpref("run_from_line", "no_run")
            self.widgets.btn_from_line.set_sensitive(False)

    def on_chk_use_kb_shortcuts_toggled(self, widget, data=None):
        self.gscreen.prefs.putpref("use_keyboard_shortcuts", widget.get_active())

    def on_chk_show_dro_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("enable_dro", widget.get_active())
        self.gscreen.prefs.putpref("enable_dro", widget.get_active())
        self.widgets.show_offsets.set_sensitive(widget.get_active())
        self.widgets.chk_show_dtg.set_sensitive(widget.get_active())

    def on_chk_show_dtg_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_dtg", widget.get_active())
        self.gscreen.prefs.putpref("show_dtg", widget.get_active())

    def on_show_offsets_toggled(self, widget, data=None):
        self.widgets.gremlin.show_offsets = widget.get_active()
        self.gscreen.prefs.putpref("show_offsets", widget.get_active())

    def on_btn_delete_clicked(self, widget, data=None):
        message = _("Do you really want to delete the MDI history?\n")
        message += _("This will not delete the MDI History file, but will\n")
        message += _("delete the listbox entries for this session")
        self.halcomp["clear_mdi-waiting"] = True
        self.gscreen.warning_dialog(message, False, None, "clear_mdi")

    def on_change_sound(self, widget, sound=None):
        file = widget.get_filename()
        if file:
            if widget == self.widgets.audio_error_chooser:
                self.error_sound = file
                self.gscreen.prefs.putpref("audio_error", file)
            else:
                self.alert_sound = file
                self.gscreen.prefs.putpref("audio_alert", file)

    def on_homed_colorbtn_color_set(self, widget):
        self.homed_color = self.gscreen.convert_to_rgb(widget.get_color())
        self.gscreen.prefs.putpref('homed_textcolor', widget.get_color(), str)

    def on_unhomed_colorbtn_color_set(self, widget):
        self.unhomed_color = self.gscreen.convert_to_rgb(widget.get_color())
        self.gscreen.prefs.putpref('unhomed_textcolor', widget.get_color(), str)

    def _from_internal_linear_unit(self, v, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4
        return v * lu

    def on_adj_x_pos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("x_pos", value, float)
        self.xpos = value
        self.widgets.window1.move(value, self.ypos)

    def on_adj_y_pos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("y_pos", value, float)
        self.ypos = value
        self.widgets.window1.move(self.xpos, value)

    def on_adj_width_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("width", value, float)
        self.width = value
        self.widgets.window1.resize(value, self.height)

    def on_adj_start_spindle_RPM_value_changed(self, widget, data=None):
        self.spindle_start_rpm = widget.get_value()
        self.gscreen.prefs.putpref("spindle_start_rpm", self.spindle_start_rpm, float)

    def on_adj_scale_jog_vel_value_changed(self, widget, data=None):
        self.gscreen.prefs.putpref("scale_jog_vel", widget.get_value(), float)
        self.scale_jog_vel = widget.get_value()

    def on_adj_scale_feed_override_value_changed(self, widget, data=None):
        self.gscreen.prefs.putpref("scale_feed_override", widget.get_value(), float)
        self.scale_feed_override = widget.get_value()

    def on_adj_scale_rapid_override_value_changed(self, widget, data=None):
        self.gscreen.prefs.putpref("scale_rapid_override", widget.get_value(), float)
        self.scale_rapid_override = widget.get_value()

    def on_adj_scale_spindle_override_value_changed(self, widget, data=None):
        self.gscreen.prefs.putpref("scale_spindle_override", widget.get_value(), float)
        self.scale_spindle_override = widget.get_value()

    def on_adj_height_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.gscreen.prefs.putpref("height", value, float)
        self.height = value
        self.widgets.window1.resize(self.width, value)

# ==========
# HAL status
# ==========
    def on_hal_status_all_homed(self, widget):
        self.data.all_homed = True
        self.widgets.ntb_main.set_current_page(_NB_PREVIEW)
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()
        self.gscreen.sensitize_widgets(self.data.sensitive_all_homed, True)
        self.set_motion_mode(1)
        self.widgets.statusbar1.remove_message(self.gscreen.statusbar_id, self.gscreen.homed_status_message)
        self.gscreen.notify(_("INFO"), _("All axes have been homed"), INFO_ICON)
        if self.widgets.chk_reload_tool.get_active():
            if self.stat.tool_in_spindle == 0:
                self.reload_tool()
            self.command.mode(linuxcnc.MODE_MANUAL)

    def on_hal_status_not_all_homed(self, widget, joints):
        self.data.all_homed = False
        if self.no_force_homing:
            return
        self.gscreen.sensitize_widgets(self.data.sensitive_all_homed, False)
        self.set_motion_mode(0)

    def on_hal_status_file_loaded(self, widget, filename):
        if filename:
            fileobject = file(filename, 'r')
            lines = fileobject.readlines()
            fileobject.close()
            self.program_length = len(lines)
            if len(filename) > 70:
                filename = filename[0:10] + "..." + filename[len(filename) - 50:len(filename)]
            self.widgets.gcode_tab.set_text(filename)
            self.gscreen.notify(_("INFO"), _("Program loaded: %s"%filename), INFO_ICON)
        else:
            self.program_length = 0
            self.widgets.gcode_tab.set_text("No program loaded")

    def on_hal_status_line_changed(self, widget, line):
        self.current_line = line

    def on_hal_status_interp_idle(self, widget):
        print("IDLE")
        if self.load_tool:
            return
        if self.stat.task_state == linuxcnc.STATE_ESTOP or self.stat.task_state == linuxcnc.STATE_OFF:
            self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, False)
        else:
            self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, True)
        if self.tool_change:
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.tool_change = False
        self.current_line = 0
        
    def on_hal_status_interp_run(self, widget):
        print("RUN")
        self.gscreen.sensitize_widgets(self.data.sensitive_run_idle, False)

    def on_hal_status_tool_in_spindle_changed(self, object, new_tool_no):
        self.gscreen.prefs.putpref("tool_in_spindle", new_tool_no, int)
        self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)
        self.update_toolinfo(new_tool_no)

    def on_hal_status_state_estop(self, widget=None):
        self.widgets.tbtn_estop.set_active(True)
        self.widgets.tbtn_on.set_sensitive(False)
        self.widgets.tbtn_on.set_active(False)
        self.widgets.tbtn_estop.set_label("ESTOP")
        self.widgets.chk_ignore_limits.set_sensitive(False)
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_hal_status_state_estop_reset(self, widget=None):
        self.widgets.tbtn_estop.set_active(False)
        self.widgets.tbtn_on.set_sensitive(True)
        self.widgets.tbtn_estop.set_label("RESET")
        self.widgets.chk_ignore_limits.set_sensitive(True)
        self._check_limits()

    def on_hal_status_state_off(self, widget):
        if self.widgets.tbtn_on.get_active():
            self.widgets.tbtn_on.set_active(False)
        self.widgets.chk_ignore_limits.set_sensitive(True)
        self.widgets.tbtn_on.set_label("OFF")
        self.gscreen.sensitize_widgets(self.data.sensitive_on_off, False)
        
    def on_hal_status_state_on(self, widget):
        if not self.widgets.tbtn_on.get_active():
            self.widgets.tbtn_on.set_active(True)
        self.widgets.chk_ignore_limits.set_sensitive(False)
        self.widgets.tbtn_on.set_label("ON")
        self.gscreen.sensitize_widgets(self.data.sensitive_on_off, True)
        self.widgets.rbt_manual.set_active(True)
        self.widgets.ntb_mode.set_current_page(_MODE_MANUAL)
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_hal_status_mode_manual(self, widget):
        print("MANUAL Mode")
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            return
        self.widgets.rbt_manual.set_active(True)
        self.widgets.ntb_main.set_current_page(_NB_PREVIEW)
        self.widgets.ntb_mode.set_current_page(_MODE_MANUAL)
        self._check_limits()
        self.last_key_event = None, 0

    def on_hal_status_mode_mdi(self, widget):
        if self.tool_change:
            self.widgets.ntb_mode.set_current_page(_MODE_MDI)
            return
        if not self.widgets.rbt_mdi.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.gscreen.notify(_("INFO"), _("It is not possible to change to MDI Mode at the moment"), INFO_ICON)
            return
        else:
            print("MDI Mode")
            self.widgets.hal_mdihistory.entry.grab_focus()
            self.widgets.ntb_mode.set_current_page(_MODE_MDI)
            self.widgets.rbt_mdi.set_active(True)
            self.last_key_event = None, 0

    def on_hal_status_mode_auto(self, widget):
        if not self.widgets.rbt_auto.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.gscreen.notify(_("INFO"), _("It is not possible to change to AUTO Mode at the moment"), INFO_ICON)
        else:
            print("AUTO Mode")
            self.widgets.ntb_mode.set_current_page(_MODE_AUTO)
            self.widgets.rbt_auto.set_active(True)
            self.last_key_event = None, 0

    def on_hal_dro_system_changed(self, widget, system):
        self.widgets.btn_rel.set_label(system)
        self.widgets.gremlin.set_property("metric_units", metric_units)
        widgetlist = ["jog_speed"]
        # self.stat.linear_units will return 1.0 for metric and 1/25,4 for imperial
        # display units not equal machine units
        if metric_units != int(self.stat.linear_units):
            if self.stat.linear_units == _MM:
                self.factor = (1.0 / 25.4)
            else:
                self.factor = 25.4
            self._update_slider(widgetlist)
        else:
            # display units equal machine units would be factor = 1,
            # but if factor not equal 1.0 than we have to reconvert from previous first
            if self.factor != 1.0:
                self.factor = 1 / self.factor
                self._update_slider(widgetlist)
                self.factor = 1.0
                self._update_slider(widgetlist)

    def on_hal_status_motion_mode_changed(self, widget, new_mode):
        if self.widgets.ntb_main.get_current_page() == _NB_SETUP:
            return
        if new_mode == 1 and self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            self.widgets.gremlin.set_property("enable_dro", True)
            self.widgets.gremlin.use_joints_mode = True
        else:
            self.widgets.gremlin.use_joints_mode = False

# ================
# Helper functions
# ================
    def init_unit_labels(self):
        if self.stat.linear_units == 1:
            self.widgets.lbl_feed_units.set_text("MM/MIN")
            self.widgets.lbl_vc_units.set_text("M/MIN")
        else:
            self.widgets.lbl_feed_units.set_text("IN/MIN")
            self.widgets.lbl_vc_units.set_text("FT/MIN")

    def _check_limits(self):
        for axis in self.data.axis_list:
            axisnumber = "xyzabcuvw".index(axis)
            if self.stat.limit[axisnumber] != 0:
                return True
        if self.widgets.chk_ignore_limits.get_active():
            self.widgets.chk_ignore_limits.set_active(False)
        return False

    def on_blockheight_changed(self, pin):
        self.widgets.lbl_blockheight.set_text("{0:.3f}".format(pin.get()))

    def on_clear_mdi_changed(self, pin):
        if self.halcomp["clear_mdi-waiting"] is False:
            if self.halcomp["clear_mdi-response"] is True:
                self.widgets.hal_mdihistory.model.clear()

    def on_tool_change(self,widget):
        change = self.halcomp['change-tool']
        tool_no = self.halcomp['tool-number']
        if change:
            if tool_no == 0:
                message = _("Please remove the mounted tool")
                secondary = ""
            else:
                message = _("Please change to tool # %s"%tool_no)
                secondary = self.widgets.tooledit1.get_toolinfo(tool_no)[16]
            self.data.tool_message = self.gscreen.notify(_("INFO:"),message,None)
            self.gscreen.warning_dialog(message, True, secondary, pinname="TOOLCHANGE")
        else:
            self.halcomp['tool-changed'] = False

    def notify(self, alert, message):
        msg_text = alert + " " + message
        self.widgets.lbl_notify.set_text(msg_text)

    def check_mode(self):
        string=[]
        if self.stat.task_mode == linuxcnc.MODE_MANUAL and self.widgets.ntb_main.get_current_page() == _NB_PREVIEW:
            string.append( self.data._MAN)
            if self.stat.interp_state == linuxcnc.INTERP_IDLE:
                string.append(self.data._JOG)
        return string

# ==========================
# override gscreen key calls
# ==========================
    def on_keycall_ESTOP(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.command.state(linuxcnc.STATE_ESTOP)
            return True

    def on_keycall_POWER(self,state,SHIFT,CNTRL,ALT):
        if state:
            if self.widgets.tbtn_estop.get_active():
                return True
            self.widgets.tbtn_on.emit("clicked")
            return True

    def on_keycall_ABORT(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.command.abort()
            return True

    def on_keycall_XPOS(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(0, 1, state)

    def on_keycall_XNEG(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(0, 0, state)

    def on_keycall_YPOS(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(1, 1, state)

    def on_keycall_YNEG(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(1, 0, state)

    def on_keycall_ZPOS(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(2, 1, state)

    def on_keycall_ZNEG(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        self.gscreen.do_key_jog(2, 0, state)

    def on_keycall_INCREMENTS(self,state,SHIFT,CNTRL,ALT):
        if not self.check_kb_shortcuts(): return
        widget = self.widgets.cmb_increments
        if state:
            index = widget.get_active()
            length = len(self.data.jog_increments)
            if SHIFT: # increment index
                if index < length - 1:
                    index = index + 1
            else: # decrement index
                if index > 0:
                    index = index - 1
            widget.set_active(index)
            self.on_cmb_increments_changed(widget)
        return(True)

    def check_kb_shortcuts(self):
        use_shortcuts = self.widgets.chk_use_kb_shortcuts.get_active()
        if not use_shortcuts:
            self.gscreen.notify(_("INFO"), _("Keyboard shortcuts are disabled"), INFO_ICON)
        return (use_shortcuts)

    def _update_vel(self):
        real_feed = float(self.stat.settings[1] * self.stat.feedrate)
        if self.stat.linear_units == _MM: # metric
            self.widgets.lbl_velocity.set_text("{0:d}".format(int(self.stat.current_vel * 60.0 * self.factor)))
            if "G95" in self.data.active_gcodes:
                feed_str = "{0:d}".format(int(self.stat.settings[1]))
                real_feed_str = "{0:.2f}".format(real_feed)
            else:
                feed_str = "{0:d}".format(int(self.stat.settings[1]))
                real_feed_str = "{0:d}".format(int(real_feed))
        else: # imperial
            self.widgets.lbl_velocity.set_text("{0:.2f}".format(self.stat.current_vel * 60.0 * self.factor))
            if "G95" in self.data.active_gcodes:
                feed_str = "{0:.4f}".format(self.stat.settings[1])
                real_feed_str = "{0:.4f}".format(real_feed)
            else:
                feed_str = "{0:.3f}".format(self.stat.settings[1])
                real_feed_str = "{0:.3f}".format(real_feed)

    def set_motion_mode(self, state):
        # 1:teleop, 0: joint
        self.command.teleop_enable(state)
        self.command.wait_complete()

    def reload_tool(self):
        tool_to_load = self.gscreen.prefs.getpref("tool_in_spindle", 0, int)
        if tool_to_load == 0:
            return
        self.load_tool = True
        self.tool_change = True
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        command = "M61 Q {0} G43".format(tool_to_load)
        self.command.mdi(command)
        self.command.wait_complete()

    def _update_coolant(self):
        if self.stat.flood != self.widgets.tbtn_flood.get_active():
            self.widgets.tbtn_flood.set_active(self.stat.flood)
        if self.stat.mist != self.widgets.tbtn_mist.get_active():
            self.widgets.tbtn_mist.set_active(self.stat.mist)

    def _update_spindle(self):
        if self.stat.spindle[0]['direction'] > 0:
            if not self.widgets.rbt_forward.get_active():
                self.widgets.rbt_forward.set_active(True)
        elif self.stat.spindle[0]['direction'] < 0:
            if not self.widgets.rbt_reverse.get_active():
                self.widgets.rbt_reverse.set_active(True)
        elif not self.widgets.rbt_stop.get_active():
            self.widgets.rbt_stop.set_active(True)
        if not abs(self.stat.spindle[0]['speed']):
            self.widgets.rbt_stop.set_active(True)
        if self.stat.spindle[0]['speed'] == 0:
            speed = 0
        else:
            speed = self.stat.spindle[0]['speed']
        self.widgets.lbl_spindle_rpm.set_text("{0}".format(int(speed * self.data.spindle_override)))

    def _update_vc(self):
        if self.stat.spindle[0]['direction'] != 0:
            if self.stat.spindle[0]['speed'] == 0:
                speed = 0
            else:
                speed = self.stat.spindle[0]['speed']
            vc = abs(int(speed * self.data.spindle_override) * self.tool_diameter * 3.1416 / 1000)
        else:
            vc = 0
        if vc >= 100:
            text = "{0:d}".format(int(vc))
        elif vc >= 10:
            text = "{0:2.1f}".format(vc)
        else:
            text = "{0:.2f}".format(vc)
        self.widgets.lbl_vc.set_text(text)

    def update_dro(self):
        j = 0
        for i in self.data.axis_list:
            if self.stat.joint[j]['homed']:
                self["label_home_%s"%i].modify_fg(gtk.STATE_NORMAL, gdk.color_parse("#0000FF"))
                color = self.homed_color
            else:
                self["label_home_%s"%i].modify_fg(gtk.STATE_NORMAL, gdk.color_parse("#FF0000"))
                color = self.unhomed_color
            j += 1
            attr = pango.AttrList()
            fg_color = pango.AttrForeground(color[0],color[1],color[2], 0, 11)
            size = pango.AttrSize(22000, 0, -1)
            weight = pango.AttrWeight(600, 0, -1)
            attr.insert(fg_color)
            attr.insert(size)
            attr.insert(weight)
            self.widgets["hal_dro_%s"%i].set_attributes(attr)

    def _set_spindle(self, command):
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            return
        if self.stat.task_mode != linuxcnc.MODE_MANUAL:
            if self.stat.interp_state == linuxcnc.INTERP_READING or self.stat.interp_state == linuxcnc.INTERP_WAITING:
                if self.stat.spindle[0]['direction'] > 0:
                    self.widgets.rbt_forward.set_sensitive(True)
                    self.widgets.rbt_reverse.set_sensitive(False)
                    self.widgets.rbt_stop.set_sensitive(False)
                elif self.stat.spindle[0]['direction'] < 0:
                    self.widgets.rbt_forward.set_sensitive(False)
                    self.widgets.rbt_reverse.set_sensitive(True)
                    self.widgets.rbt_stop.set_sensitive(False)
                else:
                    self.widgets.rbt_forward.set_sensitive(False)
                    self.widgets.rbt_reverse.set_sensitive(False)
                    self.widgets.rbt_stop.set_sensitive(True)
                return
        rpm = self.check_spindle_range()
        try:
            rpm_out = rpm / self.stat.spindle[0]['override']
        except:
            rpm_out = 0
        if command == "stop":
            self.command.spindle(0)
        elif command == "forward":
            self.command.spindle(1, rpm_out)
        elif command == "reverse":
            self.command.spindle(-1, rpm_out)
        else:
            print(_("Something went wrong, we have an unknown spindle widget {0}").format(command))

    def update_progress(self):
        if self.stat.task_mode != linuxcnc.MODE_AUTO:
            return
        if self.program_length > 0:
            progress = float(self.current_line) / float(self.program_length)
        else:
            progress = 0.0
        self.widgets.pgm_progress.set_fraction(progress)
        self.widgets.pgm_progress.set_text("{0:2.1f} % Complete".format(progress * 100))

    def init_jog_increments(self):
        list = self.data.jog_increments[:]
        del list[-1]
        # populate the increments combobox with rearranged increment values
        model = self.widgets.cmb_increments.get_model()
        model.clear()
        model.append(["continuous"])
        for index, inc in enumerate(list):
            model.append((inc,))
        self.widgets.cmb_increments.set_active(0)
        self.data.current_jogincr_index = index + 1

    def check_spindle_range(self):
        rpm = (self.stat.settings[2])
        if rpm == 0:
            rpm = abs(self.spindle_start_rpm)
        spindle_override = self.widgets.spc_spindle.get_value() / 100
        real_spindle_speed = rpm * spindle_override
        return real_spindle_speed

    def init_file_to_load(self):
        default_path = self.gscreen.inifile.find("DISPLAY", "PROGRAM_PREFIX")
        if not default_path:
            print("Path %s from DISPLAY , PROGRAM_PREFIX does not exist" % default_path)
            print("Trying default path...")
            default_path = "~/linuxcnc/nc_files/"
        open_file = self.gscreen.inifile.find("DISPLAY", "OPEN_FILE")
        if not open_file:
            print("No file to open")
        else:
            self.file_to_load = default_path + open_file

    def update_toolinfo(self, tool):
        toolinfo = self.widgets.tooledit1.get_toolinfo(tool)
        if toolinfo:
            # toolinfo[0] = cell toggle
            # toolinfo[1] = tool number
            # toolinfo[2] = pocket number
            # toolinfo[3] = X offset
            # toolinfo[4] = Y offset
            # toolinfo[5] = Z offset
            # toolinfo[6] = A offset
            # toolinfo[7] = B offset
            # toolinfo[8] = C offset
            # toolinfo[9] = U offset
            # toolinfo[10] = V offset
            # toolinfo[11] = W offset
            # toolinfo[12] = tool diameter
            # toolinfo[13] = frontangle
            # toolinfo[14] = backangle
            # toolinfo[15] = tool orientation
            # toolinfo[16] = tool info
            self.widgets.enter_tool.set_text(str(toolinfo[1]))
            # this assumes that all values in the tool table are metric
            self.widgets.lbl_tool_diameter.set_text(toolinfo[12])
            self.tool_diameter = float(toolinfo[12])
            self.widgets.lbl_tool_comment.set_text(toolinfo[16])
        if tool <= 0:
            self.widgets.enter_tool.set_text("0")
            self.widgets.lbl_tool_diameter.set_text("----")
            self.widgets.lbl_tool_comment.set_text("NO TOOL LOADED")
        if self.load_tool:
            self.load_tool = False
            self.on_hal_status_interp_idle(None)
            return
        if "G43" in self.data.active_gcodes and self.stat.task_mode != linuxcnc.MODE_AUTO:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi("G43")
            self.command.wait_complete()

    def _update_slider(self, widgetlist):
        for widget in widgetlist:
            value = self.widgets[widget].get_value()
            min = self.widgets[widget].get_property("min")
            max = self.widgets[widget].get_property("max")
            self.widgets[widget].set_range(min * self.factor, max * self.factor)
            self.widgets[widget].set_value(value * self.factor)

        self.scale_jog_vel = self.scale_jog_vel * self.factor
        self.fast_jog = self.fast_jog * self.factor
        self.slow_jog = self.slow_jog * self.factor            

    def init_offsetpage(self):
        self.gscreen.init_offsetpage()
        self.widgets.offsetpage1.set_display_follows_program_units()
        if self.stat.program_units != 1:
            self.widgets.offsetpage1.set_to_mm()
            self.widgets.offsetpage1.machine_units_mm = _MM
        else:
            self.widgets.offsetpage1.set_to_inch()
            self.widgets.offsetpage1.machine_units_mm = _INCH
        self.widgets.offsetpage1.set_row_visible("1", False)
        self.widgets.offsetpage1.set_font("sans 12")
        self.widgets.offsetpage1.set_foreground_color("#28D0D9")
        self.widgets.offsetpage1.selection_mask = ("Tool", "G5x", "Rot")
        systemlist = ["Tool", "G5x", "Rot", "G92", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1",
                      "G59.2", "G59.3"]
        names = []
        for system in systemlist:
            system_name = "system_name_{0}".format(system)
            name = self.gscreen.prefs.getpref(system_name, system, str)
            names.append([system, name])
        self.widgets.offsetpage1.set_names(names)

    def on_offset_axis_return(self, widget, result, calc, userdata, userdata2):
        value = calc.get_value()
        if result == gtk.ResponseType.ACCEPT:
            if value is not None:
                r = self.axis_to_ref
                self.gscreen.prefs.putpref("offset_axis_{}".format(r), value, str)
                self.command.mode(linuxcnc.MODE_MDI)
                self.command.wait_complete()
                command = "G10 L20 P0 {}{}".format(r, value)
                self.command.mdi(command)
                self.gscreen.reload_plot()
                self.command.mode(linuxcnc.MODE_MDI)
                self.command.wait_complete()
        widget.destroy()
        self.data.entry_dialog = None

    def on_blockheight_return(self, widget, result, calc, userdata, userdata2):
        blockheight = calc.get_value()
        if result == gtk.ResponseType.ACCEPT:
            if blockheight == "CANCEL" or blockheight == "ERROR":
                return
            if blockheight is not None or blockheight is not False or blockheight == 0:
                self.halcomp["block_height"] = blockheight
                self.gscreen.prefs.putpref("blockheight", blockheight, float)
            else:
                self.halcomp["block_height"] = 0.0
                self.gscreen.prefs.putpref("blockheight", 0.0, float)
        widget.destroy()
        self.data.entry_dialog = None

