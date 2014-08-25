cdef extern from "ring.h":
    ctypedef enum  ring_type_t:
        RINGTYPE_RECORD
        RINGTYPE_MULTIPART
        RINGTYPE_STREAM
        RINGTYPE_MASK

    ctypedef enum ring_mode_flags_t:
        USE_RMUTEX
        USE_WMUTEX
        ALLOC_HALMEM
