cdef extern from "rtapi.h":
    ctypedef struct global_data_t:
        unsigned magic
        int layout_version
        unsigned long mutex

        int instance_id
        char *instance_name
        int rtapi_thread_flavor

        int rt_msg_level
        int user_msg_level
        #rtapi_atomic_type next_handle
        int hal_size
        int hal_thread_stack_size

        unsigned char service_uuid[16]
        int rtapi_app_pid
        int rtapi_msgd_pid

        #rtapi_threadstatus_t thread_status[RTAPI_MAX_TASKS + 1]

        int error_ring_full
        int error_ring_locked

        # ringheader_t rtapi_messages
        # char buf[SIZE_ALIGN(MESSAGE_RING_SIZE)]
        # ringtrailer_t rtapi_messages_trailer
