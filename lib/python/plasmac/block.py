'''
block.py

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
from shutil import copy as COPY
import gettext

for f in sys.path:
    if '/lib/python' in f:
        if '/usr' in f:
            localeDir = 'usr/share/locale'
        else:
            localeDir = os.path.join(f'{f.split("/lib")[0]}', 'share', 'locale')
        break
gettext.install("linuxcnc", localedir=localeDir)

'''
NOTE:
mirror is done externally by inverting convMirror and calling the preview function
flip is done externally by inverting convFlip and calling the preview function
'''


# Conv is the upstream calling module
def preview(Conv, fNgc, fTmp, columns, rows, cOffset,
            rOffset, xOffset, yOffset, angle,
            scale, rotation, convBlock, convMirror, convFlip,
            convMirrorToggle, convFlipToggle, g5xIndex, convUnits):
    error = ''
    msg1 = _('entry is invalid')
    valid, columns = Conv.conv_is_int(columns)
    if not valid:
        msg0 = _('COLUMNS NUMBER')
        error += f'{msg0} {msg1}\n\n'
    valid, cOffset = Conv.conv_is_float(cOffset)
    if not valid and cOffset:
        msg0 = _('COLUMNS OFFSET')
        error += f'{msg0} {msg1}\n\n'
    valid, rows = Conv.conv_is_int(rows)
    if not valid:
        msg0 = _('ROWS NUMBER')
        error += f'{msg0} {msg1}\n\n'
    valid, rOffset = Conv.conv_is_float(rOffset)
    if not valid and rOffset:
        msg0 = _('ROWS OFFSET')
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
    if columns <= 0:
        msg = _('COLUMNS NUMBER cannot be zero')
        error += f'{msg}\n\n'
    if rows <= 0:
        msg = _('ROWS NUMBER cannot be zero')
        error += f'{msg}\n\n'
    # if columns == 1 and rows == 1:
    #     msg = _('Either COLUMNS NUMBER or ROWS NUMBER must be greater then one')
    #     error += f'{msg}\n\n'
    if columns > 1 and not cOffset:
        msg = _('COLUMNS OFFSET is required')
        error += f'{msg}\n\n'
    if rows > 1 and not rOffset:
        msg = _('ROWS OFFSET is required')
        error += f'{msg}\n\n'
    if scale <= 0:
        msg = _('SCALE cannot be zero')
        error += f'{msg}\n\n'
    if error:
        return error
    COPY(fNgc, fTmp)
    inCode = open(fTmp, 'r')
    outNgc = open(fNgc, 'w')
    # edit existing parameters
    if convBlock[0]:
        for line in inCode:
            if line.startswith('#<array_x_offset>'):
                line = f'#<array_x_offset> = {cOffset}\n'
            elif line.startswith('#<array_y_offset>'):
                line = f'#<array_y_offset> = {rOffset}\n'
            elif line.startswith('#<array_columns>'):
                line = f'#<array_columns> = {columns}\n'
            elif line.startswith('#<array_rows>'):
                line = f'#<array_rows> = {rows}\n'
            elif line.startswith('#<origin_x_offset>'):
                line = f'#<origin_x_offset> = {xOffset}\n'
            elif line.startswith('#<origin_y_offset>'):
                line = f'#<origin_y_offset> = {yOffset}\n'
            elif line.startswith('#<array_angle>'):
                line = f'#<array_angle> = {angle}\n'
            elif line.startswith('#<blk_scale>'):
                line = f'#<blk_scale> = {scale}\n'
            elif line.startswith('#<shape_angle>'):
                line = f'#<shape_angle> = {rotation}\n'
            elif line.startswith('#<shape_mirror>'):
                line = f'#<shape_mirror> = {convMirror}\n'
            elif line.startswith('#<shape_flip>'):
                line = f'#<shape_flip> = {convFlip}\n'
            elif '#<shape_mirror>' in line and (convMirrorToggle or convFlipToggle):
                if 'g2' in line:
                    line = line.replace('g2', 'g3')
                elif 'g3' in line:
                    line = line.replace('g3', 'g2')
            outNgc.write(f'{line}')
    # create new array
    else:
        xIndex = [5221, 5241, 5261, 5281, 5301, 5321, 5341, 5361, 5381][(g5xIndex - 1)]
        outNgc.write(';conversational block\n\n')
        # inputs
        outNgc.write(';inputs\n')
        outNgc.write(f'#<ucs_x_offset> = #{xIndex}\n')
        outNgc.write(f'#<ucs_y_offset> = #{xIndex + 1}\n')
        outNgc.write(f'#<ucs_r_offset> = #{xIndex + 9}\n')
        outNgc.write(f'#<array_x_offset> = {cOffset}\n')
        outNgc.write(f'#<array_y_offset> = {rOffset}\n')
        outNgc.write(f'#<array_columns> = {columns}\n')
        outNgc.write(f'#<array_rows> = {rows}\n')
        outNgc.write(f'#<origin_x_offset> = {xOffset}\n')
        outNgc.write(f'#<origin_y_offset> = {yOffset}\n')
        outNgc.write(f'#<array_angle> = {angle}\n')
        outNgc.write(f'#<blk_scale> = {scale}\n')
        outNgc.write(f'#<shape_angle> = {rotation}\n')
        outNgc.write(f'#<shape_mirror> = {convMirror}\n')
        outNgc.write(f'#<shape_flip> = {convFlip}\n\n')
        # calculations
        outNgc.write(';calculations\n')
        outNgc.write('#<this_col> = 0\n')
        outNgc.write('#<this_row> = 0\n')
        outNgc.write('#<array_rot> = [#<array_angle> + #<ucs_r_offset>]\n')
        outNgc.write(f'#<blk_x_offset> = [#<origin_x_offset> + [#<ucs_x_offset> * {convUnits[0]}]]\n')
        outNgc.write(f'#<blk_y_offset> = [#<origin_y_offset> + [#<ucs_y_offset> * {convUnits[0]}]]\n')
        outNgc.write('#<x_sin> = [[#<array_x_offset> * #<blk_scale>] * SIN[#<array_rot>]]\n')
        outNgc.write('#<x_cos> = [[#<array_x_offset> * #<blk_scale>] * COS[#<array_rot>]]\n')
        outNgc.write('#<y_sin> = [[#<array_y_offset> * #<blk_scale>] * SIN[#<array_rot>]]\n')
        outNgc.write('#<y_cos> = [[#<array_y_offset> * #<blk_scale>] * COS[#<array_rot>]]\n\n')
        # main loop
        outNgc.write(';main loop\n')
        outNgc.write('o<loop> while [#<this_row> LT #<array_rows>]\n')
        outNgc.write('    #<shape_x_start> = [[#<this_col> * #<x_cos>] - [#<this_row> * #<y_sin>] + #<blk_x_offset>]\n')
        outNgc.write('    #<shape_y_start> = [[#<this_row> * #<y_cos>] + [#<this_col> * #<x_sin>] + #<blk_y_offset>]\n')
        outNgc.write('    #<blk_angle> = [#<shape_angle> + #<array_rot>]\n')
        if convUnits[1]:
            outNgc.write(f'    {convUnits[1]}\n')
        outNgc.write('    G10 L2 P0 X#<shape_x_start> Y#<shape_y_start> R#<blk_angle>\n\n')
        # the shape
        started, ended = False, False
        for line in inCode:
            # remove line numbers
            if line[0].lower() == 'n':
                line = line[1:]
                while line[0].isdigit() or line[0] == '.':
                    line = line[1:].lstrip()
                    if not line:
                        break
            # no need to process comments or empty lines
            if line[0] in ';(\n':
                outNgc.write(f'    {line}')
                continue
            line = line.strip().lower()
            # remove leading 0's from G & M codes
            if len(line) > 2 and line[0].lower() in 'gm':
                while len(line) > 2 and line[1] == '0':
                    if line[2].isdigit():
                        line = line[:1] + line[2:]
                    else:
                        break
            # scale the shape
            if line[0] in 'gxyz':
                started = True
                rLine = scale_shape(line, convMirrorToggle, convFlipToggle)
                if rLine is not None:
                    outNgc.write(f'    {rLine}\n')
                else:
                    return
            # loop counter
            elif not ended and ('m2' in line or 'm30' in line or (line.startswith('%') and started)):
                ended = True
                outNgc.write('\n    #<this_col> = [#<this_col> + 1]\n')
                outNgc.write('    o<count> if [#<this_col> EQ #<array_columns>]\n')
                outNgc.write('        #<this_col> = 0\n')
                outNgc.write('        #<this_row> = [#<this_row> + 1]\n')
                outNgc.write('    o<count> endif\n')
                outNgc.write('o<loop> endwhile\n')
            elif ended and ('m2' in line or 'm30' in line or line.startswith('%')):
                pass
            else:
                outNgc.write(f'    {line}\n')
        # reset offsets to original
        outNgc.write(f'\nG10 L2 P0 X[#<ucs_x_offset> * {convUnits[0]}] Y[#<ucs_y_offset> * {convUnits[0]}] R#<ucs_r_offset>\n')
        outNgc.write('\nM2\n')
    inCode.close()
    outNgc.close()
    return False


def scale_shape(line, convMirrorToggle, convFlipToggle):
    if line[0] == 'g' and (line[1] not in '0123' or (line[1] in '0123' and len(line) > 2 and line[2] in '0123456789')):
        return f'{line}'
    newLine = ''
    multiAxis = False
    numParam = False
    namParam = False
    fWord = False
    lastAxis = ''
    while 1:
        # remove spaces
        if line[0] == ' ':
            line = line[1:]
        # if beginning of comment
        if line[0] == '(' or line[0] == ';':
            if multiAxis and not fWord:
                if lastAxis == 'x':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'i':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'y':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis == 'j':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                else:
                    newLine += '*#<blk_scale>]'
            newLine += line
            break
        # if beginning of parameter
        elif line[0] == 'p':
            if not numParam and not namParam:
                if multiAxis and not fWord:
                    if lastAxis == 'x':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'i':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'y':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    elif lastAxis == 'j':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    else:
                        newLine += '*#<blk_scale>]'
                lastAxis = line[0]
            newLine += line[0]
            line = line[1:]
        # if alpha character
        elif line[0].isalpha():
            if not numParam and not namParam:
                if multiAxis and not fWord:
                    if lastAxis == 'x':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'i':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'y':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    elif lastAxis == 'j':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    # elif lastAxis not in 'p':
                    else:
                        newLine += '*#<blk_scale>]'
                lastAxis = line[0]
                if line[0] == 'f':
                    fWord = True
            newLine += line[0]
            line = line[1:]
        # if beginning of parameter
        elif line[0] == '#':
            numParam = True
            newLine += line[0]
            line = line[1:]
        # if parameter should be a named parameter
        elif line[0] == '<' and numParam:
            numParam = False
            namParam = True
            newLine += line[0]
            line = line[1:]
        # if end of numbered parameter
        elif not line[0].isdigit() and numParam:
            numParam = False
            newLine += line[0]
            line = line[1:]
        # if end of named parameter
        elif line[0] == '>' and namParam:
            namParam = False
            newLine += line[0]
            line = line[1:]
        # if last axis was x, y, z, i, j, or r
        elif newLine[-1] in 'xyzijr' and not numParam and not namParam:
            multiAxis = True
            newLine += f'[{line[0]}'
            line = line[1:]
        # everything else
        else:
            newLine += line[0]
            line = line[1:]
        # empty line, must be finished
        if not line:
            if not fWord:
                if lastAxis == 'x':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'i':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'y':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis == 'j':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis not in 'p':
                    newLine += '*#<blk_scale>]'
            break
    if '#<shape_mirror>' in newLine and (convMirrorToggle or convFlipToggle):
        if 'g2' in newLine:
            newLine = newLine.replace('g2', 'g3')
        elif 'g3' in newLine:
            newLine = newLine.replace('g3', 'g2')
    return (f'{newLine}')
