#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"
#include "vtable.h"
#include "tp.h"
#include "tp_private.h"

#define VTVERSION  VTTP_VERSION1

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("machinekit trajectory planner");
MODULE_LICENSE("GPL");


// tp method dispatch vtable
static vtp_t vtp = {
    .tpCreate          = tpCreate,
    .tpClear           = tpClear,
    .tpInit            = tpInit,
    .tpClearDIOs       = tpClearDIOs,
    .tpSetCycleTime    = tpSetCycleTime,
    .tpSetVmax         = tpSetVmax,
    .tpSetVlimit       = tpSetVlimit,
    .tpSetAmax         = tpSetAmax,
    .tpSetId           = tpSetId,
    .tpGetExecId       = tpGetExecId,
    .tpGetExecTag      = tpGetExecTag,
    .tpSetTermCond     = tpSetTermCond,
    .tpSetPos          = tpSetPos,
    .tpAddCurrentPos   = tpAddCurrentPos,
    .tpSetCurrentPos   = tpSetCurrentPos,
    .tpAddRigidTap     = tpAddRigidTap,
    .tpAddLine         = tpAddLine,
    .tpAddCircle       = tpAddCircle,
    .tpRunCycle        = tpRunCycle,
    .tpPause           = tpPause,
    .tpResume          = tpResume,
    .tpAbort           = tpAbort,
    .tpGetPos          = tpGetPos,
    .tpIsDone          = tpIsDone,
    .tpQueueDepth      = tpQueueDepth,
    .tpActiveDepth     = tpActiveDepth,
    .tpGetMotionType   = tpGetMotionType,
    .tpSetSpindleSync  = tpSetSpindleSync,
    .tpToggleDIOs      = tpToggleDIOs,
    .tpSetAout         = tpSetAout,
    .tpSetDout         = tpSetDout,
    .tpIsPaused        = tpIsPaused,
    .tpSnapshot        = tpSnapshot,
    .tcqFull           = tcqFull,
};

static int comp_id, vtable_id;
const  char *mod_name = "tp";
static char *name = "tp";
RTAPI_MP_STRING(name, "tp vtable name");

int rtapi_app_main(void) {
    comp_id = hal_init(mod_name);
    if(comp_id > 0) {
	vtable_id = hal_export_vtable(name, VTVERSION, &vtp, comp_id);
	if (vtable_id < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			    mod_name, name, VTVERSION, &vtp, vtable_id );
	    return -ENOENT;
	}
	hal_ready(comp_id);
	return 0;
    }
    return comp_id;
}

void rtapi_app_exit(void)
{
    hal_remove_vtable(vtable_id);
    hal_exit(comp_id);
}
