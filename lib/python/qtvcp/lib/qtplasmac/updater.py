'''
updater.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

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

def add_component_hal_file(path, inifile, halfiles):
    written = False
    tmpFile = '{}~'.format(inifile)
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
#'plasmac.axis-z-position',
    pinsToCheck = ['PLASMAC COMPONENT INPUTS','plasmac.arc-ok-in','plasmac.axis-x-position',
                   'plasmac.axis-y-position','plasmac.breakaway','plasmac.current-velocity',
                   'plasmac.cutting-start','plasmac.cutting-stop','plasmac.feed-override',
                   'plasmac.feed-reduction','plasmac.float-switch','plasmac.feed-upm',
                   'plasmac.ignore-arc-ok-0','plasmac.machine-is-on','plasmac.motion-type',
                   'plasmac.offsets-active','plasmac.ohmic-probe','plasmac.program-is-idle',
                   'plasmac.program-is-paused','plasmac.program-is-running','plasmac.scribe-start',
                   'plasmac.spotting-start','plasmac.thc-disable','plasmac.torch-off',
                   'plasmac.units-per-mm','plasmac.x-offset-current','plasmac.y-offset-current',
                   'plasmac.z-offset-current',
                   'PLASMAC COMPONENT OUTPUTS','plasmac.adaptive-feed','plasmac.feed-hold',
                   'plasmac.offset-scale','plasmac.program-pause','plasmac.program-resume',
                   'plasmac.program-run','plasmac.program-stop','plasmac.torch-on',
                   'plasmac.x-offset-counts','plasmac.y-offset-counts','plasmac.xy-offset-enable',
                   'plasmac.z-offset-counts','plasmac.z-offset-enable','plasmac.requested-velocity']
    for f in halfiles:
        if not 'custom' in f:
            halfile = os.path.join(path, f)
            with open(halfile, 'r') as inFile:
                if 'plasmac.cutting-start' in inFile.read():
                    inFile.seek(0)
                    tmpFile = '{}~'.format(halfile)
                    COPY(halfile, tmpFile)
                    with open(tmpFile, 'r') as inFile:
                        with open(f, 'w') as outFile:
                            for line in inFile:
                                if any(pin in line for pin in pinsToCheck):
                                    continue
                                outFile.write(line)
    if os.path.isfile(tmpFile):
        os.remove(tmpFile)
