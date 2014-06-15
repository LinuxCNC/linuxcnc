# hal_ring.h definitions

from .hal cimport *
from .hal_priv cimport *

cdef extern from "hal_rcomp.h":
    int RCOMP_ACCEPT_VALUES_ON_BIND

    ctypedef struct hal_compiled_comp_t:
        int n_pins

    ctypedef int(*comp_report_callback_t)(int,  hal_compiled_comp_t *,
                                          hal_pin_t *pin,
                                          hal_data_u *value,
                                          void *cb_data)

    int hal_ccomp_report(hal_compiled_comp_t *ccomp,
                         comp_report_callback_t report_cb,
                         void *cb_data, int report_all)

    int hal_ccomp_args(hal_compiled_comp_t *ccomp, int *arg1, int *arg2)
    int hal_ccomp_free(hal_compiled_comp_t *cc)
    int hal_compile_comp(const char *name, hal_compiled_comp_t **ccomp)
    int hal_ccomp_match(hal_compiled_comp_t *ccomp)

    # acquiring and binding remote components
    int hal_bind(const char *comp)
    int hal_unbind(const char *comp)
    int hal_acquire(const char *comp, int pid)
    int hal_release(const char *comp_name)

# cdef inline int comp_callback(int phase, hal_compiled_comp_t * cc, hal_pin_t *pin,hal_data_u *value,void *user_data):
#     (<object>user_data)(phase, cc, pin, value)
