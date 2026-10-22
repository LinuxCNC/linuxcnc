[XHC_HB04_BUTTONS]
# use button names according to layout file LIB:xhc-hb04-layout{n}.cfg
# note: "start_pause" is connected for standard behavior
#        controlling halui.pause/halui.resume/halui.run
# these are examples, edit as required:
stop        = halui.program.stop
start_pause = std_start_pause
rewind      = halui.program.step
safe_z      = halui.mdi-command-01
# in this sim, home-all only works first time, hold down till finished:
home        = halui.home-all
x2          = halui.mdi-command-02
y2          = halui.mdi-command-03
# probe-input for simulating a probe:
probe_z     = motion.probe-input
x0          = halui.mdi-command-04
y0          = halui.mdi-command-05
z0          = halui.mdi-command-06

goto_zero   = halui.mdi-command-00
# synthesized per-axis buttons for goto-zero button:
goto_zero_x = ""
goto_zero_y = ""
goto_zero_z = ""
goto_zero_a = ""

step        = xhc-hb04.stepsize-up
mode        = ""
spindle     = ""
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
MDI_COMMAND=(debug, example: mdi-12)
MDI_COMMAND=(debug, example: mdi-13)
MDI_COMMAND=(debug, example: mdi-14)
MDI_COMMAND=(debug, example: mdi-15)
