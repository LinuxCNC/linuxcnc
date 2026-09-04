/*
  Copyright 2019 Dewey Garrett <dgarrett@panix.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/* switchkins.c provide functions for switchable kins modules:
*    rtapi_app()
*    rtapi_exit()
*    kinematicsType()
*    kinematicsForward()
*    kinematicsInverse()
*    kinematicsSwitch()
*    kinematicsSwitchable()
*  Using modules must supply function: switchkinsSetup()
*/
#include <rtapi.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcmotcfg.h>

#include "switchkins.h"
#include <kins_rt.h>

//*********************************************************************
// kinematic functions (default=0 for err detection):
static kparms kp; // kinematics parms (common all types)

// indexed by switchkins_type (NULL==not provided, for err detection):
static KS ksetups[SWITCHKINS_MAX_TYPES] = {NULL};
static KF kfwds[SWITCHKINS_MAX_TYPES]   = {NULL};
static KI kinvs[SWITCHKINS_MAX_TYPES]   = {NULL};
static KT ktools[SWITCHKINS_MAX_TYPES]  = {NULL};
static KT kworks[SWITCHKINS_MAX_TYPES]  = {NULL};
static KTI ktinvs[SWITCHKINS_MAX_TYPES] = {NULL};
static KJ kjacs[SWITCHKINS_MAX_TYPES]   = {NULL};
static PmRotationMatrix knative[SWITCHKINS_MAX_TYPES];

// types written as pure functions (see kinematics.h): the maths of each,
// the one RT parameter block they all read, a scratch per type, and the
// pins made from the module's table
static const kins_ops *kops[SWITCHKINS_MAX_TYPES] = {NULL};
static kins_params   rt_params;
static kins_scratch  rt_scratch[SWITCHKINS_MAX_TYPES];
static kins_pin_ref *pins;
static int           inited;

// types provided, counted in rtapi_app_main() once they are all in
static int kins_count;
static int register_error;

static int switchkins_type;
static struct swdata {
    hal_bool_t kinstype_is[SWITCHKINS_MAX_TYPES];

    hal_real_t gui_x;
    hal_real_t gui_y;
    hal_real_t gui_z;
    hal_real_t gui_a;
    hal_real_t gui_b;
    hal_real_t gui_c;
} *swdata;

// Note: parallel kinematics (like genhexkins) often
//       use iterative method for Forward algorithm
//       and require an initial EmcPose.
//       If   fwd_iterates_mask is set
//       then save/use the lastpose
static int     fwd_iterates[SWITCHKINS_MAX_TYPES] = {0};
static bool    use_lastpose[SWITCHKINS_MAX_TYPES] = {0};
static EmcPose lastpose[SWITCHKINS_MAX_TYPES];

static void save_lastpose(int ktype, EmcPose* pos)
{
    lastpose[ktype].tran.x = pos->tran.x;
    lastpose[ktype].tran.y = pos->tran.y;
    lastpose[ktype].tran.z = pos->tran.z;
    lastpose[ktype].a      = pos->a;
    lastpose[ktype].b      = pos->b;
    lastpose[ktype].c      = pos->c;
    lastpose[ktype].u      = pos->u;
    lastpose[ktype].v      = pos->v;
    lastpose[ktype].w      = pos->w;
} // save_lastpose()

static void get_lastpose(int ktype, EmcPose* pos)
{
    pos->tran.x = lastpose[ktype].tran.x;
    pos->tran.y = lastpose[ktype].tran.y;
    pos->tran.z = lastpose[ktype].tran.z;
    pos->a      = lastpose[ktype].a;
    pos->b      = lastpose[ktype].b;
    pos->c      = lastpose[ktype].c;
    pos->u      = lastpose[ktype].u;
    pos->v      = lastpose[ktype].v;
    pos->w      = lastpose[ktype].w;
} // get_lastpose()

// the block sees the pins as they are now, and the type asked for
static void read_block(int ktype)
{
    rt_params.ktype = ktype;
    kinsParamsPinsRead(pins, kp.params, kp.nparams, &rt_params);
}

static void write_block(int ktype)
{
    kinsParamsPinsWrite(pins, kp.params, kp.nparams, &rt_scratch[ktype]);
}

// the forward of one type, whichever way it was provided
static int call_forward(int ktype, const double *joint, EmcPose *pos,
                        const KINEMATICS_FORWARD_FLAGS *fflags,
                        KINEMATICS_INVERSE_FLAGS *iflags)
{
    int r;
    if (kops[ktype]) {
        read_block(ktype);
        r = kinsOpsForward(kops[ktype], &rt_params, &rt_scratch[ktype],
                           joint, pos, fflags, iflags);
        write_block(ktype);
        return r;
    }
    if (!kfwds[ktype]) { return -1; }
    return kfwds[ktype](joint, pos, fflags, iflags);
}

static int gui_forward_kins(const double *joints)
{
    // the hexapod vismach gui uses these hal pins to
    // display platform position/orientation in both
    // genhexkins and identity kinematic types
    // (similar needs for many parallel kinemtic machines)
    int res;
    KINEMATICS_FORWARD_FLAGS  fflags = 0;
    KINEMATICS_INVERSE_FLAGS  iflags;
    if (   kp.gui_kinstype < 0
        || kp.gui_kinstype >= kins_count
        || (!kfwds[kp.gui_kinstype] && !kops[kp.gui_kinstype])) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "gui_forward_kins BAD gui_kinstype <%d>\n",
                        kp.gui_kinstype);
        return -1;
    }
    res = call_forward(kp.gui_kinstype, joints, &lastpose[kp.gui_kinstype],
                       &fflags, &iflags);
    hal_set_real(swdata->gui_x, lastpose[kp.gui_kinstype].tran.x);
    hal_set_real(swdata->gui_y, lastpose[kp.gui_kinstype].tran.y);
    hal_set_real(swdata->gui_z, lastpose[kp.gui_kinstype].tran.z);
    hal_set_real(swdata->gui_a, lastpose[kp.gui_kinstype].a);
    hal_set_real(swdata->gui_b, lastpose[kp.gui_kinstype].b);
    hal_set_real(swdata->gui_c, lastpose[kp.gui_kinstype].c);
    return res;
} // gui_forward_kins

//*********************************************************************
int kinematicsSwitchable() {return 1;}

int kinematicsSwitch(int new_switchkins_type)
{
    int k;

    // reject first, so a bad request leaves the running kinematics alone
    if (new_switchkins_type < 0 || new_switchkins_type >= kins_count) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "kinematicsSwitch:BAD VALUE <%d>\n",
                        new_switchkins_type);
        return -1; // FAIL
    }

    for (k=0; k< SWITCHKINS_MAX_TYPES; k++) { use_lastpose[k] = 0;}

    switchkins_type = new_switchkins_type;

    rtapi_print_msg(RTAPI_MSG_INFO,
                    "kinematicsSwitch:TYPE%d\n", switchkins_type);
    for (k=0; k < kins_count; k++) {
        hal_set_bool(swdata->kinstype_is[k], k == switchkins_type);
    }

    if (fwd_iterates[switchkins_type]) {
        use_lastpose[switchkins_type] = 1; // restarting a kins types
    }
    // a pure type keeps the same restart pose in its own scratch
    if (kops[switchkins_type] && kops[switchkins_type]->fwd_iterates) {
        rt_scratch[switchkins_type].have_pose_seed = 1;
    }
    return 0; // 0==> no error
} // kinematicsSwitch()

int kinematicsForward(const double *joint,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    int r;

    if (   switchkins_type < 0
        || switchkins_type >= kins_count
        || (!kfwds[switchkins_type] && !kops[switchkins_type])) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: Forward BAD switchkins_type </%d>\n",
                        switchkins_type);
        return -1;
    }

    if (kops[switchkins_type]) {
        r = call_forward(switchkins_type, joint, pos, fflags, iflags);
    } else {
        if (fwd_iterates[switchkins_type] && use_lastpose[switchkins_type]) {
            // initialize iterative forward kins (ok for identity too)
            get_lastpose(switchkins_type,pos);
            use_lastpose[switchkins_type] = 0;
        }
        r = kfwds[switchkins_type](joint, pos, fflags, iflags);
        if (fwd_iterates[switchkins_type]) {save_lastpose(switchkins_type,pos);}
    }
    if (r) return r;

    // gui.* pins created only if gui_kinstype>=0
    // consider alternate implementations for gui_forward_kins():
    //  a) always call and use -1 to select default 0 type
    if (kp.gui_kinstype >=0) {
        // create gui pins for a vismach gui using the
        // kins type specified by kp.gui_kinstype;
        // currently the skgui pins are only needed for
        // the hexagui vismach program (as it needs
        // world coords for switchkin-types
        r = gui_forward_kins(joint);
    }

    return r;
} // kinematicsForward()

int kinematicsInverse(const EmcPose * pos,
                      double *joint,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    int r;

    if (   switchkins_type < 0
        || switchkins_type >= kins_count
        || (!kinvs[switchkins_type] && !kops[switchkins_type])) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: Inverse BAD switchkins_type </%d>\n",
                        switchkins_type);
        return -1;
    }
    if (kops[switchkins_type]) {
        read_block(switchkins_type);
        r = kinsOpsInverse(kops[switchkins_type], &rt_params,
                           &rt_scratch[switchkins_type],
                           pos, joint, iflags, fflags);
        write_block(switchkins_type);
        return r;
    }
    r = kinvs[switchkins_type](pos, joint, iflags, fflags);
    return r;
} // kinematicsInverse()

int kinematicsToolFrame(const double *joint,
                        PmRotationMatrix *rot,
                        const KINEMATICS_FORWARD_FLAGS *fflags)
{
    int r;

    if (switchkins_type < 0 || switchkins_type >= kins_count) { return -1; }
    if (kops[switchkins_type]) {
        read_block(switchkins_type);
        return kinsOpsToolFrame(kops[switchkins_type], &rt_params,
                                joint, rot, fflags);
    }
    if (!ktools[switchkins_type]) {
        return -1; // this type does not supply one; not an error
    }
    r = ktools[switchkins_type](joint, rot, fflags);
    if (r) { return r; }

    // the type answers in its own frame; put it in the convention here so
    // no module has to get the half turn right for itself
    return toolFrameApplyNative(rot, &knative[switchkins_type]);
} // kinematicsToolFrame()

int kinematicsWorkFrame(const double *joint,
                        PmRotationMatrix *rot,
                        const KINEMATICS_FORWARD_FLAGS *fflags)
{
    if (switchkins_type < 0 || switchkins_type >= kins_count) { return -1; }
    if (kops[switchkins_type]) {
        read_block(switchkins_type);
        return kinsOpsWorkFrame(kops[switchkins_type], &rt_params,
                                joint, rot, fflags);
    }
    if (!kworks[switchkins_type]) {
        return -1; // this type does not supply one; not an error
    }
    // no native rotation here: the work frame has no tool axis to point the
    // wrong way, so there are not two conventions for it to be caught between
    return kworks[switchkins_type](joint, rot, fflags);
} // kinematicsWorkFrame()

int kinematicsToolFrameInverse(const PmCartesian *axis_in_work,
                               const PmCartesian *x_in_work,
                               const double *seed,
                               unsigned int held,
                               double *solutions,
                               int max_solutions,
                               int *free_directions,
                               double *tool_spin)
{
    if (switchkins_type < 0 || switchkins_type >= kins_count) { return -1; }
    if (kops[switchkins_type]) {
        if (!kops[switchkins_type]->tool || !kops[switchkins_type]->work) {
            return -1; // this type does not report its frames, so it cannot answer
        }
    } else if (!ktools[switchkins_type] || !kworks[switchkins_type]) {
        return -1; // this type does not report its frames, so it cannot answer
    }

    // a type that derived the answer by hand knows its own degenerate poses
    // and is faster than a search, so it wins where it exists
    if (ktinvs[switchkins_type]) {
        return ktinvs[switchkins_type](axis_in_work, x_in_work, seed, held,
                                       solutions, max_solutions,
                                       free_directions, tool_spin);
    }

    // the dispatch itself is what the search calls, so the native rotation
    // and the per-type lookup are already accounted for
    return toolFrameSolve(kinematicsWorkFrame, kinematicsToolFrame,
                          kp.max_joints,
                          axis_in_work, x_in_work, seed, held,
                          solutions, max_solutions, free_directions,
                          tool_spin);
} // kinematicsToolFrameInverse()

int kinematicsJacobian(const double *joint,
                       const EmcPose *world,
                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                       const KINEMATICS_INVERSE_FLAGS *iflags)
{
    if (switchkins_type < 0 || switchkins_type >= kins_count) {
        return -1;
    }
    if (kops[switchkins_type]) {
        read_block(switchkins_type);
        return kinsOpsJacobian(kops[switchkins_type], &rt_params,
                               &rt_scratch[switchkins_type],
                               joint, world, jac, iflags);
    }
    // a closed form is exact and knows its own singular poses
    if (kjacs[switchkins_type]) {
        return kjacs[switchkins_type](joint, world, jac, iflags);
    }
    // otherwise the type's own inverse, differenced.  The type function
    // rather than the dispatch, so this cannot recurse through a switch.
    if (!kinvs[switchkins_type]) { return -1; }
    return kinsJacobianFromInverse(kinvs[switchkins_type], kp.max_joints,
                                   joint, world, iflags, jac);
} // kinematicsJacobian()

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

int switchkinsRegister(int ktype, KS kset, KF kfwd, KI kinv)
{
    if (ktype < 0 || ktype >= SWITCHKINS_MAX_TYPES) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegister: BAD switchkins_type <%d>"
                        " (must be 0..%d)\n",
                        ktype, SWITCHKINS_MAX_TYPES - 1);
        register_error = 1;
        return -1;
    }
    if (ksetups[ktype] || kfwds[ktype] || kinvs[ktype] || kops[ktype]) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegister: switchkins-type %d"
                        " already provided\n", ktype);
        register_error = 1;
        return -1;
    }
    ksetups[ktype] = kset;
    kfwds[ktype]   = kfwd;
    kinvs[ktype]   = kinv;
    return 0;
} // switchkinsRegister()

int switchkinsRegisterOps(int ktype, const kins_ops *ops)
{
    if (ktype < 0 || ktype >= SWITCHKINS_MAX_TYPES) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterOps: BAD switchkins_type <%d>"
                        " (must be 0..%d)\n",
                        ktype, SWITCHKINS_MAX_TYPES - 1);
        register_error = 1;
        return -1;
    }
    if (ksetups[ktype] || kfwds[ktype] || kinvs[ktype] || kops[ktype]) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterOps: switchkins-type %d"
                        " already provided\n", ktype);
        register_error = 1;
        return -1;
    }
    if (!ops || !ops->forward || !ops->inverse) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterOps: switchkins-type %d"
                        " has no forward or inverse\n", ktype);
        register_error = 1;
        return -1;
    }
    if (ops->tool && ops->native && !toolFrameIsProper(ops->native)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterOps: switchkins-type %d"
                        " declared a rotation that is not orthonormal with"
                        " determinant +1\n", ktype);
        register_error = 1;
        return -1;
    }
    kops[ktype] = ops;
    return 0;
} // switchkinsRegisterOps()

int switchkinsRegisterFrames(int ktype, KT kwork, KT ktool,
                             const PmRotationMatrix *native)
{
    if (ktype < 0 || ktype >= SWITCHKINS_MAX_TYPES) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterFrames: BAD switchkins_type <%d>"
                        " (must be 0..%d)\n",
                        ktype, SWITCHKINS_MAX_TYPES - 1);
        register_error = 1;
        return -1;
    }
    // check the declared rotation once here rather than on every call
    if (!native || !toolFrameIsProper(native)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterFrames: switchkins-type %d"
                        " declared a rotation that is not orthonormal with"
                        " determinant +1\n", ktype);
        register_error = 1;
        return -1;
    }
    kworks[ktype]  = kwork;
    ktools[ktype]  = ktool;
    knative[ktype] = *native;
    return 0;
} // switchkinsRegisterFrames()

int switchkinsRegisterJacobian(int ktype, KJ kjac)
{
    if (ktype < 0 || ktype >= SWITCHKINS_MAX_TYPES) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterJacobian: BAD switchkins_type"
                        " <%d> (must be 0..%d)\n",
                        ktype, SWITCHKINS_MAX_TYPES - 1);
        register_error = 1;
        return -1;
    }
    kjacs[ktype] = kjac;
    return 0;
} // switchkinsRegisterJacobian()

int switchkinsRegisterToolFrameInverse(int ktype, KTI kinv)
{
    if (ktype < 0 || ktype >= SWITCHKINS_MAX_TYPES) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkinsRegisterToolFrameInverse: BAD"
                        " switchkins_type <%d> (must be 0..%d)\n",
                        ktype, SWITCHKINS_MAX_TYPES - 1);
        register_error = 1;
        return -1;
    }
    ktinvs[ktype] = kinv;
    return 0;
} // switchkinsRegisterToolFrameInverse()

EXPORT_SYMBOL(kinematicsSwitchable);
EXPORT_SYMBOL(kinematicsSwitch);
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsToolFrame);
EXPORT_SYMBOL(kinematicsWorkFrame);
EXPORT_SYMBOL(kinematicsToolFrameInverse);
EXPORT_SYMBOL(kinematicsJacobian);
EXPORT_SYMBOL(switchkinsRegister);
EXPORT_SYMBOL(switchkinsRegisterFrames);
EXPORT_SYMBOL(switchkinsRegisterToolFrameInverse);
EXPORT_SYMBOL(switchkinsRegisterJacobian);
EXPORT_SYMBOL(switchkinsRegisterOps);
EXPORT_SYMBOL(switchkinsInit);
EXPORT_SYMBOL(switchkinsDescribe);
EXPORT_SYMBOL(switchkinsDescribeSetup);

//*********************************************************************
// the module as registered so far, described for a caller outside RT
int switchkinsDescribeSetup(const kparms *k, kins_module_info *info)
{
    int i, n = 0;

    if (!k || !info) { return -1; }
    if (k->nparams < 0 || k->nparams > KINS_MAX_PARAMS
        || (k->nparams > 0 && !k->params)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: %s declares a bad parameter table\n",
                        k->kinsname ? k->kinsname : "?");
        return -1;
    }
    memset(info, 0, sizeof(*info));
    info->name                 = k->kinsname;
    info->halprefix            = k->halprefix ? k->halprefix : k->kinsname;
    info->params               = k->params;
    info->nparams              = k->nparams;
    info->required_coordinates = k->required_coordinates;
    info->max_joints           = k->max_joints;
    info->allow_duplicates     = k->allow_duplicates;
    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) {
        info->ops[i] = kops[i];
        if (ksetups[i] || kfwds[i] || kinvs[i] || kops[i]) { n = i + 1; }
    }
    info->ntypes = n;
    return 0;
} // switchkinsDescribeSetup()

int switchkinsDescribe(kins_module_info *info)
{
    if (!inited) { return -1; }
    return switchkinsDescribeSetup(&kp, info);
} // switchkinsDescribe()

//*********************************************************************
// The caller owns the hal component: it does hal_init() before this and
// hal_ready() after it.  Every switchkins-type must be registered by
// now.
int switchkinsInit(const int   comp_id,
                   kparms*     ksetup_parms,
                   const char* coordinates)
{
    int i;
    int res = 0;
    char* emsg = "other";

    kp = *ksetup_parms; // kinematics parms are needed after this returns

    if (register_error) {emsg = "switchkinsRegister FAIL"; goto error;}

    // an identity type answers the tool frame the same way whichever module
    // asked for it, so supply it here rather than in every switchkinsSetup()
    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) {
        if (!ktools[i] && kfwds[i] == identityKinematicsForward) {
            kworks[i]  = identityKinematicsWorkFrame;
            ktools[i]  = identityKinematicsToolFrame;
            knative[i] = TOOL_FRAME_SPINDLE;
        }
        // and its Jacobian is exact, so do not difference for it
        if (!kjacs[i] && kfwds[i] == identityKinematicsForward) {
            kjacs[i] = identityKinematicsJacobian;
        }
    }

    // the highest type registered sets the count
    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) {
        if (ksetups[i] || kfwds[i] || kinvs[i] || kops[i]) { kins_count = i + 1; }
    }
    if (!kins_count) { emsg = "no switchkins-types provided"; goto error; }

    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) {
       if (kp.fwd_iterates_mask & (1<<i)) {
           fwd_iterates[i] = 1;
           rtapi_print("switchkins-type %d: fwd_iterates\n",i);
       }
    }

    if (!kp.kinsname) { emsg = "Missing kinsname"; goto error; }

    if (!kp.halprefix) {
        kp.halprefix  = kp.kinsname;
        rtapi_print("Missing halprefix, using \"%s\"\n",kp.halprefix);
    }

    if (kp.max_joints <= 0 || kp.max_joints > EMCMOT_MAX_JOINTS) {
        emsg = "bogus max_joints"; goto error;
    }
    if (kp.gui_kinstype >= kins_count) {
        emsg = "bogus gui_kinstype"; goto error;
    }

    // a type left out below the highest one provided is a gap, not a count
    for (i=0; i < kins_count; i++) {
        if (kops[i]) { continue; }
        if (ksetups[i] && kfwds[i] && kinvs[i]) { continue; }
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: switchkins-type %d incomplete:%s%s%s\n",
                        i,
                        ksetups[i] ? "" : " no setup",
                        kfwds[i]   ? "" : " no forward",
                        kinvs[i]   ? "" : " no inverse");
        emsg = "incomplete switchkins-type"; goto error;
    }

    swdata = hal_malloc(sizeof(struct swdata));
    if (!swdata) {emsg = "hal_malloc fail"; goto error;}

    for (i=0; i < kins_count; i++) {
        res += hal_pin_new_bool(comp_id, HAL_OUT, &(swdata->kinstype_is[i]),
                                0, "kinstype.is-%d", i);
    }

    if (kp.gui_kinstype >=0) {
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_x, 0.0, "skgui.x");
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_y, 0.0, "skgui.y");
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_z, 0.0, "skgui.z");
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_a, 0.0, "skgui.a");
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_b, 0.0, "skgui.b");
        res += hal_pin_new_real(comp_id, HAL_IN, &swdata->gui_c, 0.0, "skgui.c");
    }
    if (res) {emsg = "hal pin create fail"; goto error;}

    switchkins_type = 0; // startup with default type
    kinematicsSwitch(switchkins_type);

    if (!coordinates) {coordinates = kp.required_coordinates;}

    // the pure types share one block and one set of pins from the table
    if (kp.params || kp.nparams) {
        kins_module_info mi;
        if (switchkinsDescribeSetup(&kp, &mi)) { emsg = "bad table"; goto error; }
        if (kinsParamsInit(&rt_params, &mi, coordinates)) {
            emsg = "coordinates"; goto error;
        }
        if (kinsParamsPinsCreate(comp_id, kp.halprefix, kp.params, kp.nparams,
                                 &pins)) {
            emsg = "table pin create fail"; goto error;
        }
    } else {
        for (i=0; i < kins_count; i++) {
            if (kops[i]) {
                kins_module_info mi;
                if (switchkinsDescribeSetup(&kp, &mi)) { emsg = "bad table"; goto error; }
                if (kinsParamsInit(&rt_params, &mi, coordinates)) {
                    emsg = "coordinates"; goto error;
                }
                break;
            }
        }
    }
    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) { kinsScratchInit(&rt_scratch[i]); }

    for (i=0; i < kins_count; i++) {
        if (ksetups[i]) { ksetups[i](comp_id,coordinates,&kp); }
    }

    inited = 1;
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,
        "\nSwitchkins FAIL %s:<%s>\n",kp.kinsname,emsg);
    return -1;
} // switchkinsInit()
