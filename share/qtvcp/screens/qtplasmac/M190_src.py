'''
M190

Copyright (C) 2019, 2020, 2021  Phillip A Carter
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

import sys
import time
import hal
from subprocess import Popen,PIPE

h = hal.component('dummy')
materialNum = int(float(sys.argv[1]))
timeout = 0.5

def get_material():
    return int(hal.get_value('qtplasmac.material_change_number'))

def set_material(material):
    hal.set_p('qtplasmac.material_change_number', '{}'.format(material))

def get_change():
    return int(hal.get_value('qtplasmac.material_change'))

def set_change(value):
    hal.set_p('qtplasmac.material_change', '{}'.format(value))

def set_timeout():
    hal.set_p('qtplasmac.material_change_timeout', '1')

try:
    if materialNum != get_material():
        set_change(1)
        set_material(materialNum)
    else:
        set_change(3)
    start = time.time()
    while get_change() == 1 or get_change() == 3:
        if time.time() > start + timeout:
            set_timeout()
            break
    set_change(0)
except:
    pass
exit()
