
[FILTER]
PROGRAM_EXTENSION       = .ngc,.nc,.tap GCode File (*.ngc, *.nc, *.tap)
ngc                     = qtplasmac_gcode
nc                      = qtplasmac_gcode
tap                     = qtplasmac_gcode

[RS274NGC]
RS274NGC_STARTUP_CODE   = G20 G40 G49 G80 G90 G92.1 G94 G97 M52P1
PARAMETER_FILE          = imperial_parameters.txt
SUBROUTINE_PATH         = ./:../../nc_files
USER_M_PATH             = ./:../../nc_files/plasmac/m_files

[HAL]
TWOPASS                 = ON
HALFILE                 = sim_no_stepgen.tcl
#HALFILE                 = sim_stepgen.tcl
HALFILE                 = qtplasmac_comp.hal
HALFILE                 = custom.hal
POSTGUI_HALFILE         = custom_postgui.hal
POSTGUI_HALFILE         = sim_postgui.hal
HALUI                   = halui

[EMC]
VERSION                 = 1.1
MACHINE                 = qtplasmac-imperial
DEBUG                   = 0

[EMCIO]
EMCIO                   = io
CYCLE_TIME              = 0.100
TOOL_TABLE              = imperial_tool.tbl

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
LINEAR_UNITS            = inch
DEFAULT_LINEAR_VELOCITY = 4.0
MAX_LINEAR_VELOCITY     = 13.8
ANGULAR_UNITS           = degree
POSITION_FILE           = imperial_position.txt

[AXIS_X]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 48.0001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 20.0
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 200.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_0]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 48.0001
MAX_VELOCITY            = 10
MAX_ACCELERATION        = 100
TYPE                    = LINEAR
MIN_FERROR              = 0.2
FERROR                  = 0.4
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = 1
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 200
STEP_SCALE              = 25.40
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[AXIS_Y]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 48.0001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 20.0
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 200.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_1]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 48.0001
MAX_VELOCITY            = 10
MAX_ACCELERATION        = 100
TYPE                    = LINEAR
MIN_FERROR              = 0.2
FERROR                  = 0.4
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = -2
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 200
STEP_SCALE              = 25.40
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[JOINT_2]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 48.0001
MAX_VELOCITY            = 10
MAX_ACCELERATION        = 100
TYPE                    = LINEAR
MIN_FERROR              = 0.2
FERROR                  = 0.4
HOME                    = 0.0
HOME_OFFSET             = 0.0
HOME_SEQUENCE           = -2
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 200
STEP_SCALE              = 25.40
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000

[AXIS_Z]
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 4.0001
# set to double the value in the corresponding joint
MAX_VELOCITY            = 4.8
# set to double the value in the corresponding joint
MAX_ACCELERATION        = 48.0
# shares the above two equally between the joint and the offset
OFFSET_AV_RATIO         = 0.5

[JOINT_3]
# this should be the top of your slats
MIN_LIMIT               = -0.0001
MAX_LIMIT               = 4.0001
MAX_VELOCITY            = 2.4
MAX_ACCELERATION        = 24.0
TYPE                    = LINEAR
MIN_FERROR              = 0.2
FERROR                  = 0.4
HOME                    = 3.8
HOME_OFFSET             = 4.0
HOME_SEQUENCE           = 0
HOME_USE_INDEX          = NO
STEPGEN_MAXACCEL        = 48.0
STEP_SCALE              = 25.40
STEPLEN                 = 1
STEPSPACE               = 0
DIRSETUP                = 8000
DIRHOLD                 = 5000
