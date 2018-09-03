#!/usr/bin/python2
# vim: sts=4 sw=4 et

# HAL internals introspection - the hal_data descriptor
# and shared memory heap

from machinekit import hal

hd = hal.HALData()
hd.heap_flags = -1 # log RTAPI malloc/free operations

print "hal_data layout version: %d" % hd.version
print "HAL mutex: %d" % hd.mutex
print "HAL lock: %d" % hd.lock
print "shmem_bot: %d" % hd.shmem_bot
print "shmem_top: %d" % hd.shmem_top

hs = hd.heap_status
print "HAL heap: total_avail=%d fragments=%d largest=%d" % hs

print "HAL heap free list:"
for chunk in hd.heap_freelist:
    print "chunk size %d at %x" % (chunk[0],chunk[1])

# allocate a few HAL objects
c = hal.Component('zzz')
p = c.newpin("p0", hal.HAL_S32, hal.HAL_OUT, init=42)
o0 = c.newpin("p1", hal.HAL_S32, hal.HAL_IN)
o1 = c.newpin("p2", hal.HAL_S32, hal.HAL_IN)
#s = hal.newsig("signal", hal.HAL_S32)
c.ready()

hs = hd.heap_status
print "HAL heap post usage: total_avail=%d fragments=%d largest=%d" % hs
print "HAL heap free list:"
for chunk in hd.heap_freelist:
    print "chunk size %d at %x" % (chunk[0],chunk[1])

c.exit()
hs = hd.heap_status
print "HAL heap after comp exit: total_avail=%d fragments=%d largest=%d" % hs
print "HAL heap free list:"
for chunk in hd.heap_freelist:
    print "chunk size %d at %x" % (chunk[0],chunk[1])

hd.heap_flags = 0 # stop heap logging
