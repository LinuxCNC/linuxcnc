#!/usr/bin/env python

'''
w_rectangle.py

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

class rectangle_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def rectangle_preview(self, event):
        self.s.poll()
        xPos = self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0]
        yPos = self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1]
        xLB = yLR = xLT = yLL = 0
        if self.xLEntry.get_text() and self.yLEntry.get_text():
            try:
                if float(self.xLEntry.get_text()) <= 0 or float(self.yLEntry.get_text()) <= 0:
                    msg  = 'A positive X Length is required\n\n'
                    msg += 'and\n\n'
                    msg += 'A positive Y Length is required\n'
                    self.parent.dialog_error('RECTANGLE', msg)
                    return
            except:
                msg = 'Invalid X Length or Y Length\n'
                self.parent.dialog_error('RECTANGLE', msg)
                return
            if self.r1Entry.get_text():
                radius1 = float(self.r1Entry.get_text())
            else:
                radius1 = 0.0
            if self.r2Entry.get_text():
                radius2 = float(self.r2Entry.get_text())
            else:
                radius2 = 0.0
            if self.r3Entry.get_text():
                radius3 = float(self.r3Entry.get_text())
            else:
                radius3 = 0.0
            if self.r4Entry.get_text():
                radius4 = float(self.r4Entry.get_text())
            else:
                radius4 = 0.0
            if radius1 + radius2 > float(self.xLEntry.get_text()):
                msg  = 'Radius 1 plus Radius 2 ({})\n\n'.format(radius1 + radius2)
                msg += 'can not be greater than {}\n'.format(float(self.xLEntry.get_text()))
                self.parent.dialog_error('RECTANGLE', msg)
                return
            if radius1 + radius3 > float(self.yLEntry.get_text()):
                msg  = 'Radius 1 plus Radius 3 ({})\n\n'.format(radius1 + radius3)
                msg += 'can not be greater than {}\n'.format(float(self.yLEntry.get_text()))
                self.parent.dialog_error('RECTANGLE', msg)
                return
            if radius2 + radius4 > float(self.yLEntry.get_text()):
                msg  = 'Radius 2 plus Radius 4 ({})\n\n'.format(radius2 + radius4)
                msg += 'can not be greater than {}\n'.format(float(self.yLEntry.get_text()))
                self.parent.dialog_error('RECTANGLE', msg)
                return
            if radius3 > float(self.xLEntry.get_text()) / 2 or radius4 > float(self.xLEntry.get_text()) / 2:
                msg  = 'Neither Radius 3 nor Radius 4\n\n'
                msg += 'can be greater than {}\n'.format(float(self.xLEntry.get_text()) / 2)
                self.parent.dialog_error('RECTANGLE', msg)
                return
            if self.xLEntry.get_text():
                xLB = float(self.xLEntry.get_text()) - (radius3 + radius4)
                xLT = float(self.xLEntry.get_text()) - (radius1 + radius2)
                xC = float(self.xLEntry.get_text()) / 2
            if self.yLEntry.get_text():
                yLR = float(self.yLEntry.get_text()) - (radius2 + radius4)
                yLL = float(self.yLEntry.get_text()) - (radius1 + radius3)
                yC = float(self.yLEntry.get_text()) / 2
            if xLB >= 0 and yLR >= 0 and xLT >= 0 and yLL >= 0:
                if self.angEntry.get_text():
                    angle = math.radians(float(self.angEntry.get_text()))
                else:
                    angle = 0.0
                if self.liEntry.get_text():
                    leadInOffset = math.sin(math.radians(45)) * float(self.liEntry.get_text())
                else:
                    leadInOffset = 0
                if self.loEntry.get_text():
                    leadOutOffset = math.sin(math.radians(45)) * float(self.loEntry.get_text())
                else:
                    leadOutOffset = 0
                right = math.radians(0)
                up = math.radians(90)
                left = math.radians(180)
                down = math.radians(270)
                if self.xSEntry.get_text():
                    if self.centre.get_active():
                        xS = float(self.xSEntry.get_text()) + yC * math.cos(angle + down)
                    else:
                        xS = float(self.xSEntry.get_text()) + xC * math.cos(angle + right)
                else:
                    if self.centre.get_active():
                        xS = xPos + yC * math.cos(angle + down)
                    else:
                        xS = xPos + xC * math.cos(angle + right)
                if self.ySEntry.get_text():
                    if self.centre.get_active():
                        yS = float(self.ySEntry.get_text()) + yC * math.sin(angle + down)
                    else:
                        yS = float(self.ySEntry.get_text()) + xC * math.sin(angle + right)
                else:
                    if self.centre.get_active():
                        yS = yPos + yC * math.sin(angle + down)
                    else:
                        yS = yPos + xC * math.sin(angle + right)
                if self.outside.get_active():
                    dir = [down, right, left]
                else:
                    dir = [up, left, right]
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
                outTmp.write('\n(wizard rectangle)\n')
                if leadInOffset > 0:
                    xlCentre = xS + (leadInOffset * math.cos(angle + dir[0]))
                    ylCentre = yS + (leadInOffset * math.sin(angle + dir[0]))
                    xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
                    ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    if self.offset.get_active():
                        outTmp.write('g41.1 d#<_hal[plasmac_run.kerf-width-f]>\n')
                    outTmp.write('m3 $0 s1\n')
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
                else:
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
                    outTmp.write('m3 $0 s1\n')
                if self.outside.get_active():
                    x1 = xS + (float(self.xLEntry.get_text()) / 2 - radius3) * math.cos(angle + dir[2])
                    y1 = yS + (float(self.xLEntry.get_text()) / 2 - radius3) * math.sin(angle + dir[2])
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                    if radius3 > 0:
                        if self.r3Button.child.get_text().startswith('iRadius'):
                            xrCentre = x1 + (radius3 * math.cos(angle + left))
                            yrCentre = y1 + (radius3 * math.sin(angle + left))
                            xrEnd = xrCentre + (radius3 * math.cos(angle + up))
                            yrEnd = yrCentre + (radius3 * math.sin(angle + up))
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        else:
                            xrCentre = x1 + (radius3 * math.cos(angle + up))
                            yrCentre = y1 + (radius3 * math.sin(angle + up))
                            xrEnd = xrCentre + (radius3 * math.cos(angle + left))
                            yrEnd = yrCentre + (radius3 * math.sin(angle + left))
                        if self.r3Button.child.get_text().startswith('Radius'):
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x2 = xrEnd + yLL * math.cos(angle + up)
                        y2 = yrEnd + yLL * math.sin(angle + up)
                    else:
                        x2 = x1 + yLL * math.cos(angle + up)
                        y2 = y1 + yLL * math.sin(angle + up)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                    if radius1 > 0:
                        if self.r1Button.child.get_text().startswith('iRadius'):
                            xrCentre = x2 + (radius1 * math.cos(angle + up))
                            yrCentre = y2 + (radius1 * math.sin(angle + up))
                            xrEnd = xrCentre + (radius1 * math.cos(angle + right))
                            yrEnd = yrCentre + (radius1 * math.sin(angle + right))
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        else:
                            xrCentre = x2 + (radius1 * math.cos(angle + right))
                            yrCentre = y2 + (radius1 * math.sin(angle + right))
                            xrEnd = xrCentre + (radius1 * math.cos(angle + up))
                            yrEnd = yrCentre + (radius1 * math.sin(angle + up))
                        if self.r1Button.child.get_text().startswith('Radius'):
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x3 = xrEnd + xLT * math.cos(angle + right)
                        y3 = yrEnd + xLT * math.sin(angle + right)
                    else:
                        x3 = x2 + xLT * math.cos(angle + right)
                        y3 = y2 + xLT * math.sin(angle + right)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                    if radius2 > 0:
                        if self.r2Button.child.get_text().startswith('iRadius'):
                            xrCentre = x3 + (radius2 * math.cos(angle + right))
                            yrCentre = y3 + (radius2 * math.sin(angle + right))
                            xrEnd = xrCentre + (radius2 * math.cos(angle + down))
                            yrEnd = yrCentre + (radius2 * math.sin(angle + down))
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        else:
                            xrCentre = x3 + (radius2 * math.cos(angle + down))
                            yrCentre = y3 + (radius2 * math.sin(angle + down))
                            xrEnd = xrCentre + (radius2 * math.cos(angle + right))
                            yrEnd = yrCentre + (radius2 * math.sin(angle + right))
                        if self.r2Button.child.get_text().startswith('Radius'):
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x4 = xrEnd + yLR * math.cos(angle + down)
                        y4 = yrEnd + yLR * math.sin(angle + down)
                    else:
                        x4 = x3 + yLR * math.cos(angle + down)
                        y4 = y3 + yLR * math.sin(angle + down)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                    if radius4 > 0:
                        if self.r4Button.child.get_text().startswith('iRadius'):
                            xrCentre = x4 + (radius4 * math.cos(angle + down))
                            yrCentre = y4 + (radius4 * math.sin(angle + down))
                            xrEnd = xrCentre + (radius4 * math.cos(angle + left))
                            yrEnd = yrCentre + (radius4 * math.sin(angle + left))
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                        else:
                            xrCentre = x4 + (radius4 * math.cos(angle + left))
                            yrCentre = y4 + (radius4 * math.sin(angle + left))
                            xrEnd = xrCentre + (radius4 * math.cos(angle + down))
                            yrEnd = yrCentre + (radius4 * math.sin(angle + down))
                        if self.r4Button.child.get_text().startswith('Radius'):
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                else:
                    x1 = xS + (float(self.xLEntry.get_text()) / 2 - radius4) * math.cos(angle + dir[2])
                    y1 = yS + (float(self.xLEntry.get_text()) / 2 - radius4) * math.sin(angle + dir[2])
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                    if radius4 > 0:
                        if self.r4Button.child.get_text().startswith('iRadius'):
                            xrCentre = x1 + (radius4 * math.cos(angle + right))
                            yrCentre = y1 + (radius4 * math.sin(angle + right))
                            xrEnd = xrCentre + (radius4 * math.cos(angle + up))
                            yrEnd = yrCentre + (radius4 * math.sin(angle + up))
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        else:
                            xrCentre = x1 + (radius4 * math.cos(angle + up))
                            yrCentre = y1 + (radius4 * math.sin(angle + up))
                            xrEnd = xrCentre + (radius4 * math.cos(angle + right))
                            yrEnd = yrCentre + (radius4 * math.sin(angle + right))
                        if self.r4Button.child.get_text().startswith('Radius'):
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x2 = xrEnd + yLR * math.cos(angle + up)
                        y2 = yrEnd + yLR * math.sin(angle + up)
                    else:
                        x2 = x1 + yLR * math.cos(angle + up)
                        y2 = y1 + yLR * math.sin(angle + up)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                    if radius2 > 0:
                        if self.r2Button.child.get_text().startswith('iRadius'):
                            xrCentre = x2 + (radius2 * math.cos(angle + up))
                            yrCentre = y2 + (radius2 * math.sin(angle + up))
                            xrEnd = xrCentre + (radius2 * math.cos(angle + left))
                            yrEnd = yrCentre + (radius2 * math.sin(angle + left))
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        else:
                            xrCentre = x2 + (radius2 * math.cos(angle + left))
                            yrCentre = y2 + (radius2 * math.sin(angle + left))
                            xrEnd = xrCentre + (radius2 * math.cos(angle + up))
                            yrEnd = yrCentre + (radius2 * math.sin(angle + up))
                        if self.r2Button.child.get_text().startswith('Radius'):
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x3 = xrEnd + xLT * math.cos(angle + left)
                        y3 = yrEnd + xLT * math.sin(angle + left)
                    else:
                        x3 = x2 + xLT * math.cos(angle + left)
                        y3 = y2 + xLT * math.sin(angle + left)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                    if radius1 > 0:
                        if self.r1Button.child.get_text().startswith('iRadius'):
                            xrCentre = x3 + (radius1 * math.cos(angle + left))
                            yrCentre = y3 + (radius1 * math.sin(angle + left))
                            xrEnd = xrCentre + (radius1 * math.cos(angle + down))
                            yrEnd = yrCentre + (radius1 * math.sin(angle + down))
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        else:
                            xrCentre = x3 + (radius1 * math.cos(angle + down))
                            yrCentre = y3 + (radius1 * math.sin(angle + down))
                            xrEnd = xrCentre + (radius1 * math.cos(angle + left))
                            yrEnd = yrCentre + (radius1 * math.sin(angle + left))
                        if self.r1Button.child.get_text().startswith('Radius'):
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                        x4 = xrEnd + yLL * math.cos(angle + down)
                        y4 = yrEnd + yLL * math.sin(angle + down)
                    else:
                        x4 = x3 + yLL * math.cos(angle + down)
                        y4 = y3 + yLL * math.sin(angle + down)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                    if radius3 > 0:
                        if self.r3Button.child.get_text().startswith('iRadius'):
                            xrCentre = x4 + (radius3 * math.cos(angle + down))
                            yrCentre = y4 + (radius3 * math.sin(angle + down))
                            xrEnd = xrCentre + (radius3 * math.cos(angle + right))
                            yrEnd = yrCentre + (radius3 * math.sin(angle + right))
                            outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                        else:
                            xrCentre = x4 + (radius3 * math.cos(angle + right))
                            yrCentre = y4 + (radius3 * math.sin(angle + right))
                            xrEnd = xrCentre + (radius3 * math.cos(angle + down))
                            yrEnd = yrCentre + (radius3 * math.sin(angle + down))
                        if self.r3Button.child.get_text().startswith('Radius'):
                            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                        else:
                            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
                if leadOutOffset > 0:
                    if self.outside.get_active():
                        dir = [down, left]
                    else:
                        dir = [up, right]
                    xlCentre = xS + (leadOutOffset * math.cos(angle + dir[0]))
                    ylCentre = yS + (leadOutOffset * math.sin(angle + dir[0]))
                    xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
                    ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
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
            msg  = 'A positive X Length is required\n\n'
            msg += 'and\n\n'
            msg += 'A positive Y Length is required\n'
            self.parent.dialog_error('RECTANGLE', msg)

    def rad_button_pressed(self, button, value):
        if button.child.get_text()[:3] == 'Rad':
            button.child.set_text('Chamfer {}'.format(value))
        elif button.child.get_text()[:3] == 'Cha':
            button.child.set_text('iRadius {}'.format(value))
        else:
            button.child.set_text('Radius {}'.format(value))
        self.auto_preview('local')

    def auto_preview(self, widget):
        if self.xLEntry.get_text() and self.yLEntry.get_text():
            self.rectangle_preview('auto') 

    def rectangle_show(self, parent, entries, fNgc, fNgcBkp, fTmp, rowS, xOrigin, yOrigin):
        entries.set_row_spacings(rowS)
        self.parent = parent
        for child in entries.get_children():
            entries.remove(child)
        self.fNgc = fNgc
        self.fNgcBkp = fNgcBkp
        self.fTmp = fTmp
        cutLabel = gtk.Label('Cut Type')
        cutLabel.set_alignment(0.95, 0.5)
        cutLabel.set_width_chars(8)
        entries.attach(cutLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        self.outside.connect('toggled', self.auto_preview)
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
        xLLabel = gtk.Label('X length')
        xLLabel.set_alignment(0.95, 0.5)
        xLLabel.set_width_chars(8)
        entries.attach(xLLabel, 0, 1, 6, 7)
        self.xLEntry = gtk.Entry()
        self.xLEntry.set_width_chars(8)
        self.xLEntry.connect('activate', self.auto_preview)
        self.xLEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.xLEntry, 1, 2, 6, 7)
        yLLabel = gtk.Label('Y length')
        yLLabel.set_alignment(0.95, 0.5)
        yLLabel.set_width_chars(8)
        entries.attach(yLLabel, 0, 1, 7, 8)
        self.yLEntry = gtk.Entry()
        self.yLEntry.set_width_chars(8)
        self.yLEntry.connect('activate', self.auto_preview)
        self.yLEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.yLEntry, 1, 2, 7, 8)
        angLabel = gtk.Label('Angle')
        angLabel.set_alignment(0.95, 0.5)
        angLabel.set_width_chars(8)
        entries.attach(angLabel, 0, 1, 8, 9)
        self.angEntry = gtk.Entry()
        self.angEntry.set_width_chars(8)
        self.angEntry.set_text('0')
        self.angEntry.connect('activate', self.auto_preview)
        self.angEntry.connect('changed', self.parent.entry_changed)
        entries.attach(self.angEntry, 1, 2, 8, 9)
        self.r1Button = gtk.Button('Radius 1')
        self.r1Button.connect('pressed', self.rad_button_pressed, '1')
        entries.attach(self.r1Button, 0, 1, 9, 10)
        self.r1Entry = gtk.Entry()
        self.r1Entry.set_width_chars(8)
        self.r1Entry.connect('activate', self.auto_preview)
        self.r1Entry.connect('changed', self.parent.entry_changed)
        entries.attach(self.r1Entry, 1, 2, 9, 10)
        self.r2Button = gtk.Button('Radius 2')
        self.r2Button.connect('pressed', self.rad_button_pressed, '2')
        entries.attach(self.r2Button, 2, 3, 9, 10)
        self.r2Entry = gtk.Entry()
        self.r2Entry.set_width_chars(8)
        self.r2Entry.connect('activate', self.auto_preview)
        self.r2Entry.connect('changed', self.parent.entry_changed)
        entries.attach(self.r2Entry, 3, 4, 9, 10)
        self.r3Button = gtk.Button('Radius 3')
        self.r3Button.connect('pressed', self.rad_button_pressed, '3')
        entries.attach(self.r3Button, 0, 1, 10, 11)
        self.r3Entry = gtk.Entry()
        self.r3Entry.set_width_chars(8)
        self.r3Entry.connect('activate', self.auto_preview)
        self.r3Entry.connect('changed', self.parent.entry_changed)
        entries.attach(self.r3Entry, 1, 2, 10, 11)
        self.r4Button = gtk.Button('Radius 4')
        self.r4Button.connect('pressed', self.rad_button_pressed, '4')
        entries.attach(self.r4Button, 2, 3, 10, 11)
        self.r4Entry = gtk.Entry()
        self.r4Entry.set_width_chars(8)
        self.r4Entry.connect('activate', self.auto_preview)
        self.r4Entry.connect('changed', self.parent.entry_changed)
        entries.attach(self.r4Entry, 3, 4, 10, 11)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.rectangle_preview)
        entries.attach(preview, 0, 1, 13, 14)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.parent.add_shape_to_file, self.add)
        entries.attach(self.add, 2, 3, 13, 14)
        undo = gtk.Button('Undo')
        undo.connect('pressed', self.parent.undo_shape, self.add)
        entries.attach(undo, 4, 5, 13, 14)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/rectangle.png', 
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
        self.xLEntry.grab_focus()
