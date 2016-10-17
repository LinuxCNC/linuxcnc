// HAL miscellaneous functions which dont clearly fit elsewhere

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"
#include "hal_object_selectors.h"

/***********************************************************************
*                     utility functions, mostly used by haltalk        *
*                                                                      *
************************************************************************/
char *fmt_ap(char *buffer,
	     size_t size,
	     const char *fmt,
	     va_list ap)
{
    int sz;
    sz = rtapi_vsnprintf(buffer, size, fmt, ap);
    if(sz == -1 || sz > size) {
	HALFAIL_NULL(E2BIG, "length %d too long for name starting with '%s'",
	       sz, buffer);
    }
    return buffer;
}

char *fmt_args(char *buffer,
	       size_t size,
	       const char *fmt,
	       ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *s = fmt_ap(buffer, size, fmt, ap);
    va_end(ap);
    return s;
}


// return number of pins in a component, legacy or all-insts
int halpr_pin_count(const char *name)
{
    CHECK_NULL(name);
    CHECK_HALDATA();
    hal_comp_t *comp = halpr_find_comp_by_name(name);
    if (comp == 0)
	HALFAIL_RC(ENOENT, "no such comp: '%s'", name);

    foreach_args_t args =  {
	.type = HAL_PIN,
	.owning_comp = ho_id(comp),
    };
    return halg_foreach(0, &args, NULL);
}

// return number of params in a component, legacy or all-insts
int
halpr_param_count(const char *name)
{
    CHECK_NULL(name);
    CHECK_HALDATA();
    hal_comp_t *comp = halpr_find_comp_by_name(name);
    if (comp == 0)
	HALFAIL_RC(ENOENT, "no such comp: '%s'", name);

    foreach_args_t args =  {
	.type = HAL_PARAM,
	.owning_comp = ho_id(comp),
    };
    return halg_foreach(0, &args, NULL);
}

// hal mutex scope-locked version of halpr_find_pin_by_name()
hal_pin_t *
hal_find_pin_by_name(const char *name)
{
    PCHECK_NULL(name);
    PCHECK_HALDATA();
    WITH_HAL_MUTEX();
    return halpr_find_pin_by_name(name);
}

int
hal_comp_state_by_name(const char *name)
{
    WITH_HAL_MUTEX();
    hal_comp_t *comp = halpr_find_comp_by_name(name);
    if (comp == NULL)
	HALFAIL_RC(ENOENT, "no such comp: '%s'", name);
    return comp->state;
}
