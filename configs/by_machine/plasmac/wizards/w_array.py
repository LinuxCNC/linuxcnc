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

class array_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        self.previewed = False

    def array_cancel(self, widget):
        shutil.copyfile(self.parent.fNgcBkp, self.parent.fNgc)
        if widget is not None:
            self.parent.preview.load(self.parent.fNgc)
        self.previewed = False

    def array_accept(self, widget):
        shutil.copyfile(self.parent.fNgc, self.parent.fNgcBkp)
        self.parent.preview.load(self.parent.fNgc)

    def array_preview(self, event):
        try:
            columns = int(self.xCEntry.get_text())
        except:
            columns = 1
        try:
            rows = int(self.yCEntry.get_text())
        except:
            rows = 1
        try:
            xOffset = float(self.xOEntry.get_text())
        except:
            xOffset = 0
        try:
            yOffset = float(self.yOEntry.get_text())
        except:
            yOffset = 0
        try:
            xOrgOffset = float(self.xOrgEntry.get_text())
        except:
            xOrgOffset = 0
        try:
            yOrgOffset = float(self.yOrgEntry.get_text())
        except:
            yOrgOffset = 0
        if columns > 0 and rows > 0 and (columns == 1 or (columns > 1 and xOffset <> 0)) and (rows == 1 or (rows > 1 and yOffset <> 0)):
            self.array_cancel(None)
            if self.parent.arrayMode == 'wizard':
                fPre = []
                fPst = []
                outCod = open(self.parent.fTmp, 'w')
                inWiz = open(self.parent.fNgc, 'r')
                while(1):
                    d = inWiz.readline()
                    if d.startswith('(wizard'):
                        outCod.write(d)
                        break
                    fPre.append(d)
                while(1):
                    d = inWiz.readline()
                    if '(postamble' in d:
                        fPst.append(d)
                        break
                    outCod.write(d)
                while(1):
                    d = inWiz.readline()
                    if not d: break
                    fPst.append(d)
                outCod.close()
                inWiz.close()
                outNgc = open(self.parent.fNgc, 'w')
                for line in fPre:
                    outNgc.write(line)
                for row in range(rows):
                    for column in range(columns):
                        outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                        inCod = open(self.parent.fTmp, 'r')
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
                                outNgc.write('{}x{} y{} {}\n'.format \
                                    (a, (float(c) + column * xOffset) + xOrgOffset, (float(e) + row * yOffset) + yOrgOffset, f))
                            else:
                                outNgc.write(line)
                        inCod.close()
                for line in fPst:
                    outNgc.write(line)
                outNgc.close()
                self.parent.preview.load(self.parent.fNgc)
            else:
                self.s.poll()
                mUnits = self.s.linear_units
                shutil.copyfile(self.parent.fNgc, self.parent.fTmp)
                inCod = open(self.parent.fTmp, 'r')
                units = 1
                for line in inCod:
                    if 'G21' in line.upper().replace(' ', '') and mUnits != 1:
                        units = 25.4
                        break
                    if 'G20' in line.upper().replace(' ', '') and mUnits == 1:
                        units = 0.03937
                        break
                outNgc = open(self.parent.fNgc, 'w')
                xIndex = [5221,5241,5261,5281,5301,5321,5341,5361,5381][0]
                outNgc.write('#<ucs_x_offset> = #{}\n'.format(xIndex))
                outNgc.write('#<ucs_y_offset> = #{}\n'.format(xIndex + 1))
                for row in range(rows):
                    for column in range(columns):
                        outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                        if self.parent.arrayMode == 'external':
                            outNgc.write('G10 L2 P0 X[{} + #<ucs_x_offset>] Y[{} + #<ucs_y_offset>]\n'.format\
                            ((column * xOffset * units) + xOrgOffset, (row * yOffset * units) + yOrgOffset))
                        else:
                            # check this down the track for arraying external arrays multiole time
                            outNgc.write('G10 L2 P0 X{} Y{} ($$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$)\n'.format\
                            ((column * xOffset * units) + xOrgOffset, (row * yOffset * units) + yOrgOffset))
                        inCod = open(self.parent.fTmp, 'r')
                        for line in inCod:
                            a = b = c = ''
                            a = line.upper().replace(' ', '')
                            if 'M2' in a or 'M30' in a:
                                b = a.replace('M2', '')
                                c = b.replace('M30', '')
                                outNgc.write(c)
                            elif '(postamble)' in line:
                                pass
                            else:
                                outNgc.write(line)
                        inCod.close()
                outNgc.write('G10 L2 P0 X#<ucs_x_offset> Y#<ucs_y_offset>\n')
                outNgc.write('M2\n')
                outNgc.close()
                self.parent.preview.load(self.parent.fNgc)
                if self.previewed:
                    md.destroy()
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
            self.parent.dialog_error('ARRAY', msg)
            return
        self.previewed = True
        self.add.set_sensitive(True)

    def auto_preview(self, widget):
        try:
            if int(self.xCEntry.get_text()) == 1 or (int(self.xCEntry.get_text()) > 1 and self.xOEntry.get_text()) and \
               int(self.yCEntry.get_text()) == 1 or (int(self.yCEntry.get_text()) > 1 and self.yOEntry.get_text()): 
                self.array_preview('auto') 
        except:
            pass

    def array_show(self, parent):
        self.parent = parent
        self.parent.entries.set_row_spacings(self.parent.rowSpace)
        for child in self.parent.entries.get_children():
            self.parent.entries.remove(child)
        try:
            shutil.copyfile(inFile, self.parent.fNgcBkp)
        except:
            pass
        CLabel = gtk.Label('Columns')
        CLabel.set_alignment(0.5, 1.0)
        self.parent.entries.attach(CLabel, 2, 3, 0, 1)
        xCLabel = gtk.Label('Number')
        xCLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(xCLabel, 0, 1, 1, 2)
        self.xCEntry = gtk.Entry()
        self.xCEntry.set_width_chars(8)
        self.xCEntry.set_text('1')
        self.xCEntry.connect('activate', self.auto_preview)
        self.xCEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.xCEntry, 1, 2, 1, 2)
        xOLabel = gtk.Label('Offset')
        xOLabel.set_alignment(0.05, 0.5)
        self.parent.entries.attach(xOLabel, 4, 5, 1, 2)
        self.xOEntry = gtk.Entry()
        self.xOEntry.set_width_chars(8)
        self.xOEntry.connect('activate', self.auto_preview)
        self.xOEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.xOEntry, 3, 4, 1, 2)
        RLabel = gtk.Label('Rows')
        RLabel.set_alignment(0.5, 1.0)
        self.parent.entries.attach(RLabel, 2, 3, 2, 3)
        yCLabel = gtk.Label('Number')
        yCLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(yCLabel, 0, 1, 3, 4)
        self.yCEntry = gtk.Entry()
        self.yCEntry.set_width_chars(8)
        self.yCEntry.set_text('1')
        self.yCEntry.connect('activate', self.auto_preview)
        self.yCEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.yCEntry, 1, 2, 3, 4)
        yOLabel = gtk.Label('Offset')
        yOLabel.set_alignment(0.05, 0.5)
        self.parent.entries.attach(yOLabel, 4, 5, 3, 4)
        self.yOEntry = gtk.Entry()
        self.yOEntry.set_width_chars(8)
        self.yOEntry.connect('activate', self.auto_preview)
        self.yOEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.yOEntry, 3, 4, 3, 4)
        OrgLabel = gtk.Label('Origin')
        OrgLabel.set_alignment(0.5, 1.0)
        self.parent.entries.attach(OrgLabel, 2, 3, 4, 5)
        xOrgLabel = gtk.Label('X Offset')
        xOrgLabel.set_alignment(0.95, 0.5)
        self.parent.entries.attach(xOrgLabel, 0, 1, 5, 6)
        self.xOrgEntry = gtk.Entry()
        self.xOrgEntry.set_width_chars(8)
        self.xOrgEntry.set_text('0')
        self.xOrgEntry.connect('activate', self.auto_preview)
        self.xOrgEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.xOrgEntry, 1, 2, 5, 6)
        yOrgLabel = gtk.Label('Y Offset')
        yOrgLabel.set_alignment(0.05, 0.5)
        self.parent.entries.attach(yOrgLabel, 4, 5, 5, 6)
        self.yOrgEntry = gtk.Entry()
        self.yOrgEntry.set_width_chars(8)
        self.yOrgEntry.set_text('0')
        self.yOrgEntry.connect('activate', self.auto_preview)
        self.yOrgEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.yOrgEntry, 3, 4, 5, 6)
        self.preview = gtk.Button('Preview')
        self.preview.connect('pressed', self.array_preview)
        self.parent.entries.attach(self.preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.connect('pressed', self.array_accept)
        self.parent.entries.attach(self.add, 2, 3, 12, 13)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.array_cancel)
        self.parent.entries.attach(undo, 4, 5, 12, 13)
        self.lDesc = gtk.Label('Arraying Shape')
        self.lDesc.set_alignment(0.5, 0.5)
        self.lDesc.set_width_chars(8)
        self.parent.entries.attach(self.lDesc, 1, 4, 13, 14)
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.xCEntry.grab_focus()
