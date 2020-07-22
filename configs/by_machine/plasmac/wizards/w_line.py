#!/usr/bin/env python

'''
w_line.py

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
import numpy
from subprocess import Popen,PIPE

class line:

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
        os.remove(self.fTmp)
        os.remove(self.fNgc)
        self.add.set_sensitive(False)
        self.cont.set_sensitive(False)
        self.add_segment = 0
        self.line_type_changed(self.lType)

    def continue_shape(self, event):
        self.xS = self.xE
        self.yS = self.yE
        self.cont.set_sensitive(False)
        self.add.set_sensitive(False)
        self.add_segment = 1
        self.line_type_changed(self.lType)

    def send_preview(self, event):
        if self.add_segment == 0:
            try:
                self.xS = float(self.entry1.get_text())
                self.yS = float(self.entry2.get_text())
            except:
                msg  = 'Invalid entry detected\n'
                self.dialog_error(msg)
                return
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
            outTmp.write('\n(wizard line)\n')
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(self.xS, self.yS))
            outTmp.write('m3 $0 s1\n')
            try:
                if self.lType.get_active_text() == 'line point to point':
                    self.do_line_point_to_point(float(self.entry4.get_text()), float(self.entry5.get_text()))
                elif self.lType.get_active_text() == 'line by angle':
                    if not float(self.entry4.get_text()):
                        raise Exception('length cannot be 0')
                    self.do_line_by_angle(float(self.entry4.get_text()), float(self.entry5.get_text()))
                elif self.lType.get_active_text() == 'arc 3 points':
                    self.do_arc_3_points(float(self.entry4.get_text()), float(self.entry5.get_text()), \
                                         float(self.entry7.get_text()), float(self.entry8.get_text()))
                elif self.lType.get_active_text() == 'arc 2 points & radius':
                    self.do_arc_2_points_radius(float(self.entry4.get_text()), float(self.entry5.get_text()), \
                                         float(self.entry7.get_text()))
                elif self.lType.get_active_text() == 'arc by angle & radius':
                    self.do_arc_by_angle_radius(float(self.entry4.get_text()), float(self.entry5.get_text()), \
                                         float(self.entry7.get_text()))
            except Exception as e:
                msg  = 'Invalid new entry detected\n\n'
                msg += str(e)
                self.dialog_error(msg)
                outNgc.close()
                outTmp.close()
                return
        elif self.add_segment >= 1:
            self.fTmp = '{}/shape.tmp'.format(self.tmpDir)
            self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
            inTmp = open(self.fNgc, 'r')
            outTmp = open(self.fTmp, 'w')
            while(1):
                line = inTmp.readline()
                if line == self.gcodeLine:
                    break
                else:
                    outTmp.write(line)
            if self.add_segment == 1:
                outTmp.write(line)
                while(1):
                    line = inTmp.readline()
                    if 'M5 $0' in line:
                        break
                    else:
                        outTmp.write(line)
            inTmp.close()
            shutil.copyfile(self.fTmp, self.fNgc)
            outNgc = open(self.fNgc, 'w')
            try:
                if self.lType.get_active_text() == 'line point to point':
                    self.do_line_point_to_point(float(self.entry1.get_text()), float(self.entry2.get_text()))
                elif self.lType.get_active_text() == 'line by angle':
                    if not float(self.entry1.get_text()):
                        raise Exception('Length cannot be 0')
                    self.do_line_by_angle(float(self.entry1.get_text()), float(self.entry2.get_text()))
                elif self.lType.get_active_text() == 'arc 3 points':
                    self.do_arc_3_points(float(self.entry1.get_text()), float(self.entry2.get_text()), \
                                         float(self.entry4.get_text()), float(self.entry5.get_text()))
                elif self.lType.get_active_text() == 'arc 2 points & radius':
                    self.do_arc_2_points_radius(float(self.entry1.get_text()), float(self.entry2.get_text()), \
                                         float(self.entry4.get_text()))
                elif self.lType.get_active_text() == 'arc by angle & radius':
                    self.do_arc_by_angle_radius(float(self.entry1.get_text()), float(self.entry2.get_text()), \
                                         float(self.entry4.get_text()))
            except Exception as e:
                msg  = 'Invalid add entry detected\n\n'
                msg += str(e)
                self.dialog_error(msg)
                outNgc.close()
                outTmp.close()
                return
        outTmp.write(self.gcodeLine)
        outTmp.write('M5 $0\n')
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
        self.cont.set_sensitive(True)
        if self.add_segment == 1:
            self.add_segment = 2
        hal.set_p('plasmac_run.preview-tab', '1')

    def do_line_point_to_point(self, inX, inY):
        self.xE = inX
        self.yE = inY
        self.gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(self.xE, self.yE)

    def do_line_by_angle(self, inL, inA):
        angle = math.radians(inA)
        self.xE = self.xS + (inL * math.cos(angle))
        self.yE = self.yS + (inL * math.sin(angle))
        self.gcodeLine = 'g1 x{:.6f} y{:.6f}\n'.format(self.xE, self.yE)

    def do_arc_3_points(self, inX1, inY1, inXE, inYE):
        self.xE = inXE
        self.yE = inYE
        A = numpy.array([self.xS, self.yS, 0.0])
        B = numpy.array([inX1, inY1, 0.0])
        C = numpy.array([inXE, inYE, 0.0])
        a = numpy.linalg.norm(C - B)
        b = numpy.linalg.norm(C - A)
        c = numpy.linalg.norm(B - A)
        s = (a + b + c) / 2
        R = a*b*c / 4 / numpy.sqrt(s * (s - a) * (s - b) * (s - c))
        b1 = a*a * (b*b + c*c - a*a)
        b2 = b*b * (a*a + c*c - b*b)
        b3 = c*c * (a*a + b*b - c*c)
        P = numpy.column_stack((A, B, C)).dot(numpy.hstack((b1, b2, b3)))
        P /= b1 + b2 + b3
        G = '3' if (inX1-self.xS)*(self.yE-self.yS)-(inY1-self.yS)*(self.xE-self.xS) > 0 else '2'
        self.gcodeLine = 'g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(G, self.xE, self.yE, P[0] - self.xS, P[1] - self.yS)

    def do_arc_2_points_radius(self, inXE, inYE, radius):
        self.xE = inXE
        self.yE = inYE
        dir = math.radians(270) if radius > 0 else math.radians(90)
        arcType = '2' if self.g2Arc.get_active() else '3'
        height = math.sqrt((self.xE - self.xS) ** 2 + (self.yE - self.yS) ** 2) * 0.5
        length = math.sqrt((radius ** 2) - (height ** 2))
        angle = math.atan2((self.yE - self.yS), (self.xE - self.xS))
        xLineCentre = (self.xS + self.xE) / 2
        yLineCentre = (self.yS + self.yE) / 2
        xArcCentre = xLineCentre + length * math.cos(angle + dir)
        yArcCentre = yLineCentre + length * math.sin(angle + dir)
        self.gcodeLine = 'g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(arcType, self.xE, self.yE, xArcCentre - self.xS, yArcCentre - self.yS)

    def do_arc_by_angle_radius(self, inL, inA, inR):
        angle = math.radians(inA)
        xE = self.xS + (inL * math.cos(angle))
        yE = self.yS + (inL * math.sin(angle))
        self.do_arc_2_points_radius(xE, yE, inR)

    def line_type_changed(self, widget):
        if widget.get_active_text() == 'line point to point':
            self.set_line_point_to_point()
        elif widget.get_active_text() == 'line by angle':
            self.set_line_by_angle()
        elif widget.get_active_text() == 'arc 3 points':
            self.set_arc_3_points()
        elif widget.get_active_text() == 'arc 2 points & radius':
            self.set_arc_2_points_radius()
        elif widget.get_active_text() == 'arc by angle & radius':
            self.set_arc_by_angle_radius()
            
    def set_line_point_to_point(self):
        self.image.set_from_pixbuf(self.pixLinePoint)
        if self.add_segment > 0:
            self.entry1.grab_focus()
            self.label1.set_text('End X')
            self.entry1.set_text('')
            self.label2.set_text('End Y')
            self.entry2.set_text('')
            self.label3.hide()
            self.entry3.hide()
            self.label4.hide()
            self.entry4.hide()
            self.label5.hide()
            self.entry5.hide()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry8.hide()
            self.g3Arc.hide()
            self.g2Arc.hide()
        else:
            self.entry4.grab_focus()
            self.label1.set_text('Start X')
            self.entry1.set_text(self.xOrigin)
            self.label2.set_text('Start Y')
            self.entry2.set_text(self.yOrigin)
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('End X')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('End Y')
            self.label5.show()
            self.entry5.set_text('')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry8.hide()
            self.g3Arc.hide()
            self.g2Arc.hide()

    def set_line_by_angle(self):
        self.image.set_from_pixbuf(self.pixLineAngle)
        if self.add_segment > 0:
            self.entry1.grab_focus()
            self.label1.set_text('Length')
            self.entry1.set_text('')
            self.label2.set_text('Angle')
            self.entry2.set_text('0')
            self.label3.hide()
            self.entry3.hide()
            self.label4.hide()
            self.entry4.hide()
            self.label5.hide()
            self.entry5.hide()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry8.hide()
            self.g3Arc.hide()
            self.g2Arc.hide()
        else:
            self.entry4.grab_focus()
            self.label1.set_text('Start X')
            self.entry1.set_text(self.xOrigin)
            self.label2.set_text('Start Y')
            self.entry2.set_text(self.yOrigin)
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('Length')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('Angle')
            self.label5.show()
            self.entry5.set_text('0')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry8.hide()
            self.g3Arc.hide()
            self.g2Arc.hide()

    def set_arc_3_points(self):
        self.image.set_from_pixbuf(self.pixArc3p)
        if self.add_segment > 0:
            self.entry1.grab_focus()
            self.label1.set_text('Next X')
            self.entry1.set_text('')
            self.label2.set_text('Next Y')
            self.entry2.set_text('')
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('End X')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('End Y')
            self.label5.show()
            self.entry5.set_text('')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry8.hide()
            self.g3Arc.hide()
            self.g2Arc.hide()
        else:
            self.entry4.grab_focus()
            self.label1.set_text('Start X')
            self.entry1.set_text(self.xOrigin)
            self.label2.set_text('Start Y')
            self.entry2.set_text(self.yOrigin)
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('Next X')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('Next Y')
            self.label5.show()
            self.entry5.set_text('')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.set_text('End X')
            self.label7.show()
            self.entry7.set_text('')
            self.entry7.show()
            self.label8.set_text('End Y')
            self.label8.show()
            self.entry8.set_text('')
            self.entry8.show()
            self.g3Arc.hide()
            self.g2Arc.hide()

    def set_arc_2_points_radius(self):
        self.image.set_from_pixbuf(self.pixArc2pr)
        if self.add_segment > 0:
            self.entry1.grab_focus()
            self.label1.set_text('End X')
            self.entry1.set_text('')
            self.label2.set_text('End Y')
            self.entry2.set_text('')
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('Radius')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.hide()
            self.entry5.hide()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry7.hide()
            self.t.remove(self.g3Arc)
            self.t.remove(self.g2Arc)
            self.t.attach(self.g3Arc, 0, 1, 5, 6)
            self.t.attach(self.g2Arc, 1, 2, 5, 6)
            self.g3Arc.show()
            self.g2Arc.show()
        else:
            self.entry4.grab_focus()
            self.label1.set_text('Start X')
            self.entry1.set_text(self.xOrigin)
            self.label2.set_text('Start Y')
            self.entry2.set_text(self.yOrigin)
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('End X')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('End Y')
            self.label5.show()
            self.entry5.set_text('')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.set_text('Radius')
            self.label7.show()
            self.entry7.set_text('')
            self.entry7.show()
            self.label8.hide()
            self.entry8.hide()
            self.t.remove(self.g3Arc)
            self.t.remove(self.g2Arc)
            self.t.attach(self.g3Arc, 0, 1, 8, 9)
            self.t.attach(self.g2Arc, 1, 2, 8, 9)
            self.g3Arc.show()
            self.g2Arc.show()

    def set_arc_by_angle_radius(self):
        self.image.set_from_pixbuf(self.pixArcAngle)
        if self.add_segment > 0:
            self.entry1.grab_focus()
            self.label1.set_text('Length')
            self.entry1.set_text('')
            self.label2.set_text('Angle')
            self.entry2.set_text('0')
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('Radius')
            self.entry4.set_text('')
            self.label4.show()
            self.entry4.show()
            self.label5.hide()
            self.entry5.hide()
            self.label6.hide()
            self.entry6.hide()
            self.label7.hide()
            self.entry7.hide()
            self.label8.hide()
            self.entry7.hide()
            self.t.remove(self.g3Arc)
            self.t.remove(self.g2Arc)
            self.t.attach(self.g3Arc, 0, 1, 5, 6)
            self.t.attach(self.g2Arc, 1, 2, 5, 6)
            self.g3Arc.show()
            self.g2Arc.show()
        else:
            self.entry4.grab_focus()
            self.label1.set_text('Start X')
            self.entry1.set_text(self.xOrigin)
            self.label2.set_text('Start Y')
            self.entry2.set_text(self.yOrigin)
            self.label3.hide()
            self.entry3.hide()
            self.label4.set_text('Length')
            self.label4.show()
            self.entry4.set_text('')
            self.entry4.show()
            self.label5.set_text('Angle')
            self.label5.show()
            self.entry5.set_text('0')
            self.entry5.show()
            self.label6.hide()
            self.entry6.hide()
            self.label7.set_text('Radius')
            self.label7.show()
            self.entry7.set_text('')
            self.entry7.show()
            self.label8.hide()
            self.entry8.hide()
            self.t.remove(self.g3Arc)
            self.t.remove(self.g2Arc)
            self.t.attach(self.g3Arc, 0, 1, 8, 9)
            self.t.attach(self.g2Arc, 1, 2, 8, 9)
            self.g3Arc.show()
            self.g2Arc.show()

    def do_line(self, fWizard, tmpDir):
        self.add_segment = 0
        self.tmpDir = tmpDir
        self.fWizard = fWizard
        self.W = gtk.Dialog('Lines and Arcs',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        self.W.set_default_size(250, 200)
        self.t = gtk.Table(1, 1, True)
        self.t.set_row_spacings(6)
        self.W.vbox.add(self.t)
        self.lType = gtk.combo_box_new_text()
        self.lType.append_text('line point to point')
        self.lType.append_text('line by angle')
        self.lType.append_text('arc 3 points')
        self.lType.append_text('arc 2 points & radius')
        self.lType.append_text('arc by angle & radius')
        self.lType.set_active(0)
        self.lType.connect('changed', self.line_type_changed)
        self.t.attach(self.lType, 0, 2, 0, 1)
        self.label1 = gtk.Label()
        self.label1.set_alignment(0.95, 0.5)
        self.label1.set_width_chars(10)
        self.t.attach(self.label1, 0, 1, 1, 2)
        self.entry1 = gtk.Entry()
        self.entry1.set_width_chars(10)
        self.t.attach(self.entry1, 1, 2, 1, 2)
        self.label2 = gtk.Label()
        self.label2.set_alignment(0.95, 0.5)
        self.label2.set_width_chars(10)
        self.t.attach(self.label2, 0, 1, 2, 3)
        self.entry2 = gtk.Entry()
        self.entry2.set_width_chars(10)
        self.t.attach(self.entry2, 1, 2, 2, 3)
        self.label3 = gtk.Label()
        self.label3.set_alignment(0.95, 0.5)
        self.label3.set_width_chars(10)
        self.t.attach(self.label3, 0, 1, 3, 4)
        self.entry3 = gtk.Entry()
        self.entry3.set_width_chars(10)
        self.t.attach(self.entry3, 1, 2, 3, 4)
        self.label4 = gtk.Label()
        self.label4.set_alignment(0.95, 0.5)
        self.label4.set_width_chars(10)
        self.t.attach(self.label4, 0, 1, 4, 5)
        self.entry4 = gtk.Entry()
        self.entry4.set_width_chars(10)
        self.t.attach(self.entry4, 1, 2, 4, 5)
        self.label5 = gtk.Label()
        self.label5.set_alignment(0.95, 0.5)
        self.label5.set_width_chars(10)
        self.t.attach(self.label5, 0, 1, 5, 6)
        self.entry5 = gtk.Entry()
        self.entry5.set_width_chars(10)
        self.t.attach(self.entry5, 1, 2, 5, 6)
        self.label6 = gtk.Label()
        self.label6.set_alignment(0.95, 0.5)
        self.label6.set_width_chars(10)
        self.t.attach(self.label6, 0, 1, 6, 7)
        self.entry6 = gtk.Entry()
        self.entry6.set_width_chars(10)
        self.t.attach(self.entry6, 1, 2, 6, 7)
        self.label7 = gtk.Label()
        self.label7.set_alignment(0.95, 0.5)
        self.label7.set_width_chars(10)
        self.t.attach(self.label7, 0, 1, 7, 8)
        self.entry7 = gtk.Entry()
        self.entry7.set_width_chars(10)
        self.t.attach(self.entry7, 1, 2, 7, 8)
        self.label8 = gtk.Label()
        self.label8.set_alignment(0.95, 0.5)
        self.label8.set_width_chars(10)
        self.t.attach(self.label8, 0, 1, 8, 9)
        self.entry8 = gtk.Entry()
        self.entry8.set_width_chars(10)
        self.t.attach(self.entry8, 1, 2, 8, 9)
        preview = gtk.Button('Preview')
        preview.connect('pressed', self.send_preview)
        self.t.attach(preview, 0, 1, 10, 11)
        self.cont = gtk.Button('Continue')
        self.cont.set_sensitive(False)
        self.cont.connect('pressed', self.continue_shape)
        self.t.attach(self.cont, 1, 2, 10, 11)
        self.add = gtk.Button('Add')
        self.add.set_sensitive(False)
        self.add.connect('pressed', self.add_shape_to_file)
        self.t.attach(self.add, 2, 3, 10, 11)
        end = gtk.Button('Return')
        end.connect('pressed', self.end_this_shape)
        self.t.attach(end, 4, 5, 10, 11)
        self.g2Arc = gtk.RadioButton(None, 'Clockwise')
        self.g3Arc = gtk.RadioButton(self.g2Arc, 'Counter Clock')
        self.pixLinePoint = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/line-point.png', 
                width=240, 
                height=240)
        self.pixLineAngle = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/line-angle.png', 
                width=240, 
                height=240)
        self.pixArc3p = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/arc-3p.png', 
                width=240, 
                height=240)
        self.pixArc2pr = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/arc-2pr.png', 
                width=240, 
                height=240)
        self.pixArcAngle = gtk.gdk.pixbuf_new_from_file_at_size(
                filename='./wizards/images/arc-angle.png', 
                width=240, 
                height=240)
        self.image = gtk.Image()
        self.t.attach(self.image, 2, 5, 0, 8)
        self.W.show_all()
        if os.path.exists(self.configFile):
            f_in = open(self.configFile, 'r')
            for line in f_in:
                if line.startswith('preamble'):
                    self.preamble = line.strip().split('=')[1]
                elif line.startswith('postamble'):
                    self.postamble = line.strip().split('=')[1]
        self.s.poll()
        self.xOrigin = str(self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0])
        self.yOrigin = str(self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1])
        self.set_line_point_to_point()
        response = self.W.run()
