[EMC]
# The version string for this INI file.
VERSION = 1.1

DEBUG = 0x0


[RS274NGC]
PARAMETER_FILE = sim.var
[EMCMOT]

EMCMOT = motmod
COMM_TIMEOUT = 4.0
BASE_PERIOD = 0
SERVO_PERIOD = 1000000

[TASK]
TASK = milltask
CYCLE_TIME = 0.001

[HAL]
HALUI = halui
HALFILE = core_sim.hal

[HALUI]
#No Content

[TRAJ]

NO_FORCE_HOMING=1
COORDINATES = XYZ
HOME =                  0 0 0
LINEAR_UNITS =          inch
ANGULAR_UNITS =         degree
DEFAULT_LINEAR_VELOCITY =      1.2
MAX_LINEAR_ACCELERATION =      123.45
MAX_LINEAR_VELOCITY =          45.67

[EMCIO]
# no TOOL_TABLE: tool data is provided by the db program
TOOL_CHANGE_QUILL_UP = 1
RANDOM_TOOLCHANGER = 0
DB_PROGRAM = ./db_test.py



[KINS]
KINEMATICS = trivkins coordinates=XYZ
#This is a best-guess at the number of joints, it should be checked
JOINTS = 3

[AXIS_X]
MIN_LIMIT = -40.0
MAX_LIMIT = 40.0
MAX_VELOCITY = 4
MAX_ACCELERATION = 1000.0

[JOINT_0]
TYPE =             LINEAR
HOME =             0.000
MAX_VELOCITY =     4
MAX_ACCELERATION = 1000.0
BACKLASH =         0.000
INPUT_SCALE =      4000
OUTPUT_SCALE =     1.000
MIN_LIMIT =        -40.0
MAX_LIMIT =        40.0
FERROR =           0.050
MIN_FERROR =       0.010

[AXIS_Y]
MIN_LIMIT = -40.0
MAX_LIMIT = 40.0
MAX_VELOCITY = 4
MAX_ACCELERATION = 1000.0

[JOINT_1]
TYPE =             LINEAR
HOME =             0.000
MAX_VELOCITY =     4
MAX_ACCELERATION = 1000.0
BACKLASH =         0.000
INPUT_SCALE =      4000
OUTPUT_SCALE =     1.000
MIN_LIMIT =        -40.0
MAX_LIMIT =        40.0
FERROR =           0.050
MIN_FERROR =       0.010

[AXIS_Z]
MIN_LIMIT = -40.0
MAX_LIMIT = 40.0
MAX_VELOCITY = 4
MAX_ACCELERATION = 1000.0

[JOINT_2]
TYPE =             LINEAR
HOME =             0.0
MAX_VELOCITY =     4
MAX_ACCELERATION = 1000.0
BACKLASH =         0.000
INPUT_SCALE =      4000
OUTPUT_SCALE =     1.000
MIN_LIMIT =        -40.0
MAX_LIMIT =        40.0
FERROR =           0.050
MIN_FERROR =       0.010
