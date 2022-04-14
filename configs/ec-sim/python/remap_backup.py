import linuxcnc
import emccanon
from interpreter import *
from stdglue import *
from util import lineno
import sys



# raises InterpreterException if execute() or read() fail
throw_exceptions = 1 


########################################################################
# Harmonic Spindle Speed Control
########################################################################
# NOTE: DO NOT use HSSC while threading, it will mess things up

def M600_remap(self,**words):
    # determine what vaiables have been passed
    #for i in words:
    #    print("passing '%s' = %f") % (i, words[i]) 
        
    try:
        cblock = self.blocks[self.remap_level]
        if not cblock.p_flag: 
            self.set_errormsg("HSSC requires a P value") 
            return INTERP_ERROR
        if not cblock.q_flag: 
            self.set_errormsg("HSSC requires a Q value") 
            return INTERP_ERROR            
        if not cblock.r_flag: 
            self.set_errormsg("HSSC requires a R value") 
            return INTERP_ERROR                    
        
        temp1 = float(cblock.p_number)
        temp2 = float(cblock.q_number)
        temp3 = float(cblock.r_number)
        
        # TODO; add conditions limiting P <= 25.5 and I =12.5
        
        # User Mcodes will only accept P and Q variables so we need M150 and M151
        # to set the parameters of HSSC in HAl 
        # M150 sets the period and the amplitude
        # M151 sets the interval and turns HSSC on
        # M152 turns HSSC off 
        self.execute("M150 P%f Q%f" % (temp1,temp2)) 
        self.execute("M151 P%f" % (temp3))     

    except Exception , e:
        self.set_errormsg("M600 HSSC: %s)" % (e))
        return INTERP_ERROR    
    return INTERP_OK

def M601_remap(self):
    # Turn off HSSC and reset the parameters to zero no matter what
    self.execute("M152")


########################################################################
# Spindle Run Inhibit
########################################################################

# HYD. Gear Change prolog; M36,M37,M39    
def spindle_run_inhibit(self,**words):
    # if in preview mode, do nothing 
    if self.task==0:
        return INTERP_OK
    try:
        self.status = linuxcnc.stat()
        self.status.poll()    
        print "start of gear change prolog"
        # spindle run inhibit
        spindle_status = self.params["_spindle_on"]
        print "spindle status:", spindle_status
        if spindle_status == 1:                 
            self.execute("M5", lineno())        
            self.execute("M66 P16 L3 Q5")       
            print "stop spindle command sent" 
        else:
            print"spindle stopped"        
        # zero speed
        # the spindle.0.at-speed pin must be connected to digital input 16
        self.status.poll()
        zero_speed_status = emccanon.GET_EXTERNAL_DIGITAL_INPUT(16,0)
        print "zero speed detect =" , zero_speed_status
        if zero_speed_status != 1:
                self.set_errormsg("spindle_run_inhibit error, spindle still running")
                return INTERP_ERROR        
        else:
            print "spindle_run_inhibit was successful"
            return INTERP_OK
        
    except InterpreterException,e:
        msg = "Danno look at this error : %d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
        self.set_errormsg(msg) # replace builtin error message
        print("%s" % msg)
        return INTERP_ERROR
    except:
        print "Unexpected error:", sys.exc_info()[0]
        raise        

# HYD. Gear Change epilogs M36/M37/M39      
def m36_epilog(self,**words):
    print "start of m36 epilog"
    # look at the limit switch (lsw1 motion.digital.out-00) to see if the M-Code succeeded
    self.status = linuxcnc.stat()
    self.status.poll()
    lsw1_status = emccanon.GET_EXTERNAL_DIGITAL_INPUT(00,0)
    print "lsw1_status:",lsw1_status    
    if lsw1_status != 1:
        # if the m-code routine fails or times out we need to clean up gracefully.
        # pass the fault code to o<on_abort> using parameter 31
        # named parameters (even if they are gloabal) are not recognized at start up
        # numbered parameters are
        self.params[31] = abs(int(200))
        return INTERP_ERROR

    else:
        print "M36 Successful!"
        return INTERP_OK

def m37_epilog(self,**words):
    print "start of m37 epilog"
    # look at the limit switch (lsw3 motion.digital.out-01) to see if the M-Code succeeded
    self.status = linuxcnc.stat()
    self.status.poll()
    lsw1_status = emccanon.GET_EXTERNAL_DIGITAL_INPUT(1,0)
    print "lsw1_status:",lsw1_status    
    if lsw1_status != 1:
        # if the m-code routine fails or times out we need to clean up gracefully.
        # pass the fault code to o<on_abort> using parameter 31
        # named parameters (even if they are gloabal) are not recognized at start up
        # numbered parameters are
        self.params[31] = abs(int(201))
        return INTERP_ERROR

    else:
        print "M37 Successful!"
        return INTERP_OK

def m39_epilog(self,**words):
    print "start of m39 epilog"
    # look at the limit switch (lsw4 motion.digital.out-02) to see if the M-Code succeeded
    self.status = linuxcnc.stat()
    self.status.poll()
    lsw1_status = emccanon.GET_EXTERNAL_DIGITAL_INPUT(2,0)
    print "lsw1_status:",lsw1_status    
    if lsw1_status != 1:
        # if the m-code routine fails or times out we need to clean up gracefully.
        # pass the fault code to o<on_abort> using parameter 31
        # named parameters (even if they are gloabal) are not recognized at start up
        # numbered parameters are
        self.params[31] = abs(int(202))
        return INTERP_ERROR

    else:
        print "M39 Successful!"
        return INTERP_OK




