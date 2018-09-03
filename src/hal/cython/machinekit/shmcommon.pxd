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

    int c_shm_common_init "shm_common_init"()
    int c_shm_common_new "shm_common_new" (int key, int *size, int instance, void **shmptr, int create)
    int c_shm_common_detach "shm_common_detach" (int size, void *shmptr)
    bint c_shm_common_exists "shm_common_exists" (int key)
    int c_shm_common_unlink "shm_common_unlink" (int key)
