#!/usr/bin/env python

'''
w_star.py

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
import math
import linuxcnc
import shutil
import hal
from subprocess import Popen,PIPE

class star_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def star_preview(self, event):
        self.s.poll()
        xPos = self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0]
        yPos = self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1]
        if self.pEntry.get_text():
            points = int(self.pEntry.get_text())
        else:
            points = 0
        if self.odEntry.get_text():
            oRadius = float(self.odEntry.get_text())
        else:
            oRadius = 0
        if self.idEntry.get_text():
            iRadius = float(self.idEntry.get_text())
        else:
            iRadius = 0
        if points >= 3 and iRadius > 0 and oRadius > 0 and oRadius > iRadius:
            if self.xSEntry.get_text():
                if self.centre.get_active():
                    xC = float(self.xSEntry.get_text())
                else:
                    xC = float(self.xSEntry.get_text()) + oRadius * math.cos(math.radians(0))
            else:
                if self.centre.get_active():
                    xC = xPos
                else:
                    xC = xPos + oRadius * math.cos(math.radians(0))
            if self.ySEntry.get_text():
                if self.centre.get_active():
                    yC = float(self.ySEntry.get_text())
                else:
                    yC = float(self.ySEntry.get_text()) + oRadius * math.sin(math.radians(90))
            else:
                if self.centre.get_active():
                    yC = yPos
                else:
                    yC = yPos + oRadius * math.sin(math.radians(90))
            if self.liEntry.get_text():
                leadInOffset = float(self.liEntry.get_text())
            else:
                leadInOffset = 0
            if self.loEntry.get_text():
                leadOutOffset = float(self.loEntry.get_text())
            else:
                leadOutOffset = 0
            if self.aEntry.get_text():
                angle = math.radians(float(self.aEntry.get_text()))
            else:
                angle = 0.0
            pList = []
            for i in range(points * 2):
                pAngle = angle + 2 * math.pi * i / (points * 2)
                if i % 2 == 0:
                    x = xC + oRadius * math.cos(pAngle)
                    y = yC + oRadius * math.sin(pAngle)
                else:
                    x = xC + iRadius * math.cos(pAngle)
                    y = yC + iRadius * math.sin(pAngle)
                pList.append(['{:.6f}'.format(x), '{:.6f}'.format(y)])
            outTmp = open(self.fTmp, 'w')
            outNgc = open(self.fNgc, 'w')
            inWiz = open(self.fNgcBkp, 'r')
            for line in inWiz:
                if '(new wizard)' in line:
                    outNgc.write('\n{} (preamble)\n'.format(self.preamble))
                    outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
                    break
                elif '(postamble)' in line:
                    break
                elif 'm2' in line.lower() or 'm30' in line.lower():
                    break
                outNgc.write(line)
            outTmp.write('\n(wizard star {})\n'.format(points))
            if self.outside.get_active():
                if leadInOffset > 0:
                    lAngle = math.atan2(float(pList[0][1]) - float(pList[points * 2 - 1][1]),
                                        float(pList[0][0]) - float(pList[points * 2 - 1][0]))
                    xlStart = float(pList[0][0]) + leadInOffset * math.cos(lAngle)
                    ylStart = float(pList[0][1]) + leadInOffset * math.sin(lAngle)
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    outTmp.write('m3 $0 s1\n')
                    if self.offset.get_active():
                        outTmp.write('g41.1 d#<_hal[plasmac_run.kerf-width-f]>\n')
                else:
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(pList[0][0], pList[0][1]))
                    outTmp.write('m3 $0 s1\n')
                outTmp.write('g1 x{} y{}\n'.format(pList[0][0], pList[0][1]))
                for i in range(points * 2, 0, -1):
                    outTmp.write('g1 x{} y{}\n'.format(pList[i - 1][0], pList[i - 1][1]))
            else:
                if leadInOffset > 0:
                    lAngle = math.atan2(float(pList[points * 2 - 1][1]) - float(pList[0][1]),
                                        float(pList[points * 2 - 1][0]) - float(pList[0][0]))
                    xlStart = float(pList[points * 2 - 1][0]) + leadInOffset * math.cos(lAngle)
                    ylStart = float(pList[points * 2 - 1][1]) + leadInOffset * math.sin(lAngle)
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    outTmp.write('m3 $0 s1\n')
                    if self.offset.get_active():
                        outTmp.write('g41.1 d#<_hal[plasmac_run.kerf-width-f]>\n')
                    outTmp.write('g1 x{} y{}\n'.format(pList[points * 2 - 1][0], pList[points * 2 - 1][1]))
                    outTmp.write('g1 x{} y{}\n'.format(pList[0][0], pList[0][1]))
                else:
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(pList[0][0], pList[0][1]))
                    outTmp.write('m3 $0 s1\n')
                for i in range(1, points * 2):
                    outTmp.write('g1 x{} y{}\n'.format(pList[i][0], pList[i][1]))
            if self.offset.get_active():
                outTmp.write('g40\n')
            outTmp.write('m5\n')
            outTmp.close()
            outTmp = open(self.fTmp, 'r')
            for line in outTmp:
                outNgc.write(line)
            outTmp.close()
            outNgc.write('\n{} (postamble)\n'.format(self.postamble))
            outNgc.write('m2\n')
            outNgc.close()
            self.parent.preview.load(self.fNgc)
            self.add.set_sensitive(True)
            self.parent.xOrigin = self.xSEntry.get_text()
            self.parent.yOrigin = self.ySEntry.get_text()
        else:
            msg = ''
            if points < 3:
                msg += 'Points must be 3 or more\n\n'
            if oRadius <= 0:
                msg += 'Outside Diameter is required\n\n'
            if iRadius >= oRadius:
                msg += 'Outside Diameter must be > Inside Diameter\n\n'
            if iRadius <= 0:
                msg += 'Inside Diameter is required'
            self.parent.dialog_error('STAR', msg)

    def mode_changed(self, widget):
        if self.mCombo.get_active() == 2:
            self.dLabel.set_text('Side Length')
        else:
            self.dLabel.set_text('Diameter')

    def auto_preview(self, widget):
        if self.pEntry.get_text() and self.odEntry.get_text() and self.idEntry.get_text():
            self.star_preview('auto') 

    def star_show(self, parent, entries, fNgc, fNgcBkp, fTmp, rowS, xOrigin, yOrigin):
        entries.set_row_spacings(rowS)
        self.parent = parent
        for child in entries.get_children():
            entries.remove(child)
        self.fNgc = fNgc
        self.fNgcBkp = fNgcBkp
        self.fTmp = fTmp
        cLabel = gtk.Label('Cut Type')
        cLabel.set_alignment(0.95, 0.5)
        cLabel.set_width_chars(8)
        entries.attach(cLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        self.outside.connect('toggled', self.auto_preview)
        entries.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        entries.attach(inside, 2, 3, 0, 1)
        oLabel = gtk.Label('Offset')
        oLabel.set_alignment(0.95, 0.5)
        oLabel.set_width_chars(8)
        entries.attach(oLabel, 3, 4, 0, 1)
        self.offset = gtk.CheckButton('Kerf')
        self.offset.connect('toggled', self.auto_preview)
        entries.attach(self.offset, 4, 5, 0, 1)
        lLabel = gtk.Label('Lead In')
        lLabel.set_alignment(0.95, 0.5)
        lLabel.set_width_chars(8)
        entries.attach(lLabel, 0, 1, 1, 2)
        self.liEntry = gtk.Entry()
        self.liEntry.set_width_chars(8)
        self.liEntry.connect('activate', self.auto_preview)
        self.liEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.liEntry, 1, 2, 1, 2)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.95, 0.5)
        loLabel.set_width_chars(8)
        entries.attach(loLabel, 0, 1, 2, 3)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(8)
        self.loEntry.connect('activate', self.auto_preview)
        self.loEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.loEntry, 1, 2, 2, 3)
        xSLabel = gtk.Label('X start')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(8)
        entries.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(8)
        self.xSEntry.connect('activate', self.auto_preview)
        self.xSEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label('Y start')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(8)
        entries.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(8)
        self.ySEntry.connect('activate', self.auto_preview)
        self.ySEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.ySEntry, 1, 2, 4, 5)
        self.centre = gtk.RadioButton(None, 'Centre')
        self.centre.connect('toggled', self.auto_preview)
        entries.attach(self.centre, 1, 2, 5, 6)
        self.bLeft = gtk.RadioButton(self.centre, 'Btm Lft')
        entries.attach(self.bLeft, 0, 1, 5, 6)
        pLabel = gtk.Label('# of Points')
        pLabel.set_alignment(0.95, 0.5)
        pLabel.set_width_chars(8)
        entries.attach(pLabel, 0, 1, 6, 7)
        self.pEntry = gtk.Entry()
        self.pEntry.set_width_chars(8)
        self.pEntry.connect('activate', self.auto_preview)
        self.pEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.pEntry, 1, 2, 6, 7)
        self.odLabel = gtk.Label('Outer Dia')
        self.odLabel.set_alignment(0.95, 0.5)
        self.odLabel.set_width_chars(8)
        entries.attach(self.odLabel, 0, 1, 7, 8)
        self.odEntry = gtk.Entry()
        self.odEntry.set_width_chars(8)
        self.odEntry.connect('activate', self.auto_preview)
        self.odEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.odEntry, 1, 2, 7, 8)
        self.idLabel = gtk.Label('Inner Dia')
        self.idLabel.set_alignment(0.95, 0.5)
        self.idLabel.set_width_chars(8)
        entries.attach(self.idLabel, 0, 1, 8, 9)
        self.idEntry = gtk.Entry()
        self.idEntry.set_width_chars(8)
        self.idEntry.connect('activate', self.auto_preview)
        self.idEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.idEntry, 1, 2, 8, 9)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(8)
        entries.attach(aLabel, 0, 1, 9, 10)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(8)
        self.aEntry.set_text('0')
        self.aEntry.connect('activate', self.auto_preview)
        self.aEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.aEntry, 1, 2, 9, 10)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.star_preview)
        entries.attach(preview, 0, 1, 13, 14)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.parent.add_shape_to_file, self.add)
        entries.attach(self.add, 2, 3, 13, 14)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        entries.attach(undo, 4, 5, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/star.png', 
                width=240, 
                height=240)
        image = gtk.Image()
        image.set_from_pixbuf(pixbuf)
        entries.attach(image, 2, 5, 1, 9)
        if os.path.exists(self.configFile):
            f_in = open(self.configFile, 'r')
            for line in f_in:
                if line.startswith('preamble'):
                    self.preamble = line.strip().split('=')[1]
                elif line.startswith('postamble'):
                    self.postamble = line.strip().split('=')[1]
                elif line.startswith('origin'):
                    if line.strip().split('=')[1] == 'True':
                        self.centre.set_active(1)
                    else:
                        self.bLeft.set_active(1)
                elif line.startswith('lead-in'):
                    self.liEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('lead-out'):
                    self.loEntry.set_text(line.strip().split('=')[1])
        self.xSEntry.set_text('{:0.3f}'.format(float(xOrigin)))
        self.ySEntry.set_text('{:0.3f}'.format(float(yOrigin)))
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.pEntry.grab_focus()
