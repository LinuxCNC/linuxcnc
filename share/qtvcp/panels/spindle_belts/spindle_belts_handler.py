#!/usr/bin/env python3
print("hello")

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


# Set the log level for this module
log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the QtVCP files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths

    ##########################################
    # SPECIAL FUNCTIONS SECTION              #
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    # This is where you make HAL pins or initialize state of widgets etc
    def initialized__(self):
        log.debug('INIT qtvcp handler')
        if not self.w.MAIN.PREFS_:
            print("CRITICAL - no preference file found, enable preferences in screenoptions widget")
            return

        if self.w.MAIN.PREFS_:

            # set saved front belt button checked, based on number 
            belt_en = self.w.MAIN.PREFS_.getpref('Front_Belt_enabled', 1, int, 'SPINDLE_EXTRAS')
            if belt_en == 1:
                self.w.belt_1.setChecked(True)
            elif belt_en == 2:
                self.w.belt_2.setChecked(True)
            elif belt_en == 3:
                self.w.belt_3.setChecked(True)
            elif belt_en == 4:
                self.w.belt_4.setChecked(True)

            # set saved back belt button checked, based on number
            belt_en = self.w.MAIN.PREFS_.getpref('Back_Belt_enabled', 6, int, 'SPINDLE_EXTRAS')
            if belt_en == 5:
                self.w.belt_5.setChecked(True)
            elif belt_en == 6:
                self.w.belt_6.setChecked(True)
            elif belt_en == 7:
                self.w.belt_7.setChecked(True)
            elif belt_en == 8:
                self.w.belt_8.setChecked(True)


    #######################
    # CALLBACKS FROM FORM #
    #######################

    def frontBeltSelected(self, number):
        print('Front Belt # ',number, " pressed")
        self.w.belt_5.setEnabled(True)
        self.w.belt_6.setEnabled(True)
        self.w.belt_7.setEnabled(True)            
        self.w.belt_8.setEnabled(True)
        backnumber = number + 4
        if (backnumber == 5):
            self.w.belt_5.setEnabled(False)                
        elif (backnumber == 6):
            self.w.belt_6.setEnabled(False)
        elif (backnumber == 7):
            self.w.belt_7.setEnabled(False)
        elif (backnumber == 8):
            self.w.belt_8.setEnabled(False)   
        print("Disabling Back Number ", backnumber)

    def backBeltSelected(self, number):
        print('Back Belt # ',number, " pressed")
        self.w.belt_1.setEnabled(True)
        self.w.belt_2.setEnabled(True)
        self.w.belt_3.setEnabled(True)
        self.w.belt_4.setEnabled(True)
        frontnumber = number - 4
        if (frontnumber == 1):
            self.w.belt_1.setEnabled(False)
        elif (frontnumber == 2):
            self.w.belt_2.setEnabled(False)
        elif (frontnumber == 3):
            self.w.belt_3.setEnabled(False)
        elif (frontnumber == 4):
            self.w.belt_4.setEnabled(False)
        print("Disabling Front Number ", frontnumber)
        

        
    ##############################
    # required class boiler code #
    ##############################
    def closing_cleanup__(self):
        print('***CLOSE***', self.w.belt_1.isChecked())
        if self.w.MAIN.PREFS_:

            # find currently selected front belt button and save it's number
            # default to 1
            temp = 1
            for num,i in enumerate(['belt_1','belt_2','belt_3','belt_4']):
                print (type(i),i,num)
                if self.w[i].isChecked():
                    temp = num +1
            self.w.MAIN.PREFS_.putpref('Front_Belt_enabled', temp, int, 'SPINDLE_EXTRAS')

            # find currently selected back belt button and save it's number
            # default to 5
            temp = 5
            for num,i in enumerate(['belt_5','belt_6','belt_7','belt_8']):
                print (type(i),i,num)
                if self.w[i].isChecked():
                    temp = num +5
            self.w.MAIN.PREFS_.putpref('Back_Belt_enabled', temp, int, 'SPINDLE_EXTRAS')

    def __getitem__(self, item):
        return getattr(self, item) 
    def __setitem__(self, item, value):
        return setattr(self, item, value)

################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
