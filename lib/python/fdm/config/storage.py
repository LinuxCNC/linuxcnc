from machinekit import hal


def init_storage(fileName):
    # Python user-mode HAL module for storing values
    hal.loadusr('hal_storage', name='storage', file=fileName,
                autosave=True, wait_name='storage')


def read_storage():
    hal.Pin('storage.read-trigger').set(True)  # trigger read


def setup_gantry_storage(gantryAxis, gantryJoint):
    hal.Pin('storage.gantry.home-offset-%i-%i' % (gantryAxis, gantryJoint)) \
       .link('home-offset-%i-%i' % (gantryAxis, gantryJoint))


def setup_ve_storage(extruders):
    for n in range(0, extruders):
        hal.Pin('storage.e%i.retract-vel' % n).link('ve-retract-vel-e%i' % n)
        hal.Pin('storage.e%i.retract-len' % n).link('ve-retract-len-e%i' % n)
        hal.Pin('storage.e%i.filament-dia' % n).link('ve-filament-dia-e%i' % n)
        hal.Pin('storage.e%i.jog-velocity' % n).link('ve-jog-velocity-e%i' % n)
        hal.Pin('storage.e%i.extrude-scale' % n).link('ve-extrude-scale-e%i' % n)


def setup_light_storage(name):
    for color in ('r', 'g', 'b', 'w'):
        hal.Pin('storage.%s.%s' % (name, color)).link('%s-%s' % (name, color))
