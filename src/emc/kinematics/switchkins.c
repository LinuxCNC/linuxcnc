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
#include <rtapi_app.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <kinematics.h>

#include "switchkins.h"

//*********************************************************************
// kinematic functions (default=0 for err detection):
static kparms kp; // kinematics parms (common all types)

// indexed by switchkins_type (NULL==not provided, for err detection):
static KS ksetups[SWITCHKINS_MAX_TYPES] = {NULL};
static KF kfwds[SWITCHKINS_MAX_TYPES]   = {NULL};
static KI kinvs[SWITCHKINS_MAX_TYPES]   = {NULL};

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
        || !kfwds[kp.gui_kinstype]) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "gui_forward_kins BAD gui_kinstype <%d>\n",
                        kp.gui_kinstype);
        return -1;
    }
    res = kfwds[kp.gui_kinstype](joints, &lastpose[kp.gui_kinstype],
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
    return 0; // 0==> no error
} // kinematicsSwitch()

int kinematicsForward(const double *joint,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    int r;

    if (fwd_iterates[switchkins_type] && use_lastpose[switchkins_type]) {
        // initialize iterative forward kins (ok for identity too)
        get_lastpose(switchkins_type,pos);
        use_lastpose[switchkins_type] = 0;
    }

    if (   switchkins_type < 0
        || switchkins_type >= kins_count
        || !kfwds[switchkins_type]) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: Forward BAD switchkins_type </%d>\n",
                        switchkins_type);
        return -1;
    }
    r = kfwds[switchkins_type](joint, pos, fflags, iflags);
    if (fwd_iterates[switchkins_type]) {save_lastpose(switchkins_type,pos);}
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
        || !kinvs[switchkins_type]) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: Inverse BAD switchkins_type </%d>\n",
                        switchkins_type);
        return -1;
    }
    r = kinvs[switchkins_type](pos, joint, iflags, fflags);
    return r;
} // kinematicsInverse()

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
    if (ksetups[ktype] || kfwds[ktype] || kinvs[ktype]) {
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

//*********************************************************************
static char *coordinates;
RTAPI_MP_STRING(coordinates, "Axes-to-joints-ordering");
static char *sparm;
RTAPI_MP_STRING(sparm,  "switchkins module-specific parameter");

EXPORT_SYMBOL(kinematicsSwitchable);
EXPORT_SYMBOL(kinematicsSwitch);
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(switchkinsRegister);
MODULE_LICENSE("GPL");

static int    comp_id;
//*********************************************************************
int rtapi_app_main(void)
{
    int i,res;
    char* emsg="other";

    // defaults prior to switchkinsSetup() call
    kp.kinsname   = NULL;
    kp.halprefix  = NULL;
    kp.required_coordinates = "";
    kp.max_joints        =  0; // Setup must supply
    kp.allow_duplicates  =  0;
    kp.fwd_iterates_mask =  0;
    kp.gui_kinstype      = -1; // negative means: not used

    kp.sparm = sparm; // module parm passed to kins

    // may also call switchkinsRegister()
    res = switchkinsSetup(&kp,
                          &ksetups[0], &ksetups[1], &ksetups[2],
                          &kfwds[0],   &kfwds[1],   &kfwds[2],
                          &kinvs[0],   &kinvs[1],   &kinvs[2]);
    if (res) {emsg="switchkinsSetp FAIL"; goto error;}
    if (register_error) {emsg="switchkinsRegister FAIL"; goto error;}

    // the highest type provided by either route sets the count
    for (i=0; i < SWITCHKINS_MAX_TYPES; i++) {
        if (ksetups[i] || kfwds[i] || kinvs[i]) { kins_count = i + 1; }
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
        if (ksetups[i] && kfwds[i] && kinvs[i]) { continue; }
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "switchkins: switchkins-type %d incomplete:%s%s%s\n",
                        i,
                        ksetups[i] ? "" : " no setup",
                        kfwds[i]   ? "" : " no forward",
                        kinvs[i]   ? "" : " no inverse");
        emsg = "incomplete switchkins-type"; goto error;
    }

    comp_id = hal_init(kp.kinsname);
    if(comp_id < 0) goto error;

    swdata = hal_malloc(sizeof(struct swdata));
    if (!swdata) goto error;

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
        if (res) {emsg = "hal pin create fail";goto error;}
    }

    switchkins_type = 0; // startup with default type
    kinematicsSwitch(switchkins_type);

    if (!coordinates) {coordinates = kp.required_coordinates;}

    for (i=0; i < kins_count; i++) {
        ksetups[i](comp_id,coordinates,&kp);
    }

    hal_ready(comp_id);
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,
        "\nSwitchkins FAIL %s:<%s>\n",kp.kinsname,emsg);
    hal_exit(comp_id);
    return -1;
} // rtapi_app_main()

void rtapi_app_exit(void) { hal_exit(comp_id); }
