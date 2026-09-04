/********************************************************************
* Description: kins_rt.h
*   The RT side of a kinematics module written as pure functions: the HAL
*   pins made from its geometry table, and the wrapper that supplies the
*   classic entry points for a module with one kinematics type.  A module
*   with several types gets the same from switchkins.c.
*
*   Kept apart from kinematics.h because everything here needs HAL, and
*   kinematics.h is read by callers outside RT that do not.
*
* License: GPL Version 2
********************************************************************/
#ifndef __LINUXCNC_KINS_RT_H
#define __LINUXCNC_KINS_RT_H

#include <hal.h>
#include "kinematics.h"

/* one HAL pin handle per table entry, of whichever type the entry has */
typedef union {
    hal_real_t r;
    hal_bool_t b;
    hal_sint_t s;
    hal_uint_t u;
} kins_pin_ref;

/* Make one pin per table entry, named <prefix>.<name>, inputs at their
   defaults.  *out receives the handles, from hal_malloc(), or NULL for an
   empty table.  Returns 0 or -1. */
extern int kinsParamsPinsCreate(int comp_id, const char *prefix,
                                const kins_param_desc *params, int nparams,
                                kins_pin_ref **out);

/* Copy every input pin into p->geometry[], and the tool entry into
   p->tool.tran.z as well. */
extern void kinsParamsPinsRead(const kins_pin_ref *pins,
                               const kins_param_desc *params, int nparams,
                               kins_params *p);

/* Copy s->out[] to every output pin. */
extern void kinsParamsPinsWrite(const kins_pin_ref *pins,
                                const kins_param_desc *params, int nparams,
                                const kins_scratch *s);

/* A module with one kinematics type defines this, describing itself, and
   links kins_single.c, which supplies kinematicsForward() and the rest
   from it.  ops[0] is the maths; the other entries are ignored. */
extern const kins_module_info kins_module;

/* Called once from the module's rtapi_app_main() or EXTRA_SETUP(), after
   hal_init() and before hal_ready(): makes the pins, builds the block for
   coordinates and records the KINEMATICS_TYPE that kinematicsType() will
   report.  Returns 0 or -1. */
extern int kinsSingleInit(int comp_id, const char *coordinates,
                          KINEMATICS_TYPE reported);

#endif
