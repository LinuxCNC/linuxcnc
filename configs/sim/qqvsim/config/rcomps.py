from machinekit import hal

hal.epsilon[1] = 0.1

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
