[XHC_HB04_BUTTONS]
# use button names according to layout file LIB:xhc-hb04-layout{n}.cfg
# note: "start_pause" is connected for standard behavior
#        controlling halui.pause/halui.resume/halui.run
# these are examples, edit as required:

goto_zero   = halui.mdi-command-00
# synthesized per-axis buttons for goto-zero button:
goto_zero_x = ""
goto_zero_y = ""
goto_zero_z = ""
goto_zero_a = ""

start_pause = std_start_pause
rewind      = halui.program.step
# probe-input for simulating a probe:
probe_z     = motion.probe-input
macro_3     = halui.mdi-command-03

half        = halui.spindle.0.stop
# synthesized per-axis buttons for half button:
half_x      = ""
half_y      = ""
half_z      = ""
half_a      = ""

zero        = ""
# synthesized per-axis buttons for zero button:
zero_x      = halui.mdi-command-12
zero_y      = halui.mdi-command-13
zero_z      = halui.mdi-command-14
zero_a      = halui.mdi-command-15

safe_z      = halui.mdi-command-10
# in this sim, home-all only works first time, hold down till finished:
home        = halui.home-all
macro_1     = halui.mdi-command-01
macro_2     = halui.mdi-command-02
spindle     = halui.spindle.0.start
step        = xhc-hb04.stepsize-up
mode        = ""
macro_6     = halui.mdi-command-06
macro_7     = halui.mdi-command-07
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
