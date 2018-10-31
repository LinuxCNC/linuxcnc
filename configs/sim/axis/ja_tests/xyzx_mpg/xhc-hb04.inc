[XHC_HB04_CONFIG]
# specify layout = n for LIB:xhc-hb04-layout{n}.cfg files
layout = 2
# coords: specify upto 4 letters from set {x y z a b c u v w}
# coords: switch labels are xyza but any unique 4 letters
#         from the set {xyxabcuvw} can be used (if the coord exists)
coords = x y z
# lowpass settings:
# coef: slows rate of change of output,  range: 0 < coef < 1
coefs = 1 1 1 1
# scale plus or minus, rotaries may require larger scale factor:
scales = 1 1 1 1
# jogmode normal(default) or vnormal
jogmode = normal
# sequence 1: 0.001,0.010,0.100,1.000 (mm-based machine)
# sequence 2: 0.001,0.005,0.010,0.020 (inch-based machine)
# sequence 3: 0.001,0.010,0.100
# sequence 4: 0.001,0.005,0.010,0.020,0.050,0.100
# sequence 5: 0.001,0.010,0.050,0.100,1.000
sequence = 2
# require_pendant = no: create xhc-hb04 hal pins when pendant not connecte
#                       at startup
require_pendant = no
# inch_or_mm = in | mm
inch_or_mm = in
# mpg_accels: provision for reduced acceleration for manual mode jogging
mpg_accels = 20 20 20 20

[XHC_HB04_BUTTONS]
# use button names according to layout file LIB:xhc-hb04-layout{n}.cfg
# note: "start-pause" is connected for standard behavior
#        controlling halui.pause/halui.resume/halui.run
# these are examples, edit as required:

goto-zero   = halui.mdi-command-00
# synthesized per-axis buttons for goto-zero button:
goto-zero-x = ""
goto-zero-y = ""
goto-zero-z = ""
goto-zero-a = ""

start-pause = std_start_pause
rewind      = halui.program.step
# probe-input for simulating a probe:
probe-z     = motion.probe-input
macro-3     = halui.mdi-command-03

half        = halui.spindle.0.stop
# synthesized per-axis buttons for half button:
half-x      = ""
half-y      = ""
half-z      = ""
half-a      = ""

zero        = ""
# synthesized per-axis buttons for zero button:
zero-x      = halui.mdi-command-12
zero-y      = halui.mdi-command-13
zero-z      = halui.mdi-command-14
zero-a      = halui.mdi-command-15

safe-z      = halui.mdi-command-10
# in this sim, home-all only works first time, hold down till finished:
home        = halui.home-all
macro-1     = halui.mdi-command-01
macro-2     = halui.mdi-command-02
spindle     = halui.spindle.0.start
step        = xhc-hb04.stepsize-up
mode        = ""
macro-6     = halui.mdi-command-06
macro-7     = halui.mdi-command-07
stop        = halui.program.stop
reset       = halui.estop.activate

[HALUI]
# these are examples, edit as required:
# a halui hal pin is created for each MDI_COMMAND below
# halui.mdi-command-00, halui.mdi-command-01,... etc
MDI_COMMAND=G0 X0 Y0 Z0
MDI_COMMAND=(debug, example: mdi-01)
MDI_COMMAND=(debug, example: mdi-02)
#03 M110: clears notifications
MDI_COMMAND=M110
MDI_COMMAND=(debug, example: mdi-04)
MDI_COMMAND=(debug, example: mdi-05)
MDI_COMMAND=(debug, example: mdi-06)
#07 M101: example print to stdout
MDI_COMMAND=M101
MDI_COMMAND=(debug, example: mdi-08)
MDI_COMMAND=(debug, example: mdi-09)
MDI_COMMAND=(debug, example: mdi-10)
MDI_COMMAND=(debug, example: mdi-11)
#12
MDI_COMMAND= G10 L20 P0 X0
#13
MDI_COMMAND= G10 L20 P0 Y0
#14
MDI_COMMAND= G10 L20 P0 Z0
#15
MDI_COMMAND= G10 L20 P0 C0
