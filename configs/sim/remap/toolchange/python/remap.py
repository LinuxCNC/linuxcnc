import sys
#import inspect
#import traceback

from interpreter import *
import emccanon


def prepare_prolog(self,**words):
    cblock = self.blocks[self.remap_level]
    if not cblock.t_flag:
        self.set_errormsg("T requires a tool number")
        return INTERP_ERROR
    tool  = cblock.t_number 
    (status,pocket) = self.find_tool_pocket(tool)
    if status != INTERP_OK:
        self.set_errormsg("T%d: pocket not found" % (tool))
        return status
    
    # remember for the epilog handler
    self._tool = tool
    self._pocket = pocket
    
    # these variables will be visible in the ngc oword sub
    # as #<tool>, #<pocket> etc local variables
    self.params["new_tool"] = tool
    self.params["new_pocket"] = pocket
    self.params["current_tool"] = self.current_tool
    self.params["selected_pocket"] = self.selected_pocket
    return INTERP_OK

# The minimal ngc prepare procedure looks like so:
#
# o<prepare> sub
# ; returning a positive value to commit:
# o<prepare> endsub [1]
# m2

# the prepare epilog looks at the return value from the NGC procedure
# and does the right thing:
def prepare_epilog(self, **words):
    retval = self.return_value
    if retval >= 0:
        self.selected_pocket = self._pocket
        self.selected_tool = self._tool
        emccanon.SELECT_POCKET( self._pocket, self._tool)
        return INTERP_OK
    else:
        self.set_errormsg("T%d: aborted (return code %.4f)" % (self._tool,retval))
        return INTERP_ERROR

#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  prolog=change_prolog ngc=change epilog=change_epilog
#
# Note that the following predefined named parameters can be useful here:
# #<_selected_tool>
# #<_selected_pocket>
# #<_current_tool>
# #<_current_pocket>

def change_prolog(self, **words):
	if self.selected_pocket < 0:
		self.set_errormsg("Need tool prepared -Txx- for toolchange")
		return INTERP_ERROR
	if self.cutter_comp_side:
		self.set_errormsg("Cannot change tools with cutter radius compensation on")
		return INTERP_ERROR

        print "change_prolog current_tool=%f selected_pocket=%d" % (self.current_tool,self.selected_pocket)
	return INTERP_OK

def change_epilog(self, **words):
    retval =  self.return_value
    
    print "change_epilog retval=%f selected_pocket=%d" % (retval,self.selected_pocket)
    if retval > 0.0:
        # commit change
        emccanon.CHANGE_TOOL(self.selected_pocket)
        self.current_pocket = self.selected_pocket
        self.current_tool = self.selected_tool
        # cause a sync()
        self.tool_change_flag = True
        self.set_tool_parameters()
        return INTERP_OK
    else:
        self.set_errormsg("M6 aborted (return code %.4f)" % (retval))
        return INTERP_ERROR


# M61 remapped to an all-Python handler
# demo - this really does the same thing as the builtin (non-remapped) M61
#
# Incantation:
#
# REMAP=M61    python=set_tool_number
#
# python=set_tool_number
#     the following function is executed on M61:
#
# use iocontrolv2 for this to work - CHANGE_TOOL_NUMBER() is broken in iocontrol
#
# This example does directly access the results of parse_block() and does not rely
# on argspec, although it could
def set_tool_number(self):
    cblock = self.blocks[self.remap_level]
    if not cblock.q_flag: 
        self.set_errormsg("M61: Q word required")
        return INTERP_ERROR
    
    tool = nearest_int(cblock.q_number)
    (status,pocket) = self.find_tool_pocket(tool)
    if status != INTERP_OK:
        self.set_errormsg("M61 failed: requested tool %d not in table" % (tool))
        return status
    if tool >= 0:
        self.current_pocket = pocket
        emccanon.CHANGE_TOOL_NUMBER(pocket)
        # test: self.tool_table[0].offset.tran.z = self.tool_table[pocket].offset.tran.z
        # cause a sync()
        #assert self.current_tool = tool
        self.tool_change_flag = True
        self.set_tool_parameters()
        return INTERP_OK
    else:
        self.set_errormsg("M61 failed: Q=%d" % (tool))
        return INTERP_ERROR

