
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#if defined(RTAPI_XENOMAI_USER)
#include <native/heap.h>
#include <native/timer.h>
#include <native/task.h>
#include <native/intr.h>
#include <native/sem.h>
#include <sys/mman.h>
#endif

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */

#define MAX_SHM 64
#define SHMEM_MAGIC   25453	/* random numbers used as signatures */
#define SHM_PERMISSIONS	0666

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
  shmem_data *shmem;
  struct shmid_ds d;
  int i, ret;
  int is_new = 0;

  for(i=0 ; i < MAX_SHM; i++) {
    if(shmem_array[i].magic == SHMEM_MAGIC && shmem_array[i].key == key) {
      shmem_array[i].count ++;
      return i;
    }
    if(shmem_array[i].magic != SHMEM_MAGIC) break;
  }
  if(i == MAX_SHM)
  {
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new failed due to MAX_SHM\n");
    return -ENOMEM;
  }
  shmem = &shmem_array[i];

  /* now get shared memory block from OS */

  // try to attach
  shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS);
  if (shmem->id == -1) {  
      if (errno == ENOENT) {
	  // nope, doesnt exist - create
	  shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS | IPC_CREAT);
	  is_new = 1;
      }
      if (shmem->id == -1) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
			  "Failed to allocate shared memory, key=0x%x size=%ld\n", key, size);
	  return -ENOMEM;
      }
  } 
  
  // get actual user/group and drop to ruid/rgid so removing is always possible
  if ((ret = shmctl(shmem->id, IPC_STAT, &d)) < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
		      "rtapi_shmem_new: shm_ctl(key=0x%x, IPC_STAT) failed: %d '%s'\n", 
		      key, errno, strerror(errno));      
  } else {
      // drop permissions of shmseg to real userid/group id
      if (!d.shm_perm.uid) { // uh, root perms 
	  d.shm_perm.uid = getuid();
	  d.shm_perm.gid = getgid();
	  if ((ret = shmctl(shmem->id, IPC_SET, &d)) < 0) {
	      rtapi_print_msg(RTAPI_MSG_ERR,
			      "rtapi_shmem_new: shm_ctl(key=0x%x, IPC_SET) failed: %d '%s'\n", 
			      key, errno, strerror(errno));      
	  } 
      }
  }
  /* and map it into process space */
  shmem->mem = shmat(shmem->id, 0, 0);
  if ((ssize_t) (shmem->mem) == -1) {
      rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_shmem_new: shmat(%d) failed: %d '%s'\n",
		      shmem->id, errno, strerror(errno));
      return -errno;
  }
  /* Touch each page by either zeroing the whole mem (if it's a new SHM region),
   * or by reading from it. */
  if (is_new) {
      memset(shmem->mem, 0, size);
  } else {
      unsigned int i, pagesize;
      
      pagesize = sysconf(_SC_PAGESIZE);
      for (i = 0; i < size; i += pagesize) {
	  unsigned int x = *(volatile unsigned int *)((unsigned char *)shmem->mem + i);
	  /* Use rand_r to clobber the read so GCC won't optimize it out. */
	  rand_r(&x);
      }
  }

  /* label as a valid shmem structure */
  shmem->magic = SHMEM_MAGIC;
  /* fill in the other fields */
  shmem->size = size;
  shmem->key = key;
  shmem->count = 1;

  /* return handle to the caller */
  return i;
}


int rtapi_shmem_getptr(int handle, void **ptr)
{
  shmem_data *shmem;
  if(handle < 0 || handle >= MAX_SHM)
    return -EINVAL;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return -EINVAL;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return 0;
}


int rtapi_shmem_delete(int handle, int module_id)
{
  struct shmid_ds d;
  int r1, r2;
  shmem_data *shmem;

  if(handle < 0 || handle >= MAX_SHM)
    return -EINVAL;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return -EINVAL;

  shmem->count --;
  if(shmem->count) {
      rtapi_print_msg(RTAPI_MSG_DBG,
		      "rtapi_shmem_delete: handle=%d module=%d key=0x%x:  %d remaining users\n", 
		      handle, module_id, shmem->key, shmem->count);
      return 0;
  }
  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);
  if (r1 < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
		      "rtapi_shmem_delete: shmdt(key=0x%x) failed: %d '%s'\n", 
		      shmem->key, errno, strerror(errno));      
  }
  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_STAT, &d);
  if (r2 < 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
		      "rtapi_shmem_delete: shm_ctl(0x%x, IPC_STAT) failed: %d '%s'\n", 
		      shmem->key, errno, strerror(errno));      
  }
  if(r2 == 0 && d.shm_nattch == 0) {
      r2 = shmctl(shmem->id, IPC_RMID, &d);
      if (r2 < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
			  "rtapi_shmem_delete: shm_ctl(0x%x, IPC_RMID) failed: %d '%s'\n", 
			  shmem->key, errno, strerror(errno));      
      }
  }  

  /* free the shmem structure */
  shmem->magic = 0;

  if ((r1 != 0) || (r2 != 0))
    return -EINVAL;
  return 0;
}


#if defined(RTAPI_POSIX)

long long rtapi_get_time(void) {

    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

#ifdef MSR_H_USABLE
#include <asm/msr.h>
#elif defined(__i386__) || defined(__x86_64__)
#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))
#else
#warning No implementation of rtapi_get_clocks available
#define rdtscll(val) (val)=0
#endif

long long rtapi_get_clocks(void)
{
    long long int retval;

    rdtscll(retval);
    return retval;    
}
#endif

typedef struct {
    unsigned long mutex;
    int           uuid;
} uuid_data_t;

#define UUID_KEY  0x48484c34 /* key for UUID for simulator */

int rtapi_init(const char *modname)
{
    static uuid_data_t* uuid_data   = 0;
    static         int  uuid_mem_id = 0;
    const static   int  uuid_id     = 0;

    static char* uuid_shmem_base = 0;
    int retval,id;
    void *uuid_mem;

    uuid_mem_id = rtapi_shmem_new(UUID_KEY,uuid_id,sizeof(uuid_data_t));
    if (uuid_mem_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "rtapi_init: could not open shared memory for uuid\n");
        rtapi_exit(uuid_id);
        return -EINVAL;
    }
    retval = rtapi_shmem_getptr(uuid_mem_id,&uuid_mem);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
        "rtapi_init: could not access shared memory for uuid\n");
        rtapi_exit(uuid_id);
        return -EINVAL;
    }
    if (uuid_shmem_base == 0) {
        uuid_shmem_base =        (char *) uuid_mem;
        uuid_data       = (uuid_data_t *) uuid_mem;
    }
    rtapi_mutex_get(&uuid_data->mutex);
        uuid_data->uuid++;
        id = uuid_data->uuid;
    rtapi_mutex_give(&uuid_data->mutex);

    return id;
}

int rtapi_exit(int module_id)
{
#if defined(RTAPI_XENOMAI_USER)
  munlockall();
#endif
  return 0;
}
