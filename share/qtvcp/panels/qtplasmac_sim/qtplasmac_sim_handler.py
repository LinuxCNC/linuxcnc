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
        self.prefsFile = '{}/qtplasmac.prefs'.format(paths.CONFIGPATH)
        self.styleFile = '{}/qtplasmac_sim.qss'.format(paths.CONFIGPATH)
        self.set_style()

    def initialized__(self):
        self.w.setWindowTitle('QtPlasmaC Sim')
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
        mode = hal.get_value('plasmac.mode')
        self.set_mode(mode)
        hal.set_p('estop_or.in0', '1')
        self.w.estop.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.estopColor))

    def set_style(self):
        self.foreColor = '#ffee06'
        self.backColor = '#16160e'
        self.backAlt = '#36362e'
        self.estopColor = '#ff0000'
        try:
            with open(self.prefsFile, 'r') as inFile:
                for line in inFile:
                    if line.startswith('Foreground'):
                        self.foreColor = line.split('=')[1].strip()
                    elif line.startswith('Background Alt'):
                        self.backAlt = line.split('=')[1].strip()
                    elif line.startswith('Background'):
                        self.backColor = line.split('=')[1].strip()
                    elif line.startswith('Estop'):
                        self.estopColor = line.split('=')[1].strip()
        except:
            pass
        with open(self.styleFile, 'w') as outFile:
            outFile.write(
            '\n/****** DEFAULT ************/\n'\
            '* {{\n'\
            '    color: {0};\n'\
            '    background: {1};\n'\
            '    font: 10pt Lato }}\n'\
            '\n/****** BUTTONS ************/\n'\
            'QPushButton {{\n'\
            '    color: {0};\n'\
            '    background: {1};\n'\
            '    border: 1px solid {0};\n'\
            '    border-radius: 4px;\n'\
            '}}\n'\
            '\n/****** SLIDER ************/\n'\
            'QSlider::groove:horizontal {{\n'\
            '    background: gray;\n'\
            '    border-radius: 4px;\n'\
            '    height: 20px }}\n'\
            '\nQSlider::handle:horizontal {{\n'\
            '    background: {0};\n'\
            '    border: 0px solid {0};\n'\
            '    border-radius: 4px;\n'\
            '    width: 24px }}\n'\
            '\nQSlider::add-page:horizontal {{\n'\
            '    background: {2};\n'\
            '    border: 1px solid {2};\n'\
            '    border-radius: 4px }}\n'\
            '\nQSlider::sub-page:horizontal {{\n'\
            '    background: {2};\n'\
            '    border: 1px solid {2};\n'\
            '    border-radius: 4px }}\n'\
            '\nLine {{\n'\
            '    color: red;\n'\
            '    background: red;\n'\
            '}}\n'.format(self.foreColor, self.backColor, self.backAlt, self.estopColor)
            )

    def arc_ok_pressed(self):
        if self.w.arc_ok.isChecked():
            self.w.arc_ok.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
        else:
            self.w.arc_ok.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))

    def float_timer_done(self):
        if not self.w.sensor_flt.isDown():
            self.w.sensor_float.hal_pin.set(0)
            self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def ohmic_timer_done(self):
        if not self.w.sensor_ohm.isDown():
            self.w.sensor_ohmic.hal_pin.set(0)
            self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def break_timer_done(self):
        if not self.w.sensor_brk.isDown():
            self.w.sensor_breakaway.hal_pin.set(0)
            self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def float_pressed(self):
        if self.fTimer.isActive():
            self.fTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_float.hal_pin.set(1)
            self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.w.sensor_float.hal_pin.get():
                self.fTimer.stop()
                self.w.sensor_float.hal_pin.set(0)
                self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.w.sensor_float.hal_pin.set(1)
                self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
                self.fTimer.start()

    def ohmic_pressed(self):
        if self.oTimer.isActive():
            self.oTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_ohmic.hal_pin.set(1)
            self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.w.sensor_ohmic.hal_pin.get():
                self.oTimer.stop()
                self.w.sensor_ohmic.hal_pin.set(0)
                self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.w.sensor_ohmic.hal_pin.set(1)
                self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
                self.oTimer.start()

    def break_pressed(self):
        if self.bTimer.isActive():
            self.bTimer.stop()   # stop timer so next click can start it again
            self.w.sensor_breakaway.hal_pin.set(1)
            self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.w.sensor_breakaway.hal_pin.get():
                self.bTimer.stop()
                self.w.sensor_breakaway.hal_pin.set(0)
                self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.w.sensor_breakaway.hal_pin.set(1)
                self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
                self.bTimer.start()

    def estop_pressed(self):
        if hal.get_value('estop_or.in0') == 0:
            hal.set_p('estop_or.in0', '1')
            self.w.estop.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.estopColor))
        else:
            hal.set_p('estop_or.in0', '0')
            self.w.estop.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

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
