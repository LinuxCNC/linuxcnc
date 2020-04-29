import hal
import os
import subprocess

# path to TCL for external programs eg. halshow
try:
    TCLPATH = os.environ['LINUXCNC_TCL_DIR']
    INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
except:
    pass

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
    def load_calibration(self):
        print "calibration --%s"% INIPATH
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH, INIPATH), "w")

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

    # open linuxcnc standard tool edit program
    def load_tooledit(self, filepath):
        p = os.popen("tooledit %s" % (filepath))

    def keyboard_onboard(self,args="",width="",height=""):
        try:
            self.ob = subprocess.Popen(["onboard",args,width,height],
                                       stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE,
                                       close_fds=True)
            if 'xid' in args:
                idnum = self.ob.stdout.readline()
                return idnum
            else:
                return True
        except:
            print 'Onboard keyboard could not be loaded by aux_program_loader'
            return False

