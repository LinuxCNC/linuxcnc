from libc.stdint cimport uint64_t, uint8_t, uint32_t, int32_t
from libc.stddef cimport size_t
from nanopb cimport pb_field_t

cdef extern from "msginfo.h":

    ctypedef struct pbmsginfo_t:
        uint32_t msgid
        int32_t  encoded_size
        char *name
        size_t size
        pb_field_t *fields
        void *descriptor
        void *user_ptr
        uint32_t user_flags

    # msginfo.h
    pbmsginfo_t *pbmsgdesc_by_id(unsigned msgid)
    pbmsginfo_t *pbmsgdesc_by_name(char *name)
