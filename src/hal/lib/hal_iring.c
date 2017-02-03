#include "config.h"
#include "hal_iring.h"

iring_t *hal_iring_alloc(const size_t size)
{
    iring_t *ip;
    size_t total_size = ring_memsize( RINGTYPE_RECORD, size, 0) + offsetof(iring_t,rh);
    HALDBG("size=%zu total_size=%zu", size, total_size);
    ip = shmalloc_desc_aligned(total_size, RTAPI_CACHELINE);
    if (ip == NULL) {
	HALFAIL_NULL(ENOMEM, "size %zu - insufficient HAL memory for ring",
		     total_size);
    }
    ringheader_init(&ip->rh, RINGTYPE_RECORD, size, 0);
    ringbuffer_init(&ip->rh, &ip->rb);
    return ip;
}

int hal_iring_free(iring_t **irp)
{
    if (irp && *irp) {
	shmfree_desc(*irp);
	*irp = NULL;
	return 0;
    } else {
	HALFAIL_RC(EINVAL, "BUG: invalid pointer: %p",
		     (void *) irp);
    }
}

