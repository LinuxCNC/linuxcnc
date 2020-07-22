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

class circle:

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
        self.check_entries()
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
            self.fTmp = '{}/shape.tmp'.format(self.tmpDir)
            self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
            outTmp = open(self.fTmp, 'w')
            outNgc = open(self.fNgc, 'w')
            if sHole:
                speed = 0.6
            else:
                speed = 1.0
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
            outNgc.write('\n(postamble)\n')
            outNgc.write('{}\n'.format(self.postamble))
            outNgc.write('m30\n')
            outNgc.close()
            self.load_file(self.fNgc)
            self.add.set_sensitive(True)
            hal.set_p('plasmac_run.preview-tab', '1')
        else:
            self.dialog_error('Diameter is required')

    def over_cut(self, lastX, lastY, IJ, radius, outTmp):
        scale = 0.039370 if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else 1.0
        try:
            oclength = float(self.ocEntry.get_text())
        except:
            oclength = 4 * scale
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

    def outside_toggled(self,widget):
        if widget.get_active():
            self.overcut.set_active(False)
            self.ocEntry.set_text('')
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

    def ocentry_changed(self,widget):
        if self.outside.get_active():
            self.ocEntry.set_text('')

    def check_entries(self):
        try:
            rad = float(self.dEntry.get_text()) / 2
        except:
            rad = 0
        if rad > self.sRadius:
            self.overcut.set_active(False)
        if rad <= self.sRadius and self.overcut.get_active():
            self.loEntry.set_text('')

    def diameter_changed(self,widget):
        try:
            rad = float(self.dEntry.get_text()) / 2
        except:
            rad = 0
        if rad > self.sRadius:
            self.overcut.set_active(False)
            self.ocEntry.set_text('')
            self.overcut.set_sensitive(False)
            self.ocEntry.set_sensitive(False)
        else:
            if not self.outside.get_active():
                self.overcut.set_sensitive(True)
                self.ocEntry.set_sensitive(True)

    def do_circle(self, fWizard, tmpDir):
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.sRadius = 0.0
        self.hSpeed = 100
        self.W = gtk.Dialog('Circle',
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
        self.outside.connect('toggled',self.outside_toggled)
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
        dLabel = gtk.Label('Diameter')
        dLabel.set_alignment(0.95, 0.5)
        dLabel.set_width_chars(10)
        t.attach(dLabel, 0, 1, 6, 7)
        self.dEntry = gtk.Entry()
        self.dEntry.set_width_chars(10)
        self.dEntry.connect('changed', self.diameter_changed)
        t.attach(self.dEntry, 1, 2, 6, 7)
        ocDesc = gtk.Label('Over Cut')
        ocDesc.set_alignment(0.95, 0.5)
        ocDesc.set_width_chars(10)
        t.attach(ocDesc, 0, 1, 7, 8)
        self.overcut = gtk.CheckButton('')
        self.overcut.connect('toggled', self.overcut_toggled)
        self.overcut.set_sensitive(False)
        t.attach(self.overcut, 1, 2, 7, 8)
        ocLabel = gtk.Label('OC Length')
        ocLabel.set_alignment(0.95, 0.5)
        ocLabel.set_width_chars(10)
        t.attach(ocLabel, 0, 1, 8, 9)
        self.ocEntry = gtk.Entry()
        self.ocEntry.set_width_chars(10)
        self.ocEntry.set_sensitive(False)
        self.ocEntry.connect('changed', self.ocentry_changed)
        t.attach(self.ocEntry, 1, 2, 8, 9)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.send_preview)
        t.attach(preview, 0, 1, 9, 10)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        t.attach(self.add, 2, 3, 9, 10)
        end = gtk.Button('Return')
        end.connect('pressed', self.end_this_shape)
        t.attach(end, 4, 5, 9, 10)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/circle.png', 
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
                elif line.startswith('hole-diameter'):
                    self.sRadius = float(line.strip().split('=')[1]) / 2
                elif line.startswith('hole-speed'):
                    self.hSpeed = float(line.strip().split('=')[1])
        response = self.W.run()
