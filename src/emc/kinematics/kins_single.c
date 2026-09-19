/********************************************************************
* Description: kins_single.c
*   The classic kinematics entry points for a module with one kinematics
*   type written as pure functions.  The module hands its description to
*   kinsSingleInit(); this file keeps the one RT parameter block,
*   fills it from the pins before every call, and hands the call to the
*   module's ops.  It is the counterpart of switchkins.c for a module that
*   does not switch.
*
* License: GPL Version 2
********************************************************************/

#include <rtapi.h>
#include <hal.h>
#include <emcmotcfg.h>

#include <kins_module.h>
#include <kins_rt.h>

static kins_params   rt_params;
static kins_scratch  rt_scratch;
static kins_pin_ref *pins;
static kins_tool_source tool_source;
static int           inited;
static KINEMATICS_TYPE reported_type = KINEMATICS_BOTH;
static const kins_module_info *km;

static const kins_ops *ops(void)
{
    return inited ? km->ops[0] : NULL;
}

// the block sees the pins as they are now, and the tool motion sent
static void read_pins(void)
{
    kinsParamsPinsRead(pins, km->params, km->nparams,
                       &rt_params);
    kinsToolSourceApply(&tool_source, km->halprefix,
                        km->params, km->nparams, &rt_params);
}

static void write_pins(void)
{
    kinsParamsPinsWrite(pins, km->params, km->nparams,
                        &rt_scratch);
}

int kinsSingleInit(int comp_id, const kins_module_info *m,
                   const char *coordinates, KINEMATICS_TYPE reported)
{
    if (!m) { return -1; }
    km = m;
    if (!km->ops[0] || !km->ops[0]->forward
        || !km->ops[0]->inverse) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "kinsSingleInit: %s supplies no forward or inverse\n",
            km->name ? km->name : "?");
        return -1;
    }
    if (kinsParamsInit(&rt_params, km, coordinates)) { return -1; }
    kinsScratchInit(&rt_scratch);
    if (kinsParamsPinsCreate(comp_id, km->halprefix,
                             km->params, km->nparams,
                             &pins)) {
        return -1;
    }
    reported_type = reported;
    inited = 1;
    return 0;
} // kinsSingleInit()

int kinematicsForward(const double *joint,
                      EmcPose *pos,
                      const KINEMATICS_FORWARD_FLAGS *fflags,
                      KINEMATICS_INVERSE_FLAGS *iflags)
{
    int r;
    if (!inited) { return -1; }
    read_pins();
    r = kinsOpsForward(ops(), &rt_params, &rt_scratch, joint, pos, fflags, iflags);
    write_pins();
    return r;
}

int kinematicsInverse(const EmcPose *pos,
                      double *joint,
                      const KINEMATICS_INVERSE_FLAGS *iflags,
                      KINEMATICS_FORWARD_FLAGS *fflags)
{
    int r;
    if (!inited) { return -1; }
    read_pins();
    r = kinsOpsInverse(ops(), &rt_params, &rt_scratch, pos, joint, iflags, fflags);
    write_pins();
    return r;
}

int kinematicsWorkFrame(const double *joint,
                        PmRotationMatrix *rot,
                        const KINEMATICS_FORWARD_FLAGS *fflags)
{
    if (!inited) { return -1; }
    read_pins();
    return kinsOpsWorkFrame(ops(), &rt_params, joint, rot, fflags);
}

int kinematicsToolFrame(const double *joint,
                        PmRotationMatrix *rot,
                        const KINEMATICS_FORWARD_FLAGS *fflags)
{
    if (!inited) { return -1; }
    read_pins();
    return kinsOpsToolFrame(ops(), &rt_params, joint, rot, fflags);
}

int kinematicsJacobian(const double *joint,
                       const EmcPose *pos,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags)
{
    if (!inited) { return -1; }
    read_pins();
    return kinsOpsJacobian(ops(), &rt_params, &rt_scratch, joint, pos, jac, iflags);
}

int kinematicsSetTool(const EmcPose *tool)
{
    if (!tool) { return -1; }
    kinsToolSourceSet(&tool_source, tool);
    return 0;
}

KINEMATICS_TYPE kinematicsType(void)
{
    return reported_type;
}

int kinematicsSwitchable(void) { return 0; }

int kinematicsSwitch(int switchkins_type)
{
    (void)switchkins_type;
    return 0;
}

// one type and nothing declared about it: -1, no information
int kinematicsTypeFlags(int ktype)
{
    (void)ktype;
    return -1;
}

// and so no machine frame type either
int kinematicsMachineFrame(const double *joint, EmcPose *pos)
{
    (void)joint;
    (void)pos;
    return -1;
}

// The module's description, for a copy of it loaded outside RT.  A module
// with one type does not depend on its parameters for its shape, so this
// is the table as declared.
int kinsSingleDescribe(const kins_module_info *m, kins_module_info *info)
{
    if (!m || !info) { return -1; }
    *info = *m;
    info->ntypes = 1;
    return 0;
}

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsWorkFrame);
EXPORT_SYMBOL(kinematicsToolFrame);
EXPORT_SYMBOL(kinematicsJacobian);
EXPORT_SYMBOL(kinematicsSetTool);
EXPORT_SYMBOL(kinematicsSwitchable);
EXPORT_SYMBOL(kinematicsSwitch);
EXPORT_SYMBOL(kinematicsTypeFlags);
EXPORT_SYMBOL(kinematicsMachineFrame);
EXPORT_SYMBOL(kinsSingleInit);
EXPORT_SYMBOL(kinsSingleDescribe);
