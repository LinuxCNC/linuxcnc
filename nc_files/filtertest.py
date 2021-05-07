import time
import sys

for i in range(0,100):
    try:
        # simulate calculation time
        time.sleep(.1)

        # output a line of gcode
        print >>sys.stdout, 'G0 X1'

        # update progress
        print >>sys.stderr, 'FILTER_PROGRESS={}'.format(i)
    except:
        # This causes an error message
        print >>sys.stderr, 'Error; But this was only a test'
        raise SystemExit(1)




