cdef extern from "hal.h":
    ctypedef enum comp_type_t:
        TYPE_INVALID
        TYPE_RT
        TYPE_USER
        TYPE_REMOTE
        TYPE_HALLIB

    ctypedef enum comp_state_t:
        COMP_INVALID
        COMP_INITIALIZING
        COMP_UNBOUND
        COMP_BOUND
        COMP_READY

    int  HAL_NAME_LEN

    ctypedef enum hal_type_t:
        HAL_TYPE_UNSPECIFIED
        HAL_BIT
        HAL_FLOAT
        HAL_S32
        HAL_U32

    ctypedef enum hal_pin_dir_t:
        HAL_DIR_UNSPECIFIED
        HAL_IN
        HAL_OUT
        HAL_IO

    ctypedef enum hal_param_dir_t:
        HAL_RO
        HAL_RW

    ctypedef enum pinflag_t:
       PIN_DO_NOT_TRACK

    ctypedef enum hal_funct_signature_t:
       FS_LEGACY_THREADFUNC
       FS_XTHREADFUNC
       FS_USERLAND

cdef extern from "hal_group.h":
    ctypedef enum report_phase_t:
        REPORT_BEGIN
        REPORT_SIGNAL
        REPORT_PIN
        REPORT_END
