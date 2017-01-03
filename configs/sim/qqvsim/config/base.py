from machinekit import hal
from machinekit import rtapi as rt
from machinekit import config as c

import rcomps
import motion


def usrcomp_status(compname, signame, thread, resetSignal='estop-reset'):
    sigIn = hal.newsig('%s-error-in' % signame, hal.HAL_BIT)
    sigOut = hal.newsig('%s-error' % signame, hal.HAL_BIT)
    sigOk = hal.newsig('%s-ok' % signame, hal.HAL_BIT)

    sigIn.link('%s.error' % compname)

    safetyLatch = rt.newinst('safety_latch', 'safety-latch.%s-error' % signame)
    hal.addf(safetyLatch.name, thread)
    safetyLatch.pin('error-in').link(sigIn)
    safetyLatch.pin('error-out').link(sigOut)
    safetyLatch.pin('reset').link(resetSignal)
    safetyLatch.pin('threshold').set(500)  # 500ms error
    safetyLatch.pin('latching').set(True)

    notComp = rt.newinst('not', 'not.%s-no-error' % signame)
    hal.addf(notComp.name, thread)
    notComp.pin('in').link(sigOut)
    notComp.pin('out').link(sigOk)


def usrcomp_watchdog(comps, enableSignal, thread,
                     okSignal=None, errorSignal=None):
    count = len(comps)
    watchdog = rt.loadrt('watchdog', num_inputs=count)
    hal.addf('watchdog.set-timeouts', thread)
    hal.addf('watchdog.process', thread)
    for n, comp in enumerate(comps):
        compname = comp[0]
        comptime = comp[1]
        sigIn = hal.newsig('%s-watchdog' % compname, hal.HAL_BIT)
        hal.Pin('%s.watchdog' % compname).link(sigIn)
        watchdog.pin('input-%i' % n).link(sigIn)
        watchdog.pin('timeout-%i' % n).set(comptime)
    watchdog.pin('enable-in').link(enableSignal)

    if not okSignal:
        okSignal = hal.newsig('watchdog-ok', hal.HAL_BIT)
    watchdog.pin('ok-out').link(okSignal)

    if errorSignal:
        notComp = rt.newinst('not', 'not.watchdog-error')
        hal.addf(notComp.name, thread)
        notComp.pin('in').link(okSignal)
        notComp.pin('out').link(errorSignal)


def setup_stepper(stepgenIndex, section, axisIndex=None,
                  stepgenType='hpg.stepgen', gantry=False,
                  gantryJoint=0, velocitySignal=None, thread=None):
    hasStepgen = False
    if (stepgenType != 'sim'):
        stepgen = '%s.%02i' % (stepgenType, stepgenIndex)
        hasStepgen = True
    if axisIndex is not None:
        axis = 'axis.%i' % axisIndex
    hasMotionAxis = (axisIndex is not None) and (not gantry or gantryJoint == 0)
    velocityControlled = velocitySignal is not None

    # axis enable chain
    enableIndex = axisIndex
    if axisIndex is None:
        enableIndex = 0  # use motor enable signal
    enable = hal.Signal('emcmot-%i-enable' % enableIndex, hal.HAL_BIT)
    if hasMotionAxis:
        enable.link('%s.amp-enable-out' % axis)
    if hasStepgen:
        enable.link('%s.enable' % stepgen)

    # position command and feedback
    if not velocityControlled:
        if hasMotionAxis:  # per axis fb and cmd
            posCmd = hal.newsig('emcmot-%i-pos-cmd' % axisIndex, hal.HAL_FLOAT)
            posCmd.link('%s.motor-pos-cmd' % axis)
            if hasStepgen:
                if not gantry:
                    posCmd.link('%s.position-cmd' % stepgen)
                else:
                    posCmd.link('gantry.%i.position-cmd' % axisIndex)
            else:
                posCmd.link('%s.motor-pos-fb' % axis)


            if hasStepgen:
                posFb = hal.newsig('emcmot-%i-pos-fb' % axisIndex, hal.HAL_FLOAT)
                posFb.link('%s.motor-pos-fb' % axis)
                if not gantry:
                    posFb.link('%s.position-fb' % stepgen)
                else:
                    posFb.link('gantry.%i.position-fb' % axisIndex)


        if gantry:  # per joint fb and cmd
            posCmd = hal.newsig('emcmot-%i-%i-pos-cmd' % (axisIndex, gantryJoint), hal.HAL_FLOAT)
            posCmd.link('gantry.%i.joint.%02i.pos-cmd' % (axisIndex, gantryJoint))
            if hasStepgen:
                posCmd.link('%s.position-cmd' % stepgen)
 
            posFb = hal.newsig('emcmot-%i-%i-pos-fb' % (axisIndex, gantryJoint), hal.HAL_FLOAT)
            posFb.link('gantry.%i.joint.%02i.pos-fb' % (axisIndex, gantryJoint))
            if hasStepgen:
                posFb.link('%s.position-fb' % stepgen)

    else:  # velocity control
        print "ERROR: not support velocity control yet"
#         hal.net(velocitySignal, '%s.velocity-cmd' % stepgen)

    # limits
    if hasMotionAxis:
        limitHome = hal.newsig('limit-%i-home' % axisIndex, hal.HAL_BIT)
        limitMin = hal.newsig('limit-%i-min' % axisIndex, hal.HAL_BIT)
        limitMax = hal.newsig('limit-%i-max' % axisIndex, hal.HAL_BIT)
        limitHome.link('%s.home-sw-in' % axis)
        limitMin.link('%s.neg-lim-sw-in' % axis)
        limitMax.link('%s.pos-lim-sw-in' % axis)

#     if gantry:
#         if gantryJoint == 0:
#             axisHoming = hal.newsig('emcmot-%i-homing' % axisIndex, hal.HAL_BIT)
#             axisHoming.link('%s.homing' % axis)
# 
#             hal.Pin('gantry.%i.search-vel' % axisIndex).set(c.find(section, 'HOME_SEARCH_VEL'))
#             hal.Pin('gantry.%i.homing' % axisIndex).link(axisHoming)
#             hal.Pin('gantry.%i.home' % axisIndex).link(limitHome)
# 
#             or2 = rt.newinst('or2', 'or2.limit-%i-min' % axisIndex)
#             hal.addf(or2.name, thread)
#             or2.pin('out').link(limitMin)
# 
#             or2 = rt.newinst('or2', 'or2.limit-%i-max' % axisIndex)
#             hal.addf(or2.name, thread)
#             or2.pin('out').link(limitMax)
# 
#         limitHome = hal.newsig('limit-%i-%i-home' % (axisIndex, gantryJoint),
#                                hal.HAL_BIT)
#         limitMin = hal.newsig('limit-%i-%i-min' % (axisIndex, gantryJoint),
#                               hal.HAL_BIT)
#         limitMax = hal.newsig('limit-%i-%i-max' % (axisIndex, gantryJoint),
#                               hal.HAL_BIT)
#         homeOffset = hal.Signal('home-offset-%i-%i' % (axisIndex, gantryJoint),
#                                 hal.HAL_FLOAT)
#         limitHome.link('gantry.%i.joint.%02i.home' % (axisIndex, gantryJoint))
#         limitMin.link('or2.limit-%i-min.in%i' % (axisIndex, gantryJoint))
#         limitMax.link('or2.limit-%i-max.in%i' % (axisIndex, gantryJoint))
#         homeOffset.link('gantry.%i.joint.%02i.home-offset' % (axisIndex, gantryJoint))

#         storage.setup_gantry_storage(axisIndex, gantryJoint)

    # stepper pins configured in hardware setup


def setup_stepper_multiplexer(stepgenIndex, sections, selSignal, thread):
    num = len(sections)
    sigBase = 'stepgen-%i' % stepgenIndex

    unsignedSignals = [['dirsetup', 'DIRSETUP'],
                       ['dirhold', 'DIRHOLD'],
                       ['steplen', 'STEPLEN'],
                       ['stepspace', 'STEPSPACE']]

    floatSignals = [['scale', 'SCALE'],
                    ['max-vel', 'STEPGEN_MAX_VEL'],
                    ['max-acc', 'STEPGEN_MAX_ACC']]

    for item in unsignedSignals:
        signal = hal.Signal('%s-%s' % (sigBase, item[0]), hal.HAL_U32)
        mux = rt.newinst('muxn_u32', 'mux%i.%s' % (num, signal.name), pincount=num)
        hal.addf(mux.name, thread)
        for n, section in enumerate(sections):
            mux.pin('in%i' % n).set(c.find(section, item[1]))
        mux.pin('sel').link(selSignal)
        mux.pin('out').link(signal)

    for item in floatSignals:
        signal = hal.Signal('%s-%s' % (sigBase, item[0]), hal.HAL_FLOAT)
        mux = rt.newinst('muxn', 'mux%i.%s' % (num, signal.name), pincount=num)
        hal.addf(mux.name, thread)
        for n, section in enumerate(sections):
            mux.pin('in%i' % n).set(c.find(section, item[1]))
        mux.pin('sel').link(selSignal)
        mux.pin('out').link(signal)


def setup_estop(errorSignals, thread):
    # Create estop signal chain
    estopUser = hal.Signal('estop-user', hal.HAL_BIT)
#     estopReset = hal.Signal('estop-reset', hal.HAL_BIT)
#     estopOut = hal.Signal('estop-out', hal.HAL_BIT)
#     estopIn = hal.Signal('estop-in', hal.HAL_BIT)
#     estopError = hal.Signal('estop-error', hal.HAL_BIT)

#     num = len(errorSignals)
#     orComp = rt.newinst('orn', 'or%i.estop-error' % num, pincount=num)
#     hal.addf(orComp.name, thread)
#     for n, sig in enumerate(errorSignals):
#         orComp.pin('in%i' % n).link(sig)
#     orComp.pin('out').link(estopError)

#     estopLatch = rt.newinst('estop_latch', 'estop-latch')
#     hal.addf(estopLatch.name, thread)
#     estopLatch.pin('ok-in').link(estopUser)
#     estopLatch.pin('fault-in').link(estopError)
#     estopLatch.pin('reset').link(estopReset)
#     estopLatch.pin('ok-out').link(estopOut)

    estopUser.link('iocontrol.0.user-enable-out')
#     estopReset.link('iocontrol.0.user-request-enable')

    # Monitor estop input from hardware
#     estopIn.link('iocontrol.0.emc-enable-in')
    estopUser.link('iocontrol.0.emc-enable-in')


def setup_tool_loopback():
    # create signals for tool loading loopback
    hal.net('iocontrol.0.tool-prepare', 'iocontrol.0.tool-prepared')
    hal.net('iocontrol.0.tool-change', 'iocontrol.0.tool-changed')


def setup_estop_loopback():
    # create signal for estop loopback
    hal.net('iocontrol.0.user-enable-out', 'iocontrol.0.emc-enable-in')

def setup_extras():
    name = 'extras'
    comp = hal.RemoteComponent(name, timer=100)
    comp.newpin('gpio.in.0', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('gpio.in.1', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('gpio.in.2', hal.HAL_BIT, hal.HAL_IN)
    comp.ready()

    # comp.pin('offset-left').link('home-offset-%i-0' % axisIndex)
    # comp.pin('offset-right').link('home-offset-%i-1' % axisIndex)

def init_gantry(axisIndex, joints=2, latching=True):
    if latching:
        comp = 'lgantry'
    else:
        comp = 'gantry'
    rt.newinst(comp, 'gantry.%i' % axisIndex, pincount=joints)
    rcomps.create_gantry_rcomp(axisIndex=axisIndex)


def gantry_read(gantryAxis, thread):
    hal.addf('gantry.%i.read' % gantryAxis, thread)


def gantry_write(gantryAxis, thread):
    hal.addf('gantry.%i.write' % gantryAxis, thread)
