#ifndef INTERNAL_RING_H
#define INTERNAL_RING_H

#include <rtapi.h>
#include <ring.h>
#include "hal.h"
#include "hal_priv.h"
#include "hal_internal.h"



RTAPI_BEGIN_DECLS

typedef struct internal_ring {
    ringbuffer_t rb;
    ringheader_t rh;
} iring_t;


iring_t *hal_iring_alloc(const size_t size);
int hal_iring_free(iring_t **irp);

RTAPI_END_DECLS

#endif /* INTERNAL_RING_H */
