from libc.stdint cimport uint32_t

cdef extern from "multiframe_flag.h":

    ctypedef enum mf_encoding_t:
        MF_UNKNOWN
        MF_PROTOBUF
        MF_NPB_CSTRUCT
        MF_RESERVED1
        MF_RESERVED2
        MF_RESERVED3
        MF_CUSTOM1
        MF_CUSTOM2

    ctypedef struct mfields_t:
        uint32_t msgid
        uint32_t format
        uint32_t more
        uint32_t eor
        uint32_t unused

    ctypedef union mflag_t:
        uint32_t u
        mfields_t f
