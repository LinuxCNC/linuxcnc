#!/usr/bin/env python

'''
w_circle.py

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

class circle_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        self.scale = 0.039370 if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else 1.0

    def circle_preview(self, event):
        self.s.poll()
        xPos = self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0]
        yPos = self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1]
        if self.dEntry.get_text():
            radius = float(self.dEntry.get_text()) / 2
        else:
            radius = 0
        if radius > 0:
            angle = math.radians(45)
            ijOffset = radius * math.sin(angle)
            ijDiff = 0
            if self.offset.get_active():
                if self.outside.get_active():
                    ijDiff = hal.get_value('plasmac_run.kerf-width-f') / 2 * math.sin(angle)
                else:
                    ijDiff = hal.get_value('plasmac_run.kerf-width-f') / 2 * -math.sin(angle)

            if self.liEntry.get_text():
                leadInOffset = math.sin(angle) * float(self.liEntry.get_text())
            else:
                leadInOffset = 0
            if self.loEntry.get_text():
                leadOutOffset = math.sin(math.radians(45)) * float(self.loEntry.get_text())
            else:
                leadOutOffset = 0
            if self.xSEntry.get_text():
                if self.centre.get_active():
                    xC = float(self.xSEntry.get_text())
                else:
                    xC = float(self.xSEntry.get_text()) + radius
            else:
                if self.centre.get_active():
                    xC = xPos
                else:
                    xC = xPos + radius
            if self.ySEntry.get_text():
                if self.centre.get_active():
                    yC = float(self.ySEntry.get_text())
                else:
                    yC = float(self.ySEntry.get_text()) + radius
            else:
                if self.centre.get_active():
                    yC = yPos
                else:
                    yC = yPos + radius
            xS = xC - ijOffset - ijDiff
            yS = yC - ijOffset - ijDiff
            right = math.radians(0)
            up = math.radians(90)
            left = math.radians(180)
            down = math.radians(270)
            if self.outside.get_active():
                dir = [left, down]
            else:
                dir = [right, up]
            if radius <= self.sRadius:
                sHole = True
                if leadInOffset > radius:
                    leadInOffset = radius
            else:
                sHole = False
            outTmp = open(self.fTmp, 'w')
            outNgc = open(self.fNgc, 'w')
            if sHole:
                speed = 0.6
            else:
                speed = 1.0
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
            outTmp.write('\n(wizard circle)\n')
            if sHole:
                outTmp.write('M67 E3 Q60 (reduce feed rate to 60%)\n')
            if leadInOffset > 0:
                if sHole and not self.outside.get_active():
                    xlStart = xS + leadInOffset * math.cos(angle)
                    ylStart = yS + leadInOffset * math.sin(angle)
                else:
                    xlCentre = xS + (leadInOffset * math.cos(angle + dir[0]))
                    ylCentre = yS + (leadInOffset * math.sin(angle + dir[0]))
                    xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
                    ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                outTmp.write('m3 $0 s1\n')
                if sHole and not self.outside.get_active():
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
                else:
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlCentre - xlStart, ylCentre - ylStart))
            else:
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
                outTmp.write('m3 $0 s1\n')
            if self.outside.get_active():
                outTmp.write('g2 x{0:.6f} y{1:.6f} i{2:.6f} j{2:.6f}\n'.format(xS, yS, ijOffset + ijDiff))
            else:
                outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{2:.6f}\n'.format(xS, yS, ijOffset + ijDiff))
            if leadOutOffset and not self.overcut.get_active() and not (not self.outside.get_active() and sHole):
                    if self.outside.get_active():
                        dir = [left, up]
                    else:
                        dir = [right, down]
                    xlCentre = xS + (leadOutOffset * math.cos(angle + dir[0]))
                    ylCentre = yS + (leadOutOffset * math.sin(angle + dir[0]))
                    xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
                    ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlCentre - xS, ylCentre - yS))
            torch = True
            if self.overcut.get_active() and sHole and not self.outside.get_active():
                Torch = False
                outTmp.write('m62 p3 (disable torch)\n')
                self.over_cut(xS, yS, ijOffset + ijDiff, radius, outTmp)
            outTmp.write('m5\n')
            if sHole:
                outTmp.write('M68 E3 Q0 (reset feed rate to 100%)\n')
            if not torch:
                torch = True
                outTmp.write('m65 p3 (enable torch)\n')
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
            self.parent.dialog_error('CIRCLE', 'Diameter is required')

    def over_cut(self, lastX, lastY, IJ, radius, outTmp):
        try:
            oclength = float(self.ocEntry.get_text())
        except:
            oclength = 0
        centerX = lastX + IJ
        centerY = lastY + IJ
        cosA = math.cos(oclength / radius)
        sinA = math.sin(oclength / radius)
        cosB = ((lastX - centerX) / radius)
        sinB = ((lastY - centerY) / radius)
        #clockwise arc
        if self.outside.get_active():
            endX = centerX + radius * ((cosB * cosA) + (sinB * sinA))
            endY = centerY + radius * ((sinB * cosA) - (cosB * sinA))
            dir = '2'
        #counterclockwise arc
        else:
            endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
            endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
            dir = '3'
        outTmp.write('g{0} x{1:.6f} y{2:.6f} i{3:.6f} j{3:.6f}\n'.format(dir, endX, endY, IJ))

    def outside_toggled(self, widget):
        if widget.get_active():
            self.overcut.set_active(False)
            self.overcut.set_sensitive(False)
            self.ocEntry.set_sensitive(False)
        else:
            try:
                rad = float(self.dEntry.get_text()) / 2
            except:
                rad = 0
            if rad <= self.sRadius:
                self.overcut.set_sensitive(True)
                self.ocEntry.set_sensitive(True)
        self.auto_preview('auto')

    def overcut_toggled(self,widget):
        if widget.get_active():
            try:
                lolen = float(self.loEntry.get_text())
            except:
                lolen = 0
            try:
                rad = float(self.dEntry.get_text()) / 2
            except:
                rad = 0
            if (self.outside.get_active() and lolen) or not rad or rad > self.sRadius:
                self.overcut.set_active(False)
        self.auto_preview('auto')    

    def entry_changed(self, widget):
        self.parent.entry_changed(widget)
        try:
            rad = float(self.dEntry.get_text()) / 2
        except:
            rad = 0
        if rad >= self.sRadius:
            self.overcut.set_active(False)
            self.overcut.set_sensitive(False)
            self.ocEntry.set_sensitive(False)
        else:
            if not self.outside.get_active():
                self.overcut.set_sensitive(True)
                self.ocEntry.set_sensitive(True)

    def auto_preview(self, widget):
        if self.dEntry.get_text() and float(self.dEntry.get_text()) > 0:
            self.circle_preview('auto') 

    def circle_show(self, parent, entries, fNgc, fNgcBkp, fTmp, rowS, xOrigin, yOrigin):
        entries.set_row_spacings(rowS)
        self.parent = parent
        for child in entries.get_children():
            entries.remove(child)
        self.fNgc = fNgc
        self.fNgcBkp = fNgcBkp
        self.fTmp = fTmp
        self.sRadius = 0.0
        self.hSpeed = 100
        cutLabel = gtk.Label('Cut Type')
        cutLabel.set_alignment(0.95, 0.5)
        cutLabel.set_width_chars(8)
        entries.attach(cutLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        self.outside.connect('toggled',self.outside_toggled)
        entries.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        entries.attach(inside, 2, 3, 0, 1)
        offsetLabel = gtk.Label('Offset')
        offsetLabel.set_alignment(0.95, 0.5)
        offsetLabel.set_width_chars(8)
        entries.attach(offsetLabel, 3, 4, 0, 1)
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
        self.liEntry.connect('changed', self.entry_changed)
        entries.attach(self.liEntry, 1, 2, 1, 2)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.95, 0.5)
        loLabel.set_width_chars(8)
        entries.attach(loLabel, 0, 1, 2, 3)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(8)
        self.loEntry.connect('activate', self.auto_preview)
        self.loEntry.connect('changed', self.entry_changed)
        entries.attach(self.loEntry, 1, 2, 2, 3)
        xSLabel = gtk.Label()
        xSLabel.set_markup('X <span foreground="red">origin</span>')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(8)
        entries.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(8)
        self.xSEntry.connect('activate', self.auto_preview)
        self.xSEntry.connect('changed', self.entry_changed)
        entries.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label()
        ySLabel.set_markup('Y <span foreground="red">origin</span>')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(8)
        entries.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(8)
        self.ySEntry.connect('activate', self.auto_preview)
        self.ySEntry.connect('changed', self.entry_changed)
        entries.attach(self.ySEntry, 1, 2, 4, 5)
        self.centre = gtk.RadioButton(None, 'Centre')
        self.centre.connect('toggled', self.auto_preview)
        entries.attach(self.centre, 1, 2, 5, 6)
        self.bLeft = gtk.RadioButton(self.centre, 'Btm Lft')
        entries.attach(self.bLeft, 0, 1, 5, 6)
        dLabel = gtk.Label('Diameter')
        dLabel.set_alignment(0.95, 0.5)
        dLabel.set_width_chars(8)
        entries.attach(dLabel, 0, 1, 6, 7)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(8)
        self.dEntry.connect('activate', self.auto_preview)
        self.dEntry.connect('changed', self.entry_changed)
        entries.attach(self.dEntry, 1, 2, 6, 7)
        ocDesc = gtk.Label('Over Cut')
        ocDesc.set_alignment(0.95, 0.5)
        ocDesc.set_width_chars(8)
        entries.attach(ocDesc, 0, 1, 7, 8)
        self.overcut = gtk.CheckButton('')
        self.overcut.connect('toggled', self.overcut_toggled)
        self.overcut.set_sensitive(False)
        entries.attach(self.overcut, 1, 2, 7, 8)
        ocLabel = gtk.Label('OC Length')
        ocLabel.set_alignment(0.95, 0.5)
        ocLabel.set_width_chars(8)
        entries.attach(ocLabel, 0, 1, 8, 9)
        self.ocEntry = gtk.Entry()
        self.ocEntry.set_width_chars(8)
        self.ocEntry.set_sensitive(False)
        self.ocEntry.set_text(str(4 * self.scale))
        self.ocEntry.connect('activate', self.auto_preview)
        self.ocEntry.connect('changed', self.entry_changed)
        entries.attach(self.ocEntry, 1, 2, 8, 9)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.circle_preview)
        entries.attach(preview, 0, 1, 13, 14)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.parent.add_shape_to_file, self.add)
        entries.attach(self.add, 2, 3, 13, 14)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        entries.attach(undo, 4, 5, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/circle.png', 
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
                elif line.startswith('hole-diameter'):
                    self.sRadius = float(line.strip().split('=')[1]) / 2
                elif line.startswith('hole-speed'):
                    self.hSpeed = float(line.strip().split('=')[1])
        self.xSEntry.set_text('{:0.3f}'.format(float(xOrigin)))
        self.ySEntry.set_text('{:0.3f}'.format(float(yOrigin)))
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.dEntry.grab_focus()
