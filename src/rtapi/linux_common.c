#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

static int msg_level = RTAPI_MSG_INFO;	/* message printing level */ //XXX

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

/* typedef struct { */
/* 	int magic;		/\* to check for valid handle *\/ */
/* 	int key;		/\* key to shared memory area *\/ */
/* 	int id;			/\* OS identifier for shmem *\/ */
/* 	int count;		/\* count of maps in this process *\/ */
/* 	unsigned long int size;	/\* size of shared memory area *\/ */
/* 	void *mem;		/\* pointer to the memory *\/ */
/* } rtapi_shmem_handle; */

#define MAX_SHM		64
#define SHM_PERMISSIONS	0666

#define SHMEM_MAGIC	25453	/* random numbers used as signatures */


//static shmem_data shmem_array[MAX_SHM];
static pthread_mutex_t shmem_array_mutex = PTHREAD_MUTEX_INITIALIZER;


int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
	shmem_data *shmem;
	int is_new = 0, i;

	pthread_mutex_lock(&shmem_array_mutex);
	for (i = 0; i < MAX_SHM; i++) {
		if (shmem_array[i].magic == SHMEM_MAGIC
		    && shmem_array[i].key == key) {
			shmem_array[i].count++;
			pthread_mutex_unlock(&shmem_array_mutex);
			return i;
		}
		if (shmem_array[i].magic != SHMEM_MAGIC)
			break;
	}
	if (i == MAX_SHM) {
		pthread_mutex_unlock(&shmem_array_mutex);
		return -ENOMEM;
	}
	shmem = &shmem_array[i];

	/* now get shared memory block from OS */
	shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS);
	if (shmem->id == -1) {
		if (errno == ENOENT) {
		    int euid = geteuid();
		    if (!euid) { // running as root 
			// switch to real user during shm creation
			// so segment can be removed by 'realtime stop'
			// without sudo
			if (seteuid(getuid()) < 0)
			    rtapi_print_msg(RTAPI_MSG_ERR,"seteuid(%d): %s",getuid(), strerror(errno));
		    }
		    shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS | IPC_CREAT);
		    if (!euid) { // switch back
			if (seteuid(euid) < 0) 
			    rtapi_print_msg(RTAPI_MSG_ERR,"seteuid(%d): %s",euid, strerror(errno));
		    }
		    is_new = 1;
		}
		if (shmem->id == -1) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"Failed to allocate shared memory\n");
			pthread_mutex_unlock(&shmem_array_mutex);
			return -ENOMEM;
		}
	}
	/* and map it into process space */
	shmem->mem = shmat(shmem->id, 0, 0);
	if ((ssize_t)(shmem->mem) == -1) {
		pthread_mutex_unlock(&shmem_array_mutex);
		return -ENOMEM;
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

	pthread_mutex_unlock(&shmem_array_mutex);

	/* return handle to the caller */
	return i;
}

int rtapi_shmem_getptr(int handle, void **ptr)
{
	shmem_data *shmem;

	if (handle < 0 || handle >= MAX_SHM)
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

	if (handle < 0 || handle >= MAX_SHM)
		return -EINVAL;

	pthread_mutex_lock(&shmem_array_mutex);
	shmem = &shmem_array[handle];

	/* validate shmem handle */
	if (shmem->magic != SHMEM_MAGIC) {
		pthread_mutex_unlock(&shmem_array_mutex);
		return -EINVAL;
	}

	shmem->count--;
	if (shmem->count) {
		pthread_mutex_unlock(&shmem_array_mutex);
		return 0;
	}

	/* unmap the shared memory */
	r1 = shmdt(shmem->mem);

	/* destroy the shared memory */
	r2 = shmctl(shmem->id, IPC_STAT, &d);
	if (r2 == 0 && d.shm_nattch == 0) {
		r2 = shmctl(shmem->id, IPC_RMID, &d);
	}

	/* free the shmem structure */
	shmem->magic = 0;

	pthread_mutex_unlock(&shmem_array_mutex);

	if ((r1 != 0) || (r2 != 0))
		return -EINVAL;
	return 0;
}

#define PRINTBUF_LEN	1024

void default_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap)
{
    if (level == RTAPI_MSG_ALL)
	vfprintf(stdout, fmt, ap);
     else
	 vfprintf(stderr, fmt, ap);
}

static rtapi_msg_handler_t rtapi_msg_handler = default_rtapi_msg_handler;

rtapi_msg_handler_t rtapi_get_msg_handler(void)
{
	return rtapi_msg_handler;
}

void rtapi_set_msg_handler(rtapi_msg_handler_t handler)
{
	if (handler == NULL)
		rtapi_msg_handler = default_rtapi_msg_handler;
	else
		rtapi_msg_handler = handler;
}

void rtapi_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	rtapi_msg_handler(RTAPI_MSG_ALL, fmt, args);
	va_end(args);
}

void rtapi_print_msg(int level, const char *fmt, ...)
{
	va_list args;

	if ((level <= msg_level) && (msg_level != RTAPI_MSG_NONE)) {
		va_start(args, fmt);
		rtapi_msg_handler(level, fmt, args);
		va_end(args);
	}
}

int rtapi_snprintf(char *buffer, unsigned long int size, const char *msg, ...)
{
	va_list args;
	int result;

	va_start(args, msg);
	/* call the normal library vnsprintf() */
	result = vsnprintf(buffer, size, msg, args);
	va_end(args);

	return result;
}

int rtapi_vsnprintf(char *buffer, unsigned long int size, const char *fmt,
		    va_list args)
{
	return vsnprintf(buffer, size, fmt, args);
}

int rtapi_set_msg_level(int level)
{
	msg_level = level;
	return 0;
}

int rtapi_get_msg_level()
{
	return msg_level;
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
