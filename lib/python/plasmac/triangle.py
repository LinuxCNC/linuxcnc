'''
triangle.py

Copyright (C) 2020, 2021, 2022, 2023, 2024 Phillip A Carter
Copyright (C) 2020, 2021, 2022, 2023, 2024 Gregory D Carl

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
            xOffset, yOffset, \
            kerfWidth, isExternal, \
            angA, angB, angC, sideA, sideB, sideC, angle):
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
    if not valid and leadinLength:
        msg0 = _('LEAD IN')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, leadoutLength = Conv.conv_is_float(leadoutLength)
    if not valid and leadoutLength:
        msg0 = _('LEAD OUT')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, A = Conv.conv_is_float(angA)
    if not valid and angA:
        msg0 = _('A ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, B = Conv.conv_is_float(angB)
    if not valid and angB:
        msg0 = _('B ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, C = Conv.conv_is_float(angC)
    if not valid and angC:
        msg0 = _('C ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, a = Conv.conv_is_float(sideA)
    if not valid and sideA:
        msg0 = _('a LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, b = Conv.conv_is_float(sideB)
    if not valid and sideB:
        msg0 = _('b LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, c = Conv.conv_is_float(sideC)
    if not valid and sideC:
        msg0 = _('c LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Invalid Kerf Width entry in material')
        error += '{}\n\n'.format(msg)
    if a and b and c:
        if a + b <= c:
            msg = _('c must be less than a+b')
            error += '{}\n\n'.format(msg)
        if a + c <= b:
            msg = _('b must be less than a+c')
            error += '{}\n\n'.format(msg)
        if b + c <= a:
            msg = _('a must be less than b+c')
            error += '{}\n\n'.format(msg)
    if A <= 0 and isinstance(A, float):
        msg = _('A ANGLE cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if B <= 0 and isinstance(B, float):
        msg = _('B ANGLE cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if C <= 0 and isinstance(C, float):
        msg = _('C ANGLE cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if A >= 180 and isinstance(A, float):
        msg = _('A ANGLE cannot be 180 or more')
        error += '{}\n\n'.format(msg)
    if B >= 180 and isinstance(B, float):
        msg = _('B ANGLE cannot be 180 or more')
        error += '{}\n\n'.format(msg)
    if C >= 180 and isinstance(C, float):
        msg = _('C ANGLE cannot be 180 or more')
        error += '{}\n\n'.format(msg)
    if a <= 0 and isinstance(a, float):
        msg = _('a LENGTH cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if b <= 0 and isinstance(b, float):
        msg = _('b LENGTH cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if c <= 0 and isinstance(c, float):
        msg = _('c LENGTH cannot be zero or less')
        error += '{}\n\n'.format(msg)
    if A and B and C:
        if not a and not b and not c:
            msg = _('"a" or "b" or "c" are required')
            error += '{}\n\n'.format(msg)
        if A + B + C != 180:
            msg = _('"A" + "B" + "C" must equal 180')
            error += '{}\n\n'.format(msg)
    if error:
        return error
    angle = math.radians(angle)
    A = math.radians(A)
    B = math.radians(B)
    C = math.radians(C)
    leadInOffset = math.sin(math.radians(45)) * leadinLength
    leadOutOffset = math.sin(math.radians(45)) * leadoutLength
    if A and B and C:
        if a:
            b = a / math.sin(A) * math.sin(B)
            c = a / math.sin(A) * math.sin(C)
        elif b:
            a = b / math.sin(B) * math.sin(A)
            c = b / math.sin(B) * math.sin(C)
        elif c:
            a = c / math.sin(C) * math.sin(A)
            b = c / math.sin(C) * math.sin(B)
    elif a and b and c:
        A = math.acos((b ** 2 + c ** 2 - a ** 2) / (2 * b * c))
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
        C = math.acos((a ** 2 + b ** 2 - c ** 2) / (2 * a * b))
    elif a and b and C:
        c = math.sqrt((a ** 2 + b ** 2) - 2 * a * b * math.cos(C))
        A = math.acos((b ** 2 + c ** 2 - a ** 2) / (2 * b * c))
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
    elif a and B and c:
        b = math.sqrt((a ** 2 + c ** 2) - 2 * a * c * math.cos(B))
        A = math.acos((b ** 2 + c ** 2 - a ** 2) / (2 * b * c))
        C = math.acos((a ** 2 + b ** 2 - c ** 2) / (2 * a * b))
    elif A and b and c:
        a = math.sqrt((b ** 2 + c ** 2) - 2 * b * c * math.cos(A))
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
        C = math.acos((a ** 2 + b ** 2 - c ** 2) / (2 * a * b))
    else:
        msg0 = 'MINIMUM REQUIREMENTS:\n'\
               'In processing order are:\n\n'\
               '1: "A" + "B" + "C" + ("a" or "b" or "c")  \n\n'\
               '2: "a" + "b" + "c"\n\n'\
               '3: "a" + "b" + "C"\n\n'\
               '4: "a" + "B" + "c"\n\n'\
               '5: "A" + "b" + "c"\n'
        error += '{}\n\n'.format(msg0)
        return error
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    # get start point 
    BX = xOffset
    BY = yOffset
    CX = round(BX + a * math.cos(angle), 3)
    CY = round(BY + a * math.sin(angle), 3)
    Bx, By = get_offset_coordinates([CX,CY], [BX,BY], B, kerfWidth, isExternal)
    BX = Bx + (BX - Bx) * 2
    BY = By + (BY - By) * 2
    # get remaining points
    CX = round(BX + a * math.cos(angle), 3)
    CY = round(BY + a * math.sin(angle), 3)
    AX = round(BX + c * math.cos(angle + B), 3)
    AY = round(BY + c * math.sin(angle + B), 3)
    # get offset points
    Ax, Ay = get_offset_coordinates([BX,BY], [AX,AY], A, kerfWidth, isExternal)
    Bx, By = get_offset_coordinates([CX,CY], [BX,BY], B, kerfWidth, isExternal)
    Cx, Cy = get_offset_coordinates([AX,AY], [CX,CY], C, kerfWidth, isExternal)
    # get leadin/leadout point
    hypotLength = math.sqrt((Ax - Cx) ** 2 + (Ay - Cy) ** 2)
    if Ax < Cx:
        hypotAngle = left - math.atan((Ay - Cy) / (Cx - Ax))
    elif Ax > Cx:
        hypotAngle = right - math.atan((Ay - Cy) / (Cx - Ax))
    else:
        hypotAngle = up
    xS = Cx + (hypotLength / 2) * math.cos(hypotAngle)
    yS = Cy + (hypotLength / 2) * math.sin(hypotAngle)
    # set leadin direction
    if isExternal:
        if Ay >= By:
            dir = [up, right]
        else:
            dir = [down, left]
    else:
        if Ay >= By:
            dir = [down, left]
        else:
            dir = [up, right]
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
    outTmp.write('\n(conversational triangle)\n')
    outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
    outTmp.write('M190 P{}\n'.format(matNumber))
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    if leadInOffset > 0:
        xlCentre = xS + (leadInOffset * math.cos(hypotAngle - dir[0]))
        ylCentre = yS + (leadInOffset * math.sin(hypotAngle - dir[0]))
        xlStart = xlCentre + (leadInOffset * math.cos(hypotAngle - dir[1]))
        ylStart = ylCentre + (leadInOffset * math.sin(hypotAngle - dir[1]))
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
        outTmp.write('m3 $0 s1\n')
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
    else:
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
        outTmp.write('m3 $0 s1\n')
    if isExternal:
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Cx , Cy))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Bx , By))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Ax , Ay))
    else:
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Ax , Ay))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Bx , By))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(Cx , Cy))
    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    # set leadout direction
    if leadOutOffset > 0:
        if isExternal:
            if Ay >= By:
                dir = [up, left]
            else:
                dir = [down, right]
        else:
            if Ay >= By:
                dir = [down, right]
            else:
                dir = [up, left]
        xlCentre = xS + (leadOutOffset * math.cos(hypotAngle - dir[0]))
        ylCentre = yS + (leadOutOffset * math.sin(hypotAngle - dir[0]))
        xlEnd = xlCentre + (leadOutOffset * math.cos(hypotAngle - dir[1]))
        ylEnd = ylCentre + (leadOutOffset * math.sin(hypotAngle - dir[1]))
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
    outTmp.write('m5 $0\n')
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
    return x, y
