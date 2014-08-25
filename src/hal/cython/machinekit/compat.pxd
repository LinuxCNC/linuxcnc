# vim: sts=4 sw=4 et

cdef extern from "rtapi_bitops.h":
    int RTAPI_BIT(int b)

cdef extern from "rtapi_compat.h":
    cdef int  FLAVOR_DOES_IO
    cdef int  FLAVOR_KERNEL_BUILD
    cdef int  FLAVOR_RTAPI_DATA_IN_SHM
    cdef int  POSIX_FLAVOR_FLAGS
    cdef int  RTPREEMPT_FLAVOR_FLAGS
    cdef int  RTAI_KERNEL_FLAVOR_FLAGS
    cdef int  XENOMAI_KERNEL_FLAVOR_FLAGS
    cdef int  XENOMAI_FLAVOR_FLAGS

    ctypedef struct flavor_t:
        const char *name
        const char *mod_ext
        const char *so_ext
        const char *build_sys
        int id
        unsigned long flags

    int c_is_module_loaded "is_module_loaded" (const char *module)
    int c_load_module "load_module" (const char *module, const char *modargs)
    int c_run_module_helper "run_module_helper" (const char *format)

    int c_kernel_is_xenomai "kernel_is_xenomai" ()
    int c_kernel_is_rtai "kernel_is_rtai" ()
    int c_kernel_is_rtpreempt "kernel_is_rtpreempt" ()

    int c_xenomai_gid "xenomai_gid" ()
    int c_user_in_xenomai_group "user_in_xenomai_group" ()
    int c_kernel_instance_id "kernel_instance_id" ()

    flavor_t flavors[]
    flavor_t *c_flavor_byname "flavor_byname" (const char *flavorname)
    flavor_t *c_flavor_byid "flavor_byid" (int flavor_id)
    flavor_t *c_default_flavor "default_flavor"()
    int c_module_path "module_path" (char *result, const char *basename)
    int c_get_rtapi_config "get_rtapi_config" (char *result, const char *param, int n)
