#!/usr/bin/env python

'''
w_array.py

Copyright (C) 2019, 2020  Phillip A Carter

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
import gtk
import time
import linuxcnc
import shutil
import hal
from subprocess import Popen,PIPE

class array:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        self.fNgc = ''
        self.fCode = ''
        self.previewed = False
        self.data_changed = True
        
    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR, 
                               gtk.BUTTONS_CLOSE,
                               error)
        md.run()
        md.destroy()

    def load_file(self, fName):
        if self.gui == 'axis':
            Popen('axis-remote {}'.format(fName), stdout = PIPE, shell = True)
        elif self.gui == 'gmoccapy':
            if fName == '{}/wizard.ngc'.format(self.tmpDir):
                self.c.program_open(fName)
            else:
                if self.previewed:
                    os.remove(self.outFile)
                self.outFile = '{}/array_{}.ngc'.format(self.tmpDir, time.time())
                shutil.copyfile(fName, self.outFile)
                self.c.program_open('blank.ngc')
                self.c.program_open(self.outFile)
        else:
            print('Unknown GUI in .ini file')

    def cancel(self, widget):
        if self.shapeMode:
            self.load_file(self.fOriginal)
        elif self.previewed:
            if self.gui == 'axis':
                self.load_file(self.fOriginal)
            elif self.gui == 'gmoccapy':
                self.c.program_open(self.fOriginal)
                os.remove(self.outFile)
        self.previewed = False
        self.W.destroy()
        return None

    def accept_array(self, widget):
        if not self.previewed or self.data_changed:
            self.dialog_error('Preview is required')
            return None
        elif os.path.exists(self.fCode):
            if self.gui == 'gmoccapy':
                self.c.program_open(self.fNgc)
                os.remove(self.outFile)
        elif os.path.exists(self.fOriginal) and self.fNgc:
            shutil.copyfile(self.fNgc, self.fOriginal)
            self.load_file(self.fOriginal)
        else:
            self.dialog_error('Cannot find required files')
        self.previewed = False
        self.W.destroy()
        for fName in ['original.ngc', 'shape.ngc', 'shape.tmp', 'wizard.ngc']:
            if os.path.exists('{}/{}'.format(self.tmpDir, fName)):
                os.remove('{}/{}'.format(self.tmpDir, fName))
        return None
    def preview_array(self, event):
        if self.xCEntry.get_text():
            columns = int(self.xCEntry.get_text())
        else:
            columns = 0
        if self.yCEntry.get_text():
            rows = int(self.yCEntry.get_text())
        else:
            rows = 0
        if self.xOEntry.get_text():
            xOffset = float(self.xOEntry.get_text())
        else:
            xOffset = 0
        if self.yOEntry.get_text():
            yOffset = float(self.yOEntry.get_text())
        else:
            yOffset = 0
        if columns > 0 and rows > 0 and (columns == 1 or (columns > 1 and xOffset <> 0)) and (rows == 1 or (rows > 1 and yOffset <> 0)):
            self.fCode = '{}/array_code.ngc'.format(self.tmpDir)
            if self.shapeMode:
                fPre = '{}/array_preamble.ngc'.format(self.tmpDir)
                fPst = '{}/array_postamble.ngc'.format(self.tmpDir)
                outCod = open(self.fCode, 'w')
                outPre = open(fPre, 'w')
                outPst = open(fPst, 'w')
                inWiz = open(self.fOriginal, 'r')
                while(1):
                    d = inWiz.readline()
                    if d.startswith('(wizard'):
                        outCod.write(d)
                        break
                    outPre.write(d)
                while(1):
                    d = inWiz.readline()
                    if d.startswith('(postamble'):
                        outPst.write(d)
                        break
                    outCod.write(d)
                while(1):
                    d = inWiz.readline()
                    if not d: break
                    outPst.write(d)
                outCod.close()
                outPre.close()
                outPst.close()
                inWiz.close()
                self.fNgc = '{}/array.ngc'.format(self.tmpDir)
                outNgc = open(self.fNgc, 'w')
                inPre = open(fPre, 'r')
                for line in inPre:
                    outNgc.write(line)
                inPre.close()
                for row in range(rows):
                    for column in range(columns):
                        outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                        inCod = open(self.fCode, 'r')
                        for line in inCod:
                            raw = line.strip().lower()
                            if raw.startswith('g0') or raw.startswith('g1') or raw.startswith('g2') or raw.startswith('g3'):
                                a, b = raw.split('x')
                                c, d = b.split('y')
                                if ('i') in d:
                                    e, f = d.split('i')
                                    f = 'i' + f
                                else:
                                    e = d
                                    f = ''
                                outNgc.write('{}x{} y{} {}\n'.format(a, float(c) + column * xOffset, float(e) + row * yOffset, f))
                            else:
                                outNgc.write(line)
                        inCod.close()
                inPst = open(fPst, 'r')
                for line in inPst:
                    outNgc.write(line)
                inPst.close()
                outNgc.close()
                self.load_file(self.fNgc)
            else:
                self.s.poll()
                mUnits = self.s.linear_units
                if self.previewed:
                    md = gtk.MessageDialog(self.W, 
                                           gtk.DIALOG_DESTROY_WITH_PARENT,
                                           gtk.MESSAGE_INFO, 
                                           gtk.BUTTONS_NONE,
                                           'ARRAY\n\nCalculating.....')
                    md.set_keep_above(True)
                    md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
                    md.set_default_size(200, 100)
                    md.show_all()
                    self.c.program_open(self.fOriginal)
                    watchdog = time.time() + 5
                    while(1):
                        self.s.poll()
                        if os.path.basename(self.s.file) == os.path.basename(self.fOriginal):
                            n = time.time() + 1
                            while(1):
                                if time.time() > n:
                                    break
                                while gtk.events_pending():
                                    gtk.main_iteration()
                            break
                        if time.time() > watchdog:
                            md.destroy()
                            self.dialog_error('Array file error')
                            return
                shutil.copyfile(self.s.file, self.fCode)
                inCod = open(self.fCode, 'r')
                units = 1
                for line in inCod:
                    if 'G21' in line.upper().replace(' ', '') and mUnits != 1:
                        units = 25.4
                        break
                    if 'G20' in line.upper().replace(' ', '') and mUnits == 1:
                        units = 0.03937
                        break
                self.fNgc = '{}/array.ngc'.format(self.tmpDir)
                outNgc = open(self.fNgc, 'w')
                xPos = (self.s.g5x_offset[0] - self.s.g92_offset[0]) * units
                yPos = (self.s.g5x_offset[1] - self.s.g92_offset[1]) * units
                for row in range(rows):
                    for column in range(columns):
                        outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                        outNgc.write('G10 L2 P0 X{} Y{}\n'.format(xPos + column * xOffset * units, yPos + row * yOffset * units))
                        inCod = open(self.fCode, 'r')
                        for line in inCod:
                            a = b = c = ''
                            a = line.upper().replace(' ', '')
                            if 'M2' in a or 'M30' in a:
                                b = a.replace('M2', '')
                                c = b.replace('M30', '')
                                outNgc.write(c)
                            else:
                                outNgc.write(line)
                        inCod.close()
                outNgc.write('G10 L2 P0 X{} Y{}\n'.format(xPos, yPos))
                outNgc.write('M30\n')
                outNgc.close()
                self.load_file(self.fNgc)
                if self.previewed:
                    md.destroy()
            hal.set_p('plasmac_run.preview-tab', '1')
        else:
            msg = ''
            if columns <= 0:
                msg += 'Columns are required\n\n'
            if rows <= 0:
                msg += 'Rows are required\n\n'
            if xOffset == 0 and columns > 1:
                msg += 'Column Offset is required\n\n'
            if yOffset == 0 and rows > 1:
                msg += 'Row Offset is required'
            self.dialog_error(msg)
            return
        self.previewed = True
        self.data_changed = False

    def data_change(self, widget):
        self.data_changed = True

    def do_array(self, fWizard, tmpDir, mode):
        self.tmpDir = tmpDir
        original = '{}/original.ngc'.format(tmpDir)
        shutil.copyfile(fWizard, original)
        self.fOriginal = original
        self.shapeMode = mode
        self.W = gtk.Dialog('Array',
                            None,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        t.set_row_spacings(6)
        self.W.vbox.add(t)
        xCLabel = gtk.Label('Columns')
        xCLabel.set_alignment(0.95, 0.5)
        xCLabel.set_width_chars(10)
        t.attach(xCLabel, 0, 1, 0, 1)
        self.xCEntry = gtk.Entry()
        self.xCEntry.set_width_chars(10)
        self.xCEntry.connect('changed', self.data_change)
        t.attach(self.xCEntry, 1, 2, 0, 1)
        xOLabel = gtk.Label('Column\nOffset')
        xOLabel.set_alignment(0.95, 0.5)
        xOLabel.set_justify(gtk.JUSTIFY_RIGHT)
        xOLabel.set_width_chars(10)
        t.attach(xOLabel, 2, 3, 0, 1)
        self.xOEntry = gtk.Entry()
        self.xOEntry.set_width_chars(10)
        self.xOEntry.connect('changed', self.data_change)
        t.attach(self.xOEntry, 3, 4, 0, 1)
        yCLabel = gtk.Label('Rows')
        yCLabel.set_alignment(0.95, 0.5)
        yCLabel.set_width_chars(10)
        t.attach(yCLabel, 0, 1, 1, 2)
        self.yCEntry = gtk.Entry()
        self.yCEntry.set_width_chars(10)
        self.yCEntry.set_text('1')
        self.yCEntry.connect('changed', self.data_change)
        t.attach(self.yCEntry, 1, 2, 1, 2)
        yOLabel = gtk.Label('Row\nOffset')
        yOLabel.set_alignment(0.95, 0.5)
        yOLabel.set_justify(gtk.JUSTIFY_RIGHT)
        yOLabel.set_width_chars(10)
        t.attach(yOLabel, 2, 3, 1, 2)
        self.yOEntry = gtk.Entry()
        self.yOEntry.set_width_chars(10)
        self.xOEntry.connect('changed', self.data_change)
        t.attach(self.yOEntry, 3, 4, 1, 2)
        self.preview = gtk.Button('Preview')
        self.preview.connect('pressed', self.preview_array)
        t.attach(self.preview, 1, 2, 3, 4)
        accept = gtk.Button('Accept')
        accept.connect('pressed', self.accept_array)
        t.attach(accept, 2, 3, 3, 4)
        cancel = gtk.Button('Cancel')
        cancel.connect('pressed', self.cancel)
        t.attach(cancel, 3, 4, 3, 4)
        self.xCEntry.grab_focus()
        self.W.show_all()
        response = self.W.run()
