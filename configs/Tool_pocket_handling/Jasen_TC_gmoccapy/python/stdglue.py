#!/usr/bin/env python
# -*- coding:UTF-8 -*-

import emccanon 
from interpreter import *
throw_exceptions = 1

from gmoccapy import getiniinfo
import os

CONFIGPATH = os.environ['CONFIG_DIR']


# REMAP=T   prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
# exposed parameters: #<tool> #<pocket>

def prepare_prolog(self,**words):
    # first we read the tool file, in case it has been changed
    toolfile = os.path.join(CONFIGPATH, getiniinfo.GetIniInfo().get_toolfile())
    with open(toolfile, "r") as file:
        tool_file_lines = file.readlines()
    file.close()
    
    # now we collect some info about the tool file, to decide if the selected
    # tool is really in the file, if not we abort 
    tool_file_info = {}
    for pos, line in enumerate(tool_file_lines):
        line = line.rstrip('\n')
        toolinfo = line.split(";")
        tool_str = toolinfo[0].split()[0]
        pocket_str = toolinfo[0].split()[1]
        tool_file_info[pocket_str] = tool_str

    try:
        cblock = self.blocks[self.remap_level]

        # check if an T command has been executed
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            return INTERP_ERROR
        
        # get the tool to be plöaced in the spindle
        tool  = cblock.t_number
        if tool:
            # as we compare with the tool file, we need to add the T to the string
            toolname = "T{0}".format(tool)
            # if the tool is not in the dict, we get a key error
            try:
                pocketname = tool_file_info.keys()[tool_file_info.values().index(toolname)] 
                pocket = int(pocketname[1:])
                status = INTERP_OK
            except:
                status = INTERP_ERROR

            if status != INTERP_OK:
                self.set_errormsg("T{0} is not in tool file!".format(tool))
                return status
            
            # we need to check if the mentioned tool number is not in rack,
            # otherwise we could destroy the tool handling, 
            # due to not unique identification, we have all relevant info 
            # in our dictionary rack_info
            rack_info = {}
            for pos in range(1,11):
                pocket_str = "P{0}".format(pos)
                try:
                    tool_str = tool_file_info[pocket_str]
                except KeyError:
                    tool_str = "0"
                rack_info[pos] = tool_str

            tool_in_rack = False
            for pos in range(1,11):
                if toolname == rack_info[pos]:
                    status = INTERP_OK
                    break
                
            if status != INTERP_OK:
                self.set_errormsg("T{0} is not in the changer!".format(tool))
                return status

        else:
            pocket = -1 # this is a T0 - tool unload

        self.params["tool"] = tool
        self.params["pocket"] = pocket
        return INTERP_OK

    except Exception, e:
        self.set_errormsg("T%d/prepare_prolog: %s" % (int(words['t']), e))
        return INTERP_ERROR


def prepare_epilog(self, **words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR

        if self.blocks[self.remap_level].builtin_used:
            #print "---------- T builtin recursion, nothing to do"
            return INTERP_OK
        else:
            if self.return_value > 0:
                self.selected_tool = int(self.params["tool"])
                self.selected_pocket = int(self.params["pocket"])
                emccanon.SELECT_POCKET(self.selected_pocket, self.selected_tool)
                return INTERP_OK
            else:
                if self.return_value == -1:
                    self.set_errormsg("Could not receive pocket from analog in pin 01")
                elif self.return_value == -2:
                    self.set_errormsg("The selected tool is not in the changer!")
                else:
                    self.set_errormsg("T%d: aborted (return code %.1f)" % (int(self.params["tool"]),self.return_value))
                return INTERP_ERROR
    except Exception, e:
        self.set_errormsg("T%d/prepare_epilog: %s" % (tool,e))
        return INTERP_ERROR       




# REMAP=M6  modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog
# exposed parameters:
#    #<tool_in_spindle>
#    #<selected_tool>
#    #<current_pocket>
#    #<selected_pocket>

def change_prolog(self, **words):
    try:

        if self.cutter_comp_side:
            self.set_errormsg("Cannot change tools with cutter radius compensation on")
            return INTERP_ERROR

        self.params["tool"] = self.selected_tool
        self.params["pocket"] = self.selected_pocket
        self.params["tool_in_spindle"] = self.current_tool
        self.params["current_pocket"] = self.current_pocket # this is probably nonsense
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
            yield INTERP_ERROR
        # this is relevant only when using iocontrol-v2.
        if self.params[5600] > 0.0:
            if self.params[5601] < 0.0:
                self.set_errormsg("Toolchanger hard fault %d" % (int(self.params[5601])))
                yield INTERP_ERROR
            print "change_epilog: Toolchanger soft fault %d" % int(self.params[5601])

        if self.blocks[self.remap_level].builtin_used:
            #print "---------- M6 builtin recursion, nothing to do"
            yield INTERP_OK
        else:
            if self.return_value > 0.0:
                # commit change
                self.selected_pocket =  int(self.params["pocket"])
                emccanon.CHANGE_TOOL(self.selected_pocket)
                self.current_pocket = self.selected_pocket
                self.selected_pocket = -1
                self.selected_tool = -1
                # cause a sync()
                self.set_tool_parameters()
                self.toolchange_flag = True
                yield INTERP_EXECUTE_FINISH
            else:
                self.set_errormsg("M6 aborted (return code %.1f)" % (self.return_value))
                yield INTERP_ERROR
    except Exception, e:
        self.set_errormsg("M6/change_epilog: %s" % (e))
        yield INTERP_ERROR


# this should be called from TOPLEVEL __init__()
def init_stdglue(self):
    self.sticky_params = dict()
