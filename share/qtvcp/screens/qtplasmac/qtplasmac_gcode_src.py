'''
plasmac_gcode.py

Copyright (C) 2019, 2020, 2021  Phillip A Carter
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
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import sys
import linuxcnc
import math
import shutil
import time
from subprocess import run as RUN
from PyQt5 import QtCore, QtGui
from PyQt5.QtCore import Qt, QRect
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import QApplication, QDialog, QScrollArea, QWidget, QVBoxLayout, QLabel, QPushButton, QStyle, QFrame

app = QApplication(sys.argv)
ini = linuxcnc.ini(os.environ['INI_FILE_NAME'])
cmd = linuxcnc.command()
inFile = sys.argv[1]
filteredBkp = '/tmp/qtplasmac/filtered_bkp.ngc'
errorFile = '/tmp/qtplasmac/gcode_errors.txt'
materialFile = '{}_material.cfg'.format(ini.find('EMC', 'MACHINE'))
tmpMaterialFile = '/tmp/qtplasmac/{}_material.gcode'.format(ini.find('EMC', 'MACHINE'))
tmpMatNum = 1000000
tmpMatNam = ''
prefsFile = 'qtplasmac.prefs'
response = RUN(['halcmd', 'getp', 'qtplasmac.cut_type'], capture_output = True)
cutType = int(response.stdout.decode())
response = RUN(['halcmd', 'getp', 'qtplasmac.material_change_number'], capture_output = True)
currentMat = int(response.stdout.decode())
response = RUN(['halcmd', 'getp', 'qtplasmac.color_fg'], capture_output = True)
fgColor = str(hex(int(response.stdout.decode()))).replace('0x', '#')
response = RUN(['halcmd', 'getp', 'qtplasmac.color_bg'], capture_output = True)
bgColor = str(hex(int(response.stdout.decode()))).replace('0x', '#')
response = RUN(['halcmd', 'getp', 'qtplasmac.color_bgalt'], capture_output = True)
bgAltColor = str(hex(int(response.stdout.decode()))).replace('0x', '#')
response = RUN(['halcmd', 'getp', 'plasmac.max-offset'], capture_output = True)
zMaxOffset = float(response.stdout.decode())
metric = ['mm', 4]
imperial = ['in', 6]
units, precision = imperial if ini.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else metric
if units == 'mm':
    minDiameter = 32
    ocLength = 4
    unitsPerMm = 1
else:
    minDiameter = 1.26
    ocLength = 0.157
    unitsPerMm = 0.03937
unitMultiplier = 1
gcodeList = []
newMaterial = []
firstMaterial = ''
line = ''
rapidLine = ''
lastX = 0
lastY = 0
oBurnX = 0
oBurnY = 0
lineNum = 0
lineNumOrg = 0
distMode = 90 # absolute
arcDistMode = 91.1 # incremental
holeVelocity = 60
material = [0, False]
overCut = False
holeActive = False
holeEnable = False
arcEnable = False
customDia = False
customLen = False
torchEnable = True
pierceOnly = False
scribing = False
spotting = False
offsetG4x = False
zSetup = False
zBypass = False
convBlock = False
filtered = False
firstMove = False
notice  = 'If the g-code editor is used to resolve the following issues, the lines with errors\n'
notice += 'will be highlighted. The line numbers may differ from what is shown below.\n\n'
codeError = False
errors  = 'The following errors will affect the process.\n'
errors += 'Errors must be fixed before reloading this file.\n'
errorMath = []
errorMissMat = []
errorTempMat = []
errorNewMat = []
errorEditMat = []
errorWriteMat = []
errorReadMat = []
errorCompMat = []
errorFirstMove = []
errorLines = []
codeWarn = False
warnings  = 'The following warnings may affect the quality of the process.\n'
warnings += 'It is recommended that all warnings are fixed before running this file.\n'
warnUnitsDep = []
warnPierceScribe = []
warnMatLoad = []
warnHoleDir = []
warnCompTorch = []
warnCompVel = []
warnFeed = []
warnChar = []

# feedback dialog
def dialog_box(title, icon, errorText, warningText):
    dlg = QDialog()
    scroll = QScrollArea(dlg)
    widget = QWidget()
    vbox = QVBoxLayout()
    labelN = QLabel(notice, objectName = 'labelN')
    lineE = QFrame(objectName = 'lineE')
    lineE.setFrameShape(QFrame.HLine)
    labelE1 = QLabel(objectName = 'labelE1')
    labelE2 = QLabel()
    lineW = QFrame(objectName = 'lineW')
    lineW.setFrameShape(QFrame.HLine)
    labelW1 = QLabel(objectName = 'labelW1')
    labelW2 = QLabel()
    vbox.addWidget(labelN)
    vbox.addWidget(lineE)
    vbox.addWidget(labelE1)
    vbox.addWidget(labelE2)
    vbox.addWidget(lineW)
    vbox.addWidget(labelW1)
    vbox.addWidget(labelW2)
    widget.setLayout(vbox)
    btn = QPushButton('OK', dlg)
    dlg.setWindowTitle(title)
    dlg.setWindowIcon(QIcon(dlg.style().standardIcon(icon)))
    dlg.setWindowFlags(Qt.WindowStaysOnTopHint)
    dlg.setModal(False)
    dlg.setFixedWidth(600)
    dlg.setFixedHeight(310)
    scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
    scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
    scroll.setWidgetResizable(True)
    scroll.setWidget(widget)
    scroll.setGeometry(5, 5, 590, 250)
    btn.move(270,260)
    btn.clicked.connect(lambda w: dlg_ok_clicked(dlg))
    if errorText:
        labelE1.setText(errors)
        labelE2.setText(errorText)
    else:
        lineE.hide()
        labelE1.hide()
        labelE2.hide()
    if warningText:
        labelW1.setText(warnings)
        labelW2.setText(warningText)
    else:
        lineW.hide()
        labelW1.hide()
        labelW2.hide()
    dlg.setStyleSheet(' \
                      * {{ color: {0}; background: {1}}} \
                      QScrollArea {{color:{0}; background:{1}; border:1px solid {0}; border-radius:4px; padding:4px}} \
                      QPushButton {{border:2px solid {0}; border-radius:4px; font:12pt; width:60px; height:40px}} \
                      QPushButton:pressed {{border:1px solid {0}}} \
                      QScrollBar:vertical {{background:{2}; border:0px; border-radius:4px; margin: 0px; width:20px}} \
                      QScrollBar::handle:vertical {{background:{0}; border:2px solid {0}; border-radius:4px; margin:2px; min-height:40px}} \
                      QScrollBar::add-line:vertical {{height:0px}} \
                      QScrollBar::sub-line:vertical {{height:0px}} \
                      QVboxLayout {{margin:100}} \
                      #labelN {{font-style:italic}} \
                      #lineE, #lineW {{border:1px solid {0}}} \
                      #labelE1, #labelW1 {{font-weight:bold}}'.format(fgColor, bgColor, bgAltColor))
    dlg.exec()

def dlg_ok_clicked(dlg):
    dlg.accept()

# set hole type
def set_hole_type(h):
    global holeEnable, overCut, arcEnable
    if h == 1:
        holeEnable = True
        arcEnable = False
        overCut = False
    elif h == 2:
        holeEnable = True
        arcEnable = False
        overCut = True
    elif h == 3:
        holeEnable = True
        arcEnable = True
        overCut = False
    elif h == 4:
        holeEnable = True
        arcEnable = True
        overCut = True
    else:
        holeEnable = False
        arcEnable = False
        overCut = False

# check if arc is a hole
def check_if_hole():
    global lastX, lastY, minDiameter
    I, J, isHole = 0, 0, 0
    if distMode == 91: # get absolute X & Y from incremental coordinates
        endX = lastX + get_axis_value('x') if 'x' in line else lastX
        endY = lastY + get_axis_value('y') if 'y' in line else lastY
    else: # get absolute X & Y
        endX = get_axis_value('x') if 'x' in line else lastX
        endY = get_axis_value('y') if 'y' in line else lastY
    if arcDistMode == 90.1: # convert I & J to incremental to make diameter calculations easier
        if 'i' in line: I = get_axis_value('i') - lastX
        if 'j' in line: J = get_axis_value('j') - lastY
    else: # get incremental I & J
        if 'i' in line: I = get_axis_value('i')
        if 'j' in line: J = get_axis_value('j')
    if lastX and lastY and lastX == endX and lastY == endY:
        isHole = True
    diameter = get_hole_diameter(I, J, isHole)
    gcodeList.append(line)
    if isHole and overCut and diameter <= minDiameter:
        overburn(I, J, diameter / 2)
        return
    else:
        lastX = endX
        lastY = endY

# get hole diameter and set velocity percentage
def get_hole_diameter(I, J, isHole):
    global lineNum, lineNumOrg, errorLines, holeActive, codeWarn, warnCompVel, warnHoleDir
    if offsetG4x:
        diameter = math.sqrt((I ** 2) + (J ** 2)) * 2
    else:
        if material[0] in materialDict:
            kerfWidth = materialDict[material[0]][1] / 2 * unitMultiplier
        else:
            kerfWidth = 0
        diameter = (math.sqrt((I ** 2) + (J ** 2)) * 2) + kerfWidth
    # velocity reduction is required
    if diameter <= minDiameter and (isHole or arcEnable):
        if offsetG4x:
            lineNum += 1
            gcodeList.append(';m67 e3 q0 (inactive due to g41)')
            codeWarn = True
            warnCompVel.append(lineNum)
            errorLines.append(lineNumOrg)
        elif not holeActive:
            if diameter <= minDiameter:
                lineNum += 1
                gcodeList.append('m67 e3 q{0} (diameter:{1:0.3f}, velocity:{0}%)'.format(holeVelocity, diameter))
            holeActive = True
        if line.startswith('g2') and isHole:
            codeWarn = True
            warnHoleDir.append(lineNum)
            errorLines.append(lineNumOrg)
    # no velocity reduction required
    else:
        if holeActive:
            lineNum += 1
            gcodeList.append('m67 e3 q0 (arc complete, velocity 100%)')
            holeActive = False
    return diameter

# turn torch off and move 4mm (0.157) past hole end
def overburn(I, J, radius):
    global lineNum, lineNumOrg, errorLines, lastX, lastY, torchEnable, ocLength, warnCompTorch, arcDistMode, oBurnX, oBurnY
    centerX = lastX + I
    centerY = lastY + J
    cosA = math.cos(ocLength / radius)
    sinA = math.sin(ocLength / radius)
    cosB = ((lastX - centerX) / radius)
    sinB = ((lastY - centerY) / radius)
    lineNum += 1
    if offsetG4x:
        gcodeList.append(';m62 p3 (inactive due to g41)')
        codeWarn = True
        warnCompTorch.append(lineNum)
        errorLines.append(lineNumOrg)
    else:
        gcodeList.append('m62 p3 (disable torch)')
        torchEnable = False
    #clockwise arc
    if line.startswith('g2'):
        endX = centerX + radius * ((cosB * cosA) + (sinB * sinA))
        endY = centerY + radius * ((sinB * cosA) - (cosB * sinA))
        dir = '2'
    #counterclockwise arc
    else:
        endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
        endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
        dir = '3'
    lineNum += 1
    # restore I & J back to absolute from incremental conversion in check_if_hole
    if arcDistMode == 90.1:
        I += lastX
        J += lastY
    if distMode == 91: # output incremental X & Y
        gcodeList.append('g{0} x{1:0.{5}f} y{2:0.{5}f} i{3:0.{5}f} j{4:0.{5}f}'.format(dir, endX - lastX, endY - lastY, I, J, precision))
    else: # output absolute X & Y
        gcodeList.append('g{0} x{1:0.{5}f} y{2:0.{5}f} i{3:0.{5}f} j{4:0.{5}f}'.format(dir, endX, endY, I, J, precision))
    oBurnX = endX - lastX
    oBurnY = endY - lastY

# fix incremental coordinates after overburn
def fix_overburn_incremental_coordinates(line):
    newLine = line[:2]
    if 'x' in line and 'y' in line:
        x = get_axis_value('x')
        if x is not None:
            newLine += 'x{:0.4f}'.format(x - oBurnX)
        y = get_axis_value('y')
        if y is not None:
            newLine += 'y{:0.4f}'.format(y - oBurnY)
        return newLine
    elif 'x in line':
        x = get_axis_value('x')
        if x is not None:
            newLine += 'x{:0.4f}y{:0.4f}'.format(x - oBurnX, oBurnY)
        return newLine
    elif 'y' in line:
        y = get_axis_value('y')
        if y is not None:
            newLine += 'x{:0.4f}y{:0.4f}'.format(oBurnX, y - oBurnY)
        return newLine
    else:
        return line

# get axis value
def get_axis_value(axis):
    tmp1 = line.split(axis)[1].replace(' ','')
    if not tmp1[0].isdigit() and not tmp1[0] == '.' and not tmp1[0] == '-':
        return None
    n = 0
    tmp2 = ''
    while 1:
        if tmp1[n].isdigit() or tmp1[n] == '.' or tmp1[n] == '-':
            tmp2 += tmp1[n]
            n += 1
        else:
            break
        if n >= len(tmp1):
            break
    return float(tmp2)

# set the last X and Y coordinates
def set_last_coordinates(Xpos, Ypos):
    if line[0] in ['g','x','y']:
        if 'x' in line:
            if get_axis_value('x') is not None:
                if distMode == 91: # get absolute X from incremental position
                    Xpos += get_axis_value('x')
                else: # get absolute X
                    Xpos = get_axis_value('x')
        if 'y' in line:
            if get_axis_value('y') is not None:
                if distMode == 91: # get absolute X from incremental position
                    Ypos += get_axis_value('y')
                else: # get absolute X
                    Ypos = get_axis_value('y')
    return Xpos, Ypos

# comment out all Z commands
def comment_out_z_commands():
    global lineNum, holeActive
    newline = ''
    newz = ''
    removing = 0
    comment = 0
    maths = 0
    for bit in line:
        if comment:
            if bit == ')':
                comment = 0
            newline += bit
        elif removing:
            if bit == '[':
                newz += bit
                maths += 1
            elif bit == ']':
                newz += bit
                maths -= 1
            elif maths:
                newz += bit
            elif bit in '0123456789.- ':
                newz += bit
            else:
                removing = 0
                if newz:
                    newz = newz.rstrip()
                newline += bit
        elif bit == '(':
            comment = 1
            newline += bit
        elif bit == 'z':
            removing = 1
            newz += '(' + bit
        else:
            newline += bit
    if holeActive:
        lineNum += 1
        gcodeList.append('m67 e3 q0 (arc complete, velocity 100%)')
        holeActive = False
    return '{} {})'.format(newline, newz)

# check if math used or explicit values
def check_math(axis):
    global lineNum, lineNumOrg, errorLines, codeError, errorMath
    tmp1 = line.split(axis)[1]
    if tmp1.startswith('[') or tmp1.startswith('#'):
        codeError = True
        if lineNum not in errorMath:
            errorMath.append(lineNum)
            errorLines.append(lineNumOrg)
        return True
    return False
# do material change
def do_material_change():
    global lineNum, lineNumOrg, errorLines, firstMaterial, codeError, errorMissMat
    if '(' in line:
        c = line.split('(', 1)[0]
    elif ';' in line:
        c = line.split(';', 1)[0]
    else:
        c = line
    a, b = c.split('p', 1)
    m = ''
    # get the material number
    for mNum in b.strip():
        if mNum in '0123456789':
            m += mNum
    material[0] = int(m)
    material[1] = True
    if material[0] not in materialDict and material[0] < 1000000:
        codeError = True
        errorMissMat.append(lineNum)
        errorLines.append(lineNumOrg)
    RUN(['halcmd', 'setp', 'qtplasmac.material_change_number', '{}'.format(material[0])])
    if not firstMaterial:
        firstMaterial = material[0]
    gcodeList.append(line)

# check if material edit required
def check_material_edit():
    global lineNum, lineNumOrg, errorLines, tmpMatNum, tmpMatNam, codeError, errorNewMat, errorEditMat
    tmpMaterial = False
    newMaterial = []
    th = 0
    kw = jh = jd = ca = cv = pe = gp = cm = 0.0
    ca = 15
    cv = 100
    try:
        if 'ph=' in line and 'pd=' in line and 'ch=' in line and 'fr=' in line:
            if '(o=0' in line:
                tmpMaterial = True
                nu = tmpMatNum
                na = 'Temporary {}'.format(tmpMatNum)
                tmpMatNam = na
                newMaterial.append(0)
            elif '(o=1' in line and 'nu=' in line and 'na=' in line:
                newMaterial.append(1)
            elif '(o=2' in line and 'nu=' in line and 'na=' in line:
                newMaterial.append(2)
            if newMaterial[0] in [0, 1, 2]:
                for item in line.split('(')[1].split(')')[0].split(','):
                    # mandatory items
                    if 'nu=' in item and not tmpMaterial:
                        nu = int(item.split('=')[1])
                    elif 'na=' in item:
                        na = item.split('=')[1].strip()
                        if tmpMaterial:
                            tmpMatNam = na
                    elif 'ph=' in item:
                        ph = float(item.split('=')[1])
                        if unitMultiplier != 1:
                            ph = ph / unitMultiplier
                    elif 'pd=' in item:
                        pd = float(item.split('=')[1])
                    elif 'ch=' in item:
                        ch = float(item.split('=')[1])
                        if unitMultiplier != 1:
                            ch = ch / unitMultiplier
                    elif 'fr=' in item:
                        fr = float(item.split('=')[1])
                        if unitMultiplier != 1:
                            fr = fr / unitMultiplier
                    # optional items
                    elif 'kw=' in item:
                        kw = float(item.split('=')[1])
                        if unitMultiplier != 1:
                            kw = kw / unitMultiplier
                    elif 'th=' in item:
                        th = int(item.split('=')[1])
                    elif 'jh=' in item:
                        jh = float(item.split('=')[1])
                        if unitMultiplier != 1:
                            jh = ph / unitMultiplier
                    elif 'jd=' in item:
                        jd = float(item.split('=')[1])
                    elif 'ca=' in item:
                        ca = float(item.split('=')[1])
                    elif 'cv=' in item:
                        cv = float(item.split('=')[1])
                    elif 'pe=' in item:
                        pe = float(item.split('=')[1])
                    elif 'gp=' in item:
                        gp = float(item.split('=')[1])
                    elif 'cm=' in item:
                        cm = float(item.split('=')[1])
                for i in [nu,na,kw,th,ph,pd,jh,jd,ch,fr,ca,cv,pe,gp,cm]:
                    newMaterial.append(i)
                if newMaterial[0] == 0:
                    write_temporary_material(newMaterial)
                elif nu in materialDict and newMaterial[0] == 1:
                    codeError = True
                    errorNewMat.append(lineNum)
                    errorLines.append(lineNumOrg)
                else:
                    rewrite_material_file(newMaterial)
            else:
                codeError = True
                errorEditMat.append(lineNum)
                errorLines.append(lineNumOrg)
    except:
        codeError = True
        errorLines.append(lineNumOrg)

# write temporary materials file
def write_temporary_material(data):
    global lineNum, lineNumOrg, errorLines, errorTempMat, warnMatLoad, material, codeError
    try:
        with open(tmpMaterialFile, 'w') as fWrite:
            fWrite.write('#plasmac temporary material file\n')
            fWrite.write('\nnumber={}\n'.format(tmpMatNum))
            fWrite.write('name={}\n'.format(tmpMatNam))
            fWrite.write('kerf-width={}\n'.format(data[3]))
            fWrite.write('thc-enable={}\n'.format(data[4]))
            fWrite.write('pierce-height={}\n'.format(data[5]))
            fWrite.write('pierce-delay={}\n'.format(data[6]))
            fWrite.write('puddle-jump-height={}\n'.format(data[7]))
            fWrite.write('puddle-jump-delay={}\n'.format(data[8]))
            fWrite.write('cut-height={}\n'.format(data[9]))
            fWrite.write('cut-feed-rate={}\n'.format(data[10]))
            fWrite.write('cut-amps={}\n'.format(data[11]))
            fWrite.write('cut-volts={}\n'.format(data[12]))
            fWrite.write('pause-at-end={}\n'.format(data[13]))
            fWrite.write('gas-pressure={}\n'.format(data[14]))
            fWrite.write('cut-mode={}\n'.format(data[15]))
            fWrite.write('\n')
    except:
        codeError = True
        errorTempMat.append(lineNum)
        errorLines.append(lineNumOrg)
    materialDict[tmpMatNum] = [data[10], data[3]]
    RUN(['halcmd', 'setp', 'qtplasmac.material_temp', '{}'.format(tmpMatNum)])
    material[0] = tmpMatNum
    matDelay = time.time()
    while 1:
        if time.time() > matDelay + 3:
            codeWarn = True
            warnMatLoad.append(lineNum)
            errorLines.append(lineNumOrg)
            break
        response = RUN(['halcmd', 'getp', 'qtplasmac.material_temp'], capture_output = True)
        if not int(response.stdout.decode()):
            break

# rewrite the material file
def rewrite_material_file(newMaterial):
    global lineNum, warnMatLoad, errorLines
    copyFile = '{}.bkp'.format(materialFile)
    shutil.copy(materialFile, copyFile)
    inFile = open(copyFile, 'r')
    outFile = open(materialFile, 'w')
    while 1:
        line = inFile.readline()
        if not line:
            break
        if not line.strip().startswith('[MATERIAL_NUMBER_'):
            outFile.write(line)
        else:
            break
    while 1:
        if not line:
            add_edit_material(newMaterial, outFile)
            break
        if line.strip().startswith('[MATERIAL_NUMBER_'):
            mNum = int(line.split('NUMBER_')[1].replace(']',''))
            if mNum == newMaterial[1]:
                add_edit_material(newMaterial, outFile)
        if mNum != newMaterial[1]:
            outFile.write(line)
        line = inFile.readline()
        if not line:
            break
    if newMaterial[1] not in materialDict:
        add_edit_material(newMaterial, outFile)
    inFile.close()
    outFile.close()
    RUN(['halcmd', 'setp', 'qtplasmac.material_reload', 1])
    get_materials()
    matDelay = time.time()
    while 1:
        if time.time() > matDelay + 3:
            codeWarn = True
            warnMatLoad.append(lineNum)
            errorLines.append(lineNumOrg)
            break
        response = RUN(['halcmd', 'getp', 'qtplasmac.material_reload'], capture_output = True)
        if not int(response.stdout.decode()):
            break

# add a new material or or edit an existing material
def add_edit_material(material, outFile):
    global lineNum, lineNumOrg, errorLines, codeError, ErrorWriteMat
    try:
        outFile.write('[MATERIAL_NUMBER_{}]\n'.format(material[1]))
        outFile.write('NAME               = {}\n'.format(material[2]))
        outFile.write('KERF_WIDTH         = {}\n'.format(material[3]))
        outFile.write('THC                = {}\n'.format(material[4]))
        outFile.write('PIERCE_HEIGHT      = {}\n'.format(material[5]))
        outFile.write('PIERCE_DELAY       = {}\n'.format(material[6]))
        outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(material[7]))
        outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(material[8]))
        outFile.write('CUT_HEIGHT         = {}\n'.format(material[9]))
        outFile.write('CUT_SPEED          = {}\n'.format(material[10]))
        outFile.write('CUT_AMPS           = {}\n'.format(material[11]))
        outFile.write('CUT_VOLTS          = {}\n'.format(material[12]))
        outFile.write('PAUSE_AT_END       = {}\n'.format(material[13]))
        outFile.write('GAS_PRESSURE       = {}\n'.format(material[14]))
        outFile.write('CUT_MODE           = {}\n'.format(material[15]))
        outFile.write('\n')
    except:
        codeError = True
        errorWriteMat.append(lineNum)
        errorLines.append(lineNumOrg)

# create a dict of material numbers and kerf widths
def get_materials():
    global lineNum, lineNumOrg, errorLines, materialDict, codeError, errorReadMat
    try:
        with open(prefsFile, 'r') as rFile:
            fRate = kWidth = 0.0
            for line in rFile:
                if line.startswith('Cut feed rate'):
                    fRate = float(line.split('=')[1].strip())
                if line.startswith('Kerf width'):
                    kWidth = float(line.split('=')[1].strip())
        mNumber = 0
        with open(materialFile, 'r') as mFile:
            materialDict = {mNumber: [fRate, kWidth]}
            while 1:
                line = mFile.readline()
                if not line:
                    break
                elif line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                    mNumber = int(line.rsplit('_', 1)[1].strip().strip(']'))
                    break
            while 1:
                line = mFile.readline()
                if not line:
                    materialDict[mNumber] = [fRate, kWidth]
                    break
                elif line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                    materialDict[mNumber] = [fRate, kWidth]
                    mNumber = int(line.rsplit('_', 1)[1].strip().strip(']'))
                elif line.startswith('CUT_SPEED'):
                    fRate = float(line.split('=')[1].strip())
                elif line.startswith('KERF_WIDTH'):
                    kWidth = float(line.split('=')[1].strip())
    except:
        codeError = True
        errorReadMat.append(lineNum)
        errorLines.append(lineNumOrg)

def check_f_word(line):
    global lineNum, lineNumOrg, errorLines, materialDict, codeWarn, warnFeed
    begin, inFeed = line.split('f', 1)
    # if feed rate from material file
    if inFeed.startswith('#<_hal[plasmac.cut-feed-rate]>'):
        # change feed rate if gcode file not in same units as machine units
        if unitMultiplier != 1:
            line = begin + '{}f[#<_hal[plasmac.cut-feed-rate]> * {}]\n'.format(begin, unitMultiplier)
        return line
    # if explicit feed rate
    rawFeed = ''
    codeFeed = 0.0
    # get feed rate if it is digits
    while len(inFeed) and (inFeed[0].isdigit() or inFeed[0] == '.'):
        rawFeed = rawFeed + inFeed[0]
        inFeed = inFeed[1:].lstrip()
    if not rawFeed:
        return line
    codeFeed = float(rawFeed)
    matFeed = float(materialDict[material[0]][0]) * unitMultiplier

    # this may need scaling ...
    diff = 1

    if codeFeed < matFeed - diff or codeFeed > matFeed + diff:
        codeWarn = True
        warnFeed.append([lineNum, rawFeed, material[0], materialDict[material[0]][0]])
        errorLines.append(lineNumOrg)
    return line

# *** we need a lot more work done here ***
def illegal_character(line):
    line = line.replace(' ', '')
    # single character line with invalid character
    if len(line) == 1  and line not in '/;%': return 1
    # comment is missing a parenthesis
    elif ('(' in line and line[-1] != ')') or ((line[-1] == ')' and not '(' in line)): return 1
    # process starting alpha
    elif line[0].isalpha():
        # line starts with two alpha characters (this could be refined)
        if line[1].isalpha(): return 1
    # process starting non-alpha
    elif not line[0].isalpha():
        # invalid first character
        if line[0] not in '/;(#@^%': return 1
    # process numbered and named parameters
    if line[0] == '#' or line[:2] == '#<':
        line = line.lstrip('#')
        # remove trailing comment for further processing
        if '(' in line:
            line = line.split('(')[0].strip()
        # parameter is missing equals sign
        if not '=' in line: return 1
        # left = parameter, right = value (we don't process right side yet)
        left, right = line.split('=')
        # named parameter is missing a chevron
        if left[0] == '<' and not '>' in left: return 1
        # numbered parameter is not a number
        elif left[0] != '<' and not left.isdigit(): return 1
    return 0

def message_set(msgType, msg):
    if len(msgType) > 1:
        msg += 'Lines: '
    else:
        msg += 'Line: '
    count = 0
    for line in msgType:
        if codeError:
            line += 1
        if count > 0:
            msg += ', {}'.format(line)
        else:
            msg += '{}'.format(line)
        count += 1
    msg += '\n\n'
    return msg

# get a dict of materials
get_materials()

# start processing the gcode file
with open(inFile, 'r') as inCode:
    if ';qtplasmac filtered g-code file' in inCode.read():
        filtered = True
    inCode.seek(0)
    for line in inCode:
        lineNum += 1
        lineNumOrg += 1
        # if original is already filtered there is no need to process the line
        if filtered:
            if not ';qtplasmac filtered g-code file' in line:
                gcodeList.append(line.rstrip())
            continue
        # remove whitespace and trailing periods
        line = line.strip().rstrip('.')
        # remove line numbers
        if line.lower().startswith('n'):
            line = line[1:]
            while line[0].isdigit() or line[0] == '.':
                line = line[1:].lstrip()
                if not line:
                    break
        # if empty line then no need to process
        if not line:
            gcodeList.append(line)
            continue
        # if line is a comment then gcodeList.append it and get next line
        if line.startswith(';') or line.startswith('(') and not line.startswith('(o='):
            gcodeList.append(line)
            continue
        # if illegal characters comment the line
        if illegal_character(line):
            codeWarn = True
            warnChar.append(lineNum)
            errorLines.append(lineNumOrg)
            gcodeList.append(';{}'.format(line))
            continue
        # check if original is a conversational block
        if line.startswith(';conversational block'):
            convBlock = True
        # check for a material edit
        if line.startswith('(o='):
            check_material_edit()
            # add comment for temporary material
            if line.startswith('(o=0'):
                lineNum += 1
                gcodeList.append(';temporay material #{}'.format(tmpMatNum))
            gcodeList.append(line)
            # add material change for temporary material
            if line.startswith('(o=0'):
                lineNum += 1
                gcodeList.append('m190 p{}'.format(tmpMatNum))
                lineNum += 1
                gcodeList.append('m66 p3 l3 q1')
                tmpMatNum += 1
            continue
        # if a ; comment at end of line, convert line to lower case and remove spaces, preserve comment as is
        elif ';' in line:
            a,b = line.split(';', 1)
            line = '{} ({})'.format(a.strip().lower().replace(' ',''),b.replace(';','').replace('(','').replace(')',''))
        # if a () comment at end of line, convert line to lower case and remove spaces, preserve comment as is
        elif '(' in line:
            a,b = line.split('(', 1)
            line = '{} ({})'.format(a.strip().lower().replace(' ',''),b.replace('(','').replace(')',''))
        # if any other line, convert line to lower case and remove spaces
        else:
            line = line.lower().replace(' ','')
        # remove leading 0's from G & M codes
        if (line.lower().startswith('g') or \
           line.lower().startswith('m')) and \
           len(line) > 2:
            while line[1] == '0' and len(line) > 2:
                if line[2].isdigit():
                    line = line[:1] + line[2:]
                else:
                    break
        # if incremental distance mode fix overburn coordinates
        if line[:2] in ['g0', 'g1'] and distMode == 91 and (oBurnX or oBurnY):
            line = fix_overburn_incremental_coordinates(line)
        # if z motion is to be kept
        if line.startswith('#<keep-z-motion>'):
            if '(' in line:
                keepZ = int(line.split('=')[1].split('(')[0])
            else:
                keepZ = int(line.split('=')[1])
            if keepZ == 1:
                zBypass = True
            else:
                zBypass = False
            gcodeList.append(line)
            continue
        # remove any additional z max moves
        if '[#<_ini[axis_z]max_limit>' in line and zSetup:
            continue
        # set initial Z height
        if not zSetup and not zBypass and ('g0' in line or 'g1' in line or 'm3' in line):
            offsetTopZ = (zMaxOffset * unitsPerMm * unitMultiplier)
            moveTopZ = 'g53g0z[#<_ini[axis_z]max_limit>*{}-{:.3f}] (Z just below max height)'.format(unitMultiplier, offsetTopZ)
            if not '[#<_ini[axis_z]max_limit>' in line:
                lineNum += 1
                gcodeList.append(moveTopZ)
            else:
                line = moveTopZ
            zSetup = True
        # set default units
        if 'g21' in line:
            if units == 'in':
                unitMultiplier = 25.4
                if not customDia:
                    minDiameter = 32
                if not customLen:
                    ocLength = 4
        elif 'g20' in line:
            if units == 'mm':
                unitMultiplier = 0.03937
                if not customDia:
                    minDiameter = 1.26
                if not customLen:
                    ocLength = 0.157
        # check for g41 or g42 offsets
        if 'g41' in line or 'g42' in line:
            offsetG4x = True
            if 'kerf_width-f]>' in line and unitMultiplier != 1:
                line = line.replace('#<_hal[qtplasmac.kerf_width-f]>', \
                                   '[#<_hal[qtplasmac.kerf_width-f]> * {}]'.format(unitMultiplier))
        # check for g4x offset cleared
        elif 'g40' in line:
            offsetG4x = False
        # set first movement flag
        if line[:3] in ['g0x', 'g0y', 'g1x', 'g1y'] or line[:6] in ['g53g0x', 'g53g0y', 'g53g1x', 'g53g1y']:
            firstMove = True
        # is there an m3 before we've made a movement
        if 'm3' in line and not 'm30' in line and not firstMove:
            codeError = True
            errorFirstMove.append(lineNum)
            errorLines.append(lineNumOrg)
        # are we scribing
        if line.startswith('m3$1s'):
            if pierceOnly:
                codeWarn = True
                warnPierceScribe.append(lineNum)
                errorLines.append(lineNumOrg)
                scribing = False
            else:
                scribing = True
                gcodeList.append(line)
                continue
        # if pierce only mode
        if pierceOnly:
            # Don't pierce spotting operations
            if line.startswith('m3$2'):
                spotting = True
                gcodeList.append('(Ignoring spotting operation as pierce-only is active)')
                continue
            # Ignore spotting blocks when pierceOnly
            if spotting:
                if line.startswith('m5$2'):
                    firstMove = False
                    spotting = False
                continue
            if line.startswith('g0'):
                rapidLine = line
                continue
            if line.startswith('m3') and not line.startswith('m3$1'):
                pierces += 1
                gcodeList.append('\n(Pierce #{})'.format(pierces))
                gcodeList.append(rapidLine)
                gcodeList.append('M3 $0 S1')
                gcodeList.append('G91')
                gcodeList.append('G1 X.000001')
                gcodeList.append('G90\nM5 $0')
                rapidLine = ''
                continue
            if not pierces or line.startswith('o') or line.startswith('#'):
                gcodeList.append(line)
            continue
        # test for pierce only mode
        if (line.startswith('#<pierce-only>') and line.split('=')[1][0] == '1') or (not pierceOnly and cutType == 1):
            if scribing:
                codeWarn = True
                warnPierceScribe.append(lineNum)
                errorLines.append(lineNumOrg)
            else:
                pierceOnly = True
                pierces = 0
                rapidLine = ''
                gcodeList.append(line)
            if not cutType == 1:
                continue
        if line.startswith('#<oclength>'):
            if '(' in line:
                ocLength = float(line.split('=')[1].split('(')[0])
            else:
                ocLength = float(line.split('=')[1])
            customLen = True
            gcodeList.append(line)
            continue
        # if hole sensing code
        if line.startswith('#<holes>'):
            if '(' in line:
                set_hole_type(int(line.split('=')[1].split('(')[0]))
            else:
                set_hole_type(int(line.split('=')[1]))
            gcodeList.append(line)
            continue
        # if hole diameter command
        if line.startswith('#<h_diameter>') or line.startswith('#<m_diameter>') or line.startswith('#<i_diameter>'):
            if '(' in line:
                minDiameter = float(line.split('=')[1].split('(')[0])
                customDia = True
            else:
                minDiameter = float(line.split('=')[1])
                customDia = True
            gcodeList.append(line)
            if '#<m_d' in line or '#<i_d' in line:
                codeWarn = True
                warnUnitsDep.append(lineNum)
                errorLines.append(lineNumOrg)
            continue
        # if hole velocity command
        if line.startswith('#<h_velocity>'):
            if '(' in line:
                holeVelocity = float(line.split('=')[1].split('(')[0])
            else:
                holeVelocity = float(line.split('=')[1])
            gcodeList.append(line)
            continue
        # if material change
        if line.startswith('m190'):
            do_material_change()
            if not 'm66' in line:
                continue
        # wait for material change
        if 'm66' in line:
            if offsetG4x:
                codeError = True
                errorCompMat.append(lineNum)
                errorLines.append(lineNumOrg)
            gcodeList.append(line)
            continue
        # set arc modes
        if 'g90' in line and not 'g90.' in line:
            distMode = 90 # absolute distance mode
        if 'g91' in line and not 'g91.' in line:
            distMode = 91 # incremental distance mode
        if 'g90.1' in line:
            arcDistMode = 90.1 # absolute arc distance mode
        if 'g91.1' in line:
            arcDistMode = 91.1 # incremental arc distance mode
        if not zBypass:
            # if z axis in line
            if 'z' in line and line.split('z')[1][0] in '0123456789.- [' and not '[axis_z]max_limit' in line:
                # if no other axes comment it
                if 1 not in [c in line for c in 'xybcuvw']:
                    if '(' in line:
                        gcodeList.append('({} {}'.format(line.split('(')[0], line.split('(')[1]))
                    elif ';' in line:
                        gcodeList.append('({} {}'.format(line.split(';')[0], line.split(';')[1]))
                    else:
                        gcodeList.append('({})'.format(line))
                    continue
                # other axes in line, comment out the Z axis
                if not '(z' in line:
                    if holeEnable and ('x' in line or 'y' in line):
                        lastX, lastY = set_last_coordinates(lastX, lastY)
                    result = comment_out_z_commands()
                    gcodeList.append(result)
                    continue
        # if an arc command
        if (line.startswith('g2') or line.startswith('g3')) and line[2].isalpha():
            if holeEnable and not convBlock:
                stop = False
                # check if we can read the values correctly
                if 'x' in line: stop = check_math('x')
                if 'y' in line and not stop: stop = check_math('y')
                if 'i' in line and not stop: stop = check_math('i')
                if 'j' in line and not stop: stop = check_math('j')
                if not stop:
                    check_if_hole()
            else:
                gcodeList.append(line)
            continue
        # if torch off, flag it then gcodeList.append it
        if line.startswith('m62p3') or line.startswith('m64p3'):
            torchEnable = False
            gcodeList.append(line)
            continue
        # if torch on, flag it then gcodeList.append it
        if line.startswith('m63p3') or line.startswith('m65p3'):
            torchEnable = True
            gcodeList.append(line)
            continue
        # if spindle off
        if line.startswith('m5'):
            if len(line) == 2 or (len(line) > 2 and not line[2].isdigit()):
                firstMove = False
                gcodeList.append(line)
                # restore velocity if required
                if holeActive:
                    lineNum += 1
                    gcodeList.append('m68 e3 q0 (arc complete, velocity 100%)')
                    holeActive = False
                # if torch off, allow torch on
                if not torchEnable:
                    lineNum += 1
                    gcodeList.append('m65 p3 (enable torch)')
                    torchEnable = True
            else:
                gcodeList.append(line)
            continue
        # if program end
        if line.startswith('m2') or line.startswith('m30') or line.startswith('%'):
            # restore velocity if required
            if holeActive:
                lineNum += 1
                gcodeList.append('m68 e3 q0 (arc complete, velocity 100%)')
                holeActive = False
            # if torch off, allow torch on
            if not torchEnable:
                lineNum += 1
                gcodeList.append('m65 p3 (enable torch)')
                torchEnable = True
            # restore hole sensing to default
            if holeEnable:
                lineNum += 1
                gcodeList.append('#<holes>=0 (disable hole sensing)')
                holeEnable = False
            if firstMaterial:
                RUN(['halcmd', 'setp', 'qtplasmac.material_change_number', '{}'.format(firstMaterial)])
            gcodeList.append(line)
            continue
        # check feed rate
        if 'f' in line:
            line = check_f_word(line)
        # restore velocity if required
        if holeActive:
            lineNum += 1
            gcodeList.append('m67 e3 q0 (arc complete, velocity 100%)')
            holeActive = False
        # set last X/Y position
        if holeEnable and len(line) and ('x' in line or 'y' in line):
            lastX, lastY = set_last_coordinates(lastX, lastY)
        gcodeList.append(line)

# for pierce only mode
if pierceOnly:
    gcodeList.append('')
    if rapidLine:
        gcodeList.append('{}'.format(rapidLine))
    gcodeList.append('M2 (END)')

# error and warning notifications
if codeError or codeWarn:
    errorText = ''
    warnText = ''
    with open(errorFile, 'w') as errFile:
        for line in errorLines:
                errFile.write('{}\n'.format(line))
    if codeError:
        print('M2 (end program)')
        if errorMath:
            msg  = 'G2 and G3 moves require explicit values if hole sensing is enabled.\n'
            errorText += message_set(errorMath, msg)
        if errorMissMat:
            msg  = 'The Material selected is missing from the material file.\n'
            errorText += message_set(errorMissMat, msg)
        if errorTempMat:
            msg  = 'Error attempting to add a temporary material.\n'
            errorText += message_set(errorTempMat, msg)
        if errorNewMat:
            msg  = 'Cannot add new material, number is in use.\n'
            errorText += message_set(errorNewMat, msg)
        if errorEditMat:
            msg  = 'Cannot add or edit material from G-Code file with invalid parameter or value.\n'
            errorText += message_set(errorEditMat, msg)
        if errorWriteMat:
            msg  = 'Error attempting to write to the material file.\n'
            errorText += message_set(errorWriteMat, msg)
        if errorReadMat:
            msg  = 'Error attempting to read from the material file.\n'
            errorText += message_set(errorReadMat, msg)
        if errorCompMat:
            msg  = 'Cannot validate a material change with cutter compensation active.\n'
            errorText += message_set(errorCompMat, msg)
        if errorFirstMove:
            msg  = 'M3 command detected before movement.\n'
            errorText += message_set(errorFirstMove, msg)
    if codeWarn:
        if warnUnitsDep:
            msg  = '<m_diameter> and #<i_diameter> are deprecated in favour of #<h_diameter>.\n'
            msg += 'The diameter will be set in the current units of the G-Code file.\n'
            warnText += message_set(warnUnitsDep, msg)
        if warnPierceScribe:
            msg  = 'Pierce only mode is invalid while scribing.\n'
            warnText += message_set(warnPierceScribe, msg)
        if warnMatLoad:
            msg  = 'Materials were not reloaded in a timely manner.\n'
            msg  = 'Try reloading the G-Code file.\n'
            warnText += message_set(warnMatLoad, msg)
        if warnHoleDir:
            msg  = 'This cut appears to be a hole, did you mean to cut it clockwise?\n'
            warnText += message_set(warnHoleDir, msg)
        if warnCompTorch:
            msg  = 'Cannot enable/disable torch with G41/G42 compensation active.\n'
            warnText += message_set(warnCompTorch, msg)
        if warnCompVel:
            msg  = 'Cannot reduce velocity with G41/G42 compensation active.\n'
            warnText += message_set(warnCompVel, msg)
        if warnFeed:
            for n in range(0, len(warnFeed)):
                msg0 = 'Line'
                msg1 = 'does not match Material'
                msg2 = 'feed rate of '
                warnText += '{} {:0.0f}: F{} {}_{}\'s {} {:0.0f}\n'.format(msg0, warnFeed[n][0], warnFeed[n][1], msg1, warnFeed[n][2], msg2, warnFeed[n][3])
        if warnChar:
            msg  = 'Invalid characters, line has been commented out.\n'
            warnText += message_set(warnChar, msg)
    dialog_box('G-CODE ERRORS & WARNINGS', QStyle.SP_MessageBoxCritical, errorText, warnText)
# create empty error file if no errors
else:
    with open(errorFile, 'w') as errFile:
        pass

# output the finalised g-code
with open(filteredBkp, 'w') as outFile:
    for line in gcodeList:
        print(line)
        outFile.write('{}\n'.format(line))
    print(';qtplasmac filtered g-code file')
    outFile.write(';qtplasmac filtered g-code file')
