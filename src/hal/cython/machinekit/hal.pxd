# vim: sts=4 sw=4 et

cimport hal_const
# Copyright Pavel Shramov, 4/2014
# see http://psha.org.ru/cgit/psha/emc2.git/commit/?h=wip-cython
# License: MIT

cdef extern from "hal.h":
    int hal_init(char *name)
    int hal_exit(int comp_id)
    int hal_ready(int comp_id)
    const char * hal_comp_name(int comp_id)
    const char * hal_lasterror()
    hal_print_msg(int level, const char *msg)

    void * hal_malloc(long size)
    int hal_start_threads()
    int hal_stop_threads()

    # superset of hal_init()
    int hal_init_mode(char *name, int mode, int userarg1, int userarg2)

    ctypedef int hal_bit_t
    ctypedef float hal_float_t
    ctypedef int hal_s32_t
    ctypedef unsigned hal_u32_t

    int hal_signal_new(const char *sig, hal_const.hal_type_t)
    int hal_signal_delete(const char *sig)

    int hal_link(const char *pin, const char *sig)
    int hal_unlink(const char *pin)

    int hal_pin_new(const char *name, int type, int dir,
        void **data_ptr_addr, int comp_id)

    int hal_add_funct_to_thread(const char *funct_name,
                                const char *thread_name, int position)

    int hal_del_funct_from_thread(const char *funct_name, const char *thread_name)

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

