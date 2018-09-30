
cdef extern from "hal.h":
    int hal_init(char *name)
    int hal_exit(int comp_id)
    int hal_ready(int comp_id)
    int hal_signal_new(const char *name, hal_type_t type)
    int hal_signal_delete(const char *name)
    int hal_link(const char *pin_name, const char *sig_name)
    int hal_unlink(const char *pin_name)
    int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
        void **data_ptr_addr, int comp_id)

    void * hal_malloc(long int size)
    const char * hal_comp_name(int comp_id)

    ctypedef float hal_float_t
    ctypedef int hal_s32_t
    ctypedef unsigned hal_u32_t

    int  HAL_NAME_LEN

    ctypedef enum hal_type_t:
        HAL_TYPE_UNSPECIFIED
        HAL_BIT
        HAL_FLOAT
        HAL_S32
        HAL_U32
        HAL_S64
        HAL_U64

    ctypedef enum hal_pin_dir_t:
        HAL_DIR_UNSPECIFIED
        HAL_IN
        HAL_OUT
        HAL_IO

ctypedef union pinunion:
    void *v
    hal_u32_t *u32
    hal_s32_t *s32
    hal_float_t *f

