
[QTPLASMAC]
# set the operating mode (default is 0)
MODE                    = 0

# estop is a indicator only
ESTOP_TYPE = 0
# estop is hidden
#ESTOP_TYPE = 1
# estop is a button
#ESTOP_TYPE = 2

CAMERA_TOUCHOFF         = 25 0
LASER_TOUCHOFF          = -25 0

# user buttons in the main window
BUTTON_1_NAME           = OHMIC\TEST
BUTTON_1_CODE           = ohmic-test
BUTTON_2_NAME           = PROBE\TEST
BUTTON_2_CODE           = probe-test 10
BUTTON_3_NAME           = SINGLE\CUT
BUTTON_3_CODE           = single-cut
BUTTON_4_NAME           = NORMAL\CUT
BUTTON_4_CODE           = cut-type
BUTTON_5_NAME           = TORCH\PULSE
BUTTON_5_CODE           = torch-pulse .5
BUTTON_6_NAME           = 
BUTTON_6_CODE           = 

# powermax communications
#PM_PORT                 = /dev/ttyUSB0
#PM_PORT                 = /dev/ttyS0

# this section required as is for removing z axis moves from the loaded gcode file
[FILTER]
PROGRAM_EXTENSION       = .ngc,.nc,.tap GCode File (*.ngc, *.nc, *.tap)
ngc                     = ./qtplasmac/qtplasmac_gcode.py
nc                      = ./qtplasmac/qtplasmac_gcode.py
tap                     = ./qtplasmac/qtplasmac_gcode.py

[RS274NGC]
RS274NGC_STARTUP_CODE   = o<metric_startup> call
PARAMETER_FILE          = metric_parameters.txt

SUBROUTINE_PATH         = ./:./qtplasmac:../../nc_files/subroutines

# required for our material change
USER_M_PATH             = ./:./qtplasmac

[HAL]
TWOPASS                 = ON
# the base machine
HALFILE                 = machine.tcl
# custom hal commands
HALFILE                 = custom.hal
# custom postgui hal
POSTGUI_HALFILE         = custom_postgui.hal
# sim postgui hal
POSTGUI_HALFILE         = sim_postgui.hal
# required
HALUI                   = halui

[EMC]
VERSION                 = 1.1
MACHINE                 = qtplasmac-metric
DEBUG                   = 0

[EMCIO]
EMCIO                   = io
CYCLE_TIME              = 0.100
TOOL_TABLE              = tool.tbl

[TASK]
TASK                    = milltask
CYCLE_TIME              = 0.01

[EMCMOT]
EMCMOT                  = motmod
BASE_PERIOD             = 32000
SERVO_PERIOD            = 1000000

[KINS]
KINEMATICS              = trivkins coordinates=xyyz
JOINTS                  = 4
 
[TRAJ]
SPINDLES                = 3
COORDINATES             = XYYZ
LINEAR_UNITS            = mm
MAX_LINEAR_VELOCITY     = 350
DEFAULT_LINEAR_VELOCITY = 100
ANGULAR_UNITS           = degree
POSITION_FILE           = metric_position.txt

[AXIS_X]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 612.001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 500.0
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 5000.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_0]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 612.001
MAX_VELOCITY            = 250
MAX_ACCELERATION        = 2500
TYPE                    = LINEAR
MIN_FERROR              = 0.025
FERROR                  = 1
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = 1
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 3000
SCALE                   = 1
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[AXIS_Y]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 612.001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 500.0
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 5000.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_1]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 612.001
MAX_VELOCITY            = 250
MAX_ACCELERATION        = 2500
TYPE                    = LINEAR
MIN_FERROR              = 0.025
FERROR                  = 1
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = -2
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 3000
SCALE                   = 1
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[JOINT_2]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 612.001
MAX_VELOCITY            = 250
MAX_ACCELERATION        = 2500
TYPE                    = LINEAR
MIN_FERROR              = 0.025
FERROR                  = 1
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = -2
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 3000
SCALE                   = 1
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[AXIS_Z]
MIN_LIMIT               = -0.001
MAX_LIMIT               = 70.001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 120.0
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 1200.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_3]
# this should be the top of your slats
MIN_LIMIT               = -0.001
MAX_LIMIT               = 70.001
MAX_VELOCITY            = 60
MAX_ACCELERATION        = 600
TYPE                    = LINEAR
MIN_FERROR              = 0.025
FERROR                  = 1.0
HOME                    = 65.0
HOME_OFFSET             = 70.0
HOME_SEQUENCE           = 0
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 900.0
SCALE                   = 1
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000
