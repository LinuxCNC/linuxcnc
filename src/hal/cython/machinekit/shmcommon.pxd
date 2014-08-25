# vim: sts=4 sw=4 et


cdef extern from "shmdrv.h":
    cdef extern int c_shmdrv_loaded "shmdrv_loaded"

    ctypedef struct shm_stat:
        int driver_fd
        int  key
        size_t size
        size_t act_size
        void *addr
        int flags
        int id
        int n_kattach
        int n_uattach
        int creator
        int shmdrv_loaded

    # bint c_shmdrv_available "shmdrv_available"()
    # int c_shmdrv_driver_fd "shmdrv_driver_fd"()
    # int c_shmdrv_status "shmdrv_status"(shm_status *shmstat)
    # int c_shmdrv_create "shmdrv_create"(shm_status *shmstat)
    # int c_shmdrv_attach "shmdrv_attach"(shm_status *shmstat, void **shm)
    # int c_shmdrv_detach "shmdrv_detach" (shm_status *shmstat, void *shm)
    # int c_shmdrv_gc "shmdrv_gc"()
    # void c_shmdrv_print_status "shmdrv_print_status" (shm_status *sm, const char *tag)

    int c_shm_common_init "shm_common_init"()
    int c_shm_common_new "shm_common_new" (int key, int *size, int instance, void **shmptr, int create)
    int c_shm_common_detach "shm_common_detach" (int size, void *shmptr)
    bint c_shm_common_exists "shm_common_exists" (int key)
    int c_shm_common_unlink "shm_common_unlink" (int key)
