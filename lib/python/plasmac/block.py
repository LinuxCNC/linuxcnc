'''
block.py

Copyright (C) 2020 - 2025 Phillip A Carter
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
import re
import gettext
import linuxcnc
from rs274.glcanon import GlCanonDraw as DRAW

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f'{f.split("/lib")[0]}', 'share', 'locale')
        break
gettext.install("linuxcnc", localedir=localeDir)

STAT = linuxcnc.stat()

gcodePattern = re.compile(
        r"(;row(?P<OP>.[^X]+) *)?"
        r"([Gg] *(?P<G1>\d{1,2}) *)?"
        r"([Gg] *(?P<G2>\d{1,2}) *)?"
        r"([Xx] *(?P<X>-*\d+\.?\d*) *)?"
        r"([Yy] *(?P<Y>-*\d+\.?\d*) *)?"
        r"([Zz] *(?P<Z>(-*\d+\.?\d*)|([^;^(]*)) *)?"
        r"([Ii] *(?P<I>-*\d+\.?\d*) *)?"
        r"([Jj] *(?P<J>-*\d+\.?\d*) *)?"
        r"([Mm] *(?P<M>\d{1,3}) *)?"
        r"([Pp] *(?P<P>-*\d+\.?\d*) *)?"
        r"([Qq] *(?P<Q>-*\d+\.?\d*) *)?"
        r"((?P<C>[;(].+) *)?"
        )

# Conv is the upstream calling module
def preview(Conv, fNgc, isConvBlock, window, columns, rows, cSpacing, rSpacing,
            xOffset, yOffset, angle, scale, rotation, doMirror, doFlip):
    ''' preview the new block '''
    # validate inputs
    error = ''
    msg1 = _('entry is invalid')
    valid, columns = Conv.conv_is_int(columns)
    if not valid:
        msg0 = _('COLUMNS NUMBER')
        error += f'{msg0} {msg1}\n\n'
    valid, cSpacing = Conv.conv_is_float(cSpacing)
    if not valid and cSpacing:
        msg0 = _('COLUMNS SPACING')
        error += f'{msg0} {msg1}\n\n'
    valid, rows = Conv.conv_is_int(rows)
    if not valid:
        msg0 = _('ROWS NUMBER')
        error += f'{msg0} {msg1}\n\n'
    valid, rSpacing = Conv.conv_is_float(rSpacing)
    if not valid and rSpacing:
        msg0 = _('ROWS SPACING')
        error += f'{msg0} {msg1}\n\n'
    valid, xOffset = Conv.conv_is_float(xOffset)
    if not valid and xOffset:
        msg0 = _('X OFFSET')
        error += f'{msg0} {msg1}\n\n'
    valid, yOffset = Conv.conv_is_float(yOffset)
    if not valid and yOffset:
        msg0 = _('Y OFFSET')
        error += f'{msg0} {msg1}\n\n'
    valid, angle = Conv.conv_is_float(angle)
    if not valid and angle:
        msg0 = _('PATTERN ANGLE')
        error += f'{msg0} {msg1}\n\n'
    valid, scale = Conv.conv_is_float(scale)
    if not valid:
        msg0 = _('SHAPE SCALE')
        error += f'{msg0} {msg1}\n\n'
    valid, rotation = Conv.conv_is_float(rotation)
    if not valid and rotation:
        msg0 = _('SHAPE ROTATION')
        error += f'{msg0} {msg1}\n\n'
    if error:
        return error
    # validate some entry values
    if columns <= 0:
        msg = _('COLUMNS NUMBER cannot be zero')
        error += f'{msg}\n\n'
    if rows <= 0:
        msg = _('ROWS NUMBER cannot be zero')
        error += f'{msg}\n\n'
    if columns > 1 and not cSpacing:
        msg = _('COLUMNS SPACING is required')
        error += f'{msg}\n\n'
    if rows > 1 and not rSpacing:
        msg = _('ROWS SPACING is required')
        error += f'{msg}\n\n'
    if scale <= 0:
        msg = _('SCALE cannot be zero')
        error += f'{msg}\n\n'
    if error:
        return error
    if isConvBlock:
        # this is a valid conversational block version:
        # get the inputs from the conversational entries
        # get current mirror and flip state from the file
        # get the gcode from the file
        inputsOld, preCode, gCode, postCode = get_v2_code(fNgc)
        inputs = [cSpacing, rSpacing, columns, rows, xOffset, yOffset, angle, scale, rotation,
                  inputsOld[9], inputsOld[10], inputsOld[11], inputsOld[12]]
        # files converted from v1 will not have the mid points set yet
        # get the midpoints
        if inputsOld[11].strip() == '0' and inputsOld[12].strip() == '0':
            xMid, yMid = get_midpoints(window)
            # need the midpoints of the first shape only
            if columns > 1:
                xMid -= (columns - 1) * cSpacing / 2
            if rows > 1:
                yMid -= (rows - 1) * rSpacing / 2
            inputs[11] = f"{xMid:0.4f}"
            inputs[12] = f"{yMid:0.4f}"
    else:
        # this is not a conversational block:
        # get the inputs from the conversational entries
        # set the mirror and flip state to off
        # get the gcode from the file
        inputs = [cSpacing, rSpacing, columns, rows, xOffset, yOffset, angle, scale, rotation, 1, 1]
        preCode, gCode, postCode = get_raw_code(fNgc)
        # get the midpoints
        xMid, yMid = get_midpoints(window)
        inputs.extend([f"{xMid:0.4f}", f"{yMid:0.4f}"])
    write_block(fNgc, inputs, preCode, gCode, postCode, doMirror, doFlip)

def write_block(file, inputs, preCode, gCode, postCode, doMirror=False, doFlip=False):
    ''' write the gcode for the block '''
    # write the file
    with open(file, 'w') as ngc:
        cSpacing = float(inputs[0])
        rSpacing = float(inputs[1])
        columns = int(inputs[2])
        rows = int(inputs[3])
        xOffset = float(inputs[4])
        yOffset = float(inputs[5])
        angle = float(inputs[6]) # pattern angle
        scale = float(inputs[7])
        rotation = float(inputs[8]) # shape rotation
        mirrored = int(inputs[9])
        flipped = int(inputs[10])
        xMid = float(inputs[11])
        yMid = float(inputs[12])
        outCode = []                # temporary storage for output gcode
        templateWritten = False     # write all subsequent output gcode to temporary storage
        # set mirror
        mirror = -1 if (doMirror and mirrored != -1) or (not doMirror and mirrored == -1) else 1
        # set flip
        flip = -1 if (doFlip and flipped != -1) or (not doFlip and flipped == -1) else 1
        outCode.append('\n;CONV_BLOCK GCODE')
        # iterate through the column and row combination
        for row in range(rows):
            for col in range(columns):
                # set start coordinates
                xStart, yStart = rotate_point(angle, (col * cSpacing * scale, row * rSpacing * scale))
                xStart += xOffset
                yStart += yOffset
                if templateWritten:
                    ngc.write(f'\n;row{row} col{col} - origin: X{xStart:0.5f} Y{yStart:0.5f}\n')
                else:
                    outCode.append(f'\n;row{row} col{col} - origin: X{xStart:0.4f} Y{yStart:0.4f}')
                gCurrent = None # the current active gcode
                xCurrent = 0.0  # the current original x coordinate
                yCurrent = 0.0  # the current original y coordinate
                previous = {}   # the previous original and rotated x and y coordinates
                # iterate through the gcode
                for line in gCode:
                    line = line.strip()
                    # create the code dictionary
                    code = gcodePattern.match(line).groupdict()
                    # set the current gcode
                    if code['G1']:
                        gCurrent = code['G1'] if code['G1'] in ['00', '01', '02', '03'] else None
                    elif code['M']:
                        gCurrent = None
                    # if rapid, line, or arc
                    if gCurrent in ['00', '01', '02', '03']:
                        # reverse arc direction only if one of mirror or flip
                        if mirror != flip:
                            if 'G02' in line:
                                line = line.replace('G02', 'G03')
                            elif 'G03' in line:
                                line = line.replace('G03', 'G02')
                        # set the current x and y coordinates
                        xCurrent = xCurrent if not code['X'] else float(code['X'])
                        yCurrent = yCurrent if not code['Y'] else float(code['Y'])
                        # rotate end point for pattern angle
                        x, y = rotate_point(angle, (xCurrent * scale * mirror, yCurrent * scale * flip))
                        # rotate end point for shape rotation
                        x, y = rotate_point(rotation, (x, y), (xMid, yMid))
                        # save the calculated x and y points
                        line=line.replace(f"x{code['X']}", f' X{xStart + x:0.5f}').replace(f"X{code['X']}", f' X{xStart + x:0.5f}')
                        line=line.replace(f"y{code['Y']}", f' Y{yStart + y:0.5f}').replace(f"Y{code['Y']}", f' Y{yStart + y:0.5f}')
                        # if this is an arc
                        if code['I'] or code['J']:
                            # set new i and j relative coordinates
                            ir = 0.0 if not code['I'] else float(code['I'])
                            yr = 0.0 if not code['J'] else float(code['J'])
                            # set arc absolute center points
                            xCenter = previous['xOriginal'] + ir
                            yCenter = previous['yOriginal'] + yr
                            # rotate arc center point for pattern angle
                            xCenter, yCenter = rotate_point(angle, (xCenter * scale * mirror, yCenter * scale * flip))
                            # rotate arc center point for shape rotation
                            xCenter, yCenter = rotate_point(rotation, (xCenter, yCenter), (xMid, yMid))
                            # set the relative i and j cordinates
                            i = xCenter - previous['xRotated']
                            j = yCenter - previous['yRotated']
                            # save the calculated i and j coordinates
                            line=line.replace(f"i{code['I']}", f' I{i:0.5f}').replace(f"I{code['I']}", f' I{i:0.5f}')
                            line=line.replace(f"j{code['J']}", f' J{j:0.5f}').replace(f"J{code['J']}", f' J{j:0.5f}')
                        previous['xOriginal'] = xCurrent
                        previous['yOriginal'] = yCurrent
                        previous['xRotated'] = x
                        previous['yRotated'] = y
                    # write output gcode to file
                    if templateWritten:
                        ngc.write(f'{line}\n')
                    # write output gcode to temporary storage and test if template is ready to be written
                    else:
                        if mirror or flip:
                            outCode.append(line)
                            inputs[9] = mirror
                            inputs[10] = flip
                            # write the header to the file
                            ngc.write(';conversational block V2\n')
                            # write the version 2 template to the file
                            ngc.write(f'\n;CONV_BLOCK TEMPLATE START *****************************\n')
                            ngc.write(f'\n;CONV_BLOCK INPUTS\n;')
                            ins = f'{float(inputs[0]):0.4f}'
                            for i in range(1, len(inputs)):
                                if i in [2, 3, 9, 10]:
                                    ins += f', {inputs[i]}'
                                else:
                                    ins += f', {float(inputs[i]):0.4f}'
                                    #ins += f', {inputs[i]}'
                            ngc.write(f'{ins}')
                            ngc.write(f'\n\n;CONV_BLOCK PRECODE\n')
                            for line in preCode:
                                ngc.write(f';{line.replace(";;", ";")}\n')
                            ngc.write('\n;CONV_BLOCK GCODE\n')
                            for line in gCode:
                                ngc.write(f';{line.replace(";;", ";")}\n')
                            ngc.write('\n;CONV_BLOCK POSTCODE\n')
                            for line in postCode:
                                ngc.write(f';{line.replace(";;", ";")}\n')
                            ngc.write(f';CONV_BLOCK TEMPLATE END *******************************\n\n')
                            # write the precode to the file
                            ngc.write(f'\n;CONV_BLOCK PRECODE\n')
                            for line in preCode:
                                ngc.write(f'{line}\n')
                            for line in outCode:
                                ngc.write(f'{line}\n')
                            # write all subsequent output gcode directly to file
                            templateWritten = True
                        else:
                            outCode.append(line)
        # write the postcode to the file
        ngc.write('\n;CONV_BLOCK POSTCODE\n')
        for line in postCode:
            ngc.write(f'{line}\n')
        ngc.write(';qtplasmac filtered G-code file\n')
    return False

def rotate_point(rotation, point, origin=(0, 0)):
    ''' rotate a point counterclockwise by rotation degrees '''
    angle = math.radians(rotation)
    xP, yP = point
    xO, yO = origin
    x = xO + math.cos(angle) * (xP - xO) - math.sin(angle) * (yP - yO)
    y = yO + math.sin(angle) * (xP - xO) + math.cos(angle) * (yP - yO)
    return (x, y)

def get_midpoints(window):
    ''' retrive the midpoints of the shape '''
    STAT.poll()
    mult = 1
    if STAT.linear_units == 1:
        mult = 25.4
    xMid = DRAW.extents_info(window)[0][0] * mult - STAT.g5x_offset[0]
    yMid = DRAW.extents_info(window)[0][1] * mult - STAT.g5x_offset[1]
    return xMid, yMid

def set_to_upper_case(data):
    ''' set gcode words to upper case '''
    tmp = ''
    keep = False
    for d in data:
        if d in '#(':
            keep = True
            tmp += d
        elif d in '>)':
            keep = False
            tmp += d
        else:
            if keep:
                tmp += d
            else:
                tmp += d.upper()
    return tmp

def get_raw_code(file):
    ''' read qtplasmac format file to allow block manipulation
        file has been filtered so no preprocessing is required '''
    preCode, gCode, postCode, outCode = [], [], [], 'precode'
    with open(file, 'r') as inCode:
        raw = inCode.read()
    pierce = raw.count('M03')
    m03 = False
    with open(file, 'r') as inCode:
        for line in inCode:
            line = line.strip()
            if 'M03' in line:
                m03 = True
            if outCode == 'precode':
                preCode.append(line)
                if '#<_hal[plasmac.cut-feed-rate]>' in line:
                    outCode = 'gcode'
            elif outCode == 'gcode':
                # no need to process comments or empty lines
                if line and line[0] in ';(':
                    gCode.append(line)
                    continue
                elif 'M05' in line:
                    if m03: # or 'postamble' in line:
                        m03 = False
                        pierce -= 1
                        if not pierce:
                            outCode = 'postcode'
                        gCode.append(line)
                else:
                    gCode.append(line)
            else:
                if 'M68' in line:
                    gCode.append(line)
                else:
                    postCode.append(line)
        return(preCode, gCode, postCode)

def get_v2_code(file):
    ''' read new format conversational block files '''
    inputs, preCode, gCode, postCode, outCode = [], [], [], [], None
    with open(file, 'r') as inCode:
        for line in inCode:
            line = line.strip()
            if outCode is None:
                if ';CONV_BLOCK INPUTS' in line:
                    outCode = 'inputs'
            elif outCode == 'inputs':
                if ';CONV_BLOCK PRECODE' in line:
                    outCode = 'precode'
                elif line:
                    inputs = line[1:].split(', ')
            elif outCode == 'precode':
                if ';CONV_BLOCK GCODE' in line:
                    outCode = 'gcode'
                elif line:
                    preCode.append(line[1:])
            elif outCode == 'gcode':
                if ';CONV_BLOCK POSTCODE' in line:
                    outCode = 'postCode'
                elif line:
                    gCode.append(line[1:])
            elif outCode == 'postCode':
                if ';CONV_BLOCK TEMPLATE END' in line:
                    outCode = 'completed'
                else:
                    postCode.append(line[1:])
    return(inputs, preCode, gCode, postCode)

def convert_v1_code(file):
    ''' convert old format conversational block files '''
    inputs, preCode, gCode, postCode, outCode = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [], [], [], None
    mirror = '*#<blk_scale>*#<shape_mirror>]'
    flip = '*#<blk_scale>*#<shape_flip>]'
    with open(file, 'r') as raw:
        check = raw.read().upper()
    pierces = sum(check.count(x) for x in ('M3', 'M03')) - check.count('M30')
    with open(file, 'r') as inCode:
        for line in inCode:
            # strip leading and trailing whitespace
            line = line.strip()
            # set to uppercase
            line = set_to_upper_case(line)
            # add leading 0's to short G & M codes
            if len(line) == 2 and line[0] in 'gmGM':
                line = f'{line[0].upper()}0{line[1]}'
            elif len(line) > 3 and ('g' in line or 'G' in line or 'm' in line or 'M' in line):
                tmp = ''
                while len(line) > 1:
                    if line[0] in 'gmGM' and line[1].isdigit() and not line[2].isdigit():
                        tmp += f'{line[0].upper()}0{line[1]}'
                        line = line[2:]
                    else:
                        tmp += line[0]
                        line = line[1:]
                tmp += line
                line = tmp
            # get old inputs
            if outCode is None:
                if line[:6] in ['g10 l2', 'G10 L2']:
                    outCode = 'precode'
                if line.startswith('#<array_x_offset>'):
                    inputs[0] = line.split("=")[1].strip()
                elif line.startswith('#<array_y_offset>'):
                    inputs[1] = line.split("=")[1].strip()
                elif line.startswith('#<array_columns>'):
                    inputs[2] = line.split('=')[1].strip()
                elif line.startswith('#<array_rows>'):
                    inputs[3] = line.split('=')[1].strip()
                elif line.startswith('#<origin_x_offset>'):
                    inputs[4] = line.split("=")[1].strip()
                elif line.startswith('#<origin_y_offset>'):
                    inputs[5] = line.split("=")[1].strip()
                elif line.startswith('#<array_angle>'):
                    inputs[6] = line.split('=')[1].strip()
                elif line.startswith('#<blk_scale>'):
                    inputs[7] = line.split('=')[1].strip()
                elif line.startswith('#<shape_angle>'):
                    inputs[8] = line.split('=')[1].strip()
                elif line.startswith('#<shape_mirror>'):
                    inputs[9] = line.split('=')[1].strip()
                elif line.startswith('#<shape_flip>'):
                    inputs[10] = line.split('=')[1].strip()
            elif outCode == 'precode':
                if line[:3] in ['g00', 'G00']:
                    outCode = 'gcode'
                    gCode.append(line.replace('[', '').replace(mirror, '').replace(flip, ''))
                elif line:
                    preCode.append(line)
            elif outCode == 'gcode':
                if line[:3] in ['m05', 'M05']:
                    pierces -= 1
                    if not pierces:
                        outCode = 'postcode'
                    gCode.append(line)
                elif line:
                    gCode.append(line.replace('[', '').replace(mirror, '').replace(flip, ''))
            elif outCode == 'postcode':
                if '#<this_col>' in line:
                    outCode = 'invalid'
                    postCode.append('M02')
                    break
                elif line[:3] in ['m68', 'M68']:
                    gCode.append(line)
                elif line:
                        postCode.append(line)
    return inputs, preCode, gCode, postCode
