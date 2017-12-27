#!/usr/bin/python2
# vim: sts=4 sw=4 et

# HAL internals introspection - the hal_data descriptor
# and shared memory heap

from machinekit import hal

hd = hal.HALData()
hd.heap_flags = -1 # log RTAPI malloc/free operations

hs = hd.heap_status
print "HAL heap: total_avail=%d fragments=%d largest=%d" % hs

for chunk in hd.heap_freelist:
    print "chunk size %d at %x" % (chunk[0],chunk[1])
