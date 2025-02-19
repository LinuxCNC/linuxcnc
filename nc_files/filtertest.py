import time
import sys

for i in range(0,100):
    try:
        # simulate calculation time
        time.sleep(.1)

        # output a line of gcode
        print('G0 X1', file=sys.stdout)

        # update progress
        print('FILTER_PROGRESS={}'.format(i), file=sys.stderr)
    except:
        # This causes an error message
        print('Error; But this was only a test', file=sys.stderr)
        raise SystemExit(1)

print('M2', file=sys.stdout)
