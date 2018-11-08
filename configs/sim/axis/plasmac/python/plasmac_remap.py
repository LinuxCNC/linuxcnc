# mostly copied from nc_files/remap_lib/python-stdglue/stdglue.py
#
# changed to set plasmac.requested-velocity pin to the
# return value from plasmac_feed.ngc
#

import emccanon
from interpreter import *
import subprocess as sp
import hal
throw_exceptions = 1

# REMAP=F   prolog=setfeed_prolog  ngc=setfeed epilog=setfeed_epilog
# exposed parameter: #<feed>

def plasmac_feed_prolog(self,**words):
    try:
        c = self.blocks[self.remap_level]
        if not c.f_flag:
            self.set_errormsg("F requires a value")
            return INTERP_ERROR
        self.params["feed"] = c.f_number
    except Exception,e:
        self.set_errormsg("F/setfeed_prolog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK

def plasmac_feed_epilog(self,**words):
    try:
        if not self.value_returned:
            r = self.blocks[self.remap_level].executing_remap
            self.set_errormsg("the %s remap procedure %s did not return a value"
                             % (r.name,r.remap_ngc if r.remap_ngc else r.remap_py))
            return INTERP_ERROR
        if self.blocks[self.remap_level].builtin_used:
            self.params[31] = self.return_value
            sp.Popen('halcmd setp plasmac.requested-velocity %f' % self.return_value, shell=True)
            pass
            #print "---------- F builtin recursion, nothing to do"
        else:
            self.feed_rate = self.params["feed"]
            emccanon.enqueue_SET_FEED_RATE(self.feed_rate)
            sp.Popen('halcmd setp plasmac.requested-velocity %f' % self.feed_rate, shell=True)
        return INTERP_OK
    except Exception,e:
        self.set_errormsg("F/setfeed_epilog: %s)" % (e))
        return INTERP_ERROR
    return INTERP_OK


