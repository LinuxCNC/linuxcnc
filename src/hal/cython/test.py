
import py3hal as hal
import time
c = hal.component('component1')
try:
    c.new_signal('connection1', hal.BIT)
    c.new_pin('test-pin-in',hal.BIT,hal.IN)
    c.set_prefix('qwerty')
    c.new_pin('test-pin-out',hal.BIT,hal.OUT)

    c.connect('component1.test-pin-in', 'connection1')
    c.connect('qwerty.test-pin-out', 'connection1')

    c.ready()
    while 1:
        time.sleep(1)
except KeyboardInterrupt:
    c.exit()
    raise SystemExit
