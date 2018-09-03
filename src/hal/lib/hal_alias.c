// HAL alias (aka 'oldname') API
// usage not recommended - ready for deprecation

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"


hal_oldname_t *halpr_alloc_oldname_struct(void)
{
    hal_oldname_t *p;

    /* check the free list */
    if (hal_data->oldname_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->oldname_free_ptr);
	/* unlink it from the free list */
	hal_data->oldname_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_oldname_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->name[0] = '\0';
    }
    return p;
}




void free_oldname_struct(hal_oldname_t * oldname)
{
    /* clear contents of struct */
    oldname->name[0] = '\0';
    /* add it to free list */
    oldname->next_ptr = hal_data->oldname_free_ptr;
    hal_data->oldname_free_ptr = SHMOFF(oldname);
}
