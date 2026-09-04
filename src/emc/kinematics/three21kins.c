#include <rtapi.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>

#include <switchkins.h>

/* default values for ar2 robot */
#define DEFAULT_THREE21_A1 64.2
#define DEFAULT_THREE21_A2 305.0
#define DEFAULT_THREE21_A3 0.0
#define DEFAULT_THREE21_D1 169.77
#define DEFAULT_THREE21_D2 0.0
#define DEFAULT_THREE21_D3 -6.25
#define DEFAULT_THREE21_D4 223.63
#define DEFAULT_THREE21_D6 36.5

#define SINGULAR_FUZZ 0.000001
#define FLAG_FUZZ     0.000001

/* flags for inverse kinematics */
#define THREE21_SHOULDER_RIGHT 0x01
#define THREE21_ELBOW_DOWN     0x02
#define THREE21_WRIST_FLIP     0x04
#define THREE21_SINGULAR       0x08

/* flags for forward kinematics */
#define THREE21_REACH          0x01

// the eight dimensions, one pin each; the maths reads them from the block
static const kins_param_desc three21_params[] = {
    { "A1", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_A1 },
    { "A2", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_A2 },
    { "A3", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_A3 },
    { "D1", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_D1 },
    { "D2", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_D2 },
    { "D3", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_D3 },
    { "D4", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_D4 },
    { "D6", KINS_PARAM_FLOAT, KINS_IN, 0, DEFAULT_THREE21_D6 },
};
enum { P_A1, P_A2, P_A3, P_D1, P_D2, P_D3, P_D4, P_D6 };

/* the difference of two angles, brought into (-pi, pi] so that a joint a
   whole turn from the formula still matches it */
static double angleDiff(double a, double b)
{
   double d = a - b;
   while (d > PM_PI) { d -= 2*PM_PI; }
   while (d <= -PM_PI) { d += 2*PM_PI; }
   return d;
}

static int three21_forward(const kins_params *p, kins_scratch *s,
                           const double * joint,
                           EmcPose * world,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
   (void)s;
   (void)fflags;
   double a1 = p->geometry[P_A1];
   double a2 = p->geometry[P_A2];
   double a3 = p->geometry[P_A3];
   double d1 = p->geometry[P_D1];
   double d2 = p->geometry[P_D2];
   double d3 = p->geometry[P_D3];
   double d4 = p->geometry[P_D4];
   double d6 = p->geometry[P_D6];

   double s1, s2, s3, s4, s5, s6;
   double c1, c2, c3, c4, c5, c6;
   double s23;
   double c23;
   double t1, t2, t3, t4, t5;
   double sumSq, k;
   double d23;
   PmHomogeneous hom;
   PmPose worldPose;
   PmRpy rpy;

   /* calculate sin of joints for future use */
   s1 = sin(joint[0]*PM_PI/180);
   s2 = sin(joint[1]*PM_PI/180);
   s3 = sin(joint[2]*PM_PI/180);
   s4 = sin(joint[3]*PM_PI/180);
   s5 = sin(joint[4]*PM_PI/180);
   s6 = sin(joint[5]*PM_PI/180);

   /* calculate cos of joints for future use */
   c1 = cos(joint[0]*PM_PI/180);
   c2 = cos(joint[1]*PM_PI/180);
   c3 = cos(joint[2]*PM_PI/180);
   c4 = cos(joint[3]*PM_PI/180);
   c5 = cos(joint[4]*PM_PI/180);
   c6 = cos(joint[5]*PM_PI/180);

   s23 = c2 * s3 + s2 * c3;
   c23 = c2 * c3 - s2 * s3;

   /* calculate terms for first column of rotation matrix */
   t1 = c4 * c5 * c6 - s4 * s6;
   t2 = s23 * s5 * c6;
   t3 = s4 * c5 * c6 + c4 * s6;
   t4 = c23 * t1 - t2;
   t5 = c23 * s5 * c6;

   /* define first column of rotation matrix */
   hom.rot.x.x = c1 * t4 + s1 * t3;
   hom.rot.x.y = s1 * t4 - c1 * t3;
   hom.rot.x.z = -s23 * t1 - t5;

   /* calculate terms for second column of rotation matrix */
   t1 = -c4 * c5 * s6 - s4 * c6;
   t2 = s23 * s5 * s6;
   t3 = c4 * c6 - s4 * c5 * s6;
   t4 = c23 * t1 + t2;
   t5 = c23 * s5 * s6;

   /* define second column of rotation matrix */
   hom.rot.y.x = c1 * t4 + s1 * t3;
   hom.rot.y.y = s1 * t4 - c1 * t3;
   hom.rot.y.z = -s23 * t1 + t5;

   /* calculate terms for third column of rotation matrix */
   t1 = c23 * c4 * s5 + s23 * c5;

   /* define third column of rotation matrix */
   hom.rot.z.x = -c1 * t1 - s1 * s4 * s5;
   hom.rot.z.y = -s1 * t1 + c1 * s4 * s5;
   hom.rot.z.z = s23 * c4 * s5 - c23 * c5;

   /* calculate term for position vector */
   t1 = a1 + a2 * c2 + a3 * c23 - d4 * s23;

   /* define position vector */
   d23 = d2 + d3;
   hom.tran.x = c1 * t1 - d23 * s1;
   hom.tran.y = s1 * t1 + d23 * c1;
   hom.tran.z = d1 - a3 * s23 - a2 * s2 - d4 * c23;

   /* calculate terms to determine flags */
   sumSq = hom.tran.x * hom.tran.x + hom.tran.y * hom.tran.y - d23 * d23;
   if (sumSq < 0.0) {
       sumSq = 0.0;
   }
   k = (sumSq + (hom.tran.z - d1) * (hom.tran.z - d1) + a1 * a1 -
       2.0 * a1 * (c1 * hom.tran.x + s1 * hom.tran.y) -
       a2 * a2 - a3 * a3 - d4 * d4) /
       (2.0 * a2);

   /* reset flags */
   *iflags = 0;

   /* set shoulder flag */
   if (fabs(angleDiff(joint[0]*PM_PI/180, atan2(hom.tran.y, hom.tran.x) -
       atan2(d23, -sqrt(sumSq)))) < FLAG_FUZZ)
   {
     *iflags |= THREE21_SHOULDER_RIGHT;
   }

   /* set elbow flag */
   double discr = a3 * a3 + d4 * d4 - k * k;
   if (discr < 0.0) {
       discr = 0.0;
   }
   if (fabs(angleDiff(joint[2]*PM_PI/180, atan2(a3, d4) -
       atan2(k, -sqrt(discr)))) < FLAG_FUZZ)
   {
      *iflags |= THREE21_ELBOW_DOWN;
   }

   /* set singular flag */
   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 + hom.rot.z.z * s23;
   if (fabs(t1) < SINGULAR_FUZZ && fabs(t2) < SINGULAR_FUZZ)
   {
      *iflags |= THREE21_SINGULAR;
   }
   else
   {
     if (! (fabs(angleDiff(joint[3]*PM_PI/180, atan2(t1, t2))) < FLAG_FUZZ))
     {
       *iflags |= THREE21_WRIST_FLIP;
     }
   }

   /* add effect of d6 parameter */
   hom.tran.x = hom.tran.x + hom.rot.z.x * d6;
   hom.tran.y = hom.tran.y + hom.rot.z.y * d6;
   hom.tran.z = hom.tran.z + hom.rot.z.z * d6;

   /* convert hom to pose */
   pmHomPoseConvert(&hom, &worldPose);
   pmQuatRpyConvert(&worldPose.rot,&rpy);
   world->tran = worldPose.tran;
   world->a = rpy.r * 180.0/PM_PI;
   world->b = rpy.p * 180.0/PM_PI;
   world->c = rpy.y * 180.0/PM_PI;

   return 0;
}

static int three21_inverse(const kins_params *p, kins_scratch *s,
                           const EmcPose * world,
                           double * joint,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
   (void)s;
   PmHomogeneous hom;
   PmPose worldPose;
   PmRpy rpy;

   double a1 = p->geometry[P_A1];
   double a2 = p->geometry[P_A2];
   double a3 = p->geometry[P_A3];
   double d1 = p->geometry[P_D1];
   double d2 = p->geometry[P_D2];
   double d3 = p->geometry[P_D3];
   double d4 = p->geometry[P_D4];
   double d6 = p->geometry[P_D6];

   double t1, t2, t3;
   double k;
   double sumSq;
   double d23;

   double th1;
   double th3;
   double th23;
   double th2;
   double th4;
   double th5;
   double th6;

   double s1, c1;
   double s3, c3;
   double s23, c23;
   double s4, c4;
   double s5, c5;
   double s6, c6;
   double px, py, pz;

   /* reset flags */
   *fflags = 0;

   /* convert pose to hom */
   worldPose.tran = world->tran;
   rpy.r = world->a*PM_PI/180.0;
   rpy.p = world->b*PM_PI/180.0;
   rpy.y = world->c*PM_PI/180.0;
   pmRpyQuatConvert(&rpy,&worldPose.rot);
   pmPoseHomConvert(&worldPose, &hom);

   /* remove effect of d6 parameter */
   px = hom.tran.x - d6 * hom.rot.z.x;
   py = hom.tran.y - d6 * hom.rot.z.y;
   pz = hom.tran.z - d1 - d6 * hom.rot.z.z;

   /* joint 1 (2 independent solutions) */
   d23 = d2 + d3;
   sumSq = px * px + py * py - d23 * d23;
   if (sumSq < 0.0) {
       sumSq = 0.0;
   }

   if (*iflags & THREE21_SHOULDER_RIGHT) {
     th1 = atan2(py, px) - atan2(d23, -sqrt(sumSq));
   } else {
     th1 = atan2(py, px) - atan2(d23, sqrt(sumSq));
   }

   /* save sin, cos for later calcs */
   s1 = sin(th1);
   c1 = cos(th1);

   /* joint 3 (2 independent solutions) */
   k = (sumSq + pz * pz + a1 * a1 -
       2.0 * a1 * (c1 * px + s1 * py) -
       a2 * a2 - a3 * a3 - d4 * d4) /
       (2.0 * a2);

   double discr = a3 * a3 + d4 * d4 - k * k;
   if (discr < 0.0) {
       discr = 0.0;
   }

   if (*iflags & THREE21_ELBOW_DOWN) {
     th3 = atan2(a3, d4) -
           atan2(k, -sqrt(discr));
   } else {
     th3 = atan2(a3, d4) -
           atan2(k, sqrt(discr));
   }

   /* compute sin, cos for later calcs */
   s3 = sin(th3);
   c3 = cos(th3);

   /* joint 2 */
   t1 = (-a3 - a2 * c3) * pz +
        (c1 * px + s1 * py - a1) * (a2 * s3 - d4);
   t2 = (a2 * s3 - d4) * pz +
        (a3 + a2 * c3) * (c1 * px + s1 * py - a1);
   t3 = pz * pz + (c1 * px + s1 * py - a1) * (c1 * px + s1 * py - a1);

   th23 = atan2(t1, t2);
   th2 = th23 - th3;

   /* compute sin, cos for later calcs */
   s23 = t1 / t3;
   c23 = t2 / t3;

   /* joint 4 */
   t1 = -hom.rot.z.x * s1 + hom.rot.z.y * c1;
   t2 = -hom.rot.z.x * c1 * c23 - hom.rot.z.y * s1 * c23 + hom.rot.z.z * s23;
   if (fabs(t1) < SINGULAR_FUZZ && fabs(t2) < SINGULAR_FUZZ) {
     *fflags |= THREE21_REACH;
     th4 = joint[3]*PM_PI/180;            /* use current value */
   } else {
     th4 = atan2(t1, t2);
   }

   /* compute sin, cos for later calcs */
   s4 = sin(th4);
   c4 = cos(th4);

   /* joint 5 */
   s5 = hom.rot.z.z * (s23 * c4) -
        hom.rot.z.x * (c1 * c23 * c4 + s1 * s4) -
        hom.rot.z.y * (s1 * c23 * c4 - c1 * s4);
   c5 = -hom.rot.z.x * (c1 * s23) - hom.rot.z.y *
        (s1 * s23) - hom.rot.z.z * c23;
   th5 = atan2(s5, c5);

   /* joint 6 */
   s6 = hom.rot.x.z * (s23 * s4) - hom.rot.x.x *
        (c1 * c23 * s4 - s1 * c4) - hom.rot.x.y *
        (s1 * c23 * s4 + c1 * c4);
   c6 = hom.rot.x.x * ((c1 * c23 * c4 + s1 * s4) *
        c5 - c1 * s23 * s5) + hom.rot.x.y *
        ((s1 * c23 * c4 - c1 * s4) * c5 - s1 * s23 * s5) -
        hom.rot.x.z * (s23 * c4 * c5 + c23 * s5);
   th6 = atan2(s6, c6);

   /* wrist flip */
   if (*iflags & THREE21_WRIST_FLIP) {
     th4 = th4 + PM_PI;
     th5 = -th5;
     th6 = th6 + PM_PI;
   }

   /* copy out */
   joint[0] = th1*180/PM_PI;
   joint[1] = th2*180/PM_PI;
   joint[2] = th3*180/PM_PI;
   joint[3] = th4*180/PM_PI;
   joint[4] = th5*180/PM_PI;
   joint[5] = th6*180/PM_PI;

   return 0;
}

// no frames reported and no closed form Jacobian: the shared code
// differences the inverse
static const kins_ops three21_ops = {
    .forward = three21_forward,
    .inverse = three21_inverse,
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
    kp->kinsname    = "three21kins";
    kp->halprefix   = "three21kins";
    kp->required_coordinates = "xyzabc";
    kp->allow_duplicates     = 0;
    kp->max_joints = strlen(kp->required_coordinates);
    kp->params     = three21_params;
    kp->nparams    = sizeof(three21_params)/sizeof(three21_params[0]);

    switchkinsRegisterOps(0, &three21_ops);
    switchkinsRegisterOps(1, &KINS_IDENTITY_OPS);
    switchkinsRegisterOps(2, &USERK_OPS);

    return 0;
}
