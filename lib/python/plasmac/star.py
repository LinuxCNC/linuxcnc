'''
star.py

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
            points, extDia, intDia, angle):
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
    valid, points = Conv.conv_is_int(points)
    if not valid and points:
        msg0 = _('POINTS')
        error += f'{msg0} {msg1}\n\n'
    valid, extDia = Conv.conv_is_float(extDia)
    if not valid and extDia:
        msg0 = _('OUTER DIA')
        error += f'{msg0} {msg1}\n\n'
    valid, intDia = Conv.conv_is_float(intDia)
    if not valid and intDia:
        msg0 = _('INNER DIA')
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
    if points < 2:
        msg = _('More than two POINTS required')
        error += f'{msg}\n\n'
    if extDia == 0:
        msg = _('OUTER DIA cannot be zero')
        error += f'{msg}\n\n'
    if intDia == 0:
        msg = _('INNER DIA cannot be zero')
        error += f'{msg}\n\n'
    if intDia >= extDia:
        msg = _('INNER DIA must be less than OUTER DIA')
        error += f'{msg}\n\n'
    if error:
        return error
    extRadius = extDia / 2
    intRadius = intDia / 2
    angle = math.radians(angle)
    # get start pop
    if isCenter:
        xC = xOffset
        yC = yOffset
    else:
        xC = xOffset + extRadius * math.cos(math.radians(0))
        yC = yOffset + extRadius * math.sin(math.radians(90))
    # get all points
    pList = get_points(points, angle, xC, yC, extRadius, intRadius)
    # get external offset required
    extOffset = get_offset([pList[3][0], pList[3][1]], [pList[2][0], pList[2][1]], [pList[1][0], pList[1][1]], kerfWidth)
    # get internal offset required
    intOffset = get_offset([pList[0][0], pList[0][1]], [pList[1][0], pList[1][1]], [pList[2][0], pList[2][1]], kerfWidth)
    # get new points
    move = 0 if isCenter else extOffset
    if isExternal:
        pList = get_points(points, angle, xC + move, yC + move, extRadius + extOffset, intRadius + intOffset)
    else:
        pList = get_points(points, angle, xC - move, yC - move, extRadius - extOffset, intRadius - intOffset)
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
    outTmp.write(f'\n(conversational star {points})\n')
    outTmp.write(f';using material #{matNumber}: {matName}\n')
    outTmp.write(f'M190 P{matNumber}\n')
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    if isExternal:
        if leadinLength > 0:
            lAngle = math.atan2(pList[0][1] - pList[-1][1],
                                pList[0][0] - pList[-1][0])
            xlStart = pList[0][0] + leadinLength * math.cos(lAngle)
            ylStart = pList[0][1] + leadinLength * math.sin(lAngle)
            outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write(f'g1 x{pList[0][0]} y{pList[0][1]}\n')
        else:
            outTmp.write(f'g0 x{pList[0][0]} y{pList[0][1]}\n')
            outTmp.write('m3 $0 s1\n')
        for i in range(points * 2, 0, -1):
            outTmp.write(f'g1 x{pList[i - 1][0]} y{pList[i - 1][1]}\n')
        if leadoutLength > 0:
            lAngle = math.atan2(pList[0][1] - pList[1][1],
                                pList[0][0] - pList[1][0])
            xlEnd = pList[0][0] + leadoutLength * math.cos(lAngle)
            ylEnd = pList[0][1] + leadoutLength * math.sin(lAngle)
            outTmp.write(f'g1 x{xlEnd:.6f} y{ylEnd:.6f}\n')
    else:
        if leadinLength > 0:
            lAngle = math.atan2(pList[-1][1] - pList[0][1],
                                pList[-1][0] - pList[0][0])
            xlStart = pList[points * 2 - 1][0] + leadinLength * math.cos(lAngle)
            ylStart = pList[points * 2 - 1][1] + leadinLength * math.sin(lAngle)
            outTmp.write(f'g0 x{xlStart:.6f} y{ylStart:.6f}\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write(f'g1 x{pList[points * 2 - 1][0]} y{pList[points * 2 - 1][1]}\n')
            outTmp.write(f'g1 x{pList[0][0]} y{pList[0][1]}\n')
        else:
            outTmp.write(f'g0 x{pList[points * 2 - 1][0]} y{pList[points * 2 - 1][1]}\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write(f'g1 x{pList[0][0]} y{pList[0][1]}\n')
        for i in range(1, points * 2):
            outTmp.write(f'g1 x{pList[i][0]} y{pList[i][1]}\n')
        if leadoutLength > 0:
            lAngle = math.atan2(pList[-1][1] - pList[-2][1],
                                pList[-1][0] - pList[-2][0])
            xlEnd = pList[-1][0] + leadoutLength * math.cos(lAngle)
            ylEnd = pList[-1][1] + leadoutLength * math.sin(lAngle)
            outTmp.write(f'g1 x{xlEnd:.6f} y{ylEnd:.6f}\n')
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


def get_points(points, angle, xC, yC, extRadius, intRadius):
    pList = []
    for i in range(points * 2):
        pAngle = angle + 2 * math.pi * i / (points * 2)
        if i % 2 == 0:
            x = xC + extRadius * math.cos(pAngle)
            y = yC + extRadius * math.sin(pAngle)
        else:
            x = xC + intRadius * math.cos(pAngle)
            y = yC + intRadius * math.sin(pAngle)
        pList.append([round(x, 3), round(y, 3)])
    return pList


def get_offset(A, B, C, kerfWidth):
    # Ax, Ay = A[0] - B[0], A[1] - B[1]
    # Cx, Cy = C[0] - B[0], C[1] - B[1]
    # a = math.atan2(Ay, Ax)
    # c = math.atan2(Cy, Cx)
    a = math.atan2(A[1] - B[1], A[0] - B[0])
    c = math.atan2(C[1] - B[1], C[0] - B[0])
    if a < 0:
        a += math.pi * 2
    if c < 0:
        c += math.pi * 2
    ang = (math.pi * 2 + c - a) if a > c else (c - a)
    ang = math.radians(90) - (ang / 2)
    adj = (kerfWidth / 2) / math.sin(ang)
    ofs = math.tan(ang) * adj
    return ofs
