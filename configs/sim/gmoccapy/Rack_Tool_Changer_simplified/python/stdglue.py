# stdglue - canned prolog and epilog functions for the remappable builtin codes (T,M6,M61,S,F)
#

import emccanon 
from interpreter import *
throw_exceptions = 1


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
                self.selected_pocket =  int(self.params["selected_pocket"])
                emccanon.CHANGE_TOOL(self.selected_pocket)
                self.current_pocket = self.selected_pocket
                self.selected_pocket = -1
                self.selected_tool = -1
                # cause a sync()
                self.set_tool_parameters()
                self.toolchange_flag = True
                yield INTERP_EXECUTE_FINISH
            else:
                # The following errors can be thrown:
                # -101 = wrong pocket number
                # -102 = spindle did not stop
                # -103 = changer has not reached the front position
                # -104 = changer has not realeased the tool
                # -105 = changer has not greped the tool
                # -106 = changer has not reached the back position
                if self.return_value == -101:
                    errormsg = "the pocket number is not within the allowed limites\n"
                    errormsg += "due to LinuxCNC limitations the tool number must match the pocket number\n"
                    errormsg += "and the number must be in the range of 0 to the number of your pockets"
                elif self.return_value == -102:
                    errormsg = "the spindle did not stop\n"
                    errormsg += "check connections as this can cause serious damages"
                elif self.return_value == -103:
                    errormsg = "the changer did not reach the front position\n"
                    errormsg += "can not drop or grep any tool"
                elif self.return_value == -104:
                    errormsg = "changer indicates tool release position has not been reached"
                elif self.return_value == -105:
                    errormsg = "changer indicates tool grep position has not been reached"
                elif self.return_value == -103:
                    errormsg = "the changer did not reach the back position\n"
                    errormsg += "any movement can damage the changer"
                else:
                    errormsg = "M6 aborted (return code {0})".format(self.return_value)
                self.set_errormsg(errormsg)
                yield INTERP_ERROR
    except Exception, e:
        self.set_errormsg("M6/change_epilog: %s" % (e))
        yield INTERP_ERROR

# this should be called from TOPLEVEL __init__()
def init_stdglue(self):
    self.sticky_params = dict()
