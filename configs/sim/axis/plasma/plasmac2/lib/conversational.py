'''
conversational.py

Copyright (C) 2019, 2020, 2021, 2022  Phillip A Carter
Copyright (C)       2020, 2021, 2022  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os, sys
import gettext
import tkinter as tk
from tkinter import ttk
from tkinter.filedialog import asksaveasfilename
from importlib import reload
from shutil import copy as COPY
import conv_settings as CONVSET
import conv_line as CONVLINE
import conv_circle as CONVCIRC
import conv_ellipse as CONVELLI
import conv_triangle as CONVTRIA
import conv_rectangle as CONVRECT
import conv_polygon as CONVPOLY
import conv_bolt as CONVBOLT
import conv_slot as CONVSLOT
import conv_star as CONVSTAR
import conv_gusset as CONVGUST
import conv_sector as CONVSECT
import conv_block as CONVBLCK

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f"{f.split('/lib')[0]}",'share','locale')
        break
gettext.install("linuxcnc", localedir=localeDir)

class Conv(tk.Tk):
    def __init__(self, convFirstRun, root, toolFrame, convFrame, combobox, \
                 imagePath, tmpPath, pVars, unitsPerMm, comp, prefs, getprefs, \
                 putprefs, fileOpener, wcs_rotation, conv_toggle, color_change, \
                 plasmacPopUp, o):
        self.r = root
        self.rE = root.tk.eval
        self.toolFrame = toolFrame
        self.convFrame = convFrame
        self.unitsPerMm = unitsPerMm
        self.prefs = prefs
        self.getPrefs = getprefs
        self.putPrefs = putprefs
        self.comp = comp
        self.fileOpener = fileOpener
        self.wcs_rotation = wcs_rotation
        self.pVars = pVars
        self.conv_toggle = conv_toggle
        self.plasmacPopUp = plasmacPopUp
        self.o = o
        self.shapeLen = {'x':None, 'y':None}
        self.combobox = combobox
        self.fTmp = os.path.join(tmpPath, 'temp.ngc')
        self.fNgc = os.path.join(tmpPath, 'shape.ngc')
        self.fNgcBkp = os.path.join(tmpPath, 'backup.ngc')
        self.fNgcSent = os.path.join(tmpPath, 'sent_shape.ngc')
        self.filteredBkp = os.path.join(tmpPath, 'filtered_bkp.ngc')
        self.savedSettings = {'start_pos':None, 'cut_type':None, 'lead_in':None, 'lead_out':None}
        self.buttonState = {'newC':False, 'saveC':False, 'sendC':False, 'settingsC':False}
        self.oldConvButton = False
        self.module = None
        self.oldModule = None
        if self.unitsPerMm == 1:
            smallHole = 32
            leadin = 5
            preAmble = 'G21 G64P0.25'
        else:
            smallHole = 1.25
            leadin = 0.2
            preAmble = 'G20 G64P0.01'
        preAmble = f"{preAmble} G40 G49 G80 G90 G92.1 G94 G97"
        self.xOrigin = 0.000
        self.yOrigin = 0.000
        if self.comp['development']:
            reload(CONVSET)
        CONVSET.load(self, preAmble, leadin, smallHole, preAmble)
        if convFirstRun or self.comp['development']:
            self.create_widgets(imagePath)
            self.show_common_widgets()
            color_change()
        self.polyCombo['values'] = (_('CIRCUMSCRIBED'), _('INSCRIBED'), _('SIDE LENGTH'))
        self.lineCombo['values'] = (_('LINE POINT ~ POINT'), _('LINE BY ANGLE'), _('ARC 3P'),\
                               _('ARC 2P +RADIUS'), _('ARC ANGLE +RADIUS'))
        self.addC['command'] = lambda:self.add_shape_to_file()
        self.undoC['command'] = lambda:self.undo_shape()
        self.newC['command'] = lambda:self.new_pressed(True)
        self.saveC['command'] = lambda:self.save_pressed()
        self.settingsC['command'] = lambda:self.settings_pressed()
        self.sendC['command'] = lambda:self.send_pressed()
        for entry in self.entries:
            #entry.bind('<KeyRelease>', self.auto_preview) # plays havoc with good error detection
            #entry.bind('<Return>', self.auto_preview, add='+')
            #entry.bind('<KP_Enter>', self.auto_preview, add='+')
            entry.bind('<Return>', self.preview, add='+')
            entry.bind('<KP_Enter>', self.preview, add='+')
        for entry in self.buttons: entry.bind('<space>', self.button_key_pressed)
        self.entry_validation()

    def start(self, materialFileDict, matIndex, existingFile, g5xIndex, setViewZ):
        sDiff = 40 + int(self.pVars.fontSize.get()) - 7
        for i in range(len(self.convShapes)):
            self.toolButton[i]['width'] = sDiff
            self.toolButton[i]['height'] = sDiff
        self.materials = materialFileDict
        self.matIndex = matIndex
        self.existingFile = existingFile
        self.g5xIndex = g5xIndex
        self.setViewZ = setViewZ
        self.previewActive = False
        self.settingsExited = False
        self.validShape = False
        self.invalidLeads = 0
        self.ocEntry['state'] = 'disabled'
        self.undoC['text'] = _('RELOAD')
        self.addC['state'] = 'disabled'
        self.newC['state'] = 'normal'
        self.saveC['state'] = 'disabled'
        self.sendC['state'] = 'disabled'
        self.settingsC['state'] = 'normal'
        self.polyCombo.setvalue('first')
        self.lineCombo.setvalue('first')
        self.convLine = {}
        self.convBlock = [False, False]
        for m in self.materials:
            values = [str(m) + ': ' + self.materials[m]['name'] for m in self.materials]
            self.matCombo['values'] = values
        self.matCombo.setvalue(f"@{self.matIndex}")
        longest = max(values, key=len)
        tmpLabel = tk.Label(text=longest)
        self.matCombo['listboxwidth'] = tmpLabel.winfo_reqwidth() + 20
        if self.oldConvButton:
            self.toolButton[self.convShapes.index(self.oldConvButton)].select()
            self.shape_request(self.oldConvButton)
        else:
            self.toolButton[0].select()
            self.shape_request('line')
        if self.existingFile:
            if 'shape.ngc' not in self.existingFile:
                COPY(self.filteredBkp, self.fNgc)
                COPY(self.filteredBkp, self.fNgcBkp)
        else:
            self.new_pressed(False)
            self.l3Entry.focus()

    def entry_validation(self):
        self.vcmd = (self.r.register(self.validate_entries))
        for w in self.uInts:
            w.configure(validate='key', validatecommand=(self.vcmd, 'int', '%P', '%W'))
        for w in self.sFloats:
            w.configure(validate='key', validatecommand=(self.vcmd, 'flt', '%P', '%W'))
        for w in self.uFloats:
            w.configure(validate='key', validatecommand=(self.vcmd, 'pos', '%P', '%W'))

    def validate_entries(self, style, value, widget):
        widget = self.r.nametowidget(widget)
        # determine the type of circle
        if self.convButton.get() == 'circle' and (widget == self.dEntry or widget == self.shEntry):
            circleType = 'circle'
        elif self.convButton.get() == 'boltcircle' and (widget == self.hdEntry or widget == self.shEntry):
            circleType = 'boltcircle'
        else:
            circleType = None
        # blank is valid
        if value == '':
            if circleType:
                self.circle_check(circleType, value)
            return True
        # period in a int is invalid and more than one period is invalid
        if ('.' in value and style == 'int') or value.count('.') > 1:
            return False
        # a negative in a posfloat or int is invalid and more than one negative is invalid
        if ('-' in value and (style == 'pos' or style == 'int')) or value.count('-') > 1:
            return False
        # cannot do math on "-", ".", or "-." but they are valid
        if value == '.' or value == '-' or value == '-.':
            if circleType:
                self.circle_check(circleType, value)
            return True
        # if a float cannot be calculated from value then it is invalid
        try:
            v = float(value)
        except:
            return False
        # must be a valid value
        if circleType:
            self.circle_check(circleType, value)
        return True

    def preview(self, event):
#        # this stops focus moving to the root when return pressed
#        self.rE(f"after 5 focus {event.widget}")
        self.module.preview(self)

    def auto_preview(self, event):
        # this stops focus moving to the root when return pressed
        self.rE(f"after 5 focus {event.widget}")
        # we have no interest in this list of keys
        if event.keysym in ['Tab']:
            return
        # not enough info for an auto_preview yet
        if event.widget.get() in ['.','-','-.']:
            print('return due to . - -.')
            return
        # if auto preview is valid
        if self.previewC['state'] == 'normal' and self.convButton.get() != 'settings':
            self.module.auto_preview(self)

    def button_key_pressed(self, event):
        # dummy function for:
        # press a button widget via the keyboard spacebar
        pass

    def circle_check(self, circleType, dia):
        self.invalidLeads = 0
        try:
            dia = float(dia)
        except:
            dia = 0
        # cannot do overcut if large hole, no hole, or external circle
        if dia >= self.smallHoleDia or dia == 0 or (self.ctbValue.get() == _('EXTERNAL') and circleType == 'circle'):
            self.ocbValue.set(_('OFF'))
            self.ocButton['state'] = 'disabled'
            self.ocEntry['state'] = 'disabled'
        else:
            self.ocButton['state'] = 'normal'
            self.ocEntry['state'] = 'normal'
        # cannot do leadout if small hole
        if self.ctbValue.get() == _('INTERNAL') or circleType == 'boltcircle':
            if dia < self.smallHoleDia:
                self.loLabel['state'] = 'disabled'
                self.loEntry['state'] = 'disabled'
                self.invalidLeads = 2
            else:
            # test for large leadin or leadout
                try:
                    lIn = float(self.liValue.get())
                except:
                    lIn = 0
                try:
                    lOut = float(self.loValue.get())
                except:
                    lOut = 0
                if lIn and lIn > dia / 4:
                    self.loLabel['state'] = 'disabled'
                    self.loEntry['state'] = 'disabled'
                    self.invalidLeads = 2
                elif lOut and lOut > dia / 4:
                    self.loLabel['state'] = 'disabled'
                    self.invalidLeads = 1
                else:
                    self.loLabel['state'] = 'normal'
                    self.loEntry['state'] = 'normal'
                    self.invalidLeads = 0
        else:
            self.loLabel['state'] = 'normal'
            self.loEntry['state'] = 'normal'
            self.invalidLeads = 0

    # dialog call from shapes
    def dialog_show_ok(self, title, text):
        reply = self.plasmacPopUp('error', title, text).reply
        return reply

    def new_pressed(self, buttonPressed):
        if buttonPressed and (self.saveC['state'] or self.sendC['state'] or self.previewActive):
            title = _('Unsaved Shape')
            msg0 = _('You have an unsaved, unsent, or active previewed shape')
            msg1 = _('If you continue it will be deleted')
            if not self.plasmacPopUp('yesno', title, f"{msg0}\n\n{msg1}\n").reply:
                return
        if self.oldConvButton == 'line':
            if self.lineCombo.get() == _('LINE POINT ~ POINT'):
                CONVLINE.set_line_point_to_point(self)
            elif self.lineCombo.get() == _('LINE BY ANGLE'):
                CONVLINE.set_line_by_angle(self)
            elif self.lineCombo.get() == _('ARC 3P'):
                CONVLINE.set_arc_3_points(self)
            elif self.lineCombo.get() == _('ARC 2P +RADIUS'):
                CONVLINE.set_arc_2_points_radius(self)
            elif self.lineCombo.get() == _('ARC ANGLE +RADIUS'):
                CONVLINE.set_arc_by_angle_radius(self)
        with open(self.fNgc, 'w') as outNgc:
            outNgc.write('(new conversational file)\nM2\n')
        COPY(self.fNgc, self.fTmp)
        COPY(self.fNgc, self.fNgcBkp)
        self.fileOpener(self.fNgc, True, False)
        self.saveC['state'] = 'disabled'
        self.sendC['state'] = 'disabled'
        self.validShape = False
        self.convLine['xLineStart'] = self.convLine['xLineEnd'] = 0
        self.convLine['yLineStart'] = self.convLine['yLineEnd'] = 0
        self.coValue.set('')
        self.roValue.set('')
        self.preview_button_pressed(False)

    def save_pressed(self):
        self.convFrame.unbind_all('<KeyRelease>')
        self.convFrame.unbind_all('<Return>')
        title = _('Save Error')
        with open(self.fNgc, 'r') as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    msg0 = _('An empty file cannot be saved')
                    self.plasmacPopUp('error', title, msg0)
                    return
#        self.vkb_show() if we add a virtual keyboard ??????????????????????????
        fileTypes = [('G-Code Files', '*.ngc *.nc *.tap'),
                 ('All Files', '*.*')]
        defaultExt = '.ngc'
        file = asksaveasfilename(filetypes=fileTypes, defaultextension=defaultExt)
        if file:
             COPY(self.fNgc, file)
             self.saveC['state'] = 'disabled'
#        self.vkb_show(True) if we add a virtual keyboard ??????????????????????
        for entry in self.entries:
            #entry.bind('<KeyRelease>', self.auto_preview) # plays havoc with good error detection
            #entry.bind('<Return>', self.auto_preview, add='+')
            #entry.bind('<KP_Enter>', self.auto_preview, add='+')
            entry.bind('<Return>', self.preview, add='+')
            entry.bind('<KP_Enter>', self.preview, add='+')

    def settings_pressed(self):
        self.savedSettings['lead_in'] = self.liValue.get()
        self.savedSettings['lead_out'] = self.loValue.get()
        self.savedSettings['start_pos'] = self.spbValue.get()
        self.savedSettings['cut_type'] = self.ctbValue.get()
        self.buttonState['newC'] = self.newC['state']
        self.buttonState['sendC'] = self.sendC['state']
        self.buttonState['saveC'] = self.saveC['state']
        self.buttonState['settingsC'] = self.settingsC['state']
        self.newC['state'] = 'disabled'
        self.sendC['state'] = 'disabled'
        self.saveC['state'] = 'disabled'
        self.settingsC['state'] = 'disabled'
        self.clear_widgets()
        if self.comp['development']:
            reload(CONVSET)
        for child in self.toolFrame.winfo_children():
            if child.winfo_class() == 'Radiobutton':
                child['state'] = 'disabled'
        CONVSET.widgets(self)
        CONVSET.show(self)

    def send_pressed(self):
        COPY(self.fNgc, self.fNgcSent)
        #COPY(self.fNgc, self.filteredBkp)
        self.existingFile = self.fNgc
        self.saveC['state'] = 'disabled'
        self.sendC['state'] = 'disabled'
        self.fileOpener(self.fNgc, False, False)
        self.conv_toggle(0, True)

    def block_request(self):
#       may need to add this halpin for wcs rotation on abort etc.
#       self.convBlockLoaded = self.h.newpin('conv_block_loaded', hal.HAL_BIT, hal.HAL_IN)
        if not self.settingsExited:
            if self.previewActive and self.active_shape():
                return True
            title = _('Array Error')
            with open(self.fNgc) as inFile:
                for line in inFile:
                    if '(new conversational file)' in line:
                        msg0 = _('An empty file cannot be arrayed, rotated, or scaled')
                        self.plasmacPopUp('error', title, msg0)
                        return True
                    # see if we can do something about NURBS blocks down the track
                    elif 'g5.2' in line.lower() or 'g5.3' in line.lower():
                        msg0 = _('Cannot scale a GCode NURBS block')
                        self.plasmacPopUp('error', title, msg0)
                        return True
                    elif 'M3' in line or 'm3' in line:
                        break
        return

    def shape_request(self, shape): #, material):
        if shape == 'closer':
            self.conv_toggle(0)
            return
        self.oldModule = self.module
        if shape == 'block':
            if self.o.canon is not None:
                unitsMultiplier = 25.4 if self.unitsPerMm == 1 else 1
                self.shapeLen['x'] = (self.o.canon.max_extents[0] - self.o.canon.min_extents[0]) * unitsMultiplier
                self.shapeLen['y'] = (self.o.canon.max_extents[1] - self.o.canon.min_extents[1]) * unitsMultiplier
            self.module = CONVBLCK
            if self.block_request():
                self.toolButton[self.convShapes.index(self.oldConvButton)].invoke()
                return
        elif shape == 'line': self.module = CONVLINE
        elif shape == 'circle': self.module = CONVCIRC
        elif shape == 'ellipse': self.module = CONVELLI
        elif shape == 'triangle': self.module = CONVTRIA
        elif shape == 'rectangle': self.module = CONVRECT
        elif shape == 'polygon': self.module = CONVPOLY
        elif shape == 'boltcircle': self.module = CONVBOLT
        elif shape == 'slot': self.module = CONVSLOT
        elif shape == 'star': self.module = CONVSTAR
        elif shape == 'gusset': self.module = CONVGUST
        elif shape == 'sector': self.module = CONVSECT
        else: return
        if self.comp['development']:
            reload(self.module)
        if not self.settingsExited:
            if self.previewActive and self.active_shape():
                return
            self.preview_button_pressed(False)
        self.oldConvButton = shape
        self.settingsC['state'] = 'normal'
        self.previewC['state'] = 'normal'
        self.loLabel['state'] = 'normal'
        self.loEntry['state'] = 'normal'
        if self.validShape:
            self.undoC['state'] = 'normal'
        self.addC['state'] = 'disabled'
        self.clear_widgets()
        self.ocButton.config(relief="raised", bg=self.bBackColor)
        if self.settingsExited:
            self.liValue.set(self.savedSettings['lead_in'])
            self.loValue.set(self.savedSettings['lead_out'])
            self.spbValue.set(self.savedSettings['start_pos'])
            self.ctbValue.set(self.savedSettings['cut_type'])
        else:
            self.ctbValue.set(_('EXTERNAL'))
            if self.origin:
                self.spbValue.set(_('CENTER'))
            else:
                self.spbValue.set(_('BTM LEFT'))
            self.liValue.set(f"{self.leadIn}")
            self.loValue.set(f"{self.leadOut}")
        self.module.widgets(self)
        self.settingsExited = False

    def preview_button_pressed(self, state):
        self.previewActive = state
        if state:
            self.saveC['state'] = 'disabled'
            self.sendC['state'] = 'disabled'
            self.undoC['text'] = _('UNDO')
        else:
            if self.validShape:
                self.saveC['state'] = 'normal'
                self.sendC['state'] = 'normal'
            self.undoC['text'] = _('RELOAD')
            self.addC['state'] = 'disabled'
        self.setViewZ()

    def active_shape(self):
        for child in self.toolFrame.winfo_children():
            child.deselect()
        title = _('Active Preview')
        msg0 = _('Cannot continue with an active previewed shape')
        reply = self.plasmacPopUp('warn', title, f"{msg0}\n").reply
        if self.oldConvButton:
            self.toolButton[self.convShapes.index(self.oldConvButton)].select()
        self.module = self.oldModule
        return reply

    def restore_buttons(self):
        self.newC['state'] = self.buttonState['newC']
        self.sendC['state'] = self.buttonState['sendC']
        self.saveC['state'] = self.buttonState['saveC']
        self.settingsC['state'] = self.buttonState['settingsC']

    def conv_is_float(self, entry):
        try:
            return True, float(entry)
        except:
            reply = -1 if entry else 0
            return False, reply

    def conv_is_int(self, entry):
        try:
            return True, int(entry)
        except:
            reply = -1 if entry else 0
            return False, reply

    def undo_shape(self):
        if not self.previewActive:
            title = _('Reload Request')
            if self.existingFile:
                name = os.path.basename(self.existingFile)
                msg0 = _('The original file will be loaded')
                msg1 = _('If you continue all changes will be deleted')
                if not self.plasmacPopUp('yesno', title, f"{msg0}:\n\n{name}\n\n{msg1}\n").reply:
                    return(True)
            else:
                msg0 = _('An empty file will be loaded')
                msg1 = _('If you continue all changes will be deleted')
                if not self.plasmacPopUp('yesno', title, f"{msg0}\n\n{msg1}\n").reply:
                    return(True)
            if self.existingFile:
                COPY(self.existingFile, self.fNgcBkp)
            else:
                with open(self.fNgcBkp, 'w') as outNgc:
                    outNgc.write('(new conversational file)\nM2\n')
            self.validShape = False
            self.previewC['state'] = 'normal'
            self.undoC['state'] = 'disabled'
            self.saveC['state'] = 'disabled'
            self.sendC['state'] = 'disabled'
        if self.convButton.get() == 'line':
            if self.previewActive:
                if self.convLine['addSegment'] > 1:
                    self.convLine['addSegment'] = 1
                self.module.line_type_changed(self, True)
            else:
                self.convLine['addSegment'] = 0
                self.module.line_type_changed(self, False)
            if self.undoC['text'] == _('RELOAD'):
                self.convLine['xLineStart'] = 0.000
                self.convLine['yLineStart'] = 0.000
                self.l1Value.set('0.000')
                self.l2Value.set('0.000')
            self.convLine['xLineEnd'] = self.convLine['xLineStart']
            self.convLine['yLineEnd'] = self.convLine['yLineStart']
            if len(self.convLine['gcodeSave']):
                self.convLine['gcodeLine'] = self.convLine['gcodeSave']
            self.previewActive = False
        # undo the shape
        if os.path.exists(self.fNgcBkp):
            COPY(self.fNgcBkp, self.fNgc)
            self.fileOpener(self.fNgc, True, False)
            self.addC['state'] = 'disabled'
            if not self.validShape:
                self.undoC['state'] = 'disabled'
            if not self.convBlock[1]:
                self.convBlock[0] = False
            self.preview_button_pressed(False)

    def add_shape_to_file(self):
        COPY(self.fNgc, self.fNgcBkp)
        self.validShape = True
        self.addC['state'] = 'disabled'
        self.saveC['state'] = 'normal'
        self.sendC['state'] = 'normal'
        self.preview_button_pressed(False)
        if self.convButton.get() == 'line':
            self.convLine['convAddSegment'] = self.convLine['gcodeLine']
            self.convLine['xLineStart'] = self.convLine['xLineEnd']
            self.convLine['yLineStart'] = self.convLine['yLineEnd']
            self.l1Value.set(f"{self.convLine['xLineEnd']:0.3f}")
            self.l2Value.set(f"{self.convLine['yLineEnd']:0.3f}")
            self.convLine['addSegment'] = 1
            self.module.line_type_changed(self, True)
            self.addC['state'] = 'disabled'
            self.previewActive = False

    def clear_widgets(self):
        for child in self.convFrame.winfo_children():
            if not self.settingsExited and isinstance(child, tk.Entry):
                if child.winfo_name() == str(getattr(self, 'liEntry')).rsplit('.',1)[1]:
                    pass
                elif child.winfo_name() == str(getattr(self, 'loEntry')).rsplit('.',1)[1]:
                    pass
                elif child.winfo_name() == str(getattr(self, 'liEntry')).rsplit('.',1)[1]:
                    pass
                elif child.winfo_name() == str(getattr(self, 'liEntry')).rsplit('.',1)[1]:
                    pass
                elif child.winfo_name() == str(getattr(self, 'caEntry')).rsplit('.',1)[1]:
                    self.caValue.set('360')
                elif child.winfo_name() == str(getattr(self, 'cnEntry')).rsplit('.',1)[1]:
                    self.cnValue.set('1')
                elif child.winfo_name() == str(getattr(self, 'rnEntry')).rsplit('.',1)[1]:
                    self.rnValue.set('1')
                elif child.winfo_name() == str(getattr(self, 'scEntry')).rsplit('.',1)[1]:
                    self.scValue.set('1.0')
                elif child.winfo_name() == str(getattr(self, 'ocEntry')).rsplit('.',1)[1]:
                    self.ocValue.set(f"{4 * self.unitsPerMm}")
            child.grid_remove()
        self.show_common_widgets()

    def cut_type_clicked(self, circle=False):
        if self.ctbValue.get() == _('EXTERNAL'):
            self.ctbValue.set(_('INTERNAL'))
        else:
            self.ctbValue.set(_('EXTERNAL'))
        if circle:
            try:
                dia = float(self.dValue.get())
            except:
                dia = 0
            if dia >= self.smallHoleDia or dia == 0 or self.ctbValue.get() == _('EXTERNAL'):
                self.ocButton.config(relief='raised', bg=self.bBackColor, state='disabled')
                self.ocEntry['state'] = 'disabled'
            else:
                self.ocButton['state'] = 'normal'
                self.ocEntry['state'] = 'normal'
        self.module.auto_preview(self)

    def start_point_clicked(self):
        if self.spbValue.get() == _('BTM LEFT'):
            self.spbValue.set(_('CENTER'))
        else:
            self.spbValue.set(_('BTM LEFT'))
        self.module.auto_preview(self)

    def material_changed(self):#, event):
#        self.matCombo.selection_clear()
        self.module.auto_preview(self)

    def show_common_widgets(self):
        self.convFrame.rowconfigure(11, weight=1)
        self.spacer.grid(column=0, row=11, sticky='ns')
        self.previewC.grid(column=0, row=12, padx=(4,0))
        self.addC.grid(column=1, row=12, columnspan=2)
        self.undoC.grid(column=3, row=12)
        self.sepline.grid(column=0, row=13, columnspan=4, sticky='ew', pady=8, padx=8)
        self.newC.grid(column=0, row=14, padx=(4,0), pady=(0,4))
        self.saveC.grid(column=1, row=14, padx=(4,0), pady=(0,4))
        self.settingsC.grid(column=2, row=14, padx=(4,0), pady=(0,4))
        self.sendC.grid(column=3, row=14, padx=(4,0), pady=(0,4))

    def create_widgets(self, imagePath):
        for s in self.toolFrame.pack_slaves():
            s.destroy()
        for s in self.convFrame.grid_slaves():
            s.destroy()
        self.convShapes = ['line','circle','ellipse','triangle','rectangle','polygon',\
                           'boltcircle','slot','star','gusset','sector','block','closer']
        self.convButton = tk.StringVar()
        self.toolButton = []
        self.images = []
        # toolbar buttons
        for i in range(len(self.convShapes)):
            self.toolButton.append(tk.Radiobutton(self.toolFrame, text=self.convShapes[i], \
                    command=lambda i=i: self.shape_request(self.convShapes[i]), anchor='center', \
                    indicatoron=0, selectcolor='gray60', highlightthickness=0, bd=2, \
                    variable=self.convButton, value=self.convShapes[i], takefocus=0))
            self.images.append(tk.PhotoImage(file = os.path.join(imagePath, self.convShapes[i]) + '.png'))
            self.toolButton[i]['image'] = self.images[i]
            self.toolButton[i]['width'] = 40
            self.toolButton[i]['height'] = 40
            if self.convShapes[i] == 'line':
                self.toolButton[i].pack(side='left', padx=(2,0), pady=1)
            elif self.convShapes[i] == 'closer':
                self.toolButton[i].pack(side='right', padx=(0,2), pady=1)
            else:
                self.toolButton[i].pack(side='left', padx=(4,0), pady=1)
        lLength = 10
        eLength = 10
        self.matLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('MATERIAL'))
        self.matCombo = self.combobox(self.convFrame, width=lLength, justify='left', bd=1, editable=False)
        self.matCombo['modifycmd'] = self.material_changed
        self.spLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('START'))
        self.spbValue = tk.StringVar()
        self.spButton = tk.Button(self.convFrame, width=eLength, textvariable=self.spbValue, padx=1, pady=1)
        self.ctLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('CUT TYPE'))
        self.ctbValue = tk.StringVar()
        self.ctButton = tk.Button(self.convFrame, width=eLength, textvariable=self.ctbValue, padx=1, pady=1)
        self.xsLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('X ORIGIN'))
        self.xsValue = tk.StringVar()
        self.xsEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.xsValue, justify='right', bd=1)
        self.ysLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('Y ORIGIN'))
        self.ysValue = tk.StringVar()
        self.ysEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.ysValue, justify='right', bd=1)
        self.liLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('LEAD IN'))
        self.liValue = tk.StringVar()
        self.liEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.liValue, justify='right', bd=1)
        self.loLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('LEAD OUT'))
        self.loValue = tk.StringVar()
        self.loEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.loValue, justify='right', bd=1)
        self.polyLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('TYPE'))
        self.polyCombo = self.combobox(self.convFrame, width=lLength, justify='left', bd=1, editable=False)
        self.sLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('SIDES'))
        self.sValue = tk.StringVar()
        self.sEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.sValue, justify='right', bd=1)
        self.dLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('DIAMETER'))
        self.dValue = tk.StringVar()
        self.dEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.dValue, justify='right', bd=1)
        self.lLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('LENGTH'))
        self.lValue = tk.StringVar()
        self.lEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.lValue, justify='right', bd=1)
        self.wLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('WIDTH'))
        self.wValue = tk.StringVar()
        self.wEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.wValue, justify='right', bd=1)
        self.hLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('HEIGHT'))
        self.hValue = tk.StringVar()
        self.aaLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('A ANGLE'))
        self.aaValue = tk.StringVar()
        self.aaEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.aaValue, justify='right', bd=1)
        self.alLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('a LENGTH'))
        self.alValue = tk.StringVar()
        self.alEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.alValue, justify='right', bd=1)
        self.baLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('B ANGLE'))
        self.baValue = tk.StringVar()
        self.baEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.baValue, justify='right', bd=1)
        self.blLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('b LENGTH'))
        self.blValue = tk.StringVar()
        self.blEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.blValue, justify='right', bd=1)
        self.caLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('C ANGLE'))
        self.caValue = tk.StringVar()
        self.caEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.caValue, justify='right', bd=1)
        self.clLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('c LENGTH'))
        self.clValue = tk.StringVar()
        self.clEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.clValue, justify='right', bd=1)
        self.hEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.hValue, justify='right', bd=1)
        self.hdLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('HOLE DIA'))
        self.hdValue = tk.StringVar()
        self.hdEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.hdValue, justify='right', bd=1)
        self.hoLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('# HOLES'))
        self.hoValue = tk.StringVar()
        self.hoEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.hoValue, justify='right', bd=1)
        self.bcaLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('CIRCLE ANG'))
        self.bcaValue = tk.StringVar()
        self.bcaEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.bcaValue, justify='right', bd=1)
        self.ocLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('OVERCUT'))
        self.ocValue = tk.StringVar()
        self.ocEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.ocValue, justify='right', bd=1)
        self.ocbValue = tk.StringVar(self.r, _('OFF'))
        self.ocButton = tk.Button(self.convFrame, width=eLength, textvariable=self.ocbValue, padx=1, pady=1)
        self.pLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('POINTS'))
        self.pValue = tk.StringVar()
        self.pEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.pValue, justify='right', bd=1)
        self.odLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('OUTER DIA'))
        self.odValue = tk.StringVar()
        self.odEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.odValue, justify='right', bd=1)
        self.idLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('INNER DIA'))
        self.idValue = tk.StringVar()
        self.idEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.idValue, justify='right', bd=1)

        self.rLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('RADIUS'))
        self.rbValue = tk.StringVar(self.r, _('RADIUS'))
        self.rButton = tk.Button(self.convFrame, width=eLength, textvariable=self.rbValue, padx=1, pady=1)
        self.rValue = tk.StringVar()
        self.rEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.rValue, justify='right', bd=1)

        self.saLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('SEC ANGLE'))
        self.saValue = tk.StringVar()
        self.saEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.saValue, justify='right', bd=1)

        self.aLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('ANGLE'))
        self.aValue = tk.StringVar()
        self.aEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.aValue, justify='right', bd=1)

        self.r1bValue = tk.StringVar()
        self.r1Button = tk.Button(self.convFrame, width=eLength, textvariable=self.r1bValue, padx=1, pady=1)
        self.r1Value = tk.StringVar()
        self.r1Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.r1Value, justify='right', bd=1)

        self.r2bValue = tk.StringVar()
        self.r2Button = tk.Button(self.convFrame, width=eLength, textvariable=self.r2bValue, padx=1, pady=1)
        self.r2Value = tk.StringVar()
        self.r2Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.r2Value, justify='right', bd=1)

        self.r3bValue = tk.StringVar()
        self.r3Button = tk.Button(self.convFrame, width=eLength, textvariable=self.r3bValue, padx=1, pady=1)
        self.r3Value = tk.StringVar()
        self.r3Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.r3Value, justify='right', bd=1)

        self.r4bValue = tk.StringVar()
        self.r4Button = tk.Button(self.convFrame, width=eLength, textvariable=self.r4bValue, padx=1, pady=1)
        self.r4Value = tk.StringVar()
        self.r4Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.r4Value, justify='right', bd=1)

        # block
        self.ccLabel = tk.Label(self.convFrame, width=lLength, text=_('COLUMNS'))
        self.cnLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('NUMBER'))
        self.cnValue = tk.StringVar()
        self.cnEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.cnValue, justify='right', bd=1)
        self.coLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('OFFSET'))
        self.coValue = tk.StringVar()
        self.coEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.coValue, justify='right', bd=1)

        self.rrLabel = tk.Label(self.convFrame, width=lLength, text=_('ROWS'))
        self.rnLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('NUMBER'))
        self.rnValue = tk.StringVar()
        self.rnEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.rnValue, justify='right', bd=1)
        self.roLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('OFFSET'))
        self.roValue = tk.StringVar()
        self.roEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.roValue, justify='right', bd=1)

        self.bsLabel = tk.Label(self.convFrame, width=lLength, text=_('SHAPE'))
        self.scLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('SCALE'))
        self.scValue = tk.StringVar()
        self.scEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.scValue, justify='right', bd=1)
        self.rtLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('ROTATION'))
        self.rtValue = tk.StringVar()
        self.rtEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.rtValue, justify='right', bd=1)
        self.mirror = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('MIRROR'))
        self.flip = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('FLIP'))
        self.oLabel = tk.Label(self.convFrame, width=lLength, text=_('ORIGIN OFFSET'))
        # lines and arcs
        self.lnLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('TYPE'))
        self.lineCombo = self.combobox(self.convFrame, width=lLength, justify='left', bd=1, editable=False)
        self.l1Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l1Value = tk.StringVar()
        self.l1Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l1Value, justify='right', bd=1)
        self.l2Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l2Value = tk.StringVar()
        self.l2Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l2Value, justify='right', bd=1)
        self.l3Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l3Value = tk.StringVar()
        self.l3Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l3Value, justify='right', bd=1)
        self.l4Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l4Value = tk.StringVar()
        self.l4Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l4Value, justify='right', bd=1)
        self.l5Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l5Value = tk.StringVar()
        self.l5Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l5Value, justify='right', bd=1)
        self.l6Label = tk.Label(self.convFrame, width=lLength, anchor='e', text='')
        self.l6Value = tk.StringVar()
        self.l6Entry = tk.Entry(self.convFrame, width=eLength, textvariable=self.l6Value, justify='right', bd=1)
        self.g23bValue = tk.StringVar()
        self.g23Arc = tk.Button(self.convFrame, width=eLength, textvariable=self.g23bValue, padx=1, pady=1)
        # settings
        self.preLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('PREAMBLE'))
        self.preValue = tk.StringVar()
        self.preEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.preValue, bd=1)
        self.pstLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('POSTAMBLE'))
        self.pstValue = tk.StringVar()
        self.pstEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.pstValue, bd=1)
        self.llLabel = tk.Label(self.convFrame, width=lLength, text=_('LEAD LENGTHS'))
        self.shLabel = tk.Label(self.convFrame, width=lLength, text=_('SMALL HOLES'))
        self.shValue = tk.StringVar()
        self.shEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.shValue, justify='right', bd=1)
        self.hsValue = tk.StringVar()
        self.hsEntry = tk.Entry(self.convFrame, width=eLength, textvariable=self.hsValue, justify='right', bd=1)
        self.hsLabel = tk.Label(self.convFrame, width=lLength, anchor='e', text=_('SPEED %'))
        self.pvLabel = tk.Label(self.convFrame, width=lLength, text=_('PREVIEW'))
        self.saveS = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('SAVE'))
        self.reloadS = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('RELOAD'))
        self.exitS = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('EXIT'))
        # bottom
        self.previewC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('PREVIEW'))
        self.addC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('ADD'))
        self.undoC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('UNDO'))
        self.newC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('NEW'))
        self.saveC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('SAVE'))
        self.settingsC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('SETTINGS'))
        self.sendC = tk.Button(self.convFrame, width=eLength, padx=1, pady=1, text=_('SEND'))
        # spacer and separator
        self.spacer = tk.Label(self.convFrame)
        self.sepline = ttk.Separator(self.convFrame, orient='horizontal')
        # get default background color
        self.bBackColor = self.sendC.cget('bg')
        # create lists of entries
        self.uInts   = [self.hoEntry, self.sEntry, self.pEntry, self.cnEntry, self.rnEntry]
        self.sFloats = [self.xsEntry, self.ysEntry, self.aEntry, self.caEntry, \
                        self.coEntry, self.roEntry, self.rtEntry]
        self.uFloats = [self.liEntry, self.loEntry, self.dEntry, self.ocEntry, self.hdEntry, \
                        self.wEntry, self.rEntry, self.saEntry, self.hEntry, self.r1Entry, \
                        self.r2Entry, self.r3Entry, self.r4Entry, self.lEntry, self.odEntry, \
                        self.idEntry, self.aaEntry, self.baEntry, self.caEntry, self.alEntry, \
                        self.blEntry, self.clEntry, self.scEntry, self.shEntry, self.hsEntry, \
                        self.l1Entry, self.l2Entry, self.l3Entry, self.l4Entry, self.l5Entry, \
                        self.l6Entry, ]
        self.entries   = []
        for entry in self.uInts: self.entries.append(entry)
        for entry in self.sFloats: self.entries.append(entry)
        for entry in self.uFloats: self.entries.append(entry)
        self.buttons   = [self.spButton, self.ctButton, self.ocButton, self.rButton, self.r1Button, \
                          self.r2Button, self.r3Button, self.r4Button, self.mirror, self.flip, \
                          self.g23Arc, self.saveS, self.reloadS, self.exitS, self.previewC, \
                          self.addC, self.undoC, self.newC, self.saveC, self.settingsC, self.sendC]
        for button in self.buttons:
            button.configure(takefocus=0)
        self.combos = [self.matCombo, self.polyCombo, self.lineCombo]
        for combo in self.combos:
            combo.configure(takefocus=0)
