[DISPLAY]
DISPLAY =               axis
CYCLE_TIME =            0.100
POSITION_OFFSET =       RELATIVE
POSITION_FEEDBACK =     ACTUAL
MAX_FEED_OVERRIDE =     1.2
MAX_SPINDLE_OVERRIDE =  1.0
PROGRAM_PREFIX =        ../../nc_files/
INTRO_GRAPHIC =         linuxcnc.gif
INTRO_TIME =            5
EDITOR =                gedit
GEOMETRY =              XYZABCUVW
INCREMENTS = 1 mm, .01 in, .1mm, 1 mil, .1 mil, 1/8000 in

[FILTER]
PROGRAM_EXTENSION = .png,.gif,.jpg Grayscale Depth Image
PROGRAM_EXTENSION = .py Python Script
png = image-to-gcode
gif = image-to-gcode
jpg = image-to-gcode
py = python

[TASK]
TASK =                  milltask
CYCLE_TIME =            0.001

[RS274NGC]
PARAMETER_FILE = sim-9axis.var
SUBROUTINE_PATH   = ../../nc_files/ngcgui_lib:../../nc_files/ngcgui_lib/utilitysubs
# for demo of M110:
USER_M_PATH       = ../../nc_files/ngcgui_lib/mfiles

[EMCMOT]
EMCMOT =                motmod
COMM_TIMEOUT =          1.0
COMM_WAIT =             0.010
SERVO_PERIOD =          1000000

[AXIS_X]
MIN_LIMIT = -10.0
MAX_LIMIT = 10.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_0]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -10.0
MAX_LIMIT =             10.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       5.0
HOME_LATCH_VEL =        1.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1
HOME_IS_SHARED =        1

[AXIS_Y]
MIN_LIMIT = -10.0
MAX_LIMIT = 10.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_1]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -10.0
MAX_LIMIT =             10.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       5.0
HOME_LATCH_VEL =        1.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_Z]
MIN_LIMIT = -2.0
MAX_LIMIT = 4.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_2]
TYPE =                  LINEAR
HOME =                  0.0
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -2.0
MAX_LIMIT =             4.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           1.0
HOME_SEARCH_VEL =       5.0
HOME_LATCH_VEL =        1.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        1

[AXIS_A]
MAX_VELOCITY = 90.0
MAX_ACCELERATION = 1200.0

[JOINT_3]
TYPE =                  ANGULAR
HOME =                  0.0
MAX_VELOCITY =          90.0
MAX_ACCELERATION =      1200.0
BACKLASH =              0.000
INPUT_SCALE =           40
OUTPUT_SCALE =          1.000
FERROR =                5.0
MIN_FERROR =            1.0
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_B]
MAX_VELOCITY = 90.0
MAX_ACCELERATION = 1200.0

[JOINT_4]
TYPE =                  ANGULAR
HOME =                  0.0
MAX_VELOCITY =          90.0
MAX_ACCELERATION =      1200.0
BACKLASH =              0.000
INPUT_SCALE =           40
OUTPUT_SCALE =          1.000
FERROR =                5.0
MIN_FERROR =            1.0
HOME_OFFSET =           45.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_C]
MAX_VELOCITY = 90.0
MAX_ACCELERATION = 1200.0

[JOINT_5]
TYPE =                  ANGULAR
HOME =                  0.0
MAX_VELOCITY =          90.0
MAX_ACCELERATION =      1200.0
BACKLASH =              0.000
INPUT_SCALE =           40
OUTPUT_SCALE =          1.000
FERROR =                5.0
MIN_FERROR =            1.0
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_U]
MIN_LIMIT = -10.0
MAX_LIMIT = 10.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_6]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -10.0
MAX_LIMIT =             10.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        0

[AXIS_V]
MIN_LIMIT = -10.0
MAX_LIMIT = 10.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_7]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -10.0
MAX_LIMIT =             10.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        0

[AXIS_W]
MIN_LIMIT = -2.0
MAX_LIMIT = 4.0
MAX_VELOCITY = 1.2
MAX_ACCELERATION = 20.0

[JOINT_8]
TYPE =                  LINEAR
HOME =                  0.0
MAX_VELOCITY =          1.2
MAX_ACCELERATION =      20.0
BACKLASH =              0.000
INPUT_SCALE =           4000
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -2.0
MAX_LIMIT =             4.0
FERROR =                0.050
MIN_FERROR =            0.010
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       0.0
HOME_LATCH_VEL =        0.0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        0

[EMCIO]
EMCIO =                io
CYCLE_TIME =           0.100
TOOL_TABLE =           sim.tbl
TOOL_CHANGE_POSITION = 0 0 2
