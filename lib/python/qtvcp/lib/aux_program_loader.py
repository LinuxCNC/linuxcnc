import hal
import os
import subprocess
import threading
import time
from qtvcp.core import Action, Path

ACTION = Action()
PATH = Path()

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
    def load_ladder(self, *args):
        if hal.component_exists('classicladder_rt'):
            p = os.popen("classicladder  &", "w")
        else:
            ACTION.SET_ERROR_MESSAGE("Classiclader's realtime component is not present\n So the user component will not  be loaded.")

    def load_gcode_ripper(self,*args):
        if args:
            pass
        else:
            p = os.popen('python3 {}'.format(os.path.join(PATH.LIBDIR, 'ripper/gcode_ripper.py')))

    # opens halshow
    def load_halshow(self, *args):
        if args:
            self.load_haltool_args('halshow', args)
        else:
            p = os.popen("tclsh %s/bin/halshow.tcl &" % (TCLPATH))

    # opens the calibration program
    def load_calibration(self):
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH, INIPATH), "w")

    # opens the linuxcnc status program
    def load_status(self, *args):
        p = os.popen("linuxcnctop  > /dev/null &", "w")

    # opens a halmeter
    def load_halmeter(self, *args):
        if args:
            self.load_haltool_args('halmeter', args)
        else:
            p = os.popen("halmeter &")

    # opens the halscope
    def load_halscope(self, *args):
        if args:
            self.load_haltool_args('halscope', args)
        else:
            p = os.popen("halscope  > /dev/null &", "w")

    # open linuxcnc standard tool edit program
    def load_tooledit(self, filepath):
        p = os.popen("tooledit %s" % (filepath))

    def load_test_button(self, *args):
        if args:
            p = os.popen("qtvcp {} test_button".format(args), "w")
        else:
            p = os.popen("qtvcp test_button", "w")

    def load_test_led(self, *args):
        if args:
            p = os.popen("qtvcp {} test_led".format(args), "w")
        else:
            p = os.popen("qtvcp test_led", "w")

    def load_test_dial(self, *args):
        if args:
            p = os.popen("qtvcp {} test_dial".format(args), "w")
        else:
            p = os.popen("qtvcp test_dial", "w")

    def keyboard_onboard(self, args="", width="", height=""):
        try:
            self.ob = subprocess.Popen(["onboard", args, width, height],
                                       stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE,
                                       close_fds=True)
            if 'xid' in args:
                idnum = self.ob.stdout.readline()
                return idnum
            else:
                return True
        except:
            print('Onboard keyboard could not be loaded by aux_program_loader')
            return False

    def new_thread(self, args):
        try:
            reply = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except Exception as e:
            pass
        stdout, stderr = reply.communicate()
        if stdout:
            self.threadError = stdout.decode()
        if stderr:
            self.threadError = stderr.decode()

    # used via above functions to open halshow, halmeter, or halscope with arguments
    def load_haltool_args(self, tool, args):
            args = args[0].split()
            args.insert(0, tool)
            self.threadError = ''
            t = threading.Thread(target=self.new_thread, args=(args,), daemon=True)
            t.start()
            #this delay is necessary to allow the thread to finish on error
            time.sleep(0.5)
            if self.threadError:
                if 'Cannot read file' in self.threadError:
                    err = 'AUX LOAD ERROR:\nCannot open \'{}\'\n'.format(args[1:][0])
                elif 'is not a valid probe type' in self.threadError:
                    err = 'AUX LOAD ERROR:\n\'{}\' is not a valid probe type\n'.format(args[1:][0])
                ACTION.SET_ERROR_MESSAGE(err)
