/********************************************************************
* Description: tripodkins.c
*   Kinematics for 3 axis Tripod machine
*
*   Derived from a work by Fred Proctor
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

/*
  These kinematics are for a tripod with point vertices.

  Vertices A, B, and C are the base, and vertex D is the controlled point.
  Three tripod strut lengths AD, BD, and CD are the joints that move
  point D around.

  Point A is the origin, with coordinates (0, 0, 0). Point B lies on the
  x axis, with coordinates (Bx, 0, 0). Point C lies in the xy plane, with
  coordinates (Cx, Cy, 0). Point D has coordinates (Dx, Dy, Dz).

  The controlled Cartesian values are Dx, Dy, and Dz. A frame attached to
  D, say with x parallel to AD and y in the plane ABD, would change its
  orientation as the strut lengths changed. The orientation of this frame
  relative to the world frame is not computed.

  With respect to the kinematics functions,

  pos->tran.x = Dx
  pos->tran.y = Dy
  pos->tran.z = Dz
  pos->a,b,c  = 0

  joints[0] = AD
  joints[1] = BD
  joints[2] = CD

  The inverse kinematics have no singularities. Any values for Dx, Dy, and
  Dz will yield numerical results. Of course, these may be beyond the
  strut length limits, but there are no singular effects like infinite speed.

  The forward kinematics has a singularity due to the triangle inequalities
  for triangles ABD, BCD, and CAD. When any of these approach the limit,
  Dz is zero and D lies in the base plane.

  The forward kinematics flags, referred to in kinematicsForward and
  set in kinematicsInverse, let the forward kinematics select between
  the positive and negative values of Dz for given strut values.
  Dz > 0 is "above", Dz < 0 is "below". Dz = 0 is the singularity.

  fflags == 0 selects Dz > 0,
  fflags != 0 selects Dz < 0.

  The inverse kinematics flags let the inverse kinematics select between
  multiple valid solutions of strut lengths for given Cartesian values
  for D. There are no multiple solutions: D constrains the strut lengths
  completely. So, the inverse flags are ignored.
 */

#include <rtapi.h>		/* RTAPI realtime OS API */
#include <rtapi_app.h>		/* RTAPI realtime module decls */
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>             /* these decls */
#include <kins_rt.h>

// the base geometry, one pin each, poked from HAL as before
static const kins_param_desc tripod_params[] = {
    { "Bx", KINS_PARAM_FLOAT, KINS_IO, 0, 1.0 },
    { "Cx", KINS_PARAM_FLOAT, KINS_IO, 0, 1.0 },
    { "Cy", KINS_PARAM_FLOAT, KINS_IO, 0, 1.0 },
};
enum { P_BX, P_CX, P_CY };

#define sq(x) ((x)*(x))

/*
  forward kinematics takes three strut lengths and computes Dx, Dy, and Dz
  pos->tran.x,y,z, respectively. The forward flag is used to resolve
  D above/below the xy plane. The inverse flags are not set since there
  are no ambiguities going from world to joint coordinates.

  The forward kins are derived as follows:

  1. Let x, y, z be Dx, Dy, Dz to save pixels. Cartesian displacement from
  D to A, B, and C gives

  AD^2 = x^2 + y^2 + z^2
  BD^2 = (x - Bx)^2 + y^2 + z^2
  CD^2 = (x - Cx)^2 + (y - Cy)^2 + z^2

  This yields

  I.   P = x^2 + y^2 + z^2
  II.  Q = x^2 + y^2 + z^2 + sx
  III. R = x^2 + y^2 + z^2 + tx + uy

  Where

  P = AD^2,
  Q = BD^2 - Bx^2
  R = CD^2 - Cx^2 - Cy^2
  s = -2Bx
  t = -2Cx
  u = -2Cy

  II - I gives Q - P = sx, so x = (Q - P)/s, s != 0. The constraint on s
  means that Bx != 0, or points A and B can't be the same.

  III - II gives R - Q = (t - s)x + uy, so y = (R - Q - (t - s)x)/u, u != 0.
  The constraint on u means that Cy != 0, or points A B C can't be collinear.

  Substituting x, y into I gives z = sqrt(P - x^2 - y^2), which has two
  solutions. Positive means the tripod is above the xy plane, negative
  means below.
*/
static int tripod_forward(const kins_params *p, kins_scratch *s_,
                          const double * joints,
                          EmcPose * pos,
                          const KINEMATICS_FORWARD_FLAGS * fflags,
                          KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)s_;
    (void)iflags;
#define AD (joints[0])
#define BD (joints[1])
#define CD (joints[2])
#define Dx (pos->tran.x)
#define Dy (pos->tran.y)
#define Dz (pos->tran.z)
  double P, Q, R;
  double s, t, u;
  const double Bx = p->geometry[P_BX];
  const double Cx = p->geometry[P_CX];
  const double Cy = p->geometry[P_CY];

  P = sq(AD);
  Q = sq(BD) - sq(Bx);
  R = sq(CD) - sq(Cx) - sq(Cy);
  s = -2.0 * Bx;
  t = -2.0 * Cx;
  u = -2.0 * Cy;

  if (s == 0.0) {
    /* points A and B coincident. Fix Bx, #defined up top. */
    return -1;
  }
  Dx = (Q - P) / s;

  if (u == 0.0) {
    /* points A B C are colinear. Fix Cy, #defined up top. */
    return -1;
  }
  Dy = (R - Q - (t - s) * Dx) / u;
  Dz = P - sq(Dx) - sq(Dy);
  if (Dz < 0.0) {
    /* triangle inequality violated */
    return -1;
  }
  Dz = sqrt(Dz);
  if (*fflags) {
    Dz = -Dz;
  }

  pos->a = 0.0;
  pos->b = 0.0;
  pos->c = 0.0;

  return 0;

#undef AD
#undef BD
#undef CD
#undef Dx
#undef Dy
#undef Dz
}

static int tripod_inverse(const kins_params *p, kins_scratch *s,
                          const EmcPose * pos,
                          double * joints,
                          const KINEMATICS_INVERSE_FLAGS * iflags,
                          KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)s;
    (void)iflags;
#define AD (joints[0])
#define BD (joints[1])
#define CD (joints[2])
#define Dx (pos->tran.x)
#define Dy (pos->tran.y)
#define Dz (pos->tran.z)
  const double Bx = p->geometry[P_BX];
  const double Cx = p->geometry[P_CX];
  const double Cy = p->geometry[P_CY];

  AD = sqrt(sq(Dx) + sq(Dy) + sq(Dz));
  BD = sqrt(sq(Dx - Bx) + sq(Dy) + sq(Dz));
  CD = sqrt(sq(Dx - Cx) + sq(Dy - Cy) + sq(Dz));

  *fflags = 0;
  if (Dz < 0.0) {
    *fflags = 1;
  }

  return 0;

#undef AD
#undef BD
#undef CD
#undef Dx
#undef Dy
#undef Dz
}

static int tripod_jacobian(const kins_params *p, const double * joints,
                           const EmcPose * pos,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS * iflags)
{
  const double Bx = p->geometry[P_BX];
  const double Cx = p->geometry[P_CX];
  const double Cy = p->geometry[P_CY];
  /* the three strut base points, in the order of the joints */
  const double base[3][2] = { {0, 0}, {Bx, 0}, {Cx, Cy} };
  int i, j, a;

  (void)iflags;
  for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
    for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
  }
  /* a strut length changes by the component of the motion along the
     strut, so each row is the unit vector from base to D */
  for (i = 0; i < 3; i++) {
    double dx = pos->tran.x - base[i][0];
    double dy = pos->tran.y - base[i][1];
    double dz = pos->tran.z;
    double len = joints[i];
    if (len <= 0) { return -1; }
    jac[i][0] = dx/len;
    jac[i][1] = dy/len;
    jac[i][2] = dz/len;
  }
  return 0;
}

static const kins_ops tripod_ops = {
    .forward  = tripod_forward,
    .inverse  = tripod_inverse,
    .jacobian = tripod_jacobian,
};

// three struts for three coordinates; the entry points come from
// kins_single.c
const kins_module_info kins_module = {
    .name                 = "tripodkins",
    .halprefix            = "tripodkins",
    .params               = tripod_params,
    .nparams              = sizeof(tripod_params)/sizeof(tripod_params[0]),
    .required_coordinates = "XYZ",
    .max_joints           = 3,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &tripod_ops },
};

MODULE_LICENSE("GPL");

static int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("tripodkins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, "XYZ", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
