from stdglue import *

# Extract the T number into a parameter, but don't try anything else clever

def get_T_number(self,**words):
    try:
        cblock = self.blocks[self.remap_level]
        if not cblock.t_flag:
            self.set_errormsg("T requires a tool number")
            return INTERP_ERROR
        self.params["tool"] = cblock.t_number
        return INTERP_OK
    except Exception, e:
        self.set_errormsg("T%d/prepare_prolog: %s" % (int(words['t']), e))
        return INTERP_ERROR
