#//////////////////////////////////////////////////////////////////////////
### Example edit your INI and add :
#
#[HAL]
#HALFILE = <your-machine-hal>.hal
#HALFILE = xhc-whb04b-6.hal
#
## This section is for button mapping, you need to create for your own use.
#[HALUI]
#MDI_COMMAND=(debug,macro0)
#MDI_COMMAND=(debug,macro1)
#MDI_COMMAND=(debug,macro2)
#MDI_COMMAND=(debug,macro3)
#MDI_COMMAND=(debug,macro4)
#MDI_COMMAND=(debug,macro5)
#MDI_COMMAND=(debug,macro6)
#MDI_COMMAND=(debug,macro7)
#MDI_COMMAND=(debug,macro8)
#MDI_COMMAND=(debug,macro9)
#MDI_COMMAND=(debug,macro10)
#MDI_COMMAND=(debug,macro11)
#MDI_COMMAND=(debug,macro12)
#MDI_COMMAND=(debug,macro13)
#MDI_COMMAND=(debug,macro14)
#MDI_COMMAND=(debug,macro15)
#MDI_COMMAND=(debug,macro16)
#
#//////////////////////////////////////////////////////////////////////////
#
### Hal File xhc_whb04b_6.hal Example for 3 axis but i hope this is easy to add missing axis:
#

# ######################################################################
# load pendant components
# ######################################################################

loadusr -W xhc-whb04b-6 -HsfB

# ######################################################################
# pendant signal configuration
# ######################################################################

# On/Off signals
net machine.is-on                         halui.machine.is-on                   whb.halui.machine.is-on
net pdnt.machine.on                       whb.halui.machine.on                  halui.machine.on
net pdnt.machine.off                      whb.halui.machine.off                 halui.machine.off

# program related signals
net pdnt.program.is-idle                  whb.halui.program.is-idle             halui.program.is-idle
net pdnt.program.is-paused                whb.halui.program.is-paused           halui.program.is-paused
net pdnt.program-is-running               whb.halui.program.is-running          halui.program.is-running
net pdnt.program.resume                   whb.halui.program.resume              halui.program.resume
net pdnt.program.pause                    whb.halui.program.pause               halui.program.pause
net pdnt.program.run                      whb.halui.program.run                 halui.program.run
net pdnt.program.stop                     whb.halui.program.stop                halui.program.stop

# machine mode related signals
net pdnt.mode.auto                        whb.halui.mode.auto                   halui.mode.auto
net pdnt.mode.manual                      whb.halui.mode.manual                 halui.mode.manual
net pdnt.mode.mdi                         whb.halui.mode.mdi                    halui.mode.mdi
net pdnt.mode.joint                       whb.halui.mode.joint                  halui.mode.joint
net pdnt.mode.teleop                      whb.halui.mode.teleop                 halui.mode.teleop
net pdnt.mode.is-auto                     halui.mode.is-auto                    whb.halui.mode.is-auto
net pdnt.mode.is-manual                   halui.mode.is-manual                  whb.halui.mode.is-manual
net pdnt.mode.is-mdi                      halui.mode.is-mdi                     whb.halui.mode.is-mdi
net pdnt.mode.is-joint                    halui.mode.is-joint                   whb.halui.mode.is-joint
net pdnt.mode.is-teleop                   halui.mode.is-teleop                  whb.halui.mode.is-teleop

# "is-homed" axis signal for allowing pendant when machine is not homed
net pdnt.axis.X.is-homed                  halui.joint.0.is-homed                whb.halui.joint.x.is-homed
net pdnt.axis.Y.is-homed                  halui.joint.1.is-homed                whb.halui.joint.y.is-homed
net pdnt.axis.Z.is-homed                  halui.joint.2.is-homed                whb.halui.joint.z.is-homed

# "selected axis" signals
net pdnt.axis.X.select                    whb.halui.axis.x.select               halui.axis.x.select
net pdnt.axis.y.select                    whb.halui.axis.y.select               halui.axis.y.select
net pdnt.axis.Z.select                    whb.halui.axis.z.select               halui.axis.z.select

net pdnt.axis.x.jog-scale                 whb.axis.x.jog-scale                  axis.x.jog-scale
net pdnt.axis.y.jog-scale                 whb.axis.y.jog-scale                  axis.y.jog-scale
net pdnt.axis.z.jog-scale                 whb.axis.z.jog-scale                  axis.z.jog-scale

net pdnt.axis.x.jog-counts                whb.axis.x.jog-counts                 axis.x.jog-counts
net pdnt.axis.y.jog-counts                whb.axis.y.jog-counts                 axis.y.jog-counts
net pdnt.axis.z.jog-counts                whb.axis.z.jog-counts                 axis.z.jog-counts

net pdnt.axis.x.jog-enable                whb.axis.x.jog-enable                 axis.x.jog-enable
net pdnt.axis.y.jog-enable                whb.axis.y.jog-enable                 axis.y.jog-enable
net pdnt.axis.z.jog-enable                whb.axis.z.jog-enable                 axis.z.jog-enable

net pdnt.axis.x.jog-vel-mode              whb.axis.x.jog-vel-mode               axis.x.jog-vel-mode
net pdnt.axis.y.jog-vel-mode              whb.axis.y.jog-vel-mode               axis.y.jog-vel-mode
net pdnt.axis.z.jog-vel-mode              whb.axis.z.jog-vel-mode               axis.z.jog-vel-mode


# macro buttons to MDI commands
net pdnt.macro-1                          whb.button.macro-1                    halui.mdi-command-01             # use MDI command from main.ini
net pdnt.macro-2                          whb.button.macro-2                    halui.mdi-command-02             # use MDI command from main.ini or used for Hardcoded lube on/off
net pdnt.reserved.for.spindle+            whb.button.macro-3                                                     # Hardcoded for spindle+ whb.halui.spindle.increase
net pdnt.reserved.for.spindle-            whb.button.macro-4                                                     # Hardcoded for spindle- whb.halui.spindle.decrease
net pdnt.macro-5                          whb.button.macro-5                    halui.mdi-command-05             # use MDI command from main.ini
net pdnt.macro-6                          whb.button.macro-6                    halui.mdi-command-06             # use MDI command from main.ini
net pdnt.macro-7                          whb.button.macro-7                    halui.mdi-command-07             # use MDI command from main.ini
net pdnt.reserved.for.spindle.dir         whb.button.macro-8                                                     # Hardcoded for spindle direction inside pendant
net pdnt.macro-9                          whb.button.macro-9                    halui.mdi-command-09             # use MDI command from main.ini
net pdnt.reserved.for.ABS-REL             whb.button.macro-10                                                    # Hardcoded for swap Dro  Relative/Absolute
net pdnt.macro-14                         whb.button.macro-14                   halui.mdi-command-14             # use MDI command from main.ini
net pdnt.reserved.for.flood               whb.button.macro-15                                                    # Hardcoded for halui.flood on/off
net pdnt.reserved.for.mist                whb.button.macro-16                                                    # Hardcoded for halui.mist on/off

net pdnt.macro.11                         whb.button.macro-11                   halui.mdi-command-11             # use MDI command from main.ini
net pdnt.macro.12                         whb.button.macro-12                   halui.mdi-command-12             # use MDI command from main.ini
net pdnt.macro.13                         whb.button.macro-13                   halui.mdi-command-13             # use MDI command from main.ini


# flood and mist toggle signals
net  pdnt.flood.is-on                     whb.halui.flood.is-on                 halui.flood.is-on                #return signal is on or off
net  pdnt.flood.off                       whb.halui.flood.off                   halui.flood.off                  #reserved whb.button.macro-15
net  pdnt.flood.on                        whb.halui.flood.on                    halui.flood.on                   #reserved whb.button.macro-15

net  pdnt.mist.is-on                      whb.halui.mist.is-on                  halui.mist.is-on                 #return signal is on or off
net  pdnt.mist.off                        whb.halui.mist.off                    halui.mist.off                   #reserved whb.button.macro-16
net  pdnt.mist.on                         whb.halui.mist.on                     halui.mist.on                    #reserved whb.button.macro-16


# default function button signals
net pdnt.button.m-home                    whb.button.m-home                     halui.home-all                   # Homing use built-in halui home all
net pdnt.button.safe-z                    whb.button.safe-z                     halui.mdi-command-03             # Safe-z  use MDI command from main.ini
net pdnt.button.w-home                    whb.button.w-home                     halui.mdi-command-04             # Unpark  use MDI command from main.ini
net pdnt.button.probe-z                   whb.button.probe-z                    halui.mdi-command-08             # Probe-Z use MDI command from main.ini


# unused, just exposes pendant internal status or as basic button
#net pdnt.mode-lead                        whb.halui.feed.selected-lead
#net pdnt.mode-mpg-feed                    whb.halui.feed.selected-mpg-feed
#net pdnt.mode-continuous                  whb.halui.feed.selected-continuous
#net pdnt.mode-step                        whb.halui.feed.selected-step

#net pdnt.button.mode-mpg                  whb.button.mode-continuous
#net pdnt.button.mode-step                 whb.button.mode-step
#net pdnt.button.fn                        whb.button.fn
#net pdnt.button.reset                     whb.button.reset
#net pdnt.button.stop                      whb.button.stop
#net pdnt.button.start-pause               whb.button.start-pause
#net pdnt.button.s-on-off                  whb.button.s-on-off
#net pdnt.button.spindle-plus              whb.button.spindle-plus
#net pdnt.button.spindle-minus             whb.button.spindle-minus
#net pdnt.button.feed-plus                 whb.button.feed-plus
#net pdnt.button.feed-minus                whb.button.feed-minus


# spindle related signals
net pdnt.spindle.is-on                    whb.halui.spindle.is-on               spindle.0.on
net pdnt.spindle.start                    whb.halui.spindle.start               halui.spindle.0.start
net pdnt.spindle.stop                     whb.halui.spindle.stop                halui.spindle.0.stop
net pdnt.spindle.forward                  whb.halui.spindle.forward             halui.spindle.0.forward
net pdnt.spindle.reverse                  whb.halui.spindle.reverse             halui.spindle.0.reverse
net pdnt.spindle.increase                 whb.halui.spindle.increase            halui.spindle.0.increase         # reserved whb.button.macro-3
net pdnt.spindle.decrease                 whb.halui.spindle.decrease            halui.spindle.0.decrease         # reserved whb.button.macro-4
net pdnt.spindle-speed-abs                whb.halui.spindle-speed-cmd           spindle.0.speed-out-abs          # speed cmd from motion in rpm absolute


# spindle speed override signals
net pdnt.spindle-override.scale           whb.halui.spindle-override.scale      halui.spindle.0.override.scale   # needed for both spindle+/- and spindleoverride+/- button
net pdnt.spindle.override.value           halui.spindle.0.override.value        whb.halui.spindle-override.value # GUI feed rate related signals
net pdnt.spindle.override.increase        whb.halui.spindle-override.increase   halui.spindle.0.override.increase
net pdnt.spindle.override.decrease        whb.halui.spindle-override.decrease   halui.spindle.0.override.decrease


# GUI feed rate related signals can be used when program is running moving GUI slider
net pdnt.feed-override.scale              whb.halui.feed-override.scale         halui.feed-override.scale        # needed for both FeedOverride+/- and rotary knob button
net pdnt.max-velocity.value               whb.halui.max-velocity.value          halui.max-velocity.value         # needed for Mpg mode : button feed position% * max-velocity = Mpg feedrate


# take feed override min/max values from/to the GUI
net pdnt.feed-override.value              halui.feed-override.value             whb.halui.feed-override.value    # GUI feed rate related signals
net pdnt.feed-override.increase           whb.halui.feed-override.increase      halui.feed-override.increase
net pdnt.feed-override.decrease           whb.halui.feed-override.decrease      halui.feed-override.decrease


# axis position related signals feedback
net pdnt.axis.x.pos-feedback              halui.axis.x.pos-feedback             whb.halui.axis.x.pos-feedback
net pdnt.axis.y.pos-feedback              halui.axis.y.pos-feedback             whb.halui.axis.y.pos-feedback
net pdnt.axis.z.pos-feedback              halui.axis.z.pos-feedback             whb.halui.axis.z.pos-feedback


# axis position related signals relative
net pdnt.axis.x.pos-relative              halui.axis.x.pos-relative             whb.halui.axis.x.pos-relative
net pdnt.axis.y.pos-relative              halui.axis.y.pos-relative             whb.halui.axis.y.pos-relative
net pdnt.axis.z.pos-relative              halui.axis.z.pos-relative             whb.halui.axis.z.pos-relative



