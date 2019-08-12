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
        self.W.set_title('Plasmac Material File Creator')
        self.W.connect('delete_event', self.on_window_delete_event)
        self.create_widgets()
        self.W.show_all()
        self.inLabel.hide()
        self.inFile.hide()
        self.outUnits.hide()
        self.outMetric.hide()
        self.outImperial.hide()
        self.inFile.connect('button_press_event', self.on_infile_button_press_event)
        self.outFile.connect('button_press_event', self.on_outfile_button_press_event)
        self.outMetric.connect('toggled', self.on_units_toggled)
        self.inManual.connect('toggled', self.on_source_toggled)
        self.inSheetcam.connect('toggled', self.on_source_toggled)
        self.inFusion.connect('toggled', self.on_source_toggled)
        self.convert.connect('clicked', self.on_convert_clicked)
        self.cancel.connect('clicked', self.on_cancel_clicked)
        self.divisor = 1
        self.precision = 2
        self.inFileName = ''
        self.outFileName = ''
        self.fNUM = '1'
        self.fNAM = '0'
        self.fKW = '0'
        self.fTHC = False
        self.fPH = '0'
        self.fPD = '0'
        self.fPJH = '0'
        self.fPJD = '0'
        self.fCH = '0'
        self.fCS = '0'
        self.fCA = '0'
        self.fCV = '0'

    def create_widgets(self):
        self.T = gtk.Table(9, 7)
        self.T.set_homogeneous(True)
        self.W.add(self.T)
        self.inManual = gtk.RadioButton(None, 'Manual')
        self.T.attach(self.inManual, 0, 2, 0, 1)
        self.inSheetcam = gtk.RadioButton(self.inManual, 'SheetCam')
        self.T.attach(self.inSheetcam, 0, 2, 1, 2)
        self.inFusion = gtk.RadioButton(self.inManual, 'Fusion 360')
        self.T.attach(self.inFusion, 0, 2, 2, 3)
        self.outUnits = gtk.Label('Output Units')
        self.outUnits.set_alignment(0, 0.5)
        self.T.attach(self.outUnits, 5, 7, 0, 1)
        self.outMetric = gtk.RadioButton(None, 'Metric')
        self.T.attach(self.outMetric, 5, 7, 1, 2)
        self.outImperial = gtk.RadioButton(self.outMetric, 'Imperial')
        self.T.attach(self.outImperial, 5, 7, 2, 3)
        self.inLabel = gtk.Label('Input File')
        self.inLabel.set_alignment(0, 1)
        self.T.attach(self.inLabel, 0, 1, 3, 4)
        self.inFile = gtk.Entry()
        self.inFile.set_width_chars(50)
        self.T.attach(self.inFile, 0, 7, 4, 5)
        self.outLabel = gtk.Label('Output File')
        self.outLabel.set_alignment(0,1 )
        self.T.attach(self.outLabel, 0, 1, 5, 6)
        self.outFile = gtk.Entry()
        self.outFile.set_width_chars(50)
        self.T.attach(self.outFile, 0, 7, 6, 7)
        self.convert = gtk.Button('Create')
        self.T.attach(self.convert, 0, 2, 8, 9)
        self.outLabel = gtk.Label('')
        self.T.attach(self.outLabel, 2, 5, 8, 9)
        self.cancel = gtk.Button('Exit')
        self.T.attach(self.cancel, 5, 7, 8, 9)

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
            self.outFile.set_text('')
            self.outFileName = ''
        dlg.destroy()

    def on_units_toggled(self,button):
        self.outLabel.set_text('')
        if button.get_active():
            self.divisor = 1
            self.precision = 2
        else:
            self.divisor = 25.4
            self.precision = 3

    def on_source_toggled(self,button):
        self.outLabel.set_text('')
        if button.get_active():
            if button.get_label() == 'SheetCam':
                self.outUnits.show()
                self.outMetric.show()
                self.outImperial.show()
            else:
                self.outUnits.hide()
                self.outMetric.hide()
                self.outImperial.hide()
            if button.get_label() == 'Manual':
                self.inLabel.hide()
                self.inFile.hide()
                self.convert.set_label('Create')
            else:
                self.inLabel.show()
                self.inFile.show()
                self.convert.set_label('Convert')

    def on_convert_clicked(self,button):
        self.outLabel.set_text('')
        if self.convert.get_label() != 'Add':
            if not self.inManual.get_active():
                if not self.inFileName:
                    self.outLabel.set_text('missing input filename')
                    return
                if not os.path.exists(self.inFileName):
                    self.outLabel.set_text('{} missing'.format(self.inFileName))
                    return
            if not self.outFileName:
                self.outLabel.set_text('missing output filename')
                return
            self.outLabel.set_text('converting...')
            version = '[VERSION 1]'
#            try:
            with open(self.outFileName, 'w') as f_out:
                f_out.write(\
                    '#plasmac material file\n'\
                    '#the next line is required for version checking\n'\
                     + version + '\n\n'\
                    '#example only, may be deleted\n'\
                    '#[MATERIAL_NUMBER_1]\n'\
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
#            except:
#                self.outLabel.set_text('WRITE ERROR!!!')
        else:
            self.fNUM = str(int(self.fNUM) + 1)
        if self.inManual.get_active():
            getParams = self.fusion_dialog()
            if getParams == gtk.RESPONSE_REJECT:
                return
            self.materialNum = '[MATERIAL_NUMBER_{}]'.format(self.fNUM)
            self.materialName = 'NAME               = {}'.format(self.fNAM)
            self.materialKerf = "{id:}{val:.{i}f}".format(id='KERF_WIDTH         = ',
                                 i=self.precision, val=float(self.fKW) / self.divisor)
            self.materialTHC = 'THC                = {}'.format(self.fTHC)
            self.materialPierceH = "{id:}{val:.{i}f}".format(id='PIERCE_HEIGHT      = ',
                                    i=self.precision, val=float(self.fPH) / self.divisor)
            self.materialPierceD = 'PIERCE_DELAY       = {}'.format(self.fPD)
            self.materialPuddleH = "{id:}{val:.{i}f}".format(id='PUDDLE_JUMP_HEIGHT = ',
                                    i=self.precision, val=float(self.fPJH) / self.divisor)
            self.materialPuddleD = 'PUDDLE_JUMP_DELAY  = {}'.format(self.fPJD)
            self.materialCutH = "{id:}{val:.{i}f}".format(id='CUT_HEIGHT         = ',
                                 i=self.precision, val=float(self.fCH) / self.divisor)
            self.materialCutS = "{id:}{val:.{i}f}".format(id='CUT_SPEED          = ',
                                 i=0, val=float(self.fCS) / self.divisor)
            self.materialCutA = 'CUT_AMPS           = {}'.format(self.fCA)
            self.materialCutV = 'CUT_VOLTS          = {}'.format(self.fCV)
            self.output()
            self.convert.set_label('Add')
        elif self.inSheetcam.get_active():
#            try:
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
                        self.materialTHC = 'THC                = 0'
                        self.materialPierceH = 'PIERCE_HEIGHT      = '
                        self.materialPierceD = 'PIERCE_DELAY       = '
                        self.materialPuddleH = 'PUDDLE_JUMP_HEIGHT = 0'
                        self.materialPuddleD = 'PUDDLE_JUMP_DELAY  = 0'
                        self.materialCutH = 'CUT_HEIGHT         = '
                        self.materialCutS = 'CUT_SPEED          = '
                        self.materialCutA = 'CUT_AMPS           = 0'
                        self.materialCutV = 'CUT_VOLTS          = 0'
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
#            except:
#                self.outLabel.set_text('READ ERROR!!!')
            self.outLabel.set_text('FINISHED')
        elif self.inFusion.get_active():
#            try:
            with open(self.inFileName, 'r') as f_in:
                for line in f_in:
                    line = line.lower().strip().split('\t')
                    if line[0].lower() == 'type':
                        lNum = line.index('number')
                        lNam = line.index('description')
                        lCS = line.index('cutting-feedrate')
                        lKW = line.index('kerf-width')
                    else:
                        self.fNUM = line[lNum]
                        self.fNAM = line[lNam]
                        getParams = self.fusion_dialog()
                        if getParams == gtk.RESPONSE_REJECT:
                            return
                        self.materialNum = '[MATERIAL_NUMBER_{}]'.format(self.fNUM)
                        self.materialName = 'NAME               = {}'.format(self.fNAM)
                        self.materialKerf = "{id:}{val:.{i}f}".format(id='KERF_WIDTH         = ',
                                             i=self.precision, val=float(line[lKW]) / self.divisor)
                        self.materialTHC = 'THC                = {}'.format(self.fTHC)
                        self.materialPierceH = "{id:}{val:.{i}f}".format(id='PIERCE_HEIGHT      = ',
                                                i=self.precision, val=float(self.fPH) / self.divisor)
                        self.materialPierceD = 'PIERCE_DELAY       = {}'.format(self.fPD)
                        self.materialPuddleH = "{id:}{val:.{i}f}".format(id='PUDDLE_JUMP_HEIGHT = ',
                                                i=self.precision, val=float(self.fPJH) / self.divisor)
                        self.materialPuddleD = 'PUDDLE_JUMP_DELAY  = {}'.format(self.fPJD)
                        self.materialCutH = "{id:}{val:.{i}f}".format(id='CUT_HEIGHT         = ',
                                             i=self.precision, val=float(self.fCH) / self.divisor)
                        self.materialCutS = "{id:}{val:.{i}f}".format(id='CUT_SPEED          = ',
                                             i=0, val=float(line[lCS]) / self.divisor)
                        self.materialCutA = 'CUT_AMPS           = {}'.format(self.fCA)
                        self.materialCutV = 'CUT_VOLTS          = {}'.format(self.fCV)
                        self.output()
#            except:
#                self.outLabel.set_text('READ ERROR!!!')
            self.outLabel.set_text('FINISHED')
        else:
            self.outLabel.set_text('Invalid Conversion')
    def on_cancel_clicked(self,button):
        gtk.main_quit()

    def output(self):
#        try:
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
#        except:
#            self.outLabel.set_text('WRITE ERROR!!!')

    def fusion_dialog(self):
        dialog = gtk.Dialog('CUT PARAMETERS',
                            self.W,
                            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                            (gtk.STOCK_OK, gtk.RESPONSE_ACCEPT,
                            gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT))
        infL = gtk.Label('For Material # {}\n{}'.format(self.fNUM,self.fNAM))
        dNUl = gtk.Label('Material Number')
        dNUl.set_alignment(0, 1)
        dNU = gtk.Entry()
        dNU.set_text(self.fNUM)
        dNU.set_alignment(0.95)
        dNAl = gtk.Label('Material Name')
        dNAl.set_alignment(0, 1)
        dNA = gtk.Entry()
        dNA.set_text(self.fNAM)
        dNA.set_alignment(0.95)
        dKWl = gtk.Label('Kerf Width')
        dKWl.set_alignment(0, 1)
        dKW = gtk.Entry()
        dKW.set_text(self.fKW)
        dKW.set_alignment(0.95)
        dTHC = gtk.CheckButton('THC Enabled')
        dTHC.set_active(self.fTHC)
        dPHl = gtk.Label('Pierce Height')
        dPHl.set_alignment(0, 1)
        dPH = gtk.Entry()
        dPH.set_text(self.fPH)
        dPH.set_alignment(0.95)
        dPDl = gtk.Label('Pierce Delay')
        dPDl.set_alignment(0, 1)
        dPD = gtk.Entry()
        dPD.set_text(self.fPD)
        dPD.set_alignment(0.95)
        dPJHl = gtk.Label('Puddle Jump Height')
        dPJHl.set_alignment(0, 1)
        dPJH = gtk.Entry()
        dPJH.set_text(self.fPJH)
        dPJH.set_alignment(0.95)
        dPJDl = gtk.Label('Puddle Jump Delay')
        dPJDl.set_alignment(0, 1)
        dPJD = gtk.Entry()
        dPJD.set_text(self.fPJD)
        dPJD.set_alignment(0.95)
        dCHl = gtk.Label('Cut Height')
        dCHl.set_alignment(0, 1)
        dCH = gtk.Entry()
        dCH.set_text(self.fCH)
        dCH.set_alignment(0.95)
        dCSl = gtk.Label('Cut Speed')
        dCSl.set_alignment(0, 1)
        dCS = gtk.Entry()
        dCS.set_text(self.fCS)
        dCS.set_alignment(0.95)
        dCAl = gtk.Label('Cut Amps')
        dCAl.set_alignment(0, 1)
        dCA = gtk.Entry()
        dCA.set_text(self.fCA)
        dCA.set_alignment(0.95)
        dCVl = gtk.Label('Cut Volts')
        dCVl.set_alignment(0, 1)
        dCV = gtk.Entry()
        dCV.set_text(self.fCV)
        dCV.set_alignment(0.95)
        if self.inManual.get_active():
            dialog.vbox.add(dNUl)
            dialog.vbox.add(dNU)
            dialog.vbox.add(dNAl)
            dialog.vbox.add(dNA)
            dialog.vbox.add(dKWl)
            dialog.vbox.add(dKW)
        else:
            dialog.vbox.add(infL)
        dialog.vbox.add(dTHC)
        dialog.vbox.add(dPHl)
        dialog.vbox.add(dPH)
        dialog.vbox.add(dPDl)
        dialog.vbox.add(dPD)
        dialog.vbox.add(dPJHl)
        dialog.vbox.add(dPJH)
        dialog.vbox.add(dPJDl)
        dialog.vbox.add(dPJD)
        dialog.vbox.add(dCHl)
        dialog.vbox.add(dCH)
        if self.inManual.get_active():
            dialog.vbox.add(dCSl)
            dialog.vbox.add(dCS)
        dialog.vbox.add(dCAl)
        dialog.vbox.add(dCA)
        dialog.vbox.add(dCVl)
        dialog.vbox.add(dCV)
        dialog.show_all()
        response = dialog.run()
        if self.inManual.get_active():
            self.fNUM = dNU.get_text()
            self.fNAM = dNA.get_text()
            self.fKW = dKW.get_text()
        if dTHC.get_active():
            self.fTHC = 1
        else:
            self.fTHC = 0
        self.fPH = dPH.get_text()
        self.fPD = dPD.get_text()
        self.fPJH = dPJH.get_text()
        self.fPJD = dPJD.get_text()
        self.fCH = dCH.get_text()
        if self.inManual.get_active():
            self.fCS = dCS.get_text()
        self.fCA = dCA.get_text()
        self.fCV = dCV.get_text()
        dialog.destroy()
        return response

if __name__ == '__main__':
    try:
        a = materialConverter()
        gtk.main()
    except KeyboardInterrupt:
        pass
