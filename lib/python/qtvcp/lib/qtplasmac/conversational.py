'''
conversational.py

Copyright (C) 2019 - 2025 Phillip A Carter
Copyright (C) 2020 - 2025 Gregory D Carl

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

import os
import re
from shutil import copy as COPY
from importlib import reload
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QFileDialog, QMessageBox, QPushButton, QLabel, QLineEdit
from PyQt5.QtWidgets import QComboBox
from qtvcp.core import Status, Action
from qtvcp.lib.qtplasmac import conv_settings as CONVSET
from qtvcp.lib.qtplasmac import conv_line as CONVLINE
from qtvcp.lib.qtplasmac import conv_circle as CONVCIRC
from qtvcp.lib.qtplasmac import conv_ellipse as CONVELLI
from qtvcp.lib.qtplasmac import conv_triangle as CONVTRIA
from qtvcp.lib.qtplasmac import conv_rectangle as CONVRECT
from qtvcp.lib.qtplasmac import conv_polygon as CONVPOLY
from qtvcp.lib.qtplasmac import conv_bolt as CONVBOLT
from qtvcp.lib.qtplasmac import conv_slot as CONVSLOT
from qtvcp.lib.qtplasmac import conv_star as CONVSTAR
from qtvcp.lib.qtplasmac import conv_gusset as CONVGUST
from qtvcp.lib.qtplasmac import conv_sector as CONVSECT
from qtvcp.lib.qtplasmac import conv_block as CONVBLCK
from rs274.glcanon import GlCanonDraw as DRAW

STATUS = Status()
ACTION = Action()

_translate = QCoreApplication.translate

gcodePattern = re.compile(
        r"(;row(?P<OP>.[^X]+) *)?"
        r"([Gg](?P<G1> *\d{1,2}) *)?"
        r"([Gg](?P<G2> *\d{1,2}) *)?"
        r"([Xx](?P<X> *-*\d+\.?\d*) *)?"
        r"([Yy](?P<Y> *-*\d+\.?\d*) *)?"
        r"([Zz](?P<Z>( *-*\d+\.?\d*)|([^;^(]*)) *)?"
        r"([Ii](?P<I> *-*\d+\.?\d*) *)?"
        r"([Jj](?P<J> *-*\d+\.?\d*) *)?"
        r"([Mm](?P<M> *\d{1,3}) *)?"
        r"([Pp](?P<P> *-*\d+\.?\d*) *)?"
        r"([Qq](?P<Q> *-*\d+\.?\d*) *)?"
        r"((?P<C>[;(].+) *)?"
)

def conv_setup(P, W):
    P.convSettingsChanged = False
    P.validShape = False
    P.savedSettings = {'origin': None, 'intext': None, 'in': None, 'out': None}
    P.invalidLeads = 0
    P.isConvBlock = None
    if not P.convWidgetsLoaded or P.developmentPin.get():
        conv_widgets(P, W)
    if not ACTION.prefilter_path:
        W.undo.setEnabled(False)
    conv_preview_button(P, W, False)
    P.convButtonState = {}
    P.convCommonButtons = ['new', 'save', 'send', 'settings']
    for w in P.convCommonButtons:
        P.convButtonState[w] = False
    if P.unitsPerMm == 1:
        P.unitCode = ['21', '0.25', 32]
        W.conv_preview.setmetric(True)
    else:
        P.unitCode = ['20', '0.004', 1.26]
        W.conv_preview.setmetric(False)
    P.ambles = f'G{P.unitCode[0]}\nG64P{P.unitCode[1]}\nG40\nG49\nM52P1\nG80\nG90\nG92.1\nG94\nG97'
    CONVSET.load(P, W)
    # grid size is in inches
    W.conv_preview.grid_size = P.gridSize / P.unitsPerMm / 25.4
    W.conv_preview.set_current_view()
    W.conv_new.setEnabled(True)
    W.conv_save.setEnabled(False)
    W.conv_send.setEnabled(False)
    W.conv_settings.setEnabled(True)
    if ACTION.prefilter_path and P.fileOpened:
        W.conv_preview.load(P.filteredBkp)
        machine = W.conv_preview.gcode_properties['machine_unit_sys']
        gcode = W.conv_preview.gcode_properties['gcode_units']
        if machine == 'Metric' and gcode == 'in':
                gcode_convert_units(P.filteredBkp, P.fNgcBkp, W.conv_preview, P.unitsPerMm, '20', '21', 25.4, 25.4)
        elif machine == 'Imperial' and gcode == 'mm':
                gcode_convert_units(P.filteredBkp, P.fNgcBkp, W.conv_preview, P.unitsPerMm, '21', '20', 1, 1/25.4)
        with open(P.filteredBkp, 'r') as blockCheck:
            line = blockCheck.readline()
            if line.startswith(';conversational block V2'):
                P.isConvBlock = 'loaded'
        W.conv_preview.set_current_view()
        conv_enable_tabs(P, W)
        COPY(P.filteredBkp, P.fNgc)
        COPY(P.filteredBkp, P.fNgcBkp)
    else:
        with open(P.filteredBkp, 'w') as outNgc:
            outNgc.write('(new conversational file)\nM2\n')
        conv_new_pressed(P, W, None)
    P.xOrigin = STATUS.get_position()[1][0]
    P.yOrigin = STATUS.get_position()[1][1]
    P.xSaved = '0.000'
    P.ySaved = '0.000'
    if P.isConvBlock:
        conv_shape_request(P, W, 'conv_block')
    elif P.oldConvButton and not P.oldConvButton == 'conv_block':
        conv_shape_request(P, W, P.oldConvButton)
    else:
        conv_shape_request(P, W, 'conv_line')

def gcode_convert_units(file, bkp, window, unitsPerMm, oldG, newG, eMult, uMult):
    COPY(file, bkp)
    template = ''
    inputs = False
    with open(bkp, 'r') as inFile:
        with open(file, 'w') as outFile:
            for line in inFile:
                line = line.strip()
                if line and line[0] != '(':
                    if line[0] == ';':
                        if ';conversational block V2' in line:
                            template = ';'
                        elif ';CONV_BLOCK TEMPLATE END' in line:
                            template = ''
                        # if it is a conversational block
                        elif inputs:
                            values = line.lstrip(';').split(', ')
                            cSpacing = float(values[0]) * uMult
                            rSpacing = float(values[1]) * uMult
                            columns = int(values[2])
                            rows    = int(values[3])
                            # set the converted values
                            values[0] = f"{cSpacing:0.4f}"
                            values[1] = f"{rSpacing:0.4f}"
                            values[4] = f"{float(values[4]) * uMult:0.4f}"
                            values[5] = f"{float(values[5]) * uMult:0.4f}"
                            if not float(values[11]) and not float(values[12]):
                                # get the overall extents
                                xMid = DRAW.extents_info(window)[0][0] * eMult - STATUS.stat.g5x_offset[0]
                                yMid = DRAW.extents_info(window)[0][1] * eMult - STATUS.stat.g5x_offset[1]
                                # get the midpoints of the first shape only
                                if columns > 1:
                                    xMid -= (columns - 1) * cSpacing / 2
                                if rows > 1:
                                    yMid -= (rows - 1) * rSpacing / 2
                                values[11] = f"{xMid:0.4f}"
                                values[12] = f"{yMid:0.4f}"
                            else:
                                values[11] = f"{float(values[11]) * uMult:0.4f}"
                                values[12] = f"{float(values[12]) * uMult:0.4f}"
                            line = f";{', '.join(values)}"
                            inputs = False
                        elif ';CONV_BLOCK INPUTS' in line:
                            inputs = True
                    line = line[1:] if template else line
                    code = gcodePattern.match(line).groupdict()
                    comment = code['C'].strip() if code['C'] else ''
                    originPoint = f";row{code['OP'].strip()}" if code['OP'] else ''
                    if originPoint:
                        line = f"{originPoint} X{float(code['X']) * uMult:0.4f} Y{float(code['Y']) * uMult:0.4f}"
                    if 'F[#<_hal[plasmac.cut-feed-rate]>' in line:
                        line = f"F#<_hal[plasmac.cut-feed-rate]> {comment}"
                    if code['G1'] == oldG:
                        line = line.replace(code['G1'], newG)
                    elif code['G1'] == '53' and '#<_ini[axis_z]max_limit>' in line:
                        line = f"G53 G00 Z[#<_ini[axis_z]max_limit> - {5.00 * unitsPerMm:0.4f}] {comment}"
                    elif code['G1'] == '64':
                        Q = float(code['Q']) if code['Q'] else 0.0
                        line = f"G{code['G1']} P{float(code['P']) * uMult:0.4f} Q{Q * uMult:0.4f} {comment}"
                    if code['X']:
                        line = line.replace(f"X{code['X']}", f"X{float(code['X']) * uMult:0.4f}")
                    if code['Y']:
                        line = line.replace(f"Y{code['Y']}", f"Y{float(code['Y']) * uMult:0.4f}")
                    if code['I']:
                        line = line.replace(f"I{code['I']}", f"I{float(code['I']) * uMult:0.4f}")
                    if code['J']:
                        line = line.replace(f"J{code['J']}", f"J{float(code['J']) * uMult:0.4f}")
                    line = template + line
                outFile.write(f"{line}\n")
    window.load(file)

def conv_new_pressed(P, W, button):
    if button and (W.conv_save.isEnabled() or W.conv_send.isEnabled() or P.convPreviewActive or P.oldConvButton == 'conv_block'):
        head = _translate('HandlerClass', 'Unsaved Shape')
        btn1 = _translate('HandlerClass', 'CONTINUE')
        btn2 = _translate('HandlerClass', 'CANCEL')
        if P.oldConvButton == 'conv_block':
            msg0 = _translate('HandlerClass', 'A loaded file from the MAIN tab is being previewed')
            msg1 = _translate('HandlerClass', 'If you continue it will be removed from the Conversational preview')
        else:
            msg0 = _translate('HandlerClass', 'You have an unsaved, unsent, or active previewed shape')
            msg1 = _translate('HandlerClass', 'If you continue it will be deleted')
        if not P.dialog_show_yesno(QMessageBox.Warning, f'{head}', f'{msg0}\n\n{msg1}\n', f'{btn1}', f'{btn2}'):
            return
    if P.oldConvButton == 'conv_line':
        if W.lType.currentText() == _translate('Conversational', 'LINE POINT ~ POINT'):
            CONVLINE.set_line_point_to_point(P, W)
        elif W.lType.currentText() == _translate('Conversational', 'LINE BY ANGLE'):
            CONVLINE.set_line_by_angle(P, W)
        elif W.lType.currentText() == _translate('Conversational', 'ARC 3P'):
            CONVLINE.set_arc_3_points(P, W)
        elif W.lType.currentText() == _translate('Conversational', 'ARC 2P +RADIUS'):
            CONVLINE.set_arc_2_points_radius(P, W)
        elif W.lType.currentText() == _translate('Conversational', 'ARC ANGLE +RADIUS'):
            CONVLINE.set_arc_by_angle_radius(P, W)
        W.entry1.setText('0.000')
        W.entry2.setText('0.000')
    with open(P.fNgc, 'w') as outNgc:
        outNgc.write('(new conversational file)\nM2\n')
    COPY(P.fNgc, P.fTmp)
    COPY(P.fNgc, P.fNgcBkp)
    W.conv_preview.load(P.fNgc)
    W.conv_save.setEnabled(False)
    W.conv_send.setEnabled(False)
    P.validShape = False
    conv_preview_button(P, W, False)
    conv_enable_tabs(P, W)
    if P.oldConvButton == 'conv_block':
        conv_shape_request(P, W, 'conv_line')
    P.isConvBlock = None

def conv_save_pressed(P, W):
    head = _translate('HandlerClass', 'Save Error')
    with open(P.fNgc, 'r') as inFile:
        for line in inFile:
            if '(new conversational file)' in line:
                msg0 = _translate('HandlerClass', 'An empty file cannot be saved')
                P.dialog_show_ok(QMessageBox.Warning, f'{head}', f'{msg0}\n')
                return
    P.vkb_show()
    dlg = QFileDialog(W)
    dlg.setOptions(QFileDialog.DontUseNativeDialog)
    dlg.setAcceptMode(QFileDialog.AcceptSave)
    dlg.setNameFilters(['G-Code Files (*.ngc *.nc *.tap)', 'All Files (*)'])
    dlg.setDefaultSuffix('ngc')
    dlg.setDirectory(P.programPrefix)
    name = ''
    if dlg.exec_():
        name = dlg.selectedFiles()[0]
    if name:
        COPY(P.fNgc, name)
        W.conv_save.setEnabled(False)
        conv_enable_tabs(P, W)
    P.vkb_show(True)

def conv_settings_pressed(P, W):
    W.conv_material.hide()
    P.color_item(P.oldConvButton, P.foreColor, 'button')
    W[P.oldConvButton].setStyleSheet(f'QPushButton {{ background: {P.backColor} }} \
                                     QPushButton:pressed {{ background: {P.backColor} }}')
    for w in P.convCommonButtons:
        P.convButtonState[w] = W[f'conv_{w}'].isEnabled()
        W[f'conv_{w}'].setEnabled(False)
    conv_clear_widgets(P, W, True)
    if P.developmentPin.get():
        reload(CONVSET)
    CONVSET.widgets(P, W, P.CONV)
    CONVSET.show(P, W)

def conv_send_pressed(P, W):
    COPY(P.fNgcBkp, P.fNgcSent)
    W.conv_send.setEnabled(False)
    W.conv_save.setEnabled(False)
    conv_enable_tabs(P, W)
    P.vkb_hide()
    ACTION.OPEN_PROGRAM(P.fNgcSent)

def conv_block_pressed(P, W):
    if not P.convSettingsChanged:
        if P.convPreviewActive and not conv_active_shape(P, W):
            return
        head = _translate('HandlerClass', 'Array Error')
        with open(P.fNgc) as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    msg0 = _translate('HandlerClass', 'An empty file cannot be arrayed, rotated, or scaled')
                    P.dialog_show_ok(QMessageBox.Warning, f'{head}', f'{msg0}\n')
                    return
                # see if we can do something about NURBS blocks down the track
                # elif 'g5.2' in line.lower() or 'g5.3' in line.lower():
                #     head = _translate('HandlerClass', 'Scale Error')
                #     msg0 = _translate('HandlerClass', 'Cannot scale a GCode NURBS block')
                #     P.dialog_show_ok(QMessageBox.Warning, f'{head}', f'{msg0}\n\n{line}')
                #     return
                elif 'M3' in line.upper() or 'M03' in line.upper():
                    break
    conv_shape_request(P, W, W.sender().objectName())

def conv_shape_request(P, W, shape):
    if shape == 'conv_line':
        module = CONVLINE
    elif shape == 'conv_circle':
        module = CONVCIRC
    elif shape == 'conv_ellipse':
        module = CONVELLI
    elif shape == 'conv_triangle':
        module = CONVTRIA
    elif shape == 'conv_rectangle':
        module = CONVRECT
    elif shape == 'conv_polygon':
        module = CONVPOLY
    elif shape == 'conv_bolt':
        module = CONVBOLT
    elif shape == 'conv_slot':
        module = CONVSLOT
    elif shape == 'conv_star':
        module = CONVSTAR
    elif shape == 'conv_gusset':
        module = CONVGUST
    elif shape == 'conv_sector':
        module = CONVSECT
    elif shape == 'conv_block':
        module = CONVBLCK
    else:
        return
    if P.developmentPin.get():
        reload(module)
    if not P.convSettingsChanged:
        if P.convPreviewActive and not conv_active_shape(P, W):
            return
        conv_preview_button(P, W, False)
    if shape == 'conv_block':
        W.conv_material.hide()
        if P.isConvBlock:
            conv_shape_buttons(W, False)
        else:
            conv_shape_buttons(W, True)
    else:
        W.conv_material.show()
        conv_shape_buttons(W, True)
    # we use exception handlers here as there may be no signals connected
    try:
        W.conv_material.currentTextChanged.disconnect()
    except:
        pass
    try:
        W.preview.pressed.disconnect()
    except:
        pass
    try:
        W.undo.pressed.disconnect()
    except:
        pass
    conv_button_color(P, W, shape)
    W.conv_settings.setEnabled(True)
    W.preview.setEnabled(True)
    W.loLabel.setEnabled(True)
    W.loEntry.setEnabled(True)
    if P.validShape:
        W.undo.setEnabled(True)
    W.add.setEnabled(False)
    conv_clear_widgets(P, W)
    # common starting parameters
    if P.convSettingsChanged == 0:
        W.intExt.setText('EXTERNAL')
        P.intExt = True
    if P.convSettingsChanged <= 1:
        if P.origin:
            W.centLeft.setText('CENTER')
        else:
            W.centLeft.setText('BTM LEFT')
        W.liEntry.setText(f'{P.leadIn}')
        W.loEntry.setText(f'{P.leadOut}')
    # call the shape
    module.widgets(P, W, P.CONV)
    P.convSettingsChanged = False

def conv_preview_button(P, W, state):
    if state and P.oldConvButton == 'conv_block':
        conv_shape_buttons(W, False)
    P.convPreviewActive = state
    conv_enable_tabs(P, W)
    if state:
        W.preview.setStyleSheet(f'QPushButton {{ color: {P.estopColor} }} \
                                      QPushButton:disabled {{ color: {P.disabledColor} }}')
        W.conv_save.setEnabled(False)
        W.conv_send.setEnabled(False)
        W.undo.setText(_translate('HandlerClass', 'UNDO'))
    else:
        W.preview.setStyleSheet(f'QPushButton {{ color: {P.foreColor} }} \
                                      QPushButton:disabled {{ color: {P.disabledColor} }}')
        if P.validShape:
            W.conv_save.setEnabled(True)
            W.conv_send.setEnabled(True)
        W.undo.setText(_translate('HandlerClass', 'RELOAD'))
        if P.convWidgetsLoaded:
            W.add.setEnabled(False)

def conv_shape_buttons(W, state):
    buttons = ['line', 'circle', 'ellipse', 'triangle', 'rectangle', 'polygon', 'bolt', 'slot', 'star', 'gusset', 'sector']
    for button in buttons:
        W[f'conv_{button}'].setEnabled(state)

def conv_active_shape(P, W):
    btn1 = _translate('HandlerClass', 'CONTINUE')
    btn2 = _translate('HandlerClass', 'CANCEL')
    head = _translate('HandlerClass', 'Active Preview')
    msg0 = _translate('HandlerClass', 'You have an active previewed shape')
    msg1 = _translate('HandlerClass', 'If you continue it will be deleted')
    response = P.dialog_show_yesno(QMessageBox.Warning, f'{head}', f'{msg0}\n\n{msg1}\n', f'{btn1}', f'{btn2}')
    if response:
        conv_undo_shape(P, W)
        conv_preview_button(P, W, False)
    return response

def conv_button_color(P, W, button):
    if P.oldConvButton:
        P.color_item(P.oldConvButton, P.foreColor, 'button')
        W[P.oldConvButton].setStyleSheet(f'QPushButton {{ background: {P.backColor} }} \
                                         QPushButton:pressed {{ background: {P.backColor} }}')
    P.oldConvButton = button
    P.color_item(button, P.backColor, 'button')
    W[button].setStyleSheet(f'QPushButton {{ background: {P.foreColor} }} \
                            QPushButton:pressed {{ background: {P.foreColor} }}')

def conv_restore_buttons(P, W):
    for button in P.convCommonButtons:
        W[f'conv_{button}'].setEnabled(P.convButtonState[button])

def conv_enable_tabs(P, W):
    if W.conv_save.isEnabled() or P.convPreviewActive:
        for n in range(W.main_tab_widget.count()):
            if n != 1:
                W.main_tab_widget.setTabEnabled(n, False)
    else:
        for n in range(W.main_tab_widget.count()):
            W.main_tab_widget.setTabEnabled(n, True)
            # enabling tabs causes issues with the gcode widgets margin styles
            # so we refresh the style here as a workaround
            W.gcode_editor.setStyleSheet(f'EditorBase{{ qproperty-styleColorMarginText: {P.foreColor} }}')
            W.gcode_display.setStyleSheet(f'EditorBase{{ qproperty-styleColorMarginText: {P.foreColor} }}')

def conv_entry_changed(P, W, widget, circleType=False):
    if widget:
        name = widget.objectName()
        value = widget.text()
    else:
        name = value = None
    # circle specific code
    if circleType:
        P.invalidLeads = 0
        dia = W.hdEntry.text() if circleType == 'bolt' else W.dEntry.text()
        try:
            dia = float(dia)
        except:
            dia = 0
        # cannot do overcut if large hole, no hole, or external circle
        if dia >= P.holeDiameter or dia == 0 or (P.intExt and circleType != 'bolt'):
            W.overcut.setChecked(False)
            W.overcut.setEnabled(False)
            W.ocEntry.setEnabled(False)
        else:
            W.overcut.setEnabled(True)
            W.ocEntry.setEnabled(True)
        # cannot do leadout if small hole
        if not P.intExt or circleType == 'bolt':
            if dia < P.holeDiameter:
                W.loLabel.setEnabled(False)
                W.loEntry.setEnabled(False)
                P.invalidLeads = 2
            else:
                # test for large leadin or leadout
                try:
                    lIn = float(W.liEntry.text())
                except:
                    lIn = 0
                try:
                    lOut = float(W.loEntry.text())
                except:
                    lOut = 0
                if lIn and lIn > dia / 4:
                    W.loLabel.setEnabled(False)
                    W.loEntry.setEnabled(False)
                    P.invalidLeads = 2
                elif lOut and lOut > dia / 4:
                    W.loLabel.setEnabled(False)
                    P.invalidLeads = 1
                else:
                    W.loLabel.setEnabled(True)
                    W.loEntry.setEnabled(True)
                    P.invalidLeads = 0
        else:
            W.loLabel.setEnabled(True)
            W.loEntry.setEnabled(True)
            P.invalidLeads = 0
    if value:
        cursor_position = widget.cursorPosition()
        if name in ['intEntry', 'hsEntry', 'cnEntry', 'rnEntry']:
            good = '0123456789'
        elif name in ['xsEntry', 'ysEntry', 'aEntry', 'csEntry', 'rsEntry', 'neg']:
            good = '-.0123456789'
        else:
            good = '.0123456789'
        out = ''
        for t in value:
            if t in good and not(t == '-' and len(out) > 0) and not(t == '.' and t in out):
                out += t
        widget.setText(out)
        widget.setCursorPosition(cursor_position)
        if value in ['', '.', '-', '-.']:
            return True
        try:
            float(value)
            reply = False
        except:
            widget.setCursorPosition(cursor_position - 1)
            reply = True
        if not reply and name == 'gsEntry':
            # grid size is in inches
            W.conv_preview.grid_size = float(value) / P.unitsPerMm / 25.4
            W.conv_preview.set_current_view()
        return reply
    else:
        return True

def conv_is_float(entry):
    try:
        return True, float(entry)
    except:
        reply = -1 if entry else 0
        return False, reply

def conv_is_int(entry):
    try:
        return True, int(entry)
    except:
        reply = -1 if entry else 0
        return False, reply

def conv_undo_shape(P, W):
    # setup for a reload if required
    if not P.convPreviewActive:
        head = _translate('HandlerClass', 'Reload Request')
        btn1 = _translate('HandlerClass', 'CONTINUE')
        btn2 = _translate('HandlerClass', 'CANCEL')
        if P.fileOpened:
            name = os.path.basename(ACTION.prefilter_path)
            msg0 = _translate('HandlerClass', 'The original file will be loaded')
            msg1 = _translate('HandlerClass', 'If you continue all changes will be deleted')
            if not P.dialog_show_yesno(QMessageBox.Warning, f'{head}', f'{msg0}:\n\n{name}\n\n{msg1}\n', f'{btn1}', f'{btn2}'):
                return(True)
        else:
            msg0 = _translate('HandlerClass', 'An empty file will be loaded')
            msg1 = _translate('HandlerClass', 'If you continue all changes will be deleted')
            if not P.dialog_show_yesno(QMessageBox.Warning, f'{head}', f'{msg0}\n\n{msg1}\n', f'{btn1}', f'{btn2}'):
                return(True)
        P.validShape = False
        W.preview.setEnabled(True)
        W.undo.setEnabled(False)
        W.conv_save.setEnabled(False)
        W.conv_send.setEnabled(False)
    # undo the shape
    if os.path.exists(P.fNgcBkp) and W.undo.text() == 'UNDO':
        COPY(P.fNgcBkp, P.fNgc)
    elif os.path.exists(P.filteredBkp) and W.undo.text() == 'RELOAD':
        COPY(P.filteredBkp, P.fNgc)
        COPY(P.filteredBkp, P.fNgcBkp)
    else:
        return
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(False)
    if not P.validShape:
        W.undo.setEnabled(False)
    conv_preview_button(P, W, False)
    conv_enable_tabs(P, W)
    if P.isConvBlock == 'created':
        P.isConvBlock = None
        conv_shape_buttons(W, True)

def conv_add_shape_to_file(P, W):
    COPY(P.fNgc, P.fNgcBkp)
    try:
        if W.xsEntry.text():
            P.xSaved = W.xsEntry.text()
    except:
        pass
    try:
        if W.ysEntry.text():
            P.ySaved = W.ysEntry.text()
    except:
        pass
    # try:
    #     P.oSaved = W.centLeft.isChecked()
    # except:
    #     pass
    P.validShape = True
    W.add.setEnabled(False)
    W.conv_save.setEnabled(True)
    W.conv_send.setEnabled(True)
    conv_preview_button(P, W, False)
    conv_enable_tabs(P, W)

def conv_accept(P, W):
    P.validShape = True
    conv_preview_button(P, W, False)
    COPY(P.fNgc, P.fNgcBkp)
    W.conv_preview.load(P.fNgc)
    W.add.setEnabled(False)
    W.conv_save.setEnabled(True)
    W.conv_send.setEnabled(True)
    conv_enable_tabs(P, W)

def conv_clear_widgets(P, W, settings=False):
    for i in reversed(range(W.entries.count())):
        widget = W.entries.itemAt(i).widget()
        try:
            widget.disconnect()
        except:
            pass
        name = widget.objectName()
        if settings:
            if name == 'liEntry':
                P.savedSettings['in'] = widget.text()
            if name == 'loEntry':
                P.savedSettings['out'] = widget.text()
            if name == 'centLeft':
                P.savedSettings['origin'] = widget.text()
            if name == 'intExt':
                P.savedSettings['intext'] = widget.text()
        else:
            if isinstance(widget, QLineEdit) and \
               name not in ['liEntry', 'loEntry', 'xsEntry', 'ysEntry']:
                widget.setText('')
            if name in ['aEntry', 'oxEntry', 'oyEntry', 'rtEntry', 'csEntry', 'rsEntry']:
                widget.setText('0.0')
            elif name in ['caEntry']:
                widget.setText('360')
            elif name in ['cnEntry', 'rnEntry']:
                widget.setText('1')
            elif name in ['scEntry']:
                widget.setText('1.0')
            elif name in ['ocEntry']:
                widget.setText(f'{4 * P.unitsPerMm}')
        W.entries.removeWidget(widget)
        widget.setParent(None)

def conv_auto_preview_button(P, W, button):
    if button == 'intext':
        text = 'INTERNAL' if W.intExt.text() == 'EXTERNAL' else 'EXTERNAL'
        W.intExt.setText(text)
        P.intExt = True if text == 'EXTERNAL' else False
        W.intExt.setChecked(False)
    elif button == 'center':
        text = 'CENTER' if W.centLeft.text() == 'BTM LEFT' else 'BTM LEFT'
        W.centLeft.setText(text)
        W.centLeft.setChecked(False)

def conv_widgets(P, W):
    ''' create all required widgets without showing them '''
    # common
    W.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
    W.preview.setFocusPolicy(Qt.ClickFocus)
    W.undo = QPushButton(_translate('Conversational', 'RELOAD'))
    W.undo.setFocusPolicy(Qt.ClickFocus)
    W.centLeft = QPushButton(_translate('Conversational', 'BTM LEFT'), objectName='centLeft')
    W.centLeft.setFocusPolicy(Qt.ClickFocus)
    W.centLeft.setCheckable(True)
    W.intExt = QPushButton(_translate('Conversational', 'EXTERNAL'), objectName='intExt')
    W.intExt.setFocusPolicy(Qt.ClickFocus)
    W.intExt.setCheckable(True)
    W.liLabel = QLabel(_translate('Conversational', 'LEAD IN'))
    W.liEntry = QLineEdit(objectName='liEntry')
    W.loLabel = QLabel(_translate('Conversational', 'LEAD OUT'))
    W.loEntry = QLineEdit(objectName='loEntry')
    W.ctLabel = QLabel(_translate('Conversational', 'CUT TYPE'))
    W.spLabel = QLabel(_translate('Conversational', 'START'))
    W.xsLabel = QLabel(_translate('Conversational', 'X ORIGIN'))
    W.xsEntry = QLineEdit('0.000', objectName='xsEntry')
    W.ysLabel = QLabel(_translate('Conversational', 'Y ORIGIN'))
    W.ysEntry = QLineEdit('0.000', objectName='ysEntry')
    W.overcut = QPushButton(_translate('Conversational', 'OVER CUT'))
    W.overcut.setFocusPolicy(Qt.ClickFocus)
    W.overcut.setEnabled(False)
    W.overcut.setCheckable(True)
    W.ocLabel = QLabel(_translate('Conversational', 'OC LENGTH'))
    W.ocEntry = QLineEdit(objectName='ocEntry')
    W.ocEntry.setEnabled(False)
    W.ocEntry.setText(f'{4 * P.unitsPerMm}')
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.add.setFocusPolicy(Qt.ClickFocus)
    W.lDesc = QLabel('')
    W.iLabel = QLabel()
    W.iLabel.setAlignment(Qt.AlignRight | Qt.AlignTop)
    W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
    W.aEntry = QLineEdit('0.0', objectName='aEntry')
    W.dLabel = QLabel(_translate('Conversational', 'DIAMETER'))
    W.dEntry = QLineEdit(objectName='')
    W.hdLabel = QLabel(_translate('Conversational', 'HOLE DIA'))
    W.hdEntry = QLineEdit()
    W.hLabel = QLabel('')  # shared with different uses
    W.hEntry = QLineEdit()  # shared with different uses
    W.caLabel = QLabel(_translate('Conversational', 'CIRCLE ANG'))
    W.caEntry = QLineEdit('360', objectName='caEntry')
    W.wLabel = QLabel(_translate('Conversational', 'WIDTH'))
    W.wEntry = QLineEdit(objectName='')
    W.rLabel = QLabel(_translate('Conversational', 'RADIUS'))
    W.rButton = QPushButton(_translate('Conversational', 'RADIUS'))
    W.rButton.setFocusPolicy(Qt.ClickFocus)
    W.rEntry = QLineEdit('')
    W.sLabel = QLabel()  # shared with different uses
    W.sEntry = QLineEdit()  # shared with different uses
    W.mCombo = QComboBox()
    W.mCombo.setFocusPolicy(Qt.ClickFocus)
    W.mCombo.addItem(_translate('Conversational', 'CIRCUMSCRIBED'))
    W.mCombo.addItem(_translate('Conversational', 'INSCRIBED'))
    W.mCombo.addItem(_translate('Conversational', 'SIDE LENGTH'))
    W.r1Button = QPushButton(_translate('Conversational', 'RADIUS 1'))
    W.r1Button.setFocusPolicy(Qt.ClickFocus)
    W.r1Entry = QLineEdit()
    W.r2Button = QPushButton(_translate('Conversational', 'RADIUS 2'))
    W.r2Button.setFocusPolicy(Qt.ClickFocus)
    W.r2Entry = QLineEdit()
    W.r3Button = QPushButton(_translate('Conversational', 'RADIUS 3'))
    W.r3Button.setFocusPolicy(Qt.ClickFocus)
    W.r3Entry = QLineEdit()
    W.r4Button = QPushButton(_translate('Conversational', 'RADIUS 4'))
    W.r4Button.setFocusPolicy(Qt.ClickFocus)
    W.r4Entry = QLineEdit()
    W.lLabel = QLabel(_translate('Conversational', 'LENGTH'))
    W.lEntry = QLineEdit()
    W.pLabel = QLabel(_translate('Conversational', 'POINTS'))
    W.pEntry = QLineEdit(objectName='intEntry')
    W.odLabel = QLabel(_translate('Conversational', 'OUTER DIA'))
    W.odEntry = QLineEdit()
    W.idLabel = QLabel(_translate('Conversational', 'INNER DIA'))
    W.idEntry = QLineEdit()
    W.shLabel = QLabel(_translate('Conversational', 'SHAPE'))  # shared with different uses
    # triangle
    W.AaLabel = QLabel(_translate('Conversational', 'A ANGLE'))
    W.AaEntry = QLineEdit()
    W.BaLabel = QLabel(_translate('Conversational', 'B ANGLE'))
    W.BaEntry = QLineEdit()
    W.CaLabel = QLabel(_translate('Conversational', 'C ANGLE'))
    W.CaEntry = QLineEdit()
    W.AlLabel = QLabel(_translate('Conversational', 'a LENGTH'))
    W.AlEntry = QLineEdit()
    W.BlLabel = QLabel(_translate('Conversational', 'b LENGTH'))
    W.BlEntry = QLineEdit()
    W.ClLabel = QLabel(_translate('Conversational', 'c LENGTH'))
    W.ClEntry = QLineEdit()
    # block
    W.ccLabel = QLabel(_translate('Conversational', 'COLUMNS'))
    W.cnLabel = QLabel(_translate('Conversational', 'NUMBER'))
    W.cnEntry = QLineEdit('1', objectName='cnEntry')
    W.csEntry = QLineEdit('0.0', objectName='csEntry')
    W.csLabel = QLabel(_translate('Conversational', 'SPACING'))
    W.rrLabel = QLabel(_translate('Conversational', 'ROWS'))
    W.rnLabel = QLabel(_translate('Conversational', 'NUMBER'))
    W.rnEntry = QLineEdit('1', objectName='rnEntry')
    W.rsEntry = QLineEdit('0.0', objectName='rsEntry')
    W.rsLabel = QLabel(_translate('Conversational', 'SPACING'))
    W.oLabel = QLabel(_translate('Conversational', 'ORIGIN'))
    W.oxLabel = QLabel(_translate('Conversational', 'X OFFSET'))
    W.oxEntry = QLineEdit('0.0', objectName='xsEntry')
    W.oyEntry = QLineEdit('0.0', objectName='ysEntry')
    W.oyLabel = QLabel(_translate('Conversational', 'Y OFFSET'))
    W.ptLabel = QLabel(_translate('Conversational', 'PATTERN'))
    W.scLabel = QLabel(_translate('Conversational', 'SCALE'))
    W.scEntry = QLineEdit('1.0', objectName='scEntry')
    W.rtEntry = QLineEdit('0.0', objectName='aEntry')
    W.rtLabel = QLabel(_translate('Conversational', 'ROTATION'))
    W.mirror = QPushButton(_translate('Conversational', 'MIRROR'))
    W.mirror.setFocusPolicy(Qt.ClickFocus)
    W.flip = QPushButton(_translate('Conversational', 'FLIP'))
    W.flip.setFocusPolicy(Qt.ClickFocus)
    # lines and arcs
    W.lType = QComboBox()
    W.lType.setFocusPolicy(Qt.ClickFocus)
    W.label1 = QLabel()
    W.entry1 = QLineEdit(objectName='neg')
    W.label2 = QLabel()
    W.entry2 = QLineEdit(objectName='neg')
    W.label3 = QLabel()
    W.entry3 = QLineEdit()
    W.label4 = QLabel()
    W.entry4 = QLineEdit(objectName='neg')
    W.label5 = QLabel()
    W.entry5 = QLineEdit()
    W.label6 = QLabel()
    W.entry6 = QLineEdit(objectName='neg')
    W.g23Arc = QPushButton('CW - G2')
    W.g23Arc.setFocusPolicy(Qt.ClickFocus)
    W.g23Arc.setCheckable(True)
    # settings
    W.preLabel = QLabel(_translate('Conversational', 'PREAMBLE'))
    W.preEntry = QLineEdit()
    W.pstLabel = QLabel(_translate('Conversational', 'POSTAMBLE'))
    W.pstEntry = QLineEdit()
    W.llLabel = QLabel(_translate('Conversational', 'LEAD LENGTHS'))
    W.shEntry = QLineEdit()
    W.hsEntry = QLineEdit()
    W.hsLabel = QLabel(_translate('Conversational', 'SPEED %'))
    W.pvLabel = QLabel(_translate('Conversational', 'PREVIEW'))
    W.gsLabel = QLabel(_translate('Conversational', 'GRID SIZE'))
    W.gsEntry = QLineEdit(objectName='gsEntry')
    W.save = QPushButton(_translate('Conversational', 'SAVE'))
    W.save.setFocusPolicy(Qt.ClickFocus)
    W.reload = QPushButton(_translate('Conversational', 'RELOAD'))
    W.reload.setFocusPolicy(Qt.ClickFocus)
    W.cExit = QPushButton(_translate('Conversational', 'EXIT'))
    W.cExit.setFocusPolicy(Qt.ClickFocus)
    if not W.lType.count():
        W.lType.addItem(_translate('Conversational', 'LINE POINT ~ POINT'))
        W.lType.addItem(_translate('Conversational', 'LINE BY ANGLE'))
        W.lType.addItem(_translate('Conversational', 'ARC 3P'))
        W.lType.addItem(_translate('Conversational', 'ARC 2P +RADIUS'))
        W.lType.addItem(_translate('Conversational', 'ARC ANGLE +RADIUS'))
    if not W.mCombo.count():
        W.mCombo.addItem(_translate('Conversational', 'CIRCUMSCRIBED'))
        W.mCombo.addItem(_translate('Conversational', 'INSCRIBED'))
        W.mCombo.addItem(_translate('Conversational', 'SIDE LENGTH'))
    P.convWidgetsLoaded = True
