
import py3hal as hal
import time
c = hal.component('component1')
try:
    c.new_signal('connection-bit', hal.BIT)
    c.new_pin('test-pin-bit-in',hal.BIT,hal.IN)
    c.new_pin('test-pin-bit-out',hal.BIT,hal.OUT)
    c.connect('component1.test-pin-bit-in', 'connection-bit')

    c.new_signal('connection-s32', hal.S32)
    c.new_pin('test-pin-s32-in',hal.S32,hal.IN)
    c.new_pin('test-pin-s32-out',hal.S32,hal.OUT)
    c.connect('component1.test-pin-s32-in', 'connection-s32')

    c.new_signal('connection-float', hal.FLOAT)
    c.new_pin('test-pin-float-in',hal.FLOAT,hal.IN)
    c.new_pin('test-pin-float-out',hal.FLOAT,hal.OUT)
    c.connect('component1.test-pin-float-in', 'connection-float')

    c.ready()
    print ('start')
    while 1:
        time.sleep(.5)
        c['test-pin-bit-out'].set(c['test-pin-bit-in'].get())
        c['test-pin-s32-out'].set(c['test-pin-s32-in'].get())
        c['test-pin-float-out'].set(c['test-pin-float-in'].get())
        print('test-pin-bit-out = ',c['test-pin-bit-out'].get())
        print('test-pin-s32-out = ',c['test-pin-s32-out'].get())
        print('test-pin-float-out = ',c['test-pin-float-out'].get())
except KeyboardInterrupt:
    c.exit()
    raise SystemExit
