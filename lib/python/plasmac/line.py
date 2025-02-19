'''
line.py

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
def do_line_point_to_point(Conv, xStart, yStart, xEnd, yEnd):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += f'{msg0} {msg1}\n\n'
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += f'{msg0} {msg1}\n\n'
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += f'{msg0} {msg1}\n\n'
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return True, error
    if xEnd == xStart and yEnd == yStart:
        msg = _('Line length would be zero')
        error += f'{msg}\n\n'
    if error:
        return True, error
    code = f'g1 x{float(xEnd):.6f} y{float(yEnd):.6f}\n'
    return False, xEnd, yEnd, code


def do_line_by_angle(Conv, xStart, yStart, length, angle):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += f'{msg0} {msg1}\n\n'
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += f'{msg0} {msg1}\n\n'
    valid, length = Conv.conv_is_float(length)
    if not valid and length:
        msg0 = _('LENGTH')
        error += f'{msg0} {msg1}\n\n'
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return True, error
    if length == 0:
        msg = _('LENGTH must be greater than zero')
        error += f'{msg}\n\n'
    if error:
        return True, error
    ang = math.radians(angle)
    x = xStart + (length * math.cos(ang))
    y = yStart + (length * math.sin(ang))
    code = f'g1 x{x:.6f} y{y:.6f}\n'
    return False, x, y, code


def do_arc_3_points(Conv, xStart, yStart, xNext, yNext, xEnd, yEnd):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += f'{msg0} {msg1}\n\n'
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += f'{msg0} {msg1}\n\n'
    valid, xNext = Conv.conv_is_float(xNext)
    if not valid and xNext:
        msg0 = _('X NEXT')
        error += f'{msg0} {msg1}\n\n'
    valid, yNext = Conv.conv_is_float(yNext)
    if not valid and yNext:
        msg0 = _('Y NEXT')
        error += f'{msg0} {msg1}\n\n'
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += f'{msg0} {msg1}\n\n'
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return True, error
    if xNext == xEnd == xStart:
        msg = _('At least one X value must be different')
        error += f'{msg}\n\n'
    if yNext == yEnd == yStart:
        msg = _('At least one Y value must be different')
        error += f'{msg}\n\n'
    if xNext == yNext and xEnd == yEnd and xStart == yStart:
        msg = _('Cannot create a arc, check all entries')
        error += f'{msg}\n\n'
    if error:
        return True, error
    try:
        A = numpy.array([xStart, yStart, 0.0])
        B = numpy.array([xNext, yNext, 0.0])
        C = numpy.array([xEnd, yEnd, 0.0])
        a = numpy.linalg.norm(C - B)
        b = numpy.linalg.norm(C - A)
        c = numpy.linalg.norm(B - A)
        b1 = a*a * (b*b + c*c - a*a)
        b2 = b*b * (a*a + c*c - b*b)
        b3 = c*c * (a*a + b*b - c*c)
        p = numpy.column_stack((A, B, C)).dot(numpy.hstack((b1, b2, b3)))
        p /= b1 + b2 + b3
        G = '3' if (xNext - xStart) * (yEnd - yStart) - (yNext - yStart) * (xEnd - xStart) > 0 else '2'
        if numpy.isnan(p[0] - xStart) or numpy.isnan(p[1] - yStart):
            msg0 = _('Unknown calculation error')
            msg1 = _('Ensure entries are correct')
            error = f'{msg0},\n\n{msg1}'
            return True, error
        code = f'g{G} x{xEnd:.6f} y{yEnd:.6f} i{p[0] - xStart:.6f} j{p[1] - yStart:.6f}\n'
        return False, xEnd, yEnd, code
    except Exception as e:
        msg = _('SYSTEM ERROR')
        error = f'{msg}:\n\n{e}'
        return True, e


def do_arc_2_points_radius(Conv, xStart, yStart, xEnd, yEnd, radius, arcType):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += f'{msg0} {msg1}\n\n'
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += f'{msg0} {msg1}\n\n'
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += f'{msg0} {msg1}\n\n'
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += f'{msg0} {msg1}\n\n'
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        msg0 = _('RADIUS')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return True, error
    if radius == 0:
        msg = _('RADIUS cannot be zero')
        error += f'{msg}\n\n'
    if xEnd == xStart and yEnd == yStart:
        msg = _('Arc length would be zero')
        error += f'{msg}\n\n'
    if error:
        return True, error
    try:
        dir = math.radians(270) if radius > 0 else math.radians(90)
        height = math.sqrt((xEnd - xStart) ** 2 + (yEnd - yStart) ** 2) * 0.5
        if radius < height:
            return True, _('Radius must be at least half the length')
        length = math.sqrt((radius ** 2) - (height ** 2))
        angle = math.atan2((yEnd - yStart), (xEnd - xStart))
        xLineCentre = (xStart + xEnd) / 2
        yLineCentre = (yStart + yEnd) / 2
        xArcCentre = xLineCentre + length * math.cos(angle + dir)
        yArcCentre = yLineCentre + length * math.sin(angle + dir)
        code = (f'g{arcType} x{xEnd:.6f} y{yEnd:.6f} i{xArcCentre - xStart:.6f} j{yArcCentre - yStart:.6f}\n')
        return False, xEnd, yEnd, code
    except Exception as e:
        msg = _('SYSTEM ERROR')
        error = f'{msg}:\n\n{e}'
        return True, e


def do_arc_by_angle_radius(Conv, xStart, yStart, length, angle, radius, arcType):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += f'{msg0} {msg1}\n\n'
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += f'{msg0} {msg1}\n\n'
    valid, length = Conv.conv_is_float(length)
    if not valid and length:
        msg0 = _('LENGTH')
        error += f'{msg0} {msg1}\n\n'
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += f'{msg0} {msg1}\n\n'
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        msg0 = _('RADIUS')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return True, error
    if radius == 0:
        msg = _('RADIUS cannot be zero')
        error += f'{msg}\n\n'
    if length == 0:
        msg = _('LENGTH cannot be zero')
        error += f'{msg}\n\n'
    if error:
        return True, error
    ang = math.radians(angle)
    xEnd = xStart + (length * math.cos(ang))
    yEnd = yStart + (length * math.sin(ang))
    result = do_arc_2_points_radius(Conv, xStart, yStart, xEnd, yEnd, radius, arcType)
    return result


def first_segment(fTmp, fNgc, fNgcBkp, preAmble, lineType, xStart, yStart, matNumber, matName):
    with open(fTmp, 'w') as outTmp:
        with open(fNgc, 'w') as outNgc:
            with open(fNgcBkp, 'r') as inWiz:
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
        outTmp.write('\n(conversational line/arc)\n')
        outTmp.write(f';using material #{matNumber}: {matName}\n')
        outTmp.write(f'M190 P{matNumber}\n')
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        outTmp.write(f'g0 x{xStart:.6f} y{yStart:.6f}\n')
        outTmp.write('m3 $0 s1\n')


def next_segment(fTmp, fNgc):
    with open(fTmp, 'w') as outTmp:
        with open(fNgc, 'r') as inNgc:
            while(1):
                line = inNgc.readline()
                if not line or 'M5 $0' in line:
                    break
                else:
                    outTmp.write(line)
    open(fNgc, 'w')


def last_segment(fTmp, fNgc, code, postAmble):
    with open(fTmp, 'a') as outTmp:
        outTmp.write(code)
        outTmp.write('M5 $0\n')
    with open(fNgc, 'a') as outNgc:
        with open(fTmp, 'r') as inTmp:
            for line in inTmp:
                outNgc.write(line)
        if('\\n') in postAmble:
            outNgc.write('(postamble)\n')
            for l in postAmble.split('\\n'):
                outNgc.write(f'{l}\n')
        else:
            outNgc.write(f'\n{postAmble} (postamble)\n')
        outNgc.write('m2\n')
