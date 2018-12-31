# stdglue - canned prolog and epilog functions for the remappable builtin codes (T,M6,M61,S,F)
#

import emccanon 
from interpreter import *
throw_exceptions = 1


# REMAP=T   prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
# exposed parameters: #<tool> #<pocket>

def prepare_prolog(self,**words):
    try:
        cblock = self.blocks[self.remap_level]
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            return INTERP_ERROR
        tool  = cblock.t_number
        if tool:
            (status, pocket) = self.find_tool_pocket(tool)
            if status != INTERP_OK:
                self.set_errormsg("T%d: pocket not found" % (tool))
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
                self.set_errormsg("M6 aborted (return code %.1f)" % (self.return_value))
                yield INTERP_ERROR
    except Exception, e:
        self.set_errormsg("M6/change_epilog: %s" % (e))
        yield INTERP_ERROR

# this should be called from TOPLEVEL __init__()
def init_stdglue(self):
    self.sticky_params = dict()
