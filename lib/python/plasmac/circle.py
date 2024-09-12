'''
circle.py

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
            leadinLength, leadoutLength,
            isCenter, xOffset, yOffset,
            kerfWidth, isExternal,
            isOvercut, overCut,
            smallHoleDia, smallHoleSpeed,
            diameter, invalidLeads):
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
    valid, diameter = Conv.conv_is_float(diameter)
    if not valid and diameter:
        msg0 = _('DIAMETER')
        error += f'{msg0} {msg1}\n\n'
    valid, overCut = Conv.conv_is_float(overCut)
    if not valid and overCut:
        msg0 = _('OC LENGTH')
        error += f'{msg0} {msg1}\n\n'
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Invalid Kerf Width entry in material')
        error += f'{msg}\n\n'
    if error:
        return error
    if diameter == 0:
        msg = _('DIAMETER cannot be zero')
        error += f'{msg}\n\n'
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
                    outNgc.write(f'{l}\n')
            else:
                outNgc.write(f'\n{preAmble} (preamble)\n')
            break
        elif '(postamble)' in line:
            break
        elif 'm2' in line.lower() or 'm30' in line.lower():
            continue
        outNgc.write(line)
    outTmp.write('\n(conversational circle)\n')
    outTmp.write(f';using material #{matNumber}: {matName}\n')
    outTmp.write(f'M190 P{matNumber}\n')
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
        outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
        outTmp.write('m3 $0 s1\n')
        if sHole:
            outTmp.write(f'M67 E3 Q{smallHoleSpeed} (reduce feed rate to 60%)\n')
        if (sHole and not isExternal) or invalidLeads == 2:
            outTmp.write(f'g1 x{xS:.6f} y{yS:.6f}\n')
        else:
            outTmp.write(f'g3 x{xS:.6f} y{yS:.6f} i{xlcenter - xlStart:.6f} j{ylcenter - ylStart:.6f}\n')
    else:
        outTmp.write(f'g0 x{xS:.6f} y{yS:.6f}\n')
        outTmp.write('m3 $0 s1\n')
        if sHole:
            outTmp.write(f'M67 E3 Q{smallHoleSpeed} (reduce feed rate to 60%)\n')
    if isExternal:
        outTmp.write(f'g2 x{xS:.6f} y{yS:.6f} i{ijOffset:.6f} j{ijOffset:.6f}\n')
    else:
        outTmp.write(f'g3 x{xS:.6f} y{yS:.6f} i{ijOffset:.6f} j{ijOffset:.6f}\n')
    if leadOutOffset and not isOvercut and not (not isExternal and sHole) and not invalidLeads:
            if isExternal:
                dir = [left, up]
            else:
                dir = [right, down]
            xlcenter = xS + (leadOutOffset * math.cos(angle + dir[0]))
            ylcenter = yS + (leadOutOffset * math.sin(angle + dir[0]))
            xlEnd = xlcenter + (leadOutOffset * math.cos(angle + dir[1]))
            ylEnd = ylcenter + (leadOutOffset * math.sin(angle + dir[1]))
            outTmp.write(f'g3 x{xlEnd:.6f} y{ylEnd:.6f} i{xlcenter - xS:.6f} j{ylcenter - yS:.6f}\n')
    torch = True
    if isOvercut and sHole and not isExternal:
        torch = False
        outTmp.write('m62 p3 (disable torch)\n')
        centerX = xS + ijOffset
        centerY = yS + ijOffset
        cosA = math.cos(overCut / radius)
        sinA = math.sin(overCut / radius)
        cosB = ((xS - centerX) / radius)
        sinB = ((yS - centerY) / radius)
        endX = centerX + radius * ((cosB * cosA) - (sinB * sinA))
        endY = centerY + radius * ((sinB * cosA) + (cosB * sinA))
        outTmp.write(f'g3 x{endX:.6f} y{endY:.6f} i{ijOffset:.6f} j{ijOffset:.6f}\n')
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
