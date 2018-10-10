#ifndef HAL_INTERNAL_H
#define HAL_INTERNAL_H

#include <config.h>
#include <rtapi.h>
#include <hal_priv.h>

// definitions for library-interal use only

RTAPI_BEGIN_DECLS


/** The 'shmalloc_xx()' functions allocate blocks of shared memory.

    the shmalloc_rt() function allocates memory downwards, aligned
    as follows:
    shmalloc_rt() allocates a block that is 'size' bytes long.
    If 'size' is 3 or more, the block is aligned on a 4 byte
    boundary.  If 'size' is 2, it is aligned on a 2 byte boundary,
    and if 'size' is 1, it is unaligned.
    Blocks allocated by shmalloc_rt() can not be freed.

    The shmalloc_desc() function allocates memory upwards, using
    the rtapi_heap alloc/free methods. The heap grows upwards
    from after hal_data. rtapi_malloc returns 8-byte aligned
    blocks. Blocks allocated by shmalloc_desc() can be freed by
    rtapi_free().

    These functions do not test a mutex - they are called from
    within the hal library by code that already has the mutex.
    (The public function 'hal_malloc()' is a wrapper that gets the
    mutex and then calls 'shmalloc_rt()'.)

    This is done to improve realtime performance.  'shmalloc_rt()'
    is used to allocate data that will be accessed by realtime
    code, while 'shmalloc_desc()' is used to allocate the much
    larger structures that are accessed only occaisionally during
    init.  This groups all the realtime data together, improving
    cache performance.
*/

// must resolve intra-hallib, so move here from hal_lib.c:
void *shmalloc_rt(size_t size); // was up

void *shmalloc_desc(size_t size); // was dn
void *shmalloc_desc_aligned(size_t size, size_t alignment); // was dn
void  shmfree_desc(void *p);

void free_funct_entry_struct(hal_funct_entry_t * funct_entry);
void free_funct_struct(hal_funct_t * funct);
void free_inst_struct(hal_inst_t *inst);
int  free_comp_struct(hal_comp_t * comp);
int free_sig_struct(hal_sig_t * sig);
int free_ring_struct(hal_ring_t *hrptr);
void free_group_struct(hal_group_t * group);
int pin_by_signal_callback(hal_object_ptr o, foreach_args_t *args);

void hal_proc_clean(void);
int hal_proc_init(void);

void free_thread_struct(hal_thread_t * thread);
extern int lib_module_id;
extern int lib_mem_id;

void unlink_pin(hal_pin_t * pin);

void free_pin_struct(hal_pin_t * pin);

int hal_heap_addmem(size_t click);

int zero_hal_data_u(const int type, hal_data_u *u);

int halg_free_argv(const bool use_hal_mutex,
		    char **argv);
char **halg_dupargv(const bool use_hal_mutex,
		     const int argc,
		     char * const *argv);

RTAPI_END_DECLS

#endif /* HAL_INTERNAL_H */
