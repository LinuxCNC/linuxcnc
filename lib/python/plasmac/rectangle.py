'''
rectangle.py

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
            isCenter, xOffset, yOffset, \
            kerfWidth, isExternal, \
            width, height, angle, \
            styleR1, styleR2, styleR3, styleR4, \
            radiusR1, radiusR2, radiusR3, radiusR4, \
            textR1, textR2, textR3, textR4):
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
    valid, radius1 = Conv.conv_is_float(radiusR1)
    if not valid and radiusR1:
        error += '{} {}\n\n'.format(textR1, msg1)
    valid, radius2 = Conv.conv_is_float(radiusR2)
    if not valid and radiusR2:
        error += '{} {}\n\n'.format(textR2, msg1)
    valid, radius3 = Conv.conv_is_float(radiusR3)
    if not valid and radiusR3:
        error += '{} {}\n\n'.format(textR3, msg1)
    valid, radius4 = Conv.conv_is_float(radiusR4)
    if not valid and radiusR4:
        error += '{} {}\n\n'.format(textR4, msg1)
    valid, kerfWidth = Conv.conv_is_float(kerfWidth)
    if not valid:
        msg = _('Kerf Width entry in material is invalid')
        error += '{}\n\n'.format(msg)
    if error:
        return error
    if width == 0:
        msg = _('WIDTH cannot be zero')
        error += '{}\n\n'.format(msg)
    if height == 0:
        msg = _('HEIGHT cannot be zero')
        error += '{}\n\n'.format(msg)
    if width and height:
        msg0 = _('cannot be greater than')
        if isExternal:
            maxH = height
            maxW = width
        else:
            maxH = height + kerfWidth * -1
            maxW = width + kerfWidth * -1
        max = min(maxW / 2, maxH)
        if radius1 > max:
            error += '{} {} {}\n\n'.format(textR1, msg0, max)
        if radius2 > max:
            error += '{} {} {}\n\n'.format(textR2, msg0, max)
        if radius3 > maxH:
            error += '{} {} {}\n\n'.format(textR3, msg0, maxH)
        if radius4 > maxH:
            error += '{} {} {}\n\n'.format(textR4, msg0, maxH)
        if radius3 + radius4 > maxW:
            error += '{} + {} {} {}\n\n'.format(textR3, textR4, msg0, maxW)
        if radius1 + radius3 > maxH:
            error += '{} + {} {} {}\n\n'.format(textR1, textR3, msg0, maxH)
        if radius2 + radius4 > maxH:
            error += '{} + {} {} {}\n\n'.format(textR2, textR4,msg0, maxH)
    radii = [None, radius1, radius2, radius3, radius4]
    texts = [None, textR1, textR2, textR3, textR4]
    for r in range(1, 5):
        if radii[r] and radii[r] < kerfWidth / 2:
            msg0 = _('can not be less than kerf radius')
            error += '{} {}\n\n'.format(texts[r], msg0)
    if error:
        return error
    angle = math.radians(angle)
    leadInOffset = math.sin(math.radians(45)) * leadinLength
    leadOutOffset = math.sin(math.radians(45)) * leadoutLength
    kOffset = kerfWidth / 2
    if isExternal:
        width = width + kerfWidth
        height = height + kerfWidth
    else:
        width = width - kerfWidth
        height = height - kerfWidth
    right = math.radians(0)
    up = math.radians(90)
    left = math.radians(180)
    down = math.radians(270)
    if isExternal:
        if radius1:
            if styleR1 == 'extRadius':
                radius1 += kOffset
            elif styleR1 == 'chamfer':
                radius1 += kOffset * math.tan(math.radians(22.5))
        if radius2:
            if styleR2 == 'extRadius':
                radius2 += kOffset
            elif styleR2 == 'chamfer':
                radius2 += kOffset * math.tan(math.radians(22.5))
        if radius3:
            if styleR3 == 'extRadius':
                radius3 += kOffset
            elif styleR3 == 'chamfer':
                radius3 += kOffset * math.tan(math.radians(22.5))
        if radius4:
            if styleR4 == 'extRadius':
                radius4 += kOffset
            elif styleR4 == 'chamfer':
                radius4 += kOffset * math.tan(math.radians(22.5))
    else:
        if radius1:
            if styleR1 == 'extRadius':
                radius1 -= kOffset
            elif styleR1 == 'chamfer':
                radius1 -= kOffset * math.tan(math.radians(22.5))
        if radius2:
            if styleR2 == 'extRadius':
                radius2 -= kOffset
            elif styleR2 == 'chamfer':
                radius2 -= kOffset * math.tan(math.radians(22.5))
        if radius3:
            if styleR3 == 'extRadius':
                radius3 -= kOffset
            elif styleR3 == 'chamfer':
                radius3 -= kOffset * math.tan(math.radians(22.5))
        if radius4:
            if styleR4 == 'extRadius':
                radius4 -= kOffset
            elif styleR4 == 'chamfer':
                radius4 -= kOffset * math.tan(math.radians(22.5))
    xLB = width - (radius3 + radius4)
    xLT = width - (radius1 + radius2)
    xC = width / 2
    yLR = height - (radius2 + radius4)
    yLL = height - (radius1 + radius3)
    yC = height / 2
    blLength = math.sqrt(xC ** 2 + (yC * 2) ** 2)
    blAngle = math.atan((yC * 2) / xC)
    if isCenter:
        if isExternal:
            xS = xOffset + ((yC - radius2) * math.cos(angle + up)) + (xC * math.cos(angle + right))
            yS = yOffset +  (yC - radius2 * math.sin(angle + up)) +  (xC * math.sin(angle + right))
        else:
            xS = xOffset + yC * math.cos(angle + up)
            yS = yOffset + yC * math.sin(angle + up)
    else:
        if isExternal:
            xS = xOffset + (width * math.cos(angle + up)) + (width * math.cos(angle + right))
            yS = yOffset + ((height - radius2) * math.sin(angle + up)) + (width * math.sin(angle + right))
        else:
            xS = xOffset + (blLength * math.cos(angle + right + blAngle))
            yS = yOffset + (blLength * math.sin(angle + right + blAngle))
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
    outTmp.write('\n(conversational rectangle)\n')
    outTmp.write(';using material #{}: {}\n'.format(matNumber, matName))
    outTmp.write('M190 P{}\n'.format(matNumber))
    outTmp.write('M66 P3 L3 Q1\n')
    outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
    if isExternal:
        if leadInOffset > 0:
            xlCentre = xS + (leadInOffset * math.cos(angle + right))
            ylCentre = yS + (leadInOffset * math.sin(angle + right))
            xlStart = xlCentre + (leadInOffset * math.cos(angle + up))
            ylStart = ylCentre + (leadInOffset * math.sin(angle + up))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('m3 $0 s1\n')
        x1 = xS + yLR * math.cos(angle + down)
        y1 = yS + yLR * math.sin(angle + down)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
        if radius4:
            if styleR4 == 'intRadius':
                xrCentre = x1 + (radius4 * math.cos(angle + down))
                yrCentre = y1 + (radius4 * math.sin(angle + down))
                xrEnd = xrCentre + (radius4 * math.cos(angle + left))
                yrEnd = yrCentre + (radius4 * math.sin(angle + left))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            else:
                xrCentre = x1 + (radius4 * math.cos(angle + left))
                yrCentre = y1 + (radius4 * math.sin(angle + left))
                xrEnd = xrCentre + (radius4 * math.cos(angle + down))
                yrEnd = yrCentre + (radius4 * math.sin(angle + down))
            if styleR4 == 'extRadius':
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x2 = xrEnd + xLB * math.cos(angle + left)
            y2 = yrEnd + xLB * math.sin(angle + left)
        else:
            x2 = x1 + xLB * math.cos(angle + left)
            y2 = y1 + xLB * math.sin(angle + left)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
        if radius3:
            if styleR3 == 'intRadius':
                xrCentre = x2 + (radius3 * math.cos(angle + left))
                yrCentre = y2 + (radius3 * math.sin(angle + left))
                xrEnd = xrCentre + (radius3 * math.cos(angle + up))
                yrEnd = yrCentre + (radius3 * math.sin(angle + up))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
            else:
                xrCentre = x2 + (radius3 * math.cos(angle + up))
                yrCentre = y2 + (radius3 * math.sin(angle + up))
                xrEnd = xrCentre + (radius3 * math.cos(angle + left))
                yrEnd = yrCentre + (radius3 * math.sin(angle + left))
            if styleR3 == 'extRadius':
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x3 = xrEnd + yLL * math.cos(angle + up)
            y3 = yrEnd + yLL * math.sin(angle + up)
        else:
            x3 = x2 + yLL * math.cos(angle + up)
            y3 = y2 + yLL * math.sin(angle + up)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
        if radius1:
            if styleR1 == 'intRadius':
                xrCentre = x3 + (radius1 * math.cos(angle + up))
                yrCentre = y3 + (radius1 * math.sin(angle + up))
                xrEnd = xrCentre + (radius1 * math.cos(angle + right))
                yrEnd = yrCentre + (radius1 * math.sin(angle + right))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
            else:
                xrCentre = x3 + (radius1 * math.cos(angle + right))
                yrCentre = y3 + (radius1 * math.sin(angle + right))
                xrEnd = xrCentre + (radius1 * math.cos(angle + up))
                yrEnd = yrCentre + (radius1 * math.sin(angle + up))
            if styleR1 == 'extRadius':
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x4 = xrEnd + xLT * math.cos(angle + right)
            y4 = yrEnd + xLT * math.sin(angle + right)
        else:
            x4 = x3 + xLT * math.cos(angle + right)
            y4 = y3 + xLT * math.sin(angle + right)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
        if radius2:
            if styleR2 == 'intRadius':
                xrCentre = x4 + (radius2 * math.cos(angle + right))
                yrCentre = y4 + (radius2 * math.sin(angle + right))
                xrEnd = xrCentre + (radius2 * math.cos(angle + down))
                yrEnd = yrCentre + (radius2 * math.sin(angle + down))
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
            else:
                xrCentre = x4 + (radius2 * math.cos(angle + down))
                yrCentre = y4 + (radius2 * math.sin(angle + down))
                xrEnd = xrCentre + (radius2 * math.cos(angle + right))
                yrEnd = yrCentre + (radius2 * math.sin(angle + right))
            if styleR2 == 'extRadius':
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
    else:
        if leadInOffset > 0:
            xlCentre = xS + (leadInOffset * math.cos(angle + down))
            ylCentre = yS + (leadInOffset * math.sin(angle + down))
            xlStart = xlCentre + (leadInOffset * math.cos(angle + right))
            ylStart = ylCentre + (leadInOffset * math.sin(angle + right))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('m3 $0 s1\n')
        if radius1:
            x1 = xS + ((xLT - (radius1 - radius2)) / 2) * math.cos(angle + left)
            y1 = yS + ((xLT - (radius1 - radius2)) / 2) * math.sin(angle + left)
        else:
            x1 = xS - (width / 2)
            y1 = yS
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
        if radius1:
            if styleR1 == 'intRadius':
                xrCentre = x1 + (radius1 * math.cos(angle + left))
                yrCentre = y1 + (radius1 * math.sin(angle + left))
                xrEnd = xrCentre + (radius1 * math.cos(angle + down))
                yrEnd = yrCentre + (radius1 * math.sin(angle + down))
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            else:
                xrCentre = x1 + (radius1 * math.cos(angle + down))
                yrCentre = y1 + (radius1 * math.sin(angle + down))
                xrEnd = xrCentre + (radius1 * math.cos(angle + left))
                yrEnd = yrCentre + (radius1 * math.sin(angle + left))
            if styleR1 == 'extRadius':
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x2 = xrEnd + yLL * math.cos(angle + down)
            y2 = yrEnd + yLL * math.sin(angle + down)
        else:
            x2 = x1 + yLL * math.cos(angle + down)
            y2 = y1 + yLL * math.sin(angle + down)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
        if radius3:
            if styleR3 == 'intRadius':
                xrCentre = x2 + (radius3 * math.cos(angle + down))
                yrCentre = y2 + (radius3 * math.sin(angle + down))
                xrEnd = xrCentre + (radius3 * math.cos(angle + right))
                yrEnd = yrCentre + (radius3 * math.sin(angle + right))
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
            else:
                xrCentre = x2 + (radius3 * math.cos(angle + right))
                yrCentre = y2 + (radius3 * math.sin(angle + right))
                xrEnd = xrCentre + (radius3 * math.cos(angle + down))
                yrEnd = yrCentre + (radius3 * math.sin(angle + down))
            if styleR3 == 'extRadius':
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x3 = xrEnd + xLB * math.cos(angle + right)
            y3 = yrEnd + xLB * math.sin(angle + right)
        else:
            x3 = x2 + xLB * math.cos(angle + right)
            y3 = y2 + xLB * math.sin(angle + right)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
        if radius4:
            if styleR4 == 'intRadius':
                xrCentre = x3 + (radius4 * math.cos(angle + right))
                yrCentre = y3 + (radius4 * math.sin(angle + right))
                xrEnd = xrCentre + (radius4 * math.cos(angle + up))
                yrEnd = yrCentre + (radius4 * math.sin(angle + up))
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
            else:
                xrCentre = x3 + (radius4 * math.cos(angle + up))
                yrCentre = y3 + (radius4 * math.sin(angle + up))
                xrEnd = xrCentre + (radius4 * math.cos(angle + right))
                yrEnd = yrCentre + (radius4 * math.sin(angle + right))
            if styleR4 == 'extRadius':
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            x4 = xrEnd + yLR * math.cos(angle + up)
            y4 = yrEnd + yLR * math.sin(angle + up)
        else:
            x4 = x3 + yLR * math.cos(angle + up)
            y4 = y3 + yLR * math.sin(angle + up)
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
        if radius2:
            if styleR2 == 'intRadius':
                xrCentre = x4 + (radius2 * math.cos(angle + up))
                yrCentre = y4 + (radius2 * math.sin(angle + up))
                xrEnd = xrCentre + (radius2 * math.cos(angle + left))
                yrEnd = yrCentre + (radius2 * math.sin(angle + left))
                outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
            else:
                xrCentre = x4 + (radius2 * math.cos(angle + left))
                yrCentre = y4 + (radius2 * math.sin(angle + left))
                xrEnd = xrCentre + (radius2 * math.cos(angle + up))
                yrEnd = yrCentre + (radius2 * math.sin(angle + up))
            if styleR2 == 'extRadius':
                outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
            else:
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
    outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
    if leadOutOffset > 0: # and not (isExternal and radius2):
        if isExternal and not radius2:
            dir = ['g2', down, right]
        elif isExternal and radius2:
            dir = ['g3', right, down]
        else:
            dir = ['g3', down, left]
        xlCentre = xS + (leadOutOffset * math.cos(angle + dir[1]))
        ylCentre = yS + (leadOutOffset * math.sin(angle + dir[1]))
        xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[2]))
        ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[2]))
        outTmp.write('{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(dir[0], xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
    outTmp.write('g40\n')
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
