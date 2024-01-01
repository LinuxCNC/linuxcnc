############################
# **** IMPORT SECTION **** #
############################
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
    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        try:
            if self.w.PREFS_:
                LOG.debug('Using preference file:',self.w.PREFS_.fn)
        except:
            pass
        number = 0
        if self.w.USEROPTIONS_ is not None:
            LOG.info('cam_align user options: {}'.format(self.w.USEROPTIONS_))
            for num, i in enumerate(self.w.USEROPTIONS_):

                # override the default width and height of the window
                if 'size=' in self.w.USEROPTIONS_[num]:
                    try:
                        strg = self.w.USEROPTIONS_[num].strip('size=')
                        arg = strg.split(',')
                        self.w.resize(int(arg[0]),int(arg[1]))
                    except Exception as e:
                        print('Error with cam_align size setting:',self.w.USEROPTIONS_[num])

                elif 'rotincr=' in self.w.USEROPTIONS_[num]:
                    try:
                        strg = self.w.USEROPTIONS_[num].strip('rotincr=')
                        self.w.camview.rotationIncrement = float(strg)
                    except Exception as e:
                        print('Error with cam_align rotation increment setting:',self.w.USEROPTIONS_[num])

                # X axis scale number to use
                elif 'xscale=' in self.w.USEROPTIONS_[num]:
                    try:
                        self.scaleX = float(self.w.USEROPTIONS_[num].strip('xscale='))
                    except Exception as e:
                        print('Error with cam_align X axis scale - not a number - using 1.0')

                # camera Y axis scale number to use
                elif 'yscale=' in self.w.USEROPTIONS_[num]:
                    try:
                        self.scaleY =  float(self.w.USEROPTIONS_[num].strip('yscale='))
                    except Exception as e:
                        print('Error with cam_align camera Y axis scale- not a number - using 1.0')

                # camera number to use
                elif 'camnumber=' in self.w.USEROPTIONS_[num]:
                    try:
                        number = int(self.w.USEROPTIONS_[num].strip('camnumber='))
                    except Exception as e:
                        print('Error with cam_align camera selection - not a number - using 0')

                # camera number to use (legacy)
                elif len(self.w.USEROPTIONS_[num]) == 1 and self.w.USEROPTIONS_[num].isdigit():
                    try:
                        number = int(self.w.USEROPTIONS_[num])
                    except:
                        print('Error with cam_align camera selection - not a number - using 0')
        self.w.camview._camNum = number

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
