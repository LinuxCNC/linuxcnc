
'''
pmx_test.py

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
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import sys
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *

try:
    import serial
    import serial.tools.list_ports
    sMod = True
except:
    sMod = False

address      = '01'
regRead      = '04'
regWrite     = '06'
rCurrent     = '2094'
rCurrentMax  = '209A'
rCurrentMin  = '2099'
rFault       = '2098'
rMode        = '2093'
rPressure    = '2096'
rPressureMax = '209D'
rPressureMin = '209C'
rArcTimeLow  = '209E'
rArcTimeHigh = '209F'
validRead    = '0402'


class App(QWidget):
    def __init__(self):
        super().__init__()
        if not sMod:
            msg = '\npyserial module not available\n'\
                  '\nto install, open a terminal and enter:\n'\
                  '\nsudo apt-get install python3-serial\n'
            response = QMessageBox()
            response.setText(msg)
            response.exec_()
            raise SystemExit
        self.iconPath = 'share/icons/hicolor/scalable/apps/linuxcnc_alt/linuxcncicon_plasma.svg'
        appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        self.iconBase = '/usr' if appPath == '/usr/bin' else appPath.replace('/bin', '/debian/extras/usr')
        self.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))
        self.setWindowTitle('Powermax Communicator')
        qtRectangle = self.frameGeometry()
        centerPoint = QDesktopWidget().availableGeometry().center()
        qtRectangle.moveCenter(centerPoint)
        self.move(qtRectangle.topLeft())
        self.createGridLayout()
        self.setLayout(self.grid)
        self.show()
        self.timer = QTimer(self)
        self.portName.addItem('SELECT A PORT')
        for item in serial.tools.list_ports.comports():
            self.portName.addItem(item.device)
        self.connected = False
        self.portName.activated.connect(self.on_port_changed)
        self.portFile = None
        self.portScan.pressed.connect(self.on_port_scan)
        self.usePanel.toggled.connect(self.on_use_toggled)
        self.modeSet.currentIndexChanged.connect(lambda: self.on_value_changed(self.modeSet, rMode, 1))
        self.currentSet.valueChanged.connect(lambda: self.on_value_changed(self.currentSet, rCurrent, 64))
        self.pressureSet.valueChanged.connect(lambda: self.on_value_changed(self.pressureSet, rPressure, 128))
        self.timer.timeout.connect(self.periodic)
        self.setStyleSheet(
            'QWidget {color: #ffee06; background: #16160e} \
            QLabel {height: 20} \
            QPushButton {border: 1 solid #ffee06; border-radius: 4; height: 30; width: 80} \
            QComboBox {color: #ffee06; background-color: #16160e; border: 1 solid #ffee06; border-radius: 4; height: 30; padding-left: 10} \
            QComboBox::drop-down {width: 0} \
            QComboBox QListView {border: 4p solid #ffee06; border-radius: 0} \
            QComboBox QAbstractItemView {border: 2px solid #ffee06; border-radius: 4} \
            QDoubleSpinBox {border: 1 solid #ffee06; border-radius: 4; height: 30; width: 80} \
            QRadioButton::indicator {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 20} \
            QRadioButton::indicator:checked {background: #ffee06} \
            QDoubleSpinBox::up-button {subcontrol-origin:padding; subcontrol-position:right; width: 28px; height: 24px} \
            QDoubleSpinBox::down-button {subcontrol-origin:padding; subcontrol-position:left; width: 28px; height: 24px}'
            )

    def periodic(self):
        if not os.path.exists(self.portFile):
            self.timer.stop()
            self.connected = False
            self.usePanel.setChecked(True)
            self.useComms.setEnabled(False)
            self.clear_text()
            self.portName.clear()
            self.portName.addItem('SELECT A PORT')
            try:
                self.openPort.close()
            except:
                pass
            self.dialog_ok(QMessageBox.Warning,
                           'Error',
                           '\nCommunications device lost.\n'
                           '\nA Port Scan is required.\n')
        if self.connected:
            for reg in (rMode, rCurrent, rPressure, rFault):
                if not self.read_register(reg):
                    return True

    def on_value_changed(self, widget, reg, multiplier):
        if not self.connected:
            return
        if reg == rMode:
            mode = self.modeSet.currentIndex() + 1
            self.pressureSet.setValue(0)
            if not self.write_to_register(rMode, f'{mode:04x}'):
                return
            self.mode_changed()
            return
        elif reg == rPressure:
            if self.pressureType == 'bar':
                if widget.value() == 0.1:
                    widget.setValue(self.minPressure)
                elif widget.value() == self.minPressure - 0.1:
                    widget.setValue(0)
            else:
                if widget.value() == 1:
                    widget.setValue(self.minPressure)
                elif widget.value() == self.minPressure - 1:
                    widget.setValue(0)
            data = (f'{int(widget.value() * multiplier):04X}').upper()
        elif reg == rCurrent:
            data = (f'{int(widget.value() * multiplier):04X}').upper()
        self.write_to_register(reg, data)

    def get_lrc(self, data):
        lrc = 0
        for i in range(0, len(data), 2):
            a, b = data[i:i+2]
            try:
                lrc = (lrc + int(a + b, 16)) & 255
            except:
                print('broken packet in get_lrc')
                return '00'
        lrc = (f'{(((lrc ^ 255) + 1) & 255):02X}').upper()
        return lrc

    def write_to_register(self, reg, data):
        data = f'{address}{regWrite}{reg}{data}'
        lrc = self.get_lrc(data)
        packet = f':{data}{lrc}\r\n'
        errors = 0
        while 1:
            try:
                reply = ''
                self.openPort.write(packet.encode())
                reply = self.openPort.readline().decode()
            except:
                return False
            if reply == packet:
                break
            else:
                errors += 1
                if errors == 3:
                    self.connected = False
                    self.usePanel.setChecked(True)
                    self.dialog_ok(QMessageBox.Warning,
                                   'Error',
                                   '\nNo reply while writing to plasma unit.\n'
                                   '\nCheck connections and retry when ready.\n')
                    return False
        return True

    def read_from_register(self, reg):
        data = f'{address}{regRead}{reg}0001'
        lrc = self.get_lrc(data)
        packet = f':{data}{lrc}\r\n'
        reply = ''
        self.openPort.write(packet.encode())
        reply = self.openPort.readline().decode()
        if reply:
            return reply
        else:
            self.connected = False
            self.usePanel.setChecked(True)
            self.dialog_ok(QMessageBox.Warning,
                           'Error',
                           '\nNo reply while reading from plasma unit.\n'
                           '\nCheck connections and retry when ready.\n')
            return None

    def read_register(self, reg):
        try:
            result = self.read_from_register(reg).strip().lstrip(':')
        except:
            return
        if result:
            if int(result.strip(), 16) >= 0:
                if result[:6] == f'{address}{validRead}':
                    lrc = self.get_lrc(f'{result[:10]}')
                    if lrc == result[10:12]:
                        if reg == rMode:
                            data = int(result[6:10])
                            self.modeValue.setText(str(data))
                            return data
                        elif reg == rCurrent:
                            data = float(int(result[6:10], 16) / 64.0)
                            self.currentValue.setText(f'{data:.0f}')
                            return data
                        elif reg == rPressure:
                            data = float(int(result[6:10], 16) / 128.0)
                            if self.pressureType == 'bar':
                                self.pressureValue.setText(f'{data:.1f}')
                            else:
                                self.pressureValue.setText(f'{data:.0f}')
                            return 1
                        elif reg == rFault:
                            fault = int(result[6:10], 16)
                            code = f'{fault:04d}'
                            if fault > 0:
                                self.faultLabel.setText('FAULT')
                                self.faultValue.setText(f'{code[0]}-{code[1:3]}-{code[3]}')
                            else:
                                self.faultLabel.setText('')
                                self.faultValue.setText('')
                            if fault == 210:
                                if float(self.currentMax.text()) > 110:
                                    self.faultName.setText(f'{faultCode[code][1]}')
                                else:
                                    self.faultName.setText(f'{faultCode[code][1]}')
                            else:
                                try:
                                    self.faultName.setText(f'{faultCode[code]}')
                                except:
                                    self.faultName.setText('UNKNOWN FAULT CODE')
                            return code
                        elif reg == rCurrentMin:
                            data = float(int(result[6:10], 16) / 64.0)
                            self.currentMin.setText(f'{data:.0f}')
                            return data
                        elif reg == rCurrentMax:
                            data = float(int(result[6:10], 16) / 64.0)
                            self.currentMax.setText(f'{data:.0f}')
                            return data
                        elif reg == rPressureMin:
                            data = float(int(result[6:10], 16) / 128.0)
                            self.minimumPressure = data
                            if data < 15:
                                self.pressureType = 'bar'
                                self.pressureMin.setText(f'{data:.1f}')
                                self.pressure.setSingleStep(0.1)
                                self.pressure.setDecimals(1)
                            else:
                                self.pressureType = 'psi'
                                self.pressureMin.setText(f'{data:.0f}')
                                self.pressureSet.setSingleStep(1)
                                self.pressureSet.setDecimals(0)
                            return data
                        elif reg == rPressureMax:
                            data = float(int(result[6:10], 16) / 128.0)
                            if self.pressureType == 'bar':
                                self.pressureMax.setText(f'{data:.1f}')
                                self.pressureSet.setMaximum(data)
                            else:
                                self.pressureMax.setText(f'{data:.0f}')
                                self.pressureSet.setMaximum(data)
                            return data
                        elif reg == rArcTimeLow:
                            data = result[6:10]
                            return data
                        elif reg == rArcTimeHigh:
                            data = result[6:10]
                            return data

    def on_use_toggled(self):
        if self.usePanel.isChecked():
            if self.connected:
                self.connected = False
                if not self.write_to_register(rMode, '0000'):
                    return
                if not self.write_to_register(rCurrent, '0000'):
                    return
                if not self.write_to_register(rPressure, '0000'):
                    return
            self.clear_text()
            self.portName.setEnabled = True
        else:
            if self.currentSet.value() == 0:
                result = self.dialog_ok(QMessageBox.Warning,
                                        'Error',
                                        '\nA value is required for Current.\n')
                if result:
                    self.usePanel.setEnabled(True)
                    return
            self.portName.setEnabled = False
            mode = self.modeSet.currentIndex() + 1
            if not self.write_to_register(rMode, f'{mode:04x}'):
                return
            data = f'{int(self.currentSet.value() * 64):04X}'
            if not self.write_to_register(rCurrent, data):
                return
            data = f'{int(self.pressureSet.value() * 128):04X}'
            if not self.write_to_register(rPressure, data):
                return
            self.mode_changed()
            self.timer.start(100)
            self.connected = True

    def mode_changed(self):
        if not self.read_register(rCurrentMin):
            return
        if not self.read_register(rCurrentMax):
            return
        self.currentSet.setRange(int(float(self.currentMin.text())), int(float(self.currentMax.text())))
        if not self.read_register(rPressureMin):
            return
        if not self.read_register(rPressureMax):
            return
        if not self.read_register(rFault):
            return
        ArcTimeLow = self.read_register(rArcTimeLow)
        ArcTimeHigh = self.read_register(rArcTimeHigh)
        if ArcTimeLow and ArcTimeHigh:
            ArcTime = int((ArcTimeHigh + ArcTimeLow), 16)
            m, s = divmod(ArcTime, 60)
            h, m = divmod(m, 60)
            self.arctimeValue.setText(f'{h:.0f}:{m:02.0f}:{s:02.0f}')
        self.pressureSet.setRange((0), float(self.pressureMax.text()))
        self.minPressure = float(self.pressureMin.text())
        self.maxPressure = float(self.pressureMax.text())

    def on_port_scan(self):
        self.usePanel.setChecked(True)
        self.timer.stop()
        self.clear_text()
        try:
            self.openPort.close()
        except:
            pass
        self.portName.clear()
        self.portName.addItem('SELECT A PORT')
        for item in serial.tools.list_ports.comports():
            self.portName.addItem(item.device)
        self.portName.showPopup()
        self.usePanel.setEnabled(False)
        self.useComms.setEnabled(False)
        self.portName.setCurrentIndex(self.portName.count() - 1)
        self.on_port_changed()

    def on_port_changed(self):
        self.usePanel.setChecked(True)
        self.usePanel.setEnabled(False)
        self.useComms.setEnabled(False)
        if self.portName.currentText() == 'SELECT A PORT':
            return
        try:
            self.openPort.close()
        except:
            pass
        try:
            self.openPort = serial.Serial(
                    self.portName.currentText(),
                    baudrate=19200,
                    bytesize=8,
                    parity='E',
                    stopbits=1,
                    timeout=0.1
                    )
            print(f'\n{self.portName.currentText()} is open...\n')
        except:
            self.dialog_ok(QMessageBox.Warning,
                           'Error',
                           f'\nCould not open {self.portName.currentText()}\n')
            return
        self.usePanel.setEnabled(True)
        self.useComms.setEnabled(True)
        self.portFile = self.portName.currentText()

    def clear_text(self):
        self.modeValue.setText('')
        self.currentValue.setText('')
        self.pressureValue.setText('')
        self.faultValue.setText('')
        self.currentMin.setText('')
        self.pressureMin.setText('')
        self.faultLabel.setText('')
        self.faultName.setText('')
        self.currentMax.setText('')
        self.pressureMax.setText('')
        self.arctimeValue.setText('')
        self.modeSet.setCurrentIndex(0)
        self.currentSet.setValue(40)
        self.pressureSet.setValue(0)

    def dialog_ok(self, icon, title, text):
        response = QMessageBox()
        response.setIcon(icon)
        response.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))
        response.setWindowTitle(title)
        response.setText(text)
        response.exec_()
        return response

    def createGridLayout(self):
        self.grid = QGridLayout()
        for r in range(0, 8):
            self.grid.setRowMinimumHeight(r, 32)
        for c in range(0, 5):
            self.grid.setColumnMinimumWidth(c, 100)
        self.portScan = QPushButton('PORT SCAN')
        self.grid.addWidget(self.portScan, 0, 0)
        self.portName = QComboBox()
        self.portName.setStyleSheet('QComboBox {width: 200}')
        self.grid.addWidget(self.portName, 0, 2, 1, 2)
        self.usePanel = QRadioButton('PANEL')
        self.usePanel.setChecked(True)
        self.usePanel.setEnabled(False)
        self.grid.addWidget(self.usePanel, 0, 4)
        self.useComms = QRadioButton('RS485')
        self.useComms.setEnabled(False)
        self.grid.addWidget(self.useComms, 1, 4)
        self.minLabel = QLabel('MIN.')
        self.minLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.minLabel, 2, 1)
        self.maxLabel = QLabel('MAX.')
        self.maxLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.maxLabel, 2, 2)
        self.valueLabel = QLabel('VALUE')
        self.valueLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.valueLabel, 2, 3)
        self.setLabel = QLabel('SET TO')
        self.setLabel.setAlignment(Qt.AlignCenter | Qt.AlignVCenter)
        self.grid.addWidget(self.setLabel, 2, 4)
        self.modeLabel = QLabel('MODE')
        self.modeLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.modeLabel, 3, 0)
        self.currentLabel = QLabel('CURRENT')
        self.currentLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.currentLabel, 4, 0)
        self.pressureLabel = QLabel('PRESSURE')
        self.pressureLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.pressureLabel, 5, 0)
        self.arctimeLabel = QLabel('ARC ON TIME')
        self.arctimeLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.arctimeLabel, 6, 0)
        self.faultLabel = QLabel('ERROR')
        self.faultLabel.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.faultLabel, 7, 0)
        self.modeValue = QLabel('0')
        self.modeValue.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.modeValue, 3, 3)
        self.currentValue = QLabel('0')
        self.currentValue.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.currentValue, 4, 3)
        self.pressureValue = QLabel('0')
        self.pressureValue.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.pressureValue, 5, 3)
        self.arctimeValue = QLabel('0')
        self.arctimeValue.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.arctimeValue, 6, 3)
        self.faultValue = QLabel('0')
        self.faultValue.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.faultValue, 7, 1)
        self.currentMin = QLabel('0')
        self.currentMin.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.currentMin, 4, 1)
        self.pressureMin = QLabel('0')
        self.pressureMin.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.pressureMin, 5, 1)
        self.currentMax = QLabel('0')
        self.currentMax.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.currentMax, 4, 2)
        self.pressureMax = QLabel('0')
        self.pressureMax.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.pressureMax, 5, 2)
        self.faultName = QLabel('')
        self.faultName.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        self.grid.addWidget(self.faultName, 7, 2, 1, 3)
        self.modeSet = QComboBox()
        self.modeSet.addItems(['NORMAL', 'CPA', 'GOUGE'])
        self.modeSet.setCurrentIndex(0)
        self.grid.addWidget(self.modeSet, 3, 4)
        self.currentSet = QDoubleSpinBox()
        self.currentSet.setMaximum(125)
        self.currentSet.setWrapping(True)
        self.currentSet.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.currentSet, 4, 4)
        self.pressureSet = QDoubleSpinBox()
        self.pressureSet.setMaximum(125)
        self.pressureSet.setWrapping(True)
        self.pressureSet.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.grid.addWidget(self.pressureSet, 5, 4)
        self.clear_text()

    def shut_down(self):
        if self.connected:
            self.write_to_register(rMode, '0000')
            self.write_to_register(rCurrent, '0000')
            self.write_to_register(rPressure, '0000')


faultCode = {
             '0000': '',
             '0110': 'Remote controller mode invalid',
             '0111': 'Remote controller current invalid',
             '0112': 'Remote controller pressure invalid',
             '0120': 'Low input gas pressure',
             '0121': 'Output gas pressure low',
             '0122': 'Output gas pressure high',
             '0123': 'Output gas pressure unstable',
             '0130': 'AC input power unstable',
             '0199': 'Power board hardware protection',
             '0200': 'Low gas pressure',
             '0210': ('Gas flow lost while cutting', 'Excessive arc voltage'),
             '0220': 'No gas input',
             '0300': 'Torch stuck open',
             '0301': 'Torch stuck closed',
             '0320': 'End of consumable life',
             '0400': 'PFC/Boost IGBT module under temperature',
             '0401': 'PFC/Boost IGBT module over temperature',
             '0402': 'Inverter IGBT module under temperature',
             '0403': 'Inverter IGBT module over temperature',
             '0500': 'Retaining cap off',
             '0510': 'Start/trigger signal on at power up',
             '0520': 'Torch not connected',
             '0600': 'AC input voltage phase loss',
             '0601': 'AC input voltage too low',
             '0602': 'AC input voltage too high',
             '0610': 'AC input unstable',
             '0980': 'Internal communication failure',
             '0990': 'System hardware fault',
             '1000': 'Digital signal processor fault',
             '1100': 'A/D converter fault',
             '1200': 'I/O fault',
             '2000': 'A/D converter value out of range',
             '2010': 'Auxiliary switch disconnected',
             '2100': 'Inverter module temp sensor open',
             '2101': 'Inverter module temp sensor shorted',
             '2110': 'Pressure sensor is open',
             '2111': 'Pressure sensor is shorted',
             '2200': 'DSP does not recognize the torch',
             '3000': 'Bus voltage fault',
             '3100': 'Fan speed fault',
             '3101': 'Fan fault',
             '3110': 'PFC module temperature sensor open',
             '3111': 'PFC module temperature sensor shorted',
             '3112': 'PFC module temperature sensor circuit fault',
             '3200': 'Fill valve',
             '3201': 'Dump valve',
             '3201': 'Valve ID',
             '3203': 'Electronic regulator is disconnected',
             '3410': 'Drive fault',
             '3420': '5 or 24 VDC fault',
             '3421': '18 VDC fault',
             '3430': 'Inverter capacitors unbalanced',
             '3441': 'PFC over current',
             '3511': 'Inverter saturation fault',
             '3520': 'Inverter shoot-through fault',
             '3600': 'Power board fault',
             '3700': 'Internal serial communications fault',
            }

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = App()
    app.aboutToQuit.connect(ex.shut_down)
    sys.exit(app.exec_())
