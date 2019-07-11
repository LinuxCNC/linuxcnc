import os
import linuxcnc
import collections

# Set up logging
import logger
log = logger.getLogger(__name__)
# Set the log level for this module
log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class _IStat(object):
    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >=1:
            return
        self.__class__._instanceNum += 1

        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.inifile = linuxcnc.ini(INIPATH)
        self.MDI_HISTORY_PATH = '~/.axis_mdi_history'
        self.MACHINE_LOG_HISTORY_PATH = '~/.machine_log_history'
        self.PREFERENCE_PATH = '~/.Preferences'
        self.SUB_PATH = None

        self.MACHINE_IS_LATHE = False
        self.MACHINE_IS_METRIC = False
        self.MACHINE_UNIT_CONVERSION = 1
        self.MACHINE_UNIT_CONVERSION_9 = [1]*9
        self.AVAILABLE_AXES = ['X','Y','Z']
        self.AVAILABLE_JOINTS = [0,1,2]
        self.GET_NAME_FROM_JOINT = {0:'X',1:'Y',2:'Z'}
        self.GET_JOG_FROM_NAME = {'X':0,'Y':1,'Z':2}
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
        self.MACHINE_LOG_HISTORY_PATH = self.inifile.find('DISPLAY', 'MESSAGE_HISTORY_FILE') or '~/.machine_log_history'
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
            log.debug('Machine is METRIC based. unit Conversion constant={}'.format(self.MACHINE_UNIT_CONVERSION ))
        else:
            self.MACHINE_IS_METRIC = False
            self.MACHINE_UNIT_CONVERSION = 25.4
            self.MACHINE_UNIT_CONVERSION_9 = [25.4]*3+[1]*3+[25.4]*3
            log.debug('Machine is IMPERIAL based. unit Conversion constant={}'.format(self.MACHINE_UNIT_CONVERSION ))

        axes = self.inifile.find("TRAJ", "COORDINATES")
        if axes is not None: # i.e. LCNC is running, not just in Qt Desinger
            axes = axes.replace(" ", "")
            log.debug('TRAJ COORDINATES: {}'.format(axes))
            self.AVAILABLE_AXES = []
            self.GET_NAME_FROM_JOINT = {}
            self.AVAILABLE_JOINTS = []
            self.GET_JOG_FROM_NAME = {}
            temp = []
            for num, letter in enumerate(axes):
                temp.append(letter)

                # list of available axes
                if letter not in self.AVAILABLE_AXES:
                    self.AVAILABLE_AXES.append(letter.upper())

                # map of axis designation from joint number
                # This allows calling joints x2 or y2 etc
                count = collections.Counter(temp)
                if count[letter]>1: c = letter+str(count[letter])
                else: c = letter
                self.GET_NAME_FROM_JOINT[num] = c

                # map of axis designation to joint-to-jog when in axis mode.
                # so then you can jog either joint of an axis to move the axis
                if count[letter]>1:
                    self.GET_JOG_FROM_NAME[c] = self.GET_JOG_FROM_NAME[letter]
                else:
                    self.GET_JOG_FROM_NAME[c] = num

                # list of availble joint numbers
                self.AVAILABLE_JOINTS.append(num)

                # AXIS sanity check
                av = self.inifile.find('AXIS_%s'% letter.upper(), 'MAX_VELOCITY') or None
                aa = self.inifile.find('AXIS_%s'% letter.upper(), 'MAX_ACCELERATION') or None
                if av is None or aa is None:
                    log.critical('MISSING [AXIS_{}] MAX VeLOCITY or MAX ACCELERATION entry in INI file.'.format(letter.upper()))
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
        temp = self.inifile.find("TRAJ", "COORDINATES")
        if temp:
            self.TRAJ_COORDINATES = temp.lower().replace(" ","")
        else:
            self.TRAJ_COORDINATES = None
        self.JOINT_COUNT = int(self.inifile.find("KINS","JOINTS")or 0)
        self.DEFAULT_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","DEFAULT_LINEAR_VELOCITY", 1)) * 60
        self.MIN_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","MIN_LINEAR_VELOCITY",1)) * 60
        self.MAX_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","MAX_LINEAR_VELOCITY",5)) * 60
        self.DEFAULT_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","DEFAULT_ANGULAR_VELOCITY",6)) * 60
        self.MIN_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","MIN_ABGULAR_VELOCITY",1)) * 60
        self.MAX_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY","MAX_ANGULAR_VELOCITY",60)) * 60
        self.DEFAULT_SPINDLE_SPEED = int(self.get_error_safe_setting("DISPLAY","DEFAULT_SPINDLE_SPEED",200))
        self.MAX_SPINDLE_OVERRIDE = float(self.get_error_safe_setting("DISPLAY","MAX_SPINDLE_OVERRIDE",1)) * 100
        self.MIN_SPINDLE_OVERRIDE = float(self.get_error_safe_setting("DISPLAY","MIN_SPINDLE_OVERRIDE",0.5)) * 100
        self.MAX_FEED_OVERRIDE = float(self.get_error_safe_setting("DISPLAY","MAX_FEED_OVERRIDE",1.5)) * 100
        self.MAX_TRAJ_VELOCITY = float(self.get_error_safe_setting("TRAJ","MAX_LINEAR_VELOCITY",
                                    self.get_error_safe_setting("AXIS_X","MAX_VELOCITY", 5) )) * 60
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

        # XEmbed tabs
        self.TAB_NAMES = (self.inifile.findall("DISPLAY", "EMBED_TAB_NAME")) or None
        self.TAB_LOCATIONS = (self.inifile.findall("DISPLAY", "EMBED_TAB_LOCATION")) or []
        self.TAB_CMDS   = (self.inifile.findall("DISPLAY", "EMBED_TAB_COMMAND")) or None
        if self.TAB_NAMES is not None and len(self.TAB_NAMES) != len(self.TAB_CMDS):
            log.critical('Embeded tab configuration -invalaid number of TAB_NAMES vrs TAB_CMDs')
        if self.TAB_NAMES is not None and len(self.TAB_LOCATIONS) != len(self.TAB_NAMES):
            log.warning('Embeded tab configuration -invalaid number of TAB_NAMES vrs TAB_LOCACTION - guessng default.')
            for num,i in enumerate(self.TAB_NAMES):
                try:
                    if self.TAB_LOCATIONS[num]:
                        continue
                except:
                    self.TAB_LOCATIONS.append("default")
        try:
            self.ZIPPED_TABS = zip(self.TAB_NAMES,self.TAB_LOCATIONS,self.TAB_CMDS)
        except:
            self.ZIPPED_TABS = None

        self.MDI_COMMAND_LIST = (self.inifile.findall("MDI_COMMAND_LIST", "MDI_COMMAND")) or None
        self.TOOL_FILE_PATH = str(self.get_error_safe_setting("EMCIO", "TOOL_TABLE"))
        self.POSTGUI_HALFILE_PATH = (self.inifile.find("HAL", "POSTGUI_HALFILE")) or None

    ###################
    # helper functions
    ###################

    def get_error_safe_setting(self, heading, detail, default=None):
        result = self.inifile.find(heading, detail)
        if result:
            return result
        else:
            log.warning('INI Parcing Error, No {} Entry in {}, Using: {}'.format(detail, heading, default))
            return default

    def convert_metric_to_machine(self, data):
        if self.MACHINE_IS_METRIC:
            return data
        else:
            return data * (1/25.4)

    def convert_imperial_to_machine(self, data):
        if self.MACHINE_IS_METRIC:
            return data * 25.4
        else:
            return data 

    def convert_9_metric_to_machine(self,v):
        if self.MACHINE_IS_METRIC:
            return v
        else:
            c = [1.0/25.4]*3+[1]*3+[1.0/25.4]*3
            return map(lambda x,y: x*y, v, c)

    def convert_9_imperial_to_machine(self,v):
        if self.MACHINE_IS_METRIC:
            c = [25.4]*3+[1]*3+[25.4]*3
            return map(lambda x,y: x*y, v, c)
        else:
            return v

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
                k = k.replace('.',' *.')
                k = k.replace(',',' ')
                all_extensions.append( ( ';;%s (%s)'%(v,k)) )
            all_extensions.append((';;All (*)'))
            temp =''
            for i in all_extensions:
                temp = '%s %s'%(temp ,i)
            return temp
        except:
            return ('All (*)')


