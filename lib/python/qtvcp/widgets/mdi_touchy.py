#!/usr/bin/env python3
# Qtvcp versa probe
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# touchy style MDI based heavily from Touchy code

import os
import math

from PyQt5 import QtGui, QtCore, QtWidgets, uic

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

class mdi:
    def __init__(self):
        self.clear()

        self.axes = []
        self.polar = 0
        axisnames = ['X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W']

        try:
            STATUS.stat.poll()
            am = STATUS.stat.axis_mask
            for i in range(9):
               if am & (1<<i):
                   self.axes.append(axisnames[i])
        except:
            self.axes = ['X','Y','Z']

        self.gcode = 'M2'
        if INFO.MACHINE_IS_LATHE:
            G10 = ['Setup', 'L', 'P', 'A','R', 'I','J', 'Q']
        else:
            G10 = ['Setup', 'L', 'P', 'A', 'R']
        self.codes = {
            'M3' : ['Spindle CW', 'S'],
            'M4' : ['Spindle CCW', 'S'],
            'M6' : ['Tool change', 'T'],
            'M61' : ['Set tool number', 'Q'],
            'M66' : ['Input control', 'P', 'E', 'L', 'Q'],

            # 'A' means 'the axes'
            'G0' : ['Straight rapid', 'A'],
            'G00' : ['Straight rapid', 'A'],
            'G1' : ['Straight feed', 'A', 'F'],
            'G01' : ['Straight feed', 'A', 'F'],
            'G2' : ['Arc CW', 'A', 'I', 'J', 'K', 'R', 'P', 'F'],
            'G02' : ['Arc CW', 'A', 'I', 'J', 'K', 'R', 'P', 'F'],
            'G3' : ['Arc CCW', 'A', 'I', 'J', 'K', 'R', 'P', 'F'],
            'G03' : ['Arc CCW', 'A', 'I', 'J', 'K', 'R', 'P', 'F'],
            'G4' : ['Dwell', 'P'],
            'G04' : ['Dwell', 'P'],
            'G10': G10,
            'G33' : ['Spindle synchronized feed', 'A', 'K'],
            'G33.1' : ['Rigid tap', 'Z', 'K'],
            'G38.2' : ['Probe', 'A', 'F'],
            'G38.3' : ['Probe', 'A', 'F'],
            'G38.4' : ['Probe', 'A', 'F'],
            'G38.5' : ['Probe', 'A', 'F'],
            'G41' : ['Radius compensation left', 'D'],
            'G42' : ['Radius compensation right', 'D'],
            'G41.1' : ['Radius compensation left, immediate', 'D', 'L'],
            'G42.1' : ['Radius compensation right, immediate', 'D', 'L'],
            'G43' : ['Tool length offset', 'H'],
            'G43.1' : ['Tool length offset immediate', 'A'],
            'G43.2' : ['Tool length offset additional', 'H', 'A'],
            'G53' : ['Motion in unoffset coordinates', 'G', 'A', 'F'],
            'G64' : ['Continuous mode', 'P', 'Q'],
            'G76' : ['Thread', 'Z', 'P', 'I', 'J', 'K', 'R', 'Q', 'H', 'E', 'L'],
            'G81' : ['Drill', 'A', 'R', 'L', 'F'],
            'G82' : ['Drill with dwell', 'A', 'R', 'L', 'P', 'F'],
            'G83' : ['Peck drill', 'A', 'R', 'L', 'Q', 'F'],
            'G73' : ['Chip-break drill', 'A', 'R', 'L', 'Q', 'F'],
            'G85' : ['Bore', 'A', 'R', 'L', 'F'],
            'G89' : ['Bore with dwell', 'A', 'R', 'L', 'P', 'F'],
            'G92' : ['Offset all coordinate systems', 'A'],
            'G96' : ['CSS Mode', 'S', 'D'],
            }
        self.ocodes = []

    def add_macros(self, macros):
        for m in macros:
            words = m.split()
            call = "O<%s> call" % words[0]
            args = [''] + [w + ' ' for w in words[1:]]
            self.ocodes.append(call)
            self.codes[call] = args

    def get_description(self, gcode):
        return self.codes[gcode][0]
    
    def get_words(self, gcode):
        self.gcode = gcode
        try:
            if gcode[0] == 'M' and gcode.find(".") == -1 and int(gcode[1:]) >= 100 and int(gcode[1:]) <= 199:
                return ['P', 'Q']
        except IndexError:
            return []
        if gcode not in self.codes:
            return []
        # strip description
        words = self.codes[gcode][1:]
        # replace A with the real axis names
        if 'A' in words:
            i = words.index('A')
            words = words[:i] + self.axes + words[i+1:]
            if self.polar and 'X' in self.axes and 'Y' in self.axes:
                words[self.axes.index('X')] = '@'
                words[self.axes.index('Y')] = '^'
        return words

    def clear(self):
        self.words = {}

    def set_word(self, word, value):
        self.words[word] = value

    def set_polar(self, p):
        self.polar = p;

    def issue(self):
        m = self.gcode
        if m.lower().startswith('o'):
            codes = self.codes[m]
            for code in self.codes[m][1:]:
                v = self.words[code] or "0"
                m = m + " [%s]" % v
        else:
            w = [i for i in self.words if len(self.words.get(i)) > 0]
            if '@' in w:
                m += '@' + self.words.get('@')
                w.remove('@')
            if '^' in w:
                m += '^' + self.words.get('^')
                w.remove('^')
            for i in w:
                if len(self.words.get(i)) > 0:
                    m += i + self.words.get(i)
        ACTION.CALL_MDI(m)
        try:
            fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
            fp = open(fp, 'a')
            fp.write(m + "\n")
            fp.close()
        except:
            pass
        STATUS.emit('mdi-history-changed')

class MDITouchy(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MDITouchy, self).__init__(parent)
        self.setMinimumSize(265, 325)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'mdi_touchy.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self.mdi = mdi()
        self.numlabels = 11
        self.numwords = 1
        self.selected = 0
        self.plaintext = ['','','','','','','','','','','','']
        self.dColor = self.label_0.palette().button().color()
        self.dialog_code = 'CALCULATOR'

    def _hal_init(self):
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('general',self.return_value)
        macros = INFO.INI_MACROS
        if len(macros) > 0:
            self.mdi.add_macros(macros)
        else:
            self.pushButton_macro.setEnabled(0)

    def gxClicked(self):
        self.update("G")

    def grClicked(self):
        self.update("G", 1)

    def mClicked(self):
        self.update("M")

    def tClicked(self):
        self.update("T")

    def pointClicked(self):
        t = self.get_text()
        if t.find(".") == -1:
            self.set_text(t + ".")

    def minusClicked(self):
        t = self.get_text()
        if self.selected > 0:
            head = t.rstrip("0123456789.-")
            tail = t[len(head):]
            if tail.find("-") == -1:
                self.set_text(head + "-" + tail)
            else:
                self.set_text(head + tail[1:])

    def nextClicked(self):
        self.fill_out();
        if self.numwords > 0:
            self.editing(max(1,(self.selected+1) % (self.numwords+1)))

    def pointClicked(self):
        t = self.get_text()
        if t.find(".") == -1:
            self.set_text(t + ".")

    def clearClicked(self):
        t = self.get_text()
        self.set_text(t.rstrip("0123456789.-"))
        
    def backClicked(self):
        t = self.get_text()
        if t[-1:] in "0123456789.-":
            self.set_text(t[:-1])

    def numKeyClicked(self):
        t = self.get_text()
        sending_button = self.sender()
        num = sending_button.objectName().strip('pushButton_')
        self.set_text(t + num)

    def setToolClicked(self):
        tool = STATUS.stat.tool_in_spindle
        self.set_tool(tool,True)

    def setOriginClicked(self):
        self.set_origin()

    def macroClicked(self):
        self.cycle_ocodes()

    def calcClicked(self):
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'MDI Calculator'}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    #######################################
    #
    #######################################

    # process the STATUS return message
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            t = self.get_text().rstrip("0123456789.-")
            self.set_text('{}{}'.format(t, num))
            STATUS.emit('update-machine-log', 'Calculation from MDI {}:'.format(num), 'TIME')

    def run_command(self):
        self.fill_out()
        self.mdi.issue()

    def update(self, code="G", polar=0):
        self.set_text('G', self.selected)
        self.mdi.set_polar(polar)
        self.set_text(code, 0)
        for i in range(1, self.numlabels):
            self.set_text("", i)
        self.editing(0)
        self.mdi.clear()

    def fill_out(self):
        if self.selected == 0:
            w = self.mdi.get_words(self.get_text())
            self.numwords = len(w)
            for i in range(1,self.numlabels):
                if i <= len(w):
                    self.set_text(w[i-1], i)
                else:
                    self.set_text("", i)

    def not_editing(self, n):
        #self['label_{}'.format(n)].setEnabled(False)
        self['label_{}'.format(n)].setStyleSheet("background-color:();".format( self.dColor.name()))

    def editing(self, n):
        self.not_editing(self.selected)
        self.selected = n
        #self['label_{}'.format(n)].setEnabled(True)
        self['label_{}'.format(n)].setStyleSheet("background-color:#ffffff;")

    def get_text(self):
        return self.plaintext[self.selected]

    def set_text(self, t, n = -1):
        if n == -1: n = self.selected
        w = self['label_{}'.format(n)]
        w.setText('<b>{}</b>'.format(t))
        self.plaintext[n] = t
        if n > 0:
            head = t.rstrip("0123456789.-")
            tail = t[len(head):]
            self.mdi.set_word(head, tail)
        if len(t) < 2:
            w.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        else:
            w.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)

    def cycle_ocodes(self):
        # strip off richText bold encoding
        doc = self.label_0.text().lstrip('<b>')
        old_code = doc.rstrip('</b>')
        ocodes = self.mdi.ocodes
        if old_code in ocodes:
            j = (ocodes.index(old_code) + 1) % len(ocodes)
        else:
            j = 0
        self.update(ocodes[j])
        self.nextClicked()            

    def set_tool(self, tool, g10l11):
        self.update()
        self.set_text("G10", 0)
        self.nextClicked()
        if g10l11:
            self.set_text("L11", 1)
        else:
            self.set_text("L10", 1)
        self.nextClicked()
        self.set_text("P%d" % tool, 2)
        self.nextClicked() # go to first axis
        if ('X' in self.mdi.axes and
            'Y' in self.mdi.axes and
            'Z' in self.mdi.axes):
            # this is fairly mill-like, so go to Z
            self.nextClicked()
            self.nextClicked()

    def set_origin(self, system=0):
        self.update()
        self.set_text("G10", 0)
        self.nextClicked()
        self.set_text("L20", 1)
        self.nextClicked()
        self.set_text("P%d" % system, 2)
        self.nextClicked()
        if ('X' in self.mdi.axes and
            'Z' in self.mdi.axes and
            not 'Y' in self.mdi.axes):
            # this is fairly lathe-like, so go to Z
            self.nextClicked()

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

####################################
# Testing
####################################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    import sys

    app = QtWidgets.QApplication(sys.argv)
    w = MDITouchy()
    w.show()
    sys.exit( app.exec_() )

