from .hal_priv cimport *
from .hal_group cimport *
from .hal_ring cimport *
from cpython.bool  cimport bool
from libc.stdint cimport uint8_t,uint32_t


cdef extern from "hal_object.h":

    ctypedef union hal_object_ptr:
        halhdr_t     *hdr
        hal_comp_t   *comp
        hal_inst_t   *inst
        hal_pin_t    *pin
        hal_param_t  *param
        hal_sig_t    *sig
        hal_group_t  *group
        hal_member_t *member
        hal_funct_t  *funct
        hal_thread_t *thread
        hal_vtable_t *vtable
        hal_ring_t   *ring
        void         *any

    ctypedef struct foreach_args_t:
        int type
        int id
        int owner_id
        int owning_comp
        char *name
        int user_arg1
        int user_arg2
        void *user_ptr1
        void *user_ptr2
        void *user_ptr3

    ctypedef int (*hal_object_callback_t) (hal_object_ptr object,  foreach_args_t *args)
    int halg_foreach(int use_hal_mutex,
                     foreach_args_t *args,
                     const hal_object_callback_t callback)
    hal_object_ptr halg_find_object_by_name(const int use_hal_mutex,
                                            const int type,
                                            const char *name)
    hal_object_ptr halg_find_object_by_id(const int use_hal_mutex,
                                          const int type,
                                          const int id)
    int hh_get_id(halhdr_t *h)
    int hh_get_owner_id(halhdr_t *h)
    int hh_get_object_type(halhdr_t *h)
    char *hh_get_name(halhdr_t *h)
    char *hh_get_object_typestr(halhdr_t *h)
    char *hal_object_typestr(const unsigned type)
    int hh_get_refcnt(const halhdr_t *o)
    int hh_incr_refcnt(halhdr_t *o)
    int hh_decr_refcnt(halhdr_t *o)
    int hh_snprintf(char *buf, size_t size, halhdr_t *hh)
    bool hh_is_valid(const halhdr_t *o)
    uint32_t hh_valid(const halhdr_t *o)
