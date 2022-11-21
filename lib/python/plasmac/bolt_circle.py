'''
bolt_circle.py

Copyright (C) 2020, 2021, 2022  Phillip A Carter
Copyright (C) 2020, 2021, 2022  Gregory D Carl

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
import sys
import math
import gettext

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join('{}'.format(f.split('/lib')[0]),'share','locale')
        break
gettext.install("linuxcnc", localedir=localeDir)

# Conv is the upstream calling module
def preview(Conv, fTmp, fNgc, fNgcBkp, \
            matNumber, matName, \
            preAmble, postAmble, \
            leadinLength, leadoutLength, shapeAng, \
            isCenter, xOffset, yOffset, \
            kerfWidth, \
            isOvercut, overCut, \
            smallHoleDia, smallHoleSpeed, \
            circleDia, holeDia, holeNum, circleAng, \
            invalidLeads):
    error = ''
    msg1 = _('entry is invalid')
    valid, xOffset = Conv.conv_is_float(xOffset)
    if not valid and xOffset:
        msg0 = _('X ORIGIN')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yOffset = Conv.conv_is_float(yOffset)
    if not valid and yOffset:
        msg0 = _('Y ORIGIN')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, leadinLength = Conv.conv_is_float(leadinLength)
    if not valid and leadinLength :
        msg0 = _('LEAD IN')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, leadoutLength = Conv.conv_is_float(leadoutLength)
    if not valid and leadoutLength:
        msg0 = _('LEAD OUT')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, circleDia = Conv.conv_is_float(circleDia)
    if not valid and circleDia:
        msg0 = _('DIAMETER')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, holeDia = Conv.conv_is_float(holeDia)
    if not valid and holeDia:
        msg0 = _('HOLE DIAMETER')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, holeNum = Conv.conv_is_int(holeNum)
    if not valid and holeNum:
        msg0 = _('# OF HOLES')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, shapeAng = Conv.conv_is_float(shapeAng)
    if not valid and shapeAng:
        msg0 = _('ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, circleAng = Conv.conv_is_float(circleAng)
    if not valid and circleAng:
        msg0 = _('CIRCLE ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, oclength = Conv.conv_is_float(overCut)
    if not valid and oclength:
        msg0 = _('OC LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg0 = _('Invalid Kerf Width entry in material')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return error
    if circleDia == 0:
        msg = _('DIAMETER cannot be zero')
        error += '{}\n\n'.format(msg)
    if holeDia == 0:
        msg = _('HOLE DIAMETER cannot be zero')
        error += '{}\n\n'.format(msg)
    if holeNum == 0:
        msg = _('# OF HOLES cannot be zero')
        error += '{}\n\n'.format(msg)
    if circleAng == 0:
        msg = _('CIRCLE ANG cannot be zero')
        error += '{}\n\n'.format(msg)
    if error:
        return error
    cRadius = circleDia / 2
    if circleAng == 360:
        hAngle = math.radians(circleAng / holeNum)
    else:
        hAngle = math.radians(circleAng / (holeNum - 1))
    kOffset = kerfWidth / 2
    hRadius = (holeDia / 2) - kOffset
    angle = math.radians(shapeAng)
    leadinOffset = leadinLength * math.sin(math.radians(45))
    leadoutOffset = leadoutLength * math.sin(math.radians(45))
    if leadinOffset > hRadius - kOffset:
        leadinOffset = hRadius - kOffset
    if holeDia < smallHoleDia:
        sHole = True
    else:
        sHole = False
    if isCenter:
        xC = xOffset
        yC = yOffset
    else:
        xC = xOffset + cRadius
        yC = yOffset + cRadius
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    outTmp = open(fTmp, 'w')
    outNgc = open(fNgc, 'w')
    inWiz = open(fNgcBkp, 'r')
    for line in inWiz:
        if '(new conversational file)' in line:
            if('\\n') in preAmble:
                outNgc.write('(preamble)\n')
                for l in preAmble.split('\\n'):
                    outNgc.write('{}\n'.format(l))
            else:
                outNgc.write('\n{} (preamble)\n'.format(preAmble))
            break
        elif '(postamble)' in line:
            break
        elif 'm2' in line.lower() or 'm30' in line.lower():
            continue
        outNgc.write(line)
    for hole in range(holeNum):
        outTmp.write('\n(conversational bolt circle, hole #{})\n'.format(hole + 1))
        outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
        outTmp.write('M190 P{}\n'.format(matNumber))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        xhC = xC + cRadius * math.cos(hAngle * hole + angle)
        yhC = yC + cRadius * math.sin(hAngle * hole + angle)
        xS = xhC - hRadius
        yS = yhC
        if sHole or invalidLeads == 2:
            xlStart = xS + leadinOffset
            ylStart = yhC
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
            if sHole:
                outTmp.write('m67 E3 Q{}\n'.format(smallHoleSpeed))
        else:
            xlCentre = xS + (leadinOffset * math.cos(angle + right))
            ylCentre = yS + (leadinOffset * math.sin(angle + right))
            xlStart = xlCentre + (leadinOffset * math.cos(angle + up))
            ylStart = ylCentre + (leadinOffset * math.sin(angle + up))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            outTmp.write('m3 $0 s1\n')
            if leadinLength:
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlCentre - xlStart, ylCentre - ylStart))
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f}\n'.format(xS, yS, hRadius))
        torch = True
        if sHole:
            if isOvercut:
                Torch = False
                outTmp.write('m62 p3 (disable torch)\n')
                centerX = xS + hRadius
                centerY = yS
                cosA = math.cos(oclength / hRadius)
                sinA = math.sin(oclength / hRadius)
                cosB = (xS - centerX) / hRadius
                sinB = (yS - centerY) / hRadius
                endX = centerX + hRadius * ((cosB * cosA) - (sinB * sinA))
                endY = centerY + hRadius * ((sinB * cosA) + (cosB * sinA))
                outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{3:.6f}\n'.format(endX, endY, hRadius, 0))
        else:
            if leadoutLength and not invalidLeads:
                xlCentre = xS + (leadinOffset * math.cos(angle + right))
                ylCentre = yS + (leadinOffset * math.sin(angle + right))
                xlStart = xlCentre + (leadinOffset * math.cos(angle + down))
                ylStart = ylCentre + (leadinOffset * math.sin(angle + down))
                outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                outTmp.write('m3 $0 s1\n')
                if leadinLength:
                    outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlCentre - xlStart, ylCentre - ylStart))
        outTmp.write('m5 $0\n')
        if sHole:
            outTmp.write('M68 E3 Q0 (reset feed rate to 100%)\n')
        if not torch:
            torch = True
            outTmp.write('m65 p3 (enable torch)\n')
    outTmp.close()
    outTmp = open(fTmp, 'r')
    for line in outTmp:
        outNgc.write(line)
    outTmp.close()
    if('\\n') in postAmble:
        outNgc.write('(postamble)\n')
        for l in postAmble.split('\\n'):
            outNgc.write('{}\n'.format(l))
    else:
        outNgc.write('\n{} (postamble)\n'.format(postAmble))
    outNgc.write('m2\n')
    outNgc.close()
    return False
