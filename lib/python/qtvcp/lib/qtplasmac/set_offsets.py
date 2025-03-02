'''
set_offsets.py

Copyright (C) 2020 - 2024 Phillip A Carter
Copyright (C) 2020 - 2024 Gregory D Carl

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
from shutil import copy as COPY
from PyQt5 import QtCore
from PyQt5.QtCore import QCoreApplication
from PyQt5.QtWidgets import QDialog, QMessageBox, QPushButton, QGridLayout, QLabel, QComboBox
from PyQt5.QtGui import QIcon

_translate = QCoreApplication.translate

def dialog_show(P, W, prefs, iniPath, STATUS, ACTION, TOOL):
    dlg = QDialog(W)
    dlg.setWindowIcon(QIcon(os.path.join(P.iconBase, P.iconPath)))
    dlg.setModal(False)
    grid = QGridLayout()
    lbl = QLabel()
    btn0 = QPushButton('CANCEL')
    btn1 = QPushButton()
    btn2 = QPushButton()
    btn3 = QPushButton()
    btn4 = QPushButton()
    dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
    grid.addWidget(lbl, 0, 0, 1, 5)
    grid.addWidget(btn0, 1, 0)
    grid.addWidget(btn1, 1, 1)
    grid.addWidget(btn2, 1, 2)
    grid.addWidget(btn3, 1, 3)
    grid.addWidget(btn4, 1, 4)
    dlg.setLayout(grid)
    btn0.clicked.connect(lambda w: dlg_cancel_clicked(P, W, prefs, dlg, lbl, btn1, btn2, btn3, btn4))
    btn1.clicked.connect(lambda w: dlg_laser_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4))
    btn2.clicked.connect(lambda w: dlg_camera_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4))
    btn3.clicked.connect(lambda w: dlg_scribe_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4))
    btn4.clicked.connect(lambda w: dlg_probe_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4))
    dlg.setGeometry(dlg.parent().geometry().x(), dlg.parent().geometry().y(), dlg.width(), dlg.height())
    dlg.show()
    dlg.setStyleSheet(f'* {{ color: {P.foreColor}; background: {P.backColor}; margin: 4px }} \
                        QPushButton {{ background: {P.backColor}; height: 40px; font: 12pt; border: 1px solid {P.foreColor}; border-radius: 4px }} \
                        QPushButton:disabled {{color: {P.disabledColor}; border: 1px solid {P.disabledColor} }} \
                        QPushButton:pressed {{ color: {P.backColor} ; background: {P.foreColor} }}')

def dlg_cancel_clicked(P, W, prefs, dlg, lbl, btn1, btn2, btn3, btn4):
    P.laserOnPin.set(0)
    P.offsetSetProbePin.set(0)
    P.offsetSetScribePin.set(0)
    W.preview_stack.setCurrentIndex(0)
    P.camNum = prefs.getpref('Camera port', 0, int, 'CAMERA_OFFSET')
    W.camview.set_camnum(P.camNum)
    if 'OFFSETS' in btn1.text() or 'OFFSETS' in btn2.text() or 'OFFSETS' in btn3.text() or 'OFFSETS' in btn4.text():
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
    else:
        dlg.reject()

def dlg_laser_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4):
    if 'LASER' in btn1.text():
        dlg_set_text(False, dlg, lbl, btn1, btn2, btn3, btn4)
        P.laserOnPin.set(1)
        return
    if get_reply(P, STATUS, P.laserOffsetX, P.laserOffsetY):
        P.laserOffsetX = round(STATUS.get_position()[1][0], 4) + 0
        P.laserOffsetY = round(STATUS.get_position()[1][1], 4) + 0
        prefs.putpref('X axis', P.laserOffsetX, float, 'LASER_OFFSET')
        prefs.putpref('Y axis', P.laserOffsetY, float, 'LASER_OFFSET')
        if P.laserOffsetX or P.laserOffsetY:
            W.laser.show()
        else:
            W.laser.hide()
        P.laserOnPin.set(0)
        head = _translate('HandlerClass', 'Laser Offsets')
        msg0 = _translate('HandlerClass', 'Laser offsets have been saved')
        P.dialog_show_ok(QMessageBox.Information, f'{head}', f'\n{msg0}\n')
        dlg.close()
    else:
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
        P.laserOnPin.set(0)

def dlg_camera_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4):
    if 'CAMERA' in btn2.text():
        dlg.hide()
        W.setCursor(QtCore.Qt.WaitCursor)
        cameras = camera_search(P, W, dlg)
        if not cameras:
            W.unsetCursor()
            dlg.show()
            return
        W.preview_stack.setCurrentIndex(3)
        if len(cameras) > 1:
            camera_select(P, W, STATUS, cameras)
        else:
            P.camNum = cameras[0]
            W.camview.set_camnum(cameras[0])
            W.unsetCursor()
        dlg.show()
        if P.camNum == -1: # cancelled from camera selection
            W.preview_stack.setCurrentIndex(0)
            P.camNum = prefs.getpref('Camera port', 0, int, 'CAMERA_OFFSET')
            W.camview.set_camnum(P.camNum)
            return
        dlg_set_text(False, dlg, lbl, btn2, btn1, btn3, btn4)
        return
    if get_reply(P, STATUS, P.camOffsetX, P.camOffsetY):
        P.camOffsetX = round(STATUS.get_position()[1][0], 4) + 0
        P.camOffsetY = round(STATUS.get_position()[1][1], 4) + 0
        prefs.putpref('X axis', P.camOffsetX, float, 'CAMERA_OFFSET')
        prefs.putpref('Y axis', P.camOffsetY, float, 'CAMERA_OFFSET')
        prefs.putpref('Camera port', P.camNum, int, 'CAMERA_OFFSET')
        if P.camOffsetX or P.camOffsetY:
            W.camera.show()
        else:
            W.camera.hide()
        W.preview_stack.setCurrentIndex(0)
        head = _translate('HandlerClass', 'Camera Offsets')
        msg0 = _translate('HandlerClass', 'Camera offsets have been saved')
        P.dialog_show_ok(QMessageBox.Information, f'{head}', f'\n{msg0}\n')
        dlg.close()
    else:
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
        W.preview_stack.setCurrentIndex(0)
        P.camNum = prefs.getpref('Camera port', 0, int, 'CAMERA_OFFSET')
        W.camview.set_camnum(P.camNum)

def dlg_scribe_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4):
    if 'SCRIBE' in btn3.text():
        dlg_set_text(False, dlg, lbl, btn3, btn1, btn2, btn4)
        P.offsetSetScribePin.set(1)
        return
    xOffset = yOffset = 0.000
    toolFile = os.path.join(os.path.dirname(iniPath), TOOL.toolfile)
    inFile = open(f'{toolFile}', 'r')
    tool = []
    for line in inFile:
        if line.startswith('T1'):
            tool = line.split()
            inFile.close()
            break
    if tool:
        for item in tool:
            if item.startswith('X'):
                xOffset = float(item.replace('X','').replace('+',''))
            elif item.startswith('Y'):
                yOffset = float(item.replace('Y','').replace('+',''))
    if get_reply(P, STATUS, xOffset, yOffset):
        xOffset = round(STATUS.get_position()[1][0], 4) + 0
        yOffset = round(STATUS.get_position()[1][1], 4) + 0
        do_tool_file(P, W, toolFile, xOffset, yOffset)
        ACTION.RELOAD_TOOLTABLE()
        P.offsetSetScribePin.set(0)
        head = _translate('HandlerClass', 'Scribe Offsets')
        msg0 = _translate('HandlerClass', 'Scribe offsets have been saved')
        P.dialog_show_ok(QMessageBox.Information, f'{head}', f'\n{msg0}\n')
        dlg.close()
    else:
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
        P.offsetSetScribePin.set(0)

def dlg_probe_clicked(P, W, prefs, iniPath, STATUS, ACTION, TOOL, dlg, lbl, btn1, btn2, btn3, btn4):
    if 'PROBE' in btn4.text():
        dlg_set_text(False, dlg, lbl, btn4, btn1, btn2, btn3)
        P.offsetSetProbePin.set(1)
        return
    new = 0.0
    head = _translate('HandlerClass', 'Offset Probe Delay')
    text = _translate('HandlerClass', 'Delay (Seconds)')
    btnOk = _translate('HandlerClass', 'OK')
    btnCancel = _translate('HandlerClass', 'CANCEL')
    virtkb = 4
    valid, value = (P.dialog_input(virtkb, head, text, btnOk, btnCancel, P.probeDelay))
    if valid:
        try:
            new = float(value)
        except:
            head = _translate('HandlerClass', 'Entry Error')
            msg0 = _translate('HandlerClass', 'is not a valid number')
            P.dialog_show_ok(QMessageBox.Warning, f'{head}', f'\'{value}\' {msg0}\n')
            return
    else:
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
        P.offsetSetProbePin.set(0)
        return
    if get_reply(P, STATUS, P.probeOffsetX, P.probeOffsetY, True, P.probeDelay, new):
        P.probeOffsetX = round(STATUS.get_position()[1][0], 4) + 0
        P.probeOffsetY = round(STATUS.get_position()[1][1], 4) + 0
        P.probeDelay = new
        prefs.putpref('X axis', P.probeOffsetX, float, 'OFFSET_PROBING')
        prefs.putpref('Y axis', P.probeOffsetY, float, 'OFFSET_PROBING')
        prefs.putpref('Delay', P.probeDelay, float, 'OFFSET_PROBING')
        P.set_probe_offset_pins()
        P.offsetSetProbePin.set(0)
        if P.probeOffsetX or P.probeOffsetY:
            W.offset_feed_rate.show()
            W.offset_feed_rate_lbl.show()
        else:
            W.offset_feed_rate.hide()
            W.offset_feed_rate_lbl.hide()
        head = _translate('HandlerClass', 'Probe Offsets')
        msg0 = _translate('HandlerClass', 'Probe offsets have been saved')
        P.dialog_show_ok(QMessageBox.Information, f'{head}', f'\n{msg0}\n')
        dlg.close()
    else:
        dlg_set_text(True, dlg, lbl, btn1, btn2, btn3, btn4)
        P.offsetSetProbePin.set(0)

def dlg_set_text(main, dlg, lbl, btnA, btnB, btnC, btnD):
    if main:
        dlg.setWindowTitle(_translate('Offsets', 'Set Peripheral Offsets'))
        msg1 = _translate('Offsets', 'Touchoff the torch to X0 Y0')
        msg2 = _translate('Offsets', 'Mark the material with a torch pulse')
        msg3 = _translate('Offsets', 'Click the appropriate button to activate the peripheral')
        btnA.setText('LASER')
        btnB.setText('CAMERA')
        btnC.setText('SCRIBE')
        btnD.setText('PROBE')
        btnA.show()
        btnB.show()
        btnC.show()
        btnD.show()
    else:
        dlg.setWindowTitle(_translate('Offsets', 'Get Peripheral Offsets'))
        msg1 = _translate('Offsets', 'Jog until the peripheral is centered on the mark')
        msg2 = _translate('Offsets', 'Click the GET OFFSETS button to get the offsets')
        msg3 = _translate('Offsets', 'Confirm the setting of the offsets')
        btnA.setText('GET\nOFFSETS')
        btnB.hide()
        btnC.hide()
        btnD.hide()
    msg0 = _translate('Offsets', 'Usage is as follows')
    msg4 = _translate('Offsets', 'Note: It may be necessary to click the preview window to enable jogging')
    lbl.setText(f'{msg0}:\n\n1. {msg1}.\n2. {msg2}.\n3. {msg3}.\n\n{msg4}.\n')

def get_reply(P, STATUS, xOffset, yOffset, probe=False, delay=0.0, new=0.0):
    head = _translate('HandlerClass', 'Offset Change')
    btn1 = _translate('HandlerClass', 'SET OFFSETS')
    btn2 = _translate('HandlerClass', 'CANCEL')
    msg0  = _translate('HandlerClass', 'Change offsets from')
    if probe:
        msg0 += f':\nX:{xOffset:0.4f}   Y:{yOffset:0.4f}   Delay:{delay:0.2f}\n\n'
    else:
        msg0 += f':\nX:{xOffset:0.4f}   Y:{yOffset:0.4f}\n\n'
    msg0 += _translate('HandlerClass', 'To')
    xP = round(STATUS.get_position()[1][0], 4) + 0
    yP = round(STATUS.get_position()[1][1], 4) + 0
    if probe:
        msg0 += f':\nX:{xP:0.4f}   Y:{yP:0.4f}   Delay:{new:0.2f}\n'
    else:
        msg0 += f':\nX:{xP:0.4f}   Y:{yP:0.4f}\n'
    if P.dialog_show_yesno(QMessageBox.Warning, f'{head}', f'\n{msg0}', f'{btn1}', f'{btn2}'):
        return True
    else:
        return False

def camera_search(P, W, dlg):
    head = _translate('Offsets', 'Camera Search')
    cameras = []
    try:
        import cv2
    except:
        msg0 = _translate('Offsets', 'Could not load the cv2 module')
        msg1 = _translate('Offsets', 'Try installing by entering the following in a terminal')
        msg2 = _translate('Offsets', 'sudo apt install python3-opencv')
        W.unsetCursor()
        P.dialog_show_ok(QMessageBox.Critical, f'{head}', f'\n{msg0}.\n\n{msg1}:\n\n{msg2}\n')
        return
    for file in os.listdir('/dev'):
        if file.startswith('video'):
            cap = cv2.VideoCapture(int(file.replace('video', '')))
            if cap is None or not cap.isOpened():
                pass
            else:
                cameras.append(int(file.replace('video', '')))
                cap.release()
    if not cameras:
        msg0 = _translate('HandlerClass', 'No cameras have been found')
        P.dialog_show_ok(QMessageBox.Warning, f'{head}', f'\n{msg0}\n')
    return cameras

def camera_select(P, W, STATUS, cameras):
    dlg = QDialog(W)
    dlg.setWindowIcon(QIcon(os.path.join(P.iconBase, P.iconPath)))
    dlg.setWindowTitle(_translate('Offsets', 'Select Camera'))
    dlg.setModal(True)
    grid = QGridLayout()
    label = QLabel()
    combo = QComboBox()
    for camera in cameras:
        combo.addItem(f'Camera_{camera}')
    cam = P.camNum if P.camNum in cameras else cameras[0]
    combo.setCurrentIndex(cameras.index(cam))
    btn0 = QPushButton('CANCEL', dlg)
    btn1 = QPushButton('OK', dlg)
    grid.addWidget(label, 0, 0, 1, 2)
    grid.addWidget(combo, 1, 0, 1, 2)
    grid.addWidget(btn0, 2, 0)
    grid.addWidget(btn1, 2, 1)
    dlg.setLayout(grid)
    msg0 = _translate('Offsets', 'Select the camera to use')
    label.setText(f'{msg0}.\n')
    combo.currentIndexChanged.connect(lambda w: camera_changed(P, W, STATUS, dlg, int(combo.currentText()[-1:])))
    btn0.clicked.connect(lambda w: camera_btn_clicked(P, W, dlg, -1))
    btn1.clicked.connect(lambda w: camera_btn_clicked(P, W, dlg, int(combo.currentText()[-1:])))
    dlg.setStyleSheet(f'* {{ color: {P.foreColor}; background: {P.backColor}; margin: 4px }} \
                        QPushButton {{ background: {P.backColor}; height: 40px; font: 12pt; border: 1px solid {P.foreColor}; border-radius: 4px }} \
                        QPushButton:disabled {{color: {P.disabledColor}; border: 1px solid {P.disabledColor} }} \
                        QPushButton:pressed {{ color: {P.backColor} ; background: {P.foreColor} }}')
    dlg.setGeometry(dlg.parent().geometry().x(), dlg.parent().geometry().y(), dlg.width(), dlg.height())
    W.camview.set_camnum(int(combo.currentText()[-1:]))
    W.unsetCursor()
    dlg.exec()

def camera_changed(P, W, STATUS, dlg, camnum):
    dlg.setCursor(QtCore.Qt.WaitCursor)
    W.setCursor(QtCore.Qt.WaitCursor)
    W.camview.hideEvent(None)
    W.camview.set_camnum(camnum)
    W.camview.showEvent(None)
    dlg.unsetCursor()
    W.unsetCursor()

def camera_btn_clicked(P, W, dlg, camnum):
    P.camNum = int(camnum)
    dlg.reject()

def do_tool_file(P, W, toolFile, xOffset, yOffset):
    written = False
    COPY(toolFile, f'{toolFile}~')
    inFile = open(f'{toolFile}~', 'r')
    outFile = open(toolFile, 'w')
    for line in inFile:
        if line.startswith('T1'):
            outFile.write(f'T1 P1 X{xOffset:0.4f} Y{yOffset:0.4f} ;scribe\n')
            written = True
        else:
            outFile.write(line)
    if not written:
        outFile.write(f'T1 P1 X{xOffset:0.4f} Y{yOffset:0.4f} ;scribe\n')
    inFile.close()
    outFile.close()
    os.remove(f'{toolFile}~')
