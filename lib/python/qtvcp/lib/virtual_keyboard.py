
# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

import subprocess
import os
from qtvcp.qt_glib import GStat

# Instantiate the libraries with global reference
# GSTAT gives us status messages from linuxcnc
GSTAT = GStat()

class virt_keyboard():
    def _init__(self):
        pass

    def _hal_init(self):
        GSTAT.connect('virtual-keyboard', self.command)
    
    def command(self, w, command):
        if command.lower() == 'show':
            self.init_keyboard()
        else:
            self.kiil_keyboard()

    def init_keyboard( self, args = "", x = "", y = "" ):
        # now we check if onboard or matchbox-keyboard is installed
        try:
            if os.path.isfile( "/usr/bin/onboard" ):
                self.onboard_kb = subprocess.Popen( ["onboard", "--xid", args, x, y],
                                                   stdin = subprocess.PIPE,
                                                   stdout = subprocess.PIPE,
                                                   close_fds = True )
            elif os.path.isfile( "/usr/bin/matchbox-keyboard" ):
                self.onboard_kb = subprocess.Popen( ["matchbox-keyboard", "--xid"],
                                                   stdin = subprocess.PIPE,
                                                   stdout = subprocess.PIPE,
                                                   close_fds = True )
            else:
                return
            sid = self.onboard_kb.stdout.readline()
            log.debug("XID: {}".format(sid))
            #socket = gtk.Socket()
            #socket.show()
            #self.widgets.key_box.add( socket )
            #socket.add_id( long( sid ) )
        except Exception, e:
            pass

    def kill_keyboard( self ):
        try:
            self.onboard_kb.kill()
            self.onboard_kb.terminate()
            self.onboard_kb = None
        except:
            try:
                self.onboard_kb.kill()
                self.onboard_kb.terminate()
                self.onboard_kb = None
            except:
                pass
