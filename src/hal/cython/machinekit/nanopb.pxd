from libc.stdint cimport uint64_t, uint8_t, uint32_t, int32_t
from libc.stddef cimport size_t
from libcpp cimport bool


cdef extern from "pb_decode.h":

    ctypedef struct pb_istream_t:
        size_t bytes_left
        char *errmsg

    ctypedef struct pb_ostream_t:
        size_t max_size
        size_t bytes_written
        char *errmsg
        void *state

    # NB: this assumes defined(PB_FIELD_32BIT) !
    ctypedef struct pb_field_t:
        uint32_t tag
        uint8_t  type
        uint32_t data_offset
        int32_t  size_offset
        uint32_t data_size
        uint32_t array_size
        void *ptr

    # pb_decode.h
    bint pb_decode(pb_istream_t *stream, pb_field_t *fields, void *dest_struct)
    bint pb_decode_noinit(pb_istream_t *stream, pb_field_t *fields, void *dest_struct)
    bint pb_decode_noinit_delimited(pb_istream_t *stream, pb_field_t *fields, void *dest_struct)
    pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize)

    # pb_encode.h
    bint pb_encode(pb_ostream_t *stream, pb_field_t *fields, void *src_struct)
    bint pb_encode_delimited(pb_ostream_t *stream, pb_field_t *fields,  void *src_struct)
    bint pb_get_encoded_size(size_t *size, pb_field_t *fields, void *src_struct)
    pb_ostream_t pb_ostream_from_buffer(uint8_t *buf, size_t bufsize)


cdef extern from "pb.h":
    char  *PB_GET_ERROR(pb_istream_t *stream)
