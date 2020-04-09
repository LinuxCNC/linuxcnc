[HAL]
HALFILE = LIB:basic_sim.tcl
  HALUI = halui

[TRAJ]
         COORDINATES = XZ
        LINEAR_UNITS = inch
       ANGULAR_UNITS = degree
DEFAULT_LINEAR_VELOCITY = 1.0
DEFAULT_LINEAR_ACCELERATION = 20.0
MAX_LINEAR_VELOCITY = 4.44444
MAX_LINEAR_ACCELERATION = 22.2222

[DISPLAY]
             DISPLAY = tklinuxcnc
               LATHE = 1
              #EDITOR = gedit
         TOOL_EDITOR = tooledit diam z
      PROGRAM_PREFIX = ../../nc_files/
          CYCLE_TIME = 0.100
           HELP_FILE = doc/help.txt
     POSITION_OFFSET = RELATIVE
   POSITION_FEEDBACK = ACTUAL
   MAX_FEED_OVERRIDE = 1.2
MAX_SPINDLE_OVERRIDE = 1.0
       INTRO_GRAPHIC = linuxcnc.gif
          INTRO_TIME = 5

[FILTER]
PROGRAM_EXTENSION = .png,.gif,.jpg Grayscale Depth Image
PROGRAM_EXTENSION = .py Python Script
              png = image-to-gcode
              gif = image-to-gcode
              jpg = image-to-gcode
               py = python

[RS274NGC]
PARAMETER_FILE = xz.var

[EMCMOT]
      EMCMOT = motmod
COMM_TIMEOUT = 1.0
 BASE_PERIOD = 50000
SERVO_PERIOD = 1000000

[TASK]
      TASK = milltask
CYCLE_TIME = 0.001


[EMCIO]
                      EMCIO = io
                 CYCLE_TIME = 0.100
                 TOOL_TABLE = xz.tbl
       TOOL_CHANGE_POSITION = 2 0 1
TOOL_CHANGE_WITH_SPINDLE_ON = 1

[JOINT_0]
              TYPE = LINEAR
              HOME = 0.000
      MAX_VELOCITY = 3.3
  MAX_ACCELERATION = 20.3
         MIN_LIMIT = -10.0
         MAX_LIMIT = 10.0
       HOME_OFFSET = 1.0
   HOME_SEARCH_VEL = 5.0
    HOME_LATCH_VEL = 1.0
     HOME_SEQUENCE = 0
    HOME_IS_SHARED = 0

[JOINT_1]
              TYPE = LINEAR
              HOME = 0.0
      MAX_VELOCITY = 3.4
  MAX_ACCELERATION = 20.4
         MIN_LIMIT = -2.0
         MAX_LIMIT = 4.0
       HOME_OFFSET = 1.0
   HOME_SEARCH_VEL = 5.0
    HOME_LATCH_VEL = 1.0
     HOME_SEQUENCE = 1
    HOME_IS_SHARED = 1

[AXIS_X]
       MIN_LIMIT = -10.0
       MAX_LIMIT = 10.0
    MAX_VELOCITY = 3.1
MAX_ACCELERATION = 20.1

[AXIS_Z]
       MIN_LIMIT = -2.0
       MAX_LIMIT = 4.0
    MAX_VELOCITY = 3.2
MAX_ACCELERATION = 20.2
