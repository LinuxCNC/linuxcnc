#!/usr/bin/env python2

'''
pmx_test.py

Copyright (C) 2019 2020 Phillip A Carter

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

import gtk
import gobject
import time

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
validRead    = '0402'

while 1:
    try:
        import serial
        import serial.tools.list_ports
        break
    except:
        msg = '\npyserial module not available\n'\
              '\nto install, open a terminal and enter:\n'\
              '\nsudo apt-get install python-serial\n'
        dialog = gtk.Dialog('ERROR',
                            None,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            ('OK', 1)
                           )
        label = gtk.Label(msg)
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        raise SystemExit

class pmx(gtk.Window):

    def __init__(self):
        super(pmx, self).__init__()
        self.create_widgets()
        self.show_all()
        self.model = self.portName.get_model()
        self.portName.set_model(None)
        self.model.clear()
        for item in serial.tools.list_ports.comports():
            self.model.append([item.device])
        self.portName.set_model(self.model)
        self.set_title('Powermax Communicator')
        self.set_position(gtk.WIN_POS_CENTER)
        self.connect('delete_event', self.on_window_delete_event)
        self.portName.connect('changed', self.on_port_changed)
        self.portScan.connect('pressed', self.on_port_scan)
        self.modeSet.connect('changed', self.on_value_changed, rMode, 1)
        self.currentAdj.connect('value_changed', self.on_value_changed, rCurrent, 64)
        self.pressureAdj.connect('value_changed', self.on_value_changed, rPressure, 128)
        self.usePanel.connect('toggled', self.on_use_toggled)
        self.writing = False
        self.connected = False
        gobject.timeout_add(250, self.periodic)

    def periodic(self):
        if self.connected:
            for reg in (rMode, rCurrent, rPressure, rFault):
                if not self.read_register(reg): return True
        return True

    def on_value_changed(self, widget, reg, multiplier):
        if not self.connected: return
        if reg == rMode:
            mode = self.modeSet.get_active() + 1
            self.pressureSet.set_value(0)
            if not self.write_to_register(rMode, '{:04x}'.format(mode)): return
            self.mode_changed()
            return
        elif reg == rPressure:
            if self.pressureType == 'bar':
                if widget.get_value() == 0.1:
                    widget.set_value(self.minPressure)
                elif widget.get_value() == self.minPressure - 0.1:
                    widget.set_value(0)
            else:
                if widget.get_value() == 1:
                    widget.set_value(self.minPressure)
                elif widget.get_value() == self.minPressure - 1:
                    widget.set_value(0)
            data = ('{:04X}'.format(int(widget.get_value() * multiplier))).upper()
        elif reg == rCurrent:
            data = ('{:04X}'.format(int(widget.get_value() * multiplier))).upper()
        self.write_to_register(reg , data)

    def get_lrc(self, data):
        lrc = 0
        for i in xrange(0, len(data), 2):
            a, b = data[i:i+2]
            try:
                lrc = (lrc + int(a + b, 16)) & 255
            except:
                print('broken packet in get_lrc')
                return '00'
        lrc = ('{:02X}'.format((((lrc ^ 255) + 1) & 255))).upper()
        return lrc

    def write_to_register(self, reg, data):
        self.writing = True
        data = '{}{}{}{}'.format(address, regWrite, reg, data)
        lrc = self.get_lrc(data)
        packet = ':{}{}\r\n'.format(data, lrc)
        reply = ''
        self.openPort.write(packet)
        reply = self.openPort.readline()
        if not reply:
            self.usePanel.set_active(True)
            result = self.dialog_ok(
                        'ERROR',\
                        '\nno reply while writing to plasma unit\n'\
                        '\ncheck connections and retry when ready\n')
            return False
        elif reply == packet:
            self.writing = False
        else:
            result = self.dialog_ok(
                        'ERROR',\
                        '\nbad packet while writing to plasma unit\n'\
                        '\ncheck connections and retry when ready\n')
            return False
        return True

    def read_from_register(self, reg):
        data = '{}{}{}0001'.format(address, regRead, reg)
        lrc =self.get_lrc(data)
        packet = ':{}{}\r\n'.format(data, lrc)
        reply = ''
        self.openPort.write(packet)
        reply = self.openPort.readline()
        if reply:
            return reply
        else:
            self.usePanel.set_active(True)
            result = self.dialog_ok('ERROR',\
                        '\nno reply while reading from plasma unit\n'\
                        '\ncheck connections and retry when ready\n')
            return None

    def read_register(self, reg):
        result = self.read_from_register(reg)
        if result:
            if result >= 0:
                if result[:7] == ':{}{}'.format(address, validRead):
                    lrc = self.get_lrc(result[1:11])
                    if lrc == result[11:13]:
                        if reg == rMode:
                            data = int(result[7:11])
                            #if not self.first:
                            self.modeValue.set_text(str(data))
                            return data
                        elif reg == rCurrent:
                            data = float(int(result[7:11], 16) / 64.0)
                            #if not self.first:
                            self.currentValue.set_text('{:.0f}'.format(data))
                            return data
                        elif reg == rPressure:
                            data = float(int(result[7:11], 16) / 128.0)
                            if self.pressureType == 'bar':
                                self.pressureValue.set_text('{:.1f}'.format(data))
                            else:
                                self.pressureValue.set_text('{:.0f}'.format(data))
                            return data
                        elif reg == rFault:
                            fault = int(result[7:11], 16)
                            code = '{:04d}'.format(fault)
                            if fault > 0:
                                self.faultLabel.set_text('Fault')
                                self.faultValue.set_text('{}-{}-{}'.format(code[0], code[1:3], code[3]))
                            else:
                                self.faultLabel.set_text('')
                                self.faultValue.set_text('')
                            if fault == 210:
                                if float(self.currentMax.get_text()) >110: 
                                    self.faultName.set_text('{}'.format(faultCode[code][1]))
                                else:
                                    self.faultName.set_text('{}'.format(faultCode[code][1]))
                            else:
                                try:
                                    self.faultName.set_text('{}'.format(faultCode[code]))
                                except:
                                    self.faultName.set_text('Unkown fault code')
                            return code
                        elif reg == rCurrentMin:
                            data = float(int(result[7:11], 16) / 64.0)
                            self.currentMin.set_text('{:.0f}'.format(data))
                            return data
                        elif reg == rCurrentMax:
                            data = float(int(result[7:11], 16) / 64.0)
                            self.currentMax.set_text('{:.0f}'.format(data))
                            return data
                        elif reg == rPressureMin:
                            data = float(int(result[7:11], 16) / 128.0)
                            self.minimumPressure = data
                            if data < 15:
                                self.pressureType = 'bar'
                                self.pressureMin.set_text('{:.1f}'.format(data))
                                self.pressureAdj.set_step_increment(0.1)
                                self.pressureSet.set_digits(1)
                            else:
                                self.pressureType = 'psi'
                                self.pressureMin.set_text('{:.0f}'.format(data))
                                self.pressureAdj.set_step_increment(1)
                                self.pressureSet.set_digits(0)
                            return data
                        elif reg == rPressureMax:
                            data = float(int(result[7:11], 16) / 128.0)
                            if self.pressureType == 'bar':
                                self.pressureMax.set_text('{:.1f}'.format(data))
                                self.pressureAdj.set_upper(data)
                            else:
                                self.pressureMax.set_text('{:.0f}'.format(data))
                                self.pressureAdj.set_upper(data)
                            return data

    def on_use_toggled(self,button):
        if self.usePanel.get_active():
            if self.connected:
                self.connected = False
                if not self.write_to_register(rMode, '0000'): return
                if not self.write_to_register(rCurrent, '0000'): return
                if not self.write_to_register(rPressure, '0000'): return
            self.clear_text()
            self.portName.set_sensitive = True
        else:
            if self.currentSet.get_value() == 0:
                result = self.dialog_ok(
                        'ERROR',\
                        '\nA value is required for Current\n')
                if result:
                    self.usePanel.set_active(True)
                    return
            self.portName.set_sensitive = False
            mode = self.modeSet.get_active() + 1
            if not self.write_to_register(rMode, '{:04x}'.format(mode)): return
            data = '{:04X}'.format(int(self.currentSet.get_value() * 64))
            if not self.write_to_register(rCurrent, data): return
            data = '{:04X}'.format(int(self.pressureSet.get_value() * 128))
            if not self.write_to_register(rPressure, data): return
            self.mode_changed()
            self.connected = True

    def mode_changed(self):
        if not self.read_register(rCurrentMin): return
        if not self.read_register(rCurrentMax): return
        self.currentSet.set_range(int(float(self.currentMin.get_text())),int(float(self.currentMax.get_text())))
        if not self.read_register(rPressureMin): return
        if not self.read_register(rPressureMax): return
        self.pressureSet.set_range((0),float(self.pressureMax.get_text()))
        self.minPressure = float(self.pressureMin.get_text())
        self.maxPressure = float(self.pressureMax.get_text())

    def on_port_scan(self,widget):
        try:
            self.openPort.close()
        except:
            pass
        self.model.clear()
        for item in serial.tools.list_ports.comports():
            self.model.append([item.device])
        self.portName.popup()
        self.usePanel.set_sensitive(False)
        self.useComms.set_sensitive(False)

    def on_port_changed(self,widget):
        self.usePanel.set_active(True)
        self.usePanel.set_sensitive(False)
        self.useComms.set_sensitive(False)
        try:
            self.openPort.close()
        except:
            pass
        comPort = (widget.get_active_text())
        try:
            self.openPort = serial.Serial(
                    comPort,
                    baudrate = 19200,
                    bytesize = 8,
                    parity = 'E',
                    stopbits = 1,
                    timeout = 0.1
                    )
        except:
            result = self.dialog_ok(
                    'ERROR',\
                    '\ncould not open {}\n'.format(comPort))
            return
        self.usePanel.set_sensitive(True)
        self.useComms.set_sensitive(True)

    def clear_text(self):
        self.modeValue.set_text('')
        self.currentValue.set_text('')
        self.pressureValue.set_text('')
        self.faultValue.set_text('')
        self.currentMin.set_text('')
        self.pressureMin.set_text('')
        self.faultLabel.set_text('')
        self.faultName.set_text('')
        self.currentMax.set_text('')
        self.pressureMax.set_text('')
        self.modeSet.set_active(0)
        self.currentSet.set_value(41)
        self.pressureSet.set_value(0)

    def dialog_ok(self,title,text):
        dialog = gtk.Dialog(title,
                            self,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            ('OK', 1)
                           )
        label = gtk.Label(text)
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        return response

    def dialog_ok_cancel(self,title,text,name1,name2):
        dialog = gtk.Dialog(title,
                            self,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            (name1, 1,
                            name2, 0)
                           )
        label = gtk.Label(text)
        dialog.vbox.add(label)
        label.show()
        response = dialog.run()
        dialog.destroy()
        return response

    def create_widgets(self):
        self.T = gtk.Table(7, 5)
        self.T.set_homogeneous(True)
        self.add(self.T)
        self.portScan = gtk.Button('Port Scan')
        self.T.attach(self.portScan, 0, 1, 0, 1)
        self.portLabel = gtk.Label('Port Select:')
        self.portLabel.set_alignment(0.9, 0.5)
        self.T.attach(self.portLabel, 1, 2, 0, 1)
        self.portName = gtk.combo_box_new_text()
        self.T.attach(self.portName, 2, 4, 0, 1)
        self.usePanel = gtk.RadioButton(None, 'Panel')
        self.usePanel.set_sensitive(False)
        self.T.attach(self.usePanel, 4, 5, 0, 1)
        self.useComms = gtk.RadioButton(self.usePanel, 'RS485')
        self.useComms.set_sensitive(False)
        self.T.attach(self.useComms, 4, 5, 1, 2)
        self.minLabel = gtk.Label('Min.')
        self.minLabel.set_alignment(1, 0.5)
        self.T.attach(self.minLabel, 1, 2, 2, 3)
        self.maxLabel = gtk.Label('Max.')
        self.maxLabel.set_alignment(1, 0.5)
        self.T.attach(self.maxLabel, 2, 3, 2, 3)
        self.valueLabel = gtk.Label('Value')
        self.valueLabel.set_alignment(0.9, 0.5)
        self.T.attach(self.valueLabel, 3, 4, 2, 3)
        self.setLabel = gtk.Label('Set To')
        self.setLabel.set_alignment(0.9, 0.5)
        self.T.attach(self.setLabel, 4, 5, 2, 3)
        self.modeLabel = gtk.Label('Mode')
        self.modeLabel.set_alignment(1, 0.5)
        self.T.attach(self.modeLabel, 0, 1, 3, 4)
        self.currentLabel = gtk.Label('Current')
        self.currentLabel.set_alignment(1, 0.5)
        self.T.attach(self.currentLabel, 0, 1, 4, 5)
        self.pressureLabel = gtk.Label('Pressure')
        self.pressureLabel.set_alignment(1, 0.5)
        self.T.attach(self.pressureLabel, 0, 1, 5, 6)
        self.faultLabel = gtk.Label('')
        self.faultLabel.set_alignment(1, 0.5)
        self.T.attach(self.faultLabel, 0, 1, 6, 7)
        self.modeValue = gtk.Label('0')
        self.modeValue.set_alignment(0.9, 0.5)
        self.T.attach(self.modeValue, 3, 4, 3, 4)
        self.currentValue = gtk.Label('0')
        self.currentValue.set_alignment(0.9, 0.5)
        self.T.attach(self.currentValue, 3, 4, 4, 5)
        self.pressureValue = gtk.Label('0')
        self.pressureValue.set_alignment(0.9, 0.5)
        self.T.attach(self.pressureValue, 3, 4, 5, 6)
        self.faultValue = gtk.Label('')
        self.faultValue.set_alignment(1, 0.5)
        self.T.attach(self.faultValue, 1, 2, 6, 7)
        self.currentMin = gtk.Label('0')
        self.currentMin.set_alignment(1, 0.5)
        self.T.attach(self.currentMin, 1, 2, 4, 5)
        self.pressureMin = gtk.Label('0')
        self.pressureMin.set_alignment(1, 0.5)
        self.T.attach(self.pressureMin, 1, 2, 5, 6)
        self.faultName = gtk.Label('')
        self.T.attach(self.faultName, 2, 5, 6, 7)
        self.currentMax = gtk.Label('0')
        self.currentMax.set_alignment(1, 0.5)
        self.T.attach(self.currentMax, 2, 3, 4, 5)
        self.pressureMax = gtk.Label('0')
        self.pressureMax.set_alignment(1, 0.5)
        self.T.attach(self.pressureMax, 2, 3, 5, 6)
        self.modeSet = gtk.combo_box_new_text()
        self.modeSet.append_text('Normal')
        self.modeSet.append_text('CPA')
        self.modeSet.append_text('Gouge')
        self.modeSet.set_active(0)
        self.T.attach(self.modeSet, 4, 5, 3, 4)
        self.currentAdj = gtk.Adjustment(0, 0, 125, 1, 10, 0.0)
        self.currentSet = gtk.SpinButton(self.currentAdj, 0, 0)
        self.currentSet.set_wrap(True)
        self.T.attach(self.currentSet, 4, 5, 4, 5)
        self.pressureAdj = gtk.Adjustment(0, 0, 100, 0.1, 5.0, 0.0)
        self.pressureSet = gtk.SpinButton(self.pressureAdj, 0, 1)
        self.pressureSet.set_wrap(True)
        self.T.attach(self.pressureSet, 4, 5, 5, 6)
        self.clear_text()

    def on_window_delete_event(self,window,event):
        try:
            self.openPort.close()
        except:
            pass
        gtk.main_quit()

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
    try:
        a = pmx()
        gtk.main()
    except KeyboardInterrupt:
        pass
