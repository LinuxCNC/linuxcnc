#ifndef HAL_RING_H
#define HAL_RING_H

#include <rtapi.h>
#include "rtapi_ring.h"	


RTAPI_BEGIN_DECLS

typedef struct {
    char name[HAL_NAME_LEN + 1];  // ring HAL name
    int next_ptr;		 // next ring in free list
    int ring_id;                 // RTAPI ring handle as returned by rtapi_ring_new
    int owner;                   // creating HAL module 
} hal_ring_t;

// some components use a fifo and a scratchpad shared memory area,
// like sampler.c and streamer.c. ringbuffer_t supports this through
// the optional scratchpad, which is created if spsize is > 0
// on hal_ring_new(). The scratchpad size is recorded in
// ringheader_t.scratchpad_size.

// generic ring methods for all modes:

/* create a named ringbuffer, owned by comp module_id
 * mode is one of: MODE_RECORD, MODE_STREAM
 * optionally or'd with USE_RMUTEX, USE_WMUTEX
 * spsize > 0 will allocate a shm scratchpad buffer
 * accessible through ringbuffer_t.scratchpad/ringheader_t.scratchpad
 */
int hal_ring_new(const char *name, int size, int spsize, int module_id, int mode);

/* delete a named ringbuffer */
int hal_ring_detach(const char *name);

/* make an existing ringbuffer accessible to a component
 * rb must point to storage of type ringbuffer_t
 */
int hal_ring_attach(const char *name, ringbuffer_t *rb, int module_id);


RTAPI_END_DECLS

#endif /* HAL_RING_PRIV_H */
