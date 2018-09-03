from machinekit import hal

hal.epsilon[1] = 0.1


def create_temperature_rcomp(name, timer=100):
    comp = hal.RemoteComponent('fdm-%s' % name, timer=timer)
    comp.newpin('temp.meas', hal.HAL_FLOAT, hal.HAL_IN, eps=1)
    comp.newpin('temp.set', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('temp.standby', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('temp.limit.min', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('temp.limit.max', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('temp.in-range', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('error', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('active', hal.HAL_BIT, hal.HAL_IN)
    comp.ready()

    comp.pin('temp.meas').link('%s-temp-meas' % name)
    comp.pin('temp.set').link('%s-temp-set' % name)
    comp.pin('temp.standby').link('%s-temp-standby' % name)
    comp.pin('temp.limit.min').link('%s-temp-limit-min' % name)
    comp.pin('temp.limit.max').link('%s-temp-limit-max' % name)
    comp.pin('temp.in-range').link('%s-temp-in-range' % name)
    comp.pin('error').link('%s-error' % name)
    comp.pin('active').link('%s-active' % name)


def create_fan_rcomp(name, timer=100):
    comp = hal.RemoteComponent('fdm-%s' % name, timer=timer)
    comp.newpin('set', hal.HAL_FLOAT, hal.HAL_IO)
    comp.ready()

    comp.pin('set').link('%s-set' % name)


def create_light_rcomp(name, timer=100):
    comp = hal.RemoteComponent('fdm-%s' % name, timer=timer)
    comp.newpin('r', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('g', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('b', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('w', hal.HAL_FLOAT, hal.HAL_IO)
    comp.ready()

    comp.pin('r').link('%s-r' % name)
    comp.pin('g').link('%s-g' % name)
    comp.pin('b').link('%s-b' % name)
    comp.pin('w').link('%s-w' % name)


def create_ve_jog_rcomp(extruders, timer=100):
    name = 'fdm-ve-jog'
    comp = hal.RemoteComponent(name, timer=timer)
    comp.newpin('distance', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('velocity', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('direction', hal.HAL_BIT, hal.HAL_IO)
    comp.newpin('trigger', hal.HAL_BIT, hal.HAL_IO)
    comp.newpin('continuous', hal.HAL_BIT, hal.HAL_IO)
    comp.newpin('dtg', hal.HAL_FLOAT, hal.HAL_IN, eps=1)
    comp.newpin('max-velocity', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('extruder-count', hal.HAL_U32, hal.HAL_IN)
    comp.newpin('extruder-sel', hal.HAL_S32, hal.HAL_IN)
    comp.ready()

    comp.pin('distance').link('ve-jog-distance')
    comp.pin('velocity').link('ve-jog-velocity')
    comp.pin('direction').link('ve-jog-direction')
    comp.pin('trigger').link('ve-jog-trigger')
    comp.pin('continuous').link('ve-jog-continuous')
    comp.pin('dtg').link('ve-jog-dtg')
    comp.pin('max-velocity').link('ve-max-velocity')
    comp.pin('extruder-count').set(extruders)
    comp.pin('extruder-sel').link('extruder-sel')


def create_ve_params_rcomp(timer=100):
    name = 'fdm-ve-params'
    comp = hal.RemoteComponent(name, timer=timer)
    comp.newpin('filament-dia', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('retract-vel', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('retract-len', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('extrude-scale', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('accel-adj-gain', hal.HAL_FLOAT, hal.HAL_IO)
    comp.ready()

    comp.pin('filament-dia').link('ve-filament-dia')
    comp.pin('retract-vel').link('ve-retract-vel')
    comp.pin('retract-len').link('ve-retract-len')
    comp.pin('extrude-scale').link('ve-extrude-scale')
    comp.pin('accel-adj-gain').link('ve-extrude-accel-adj-gain')


def create_gantry_rcomp(axisIndex, timer=100):
    name = 'gantry-config'
    comp = hal.RemoteComponent(name, timer=timer)
    comp.newpin('offset-left', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('offset-right', hal.HAL_FLOAT, hal.HAL_IO)
    comp.ready()

    comp.pin('offset-left').link('home-offset-%i-0' % axisIndex)
    comp.pin('offset-right').link('home-offset-%i-1' % axisIndex)


def create_pid_rcomp(name, timer=100):
    comp = hal.RemoteComponent(name, timer=timer)
    comp.newpin('Pgain', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('Igain', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('Dgain', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('maxerrorI', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('bias', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('max', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('min', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('command', hal.HAL_FLOAT, hal.HAL_IO)
    comp.newpin('feedback', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('output', hal.HAL_FLOAT, hal.HAL_IN)
    comp.ready()

    # TODO link to something
