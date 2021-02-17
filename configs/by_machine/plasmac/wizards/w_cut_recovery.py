#!/usr/bin/env python

'''
w_cut_recovery.py

Copyright (C) 2020 Phillip A Carter

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
import hal
import linuxcnc
import gobject
import time

class recovery:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.s = linuxcnc.stat()
        self.c = linuxcnc.command()
        try:
            # we need this to connect to hal
            if not hal.component_exists('dummy'):
                self.h = hal.component('dummy')
                self.h.ready()
        except:
            print('Error connecting to HAL instance')
            self.exit()
        # no point in continuing if not paused
        if not hal.get_value('halui.program.is-paused'):
            print('Cannot load cut recovery because program is not paused')
            self.exit()
        self.clear_offsets()
        self.xOrig = hal.get_value('axis.x.eoffset-counts')
        self.yOrig = hal.get_value('axis.y.eoffset-counts')
        self.zOrig = hal.get_value('axis.z.eoffset-counts')
        self.oScale = hal.get_value('plasmac.offset-scale')
        self.xMin = float(self.i.find('AXIS_X', 'MIN_LIMIT'))
        self.xMax = float(self.i.find('AXIS_X', 'MAX_LIMIT'))
        self.yMin = float(self.i.find('AXIS_Y', 'MIN_LIMIT'))
        self.yMax = float(self.i.find('AXIS_Y', 'MAX_LIMIT'))
        self.zMin = float(self.i.find('AXIS_Z', 'MIN_LIMIT'))
        self.zMax = float(self.i.find('AXIS_Z', 'MAX_LIMIT'))
        self.cancelWait = False
        self.resumeWait = False
        gobject.timeout_add(200, self.periodic)

    def periodic(self):

        # if we are not paused then we have resumed
        if self.resumeWait and not hal.get_value('plasmac.x-offset-counts') and not hal.get_value('plasmac.y-offset-counts'):
            self.resumeWait = False


        # hide if required
        # if hal.get_value('plasmac.state-out') < 23 and \
        #    (hal.get_value('plasmac.cut-recovery') or hal.get_value('plasmac.cut-recovering')):
        #    self.W.hide()
        # if we are stopped and the offsets are cleared then we should exit
        if hal.get_value('plasmac:program-is-idle') and not hal.get_value('plasmac.x-offset-counts') and not hal.get_value('plasmac.y-offset-counts'):
            hal.set_p('plasmac.cut-recovery', '0')
            self.exit()
            return False
        # if we cancelled and the offsets are cleared then cut recovery must be finished
        if self.cancelWait and not hal.get_value('plasmac.x-offset-counts') and not hal.get_value('plasmac.y-offset-counts'):
            self.cancelWait = False
            hal.set_p('plasmac.cut-recovery', '0')
            self.exit()
        # if we return to cutting then cut recovery must be finished
        if hal.get_value('plasmac.state-out') == 11:
            hal.set_p('plasmac.cut-recovery', '0')
            self.exit()
            return False
        # hide/show the reverse run controls
        if hal.get_value('plasmac.x-offset-counts') or hal.get_value('plasmac.y-offset-counts'):
            self.feed_disable()
            # if self.resumeWait:
            if hal.get_value('plasmac.state-out') < 23 :
                self.buttons_disable()
        else:
            if not self.cancelWait or not self.resumeWait:
                self.feed_enable()
                self.buttons_enable()
        self.info.set_text('Move\n{}'.format(hal.get_value('plasmac_run.kerf-width-f')))
        return True

    def dialog_error(self, error):
        md = gtk.MessageDialog(self.W, 
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR, 
                               gtk.BUTTONS_CLOSE,
                               error)
        md.run()
        md.destroy()

    def rev_pressed(self, widget):
        self.paused_motion(-1)

    def rev_released(self, widget):
        self.paused_motion(0)

    def fwd_pressed(self, widget):
        self.paused_motion(1)

    def fwd_released(self, widget):
        self.paused_motion(0)

    def paused_motion(self, direction):
        speed = float(self.rate.get_value()) * 0.01 * direction
        hal.set_p('plasmac.paused-motion-speed',str(speed))

    def XminusYplus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.xMin:
            msg = 'X axis motion would trip X minimum limit'
            self.dialog_error(msg)
            return
        move1 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.yMax:
            msg = 'Y axis motion would trip Y maximum limit'
            self.dialog_error(msg)
            return
        move2 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move1)))
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move2)))
        hal.set_p('plasmac.cut-recovery', '1')

    def Yplus_pressed(self, widget):
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.yMax:
            msg = 'Y axis motion would trip Y maximum limit'
            self.dialog_error(msg)
            return
        move = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move)))
        hal.set_p('plasmac.cut-recovery', '1')

    def XplusYplus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.xMax:
            msg = 'X axis motion would trip X maximum limit'
            self.dialog_error(msg)
            return
        move1 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.yMax:
            msg = 'Y axis motion would trip Y maximum limit'
            self.dialog_error(msg)
            return
        move2 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move1)))
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move2)))
        hal.set_p('plasmac.cut-recovery', '1')

    def Xminus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.xMin:
            msg = 'X axis motion would trip X minimum limit'
            self.dialog_error(msg)
            return
        move = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move)))
        hal.set_p('plasmac.cut-recovery', '1')

    def Xplus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.xMax:
            msg = 'X axis motion would trip X maximum limit'
            self.dialog_error(msg)
            return
        move = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move)))
        hal.set_p('plasmac.cut-recovery', '1')

    def XminusYminus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.xMin:
            msg = 'X axis motion would trip X minimum limit'
            self.dialog_error(msg)
            return
        move1 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.yMin:
            msg = 'Y axis motion would trip Y minimum limit'
            self.dialog_error(msg)
            return
        move2 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move1)))
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move2)))
        hal.set_p('plasmac.cut-recovery', '1')

    def Yminus_pressed(self, widget):
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.yMin:
            msg = 'Y axis motion would trip Y minimum limit'
            self.dialog_error(msg)
            return
        move = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move)))
        hal.set_p('plasmac.cut-recovery', '1')

    def XplusYminus_pressed(self, widget):
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale + \
           hal.get_value('plasmac_run.kerf-width-f') > self.xMax:
            msg = 'X axis motion would trip X maximum limit'
            self.dialog_error(msg)
            return
        move1 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * 1)
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale - \
           hal.get_value('plasmac_run.kerf-width-f') < self.yMin:
            msg = 'Y axis motion would trip Y minimum limit'
            self.dialog_error(msg)
            return
        move2 = int(hal.get_value('plasmac_run.kerf-width-f') / self.oScale * -1)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + move1)))
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + move2)))
        hal.set_p('plasmac.cut-recovery', '1')

    def zPlus_pressed(self, widget):
        msg = 'Z axis motion not enable yet'
        self.dialog_error(msg)

    def zMinus_pressed(self, widget):
        msg = 'Z axis motion not enable yet'
        self.dialog_error(msg)

    def resume_pressed(self, widget):
        self.s.poll()
        if not self.s.paused:
            return
        # self.W.hide()
        if self.s.task_mode != linuxcnc.MODE_AUTO:
            msg = 'LinuxCNC is not in auto mode'
            self.dialog_error(msg)
            self.exit()
        self.c.auto(linuxcnc.AUTO_RESUME)
        while not hal.get_value('halui.program.is-running'):
            pass
        self.resumeWait = True
        self.clear_offsets()

    def cancel_pressed(self, widget):
        self.W.hide()
        self.clear_offsets()
        self.cancelWait = True

    def on_window_delete_event(self, window, event):
        self.exit()

    def exit(self):
        if hal.get_value('plasmac.x-offset-counts') or hal.get_value('plasmac.y-offset-counts'):
            self.clear_offsets()
        self.W.destroy()

    def clear_offsets(self):
        hal.set_p('plasmac.x-offset', '0')
        hal.set_p('plasmac.y-offset', '0')

    def feed_enable(self):
        self.rev.set_sensitive(True)
        self.feed.set_sensitive(True)
        self.fwd.set_sensitive(True)

    def feed_disable(self):
        self.rev.set_sensitive(False)
        self.feed.set_sensitive(False)
        self.fwd.set_sensitive(False)

    def buttons_enable(self):
        self.XminusYplus.set_sensitive(True)
        self.Yplus.set_sensitive(True)
        self.XplusYplus.set_sensitive(True)
        self.Xminus.set_sensitive(True)
        self.Xplus.set_sensitive(True)
        self.XminusYminus.set_sensitive(True)
        self.Yminus.set_sensitive(True)
        self.XplusYminus.set_sensitive(True)
        self.resume.set_sensitive(True)
        self.cancel.set_sensitive(True)

    def buttons_disable(self):
        self.XminusYplus.set_sensitive(False)
        self.Yplus.set_sensitive(False)
        self.XplusYplus.set_sensitive(False)
        self.Xminus.set_sensitive(False)
        self.Xplus.set_sensitive(False)
        self.XminusYminus.set_sensitive(False)
        self.Yminus.set_sensitive(False)
        self.XplusYminus.set_sensitive(False)
        self.resume.set_sensitive(False)
        self.cancel.set_sensitive(False)

    def create_widgets(self):
        self.W = gtk.Window()
        self.W.set_title('Cut Recovery')
        self.W.set_position(gtk.WIN_POS_MOUSE)
        self.W.set_keep_above(True)
        self.W.connect('delete_event', self.on_window_delete_event)
        self.T = gtk.Table()
        self.T.set_homogeneous(True)
        self.W.add(self.T)
        self.rev = gtk.Button('Rev')
        self.rev.connect('pressed', self.rev_pressed)
        self.rev.connect('released', self.rev_released)
        self.T.attach(self.rev, 0, 1, 0, 2)
        self.rate = gtk.Adjustment(value=50, lower=1, upper=100, step_incr=1, page_incr=10, page_size=0) 
        self.rate.emit('value_changed')
        self.feed = gtk.HScale(adjustment = self.rate)
        self.feed.set_digits(0)
        self.T.attach(self.feed, 1, 4, 0, 2)
        self.fwd = gtk.Button('Fwd')
        self.fwd.connect('pressed', self.fwd_pressed)
        self.fwd.connect('released', self.fwd_released)
        self.T.attach(self.fwd, 4, 5, 0, 2)
        self.XminusYplus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_left_up.png')
        self.XminusYplus.add(image)
        self.XminusYplus.connect('pressed', self.XminusYplus_pressed)
        self.T.attach(self.XminusYplus, 1, 2, 3, 5)
        self.Yplus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_up.png')
        self.Yplus.add(image)
        self.Yplus.connect('pressed', self.Yplus_pressed)
        self.T.attach(self.Yplus, 2, 3, 3, 5)
        self.XplusYplus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_right_up.png')
        self.XplusYplus.add(image)
        self.XplusYplus.connect('pressed', self.XplusYplus_pressed)
        self.T.attach(self.XplusYplus, 3, 4, 3, 5)
        self.Xminus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_left.png')
        self.Xminus.add(image)
        self.Xminus.connect('pressed', self.Xminus_pressed)
        self.T.attach(self.Xminus, 1, 2, 5, 7)
        self.info = gtk.Label()
        self.info.set_justify(gtk.JUSTIFY_CENTER)
        self.T.attach(self.info, 2, 3, 5, 7)
        self.Xplus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_right.png')
        self.Xplus.add(image)
        self.Xplus.connect('pressed', self.Xplus_pressed)
        self.T.attach(self.Xplus, 3, 4, 5, 7)
        self.XminusYminus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_left_down.png')
        self.XminusYminus.add(image)
        self.XminusYminus.connect('pressed', self.XminusYminus_pressed)
        self.T.attach(self.XminusYminus, 1, 2, 7, 9)
        self.Yminus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_down.png')
        self.Yminus.add(image)
        self.Yminus.connect('pressed', self.Yminus_pressed)
        self.T.attach(self.Yminus, 2, 3, 7, 9)
        self.XplusYminus = gtk.Button()
        image = gtk.image_new_from_file('./wizards/images/arrow_right_down.png')
        self.XplusYminus.add(image)
        self.XplusYminus.connect('pressed', self.XplusYminus_pressed)
        self.T.attach(self.XplusYminus, 3, 4, 7, 9)
        self.resume = gtk.Button('Resume\nCut')
        for child in self.resume.children():
            child.set_justify(gtk.JUSTIFY_CENTER)
        self.resume.connect('pressed', self.resume_pressed)
        self.T.attach(self.resume, 0, 1, 10, 12)
        self.cancel = gtk.Button('Cancel')
        self.cancel.connect('pressed', self.cancel_pressed)
        self.T.attach(self.cancel, 4, 5, 10, 12)
        hal.set_p('plasmac_run.preview-tab', '1')
        self.W.show_all()
