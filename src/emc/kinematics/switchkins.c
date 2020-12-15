//#include <stdio.h>

/* switchkins.c provide for switchable kins modules:
*    rtapi_app()
*    rtapi_exit()
*    kinematicsType()
*    kinematicsForward()
*    kinematicsInverse()
*    kinematicsSwitch()
*    kinematicsSwitchable()
*  setup using: switchkinsSetup()
*/
#include "motion.h"
#include "hal.h"
#include "rtapi_app.h"
#include "kinematics.h"
#include "switchkins.h"

//*********************************************************************
// kinematic functions (default=0 for err detection):
static kparms kp; // kinematics parms (common all types)

static KF kfwd0 = 0; // 0==switchkins_type kinematics forward
static KF kfwd1 = 0; // 1
static KF kfwd2 = 0; // 2

static KI kinv0 = 0; // 0==switchkins_type kinematics inverse
static KI kinv1 = 0; // 1
static KI kinv2 = 0; // 2

static hal_u32_t switchkins_type;
static struct swdata {
    hal_bit_t   *kinstype_is_0;
    hal_bit_t   *kinstype_is_1;
    hal_bit_t   *kinstype_is_2;

    hal_float_t *gui_x;
    hal_float_t *gui_y;
    hal_float_t *gui_z;
    hal_float_t *gui_a;
    hal_float_t *gui_b;
    hal_float_t *gui_c;
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
    KINEMATICS_FORWARD_FLAGS  fflags;
    KINEMATICS_INVERSE_FLAGS  iflags;
    switch (kp.gui_kinstype) {
        case 0: res = kfwd0(joints, &lastpose[0], &fflags, &iflags);break;
        case 1: res = kfwd1(joints, &lastpose[1], &fflags, &iflags);break;
        case 2: res = kfwd2(joints, &lastpose[2], &fflags, &iflags);break;
       default: rtapi_print_msg(RTAPI_MSG_ERR,
                  "gui_forward_kins BAD gui_kinstype <%d>\n",
                  kp.gui_kinstype);
                  return -1;
     }
    *swdata->gui_x = lastpose[kp.gui_kinstype].tran.x;
    *swdata->gui_y = lastpose[kp.gui_kinstype].tran.y;
    *swdata->gui_z = lastpose[kp.gui_kinstype].tran.z;
    *swdata->gui_a = lastpose[kp.gui_kinstype].a;
    *swdata->gui_b = lastpose[kp.gui_kinstype].b;
    *swdata->gui_c = lastpose[kp.gui_kinstype].c;
    return res;
} // gui_forward_kins

//*********************************************************************
int kinematicsSwitchable() {return 1;}

int kinematicsSwitch(int new_switchkins_type)
{
    int k;
    for (k=0; k< SWITCHKINS_MAX_TYPES; k++) { use_lastpose[k] = 0;}

    switchkins_type = new_switchkins_type;
    switch (switchkins_type) {
        case 0: rtapi_print_msg(RTAPI_MSG_INFO,
                "kinematicsSwitch:TYPE0\n");
                *swdata->kinstype_is_0 = 1;
                *swdata->kinstype_is_1 = 0;
                *swdata->kinstype_is_2 = 0;
                break;
        case 1: rtapi_print_msg(RTAPI_MSG_INFO,
                "kinematicsSwitch:TYPE1\n");
                *swdata->kinstype_is_1 = 1;
                *swdata->kinstype_is_0 = 0;
                *swdata->kinstype_is_2 = 0;
                break;
        case 2: rtapi_print_msg(RTAPI_MSG_INFO,
                "kinematicsSwitch:TYPE2\n");
                *swdata->kinstype_is_0 = 0;
                *swdata->kinstype_is_1 = 0;
                *swdata->kinstype_is_2 = 1;
                break;
       default: rtapi_print_msg(RTAPI_MSG_ERR,
                "kinematicsSwitch:BAD VALUE <%d>\n",
                switchkins_type);
                *swdata->kinstype_is_1 = 0;
                *swdata->kinstype_is_0 = 0;
                *swdata->kinstype_is_2 = 0;
                return -1; // FAIL
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

    switch (switchkins_type) {
       case 0: r = kfwd0(joint, pos, fflags, iflags); break;
       case 1: r = kfwd1(joint, pos, fflags, iflags); break;
       case 2: r = kfwd2(joint, pos, fflags, iflags); break;
      default: rtapi_print_msg(RTAPI_MSG_ERR,
                    "switchkins: Forward BAD switchkins_type </%d>\n",
                    switchkins_type);
               return -1;
    }
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

    switch (switchkins_type) {
       case 0: r = kinv0(pos, joint, iflags, fflags); break;
       case 1: r = kinv1(pos, joint, iflags, fflags); break;
       case 2: r = kinv2(pos, joint, iflags, fflags); break;
       default: rtapi_print_msg(RTAPI_MSG_ERR,
                     "switchkins: Inverse BAD switchkins_type </%d>\n",
                     switchkins_type);
               return -1;
    }
    return r;
} // kinematicsInverse()

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

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
MODULE_LICENSE("GPL");

static int    comp_id;
//*********************************************************************
int rtapi_app_main(void)
{
    #define DISALLOW_DUPLICATES 0
    int i,res;
    char* emsg="other";

    // defaults prior to switchkinsSetup() call
    kp.kinsname   = NULL;
    kp.halprefix  = NULL;
    kp.max_joints = 0;
    kp.required_coordinates = "";
    kp.fwd_iterates_mask = 0;
    kp.gui_kinstype = -1; // negative: not used

    kp.sparm = sparm; // module parm passed to kins

    KS ksetup0 = 0;
    KS ksetup1 = 0;
    KS ksetup2 = 0;

    res = switchkinsSetup(&kp,
                          &ksetup0, &ksetup1, &ksetup2,
                          &kfwd0,   &kfwd1,   &kfwd2,
                          &kinv0,   &kinv1,   &kinv2);
    if (res) {emsg="switchkinsSetp FAIL"; goto error;}

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

    if (kp.max_joints < 0 || kp.max_joints > EMCMOT_MAX_JOINTS) {
        emsg = "bogus max_joints"; goto error;
    }
    if (kp.gui_kinstype >= SWITCHKINS_MAX_TYPES) {
        emsg = "bogus gui_kinstype"; goto error;
    }

    if (!ksetup0 || !ksetup1 || !ksetup2) {
        emsg = "Missing setup function"; goto error;
    }
    if (!kfwd0 || !kfwd1 || !kfwd2) {
        emsg = "Missing fwd functionn"; goto error;
    }
    if (!kinv0 || !kinv1 || !kinv2) {
        emsg =  "Missing inv function"; goto error;
    }

    comp_id = hal_init(kp.kinsname);
    if(comp_id < 0) goto error;

    swdata = hal_malloc(sizeof(struct swdata));
    if (!swdata) goto error;

    res += hal_pin_bit_new("kinstype.is-0", HAL_OUT, &(swdata->kinstype_is_0), comp_id);
    res += hal_pin_bit_new("kinstype.is-1", HAL_OUT, &(swdata->kinstype_is_1), comp_id);
    res += hal_pin_bit_new("kinstype.is-2", HAL_OUT, &(swdata->kinstype_is_2), comp_id);

    if (kp.gui_kinstype >=0) {
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_x, comp_id, "skgui.x");
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_y, comp_id, "skgui.y");
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_z, comp_id, "skgui.z");
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_a, comp_id, "skgui.a");
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_b, comp_id, "skgui.b");
        res += hal_pin_float_newf(HAL_IN, &swdata->gui_c, comp_id, "skgui.c");
        if (res) {emsg = "hal pin create fail";goto error;}
    }

    switchkins_type = 0; // startup with default type
    kinematicsSwitch(switchkins_type);

    if (!coordinates) {coordinates = kp.required_coordinates;}

    ksetup0(comp_id,coordinates,&kp);
    ksetup1(comp_id,coordinates,&kp);
    ksetup2(comp_id,coordinates,&kp);

    hal_ready(comp_id);
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,
        "\nSwitchkins FAIL %s:<%s>\n",kp.kinsname,emsg);
    hal_exit(comp_id);
    return -1;
} // rtapi_app_main()

void rtapi_app_exit(void) { hal_exit(comp_id); }
