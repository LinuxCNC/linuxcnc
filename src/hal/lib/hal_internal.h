#ifndef HAL_INTERNAL_H
#define HAL_INTERNAL_H

#include <config.h>
#include <rtapi.h>
#include <hal_priv.h>

// definitions for library-interal use only

RTAPI_BEGIN_DECLS
// must resolve intra-hallib, so move here from hal_lib.c:
void *shmalloc_up(long int size);
void *shmalloc_dn(long int size);
void free_funct_entry_struct(hal_funct_entry_t * funct_entry);
void free_funct_struct(hal_funct_t * funct);
void free_inst_struct(hal_inst_t *inst);

void hal_proc_clean(void);
int hal_proc_init(void);

void free_thread_struct(hal_thread_t * thread);
extern int lib_module_id;
extern int lib_mem_id;

void free_param_struct(hal_param_t * param);

hal_oldname_t *halpr_alloc_oldname_struct(void);
void free_oldname_struct(hal_oldname_t * oldname);

void unlink_pin(hal_pin_t * pin);

void free_pin_struct(hal_pin_t * pin);

RTAPI_END_DECLS

#endif /* HAL_INTERNAL_H */
