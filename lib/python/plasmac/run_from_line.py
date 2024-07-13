'''
run_from_line.py

Copyright (C) 2019-2024  Phillip A Carter
Copyright (C) 2020-2024  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import math

def run_from_line_get(file, startLine):
    preData,postData,newData,params,material = [],[],[],[],[]
    codes = {'g2_':'','g4_':'','g6_':'','g9_':'','g9arc':'','d3':'','d2':'','a3':'','x1':'','y1':'','x2':'','y2':''}
    codes['last'] = {'feed':'', 'code':''}
    codes['move'] = {'isSet':False, 'isG00':False}
    codes['spindle'] = {'code':False, 'line':None}
    oSub = []
    cutComp = False
    count = 0
    with open(file, 'r') as inFile:
        for line in inFile:
            # code before selected line
            if count < startLine:
                preData.append(line)
            # remaining code
            else:
                if count == startLine:
                    if 'G21' in line:
                        newData.append('G21')
                    elif 'G20' in line:
                        newData.append('G20')
                # find the type of first move
                    ''' IT IS POSSIBLE THERE MAY BE SPACES IN THE INCOMING LINE '''
                if not codes['move']['isSet'] and not 'G53G0' in line.replace(' ','') and not 'G20' in line and not 'G21' in line:
                    if 'G00' in line:
                        codes['move']['isSet'] = True
                        codes['move']['isG00'] = True
                        codes['x2'] = get_rfl_pos(line.strip(), codes['x2'], 'X')
                        codes['y2'] = get_rfl_pos(line.strip(), codes['y2'], 'Y')
                    if 'G01' in line or 'G02' in line or 'G03' in line:
                        codes['move']['isSet'] = True
                        codes['move']['isG00'] = False
                        codes['x2'] = get_rfl_pos(line.strip(), codes['x2'], 'X')
                        codes['y2'] = get_rfl_pos(line.strip(), codes['y2'], 'Y')
                    if 'm03' in line:
                        if not codes['spindle']['line']:
                            codes['spindle']['line'] = line.strip()
                            continue
                        codes['spindle']['line'] = line.strip()
                postData.append(line)
            count += 1
    # read all lines before selected line to get last used codes
    for line in preData:
        if line.startswith('('):
            if line.startswith('(o='):
                material = [line.strip()]
            continue
        elif line.startswith('M190'):
            material.append(line.strip())
            continue
        elif line.replace(' ','').startswith('M66P3'):
            material.append(line.strip())
            continue
        elif line.startswith('#'):
            params.append(line.strip())
            continue
        for t1 in ['G20','G21','G40','G41.1','G42.1','G61','G61.1','G64','G90','G90.1','G91','G91.1']:
            if t1 in line:
                if t1[1] == '2':
                    codes['g2_'] = t1
                elif t1[1] == '4':
                    codes['g4_'] = t1
                    if t1 != 'G40':
                        cutComp = True
                    else:
                        cutComp = False
                elif t1[1] == '6':
                    codes['g6_'] = t1 + line.split(t1)[1]
                elif t1 == 'G90' and not 'G90.1' in line:
                    codes['g9_'] = 'G90'
                elif t1 == 'G91' and not 'G91.1' in line:
                    codes['g9_'] = 'G91'
                elif t1 == 'G90.1' in line:
                    codes['g9arc'] = 'G90.1'
                elif t1 == 'G91.1' in line:
                    codes['g9arc'] = 'G91.1'
        if 'G00' in line and not 'G53g00' in line:
            codes['last']['code'] = 'G0'
        if 'G01' in line:
            tmp = line.split('G01')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'G01'
        if 'G02' in line:
            tmp = line.split('G02')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'G02'
        if 'G03' in line:
            tmp = line.split('G03')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'G03'
        if 'X' in line:
            codes['x1'] = get_rfl_pos(line.strip(), codes['x1'], 'X')
        if 'Y' in line:
            codes['y1'] = get_rfl_pos(line.strip(), codes['y1'], 'Y')
        if 'M03$' in line.replace(' ','') and not codes['spindle']['line']:
            codes['spindle']['line'] = line.strip()
        if 'M62P3' in line.replace(' ',''):
            codes['d3'] = 'M62 P3 (Disable Torch)'
        elif 'M63P3' in line.replace(' ',''):
            codes['d3'] = 'M63 P3 (Enable Torch)'
        elif 'M64P3' in line.replace(' ',''):
            codes['d3'] = 'M64 P3 (Disable Torch)'
        elif 'M65P3' in line.replace(' ',''):
            codes['d3'] = 'M65 P3 (Enable Torch)'
        if 'M62P2' in line.replace(' ',''):
            codes['d2'] = 'M62 P2 (Disable THC)'
        elif 'M63P2' in line.replace(' ',''):
            codes['d2'] = 'M63 P2 (Enable THC)'
        elif 'M64P2' in line.replace(' ',''):
            codes['d2'] = 'M64 P2 (Disable THC)'
        elif 'M65P2' in line.replace(' ',''):
            codes['d2'] = 'M65 P2 (Enable THC)'
        if 'M67E3Q' in line.replace(' ',''):
            codes['a3'] = 'M67 E3 Q'
            tmp = line.replace(' ','').split('M67E3Q')[1]
            while 1:
                if tmp[0] in '-.0123456789':
                    codes['a3'] += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            pc = float(codes['a3'].split('M67 E3 Q')[1])
            pc = pc if pc > 0 else 100
            codes['a3'] += ' (Velocity {}%)'.format(pc)
        if 'M68E3Q' in line.replace(' ',''):
            codes['a3'] = 'M68 E3 Q'
            tmp = line.replace(' ','').split('M68E3Q')[1]
            while 1:
                if tmp[0] in '-.0123456789':
                    codes['a3'] += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            pc = float(codes['a3'].split('M68 E3 Q')[1])
            pc = pc if pc > 0 else 100
            codes['a3'] += ' (Velocity {}%)'.format(pc)
        # test if inside a subroutine
        if line.startswith('O'):
            if 'END' in line:
                oSub = False
            else:
                if line[1] == '<':
                    os = 'O<'
                    tmp = line.replace(' ','').split('O<')[1]
                    while 1:
                        if tmp[0] != '>':
                            os += tmp[0]
                            tmp = tmp[1:]
                        else:
                            break
                    oSub.append('{}>'.format(os))
                else:
                    os = 'O'
                    tmp = line.replace(' ','').split('O')[1]
                    while 1:
                        if tmp[0] in '0123456789':
                            os += tmp[0]
                            tmp = tmp[1:]
                        else:
                            break
                    oSub.append(os)
        if '#<_hal[plasmac.cut-feed-rate]>' in line:
            codes['last']['feed'] = line.strip()
    # return an error line within a subroutine or if using cutter compensation
    if cutComp or oSub:
        return {'error':True, 'compError':cutComp, 'subError':oSub}
    # else return all data
    return {'error':False, 'codes':codes, 'params':params, 'material':material, 'postData':postData, 'newData':newData}

def run_from_line_set(rflFile, data, leadin, unitsPerMm):
    error = False
    # add all the codes retrieved from before the start line
    for param in data['params']:
        if param:
            data['newData'].append(param)
    scale = 1
    zMax = ''
    if unitsPerMm == 1:
        if data['codes']['g2_'] == 'G20':
            scale = 0.03937
            zMax = 'G53 G00 Z[[#<_ini[axis_z]max_limit> - 5] * 0.03937]'
        else:
            zMax = 'G53 G00 Z[#<_ini[axis_z]max_limit> - 5]'
    else:
        if data['codes']['g2_'] == 'G21':
            scale = 25.4
            zMax = 'G53 G00 Z[[#<_ini[axis_z]max_limit> * 25.4] - 5]'
        else:
            zMax = 'G53 G00 Z[#<_ini[axis_z]max_limit> - 0.02]'
    if data['codes']['g2_']:
        data['newData'].append(data['codes']['g2_'])
    if data['codes']['g4_']:
        data['newData'].append(data['codes']['g4_'])
    if data['codes']['g6_']:
        data['newData'].append(data['codes']['g6_'])
    if data['codes']['g9_']:
        data['newData'].append(data['codes']['g9_'])
    if data['codes']['g9arc']:
        data['newData'].append(data['codes']['g9arc'])
    data['newData'].append('M52 P1')
    if data['codes']['d3']:
        data['newData'].append(data['codes']['d3'])
    if data['codes']['d2']:
        data['newData'].append(data['codes']['d2'])
    if data['codes']['a3']:
        data['newData'].append(data['codes']['a3'])
    if zMax:
        data['newData'].append(zMax)
    if data['material']:
        for line in data['material']:
            data['newData'].append(line)
    if data['codes']['last']['feed']:
        data['newData'].append(data['codes']['last']['feed'])
    # if g00 is not the first motion command after selected line set x and y coordinates
    if not data['codes']['move']['isG00']:
        if leadin['do']:
            error, xL, yL = set_leadin_coordinates(data['codes']['x1'], data['codes']['y1'], scale, leadin['length'], leadin['angle'])
        else:
            xL = data['codes']['x1']
            yL = data['codes']['y1']
        data['codes']['spindle']['code'] = set_spindle_start(xL, yL, data['codes']['x1'], data['codes']['y1'], data['codes']['spindle']['line'], data['newData'], False)
    # if no spindle command yet then find the next one for the correct tool
    if not data['codes']['spindle']['line']:
        for line in data['postData']:
            if 'M3$' in line.replace(' ',''):
                data['codes']['spindle']['line'] = line.strip()
                break
    # add all the code from the selected line to the end
    for line in data['postData']:
        # if we have the first spindle code we don't need it again
        if 'M03$' in line.replace(' ','') and data['codes']['spindle']['code']:
            data['codes']['spindle']['code'] = False
            continue
        # if G00 is the first motion command after the selected line
        if data['codes']['move']['isG00']:
            # if G0 is the current motion command
            if 'G00' in line:
                # no need to process a G53G0 command]
                if 'G53G00' in line or 'G20' in line or 'G21' in line:
                    data['newData'].append(line.strip())
                    continue
                if leadin['do']:
                    error, xL, yL = set_leadin_coordinates(data['codes']['x2'], data['codes']['y2'], scale, leadin['length'], leadin['angle'])
                else:
                    xL = data['codes']['x2']
                    yL = data['codes']['y2']
                data['codes']['spindle']['code'] = set_spindle_start(xL, yL, data['codes']['x2'], data['codes']['y2'], data['codes']['spindle']['line'], data['newData'], True)
                # no need to process any more G00 commands
                data['codes']['move']['isG00'] = False
                continue
        data['newData'].append(line.strip())
    # create the rfl file
    with open(rflFile, 'w') as outFile:
        for line in data['newData']:
            outFile.write('{}\n'.format(line))
    return {'error':error}

def set_leadin_coordinates(x, y, scale, length, angle):
    xL = x
    yL = y
    try:
        if x[-1] == ']':
            xL = '{}[[{}]+{:0.6f}]'.format(x[:1], x[1:], (length * scale) * math.cos(math.radians(angle)))
            yL = '{}[[{}]+{:0.6f}]'.format(y[:1], y[1:], (length * scale) * math.sin(math.radians(angle)))
        else:
            xL = float(x) + ((length * scale) * math.cos(math.radians(angle)))
            yL = float(y) + ((length * scale) * math.sin(math.radians(angle)))
    except:
        return(True, x, y)
    return(False, xL, yL)

def set_spindle_start(xL, yL, x, y, line, newData, reply):
    leadIn = {}
    if xL != x and yL != y:
        newData.append('G00 X{} Y{}'.format(xL, yL))
        leadIn['x'] = x
        leadIn['y'] = y
    else:
        if x and y:
            newData.append('G00 X{} Y{}'.format(x, y))
        elif x:
            newData.append('G00 X{}'.format(x))
        elif y:
            newData.append('G00 Y{}'.format(y))
    if line:
        newData.append(line)
    if leadIn:
        newData.append('G01 X{} Y{} (leadin)'.format(leadIn['x'], leadIn['y']))
    return reply

def get_rfl_pos(line, axisPos, axisLetter):
    maths = 0
    pos = ''
    done = False
    if line.startswith('(') or line.startswith(';'):
        return pos if pos else axisPos
    while len(line):
        if line[0] == ('('):
            break
        if not line[0] == axisLetter:
            line = line[1:]
        else:
            while 1:
                line = line[1:]
                if line[0] in '-.0123456789#':
                    pos += line[0]
                elif line[0] == '[' or line[0] == '<':
                    pos += line[0]
                    maths += 1
                elif (line[0] == ']' or line[0] == '>') and maths > 0:
                    pos += line[0]
                    maths -= 1
                elif maths:
                    pos += line[0]
                elif (pos and not maths) or line[0] == '(':
                    done = True
                    break
                else:
                    if len(line) == 1: break
                    break
                if len(line) == 1:
                    break
        if done:
            break
    return pos if pos else axisPos
