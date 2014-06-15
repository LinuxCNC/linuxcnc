from .hal_priv cimport hal_shmem_base, hal_data_u, hal_data, hal_sig_t
from .rtapi cimport rtapi_atomic_type

cdef extern from "hal_group.h" :

    int REPORT_BEGIN
    int REPORT_SIGNAL
    int REPORT_PIN
    int REPORT_END

    int GROUP_REPORT_ON_CHANGE

    int RESOLVE_NESTED_GROUPS

    int HAL_SIGNAL
    int HAL_GROUP

    ctypedef struct hal_member_t:
        int next_ptr
        int sig_member_ptr
        int group_member_ptr
        int userarg1
        unsigned char eps_index
        int handle

    ctypedef struct  hal_group_t:
        int next_ptr
        int refcount
        int userarg1
        int userarg2
        int handle
        char *name
        int member_ptr

    ctypedef struct hal_compiled_group_t:
        int magic
        hal_group_t *group
        int n_members
        int mbr_index
        int mon_index
        hal_member_t  **member
        rtapi_atomic_type *changed
        int n_monitored
        hal_data_u    *tracking
        unsigned long user_flags
        void *user_data

    ctypedef int (*group_report_callback_t)(int,  hal_compiled_group_t *,
	      hal_sig_t *sig, void *cb_data)

    ctypedef int (*hal_group_callback_t)(hal_group_t *group,  void *cb_data)

    ctypedef int (*hal_member_callback_t)(int level, hal_group_t **groups,
                                          hal_member_t *member, void *cb_data)
    int hal_group_new(const char *group, int arg1, int arg2)
    int hal_group_delete(const char *group)

    int hal_member_new(const char *group, const char *member, int arg1, int eps_index)
    int hal_member_delete(const char *group, const char *member)

    int hal_cgroup_report(hal_compiled_group_t *cgroup,
                          group_report_callback_t report_cb,
                          void *cb_data, int force_all)
    int hal_cgroup_free(hal_compiled_group_t *cgroup)
    int hal_ref_group(const char *group)
    int hal_unref_group(const char *group)

    hal_group_t *halpr_find_group_of_member(const char *name)
    hal_group_t *halpr_find_group_by_name(const char *name)

    int halpr_foreach_group(const char *groupname,
                            hal_group_callback_t callback,
                            void *cb_data)
    int hal_cgroup_timer(hal_compiled_group_t *cgroup)
    int halpr_group_compile(const char *name, hal_compiled_group_t **cgroup)
    int hal_cgroup_match(hal_compiled_group_t *cgroup)

    int halpr_foreach_member(const char *group,
                             hal_member_callback_t callback,
                             void *cb_data, int flags)

    hal_group_t *halpr_find_group_by_name(const char *name)
    hal_group_t *halpr_find_group_of_member(const char *member)
