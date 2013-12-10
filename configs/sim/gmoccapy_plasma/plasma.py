# This is only to show, that you can implement also your
# own python callbacks.

import linuxcnc
import os

class PlasmaClass:

    def __init__(self,halcomp,builder,useropts):
        inifile = linuxcnc.ini(os.getenv("INI_FILE_NAME"))
        self.builder = builder

        self.THC_StepSize = self.builder.get_object("THC_StepSize")
        value = inifile.find("PLASMA","THC_StepSize")
        self.THC_StepSize.set_value(float(value))

        self.TravelHeight = self.builder.get_object("TravelHeight")
        value = inifile.find("PLASMA","TravelHeight")
        self.TravelHeight.set_value(float(value))

        self.SwitchTravel = self.builder.get_object("SwitchTravel")
        value = inifile.find("PLASMA","SwitchTravel")
        self.SwitchTravel.set_value(float(value))

        self.PierceDelay = self.builder.get_object("PierceDelay")
        value = inifile.find("PLASMA","PierceDelay")
        self.PierceDelay.set_value(float(value))

        self.PierceGap = self.builder.get_object("PierceGap")
        value = inifile.find("PLASMA","PierceGap")
        self.PierceGap.set_value(float(value))

        self.Piercing_autostart = self.builder.get_object("Piercing_autostart")
        value = inifile.find("PLASMA","Piercing_autostart")
        self.Piercing_autostart.set_active(int(value))

        self.enable_HeightLock = self.builder.get_object("enable_HeightLock")
        value = inifile.find("PLASMA","enable_HeightLock")
        self.enable_HeightLock.set_active(int(value))

        self.CHL_Threshold = self.builder.get_object("CHL_Threshold")
        value = inifile.find("PLASMA","CHL_Threshold")
        self.CHL_Threshold.set_value(float(value))

        self.THC_TargetVoltage = self.builder.get_object("THC_TargetVoltage")
        value = inifile.find("PLASMA","THC_TargetVoltage")
        self.THC_TargetVoltage.set_value(float(value))

        self.btn_torch = self.builder.get_object("btn_torch")
        self.btn_torch.connect("toggled",self.on_btn_torch_toggled,self.btn_torch)

        self.Piercing_autostart = self.builder.get_object("Piercing_autostart")
        self.Piercing_autostart.connect("toggled",self.on_Piercing_autostart_toggled,self.Piercing_autostart)

        self.connect_signals()

    def on_btn_torch_toggled(self, widget, data=None):
        if widget.get_active():
            print("active")
        else:
            print("not active")

    def on_Piercing_autostart_toggled(self, widget, data=None):
        print("Checkbox Piercing has been toggled to state %s"%widget.get_active())

def get_handlers(halcomp,builder,useropts):
    return(PlasmaClass(halcomp,builder,useropts))
