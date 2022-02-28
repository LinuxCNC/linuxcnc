############################
# **** IMPORT SECTION **** #
############################
from PyQt5.QtWidgets import (QMessageBox)
from qtvcp.core import Status, Action

# Set up logging
from qtvcp import logger
from qtvcp.widgets.dialog_widget import ToolDialog
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
        self.init_tool_dialog()

    # don't ever show the main window
    def show(self):
        pass

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################

    #####################
    # general functions #
    #####################
    def init_tool_dialog(self):
        w = self.w
        w.toolDialog_ = ToolDialog()
        w.toolDialog_.setObjectName('toolDialog_')
        w.toolDialog_.hal_init(HAL_NAME='')
        w.toolDialog_.removeButton( w.toolDialog_._actionbutton)
        if self.w.USEROPTIONS_ is not None:
            if 'notify_on' in self.w.USEROPTIONS_:
                w.toolDialog_.setProperty('useDesktopNotify', True)
            if 'audio_on' in self.w.USEROPTIONS_:
                w.toolDialog_.play_sound = True
            if 'speak_on' in self.w.USEROPTIONS_:
                w.toolDialog_.speak = True
        w.toolDialog_.setProperty('frameless', False)

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
