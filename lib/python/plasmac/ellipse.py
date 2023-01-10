'''
ellipse.py

Copyright (C) 2021, 2022  Phillip A Carter
Copyright (C) 2021, 2022  Gregory D Carl

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
import numpy
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
            width, height, angle, unitsPerMm):
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
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
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
    if error:
        return error
    angle = numpy.radians(angle)
    if isExternal:
        width += kerfWidth
        height += kerfWidth
    else:
        width -= kerfWidth
        height -= kerfWidth
    if isCenter:
        centerX = xOffset
        centerY = yOffset
    else:
        centerX = xOffset + width / 2
        centerY = yOffset + height / 2
    leadInOffset = numpy.sin(45) * leadinLength
    leadOutOffset = numpy.sin(45) * leadoutLength
    # approx perimeter in mm
    perim = (numpy.pi * (3 * (width + height) - numpy.sqrt((3 * width + height) * (width + 3 * height)))) * unitsPerMm
    # number of points is 360 unless perimeter is > 360mm then have a segment length of 1mm
    points = 360 if perim <= 360 else int(perim)
    mult = float(360) / points
    # start/end point of cut
    start = int(points * 0.625)
    # create the ellipse points in an array
    array = numpy.linspace(0, points, points)
    X = centerX + width / 2 * numpy.cos(numpy.radians(array * mult))
    Y = centerY + height / 2 * numpy.sin(numpy.radians(array * mult))
    # rotate the ellipse if required
    for point in range(points):
        x = X[point]
        y = Y[point]
        X[point] = x * numpy.cos(angle) - y * numpy.sin(angle)
        Y[point] = x * numpy.sin(angle) + y * numpy.cos(angle)
    # initialize files
    outTmp = open(fTmp, 'w')
    outNgc = open(fNgc, 'w')
    inWiz = open(fNgcBkp, 'r')
    # begin writing the gcode
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
    outTmp.write('\n(conversational ellipse)\n')
    outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
    outTmp.write('M190 P{}\n'.format(matNumber))
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    # get the angle of the first segment
    delta_x = -1 - 0
    delta_y = 1 - 0
    theta_radians = numpy.arctan2(delta_y, delta_x)
    if isExternal:
        dX = X[start - 1] - X[start]
        dY = Y[start - 1] - Y[start]
    else:
        dX = X[start] - X[start + 1]
        dY = Y[start] - Y[start + 1]
    segAngle = numpy.arctan2(dY, dX)
    # set direction constants
    right = numpy.radians(0)
    up = numpy.radians(90)
    left = numpy.radians(180)
    down = numpy.radians(270)
    # leadin points if required
    if leadInOffset > 0:
        if isExternal:
            dir = [up, left]
        else:
            dir = [down, right]
        xlcenter = X[start] + (leadInOffset * numpy.cos(segAngle + dir[0]))
        ylcenter = Y[start] + (leadInOffset * numpy.sin(segAngle + dir[0]))
        xlStart = xlcenter + (leadInOffset * numpy.cos(segAngle + dir[1]))
        ylStart = ylcenter + (leadInOffset * numpy.sin(segAngle + dir[1]))
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
        outTmp.write('m3 $0 s1\n')
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(X[start], Y[start], xlcenter - xlStart, ylcenter - ylStart))
    else:
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(X[start], Y[start]))
        outTmp.write('m3 $0 s1\n')
    # write the ellipse points
    if isExternal:
        for point in range(start, -1, -1):
            outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
        for point in range(points - 1, start - 1, -1):
            outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
    else:
        for point in range(start, points, 1):
            outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
        for point in range(0, start + 1, 1):
            outTmp.write('g1 x{0:.6f} y{1:.6f}\n'.format(X[point], Y[point]))
    # leadout points if required
    if leadOutOffset:
        if isExternal:
            dir = [up, right]
        else:
            dir = [down, left]
        xlcenter = X[start] + (leadOutOffset * numpy.cos(segAngle + dir[0]))
        ylcenter = Y[start] + (leadOutOffset * numpy.sin(segAngle + dir[0]))
        xlEnd = xlcenter + (leadOutOffset * numpy.cos(segAngle + dir[1]))
        ylEnd = ylcenter + (leadOutOffset * numpy.sin(segAngle + dir[1]))
        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd, xlcenter - X[start], ylcenter - Y[start]))
    # finish off and close files
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
