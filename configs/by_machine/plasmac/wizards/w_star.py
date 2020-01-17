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

class star:

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
            self.c.program_open('blank.ngc')
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
                outNgc.write('{}\n'.format(self.preamble))
                outNgc.write('f#<_hal[plasmac.cut-feed-rate]>\n')
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
            outNgc.write('\n(postamble)\n')
            outNgc.write('{}\n'.format(self.postamble))
            outNgc.write('m30\n')
            outNgc.close()
            self.load_file(self.fNgc)
            self.add.set_sensitive(True)
            hal.set_p('plasmac_run.preview-tab', '1')
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
            self.dialog_error(msg)

    def mode_changed(self, widget):
        if self.mCombo.get_active() == 2:
            self.dLabel.set_text('Side Length')
        else:
            self.dLabel.set_text('Diameter')

    def do_star(self, fWizard, tmpDir):
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.W = gtk.Dialog('Star',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        t = gtk.Table(1, 1, True)
        t.set_row_spacings(6)
        self.W.vbox.add(t)
        cLabel = gtk.Label('Cut Type')
        cLabel.set_alignment(0.95, 0.5)
        cLabel.set_width_chars(10)
        t.attach(cLabel, 0, 1, 0, 1)
        self.outside = gtk.RadioButton(None, 'Outside')
        t.attach(self.outside, 1, 2, 0, 1)
        inside = gtk.RadioButton(self.outside, 'Inside')
        t.attach(inside, 2, 3, 0, 1)
        oLabel = gtk.Label('Offset')
        oLabel.set_alignment(0.95, 0.5)
        oLabel.set_width_chars(10)
        t.attach(oLabel, 3, 4, 0, 1)
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
        bLeft = gtk.RadioButton(self.centre, 'Bottom Left')
        t.attach(bLeft, 0, 1, 5, 6)
        pLabel = gtk.Label('# of Points')
        pLabel.set_alignment(0.95, 0.5)
        pLabel.set_width_chars(10)
        t.attach(pLabel, 0, 1, 6, 7)
        self.pEntry = gtk.Entry()
        self.pEntry.set_width_chars(10)
        t.attach(self.pEntry, 1, 2, 6, 7)
        self.odLabel = gtk.Label('Outside Dia')
        self.odLabel.set_alignment(0.95, 0.5)
        self.odLabel.set_width_chars(10)
        t.attach(self.odLabel, 0, 1, 7, 8)
        self.odEntry = gtk.Entry()
        self.odEntry.set_width_chars(10)
        t.attach(self.odEntry, 1, 2, 7, 8)
        self.idLabel = gtk.Label('Inside Dia')
        self.idLabel.set_alignment(0.95, 0.5)
        self.idLabel.set_width_chars(10)
        t.attach(self.idLabel, 0, 1, 8, 9)
        self.idEntry = gtk.Entry()
        self.idEntry.set_width_chars(10)
        t.attach(self.idEntry, 1, 2, 8, 9)
        aLabel = gtk.Label('Angle')
        aLabel.set_alignment(0.95, 0.5)
        aLabel.set_width_chars(10)
        t.attach(aLabel, 0, 1, 9, 10)
        self.aEntry = gtk.Entry()
        self.aEntry.set_width_chars(10)
        self.aEntry.set_text('0')
        t.attach(self.aEntry, 1, 2, 9, 10)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.send_preview)
        t.attach(preview, 0, 1, 11, 12)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        t.attach(self.add, 2, 3, 11, 12)
        end = gtk.Button('Return')
        end.connect('pressed', self.end_this_shape)
        t.attach(end, 4, 5, 11, 12)
        pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/star.png', 
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
                elif line.startswith('lead-in'):
                    self.liEntry.set_text(line.strip().split('=')[1])
                elif line.startswith('lead-out'):
                    self.loEntry.set_text(line.strip().split('=')[1])
        response = self.W.run()
