# twopass_base.inc

[DISPLAY]
                DISPLAY = axis
             CYCLE_TIME = 0.100
              HELP_FILE = doc/help.txt
        POSITION_OFFSET = RELATIVE
      POSITION_FEEDBACK = ACTUAL
      MAX_FEED_OVERRIDE = 1.2
   MAX_SPINDLE_OVERRIDE = 1.0
    MAX_LINEAR_VELOCITY = 1.2
DEFAULT_LINEAR_VELOCITY = .25
         PROGRAM_PREFIX = ../../nc_files/
          INTRO_GRAPHIC = linuxcnc.gif
             INTRO_TIME = 5
                 #EDITOR = gedit
            TOOL_EDITOR = tooledit
             INCREMENTS = 1 in, 0.1 in, 10 mil, 1 mil, 1mm, .1mm, 1/8000 in

[FILTER]
PROGRAM_EXTENSION = .png,.gif,.jpg Grayscale Depth Image
PROGRAM_EXTENSION = .py Python Script
              png = image-to-gcode
              gif = image-to-gcode
              jpg = image-to-gcode
               py = python

[TASK]
TASK       = milltask
CYCLE_TIME = 0.001

[RS274NGC]
PARAMETER_FILE = sim.var

[EMCMOT]
EMCMOT       = motmod
COMM_TIMEOUT = 1.0
BASE_PERIOD  = 0
SERVO_PERIOD = 1000000

[TRAJ]
        COORDINATES = X Y Z
       LINEAR_UNITS = inch
      ANGULAR_UNITS = degree
DEFAULT_LINEAR_VELOCITY = 1.2
      POSITION_FILE = position.txt
MAX_LINEAR_VELOCITY = 1.2

[KINS]
    JOINTS = 3
KINEMATICS = trivkins

[AXIS_X]
       MIN_LIMIT = -40.0
       MAX_LIMIT = 40.0
    MAX_VELOCITY = 4
MAX_ACCELERATION = 100.0

[JOINT_0]
              TYPE = LINEAR
              HOME = 0.000
      MAX_VELOCITY = 4
  MAX_ACCELERATION = 100.0
         MIN_LIMIT = -40.0
         MAX_LIMIT = 40.0
       HOME_OFFSET = 0.0
   HOME_SEARCH_VEL = 20.0
    HOME_LATCH_VEL = 20.0
     HOME_SEQUENCE = 1
    HOME_IS_SHARED = 1

[AXIS_Y]
       MIN_LIMIT = -40.0
       MAX_LIMIT = 40.0
    MAX_VELOCITY = 4
MAX_ACCELERATION = 100.0

[JOINT_1]
              TYPE = LINEAR
              HOME = 0.000
      MAX_VELOCITY = 4
  MAX_ACCELERATION = 100.0
         MIN_LIMIT = -40.0
         MAX_LIMIT = 40.0
       HOME_OFFSET = 0.0
   HOME_SEARCH_VEL = 20.0
    HOME_LATCH_VEL = 20.0
     HOME_SEQUENCE = 1

[AXIS_Z]
       MIN_LIMIT = -8.0
       MAX_LIMIT = 1.0
    MAX_VELOCITY = 4
MAX_ACCELERATION = 100.0

[JOINT_2]
              TYPE = LINEAR
              HOME = 0.0
      MAX_VELOCITY = 4
  MAX_ACCELERATION = 100.0
         MIN_LIMIT = -8.0
         MAX_LIMIT = 1.0
       HOME_OFFSET = 1.0
   HOME_SEARCH_VEL = 20.0
    HOME_LATCH_VEL = 20.0
     HOME_SEQUENCE = 0
    HOME_IS_SHARED = 1

[EMCIO]
               EMCIO = io
          CYCLE_TIME = 0.100
          TOOL_TABLE = sim.tbl
TOOL_CHANGE_POSITION = 0 0 0
TOOL_CHANGE_QUILL_UP = 1
