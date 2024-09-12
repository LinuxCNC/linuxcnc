'''
updater.py

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
from shutil import copy as COPY

###########################################################################################################
# set user_m_path to include ../../nc_files/plasmac/m_files (pre V2.10-001.017 2024/01/23)                #
###########################################################################################################


def insert_user_m_path(configPath, simPath):
    try:
        if os.path.isfile(os.path.join(configPath, 'M190')):
            os.rename(os.path.join(configPath, 'M190'), os.path.join(configPath, 'M190.bak'))
        if simPath and os.path.isfile(os.path.join(simPath, 'M190')):
            os.remove(os.path.join(simPath, 'M190'))
        return(False, False, '')
    except Exception as e:
        return(False, True, e)


def insert_user_m_path_iniwrite(inifile, data=None):
    try:
        tmpFile = f'{inifile}~'
        COPY(inifile, tmpFile)
        with open(tmpFile, 'r') as inFile:
            with open(inifile, 'w') as outFile:
                for line in inFile:
                    if line.startswith('USER_M_PATH'):
                        line = line.split('=')[1].strip().replace('./:', '')
                        if line.endswith('./'):
                            line = line[:-2]
                        line = f'USER_M_PATH             = ./:{data}:{line}\n'
                    outFile.write(line)
        if os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(True, False, 'Updated to V2.10-001.017')


###########################################################################################################
# change runcritical to cutcritical in <machine_name>.prefs file (pre V2.10-001.015 2023/12/23)           #
###########################################################################################################


def rename_runcritical(prefs):
    with open(prefs, 'r+') as file:
        text = file.read()
        text = text.replace('runcritical', 'cutcritical')
        file.seek(0)
        file.truncate()
        file.write(text)
    return(False, False, 'Updated to V2.10-001.015')


###########################################################################################################
# move default material from prefs file to material 0 in materials file (pre V2.9-236.278 2023/07/07)       #
###########################################################################################################


def move_default_material(prefs, materials, unitsPerMm):
    if os.path.isfile(materials):
        with open(materials, 'r') as f_in:
            mats = f_in.readlines()
    else:
        mats = None
    mat = '[MATERIAL_NUMBER_0]\n'
    mat += f'NAME               = Default\n'
    mat += f'KERF_WIDTH         = {prefs.getpref("Kerf width", round(1 * unitsPerMm, 2), float, "DEFAULT MATERIAL")}\n'
    mat += f'PIERCE_HEIGHT      = {prefs.getpref("Pierce height", round(3 * unitsPerMm, 2), float, "DEFAULT MATERIAL")}\n'
    mat += f'PIERCE_DELAY       = {prefs.getpref("Pierce delay", 0, float, "DEFAULT MATERIAL")}\n'
    mat += f'PUDDLE_JUMP_HEIGHT = {prefs.getpref("Puddle jump height", 0, float, "DEFAULT MATERIAL")}\n'
    mat += f'PUDDLE_JUMP_DELAY  = {prefs.getpref("Puddle jump delay", 0, float, "DEFAULT MATERIAL")}\n'
    mat += f'CUT_HEIGHT         = {prefs.getpref("Cut height", round(1 * unitsPerMm, 2), float, "DEFAULT MATERIAL")}\n'
    mat += f'CUT_SPEED          = {prefs.getpref("Cut feed rate", round(4000 * unitsPerMm, 0), float, "DEFAULT MATERIAL")}\n'
    mat += f'CUT_AMPS           = {prefs.getpref("Cut amps", 45, float, "DEFAULT MATERIAL")}\n'
    mat += f'CUT_VOLTS          = {prefs.getpref("Cut volts", 99, float, "DEFAULT MATERIAL")}\n'
    mat += f'PAUSE_AT_END       = {prefs.getpref("Pause at end", 0, float, "DEFAULT MATERIAL")}\n'
    mat += f'GAS_PRESSURE       = {prefs.getpref("Gas pressure", 0, float, "DEFAULT MATERIAL")}\n'
    mat += f'CUT_MODE           = {prefs.getpref("Cut mode", 1, float, "DEFAULT MATERIAL")}\n\n'
    with open(materials, 'w') as f_out:
        f_out.write(mat)
        if mats:
            valid = False
            for line in mats:
                if line[0] != '#':
                    if line[0] == '[':
                        if line[1:17] == 'MATERIAL_NUMBER_' and line.strip()[-3:] != '_0]':
                            valid = True
                        else:
                            valid = False
                    if valid:
                        f_out.write(line)
    prefs.remove_section('DEFAULT MATERIAL')
    prefs.write(open(prefs.fn, 'w'))
    return(False, False, 'Updated to V2.9-236.278')


###########################################################################################################
# move port info from [GUI_OPTIONS] section (if it was moved via V2.9-227.219 update) to [POWERMAX] section #
###########################################################################################################


def move_port(prefs):
    if not prefs.has_option('POWERMAX', 'Port'):
        data = prefs.getpref('Port', '', str, 'GUI_OPTIONS')
        prefs.putpref('Port', data, str, 'POWERMAX')
    prefs.removepref('Port', 'GUI_OPTIONS')
    prefs.write(open(prefs.fn, 'w'))
    return(False, False, 'Updated to V2.9-232.240')


###########################################################################################################
# move qtplasmac options from INI file to prefs file (pre V2.9-227.219 2022/07/14)                          #
###########################################################################################################


def move_options_to_prefs_file(inifile, prefs):
    try:
        text = prefs.getpref('shutdown_msg_detail', '', str, 'SHUTDOWN_OPTIONS')
        prefs.putpref('Exit warning text', text, str, 'GUI_OPTIONS')
        prefs.remove_section('SHUTDOWN_OPTIONS')
        prefs.write(open(prefs.fn, 'w'))
        data = inifile.find('QTPLASMAC', 'MODE') or None
        if data:
            prefs.putpref('Mode', data, int, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'ESTOP_TYPE') or None
        if data:
            prefs.putpref('Estop type', data, int, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'DRO_POSITION') or None
        if data:
            prefs.putpref('DRO position', data, str, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'FLASH_ERROR') or None
        if data:
            data = True if data.lower() in ('yes', 'y', 'true', 't', '1') else False
            prefs.putpref('Flash error', data, bool, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'HIDE_RUN') or None
        if data:
            data = True if data.lower() in ('yes', 'y', 'true', 't', '1') else False
            prefs.putpref('Hide run', data, bool, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'HIDE_PAUSE') or None
        if data:
            data = True if data.lower() in ('yes', 'y', 'true', 't', '1') else False
            prefs.putpref('Hide pause', data, bool, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'HIDE_ABORT') or None
        if data:
            data = True if data.lower() in ('yes', 'y', 'true', 't', '1') else False
            prefs.putpref('Hide abort', data, bool, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'CUSTOM_STYLE') or None
        if data:
            prefs.putpref('Custom style', data, str, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'AUTOREPEAT_ALL') or None
        if data:
            data = True if data.lower() in ('yes', 'y', 'true', 't', '1') else False
            prefs.putpref('Autorepeat all', data, bool, 'GUI_OPTIONS')
        data = inifile.find('QTPLASMAC', 'LASER_TOUCHOFF') or None
    except Exception as e:
        return(False, True, e)
    if data:
        x, y, d, error = get_offsets(data, 'LASER_OFFSET')
        if error:
            return(False, True, error)
        prefs.putpref('X axis', x, float, 'LASER_OFFSET')
        prefs.putpref('Y axis', y, float, 'LASER_OFFSET')
    data = inifile.find('QTPLASMAC', 'CAMERA_TOUCHOFF') or None
    if data:
        x, y, d, error = get_offsets(data, 'CAMERA_OFFSET')
        if error:
            return(False, True, error)
        prefs.putpref('X axis', x, float, 'CAMERA_OFFSET')
        prefs.putpref('Y axis', y, float, 'CAMERA_OFFSET')
    data = inifile.find('QTPLASMAC', 'OFFSET_PROBING') or None
    if data:
        x, y, d, error = get_offsets(data, 'OFFSET_PROBING')
        if error:
            return(False, True, error)
        prefs.putpref('X axis', x, float, 'OFFSET_PROBING')
        prefs.putpref('Y axis', y, float, 'OFFSET_PROBING')
        prefs.putpref('Delay', d, float, 'OFFSET_PROBING')
    data = inifile.find('QTPLASMAC', 'PM_PORT') or None
    if data:
        prefs.putpref('Port', data, str, 'POWERMAX')
    for bNum in range(1, 21):
        bName = inifile.find('QTPLASMAC', f'BUTTON_{bNum}_NAME') or None
        bCode = inifile.find('QTPLASMAC', f'BUTTON_{bNum}_CODE') or None
        if bName and bCode:
            prefs.putpref(f'{bNum} Name', bName, str, 'BUTTONS')
            prefs.putpref(f'{bNum} Code', bCode, str, 'BUTTONS')
    return(False, False, '')


def move_options_to_prefs_file_iniwrite(inifile):
    try:
        tmpFile = f'{inifile}~'
        COPY(inifile, tmpFile)
        with open(tmpFile, 'r') as inFile:
            with open(inifile, 'w') as outFile:
                remove = False
                for line in inFile:
                    if not line:
                        break
                    elif line.startswith('[QTPLASMAC]'):
                        remove = True
                        continue
                    elif remove:
                        if line.startswith('['):
                            remove = False
                        else:
                            continue
                    outFile.write(line)
        if os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(True, False, 'Updated to V2.9-227.219')


def get_offsets(data, oType):
    x = y = d = 0
    error = False
    parms = data.lower().split()
    for parm in parms:
        if parm.startswith('x'):
            x = float(parm.replace('x', ''))
        elif parm.startswith('y'):
            y = float(parm.replace('y', ''))
        elif oType == 'OFFSET_PROBING':
            d = float(parm)
        else:
            error = f'Parameter error in {oType}. "{data}" is invalid.'
    return x, y, d, error


###########################################################################################################
# remove the qtplasmac link from the config directory (pre V2.9-225.208 2022/06/29)                         #
###########################################################################################################


def remove_qtplasmac_link_iniwrite(inifile):
    try:
        tmpFile = f'{inifile}~'
        COPY(inifile, tmpFile)
        with open(tmpFile, 'r') as inFile:
            with open(inifile, 'w') as outFile:
                for line in inFile:
                    if line.replace(' ', '').startswith('ngc='):
                        line = 'ngc = qtplasmac_gcode\n'
                    elif line.replace(' ', '').startswith('nc='):
                        line = 'nc  = qtplasmac_gcode\n'
                    elif line.replace(' ', '').startswith('tap='):
                        line = 'tap = qtplasmac_gcode\n'
                    elif (line.startswith('SUBROUTINE_PATH') or line.startswith('USER_M_PATH')) and ':./qtplasmac' in line:
                        line = line.replace(':./qtplasmac', '')
                    outFile.write(line)
        if os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(True, False, 'Updated to V2.9-225.208')


###########################################################################################################
# change startup parameters from a subroutine (pre V2.9-225.207 2022/06/22)                                 #
###########################################################################################################


def rs274ngc_startup_code_iniwrite(inifile):
    try:
        tmpFile = f'{inifile}~'
        COPY(inifile, tmpFile)
        with open(tmpFile, 'r') as inFile:
            with open(inifile, 'w') as outFile:
                for line in inFile:
                    if line.startswith('RS274NGC_STARTUP_CODE') and ('metric' in line or 'imperial' in line):
                        units = 21 if 'metric' in line else 20
                        line = f'RS274NGC_STARTUP_CODE   = G{units} G40 G49 G80 G90 G92.1 G94 G97 M52P1\n'
                    outFile.write(line)
        if os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(True, False, 'Updated to V2.9-225.207')


###########################################################################################################
# move [CONVERSATIONAL] prefs from qtvcp.prefs to <machine_name>.prefs (pre V2.9-222.187 2022/05/03)        #
###########################################################################################################


def move_prefs(qtvcp, machine):
    try:
        deleteMachineLine = False
        moveQtvcpLine = False
        with open(machine, 'r') as inFile:
                machineData = inFile.readlines()
        with open(qtvcp, 'r') as inFile:
                qtvcpData = inFile.readlines()
        with open(machine, 'w') as machineFile:
            with open(qtvcp, 'w') as qtvcpFile:
                for line in machineData:
                    if line.strip() == '[CONVERSATIONAL]':
                        deleteMachineLine = True
                    elif line.startswith('['):
                        deleteMachineLine = False
                    if not deleteMachineLine:
                        machineFile.write(line)
                for line in qtvcpData:
                    if line.strip() == '[CONVERSATIONAL]':
                        moveQtvcpLine = True
                    elif line.startswith('['):
                        moveQtvcpLine = False
                    if moveQtvcpLine:
                        machineFile.write(line)
                    else:
                        qtvcpFile.write(line)
    except Exception as e:
        return(False, True, e)
    return(False, False, 'Updated to V2.9-222.187')


###########################################################################################################
# split out qtplasmac.prefs into <machine_name>.prefs and qtvcp.prefs (pre V2.9-222.170 2022/03/08)         #
###########################################################################################################


def split_prefs_file(old, new, prefs):
    try:
        move = False
        copy = False
        moves = ['[GUI_OPTIONS]', '[COLOR_OPTIONS]', '[ENABLE_OPTIONS]', '[STATISTICS]',
                 '[PLASMA_PARAMETERS]', '[DEFAULT MATERIAL]', '[SINGLE CUT]', '[CONVERSATIONAL]']
        copies = ['[SHUTDOWN_OPTIONS]']
        with open(old, 'r') as inFile:
            data = inFile.readlines()
        with open(new, 'w') as newFile:
            with open(prefs, 'w') as prefsFile:
                for line in data:
                    if line.strip() in moves:
                        move = True
                        copy = False
                    elif line.strip() in copies:
                        move = False
                        copy = True
                    elif line.strip().startswith('['):
                        move = False
                        copy = False
                    if move:
                        prefsFile.write(line)
                    elif copy:
                        if line.strip().startswith('['):
                            prefsFile.write(line)
                        if 'shutdown_msg_detail' in line:
                            prefsFile.write(line)
                            prefsFile.write('\n')
                        newFile.write(line)
                    else:
                        newFile.write(line)
        if os.path.isfile(old):
            os.remove(old)
    except Exception as e:
        return(False, True, e)
    return(False, False, 'Updated to V2.9-222.170')


###########################################################################################################
# use qtplasmac_comp.hal for component connections (pre V2.9-221.154 2022/01/18)                            #
###########################################################################################################


def add_component_hal_file(path, halfiles):
    try:
        tmpFile = None
        pinsToCheck = ['PLASMAC COMPONENT INPUTS', 'plasmac.arc-ok-in', 'plasmac.axis-x-position',
                       'plasmac.axis-y-position', 'plasmac.breakaway', 'plasmac.current-velocity',
                       'plasmac.cutting-start', 'plasmac.cutting-stop', 'plasmac.feed-override',
                       'plasmac.feed-reduction', 'plasmac.float-switch', 'plasmac.feed-upm',
                       'plasmac.ignore-arc-ok-0', 'plasmac.machine-is-on', 'plasmac.motion-type',
                       'plasmac.offsets-active', 'plasmac.ohmic-probe', 'plasmac.program-is-idle',
                       'plasmac.program-is-paused', 'plasmac.program-is-running', 'plasmac.scribe-start',
                       'plasmac.spotting-start', 'plasmac.thc-disable', 'plasmac.torch-off',
                       'plasmac.units-per-mm', 'plasmac.x-offset-current', 'plasmac.y-offset-current',
                       'plasmac.z-offset-current',
                       'PLASMAC COMPONENT OUTPUTS', 'plasmac.adaptive-feed', 'plasmac.feed-hold',
                       'plasmac.offset-scale', 'plasmac.program-pause', 'plasmac.program-resume',
                       'plasmac.program-run', 'plasmac.program-stop', 'plasmac.torch-on',
                       'plasmac.x-offset-counts', 'plasmac.y-offset-counts', 'plasmac.xy-offset-enable',
                       'plasmac.z-offset-counts', 'plasmac.z-offset-enable', 'plasmac.requested-velocity']
        for f in halfiles:
            f = os.path.expanduser(f)
            if 'custom' not in f:
                halfile = os.path.join(path, f)
                with open(halfile, 'r') as inFile:
                    if 'plasmac.cutting-start' in inFile.read():
                        inFile.seek(0)
                        tmpFile = f'{halfile}~'
                        COPY(halfile, tmpFile)
                        with open(tmpFile, 'r') as inFile:
                            with open(f, 'w') as outFile:
                                for line in inFile:
                                    if any(pin in line for pin in pinsToCheck):
                                        continue
                                    outFile.write(line)
        if tmpFile and os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(False, False, '')


def add_component_hal_file_iniwrite(inifile):
    try:
        written = False
        tmpFile = f'{inifile}~'
        COPY(inifile, tmpFile)
        with open(tmpFile, 'r') as inFile:
            with open(inifile, 'w') as outFile:
                while 1:
                    line = inFile.readline()
                    if not line:
                        break
                    elif line.startswith('[HAL]'):
                        outFile.write(line)
                        break
                    else:
                        outFile.write(line)
                while 1:
                    line = inFile.readline()
                    if not line:
                        if not written:
                            outFile.write('HALFILE = qtplasmac_comp.hal\n')
                        break
                    elif line.startswith('['):
                        if not written:
                            outFile.write('HALFILE = qtplasmac_comp.hal\n')
                        outFile.write(line)
                        break
                    elif 'custom.hal' in line.lower():
                        outFile.write('HALFILE = qtplasmac_comp.hal\n')
                        outFile.write(line)
                        written = True
                        break
                    else:
                        outFile.write(line)
                while 1:
                    line = inFile.readline()
                    if not line:
                        break
                    else:
                        outFile.write(line)
        if os.path.isfile(tmpFile):
            os.remove(tmpFile)
    except Exception as e:
        return(False, True, e)
    return(True, False, 'Updated to V2.9-221.154')
