# hal_ring.h definitions

from .hal cimport *
from .hal_priv cimport halhdr_t
from .ring cimport ringbuffer_t

cdef extern from "hal_ring.h":
    ctypedef struct hal_ring_t:
        halhdr_t hdr
        int ring_id
        int ring_shmkey
        int total_size
        unsigned ring_offset
        unsigned flags

    hal_ring_t *halg_ring_newf(const int use_hal_mutex,
                               const int size,
                               const int sp_size,
                               const int mode,
                               const char *fmt)
    int halg_ring_deletef(const int use_hal_mutex,const char *fmt)
    int halg_ring_attachf(const int use_hal_mutex,
                          ringbuffer_t *rb,
                          unsigned *flags,
                          const char *fmt)
    int halg_ring_detach(const int use_hal_mutex, ringbuffer_t *rb)
    # not part of public API. Use with HAL lock engaged.
    hal_ring_t *halpr_find_ring_by_name(const char *name)
