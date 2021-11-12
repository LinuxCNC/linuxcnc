'''
set_offsets.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

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
from PyQt5.QtCore import QCoreApplication
from PyQt5.QtWidgets import QMessageBox, QPushButton
from PyQt5.QtGui import QIcon

_translate = QCoreApplication.translate

def dialog_show(P, W, iniPath, STATUS, ACTION, TOOL, foreColor, backColor):
    msg = QMessageBox()
    msg.setWindowIcon(QIcon(os.path.join(P.IMAGES, 'linuxcncicon.png')))
    msg.setWindowTitle(_translate('Offsets', 'SET PERIPHERAL OFFSET'))
    msg0 = _translate('Offsets', 'Usage is as follows')
    msg1 = _translate('Offsets', 'Touchoff the torch to X0 Y0')
    msg2 = _translate('Offsets', 'Mark the material with a torch pulse')
    msg3 = _translate('Offsets', 'Jog until the peripheral is centered on the mark')
    msg4 = _translate('Offsets', 'Click the appropriate peripheral button')
    msg.setText('{}:\n\n{}.\n{}.\n{}.\n{}.\n'.format(msg0, msg1, msg2, msg3, msg4))
    buttonCancel = msg.addButton('CANCEL', QMessageBox.ActionRole)
    buttonLaser = msg.addButton('LASER', QMessageBox.ActionRole)
    buttonCamera = msg.addButton('CAMERA', QMessageBox.ActionRole)
    buttonScribe = msg.addButton('SCRIBE', QMessageBox.ActionRole)
    buttonProbe = msg.addButton('PROBE', QMessageBox.ActionRole)
    msg.setStyleSheet( '* {{ color: {0}; background: {1} }} \
                        QPushButton {{ background: {1}; height: 40px; font: 12pt; border: 1px solid {0}; border-radius: 4px }} \
                        QPushButton:pressed {{ color: {1} ; background: {0} }}'.format(foreColor, backColor))
    choice = msg.exec_()
    if choice == 1:
        if get_reply(P, STATUS, P.laserOffsetX, P.laserOffsetY):
            P.laserOffsetX = round(STATUS.get_position()[1][0], 4) + 0
            P.laserOffsetY = round(STATUS.get_position()[1][1], 4) + 0
            do_ini_file(P, W, iniPath, P.laserOffsetX, P.laserOffsetY, None, 'LASER_TOUCHOFF', '# laser touchoff')
            if P.laserOffsetX or P.laserOffsetY:
                W.laser.show()
            else:
                W.laser.hide()
    elif choice == 2:
        if get_reply(P, STATUS, P.camOffsetX, P.camOffsetY):
            P.camOffsetX = round(STATUS.get_position()[1][0], 4) + 0
            P.camOffsetY = round(STATUS.get_position()[1][1], 4) + 0
            do_ini_file(P, W, iniPath, P.camOffsetX, P.camOffsetY, None, 'CAMERA_TOUCHOFF', '# camera touchoff')
            if P.camOffsetX or P.camOffsetY:
                W.camera.show()
            else:
                W.camera.hide()
    elif choice == 3:
        xOffset = yOffset = 0.000
        toolFile = os.path.join(os.path.dirname(iniPath), TOOL.toolfile)
        inFile = open('{}'.format(toolFile), 'r')
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
    elif choice == 4:
        new = 0.0
        btn1 = _translate('HandlerClass', 'OK')
        btn2 = _translate('HandlerClass', 'CANCEL')
        valid, value = (P.dialog_input('Offset Probe Delay', 'Delay (Seconds)', btn1, btn2, P.probeDelay))
        if valid:
            try:
                new = float(value)
            except:
                head = _translate('HandlerClass', 'Entry Error')
                msg0 = _translate('HandlerClass', 'is not a valid number')
                P.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\'{}\' {}\n'.format(value, msg0))
                return
        else:
            return
        if get_reply(P, STATUS, P.probeOffsetX, P.probeOffsetY, True, P.probeDelay, new):
            print(round(STATUS.get_position()[1][0], 4) + 0)
            print(round(STATUS.get_position()[1][1], 4) + 0)
            print(new)
            P.probeOffsetX = round(STATUS.get_position()[1][0], 4) + 0
            P.probeOffsetY = round(STATUS.get_position()[1][1], 4) + 0
            P.probeDelay = new
            do_ini_file(P, W, iniPath, P.probeOffsetX, P.probeOffsetY, P.probeDelay, 'OFFSET_PROBING', '# offset probing')
            P.set_probe_offset_pins()

def get_reply(P, STATUS, xOffset, yOffset, probe=False, delay=0.0, new=0.0):
    head = _translate('HandlerClass', 'Offset Change')
    btn1 = _translate('HandlerClass', 'CONTINUE')
    btn2 = _translate('HandlerClass', 'CANCEL')
    msg0  = _translate('HandlerClass', 'Change offsets from')
    if probe:
        msg0 += ':\nX:{:0.3f}   Y:{:0.3f}   Delay:{:0.2f}\n\n'.format(xOffset, yOffset, delay)
    else:
        msg0 += ':\nX:{:0.3f}   Y:{:0.3f}\n\n'.format(xOffset, yOffset)
    msg0 += _translate('HandlerClass', 'To')
    if probe:
        msg0 += ':\nX:{:0.3f}   Y:{:0.3f}   Delay:{:0.2f}\n' \
                .format(round(STATUS.get_position()[1][0], 4) + 0,
                        round(STATUS.get_position()[1][1], 4) + 0,
                        new)
    else:
        msg0 += ':\nX:{:0.3f}   Y:{:0.3f}\n'.format(round(STATUS.get_position()[1][0], 4) + 0,
                                                    round(STATUS.get_position()[1][1], 4) + 0)
    if P.dialog_show_yesno(QMessageBox.Warning, 
                              '{}'.format(head), 
                              '\n{}'.format(msg0), 
                              '{}'.format(btn1), 
                              '{}'.format(btn2)):
        return True
    else:
        return False

def do_ini_file(P, W, iniPath, xOffset, yOffset, pDelay, param, comment):
    written = False
    COPY(iniPath, '{}~'.format(iniPath))
    inFile = open('{}~'.format(iniPath), 'r')
    outFile = open(iniPath, 'w')
    while 1:
        line = inFile.readline()
        if not line:
            break
        elif line.startswith('[QTPLASMAC]'):
            outFile.write(line)
            break
        else:
            outFile.write(line)
    while 1:
        line = inFile.readline()
        if not line:
            if not written:
                outFile.write('{}\n'.format(comment))
                if xOffset or yOffset:
                    outFile.write('{} = X{:0.3f} Y{:0.3f} {}\n\n'.format(param, xOffset, yOffset, pDelay))
                else:
                    outFile.write('#{} = X0.000 Y0.000\n\n'.format(param))
            break
        elif line.startswith('['):
            if not written:
                outFile.write('{}\n'.format(comment))
                if xOffset or yOffset:
                    outFile.write('{} = X{:0.3f} Y{:0.3f} {}\n\n'.format(param, xOffset, yOffset, pDelay))
                else:
                    outFile.write('#{} = X0.000 Y0.000\n\n'.format(param))
            outFile.write(line)
            break
        elif line.startswith('{}'.format(param)) or line.startswith('#{}'.format(param)):
            if xOffset or yOffset:
                outFile.write('{} = X{:0.3f} Y{:0.3f} {}\n'.format(param, xOffset, yOffset, pDelay))
            else:
                outFile.write('#{} = X0.000 Y0.000\n'.format(param))
            written = True
            break
        else:
            outFile.write(line)
    while 1:
        line = inFile.readline()
        if not line:
            break
        else:
            outFile.write(line)
    inFile.close()
    outFile.close()
    os.remove('{}~'.format(iniPath))

def do_tool_file(P, W, toolFile, xOffset, yOffset):
    written = False
    COPY(toolFile, '{}~'.format(toolFile))
    inFile = open('{}~'.format(toolFile), 'r')
    outFile = open(toolFile, 'w')
    for line in inFile:
        if line.startswith('T1'):
            outFile.write('T1 P1 X{:0.3f} Y{:0.3f} ;scribe\n'.format(xOffset, yOffset))
            written = True
        else:
            outFile.write(line)
    if not written:
        outFile.write('T1 P1 X{:0.3f} Y{:0.3f} ;scribe\n'.format(xOffset, yOffset))
    inFile.close()
    outFile.close()
    os.remove('{}~'.format(toolFile))





