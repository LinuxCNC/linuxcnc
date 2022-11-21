'''
gusset.py

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
            xOffset, yOffset, \
            kerfWidth, isExternal, \
            width, height, angle, radius, rButton):
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
    valid, width = Conv.conv_is_float(width)
    if not valid and width:
        msg0 = _('WIDTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, height = Conv.conv_is_float(height)
    if not valid and height:
        msg0 = _('HEIGHT')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        error += '{} {}\n\n'.format(rButton, msg1)
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _(' ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Invalid Kerf Width entry in material')
        error += '{}\n\n'.format(msg)
    if error:
        return error
    if width == 0:
        msg = _('WIDTH cannot be zero')
        error += '{}\n\n'.format(msg)
    if height == 0:
        msg = _('HEIGHT cannot be zero')
        error += '{}\n\n'.format(msg)
    if angle == 0:
        msg = _('ANGLE cannot be zero')
        error += '{}\n\n'.format(msg)
    if radius > width: # this needs a real calculation in the future *********
        msg = _('must be less than WIDTH')
        error += '{} {}\n\n'.format(rButton, msg)
    if radius > height: # this needs a real calculation in the future *********
        msg = _('must be less than HEIGHT')
        error += '{} {}\n\n'.format(rButton, msg)
    if error:
        return error
    angle = math.radians(angle)
    leadInOffset = math.sin(math.radians(45)) * float(leadinLength)
    leadOutOffset = math.sin(math.radians(45)) * float(leadoutLength)
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    # get original points
    x0 = xOffset
    y0 = yOffset
    x1 = x0 + width * math.cos(right)
    y1 = y0 + width * math.sin(right)
    # get offset start point
    x0n, y0n, = get_offset_coordinates([x1, y1], [x0, y0], angle, kerfWidth, isExternal)
    # get new start poin
    x0 = x0n + (x0 - x0n) * 2
    y0 = y0n + (y0 - y0n) * 2
    # get new points
    x1 = x0 + width * math.cos(right)
    y1 = y0 + width * math.sin(right)
    x2 = x0 + height * math.cos(angle)
    y2 = y0 + height * math.sin(angle)
    ang1 = get_angle([x2, y2], [x1, y1], [x0, y0])
    ang2 = get_angle([x0, y0], [x2, y2], [x1, y1])
    # get new offset points
    x0, y0, = get_offset_coordinates([x1, y1], [x0, y0], angle, kerfWidth, isExternal)
    x1, y1, = get_offset_coordinates([x2, y2], [x1, y1], ang1, kerfWidth, isExternal)
    x2, y2, = get_offset_coordinates([x0, y0], [x2, y2], ang2, kerfWidth, isExternal)
    # get leadin point
    hypotLength = math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)
    if x2 <= x1:
        hypotAngle = left - math.atan((y2 - y1) / (x1 - x2))
    else:
        hypotAngle = right - math.atan((y2 - y1) / (x1 - x2))
    xS = x1 + (hypotLength / 2) * math.cos(hypotAngle)
    yS = y1 + (hypotLength / 2) * math.sin(hypotAngle)
    if isExternal:
        if y2 >= y0:
            dir = [up, right]
        else:
            dir = [down, left]
    else:
        if y2 >= y0:
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
    outTmp.write('\n(conversational gusset)\n')
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
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
        if radius > 0:
            x3 = x0 + radius
            y3 = y0
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
            x4 = x0 + radius * math.cos(angle)
            y4 = y0 + radius * math.sin(angle)
            if rButton.startswith(_('RADIUS')):
                if y2 >= y0:
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
                else:
                    outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x4, y4 , x0 - x3, y0 - y3))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
        else:
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    else:
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
        if radius > 0:
            x3 = x0 + radius
            y3 = y0
            x4 = x0 + radius * math.cos(angle)
            y4 = y0 + radius * math.sin(angle)
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
            if rButton.startswith(_('RADIUS')):
                if y2 >= y0:
                    outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
                else:
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(x3, y3 , x0 - x4, y0 - y4))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
        else:
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x0, y0))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    if leadOutOffset > 0:
        if isExternal:
            if y2 >= y0:
                dir = [up, left]
            else:
                dir = [down, right]
        else:
            if y2 >= y0:
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
    return None

def get_offset_coordinates(fromPoint, thisPoint, angle, kerfWidth, isExternal):
    kOffset = kerfWidth / 2
    inAng = math.atan2(thisPoint[1] - fromPoint[1], thisPoint[0] - fromPoint[0])
    ang = math.radians(90) - (angle / 2)
    offset = math.tan(ang) * kOffset
    if isExternal:
        x = thisPoint[0] + offset * math.cos(inAng)
        y = thisPoint[1] + offset * math.sin(inAng)
        x = x + kOffset * math.cos(inAng + math.radians(90))
        y = y + kOffset * math.sin(inAng + math.radians(90))
    else:
        x = thisPoint[0] - offset * math.cos(inAng)
        y = thisPoint[1] - offset * math.sin(inAng)
        x = x + kOffset * math.cos(inAng + math.radians(-90))
        y = y + kOffset * math.sin(inAng + math.radians(-90))
    return x, y

def get_angle(fromPoint, thisPoint, nextPoint):
    a = math.atan2(fromPoint[1] - thisPoint[1], fromPoint[0] - thisPoint[0])
    c = math.atan2(nextPoint[1] - thisPoint[1], nextPoint[0] - thisPoint[0])
    if a < 0: a += math.pi * 2
    if c < 0: c += math.pi * 2
    ang = (math.pi * 2 + c - a) if a > c else (c - a)
    return ang
