
from .hal_priv cimport *
from .hal_ring cimport *
from .hal_group cimport hal_group_t, hal_member_t

cdef extern from "hal_iter.h":
    ctypedef int (*hal_comp_callback_t)  (hal_comp_t *comp,  void *arg)
    ctypedef int (*hal_sig_callback_t)   (hal_sig_t *sig,  void *arg)
    ctypedef int (*hal_ring_callback_t)  (hal_ring_t *ring,  void *arg)
    ctypedef int (*hal_funct_callback_t) (hal_funct_t *funct,  void *arg)
    ctypedef int (*hal_thread_callback_t)(hal_thread_t *thread,  void *arg)
    ctypedef int (*hal_pin_callback_t)   (hal_pin_t *pin,  void *arg)
    ctypedef int (*hal_group_callback_t) (hal_group_t *group,  void *cb_data)
    ctypedef int (*hal_member_callback_t)(int level, hal_group_t **groups,
                                          hal_member_t *member, void *cb_data)
    ctypedef int (*hal_vtable_callback_t)(hal_vtable_t *vtable,  void *arg)
    ctypedef int (*hal_inst_callback_t)  (hal_inst_t *vtable,  void *cb_data)

    int halpr_foreach_comp(const char *name,
                           hal_comp_callback_t callback,
                           void *arg)
    int halpr_foreach_sig(const char *name,
                          hal_sig_callback_t callback,
                          void *arg)
    int halpr_foreach_ring(const char *name,
                           hal_ring_callback_t callback,
                           void *arg)
    int halpr_foreach_funct(const char *name,
                            hal_funct_callback_t callback,
                            void *arg)
    int halpr_foreach_thread(const char *name,
                             hal_thread_callback_t callback,
                             void *arg)
    int halpr_foreach_pin(const char *name,
                          hal_pin_callback_t callback,
                          void *arg)
    int halpr_foreach_group(const char *groupname,
                            hal_group_callback_t callback,
                            void *arg)
    int halpr_foreach_member(const char *group,
                             hal_member_callback_t callback,
                             void *cb_data,
                             int flags)
    int halpr_foreach_vtable(const char *name,
                             hal_vtable_callback_t
                             callback,
                             void *arg)
    int halpr_foreach_inst(const char *name,
                           hal_inst_callback_t callback,
                           void *arg)
