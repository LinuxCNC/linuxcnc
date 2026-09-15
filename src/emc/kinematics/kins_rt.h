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

/* Where the RT block's tool comes from.  kinematicsSetTool() records what
   motion sends in one of these; kinsToolSourceApply() writes it into a
   block after the pins have been read, over the tool entry, once motion
   has sent anything.  Until then the tool entry's pin is all there is, as
   under halrun with the module alone.  A config that still nets the tool
   to the module's pin loses nothing; one that sets that pin to something
   else is told, once, after the two have disagreed for a thousand calls,
   since the pin lags the send by a cycle. */
typedef struct {
    EmcPose tool;
    int     have;            /* motion has sent a tool */
    int     disagreeing;     /* consecutive calls with the pin elsewhere */
    int     warned;
} kins_tool_source;

extern void kinsToolSourceSet(kins_tool_source *src, const EmcPose *tool);
extern void kinsToolSourceApply(kins_tool_source *src, const char *prefix,
                                const kins_param_desc *params, int nparams,
                                kins_params *p);

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
