#   This is a component of LinuxCNC
#   Copyright 2014 Norbert Schechner <nieson@web.de>
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
# gmoccapy - Remap of M6 for auto tool measurement

import os
import sys
import emccanon 
from interpreter import *
from gscreen import preferences
throw_exceptions = 1

debug = False
if debug:
    pydevdir = '/home/emcmesa/Aptana_Studio_3/plugins/org.python.pydev_2.7.0.2013032300/pysrc'

    # the 'emctask' module is present only in the milltask instance, otherwise both the UI and
    # milltask would try to connect to the debug server.

    if os.path.isdir( pydevdir ) and  'emctask' in sys.builtin_module_names:
        sys.path.append( pydevdir )
        sys.path.insert( 0, pydevdir )
        try:
            import pydevd
            emccanon.MESSAGE( "pydevd imported, connecting to Eclipse debug server..." )
            pydevd.settrace()
        except:
            emccanon.MESSAGE( "no pydevd module found" )
            pass

# REMAP=M6  modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog
# exposed parameters:
#    #<tool_in_spindle>
#    #<selected_tool>
#    #<current_pocket>
#    #<selected_pocket>

def change_prolog(self, **words):
    try:
        # this is relevant only when using iocontrol-v2.
        if self.params[5600] > 0.0:
            if self.params[5601] < 0.0:
                self.set_errormsg("Toolchanger hard fault %d" % (int(self.params[5601])))
                return INTERP_ERROR
            print "change_prolog: Toolchanger soft fault %d" % int(self.params[5601])

        if self.selected_pocket < 0:
            self.set_errormsg("M6: no tool prepared")
            return INTERP_ERROR

        if self.cutter_comp_side:
            self.set_errormsg("Cannot change tools with cutter radius compensation on")
            return INTERP_ERROR
        self.params["tool_in_spindle"] = self.current_tool
        self.params["selected_tool"] = self.selected_tool
        self.params["current_pocket"] = self.current_pocket # this is probably nonsense
        self.params["selected_pocket"] = self.selected_pocket
        return INTERP_OK

    except Exception, e:
        self.set_errormsg("M6/change_prolog: %s" % (e))
        return INTERP_ERROR

def change_epilog(self, **words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR

        if self.return_value > 0.0:
            if self.return_value == 3:
                message = "No tool measurement ! Please take care of the entry in the tool table"
                emccanon.MESSAGE(message)
            return INTERP_OK
        else:
            if self.return_value == -1:
                message = "Searchvel <= 0, not permitted!, Please correct INI Settings."
            elif self.return_value == -2:
                message = "Probevel <= 0, not permitted!, Please correct INI Settings."
            elif self.return_value == -3:
                message = "Probe contact failiure !!"
            else:
                message = "M6 aborted (return code %.1f)" % (self.return_value)
            self.set_errormsg(message)
            return INTERP_ERROR

    except Exception, e:
        self.set_errormsg("M6/change_epilog: %s" % (e))
        return INTERP_ERROR


_uvw = ("u","v","w","a","b","c")
_xyz = ("x","y","z","a","b","c")
# given a plane, return  sticky words, incompatible axis words and plane name
# sticky[0] is also the movement axis
_compat = {
    emccanon.CANON_PLANE_XY : (("z","r"),_uvw,"XY"),
    emccanon.CANON_PLANE_YZ : (("x","r"),_uvw,"YZ"),
    emccanon.CANON_PLANE_XZ : (("y","r"),_uvw,"XZ"),
    emccanon.CANON_PLANE_UV : (("w","r"),_xyz,"UV"),
    emccanon.CANON_PLANE_VW : (("u","r"),_xyz,"VW"),
    emccanon.CANON_PLANE_UW : (("v","r"),_xyz,"UW")}           

# extract and pass parameters from current block, merged with extra parameters on a continuation line
# keep tjose parameters across invocations
# export the parameters into the oword procedure
def cycle_prolog(self,**words):
    # self.sticky_params is assumed to have been initialized by the
    # init_stgdlue() method below
    global _compat
    try:    
        # determine whether this is the first or a subsequent call
        c = self.blocks[self.remap_level]
        r = c.executing_remap
        if c.g_modes[1] == r.motion_code:
            # first call - clear the sticky dict
            self.sticky_params[r.name] = dict()

        self.params["motion_code"] = c.g_modes[1]
        
        (sw,incompat,plane_name) =_compat[self.plane]
        for (word,value) in words.items():
            # inject current parameters
            self.params[word] = value
            # record sticky words
            if word in sw:
                if self.debugmask & 0x00080000: print "%s: record sticky %s = %.4f" % (r.name,word,value)
                self.sticky_params[r.name][word] = value
            if word in incompat:
                return "%s: Cannot put a %s in a canned cycle in the %s plane" % (r.name, word.upper(), plane_name)

        # inject sticky parameters which were not in words:
        for (key,value) in self.sticky_params[r.name].items():
            if not key in words:
                if self.debugmask & 0x00080000: print "%s: inject sticky %s = %.4f" % (r.name,key,value)
                self.params[key] = value

        if not "r" in self.sticky_params[r.name]:
            return "%s: cycle requires R word" % (r.name)
        else:
            if self.sticky_params[r.name] <= 0.0:
                return "%s: R word must be > 0 if used (%.4f)" % (r.name, words["r"])

        if "l" in words:
            # checked in interpreter during block parsing
            # if l <= 0 or l not near an int
            self.params["l"] = words["l"]
            
        if "p" in words:
            p = words["p"]
            if p < 0.0:
                return "%s: P word must be >= 0 if used (%.4f)" % (r.name, p)
            self.params["p"] = p

        if self.feed_rate == 0.0:
            return "%s: feed rate must be > 0" % (r.name)
        if self.feed_mode == INVERSE_TIME:
            return "%s: Cannot use inverse time feed with canned cycles" % (r.name)
        if self.cutter_comp_side:
            return "%s: Cannot use canned cycles with cutter compensation on" % (r.name)
        return INTERP_OK
    
    except Exception, e:
        raise
        return "cycle_prolog failed: %s" % (e)

# make sure the next line has the same motion code, unless overriden by a
# new G code
def cycle_epilog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        self.motion_mode = c.executing_remap.motion_code # retain the current motion mode
        return INTERP_OK
    except Exception, e:
        return "cycle_epilog failed: %s" % (e)

# this should be called from TOPLEVEL __init__()
def init_stdglue(self):
    self.sticky_params = dict()
