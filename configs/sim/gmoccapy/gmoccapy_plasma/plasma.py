# This is only to show, that you can implement also your
# own python callbacks.

from linuxcnc import ini    # to read initial values
import os                   # to get the location of the INI file
import hal_glib             # needed to make our own hal pins
import hal                  # needed to make our own hal pins

class PlasmaClass:

    def __init__(self,halcomp,builder,useropts):
        self.inifile = ini(os.getenv("INI_FILE_NAME"))
        self.builder = builder
        self.halcomp = halcomp

        # get all information from INI File
        self.THC_Speed_incr         = float(self.inifile.find("PLASMA","THC_Speed_incr"))
        self.Cut_Gap_incr           = float(self.inifile.find("PLASMA","Cut_Gap_incr"))
        self.G0_Gap_incr            = float(self.inifile.find("PLASMA","G0_Gap_incr"))
        self.Pierce_Gap_incr        = float(self.inifile.find("PLASMA","Pierce_Gap_incr"))
        self.Pierce_Delay_incr      = float(self.inifile.find("PLASMA","Pierce_Delay_incr"))
        self.CHL_Threshold_incr     = float(self.inifile.find("PLASMA","CHL_Threshold_incr"))
        self.THC_Target_Voltage_incr= float(self.inifile.find("PLASMA","THC_Target_Voltage_incr"))

        # lets make our pins
        self.THC_speed = hal_glib.GPin(halcomp.newpin("THC-Speed", hal.HAL_FLOAT, hal.HAL_OUT))
        self.cut_gap = hal_glib.GPin(halcomp.newpin("Cut-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.g0_gap = hal_glib.GPin(halcomp.newpin("G0-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.pierce_deley = hal_glib.GPin(halcomp.newpin("Pierce-Delay", hal.HAL_FLOAT, hal.HAL_OUT))
        self.pierce_gap = hal_glib.GPin(halcomp.newpin("Pierce-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.target_voltage = hal_glib.GPin(halcomp.newpin("Target-Voltage", hal.HAL_FLOAT, hal.HAL_OUT))

        # get all widgets and connect them
        self.btn_THC_speed_minus = self.builder.get_object("btn_THC_speed_minus")
        self.btn_THC_speed_minus.connect("pressed",self.on_btn_THC_speed_pressed, -1)

        self.btn_THC_speed_plus = self.builder.get_object("btn_THC_speed_plus")
        self.btn_THC_speed_plus.connect("pressed",self.on_btn_THC_speed_pressed, 1)

        self.adj_THC_speed = self.builder.get_object("adj_THC_speed")
        self.adj_THC_speed.connect("value_changed", self.on_adj_THC_speed_value_changed)
        self.adj_THC_speed.upper = float(self.inifile.find("PLASMA","THC_Speed_max"))
        self.adj_THC_speed.lower = float(self.inifile.find("PLASMA","THC_Speed_min"))
        self.adj_THC_speed.set_value(float(self.inifile.find("PLASMA","THC_Speed")))

        self.btn_cut_gap_minus = self.builder.get_object("btn_cut_gap_minus")
        self.btn_cut_gap_minus.connect("pressed",self.on_btn_cut_gap_pressed, -1)

        self.btn_cut_gap_plus = self.builder.get_object("btn_cut_gap_plus")
        self.btn_cut_gap_plus.connect("pressed",self.on_btn_cut_gap_pressed, 1)

        self.adj_cut_gap = self.builder.get_object("adj_cut_gap")
        self.adj_cut_gap.connect("value_changed", self.on_adj_cut_gap_value_changed)
        self.adj_cut_gap.upper = float(self.inifile.find("PLASMA","Cut_Gap_max"))
        self.adj_cut_gap.lower = float(self.inifile.find("PLASMA","Cut_Gap_min"))
        self.adj_cut_gap.set_value(float(self.inifile.find("PLASMA","Cut_Gap")))

        self.btn_g0_minus = self.builder.get_object("btn_g0_minus")
        self.btn_g0_minus.connect("pressed",self.on_btn_g0_pressed, -1)

        self.btn_g0_plus = self.builder.get_object("btn_g0_plus")
        self.btn_g0_plus.connect("pressed",self.on_btn_g0_pressed, 1)

        self.adj_G0_gap = self.builder.get_object("adj_G0_gap")
        self.adj_G0_gap.connect("value_changed", self.on_adj_G0_gap_value_changed)
        self.adj_G0_gap.upper = float(self.inifile.find("PLASMA","G0_Gap_max"))
        self.adj_G0_gap.lower = float(self.inifile.find("PLASMA","G0_Gap_min"))
        self.adj_G0_gap.set_value(float(self.inifile.find("PLASMA","G0_Gap")))

        self.Piercing_autostart = self.builder.get_object("Piercing-autostart")
        value = self.inifile.find("PLASMA","Piercing_autostart")
        self.Piercing_autostart.set_active(int(value))

        self.btn_pierce_gap_minus = self.builder.get_object("btn_pierce_gap_minus")
        self.btn_pierce_gap_minus.connect("pressed",self.on_btn_pierce_gap_pressed, -1)

        self.btn_pierce_gap_plus = self.builder.get_object("btn_pierce_gap_plus")
        self.btn_pierce_gap_plus.connect("pressed",self.on_btn_pierce_gap_pressed, 1)

        self.adj_pierce_gap = self.builder.get_object("adj_pierce_gap")
        self.adj_pierce_gap.connect("value_changed", self.on_adj_pierce_gap_value_changed)
        self.adj_pierce_gap.upper = float(self.inifile.find("PLASMA","Pierce_Gap_max"))
        self.adj_pierce_gap.lower = float(self.inifile.find("PLASMA","Pierce_Gap_min"))
        self.adj_pierce_gap.set_value(float(self.inifile.find("PLASMA","Pierce_Gap")))

        self.btn_pierce_delay_minus = self.builder.get_object("btn_pierce_delay_minus")
        self.btn_pierce_delay_minus.connect("pressed",self.on_btn_pierce_delay_pressed, -1)

        self.btn_pierce_delay_plus = self.builder.get_object("btn_pierce_delay_plus")
        self.btn_pierce_delay_plus.connect("pressed",self.on_btn_pierce_delay_pressed, 1)

        self.adj_pierce_delay = self.builder.get_object("adj_pierce_delay")
        self.adj_pierce_delay.connect("value_changed", self.on_adj_pierce_delay_value_changed)
        self.adj_pierce_delay.upper = float(self.inifile.find("PLASMA","Pierce_Delay_max"))
        self.adj_pierce_delay.lower = float(self.inifile.find("PLASMA","Pierce_Delay_min"))
        self.adj_pierce_delay.set_value(float(self.inifile.find("PLASMA","Pierce_Delay")))

        self.enable_HeightLock = self.builder.get_object("enable-HeightLock")
        value = self.inifile.find("PLASMA","enable_Height_Lock")
        self.enable_HeightLock.set_active(int(value))

        self.adj_CHL_threshold = self.builder.get_object("adj_CHL_threshold")
        self.adj_CHL_threshold.connect("value_changed", self.on_adj_CHL_threshold_value_changed)
        self.adj_CHL_threshold.upper = float(self.inifile.find("PLASMA","CHL_Threshold_max"))
        self.adj_CHL_threshold.lower = float(self.inifile.find("PLASMA","CHL_Threshold_min"))
        self.adj_CHL_threshold.set_value(float(self.inifile.find("PLASMA","CHL_Threshold")))

        self.btn_THC_target_minus = self.builder.get_object("btn_THC_target_minus")
        self.btn_THC_target_minus.connect("pressed",self.on_btn_THC_target_pressed, -1)

        self.btn_THC_target_plus = self.builder.get_object("btn_THC_target_plus")
        self.btn_THC_target_plus.connect("pressed",self.on_btn_THC_target_pressed, 1)

        #self.lbl_prog_volt = self.builder.get_object("lbl_prog_volt")

        self.adj_THC_Voltage = self.builder.get_object("adj_THC_Voltage")
        self.adj_THC_Voltage.connect("value_changed", self.on_adj_THC_Voltage_value_changed)
        self.adj_THC_Voltage.upper = float(self.inifile.find("PLASMA","THC_Target_Voltage_max"))
        self.adj_THC_Voltage.lower = float(self.inifile.find("PLASMA","THC_Target_Voltage_min"))
        self.adj_THC_Voltage.set_value(float(self.inifile.find("PLASMA","THC_Target_Voltage")))

        self.lbl_prog_volt = self.builder.get_object("lbl_prog_volt")
        self.lbl_cut_speed = self.builder.get_object("lbl_cut_speed")
        self.lbl_cut_gap = self.builder.get_object("lbl_cut_gap")
        self.lbl_g0_gap = self.builder.get_object("lbl_g0_gap")
        self.lbl_pierce_gap = self.builder.get_object("lbl_pierce_gap")
        self.lbl_pierce_delay = self.builder.get_object("lbl_pierce_delay")
        self.init_labels()

    def init_labels(self):
        self.lbl_prog_volt.set_label("%d" %self.adj_THC_Voltage.get_value())
        self.lbl_cut_speed.set_label("%.1f" %self.adj_THC_speed.get_value())
        self.lbl_cut_gap.set_label("%.3f" %self.adj_cut_gap.get_value())
        self.lbl_g0_gap.set_label("%.3f" %self.adj_G0_gap.get_value())
        self.lbl_pierce_gap.set_label("%.3f" %self.adj_pierce_gap.get_value())
        self.lbl_pierce_delay.set_label("%.2f" %self.adj_pierce_delay.get_value())

    # What to do on button pres events?
    def on_btn_THC_speed_pressed(self, widget, dir):
        increment = self.THC_Speed_incr * dir
        value = self.adj_THC_speed.get_value() + increment
        self.adj_THC_speed.set_value(value) 

    def on_btn_cut_gap_pressed(self, widget, dir):
        increment = self.Cut_Gap_incr * dir
        value = self.adj_cut_gap.get_value() + increment
        self.adj_cut_gap.set_value(float(value))

    def on_btn_g0_pressed(self, widget, dir):
        increment = self.G0_Gap_incr * dir
        value = self.adj_G0_gap.get_value() + increment
        self.adj_G0_gap.set_value(float(value))

    def on_btn_pierce_gap_pressed(self, widget, dir):
        increment = self.Pierce_Gap_incr * dir
        value = self.adj_pierce_gap.get_value() + increment
        self.adj_pierce_gap.set_value(float(value))

    def on_btn_pierce_delay_pressed(self, widget, dir):
        increment = self.Pierce_Delay_incr * dir
        value = self.adj_pierce_delay.get_value() + increment
        self.adj_pierce_delay.set_value(float(value))

    def on_btn_THC_target_pressed(self, widget, dir):
        increment = self.THC_Target_Voltage_incr * dir
        value = self.adj_THC_Voltage.get_value() + increment
        self.adj_THC_Voltage.set_value(float(value))

    # and the behavior of the adjustments to control max and min values
    def on_adj_THC_speed_value_changed(self, widget, data=None):
        print("THC Speed value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_THC_speed_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_THC_speed_minus.set_sensitive(False)
        else:
            self.btn_THC_speed_plus.set_sensitive(True)
            self.btn_THC_speed_minus.set_sensitive(True)
        self.halcomp["THC-Speed"] = widget.get_value()
        self.lbl_cut_speed.set_label("%.1f"%(widget.get_value()))

    def on_adj_cut_gap_value_changed(self, widget, data=None):
        print("cut gap value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_cut_gap_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_cut_gap_minus.set_sensitive(False)
        else:
            self.btn_cut_gap_plus.set_sensitive(True)
            self.btn_cut_gap_minus.set_sensitive(True)
        self.halcomp["Cut-Gap"] = widget.get_value()
        self.lbl_cut_gap.set_label("%.3f"%(widget.get_value()))

    def on_adj_G0_gap_value_changed(self, widget, data=None):
        print("G0 Gap value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_g0_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_g0_minus.set_sensitive(False)
        else:
            self.btn_g0_plus.set_sensitive(True)
            self.btn_g0_minus.set_sensitive(True)
        self.halcomp["G0-Gap"] = widget.get_value()
        self.lbl_g0_gap.set_label("%.3f"%(widget.get_value()))

    def on_adj_pierce_gap_value_changed(self, widget, data=None):
        print("Pierce Gap value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_pierce_gap_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_pierce_gap_minus.set_sensitive(False)
        else:
            self.btn_pierce_gap_plus.set_sensitive(True)
            self.btn_pierce_gap_minus.set_sensitive(True)
        self.halcomp["Pierce-Gap"] = widget.get_value()
        self.lbl_pierce_gap.set_label("%.3f"%(widget.get_value()))

    def on_adj_pierce_delay_value_changed(self, widget, data=None):
        print("Pierce_delay value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_pierce_delay_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_pierce_delay_minus.set_sensitive(False)
        else:
            self.btn_pierce_delay_plus.set_sensitive(True)
            self.btn_pierce_delay_minus.set_sensitive(True)
        self.halcomp["Pierce-Delay"] = widget.get_value()
        self.lbl_pierce_delay.set_label("%.2f"%(widget.get_value()))

    def on_adj_CHL_threshold_value_changed(self, widget, data=None):
        print("CHL Threshold value = ",widget.get_value())

    def on_adj_THC_Voltage_value_changed(self, widget, data=None):
        print("THC Voltage value = ",widget.get_value())
        if widget.get_value() >= widget.upper:
            self.btn_THC_target_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_THC_target_minus.set_sensitive(False)
        else:
            self.btn_THC_target_plus.set_sensitive(True)
            self.btn_THC_target_minus.set_sensitive(True)
        self.halcomp["Target-Voltage"] = widget.get_value()
        self.lbl_prog_volt.set_label("%d"%(widget.get_value()))

def get_handlers(halcomp,builder,useropts):
    return(PlasmaClass(halcomp,builder,useropts))
