# hal_priv.h declarations

from .hal cimport *
from hal_const cimport hal_type_t, hal_pin_dir_t, hal_param_dir_t

cdef extern from "hal_priv.h":
    int MAX_EPSILON
    int HAL_MAX_RINGS


    ctypedef union hal_data_u:
        hal_bit_t b
        hal_s32_t s
        hal_s32_t u
        hal_float_t f

    ctypedef struct hal_pin_t:
        int next_ptr
        int data_ptr_addr
        int owner_id
        int signal
        hal_data_u dummysig
        int oldname
        hal_type_t type
        hal_pin_dir_t dir
        int handle
        int flags
        unsigned char eps_index
        char *name

    ctypedef struct hal_sig_t:
        int next_ptr
        int data_ptr
        hal_type_t type
        int readers
        int writers
        int bidirs
        int handle
        char *name

    ctypedef struct hal_param_t:
        int next_ptr
        int data_ptr
        int owner_id
        int oldname
        hal_type_t type
        hal_param_dir_t dir
        int handle
        char *name

    ctypedef struct hal_comp_t:
        int next_ptr
        int comp_id
        int type
        int state
        long int last_update
        long int last_bound
        long int last_unbound
        int pid
        void *shmem_base
        char *name
        hal_constructor_t ctor
        hal_destructor_t dtor
        int insmod_args
        int userarg1
        int userarg2

    ctypedef struct hal_inst_t:
        int next_ptr
        int comp_id
        int inst_id
        int inst_data_dtr
        int inst_size
        char *name

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
        int next_ptr
        int type
        int uses_fp
        int owner_id
        int reentrant
        int users
        void *arg
        hal_funct_t funct
        hal_s32_t runtime
        hal_s32_t maxtime
        int handle
        char *name

    ctypedef struct hal_thread_t:
        int next_ptr
        int uses_fp
        long int period
        int priority
        int task_id
        hal_s32_t runtime
        hal_s32_t maxtime
        #hal_list_t funct_list
        int cpu_id
        int handle
        char *name

    ctypedef struct hal_vtable_t:
        int next_ptr
        int context
        int comp_id
        int instance_id
        int handle
        int refcount
        int version
        void *vtable
        char *name

    ctypedef struct hal_data_t:
        int version
        unsigned long mutex
        hal_s32_t shmem_avail
        int shmem_bot
        int shmem_top

        int comp_list_ptr
        int pin_list_ptr
        int sig_list_ptr
        int param_list_ptr
        int funct_list_ptr
        int thread_list_ptr
        int vtable_list_ptr
        int group_list_ptr

        int ring_list_ptr
        int member_list_ptr
        int inst_list_ptr


        long base_period
        int threads_running
        int exact_base_period
        unsigned char lock
        # RTAPI_DECLARE_BITMAP(rings, HAL_MAX_RINGS);
        double *epsilon #[MAX_EPSILON]


    hal_data_t *hal_data
    char *hal_shmem_base


    hal_comp_t *halpr_find_comp_by_name(const char *name)
    hal_pin_t *halpr_find_pin_by_name(const char *name)
    hal_sig_t *halpr_find_sig_by_name(const char *name)
    hal_param_t *halpr_find_param_by_name(const char *name)
    hal_thread_t *halpr_find_thread_by_name(const char *name)
    hal_funct_t *halpr_find_funct_by_name(const char *name)

    hal_comp_t *halpr_find_owning_comp(const int owner_id)

    hal_pin_t *halpr_find_pin_by_owner_id(const int owner_id, hal_pin_t * start)
    hal_param_t *halpr_find_param_by_owner_id(const int owner_id, hal_param_t * start)
    hal_funct_t *halpr_find_funct_by_owner_id(const int owner_id, hal_funct_t * start)

    hal_pin_t *halpr_find_pin_by_sig(hal_sig_t * sig, hal_pin_t * start)

    hal_inst_t *halpr_find_inst_by_name(const char *name)
