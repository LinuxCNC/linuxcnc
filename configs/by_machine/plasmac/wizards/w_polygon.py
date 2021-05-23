#!/usr/bin/env python

'''
w_polygon.py

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

class polygon_wiz:

    def __init__(self):
        pass

    def polygon_preview(self, event):
        if self.sEntry.get_text():
            sides = int(self.sEntry.get_text())
        else:
            sides = 0
        if self.dEntry.get_text():
            data = float(self.dEntry.get_text())
            if self.mCombo.get_active() == 0:
                radius = data / 2
            elif self.mCombo.get_active() == 1:
                radius = (data / 2) / math.cos(math.radians(180 / sides))
            else:
                radius = data / (2 * math.sin(math.radians(180 / sides)))
        else:
            radius = 0
        if sides >= 3 and radius > 0:
            ijOffset = radius * math.sin(math.radians(45))
            if not self.xSEntry.get_text():
                self.xSEntry.set_text('{:0.3f}'.format(self.parent.xOrigin))
            if self.centre.get_active():
                xS = float(self.xSEntry.get_text())
            else:
                xS = float(self.xSEntry.get_text()) + radius * math.cos(math.radians(0))
            if not self.ySEntry.get_text():
                self.ySEntry.set_text('{:0.3f}'.format(self.parent.yOrigin))
            if self.centre.get_active():
                yS = float(self.ySEntry.get_text())
            else:
                yS = float(self.ySEntry.get_text()) + radius * math.sin(math.radians(90))
            if self.liEntry.get_text():
                leadInOffset = float(self.liEntry.get_text()) / (2 * math.pi * (90.0 / 360))
            else:
                leadInOffset = 0
            if self.loEntry.get_text():
                leadOutOffset = math.sin(math.radians(45)) * float(self.loEntry.get_text())
            else:
                leadOutOffset = 0
            if self.aEntry.get_text():
                sAngle = math.radians(float(self.aEntry.get_text()))
            else:
                sAngle = 0.0
            pList = []
            for i in range(sides):
                angle = sAngle + 2 * math.pi * i / sides
                x = xS + radius * math.cos(angle)
                y = yS + radius * math.sin(angle)
                pList.append(['{:.6f}'.format(x), '{:.6f}'.format(y)])
            xCentre = (float(pList[0][0]) + float(pList[sides - 1][0])) / 2
            yCentre = (float(pList[0][1]) + float(pList[sides - 1][1])) / 2
            angle = math.atan2(float(pList[0][1]) - yCentre, float(pList[0][0]) - xCentre)
            right = math.radians(0)
            up = math.radians(90)
            left = math.radians(180)
            down = math.radians(270)
            if self.outside.get_active():
                dir = [down, right]
            else:
                dir = [up, left]
            outTmp = open(self.parent.fTmp, 'w')
            outNgc = open(self.parent.fNgc, 'w')
            inWiz = open(self.parent.fNgcBkp, 'r')
            for line in inWiz:
                if '(new wizard)' in line:
                    outNgc.write('\n{} (preamble)\n'.format(self.parent.preamble))
                    outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
                    break
                elif '(postamble)' in line:
                    break
                elif 'm2' in line.lower() or 'm30' in line.lower():
                    break
                outNgc.write(line)
            outTmp.write('\n(wizard polygon {})\n'.format(sides))
            if leadInOffset > 0:
                xlCentre = xCentre + (leadInOffset * math.cos(angle + dir[0]))
                ylCentre = yCentre + (leadInOffset * math.sin(angle + dir[0]))
                xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
                ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                if self.offset.get_active():
                    outTmp.write('g41.1 d#<_hal[plasmac_run.kerf-width-f]>\n')
                outTmp.write('m3 $0 s1\n')
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xCentre, yCentre, xlCentre - xlStart, ylCentre - ylStart))
            else:
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xCentre, yCentre))
                outTmp.write('m3 $0 s1\n')
            if self.outside.get_active():
                for i in range(sides, 0, -1):
                    outTmp.write('g1 x{} y{}\n'.format(pList[i - 1][0], pList[i - 1][1]))
            else:
                for i in range(sides):
                    outTmp.write('g1 x{} y{}\n'.format(pList[i][0], pList[i][1]))
            outTmp.write('g1 x{} y{}\n'.format(xCentre, yCentre))
            if leadOutOffset > 0:
                if self.outside.get_active():
                    dir = [down, left]
                else:
                    dir = [up, right]
                xlCentre = xCentre + (leadOutOffset * math.cos(angle + dir[0]))
                ylCentre = yCentre + (leadOutOffset * math.sin(angle + dir[0]))
                xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
                ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xCentre, ylCentre - yCentre))
            outTmp.write('g40\n')
            outTmp.write('m5 $0\n')
            outTmp.close()
            outTmp = open(self.parent.fTmp, 'r')
            for line in outTmp:
                outNgc.write(line)
            outTmp.close()
            outNgc.write('\n{} (postamble)\n'.format(self.parent.postamble))
            outNgc.write('m2\n')
            outNgc.close()
            self.parent.preview.load(self.parent.fNgc)
            self.add.set_sensitive(True)
        else:
            msg = ''
            if sides < 3:
                msg += 'Sides must be 3 or more\n\n'
            if radius <= 0:
                msg += 'Diameter is required'
            self.parent.dialog_error('POLYGON', msg)

    def mode_changed(self, widget):
        if self.mCombo.get_active() == 2:
            self.dLabel.set_text('Side Length')
        else:
            self.dLabel.set_text('Diameter')
        self.auto_preview('auto')

    def auto_preview(self, widget):
        if self.sEntry.get_text() and self.dEntry.get_text():
            self.polygon_preview('auto') 

    def entry_changed(self, widget):
        if not self.liEntry.get_text() or float(self.liEntry.get_text()) == 0:
            self.offset.set_sensitive(False)
        else:
            self.offset.set_sensitive(True)
        self.parent.entry_changed(widget)

    def add_shape_to_file(self, button):
        self.parent.add_shape_to_file(self.add, self.xSEntry.get_text(), self.ySEntry.get_text(), self.centre.get_active())

    def polygon_show(self, parent):
        self.parent = parent
        self.parent.entries.set_row_spacings(self.parent.rowSpace - 2)
        for child in self.parent.entries.get_children():
            self.parent.entries.remove(child)
        cLabel = gtk.Label('Cut Type')
        cLabel.set_alignment(0.95, 0.5)
        cLabel.set_width_chars(8)
        self.parent.entries.attach(cLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        self.outside.connect('toggled', self.auto_preview)
        self.parent.entries.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        self.parent.entries.attach(inside, 2, 3, 0, 1)
        oLabel = gtk.Label('Offset')
        oLabel.set_alignment(0.95, 0.5)
        oLabel.set_width_chars(8)
        self.parent.entries.attach(oLabel, 3, 4, 0, 1)
        self.offset = gtk.CheckButton('Kerf')
        self.offset.connect('toggled', self.auto_preview)
        self.parent.entries.attach(self.offset, 4, 5, 0, 1)
        lLabel = gtk.Label('Lead In')
        lLabel.set_alignment(0.95, 0.5)
        lLabel.set_width_chars(8)
        self.parent.entries.attach(lLabel, 0, 1, 1, 2)
        self.liEntry = gtk.Entry()
        self.liEntry.set_width_chars(8)
        self.liEntry.connect('activate', self.auto_preview)
        self.liEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.liEntry, 1, 2, 1, 2)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.95, 0.5)
        loLabel.set_width_chars(8)
        self.parent.entries.attach(loLabel, 0, 1, 2, 3)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(8)
        self.loEntry.connect('activate', self.auto_preview)
        self.loEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.loEntry, 1, 2, 2, 3)
        xSLabel = gtk.Label()
        xSLabel.set_markup('X <span foreground="red">origin</span>')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(8)
        self.parent.entries.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(8)
        self.xSEntry.connect('activate', self.auto_preview)
        self.xSEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label()
        ySLabel.set_markup('Y <span foreground="red">origin</span>')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(8)
        self.parent.entries.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(8)
        self.ySEntry.connect('activate', self.auto_preview)
        self.ySEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.ySEntry, 1, 2, 4, 5)
        self.centre = gtk.RadioButton(None, 'Centre')
        self.centre.connect('toggled', self.auto_preview)
        self.parent.entries.attach(self.centre, 1, 2, 5, 6)
        self.bLeft = gtk.RadioButton(self.centre, 'Btm Lft')
        self.parent.entries.attach(self.bLeft, 0, 1, 5, 6)
        sLabel = gtk.Label('Sides')
        sLabel.set_alignment(0.95, 0.5)
        sLabel.set_width_chars(8)
        self.parent.entries.attach(sLabel, 0, 1, 6, 7)
        self.sEntry = gtk.Entry()
        self.sEntry.set_width_chars(8)
        self.sEntry.connect('activate', self.auto_preview)
        self.sEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.sEntry, 1, 2, 6, 7)
        self.mCombo = gtk.combo_box_new_text()
        self.mCombo.append_text('Circumscribed')
        self.mCombo.append_text('Inscribed')
        self.mCombo.append_text('Side Length')
        self.mCombo.set_active(0)
        self.mCombo.connect('changed', self.mode_changed)
        self.parent.entries.attach(self.mCombo, 0, 2, 7, 8)
        self.dLabel = gtk.Label('Diameter')
        self.dLabel.set_alignment(0.95, 0.5)
        self.dLabel.set_width_chars(8)
        self.parent.entries.attach(self.dLabel, 0, 1, 8, 9)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(8)
        self.dEntry.connect('activate', self.auto_preview)
        self.dEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.dEntry, 1, 2, 8, 9)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(8)
        self.parent.entries.attach(aLabel, 0, 1, 9, 10)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(8)
        self.aEntry.set_text('0')
        self.aEntry.connect('activate', self.auto_preview)
        self.aEntry.connect('changed', self.parent.entry_changed)
        self.parent.entries.attach(self.aEntry, 1, 2, 9, 10)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.polygon_preview)
        self.parent.entries.attach(preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        self.parent.entries.attach(self.add, 2, 3, 12, 13)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        self.parent.entries.attach(undo, 4, 5, 12, 13)
        self.lDesc = gtk.Label('Creating Polygon')
        self.lDesc.set_alignment(0.5, 0.5)
        self.lDesc.set_width_chars(8)
        self.parent.entries.attach(self.lDesc, 1, 4, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/polygon.png', 
                width=240, 
                height=240)
        image = gtk.Image()
        image.set_from_pixbuf(pixbuf)
        self.parent.entries.attach(image, 2, 5, 1, 9)
        if self.parent.oSaved:
            self.centre.set_active(1)
        else:
            self.bLeft.set_active(1)
        self.liEntry.set_text(self.parent.leadIn)
        self.loEntry.set_text(self.parent.leadOut)
        self.xSEntry.set_text('{}'.format(self.parent.xSaved))
        self.ySEntry.set_text('{}'.format(self.parent.ySaved))
        if not self.liEntry.get_text() or float(self.liEntry.get_text()) == 0:
            self.offset.set_sensitive(False)
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.sEntry.grab_focus()
