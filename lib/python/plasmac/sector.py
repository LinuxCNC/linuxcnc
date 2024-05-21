'''
sector.py

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
            localeDir = os.path.join('{f.split("/lib")[0]}', 'share', 'locale')
        break
gettext.install("linuxcnc", localedir=localeDir)


# Conv is the upstream calling module
def preview(Conv, fTmp, fNgc, fNgcBkp,
            matNumber, matName,
            preAmble, postAmble,
            leadinLength, leadoutLength,
            xOffset, yOffset,
            kerfWidth, isExternal,
            radius, sAngle, angle):
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
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        msg0 = _('RADIUS')
        error += f'{msg0} {msg1}\n\n'
    valid, sAngle = Conv.conv_is_float(sAngle)
    if not valid and sAngle:
        msg0 = _('SEC ANGLE')
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
    if radius == 0:
        msg = _('RADIUS cannot be zero')
        error += f'{msg}\n\n'
    if sAngle == 0:
        msg = _('SEC ANGLE cannot be zero')
        error += f'{msg}\n\n'
    if error:
        return error
    sAngle = math.radians(sAngle)
    angle = math.radians(angle)
    leadInOffset = math.sin(math.radians(45)) * leadinLength
    leadOutOffset = math.sin(math.radians(45)) * leadoutLength
    # get origin
    xO = xOffset
    yO = yOffset
    # get bottom point
    xB = xO + radius * math.cos(angle)
    yB = yO + radius * math.sin(angle)
    # get offset origin
    x0, y0, rOffset = get_offset_coordinates([xB, yB], [xO, yO], sAngle, kerfWidth, isExternal)
    xO = x0 + (xO - x0) * 2
    yO = y0 + (yO - y0) * 2
    # get offset bottom point
    xB = xO + radius * math.cos(angle)
    yB = yO + radius * math.sin(angle)
    xO, yO, rOffset = get_offset_coordinates([xB, yB], [xO, yO], sAngle, kerfWidth, isExternal)
    # get new radius
    if isExternal:
        radius += rOffset + kerfWidth / 2
    else:
        radius -= rOffset + kerfWidth / 2
    # get leadin/leadout point
    xS = xO + (radius * 0.75) * math.cos(angle)
    yS = yO + (radius * 0.75) * math.sin(angle)
    # get new bottom point
    xB = xO + radius * math.cos(angle)
    yB = yO + radius * math.sin(angle)
    # get new top point
    xT = xO + radius * math.cos(angle + sAngle)
    yT = yO + radius * math.sin(angle + sAngle)
    # get directions
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    if isExternal:
        dir = [down, right, left, up]
    else:
        dir = [up, left, right, down]
        # get leadin and leadout points
    xIC = xS + (leadInOffset * math.cos(angle + dir[0]))
    yIC = yS + (leadInOffset * math.sin(angle + dir[0]))
    xIS = xIC + (leadInOffset * math.cos(angle + dir[1]))
    yIS = yIC + (leadInOffset * math.sin(angle + dir[1]))
    xOC = xS + (leadOutOffset * math.cos(angle + dir[0]))
    yOC = yS + (leadOutOffset * math.sin(angle + dir[0]))
    xOE = xOC + (leadOutOffset * math.cos(angle + dir[2]))
    yOE = yOC + (leadOutOffset * math.sin(angle + dir[2]))
    # setup files and write G-code
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
    outTmp.write('\n(conversational sector)\n')
    outTmp.write(f';using material #{matNumber}: {matName}\n')
    outTmp.write(f'M190 P{matNumber}\n')
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    outTmp.write(f'g0 x{xIS:.6f} y{yIS:.6f}\n')
    outTmp.write('m3 $0 s1\n')
    if leadInOffset:
        outTmp.write(f'g3 x{xS:.6f} y{yS:.6f} i{xIC - xIS:.6f} j{yIC - yIS:.6f}\n')
    if isExternal:
        outTmp.write(f'g1 x{xO:.6f} y{yO:.6f}\n')
        outTmp.write(f'g1 x{xT:.6f} y{yT:.6f}\n')
        outTmp.write(f'g2 x{xB:.6f} y{yB:.6f} i{xO - xT:.6f} j{yO - yT:.6f}\n')
    else:
        outTmp.write(f'g1 x{xB:.6f} y{yB:.6f}\n')
        outTmp.write(f'g3 x{xT:.6f} y{yT:.6f} i{xO - xB:.6f} j{yO - yB:.6f}\n')
        outTmp.write(f'g1 x{xO:.6f} y{yO:.6f}\n')
    outTmp.write(f'g1 x{xS:.6f} y{yS:.6f}\n')
    if leadOutOffset:
        outTmp.write(f'g3 x{xOE:.6f} y{yOE:.6f} i{xOC - xS:.6f} j{yOC - yS:.6f}\n')
    outTmp.write('m5 $0\n')
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


def get_offset_coordinates(fromPoint, thisPoint, angle, kerfWidth, isExternal):
    kOffset = kerfWidth / 2
    inAng = math.atan2(thisPoint[1] - fromPoint[1], thisPoint[0] - fromPoint[0])
    ang = math.radians(90) - (angle / 2)
    offset = math.tan(ang) * kOffset
    if isExternal:
        x = round(thisPoint[0] + offset * math.cos(inAng), 3)
        y = round(thisPoint[1] + offset * math.sin(inAng), 3)
        x = round(x + kOffset * math.cos(inAng + math.radians(90)), 3)
        y = round(y + kOffset * math.sin(inAng + math.radians(90)), 3)
    else:
        x = round(thisPoint[0] - offset * math.cos(inAng), 3)
        y = round(thisPoint[1] - offset * math.sin(inAng), 3)
        x = round(x + kOffset * math.cos(inAng + math.radians(-90)), 3)
        y = round(y + kOffset * math.sin(inAng + math.radians(-90)), 3)
    return x, y, offset
