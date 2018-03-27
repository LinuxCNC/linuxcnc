#!/usr/bin/python2
# encoding: utf-8

from machinekit import rtapi
from machinekit import hal
from machinekit import config
import os
import math

SERVO_THREAD = 'servo-thread'
USR_HAL_PATH = os.path.dirname(os.path.realpath(__file__)) + '/hal/'

class Temperature(object):
    def __init__(self, temp_hal, pwm_pin, on_pin, 
                 enable_sig, estop_reset_sig, 
                 config_name, comp_name, hal_name, motion_pin_number):
        self.temp_hal = temp_hal

        comp = hal.RemoteComponent(comp_name, timer=100)
        comp_limit_min_pin = comp.newpin('temp.limit.min', hal.HAL_FLOAT, hal.HAL_IN)
        comp_limit_max_pin = comp.newpin('temp.limit.max', hal.HAL_FLOAT, hal.HAL_IN)
        comp_standby_pin = comp.newpin('temp.standby', hal.HAL_FLOAT, hal.HAL_IN)
        comp_meas_pin = comp.newpin('temp.meas', hal.HAL_FLOAT, hal.HAL_IN)
        comp_set_pin = comp.newpin('temp.set', hal.HAL_FLOAT, hal.HAL_IO)
        comp_in_range_pin = comp.newpin('temp.in-range', hal.HAL_BIT, hal.HAL_IN)
        comp_active_pin = comp.newpin('active', hal.HAL_BIT, hal.HAL_IN)
        comp_error_pin = comp.newpin('error', hal.HAL_BIT, hal.HAL_IN)
        comp.ready()

        # -- General Signals --
        meas_sig = hal.newsig('%s.meas' % hal_name, hal.HAL_FLOAT)
        meas_sig.link(temp_hal.pin('value'))
        meas_sig.link(comp_meas_pin)
        meas_sig.link('motion.analog-in-%02d' % motion_pin_number)

        set_sig = hal.newsig('%s.set' % hal_name, hal.HAL_FLOAT)
        set_sig.link(comp_set_pin)
        set_sig.link('motion.analog-out-io-%02d' % motion_pin_number)

        limit_min_sig = hal.newsig('%s.limit.min' % hal_name, hal.HAL_FLOAT)
        limit_min_sig.link(comp_limit_min_pin)
        limit_min_sig.set(config.find(config_name, 'TEMP_LIMIT_MIN', 0.0))
        
        limit_max_sig = hal.newsig('%s.limit.max' % hal_name, hal.HAL_FLOAT)
        limit_max_sig.link(comp_limit_max_pin)
        limit_max_sig.set(config.find(config_name, 'TEMP_LIMIT_MAX', 30.0))

        standby_sig = hal.newsig('%s.standby' % hal_name, hal.HAL_FLOAT)
        standby_sig.link(comp_standby_pin)
        standby_sig.set(config.find(config_name, 'TEMP_STANDBY', 15.0))

        on_sig = hal.newsig('%s.on' % hal_name, hal.HAL_BIT)
        on_sig.link(on_pin)
        on_sig.link(comp_active_pin)

        error_sig = hal.newsig('%s.error' % hal_name, hal.HAL_BIT)
        self.error_sig = error_sig
        error_sig.link(comp_error_pin)

        self.temp_watchdog_sig = hal.newsig('%s.watchdog' % hal_name, hal.HAL_BIT)
        temp_hal.pin('watchdog').link(self.temp_watchdog_sig)

        # -- Measurement --

        # In-Range Signal
        in_range_sig = hal.newsig('%s.in-range' % hal_name, hal.HAL_BIT)
        in_range_sig.link(comp_in_range_pin)
        in_range_sig.link('motion.digital-in-%02d' % motion_pin_number)

        range_lb_sum = rtapi.newinst('sum2', '%s.range.lb.sum2' % hal_name)
        hal.addf(range_lb_sum.name, SERVO_THREAD)
        range_ub_sum = rtapi.newinst('sum2', '%s.range.ub.sum2' % hal_name)
        hal.addf(range_ub_sum.name, SERVO_THREAD)
        range_lb_sig = hal.newsig('%s.range.lb' % hal_name, hal.HAL_FLOAT)
        range_ub_sig = hal.newsig('%s.range.ub' % hal_name, hal.HAL_FLOAT)

        range_lb_sum.pin('in0').link(set_sig)
        range_lb_sum.pin('in1').set(float(config.find(config_name, 'TEMP_RANGE_NEG_ERROR', -1.0)))
        range_lb_sum.pin('out').link(range_lb_sig)
        range_ub_sum.pin('in0').link(set_sig)
        range_ub_sum.pin('in1').set(float(config.find(config_name, 'TEMP_RANGE_POS_ERROR', 1.0)))
        range_ub_sum.pin('out').link(range_ub_sig)

        range_wcomp = rtapi.newinst('wcomp', '%s.range.wcomp' % hal_name)
        hal.addf(range_wcomp.name, SERVO_THREAD)
        range_wcomp.pin('min').link(range_lb_sig)
        range_wcomp.pin('max').link(range_ub_sig)
        range_wcomp.pin('in').link(meas_sig)
        range_wcomp.pin('out').link(in_range_sig)

        # -- Output --
        pwm_raw_sig = hal.newsig('%s.pwm_raw' % hal_name, hal.HAL_FLOAT)
        pwm_sig = hal.newsig('%s.pwm' % hal_name, hal.HAL_FLOAT)
        pwm_max = float(config.find(config_name, 'PWM_MAX', 1.0))

        # PID
        pid = rtapi.newinst('at_pid', '%s.pid' % hal_name)
        hal.addf(pid.name + '.do-pid-calcs', SERVO_THREAD)
        pid.pin('enable').link(enable_sig)
        pid.pin('feedback').link(meas_sig)
        pid.pin('command').link(set_sig)
        pid.pin('output').link(pwm_raw_sig)
        pid.pin('maxoutput').set(pwm_max)
        pid.pin('Pgain').set(float(config.find(config_name, 'PID_PGAIN', 0)))
        pid.pin('Igain').set(float(config.find(config_name, 'PID_IGAIN', 0)))
        pid.pin('Dgain').set(float(config.find(config_name, 'PID_DGAIN', 0)))
        pid.pin('maxerrorI').set(float(config.find(config_name, 'PID_MAXERRORI', 1.0)))
        pid.pin('bias').set(float(config.find(config_name, 'PID_BIAS', 0.0)))

        # PWM Limit (PID can output negative values)
        pwm_limit = rtapi.newinst('limit1', '%s.limit1.pwm' % hal_name)
        hal.addf(pwm_limit.name, SERVO_THREAD)
        pwm_limit.pin('min').set(0.0)
        pwm_limit.pin('max').set(pwm_max)
        pwm_limit.pin('in').link(pwm_raw_sig)
        pwm_limit.pin('out').link(pwm_sig)

        pwm_pin.link(pwm_sig)

        # -- Safety Check --
        check_limit_ok_sig = hal.newsig('%s.check.limit-ok' % hal_name, hal.HAL_BIT)

        limit_wcomp = rtapi.newinst('wcomp', '%s.check.limit.wcomp' % hal_name)
        hal.addf(limit_wcomp.name, SERVO_THREAD)
        limit_wcomp.pin('min').link(limit_min_sig)
        limit_wcomp.pin('max').link(limit_max_sig)
        limit_wcomp.pin('in').link(meas_sig)
        limit_wcomp.pin('out').link(check_limit_ok_sig)

        out_range_sig = hal.newsig('%s.out-range' % hal_name, hal.HAL_BIT)
        out_range_not = rtapi.newinst('not', '%s.out-range.not' % hal_name)
        hal.addf(out_range_not.name, SERVO_THREAD)
        out_range_not.pin('in').link(in_range_sig)
        out_range_not.pin('out').link(out_range_sig)

        thermistor_check_enable_sig = hal.newsig('%s.check.therm.enable' % hal_name, hal.HAL_BIT)
        thermistor_check_enable_and = rtapi.newinst('and2', '%s.check.therm.enable.and2' % hal_name)
        hal.addf(thermistor_check_enable_and.name, SERVO_THREAD)
        thermistor_check_enable_and.pin('in0').link(enable_sig)
        thermistor_check_enable_and.pin('in1').link(out_range_sig)
        thermistor_check_enable_and.pin('out').link(thermistor_check_enable_sig)

        check_thermistor_ok_sig = hal.newsig('%s.check.therm-ok' % hal_name, hal.HAL_BIT)
        thermistor_check = rtapi.newinst('thermistor_check', '%s.check.therm-check' % hal_name)
        hal.addf(thermistor_check.name, SERVO_THREAD)
        thermistor_check.pin('enable').link(thermistor_check_enable_sig)
        thermistor_check.pin('pid').link(pwm_sig)
        thermistor_check.pin('temp').link(meas_sig)
        thermistor_check.pin('min-pid').set(float(config.find(config_name, 'CHECK_MIN_PID', 0.25)))
        thermistor_check.pin('min-temp').set(float(config.find(config_name, 'CHECK_MIN_TEMP', 2.0)))
        thermistor_check.pin('wait').set(float(config.find(config_name, 'CHECK_WAIT', 30.0)))
        thermistor_check.pin('no-error').link(check_thermistor_ok_sig)

        check_all_ok_sig = hal.newsig('%s.check.all-ok' % hal_name, hal.HAL_BIT)
        check_all_ok = rtapi.newinst('and2', '%s.check.all.and2' % hal_name)
        hal.addf(check_all_ok.name, SERVO_THREAD)
        check_all_ok.pin('in0').link(check_limit_ok_sig)
        check_all_ok.pin('in1').link(check_thermistor_ok_sig)
        check_all_ok.pin('out').link(check_all_ok_sig)

        check_error_sig = hal.newsig('%s.check.error' % hal_name, hal.HAL_BIT)
        check_error = rtapi.newinst('not', '%s.check.error.not' % hal_name)
        hal.addf(check_error.name, SERVO_THREAD)
        check_error.pin('in').link(check_all_ok_sig)
        check_error.pin('out').link(check_error_sig)

        error_latch = rtapi.newinst('flipflop', '%s.check.error.flipflop' % hal_name)
        hal.addf(error_latch.name, SERVO_THREAD)
        error_latch.pin('set').link(check_error_sig)
        error_latch.pin('reset').link(estop_reset_sig)
        error_latch.pin('out').link(error_sig)

    def get_temp_watchdog_sig(self):
        return self.temp_watchdog_sig

    def get_error_sig(self):
        return self.error_sig


class ExtruderTemperature(Temperature):
    def __init__(self, replicape, index, enable_sig, estop_reset_sig):
        config_name = 'EXTRUDER_%s' % (index)
        thermistor = config.find(config_name, 'THERMISTOR', '')
        if thermistor == '':
            raise ValueError('[%s] THERMISTOR must be defined' % (config_name))

        temp_hal = hal.loadusr(USR_HAL_PATH + 'hal_bbb_temp',
            name='Therm%s' % (index),
            wait_name='Therm%s' % (index),
            cape_board='Replicape',
            channel=replicape.get_extruder_adc_channel(index),
            thermistor=thermistor)

        super(ExtruderTemperature, self).__init__(
            temp_hal, replicape.get_extruder_pwm_pin(index), replicape.get_extruder_on_pin(index), 
            enable_sig, estop_reset_sig,
            config_name, 'fdm-e%s' % (index), 'temp.e%s' % (index), 
            index + 2
        )

class HbpTemperature(Temperature):
    def __init__(self, replicape, enable_sig, estop_reset_sig):
        config_name = 'HBP'
        thermistor = config.find(config_name, 'THERMISTOR', '')
        if thermistor == '':
            raise ValueError('[%s] THERMISTOR must be defined' % (config_name))

        temp_hal = hal.loadusr(USR_HAL_PATH + 'hal_bbb_temp',
            name='ThermHbp',
            wait_name='ThermHbp',
            cape_board='Replicape',
            channel=replicape.get_hbp_adc_channel(),
            thermistor=thermistor)

        super(HbpTemperature, self).__init__(
            temp_hal, replicape.get_hbp_pwm_pin(), replicape.get_hbp_on_pin(), 
            enable_sig, estop_reset_sig,
            config_name, 'fdm-hbp', 'temp.hbp', 0)
