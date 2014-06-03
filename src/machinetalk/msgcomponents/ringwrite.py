# HAL ring example
# run as:
#   halrun -I recordread.hal
#   python ringwrite..py
#
# create new rings
# inspect rings
# attach to an existing ring and write a few messages

import os, time
from machinekit import hal

# print ring properties
def print_ring(r):
    print "name=%s size=%d reader=%d writer=%d scratchpad=%d" % (r.name,r.size,r.reader,r.writer,r.scratchpad_size),
    print "use_rmutex=%d use_wmutex=%d type=%d in_halmem=%d" % (r.rmutex_mode, r.wmutex_mode,r.type,r.in_halmem)

halring = hal.Ring("halmemring%d" % os.getpid(), size=4096, in_halmem=True)
shmring = hal.Ring("shmsegring%d" % os.getpid(), size=4096)

print_ring(halring)
print_ring(shmring)

# retrieve list of ring names
rings = hal.rings()
print "rings: ", rings

if "ring_0" in rings:

    # attach to existing ring
    w = hal.Ring("ring_0")

    # see what we have
    print_ring(w)

    if w.writer == 0: # not in use write-side

        # mark as 'writer side in use by <pid>'
        w.writer = os.getpid()
        print "max msgsize: ", w.available
        # push a few messages down the ring
        for i in range(10):
            msg = "message %d" % (i)
            w.write(msg)

        # see what's left
        print "max msgsize: ", w.available

    # give some time so rt comp may write something to scratchpad
    time.sleep(1)

    # investigate scratchpad region if one is defined
    if w.scratchpad_size:
        print "scratchpad:%d = '%s'" % (w.scratchpad_size, w.scratchpad.tobytes())

    # since we're about to detach the ring, clear the writer id
    w.writer = 0
