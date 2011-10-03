import sys
from interpreter import *
import emccanon

#
# M6 remapped to a NGC handler, with Python prolog+epilog
#
# Incantation:
# REMAP=M6   modalgroup=6  prolog=change_prolog ngc=change epilog=change_epilog
#
def change_prolog(self, **words):
	if self.selected_pocket < 0:
		self.set_errormsg("Need tool prepared -Txx- for toolchange")
		return INTERP_ERROR
	if self.cutter_comp_side:
		self.set_errormsg("Cannot change tools with cutter radius compensation on")
		return INTERP_ERROR

        #print "change_prolog current_tool=%f selected_pocket=%d" % (self.current_tool,self.selected_pocket)
	return INTERP_OK

def change_epilog(self, **words):
    retval =  self.return_value
    
    print "change_epilog retval=%f selected_pocket=%d" % (retval,self.selected_pocket)
    if retval > 0.0:
        # commit change
        emccanon.CHANGE_TOOL(self.selected_pocket)
        self.current_pocket = self.selected_pocket
        # cause a sync()
        self.tool_change_flag = True
        self.set_tool_parameters()
        return INTERP_OK
    else:
        self.set_errormsg("M6 aborted (return code %.4f)" % (retval))
        return INTERP_ERROR

