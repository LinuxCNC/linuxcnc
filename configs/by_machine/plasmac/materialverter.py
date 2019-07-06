#!/usr/bin/env python

'''
materialverter.py

Copyright (C) 2019  Phillip A Carter

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

import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import os

class materialConverter:

    def __init__(self):
        self.W = gtk.Window()
        self.W.set_title('Plasmac Material File Converter')
        self.W.connect('delete_event', self.on_window_delete_event)
        self.create_widgets()
        self.W.show_all()
        self.inFile.connect('button_press_event', self.on_infile_button_press_event)
        self.outFile.connect('button_press_event', self.on_outfile_button_press_event)
        self.outMetric.connect('toggled', self.on_metric_toggled, 'metric')
        self.convert.connect('clicked', self.on_convert_clicked)
        self.cancel.connect('clicked', self.on_cancel_clicked)
        self.divisor = 1
        self.precision = 2
        self.inFileName = ''
        self.outFileName = ''

    def create_widgets(self):
        self.T = gtk.Table(3, 9)
        self.T.set_row_spacings(8)
        self.T.set_homogeneous(True)
        self.W.add(self.T)
        self.inLabel = gtk.Label('Input File')
        self.inLabel.set_alignment(0.95,0.5)
        self.T.attach(self.inLabel, 0, 1, 0, 1)
        self.outLabel = gtk.Label('Output File')
        self.inLabel.set_alignment(0.95,0.5)
        self.T.attach(self.outLabel, 0, 1, 1, 2)
        self.inFile = gtk.Entry()
        self.inFile.set_width_chars(50)
        self.T.attach(self.inFile, 1, 8, 0, 1)
        self.outFile = gtk.Entry()
        self.outFile.set_width_chars(50)
        self.T.attach(self.outFile, 1, 8, 1, 2)
        self.outMetric = gtk.RadioButton(group=None, label='Metric')
        self.T.attach(self.outMetric, 8, 9, 0, 1)
        self.outImperial = gtk.RadioButton(group=self.outMetric, label='Imperial')
        self.T.attach(self.outImperial, 8, 9, 1, 2)
        self.convert = gtk.Button('Convert')
        self.T.attach(self.convert, 0, 2, 2, 3)
        self.outLabel = gtk.Label('')
        self.T.attach(self.outLabel, 3, 6, 2, 3)
        self.cancel = gtk.Button('Exit')
        self.T.attach(self.cancel, 7, 9, 2, 3)

    def on_window_delete_event(self,window,event):
        gtk.main_quit()

    def on_infile_button_press_event(self,button,event):
        self.outLabel.set_text('')
        dlg = gtk.FileChooserDialog("Open..", None, gtk.FILE_CHOOSER_ACTION_OPEN,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        response = dlg.run()
        if response == gtk.RESPONSE_OK:
            self.inFile.set_text(dlg.get_filename())
            self.inFileName = dlg.get_filename()
        else:
            self.inFile.set_text('')
            self.inFileName = ''
        dlg.destroy()

    def on_outfile_button_press_event(self,button,event):
        self.outLabel.set_text('')
        dlg = gtk.FileChooserDialog("Save..", None, gtk.FILE_CHOOSER_ACTION_SAVE,
          (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_SAVE, gtk.RESPONSE_OK))
        response = dlg.run()
        if response == gtk.RESPONSE_OK:
            self.outFile.set_text(dlg.get_filename())
            self.outFileName = dlg.get_filename()
        else:
            self.outFil.set_text('')
            self.outFileName = ''
        dlg.destroy()

    def on_metric_toggled(self,button,name):
        self.outLabel.set_text('')
        if button.get_active():
            self.divisor = 1
            self.precision = 2
        else:
            self.divisor = 25.4
            self.precision = 3
        print 'divisor = {:.1f}'.format(self.divisor)

    def on_convert_clicked(self,button):
        self.outLabel.set_text('')
        if not self.inFileName:
            print 'missing input filename'
            return
        if not self.outFileName:
            print 'missing output filename'
            return
        if not os.path.exists(self.inFileName):
            print '{} does not exist'.format(self.inFileName)
            return
        print 'convert from {} to {}...'.format(self.inFileName,self.outFileName)
        version = '[VERSION 1]'
        try:
            with open(self.outFileName, 'w') as f_out:
                f_out.write(\
                    '#plasmac material file\n'\
                    '#the next line is required for version checking\n'\
                     + version + '\n\n'\
                    '#example only, may be deleted\n'\
                    '#[MATERIAL_NUMBER_1]    = \n'\
                    '#NAME               = \n'\
                    '#KERF_WIDTH         = \n'\
                    '#THC                = (0 = off, 1 = on)\n'\
                    '#PIERCE_HEIGHT      = \n'\
                    '#PIERCE_DELAY       = \n'\
                    '#PUDDLE_JUMP_HEIGHT = (optional, set to 0 or delete if not required)\n'\
                    '#PUDDLE_JUMP_DELAY  = (optional, set to 0 or delete if not required)\n'\
                    '#CUT_HEIGHT         = \n'\
                    '#CUT_SPEED          = \n'\
                    '#CUT_AMPS           = (optional, only used for operator information)\n'\
                    '#CUT_VOLTS          = (modes 0 & 1 only, if not using auto voltage sampling)\n'\
                    '\n')
        except:
            self.outLabel.set_text('WRITE ERROR!!!')
        try:
            with open(self.inFileName, 'r') as f_in:
                count = 0
                valid = False
                for line in f_in:
                    if line.startswith('[Tool') and not 'Custom' in line:
                        if count and valid:
                            self.output()
                        valid = False
                        self.materialNum = ''
                        self.materialName = 'NAME               = '
                        self.materialKerf = 'KERF_WIDTH         = '
                        self.materialTHC = 'THC                = '
                        self.materialPierceH = 'PIERCE_HEIGHT      = '
                        self.materialPierceD = 'PIERCE_DELAY       = '
                        self.materialPuddleH = 'PUDDLE_JUMP_HEIGHT = 0'
                        self.materialPuddleD = 'PUDDLE_JUMP_DELAY  = 0'
                        self.materialCutH = 'CUT_HEIGHT         = '
                        self.materialCutS = 'CUT_SPEED          = '
                        self.materialCutA = 'CUT_AMPS           = '
                        self.materialCutV = 'CUT_VOLTS          = '
                    elif 'PlasmaTool' in line:
                        valid = True
                    elif line.startswith('Tool\ number'):
                        a,b = line.split('=')
                        self.materialNum = '[MATERIAL_NUMBER_{}]'.format(b.strip().replace(']',''))
                    elif line.startswith('Name'):
                        a,b = line.split('=',1)
                        self.materialName = 'NAME               = {}'.format(b.strip())
                    elif line.startswith('Kerf\ width'):
                        a,b = line.split('=',1)
                        self.materialKerf = "{id:}{val:.{i}f}".format(id='KERF_WIDTH         = ',
                                             i=self.precision, val=float(b.strip()) / self.divisor)
                    elif line.startswith('NO\ DTHC'):
                        a,b = line.split('=',1)
                        self.materialTHC = 'THC                = {}'.format(b.strip())
                    elif line.startswith('Pierce\ height'):
                        a,b = line.split('=',1)
                        self.materialPierceH = "{id:}{val:.{i}f}".format(id='PIERCE_HEIGHT      = ',
                                                i=self.precision, val=float(b.strip()) / self.divisor)
                    elif line.startswith('Pierce\ delay'):
                        a,b = line.split('=',1)
                        self.materialPierceD = 'PIERCE_DELAY       = {}'.format(b.strip())
                    elif line.startswith('Cut\ height'):
                        a,b = line.split('=',1)
                        self.materialCutH = "{id:}{val:.{i}f}".format(id='CUT_HEIGHT         = ',
                                             i=self.precision, val=float(b.strip()) / self.divisor)
                    elif line.startswith('Feed\ rate'):
                        a,b = line.split('=',1)
                        self.materialCutS = "{id:}{val:.{i}f}".format(id='CUT_SPEED          = ',
                                             i=0, val=float(b.strip()) / self.divisor)
                    elif line.startswith('Tip\ Size\ -Amps'):
                        a,b = line.split('=',1)
                        self.materialCutA = 'CUT_AMPS           = {}'.format(b.strip())
                    elif line.startswith('Preset\ volts'):
                        a,b = line.split('=',1)
                        self.materialCutV = 'CUT_VOLTS          = {}'.format(b.strip())
                    count += 1
                if valid:
                    self.output()
        except:
            self.outLabel.set_text('READ ERROR!!!')
        self.outLabel.set_text('FINISHED')

    def on_cancel_clicked(self,button):
        gtk.main_quit()

    def output(self):
        try:
            with open(self.outFileName, 'a') as f_out:
                f_out.write(self.materialNum + '\n' + \
                            self.materialName + '\n' + \
                            self.materialKerf + '\n' + \
                            self.materialTHC + '\n' + \
                            self.materialPierceH + '\n' + \
                            self.materialPierceD + '\n' + \
                            self.materialPuddleH + '\n' + \
                            self.materialPuddleD + '\n' + \
                            self.materialCutH + '\n' + \
                            self.materialCutS + '\n' + \
                            self.materialCutA + '\n' + \
                            self.materialCutV + '\n\n')
        except:
            self.outLabel.set_text('WRITE ERROR!!!')

if __name__ == '__main__':
    try:
        a = materialConverter()
        gtk.main()
    except KeyboardInterrupt:
        pass
