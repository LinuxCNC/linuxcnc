# do not change the contents of this file as it will be overwiten by updates
# make custom changes in <machinename>_connections.hal

#***** PLASMAC COMPONENT *****
loadrt  plasmac
addf    plasmac  servo-thread

# inputs
net plasmac:axis-x-position         axis.x.pos-cmd              =>  plasmac.axis-x-position
net plasmac:axis-y-position         axis.y.pos-cmd              =>  plasmac.axis-y-position
net plasmac:breakaway-switch-out    debounce.0.1.out            =>  plasmac.breakaway
net plasmac:current-velocity        motion.current-vel          =>  plasmac.current-velocity
net plasmac:cutting-start           spindle.0.on                =>  plasmac.cutting-start
net plasmac:feed-override           halui.feed-override.value   =>  plasmac.feed-override
net plasmac:feed-reduction          motion.analog-out-03        =>  plasmac.feed-reduction
net plasmac:float-switch-out        debounce.0.0.out            =>  plasmac.float-switch
net plasmac:motion-type             motion.motion-type          =>  plasmac.motion-type
net plasmac:offset-current          axis.z.eoffset              =>  plasmac.offset-current
net plasmac:ohmic-probe-out         debounce.0.2.out            =>  plasmac.ohmic-probe
net plasmac:program-is-idle         halui.program.is-idle       =>  plasmac.program-is-idle
net plasmac:program-is-paused       halui.program.is-paused     =>  plasmac.program-is-paused
net plasmac:program-is-running      halui.program.is-running    =>  plasmac.program-is-running
net plasmac:requested-velocity      motion.requested-vel        =>  plasmac.requested-velocity
net plasmac:thc-disable             motion.digital-out-02       =>  plasmac.thc-disable
net plasmac:torch-off               motion.digital-out-03       =>  plasmac.torch-off
net plasmac:units-per-mm            halui.machine.units-per-mm  =>  plasmac.units-per-mm

# outputs
net plasmac:adaptive-feed           plasmac.adaptive-feed       =>  motion.adaptive-feed
net plasmac:feed-hold               plasmac.feed-hold           =>  motion.feed-hold
net plasmac:offset-enable           plasmac.offset-enable       =>  axis.x.eoffset-enable axis.y.eoffset-enable axis.z.eoffset-enable
net plasmac:offset-scale            plasmac.offset-scale        =>  axis.x.eoffset-scale axis.y.eoffset-scale axis.z.eoffset-scale
net plasmac:program-pause           plasmac.program-pause       =>  halui.program.pause
net plasmac:program-resume          plasmac.program-resume      =>  halui.program.resume
net plasmac:program-run             plasmac.program-run         =>  halui.program.run
net plasmac:program-stop            plasmac.program-stop        =>  halui.program.stop
net plasmac:torch-on                plasmac.torch-on
net plasmac:x-offset-counts         plasmac.x-offset-counts     =>  axis.x.eoffset-counts
net plasmac:y-offset-counts         plasmac.y-offset-counts     =>  axis.y.eoffset-counts
net plasmac:z-offset-counts         plasmac.z-offset-counts     =>  axis.z.eoffset-counts

# multiple spindles
if [info exists ::TRAJ(SPINDLES)] {
    set num_spindles [lindex $::TRAJ(SPINDLES) 0]
    if {$num_spindles > 1} {net plasmac:scribe-start spindle.1.on => plasmac.scribe-start}
    if {$num_spindles > 2} {net plasmac:spotting-start spindle.2.on => plasmac.spotting-start}
}

# powermax serial communications
if [info exists ::PLASMAC(PM_PORT)] {loadusr -Wn pmx485 pmx485 [lindex $::PLASMAC(PM_PORT) 0]}
