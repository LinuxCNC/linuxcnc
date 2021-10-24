# GENERIC HAL FILE FOR QTPLASMAC SIM CONFIGS

# ---SET CONSTANTS---
set numJoints $::KINS(JOINTS)
set z-axis [string first "z" [string tolower $::TRAJ(COORDINATES)]]

# ---COMPONENTS---
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$numJoints num_spindles=$::TRAJ(SPINDLES)
loadrt  plasmac
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    loadrt pid names=sim:${jnum}_pid
    loadrt mux2 names=sim:${jnum}_mux
    loadrt ddt names=sim:${jnum}_vel,sim:${jnum}_accel
    loadrt sim_home_switch names=sim:${jnum}_switch
}
loadrt hypot names=sim:hyp_xy,sim:hyp_xyz

# ---THREAD LINKS---
addf motion-command-handler servo-thread
addf motion-controller servo-thread
addf plasmac servo-thread
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    addf sim:${jnum}_pid.do-pid-calcs servo-thread
    addf sim:${jnum}_mux servo-thread
    addf sim:${jnum}_vel servo-thread
    addf sim:${jnum}_accel servo-thread
}
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    addf sim:${jnum}_switch servo-thread
}

# ---SETP COMMANDS FOR UNCONNECTED INPUT PINS---
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    setp sim:${jnum}_pid.Pgain 0
    setp sim:${jnum}_pid.Dgain 0
    setp sim:${jnum}_pid.Igain 0
    setp sim:${jnum}_pid.FF0 1.0
    setp sim:${jnum}_pid.FF1 0
    setp sim:${jnum}_pid.FF2 0
}

# ---MACHINE NET CONNECTIONS---
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    net sim:j${jnum}-acc sim:${jnum}_accel.out
    net sim:j${jnum}-enable joint.${jnum}.amp-enable-out => sim:${jnum}_pid.enable
    net sim:j${jnum}-homesw sim:${jnum}_switch.home-sw => joint.${jnum}.home-sw-in
    net sim:j${jnum}-on-pos sim:${jnum}_pid.output => sim:${jnum}_mux.in1
    net sim:j${jnum}-pos-cmd joint.${jnum}.motor-pos-cmd => sim:${jnum}_pid.command
    net sim:j${jnum}-pos-fb sim:${jnum}_mux.out => sim:${jnum}_mux.in0 sim:${jnum}_switch.cur-pos sim:${jnum}_vel.in joint.${jnum}.motor-pos-fb
    net sim:j${jnum}-vel sim:${jnum}_vel.out => sim:${jnum}_accel.in
}
net sim:xy-vel sim:hyp_xy.out
net sim:xyz-vel sim:hyp_xyz.out
net sim:enable motion.motion-enabled
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    net sim:enable sim:${jnum}_mux.sel
}
foreach {x y z } {0 0 0 } {}
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    if {[string index $::TRAJ(COORDINATES) $jnum] == "X"} {
        if {$x == 0} {
            incr x
            net sim:j${jnum}-vel => sim:hyp_xy.in0 sim:hyp_xyz.in0
        }
    } elseif {[string index $::TRAJ(COORDINATES) $jnum] == "Y"} {
        if {$y == 0} {
            incr y
            net sim:j${jnum}-vel => sim:hyp_xy.in1 sim:hyp_xyz.in1
        }
    } elseif {[string index $::TRAJ(COORDINATES) $jnum] == "Z"} {
        if {$z == 0} {
            incr z
            net sim:j${jnum}-vel => sim:hyp_xyz.in2
        }
    }
}

# ---PLASMA INPUT DEBOUNCE---
loadrt dbounce names=db_breakaway,db_float,db_ohmic,db_arc-ok
addf db_float     servo-thread
addf db_ohmic     servo-thread
addf db_breakaway servo-thread
addf db_arc-ok    servo-thread

# ---PLASMAC COMPONENT INPUTS---
net plasmac:arc-ok               db_arc-ok.out               =>  plasmac.arc-ok-in
net plasmac:axis-position        joint.${z-axis}.pos-fb      =>  plasmac.axis-z-position
net plasmac:axis-x-position      axis.x.pos-cmd              =>  plasmac.axis-x-position
net plasmac:axis-y-position      axis.y.pos-cmd              =>  plasmac.axis-y-position
net plasmac:breakaway-switch-out db_breakaway.out            =>  plasmac.breakaway
net plasmac:current-velocity     motion.current-vel          =>  plasmac.current-velocity
net plasmac:cutting-start        spindle.0.on                =>  plasmac.cutting-start
net plasmac:feed-override        halui.feed-override.value   =>  plasmac.feed-override
net plasmac:feed-reduction       motion.analog-out-03        =>  plasmac.feed-reduction
net plasmac:float-switch-out     db_float.out                =>  plasmac.float-switch
net plasmac:ignore-arc-ok-0      motion.digital-out-01       =>  plasmac.ignore-arc-ok-0
net machine-is-on                halui.machine.is-on         =>  plasmac.machine-is-on
net plasmac:motion-type          motion.motion-type          =>  plasmac.motion-type
net plasmac:offsets-active       motion.eoffset-active       =>  plasmac.offsets-active
net plasmac:ohmic-probe-out      db_ohmic.out                =>  plasmac.ohmic-probe
net plasmac:program-is-idle      halui.program.is-idle       =>  plasmac.program-is-idle
net plasmac:program-is-paused    halui.program.is-paused     =>  plasmac.program-is-paused
net plasmac:program-is-running   halui.program.is-running    =>  plasmac.program-is-running
net plasmac:requested-velocity   motion.requested-vel        =>  plasmac.requested-velocity
net plasmac:scribe-start         spindle.1.on                =>  plasmac.scribe-start
net plasmac:spotting-start       spindle.2.on                =>  plasmac.spotting-start
net plasmac:thc-disable          motion.digital-out-02       =>  plasmac.thc-disable
net plasmac:torch-off            motion.digital-out-03       =>  plasmac.torch-off
net plasmac:units-per-mm         halui.machine.units-per-mm  =>  plasmac.units-per-mm
net plasmac:x-offset-current     axis.x.eoffset              =>  plasmac.x-offset-current
net plasmac:y-offset-current     axis.y.eoffset              =>  plasmac.y-offset-current
net plasmac:z-offset-current     axis.z.eoffset              =>  plasmac.z-offset-current

# ---PLASMAC COMPONENT OUTPUTS---
net plasmac:adaptive-feed        plasmac.adaptive-feed       =>  motion.adaptive-feed
net plasmac:cutting-stop         halui.spindle.0.stop        =>  plasmac.cutting-stop
net plasmac:feed-hold            plasmac.feed-hold           =>  motion.feed-hold
net plasmac:offset-scale         plasmac.offset-scale        =>  axis.x.eoffset-scale axis.y.eoffset-scale axis.z.eoffset-scale
net plasmac:program-pause        plasmac.program-pause       =>  halui.program.pause
net plasmac:program-resume       plasmac.program-resume      =>  halui.program.resume
net plasmac:program-run          plasmac.program-run         =>  halui.program.run
net plasmac:program-stop         plasmac.program-stop        =>  halui.program.stop
net plasmac:torch-on             plasmac.torch-on
net plasmac:x-offset-counts      plasmac.x-offset-counts     =>  axis.x.eoffset-counts
net plasmac:y-offset-counts      plasmac.y-offset-counts     =>  axis.y.eoffset-counts
net plasmac:xy-offset-enable     plasmac.xy-offset-enable    =>  axis.x.eoffset-enable axis.y.eoffset-enable
net plasmac:z-offset-counts      plasmac.z-offset-counts     =>  axis.z.eoffset-counts
net plasmac:z-offset-enable      plasmac.z-offset-enable     =>  axis.z.eoffset-enable

# ---POWERMAX RS485 COMPONENT---
#loadusr -Wn pmx485 pmx485 /dev/ttyUSB0

# ---TOOL CHANGE PASSTHROUGH
net tool-number <= iocontrol.0.tool-prep-number
net tool-change-loopback iocontrol.0.tool-change => iocontrol.0.tool-changed
net tool-prepare-loopback iocontrol.0.tool-prepare => iocontrol.0.tool-prepared

# ---QTPLASMAC ESTOP HANDLING---
loadrt or2 names=estop_or
loadrt not names=estop_not
addf estop_or servo-thread
addf estop_not servo-thread
net sim:estop-raw estop_or.out estop_not.in
net sim:estop-out estop_not.out iocontrol.0.emc-enable-in
if {[info exists ::QTPLASMAC(ESTOP_TYPE)]} {
    if {$::QTPLASMAC(ESTOP_TYPE) == 2} {
        loadrt not names=estop_not_1
        addf estop_not_1 servo-thread
        net sim:estop-1-raw iocontrol.0.user-enable-out => estop_not_1.in
        net sim:estop-1-in estop_not_1.out => estop_or.in1
    }
}