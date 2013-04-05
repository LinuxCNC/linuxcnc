# HAL ring example
#
# list existing rings
# attach to an existing ring and write a few messages
# create a new ring
# detach a ring

import ring

rings = ring.rings()
print "rings: ", rings

def print_ring(r):
    print "name=%s size=%d reader=%d writer=%d " % (name,r.size,r.reader,r.writer),
    print "use_rmutex=%d use_wmutex=%d stream=%d" % ( r.use_rmutex, r.use_wmutex,r.is_stream)

for name in rings:
    r = ring.attach(name)
    print_ring(r)
    print str(r)

    for i in range(10):
        msg = "message %d" % (i)
        r.write(msg,len(msg))
        print "available: ", r.available()

        #ring.detach(name)
    rings = ring.rings()
    print "leftover rings: ", rings

