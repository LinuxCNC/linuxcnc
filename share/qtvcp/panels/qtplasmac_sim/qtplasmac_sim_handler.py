
'''
qtplasmac_sim_handler.py

Copyright (C) 2020 - 2024 Phillip A Carter
Copyright (C) 2020 - 2024 Gregory D Carl

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
import sys
import hal
from subprocess import run as RUN
from PyQt5 import QtCore
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import QMessageBox
from qtvcp.core import Info
from qtvcp.lib.preferences import Access

INFO = Info()


class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.hal = halcomp
        self.w = widgets
        self.paths = paths
        self.iniFile = INFO.INI
        self.w.setWindowFlags(QtCore.Qt.CustomizeWindowHint |
                              QtCore.Qt.WindowTitleHint |
                              QtCore.Qt.WindowStaysOnTopHint)
        self.machineName = self.iniFile.find('EMC', 'MACHINE')
        self.prefs = Access(os.path.join(self.paths.CONFIGPATH, self.machineName + '.prefs'))
        self.styleFile = f'{paths.CONFIGPATH}/qtplasmac_sim.qss'
        self.set_estop()
        self.set_style()

    def initialized__(self):
        self.w.setWindowTitle('QtPlasmaC Sim')
        self.iconPath = 'share/icons/hicolor/scalable/apps/linuxcnc_alt/linuxcncicon_plasma.svg'
        appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        self.iconBase = '/usr' if appPath == '/usr/bin' else appPath.replace('/bin', '/debian/extras/usr')
        self.w.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))
        self.breakPin = self.hal.newpin('sensor_breakaway', hal.HAL_BIT, hal.HAL_OUT)
        self.floatPin = self.hal.newpin('sensor_float', hal.HAL_BIT, hal.HAL_OUT)
        self.ohmicPin = self.hal.newpin('sensor_ohmic', hal.HAL_BIT, hal.HAL_OUT)
        self.torchPin = self.hal.newpin('torch_on', hal.HAL_BIT, hal.HAL_IN)
        self.statePin = self.hal.newpin('state', hal.HAL_S32, hal.HAL_IN)
        self.zPosPin = self.hal.newpin('z_position', hal.HAL_FLOAT, hal.HAL_IN)
        self.materialPin = self.hal.newpin('material_height', hal.HAL_FLOAT, hal.HAL_IN)
        self.arcVoltsOffsetPin = self.hal.newpin('arc_voltage_offset-f', hal.HAL_FLOAT, hal.HAL_OUT)
        simStepconf = False
        for sig in hal.get_info_signals():
            if sig['NAME'] == 'Zjoint-pos-fb':
                simStepconf = True
                break
        if simStepconf:
            RUN(['halcmd', 'net', 'Zjoint-pos-fb', 'qtplasmac_sim.z_position'])
        else:
            RUN(['halcmd', 'net', 'plasmac:axis-position', 'qtplasmac_sim.z_position'])
        RUN(['halcmd', 'net', 'plasmac:state', 'qtplasmac_sim.state'])
        self.torchPin.value_changed.connect(self.torch_changed)
        self.zPosPin.value_changed.connect(lambda v: self.z_position_changed(v))
        self.statePin.value_changed.connect(lambda v: self.plasmac_state_changed(v))
        self.w.arc_voltage_offset.valueChanged.connect(lambda v: self.arc_volts_offset_changed(v))
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
        self.height = 5 if hal.get_value('halui.machine.units-per-mm') == 1 else 0.2
        self.materialPin.set(self.height)
        self.w.estop.setStyleSheet(f'color: {self.foreColor}; background: {self.estopColor}')
        self.floatLatched = False
        self.ohmicLatched = False

    def set_estop(self):
        if self.prefs.getpref('Estop type', 0, int, 'GUI_OPTIONS') == 2:
            RUN(['halcmd', 'net', 'sim:estop-1-raw', 'iocontrol.0.user-enable-out', 'estop_not_1.in'])
            RUN(['halcmd', 'net', 'sim:estop-1-in', 'estop_not_1.out', 'estop_or.in1'])

    def set_style(self):
        self.foreColor = self.prefs.getpref('Foreground', '', str, 'COLOR_OPTIONS')
        self.backColor = self.prefs.getpref('Background', '', str, 'COLOR_OPTIONS')
        self.backAlt = self.prefs.getpref('Background Alt', '', str, 'COLOR_OPTIONS')
        self.estopColor = self.prefs.getpref('Estop', '', str, 'COLOR_OPTIONS')
        with open(self.styleFile, 'w') as outFile:
            outFile.write(
                '\n/****** DEFAULT ************/\n'
                f'* {{\n'
                f'    color: {self.foreColor};\n'
                f'    background: {self.backColor};\n'
                f'    font: 10pt DejaVuSans }}\n'
                '\n/****** BUTTONS ************/\n'
                f'QPushButton {{\n'
                f'    color: {self.foreColor};\n'
                f'    background: {self.backColor};\n'
                f'    border: 1px solid {self.foreColor};\n'
                '    border-radius: 4px;\n'
                f'}}\n'
                '\n/****** SLIDER ************/\n'
                f'QSlider::groove:horizontal {{\n'
                '    background: gray;\n'
                '    border-radius: 4px;\n'
                f'    height: 20px }}\n'
                f'\nQSlider::handle:horizontal {{\n'
                f'    background: {self.foreColor};\n'
                f'    border: 0px solid {self.foreColor};\n'
                '    border-radius: 4px;\n'
                f'    width: 24px }}\n'
                f'\nQSlider::handle:horizontal:disabled {{\n'
                f'    background: {self.backColor} }}\n'
                f'\nQSlider::add-page:horizontal {{\n'
                f'    background: {self.backAlt};\n'
                f'    border: 1px solid {self.backAlt};\n'
                f'    border-radius: 4px }}\n'
                f'\nQSlider::sub-page:horizontal {{\n'
                f'    background: {self.backAlt};\n'
                f'    border: 1px solid {self.backAlt};\n'
                f'    border-radius: 4px }}\n'
                f'\nLine {{\n'
                '    color: red;\n'
                f'    background: red }}\n'
                f'\nQCheckBox {{\n'
                f'    spacing: 20px }}\n'
                f'\nQCheckBox::indicator {{\n'
                f'    border: 1px solid {self.foreColor};\n'
                '    border-radius: 4px;\n'
                '    width: 20px;\n'
                f'    height: 20px }}\n'
                f'\nQCheckBox::indicator:pressed {{\n'
                f'    background: {self.foreColor} }}\n'
                f'\nQCheckBox::indicator:checked {{\n'
                f'    background: {self.foreColor} }}\n'
                f'\nQCheckBox::indicator:checked:pressed {{\n'
                f'    background: {self.foreColor} }}\n'
            )

    def arc_ok_clicked(self):
        if self.w.arc_ok.isChecked():
            self.w.arc_ok.setStyleSheet(f'color: {self.backColor}; background: {self.foreColor}')
        else:
            self.w.arc_ok.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')

    def float_timer_done(self):
        if self.below_material():
            return
        if self.w.sensor_flt.isDown():
            self.floatLatched = True
            return
        self.float_reset()

    def ohmic_timer_done(self):
        if self.below_material():
            return
        if self.w.sensor_ohm.isDown():
            self.ohmicLatched = True
            return
        self.ohmic_reset()

    def break_timer_done(self):
        if not self.w.sensor_brk.isDown():
            self.breakPin.set(0)
            self.w.sensor_brk.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')

    def float_pressed(self):
        if self.fTimer.isActive():
            self.fTimer.stop()   # stop timer so next click can start it again
            self.float_set()
        else:
            if self.floatPin.get():
                self.fTimer.stop()
                self.float_reset()
                self.floatLatched = False
            else:
                self.float_set()
                self.fTimer.start()

    def ohmic_pressed(self):
        if self.oTimer.isActive():
            self.oTimer.stop()   # stop timer so next click can start it again
            self.ohmic_set()
        else:
            if self.ohmicPin.get():
                self.oTimer.stop()
                self.ohmic_reset()
                self.ohmicLatched = False
            else:
                self.ohmic_set()
                self.oTimer.start()

    def float_set(self):
        self.floatPin.set(1)
        self.w.sensor_flt.setStyleSheet(f'color: {self.backColor}; background: {self.foreColor}')

    def float_reset(self):
        self.floatPin.set(0)
        self.w.sensor_flt.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')

    def ohmic_set(self):
        self.ohmicPin.set(1)
        self.w.sensor_ohm.setStyleSheet(f'color: {self.backColor}; background: {self.foreColor}')

    def ohmic_reset(self):
        self.ohmicPin.set(0)
        self.w.sensor_ohm.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')

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
            self.w.sensor_brk.setStyleSheet(f'color: {self.backColor}; background: {self.foreColor}')
        else:
            if self.breakPin.get():
                self.bTimer.stop()
                self.breakPin.set(0)
                self.w.sensor_brk.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')
            else:
                self.breakPin.set(1)
                self.w.sensor_brk.setStyleSheet(f'color: {self.backColor}; background: {self.foreColor}')
                self.bTimer.start()

    def estop_pressed(self):
        if hal.get_value('estop_or.in0') == 0:
            hal.set_p('estop_or.in0', '1')
            self.w.estop.setStyleSheet(f'color: {self.foreColor}; background: {self.estopColor}')
        else:
            hal.set_p('estop_or.in0', '0')
            self.w.estop.setStyleSheet(f'color: {self.foreColor}; background: {self.backColor}')

    def set_mode(self, mode):
        mode0 = [self.w.sensor_line,
                 self.w.arc_ok, self.w.arc_ok_label, self.w.arc_ok_line,
                 self.w.move_up, self.w.move_down, self.w.move_label]
        mode1 = [self.w.arc_ok_line,
                 self.w.move_up, self.w.move_down, self.w.move_label]
        mode2 = [self.w.offset_label, self.w.arc_voltage_offset,
                 self.w.arc_voltage_label, self.w.arc_voltage_line]
        if mode == 1:
            self.w.mode_label.setText('Mode 1')
            for widget in mode1:
                widget.hide()
        elif mode == 2:
            self.w.mode_label.setText('Mode 2')
            for widget in mode2:
                widget.hide()
        else:
            for widget in mode0:
                widget.hide()
        self.w.resize(self.w.minimumSizeHint())

    def torch_changed(self, halpin):
        if halpin:
            if (hal.get_value('plasmac.mode') == 1 or hal.get_value('plasmac.mode') == 2) and not self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()
                self.w.arc_ok_clicked()
        else:
            if self.w.arc_ok.isChecked():
                self.w.arc_ok.toggle()
                self.w.arc_ok_clicked()

    def arc_volts_offset_changed(self, value):
            self.arcVoltsOffsetPin.set(value * 0.1)
            self.w.offset_label.setText(f'{self.arcVoltsOffsetPin.get():0.1f} V')

    def plasmac_state_changed(self, state):
        if state == 11 and self.torchPin.get():
            self.w.arc_voltage_offset.setEnabled(True)
        else:
            self.w.arc_voltage_offset.setEnabled(False)
            self.w.arc_voltage_offset.setValue(0)

    def z_position_changed(self, height):
        if self.statePin.get() == 0:
            return
        if self.w.auto_flt.isChecked():
            if self.below_material() and not self.floatPin.get():
                self.float_pressed()
            elif not self.below_material() and self.floatPin.get() and not self.floatLatched:
                self.float_pressed()
        elif self.w.auto_ohm.isChecked():
            if self.below_material() and not self.ohmicPin.get():
                self.ohmic_set()
            elif not self.below_material() and self.ohmicPin.get() and not self.ohmicLatched:
                self.ohmic_reset()

    def below_material(self):
        mThick = self.materialPin.get() if self.materialPin.get() >= 0 else 0
        mTop = hal.get_value('ini.z.min_limit') + self.height + mThick if mThick else 0
        if self.zPosPin.get() < mTop:
            return True
        else:
            return False

    def help_pressed(self):
        msg = QMessageBox(self.w)
        buttonY = msg.addButton(QMessageBox.Ok)
        buttonY.setText('OK')
        msg.setIcon(QMessageBox.Information)
        msg.setWindowTitle('Sim Panel Help')
        message = 'This panel provides buttons for simulating basic plasma signals.\n'
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


def get_handlers(halcomp, widgets, paths):
    return [HandlerClass(halcomp, widgets, paths)]
