#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "ring.h"



int main()
{
	ringbuffer_t ring1, ring2;
	ringbuffer_t *ro = &ring1, *rw = &ring2;
	ring_init(ro, 1024, 0);
	ring_init(rw, 0, ro->header);

	ring_dump(ro, "ro"); ring_dump(rw, "rw");

	ring_write(rw, "test", 4);
	ring_dump(ro, "ro"); ring_dump(rw, "rw");

	ring_write(rw, "test", 0);
	ring_dump(ro, "ro"); ring_dump(rw, "rw");

	ring_shift(ro);
	ring_dump(ro, "ro"); ring_dump(rw, "rw");

	ring_shift(ro);
	ring_dump(ro, "ro"); ring_dump(rw, "rw");

	exit(0);
}


#if 0
Ring ro is empty
Ring rw is empty

Data in ro: 4 test
Data in rw: 4 test

Data in ro: 4 test
Data in rw: 4 test

Data in ro: 0
Data in rw: 0

Ring ro is empty
Ring rw is empty

#endif
