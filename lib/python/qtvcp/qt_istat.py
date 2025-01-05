import os
import linuxcnc
import collections
import configparser

PARSER = configparser.RawConfigParser
PARSER.optionxform = str

# Set up logging
from . import logger

log = logger.getLogger(__name__)
# Force the log level for this module
#log.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

try:
    LINUXCNCVERSION = os.environ['LINUXCNCVERSION']
except:
    LINUXCNCVERSION = 'UNAVAILABLE'

INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')

HOME = os.environ.get('LINUXCNC_HOME', '/usr')
if HOME is not None:
    IMAGEDIR = os.path.join(HOME, "share", "qtvcp", "images")
else:
    IMAGEDIR = None


class _IStat(object):
    def __init__(self, ini=None):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
 
        self.__class__._instanceNum += 1

        self.LINUXCNC_IS_RUNNING = bool(INIPATH != '/dev/null')
        self.ENVIRO_INI_PATH = INIPATH

        if not self.LINUXCNC_IS_RUNNING:
            # Reset the log level for this module
            # Linuxcnc isn't running so we expect INI errors
            log.setLevel(logger.CRITICAL)

        self.LINUXCNC_VERSION = LINUXCNCVERSION

        self.MACRO_PATH_LIST = []
        self.SUB_PATH_LIST = []
        self.USER_M_PATH_LIST = []

        self.IMAGE_PATH = IMAGEDIR
        self.LIB_PATH = os.path.join(HOME, "share", "qtvcp")
        self.TITLE = ""
        self.ICON = ""
        # this is updated in qtvcp.py on startup
        self.IS_SCREEN = False

        # if no INI was given use the one from the environment
        if ini is None:
            ini = INIPATH

        self.INIPATH = ini or  '/dev/null'
        log.debug('INI Path: {}'.format(self.INIPATH))

        self.INI = linuxcnc.ini(self.INIPATH)

        self.update()

    def update(self):

        # use configParser so we can iter thru header
        self.parser = PARSER(strict=False)
        try:
            self.parser.read(filenames=self.INIPATH)
        except:
            pass

        ct = float(self.INI.find('DISPLAY', 'CYCLE_TIME') or 100) # possibly in seconds or ms
        if ct < 1:
            self.CYCLE_TIME = int(ct * 1000)
        else:
            self.CYCLE_TIME = int(ct)
        self.GRAPHICS_CYCLE_TIME =int(self.INI.find('DISPLAY', 'GRAPHICS_CYCLE_TIME') or 100) # in seconds
        self.HALPIN_CYCLE_TIME = int(self.INI.find('DISPLAY', 'HALPIN_CYCLE_TIME') or 100) # in seconds
        self.MDI_HISTORY_PATH = self.INI.find('DISPLAY', 'MDI_HISTORY_FILE') or '~/.axis_mdi_history'
        self.QTVCP_LOG_HISTORY_PATH = self.INI.find('DISPLAY', 'LOG_FILE') or '~/qtvcp.log'
        self.MACHINE_LOG_HISTORY_PATH = self.INI.find('DISPLAY', 'MACHINE_LOG_PATH') or '~/.machine_log_history'
        self.PREFERENCE_PATH = self.INI.find("DISPLAY", "PREFERENCE_FILE_PATH") or None
        self.PROGRAM_PREFIX = self.get_error_safe_setting("DISPLAY", "PROGRAM_PREFIX", '~/linuxcnc/nc_files')
        if not os.path.exists(os.path.expanduser(self.PROGRAM_PREFIX)):
            log.warning('Path not valid in INI File [DISPLAY] PROGRAM_PREFIX section')

        temp = self.INI.find("DISPLAY", "USER_COMMAND_FILE")
        if not temp is None:
            self.USER_COMMAND_FILE = os.path.expanduser(temp)
        else:
            self.USER_COMMAND_FILE = None

        self.STARTUP_CODES = (self.INI.find('RS274NGC', 'RS274NGC_STARTUP_CODE') ) or None

        self.SUB_PATH = (self.INI.find("RS274NGC", "SUBROUTINE_PATH")) or None
        if self.SUB_PATH is not None:
            for mpath in (self.SUB_PATH.split(':')):
                self.SUB_PATH_LIST.append(mpath)
                if 'macro' in mpath:
                    path = mpath
                    self.MACRO_PATH_LIST.append(mpath)
            self.MACRO_PATH = mpath or None
        else:
            self.MACRO_PATH = None

        self.USER_M_PATH = (self.INI.find("RS274NGC", "USER_M_PATH")) or None
        if self.USER_M_PATH is not None:
            for mpath in (self.USER_M_PATH.split(':')):
                self.USER_M_PATH_LIST.append(mpath)

        self.INI_MACROS = self.INI.findall("DISPLAY", "MACRO")

        self.NGC_SUB_PATH = (self.INI.find("DISPLAY","NGCGUI_SUBFILE_PATH")) or None
        if not self.NGC_SUB_PATH is None:
            self.NGC_SUB_PATH = os.path.expanduser(self.NGC_SUB_PATH)
        self.NGC_SUB = (self.INI.findall("DISPLAY", "NGCGUI_SUBFILE")) or None

        self.MACHINE_IS_LATHE = bool(self.INI.find("DISPLAY", "LATHE"))
        try:
            self.MACHINE_IS_QTPLASMAC = 'qtplasmac' in self.INI.find("DISPLAY", "DISPLAY")
        except:
            self.MACHINE_IS_QTPLASMAC = False
        extensions = self.INI.findall("FILTER", "PROGRAM_EXTENSION")
        self.PROGRAM_FILTERS = ([e.split(None, 1) for e in extensions]) or None
        self.PROGRAM_FILTERS_EXTENSIONS = self.get_filters_extensions()
        self.VALID_PROGRAM_EXTENSIONS = self.get_all_valid_extensions()

        self.PARAMETER_FILE = (self.INI.find("RS274NGC", "PARAMETER_FILE")) or None
        try:
            # check the INI file if UNITS are set to mm"
            # first check the global settings
            units = self.INI.find("TRAJ", "LINEAR_UNITS")
            if units is None:
                if self.LINUXCNC_IS_RUNNING:
                    log.critical('Missing LINEAR_UNITS in TRAJ, guessing units for machine from JOINT 0')
                # else then guess; The joint 0 is usually X axis
                units = self.INI.find("JOINT_0", "UNITS")
                if units is None:
                    if self.LINUXCNC_IS_RUNNING:
                        log.critical('Missing UNITS in JOINT_0, assuming metric based machine')
                    units = 'metric'
        except:
            units = "metric"
        finally:
            units = units.lower()
        # set up the conversion arrays based on what units we discovered
        if units == "mm" or units == "metric" or units == "1.0":
            self.MACHINE_IS_METRIC = True
            self.MACHINE_UNIT_CONVERSION = 1.0 / 25.4
            self.MACHINE_UNIT_CONVERSION_9 = [1.0 / 25.4] * 3 + [1] * 3 + [1.0 / 25.4] * 3
            log.debug('Machine is METRIC based. unit Conversion constant={}'.format(self.MACHINE_UNIT_CONVERSION))
        else:
            self.MACHINE_IS_METRIC = False
            self.MACHINE_UNIT_CONVERSION = 25.4
            self.MACHINE_UNIT_CONVERSION_9 = [25.4] * 3 + [1] * 3 + [25.4] * 3
            log.debug('Machine is IMPERIAL based. unit Conversion constant={}'.format(self.MACHINE_UNIT_CONVERSION))

        axes = self.INI.find("TRAJ", "COORDINATES")
        try:
            self.trajcoordinates = self.INI.find("TRAJ", "COORDINATES").lower().replace(" ","")
        except:
            self.trajcoordinates ='xyz'

        self.AVAILABLE_AXES = []
        self.AVAILABLE_JOINTS = []
        self.GET_NAME_FROM_JOINT = {}
        self.GET_JOG_FROM_NAME = {}
        if axes is not None:  # i.e. LCNC is running, not just in Qt Designer
            axes = axes.replace(" ", "")
            self.TRAJCO = axes.lower()
            log.debug('TRAJ COORDINATES: {}'.format(axes))
            temp = []
            for num, letter in enumerate(axes):
                temp.append(letter)

                # list of available axes
                if letter not in self.AVAILABLE_AXES:
                    self.AVAILABLE_AXES.append(letter.upper())

                # map of axis designation from joint number
                # This allows calling joints x2 or y2 etc
                count = collections.Counter(temp)
                if count[letter] > 1:
                    c = letter + str(count[letter])
                else:
                    c = letter
                self.GET_NAME_FROM_JOINT[num] = c

                # map of axis designation to joint-to-jog when in axis mode.
                # so then you can jog either joint of an axis to move the axis
                if count[letter] > 1:
                    self.GET_JOG_FROM_NAME[c] = self.GET_JOG_FROM_NAME[letter]
                else:
                    self.GET_JOG_FROM_NAME[c] = num

                # list of available joint numbers
                self.AVAILABLE_JOINTS.append(num)

                # AXIS sanity check
                av = self.INI.find('AXIS_%s' % letter.upper(), 'MAX_VELOCITY') or None
                aa = self.INI.find('AXIS_%s' % letter.upper(), 'MAX_ACCELERATION') or None
                if av is None or aa is None:
                    # some lathe configs have dummy Y axis for axis rotation G code
                    if letter == "Y" and self.MACHINE_IS_LATHE:
                        pass
                    else:
                        log.critical(
                            'MISSING [AXIS_{}] MAX VELOCITY or MAX ACCELERATION entry in INI file.'.format(letter.upper()))

        # convert joint number to axis index
        # used by dro_widget
        self.GET_AXIS_INDEX_FROM_JOINT_NUM = {}
        self.GET_JOINT_NUM_FROM_AXIS_INDEX = {}
        for i in self.AVAILABLE_JOINTS:
            let = self.GET_NAME_FROM_JOINT[i][0]
            axisnum = "XYZABCUVW".index(let)
            self.GET_AXIS_INDEX_FROM_JOINT_NUM[int(i)] = int(axisnum)
            self.GET_JOINT_NUM_FROM_AXIS_INDEX[int(axisnum)] = int(i)

        self.NO_HOME_REQUIRED = int(self.INI.find("TRAJ", "NO_FORCE_HOMING") or 0)

        # home all check
        self.HOME_ALL_FLAG = 1
        # set Home All Flag only if ALL joints specify a HOME_SEQUENCE
        jointcount = len(self.AVAILABLE_JOINTS)
        self.JOINT_SEQUENCE_LIST = {}
        for j in range(jointcount):
            seq = self.INI.find("JOINT_" + str(j), "HOME_SEQUENCE")
            if seq is None:
                seq = 0
                self.HOME_ALL_FLAG = 0
            self.JOINT_SEQUENCE_LIST[j] = int(seq)
        # joint sequence/type
        self.JOINT_TYPE = [None] * jointcount
        self.JOINT_TYPE_INT = [None] * jointcount
        self.JOINT_SEQUENCE = [None] * jointcount
        self.HAS_ANGULAR_JOINT = False
        for j in range(jointcount):
            section = "JOINT_%d" % j
            self.JOINT_TYPE[j] = self.INI.find(section, "TYPE") or "LINEAR"
            if self.JOINT_TYPE[j] == "LINEAR":
                self.JOINT_TYPE_INT[j] = 1
            else:
                self.JOINT_TYPE_INT[j] = 2
                self.HAS_ANGULAR_JOINT = True
            self.JOINT_SEQUENCE[j] = int(self.INI.find(section, "HOME_SEQUENCE") or 0)

        # jog synchronized sequence
        # gives a list of joints combined to make an axis
        templist = []
        for j in self.AVAILABLE_JOINTS:
            temp = []
            flag = False
            for hj, hs in list(self.JOINT_SEQUENCE_LIST.items()):
                if abs(int(hs)) == abs(int(self.JOINT_SEQUENCE_LIST.get(j))):
                    temp.append(hj)
                    if int(hs) < 0:
                        flag = True
            if flag:
                templist.append(temp)
        # remove duplicates
        self.JOINT_SYNC_LIST = list(set(tuple(sorted(sub)) for sub in templist))

        # This is a list of joints that are related to a joint.
        #ie. JOINT_RELATIONS_LIST(0) will give a list of joints that go with joint 0
        # to make an axis or else a list with just 0 in it.
        # current use case is to find out what other joints should be unhomed if you unhome
        # a combined joint axis.
        self.JOINT_RELATIONS_LIST = [None] * jointcount
        for j in range(jointcount):
            temp = []
            for hj, hs in list(self.JOINT_SEQUENCE_LIST.items()):
                # the absolute numbers must be equal first
                if abs(int(hs)) == abs(int(self.JOINT_SEQUENCE_LIST.get(j))):
                    # theN one has to be negative to signal syncing
                    if int(hs) <0 or int(self.JOINT_SEQUENCE_LIST.get(j)) < 0:
                        temp.append(hj)
            # If empty list: no synced joints, just add the jointcount number
            if temp == []:
                temp.append(j)
            self.JOINT_RELATIONS_LIST[j] = temp

        # jogging increments
        increments = self.INI.find("DISPLAY", "INCREMENTS")
        if increments:
            if "," in increments:
                self.JOG_INCREMENTS = [i.strip() for i in increments.split(",")]
            else:
                self.JOG_INCREMENTS = increments.split()
            if not "continuous" in increments:
                self.JOG_INCREMENTS.insert(0, "Continuous")
        else:
            if self.MACHINE_IS_METRIC:
                self.JOG_INCREMENTS = ["Continuous", ".001 mm", ".01 mm", ".1 mm", "1 mm"]
            else:
                self.JOG_INCREMENTS = ["Continuous", ".0001 in", ".001 in", ".01 in", ".1 in"]
            log.warning('Missing [DISPLAY] LINEAR_INCREMENTS- using defaults.')

        # angular jogging increments
        increments = self.INI.find("DISPLAY", "ANGULAR_INCREMENTS")
        if increments:
            if "," in increments:
                self.ANGULAR_INCREMENTS = [i.strip() for i in increments.split(",")]
            else:
                self.ANGULAR_INCREMENTS = increments.split()
            if not "continuous" in increments:
                self.ANGULAR_INCREMENTS.insert(0, "Continuous")
        else:
            self.ANGULAR_INCREMENTS = ["Continuous", "1", "45", "180", "360"]
            if self.HAS_ANGULAR_JOINT:
                log.warning('Missing [DISPLAY] ANGULAR_INCREMENTS- using defaults.')

        # grid increments
        grid_increments = self.INI.find("DISPLAY", "GRIDS")
        if grid_increments:
            if "," in grid_increments:
                self.GRID_INCREMENTS = [i.strip() for i in grid_increments.split(",")]
            else:
                self.GRID_INCREMENTS = grid_increments.split()
            flag = True
            for i in grid_increments:
                if i.upper() in ('0', 'OFF'): flag = False
                break
            if flag:
                self.GRID_INCREMENTS.insert(0, '0')
        else:
            if self.MACHINE_IS_METRIC:
                self.GRID_INCREMENTS = ["0", ".1 mm", "1 mm", "10 mm", "50 mm"]
            else:
                self.GRID_INCREMENTS = ["0", ".5 in", "1 in", "2 in", "6 in"]

        temp = self.INI.find("TRAJ", "COORDINATES")
        if temp:
            self.TRAJ_COORDINATES = temp.lower().replace(" ", "")
        else:
            self.TRAJ_COORDINATES = None
        self.JOINT_COUNT = int(self.INI.find("KINS", "JOINTS") or 0)

        # check for weird kinematics like robots
        self.IS_TRIVIAL_MACHINE = bool('trivkins' in self.get_error_safe_setting("KINS", "KINEMATICS",'trivial'))

        kinsmodule = self.INI.find("KINS", "KINEMATICS") or 'trivkins'
        if kinsmodule.split()[0] == "trivkins":
            self.trivkinscoords = "XYZABCUVW"
            for item in kinsmodule.split():
                if "coordinates=" in item:
                    self.trivkinscoords = item.split("=")[1].upper()

        safe = 25 if self.MACHINE_IS_METRIC else 1
        self.DEFAULT_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "DEFAULT_LINEAR_VELOCITY", safe)) * 60
        self.MIN_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MIN_LINEAR_VELOCITY", 0)) * 60
        safe = 125 if self.MACHINE_IS_METRIC else 5
        self.MAX_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MAX_LINEAR_VELOCITY", safe)) * 60
        log.debug('DEFAULT_LINEAR_VELOCITY = {}'.format(self.DEFAULT_LINEAR_JOG_VEL))
        log.debug('MIN_LINEAR_VELOCITY = {}'.format(self.MIN_LINEAR_JOG_VEL))
        log.debug('MAX_LINEAR_VELOCITY = {}'.format(self.MAX_LINEAR_JOG_VEL))

        self.DEFAULT_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "DEFAULT_ANGULAR_VELOCITY", 6)) * 60
        self.MIN_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MIN_ANGULAR_VELOCITY", 0)) * 60
        self.MAX_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MAX_ANGULAR_VELOCITY", 60)) * 60
        log.debug('DEFAULT_ANGULAR_VELOCITY = {}'.format(self.DEFAULT_ANGULAR_JOG_VEL))
        log.debug('MIN_ANGULAR_VELOCITY = {}'.format(self.MIN_ANGULAR_JOG_VEL))
        log.debug('MAX_ANGULAR_VELOCITY = {}'.format(self.MAX_ANGULAR_JOG_VEL))

        self.AVAILABLE_SPINDLES = int(self.INI.find("TRAJ", "SPINDLES") or 1)
        self.SPINDLE_INCREMENT = int(self.INI.find("DISPLAY", "SPINDLE_INCREMENT") or 100)
        for i in range(0, self.AVAILABLE_SPINDLES):
            self['DEFAULT_SPINDLE_{}_SPEED'.format(i)] = int(
                self.get_error_safe_setting("DISPLAY", "DEFAULT_SPINDLE_{}_SPEED".format(i), 200))
            self['MIN_SPINDLE_{}_SPEED'.format(i)] = int(
                self.get_error_safe_setting("DISPLAY", "MIN_SPINDLE_{}_SPEED".format(i), 100))
            self['MAX_SPINDLE_{}_SPEED'.format(i)] = int(
                self.get_error_safe_setting("DISPLAY", "MAX_SPINDLE_{}_SPEED".format(i), 2500))
            self['MAX_SPINDLE_{}_OVERRIDE'.format(i)] = float(
                self.get_error_safe_setting("DISPLAY", "MAX_SPINDLE_{}_OVERRIDE".format(i), 1)) * 100
            self['MIN_SPINDLE_{}_OVERRIDE'.format(i)] = float(
                self.get_error_safe_setting("DISPLAY", "MIN_SPINDLE_{}_OVERRIDE".format(i), 0.5)) * 100
        # check Legacy
        self.DEFAULT_SPINDLE_SPEED = int(self.INI.find("DISPLAY", "DEFAULT_SPINDLE_SPEED") or -1)
        if self.DEFAULT_SPINDLE_SPEED < 0:
            self.DEFAULT_SPINDLE_SPEED = self.DEFAULT_SPINDLE_0_SPEED
        self.MIN_SPINDLE_SPEED = int(self.INI.find("DISPLAY", "MIN_SPINDLE_SPEED") or -1)
        if self.MIN_SPINDLE_SPEED < 0:
            self.MIN_SPINDLE_SPEED = self.MIN_SPINDLE_0_SPEED
        self.MAX_SPINDLE_SPEED = int(self.INI.find("DISPLAY", "MAX_SPINDLE_SPEED") or -1)
        if self.MAX_SPINDLE_SPEED < 0:
            self.MAX_SPINDLE_SPEED = self.MAX_SPINDLE_0_SPEED
        self.MAX_SPINDLE_OVERRIDE = float(self.INI.find("DISPLAY", "MAX_SPINDLE_OVERRIDE") or -1) * 100
        if self.MAX_SPINDLE_OVERRIDE < 0:
            self.MAX_SPINDLE_OVERRIDE = self.MAX_SPINDLE_0_OVERRIDE
        self.MIN_SPINDLE_OVERRIDE = float(self.INI.find("DISPLAY", "MIN_SPINDLE_OVERRIDE") or -1) * 100
        if self.MIN_SPINDLE_OVERRIDE < 0:
            self.MIN_SPINDLE_OVERRIDE = self.MIN_SPINDLE_0_OVERRIDE

        self.MAX_FEED_OVERRIDE = float(self.get_error_safe_setting("DISPLAY", "MAX_FEED_OVERRIDE", 1.5)) * 100
        if self.INI.find("TRAJ", "MAX_LINEAR_VELOCITY") is None:
            if self.LINUXCNC_IS_RUNNING:
                log.critical('INI Parsing Error, No MAX_LINEAR_VELOCITY Entry in TRAJ')
        self.MAX_TRAJ_VELOCITY = float(self.get_error_safe_setting("TRAJ", "MAX_LINEAR_VELOCITY",
                                            self.get_error_safe_setting("AXIS_X", "MAX_VELOCITY", 5))) * 60

        # user message dialog system
        self.USRMESS_BOLDTEXT = self.INI.findall("DISPLAY", "MESSAGE_BOLDTEXT")
        self.USRMESS_TEXT = self.INI.findall("DISPLAY", "MESSAGE_TEXT")
        self.USRMESS_TYPE = self.INI.findall("DISPLAY", "MESSAGE_TYPE")
        self.USRMESS_PINNAME = self.INI.findall("DISPLAY", "MESSAGE_PINNAME")
        self.USRMESS_DETAILS = self.INI.findall("DISPLAY", "MESSAGE_DETAILS")
        self.USRMESS_ICON = self.INI.findall("DISPLAY", "MESSAGE_ICON")

        if len(self.USRMESS_TEXT) != len(self.USRMESS_TYPE):
            log.warning('Invalid message configuration (missing text or type) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_PINNAME):
            log.warning('Invalid message configuration (missing pinname) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_BOLDTEXT):
            log.warning('Invalid message configuration (missing boldtext) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_DETAILS):
            log.warning('Invalid message configuration (missing details) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_ICON):
            log.warning('Invalid message configuration (missing icon) in INI File [DISPLAY] section')
            if self.USRMESS_ICON == []:
                temp = 'INFO'
            else:
                temp = self.USRMESS_ICON[0]
                self.USRMESS_ICON = []
            for i in self.USRMESS_TEXT:
                self.USRMESS_ICON.append(temp)

        try:
            self.ZIPPED_USRMESS = list(
                zip(self.USRMESS_BOLDTEXT, self.USRMESS_TEXT, self.USRMESS_DETAILS, self.USRMESS_TYPE,
                    self.USRMESS_PINNAME, self.USRMESS_ICON))
        except:
            self.ZIPPED_USRMESS = None

        ##################################
        # user multi message dialog system
        ##################################
        self.USRMULTIMESS_ID = self.INI.findall("DISPLAY", "MULTIMESSAGE_ID") or None
        #print(self.USRMULTIMESS_ID)
        if not self.USRMULTIMESS_ID is None:
            for item in self.USRMULTIMESS_ID:
                NUMBER = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_NUMBER".format(item))
                #print(NUMBER)
                TYPE = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_TYPE".format(item))
                #print ('Type:',TYPE)
                TITLE = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_TITLE".format(item))
                #print(TITLE)
                TEXT = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_TEXT".format(item))
                #print(TEXT)
                DETAILS = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_DETAILS".format(item))
                #print(DETAILS)
                ICON = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_ICON".format(item))
                #print(ICON)
                OPTIONS = self.INI.findall("DISPLAY", "MULTIMESSAGE_{}_OPTIONS".format(item))
                #print(OPTIONS)

            # fix any missing ICON to default to what the first entry was or 'INFO'
            if len(TEXT) != len(ICON):
                log.warning('Invalid MULTI message configuration (missing icon) in INI File [DISPLAY] section')
                if ICON == []:
                    temp = 'INFO'
                else:
                    temp = ICON[0]
                    ICON = []
                for i in TEXT:
                    ICON.append(temp)
                ##print(ICON)

            # fix any missing ICON to default to what the first entry was or 'LOG=True,LEVEL=DEFAULT'
            if len(TEXT) != len(OPTIONS):
                log.warning('Invalid message configuration (missing MESSAGE_OPTIONS) in INI File [DISPLAY] section')
                if OPTIONS == []:
                    temp = 'LOG=True,LEVEL=DEFAULT'
                else:
                    temp = OPTIONS[0]
                    OPTIONS = []
                for i in range(len(TEXT)):
                    OPTIONS.append(temp)

            # process message OPTIONS
            for num,i in enumerate(OPTIONS):
                OPTIONS[num] = self.parse_message_options(i)

            # ZIP them up neatly
            try:
                z = list(zip( TYPE,TITLE,TEXT,DETAILS,ICON,OPTIONS))
            except Exception as e:
                print('error:',e)
                z = None

            # make a dict from the raw zipped list
            # ie for a VFD multi message with 2 dialog messages:
            # self.VFD_MULTIMESS={1:[].2:[] }
            if not z is None:
                d = dict()
                for num, i in enumerate(NUMBER):
                    d[int(i)] = z[num]
                self['{}_MULTIMESS'.format(item)] = d
            else:
                pass
            #print('{}_MULTIMESS'.format(item),d)

        ##############
        # Embed tabs #
        ##############

        # AXIS panel style:
        self.GLADEVCP = (self.INI.find("DISPLAY", "GLADEVCP")) or None

        # tab style for qtvcp tab. style is used everywhere
        good_flag = True
        self.TAB_NAMES = (self.INI.findall("DISPLAY", "EMBED_TAB_NAME")) or None
        self.TAB_LOCATIONS = (self.INI.findall("DISPLAY", "EMBED_TAB_LOCATION")) or []
        self.TAB_CMDS = (self.INI.findall("DISPLAY", "EMBED_TAB_COMMAND")) or []
        if self.TAB_NAMES is not None and len(self.TAB_NAMES) != len(self.TAB_CMDS):
            log.critical('Embedded tab configuration -invalid number of TAB_NAMES vs TAB_CMDs')
            good_flag = False
        if self.TAB_NAMES is not None and len(self.TAB_LOCATIONS) != len(self.TAB_NAMES):
            log.warning('Embedded tab configuration -invalid number of TAB_NAMES vs TAB_LOCATION - guessing default.')
            for num, i in enumerate(self.TAB_NAMES):
                try:
                    if self.TAB_LOCATIONS[num]:
                        continue
                except:
                    self.TAB_LOCATIONS.append("default")

        # initial/default
        self.NATIVE_EMBED = []
        self.ZIPPED_TABS = None

        # if no critical errors
        if good_flag:
            # check for duplicate names if qtvcp panels
            if self.TAB_CMDS is not None:
                nameList=[]
                for num,i in enumerate(self.TAB_CMDS):
                    if 'qtvcp' in i:
                        nameList.append( self.TAB_NAMES[num])
                # code to check for duplicate names
                dup = {x for x in nameList if nameList.count(x) > 1}
                if not dup == set():
                    log.error('Embedded Qtvcp panel tab: Duplicate TAB_NAMES:{} in INI.'.format(dup))

            try:
                self.ZIPPED_TABS = list(zip(self.TAB_NAMES, self.TAB_LOCATIONS, self.TAB_CMDS))
            except:
                self.ZIPPED_TABS = None

            # find qtvcp embedded - because they are added directly rather then x11 embedding
            if self.TAB_CMDS is not None:
                for i in self.TAB_CMDS:
                    if i.split()[0].lower() == 'qtvcp':
                        self.NATIVE_EMBED.append(True)
                    else:
                        self.NATIVE_EMBED.append(False)

        ################
        # MDI commands #
        ################
        # users can specify a label for the MDI action button by adding ',Some\nText'
        # to the end of the MDI command
        # here we separate them to two lists (legacy) and one dict
        # action_button takes it from there.
        self.MDI_COMMAND_DICT={}
        # suppress error message is there is no section at all
        if self.parser.has_section('MDI_COMMAND_LIST'):
            try:
                for key in self.parser['MDI_COMMAND_LIST']:

                    # legacy way: list of repeat 'MDI_COMMAND=XXXX'
                    # in this case order matters in the INI
                    if key == 'MDI_COMMAND':
                        log.warning("INI file's MDI_COMMAND_LIST is using legacy 'MDI_COMMAND =' entries")
                        self.MDI_COMMAND_LIST = []
                        self.MDI_COMMAND_LABEL_LIST = []
                        temp = (self.INI.findall("MDI_COMMAND_LIST", "MDI_COMMAND")) or None
                        if temp is None:
                            self.MDI_COMMAND_LABEL_LIST.append(None)
                            self.MDI_COMMAND_LABEL_LIST.append(None)
                        else:
                            for i in temp:
                                for num,k in enumerate(i.split(',')):
                                    if num == 0:
                                        self.MDI_COMMAND_LIST.append(k)
                                        if len(i.split(',')) <2:
                                            self.MDI_COMMAND_LABEL_LIST.append(None)
                                    else:
                                        self.MDI_COMMAND_LABEL_LIST.append(k)

                    # new way: 'MDI_COMMAND_SSS = XXXX' (SSS being any string)
                    # order of commands doesn't matter in the INI
                    else:
                        try:
                            temp = self.INI.find("MDI_COMMAND_LIST",key)
                            name = (key.replace('MDI_COMMAND_',''))
                            mdidatadict = {}
                            for num,k in enumerate(temp.split(',')):
                                if num == 0:
                                    mdidatadict['cmd'] = k
                                    if len(temp.split(',')) <2:
                                        mdidatadict['label'] = None
                                else:
                                    mdidatadict['label'] = k
                            self.MDI_COMMAND_DICT[name] = mdidatadict
                        except Exception as e:
                            log.error('INI MDI command parse error:{}'.format(e))
            except Exception as e:
                log.error('INI MDI command parse error:{}'.format(e))

        self.TOOL_FILE_PATH = self.get_error_safe_setting("EMCIO", "TOOL_TABLE")
        self.POSTGUI_HALFILE_PATH = (self.INI.findall("HAL", "POSTGUI_HALFILE")) or None
        self.POSTGUI_HAL_COMMANDS = (self.INI.findall("HAL", "POSTGUI_HALCMD")) or None

        # Some systems need repeat disabled for keyboard jogging because repeat rate is uneven
        self.DISABLE_REPEAT_KEYS_LIST = self.INI.find("DISPLAY", "DISABLE_REPEAT_KEYS") or None

        # maximum number of errors shown in on screen display
        self.MAX_DISPLAYED_ERRORS = int(self.INI.find("DISPLAY", "MAX_DISPLAYED_ERRORS") or 10)
        self.TITLE = (self.INI.find("DISPLAY", "TITLE")) or ""
        self.ICON = (self.INI.find("DISPLAY", "ICON")) or ""

        # detect historical lathe config with dummy joint 1
        if      (self.MACHINE_IS_LATHE
            and (self.trajcoordinates.upper() == "XZ")
            and (len(self.AVAILABLE_JOINTS) == 3)):
            self.LATHE_HISTORICAL_CONFIG = True
        else:
            self.LATHE_HISTORICAL_CONFIG = False

    ###################
    # helper functions
    ###################
    # return a found string or else None by default, anything else by option
    # since this is used in this file there are some workarounds for plasma machines
    def get_error_safe_setting(self, heading, detail, default=None):
        result = self.INI.find(heading, detail)
        if result:
            return result
        else:
            if ('SPINDLE' in detail and self.MACHINE_IS_QTPLASMAC) or \
               ('ANGULAR' in detail and not self.HAS_ANGULAR_JOINT):
                return default
            else:
                log.warning('INI Parsing Error, No {} Entry in {}, Using: {}'.format(detail, heading, default))
            return default

    # return a found float or else None by default, anything else by option
    def get_safe_float(self, heading, detail, default=None):
        try:
            result = float(self.INI.find(heading, detail))
            return result
        except:
            return default

    # return a found integer or else None by default, anything else by option
    def get_safe_int(self, heading, detail, default=None):
        try:
            result = int(self.INI.find(heading, detail))
            return result
        except:
            return default

    def convert_machine_to_metric(self, data):
        if self.MACHINE_IS_METRIC:
            return data
        else:
            return data * 25.4

    def convert_machine_to_imperial(self, data):
        if self.MACHINE_IS_METRIC:
            return data * (1 / 25.4)
        else:
            return data

    def convert_metric_to_machine(self, data):
        if self.MACHINE_IS_METRIC:
            return data
        else:
            return data * (1 / 25.4)

    def convert_imperial_to_machine(self, data):
        if self.MACHINE_IS_METRIC:
            return data * 25.4
        else:
            return data

    def convert_9_metric_to_machine(self, v):
        if self.MACHINE_IS_METRIC:
            return v
        else:
            c = [1.0 / 25.4] * 3 + [1] * 3 + [1.0 / 25.4] * 3
            return list(map(lambda x, y: x * y, v, c))

    def convert_9_imperial_to_machine(self, v):
        if self.MACHINE_IS_METRIC:
            c = [25.4] * 3 + [1] * 3 + [25.4] * 3
            return list(map(lambda x, y: x * y, v, c))
        else:
            return v

    def convert_units(self, data):
        return data * self.MACHINE_UNIT_CONVERSION

    def convert_units_9(self, v):
        c = self.MACHINE_UNIT_CONVERSION_9
        return list(map(lambda x, y: x * y, v, c))

    # This finds the filter program's initializing
    # program eg python for .py from INI
    def get_filter_program(self, fname):
        ext = os.path.splitext(fname)[1]
        if ext:
            return self.INI.find("FILTER", ext[1:])
        else:
            return None

    def get_all_valid_extensions(self):
        temp = []
        try:
            for i in (self.PROGRAM_FILTERS):
                for q in i[0].split(','):
                    temp.append('{}'.format(q))
            if not '.ngc' in temp:
                temp.append('.ngc')
            return temp
        except Exception as e:
            log.warning('Valid Extension Parsing Error: {}\n Using Default: *'.format(e))
            return ('*')

    def get_filters_extensions(self):
        all_extensions = []
        try:
            for k, v in self.PROGRAM_FILTERS:
                k = k.replace('.', ' *.')
                k = k.replace(' ', '')
                temp = []
                for q in k.split(','):
                    temp.append('{}'.format(q))
                all_extensions.append(['{}'.format(v), temp])
            all_extensions.append(['All (*)', ['*']])
            return all_extensions
        except Exception as e:
            log.warning('filter Extension Parsing Error: {}\n Using Default: ALL (*)'.format(e))
            return [['All (*)', ['*']]]

    # get filter extensions in QT format
    def get_qt_filter_extensions(self):
        all_extensions = []
        try:
            for k, v in self.PROGRAM_FILTERS:
                k = k.replace('.', ' *.')
                k = k.replace(',', ' ')
                all_extensions.append((';;%s (%s)' % (v, k)))
            all_extensions.append((';;All (*)'))
            temp = ''
            for i in all_extensions:
                temp = '%s %s' % (temp, i)
            return temp
        except Exception as e:
            log.warning('Qt filter Extension Parsing Error: {}\n Using Default: ALL (*)'.format(e))
            return ('All (*)')

    def program_extension_valid(self, fname):
        filename, file_extension = os.path.splitext(fname)
        if '*' in self.VALID_PROGRAM_EXTENSIONS:
            return True
        elif file_extension.lower() in (self.VALID_PROGRAM_EXTENSIONS):
            return True
        return False

    def get_jnum_from_axisnum(self, axisnum):
        joint = self.TRAJCO.index( "xyzabcuvw"[axisnum] )
        return joint

    # check to see if file name plus paths from
    # SUBROUTINE_PATH, USER_M_PATH or PROGRAM_PREFIX from INI
    # is an existing path (meaning linuxcnc can find it)
    # fname should just be the filename
    # returns the full path or None
    def check_known_paths(self,fname, prefix = True, sub=True, user_m=True):
        fname = os.path.split(fname)[1]
        if prefix:
            path = os.path.join(self.PROGRAM_PREFIX,fname)
            if os.path.exists(path): return path
        if sub:
            for i in self.SUB_PATH_LIST:
                path = os.path.expanduser(os.path.join(i,fname))
                if os.path.exists(path):
                    return path
        if user_m:
            for i in self.USER_M_PATH_LIST:
                path = os.path.expanduser(os.path.join(i,fname))
                if os.path.exists(path):
                    return path
        return None

    # same as above but just return True or False
    def is_in_known_paths(self,fname, prefix = True, sub=True, user_m=True):
        fname = os.path.split(fname)[1]
        if self.check_known_paths(fname,prefix,sub,user_m) is None:
            return False
        return True

    def get_ini_mdi_command(self, key):
        """ returns A MDI command string from the INI heading [MDI_COMMAND_LIST] or None

        key -- can be a integer or a string
        using an integer is the legacy way to refer to the nth line.
        using a string will refer to the specific command regardless what line 
        it is on."""
        try:
            # should fail if not string
            return self.MDI_COMMAND_DICT[key]['cmd']
        except:
            # fallback to legacy variable
            try:
                # should fail if not int
                return self.MDI_COMMAND_LIST[key]
            except:
                return None

    def get_ini_mdi_label(self, key):
        """ returns A MDI label string from the INI heading [MDI_COMMAND_LIST] or None

        key -- can be a integer or a string
        Using an integer is the legacy way to refer to the nth line in the INI.
        Using a string will refer to the specific command regardless of what line 
        it is on."""
        try:
            # should fail if not string
            return self.MDI_COMMAND_DICT[key]['label']
        except:
            # fallback to legacy variable
            try:
                # should fail if not int
                return self.MDI_COMMAND_LABEL_LIST[key]
            except:
                return None

    # Process any multi message options like log level
    def parse_message_options(self, options):
        temp = {}
        if options is None:
            return
        l = options.split(',')
        for i in l:
            o = i.split('=')
            if o[0].upper() == "LEVEL":
                if o[1].upper() == 'WARNING':
                    arg = 1
                elif o[1].upper() == 'CRITICAL':
                    arg = 2
                else:
                    arg = 0
                temp['LEVEL']=arg
            elif o[0].upper() == "LOG":
                temp['LOG']= bool(o[1])
            else:
                 temp[o[0]]= o[1]
        return temp

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)
