'''
bolt_circle.py

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
import sys
import math
import gettext

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f'{f.split("/lib")[0]}', 'share', 'locale')
        break
gettext.install("linuxcnc", localedir=localeDir)


# Conv is the upstream calling module
def preview(Conv, fTmp, fNgc, fNgcBkp,
            matNumber, matName,
            preAmble, postAmble,
            leadinLength, leadoutLength, shapeAng,
            isCenter, xOffset, yOffset,
            kerfWidth,
            isOvercut, overCut,
            smallHoleDia, smallHoleSpeed,
            circleDia, holeDia, holeNum, circleAng,
            invalidLeads):
    error = ''
    msg1 = _('entry is invalid')
    valid, xOffset = Conv.conv_is_float(xOffset)
    if not valid and xOffset:
        msg0 = _('X ORIGIN')
        error += f'{msg0} {msg1}\n\n'
    valid, yOffset = Conv.conv_is_float(yOffset)
    if not valid and yOffset:
        msg0 = _('Y ORIGIN')
        error += f'{msg0} {msg1}\n\n'
    valid, leadinLength = Conv.conv_is_float(leadinLength)
    if not valid and leadinLength:
        msg0 = _('LEAD IN')
        error += f'{msg0} {msg1}\n\n'
    valid, leadoutLength = Conv.conv_is_float(leadoutLength)
    if not valid and leadoutLength:
        msg0 = _('LEAD OUT')
        error += f'{msg0} {msg1}\n\n'
    valid, circleDia = Conv.conv_is_float(circleDia)
    if not valid and circleDia:
        msg0 = _('DIAMETER')
        error += f'{msg0} {msg1}\n\n'
    valid, holeDia = Conv.conv_is_float(holeDia)
    if not valid and holeDia:
        msg0 = _('HOLE DIAMETER')
        error += f'{msg0} {msg1}\n\n'
    valid, holeNum = Conv.conv_is_int(holeNum)
    if not valid and holeNum:
        msg0 = _('# OF HOLES')
        error += f'{msg0} {msg1}\n\n'
    valid, shapeAng = Conv.conv_is_float(shapeAng)
    if not valid and shapeAng:
        msg0 = _('ANGLE')
        error += f'{msg0} {msg1}\n\n'
    valid, circleAng = Conv.conv_is_float(circleAng)
    if not valid and circleAng:
        msg0 = _('CIRCLE ANGLE')
        error += f'{msg0} {msg1}\n\n'
    valid, oclength = Conv.conv_is_float(overCut)
    if not valid and oclength:
        msg0 = _('OC LENGTH')
        error += f'{msg0} {msg1}\n\n'
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg0 = _('Invalid Kerf Width entry in material')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return error
    if circleDia == 0:
        msg = _('DIAMETER cannot be zero')
        error += f'{msg}\n\n'
    if holeDia == 0:
        msg = _('HOLE DIAMETER cannot be zero')
        error += f'{msg}\n\n'
    if holeNum == 0:
        msg = _('# OF HOLES cannot be zero')
        error += f'{msg}\n\n'
    if circleAng == 0:
        msg = _('CIRCLE ANG cannot be zero')
        error += f'{msg}\n\n'
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
    down = math.radians(270)
    outTmp = open(fTmp, 'w')
    outNgc = open(fNgc, 'w')
    inWiz = open(fNgcBkp, 'r')
    for line in inWiz:
        if '(new conversational file)' in line:
            if('\\n') in preAmble:
                outNgc.write('(preamble)\n')
                for l in preAmble.split('\\n'):
                    outNgc.write(f'{l}\n')
            else:
                outNgc.write(f'\n{preAmble} (preamble)\n')
            break
        elif '(postamble)' in line:
            break
        elif 'm2' in line.lower() or 'm30' in line.lower():
            continue
        outNgc.write(line)
    for hole in range(holeNum):
        outTmp.write(f'\n(conversational bolt circle, hole #{hole + 1})\n')
        outTmp.write(f';using material #{matNumber}: {matName}\n')
        outTmp.write(f'M190 P{matNumber}\n')
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        xhC = xC + cRadius * math.cos(hAngle * hole + angle)
        yhC = yC + cRadius * math.sin(hAngle * hole + angle)
        xS = xhC - hRadius
        yS = yhC
        if sHole or invalidLeads == 2:
            xlStart = xS + leadinOffset
            ylStart = yhC
            outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write(f'g1 x{xS:.6f} y{yS:.6f}\n')
            if sHole:
                outTmp.write(f'm67 E3 Q{smallHoleSpeed}\n')
        else:
            xlCentre = xS + (leadinOffset * math.cos(angle + right))
            ylCentre = yS + (leadinOffset * math.sin(angle + right))
            xlStart = xlCentre + (leadinOffset * math.cos(angle + up))
            ylStart = ylCentre + (leadinOffset * math.sin(angle + up))
            outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
            outTmp.write('m3 $0 s1\n')
            if leadinLength:
                outTmp.write(f'g3 x{xS:.6f} y{yS:.6f} i{xlCentre - xlStart:.6f} j{ylCentre - ylStart:.6f}\n')
        outTmp.write(f'g3 x{xS:.6f} y{yS:.6f} i{hRadius:.6f}\n')
        torch = True
        if sHole:
            if isOvercut:
                torch = False
                outTmp.write('m62 p3 (disable torch)\n')
                centerX = xS + hRadius
                centerY = yS
                cosA = math.cos(oclength / hRadius)
                sinA = math.sin(oclength / hRadius)
                cosB = (xS - centerX) / hRadius
                sinB = (yS - centerY) / hRadius
                endX = centerX + hRadius * ((cosB * cosA) - (sinB * sinA))
                endY = centerY + hRadius * ((sinB * cosA) + (cosB * sinA))
                outTmp.write(f'g3 x{endX:.6f} y{endY:.6f} i{hRadius:.6f} j{0:.6f}\n')
        else:
            if leadoutLength and not invalidLeads:
                xlCentre = xS + (leadoutOffset * math.cos(angle + right))
                ylCentre = yS + (leadoutOffset * math.sin(angle + right))
                xlStart = xlCentre + (leadoutOffset * math.cos(angle + down))
                ylStart = ylCentre + (leadoutOffset * math.sin(angle + down))
                outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
                outTmp.write('m3 $0 s1\n')
                if leadinLength:
                    outTmp.write(f'g2 x{xS:.6f} y{yS:.6f} i{xlCentre - xlStart:.6f} j{ylCentre - ylStart:.6f}\n')
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
            outNgc.write(f'{l}\n')
    else:
        outNgc.write(f'\n{postAmble} (postamble)\n')
    outNgc.write('m2\n')
    outNgc.close()
    return False
