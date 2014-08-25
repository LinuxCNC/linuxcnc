from .shmcommon cimport *
from .global_data cimport *
#from os import strerror

def shmdrv_loaded():
    return c_shmdrv_loaded != 0

def shm_common_exists(int segment):
    return c_shm_common_exists(segment)

c_shm_common_init()
