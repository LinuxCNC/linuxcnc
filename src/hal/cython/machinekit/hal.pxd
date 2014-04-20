# vim: sts=4 sw=4 et

# Copyright Pavel Shramov, 4/2014
# see http://psha.org.ru/cgit/psha/emc2.git/commit/?h=wip-cython
# License: MIT

cdef extern from "hal.h":
    int hal_init(const char *name)
    int hal_exit(int comp_id)
    int hal_ready(int comp_id)
    const char * hal_comp_name(int comp_id)

    void * hal_malloc(long size)

    ctypedef enum hal_type_t:
        HAL_TYPE_UNSPECIFIED
        HAL_BIT
        HAL_FLOAT
        HAL_S32
        HAL_U32

    ctypedef enum hal_pin_dir_t:
        HAL_DIR_UNSPECIFIED
        HAL_IN
        HAL_OUT
        HAL_IO

    ctypedef enum hal_param_dir_t:
        HAL_RO
        HAL_RW

    ctypedef int hal_bit_t
    ctypedef float hal_float_t
    ctypedef int hal_s32_t
    ctypedef int hal_u32_t

    int hal_signal_new(const char *sig, hal_type_t)
    int hal_signal_delete(const char *sig)

    int hal_link(const char *pin, const char *sig)
    int hal_unlink(const char *pin)

    int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
        void **data_ptr_addr, int comp_id)

"""
int hal_pin_bit_new(const char *name, hal_pin_dir_t dir,
    hal_bit_t ** data_ptr_addr, int comp_id)
int hal_pin_float_new(const char *name, hal_pin_dir_t dir,
    hal_float_t ** data_ptr_addr, int comp_id)
int hal_pin_u32_new(const char *name, hal_pin_dir_t dir,
    hal_u32_t ** data_ptr_addr, int comp_id)
int hal_pin_s32_new(const char *name, hal_pin_dir_t dir,
    hal_s32_t ** data_ptr_addr, int comp_id)
"""

