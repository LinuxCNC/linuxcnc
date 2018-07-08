#   This is a component of LinuxCNC
#   Copyright 2011, 2012, 2013 Michael Haberler <git@mah.priv.at>,
#   Sebastian Kuzminsky <seb@highlab.com>
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
import interpreter
EMC_DEBUG_CONFIG            = 0x00000002
EMC_DEBUG_VERSIONS          = 0x00000008
EMC_DEBUG_TASK_ISSUE        = 0x00000010
EMC_DEBUG_NML               = 0x00000040
EMC_DEBUG_MOTION_TIME       = 0x00000080
EMC_DEBUG_INTERP            = 0x00000100
EMC_DEBUG_RCS               = 0x00000200
EMC_DEBUG_INTERP_LIST       = 0x00000800
EMC_DEBUG_IOCONTROL         = 0x00001000
EMC_DEBUG_OWORD             = 0x00002000
EMC_DEBUG_REMAP             = 0x00004000
EMC_DEBUG_PYTHON            = 0x00008000
EMC_DEBUG_NAMEDPARAM        = 0x00010000
EMC_DEBUG_GDBONSIGNAL       = 0x00020000


def introspect(self,*args):
    print "debug: ", self.debugmask,"logging:",self.loggingLevel
    print "call_level=",self.call_level
    print "argc=", len(args)
    print "args=(",
    for arg in args:
        if type(arg) == float:
            print "%0.5f" % arg,
        else:
            print arg,
    print ")"

    # the low-level access within the block
    for n in range(len(args)):
        print "param #",n ,"=", self.blocks[0].params[n]

    # this is a low-level interface.
    for x in self.sub_context[1].named_params:
        print "name:",x.key(),"value=",x.data().value, "attr=",x.data().attr

    #for r in self.remaps:
    #    print r.key(), str(r.data())

    print "current oword subname=", self.blocks[0].o_name
    print "m_modes[0]=", self.blocks[0].m_modes[0]
    print "g_modes[0]=", self.blocks[0].g_modes[0]

    # this is the high level named & numbered parameter interface
    print "current tool=",self.params[5400],self.params["_current_tool"]

    print "feed=",self.params['_feed']
    print "speed=",self.params['_rpm']

    print "global parameter set in test.ngc:",self.params['_a_global_set_in_test_dot_ngc']
    print "parameter set via test.ini:",self.params['_ini[example]variable']
    assert self.params['_ini[example]variable'] == args[3]

    self.params["a_new_local"] = 321.0
    self.params["_a_new_global"] = 456.0

    print "locals: ",self.params.locals()
    print "globals: ",self.params.globals()
    print "params(): ",self.params()
    return 2.71828

