'''
slot.py

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
            length, width, angle):
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
    valid, length = Conv.conv_is_float(length)
    if not valid and length:
        msg0 = _('LENGTH')
        error += f'{msg0} {msg1}\n\n'
    valid, width = Conv.conv_is_float(width)
    if not valid and width:
        msg0 = _('WIDTH')
        error += f'{msg0} {msg1}\n\n'
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += f'{msg0} {msg1}\n\n'
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Invalid Kerf Width entry in material')
        error += f'{msg}\n\n'
    if error:
        return error
    if length == 0:
        msg = _('LENGTH cannot be zero')
        error += f'{msg}\n\n'
    if width == 0:
        msg = _('WIDTH cannot be zero')
        error += f'{msg}\n\n'
    if width > length:
        msg = _('WIDTH cannot be greater than LENGTH')
        error += f'{msg}\n\n'
    if error:
        return error
    if isExternal:
        length += kerfWidth
        width += kerfWidth
    else:
        length -= kerfWidth
        width -= kerfWidth
    angle = math.radians(angle)
    leadInOffset = math.sin(math.radians(45)) * leadinLength
    leadOutOffset = math.sin(math.radians(45)) * leadoutLength
    radius = width / 2
    blLength = math.sqrt((length / 2) ** 2 + width ** 2)
    blAngle = math.atan(width / (length / 2))
    length = length - width
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    if isCenter:
        xS = xOffset + (width / 2) * math.cos(angle + up)
        yS = yOffset + (width / 2) * math.sin(angle + up)
    else:
        xS = xOffset + blLength * math.cos(angle + right + blAngle)
        yS = yOffset + blLength * math.sin(angle + right + blAngle)
    if isExternal:
        dir = [up, left, right]
    else:
        dir = [down, right, left]
    outTmp = open(fTmp, 'w')
    outNgc = open(fNgc, 'w')
    inWiz = open(fNgcBkp, 'r')
    for line in inWiz:
        line = line.strip()
        if line and line[0] not in ';':
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
            elif 'M2' in line.upper() or 'M02' in line.upper() or 'M30' in line.upper():
                continue
        outNgc.write(f"{line}\n")
    outTmp.write('\n(conversational slot)\n')
    outTmp.write(f';using material #{matNumber}: {matName}\n')
    outTmp.write(f'M190 P{matNumber}\n')
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('F#<_hal[plasmac.cut-feed-rate]>\n')
    if leadInOffset > 0:
        xlCentre = xS + (leadInOffset * math.cos(angle + dir[0]))
        ylCentre = yS + (leadInOffset * math.sin(angle + dir[0]))
        xlStart = xlCentre + (leadInOffset * math.cos(angle + dir[1]))
        ylStart = ylCentre + (leadInOffset * math.sin(angle + dir[1]))
        outTmp.write(f'G00 X{xlStart:.6f} Y{ylStart:.6f}\n')
        outTmp.write('M03 $0 S1\n')
        outTmp.write(f'G03 X{xS:.6f} Y{yS:.6f} I{xlCentre - xlStart:.6f} J{ylCentre - ylStart:.6f}\n')
    else:
        outTmp.write(f'G00 X{xS:.6f} Y{yS:.6f}\n')
        outTmp.write('M03 $0 S1\n')
    x1 = xS + (length / 2) * math.cos(angle + dir[2])
    y1 = yS + (length / 2) * math.sin(angle + dir[2])
    outTmp.write(f'G01 X{x1:.6f} Y{y1:.6f}\n')
    if isExternal:
        xrCentre = x1 + (radius * math.cos(angle + down))
        yrCentre = y1 + (radius * math.sin(angle + down))
        xrEnd = xrCentre + (radius * math.cos(angle + down))
        yrEnd = yrCentre + (radius * math.sin(angle + down))
        outTmp.write(f'G02 X{xrEnd:.6f} Y{yrEnd:.6f} I{xrCentre - x1:.6f} J{yrCentre - y1:.6f}\n')
        x2 = xrEnd + length * math.cos(angle + left)
        y2 = yrEnd + length * math.sin(angle + left)
        outTmp.write(f'G01 X{x2:.6f} Y{y2:.6f}\n')
        xrCentre = x2 + (radius * math.cos(angle + up))
        yrCentre = y2 + (radius * math.sin(angle + up))
        xrEnd = xrCentre + (radius * math.cos(angle + up))
        yrEnd = yrCentre + (radius * math.sin(angle + up))
        outTmp.write(f'G02 X{xrEnd:.6f} Y{yrEnd:.6f} I{xrCentre - x2:.6f} J{yrCentre - y2:.6f}\n')
    else:
        xrCentre = x1 + (radius * math.cos(angle + down))
        yrCentre = y1 + (radius * math.sin(angle + down))
        xrEnd = xrCentre + (radius * math.cos(angle + down))
        yrEnd = yrCentre + (radius * math.sin(angle + down))
        outTmp.write(f'G03 X{xrEnd:.6f} Y{yrEnd:.6f} I{xrCentre - x1:.6f} J{yrCentre - y1:.6f}\n')
        x2 = xrEnd + length * math.cos(angle + right)
        y2 = yrEnd + length * math.sin(angle + right)
        outTmp.write(f'G01 X{x2:.6f} Y{y2:.6f}\n')
        xrCentre = x2 + (radius * math.cos(angle + up))
        yrCentre = y2 + (radius * math.sin(angle + up))
        xrEnd = xrCentre + (radius * math.cos(angle + up))
        yrEnd = yrCentre + (radius * math.sin(angle + up))
        outTmp.write(f'G03 X{xrEnd:.6f} Y{yrEnd:.6f} I{xrCentre - x2:.6f} J{yrCentre - y2:.6f}\n')
    outTmp.write(f'G01 X{xS:.6f} Y{yS:.6f}\n')
    if leadOutOffset > 0:
        if isExternal:
            dir = [up, right]
        else:
            dir = [down, left]
        xlCentre = xS + (leadOutOffset * math.cos(angle + dir[0]))
        ylCentre = yS + (leadOutOffset * math.sin(angle + dir[0]))
        xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[1]))
        ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[1]))
        outTmp.write(f'G03 X{xlEnd:.6f} Y{ylEnd:.6f} I{xlCentre - xS:.6f} J{ylCentre - yS:.6f}\n')
    outTmp.write('M05 $0\n')
    outTmp.close()
    outTmp = open(fTmp, 'r')
    for line in outTmp:
        outNgc.write(line)
    outTmp.close()
    if('\\n') in postAmble:
        outNgc.write('\n(postamble)\n')
        for l in postAmble.split('\\n'):
            outNgc.write(f'{l}\n')
    else:
        outNgc.write(f'\n{postAmble} (postamble)\n')
    outNgc.write('M02\n')
    outNgc.close()
    return False
