[PRUCONF]
DRIVER=hal_pru_generic
PRUBIN=xenomai/pru_generic.bin

[MACHINE]

# Machine configuration for delta printer

DELTA_R 		= 137.38

CF_ROD 			= 343
JOINT_1_ANGLE_OFFSET 	= 0.36
JOINT_2_ANGLE_OFFSET 	= -0.09
JOINT_0_OFFSET 		= -0.06
JOINT_1_OFFSET 		= 0
JOINT_2_OFFSET 		= -0.15

[FILTER]
PROGRAM_EXTENSION = .png,.gif,.jpg Grayscale Depth Image
PROGRAM_EXTENSION = .py Python Script
PROGRAM_EXTENSION = .gcode RepRap Flavour GCode

png = image-to-gcode
gif = image-to-gcode
jpg = image-to-gcode
py = python
gcode = gcode-to-ngc

[TASK]

# Name of task controller program, e.g., milltask
TASK =                  milltask

# Cycle time, in seconds, that task controller will sleep between polls
CYCLE_TIME =            0.010


[RS274NGC]

# File containing interpreter variables
PARAMETER_FILE =        pru-stepper.var

# subroutine path for the custom FDM printing .ngc files used for remapping
# use directory in config path until more suitable location. this w.r.t the
# fact that not every machine will have an extruder and/or fan
SUBROUTINE_PATH = /home/machinekit/dev_mk/machinekit/nc_files/remap-subroutines/fdm
#SUBROUTINE_PATH = subroutines

# remapping Machinekit FDM GCodes
REMAP=G22 modalgroup=1 ngc=g22
REMAP=G23 modalgroup=1 ngc=g23
REMAP=G28 modalgroup=1 argspec=xyza ngc=g28
REMAP=G29 modalgroup=1 ngc=g29
REMAP=G29.1 modalgroup=1 argspec=xyz ngc=g29_1
REMAP=G29.2 modalgroup=1 argspec=xyz ngc=g29_2
REMAP=G30 modalgroup=1 argspec=pxy ngc=g30
#REMAP=M83 modalgroup=10 ngc=
REMAP=M104 modalgroup=10 argspec=iPt ngc=m104
REMAP=M106 modalgroup=10 argspec=iPt ngc=m106
REMAP=M107 modalgroup=10 argspec=it ngc=m107
REMAP=M109 modalgroup=10 argspec=tP ngc=m109
REMAP=M140 modalgroup=10 argspec=iP ngc=m140
REMAP=M141 modalgroup=10 argspec=iP ngc=m141
REMAP=M190 modalgroup=10 argspec=P ngc=m190
REMAP=M191 modalgroup=10 argspec=P ngc=m191
REMAP=M200 modalgroup=10 argspec=D ngc=m200
REMAP=M207 modalgroup=10 argspec=PQ ngc=m207
REMAP=M221 modalgroup=10 argspec=P ngc=m221
REMAP=M226 modalgroup=10 ngc=m226
REMAP=M280 modalgroup=10 argspec=itP ngc=m280
REMAP=M300 modalgroup=10 argspec=iqP ngc=m300
REMAP=M400 modalgroup=10 ngc=m400
REMAP=M420 modalgroup=10 argspec=itredp ngc=m420
REMAP=M700 modalgroup=10 argspec=iP ngc=m700
REMAP=M701 modalgroup=10 argspec=iP ngc=m701
REMAP=M702 modalgroup=10 argspec=iP ngc=m702
REMAP=M710 modalgroup=10 argspec=epq ngc=m710

# enable ini parameter passing
FEATURES = 4


[EMCMOT]

EMCMOT =                motmod

# Timeout for comm to emcmot, in seconds
COMM_TIMEOUT =          1.0

# Interval between tries to emcmot, in seconds
COMM_WAIT =             0.010

# Servo task period, in nanoseconds
SERVO_PERIOD =          1000000


[EXTRUDER_0]
PID_PGAIN              = 0.3
PID_IGAIN              = 0.00001
PID_DGAIN              = 0.9375
PID_MAXERRORI          = 1.0
PID_BIAS               = 0.5
PWM_MAX                = 1.0
TEMP_RANGE_POS_ERROR   = 1.0
TEMP_RANGE_NEG_ERROR   = -1.0
TEMP_RANGE_LOWPASSGAIN = 0.0008
TEMP_LIMIT_MIN         = 0.0
TEMP_LIMIT_MAX         = 300.0
TEMP_STANDBY           = 200.0
THERMISTOR             = epcos_B57560G1104

RETRACT_LEN            = 4.2
RETRACT_VEL            = 40.0

FILAMENT_DIA           = 1.75
EXTRUDE_SCALE          = 1.00

FAN_BIAS = 0.0

MAX_VELOCITY = 32.0
MAX_ACCELERATION = 3000.0
# Set Stepgen max 20% higher than the axis
STEPGEN_MAX_VEL = 37.0
STEPGEN_MIN_VEL = 0.001
STEPGEN_MAX_ACC = 3600.0

#SCALE = -486.15 -- Artifex
#SCALE = 495.0 -- Bulldog XL
#SCALE = -107.0 -- Bulldog Lite Original
#SCALE = -90.0
#SCALE = 744.530002058
SCALE = -151.485

# these are in nanoseconds
DIRSETUP   =              200
DIRHOLD    =              200
STEPLEN    =              1000
STEPSPACE  =              1000


[HBP]
PID_PGAIN              = 1.0
PID_IGAIN              = 0.0
PID_DGAIN              = 0.00
PID_MAXERRORI          = 1.0
PID_BIAS               = 0.5
PWM_MAX                = 1.0
TEMP_RANGE_POS_ERROR   = 10.0
TEMP_RANGE_NEG_ERROR   = -2.0
TEMP_LIMIT_MIN         = 0.0
TEMP_LIMIT_MAX         = 150.0
TEMP_STANDBY           = 50.0
THERMISTOR             = epcos_B57560G1104


[PROBE]
COUNT = 0
SEARCH_VEL = 10.0
TOOL_NR = 100
START_Z = 0.0
END_Z = 1.0
HBP_TEMP = 50.0


[P0]
X = 0.0
Y = 0.0


[EMCIO]

# Name of IO controller program, e.g., io
EMCIO =                 io

# cycle time, in seconds
CYCLE_TIME =            0.100

# tool table file
TOOL_TABLE =            tool.tbl

