/********************************************************************
* Description: twinspindlekins.c
*   kinematics for a mill-turn with a tilting head and two spindles
*
* License: GPL Version 2
*
* Notes:
*  1) The head carries the tool on X Y Z and tilts it on B, about Y, from
*     a pivot pivot-length above the gauge line.  With B at 0 the tool
*     points along -Z, tool axis +Z, and the slides read the gauge line.
*     The pose is the tip plus the tool length along the part's Z, which
*     G43 takes off again, so a tilt moves the tip by the length along
*     the tool axis less the length along Z the offset stands for.
*  2) The main spindle turns the work about machine Z on C, its face in
*     the plane Z 0.  The sub spindle faces it from spindle-distance along
*     +Z and turns its own work on W, which the INI makes ANGULAR.
*  3) Type 0 works on the part in the main spindle and type 1 on the part
*     in the sub spindle, each in that part's own frame: X Y Z of the part
*     and B, C or W the orientation.  Both are primary: G12.1 picks the
*     spindle.  Type 2 is the identity and stands in for the machine frame.
*  4) The sub spindle part frame is the main spindle's turned half a turn
*     about X: X stays, Y and Z reverse, so Z still points out of the face
*     towards the tool.  Moved to the other spindle, a program reads the
*     same.
*  5) The rotary directions are the conventional ones: C and W turn the
*     work the way that turns the tool positively about the part's Z.
*  6) Coordinates XYZBCW are required, A U V may be added with the
*     coordinates parameter and map one to one to their joints.
********************************************************************/

#define REQUIRED_COORDINATES "XYZBCW"

#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <emcmotcfg.h>

#include <switchkins.h>

static const kins_param_desc twin_params[] = {
    { "pivot-length",     KINS_PARAM_FLOAT, KINS_IN, 0, 150.0 },
    { "tool-length",      KINS_PARAM_FLOAT, KINS_IN, 1,   0.0 },
    { "spindle-distance", KINS_PARAM_FLOAT, KINS_IN, 0, 500.0 },
};
enum { P_PIVOT_LENGTH, P_TOOL_LENGTH, P_SPINDLE_DISTANCE };

#define JX (p->joint_of_axis[0])
#define JY (p->joint_of_axis[1])
#define JZ (p->joint_of_axis[2])
#define JA (p->joint_of_axis[3])
#define JB (p->joint_of_axis[4])
#define JC (p->joint_of_axis[5])
#define JU (p->joint_of_axis[6])
#define JV (p->joint_of_axis[7])
#define JW (p->joint_of_axis[8])

// the tool tip in the machine frame, from the slides and B (note 1)
static PmCartesian tip_from_joints(const kins_params *p, const double *joints)
{
    const double P = p->geometry[P_PIVOT_LENGTH];
    const double L = P + p->tool.tran.z;
    const double b = joints[JB]*TO_RAD;
    PmCartesian t;

    t.x = joints[JX] - L*sin(b);
    t.y = joints[JY];
    t.z = joints[JZ] + P - L*cos(b);
    return t;
} // tip_from_joints()

// the part frame of each spindle, the machine seen from the part: machine
// = O + R part, with R = Rz(-C) for the main spindle and Rx(180) Rz(-W) for
// the sub spindle (notes 2, 4, 5)
static PmCartesian part_from_machine(const kins_params *p, int sub,
                                     double angle, PmCartesian m)
{
    const double s = sin(angle*TO_RAD), c = cos(angle*TO_RAD);
    PmCartesian q, r;

    q = m;
    if (sub) {
        q.y = -m.y;
        q.z = p->geometry[P_SPINDLE_DISTANCE] - m.z;
    }
    r.x = c*q.x - s*q.y;
    r.y = s*q.x + c*q.y;
    r.z = q.z;
    return r;
} // part_from_machine()

static PmCartesian machine_from_part(const kins_params *p, int sub,
                                     double angle, PmCartesian r)
{
    const double s = sin(angle*TO_RAD), c = cos(angle*TO_RAD);
    PmCartesian q, m;

    q.x =  c*r.x + s*r.y;
    q.y = -s*r.x + c*r.y;
    q.z = r.z;
    m = q;
    if (sub) {
        m.y = -q.y;
        m.z = p->geometry[P_SPINDLE_DISTANCE] - q.z;
    }
    return m;
} // machine_from_part()

static int twin_forward(const kins_params *p, int sub, const double *joints,
                        EmcPose *pos)
{
    const double angle = sub ? joints[JW] : joints[JC];

    pos->tran = part_from_machine(p, sub, angle, tip_from_joints(p, joints));
    pos->tran.z += p->tool.tran.z;
    pos->b = joints[JB];
    pos->c = joints[JC];
    pos->w = joints[JW];

    // optional letters (specify with coordinates module parameter)
    pos->a = (JA != -1)? joints[JA] : 0;
    pos->u = (JU != -1)? joints[JU] : 0;
    pos->v = (JV != -1)? joints[JV] : 0;

    return 0;
} // twin_forward()

static int twin_inverse(const kins_params *p, int sub, const EmcPose *pos,
                        double *joints)
{
    const double T = p->tool.tran.z;
    const double L = p->geometry[P_PIVOT_LENGTH] + T;
    const double b = pos->b*TO_RAD;
    PmCartesian tip = pos->tran;
    PmCartesian t;
    EmcPose P; // computed position

    tip.z -= T;
    t = machine_from_part(p, sub, sub ? pos->w : pos->c, tip);
    P.tran.x = t.x + L*sin(b);
    P.tran.y = t.y;
    P.tran.z = t.z - p->geometry[P_PIVOT_LENGTH] + L*cos(b);
    P.b = pos->b;
    P.c = pos->c;
    P.w = pos->w;

    // optional letters (specify with coordinates module parameter)
    P.a = (JA != -1)? pos->a : 0;
    P.u = (JU != -1)? pos->u : 0;
    P.v = (JV != -1)? pos->v : 0;

    return kinsPoseToMappedJoints(p, &P, joints);
} // twin_inverse()

static int main_forward(const kins_params *p, kins_scratch *s,
                        const double *joints, EmcPose *pos,
                        const KINEMATICS_FORWARD_FLAGS *fflags,
                        KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)s; (void)fflags; (void)iflags;
    return twin_forward(p, 0, joints, pos);
}

static int main_inverse(const kins_params *p, kins_scratch *s,
                        const EmcPose *pos, double *joints,
                        const KINEMATICS_INVERSE_FLAGS *iflags,
                        KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)s; (void)iflags; (void)fflags;
    return twin_inverse(p, 0, pos, joints);
}

static int sub_forward(const kins_params *p, kins_scratch *s,
                       const double *joints, EmcPose *pos,
                       const KINEMATICS_FORWARD_FLAGS *fflags,
                       KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)s; (void)fflags; (void)iflags;
    return twin_forward(p, 1, joints, pos);
}

static int sub_inverse(const kins_params *p, kins_scratch *s,
                       const EmcPose *pos, double *joints,
                       const KINEMATICS_INVERSE_FLAGS *iflags,
                       KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)s; (void)iflags; (void)fflags;
    return twin_inverse(p, 1, pos, joints);
}

// the tool frame, Ry(B), is the same on both spindles
static int twin_tool_frame(const kins_params *p, const double *joints,
                           PmRotationMatrix *rot,
                           const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    const double sb = sin(joints[JB]*TO_RAD), cb = cos(joints[JB]*TO_RAD);

    rot->x.x =  cb;   rot->y.x = 0;   rot->z.x = sb;
    rot->x.y =  0;    rot->y.y = 1;   rot->z.y = 0;
    rot->x.z = -sb;   rot->y.z = 0;   rot->z.z = cb;

    return 0;
} // twin_tool_frame()

// the work frames, the R of part_from_machine(), its columns the part axes
static int main_work_frame(const kins_params *p, const double *joints,
                           PmRotationMatrix *rot,
                           const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    const double s = sin(joints[JC]*TO_RAD), c = cos(joints[JC]*TO_RAD);

    rot->x.x =  c;   rot->y.x = s;   rot->z.x = 0;
    rot->x.y = -s;   rot->y.y = c;   rot->z.y = 0;
    rot->x.z =  0;   rot->y.z = 0;   rot->z.z = 1;

    return 0;
} // main_work_frame()

static int sub_work_frame(const kins_params *p, const double *joints,
                          PmRotationMatrix *rot,
                          const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)fflags;
    const double s = sin(joints[JW]*TO_RAD), c = cos(joints[JW]*TO_RAD);

    rot->x.x =  c;   rot->y.x =  s;   rot->z.x =  0;
    rot->x.y =  s;   rot->y.y = -c;   rot->z.y =  0;
    rot->x.z =  0;   rot->y.z =  0;   rot->z.z = -1;

    return 0;
} // sub_work_frame()

// each spindle orients with its own rotary, the table first, and B; the
// joint map gives the same letters, declared so the table reads
static const kins_ops main_ops = {
    .forward  = main_forward,
    .inverse  = main_inverse,
    .work     = main_work_frame,
    .tool     = twin_tool_frame,
    .native   = &TOOL_FRAME_SPINDLE,
    .primary  = 1,
    .orient   = "CB",
};

static const kins_ops sub_ops = {
    .forward  = sub_forward,
    .inverse  = sub_inverse,
    .work     = sub_work_frame,
    .tool     = twin_tool_frame,
    .native   = &TOOL_FRAME_SPINDLE,
    .primary  = 1,
    .orient   = "WB",
};

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    (void)kset0; (void)kset1; (void)kset2;
    (void)kfwd0; (void)kfwd1; (void)kfwd2;
    (void)kinv0; (void)kinv1; (void)kinv2;
    kp->kinsname    = "twinspindlekins"; // !!! must agree with filename
    kp->halprefix   = "twinspindlekins"; // hal pin names
    kp->required_coordinates = REQUIRED_COORDINATES;
    kp->allow_duplicates     = 1;
    kp->max_joints           = EMCMOT_MAX_JOINTS;
    kp->params               = twin_params;
    kp->nparams              = sizeof(twin_params)/sizeof(twin_params[0]);

    switchkinsRegisterOps(0, &main_ops);
    switchkinsRegisterOps(1, &sub_ops);
    switchkinsRegisterOps(2, &KINS_IDENTITY_OPS);

    return 0;
} // switchkinsSetup()
