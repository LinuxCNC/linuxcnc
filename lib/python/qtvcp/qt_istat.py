import os
import linuxcnc
import collections

# Set up logging
from . import logger

log = logger.getLogger(__name__)
# Force the log level for this module
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

try:
    LINUXCNCVERSION = os.environ['LINUXCNCVERSION']
except:
    LINUXCNCVERSION = 'UNAVAILABLE'

INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')

HOME = os.environ.get('EMC2_HOME', '/usr')
if HOME is not None:
    IMAGEDIR = os.path.join(HOME, "share", "qtvcp", "images")
else:
    IMAGEDIR = None


class _IStat(object):
    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1
        self.LINUXCNC_IS_RUNNING = bool(INIPATH != '/dev/null')
        if not self.LINUXCNC_IS_RUNNING:
            # Reset the log level for this module
            # Linuxcnc isn't running so we expect INI errors
            log.setLevel(logger.CRITICAL)
        self.LINUXCNC_VERSION = LINUXCNCVERSION
        self.INIPATH = INIPATH
        self.INI = linuxcnc.ini(INIPATH)
        self.MDI_HISTORY_PATH = '~/.axis_mdi_history'
        self.QTVCP_LOG_HISTORY_PATH = '~/qtvcp.log'
        self.MACHINE_LOG_HISTORY_PATH = '~/.machine_log_history'
        self.PREFERENCE_PATH = '~/.Preferences'
        self.SUB_PATH = None
        self.SUB_PATH_LIST = []
        self.MACRO_PATH_LIST = []
        self.IMAGE_PATH = IMAGEDIR
        self.LIB_PATH = os.path.join(HOME, "share", "qtvcp")

        self.MACHINE_IS_LATHE = False
        self.MACHINE_IS_METRIC = False
        self.MACHINE_UNIT_CONVERSION = 1
        self.MACHINE_UNIT_CONVERSION_9 = [1] * 9
        self.AVAILABLE_AXES = ['X', 'Y', 'Z']
        self.AVAILABLE_JOINTS = [0, 1, 2]
        self.GET_NAME_FROM_JOINT = {0: 'X', 1: 'Y', 2: 'Z'}
        self.GET_JOG_FROM_NAME = {'X': 0, 'Y': 1, 'Z': 2}
        self.NO_HOME_REQUIRED = False
        self.JOG_INCREMENTS = None
        self.ANGULAR_INCREMENTS = None

        self.MAX_TRAJ_VELOCITY = 60

        self.DEFAULT_LINEAR_VELOCITY = 15.0

        self.AVAILABLE_SPINDLES = 1
        self.DEFAULT_SPINDLE_SPEED = 200
        self.MAX_SPINDLE_SPEED = 2500
        self.MAX_FEED_OVERRIDE = 1.5
        self.MAX_SPINDLE_OVERRIDE = 1.5
        self.MIN_SPINDLE_OVERRIDE = 0.5
        self.TITLE = ""
        self.ICON = ""

        self.update()

    def update(self):
        ct = float(self.INI.find('DISPLAY', 'CYCLE_TIME') or 100) # possibly in seconds or ms
        if ct < 1:
            self.CYCLE_TIME = int(ct * 1000)
        else:
            self.CYCLE_TIME = int(ct)
        self.GRAPHICS_CYCLE_TIME = int(self.INI.find('DISPLAY', 'GRAPHICS_CYCLE_TIME') or 100) # in seconds
        self.HALPIN_CYCLE_TIME = float(self.INI.find('DISPLAY', 'HALPIN_CYCLE_TIME') or 100) # in seconds
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

        self.SUB_PATH = (self.INI.find("RS274NGC", "SUBROUTINE_PATH")) or None
        self.STARTUP_CODES = (self.INI.find('RS274NGC', 'RS274NGC_STARTUP_CODE') ) or None
        if self.SUB_PATH is not None:
            for mpath in (self.SUB_PATH.split(':')):
                self.SUB_PATH_LIST.append(mpath)
                if 'macro' in mpath:
                    path = mpath
                    self.MACRO_PATH_LIST.append(mpath)
            self.MACRO_PATH = mpath or None
        else:
            self.MACRO_PATH = None
        self.INI_MACROS = self.INI.findall("DISPLAY", "MACRO")
        self.MACHINE_IS_LATHE = bool(self.INI.find("DISPLAY", "LATHE"))
        self.MACHINE_IS_QTPLASMAC = (self.INI.find("QTPLASMAC", "MODE")) or None

        extensions = self.INI.findall("FILTER", "PROGRAM_EXTENSION")
        self.PROGRAM_FILTERS = ([e.split(None, 1) for e in extensions]) or None
        self.PROGRAM_FILTERS_EXTENSIONS = self.get_filters_extensions()
        self.VALID_PROGRAM_EXTENSIONS = self.get_all_valid_extensions()

        self.PARAMETER_FILE = (self.INI.find("RS274NGC", "PARAMETER_FILE")) or None
        try:
            # check the ini file if UNITS are set to mm"
            # first check the global settings
            units = self.INI.find("TRAJ", "LINEAR_UNITS")
            if units is None:
                log.critical('Misssing LINEAR_UNITS in TRAJ, guessing units for machine from JOINT 0') 
                # else then guess; The joint 0 is usually X axis
                units = self.INI.find("JOINT_0", "UNITS")
                if units is None:
                    log.critical('Misssing UNITS in JOINT_0, assuming metric based machine') 
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
        if axes is not None:  # i.e. LCNC is running, not just in Qt Designer
            axes = axes.replace(" ", "")
            self.TRAJCO = axes.lower()
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
                    log.critical(
                        'MISSING [AXIS_{}] MAX VeLOCITY or MAX ACCELERATION entry in INI file.'.format(letter.upper()))

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
                seq = -1
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
        self.JOINT_SYNCH_LIST = list(set(tuple(sorted(sub)) for sub in templist))

        # This is a list of joints that are related to a joint.
        #ie. JOINT_RELATIONS_LIST(0) will give a list of joints that go with joint 0
        # to make an axis or else a list with just 0 in it.
        # current use case is to find out what other joints should be unhomed if you unhome 
        # a combined joint axis.
        self.JOINT_RELATIONS_LIST = [None] * jointcount
        for j in range(jointcount):
            temp = []
            for hj, hs in list(self.JOINT_SEQUENCE_LIST.items()):
                if abs(int(hs)) == abs(int(self.JOINT_SEQUENCE_LIST.get(j))):
                    temp.append(hj)
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
        self.DEFAULT_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "DEFAULT_LINEAR_VELOCITY", 1)) * 60
        self.MIN_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MIN_LINEAR_VELOCITY", 1)) * 60
        self.MAX_LINEAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MAX_LINEAR_VELOCITY", 5)) * 60
        self.DEFAULT_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "DEFAULT_ANGULAR_VELOCITY", 6)) * 60
        self.MIN_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MIN_ANGULAR_VELOCITY", 1)) * 60
        self.MAX_ANGULAR_JOG_VEL = float(self.get_error_safe_setting("DISPLAY", "MAX_ANGULAR_VELOCITY", 60)) * 60
        log.debug('DEFAULT_LINEAR_VELOCITY = {}'.format(self.DEFAULT_LINEAR_JOG_VEL))
        log.debug('MIN_LINEAR_VELOCITY = {}'.format(self.MIN_LINEAR_JOG_VEL))
        log.debug('MAX_LINEAR_VELOCITY = {}'.format(self.MAX_LINEAR_JOG_VEL))

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
            log.warning('Invalid message configuration (missing text or type) in INI File [DISPLAY] section ')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_PINNAME):
            log.warning('Invalid message configuration (missing pinname) in INI File [DISPLAY] section')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_BOLDTEXT):
            log.warning('Invalid message configuration (missing boldtext) in INI File [DISPLAY] sectioN')
        if len(self.USRMESS_TEXT) != len(self.USRMESS_DETAILS):
            log.warning('Invalid message configuration (missing details) in INI File [DISPLAY] sectioN')
        try:
            self.ZIPPED_USRMESS = list(
                zip(self.USRMESS_BOLDTEXT, self.USRMESS_TEXT, self.USRMESS_DETAILS, self.USRMESS_TYPE,
                    self.USRMESS_PINNAME))
        except:
            self.ZIPPED_USRMESS = None

        # XEmbed tabs
        # AXIS panel style:
        self.GLADEVCP = (self.INI.find("DISPLAY", "GLADEVCP")) or None

        # tab style for qtvcp tab style is used everty where
        self.TAB_NAMES = (self.INI.findall("DISPLAY", "EMBED_TAB_NAME")) or None
        self.TAB_LOCATIONS = (self.INI.findall("DISPLAY", "EMBED_TAB_LOCATION")) or []
        self.TAB_CMDS = (self.INI.findall("DISPLAY", "EMBED_TAB_COMMAND")) or None
        if self.TAB_NAMES is not None and len(self.TAB_NAMES) != len(self.TAB_CMDS):
            log.critical('Embeded tab configuration -invalaid number of TAB_NAMES vrs TAB_CMDs')
        if self.TAB_NAMES is not None and len(self.TAB_LOCATIONS) != len(self.TAB_NAMES):
            log.warning('Embeded tab configuration -invalaid number of TAB_NAMES vrs TAB_LOCATION - guessng default.')
            for num, i in enumerate(self.TAB_NAMES):
                try:
                    if self.TAB_LOCATIONS[num]:
                        continue
                except:
                    self.TAB_LOCATIONS.append("default")
        try:
            self.ZIPPED_TABS = list(zip(self.TAB_NAMES, self.TAB_LOCATIONS, self.TAB_CMDS))
        except:
            self.ZIPPED_TABS = None

        self.MDI_COMMAND_LIST = (self.INI.findall("MDI_COMMAND_LIST", "MDI_COMMAND")) or None
        self.TOOL_FILE_PATH = self.get_error_safe_setting("EMCIO", "TOOL_TABLE")
        self.POSTGUI_HALFILE_PATH = (self.INI.findall("HAL", "POSTGUI_HALFILE")) or None
        self.POSTGUI_HAL_COMMANDS = (self.INI.findall("HAL", "POSTGUI_HALCMD")) or None

        # Some systems need repeat disabled for keyboard jogging because repeat rate is uneven
        self.DISABLE_REPEAT_KEYS_LIST = self.INI.find("DISPLAY", "DISABLE_REPEAT_KEYS") or None

        # maximum number of errors shown in on screen display
        self.MAX_DISPLAYED_ERRORS = int(self.INI.find("DISPLAY", "MAX_DISPLAYED_ERRORS") or 10)
        self.TITLE = (self.INI.find("DISPLAY", "TITLE")) or ""
        self.ICON = (self.INI.find("DISPLAY", "ICON")) or ""
    ###################
    # helper functions
    ###################

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

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)
