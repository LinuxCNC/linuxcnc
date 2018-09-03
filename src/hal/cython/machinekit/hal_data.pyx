# methods and properties for exposing the HAL shared memory segment
# and the HAL heap

from libc.stdint cimport uintptr_t

cdef class HALData:
    def __cinit__(self):
        hal_required()

    property version:
        def __get__(self):
            return hal_data.version

    property mutex:
        def __get__(self):
            return hal_data.mutex
        def __set__(self, int m):
            hal_data.mutex = m

    property base_period:
        def __get__(self):
            return hal_data.base_period

    property exact_base_period:
        def __get__(self):
            return hal_data.exact_base_period

    property threads_running:
        def __get__(self):
            return hal_data.threads_running
        def __set__(self, int r):
            hal_data.threads_running = r

    # property shmem_base:
    #     def __get__(self):
    #         return (void *) shmem_base

    property shmem_bot:
        def __get__(self):
            return hal_data.shmem_bot

    property shmem_top:
        def __get__(self):
            return hal_data.shmem_top

    property lock:
        def __get__(self):
            return hal_data.lock
        def __set__(self, int l):
            hal_data.lock = l

    property heap_status:
        def __get__(self):
            cdef rtapi_heap_stat rhs
            rhs.total_avail = 0
            rhs.fragments = 0
            rhs.largest = 0
            rtapi_heap_status(&hal_data.heap, &rhs)
            return (rhs.total_avail, rhs.fragments, rhs.largest)

    property heap_flags:
        def __set__(self, int f):
            rtapi_heap_setflags(&hal_data.heap,f)

    property heap_freelist:
        def __get__(self):
            f = []
            rtapi_heap_walk_freelist(&hal_data.heap,
                                    _append_chunk_cb,
                                     <void *>f)
            return f

cdef void _append_chunk_cb(size_t size, void *blob, void *user):
    arg =  <object>user
    arg.append((size, <uintptr_t>blob))
