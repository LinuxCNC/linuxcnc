'''
ellipse.py

Copyright (C) 2021 - 2024 Phillip A Carter
Copyright (C) 2021 - 2024 Gregory D Carl

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
            width, height, angle, unitsPerMm):
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
    valid, width = Conv.conv_is_float(width)
    if not valid and width:
        msg0 = _('WIDTH')
        error += f'{msg0} {msg1}\n\n'
    valid, height = Conv.conv_is_float(height)
    if not valid and height:
        msg0 = _('HEIGHT')
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
    if width == 0:
        msg = _('WIDTH cannot be zero')
        error += f'{msg}\n\n'
    if height == 0:
        msg = _('HEIGHT cannot be zero')
        error += f'{msg}\n\n'
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
    outTmp.write('\n(conversational ellipse)\n')
    outTmp.write(f';using material #{matNumber}: {matName}\n')
    outTmp.write(f'M190 P{matNumber}\n')
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('F#<_hal[plasmac.cut-feed-rate]>\n')
    # get the angle of the first segment
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
        outTmp.write(f'G00 X{xlStart:.6f} Y{ylStart:.6f}\n')
        outTmp.write('M03 $0 S1\n')
        outTmp.write(f'G03 X{X[start]:.6f} Y{Y[start]:.6f} I{xlcenter - xlStart:.6f} J{ylcenter - ylStart:.6f}\n')
    else:
        outTmp.write(f'G00 X{X[start]:.6f} Y{Y[start]:.6f}\n')
        outTmp.write('M03 $0 S01\n')
    # write the ellipse points
    if isExternal:
        for point in range(start, -1, -1):
            outTmp.write(f'G01 X{X[point]:.6f} Y{Y[point]:.6f}\n')
        for point in range(points - 1, start - 1, -1):
            outTmp.write(f'G01 X{X[point]:.6f} Y{Y[point]:.6f}\n')
    else:
        for point in range(start, points, 1):
            outTmp.write(f'G01 X{X[point]:.6f} Y{Y[point]:.6f}\n')
        for point in range(0, start + 1, 1):
            outTmp.write(f'G01 X{X[point]:.6f} Y{Y[point]:.6f}\n')
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
        outTmp.write(f'G03 X{xlEnd:.6f} Y{ylEnd:.6f} I{xlcenter - X[start]:.6f} J{ylcenter - Y[start]:.6f}\n')
    # finish off and close files
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
