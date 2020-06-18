#!/usr/bin/env python2

'''
plasmac_gcode.py

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
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import sys
import linuxcnc
import math
import gtk
import shutil
import time
from subprocess import Popen, PIPE

ini = linuxcnc.ini(os.environ['INI_FILE_NAME'])

codeError = False
overCut = False
holeActive = False
holeEnable = False
imperial = [25.4, 6]
lastX = 0
lastY = 0
inCode = sys.argv[1]
materialFile = ini.find('EMC', 'MACHINE').lower() + '_material.cfg'
tmpMmaterialFile = ini.find('EMC', 'MACHINE').lower() + '_material.tmp'
runFile = materialFile.replace('material','run')
metric = [1, 4]
minDiameter = 32
scale, precision = imperial if ini.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else metric
torchEnable = True
velocity = 60
pierceOnly = False
scribing = False
rapidLine = ''
thisMaterial = 0
offsetG41 = False
cutType = int(Popen('halcmd getp plasmac_run.cut-type', stdout = PIPE, shell = True).communicate()[0])
#pauseAtEnd = 2
# error dialog
def dialog_error(mode, title, error):
    md = gtk.MessageDialog(None,
                           gtk.DIALOG_DESTROY_WITH_PARENT,
                           mode,
                           gtk.BUTTONS_CLOSE,
                           error)
    md.set_position(gtk.WIN_POS_CENTER_ALWAYS)
    md.set_keep_above(True)
    md.set_title(title)
    md.run()
    md.destroy()

# check if arc is a hole
def check_if_hole():
    global lastX, lastY
    endX = lastX
    endY = lastY
    if 'x' in line: endX = get_position('x')
    if 'y' in line: endY = get_position('y')
    if lastX == endX and lastY == endY:
        I = J = 0
        if 'i' in line: I = get_position('i')
        if 'j' in line: J = get_position('j')
        radius = get_hole_radius(I, J)
        print(line)
        if overCut and radius <= (minDiameter / 2 / scale):
            overburn(I, J, radius)
    else:
        print(line)
        lastX = endX
        lastY = endY

# turn torch off and move 4mm (0.157) past hole end
def overburn(I, J, radius):
    global lastX, lastY, torchEnable, lineNum
    centerX = lastX + I
    centerY = lastY + J
    cosA = math.cos(oclength / radius)
    sinA = math.sin(oclength / radius)
    cosB = ((lastX - centerX) / radius)
    sinB = ((lastY - centerY) / radius)
    lineNum += 1
    if offsetG41:
        print(';;; m62 p3 (((inactive due to g41)))')
        wng  = 'Cannot enable/disable torch\n'
        wng += 'With cutter compensation active\n'
        wng += '\nWarning for line #{}\n'.format(lineNum)
        dialog_error(gtk.MESSAGE_WARNING,'WARNING', wng)
    else:
        print('m62 p3 (disable torch)')
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
    print('g{0} x{1:0.{5}f} y{2:0.{5}f} i{3:0.{5}f} j{4:0.{5}f}'.format(dir, endX, endY, I, J, precision))
    lastX = endX
    lastY = endY

# get hole radius and set velocity percentage
def get_hole_radius(I, J):
    global holeActive, lineNum
    if offsetG41:
        radius = math.sqrt((I ** 2) + (J ** 2))
    else:
        radius = math.sqrt((I ** 2) + (J ** 2)) + (materialDict[thisMaterial][1] / 2)
    # velocity reduction required
    if radius <= (minDiameter / 2 / scale):
        if offsetG41:
            print(';;; m67 e3 q0 (((inactive due to g41)))')
            lineNum += 1
            wng  = 'Cannot reduce velocity\n'
            wng += 'With cutter compensation active\n'
            wng += '\nWarning for line #{}\n'.format(lineNum)
            dialog_error(gtk.MESSAGE_WARNING,'WARNING', wng)
        else:
            print('m67 e3 q{0} (diameter:{1:0.3f}, velocity:{0}%)'.format(velocity, radius * 2))
            lineNum += 1
            if line.startswith('g2'):
                wng = 'This cut appears to be a hole\n'
                wng += 'Did you mean to cut clockwise?\n'
                wng += '\nWarning for line {}\n'.format(lineNum)
                dialog_error(gtk.MESSAGE_WARNING,'WARNING', wng)
            holeActive = True
    # no velocity reduction required
    else:
        if holeActive:
            print('m67 e3 q0 (arc complete, velocity 100%)')
            holeActive = False
    return radius

# get axis position
def get_position(axis):
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

# get the last X and Y positions
def get_last_position(Xpos, Ypos):
    if line.startswith('g') or \
       line.startswith('x') or \
       line.startswith('y'):
        if 'x' in line:
            if get_position('x') is not None:
                Xpos = get_position('x')
        if 'y' in line:
            if get_position('y') is not None:
                Ypos = get_position('y')
    return Xpos, Ypos

# comment out all Z commands
def comment_out_z_commands():
    global holeActive
    newline = ''
    newz = ''
    removing = 0
    comment = 0
    for bit in line:
        if comment:
            if bit == ')':
                comment = 0
            newline += bit
        elif removing:
            if bit in '0123456789.- ':
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
        print('m67 e3 q0 (arc complete, velocity 100%)')
        holeActive = False
    print('{} {})'.format(newline, newz))

# check if math used or explicit values
def check_math(axis):
    global codeError
    tmp1 = line.split(axis)[1]
    if tmp1.startswith('[') or tmp1.startswith('#'):
        codeError = True
        wng  = 'PlasmaC GCode parser\n'
        wng += 'requires explicit values\n'
        wng += '\nError near line #{}\n'.format(lineNum)
        wng += '\nDisable hole sensing\n'
        wng += 'or edit GCode file to suit\n'
        dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)

# write temporary materials file
def write_temp_material(data):
    with open(tmpMmaterialFile, 'w') as fWrite:
        fWrite.write('#plasmac temp default material file, format is:\n')
        fWrite.write('#name = value\n\n')
        fWrite.write('kerf-width={}\n'.format(data[2]))
        fWrite.write('thc-enable={}\n'.format(data[3]))
        fWrite.write('pierce-height={}\n'.format(data[4]))
        fWrite.write('pierce-delay={}\n'.format(data[5]))
        fWrite.write('puddle-jump-height={}\n'.format(data[6]))
        fWrite.write('puddle-jump-delay={}\n'.format(data[7]))
        fWrite.write('cut-height={}\n'.format(data[8]))
        fWrite.write('cut-feed-rate={}\n'.format(data[9]))
        fWrite.write('cut-amps={}\n'.format(data[10]))
        fWrite.write('cut-volts={}\n'.format(data[11]))
        fWrite.write('pause-at-end={}\n'.format(data[12]))
        fWrite.write('gas-pressure={}\n'.format(data[13]))
        fWrite.write('cut-mode={}\n'.format(data[14]))
        fWrite.write('\n')
    Popen('halcmd setp plasmac_run.temp-material 1', stdout = PIPE, shell = True)
    matDelay = time.time()
    while 1:
        if time.time() > matDelay + 3:
            wng  = 'Temporary materials was not loaded in a timely manner:\n\n'
            wng += 'Try to reload the G-Code file.\n'
            dialog_error(gtk.MESSAGE_ERROR, 'MATERIAL ERROR', wng)
            break
        if Popen('halcmd getp plasmac_run.temp-material', stdout = PIPE, shell = True).communicate()[0] == 'FALSE\n':
            break

# rewrite the material file
def rewrite_materials(newMats):
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
        if line.strip().startswith('[MATERIAL_NUMBER_'):
            mNum = int(line.split('NUMBER_')[1].replace(']',''))
            if mNum in newMats:
                add_edit_material(mNum, newMats[mNum], outFile)
        if mNum not in newMats:
            outFile.write(line)
        line = inFile.readline()
        if not line:
            break
    for mat in sorted(newMats):
        if newMats[mat][0] == 1 or mat not in materialDict:
            add_edit_material(mat, newMats[mat], outFile)
    inFile.close()
    outFile.close()
    Popen('halcmd setp plasmac_run.material-reload 1', stdout = PIPE, shell = True)
    get_defaults()
    get_materials()
    matDelay = time.time()
    while 1:
        if time.time() > matDelay + 3:
            wng  = 'Materials were not reloaded in a timely manner:\n\n'
            wng += 'Try a manual Reload or reload the G-Code file.\n'
            dialog_error(gtk.MESSAGE_ERROR, 'MATERIAL ERROR', wng)
            break
        if Popen('halcmd getp plasmac_run.material-reload', stdout = PIPE, shell = True).communicate()[0] == 'FALSE\n':
            break

# add a new material or or edit an existing material
def add_edit_material(num, data, outFile):
    outFile.write('[MATERIAL_NUMBER_{}]\n'.format(num))
    outFile.write('NAME               = {}\n'.format(data[1]))
    outFile.write('KERF_WIDTH         = {}\n'.format(data[2]))
    outFile.write('THC                = {}\n'.format(data[3]))
    outFile.write('PIERCE_HEIGHT      = {}\n'.format(data[4]))
    outFile.write('PIERCE_DELAY       = {}\n'.format(data[5]))
    outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(data[6]))
    outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(data[7]))
    outFile.write('CUT_HEIGHT         = {}\n'.format(data[8]))
    outFile.write('CUT_SPEED          = {}\n'.format(data[9]))
    outFile.write('CUT_AMPS           = {}\n'.format(data[10]))
    outFile.write('CUT_VOLTS          = {}\n'.format(data[11]))
    outFile.write('PAUSE_AT_END       = {}\n'.format(data[12]))
    outFile.write('GAS_PRESSURE       = {}\n'.format(data[13]))
    outFile.write('CUT_MODE           = {}\n'.format(data[14]))
    outFile.write('\n')

#get the default feed rate and kerf width
def get_defaults():
    global fRate, kWidth
    with open(runFile, 'r') as rFile:
        fRate = kWidth = 0.0
        for line in rFile:
            if line.startswith('cut-feed-rate'):
                fRate = float(line.split('=')[1])
            if line.startswith('kerf-width'):
                kWidth = float(line.split('=')[1])

# create a dict of material numbers and kerf widths
def get_materials():
    global fRate, kWidth, materialDict
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

# start processing
get_defaults()
get_materials()

# first pass check for material file edits
with open(inCode, 'r') as fRead:
    newMats = {}
    tmpDefault = []
    for line in fRead:
        # if end of program
        if line.lower().strip().startswith('m2') or line.lower().strip().startswith('m30'):
            break
        tmpDict = {}
        tmpList = []
        th = 0
        kw = jh = jd = ca = cv = pe = gp = cm = 0.0
        try:
            if 'ph=' in line and 'pd=' in line and 'ch=' in line and 'fr=' in line:
                if '(o=0' in line and not tmpDefault:
                    nu = 0
                    na = 'Temporary'
                    tmpList.append(0)
                elif '(o=1' in line and 'nu=' in line and 'na=' in line:
                    tmpList.append(1)
                elif '(o=2' in line and 'nu=' in line and 'na=' in line:
                    tmpList.append(2)
                if tmpList[0] in [0, 1, 2]:
                    for item in line.split('(')[1].split(')')[0].split(','):
                        # mandatory items
                        if 'nu=' in item:
                            nu = int(item.split('=')[1])
                        elif 'na=' in item:
                            na = item.split('=')[1].strip()
                        elif 'ph=' in item:
                            ph = float(item.split('=')[1])
                        elif 'pd=' in item:
                            pd = float(item.split('=')[1])
                        elif 'ch=' in item:
                            ch = float(item.split('=')[1])
                        elif 'fr=' in item:
                            fr = float(item.split('=')[1])
                        # optional items
                        elif 'kw=' in item:
                            kw = float(item.split('=')[1])
                        elif 'th=' in item:
                            th = int(item.split('=')[1])
                        elif 'jh=' in item:
                            jh = float(item.split('=')[1])
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
                    for i in [na,kw,th,ph,pd,jh,jd,ch,fr,ca,cv,pe,gp,cm]:
                        tmpList.append(i)
                    if tmpList[0] == 0:
                        tmpDefault = tmpList
                    elif nu in materialDict and tmpList[0] == 1:
                        wng  = 'Cannot add new Material #{}\n\n'.format(nu)
                        wng += 'Material number is in use\n'
                        dialog_error(gtk.MESSAGE_ERROR, 'GCODE MATERIALS ERROR', wng)
                    else:
                        newMats[nu] = tmpList
                else:
                    wng  = 'Cannot add or edit material from G-Code file.\n\n'
                    wng += 'Invalid parameter or value in:'
                    wng += '{}\n'.format(line)
                    wng += 'This material will not be processed'
                    dialog_error(gtk.MESSAGE_ERROR, 'GCODE MATERIALS ERROR', wng)
        except:
            wng  = 'Cannot add or edit material from G-Code file.\n\n'
            wng += 'Invalid/missing parameter or value in:\n\n'
            wng += '{}\n'.format(line)
            wng += 'This material will not be processed'
            dialog_error(gtk.MESSAGE_ERROR, 'GCODE MATERIALS ERROR', wng)
    if newMats:
        rewrite_materials(newMats)
    if tmpDefault:
        write_temp_material(tmpDefault)

# second pass, check for valid material numbers and distance modes
with open(inCode, 'r')as fRead:
    lineNum = 0
    material = 0
    firstMaterial = False
    oclength = 4
    for line in fRead:
        lineNum += 1
        # convert to lower case and remove whitespace and spaces
        line = line.lower().strip().replace(' ','')
        # remove line numbers
        if line.lower().startswith('n'):
            line = line[1:]
            while line[0].isdigit() or line[0] == '.':
                line = line[1:].lstrip()
                if not line:
                    break
        # if line is a comment get next line
        if line.startswith(';') or line.startswith('('):
            continue
        # if end of program
        if line.startswith('m2') or line.startswith('m30'):
            break
        # get overcut length
        if line.startswith('#<oclength>'):
            oclength = float(line.split('=')[1])
        # get cut type
        elif (line.startswith('#<pierce-only>') and line.split('=')[1][0] == '1') or cutType == 1:
            pierceOnly = True
        # are we scribing
        elif line.startswith('m3$1s'):
            scribing = True
        # cannot scribe and pierce together
        elif pierceOnly and scribing:
            codeError = True
            wng  = 'scribe is invalid for pierce only mode\n'
            wng += '\nError near line #{}\n'.format(lineNum)
            wng += '\nEdit GCode file to suit'
            dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)
            scribing = False
        # if material change
        elif line.startswith('m190'):
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
            material = int(m)
            if material not in materialDict:
                codeError = True
                wng  = 'Material {} is missing from:\n'.format(material)
                wng += '{}\n'.format(materialFile)
                wng += '\nError near line #{}\n'.format(lineNum)
                wng += '\nAdd a new material\n'
                wng += 'or edit GCode file to suit'
                dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)
            if not firstMaterial:
                firstMaterial = True
                Popen('halcmd setp plasmac_run.first-material {}'.format(material), stdout = PIPE, shell = True)
        # set units
        elif 'g21' in line:
            scale, precision = metric
        elif 'g20' in line:
            scale, precision = imperial
        # check for g41 offset set
        elif 'g41' in line:
            offsetG41 = True
        # check for g41 offset cleared
        elif 'g40' in line:
            offsetG41 = False
        # if hole sensing code
        elif line.startswith('#<holes>'):
            if line.split('=')[1][0] == '1':
                holeEnable = True
            elif line.split('=')[1][0] == '2':
                holeEnable = overCut = True
            else:
                holeEnable = overCut = False
        # if unsupported distance mode
        elif holeEnable and 'g91' in line and not 'g91.1' in line:
                codeError = True
                wng  = 'PlasmaC GCode parser only\n'
                wng += 'supports Distance Mode G90\n'
                wng += '\nError near line #{}\n'.format(lineNum)
                wng += '\nEdit GCode file to suit'
                dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)
        # if unsupported arc distance mode
        elif holeEnable and 'g90.1' in line:
                codeError = True
                wng  = 'PlasmaC GCode parser only\n'
                wng += 'supports Arc Distance Mode G91.1\n'
                wng += '\nError near line #{}\n'.format(lineNum)
                wng += '\nEdit GCode file to suit'
                dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)
        elif holeEnable and 'x' in line: check_math('x')
        elif holeEnable and 'y' in line: check_math('y')
        elif holeEnable and 'i' in line: check_math('i')
        elif holeEnable and 'j' in line: check_math('j')
        elif holeEnable and 'diameter>' in line:
            if not line.startswith('#<m_d') and not line.startswith('#<i_d'):
                codeError = True
                wng  = 'Invalid diameter word\n'
                wng += '\nError near line #{}\n'.format(lineNum)
                wng  += '\nOptions are:\n'
                wng  += '#<m_diameter> for metric\n'
                wng  += '#<i_diameter> for imperial\n'
                wng  += '\nEdit GCode file to suit'
                dialog_error(gtk.MESSAGE_ERROR, 'GCODE ERROR', wng)
        elif 'f' in line and material in materialDict:
            inFeed = line.split('f')[1].strip()
            rawFeed = ''
            codeFeed = 0.0
            while len(inFeed) and (inFeed[0].isdigit() or inFeed[0] == '.'):
                rawFeed = rawFeed + inFeed[0]
                inFeed = inFeed[1:].lstrip()
            if rawFeed:
                codeFeed = float(rawFeed)
                if codeFeed != float(materialDict[material][0]):
                    cutFeed = materialDict[material][0]
                    dec = 0 if scale == 1 else 1

#                   FIX_ME if state tag ups pin released in master branch
                    if linuxcnc.version.startswith('2.9.') or True:
                        if cutFeed and cutFeed != codeFeed:
                            wng   = 'Gcode feed rate is F{:0.{}f}\n'.format(codeFeed, dec)
                            wng  += '\nMaterial #{} feed rate is F{:0.{}f}\n'.format(material, cutFeed, dec)
                            wng  += '\nTHC calculations will be based on the\n'
                            wng  += 'material #{} feed rate which may cause issues.\n'.format(material)
                        else:
                            wng   = 'Gcode feed rate is F{:0.{}f}\n'.format(codeFeed, dec)
                            wng  += '\nMaterial #{} feed rate is F{:0.{}f}\n'.format(material, cutFeed, dec)
                            wng  += '\nThis will cause the THC calculations\n'
                            wng  += 'to use the motion.requested-vel HAL pin\n'
                            wng  += 'which is not recommended.\n'
                        wng  += '\nThe recommended settings are to use\n'
                        wng  += 'F#<_hal[plasmac.cut-feed-rate]>\n'
                        wng  += 'in the G-Code file and a valid cut feed rate\n'
                        wng  += 'in the material cut parameters.\n'
                        wng  += '\nWarning near line #{}\n'.format(lineNum)
                        dialog_error(gtk.MESSAGE_WARNING, 'Feed Rate WARNING', wng)
                    else:
                        pass

# third pass, process every line
# if full cut
if not pierceOnly:
    with open(inCode, 'r') as fRead:
        if codeError:
            lineNum = 1
            print ('M2 (End due to GCode error)')
        else:
            lineNum = 0
            offsetG41 = False
        for line in fRead:
            lineNum += 1
            # remove whitespace
            line = line.strip()
            # remove line numbers
            if line.lower().startswith('n'):
                line = line[1:]
                while line[0].isdigit() or line[0] == '.':
                    line = line[1:].lstrip()
                    if not line:
                        break
            # remove leading 0's from G & M codes
            if (line.lower().startswith('g') or \
               line.lower().startswith('m')) and \
               len(line) > 2:
                while line[1] == '0':
                    if line[2].isdigit():
                        line = line[:1] + line[2:]
                    else:
                        break
            # if a material edit line throw it away
            if 'o=' in line and 'nu=' in line and 'na=' in line and 'ph=' in line and \
                                'pd=' in line and 'ch=' in line and 'fr=' in line:
                continue
            # if a commented line then print it and get next line
            if line.startswith(';') or line.startswith('('):
                print line
                continue
            # if a ; comment at end of line preprocess it
            elif ';' in line:
                a,b = line.split(';', 1)
                line = '{} ({})'.format(a.strip().lower(),b)
            # if a () comment at end of line preprocess it
            elif '(' in line:
                a,b = line.split('(', 1)
                line = '{} ({}'.format(a.strip().lower(),b)
            # if any other line preprocess it
            else:
                line = line.lower()
            # if material change
            if line.startswith('m190'):
                if '(' in line:
                    a, b = line.split('p', 1)
                    b, c = b.split('(', 1)
                else:
                    a, b = line.split('p', 1)
                m = ''
                # get the material number
                for mNum in b.strip():
                    if mNum in '0123456789':
                        m += mNum
                material = int(m)
                thisMaterial = material
                print(';    material = {}'.format(material))
                print(';thisMaterial = {}'.format(thisMaterial))
                print(line)
            # if material change with cutter compensation
            elif 'm66' in line and offsetG41:
                wng  = 'Cannot complete a material change\n'
                wng += 'with cutter compensation acive\n'
                wng += '\nError near line #{}\n'.format(lineNum)
                dialog_error(gtk.MESSAGE_ERROR, 'ERROR', wng)
                print ';;; {} (((inactive due to g41)))'.format(line.strip())
            # check for g41 offset set
            elif 'g41' in line:
                offsetG41 = True
                print(line)
            # check for g41 offset cleared
            elif 'g40' in line:
                offsetG41 = False
                print(line)
            # if hole sense command
            elif line.startswith('#<holes>'):
                if line.split('=')[1].replace(' ','')[0] == '2':
                    holeEnable = overCut = True
                    print('{} (overcut for holes)'.format('#<holes> = 2'))
                elif line.split('=')[1].replace(' ','')[0] == '1':
                    holeEnable = True
                    overCut = False
                    print('{} (velocity reduction for holes)'.format('#<holes> = 1'))
                else:
                    holeEnable = overCut = False
                    print('{} (disable hole sensing)'.format('#<holes> = 0'))
            # if diameter command
            elif '_diameter>' in line:
                if line.startswith('#<i_d'):
                    multiplier = 25.4
                else:
                    multiplier = 1
                if (';') in line:
                    minDiameter = float(line.split('=')[1].split(';')[0]) * multiplier
                elif ('(') in line:
                    minDiameter = float(line.split('=')[1].split('(')[0]) * multiplier
                else:
                    minDiameter = float(line.split('=')[1]) * multiplier
                print(line)
            # if z axis in line but no other axes comment it
            elif 'z' in line and 1 not in [c in line for c in 'xyabcuvw'] and\
                 line.split('z')[1][0].isdigit():
                print('({})'.format(line))
            # if z axis and other axes in line, comment out the Z axis
            elif 'z' in line and line.split('z')[1][0] in '0123456789.- ':
                if holeEnable:
                    lastX, lastY = get_last_position(lastX, lastY)
                comment_out_z_commands()
            # if an arc command
            elif (line.startswith('g2') or line.startswith('g3')) and line.replace(' ','')[2].isalpha():
                if holeEnable:
                    check_if_hole()
                else:
                    print(line)
            # if torch off, flag it then print it
            elif line.replace(' ','').startswith('m62p3') or line.replace(' ','').startswith('m64p3'):
                torchEnable = False
                # if line.replace(' ','').startswith('m64p3'):
                #    pauseAtEnd += 1
                print(line)
            # if torch on, flag it then print it
            elif line.replace(' ','').startswith('m63p3') or line.replace(' ','').startswith('m65p3'):
                torchEnable = True
                print(line)
            # # if spindle on
            # elif line.startswith('m3') and not line.startswith('m30'):
            #    pauseAtEnd = 0
            #    print(line)
            # # if dwell
            # elif line.replace(' ','').startswith('g4p'):
            #    pauseAtEnd += 1
            #    print(line)
            # if spindle off
            elif line.startswith('m5'):
            # elif line.startswith('m5') and not line.startswith('m52'):
            #    if pauseAtEnd < 2:
            #        print('m64 p3 (disable torch)')
            #        torchEnable = False
            #        print('g4 p#<_hal[plasmac_run.pause-at-end-f]> (end of cut pause)')
            #        pauseAtEnd = 2
                print(line)
                # restore velocity if required
                if holeActive:
                    lineNum += 1
                    print('m68 e3 q0 (arc complete, velocity 100%)')
                    holeActive = False
                # if torch off, allow torch on 
                if not torchEnable:
                    lineNum += 1
                    print('m65 p3 (enable torch)')
                    torchEnable = True
            # if program end
            elif line.startswith('m2') or line.startswith('m30') or line.startswith('%'):
                # restore velocity if required
                if holeActive:
                    print('m68 e3 q0 (arc complete, velocity 100%)')
                    holeActive = False
                    lineNum += 1
                # if torch off, allow torch on 
                if not torchEnable:
                    print('m65 p3 (enable torch)')
                    torchEnable = True
                    lineNum += 1
                # restore hole sensing to default
                if holeEnable:
                    print('#<holes> = 0 (disable hole sensing)')
                    holeEnable = False
                    lineNum += 1
                print(line)
            # any other line
            else:
                if holeEnable:
                    # restore velocity if required
                    if holeActive:
                        print('m67 e3 q0 (arc complete, velocity 100%)')
                        holeActive = False
                    lastX, lastY = get_last_position(lastX, lastY)
                print(line)
#if pierce only
else:
    print('(Piercing Only)')
    spindleOn = False
    pierces = 0
    with open(inCode, 'r') as fRead:
        # print all lines up to the first spindle on
        for line in fRead:
            # remove whitespace
            line = line.strip()
            # remove line numbers
            if line.lower().startswith('n'):
                line = line[1:]
                while line[0].isdigit() or line[0] == '.':
                    line = line[1:].lstrip()
            # if a rapid move
            if line.lower().startswith('g0'):
                rapidLine = line
            # if a spindle on
            elif line.lower().replace(' ','').startswith('m3') and not \
                 line.lower().replace(' ','').startswith('m3$1'):
                spindleOn = True
                break
            elif not '#<pierce-only>' in line:
                print(line)
        #find all other spindle ons
        for line in fRead:
            if spindleOn:
                pierces += 1
                print('\n(Pierce #{})'.format(pierces))
                print(rapidLine)
                print('M3 $0 S1')
                print('G91')
                print('G1 X.000001')
                print('G90\nM5')
                rapidLine = ''
                spindleOn = False
            # remove whitespace
            line = line.strip()
            # remove line numbers
            if line.lower().startswith('n'):
                line = line[1:]
                while line[0].isdigit() or line[0] == '.':
                    line = line[1:].strip()
            # if a rapid move
            if line.lower().startswith('g0'):
                rapidLine = line
            # if a spindle on
            elif line.lower().replace(' ','').startswith('m3'):
                spindleOn = True
    print('')
    if rapidLine:
        print('{}'.format(rapidLine))
    print('M30 (END)')
