#include <sys/time.h>
#include <time.h>
#include <stdio.h>

static int msg_level = RTAPI_MSG_INFO;	/* message printing level */

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

typedef struct {
  int magic;			/* to check for valid handle */
  int key;			/* key to shared memory area */
  int id;			/* OS identifier for shmem */
  int count;                    /* count of maps in this process */
  unsigned long int size;	/* size of shared memory area */
  void *mem;			/* pointer to the memory */
} rtapi_shmem_handle;

#define MAX_SHM 64

#define SHMEM_MAGIC   25453	/* random numbers used as signatures */

static rtapi_shmem_handle shmem_array[MAX_SHM] = {{0},};

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
  rtapi_shmem_handle *shmem;
  int i;

  for(i=0 ; i < MAX_SHM; i++) {
    if(shmem_array[i].magic == SHMEM_MAGIC && shmem_array[i].key == key) {
      shmem_array[i].count ++;
      return i;
    }
    if(shmem_array[i].magic != SHMEM_MAGIC) break;
  }
  if(i == MAX_SHM)
    return RTAPI_NOMEM;

  shmem = &shmem_array[i];

  /* now get shared memory block from OS */
  shmem->id = shmget((key_t) key, (int) size, IPC_CREAT | 0666);
  if (shmem->id == -1) {
    return RTAPI_NOMEM;
  }
  /* and map it into process space */
  shmem->mem = shmat(shmem->id, 0, 0);
  if ((ssize_t) (shmem->mem) == -1) {
    return RTAPI_NOMEM;
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
  rtapi_shmem_handle *shmem;
  if(handle < 0 || handle >= MAX_SHM)
    return RTAPI_BADID;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADID;

  /* pass memory address back to caller */
  *ptr = shmem->mem;
  return RTAPI_SUCCESS;
}


int rtapi_shmem_delete(int handle, int module_id)
{
  struct shmid_ds d;
  int r1, r2;
  rtapi_shmem_handle *shmem;

  if(handle < 0 || handle >= MAX_SHM)
    return RTAPI_BADID;

  shmem = &shmem_array[handle];

  /* validate shmem handle */
  if (shmem->magic != SHMEM_MAGIC)
    return RTAPI_BADID;

  shmem->count --;
  if(shmem->count) return RTAPI_SUCCESS;

  /* unmap the shared memory */
  r1 = shmdt(shmem->mem);

  /* destroy the shared memory */
  r2 = shmctl(shmem->id, IPC_STAT, &d);
  if(r2 == 0 && d.shm_nattch == 0) {
      r2 = shmctl(shmem->id, IPC_RMID, &d);
  }

  /* free the shmem structure */
  shmem->magic = 0;

  if ((r1 != 0) || (r2 != 0))
    return RTAPI_FAIL;
  return RTAPI_SUCCESS;
}




#define BUFFERLEN 1024

void rtapi_print(const char *fmt, ...)
{
    char buffer[BUFFERLEN + 1];
    va_list args;

    va_start(args, fmt);
    /* call the normal library vnsprintf() */
    vsnprintf(buffer, BUFFERLEN, fmt, args);
    fputs(buffer, stdout);
    va_end(args);
}


void rtapi_print_msg(int level, const char *fmt, ...)
{
    char buffer[BUFFERLEN + 1];
    va_list args;

    if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
	va_start(args, fmt);
	/* call the normal library vnsprintf() */
	vsnprintf(buffer, BUFFERLEN, fmt, args);
	fputs(buffer, stderr);
	va_end(args);
    }
}

int rtapi_snprintf(char *buffer, unsigned long int size, const char *msg, ...) {
    va_list args;
    int result;

    va_start(args, msg);
    /* call the normal library vnsprintf() */
    result = vsnprintf(buffer, size, msg, args);
    va_end(args);
    return result;
}

int rtapi_vsnprintf(char *buffer, unsigned long int size, const char *fmt,
	va_list args) {
    return vsnprintf(buffer, size, fmt, args);
}

int rtapi_set_msg_level(int level) {
    msg_level = level;
    return RTAPI_SUCCESS;
}

int rtapi_get_msg_level() { 
    return msg_level;
}

long long rtapi_get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

#define rdtscll(val) \
         __asm__ __volatile__("rdtsc" : "=A" (val))

long long rtapi_get_clocks(void)
{
    long long int retval;

    rdtscll(retval);
    return retval;    
}



