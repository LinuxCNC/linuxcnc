#!/usr/bin/python2
# encoding: utf-8

from machinekit import rtapi
from machinekit import hal
from machinekit import config
import os

SERVO_THREAD = 'servo-thread'
USR_HAL_PATH = os.path.dirname(os.path.realpath(__file__)) + '/hal/'

class Replicape(object):
    def __init__(self):
        self.pru = rtapi.loadrt('hal_pru_generic', 
            pru=0, num_stepgens=5, num_pwmgens=0, halname='hpg',
            prucode='%s/rt-preempt/pru_generic.bin' % (config.Config().EMC2_RTLIB_DIR))
# 4.14.18-ti-rt-r33 kernel and rt-preempt
        hal.addf('hpg.capture-position', SERVO_THREAD)
        hal.addf('hpg.update', SERVO_THREAD)
        hal.addf('bb_gpio.read', SERVO_THREAD)
        hal.addf('bb_gpio.write', SERVO_THREAD)
        minvel = config.find('TRAJ','MIN_VELOCITY', 0.001)
        for i in xrange(5):
            self.get_pru_pin('stepgen.%02i.dirsetup' % i).set(200)
            self.get_pru_pin('stepgen.%02i.dirhold' % i).set(200)
            self.get_pru_pin('stepgen.%02i.steplen' % i).set(1000)
            self.get_pru_pin('stepgen.%02i.stepspace' % i).set(1000)
            self.get_pru_pin('stepgen.%02i.dirpin' % i).set(self.pru_dir_pin(i))
            self.get_pru_pin('stepgen.%02i.steppin' % i).set(self.pru_step_pin(i))
# setting to zero: pru_generic adapts to maximum velocity and acceleration
# see http://www.machinekit.io/docs/man/man9/hal_pru_generic/           
            self.get_pru_pin('stepgen.%02i.maxvel' % i).set(0)
            self.get_pru_pin('stepgen.%02i.maxaccel' % i).set(0)
            
# use new pru stepgen minvel pin to avoid pru hunting problem (without PID loop)
# see this discussion https://groups.google.com/forum/#!topic/machinekit/ATEwvfgoIb4
# except for extruder(s)
            if i < 3 :
                self.get_pru_pin('stepgen.%02i.minvel' % i).set(minvel)

        self.pwm = hal.loadusr(USR_HAL_PATH + 'hal_replicape_pwm',
            name='replicape_pwm',
            wait_name='replicape_pwm')

        self.watchdog_sigs = []
        for pin in self.get_watchdog_pins():
            s = hal.newsig('replicape.watchdog.%d' % len(self.watchdog_sigs), hal.HAL_BIT)
            pin.link(s)
            self.watchdog_sigs.append(s)

    def get_pru_pin(self, pin_name):
        return hal.Pin('hpg.%s' % pin_name)

    def get_gpio_pin(self, pin_name):
        return hal.Pin('bb_gpio.%s' % pin_name)

    def pru_step_pin(self, nr):
        if nr == 0:
            return 817
        if nr == 1:
            return 812
        if nr == 2:
            return 813
        if nr == 3:
            return 912
        if nr == 4:
            return 811
        raise ValueError('Axis out of range')

    def pru_dir_pin(self, nr):
        if nr == 0:
            return 826
        if nr == 1:
            return 819
        if nr == 2:
            return 814
        if nr == 3:
            return 815
        if nr == 4:
            return 816
        raise ValueError('Axis out of range')

    def link_enable(self, enable_sig, enable_inv_sig):
        raise NotImplementedError()

    def set_motor_current(self, port, current):
        raise NotImplementedError()

    def get_watchdog_pins(self):
        raise NotImplementedError()

    def get_watchdog_sigs(self):
        return self.watchdog_sigs

    def link_to_pwm(self, port, signal):
        self.pwm.pin('%i.out' % port).link(signal)

    def get_stepgen(self, port):
        return 'stepgen.%02i' % self.get_motor_port(port)

    def get_motor_port(self, port):
        port = str(port)
        if port == 'X' or port == '0':
            return 0
        if port == 'Y' or port == '1':
            return 1
        if port == 'Z' or port == '2':
            return 2
        if port == 'E' or port == 'E1' or port == '3':
            return 3
        if port == 'H' or port == 'E2' or port == '4':
            return 4
        raise ValueError('Port %s is unknown' % port)

    def get_limit_pin(self, axis, is_max):
        raise NotImplementedError()

    def get_probe_pin(self):
        raise NotImplementedError()

    def get_fan_pwm_pin(self, index):
        raise NotImplementedError()

    def get_fan_on_pin(self, index):
        raise NotImplementedError()

    def get_extruder_pwm_pin(self, index):
        raise NotImplementedError()

    def get_extruder_on_pin(self, index):
        raise NotImplementedError()

    def get_hbp_pwm_pin(self):
        raise NotImplementedError()

    def get_hbp_on_pin(self):
        raise NotImplementedError()

    def get_extruder_adc_channel(self, index):
        if index == 0: return 4
        if index == 1: return 5
        raise ValueError('index must be 0 to 1')
    
    def get_hbp_adc_channel(self):
        return 6

class ReplicapeB3A(Replicape):
    def __init__(self):
        self.gpio = rtapi.loadrt('hal_bb_gpio', 
            output_pins='941', 
            input_pins='810,809,924,818,923,925,928,918,911,913')

        self.hwconfig = hal.loadusr(USR_HAL_PATH + 'hal_replicape_B3_hwconfig',
            name='replicape_hwconfig',
            wait_name='replicape_hwconfig')

        for i in xrange(5):
            # Default: spreadMode, microstepping=1/16
            self.hwconfig.pin('stepper.%i.mode' % i).set(config.find('FDM','STEPPER_%i_MODE' % i, 0x80))

        super(ReplicapeB3A, self).__init__()

    def link_enable(self, enable_sig, enable_inv_sig):
        self.get_gpio_pin('p9.out-41').link(enable_inv_sig)
        self.hwconfig.pin('enable').link(enable_sig)
        self.pwm.pin('enable').link(enable_sig)
        for i in xrange(5):
            self.get_pru_pin('stepgen.%02i.enable' % i).link(enable_sig)

    def set_motor_current(self, port, current):
        if port < 0 or port > 4:
            raise ValueError('Port must be 0 to 4')
        if current < 0 or current > 1.5:
            raise ValueError('Current must be 0 to 1.5')
        value = current / 3.84
        self.pwm.pin('%i.out' % (port + 11)).set(value)

    def get_watchdog_pins(self):
        return [self.hwconfig.pin('watchdog'), self.pwm.pin('watchdog')]

    def get_limit_pin(self, axis, is_max):
        # Return END_STOP_*_1 pins in all situations
        if axis == 'X': return self.get_gpio_pin('p9.in-25')
        if axis == 'Y': return self.get_gpio_pin('p9.in-23')
        if axis == 'Z': return self.get_gpio_pin('p9.in-13')
        raise NotImplementedError()

    def get_probe_pin(self):
        return self.get_gpio_pin('p9.in-18')

    def get_fan_pwm_pin(self, index):
        if index < 0 or index > 3:
            raise ValueError('index must be 0 to 3')
        return self.pwm.pin('%i.out' % (index + 7))

    def get_fan_on_pin(self, index):
        if index < 0 or index > 3:
            raise ValueError('index must be 0 to 3')
        return self.pwm.pin('%i.on' % (index + 7))

    def get_extruder_pwm_pin(self, index):
        if index == 0: return self.pwm.pin('5.out')
        if index == 1: return self.pwm.pin('3.out')
        raise ValueError('index must be 0 to 1')

    def get_extruder_on_pin(self, index):
        if index == 0: return self.pwm.pin('5.on')
        if index == 1: return self.pwm.pin('3.on')
        raise ValueError('index must be 0 to 1')

    def get_hbp_pwm_pin(self):
        return self.pwm.pin('4.out')

    def get_hbp_on_pin(self):
        return self.pwm.pin('4.on')

class ReplicapeA4A(Replicape):
    def __init__(self):
        self.gpio = rtapi.loadrt('hal_bb_gpio', 
            output_pins='', 
            input_pins='810,809,924,818,923,925,916,918,911,913')

        self.dac = hal.loadusr(USR_HAL_PATH + 'hal_replicape_dac',
            name='replicape_dac',
            wait_name='replicape_dac')

        self.hwconfig = hal.loadusr(USR_HAL_PATH + 'hal_replicape_A4_hwconfig',
            name='replicape_hwconfig',
            wait_name='replicape_hwconfig')

        for i in xrange(5):
            # Default: microstepping=1/32, fast decay
            self.hwconfig.pin('stepper.%i.microstepping' % i).set(config.find('FDM','STEPPER_%i_MICROSTEPPING' % i, 5))
            self.hwconfig.pin('stepper.%i.decay' % i).set(config.find('FDM','STEPPER_%i_DECAY' % i, 0))

        super(ReplicapeA4A, self).__init__()

    def link_enable(self, enable_sig, enable_inv_sig):
        self.dac.pin('enable').link(enable_sig)
        self.hwconfig.pin('enable').link(enable_sig)
        self.pwm.pin('enable').link(enable_sig)
        for i in xrange(5):
            self.get_pru_pin('stepgen.%02i.enable' % i).link(enable_sig)

    def set_motor_current(self, port, current):
        if port < 0 or port > 4:
            raise ValueError('Port must be 0 to 4')
        if current < 0 or current > 1.5:
            raise ValueError('Current must be 0 to 1.5')
        self.dac.pin('%i.current' % port).set(current)

    def get_watchdog_pins(self):
        return [self.hwconfig.pin('watchdog'), self.pwm.pin('watchdog'), self.dac.pin('watchdog')]

    def get_limit_pin(self, axis, is_max):
        if axis == 'X' and not is_max: return self.get_gpio_pin('p9.in-25')
        if axis == 'X' and is_max: return self.get_gpio_pin('p9.in-11')
        if axis == 'Y' and not is_max: return self.get_gpio_pin('p9.in-23')
        if axis == 'Y' and is_max: return self.get_gpio_pin('p9.in-16')
        if axis == 'Z' and not is_max: return self.get_gpio_pin('p9.in-13')
        if axis == 'Z' and is_max: return self.get_gpio_pin('p9.in-18')
        raise NotImplementedError()

    def get_probe_pin(self):
        return None

    def get_fan_pwm_pin(self, index):
        if index == 3 or index == 4: return self.pwm.pin('%i.out' % (index - 3 + 14))
        if index < 3: return self.pwm.pin('%i.out' % (index + 8))
        if index == 5: return self.pwm.pin('%i.out' % (7))
        raise ValueError('index must be 0 to 5')

    def get_fan_on_pin(self, index):
        if index == 3 or index == 4: return self.pwm.pin('%i.on' % (index - 3 + 14))
        if index < 3: return self.pwm.pin('%i.on' % (index + 8))
        if index == 5: return self.pwm.pin('%i.on' % (7))
        raise ValueError('index must be 0 to 5')

    def get_extruder_pwm_pin(self, index):
        if index == 0: return self.pwm.pin('5.out')
        if index == 1: return self.pwm.pin('3.out')
        raise ValueError('index must be 0 to 1')

    def get_extruder_on_pin(self, index):
        if index == 0: return self.pwm.pin('5.on')
        if index == 1: return self.pwm.pin('3.on')
        raise ValueError('index must be 0 to 1')

    def get_hbp_pwm_pin(self):
        return self.pwm.pin('4.out')

    def get_hbp_on_pin(self):
        return self.pwm.pin('4.on')
