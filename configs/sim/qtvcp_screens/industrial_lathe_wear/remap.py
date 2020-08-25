#   This is a component of LinuxCNC
#   Copyright 2020 Chris Morley
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# see nc_files/remap_lib/stdglue.py
#
# use standard python remap functions:
# index_lathe_tool_with_wear()
# ignore_m6()

from stdglue import *
from interpreter import INTERP_OK
from emccanon import MESSAGE
COUNT = 0

def ignore_m6(self,**words):
    MESSAGE("remapped m6")
    try:
        return INTERP_OK
    except Exception, e:
        return "Ignore M6 failed: %s" % (e)

def m2_remap(self, **words): # in remap module
    print 'remap level:'.format(self.remap_level)
    print 'call level:',self.call_level
    global COUNT
    COUNT +=1
    print 'Run count:{}'.format(COUNT)

    #MESSAGE("remapped m2")
    self.execute("m2")
    #emccanon.PROGRAM_END()
    return INTERP_OK
