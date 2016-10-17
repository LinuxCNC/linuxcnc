# hal_priv.h declarations

from .hal cimport *
from hal_const cimport hal_type_t, hal_pin_dir_t, hal_param_dir_t
from rtapi cimport rtapi_heap
from cpython.bool  cimport bool

cdef extern from "hal_object.h":

    ctypedef struct halhdr_t:
       pass


cdef extern from "hal_priv.h":
    int MAX_EPSILON
    int HAL_MAX_RINGS


    ctypedef union hal_data_u:
        hal_bit_t b
        hal_s32_t s
        hal_s32_t u
        hal_float_t f

    ctypedef struct hal_pin_t:
        halhdr_t hdr
        int data_ptr_addr # legacy
        int data_ptr  # v2
        int signal
        hal_data_u dummysig
        hal_type_t type
        hal_pin_dir_t dir
        int flags
        unsigned char eps_index

    ctypedef struct hal_sig_t:
        halhdr_t hdr
        hal_data_u value
        hal_type_t type
        int readers
        int writers
        int bidirs

    ctypedef struct hal_param_t:
        halhdr_t hdr
        int data_ptr
        hal_type_t type
        hal_param_dir_t dir

    ctypedef struct hal_comp_t:
        halhdr_t hdr
        int type
        int state
        long int last_update
        long int last_bound
        long int last_unbound
        int pid
        void *shmem_base
        hal_constructor_t ctor
        hal_destructor_t dtor
        int insmod_args
        int userarg1
        int userarg2

    ctypedef struct hal_inst_t:
        halhdr_t hdr
        int inst_data_ptr
        int inst_size

    ctypedef struct hal_funct_args_t:
        long long int thread_start_time
        long long int start_time
        hal_thread_t  *thread
        hal_funct_t   *funct
        int argc
        const char **argv

    ctypedef void (*legacy_funct_t) (void *, long)
    ctypedef int  (*xthread_funct_t) (const void *, const hal_funct_args_t *)
    ctypedef int  (*userland_funct_t) (const hal_funct_args_t *)

    ctypedef union hal_funct_u:
        legacy_funct_t   l
        xthread_funct_t  x
        userland_funct_t u

    ctypedef struct hal_xfunct_t:
        int type  # hal_funct_signature_t
        hal_funct_u funct
        void *arg
        int uses_fp
        int reentrant
        int owner_id

    ctypedef struct hal_funct_t:
        halhdr_t hdr
        int type
        int uses_fp
        int owner_id
        int reentrant
        int users
        void *arg
        hal_funct_t funct
        hal_s32_t runtime
        hal_s32_t maxtime

    ctypedef struct hal_thread_t:
        halhdr_t hdr
        int uses_fp
        long int period
        int priority
        int task_id
        hal_s32_t runtime
        hal_s32_t maxtime
        #hal_list_t funct_list
        int cpu_id

    ctypedef struct hal_vtable_t:
        halhdr_t hdr
        int context
        int refcount
        int version
        void *vtable

    ctypedef struct hal_data_t:
        int version
        unsigned long mutex
        int shmem_bot
        int shmem_top
        long base_period
        int exact_base_period
        int threads_running
        unsigned char lock
        # RTAPI_DECLARE_BITMAP(rings, HAL_MAX_RINGS);
        double *epsilon #[MAX_EPSILON]
        rtapi_heap heap
        char *arena

    hal_data_t *hal_data
    char *hal_shmem_base
    int _halerrno

    # superset of hal_init()
    hal_comp_t *halg_xinitf(const int use_hal_mutex, int mode, int userarg1,
                            int userarg2, hal_constructor_t ctor,
                            hal_destructor_t dtor, char *name, ...)

    hal_comp_t *halpr_find_comp_by_name(const char *name)
    hal_pin_t *halpr_find_pin_by_name(const char *name)
    hal_sig_t *halpr_find_sig_by_name(const char *name)
    hal_param_t *halpr_find_param_by_name(const char *name)
    hal_thread_t *halpr_find_thread_by_name(const char *name)
    hal_funct_t *halpr_find_funct_by_name(const char *name)

    hal_comp_t *halpr_find_owning_comp(const int owner_id)

    hal_pin_t   *halpr_find_pin_by_owner_id(const int owner_id, hal_pin_t * start)
    hal_param_t *halpr_find_param_by_owner_id(const int owner_id, hal_param_t * start)
    hal_funct_t *halpr_find_funct_by_owner_id(const int owner_id, hal_funct_t * start)

    hal_inst_t *halpr_find_inst_by_name(const char *name)

    #hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start)
    ctypedef int (*hal_pin_signal_callback_t)(hal_pin_t *pin,
                                              hal_sig_t *sig,
                                              void *user)
    int halg_foreach_pin_by_signal(const int use_hal_mutex,
                                   hal_sig_t *sig,
                                   hal_pin_signal_callback_t cb,
                                   void *user)

    int halg_inst_create(const int use_hal_mutex,
                         const char *name,
                         const int comp_id,
                         const int size,
                         void **inst_data)

    int halg_inst_delete(const int use_hal_mutex, const char *name)
    hal_pin_t *halg_pin_newf(const int use_hal_mutex,
                             const int type,
                             const int dir,
                             void **data_ptr_addr,
                             const int owner_id,
                             const char *name)

    # accessors
    hal_sig_t *signal_of(const hal_pin_t *pin)
    hal_data_u *pin_value(hal_pin_t *pin)
    int pin_type(const hal_pin_t *pin)
    int param_type(const hal_param_t *param)
    int pin_dir(const hal_pin_t *pin)

    int sig_type(const hal_sig_t *sig)
    hal_data_u *sig_value(hal_sig_t *sig)
    hal_sig_t *signal_of(const hal_pin_t *pin)
    int pin_linked_to(const hal_pin_t *pin, const hal_sig_t *sig)
    bint pin_is_linked(const hal_pin_t *pin)

    # test if dir is in [HAL_IN, HAL_OUT, HAL_IO]
    const int hal_valid_dir(const hal_pin_dir_t dir)

    # test if dir is in [HAL_BIT,...]
    const int hal_valid_type(const hal_type_t type)


    # string-returning or converting accessors
    # type -> "bit", "s32" etc
    const char *hals_type(const hal_type_t type)

    # convert hal_data_u to string
    int hals_value(char *buffer,
                   const size_t s,
                   const hal_type_t type,
                   const hal_data_u *u)

    # convert pin direction to string ("IN", "OUT", "IO")
    const char *hals_pindir(const hal_pin_dir_t dir)


    # hal_data_u *accessors
    hal_bit_t set_bit_value(hal_data_u *h, const hal_bit_t value)
    hal_s32_t set_s32_value(hal_data_u *h, const hal_s32_t value)
    hal_u32_t set_u32_value(hal_data_u *h, const hal_u32_t value)
    hal_float_t set_float_value(hal_data_u *h, const hal_float_t value)
    hal_s64_t set_s64_value(hal_data_u *h, const hal_s64_t value)
    hal_u64_t set_u64_value(hal_data_u *h, const hal_u64_t value)

    hal_bit_t get_bit_value(const hal_data_u *h)
    hal_s32_t get_s32_value(const hal_data_u *h)
    hal_u32_t get_u32_value(const hal_data_u *h)
    hal_float_t get_float_value(const hal_data_u *h)
    hal_s64_t get_s64_value(const hal_data_u *h)
    hal_u64_t get_u64_value(const hal_data_u *h)
