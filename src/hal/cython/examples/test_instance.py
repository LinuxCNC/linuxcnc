#!/usr/bin/python2

from machinekit import hal


for name in hal.instances():
    i = hal.instances[name]

    print str(i) # really just the header info

    # HAL common object header fields
    print "hdr=", i.strhdr
    print i.name, i.object_type, i.strtype, i.id, i.owner_id,  i.refcnt , i.valid

    # instance-specific fields
    print "size=",i.size,
    print "owner=",i.owner.name
    print "blob size:",i.size
    print "blob.tobytes()=",i.blob.tobytes()
    print "blob.tolist()=",i.blob.tolist()

    # pins owned by this instance:
    for p in i.pins:
        print p.name, p.type, p.id, p.owner_id,  p.refcnt , p.valid, str(p)
