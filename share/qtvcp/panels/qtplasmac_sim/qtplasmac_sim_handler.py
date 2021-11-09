
'''
qtplasmac_sim_handler.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import linuxcnc
import hal
import time
from subprocess import call as CALL
from PyQt5 import QtCore
from PyQt5.QtGui import QPalette, QColor, QIcon
from PyQt5.QtWidgets import QMessageBox

class HandlerClass:

    def __init__(self, halcomp, widgets, paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths
        self.w.setWindowFlags(QtCore.Qt.CustomizeWindowHint | \
                              QtCore.Qt.WindowTitleHint | \
                              QtCore.Qt.WindowStaysOnTopHint )
        self.prefsFile = '{}/qtplasmac.prefs'.format(paths.CONFIGPATH)
        self.styleFile = '{}/qtplasmac_sim.qss'.format(paths.CONFIGPATH)
        self.set_style()

    def initialized__(self):
        self.w.setWindowTitle('QtPlasmaC Sim')
        self.IMAGES = os.path.join(self.PATHS.IMAGEDIR, 'qtplasmac/images/')
        self.w.setWindowIcon(QIcon(os.path.join(self.IMAGES, 'linuxcncicon.png')))
        self.breakPin = self.hal.newpin('sensor_breakaway', hal.HAL_BIT, hal.HAL_OUT)
        self.floatPin = self.hal.newpin('sensor_float', hal.HAL_BIT, hal.HAL_OUT)
        self.ohmicPin = self.hal.newpin('sensor_ohmic', hal.HAL_BIT, hal.HAL_OUT)
        self.torchPin = self.hal.newpin('torch_on', hal.HAL_BIT, hal.HAL_IN)
        self.statePin = self.hal.newpin('state', hal.HAL_S32, hal.HAL_IN)
        self.zPosPin = self.hal.newpin('z_position', hal.HAL_FLOAT, hal.HAL_IN)
        self.arcVoltsPin = self.hal.newpin('arc_voltage_out-f', hal.HAL_FLOAT, hal.HAL_OUT)
        CALL(['halcmd', 'net', 'plasmac:axis-position', 'qtplasmac_sim.z_position'])
        CALL(['halcmd', 'net', 'plasmac:state', 'qtplasmac_sim.state'])
        self.torchPin.value_changed.connect(self.torch_changed)
        self.zPosPin.value_changed.connect(lambda v:self.z_position_changed(v))
        self.w.arc_voltage_out.valueChanged.connect(lambda v:self.arc_volts_changed(v))
        self.w.sensor_flt.pressed.connect(self.float_pressed)
        self.w.sensor_ohm.pressed.connect(self.ohmic_pressed)
        self.w.sensor_brk.pressed.connect(self.break_pressed)
        self.w.arc_ok.clicked.connect(self.arc_ok_clicked)
        self.w.estop.pressed.connect(self.estop_pressed)
        self.w.auto_flt.pressed.connect(self.auto_float_pressed)
        self.w.auto_flt.setChecked(True)
        self.w.auto_ohm.pressed.connect(self.auto_ohmic_pressed)
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
        self.w.help.pressed.connect(self.help_pressed)
        mode = hal.get_value('plasmac.mode')
        self.set_mode(mode)
        hal.set_p('estop_or.in0', '1')
        zMin = hal.get_value('ini.z.min_limit')
        self.zProbe = zMin + (10 * hal.get_value('halui.machine.units-per-mm'))
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
            '    background: red }}\n'\
            '\nQCheckBox {{\n'\
            '    spacing: 20px }}\n'\
            '\nQCheckBox::indicator {{\n'\
            '    border: 1px solid {0};\n'\
            '    border-radius: 4px;\n'\
            '    width: 20px;\n'\
            '    height: 20px }}\n'\
            '\nQCheckBox::indicator:pressed {{\n'\
            '    background: {0} }}\n'\
            '\nQCheckBox::indicator:checked {{\n'\
            '    background: {0} }}\n'\
            '\nQCheckBox::indicator:checked:pressed {{\n'\
            '    background: {1} }}\n'\
            .format(self.foreColor, self.backColor, self.backAlt, self.estopColor)
            )

    def arc_ok_clicked(self):
        if self.w.arc_ok.isChecked():
            self.w.arc_ok.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            self.w.arc_ok.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def float_timer_done(self):
        if not self.w.sensor_flt.isDown():
            self.floatPin.set(0)
            self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def ohmic_timer_done(self):
        if not self.w.sensor_ohm.isDown():
            self.ohmicPin.set(0)
            self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def break_timer_done(self):
        if not self.w.sensor_brk.isDown():
            self.breakPin.set(0)
            self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))

    def float_pressed(self):
        if self.fTimer.isActive():
            self.fTimer.stop()   # stop timer so next click can start it again
            self.floatPin.set(1)
            self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.floatPin.get():
                self.fTimer.stop()
                self.floatPin.set(0)
                self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.floatPin.set(1)
                self.w.sensor_flt.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
                self.fTimer.start()

    def ohmic_pressed(self):
        if self.oTimer.isActive():
            self.oTimer.stop()   # stop timer so next click can start it again
            self.ohmicPin.set(1)
            self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.ohmicPin.get():
                self.oTimer.stop()
                self.ohmicPin.set(0)
                self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.ohmicPin.set(1)
                self.w.sensor_ohm.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
                self.oTimer.start()

    def auto_float_pressed(self):
        if self.w.auto_ohm.isChecked:
            self.w.auto_ohm.setChecked(False)

    def auto_ohmic_pressed(self):
        if self.w.auto_flt.isChecked:
            self.w.auto_flt.setChecked(False)

    def break_pressed(self):
        if self.bTimer.isActive():
            self.bTimer.stop()   # stop timer so next click can start it again
            self.breakPin.set(1)
            self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.backColor, self.foreColor))
        else:
            if self.breakPin.get():
                self.bTimer.stop()
                self.breakPin.set(0)
                self.w.sensor_brk.setStyleSheet('color: {}; background: {}'.format(self.foreColor, self.backColor))
            else:
                self.breakPin.set(1)
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
                self.w.arc_voltage_out.setValue(1000)
                self.w.arc_voltage_out.setMinimum(900)
                self.w.arc_voltage_out.setMaximum(1100)
                self.w.arc_voltage_out.setSingleStep(1)
                self.w.arc_voltage_out.setPageStep(1)
            if (hal.get_value('plasmac.mode') == 1 or hal.get_value('plasmac.mode') == 2) and not self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()
                self.w.arc_ok_clicked()
        else:
            self.w.arc_voltage_out.setMinimum(0)
            self.w.arc_voltage_out.setMaximum(3000)
            self.w.arc_voltage_out.setValue(0)
            self.w.arc_voltage_out.setSingleStep(10)
            self.w.arc_voltage_out.setPageStep(100)
            if self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()
                self.w.arc_ok_clicked()

    def arc_volts_changed(self, value):
        if self.w.arc_voltage_out.maximum() == 3000:
            self.arcVoltsPin.set(int(value * 0.1))
        else:
            self.arcVoltsPin.set(value * 0.1)

    def z_position_changed(self, height):
        if self.w.auto_flt.isChecked():
            if height < self.zProbe and not self.floatPin.get() and (self.statePin.get() == 1 or self.statePin.get() == 2):
                self.float_pressed()
            elif (height > self.zProbe) and self.floatPin.get() and self.statePin.get() == 3:
                self.float_pressed()
        elif self.w.auto_ohm.isChecked():
            if height < self.zProbe and not self.ohmicPin.get() and (self.statePin.get() == 1 or self.statePin.get() == 2):
                self.ohmic_pressed()
            elif (height > self.zProbe) and self.ohmicPin.get() and self.statePin.get() == 3:
                self.ohmic_pressed()

    def help_pressed(self):
        msg = QMessageBox(self.w)
        buttonY = msg.addButton(QMessageBox.Ok)
        buttonY.setText('OK')
        msg.setIcon(QMessageBox.Information)
        msg.setWindowTitle('Sim Panel Help')
        message  = 'This panel provides buttons for simulating basic plasma signals.\n'
        message += '\nThe operating mode of QtPlasmaC determines which widgets appear on the panel.\n'
        message += '\nESTOP is required to be cleared before turning the main GUI on and homing the config.\n'
        message += '\nIf a gcode program is loaded and the CYCLE START button pressed then the panel is fully automatic during the running of the program.\n'
        message += '\nThe type of probing is selected by the checkbox below the appropriate button. When the "torch" decends to 10mm (0.39") above the bottom limit then the selected signal will be emitted and the program will continue.\n'
        message += '\nThe ARC OK signal is automatic for all modes.\n'
        message += '\nFor Modes 0 & 1 the ARC VOLTAGE can be adjusted during the "cut" and the "torch" will respond appropriately.\n'
        message += '\nFor Mode 2 the UP and DOWN buttons can be pressed during the "cut" and the "torch" will respond appropriately.\n'
        message += '\nOther buttons can be pressed at any stage of a program run to show the various effect that they have on a running program.\n'
        msg.setText(message)
        msg.exec_()

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
