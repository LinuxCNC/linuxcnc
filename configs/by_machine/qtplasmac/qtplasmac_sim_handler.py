import os
import linuxcnc
import hal
import time
from PyQt5 import QtCore

class HandlerClass:

    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.w.setWindowFlags(QtCore.Qt.WindowStaysOnTopHint)

    def initialized__(self):
        self.w.torch_on.hal_pin_changed.connect(self.torch_changed)
        mode = hal.get_value('plasmac.mode')
        self.set_mode(mode)

    def set_mode(self, mode):
        mode0 = [self.w.sensor_line, \
                 self.w.arc_ok, self.w.arc_ok_label, self.w.arc_ok_line, \
                 self.w.move_up, self.w.move_down, self.w.move_label]
        mode1 = [self.w.arc_ok_line, \
                 self.w.move_up, self.w.move_down, self.w.move_label]
        mode2 = [self.w.arc_voltage_in, self.w.arc_voltage_out, \
                  self.w.arc_voltage_label, self.w.arc_voltage_line]
        if mode == 1:
            self.w.mode_label.setText('Mode 1')
            for widget in mode1: widget.hide()
        elif mode == 2:
            self.w.mode_label.setText('Mode 2')
            for widget in mode2: widget.hide()
        else:
            for widget in mode0: widget.hide()
        self.w.resize(self.w.minimumSizeHint())

    def torch_changed(self, halpin):
        if halpin:
            time.sleep(0.1)
            if hal.get_value('plasmac.mode') == 0 or hal.get_value('plasmac.mode') == 1:
                self.w.arc_voltage_out.setValue(100.0)
                self.w.arc_voltage_out.setMinimum(90.0)
                self.w.arc_voltage_out.setMaximum(110.0)
            if (hal.get_value('plasmac.mode') == 1 or hal.get_value('plasmac.mode') == 2) and not self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()
        else:
            self.w.arc_voltage_out.setMinimum(0.0)
            self.w.arc_voltage_out.setMaximum(300.0)
            self.w.arc_voltage_out.setValue(0.0)
            if self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()

    # def __getitem__(self, item):
    #     return getattr(self, item)
    # def __setitem__(self, item, value):
    #     return setattr(self, item, value)

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
