#!/usr/bin/env python

'''
w_bolt_circle.py

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

class bolt_circle_wiz:

    def __init__(self):
        pass

    def bolt_circle_preview(self, event):
        if self.dEntry.get_text():
            cRadius = float(self.dEntry.get_text()) / 2
        else:
            cRadius = 0
        if self.hdEntry.get_text():
            hRadius = float(self.hdEntry.get_text()) / 2
        else:
            hRadius = 0
        if self.hEntry.get_text():
            holes = int(self.hEntry.get_text())
        else:
            holes = 0
        if self.cAEntry.get_text():
            cAngle = float(self.cAEntry.get_text())
        else:
            cAngle = 360.0
        if cAngle == 360:
            hAngle = math.radians(cAngle / holes)
        else:
            hAngle = math.radians(cAngle / (holes - 1))
        if cRadius > 0 and hRadius > 0 and holes > 0:
            ijDiff = 0
            if self.offset.get_active():
                if self.offset.get_active():
                    ijDiff = hal.get_value('plasmac_run.kerf-width-f') / 2
            right = math.radians(0)
            up = math.radians(90)
            left = math.radians(180)
            down = math.radians(270)
            if hRadius < self.parent.holeRadius:
                sHole = True
            else:
                sHole = False
            if self.aEntry.get_text():
                angle = math.radians(float(self.aEntry.get_text()))
            else:
                angle = 0
            if self.liEntry.get_text():
                leadIn = float(self.liEntry.get_text())
                leadInOffset = leadIn * math.sin(math.radians(45))
            else:
                leadIn = 0
                leadInOffset = 0
            if leadIn > hRadius:
                leadIn = hRadius
            if leadInOffset > hRadius:
                leadInOffset = hRadius
            if not self.xSEntry.get_text():
                self.xSEntry.set_text('{:0.3f}'.format(self.parent.xOrigin))
            if self.centre.get_active():
                xC = float(self.xSEntry.get_text())
            else:
                xC = float(self.xSEntry.get_text()) + cRadius
            if not self.ySEntry.get_text():
                self.ySEntry.set_text('{:0.3f}'.format(self.parent.yOrigin))
            if self.centre.get_active():
                yC = float(self.ySEntry.get_text())
            else:
                yC = float(self.ySEntry.get_text()) + cRadius
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
            for hole in range(holes):
                outTmp.write('\n(wizard bolt circle, hole #{})\n'.format(hole + 1))
                xhC = xC + cRadius * math.cos(hAngle * hole + angle)
                yhC = yC + cRadius * math.sin(hAngle * hole + angle)
                xS = xhC - hRadius + ijDiff
                yS = yhC
                if sHole:
                    outTmp.write('m67 E3 Q{}\n'.format(self.parent.holeSpeed))
                    xlStart = xS + leadIn
                    ylStart = yhC
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    outTmp.write('m3 $0 s1\n')
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
                else:
                    xlCentre = xS + (leadInOffset * math.cos(angle + right))
                    ylCentre = yS + (leadInOffset * math.sin(angle + right))
                    xlStart = xlCentre + (leadInOffset * math.cos(angle + up))
                    ylStart = ylCentre + (leadInOffset * math.sin(angle + up))
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    outTmp.write('m3 $0 s1\n')
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlCentre - xlStart, ylCentre - ylStart))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f}\n'.format(xS, yS, hRadius - ijDiff))
                if not sHole:
                    xlEnd = xlCentre + (leadInOffset * math.cos(angle + down))
                    ylEnd = ylCentre + (leadInOffset * math.sin(angle + down))
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlCentre - xS, ylCentre - yS))
                torch = True
                if self.overcut.get_active() and sHole:
                    Torch = False
                    outTmp.write('m62 p3 (disable torch)\n')
                    self.over_cut(xS, yS, hRadius - ijDiff, hRadius - ijDiff, outTmp)
                outTmp.write('m5 $0\n')
                if sHole:
                    outTmp.write('M68 E3 Q0 (reset feed rate to 100%)\n')
                if not torch:
                    torch = True
                    outTmp.write('m65 p3 (enable torch)\n')
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
            if cRadius == 0:
                msg += 'Diameter is required\n\n'
            if hRadius == 0:
                msg += 'Hole Diameter is required\n\n'
            if holes == 0:
                msg += '# of Holes are required'
            self.parent.dialog_error('BOLT-CIRCLE', msg)

    def over_cut(self, lastX, lastY, IJ, radius, outTmp):
        try:
            oclength = float(self.ocEntry.get_text())
        except:
            oclength = 0
        centerX = lastX + IJ
        centerY = lastY
        cosA = math.cos(oclength / radius)
        sinA = math.sin(oclength / radius)
        cosB = ((lastX - centerX) / radius)
        sinB = ((lastY - centerY) / radius)
        endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
        endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
        outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{3:.6f}\n'.format(endX, endY, IJ, 0))

    def entry_changed(self, widget):
        self.parent.entry_changed(widget)
        # check if small hole valid
        try:
            rad = float(self.hdEntry.get_text()) / 2
        except:
            rad = 0
        if rad >= self.parent.holeRadius:
            self.overcut.set_active(False)
            self.overcut.set_sensitive(False)
            self.ocEntry.set_sensitive(False)
        else:
            self.overcut.set_sensitive(True)
            self.ocEntry.set_sensitive(True)

    def auto_preview(self, widget):
        if self.dEntry.get_text() and self.hdEntry.get_text() and self.hEntry.get_text():
            self.bolt_circle_preview('auto') 

    def add_shape_to_file(self, button):
        self.parent.add_shape_to_file(self.add, self.xSEntry.get_text(), self.ySEntry.get_text(), self.centre.get_active())

    def bolt_circle_show(self, parent):
        self.parent = parent
        self.parent.entries.set_row_spacings(self.parent.rowSpace)
        for child in self.parent.entries.get_children():
            self.parent.entries.remove(child)
        ocBox = gtk.HBox()
        self.ocBlank = gtk.Label('    ')
        ocBox.pack_start(self.ocBlank, expand = True, fill = True)
        self.overcut = gtk.CheckButton('Over Cut')
        self.overcut.set_sensitive(False)
        self.overcut.connect('toggled', self.auto_preview)
        ocBox.pack_start(self.overcut)
        ocLabel = gtk.Label('OC Length')
        ocLabel.set_alignment(0.95, 0.5)
        ocLabel.set_width_chars(9)
        ocBox.pack_start(ocLabel)
        self.ocEntry = gtk.Entry()
        self.ocEntry.set_width_chars(5)
        self.ocEntry.set_sensitive(False)
        self.ocEntry.set_text(str(4 * self.parent.scale))
        self.ocEntry.connect('activate', self.auto_preview)
        self.ocEntry.connect('changed', self.entry_changed)
        ocBox.pack_start(self.ocEntry)
        self.parent.entries.attach(ocBox, 0, 3, 0, 1)
        offsetLabel = gtk.Label('Offset')
        offsetLabel.set_alignment(0.95, 0.5)
        offsetLabel.set_width_chars(8)
        self.parent.entries.attach(offsetLabel, 3, 4, 0, 1)
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
        xSLabel = gtk.Label()
        xSLabel.set_markup('X <span foreground="red">origin</span>')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(8)
        self.parent.entries.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(8)
        self.xSEntry.connect('activate', self.auto_preview)
        self.xSEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label()
        ySLabel.set_markup('Y <span color="red">origin</span>')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(8)
        self.parent.entries.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(8)
        self.ySEntry.connect('activate', self.auto_preview)
        self.ySEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.ySEntry, 1, 2, 4, 5)
        self.centre = gtk.RadioButton(None, 'Centre')
        self.centre.connect('toggled', self.auto_preview)
        self.parent.entries.attach(self.centre, 1, 2, 5, 6)
        self.bLeft = gtk.RadioButton(self.centre, 'Btm Lft')
        self.parent.entries.attach(self.bLeft, 0, 1, 5, 6)
        dLabel = gtk.Label('Diameter')
        dLabel.set_alignment(0.95, 0.5)
        dLabel.set_width_chars(8)
        self.parent.entries.attach(dLabel, 0, 1, 6, 7)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(8)
        self.dEntry.connect('activate', self.auto_preview)
        self.dEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.dEntry, 1, 2, 6, 7)
        hdLabel = gtk.Label('Hole Dia')
        hdLabel.set_alignment(0.95, 0.5)
        hdLabel.set_width_chars(8)
        self.parent.entries.attach(hdLabel, 0, 1, 7, 8)
        self.hdEntry = gtk.Entry()
        self.hdEntry.set_width_chars(8)
        self.hdEntry.connect('activate', self.auto_preview)
        self.hdEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.hdEntry, 1, 2, 7, 8)
        hLabel = gtk.Label('# of holes')
        hLabel.set_alignment(0.95, 0.5)
        hLabel.set_width_chars(8)
        self.parent.entries.attach(hLabel, 0, 1, 8, 9)
        self.hEntry = gtk.Entry()
        self.hEntry.set_width_chars(8)
        self.hEntry.connect('activate', self.auto_preview)
        self.hEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.hEntry, 1, 2, 8, 9)
        aLabel = gtk.Label('Start Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(8)
        self.parent.entries.attach(aLabel, 0, 1, 9, 10)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(8)
        self.aEntry.set_text('0')
        self.aEntry.connect('activate', self.auto_preview)
        self.aEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.aEntry, 1, 2, 9, 10)
        cALabel = gtk.Label('Circle Ang')
        cALabel.set_alignment(0.95, 0.5)
        cALabel.set_width_chars(8)
        self.parent.entries.attach(cALabel, 2, 3, 9, 10)
        self.cAEntry = gtk.Entry()
        self.cAEntry.set_width_chars(8)
        self.cAEntry.set_text('360')
        self.cAEntry.connect('activate', self.auto_preview)
        self.cAEntry.connect('changed', self.entry_changed)
        self.parent.entries.attach(self.cAEntry, 3, 4, 9, 10)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.bolt_circle_preview)
        self.parent.entries.attach(preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        self.parent.entries.attach(self.add, 2, 3, 12, 13)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        self.parent.entries.attach(undo, 4, 5, 12, 13)
        self.lDesc = gtk.Label('Creating Bolt Circle')
        self.lDesc.set_alignment(0.5, 0.5)
        self.lDesc.set_width_chars(8)
        self.parent.entries.attach(self.lDesc, 1, 4, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/bolt-circle.png', 
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
        self.xSEntry.set_text('{}'.format(self.parent.xSaved))
        self.ySEntry.set_text('{}'.format(self.parent.ySaved))
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.dEntry.grab_focus()
