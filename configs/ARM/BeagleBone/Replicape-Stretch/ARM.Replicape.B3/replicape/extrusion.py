#!/usr/bin/python2
# encoding: utf-8

from machinekit import rtapi
from machinekit import hal
from machinekit import config
import os
import math

SERVO_THREAD = 'servo-thread'

class Extruder(object):
    def __init__(self, nr, motion_vel_sig, config):
        self.nr = nr
        self._init_signals()
        self._init_extrude(motion_vel_sig)
        self._init_jog()
        self._init_config(config)

    def _init_signals(self):
        # Input signal
        self.enable_sig = hal.newsig('ve%d.enable' % self.nr, hal.HAL_BIT)
        self.cross_section_sig = hal.newsig('ve%d.cross-section' % self.nr, hal.HAL_FLOAT)

        self.jog_vel_sig = hal.newsig('ve%d.jog-vel' % self.nr, hal.HAL_FLOAT)
        self.jog_direction_sig = hal.newsig('ve%d.jog-direction' % self.nr, hal.HAL_BIT)
        self.jog_distance_sig = hal.newsig('ve%d.jog-distance' % self.nr, hal.HAL_FLOAT)
        self.jog_trigger_sig = hal.newsig('ve%d.jog-trigger' % self.nr, hal.HAL_BIT)
        self.jog_continuous_sig = hal.newsig('ve%d.jog-continuous' % self.nr, hal.HAL_BIT)
        self.jog_dtg_sig = hal.newsig('ve%d.jog-dtg' % self.nr, hal.HAL_FLOAT)

        # Intermediate signal
        self.jog_vel_output_sig = hal.newsig('ve%d.jog-vel-output' % self.nr, hal.HAL_FLOAT)

        # Parameter signal
        self.filament_dia_sig = hal.newsig('ve%d.filament-dia' % self.nr, hal.HAL_FLOAT)
        self.extrude_scale_sig = hal.newsig('ve%d.extrude-scale' % self.nr, hal.HAL_FLOAT)
        self.accel_gain_sig = hal.newsig('ve%d.accel-gain' % self.nr, hal.HAL_FLOAT)
        self.max_jog_vel_sig = hal.newsig('ve%d.max-jog-vel' % self.nr, hal.HAL_FLOAT)

        self.retract_vel_sig = hal.newsig('ve%d.retract-vel' % self.nr, hal.HAL_FLOAT)
        self.retract_len_sig = hal.newsig('ve%d.retract-len' % self.nr, hal.HAL_FLOAT)

        # Output signal
        self.extrude_vel_sig = hal.newsig('ve%d.extrude-vel' % self.nr, hal.HAL_FLOAT)
        self.retracting_sig = hal.newsig('ve%d.retracting' % self.nr, hal.HAL_BIT)

    def _init_extrude(self, motion_vel_sig):
        material_flowrate_sig = hal.newsig('ve%d.material-flowrate' % self.nr, hal.HAL_FLOAT)
        material_flowrate = rtapi.newinst('mult2', 've%d.mult2.material-flowrate' % self.nr)
        hal.addf(material_flowrate.name, SERVO_THREAD)
        material_flowrate.pin('in0').link(motion_vel_sig)
        material_flowrate.pin('in1').link(self.cross_section_sig)
        material_flowrate.pin('out').link(material_flowrate_sig)

        filament_dia_sq_sig = hal.newsig('ve%d.filament-dia-sq' % self.nr, hal.HAL_FLOAT)
        filament_dia_sq = rtapi.newinst('mult2', 've%d.mult2.filament-dia-sq' % self.nr)
        hal.addf(filament_dia_sq.name, SERVO_THREAD)
        filament_dia_sq.pin('in0').link(self.filament_dia_sig)
        filament_dia_sq.pin('in1').link(self.filament_dia_sig)
        filament_dia_sq.pin('out').link(filament_dia_sq_sig)

        filament_area_sig = hal.newsig('ve%d.filament-area' % self.nr, hal.HAL_FLOAT)
        filament_area = rtapi.newinst('mult2', 've%d.mult2.filament-area' % self.nr)
        hal.addf(filament_area.name, SERVO_THREAD)
        filament_area.pin('in0').link(filament_dia_sq_sig)
        filament_area.pin('in1').set(math.pi / 4)
        filament_area.pin('out').link(filament_area_sig)

        extrude_rate_sig = hal.newsig('ve%d.extrude-rate' % self.nr, hal.HAL_FLOAT)
        extrude_rate = rtapi.newinst('div2', 've%d.div2.extrude-rate' % self.nr)
        hal.addf(extrude_rate.name, SERVO_THREAD)
        extrude_rate.pin('in0').link(material_flowrate_sig)
        extrude_rate.pin('in1').link(filament_area_sig)
        extrude_rate.pin('out').link(extrude_rate_sig)

        extrude_rate_scaled_sig = hal.newsig('ve%d.extrude-rate-scaled' % self.nr, hal.HAL_FLOAT)
        extrude_rate_scaled = rtapi.newinst('mult2', 've%d.mult2.extrude-rate-scaled' % self.nr)
        hal.addf(extrude_rate_scaled.name, SERVO_THREAD)
        extrude_rate_scaled.pin('in0').link(extrude_rate_sig)
        extrude_rate_scaled.pin('in1').link(self.extrude_scale_sig)
        extrude_rate_scaled.pin('out').link(extrude_rate_scaled_sig)

        extrude_accel_sig = hal.newsig('ve%d.extrude-accel' % self.nr, hal.HAL_FLOAT)
        extrude_accel = rtapi.newinst('ddt', 've%d.ddt.extrude-accel' % self.nr)
        hal.addf(extrude_accel.name, SERVO_THREAD)
        extrude_accel.pin('in').link(extrude_rate_scaled_sig)
        extrude_accel.pin('out').link(extrude_accel_sig)

        extrude_accel_adj_sig = hal.newsig('ve%d.extrude-accel-adj' % self.nr, hal.HAL_FLOAT)
        extrude_accel_adj = rtapi.newinst('mult2', 've%d.mult2.extrude-accel-adj' % self.nr)
        hal.addf(extrude_accel_adj.name, SERVO_THREAD)
        extrude_accel_adj.pin('in0').link(extrude_accel_sig)
        extrude_accel_adj.pin('in1').link(self.accel_gain_sig)
        extrude_accel_adj.pin('out').link(extrude_accel_adj_sig)

        extrude_rate_comp_sig = hal.newsig('ve%d.extrude-rate-comp' % self.nr, hal.HAL_FLOAT)
        extrude_rate_comp = rtapi.newinst('sum2', 've%d.sum2.extrude-rate-comp' % self.nr)
        hal.addf(extrude_rate_comp.name, SERVO_THREAD)
        extrude_rate_comp.pin('in0').link(extrude_rate_scaled_sig)
        extrude_rate_comp.pin('in1').link(extrude_accel_adj_sig)
        extrude_rate_comp.pin('out').link(extrude_rate_comp_sig)

        retract_vel_neg_sig = hal.newsig('ve%d.retract-vel-neg' % self.nr, hal.HAL_FLOAT)
        retract_time_sig = hal.newsig('ve%d.retract-time' % self.nr, hal.HAL_FLOAT)

        retract_vel_neg = rtapi.newinst('neg', 've%d.neg.retract-vel-neg' % self.nr)
        hal.addf(retract_vel_neg.name, SERVO_THREAD)
        retract_vel_neg.pin('in').link(self.retract_vel_sig)
        retract_vel_neg.pin('out').link(retract_vel_neg_sig)

        retract_time = rtapi.newinst('div2', 've%d.div2.retract-time' % self.nr)
        hal.addf(retract_time.name, SERVO_THREAD)
        retract_time.pin('in0').link(self.retract_len_sig)
        retract_time.pin('in1').link(self.retract_vel_sig)
        retract_time.pin('out').link(retract_time_sig)
        
        retracting_delay = rtapi.newinst('oneshot', 've%d.oneshot.retracting' % self.nr)
        hal.addf(retracting_delay.name, SERVO_THREAD)
        retracting_delay.pin('rising').set(True)
        retracting_delay.pin('falling').set(True)
        retracting_delay.pin('retriggerable').set(True)
        retracting_delay.pin('width').link(retract_time_sig)
        retracting_delay.pin('in').link(self.enable_sig)
        retracting_delay.pin('out').link(self.retracting_sig)

        extrude_vel = rtapi.newinst('mux4', 've%d.mux4.extrude-vel' % self.nr)
        hal.addf(extrude_vel.name, SERVO_THREAD)
        extrude_vel.pin('sel0').link(self.retracting_sig)
        extrude_vel.pin('sel1').link(self.enable_sig)
        extrude_vel.pin('in0').link(self.jog_vel_output_sig)
        extrude_vel.pin('in1').link(retract_vel_neg_sig)
        extrude_vel.pin('in2').link(extrude_rate_comp_sig)
        extrude_vel.pin('in3').link(self.retract_vel_sig)
        extrude_vel.pin('out').link(self.extrude_vel_sig)

    def _init_jog(self):
        jog_vel_limited_sig = hal.newsig('ve%d.jog-vel-limited' % self.nr, hal.HAL_FLOAT)
        jog_vel_limit = rtapi.newinst('limit1', 've%d.limit1.jog-vel-limit' % self.nr)
        hal.addf(jog_vel_limit.name, SERVO_THREAD)
        jog_vel_limit.pin('in').link(self.jog_vel_sig)
        jog_vel_limit.pin('out').link(jog_vel_limited_sig)
        jog_vel_limit.pin('min').set(0.01)
        jog_vel_limit.pin('max').link(self.max_jog_vel_sig)

        jog_vel_limited_neg_sig = hal.newsig('ve%d.jog-vel-limited-neg' % self.nr, hal.HAL_FLOAT)
        jog_vel_limited_neg = rtapi.newinst('neg', 've%d.neg.jog-vel-limited-neg' % self.nr)
        hal.addf(jog_vel_limited_neg.name, SERVO_THREAD)
        jog_vel_limited_neg.pin('in').link(jog_vel_limited_sig)
        jog_vel_limited_neg.pin('out').link(jog_vel_limited_neg_sig)

        jog_vel_signed_sig = hal.newsig('ve%d.jog-vel-signed' % self.nr, hal.HAL_FLOAT)
        jog_vel_signed = rtapi.newinst('mux2', 've%d.mux2.jog-vel-signed' % self.nr)
        hal.addf(jog_vel_signed.name, SERVO_THREAD)
        jog_vel_signed.pin('in0').link(jog_vel_limited_sig)
        jog_vel_signed.pin('in1').link(jog_vel_limited_neg_sig)
        jog_vel_signed.pin('sel').link(self.jog_direction_sig)
        jog_vel_signed.pin('out').link(jog_vel_signed_sig)

        jog_time_sig = hal.newsig('ve%d.jog-time' % self.nr, hal.HAL_FLOAT)
        jog_time_left_sig = hal.newsig('ve%d.jog-time-left' % self.nr, hal.HAL_FLOAT)
        jog_time = rtapi.newinst('div2', 've%d.div2.jog-time' % self.nr)
        hal.addf(jog_time.name, SERVO_THREAD)
        jog_time.pin('in0').link(self.jog_distance_sig)
        jog_time.pin('in1').link(jog_vel_limited_sig)
        jog_time.pin('out').link(jog_time_sig)

        jog_single_sig = hal.newsig('ve%d.jog-single' % self.nr, hal.HAL_BIT)
        jog_single_oneshot = rtapi.newinst('oneshot', 've%d.oneshot.jog-single' % self.nr)
        hal.addf(jog_single_oneshot.name, SERVO_THREAD)
        jog_single_oneshot.pin('in').link(self.jog_trigger_sig)
        jog_single_oneshot.pin('width').link(jog_time_sig)
        jog_single_oneshot.pin('time-left').link(jog_time_left_sig)
        jog_single_oneshot.pin('rising').set(True)
        jog_single_oneshot.pin('falling').set(False)
        jog_single_oneshot.pin('retriggerable').set(1)
        jog_single_oneshot.pin('out').link(jog_single_sig)

        jog_reset_trigger = rtapi.newinst('reset', 've%d.reset.jog-trigger' % self.nr)
        hal.addf(jog_reset_trigger.name, SERVO_THREAD)
        jog_reset_trigger.pin('trigger').link(jog_single_sig)
        jog_reset_trigger.pin('rising').set(False)
        jog_reset_trigger.pin('falling').set(True)
        jog_reset_trigger.pin('reset-bit').set(False)
        jog_reset_trigger.pin('out-bit').link(self.jog_trigger_sig)
 
        jog_enable_sig = hal.newsig('ve%d.jog-enable' % self.nr, hal.HAL_BIT)
        jog_enable = rtapi.newinst('or2', 've%d.or2.jog-enable' % self.nr)
        hal.addf(jog_enable.name, SERVO_THREAD)
        jog_enable.pin('in0').link(self.jog_continuous_sig)
        jog_enable.pin('in1').link(jog_single_sig)
        jog_enable.pin('out').link(jog_enable_sig)

        jog_vel_output = rtapi.newinst('mux2', 've%d.mux2.jog-vel-output' % self.nr)
        hal.addf(jog_vel_output.name, SERVO_THREAD)
        jog_vel_output.pin('in0').set(0.0)
        jog_vel_output.pin('in1').link(jog_vel_signed_sig)
        jog_vel_output.pin('sel').link(jog_enable_sig)
        jog_vel_output.pin('out').link(self.jog_vel_output_sig)
        
        jog_dtg = rtapi.newinst('mult2', 've%d.mult2.jog-dtg' % self.nr)
        hal.addf(jog_dtg.name, SERVO_THREAD)
        jog_dtg.pin('in0').link(jog_vel_limited_sig)
        jog_dtg.pin('in1').link(jog_time_left_sig)
        jog_dtg.pin('out').link(self.jog_dtg_sig)

        disable1 = rtapi.newinst('reset', 've%d.reset.on-jog-single' % self.nr)
        hal.addf(disable1.name, SERVO_THREAD)
        disable1.pin('trigger').link(self.jog_trigger_sig)
        disable1.pin('rising').set(True)
        disable1.pin('falling').set(False)
        disable1.pin('retriggerable').set(True)
        disable1.pin('reset-bit').set(False)
        disable1.pin('out-bit').link(self.enable_sig)

        disable2 = rtapi.newinst('reset', 've%d.reset.on-jog-continuous' % self.nr)
        hal.addf(disable2.name, SERVO_THREAD)
        disable2.pin('trigger').link(self.jog_continuous_sig)
        disable2.pin('rising').set(True)
        disable2.pin('falling').set(False)
        disable2.pin('retriggerable').set(True)
        disable2.pin('reset-bit').set(False)
        disable2.pin('out-bit').link(self.enable_sig)
        
    def _init_config(self, config):
        configs = [
            (self.filament_dia_sig,   ['FILAMENT_DIAMETER', 'FILAMENT_DIA'],    '3'),
            (self.extrude_scale_sig,  ['EXTRUDE_SCALE'],                        '1'),
            (self.accel_gain_sig,     ['ACCELERATION_GAIN'],                    '0.05'),
            (self.max_jog_vel_sig,    ['MAX_JOG_VELOCITY', 'MAX_VELOCITY'],     '40'),
            (self.retract_len_sig,    ['RETRACT_LENGTH', 'RETRACT_LEN'],        '3'),
            (self.retract_vel_sig,    ['RETRACT_VELOCITY', 'RETRACT_VEL'],      '60')
        ]
        for i in configs:
            (signal, config_names, config_default) = i
            v = None
            for config_name in config_names:
                v = config.find('EXTRUDER_%d' % self.nr, config_name)
                if v is not None:
                    break
            if v is None:
                v = config_default
            if v is None:
                raise ValueError('Config [EXTRUDER_%d] %s must be defined' % (self.nr, config_names[0]))
            float_v = float(v)
            signal.set(float_v)

