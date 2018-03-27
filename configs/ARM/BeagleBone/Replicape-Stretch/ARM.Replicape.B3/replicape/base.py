#!/usr/bin/python2
# encoding: utf-8

import os
import subprocess
from machinekit import rtapi as rtapi
from machinekit import hal
from machinekit import config

import hardware
from temperature import ExtruderTemperature, HbpTemperature
import re
from extrusion import Extruder

SERVO_THREAD = 'servo-thread'
HARDWARE_PATH = os.path.dirname(os.path.realpath(__file__)) + '/'
BOARD_REV = ''
AXIS_TOTAL = 0
NUM_FANS = 0
FAN_IO_START = 12
EXTRUDER_TOTAL = 1
DEFAULT_CURRENT = 0.5

def check_version():
    global BOARD_REV
# we use a 4.14.18-ti-rt-r33 kernel and have enable_uboot_overlays=1
# in /boot/uEnv.txt
# u-boot overlays do not use the slots file, so we take the board
# version from the INI file
# is there any way to find not just the loaded cape name, but also
# its version (short of reading the cape eeprom)?
# we check in /sys/proc/cmdline for the REPLICAPE cape
#
#    with open('/sys/devices/platform/bone_capemgr/slots', 'r') as file:
#        content = file.read()
    content = subprocess.check_output(['cat','/proc/cmdline'])
    if re.search('capes=BB-BONE-REPLICAP', content) is None:
        raise RuntimeError('Replicape not found. Is the cape mounted?')

    content = config.find('FDM','BOARD_REV')
    if (content is not None) and (content in ['B3A', 'A4A']):
        BOARD_REV = content
    else:
        raise RuntimeError('BOARD_REV not set in ini file. Set to B3A or A4A!')

def setup_enable_chain():
    """
    Create enable and enable_inv signal for the system
    """
    main_enable = hal.net('main-enable', 'axis.0.amp-enable-out')

    n = rtapi.newinst('not', 'main-enable.not')
    hal.addf(n.name, SERVO_THREAD)

    main_enable_inv = hal.newsig('main-enable-inv', hal.HAL_BIT)
    n.pin('in').link('main-enable') 
    n.pin('out').link('main-enable-inv')

    return (main_enable, main_enable_inv)

def setup_joints():
    """
    Create joints commands and feedbacks signal for the system
    """
    commands = [None] * AXIS_TOTAL
    feedbacks = [None] * AXIS_TOTAL
    for i in xrange(AXIS_TOTAL):
        commands[i] = hal.newsig('machine.joint.%d.command' % i, hal.HAL_FLOAT)
        feedbacks[i] = hal.newsig('machine.joint.%d.feedback' % i, hal.HAL_FLOAT)

    core_xy = config.find('FDM','CORE_XY')
    if core_xy is not None and int(core_xy) > 0:
        sum_cmd_a = rtapi.newinst('sum2', 'corexy.sum2.cmd.a')
        sum_fb_x = rtapi.newinst('sum2', 'corexy.sum2.fb.x')
        sum_cmd_b = rtapi.newinst('sum2', 'corexy.sum2.cmd.b')
        sum_fb_y = rtapi.newinst('sum2', 'corexy.sum2.fb.y')
        hal.addf(sum_cmd_a.name, SERVO_THREAD)
        hal.addf(sum_cmd_b.name, SERVO_THREAD)
        hal.addf(sum_fb_x.name, SERVO_THREAD)
        hal.addf(sum_fb_y.name, SERVO_THREAD)

        sum_cmd_a.pin('gain0').set(1)
        sum_cmd_a.pin('gain1').set(1)
        sum_cmd_b.pin('gain0').set(1)
        sum_cmd_b.pin('gain1').set(-1)

        sum_fb_x.pin('gain0').set(0.5)
        sum_fb_x.pin('gain1').set(0.5)
        sum_fb_y.pin('gain0').set(0.5)
        sum_fb_y.pin('gain1').set(-0.5)

        corex_cmd = hal.newsig('machine.joint.corex.command', hal.HAL_FLOAT)
        corey_cmd = hal.newsig('machine.joint.corey.command', hal.HAL_FLOAT)

        corex_cmd.link('axis.0.motor-pos-cmd')
        corey_cmd.link('axis.1.motor-pos-cmd')

        sum_cmd_a.pin('in0').link(corex_cmd)
        sum_cmd_a.pin('in1').link(corey_cmd)
        sum_cmd_b.pin('in0').link(corex_cmd)
        sum_cmd_b.pin('in1').link(corey_cmd)
        sum_cmd_a.pin('out').link(commands[0])
        sum_cmd_b.pin('out').link(commands[1])

        sum_fb_x.pin('in0').link(feedbacks[0])
        sum_fb_x.pin('in1').link(feedbacks[1])
        sum_fb_y.pin('in0').link(feedbacks[0])
        sum_fb_y.pin('in1').link(feedbacks[1])
        sum_fb_x.pin('out').link('axis.0.motor-pos-fb')
        sum_fb_y.pin('out').link('axis.1.motor-pos-fb')
    else:
        commands[0].link('axis.0.motor-pos-cmd')
        feedbacks[0].link('axis.0.motor-pos-fb')
        commands[1].link('axis.1.motor-pos-cmd')
        feedbacks[1].link('axis.1.motor-pos-fb')

    for i in xrange(AXIS_TOTAL):
        if i >= 2:
            commands[i].link('axis.%d.motor-pos-cmd' % i)
            feedbacks[i].link('axis.%d.motor-pos-fb' % i)

    return (commands, feedbacks)

def setup_axis(replicape, command, feedback, nr):
    section = 'AXIS_%d' % nr
    port_str = config.find(section, 'PORT', str(nr))
    ports = re.split('\s*,\s*', port_str)

    replicape.get_pru_pin('%s.position-fb' % replicape.get_stepgen(ports[0])).link(feedback)
    for port in ports:
        sg = replicape.get_stepgen(port)
        replicape.get_pru_pin('%s.position-cmd' % (sg)).link(command)
        replicape.get_pru_pin('%s.position-scale' % (sg)) \
            .set(float(
                config.find(section, 'SCALE_PORT_%s' % port, config.find(section, 'SCALE', 'SCALE must be defined'))
            ))
        replicape.get_pru_pin('%s.enable' % (sg)).link('main-enable')
        replicape.set_motor_current(
            replicape.get_motor_port(port),
            float(
                config.find(section, 'CURRENT_PORT_%s' % port, config.find(section, 'CURRENT', DEFAULT_CURRENT))
            ))

def setup_extruder(replicape, extruder, nr):
    section = 'EXTRUDER_%d' % nr
    port_str = str(config.find(section, 'PORT', str(nr + 3)))  # Extruder starts at port 3, after XYZ
    ports = re.split('\s*,\s*', port_str)

    for port in ports:
        sg = replicape.get_stepgen(port)
        replicape.get_pru_pin('%s.control-type' % (sg)).set(1)
        replicape.get_pru_pin('%s.velocity-cmd' % (sg)).link(extruder.extrude_vel_sig)
        replicape.get_pru_pin('%s.position-scale' % (sg)) \
            .set(float(
                config.find(section, 'SCALE_PORT_%s' % port, config.find(section, 'SCALE', 'SCALE must be defined'))
            ))
        replicape.get_pru_pin('%s.enable' % (sg)).link('main-enable')
        replicape.set_motor_current(
            replicape.get_motor_port(port),
            float(
                config.find(section, 'CURRENT_PORT_%s' % port, config.find(section, 'CURRENT', DEFAULT_CURRENT))
            ))

def setup_extruders(replicape, extruder_sel_sig):
    motion_vel_sig = hal.newsig('ve-motion-vel', hal.HAL_FLOAT)
    motion_vel_sig.link('motion.current-vel')
        
    extruders = [None] * EXTRUDER_TOTAL
    for i in xrange(EXTRUDER_TOTAL):
        extruders[i] = Extruder(i, motion_vel_sig, config)
        setup_extruder(replicape, extruders[i], i)

    shared_signals = [
        # Shared Signal   Type           Attribute            fdm-ve-jog     fdm-ve-params      analog- or digital-io
        ('enable',        hal.HAL_BIT,   'enable_sig',        None,          None,              1),
        ('cross-section', hal.HAL_FLOAT, 'cross_section_sig', None,          None,              41),
        ('jog-vel',       hal.HAL_FLOAT, 'jog_vel_sig',       'velocity',    None,              45),
        ('jog-direction', hal.HAL_BIT,   'jog_direction_sig', 'direction',   None,              14),
        ('jog-distance',  hal.HAL_FLOAT, 'jog_distance_sig',  'distance',    None,              46),
        ('jog-trigger',   hal.HAL_BIT,   'jog_trigger_sig',   'trigger',     None,              12),
        ('jog-continuous',hal.HAL_BIT,   'jog_continuous_sig','continuous',  None,              13),
        ('jog-dtg',       hal.HAL_FLOAT, 'jog_dtg_sig',       'dtg',         None,              None),
        ('max-jog-vel',   hal.HAL_FLOAT, 'max_jog_vel_sig',   'max-velocity',None,              None),
        ('filament-dia',  hal.HAL_FLOAT, 'filament_dia_sig',  None,          'filament-dia',    44),
        ('extrude-scale', hal.HAL_FLOAT, 'extrude_scale_sig', None,          'extrude-scale',   49),
        ('accel-gain',    hal.HAL_FLOAT, 'accel_gain_sig',    None,          'accel-adj-gain',  None),
        ('retract-vel',   hal.HAL_FLOAT, 'retract_vel_sig',   None,          'retract-vel',     48),
        ('retract-len',   hal.HAL_FLOAT, 'retract_len_sig',   None,          'retract-len',     47),

        ('extrude-vel',   hal.HAL_FLOAT, 'extrude_vel_sig',   None,          None,              None),
        ('retracting',    hal.HAL_BIT  , 'retracting_sig',    None,          None,              None),
    ]

    comp_jog = hal.RemoteComponent('fdm-ve-jog', timer=100)
    comp_params = hal.RemoteComponent('fdm-ve-params', timer=100)

    for (signal_name, signal_type, attr_name, comp_jog_name, comp_params_name, io_pin) in shared_signals:
        signal = hal.newsig('ve.%s' % signal_name, signal_type)
        comp_dir_type = hal.HAL_IO

        if signal_name == 'jog-dtg' or signal_name == 'max-jog-vel' or signal_name == 'extrude-vel':
            mux_type = 'muxn' 
            comp_dir_type = hal.HAL_IN
        elif signal_name == 'retracting':
            mux_type = 'muxn_bit' 
            comp_dir_type = hal.HAL_IN
        else:
            mux_type = 'io_muxn' if signal_type == hal.HAL_FLOAT else 'io_muxn_bit'

        mux = rtapi.newinst(
            mux_type,
            'ex.mux%d.%s' % (EXTRUDER_TOTAL, signal_name), 
            pincount=EXTRUDER_TOTAL)
        hal.addf(mux.name, SERVO_THREAD)
        mux.pin('out').link(signal)
        mux.pin('sel').link(extruder_sel_sig)
        for j in xrange(EXTRUDER_TOTAL):
            mux.pin('in%i' % j).link(getattr(extruders[j], attr_name))

        if comp_jog_name is not None:
            pin = comp_jog.newpin(comp_jog_name, signal_type, comp_dir_type)
            pin.link(signal)
        if comp_params_name is not None:
            pin = comp_params.newpin(comp_params_name, signal_type, comp_dir_type)
            pin.link(signal)
        if io_pin is not None:
            if signal_type == hal.HAL_FLOAT:
                signal.link('motion.analog-out-io-%d' % io_pin)
            else:
                signal.link('motion.digital-out-io-%02i' % io_pin)
            
    hal.Signal('ve.retracting').link('motion.feed-hold')

    extruder_sel_sig.link('iocontrol.0.tool-prep-number')
    comp_jog.newpin('extruder-sel', hal.HAL_S32, hal.HAL_IN).link(extruder_sel_sig)
    comp_jog.newpin('extruder-count', hal.HAL_U32, hal.HAL_IN).set(EXTRUDER_TOTAL)

    comp_jog.ready()
    comp_params.ready()

def setup_system_fan(replicape):
    en = config.find('FDM','SYSTEM_FAN', 0)
    if int(en) == 0:
        return

    fan_sig = hal.newsig('fan-output', hal.HAL_FLOAT)
    replicape.get_fan_pwm_pin(3).link(fan_sig)
    fan_sig.set(1.0)
    
def setup_fans(replicape):
    if NUM_FANS == 0:
    	return
    en = config.find('FDM','SYSTEM_FAN', 0)
    fan_out = [None] * NUM_FANS
    fan_scale = [None] * NUM_FANS
    fan_in = [None] * NUM_FANS
    #on-board pwm input is 0.0 to 1.0, M106 sends 0 to 255
    for i in xrange(NUM_FANS):
        fan_out[i] =  hal.newsig('fan-out-%d' % i, hal.HAL_FLOAT)
        replicape.get_fan_pwm_pin(i).link(fan_out[i])
        # the system fan is connected to the last fan pwm output
        if (int(en) > 0) and (i == NUM_FANS-1):
            fan_out[i].set(1.0)
        else:
            fan_in[i] = hal.newsig('fan-in-%d' % i, hal.HAL_FLOAT)
            fan_in[i].link('motion.analog-out-io-%d' % (FAN_IO_START + i))
            fan_scale[i] = rtapi.newinst('div2', 'fan%d.div2.scale-pwm' % i)
            hal.addf(fan_scale[i].name, SERVO_THREAD)
            fan_scale[i].pin('in0').link(fan_in[i])
            fan_scale[i].pin('in1').set(255.0)
            fan_scale[i].pin('out').link(fan_out[i])


def setup_limit_switches(replicape):
    limit_x_sig = hal.newsig('limit-x', hal.HAL_BIT)
    limit_x_sig.link('axis.0.home-sw-in')
    limit_x_pos = config.find('AXIS_0', 'HOME_SEARCH_VEL', 0)
    if limit_x_pos < 0:
        replicape.get_limit_pin('X', False).link(limit_x_sig)
        limit_x_sig.link('axis.0.neg-lim-sw-in')
    if limit_x_pos > 0:
        replicape.get_limit_pin('X', True).link(limit_x_sig)
        limit_x_sig.link('axis.0.pos-lim-sw-in')

    limit_y_sig = hal.newsig('limit-y', hal.HAL_BIT)
    limit_y_sig.link('axis.1.home-sw-in')
    limit_y_pos = config.find('AXIS_1', 'HOME_SEARCH_VEL', 0)
    if limit_y_pos < 0:
        replicape.get_limit_pin('Y', False).link(limit_y_sig)
        limit_y_sig.link('axis.1.neg-lim-sw-in')
    if limit_y_pos > 0:
        replicape.get_limit_pin('Y', True).link(limit_y_sig)
        limit_y_sig.link('axis.1.pos-lim-sw-in')

    limit_z_sig = hal.newsig('limit-z', hal.HAL_BIT)
    limit_z_sig.link('axis.2.home-sw-in')
    limit_z_pos = config.find('AXIS_2', 'HOME_SEARCH_VEL', 0)
    if limit_z_pos < 0:
        replicape.get_limit_pin('Z', False).link(limit_z_sig)
        limit_z_sig.link('axis.2.neg-lim-sw-in')
    if limit_z_pos > 0:
        replicape.get_limit_pin('Z', True).link(limit_z_sig)
        limit_z_sig.link('axis.2.pos-lim-sw-in')

    probe_pin = replicape.get_probe_pin();
    if probe_pin is not None:
        probe_sig = hal.newsig('limit-probe', hal.HAL_BIT)
        probe_sig.link(probe_pin)
        probe_sig.link('motion.probe-input')

def setup_estop(error_sigs, watchdog_sigs, estop_reset, thread):
    # Create estop signal chain
    estop_user = hal.Signal('estop-user', hal.HAL_BIT)
    estop_user.link('iocontrol.0.user-enable-out')
    
    estop_reset.link('iocontrol.0.user-request-enable')

    estop_out = hal.Signal('estop-clear', hal.HAL_BIT)
    estop_out.link('iocontrol.0.emc-enable-in')

    estop_latch = rtapi.newinst('estop_latch', 'estop.estop-latch')
    hal.addf(estop_latch.name, thread)
    estop_latch.pin('ok-in').link(estop_user)
    estop_latch.pin('reset').link(estop_reset)
    estop_latch.pin('ok-out').link(estop_out)

    watchdog_sigs = [] # TODO: Fix watchdog code
    if len(watchdog_sigs) > 0:
        watchdog_ok_sig = hal.newsig('estop.watchdog-ok', hal.HAL_BIT)
        watchdog_error_sig = hal.newsig('estop.watchdog-error', hal.HAL_BIT)
        watchdog = rtapi.newinst('watchdog', 'estop.watchdog', pincount=len(watchdog_sigs))
        hal.addf(watchdog.name, thread)
        for n, sig in watchdog_sigs:
            watchdog.pin('input-%02d' % n).link(sig)
        watchdog.pin('enable').set(True)
        watchdog.pin('ok-out').link(watchdog_ok_sig)
        
        watchdog_not = rtapi.newinst('not', 'estop.watchdog.not')
        hal.addf(watchdog_not.name, thread)
        watchdog_not.pin('in').link(watchdog_ok_sig)
        watchdog_not.pin('out').link(watchdog_error_sig)

        error_sigs.append(watchdog_error_sig)

    num = len(error_sigs)
    if num > 0:
        estop_fault = hal.Signal('estop-fault', hal.HAL_BIT)
        orn = rtapi.newinst('orn', 'estop.or%i.error' % num, pincount=num)
        hal.addf(orn.name, thread)
        for n, sig in enumerate(error_sigs):
            orn.pin('in%i' % n).link(sig)
        orn.pin('out').link(estop_fault)
        estop_latch.pin('fault-in').link(estop_fault)

def connect_tool_changer():
    p = hal.Signal('tool-prepared', hal.HAL_BIT)
    p.link('iocontrol.0.tool-prepare')
    p.link('iocontrol.0.tool-prepared')
    c = hal.Signal('tool-changed', hal.HAL_BIT)
    c.link('iocontrol.0.tool-change')
    c.link('iocontrol.0.tool-changed')

def init_hardware():
    config.load_ini(os.environ['INI_FILE_NAME'])
    check_version()
# we need to make the gpio pins used by the pru visible to userspace (??) in /sys/class/gpio
    os.system('sudo ' + HARDWARE_PATH + 'set_pru_gpio.sh')
    rtapi.init_RTAPI()
    error_sigs = []
    watchdog_sigs = []

    global AXIS_TOTAL
    AXIS_TOTAL = int(config.find('TRAJ', 'AXES', '3'))
    if AXIS_TOTAL < 3:
        raise RuntimeError("AXES must be >= 3")

    global EXTRUDER_TOTAL
    EXTRUDER_TOTAL = int(config.find('FDM', 'EXTRUDERS', '1'))
    if EXTRUDER_TOTAL < 1:
        raise RuntimeError("EXTRUDERS must be >= 1")
        
    global NUM_FANS
    NUM_FANS = int(config.find('FDM','NUM_FANS', 4))
    system_fan = int(config.find('FDM','SYSTEM_FAN', 0))
    if (NUM_FANS < system_fan):
    	raise RuntimeError("NUM_FANS must be >= 1 if SYSTEM_FAN > 0")
    if (NUM_FANS > 4) or (NUM_FANS < 0 ):
        raise RuntimeError("NUM_FANS must be > 0 and < 5")

    rtapi.loadrt('tp')
    if config.find('MACHINE','DELTA_R') is not None:
        kinematics = 'lineardeltakins'
        rtapi.loadrt(kinematics)
        hal.Pin('lineardeltakins.L').set(config.find('MACHINE', 'CF_ROD'))
        hal.Pin('lineardeltakins.R').set(config.find('MACHINE', 'DELTA_R'))
    else:
        kinematics = 'trivkins'
        rtapi.loadrt(kinematics)
    
    rtapi.loadrt(config.find('EMCMOT', 'EMCMOT'), 
        servo_period_nsec=config.find('EMCMOT', 'SERVO_PERIOD'),
        num_joints=str(AXIS_TOTAL),
        num_aio=51,
        num_dio=21,
        kins = kinematics)

    hal.addf('motion-command-handler', SERVO_THREAD)
    hal.addf('motion-controller', SERVO_THREAD)

    estop_reset = hal.newsig('estop-reset', hal.HAL_BIT)
    (main_enable, main_enable_inv) = setup_enable_chain()

    if BOARD_REV == 'B3A':
        replicape = hardware.ReplicapeB3A()
    if BOARD_REV == 'A4A':
        replicape = hardware.ReplicapeA4A()
    watchdog_sigs.extend(replicape.get_watchdog_sigs())
    replicape.link_enable(main_enable, main_enable_inv)

    (joint_commands, joint_feedbacks) = setup_joints()
    for i in xrange(AXIS_TOTAL):
        setup_axis(replicape, joint_commands[i], joint_feedbacks[i], i)
    setup_limit_switches(replicape)

    extruder_sel_sig = hal.newsig('extruder-sel', hal.HAL_S32)
    setup_extruders(replicape, extruder_sel_sig)

    for i in xrange(EXTRUDER_TOTAL):
        t = ExtruderTemperature(replicape, i, main_enable, estop_reset)
        error_sigs.append(t.get_error_sig())
        watchdog_sigs.append(t.get_temp_watchdog_sig())

    if config.find('HBP', 'THERMISTOR'):
        t = HbpTemperature(replicape, main_enable, estop_reset)
        error_sigs.append(t.get_error_sig())
        watchdog_sigs.append(t.get_temp_watchdog_sig())

#    setup_system_fan(replicape)
    setup_fans(replicape)
    setup_estop(error_sigs, watchdog_sigs, estop_reset, SERVO_THREAD)
    connect_tool_changer()


