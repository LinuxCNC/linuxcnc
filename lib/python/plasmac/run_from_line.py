'''
run_from_line.py

Copyright (C) 2019, 2020, 2021, 2022  Phillip A Carter
Copyright (C)       2020, 2021, 2022  Gregory D Carl

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
    codes = {'g2':'','g4':'','g6':'','g9':'','g9arc':'','d3':'','d2':'','a3':'','x1':'','y1':'','x2':'','y2':''}
    codes['last'] = {'feed':'', 'code':''}
    codes['move'] = {'isSet':False, 'isG0':False}
    codes['spindle'] = {'code':False, 'line':None}
    oSub = []
    cutComp = False
    count = 0
    with open(file, 'r') as inFile:
        for line in inFile:
            # code before selected line
            if count < startLine:
                preData.append(line.lower())
            # remaining code
            else:
                if count == startLine:
                    if 'g21' in line:
                        newData.append('g21')
                    elif 'g20' in line:
                        newData.append('g20')
                    if line.strip().startswith('m66p3'):
                        material.append(line.strip())
                # find the type of first move
                if not codes['move']['isSet'] and not 'g53g0' in line and not 'g20' in line and not 'g21' in line:
                    if 'g0' in line:
                        codes['move']['isSet'] = True
                        codes['move']['isG0'] = True
                        codes['x2'] = get_rfl_pos(line.strip(), codes['x2'], 'x')
                        codes['y2'] = get_rfl_pos(line.strip(), codes['y2'], 'y')
                    if 'g1' in line or 'g2' in line or 'g3' in line:
                        codes['move']['isSet'] = True
                        codes['move']['isG0'] = False
                        codes['x2'] = get_rfl_pos(line.strip(), codes['x2'], 'x')
                        codes['y2'] = get_rfl_pos(line.strip(), codes['y2'], 'y')
                    if 'm3$' in line.replace(' ',''):
                        if not codes['spindle']['line']:
                            codes['spindle']['line'] = line.lower().strip()
                            continue
                        codes['spindle']['line'] = line.strip()
                postData.append(line.lower())
            count += 1
    # read all lines before selected line to get last used codes
    for line in preData:
        if line.startswith('('):
            if line.startswith('(o='):
                material = [line.strip()]
            continue
        elif line.startswith('m190'):
            material.append(line.strip())
            continue
        elif line.replace(' ','').startswith('m66p3'):
            material.append(line.strip())
            continue
        elif line.startswith('#'):
            params.append(line.strip())
            continue
        for t1 in ['g20','g21','g40','g41.1','g42.1','g61','g61.1','g64','g90','g90.1','g91','g91.1']:
            if t1 in line:
                if t1[1] == '2':
                    codes['g2'] = t1
                elif t1[1] == '4':
                    codes['g4'] = t1
                    if t1 != 'g40':
                        cutComp = True
                    else:
                        cutComp = False
                elif t1[1] == '6':
                    codes['g6'] = t1
                    if t1 == 'g64':
                        tmp = line.split('64')[1]
                        if tmp[0] == 'p':
                            p = ''
                            tmp = tmp[1:]
                            while 1:
                                if tmp[0] in '.0123456789q':
                                    p += tmp[0]
                                    tmp = tmp[1:]
                                else:
                                    break
                            codes['g6'] = 'g64p{}'.format(p)
                elif t1 == 'g90' and not 'g90.1' in line:
                    codes['g9'] = 'g90'
                elif t1 == 'g91' and not 'g91.1' in line:
                    codes['g9'] = 'g91'
                elif t1 == 'g90.1' in line:
                    codes['g9arc'] = 'g90.1'
                elif t1 == 'g91.1' in line:
                    codes['g9arc'] = 'g91.1'
        if 'g0' in line and not 'g53g0' in line:
            codes['last']['code'] = 'g0'
        if 'g1' in line:
            tmp = line.split('g1')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'g1'
        if 'g2' in line:
            tmp = line.split('g2')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'g2'
        if 'g3' in line:
            tmp = line.split('g3')[1]
            if tmp[0] not in '0123456789':
                codes['last']['code'] = 'g3'
        if 'x' in line:
            codes['x1'] = get_rfl_pos(line.strip(), codes['x1'], 'x')
        if 'y' in line:
            codes['y1'] = get_rfl_pos(line.strip(), codes['y1'], 'y')
        if 'm3$' in line.replace(' ','') and not codes['spindle']['line']:
            codes['spindle']['line'] = line.strip()
        if 'm62p3' in line.replace(' ',''):
            codes['d3'] = 'm62p3 (Disable Torch)'
        elif 'm63p3' in line.replace(' ',''):
            codes['d3'] = 'm63p3 (Enable Torch)'
        elif 'm64p3' in line.replace(' ',''):
            codes['d3'] = 'm64p3 (Disable Torch)'
        elif 'm65p3' in line.replace(' ',''):
            codes['d3'] = 'm65p3 (Enable Torch)'
        if 'm62p2' in line.replace(' ',''):
            codes['d2'] = 'm62p2 (Disable THC)'
        elif 'm63p2' in line.replace(' ',''):
            codes['d2'] = 'm63p2 (Enable THC)'
        elif 'm64p2' in line.replace(' ',''):
            codes['d2'] = 'm64p2 (Disable THC)'
        elif 'm65p2' in line.replace(' ',''):
            codes['d2'] = 'm65p2 (Enable THC)'
        if 'm67e3q' in line.replace(' ',''):
            codes['a3'] = 'm67e3q'
            tmp = line.replace(' ','').split('m67e3q')[1]
            while 1:
                if tmp[0] in '-.0123456789':
                    codes['a3'] += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            pc = float(codes['a3'].split('m67e3q')[1])
            pc = pc if pc > 0 else 100
            codes['a3'] += ' (Velocity {}%)'.format(pc)
        if 'm68e3q' in line.replace(' ',''):
            codes['a3'] = 'm68e3q'
            tmp = line.replace(' ','').split('m68e3q')[1]
            bb=1
            while 1:
                if tmp[0] in '-.0123456789':
                    codes['a3'] += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            pc = float(codes['a3'].split('m68e3q')[1])
            pc = pc if pc > 0 else 100
            codes['a3'] += ' (Velocity {}%)'.format(pc)
        # test if inside a subroutine
        if line.startswith('o'):
            if 'end' in line:
                oSub = False
            else:
                if line[1] == '<':
                    os = 'o<'
                    tmp = line.replace(' ','').split('o<')[1]
                    while 1:
                        if tmp[0] != '>':
                            os += tmp[0]
                            tmp = tmp[1:]
                        else:
                            break
                    oSub.append('{}>'.format(os))
                else:
                    os = 'o'
                    tmp = line.replace(' ','').split('o')[1]
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
        if data['codes']['g2'] == 'g20':
            scale = 0.03937
            zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> - 5] * 0.03937]'
        else:
            zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 5]'
    else:
        if data['codes']['g2'] == 'g21':
            scale = 25.4
            zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> * 25.4] - 5]'
        else:
            zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 0.02]'
    if data['codes']['g2']:
        data['newData'].append(data['codes']['g2'])
    if data['codes']['g4']:
        data['newData'].append(data['codes']['g4'])
    if data['codes']['g6']:
        data['newData'].append(data['codes']['g6'])
    if data['codes']['g9']:
        data['newData'].append(data['codes']['g9'])
    if data['codes']['g9arc']:
        data['newData'].append(data['codes']['g9arc'])
    data['newData'].append('m52 p1')
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
    # if g0 is not the first motion command after selected line set x and y coordinates
    if not data['codes']['move']['isG0']:
        if leadin['do']:
            error, xL, yL = set_leadin_coordinates(data['codes']['x1'], data['codes']['y1'], scale, leadin['length'], leadin['angle'])
        else:
            xL = data['codes']['x1']
            yL = data['codes']['y1']
        data['codes']['spindle']['code'] = set_spindle_start(xL, yL, data['codes']['x1'], data['codes']['y1'], data['codes']['spindle']['line'], data['newData'], False)
    # if no spindle command yet then find the next one for the correct tool
    if not data['codes']['spindle']['line']:
        for line in data['postData']:
            if 'm3$' in line.replace(' ',''):
                data['codes']['spindle']['line'] = line.strip()
                break
    # add all the code from the selected line to the end
    for line in data['postData']:
        # if we have the first spindle code we don't need it again
        if 'm3$' in line.replace(' ','') and data['codes']['spindle']['code']:
            data['codes']['spindle']['code'] = False
            continue
        # if g0 is the first motion command after the selected line
        if data['codes']['move']['isG0']:
            # if g0 is the current motion command
            if 'g0' in line:
                # no need to process a g53g0 command]
                if 'g53g0' in line or 'g20' in line or 'g21' in line:
                    data['newData'].append(line.strip())
                    continue
                if leadin['do']:
                    error, xL, yL = set_leadin_coordinates(data['codes']['x2'], data['codes']['y2'], scale, leadin['length'], leadin['angle'])
                else:
                    xL = data['codes']['x2']
                    yL = data['codes']['y2']
                data['codes']['spindle']['code'] = set_spindle_start(xL, yL, data['codes']['x2'], data['codes']['y2'], data['codes']['spindle']['line'], data['newData'], True)
                # no need to process any more g0 commands
                data['codes']['move']['isG0'] = False
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
        newData.append('g0x{}y{}'.format(xL, yL))
        leadIn['x'] = x
        leadIn['y'] = y
    else:
        if x and y:
            newData.append('g0x{}y{}'.format(x, y))
        elif x:
            newData.append('g0x{}'.format(x))
        elif y:
            newData.append('g0y{}'.format(y))
    if line:
        newData.append(line)
    if leadIn:
        newData.append('g1x{}y{} (leadin)'.format(leadIn['x'], leadIn['y']))
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
