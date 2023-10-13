'''
circle.py

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
            leadinLength, leadoutLength, \
            isCenter, xOffset, yOffset, \
            kerfWidth, isExternal, \
            isOvercut, overCut, \
            smallHoleDia, smallHoleSpeed, \
            diameter, invalidLeads):
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
    valid, diameter = Conv.conv_is_float(diameter)
    if not valid and diameter:
        msg0 = _('DIAMETER')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, overCut = Conv.conv_is_float(overCut)
    if not valid and overCut:
        msg0 = _('OC LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Invalid Kerf Width entry in material')
        error += '{}\n\n'.format(msg)
    if error:
        return error
    if diameter == 0:
        msg = _('DIAMETER cannot be zero')
        error += '{}\n\n'.format(msg)
    if error:
        return error
    kOffset = kerfWidth / 2
    radius = diameter / 2
    angle = math.radians(45)
    leadInOffset = math.sin(angle) * leadinLength
    leadOutOffset = math.sin(math.radians(45)) * leadoutLength
    if leadInOffset > radius - kOffset:
        leadInOffset = radius - kOffset
    if isExternal:
        ijOffset = (radius + kOffset) * math.sin(angle)
    else:
        ijOffset = (radius - kOffset) * math.sin(angle)
    if isCenter:
        xC = xOffset
        yC = yOffset
    else:
        if isExternal:
            xC = xOffset + radius + kOffset
            yC = yOffset + radius + kOffset
        else:
            xC = xOffset + radius - kOffset
            yC = yOffset + radius - kOffset
    xS = xC - ijOffset
    yS = yC - ijOffset
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    if isExternal:
        dir = [left, down]
    else:
        dir = [right, up]
    if diameter < smallHoleDia:
        sHole = True
    else:
        sHole = False
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
    outTmp.write('\n(conversational circle)\n')
    outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
    outTmp.write('M190 P{}\n'.format(matNumber))
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    if leadInOffset:
        if (sHole and not isExternal) or invalidLeads == 2:
            xlStart = xS + leadInOffset * math.cos(angle)
            ylStart = yS + leadInOffset * math.sin(angle)
        else:
            xlcenter = xS + (leadInOffset * math.cos(angle + dir[0]))
            ylcenter = yS + (leadInOffset * math.sin(angle + dir[0]))
            xlStart = xlcenter + (leadInOffset * math.cos(angle + dir[1]))
            ylStart = ylcenter + (leadInOffset * math.sin(angle + dir[1]))
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
        outTmp.write('m3 $0 s1\n')
        if sHole:
            outTmp.write('M67 E3 Q{} (reduce feed rate to 60%)\n'.format(smallHoleSpeed))
        if (sHole and not isExternal) or invalidLeads == 2:
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        else:
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS, xlcenter - xlStart, ylcenter - ylStart))
    else:
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
        outTmp.write('m3 $0 s1\n')
        if sHole:
            outTmp.write('M67 E3 Q{} (reduce feed rate to 60%)\n'.format(smallHoleSpeed))
    if isExternal:
        outTmp.write('g2 x{0:.6f} y{1:.6f} i{2:.6f} j{2:.6f}\n'.format(xS, yS, ijOffset))
    else:
        outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{2:.6f}\n'.format(xS, yS, ijOffset))
    if leadOutOffset and not isOvercut and not (not isExternal and sHole) and not invalidLeads:
            if isExternal:
                dir = [left, up]
            else:
                dir = [right, down]
            xlcenter = xS + (leadOutOffset * math.cos(angle + dir[0]))
            ylcenter = yS + (leadOutOffset * math.sin(angle + dir[0]))
            xlEnd = xlcenter + (leadOutOffset * math.cos(angle + dir[1]))
            ylEnd = ylcenter + (leadOutOffset * math.sin(angle + dir[1]))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlcenter - xS, ylcenter - yS))
    torch = True
    if isOvercut and sHole and not isExternal:
        Torch = False
        outTmp.write('m62 p3 (disable torch)\n')
        centerX = xS + ijOffset
        centerY = yS + ijOffset
        cosA = math.cos(overCut / radius)
        sinA = math.sin(overCut / radius)
        cosB = ((xS - centerX) / radius)
        sinB = ((yS - centerY) / radius)
        endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
        endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
        outTmp.write('g3 x{0:.6f} y{1:.6f} i{2:.6f} j{2:.6f}\n'.format(endX, endY, ijOffset))
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
