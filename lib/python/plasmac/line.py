'''
line.py

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
import numpy
from shutil import copy as COPY
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
def do_line_point_to_point(Conv, xStart, yStart, xEnd, yEnd):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return True, error
    if xEnd == xStart and yEnd == yStart:
        msg = _('Line length would be zero')
        error += '{}\n\n'.format(msg)
    if error:
        return True, error
    code = 'g1 x{:.6f} y{:.6f}\n'.format(float(xEnd), float(yEnd))
    return False, xEnd, yEnd, code

def do_line_by_angle(Conv, xStart, yStart, length, angle):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, length = Conv.conv_is_float(length)
    if not valid and length:
        msg0 = _('LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return True, error
    if length == 0:
        msg = _('LENGTH must be greater than zero')
        error += '{}\n\n'.format(msg)
    if error:
        return True, error
    ang = math.radians(angle)
    x = xStart + (length * math.cos(ang))
    y = yStart + (length * math.sin(ang))
    code = 'g1 x{:.6f} y{:.6f}\n'.format(x, y)
    return False, x, y, code

def do_arc_3_points(Conv, xStart, yStart, xNext, yNext, xEnd, yEnd):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, xNext = Conv.conv_is_float(xNext)
    if not valid and xNext:
        msg0 = _('X NEXT')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yNext = Conv.conv_is_float(yNext)
    if not valid and yNext:
        msg0 = _('Y NEXT')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return True, error
    if xNext == xEnd == xStart:
        msg = _('At least one X value must be different')
        error += '{}\n\n'.format(msg)
    if yNext == yEnd == yStart:
        msg = _('At least one Y value must be different')
        error += '{}\n\n'.format(msg)
    if xNext == yNext and xEnd == yEnd and xStart == yStart:
        msg = _('Cannot create a arc, check all entries')
        error += '{}\n\n'.format(msg)
    if error:
        return True, error
    try:
        A = numpy.array([xStart, yStart, 0.0])
        B = numpy.array([xNext, yNext, 0.0])
        C = numpy.array([xEnd, yEnd, 0.0])
        a = numpy.linalg.norm(C - B)
        b = numpy.linalg.norm(C - A)
        c = numpy.linalg.norm(B - A)
        s = (a + b + c) / 2
        R = a*b*c / 4 / numpy.sqrt(s * (s - a) * (s - b) * (s - c))
        b1 = a*a * (b*b + c*c - a*a)
        b2 = b*b * (a*a + c*c - b*b)
        b3 = c*c * (a*a + b*b - c*c)
        p = numpy.column_stack((A, B, C)).dot(numpy.hstack((b1, b2, b3)))
        p /= b1 + b2 + b3
        G = '3' if (xNext - xStart) * (yEnd - yStart) - (yNext - yStart) * (xEnd - xStart) > 0 else '2'
        if numpy.isnan(p[0] - xStart) or numpy.isnan(p[1] - yStart):
            msg0 = _('Unknown calculation error')
            msg1 = _('Ensure entries are correct')
            error = '{},\n\n{}'.format(msg0, msg1)
            return True, error
        code = 'g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(G, xEnd, yEnd, p[0] - xStart, p[1] - yStart)
        return False, xEnd, yEnd, code
    except Exception as e:
        msg = _('SYSTEM ERROR')
        error = '{}:\n\n{}'.format(msg, e)
        return True, e

def do_arc_2_points_radius(Conv, xStart, yStart, xEnd, yEnd, radius, arcType):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, xEnd = Conv.conv_is_float(xEnd)
    if not valid and xEnd:
        msg0 = _('X END')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yEnd = Conv.conv_is_float(yEnd)
    if not valid and yEnd:
        msg0 = _('Y END')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        msg0 = _('RADIUS')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return True, error
    if radius == 0:
        msg = _('RADIUS cannot be zero')
        error += '{}\n\n'.format(msg)
    if xEnd == xStart and yEnd == yStart:
        msg = _('Arc length would be zero')
        error += '{}\n\n'.format(msg)
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
        code = ('g{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(arcType, xEnd, yEnd, xArcCentre - xStart, yArcCentre - yStart))
        return False, xEnd, yEnd, code
    except Exception as e:
        msg = _('SYSTEM ERROR')
        error = '{}:\n\n{}'.format(msg, e)
        return True, e

def do_arc_by_angle_radius(Conv, xStart, yStart, length, angle, radius, arcType):
    error = ''
    msg1 = _('entry is invalid')
    valid, xStart = Conv.conv_is_float(xStart)
    if not valid and xStart:
        msg0 = _('X START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, yStart = Conv.conv_is_float(yStart)
    if not valid and yStart:
        msg0 = _('Y START')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, length = Conv.conv_is_float(length)
    if not valid and length:
        msg0 = _('LENGTH')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('ANGLE')
        error += '{} {}\n\n'.format(msg0, msg1)
    valid, radius = Conv.conv_is_float(radius)
    if not valid and radius:
        msg0 = _('RADIUS')
        error += '{} {}\n\n'.format(msg0, msg1)
    if error:
        return True, error
    if radius == 0:
        msg = _('RADIUS cannot be zero')
        error += '{}\n\n'.format(msg)
    if length == 0:
        msg = _('LENGTH cannot be zero')
        error += '{}\n\n'.format(msg)
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
                                outNgc.write('{}\n'.format(l))
                        else:
                            outNgc.write('\n{} (preamble)\n'.format(preAmble))
                        break
                    elif '(postamble)' in line:
                        break
                    elif 'm2' in line.lower() or 'm30' in line.lower():
                        continue
                    outNgc.write(line)
        outTmp.write('\n(conversational line/arc)\n')
        outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
        outTmp.write('M190 P{}\n'.format(matNumber))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xStart, yStart))
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
    COPY(fTmp, fNgc)
    outNgc = open(fNgc, 'w')

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
                outNgc.write('{}\n'.format(l))
        else:
            outNgc.write('\n{} (postamble)\n'.format(postAmble))
        outNgc.write('m2\n')
