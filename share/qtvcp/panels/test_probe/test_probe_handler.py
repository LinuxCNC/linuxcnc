############################
# **** IMPORT SECTION **** #
############################
from PyQt5.QtCore import Qt

from qtvcp.core import Status, Action

# Set up logging
from qtvcp import logger

###########################################
# **** instantiate libraries section **** #
###########################################

STATUS = Status()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths
        #print self.w.USEROPTIONS_
    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        self.w.setWindowFlags(Qt.WindowStaysOnTopHint)

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################
    def buttonPressed(self):
        try:
            self.hal.hal.set_p("motion.probe-input","1")
        except Exception as e:
            print(e)
    def buttonReleased(self):
        try:
            self.hal.hal.set_p("motion.probe-input","0")
        except Exception as e:
            print(e)

    #####################
    # general functions #
    #####################
    #####################
    # KEY BINDING CALLS #
    #####################

    ###########################
    # **** closing event **** #
    ###########################

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
