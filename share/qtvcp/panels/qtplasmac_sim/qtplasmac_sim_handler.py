import linuxcnc
import hal
import time
from PyQt5 import QtCore
from PyQt5.QtGui import QPalette, QColor

class HandlerClass:

    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.w.setWindowFlags(QtCore.Qt.CustomizeWindowHint | \
                              QtCore.Qt.WindowTitleHint | \
                              QtCore.Qt.WindowStaysOnTopHint )

    def initialized__(self):
        self.w.setWindowTitle('QtPlasmaC SimPanel')
        self.w.torch_on.hal_pin_changed.connect(self.torch_changed)
        self.w.sensor_flt.pressed.connect(self.float_pressed)
        self.w.sensor_ohm.pressed.connect(self.ohmic_pressed)
        self.w.sensor_brk.pressed.connect(self.break_pressed)
        self.w.arc_ok.pressed.connect(self.arc_ok_pressed)
        self.w.estop.pressed.connect(self.estop_pressed)
        self.fTimer = QtCore.QTimer()
        self.fTimer.setInterval(500)
        self.fTimer.setSingleShot(True)
        self.fTimer.timeout.connect(self.float_timer_done)
        self.oTimer = QtCore.QTimer()
        self.oTimer.setInterval(500)
        self.oTimer.setSingleShot(True)
        self.oTimer.timeout.connect(self.ohmic_timer_done)
        self.bTimer = QtCore.QTimer()
        self.bTimer.setInterval(500)
        self.bTimer.setSingleShot(True)
        self.bTimer.timeout.connect(self.break_timer_done)
        self.backColor = '#16160e'
        mode = hal.get_value('plasmac.mode')
        self.set_mode(mode)
        hal.set_p('estop_or.in0', '1')
        self.w.estop.setStyleSheet('background: red')

    def arc_ok_pressed(self):
        if self.w.arc_ok.isChecked():
            self.w.arc_ok.setStyleSheet('background: {}'.format(self.backColor))
        else:
            self.w.arc_ok.setStyleSheet('background: green')

    def float_timer_done(self):
        if not self.w.sensor_flt.isDown():
            self.w.sensor_float.hal_pin.set(0)
            self.w.sensor_flt.setStyleSheet('background: {}'.format(self.backColor))

    def ohmic_timer_done(self):
        if not self.w.sensor_ohm.isDown():
            self.w.sensor_ohmic.hal_pin.set(0)
            self.w.sensor_ohm.setStyleSheet('background: {}'.format(self.backColor))

    def break_timer_done(self):
        if not self.w.sensor_brk.isDown():
            self.w.sensor_breakaway.hal_pin.set(0)
            self.w.sensor_brk.setStyleSheet('background: {}'.format(self.backColor))

    def float_pressed(self):
        if self.fTimer.isActive():
            self.fTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_float.hal_pin.set(1)
            self.w.sensor_flt.setStyleSheet('background: green')
        else:
            if self.w.sensor_float.hal_pin.get():
                self.fTimer.stop()
                self.w.sensor_float.hal_pin.set(0)
                self.w.sensor_flt.setStyleSheet('background: {}'.format(self.backColor))
            else:
                self.w.sensor_float.hal_pin.set(1)
                self.w.sensor_flt.setStyleSheet('background: green')
                self.fTimer.start()

    def ohmic_pressed(self):
        if self.oTimer.isActive():
            self.oTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_ohmic.hal_pin.set(1)
            self.w.sensor_ohm.setStyleSheet('background: green')
        else:
            if self.w.sensor_ohmic.hal_pin.get():
                self.oTimer.stop()
                self.w.sensor_ohmic.hal_pin.set(0)
                self.w.sensor_ohm.setStyleSheet('background: {}'.format(self.backColor))
            else:
                self.w.sensor_ohmic.hal_pin.set(1)
                self.w.sensor_ohm.setStyleSheet('background: green')
                self.oTimer.start()

    def break_pressed(self):
        if self.bTimer.isActive():
            self.bTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_breakaway.hal_pin.set(1)
            self.w.sensor_brk.setStyleSheet('background: green')
        else:
            if self.w.sensor_breakaway.hal_pin.get():
                self.bTimer.stop()
                self.w.sensor_breakaway.hal_pin.set(0)
                self.w.sensor_brk.setStyleSheet('background: {}'.format(self.backColor))
            else:
                self.w.sensor_breakaway.hal_pin.set(1)
                self.w.sensor_brk.setStyleSheet('background: green')
                self.bTimer.start()

    def estop_pressed(self):
        if hal.get_value('estop_or.in0') == 0:
            hal.set_p('estop_or.in0', '1')
            self.w.estop.setStyleSheet('background: red')
        else:
            hal.set_p('estop_or.in0', '0')
            self.w.estop.setStyleSheet('background: {}'.format(self.backColor))

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

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
