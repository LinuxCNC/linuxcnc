# example: track jog positions during pause

import time
import sys
import linuxcnc

s = linuxcnc.stat()
s.poll()
previous_state = s.pause_state

names = ("RUNNING", "PAUSING", "PAUSED", "PAUSED_IN_OFFSET", "JOGGING", "JOG_ABORTING", "RETURNING", "PAUSING_FOR_STEP")

while True:
    try:
        s.poll() # get current values
    except linuxcnc.error, detail:
        print "error", detail
        sys.exit(1)

    # detect change in s.pause_state
    if s.pause_state !=  previous_state:
        print names[previous_state], "->",names[s.pause_state]

        if s.pause_state == linuxcnc.PS_PAUSED:
            print "stopped at initial pause position:", s.actual_position

        if s.pause_state == linuxcnc.PS_PAUSED_IN_OFFSET:
            print "stopped at offset:", s.actual_position
            
        previous_state = s.pause_state

    time.sleep(0.2)
