import os
import linuxcnc

# Set up logging
import logger
log = logger.getLogger(__name__)
# Set the log level for this module
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class _IStat(object):

    def __init__(self):

        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.inifile = linuxcnc.ini(INIPATH)
        self.MDI_HISTORY_PATH = '~/.axis_mdi_history'
        self.PREFERENCE_PATH = '~/.Preferences'
        self.SUB_PATH = None

        self.MACHINE_IS_LATHE = False
        self.MACHINE_IS_METRIC = False
        self.MACHINE_UNIT_CONVERSION = 1
        self.MACHINE_UNIT_CONVERSION_9 = [1]*9
        self.AVAILABLE_AXES = ('X','Y','Z')
        self.AVAILABLE_AXES_INT = (0,1,2)
        self.NO_HOME_REQUIRED = False
        self.JOG_INCREMENTS = None
        self.ANGULAR_INCREMENTS = None

        self.MAX_LINEAR_VELOCITY = 60
        self.DEFAULT_LINEAR_VELOCITY = 15.0

        self.DEFAULT_SPINDLE_SPEED = 200
        self.MAX_FEED_OVERRIDE = 1.5
        self.MAX_SPINDLE_OVERRIDE = 1.5
        self.MIN_SPINDLE_OVERRIDE = 0.5

        self.update()

    def update(self):
        self.MDI_HISTORY_PATH = self.inifile.find('DISPLAY', 'MDI_HISTORY_FILE') or '~/.axis_mdi_history'
        self.PREFERENCE_PATH = self.inifile.find("DISPLAY","PREFERENCE_FILE_PATH") or None
        self.SUB_PATH = (self.inifile.find("RS274NGC", "SUBROUTINE_PATH")) or None
        self.MACHINE_IS_LATHE = bool(self.inifile.find("DISPLAY", "LATHE"))
        extensions = self.inifile.findall("FILTER", "PROGRAM_EXTENSION")
        self.PROGRAM_FILTERS = ([e.split(None, 1) for e in extensions]) or None
        self.PARAMETER_FILE = (self.inifile.find("RS274NGC", "PARAMETER_FILE")) or None
        try:
            # check the ini file if UNITS are set to mm"
            # first check the global settings
            units=self.inifile.find("TRAJ","LINEAR_UNITS")
            if units==None:
                # else then the X axis units
                units=self.inifile.find("AXIS_0","UNITS")
        except:
            units = "inch"
        # set up the conversion arrays based on what units we discovered
        if units=="mm" or units=="metric" or units == "1.0":
            self.MACHINE_IS_METRIC = True
            self.MACHINE_UNIT_CONVERSION = 1.0/25.4
            self.MACHINE_UNIT_CONVERSION_9 = [1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.MACHINE_IS_METRIC = False
            self.MACHINE_UNIT_CONVERSION = 25.4
            self.MACHINE_UNIT_CONVERSION_9 = [25.4]*3+[1]*3+[25.4]*3

        axes = self.inifile.find("TRAJ", "COORDINATES")
        if axes is not None: # i.e. LCNC is running, not just in Qt Desinger
            axes = axes.replace(" ", "")
            log.debug('TRAJ COORDINATES: {}'.format(axes))
            conversion = {"X":0, "Y":1, "Z":2, "A":3, "B":4, "C":5, "U":6, "V":7, "W":8}
            self.AVAILABLE_AXES = []
            self.AVAILABLE_AXES_INT = []
            for letter in axes:
                self.AVAILABLE_AXES.append(letter.upper())
                self.AVAILABLE_AXES_INT.append(conversion[letter.upper()])

        self.NO_HOME_REQUIRED = int(self.inifile.find("TRAJ", "NO_FORCE_HOMING") or 0)

        # jogging increments
        increments = self.inifile.find("DISPLAY", "INCREMENTS")
        if increments:
            if "," in increments:
                self.JOG_INCREMENTS = [i.strip() for i in increments.split(",")]
            else:
                self.JOG_INCREMENTS = increments.split()
            if not "continuous" in increments:
                self.JOG_INCREMENTS.insert(0, "Continuous")
        else:
            if self.MACHINE_IS_METRIC:
                self.JOG_INCREMENTS = ["Continuous",".001 mm",".01 mm",".1 mm","1 mm"]
            else:
                self.JOG_INCREMENTS = ["Continuous",".0001 in",".001 in",".01 in",".1 in"]

        # angular jogging increments
        increments = self.inifile.find("DISPLAY", "ANGULAR_INCREMENTS")
        if increments:
            if "," in increments:
                self.ANGULAR_INCREMENTS = [i.strip() for i in increments.split(",")]
            else:
                self.ANGULAR_INCREMENTS = increments.split()
            if not "continuous" in increments:
                self.ANGULAR_INCREMENTS.insert(0, "Continuous")
        else:
            self.ANGULAR_INCREMENTS = ["Continuous","1","45","180","360"]
        self.TRAJ_COORDINATES = self.inifile.find("TRAJ", "COORDINATES").lower().replace(" ","")
        self.JOINT_COUNT = int(self.inifile.find("KINS","JOINTS"))
        self.DEFAULT_LINEAR_JOG_VEL = float(self.inifile.find("DISPLAY","DEFAULT_LINEAR_VELOCITY") or 1) * 60
        self.MAX_LINEAR_JOG_VEL = float(self.inifile.find("DISPLAY","MAX_LINEAR_VELOCITY") or 5) * 60
        self.DEFAULT_SPINDLE_SPEED = int(self.inifile.find("DISPLAY","MAX_SPINDLE_SPEED") or 200)
        self.MAX_SPINDLE_OVERRIDE = float(self.inifile.find("DISPLAY","MAX_SPINDLE_OVERRIDE") or 1) * 100
        self.MIN_SPINDLE_OVERRIDE = float(self.inifile.find("DISPLAY","MIN_SPINDLE_OVERRIDE") or 0.5) * 100
        self.MAX_FEED_OVERRIDE = float(self.inifile.find("DISPLAY","MAX_FEED_OVERRIDE") or 1.5) * 100

        # user message dialog system
        self.USRMESS_BOLDTEXT = self.inifile.findall("DISPLAY", "MESSAGE_BOLDTEXT")
        self.USRMESS_TEXT = self.inifile.findall("DISPLAY", "MESSAGE_TEXT")
        self.USRMESS_TYPE = self.inifile.findall("DISPLAY", "MESSAGE_TYPE")
        self.USRMESS_PINNAME = self.inifile.findall("DISPLAY", "MESSAGE_PINNAME")
        self.USRMESS_DETAILS = self.inifile.findall("DISPLAY", "MESSAGE_DETAILS")
        self.USRMESS_ICON = self.inifile.findall("DISPLAY", "MESSAGE_ICON")
        if len(self.USRMESS_TEXT) != len(self.USRMESS_TYPE):
            log.warning('Invalid message configuration (missing text or type) in INI File [DISPLAY] section ')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_PINNAME):
            log.warning('Invalid message configuration (missing pinname) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_BOLDTEXT):
            log.warning('Invalid message configuration (missing boldtext) in INI File [DISPLAY] sectioN')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_DETAILS):
            log.warning('Invalid message configuration (missing details) in INI File [DISPLAY] sectioN')
        try:
            self.ZIPPED_USRMESS = zip(self.USRMESS_BOLDTEXT,self.USRMESS_TEXT,self.USRMESS_DETAILS,self.USRMESS_TYPE,self.USRMESS_PINNAME)
        except:
            self.ZIPPED_USRMESS = None
        self.TAB_NAMES = self.inifile.findall("DISPLAY", "EMBED_TAB_NAME")
        self.TAB_LOCATION = self.inifile.findall("DISPLAY", "EMBED_TAB_LOCATION")
        self.TAB_CMD   = self.inifile.findall("DISPLAY", "EMBED_TAB_COMMAND")

    ###################
    # helper functions
    ###################
    def convert_units(self, data):
        return data * self.MACHINE_UNIT_CONVERSION

    def convert_units_9(self,v):
        c = self.MACHINE_UNIT_CONVERSION_9
        return map(lambda x,y: x*y, v, c)

    # This finds the filter program's initilizing
    # program eg python for .py from INI
    def get_filter_program(self, fname):
        ext = os.path.splitext(fname)[1]
        if ext:
            return self.inifile.find("FILTER", ext[1:])
        else:
            return None

    # get filter extensions in QT format
    def get_qt_filter_extensions(self,):
        all_extensions = [("G code (*.ngc)")]
        try:
            for k, v in self.PROGRAM_FILTERS:
                k = k.replace('.','*.')
                all_extensions.append( ( ';;%s(%s)'%(v,k)) )
            temp =''
            for i in all_extensions:
                temp = '%s %s'%(temp ,i)
            return temp
        except:
            return all_extensions[0]

class IStat(_IStat):
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _IStat.__new__(cls, *args, **kwargs)
        return cls._instance
