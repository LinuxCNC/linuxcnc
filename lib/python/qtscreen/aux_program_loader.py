import hal
import os

# path to TCL for external programs eg. halshow
TCLPATH = os.environ['LINUXCNC_TCL_DIR']

class Aux_program_loader:
    def _init_(self):
        pass

    # check for classicladder realtime if so load the user GUI
    def load_ladder(self,*args):
        if  hal.component_exists('classicladder_rt'):
            p = os.popen("classicladder  &","w")

    # opens halshow
    def load_halshow(self,*args):
        print "halshow",TCLPATH
        p = os.popen("tclsh %s/bin/halshow.tcl &" % (TCLPATH))

    # opens the calibration program
    def load_calibration(self, inipath):
        print "calibration --%s"% inipath
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH,inipath),"w")

    # opens the linuxcnc status program
    def load_status(self,*args):
        p = os.popen("linuxcnctop  > /dev/null &","w")

    # opens a halmeter
    def load_halmeter(self,*args):
        print "halmeter"
        p = os.popen("halmeter &")

    # opens the halscope
    def load_halscope(self,*args):
        p = os.popen("halscope  > /dev/null &","w")

