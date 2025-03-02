############################
# **** IMPORT SECTION **** #
############################
import os
from qtvcp.core import Status, Action, Info

# Set up logging
from qtvcp import logger
from qtvcp.lib.message import Message

###########################################
# **** instantiate libraries section **** #
###########################################

STATUS = Status()
ACTION = Action()
MSG = Message()
INFO = Info()

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
        #print self.w.USEROPTIONS_
        if paths.INI_PATH is None:
            LOG.error('No INI specified')
            raise SystemExit('No INI specified')
        elif not os.path.exists(paths.INI_PATH):
            LOG.error('INI specified does not exist')
            raise SystemExit('INI specified does not exist')
        elif INFO.ZIPPED_USRMESS == []:
            LOG.error('No INI messages found at {}'.format(paths.INI_PATH))
            raise SystemExit('No INI messages found')
        LOG.debug('Found user messages in INI:\n{}\n'.format(INFO.ZIPPED_USRMESS)) 
    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):

        # load message dialogs from ini
        # pass hal component, no parent window, no notify object
        self._msg = MSG.message_setup(self.hal, None, None)

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
