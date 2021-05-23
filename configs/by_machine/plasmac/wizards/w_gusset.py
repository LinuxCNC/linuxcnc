#!/usr/bin/env python

'''
w_gusset.py

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

class gusset_wiz:

    def __init__(self):
        pass

    def gusset_preview(self, event):
        width = height = 0
        if self.wEntry.get_text():
            width = float(self.wEntry.get_text())
        if self.hEntry.get_text():
            height = float(self.hEntry.get_text())
        if width > 0 and height > 0:
            right = math.radians(0)
            up = math.radians(90)
            left = math.radians(180)
            down = math.radians(270)
            if self.aEntry.get_text():
                angle = math.radians(float(self.aEntry.get_text()))
            else:
                angle = up
            if self.rEntry.get_text():
                radius = float(self.rEntry.get_text())
                if radius > height or radius > width:
                    msg = 'Radius must be less than width and height\n\n'
                    self.parent.dialog_error('GUSSET', msg)
                    return
            else:
                radius = 0.0
            kOffset = hal.get_value('plasmac_run.kerf-width-f') * self.offset.get_active() / 2
            if not self.xSEntry.get_text():
                self.xSEntry.set_text('{:0.3f}'.format(self.parent.xOrigin))
            if not self.ySEntry.get_text():
                self.ySEntry.set_text('{:0.3f}'.format(self.parent.yOrigin))
            if self.outside.get_active():
                x0 = float(self.xSEntry.get_text()) + kOffset
                y0 = float(self.ySEntry.get_text()) + kOffset
            else:
                x0 = float(self.xSEntry.get_text()) - kOffset
                y0 = float(self.ySEntry.get_text()) - kOffset
            x1 = x0 + width * math.cos(right)
            y1 = y0 + width * math.sin(right)
            x2 = x0 + height * math.cos(angle)
            y2 = y0 + height * math.sin(angle)
            hypotLength = math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)
            if x2 <= x1:
                hypotAngle = left - math.atan((y2 - y1) / (x1 - x2))
            else:
                hypotAngle = right - math.atan((y2 - y1) / (x1 - x2))
            xS = x1 + (hypotLength / 2) * math.cos(hypotAngle)
            yS = y1 + (hypotLength / 2) * math.sin(hypotAngle)
            if self.liEntry.get_text():
                leadInOffset = math.sin(math.radians(45)) * float(self.liEntry.get_text())
            else:
                leadInOffset = 0
            if self.loEntry.get_text():
                leadOutOffset = math.sin(math.radians(45)) * float(self.loEntry.get_text())
            else:
                leadOutOffset = 0
            if self.outside.get_active():
                if y2 >= y0:
                    dir = [up, right]
                else:
                    dir = [down, left]
            else:
                if y2 >= y0:
                    dir = [down, left]
                else:
                    dir = [up, right]
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
            outTmp.write('\n(wizard gusset)\n')
            if leadInOffset > 0:
                xlCentre = xS + (leadInOffset * math.cos(hypotAngle - dir[0]))
                ylCentre = yS + (leadInOffset * math.sin(hypotAngle - dir[0]))
                xlStart = xlCentre + (leadInOffset * math.cos(hypotAngle - dir[1]))
                ylStart = ylCentre + (leadInOffset * math.sin(hypotAngle - dir[1]))
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                if self.offset.get_active():
                    outTmp.write('g41.1 d#<_hal[plasmac_run.kerf-width-f]>\n')
                outTmp.write('m3 $0 s1\n')
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
            else:
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
                outTmp.write('m3 $0 s1\n')
            if self.outside.get_active():
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                if radius > 0:
                    x3 = x0 + radius
                    y3 = y0
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                    x4 = x0 + radius * math.cos(angle)
                    y4 = y0 + radius * math.sin(angle)
                    if self.rButton.child.get_text().startswith('Radius'):
                        if y2 >= y0:
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
                        else:
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                else:
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                if radius > 0:
                    x3 = x0 + radius
                    y3 = y0
                    x4 = x0 + radius * math.cos(angle)
                    y4 = y0 + radius * math.sin(angle)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                    if self.rButton.child.get_text().startswith('Radius'):
                        if y2 >= y0:
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
                        else:
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                else:
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
            if leadOutOffset > 0:
                if self.outside.get_active():
                    if y2 >= y0:
                        dir = [up, left]
                    else:
                        dir = [down, right]
                else:
                    if y2 >= y0:
                        dir = [down, right]
                    else:
                        dir = [up, left]
                xlCentre = xS + (leadOutOffset * math.cos(hypotAngle - dir[0]))
                ylCentre = yS + (leadOutOffset * math.sin(hypotAngle - dir[0]))
                xlEnd = xlCentre + (leadOutOffset * math.cos(hypotAngle - dir[1]))
                ylEnd = ylCentre + (leadOutOffset * math.sin(hypotAngle - dir[1]))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
            if self.offset.get_active():
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
            if width <= 0:
                msg += 'A positive width value is required\n\n'
            if height <= 0:
                msg += 'A positive height value is required\n\n'
            self.parent.dialog_error('GUSSET', msg)

    def rad_button_pressed(self, button):
        if button.child.get_text()[:3] == 'Rad':
            button.child.set_text('Chamfer')
        else:
            button.child.set_text('Radius')
        self.auto_preview('local')

    def auto_preview(self, widget):
        if self.wEntry.get_text() and self.hEntry.get_text():
            self.gusset_preview('auto') 

    def entry_changed(self, widget):
        if not self.liEntry.get_text() or float(self.liEntry.get_text()) == 0:
            self.offset.set_sensitive(False)
        else:
            self.offset.set_sensitive(True)
        self.parent.entry_changed(widget)

    def add_shape_to_file(self, button):
        self.parent.add_shape_to_file(self.add, self.xSEntry.get_text(), self.ySEntry.get_text(), None)

    def gusset_show(self, parent):
        self.parent = parent
        self.parent.entries.set_row_spacings(self.parent.rowSpace)
        for child in self.parent.entries.get_children():
            self.parent.entries.remove(child)
        cutLabel = gtk.Label('Cut Type')
        cutLabel.set_alignment(0.95, 0.5)
        cutLabel.set_width_chars(8)
        self.parent.entries.attach(cutLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        self.outside.connect('toggled', self.auto_preview)
        self.parent.entries.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        self.parent.entries.attach(inside, 2, 3, 0, 1)
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
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.95, 0.5)
        loLabel.set_width_chars(8)
        self.parent.entries.attach(loLabel, 0, 1, 2, 3)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(8)
        self.loEntry.connect('activate', self.auto_preview)
        self.loEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.loEntry, 1, 2, 2, 3)
        xSLabel = gtk.Label()
        xSLabel.set_markup('X <span foreground="red">origin</span>')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(8)
        self.parent.entries.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(8)
        self.xSEntry.connect('activate', self.auto_preview)
        self.xSEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label()
        ySLabel.set_markup('Y <span foreground="red">origin</span>')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(8)
        self.parent.entries.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(8)
        self.ySEntry.connect('activate', self.auto_preview)
        self.ySEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.ySEntry, 1, 2, 4, 5)
        wLabel = gtk.Label('Width')
        wLabel.set_alignment(0.95, 0.5)
        wLabel.set_width_chars(8)
        self.parent.entries.attach(wLabel, 0, 1, 5, 6)
        self.wEntry = gtk.Entry()
        self.wEntry.set_width_chars(8)
        self.wEntry.connect('activate', self.auto_preview)
        self.wEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.wEntry, 1, 2, 5, 6)
        hLabel = gtk.Label('Height')
        hLabel.set_alignment(0.95, 0.5)
        hLabel.set_width_chars(8)
        self.parent.entries.attach(hLabel, 0, 1, 6, 7)
        self.hEntry = gtk.Entry()
        self.hEntry.set_width_chars(8)
        self.hEntry.connect('activate', self.auto_preview)
        self.hEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.hEntry, 1, 2, 6, 7)
        self.rButton = gtk.Button('Radius')
        self.rButton.connect('pressed', self.rad_button_pressed)
        self.parent.entries.attach(self.rButton, 0, 1, 7, 8)
        self.rEntry = gtk.Entry()
        self.rEntry.set_width_chars(8)
        self.rEntry.set_text('0')
        self.rEntry.connect('activate', self.auto_preview)
        self.rEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.rEntry, 1, 2, 7, 8)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(8)
        self.parent.entries.attach(aLabel, 0, 1, 8, 9)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(8)
        self.aEntry.set_text('90')
        self.aEntry.connect('activate', self.auto_preview)
        self.aEntry.connect('changed',self.parent.entry_changed)
        self.parent.entries.attach(self.aEntry, 1, 2, 8, 9)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.gusset_preview)
        self.parent.entries.attach(preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        self.parent.entries.attach(self.add, 2, 3, 12, 13)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        self.parent.entries.attach(undo, 4, 5, 12, 13)
        self.lDesc = gtk.Label('Creating Gusset')
        self.lDesc.set_alignment(0.5, 0.5)
        self.lDesc.set_width_chars(8)
        self.parent.entries.attach(self.lDesc, 1, 4, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/gusset.png', 
                width=240, 
                height=240)
        image = gtk.Image()
        image.set_from_pixbuf(pixbuf)
        self.parent.entries.attach(image, 2, 5, 1, 9)
        self.liEntry.set_text(self.parent.leadIn)
        self.loEntry.set_text(self.parent.leadOut)
        self.xSEntry.set_text('{}'.format(self.parent.xSaved))
        self.ySEntry.set_text('{}'.format(self.parent.ySaved))
        if not self.liEntry.get_text() or float(self.liEntry.get_text()) == 0:
            self.offset.set_sensitive(False)
        self.parent.undo_shape(None, self.add)
        self.parent.W.show_all()
        self.wEntry.grab_focus()
