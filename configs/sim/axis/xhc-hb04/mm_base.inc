# adapted from axis_9axis.ini for xhc-hb04 demo

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

[TRAJ]
NO_FORCE_HOMING =          1
AXES =                     9
COORDINATES =              X Y Z A B C U V W
HOME =                     0 0 0 0 0 0 0 0 0
LINEAR_UNITS =             mm
ANGULAR_UNITS =            degree
CYCLE_TIME =               0.010
DEFAULT_VELOCITY =         1.0
DEFAULT_ANGULAR_VELOCITY = 45.0
POSITION_FILE =            position9.txt
MAX_LINEAR_VELOCITY =      1.2
MAX_ANGULAR_VELOCITY =     90.0

[AXIS_0]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -254
MAX_LIMIT =             254
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       127
HOME_LATCH_VEL =        25.4
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1
HOME_IS_SHARED =        1

[AXIS_1]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -254
MAX_LIMIT =             254
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           0.0
HOME_SEARCH_VEL =       127
HOME_LATCH_VEL =        25.4
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_2]
TYPE =                  LINEAR
HOME =                  0.0
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -50.8
MAX_LIMIT =             101.6
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           25.4
HOME_SEARCH_VEL =       127
HOME_LATCH_VEL =        25.4
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        1

[AXIS_3]
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

[AXIS_4]
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

[AXIS_5]
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

[AXIS_6]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -254
MAX_LIMIT =             254
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           0
HOME_SEARCH_VEL =       0
HOME_LATCH_VEL =        0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1
HOME_IS_SHARED =        1

[AXIS_7]
TYPE =                  LINEAR
HOME =                  0.000
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -254
MAX_LIMIT =             254
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           0
HOME_SEARCH_VEL =       0
HOME_LATCH_VEL =        0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         1

[AXIS_8]
TYPE =                  LINEAR
HOME =                  0.0
MAX_VELOCITY =          30.48
MAX_ACCELERATION =      508
BACKLASH =              0.000
INPUT_SCALE =           157.48
OUTPUT_SCALE =          1.000
MIN_LIMIT =             -50.8
MAX_LIMIT =             101.6
FERROR =                1.27
MIN_FERROR =            .254
HOME_OFFSET =           0
HOME_SEARCH_VEL =       0
HOME_LATCH_VEL =        0
HOME_USE_INDEX =        NO
HOME_IGNORE_LIMITS =    NO
HOME_SEQUENCE =         0
HOME_IS_SHARED =        1

[EMCIO]
EMCIO =                io
CYCLE_TIME =           0.100
TOOL_TABLE =           sim_mm.tbl
TOOL_CHANGE_POSITION = 0 0 2
