
'''
plasmac_gcode.py

Copyright (C) 2019 - 2024 Phillip A Carter
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
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import sys
import linuxcnc
import math
import shutil
import time
from subprocess import run as RUN

INI = linuxcnc.ini(os.environ['INI_FILE_NAME'])
DIR = os.path.dirname(os.environ['INI_FILE_NAME'])
if 'axis' in INI.find('DISPLAY', 'DISPLAY'):
    from tkinter import Tk, Label, Text, Scrollbar, Button
    GUI = 'axis'
else:
    from PyQt5.QtCore import Qt
    from PyQt5.QtGui import QIcon
    from PyQt5.QtWidgets import QApplication, QDialog, QScrollArea, QWidget, QVBoxLayout, QLabel, QPushButton, QStyle, QFrame
    GUI = 'qtplasmac'


class Filter():
    def __init__(self, *args):
        super().__init__()
        self.inFile = sys.argv[1]
        # run-from-line files do not require processing
        if os.path.basename(self.inFile) == 'rfl.ngc':
            with open(self.inFile, 'r') as inLines:
                for line in inLines:
                    print(line.strip())
            sys.exit()
        self.set_gui_type()
        self.machine = INI.find('EMC', 'MACHINE')
        self.filteredBkp = f'{self.tmpPath}/filtered_bkp.ngc'
        self.errorFile = f'{self.tmpPath}/gcode_errors.txt'
        self.materialFile = f'{self.machine}_material.cfg'
        self.tmpMaterialFile = f'{self.tmpPath}/{self.machine}_material.gcode'
        self.tmpMatNum = 1000000
        self.tmpMatNam = ''
        self.prefsFile = self.machine + '.prefs'
        response = RUN(['halcmd', 'getp', self.cutTypePin], capture_output=True)
        self.cutType = int(response.stdout.decode())
        response = RUN(['halcmd', 'getp', self.matNumPin], capture_output=True)
        self.currentMat = int(response.stdout.decode())
        response = RUN(['halcmd', 'getp', 'plasmac.max-offset'], capture_output=True)
        zMaxOffset = float(response.stdout.decode())
        RUN(['halcmd', 'setp', self.convBlockPin, '0'])
        RUN(['halcmd', 'setp', 'plasmac.tube-cut', '0'])
        self.metric = ['mm', 4]
        self.imperial = ['in', 6]
        self.units, self.fmt = self.imperial if INI.find('TRAJ', 'LINEAR_UNITS').lower() == 'inch' else self.metric
        if self.units == 'mm':
            self.minDiameter = 32
            self.ocLength = 4
            self.unitsPerMm = 1
            self.blendTolerance = 0.1
        else:
            self.minDiameter = 1.26
            self.ocLength = 0.157
            self.unitsPerMm = 0.03937
            self.blendTolerance = 0.004
        self.unitMultiplier = 1
        self.offsetTopZ = zMaxOffset * self.unitsPerMm * self.unitMultiplier
        self.gcodeList = []
        self.firstMaterial = ''
        self.data = ''
        self.rapidLine = ''
        self.lastG = ''
        self.lastX = 0
        self.lastY = 0
        self.oBurnX = 0
        self.oBurnY = 0
        self.lineNum = 0
        self.lineNumOrg = 0
        self.distMode = 90  # absolute
        self.arcDistMode = 91.1  # incremental
        self.holeVelocity = 60
        self.currentMaterial = [0, False]
        self.overCut = False
        self.holeActive = False
        self.holeEnable = False
        self.arcEnable = False
        self.customDia = False
        self.customLen = False
        self.torchEnable = True
        self.pierceOnly = False
        self.scribing = False
        self.spotting = False  # cannot spot in pierce mode
        self.scribePierce = False  # cannot scribe in pierce mode
        self.offsetG4x = False
        self.zSetup = False
        self.zBypass = False
        self.tubeCut = False
        self.pathBlend = False
        self.convBlock = {'active': False, 'setup': False}
        self.firstMove = False
        self.subList = []
        self.pierceList = {'active': False, 'X': [], 'Y': []}
        self.codeError = False
        self.errors = 'The following errors will affect the process.\n'
        self.errors += 'Errors must be fixed before reloading this file.\n'
        self.errorMath = []
        self.errorMissMat = []
        self.errorNoMat = []
        self.errorBadMat = []
        self.errorTempMat = []
        self.errorTempValid = []
        self.errorTempParm = []
        self.errorNewMat = []
        self.errorEditMat = []
        self.errorWriteMat = []
        self.errorReadMat = []
        self.errorCompMat = []
        self.errorFirstMove = []
        self.errorLines = []
        self.errorG92Offset = []
        self.codeWarn = False
        self.warnings = 'The following warnings may affect the quality of the process.\n'
        self.warnings += 'It is recommended that all warnings are fixed before running this file.\n'
        self.warnUnitsDep = []
        self.warnPierceScribe = []
        self.warnPierceLimit = []
        self.warnMatLoad = []
        self.warnHoleDir = []
        self.warnCompTorch = []
        self.warnCompVel = []
        self.warnFeed = []
        self.warnChar = []
        # create a dict of material numbers and kerf widths
        self.get_materials()
        # setup for custom filtering
        self.cfFile = os.path.join(DIR, 'custom_filter.py')
        if not os.path.isfile(self.cfFile):
            self.cfFile = None
        if self.cfFile:
            exec(open(self.cfFile).read())
        # process the g-code file line by line
        self.process_file()
        # for pierce only mode
        if self.pierceOnly:
            self.gcodeList.append('')
            if self.rapidLine:
                self.gcodeList.append(self.rapidLine)
            self.gcodeList.append('M02 (END)')
        # remove last G00 coordinates if no pierce afterwards
        if self.pierceList['active']:
            if self.convBlock['active']:
                elements = self.convBlock['array_columns'] * self.convBlock['array_rows']
            else:
                elements = 1
            del self.pierceList['X'][-elements:]
            del self.pierceList['Y'][-elements:]
            self.pierceList['active'] = False
        # write the pierce extents hal pins
        if GUI == 'axis':
            RUN(['halcmd', 'setp', 'axisui.x_min_pierce_extent', str(min(self.pierceList['X']) if self.pierceList['X'] else 0)])
            RUN(['halcmd', 'setp', 'axisui.y_min_pierce_extent', str(min(self.pierceList['Y']) if self.pierceList['Y'] else 0)])
            RUN(['halcmd', 'setp', 'axisui.x_max_pierce_extent', str(max(self.pierceList['X']) if self.pierceList['X'] else 0)])
            RUN(['halcmd', 'setp', 'axisui.y_max_pierce_extent', str(max(self.pierceList['Y']) if self.pierceList['Y'] else 0)])
        else:
            RUN(['halcmd', 'setp', 'qtplasmac.x_min_pierce_extent', str(min(self.pierceList['X']) if self.pierceList['X'] else 0)])
            RUN(['halcmd', 'setp', 'qtplasmac.y_min_pierce_extent', str(min(self.pierceList['Y']) if self.pierceList['Y'] else 0)])
            RUN(['halcmd', 'setp', 'qtplasmac.x_max_pierce_extent', str(max(self.pierceList['X']) if self.pierceList['X'] else 0)])
            RUN(['halcmd', 'setp', 'qtplasmac.y_max_pierce_extent', str(max(self.pierceList['Y']) if self.pierceList['Y'] else 0)])
        # error and warning notifications
        if self.codeError or self.codeWarn:  # show errors if any
            self.write_errors()
        else:  # create empty error file if no errors
            with open(self.errorFile, 'w'):
                pass
        # write the final g-code
        self.write_gcode()

    def process_file(self):
        ''' process the file and parse any lines of code
        '''
        with open(self.inFile, 'r') as inLines:
            for line in inLines:
                self.lineNum += 1
                self.lineNumOrg += 1
                # allow custom processing before standard processing
                if self.cfFile:
                    line = self.custom_pre_process(line)
                    if not line:
                        continue
                # get conversational block information if required
                if self.convBlock['active']:
                    if line.startswith('#<array_columns>'):
                        self.convBlock['array_columns'] = int(line.split('=')[1].strip())
                    elif line.startswith('#<array_rows>'):
                        self.convBlock['array_rows'] = int(line.split('=')[1].strip())
                    elif line.startswith('#<array_x_offset>'):
                        self.convBlock['array_x_offset'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<array_y_offset>'):
                        self.convBlock['array_y_offset'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<array_columns>'):
                        self.convBlock['array_columns'] = int(line.split('=')[1].strip())
                    elif line.startswith('#<array_rows>'):
                        self.convBlock['array_rows'] = int(line.split('=')[1].strip())
                    elif line.startswith('#<origin_x_offset>'):
                        self.convBlock['origin_x_offset'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<origin_y_offset>'):
                        self.convBlock['origin_y_offset'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<array_angle>'):
                        self.convBlock['array_angle'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<blk_scale>'):
                        self.convBlock['blk_scale'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<shape_angle>'):
                        self.convBlock['shape_angle'] = float(line.split('=')[1].strip())
                    elif line.startswith('#<shape_mirror>'):
                        self.convBlock['shape_mirror'] = int(line.split('=')[1].strip())
                    elif line.startswith('#<shape_flip>'):
                        self.convBlock['shape_flip'] = int(line.split('=')[1].strip())
                # check if original is a conversational block
                if line.startswith(';conversational block'):
                    self.convBlock['active'] = True
                    RUN(['halcmd', 'setp', self.convBlockPin, '1'])
                # remove leading and trailing whitespace and trailing periods
                line = line.strip().rstrip('.')
                # if empty line then no need to process
                if not line:
                    self.gcodeList.append(line)
                    continue
                # remove line numbers
                if line[0] in 'nN':
                    line = self.remove_line_numbers(line)
                # remove lines with ;qtplasmac filtered G-code file
                if ';qtplasmac filtered G-code file' in line:
                    continue
                # if any obvious illegal characters then comment the line
                if line[0] != ';' and self.illegal_character(line):
                    continue
                # check for material edit
                if line[:3] == '(o=':
                    self.check_material_edit(line)
                    # add comment and material change for temporary material
                    if line[3] == '0':
                        self.lineNum += 3
                        self.gcodeList.append(f';temporary material #{self.tmpMatNum}')
                        self.gcodeList.append(line)
                        self.gcodeList.append(f'M190 P{self.tmpMatNum}')
                        self.gcodeList.append('M66 P3 L3 Q1')
                        if not self.firstMaterial:
                            self.firstMaterial = self.tmpMatNum
                        self.tmpMatNum += 1
                    else:
                        self.gcodeList.append(line)
                    continue
                # full line comments - only remove line numbers
                elif line[0] in ';(':
                    if len(line) > 1:
                        l0 = line[0]
                        tmp = line[1:].strip()
                        if tmp[0] in 'nN':
                            line = f'{l0}{self.remove_line_numbers(tmp)}'
                    self.gcodeList.append(line)
                    continue
                # comments after code - parse the code
                elif ';' in line or '(' in line:
                    for tag in ';(':
                        both = line.split(tag)
                        if len(both) == 1:
                            continue
                        code = self.parse_code(both[0])
                        cmnt = both[1]
                        if code:
                            line = f'{code}{tag}{cmnt}'
                        else:
                            line = f'{tag}{cmnt}'
                # code only - parse the code
                else:
                    line = self.parse_code(line)
                    if not line:
                        continue
                # restore velocity if required
                if self.holeActive:
                    self.lineNum += 1
                    line = f'{line}\nM67 E3 Q0 (arc complete, velocity 100%)'
                    self.holeActive = False
                if line:
                    if self.holeEnable and len(line) and ('X' in line or 'X' in line):
                        self.lastX, self.lastY = self.set_last_coordinates(line, self.lastX, self.lastY)
                    self.gcodeList.append(line)

    def conv_block_setup(self):
        ''' find the zero point for the four extreme blocks '''
        sinX = self.convBlock['array_x_offset'] * self.convBlock['blk_scale'] * math.sin(math.radians(self.convBlock['array_angle']))
        cosX = self.convBlock['array_x_offset'] * self.convBlock['blk_scale'] * math.cos(math.radians(self.convBlock['array_angle']))
        sinY = self.convBlock['array_y_offset'] * self.convBlock['blk_scale'] * math.sin(math.radians(self.convBlock['array_angle']))
        cosY = self.convBlock['array_y_offset'] * self.convBlock['blk_scale'] * math.cos(math.radians(self.convBlock['array_angle']))
        cols = self.convBlock['array_columns'] - 1
        rows = self.convBlock['array_rows'] - 1
        self.convBlock['zero00X'] = (0 * cosX) - (0 * sinY) + self.convBlock['origin_x_offset']
        self.convBlock['zero00Y'] = (0 * cosY) + (0 * sinX) + self.convBlock['origin_y_offset']
        self.convBlock['zero01X'] = (0 * cosX) - (rows * sinY) + self.convBlock['origin_x_offset']
        self.convBlock['zero01Y'] = (rows * cosY) + (0 * sinX) + self.convBlock['origin_y_offset']
        self.convBlock['zero10X'] = (cols * cosX) - (0 * sinY) + self.convBlock['origin_x_offset']
        self.convBlock['zero10Y'] = (0 * cosY) + (cols * sinX) + self.convBlock['origin_y_offset']
        self.convBlock['zero11X'] = (cols * cosX) - (rows * sinY) + self.convBlock['origin_x_offset']
        self.convBlock['zero11Y'] = (rows * cosY) + (cols * sinX) + self.convBlock['origin_y_offset']
        self.convBlock['setup'] = True

    def conv_block_pierce(self, data):
        ''' find the pierce points for the four extreme blocks'''
        x = self.get_axis_value(data, 'X', True)
        y = self.get_axis_value(data, 'Y', True)
        if x is not None and y is not None:
            x = x * self.convBlock['shape_mirror']
            y = y * self.convBlock['shape_flip']
            cos_ = math.cos(math.radians(self.convBlock['array_angle'] + self.convBlock['shape_angle']))
            sin_ = math.sin(math.radians(self.convBlock['array_angle'] + self.convBlock['shape_angle']))
            for point in ['00', '01', '10', '11']:
                pierceX = (x * cos_ - y * sin_) + self.convBlock[f'zero{point}X']
                pierceY = (x * sin_ + y * cos_) + self.convBlock[f'zero{point}Y']
                self.pierceList['X'].append(pierceX)
                self.pierceList['Y'].append(pierceY)
            self.pierceList['active'] = True
        else:
            self.codeWarn = True
            if self.lineNum not in self.warnPierceLimit:
                self.warnPierceLimit.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)

    def parse_code(self, data):
        # set g and m codes to upper case
        data = self.set_to_upper_case(data)
        # allow custom parsing before standard code parsing
        if self.cfFile:
            data = self.custom_pre_parse(data)
            if not data:
                return(None)
        # set the current g-code
        self.lastG = self.set_last_gcode(data, self.lastG)
        # if data starts with axis then preface with last g-code
        if data[0] in 'XYZABC':
            data = f'G{self.lastG} {data}'
        # add leading 0's to G & M codes < 10
        tmp = ''
        while data:
            tmp += data[0]
            if data[0] in 'GM' and data[1].isdigit():
                if len(data) == 2:
                    tmp += '0'
                elif len(data) > 2:
                    if not data[2].isdigit():
                        tmp += '0'
            data = data[1:]
        data = tmp
        # get all G00 coordinates
        if (data[:3] == 'G00' and ('X' in data or 'Y' in data)):
            if self.convBlock['active']:
                if not self.convBlock['setup']:
                    self.conv_block_setup()
                self.conv_block_pierce(data)
            else:
                pierceX = self.lastX
                pierceY = self.lastY
                if 'X' in data and not self.check_math(data, 'X', 'pierce'):
                    pierceX = self.get_axis_value(data, 'X')
                if 'Y' in data and not self.check_math(data, 'Y', 'pierce'):
                    pierceY = self.get_axis_value(data, 'Y')
                self.pierceList['X'].append(pierceX)
                self.pierceList['Y'].append(pierceY)
                self.pierceList['active'] = True
        # reset G00 active flag
        if data[:3] == 'M03' and self.pierceList['active']:
            self.pierceList['active'] = False
        # disallow g92 offsets in gcode
        if 'G92' in data and 'G92.1' not in data:
            self.set_g92_detected()
        # if incremental distance mode fix overburn coordinates
        if data[:3] in ['G00', 'G01'] and self.distMode == 91 and (self.oBurnX or self.oBurnY) and not self.spotting:
            data = self.fix_overburn_incremental_coordinates(data)
        # set path blending
        if 'G64' in data:
            self.pathBlend = True
        # set default units
        if 'G20' in data or 'G21' in data:
            self.set_default_units(data)
        # check for G40 G41 or G42 offsets
        if 'G40' in data or 'G41' in data or 'G42' in data:
            data = self.set_g4x_offsets(data)
        # if z motion is to be kept
        if data.replace(' ', '').startswith('#<keep-z-motion>='):
            self.set_keep_z_motion(data)
        # remove any existing z max moves
        if '[#<_ini[axis_z]max_limit>' in data:  # and self.zSetup:
            return(None)
        # set first movement flag
        if not self.firstMove and (('G00' in data or 'G01' in data) and ('X' in data or 'Y' in data)):
            self.set_first_move()
        # is there an m3 before motion started
        if not self.firstMove and 'M03' in data:
            self.set_no_first_move()
        # if path blending not set and motion started
        if not self.pathBlend and 'M03' in data:
            self.set_default_blending()
        # if pierce only mode
        if self.pierceOnly:
            data = self.do_pierce_only(data)
            if not data:
                return(None)
        # is this a scribe
        if data.startswith('M03 $1 S') and not self.tubeCut:
            self.set_scribing()
        # is this a spot
        if data.startswith('M03 $2 S') and not self.pierceOnly and not self.tubeCut:
            self.spotting = True
        # test for pierce only mode
        elif data.replace(' ', '').startswith('#<pierce-only>=1') or self.cutType == 1:
            self.set_pierce_mode()
        # set overcut length
        elif data.startswith('#<oclength>'):
            self.set_overcut_length(data)
            return data
        # set hole type
        elif data.startswith('#<holes>'):
            self.set_hole_type(data)
            return data
        # set hole diameter
        elif data[:2] == '#<' and data[3:13] == '_diameter>':
            self.set_hole_diameter(data)
            return data
        # set hole velocity
        elif data.startswith('#<h_velocity>'):
            self.set_hole_velocity(data)
            return data
        # tube cutting
        if data.startswith('#<tube-cut>=1'):
            data = self.set_tube_cut(data)
        # change material
        if data[:4] == 'M190':
            self.do_material_change(data)
        # wait for material change
        if 'M66' in data:
            self.material_change_wait()
        # set arc modes
        if 'G90' in data and 'G90.' not in data:
            self.distMode = 90  # absolute distance mode
        elif 'G91' in data and 'G91.' not in data:
            self.distMode = 91  # incremental distance mode
        if 'G91.1' in data:
            self.arcDistMode = 91.1  # incremental arc distance mode
        elif 'G90.1' in data:
            self.arcDistMode = 90.1  # absolute arc distance mode
        # comment out z axis motion
        if 'Z' in data \
            and data.split('Z')[1][0] in '0123456789.- [' \
                and '[axis_z]max_limit' not in data \
                    and not self.zBypass:
            data = self.comment_z_commands(data)
        # check the feed rate
        if 'F' in data and not self.tubeCut:
            data = self.check_f_word(data)
        # if an arc command
        if (data[:3] == 'G02' or data[:3] == 'G03'):
            data = self.do_arc(data)
        # if torch off, flag it then self.gcodeList.append it
        elif data[:6] == 'M62 P3' or data[:6] == 'M64 P3':
            self.torchEnable = False
        # if torch on, flag it then self.gcodeList.append it
        elif data[:6] == 'M63 P3' or data[:6] == 'M65 P3':
            self.torchEnable = True
        # if spindle off
        elif data[:3] == 'M05':
            data = self.spindle_off(data)
        # if program end
        elif data[:3] in ['M02', 'M30'] or data[0] == '%':
            data = self.program_end(data)
        # allow custom parsing after standard code parsing
        if self.cfFile:
            data = self.custom_post_parse(data)
            if not data:
                return(None)
        return data

    def custom_pre_process(self, line):
        ''' placeholder function for custom processing
            before standard processing '''
        return(line)

    def custom_pre_parse(self, data):
        ''' placeholder function for custom parsing
            before standard code parsing '''
        return(data)

    def custom_post_parse(self, data):
        ''' placeholder function for custom parsing
            after standard code parsing '''
        return(data)

    def write_gcode(self):
        with open(self.filteredBkp, 'w') as outFile:
            for data in self.gcodeList:
                print(data)
                outFile.write(f'{data}\n')
            print(';qtplasmac filtered G-code file')
            outFile.write(';qtplasmac filtered G-code file')

    def set_to_upper_case(self, data):
        tmp = ''
        keep = False
        for d in data:
            if d in '#':
                keep = True
                tmp += d
            elif d in '>':
                keep = False
                tmp += d
            else:
                if keep:
                    tmp += d
                else:
                    tmp += d.upper()
        return tmp

    def get_axis_value(self, data, axis, block=False):
        tmp1 = data.split(axis)[1].replace(' ', '')
        # we can use default math in conversational block code
        if block and tmp1[0] == '[':
            tmp1 = tmp1[1:]
        # if first char is not valid return None
        if not tmp1[0].isdigit() and not tmp1[0] == '.' and not tmp1[0] == '-':
            return None
        n = 0
        tmp2 = ''
        while 1:
            if tmp1[n].isdigit() or tmp1[n] == '.' or tmp1[n] == '-':
                tmp2 += tmp1[n]
                n += 1
            else:
                break
            if n >= len(tmp1):
                break
        return float(tmp2)

    def set_last_coordinates(self, data, Xpos, Ypos):
        if data[0] in 'GXY':
            if 'X' in data:
                if self.get_axis_value(data, 'X') is not None:
                    if self.distMode == 91:  # get absolute X from incremental X position
                        Xpos += self.get_axis_value(data, 'X')
                    else:  # get absolute X
                        Xpos = self.get_axis_value(data, 'X')
            if 'Y' in data:
                if self.get_axis_value(data, 'Y') is not None:
                    if self.distMode == 91:  # get absolute Y from incremental Y position
                        Ypos += self.get_axis_value(data, 'Y')
                    else:  # get absolute X
                        Ypos = self.get_axis_value(data, 'Y')
        return Xpos, Ypos

    def check_math(self, data, axis, code='arc'):
        ''' check if math used or explicit values
        '''
        tmp1 = data.split(axis)[1]
        if tmp1.startswith('[') or tmp1.startswith('#'):
            if code == 'pierce':
                self.codeWarn = True
                if self.lineNum not in self.warnPierceLimit:
                    self.warnPierceLimit.append(self.lineNum)
                    self.errorLines.append(self.lineNumOrg)
            else:
                self.set_code_error()
                if self.lineNum not in self.errorMath:
                    self.errorMath.append(self.lineNum)
                    self.errorLines.append(self.lineNumOrg)
            return True
        return False

    def illegal_character(self, data):
        ''' if illegal characters found then comment the line
        '''
# FIXME 1 we could probably do more here
# FIXME 2 not even sure we should bother with this
#        maybe just leave it to the interpreter
        code = data.replace(' ', '')
        err = 0
        # single character code with invalid character
        if len(code) == 1 and code not in '/;%':
            err = 1
        # comment is missing a parenthesis
        elif ('(' in code and code[-1] != ')') or ((code[-1] == ')' and '(' not in code)):
            err = 2
        # line starts with two alpha characters
        elif code[0].isalpha() and code[1].isalpha():
            err = 3
        # invalid first character
        elif not code[0].isalpha() and code[0] not in '/;(#@^%':
            err = 4
        # process numbered and named parameters
        if code[0] == '#' or code[:2] == '#<':
            code = code.lstrip('#')
            # remove trailing comment for further processing
            if '(' in code:
                code = code.split('(')[0].strip()
            # parameter is missing equals sign
            if '=' not in code:
                err = 5
            else:
                try:
                    # left = parameter, right = value (we don't process right side yet)
                    left, right = code.split('=')
                    # variable is not currently used
                    del right
                    # named parameter is missing a chevron
                    if left[0] == '<' and '>' not in left:
                        err = 6
                    # numbered parameter is not a number
                    elif left[0] != '<' and not left.isdigit():
                        err = 7
                except:
                    # parameter has no value
                    err = 8
        if err:
            errs = [None]
            errs.append('single character line with invalid character')
            errs.append('comment is missing a parenthesis')
            errs.append('line starts with two alpha characters')
            errs.append('invalid first character')
            errs.append('parameter is missing equals sign')
            errs.append('named parameter is missing a chevron')
            errs.append('numbered parameter is not a number')
            errs.append('parameter has no value')
            self.codeWarn = True
            self.warnChar.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
            self.gcodeList.append(f';{data} |{errs[err]}')
        return err

    def remove_line_numbers(self, data):
        idx = 1
        while data[idx].isdigit() or data[idx] in ' .':
            idx += 1
        data = f'{data[idx:]}'
        return data

    def set_last_gcode(self, data, previous):
        new = ''
        idx = data.rfind('G') + 1
        if idx:
            while data[idx].isdigit():
                new += data[idx]
                idx += 1
                if idx == len(data):
                    break
        if len(new):
            return new
        else:
            return previous

    def set_default_units(self, data):
        if 'G21' in data:
            if self.units == 'in':
                self.unitMultiplier = 25.4
                if not self.customDia:
                    self.minDiameter = 32
                if not self.customLen:
                    self.ocLength = 4
        else:
            if self.units == 'mm':
                self.unitMultiplier = 0.03937
                if not self.customDia:
                    self.minDiameter = 1.26
                if not self.customLen:
                    self.ocLength = 0.157

    def set_g4x_offsets(self, data):
        if 'G40' in data:
            self.offsetG4x = False
        else:
            self.offsetG4x = True
            if 'kerf_width-f]>' in data and self.unitMultiplier != 1:
                data = data.replace('#<_hal[qtplasmac.kerf_width-f]>',
                                    f'[#<_hal[qtplasmac.kerf_width-f]> * {self.unitMultiplier}]')
        return(data)

    def set_first_move(self):
        self.firstMove = True
        if not self.zSetup and not self.zBypass:
            self.lineNum += 1
            moveTopZ = 'G53 G0 Z[[#<_ini[axis_z]max_limit> - '
            moveTopZ += f'{self.offsetTopZ}] * {self.unitMultiplier:.3f}]'
            moveTopZ += ' (Z just below max height)'
            self.gcodeList.append(moveTopZ)
            self.zSetup = True

    def set_no_first_move(self):
        self.set_code_error()
        self.errorFirstMove.append(self.lineNum)
        self.errorLines.append(self.lineNumOrg)

    def set_g92_detected(self):
        self.set_code_error()
        self.errorG92Offset.append(self.lineNum)
        self.errorLines.append(self.lineNumOrg)

    def set_default_blending(self):
        blend = self.blendTolerance * self.unitMultiplier
        self.gcodeList.append(f'G64 P{blend}')
        self.pathBlend = True

    def set_scribing(self):
        if self.pierceOnly:
            self.codeWarn = True
            self.warnPierceScribe.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
            self.scribing = False
        else:
            self.scribing = True

    def set_pierce_mode(self):
        if self.scribing:
            self.codeWarn = True
            self.warnPierceScribe.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
        elif not self.pierceOnly:
            self.pierceOnly = True
            self.pierces = 0
            self.rapidLine = ''

    def do_pierce_only(self, data):
            if 'Z' in data \
                and data.split('Z')[1][0] in '0123456789.- [' \
                    and '[axis_z]max_limit' not in data:
                data = self.comment_z_commands(data)
            # Don't pierce spotting operations
            if data[:6] == 'M03 $2':
                self.spotting = True
                self.gcodeList.append('(Ignoring spotting operation as pierce-only is active)')
                return None
            if data[:6] == 'M03 $1':
                self.scribePierce = True
                self.gcodeList.append('(Ignoring scribing operation as pierce-only is active)')
                return None
            # Ignore spotting blocks when pierceOnly
            if self.spotting:
                if data[:6] == 'M05 $2':
                    self.firstMove = False
                    self.spotting = False
                return None
            # Ignore spotting blocks when pierceOnly
            if self.scribePierce:
                if data[:6] == 'M05 $1':
                    self.firstMove = False
                    self.scribePierce = False
                return None
            # set offsets for pierce X/Y coordinates
            if data[:3] == 'G00':
                idx, brackets, start, end = 0, 0, 0, 0
                tmp = ''
                for axis in 'XY':
                    if axis in data:
                        if GUI == 'axis':
                            offset = f"[#<_hal[axisui.{axis.lower()}-pierce-offset]> * {self.unitMultiplier}]"
                        else:
                            offset = f"[#<_hal[qtplasmac.{axis.lower()}_pierce_offset-f]> * {self.unitMultiplier}]"
                        start = data.index(axis)
                        tmp = data[:start]
                        idx = start + 1
                        while data[idx] == ' ':
                            idx += 1
                        if data[idx] == '[':
                            while 1:
                                if data[idx] == '[':
                                    brackets += 1
                                elif data[idx] == ']':
                                    brackets -= 1
                                if not brackets:
                                    end = idx
                                    break
                                idx += 1
                                if idx == len(data):
                                    break
                            data = f"{tmp}{data[start]}[{data[start+1:end+1]} + {offset}]{data[end+1:]}"
                        else:
                            while 1:
                                if data[idx] in 'XYZABC ':
                                    if '2.3652' in data:
                                        print(f';idx:{idx}   char:{data[idx]}')
                                    break
                                idx += 1
                                if idx == len(data):
                                    break
                            end = idx
                            data = f"{tmp}{data[start]}[{data[start+1:end]} + {offset}]{data[end:]}"
                self.rapidLine = data
                return None
            # create the pierce only gcode
            elif data[:3] == 'M03':
                self.pierces += 1
                self.gcodeList.append(f'(Pierce #{self.pierces})')
                self.gcodeList.append(self.rapidLine)
                self.gcodeList.append('M03 $0 S1')
                self.gcodeList.append('G91')
                self.gcodeList.append('G01 X.000001')
                self.gcodeList.append('G90\nM05 $0')
                self.rapidLine = ''
                return None
            if not self.pierces or data.startswith('O') or data.startswith('#'):
                self.gcodeList.append(data)
            return None

    def set_keep_z_motion(self, data):
        if data.split('=')[1].strip() == '1':
            self.zBypass = True
        else:
            self.zBypass = False

    def comment_z_commands(self, data):
        # if no other axes comment the complete data
        if 1 not in [c in data for c in 'XYABCUVW']:
            return(f'({data} Z axis commented out)')
        # other axes in data so comment out the Z axis only
        else:
            newline = ''
            newz = ''
            commenting = 0
            maths = 0
            for bit in data:
                if commenting:
                    if bit == '[':
                        newz += bit
                        maths += 1
                    elif bit == ']':
                        newz += bit
                        maths -= 1
                    elif maths:
                        newz += bit
                    elif bit in '0123456789.- ':
                        newz += bit
                    else:
                        commenting = 0
                        if newz:
                            newz = newz.rstrip()
                        newline += bit
                elif bit == 'Z':
                    commenting = 1
                    newz += '(' + bit
                else:
                    newline += bit
            if self.holeActive:
                self.lineNum += 1
                self.gcodeList.append('M67 E3 Q0 (arc complete, velocity 100%)')
                self.holeActive = False
            return(f'{newline} {newz} Z axis commented out)')

    def check_f_word(self, data):
        begin, inFeed = data.split('F', 1)
        inFeed = inFeed.replace(' ', '')
        # if feed rate from material file
        if inFeed.startswith('#<_hal[plasmac.cut-feed-rate]>'):
            # change feed rate if g-code file not in same units as machine units
            if self.unitMultiplier != 1:
                data = f'{begin}F[#<_hal[plasmac.cut-feed-rate]> * {self.unitMultiplier}]'
            return data
        # if explicit feed rate
        rawFeed = ''
        codeFeed = 0.0
        # get feed rate if it is digits
        while len(inFeed) and (inFeed[0].isdigit() or inFeed[0] == '.'):
            rawFeed = rawFeed + inFeed[0]
            inFeed = inFeed[1:].lstrip()
        if not rawFeed:
            return data
        codeFeed = float(rawFeed)
        matFeed = float(self.materialDict[self.currentMaterial[0]][0]) * self.unitMultiplier
        # this may need scaling ...
        diff = 1
        if (codeFeed < matFeed - diff or codeFeed > matFeed + diff):
            self.codeWarn = True
            self.warnFeed.append([self.lineNum, rawFeed, self.currentMaterial[0], self.materialDict[self.currentMaterial[0]][0]])
            self.errorLines.append(self.lineNumOrg)
        return data

    def set_tube_cut(self, data):
        self.tubeCut = True
        self.zBypass = True
        RUN(['halcmd', 'setp', 'plasmac.tube-cut', '1'])
        self.lineNum += 3
        data = f'\n;tube cutting is experimental\n{data}\n'
        return data

    def spindle_off(self, data):
        if len(data) == 3 or (len(data) > 3 and not data[3].isdigit()):
            self.firstMove = False
            # restore velocity if required
            if self.holeActive:
                self.lineNum += 1
                data = f'{data}\nM68 E3 Q0 (arc complete, velocity 100%)'
                self.holeActive = False
            # if torch off, allow torch on
            if not self.torchEnable:
                self.lineNum += 1
                data = f'{data}\nM65 P3 (enable torch)'
                self.torchEnable = True
            # if not pierce mode reset spotting flag
            if not self.pierceOnly:
                self.spotting = False
        return data

    def program_end(self, data):
        # restore velocity if required
        if self.holeActive:
            self.lineNum += 1
            data = f'M68 E3 Q0 (arc complete, velocity 100%)\n{data}'
            self.holeActive = False
        # if torch off, allow torch on
        if not self.torchEnable:
            self.lineNum += 1
            data = f'M65 P3 (enable torch)\n{data}'
            self.torchEnable = True
        # restore hole sensing to default
        if self.holeEnable:
            self.lineNum += 1
            data = f'#<holes>=0 (disable hole sensing)\n{data}'
            self.holeEnable = False
        if self.firstMaterial:
            RUN(['halcmd', 'setp', self.matNumPin, str(self.firstMaterial)])
        return data

    def set_gui_type(self):
        # assume gui to be qtplasmac unless a specific gui selected
        if GUI == 'axis':
            self.dialog = tkGui()
            self.tmpPath = '/tmp/plasmac'
            self.cutTypePin = 'axisui.cut-type'
            self.matNumPin = 'axisui.material-change-number'
            self.convBlockPin = 'axisui.conv-block-loaded'
            self.matTmpPin = 'axisui.material-temp'
            self.matReloadPin = 'axisui.material-reload'
        else:
            self.dialog = qtGui()
            self.tmpPath = '/tmp/qtplasmac'
            self.cutTypePin = 'qtplasmac.cut_type'
            self.matNumPin = 'qtplasmac.material_change_number'
            self.convBlockPin = 'qtplasmac.conv_block_loaded'
            self.matTmpPin = 'qtplasmac.material_temp'
            self.matReloadPin = 'qtplasmac.material_reload'

##############################################################################
# HOLES AND ARCS
##############################################################################
    def do_arc(self, data):
        if self.holeEnable and not self.convBlock['active']:
            stop = False
            # check if we can read the values correctly
            if 'X' in data:
                stop = self.check_math(data, 'X')
            if 'Y' in data and not stop:
                stop = self.check_math(data, 'Y')
            if 'I' in data and not stop:
                stop = self.check_math(data, 'I')
            if 'J' in data and not stop:
                stop = self.check_math(data, 'J')
            if not stop:
                data = self.check_if_hole(data)
        return(data)

    def set_overcut_length(self, data):
        if '=' not in data:
            return
        self.ocLength = float(data.split('=')[1])
        self.customLen = True

    def set_hole_type(self, data):
        if '=' not in data:
            return
        hT = int(data.split('=')[1])
        hE = [None, True, True, True, True, False]
        aE = [None, False, False, True, True, False]
        oC = [None, False, True, False, True, False]
        self.holeEnable = hE[hT]
        self.arcEnable = aE[hT]
        self.overCut = oC[hT]

    def set_hole_diameter(self, data):
        if '=' not in data:
            return
        self.minDiameter = float(data.split('=')[1])
        self.customDia = True
        # m_diameter and i_diameter are kept for legacy purposes, they may be removed in future
        if '#<m_d' in data or '#<i_d' in data:
            self.codeWarn = True
            self.warnUnitsDep.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)

    def set_hole_velocity(self, data):
        if '=' not in data:
            return
        self.holeVelocity = float(data.split('=')[1])

    def check_if_hole(self, data):
        I, J, isHole = 0, 0, 0
        if self.distMode == 91:  # get absolute X & Y from incremental coordinates
            endX = self.lastX + self.get_axis_value(data, 'X') if 'X' in data else self.lastX
            endY = self.lastY + self.get_axis_value(data, 'Y') if 'Y' in data else self.lastY
        else:  # get absolute X & Y
            endX = self.get_axis_value(data, 'X') if 'X' in data else self.lastX
            endY = self.get_axis_value(data, 'Y') if 'Y' in data else self.lastY
        if self.arcDistMode == 90.1:  # convert I & J to incremental to make diameter calculations easier
            if 'I' in data:
                I = self.get_axis_value(data, 'I') - self.lastX
            if 'J' in data:
                J = self.get_axis_value('J') - self.lastY
        else:  # get incremental I & J
            if 'I' in data:
                I = self.get_axis_value(data, 'I')
            if 'J' in data:
                J = self.get_axis_value(data, 'J')
        if self.lastX and self.lastY and self.lastX == endX and self.lastY == endY:
            isHole = True
        diameter = self.get_hole_diameter(data, I, J, isHole)
        if isHole and self.overCut and diameter <= self.minDiameter and self.ocLength:
            data = self.overburn(data, I, J, diameter / 2)
        else:
            self.lastX = endX
            self.lastY = endY
        return(data)

    def get_hole_diameter(self, data, I, J, isHole):
        ''' get hole diameter and set the velocity percentage
        '''
        if self.offsetG4x:
            diameter = math.sqrt((I ** 2) + (J ** 2)) * 2
        else:
            if self.currentMaterial[0] in self.materialDict:
                kerfWidth = self.materialDict[self.currentMaterial[0]][1] / 2 * self.unitMultiplier
            else:
                kerfWidth = 0
            diameter = (math.sqrt((I ** 2) + (J ** 2)) * 2) + kerfWidth
        # velocity reduction is required
        if diameter <= self.minDiameter and (isHole or self.arcEnable):
            if self.offsetG4x:
                self.lineNum += 1
                self.gcodeList.append(';M67 E3 Q0 (inactive due to G41)')
                self.codeWarn = True
                self.warnCompVel.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)
            elif not self.holeActive:
                if diameter <= self.minDiameter:
                    self.lineNum += 1
                    self.gcodeList.append(f'M67 E3 Q{self.holeVelocity} (arc diameter:{diameter:0.3f}, velocity:{self.holeVelocity}%)')
                self.holeActive = True
            if data[:3] == 'G02' and isHole:
                self.codeWarn = True
                self.warnHoleDir.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)
        # no velocity reduction required
        else:
            if self.holeActive:
                self.lineNum += 1
                self.gcodeList.append('M67 E3 Q0 (arc complete, velocity 100%)')
                self.holeActive = False
        return diameter

    def overburn(self, data, I, J, radius):
        ''' turn torch off and move 4mm (0.157") past hole end
        '''
        centerX = self.lastX + I
        centerY = self.lastY + J
        cosA = math.cos(self.ocLength / radius)
        sinA = math.sin(self.ocLength / radius)
        cosB = ((self.lastX - centerX) / radius)
        sinB = ((self.lastY - centerY) / radius)
        self.lineNum += 1
        if self.offsetG4x:
            data = f'{data}\n;M62 P3 (inactive due to G41)'
            self.codeWarn = True
            self.warnCompTorch.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
        else:
            data = f'{data}\nM62 P3 (disable torch)'
            self.torchEnable = False
        # clockwise arc
        if data[:3] == 'G02':
            X = centerX + radius * ((cosB * cosA) + (sinB * sinA))
            Y = centerY + radius * ((sinB * cosA) - (cosB * sinA))
            G = '02'
        # counterclockwise arc
        else:
            X = centerX + radius * ((cosB * cosA) - (sinB * sinA))
            Y = centerY + radius * ((sinB * cosA) + (cosB * sinA))
            G = '03'
        self.lineNum += 1
        # restore I & J back to absolute from incremental conversion in check_if_hole
        if self.arcDistMode == 90.1:
            I += self.lastX
            J += self.lastY
        self.oBurnX = X - self.lastX
        self.oBurnY = Y - self.lastY
        if self.distMode == 91:  # output incremental X & Y
            data = f'{data}\nG{G} X{self.oBurnX:0.{self.fmt}f} Y{self.oBurnY:0.{self.fmt}f} I{I:0.{self.fmt}f} J{J:0.{self.fmt}f} (overburn)'
        else:  # output absolute X & Y
            data = f'{data}\nG{G} X{X:0.{self.fmt}f} Y{Y:0.{self.fmt}f} I{I:0.{self.fmt}f} J{J:0.{self.fmt}f} (overburn)'
        return(data)

    def fix_overburn_incremental_coordinates(self, data):
        newData = data[:3]
        if 'X' in data and 'Y' in data:
            x = self.get_axis_value(data, 'X')
            if x is not None:
                newData += f'X{x - self.oBurnX:0.4f}'
            y = self.get_axis_value(data, 'Y')
            if y is not None:
                newData += f'Y{y - self.oBurnY:0.4f}'
            return newData
        elif 'X' in data:
            x = self.get_axis_value(data, 'X')
            if x is not None:
                newData += f'X{x - self.oBurnX:0.4f} Y{self.oBurnY:0.4f}'
            return newData
        elif 'Y' in data:
            y = self.get_axis_value(data, 'Y')
            if y is not None:
                newData += f'X{self.oBurnX:0.4f} Y{y - self.oBurnY:0.4f}'
            return newData
        else:
            return data

##############################################################################
# MATERIAL HANDLING
##############################################################################

    def do_material_change(self, data):
        code = data.replace('M190', '').strip()
        # check for missing p or material
        if not len(code) or code[0] != 'P' or code == 'P':
            self.set_code_error()
            self.errorNoMat.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
            return
        # get the material number
        try:
            num = int(code.replace('P', ''))
        except:
            num = -2
        if num < -1:
            self.set_code_error()
            self.errorBadMat.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
            return
        self.currentMaterial[0] = num
        self.currentMaterial[1] = True
        # check if material exists in dict
        if self.currentMaterial[0] not in self.materialDict and self.currentMaterial[0] < 1000000 and self.currentMaterial[0] != -1:
            self.set_code_error()
            self.errorMissMat.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)
            return
        if not self.firstMaterial:
            self.firstMaterial = self.currentMaterial[0]

    def material_change_wait(self):
        if self.offsetG4x:
            self.set_code_error()
            self.errorCompMat.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)

    def check_material_edit(self, data):
        tmpMaterial = False
        newMaterial = []
        th = 0
        kw = jh = jd = pe = gp = 0.0
        cm = 1
        ca = 15
        cv = 100
        if self.tubeCut:
            ph = ch = fr = 0.0
        try:
            if ('ph=' in data and 'pd=' in data and 'ch=' in data and 'fr=' in data) or \
               ('pd=' in data and self.tubeCut):
                if '(o=0' in data:
                    tmpMaterial = True
                    nu = self.tmpMatNum
                    na = f'Temporary {self.tmpMatNum}'
                    self.tmpMatNam = na
                    newMaterial.append(0)
                elif '(o=1' in data and 'nu=' in data and 'na=' in data:
                    newMaterial.append(1)
                elif '(o=2' in data and 'nu=' in data and 'na=' in data:
                    newMaterial.append(2)
                if newMaterial[0] in [0, 1, 2]:
                    for item in data.split('(')[1].split(')')[0].split(','):
                        # mandatory items
                        if 'nu=' in item and not tmpMaterial:
                            nu = int(item.split('=')[1])
                        elif 'na=' in item:
                            na = item.split('=')[1].strip()
                            if tmpMaterial:
                                self.tmpMatNam = na
                        elif 'ph=' in item:
                            ph = float(item.split('=')[1])
                            if self.unitMultiplier != 1:
                                ph = ph / self.unitMultiplier
                        elif 'pd=' in item:
                            pd = float(item.split('=')[1])
                        elif 'ch=' in item:
                            ch = float(item.split('=')[1])
                            if self.unitMultiplier != 1:
                                ch = ch / self.unitMultiplier
                        elif 'fr=' in item:
                            fr = float(item.split('=')[1])
                            if self.unitMultiplier != 1:
                                fr = fr / self.unitMultiplier
                        # optional items
                        elif 'kw=' in item:
                            kw = float(item.split('=')[1])
                            if self.unitMultiplier != 1:
                                kw = kw / self.unitMultiplier
                        elif 'th=' in item:
                            th = int(item.split('=')[1])
                        elif 'jh=' in item:
                            jh = float(item.split('=')[1])
                            if self.unitMultiplier != 1:
                                jh = ph / self.unitMultiplier
                        elif 'jd=' in item:
                            jd = float(item.split('=')[1])
                        elif 'ca=' in item:
                            ca = float(item.split('=')[1])
                        elif 'cv=' in item:
                            cv = float(item.split('=')[1])
                        elif 'pe=' in item:
                            pe = float(item.split('=')[1])
                        elif 'gp=' in item:
                            gp = float(item.split('=')[1])
                        elif 'cm=' in item:
                            cm = float(item.split('=')[1])
                    for i in [nu, na, kw, th, ph, pd, jh, jd, ch, fr, ca, cv, pe, gp, cm]:
                        newMaterial.append(i)
                    if newMaterial[0] == 0:
                        self.set_temporary_material(newMaterial)
                    elif nu in self.materialDict and newMaterial[0] == 1:
                        self.set_code_error()
                        self.errorNewMat.append(self.lineNum)
                        self.errorLines.append(self.lineNumOrg)
                    else:
                        self.rewrite_material_file(data, newMaterial)
                else:
                    self.set_code_error()
                    self.errorEditMat.append(self.lineNum)
                    self.errorLines.append(self.lineNumOrg)
            else:
                self.set_code_error()
                self.errorTempParm.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)
        except:
            self.set_code_error()
            self.errorTempValid.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)

    def set_temporary_material(self, data):
        outFile = open(self.tmpMaterialFile, 'w')
        self.write_one_material(data, outFile, self.errorTempMat)
        outFile.close()
        self.materialDict[self.tmpMatNum] = [data[10], data[3]]
        RUN(['halcmd', 'setp', self.matTmpPin, str(self.tmpMatNum)])
        self.currentMaterial[0] = self.tmpMatNum
        matDelay = time.time()
        while 1:
            if time.time() > matDelay + 3:
                self.codeWarn = True
                self.warnMatLoad.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)
                break
            response = RUN(['halcmd', 'getp', self.matTmpPin], capture_output=True)
            if not int(response.stdout.decode()):
                break

    def rewrite_material_file(self, data, newMaterial):
        copyFile = f'{self.materialFile}.bkp'
        shutil.copy(self.materialFile, copyFile)
        inFile = open(copyFile, 'r')
        outFile = open(self.materialFile, 'w')
        while 1:
            data = inFile.readline()
            if not data:
                break
            if not data.strip().startswith('[MATERIAL_NUMBER_'):
                outFile.write(data)
            else:
                break
        while 1:
            if not data:
                self.write_one_material(newMaterial, outFile, self.errorWriteMat)
                break
            if data.strip().startswith('[MATERIAL_NUMBER_'):
                mNum = int(data.split('NUMBER_')[1].replace(']', ''))
                if mNum == newMaterial[1]:
                    self.write_one_material(newMaterial, outFile, self.errorWriteMat)
            if mNum != newMaterial[1]:
                outFile.write(data)
            data = inFile.readline()
            if not data:
                break
        if newMaterial[1] not in self.materialDict:
            self.write_one_material(newMaterial, outFile, self.errorWriteMat)
        inFile.close()
        outFile.close()
        RUN(['halcmd', 'setp', self.matReloadPin, '1'])
        self.get_materials()
        matDelay = time.time()
        while 1:
            if time.time() > matDelay + 3:
                self.codeWarn = True
                self.warnMatLoad.append(self.lineNum)
                self.errorLines.append(self.lineNumOrg)
                break
            response = RUN(['halcmd', 'getp', self.matReloadPin], capture_output=True)
            if not int(response.stdout.decode()):
                break

    def write_one_material(self, mat, file, err):
        try:
            file.write(f'[MATERIAL_NUMBER_{mat[1]}]\n')
            file.write(f'NAME               = {mat[2]}\n')
            file.write(f'KERF_WIDTH         = {mat[3]}\n')
            file.write(f'THC                = {mat[4]}\n')
            file.write(f'PIERCE_HEIGHT      = {mat[5]}\n')
            file.write(f'PIERCE_DELAY       = {mat[6]}\n')
            file.write(f'PUDDLE_JUMP_HEIGHT = {mat[7]}\n')
            file.write(f'PUDDLE_JUMP_DELAY  = {mat[8]}\n')
            file.write(f'CUT_HEIGHT         = {mat[9]}\n')
            file.write(f'CUT_SPEED          = {mat[10]}\n')
            file.write(f'CUT_AMPS           = {mat[11]}\n')
            file.write(f'CUT_VOLTS          = {mat[12]}\n')
            file.write(f'PAUSE_AT_END       = {mat[13]}\n')
            file.write(f'GAS_PRESSURE       = {mat[14]}\n')
            file.write(f'CUT_MODE           = {mat[15]}\n')
            file.write('\n')
        except:
            self.set_code_error()
            err.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)

    def get_materials(self):
        ''' create a dict of material numbers, feed rates and kerf widths
        '''
        try:
            with open(self.prefsFile, 'r') as rFile:
                fRate = kWidth = 0.0
                for data in rFile:
                    if data.startswith('Cut feed rate'):
                        fRate = float(data.split('=')[1].strip())
                    if data.startswith('Kerf width'):
                        kWidth = float(data.split('=')[1].strip())
            mNumber = 0
            with open(self.materialFile, 'r') as mFile:
                self.materialDict = {mNumber: [fRate, kWidth]}
                while 1:
                    data = mFile.readline()
                    if not data:
                        break
                    elif data.startswith('[MATERIAL_NUMBER_') and data.strip().endswith(']'):
                        mNumber = int(data.rsplit('_', 1)[1].strip().strip(']'))
                        break
                while 1:
                    data = mFile.readline()
                    if not data:
                        self.materialDict[mNumber] = [fRate, kWidth]
                        break
                    elif data.startswith('[MATERIAL_NUMBER_') and data.strip().endswith(']'):
                        self.materialDict[mNumber] = [fRate, kWidth]
                        mNumber = int(data.rsplit('_', 1)[1].strip().strip(']'))
                    elif data.startswith('CUT_SPEED'):
                        fRate = float(data.split('=')[1].strip())
                    elif data.startswith('KERF_WIDTH'):
                        kWidth = float(data.split('=')[1].strip())
        except:
            self.set_code_error()
            self.errorReadMat.append(self.lineNum)
            self.errorLines.append(self.lineNumOrg)

##############################################################################
# ERROR AND WARNING MESSAGING
##############################################################################

    def set_code_error(self):
        if not self.codeError:
            self.lineNum -= 1
        self.codeError = True

    def write_errors(self):
        errorText = ''
        warnText = ''
        with open(self.errorFile, 'w') as errFile:
            for data in self.errorLines:
                    errFile.write(f'{data}\n')
        if self.codeError:
            print('M02 (end program)')
            if self.errorMath:
                msg = 'G02 and G03 moves require explicit values if hole sensing is enabled.\n'
                errorText += self.message_set(self.errorMath, msg)
            if self.errorMissMat:
                msg = 'The Material selected is missing from the material file.\n'
                errorText += self.message_set(self.errorMissMat, msg)
            if self.errorNoMat:
                msg = 'A Material was not specified after M190.\n'
                errorText += self.message_set(self.errorNoMat, msg)
            if self.errorBadMat:
                msg = 'An invalid Material was specified after M190 P.\n'
                errorText += self.message_set(self.errorBadMat, msg)
            if self.errorTempMat:
                msg = 'Error attempting to add a temporary material.\n'
                errorText += self.message_set(self.errorTempMat, msg)
            if self.errorTempValid:
                msg = 'Invalid parameter in temporary material.\n'
                errorText += self.message_set(self.errorTempValid, msg)
            if self.errorTempParm:
                msg = 'Parameter missing from temporary material.\n'
                errorText += self.message_set(self.errorTempParm, msg)
            if self.errorNewMat:
                msg = 'Cannot add new material, number is in use.\n'
                errorText += self.message_set(self.errorNewMat, msg)
            if self.errorEditMat:
                msg = 'Cannot add or edit material from G-Code file with invalid parameter or value.\n'
                errorText += self.message_set(self.errorEditMat, msg)
            if self.errorWriteMat:
                msg = 'Error attempting to write to the material file.\n'
                errorText += self.message_set(self.errorWriteMat, msg)
            if self.errorReadMat:
                msg = 'Error attempting to read from the material file.\n'
                errorText += self.message_set(self.errorReadMat, msg)
            if self.errorCompMat:
                msg = 'Cannot validate a material change with cutter compensation active.\n'
                errorText += self.message_set(self.errorCompMat, msg)
            if self.errorFirstMove:
                msg = 'M03 command detected before movement.\n'
                errorText += self.message_set(self.errorFirstMove, msg)
            if self.errorG92Offset:
                msg = 'G92 offsets are not allowed.\n'
                errorText += self.message_set(self.errorG92Offset, msg)
        if self.codeWarn:
            if self.warnUnitsDep:
                msg = '<m_diameter> and #<i_diameter> are deprecated in favour of #<h_diameter>.\n'
                msg += 'The diameter will be set in the current units of the G-Code file.\n'
                warnText += self.message_set(self.warnUnitsDep, msg)
            if self.warnPierceScribe:
                msg = 'Pierce only mode is invalid while scribing.\n'
                warnText += self.message_set(self.warnPierceScribe, msg)
            if self.warnPierceLimit:
                msg = 'Pierce limit checks require explicit X and Y values (no math) for G00 moves.\n'
                msg += 'Pierce points will be disregarded in G-Code limit checking.\n'
                warnText += self.message_set(self.warnPierceLimit, msg)
            if self.warnMatLoad:
                msg = 'Materials were not reloaded in a timely manner.\n'
                msg = 'Try reloading the G-Code file.\n'
                warnText += self.message_set(self.warnMatLoad, msg)
            if self.warnHoleDir:
                msg = 'This cut appears to be a hole, did you mean to cut it clockwise?\n'
                warnText += self.message_set(self.warnHoleDir, msg)
            if self.warnCompTorch:
                msg = 'Cannot enable/disable torch with G41/G42 compensation active.\n'
                warnText += self.message_set(self.warnCompTorch, msg)
            if self.warnCompVel:
                msg = 'Cannot reduce velocity with G41/G42 compensation active.\n'
                warnText += self.message_set(self.warnCompVel, msg)
            if self.warnFeed:
                for n in range(0, len(self.warnFeed)):
                    msg0 = 'Line'
                    msg1 = 'does not match Material'
                    msg2 = 'feed rate of '
                    warnText += f'{msg0} {self.warnFeed[n][0]:0.0f}: F{self.warnFeed[n][1]} {msg1}_{self.warnFeed[n][2]}\'s {msg2} {self.warnFeed[n][3]:0.0f}\n'
            if self.warnChar:
                msg = 'Invalid characters, data has been commented out.\n'
                warnText += self.message_set(self.warnChar, msg)
        self.dialog.dialog_box(self, 'G-Code Errors & Warnings', errorText, warnText)

    def message_set(self, msgType, msg):
        if len(msgType) > 1:
            msg += 'Lines: '
        else:
            msg += 'Line: '
        count = 0
        for data in msgType:
            if self.codeError:
                data += 1
            if count > 0:
                msg += f', {data}'
            else:
                msg += f'{data}'
            count += 1
        msg += '\n\n'
        return msg


class qtGui():
    def dialog_box(self, parent, title, errorText, warnText):
        icon = QStyle.SP_MessageBoxCritical
        app = QApplication(sys.argv)
        dlg = QDialog()
        scroll = QScrollArea(dlg)
        widget = QWidget()
        vbox = QVBoxLayout()
        labelN = QLabel(objectName='labelN')
        lineE = QFrame(objectName='lineE')
        lineE.setFrameShape(QFrame.HLine)
        labelE1 = QLabel(objectName='labelE1')
        labelE2 = QLabel()
        lineW = QFrame(objectName='lineW')
        lineW.setFrameShape(QFrame.HLine)
        labelW1 = QLabel(objectName='labelW1')
        labelW2 = QLabel()
        vbox.addWidget(labelN)
        vbox.addWidget(lineE)
        vbox.addWidget(labelE1)
        vbox.addWidget(labelE2)
        vbox.addWidget(lineW)
        vbox.addWidget(labelW1)
        vbox.addWidget(labelW2)
        widget.setLayout(vbox)
        btn = QPushButton('OK', dlg)
        dlg.setWindowTitle(title)
        dlg.setWindowIcon(QIcon(dlg.style().standardIcon(icon)))
        dlg.setWindowFlags(Qt.WindowStaysOnTopHint)
        dlg.setModal(False)
        dlg.setFixedWidth(600)
        dlg.setFixedHeight(310)
        scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll.setWidgetResizable(True)
        scroll.setWidget(widget)
        scroll.setGeometry(5, 5, 590, 250)
        btn.move(270, 260)
        btn.clicked.connect(dlg.accept)
        notice = 'If the G-code editor is used to resolve the following issues, the lines with errors\n'
        notice += 'will be highlighted. The data numbers may differ from what is shown below.\n\n'
        labelN.setText(notice)
        if errorText:
            labelE1.setText('errors')
            labelE2.setText(errorText)
        else:
            lineE.hide()
            labelE1.hide()
            labelE2.hide()
        if warnText:
            labelW1.setText('warnings')
            labelW2.setText(warnText)
        else:
            lineW.hide()
            labelW1.hide()
            labelW2.hide()
        fgColor, bgColor, bgAltColor = None, None, None
        with open(parent.prefsFile, 'r') as inFile:
            for line in inFile:
                if line.startswith('Foreground ='):
                    fgColor = line.split('=')[1].strip()
                elif line.startswith('Background ='):
                    bgColor = line.split('=')[1].strip()
                elif line.startswith('Background Alt ='):
                    bgAltColor = line.split('=')[1].strip()
                elif fgColor and bgColor and bgAltColor:
                    break
        dlg.setStyleSheet(f' \
                        * {{ color: {fgColor}; background: {bgColor}}} \
                        QScrollArea {{color:{fgColor}; background:{bgColor}; border:1px solid {fgColor}; border-radius:4px; padding:4px}} \
                        QPushButton {{border:2px solid {fgColor}; border-radius:4px; font:12pt; width:60px; height:40px}} \
                        QPushButton:pressed {{border:1px solid {fgColor}}} \
                        QScrollBar:vertical {{background:{bgAltColor}; border:0px; border-radius:4px; margin: 0px; width:20px}} \
                        QScrollBar::handle:vertical {{background:{fgColor}; border:2px solid {fgColor}; border-radius:4px; margin:2px; min-height:40px}} \
                        QScrollBar::add-line:vertical {{height:0px}} \
                        QScrollBar::sub-line:vertical {{height:0px}} \
                        QVboxLayout {{margin:100}} \
                        #labelN {{font-style:italic}} \
                        #lineE, #lineW {{border:1px solid {fgColor}}} \
                        #labelE1, #labelW1 {{font-weight:bold}}')
        dlg.exec()

        # prevents linter complaints
        return app


class tkGui():
    def dialog_box(self, parent, title, errorText, warnText):
        dlg = Tk()
        dlg.attributes('-type', 'popup_menu')
        dlg.overrideredirect(True)
        dlg.resizable(False, False)
        dlg.eval(f'tk::PlaceWindow {dlg} pointer')
        dlg.grid_columnconfigure(0, weight=1)
        dlg.grid_rowconfigure(1, weight=1)
        dlg['highlightthickness'] = 2
        dlg.wm_attributes("-topmost", True)
        dlg.option_add("*Font", ['sans', 10, 'normal'])
        dlg.geometry('566x360')
        lbl = Label(text=title)
        lbl.grid(row=0, column=0, columnspan=2, sticky='EW')
        txt = Text(dlg, padx=4, pady=4)
        txt.grid(row=1, column=0, sticky='EW', padx=[4, 0])
        sbr = Scrollbar(dlg, orient='vertical', command=txt.yview)
        sbr.grid(row=1, column=1, sticky='NS')
        txt['yscrollcommand'] = sbr.set
        btn = Button(text='OK', width=10, command=dlg.destroy)
        btn.grid(row=2, column=0, columnspan=2, pady=2)
        text = '\nThe line numbers in the original file may differ from what is shown below.\n\n'
        line = '____________________________________________________________________________\n'
        if errorText:
            text += line
            text += 'ERRORS:\n'
            text += errorText
        if warnText:
            text += line
            text += 'WARNINGS:\n'
            text += warnText
        txt.insert('end', text)
        txt['state'] = 'disabled'
        fgColor, bgColor, tColor = None, None, None
        with open(parent.prefsFile, 'r') as inFile:
            for line in inFile:
                if line.startswith('Foreground color'):
                    fgColor = line.split('=')[1].strip()
                elif line.startswith('Background color'):
                    bgColor = line.split('=')[1].strip()
                elif line.startswith('Trough color'):
                    tColor = line.split('=')[1].strip()
                elif fgColor and bgColor and tColor:
                    break
        lbl['bg'] = fgColor
        txt['fg'] = fgColor
        btn['fg'] = fgColor
        dlg['bg'] = bgColor
        lbl['fg'] = bgColor
        txt['bg'] = bgColor
        txt['highlightbackground'] = bgColor
        sbr['bg'] = bgColor
        sbr['activebackground'] = bgColor
        sbr['troughcolor'] = tColor
        btn['bg'] = bgColor
        btn['activebackground'] = bgColor
        btn['highlightbackground'] = bgColor
        dlg.mainloop()


app = Filter(sys.argv)
