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

class rectangle:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
            gtk.DIALOG_DESTROY_WITH_PARENT, gtk.MESSAGE_ERROR, 
            gtk.BUTTONS_CLOSE, error)
        md.run()
        md.destroy()

    def load_file(self, fName):
        if self.gui == 'axis':
            Popen('axis-remote {}'.format(fName), stdout = PIPE, shell = True)
        elif self.gui == 'gmoccapy':
            self.c = linuxcnc.command()
            self.c.program_open('./wizards/blank.ngc')
            self.c.program_open(fName)
        else:
            print('Unknown GUI in .ini file')

    def end_this_shape(self, event):
        if os.path.exists(self.fWizard):
            outWiz = open(self.fWizard, 'a+')
            post = False
            for line in outWiz:
                if '(postamble)' in line:
                    post = True
            if not post:
                outWiz.write('\n(postamble)\n')
                outWiz.write('{}\n'.format(self.postamble))
                outWiz.write('m30\n')
            outWiz.close()
            self.load_file(self.fWizard)
        self.W.destroy()
        return None

    def add_shape_to_file(self, event):
        if os.path.exists(self.fWizard):
            path = os.path.dirname(os.path.abspath(self.fWizard))
            tmp = ('{}/tmp'.format(path))
            shutil.copyfile(self.fWizard, tmp)
            inWiz = open(tmp, 'r')
            outWiz = open(self.fWizard, 'w')
            for line in inWiz:
                if '(postamble)' in line:
                    break
                outWiz.write(line)
            inWiz.close()
            outWiz.close()
            os.remove(tmp)
            inTmp = open(self.fTmp, 'r')
            outWiz = open(self.fWizard, 'a')
            for line in inTmp:
                outWiz.write(line)
        else:
            inTmp = open(self.fTmp, 'r')
            outWiz = open(self.fWizard, 'w')
            outWiz.write('(preamble)\n')
            outWiz.write('{}\n'.format(self.preamble))
            outWiz.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            for line in inTmp:
                outWiz.write(line)
        inTmp.close()
        outWiz.close()
        self.add.set_sensitive(False)

    def send_preview(self, event):
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
                    self.dialog_error(msg)
                    return
            except:
                msg = 'Invalid X Length or Y Length\n'
                self.dialog_error(msg)
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
                self.dialog_error(msg)
                return
            if radius1 + radius3 > float(self.yLEntry.get_text()):
                msg  = 'Radius 1 plus Radius 3 ({})\n\n'.format(radius1 + radius3)
                msg += 'can not be greater than {}\n'.format(float(self.yLEntry.get_text()))
                self.dialog_error(msg)
                return
            if radius2 + radius4 > float(self.yLEntry.get_text()):
                msg  = 'Radius 2 plus Radius 4 ({})\n\n'.format(radius2 + radius4)
                msg += 'can not be greater than {}\n'.format(float(self.yLEntry.get_text()))
                self.dialog_error(msg)
                return
            if radius3 > float(self.xLEntry.get_text()) / 2 or radius4 > float(self.xLEntry.get_text()) / 2:
                msg  = 'Neither Radius 3 nor Radius 4\n\n'
                msg += 'can be greater than {}\n'.format(float(self.xLEntry.get_text()) / 2)
                self.dialog_error(msg)
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
                self.fTmp = '{}/shape.tmp'.format(self.tmpDir)
                self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
                outTmp = open(self.fTmp, 'w')
                outNgc = open(self.fNgc, 'w')
                if os.path.exists(self.fWizard):
                    inWiz = open(self.fWizard, 'r')
                    for line in inWiz:
                        if '(postamble)' in line:
                            break
                        outNgc.write(line)
                else:
                    outNgc.write('(preamble)\n')
                    outNgc.write('{}\n'.format(self.preamble))
                    outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
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
                        xrCentre = x1 + (radius3 * math.cos(angle + up))
                        yrCentre = y1 + (radius3 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + left))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        x2 = xrEnd + yLL * math.cos(angle + up)
                        y2 = yrEnd + yLL * math.sin(angle + up)
                    else:
                        x2 = x1 + yLL * math.cos(angle + up)
                        y2 = y1 + yLL * math.sin(angle + up)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                    if radius1 > 0:
                        xrCentre = x2 + (radius1 * math.cos(angle + right))
                        yrCentre = y2 + (radius1 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + up))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        x3 = xrEnd + xLT * math.cos(angle + right)
                        y3 = yrEnd + xLT * math.sin(angle + right)
                    else:
                        x3 = x2 + xLT * math.cos(angle + right)
                        y3 = y2 + xLT * math.sin(angle + right)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                    if radius2 > 0:
                        xrCentre = x3 + (radius2 * math.cos(angle + down))
                        yrCentre = y3 + (radius2 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + right))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        x4 = xrEnd + yLR * math.cos(angle + down)
                        y4 = yrEnd + yLR * math.sin(angle + down)
                    else:
                        x4 = x3 + yLR * math.cos(angle + down)
                        y4 = y3 + yLR * math.sin(angle + down)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                    if radius4 > 0:
                        xrCentre = x4 + (radius4 * math.cos(angle + left))
                        yrCentre = y4 + (radius4 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + down))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                else:
                    x1 = xS + (float(self.xLEntry.get_text()) / 2 - radius4) * math.cos(angle + dir[2])
                    y1 = yS + (float(self.xLEntry.get_text()) / 2 - radius4) * math.sin(angle + dir[2])
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                    if radius4 > 0:
                        xrCentre = x1 + (radius4 * math.cos(angle + up))
                        yrCentre = y1 + (radius4 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + right))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                        x2 = xrEnd + yLR * math.cos(angle + up)
                        y2 = yrEnd + yLR * math.sin(angle + up)
                    else:
                        x2 = x1 + yLR * math.cos(angle + up)
                        y2 = y1 + yLR * math.sin(angle + up)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                    if radius2 > 0:
                        xrCentre = x2 + (radius2 * math.cos(angle + left))
                        yrCentre = y2 + (radius2 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + up))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                        x3 = xrEnd + xLT * math.cos(angle + left)
                        y3 = yrEnd + xLT * math.sin(angle + left)
                    else:
                        x3 = x2 + xLT * math.cos(angle + left)
                        y3 = y2 + xLT * math.sin(angle + left)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                    if radius1 > 0:
                        xrCentre = x3 + (radius1 * math.cos(angle + down))
                        yrCentre = y3 + (radius1 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + left))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                        x4 = xrEnd + yLL * math.cos(angle + down)
                        y4 = yrEnd + yLL * math.sin(angle + down)
                    else:
                        x4 = x3 + yLL * math.cos(angle + down)
                        y4 = y3 + yLL * math.sin(angle + down)
                    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                    if radius3 > 0:
                        xrCentre = x4 + (radius3 * math.cos(angle + right))
                        yrCentre = y4 + (radius3 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + down))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
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
                outNgc.write('\n(postamble)\n')
                outNgc.write('{}\n'.format(self.postamble))
                outNgc.write('m30\n')
                outNgc.close()
                self.load_file(self.fNgc)
                self.add.set_sensitive(True)
                hal.set_p('plasmac_run.preview-tab', '1')
        else:
            msg  = 'A positive X Length is required\n\n'
            msg += 'and\n\n'
            msg += 'A positive Y Length is required\n'
            self.dialog_error(msg)

    def do_rectangle(self, fWizard, tmpDir):
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.W = gtk.Dialog('Rectangle',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        t.set_row_spacings(6)
        self.W.vbox.add(t)
        cutLabel = gtk.Label('Cut Type')
        cutLabel.set_alignment(0.95, 0.5)
        cutLabel.set_width_chars(10)
        t.attach(cutLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        t.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        t.attach(inside, 2, 3, 0, 1)
        offsetLabel = gtk.Label('Offset')
        offsetLabel.set_alignment(0.95, 0.5)
        offsetLabel.set_width_chars(10)
        t.attach(offsetLabel, 3, 4, 0, 1)
        self.offset = gtk.CheckButton('Kerf Width')
        t.attach(self.offset, 4, 5, 0, 1)
        lLabel = gtk.Label('Lead In')
        lLabel.set_alignment(0.95, 0.5)
        lLabel.set_width_chars(10)
        t.attach(lLabel, 0, 1, 1, 2)
        self.liEntry = gtk.Entry()
        self.liEntry.set_width_chars(10)
        t.attach(self.liEntry, 1, 2, 1, 2)
        loLabel = gtk.Label('Lead Out')
        loLabel.set_alignment(0.95, 0.5)
        loLabel.set_width_chars(10)
        t.attach(loLabel, 0, 1, 2, 3)
        self.loEntry = gtk.Entry()
        self.loEntry.set_width_chars(10)
        t.attach(self.loEntry, 1, 2, 2, 3)
        xSLabel = gtk.Label('X start')
        xSLabel.set_alignment(0.95, 0.5)
        xSLabel.set_width_chars(10)
        t.attach(xSLabel, 0, 1, 3, 4)
        self.xSEntry = gtk.Entry()
        self.xSEntry.set_width_chars(10)
        t.attach(self.xSEntry, 1, 2, 3, 4)
        ySLabel = gtk.Label('Y start')
        ySLabel.set_alignment(0.95, 0.5)
        ySLabel.set_width_chars(10)
        t.attach(ySLabel, 0, 1, 4, 5)
        self.ySEntry = gtk.Entry()
        self.ySEntry.set_width_chars(10)
        t.attach(self.ySEntry, 1, 2, 4, 5)
        self.centre = gtk.RadioButton(None, 'Centre')
        t.attach(self.centre, 1, 2, 5, 6)
        self.bLeft = gtk.RadioButton(self.centre, 'Bottom Left')
        t.attach(self.bLeft, 0, 1, 5, 6)
        xLLabel = gtk.Label('X length')
        xLLabel.set_alignment(0.95, 0.5)
        xLLabel.set_width_chars(10)
        t.attach(xLLabel, 0, 1, 6, 7)
        self.xLEntry = gtk.Entry()
        self.xLEntry.set_width_chars(10)
        t.attach(self.xLEntry, 1, 2, 6, 7)
        yLLabel = gtk.Label('Y length')
        yLLabel.set_alignment(0.95, 0.5)
        yLLabel.set_width_chars(10)
        t.attach(yLLabel, 0, 1, 7, 8)
        self.yLEntry = gtk.Entry()
        self.yLEntry.set_width_chars(10)
        t.attach(self.yLEntry, 1, 2, 7, 8)
        angLabel = gtk.Label('Angle')
        angLabel.set_alignment(0.95, 0.5)
        angLabel.set_width_chars(10)
        t.attach(angLabel, 0, 1, 8, 9)
        self.angEntry = gtk.Entry()
        self.angEntry.set_width_chars(10)
        self.angEntry.set_text('0')
        t.attach(self.angEntry, 1, 2, 8, 9)
        r1Label = gtk.Label('Radius 1')
        r1Label.set_alignment(0.95, 0.5)
        r1Label.set_width_chars(10)
        t.attach(r1Label, 0, 1, 9, 10)
        self.r1Entry = gtk.Entry()
        self.r1Entry.set_width_chars(10)
        self.r1Entry.set_text('0')
        t.attach(self.r1Entry, 1, 2, 9, 10)
        r2Label = gtk.Label('Radius 2')
        r2Label.set_alignment(0.95, 0.5)
        r2Label.set_width_chars(10)
        t.attach(r2Label, 2, 3, 9, 10)
        self.r2Entry = gtk.Entry()
        self.r2Entry.set_width_chars(10)
        self.r2Entry.set_text('0')
        t.attach(self.r2Entry, 3, 4, 9, 10)
        r3Label = gtk.Label('Radius 3')
        r3Label.set_alignment(0.95, 0.5)
        r3Label.set_width_chars(10)
        t.attach(r3Label, 0, 1, 10, 11)
        self.r3Entry = gtk.Entry()
        self.r3Entry.set_width_chars(10)
        self.r3Entry.set_text('0')
        t.attach(self.r3Entry, 1, 2, 10, 11)
        r4Label = gtk.Label('Radius 4')
        r4Label.set_alignment(0.95, 0.5)
        r4Label.set_width_chars(10)
        t.attach(r4Label, 2, 3, 10, 11)
        self.r4Entry = gtk.Entry()
        self.r4Entry.set_width_chars(10)
        self.r4Entry.set_text('0')
        t.attach(self.r4Entry, 3, 4, 10, 11)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.send_preview)
        t.attach(preview, 0, 1, 12, 13)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        t.attach(self.add, 2, 3, 12, 13)
        end = gtk.Button('Return')
        end.connect('pressed', self.end_this_shape)
        t.attach(end, 4, 5, 12, 13)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/rectangle.png', 
                width=240, 
                height=240)
        image = gtk.Image()
        image.set_from_pixbuf(pixbuf)
        t.attach(image, 2, 5, 1, 9)
        self.xSEntry.grab_focus()
        self.W.show_all()
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
        response = self.W.run()
