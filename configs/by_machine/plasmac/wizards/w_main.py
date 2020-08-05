#!/usr/bin/env python

'''
w_main.py

Copyright (C) 2020  Phillip A Carter

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
import gobject
from subprocess import Popen,PIPE
import gremlin

import w_array
import w_line
import w_circle
import w_triangle
import w_rectangle
import w_polygon
import w_bolt_circle
import w_slot
import w_star
import w_gusset
import w_rotate
import w_sector
import w_settings

class main_wiz:

    def __init__(self):
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.c = linuxcnc.command()
        self.s = linuxcnc.stat()
        self.gui = self.i.find('DISPLAY', 'DISPLAY').lower()
        self.savePath = self.i.find('DISPLAYaqw', 'PROGRAM_PREFIX') or \
                        '{}/linuxcnc/nc_files'.format(os.path.expanduser("~"))
        self.configFile = '{}_wizards.cfg'.format(self.i.find('EMC', 'MACHINE').lower())
        # glcanon has a reversed scale to just about everything else...
        self.scale = 1.0 if self.i.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else 0.03937000787402
        self.preview = preview(self.i)
        self.preview.program_alpha = True
        self.preview.set_cone_basesize(0.1)
        self.preview.mouse_btn_mode = 6
        if self.scale == 1.0:
            self.preview.metric_units = False
        self.rowSpace = 2
        self.tmpDir = ('/tmp/plasmac_wizards')
        if not os.path.isdir(self.tmpDir):
            os.mkdir(self.tmpDir)
        self.fTmp = '{}/temp.ngc'.format(self.tmpDir)
        self.fNgc = '{}/shape.ngc'.format(self.tmpDir)
        self.fNgcBkp = '{}/backup.ngc'.format(self.tmpDir)
        gobject.timeout_add(100, self.periodic)

    def periodic(self):
        # exit if linuxcnc not running
        if not hal.component_exists('plasmac_run'):
            self.W.destroy()
        return True

    def dialog_error(self, wizard, error):
        md = gtk.MessageDialog(self.W, 
                               gtk.DIALOG_DESTROY_WITH_PARENT,
                               gtk.MESSAGE_ERROR, 
                               gtk.BUTTONS_CLOSE,
                               '{} WIZARD ERROR\n\n{}'.format(wizard, error))
        md.set_keep_above(True)
        md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        md.run()
        md.destroy()

    def entry_changed(self, widget):
        if widget.get_text():
            if widget.get_text()[len(widget.get_text()) - 1] not in '-.0123456789':
                widget.set_text(widget.get_text()[:len(widget.get_text()) - 1])

    def undo_shape(self, event, button):
        if os.path.exists(self.fNgcBkp):
            shutil.copyfile(self.fNgcBkp, self.fNgc)
            self.preview.load(self.fNgc)
            if button:
                button.set_sensitive(False)

    def add_shape_to_file(self, event, button):
        shutil.copyfile(self.fNgc, self.fNgcBkp)
        if button:
            button.set_sensitive(False)

    def on_new_clicked(self, widget):
        outNgc = open(self.fNgc, 'w')
        outNgc.write('(new wizard)\nM2\n')
        outNgc.close()
        shutil.copyfile(self.fNgc, self.fTmp)
        shutil.copyfile(self.fNgc, self.fNgcBkp)
        # if self.gui == 'axis':
        #     Popen('axis-remote {}'.format(self.fNgc), stdout = PIPE, shell = True)
        # elif self.gui == 'gmoccapy':
        #     self.c.program_open(self.fNgc)
        # time.sleep(0.1)
        self.preview.load(self.fNgc)

    def on_save_clicked(self, widget):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new wizard)' in line:
                    self.dialog_error('SAVE', 'The empty file: {}\n\ncannot be saved'.format(os.path.basename(self.fNgc)))
                    return
        dlg = gtk.FileChooserDialog('Save..', None, gtk.FILE_CHOOSER_ACTION_SAVE,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_SAVE, gtk.RESPONSE_OK))
        filter = gtk.FileFilter()
        filter.set_name('NGC files')
        filter.add_pattern("*.ngc")
        dlg.add_filter(filter)
        dlg.set_do_overwrite_confirmation(True)
        dlg.set_current_folder(self.savePath)
        dlg.set_current_name("shape.ngc")
        # toggle = gtk.CheckButton('Open file read-only')
        # dlg.set_extra_widget(toggle)
        response = dlg.run()
        fileName = dlg.get_filename()
        dlg.destroy()
        if response == gtk.RESPONSE_OK:
            shutil.copyfile(self.fNgc, fileName)

    def on_settings_clicked(self, widget):
        reload(w_settings)
        settings = w_settings.settings_wiz()
        settings.settings_show(self)

    def on_line_clicked(self, widget):
        reload(w_line)
        line = w_line.line_wiz()
        line.line_show(self)

    def on_circle_clicked(self, widget):
        reload(w_circle)
        circle = w_circle.circle_wiz()
        circle.circle_show(self)

    def on_triangle_clicked(self, widget):
        reload(w_triangle)
        triangle = w_triangle.triangle_wiz()
        triangle.triangle_show(self)

    def on_rectangle_clicked(self, widget):
        reload(w_rectangle)
        rectangle = w_rectangle.rectangle_wiz()
        rectangle.rectangle_show(self)

    def on_polygon_clicked(self, widget):
        reload(w_polygon)
        polygon = w_polygon.polygon_wiz()
        polygon.polygon_show(self)

    def on_bolt_circle_clicked(self, widget):
        reload(w_bolt_circle)
        bolt_circle = w_bolt_circle.bolt_circle_wiz()
        bolt_circle.bolt_circle_show(self)

    def on_slot_clicked(self, widget):
        reload(w_slot)
        slot = w_slot.slot_wiz()
        slot.slot_show(self)

    def on_star_clicked(self, widget):
        reload(w_star)
        star = w_star.star_wiz()
        star.star_show(self)

    def on_gusset_clicked(self, widget):
        reload(w_gusset)
        gusset = w_gusset.gusset_wiz()
        gusset.gusset_show(self)

    def on_sector_clicked(self, widget):
        reload(w_sector)
        sector = w_sector.sector_wiz()
        sector.sector_show(self)

    def on_rotate_clicked(self, widget):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new wizard)' in line:
                    self.dialog_error('ROTATE', 'The empty file: {}\n\ncannot be rotated'.format(os.path.basename(self.fNgc)))
                    return
        reload(w_rotate)
        rotate = w_rotate.rotate_wiz()
        rotate.rotate_show(self)

    def on_array_clicked(self, widget):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new wizard)' in line:
                    self.dialog_error('ARRAY', 'The empty file: {}\n\ncannot be arrayed'.format(os.path.basename(self.fNgc)))
                    return
                elif '(wizard' in line:
                    self.arrayMode = 'wizard'
                    break
                elif '#<ucs_' in line:
                    self.dialog_error('ARRAY', 'This existing array: {}\n\ncannot be arrayed'.format(os.path.basename(self.fNgc)))
                    return
                else:
                    self.arrayMode = 'external'
        reload(w_array)
        array = w_array.array_wiz()
        array.array_show(self)

    def on_send_clicked(self, widget):
        shutil.copyfile(self.fNgcBkp, self.fNgc)
        if self.gui == 'axis':
            Popen('axis-remote {}'.format(self.fNgc), stdout = PIPE, shell = True)
        elif self.gui == 'gmoccapy':
            self.c.program_open('./wizards/blank.ngc')
            time.sleep(0.1)
            self.c.program_open(self.fNgc)
        else:
            print('Unknown GUI in .ini file')
        self.W.destroy()

    def on_quit_clicked(self, widget):
        self.W.destroy()

    def on_delete_event(self, window, event):
        self.W.destroy()

    def remove_temp_files(self):
        for filename in os.listdir(self.tmpDir):
            file_path = os.path.join(self.tmpDir, filename)
            try:
                os.remove(file_path)
            except:
                print('Error deleting temp files')

    def resize_window(self, w, h):
        self.W.resize(w, h)
        if h > 800:
            self.rowSpace = 8
        elif h > 700:
            self.rowSpace = 4
        else:
            self.rowSpace = 2
        print('resized window to: {}'.format(self.W.get_size()))

    def main_show(self):
        self.W = gtk.Dialog('PlasmaC Conversational',
                       None,
                       gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                       buttons = None)
        self.W.set_keep_above(True)
        self.W.set_position(gtk.WIN_POS_CENTER)
        self.W.set_default_size(890, 663)
        self.W.connect('delete_event', self.on_delete_event)
        top = gtk.HBox(True, 2)
        bottom = gtk.HBox()
        self.W.vbox.pack_start(top, expand = False, fill = True)
        self.W.vbox.pack_start(bottom, expand = True, fill = True)
        self.left = gtk.VBox()
        right = gtk.Frame()
        bottom.pack_start(self.left, expand = False, fill = True)
        bottom.pack_start(right, expand = True, fill = True)
        self.line = gtk.Button()
        self.line.connect('clicked', self.on_line_clicked)
        self.circle = gtk.Button()
        self.circle.connect('clicked', self.on_circle_clicked)
        self.triangle = gtk.Button()
        self.triangle.connect('clicked', self.on_triangle_clicked)
        self.rectangle = gtk.Button()
        self.rectangle.connect('clicked', self.on_rectangle_clicked)
        self.polygon = gtk.Button()
        self.polygon.connect('clicked', self.on_polygon_clicked)
        self.bolt_circle = gtk.Button()
        self.bolt_circle.connect('clicked', self.on_bolt_circle_clicked)
        self.slot = gtk.Button()
        self.slot.connect('clicked', self.on_slot_clicked)
        self.star = gtk.Button()
        self.star.connect('clicked', self.on_star_clicked)
        self.gusset = gtk.Button()
        self.gusset.connect('clicked', self.on_gusset_clicked)
        self.sector = gtk.Button()
        self.sector.connect('clicked', self.on_sector_clicked)
        self.rotate = gtk.Button()
        self.rotate.connect('clicked', self.on_rotate_clicked)
        self.array = gtk.Button()
        self.array.connect('clicked', self.on_array_clicked)
        buttons = [self.line, self.circle, self.triangle, self.rectangle, \
                   self.polygon, self.bolt_circle, self.slot, self.star, \
                   self.gusset, self.sector, self.rotate, self.array]
        bunames = ['line', 'circle', 'triangle', 'rectangle', 'polygon', \
                   'bolt_circle', 'slot', 'star', 'gusset', 'sector', \
                   'rotate', 'array']
        for wizard in bunames:
            if bunames.index(wizard) <= 11:
                pixbuf = gtk.gdk.pixbuf_new_from_file_at_size(
                        filename='./wizards/images/{}-thumb.png'.format(wizard), 
                        width=60, 
                        height=60)
                image = gtk.Image()
                image.set_from_pixbuf(pixbuf)
                buttons[bunames.index(wizard)].set_image(image)
            top.add(buttons[bunames.index(wizard)])
        right.add(self.preview)
        self.entries = gtk.Table(15, 5, True)
        self.entries.set_row_spacings(6)
        self.left.pack_start(self.entries, expand = False, fill = True)
        spaceLabel = gtk.Label()
        self.left.pack_start(spaceLabel, expand = True, fill = True)
        self.button_box = gtk.Table(1, 5, True)
        self.left.pack_start(self.button_box, expand = False, fill = True)
        bLabel1 = gtk.Label()
        bLabel1.set_width_chars(9)
        self.button_box.attach(bLabel1, 0, 1, 0, 1)
        self.new = gtk.Button('New')
        self.new.connect('clicked', self.on_new_clicked)
        self.button_box.attach(self.new, 0, 1, 0, 1)
        self.save = gtk.Button('Save')
        self.save.connect('clicked', self.on_save_clicked)
        self.button_box.attach(self.save, 1, 2, 0, 1)
        self.settings = gtk.Button('Settings')
        self.settings.connect('clicked', self.on_settings_clicked)
        self.button_box.attach(self.settings, 2, 3, 0, 1)
        self.quit = gtk.Button('Quit')
        self.quit.connect('clicked', self.on_quit_clicked)
        self.button_box.attach(self.quit, 3, 4, 0, 1)
        self.send = gtk.Button('Send')
        self.send.connect('clicked', self.on_send_clicked)
        self.button_box.attach(self.send, 4, 5, 0, 1)
        wWidth = wHeight = gSize = 0
        if os.path.exists(self.configFile):
            f_in = open(self.configFile, 'r')
            for line in f_in:
                if line.startswith('postamble'):
                    self.postamble = line.strip().split('=')[1]
                elif line.startswith('preamble'):
                    self.preamble = line.strip().split('=')[1]
                elif line.startswith('window-width'):
                    try:
                        wWidth = int(line.strip().split('=')[1])
                    except:
                        wWidth = 0
                elif line.startswith('window-height'):
                    try:
                        wHeight = int(line.strip().split('=')[1])
                    except:
                        wHeight = 0
                elif line.startswith('grid-size'):
                    try:
                        gSize = float(line.strip().split('=')[1]) * self.scale
                    except:
                        gSize = 0
        if wWidth and wHeight:
            self.resize_window(wWidth, wHeight)
        if gSize:
            self.preview.grid_size = gSize
        self.W.show_all()
        self.s.poll()
        if self.s.file:
            try:
                shutil.copyfile(self.s.file, self.fNgc)
                shutil.copyfile(self.s.file, self.fNgcBkp)
                self.preview.load(self.fNgc)
            except:
                self.on_new_clicked(None)
        else:
            self.on_new_clicked(None)
        #hal.set_p('plasmac_run.preview-tab', '1')
        self.s.poll()
        self.xOrigin = str(self.s.actual_position[0] - self.s.g5x_offset[0] - self.s.g92_offset[0])
        self.yOrigin = str(self.s.actual_position[1] - self.s.g5x_offset[1] - self.s.g92_offset[1])
        self.on_line_clicked(None)
        response = self.W.run()

    @staticmethod
    def gcode_error(errMsg):
        print('G-CODE ERROR: {}'.format(errMsg))

class preview(gremlin.Gremlin):
    def __init__(self, inifile):
        gremlin.Gremlin.__init__(self, inifile)

    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
        return limit, homed, [''], ['']

    def report_gcode_error(self, result, seq, filename):
        import gcode
        error_str = gcode.strerror(result)
        error = 'G-Code error in {} near line {}: {}'.format(os.path.basename(filename), str(seq), gcode.strerror(result))
        main_wiz.gcode_error(error)
