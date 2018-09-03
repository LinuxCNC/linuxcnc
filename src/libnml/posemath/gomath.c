/********************************************************************
* Description: gomath.c
*   Library file with various functions for working with matrices
*
*   Derived from a work by Fred Proctor,
*   changed to work with emc2 and HAL
*
* Adapting Author: Alex Joni
* License: LGPL Version 2
* System: Linux
*    
*******************************************************************
    Similar to posemath, but using different functions.
    
    TODO: 
	* find the new functions, add them to posemath, convert the rest
*/

/* for debugging */
extern int printf(const char * fmt, ...);
#include <stddef.h>		/* NULL */

#include "rtapi_math.h"
#include <float.h>

#include "sincos.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gotypes.h"		/* GoResult ,Real, etc */
#include "gomath.h"		/* these decls, GoCartesian, etc. */

go_real go_cbrt(go_real x)
{
  if (x < 0.0) {
    return (go_real) -rtapi_pow((double) -x, 1.0/3.0);
  }
  return (go_real) rtapi_pow((double) x, 1.0/3.0);
}


/* Translation rep conversion functions */

int go_cart_sph_convert(const go_cart * v, go_sph * s)
{
  go_real r;

  s->theta = rtapi_atan2(v->y, v->x);
  r = rtapi_sqrt(go_sq(v->x) + go_sq(v->y) + go_sq(v->z));
  s->r = r;
  if (GO_TRAN_SMALL(r)) {
    s->phi = 0;
  } else {
    s->phi = rtapi_acos(v->z / r);
  }

  return GO_RESULT_OK;
}

int go_cart_cyl_convert(const go_cart * v, go_cyl * c)
{
  c->theta = rtapi_atan2(v->y, v->x);
  c->r = rtapi_sqrt(go_sq(v->x) + go_sq(v->y));
  c->z = v->z;

  return GO_RESULT_OK;
}

int go_sph_cart_convert(const go_sph * s, go_cart * v)
{
  go_real sth, cth, sph, cph;

  sincos(s->theta, &sth, &cth);
  sincos(s->phi, &sph, &cph);

  v->x = s->r * cth * sph;
  v->y = s->r * sth * sph;
  v->z = s->r * cph;

  return GO_RESULT_OK;
}

int go_sph_cyl_convert(const go_sph * s, go_cyl * c)
{
  go_real sph, cph;

  sincos(s->phi, &sph, &cph);

  c->theta = s->theta;
  c->r = s->r * sph;
  c->z = s->r * cph;

  return GO_RESULT_OK;
}

int go_cyl_cart_convert(const go_cyl * c, go_cart * v)
{
  v->x = c->r * rtapi_cos(c->theta);
  v->y = c->r * rtapi_sin(c->theta);
  v->z = c->z;

  return GO_RESULT_OK;
}

int go_cyl_sph_convert(const go_cyl * c, go_sph * s)
{
  s->theta = c->theta;
  s->r = rtapi_sqrt(go_sq(c->r) + go_sq(c->z));
  if (GO_TRAN_SMALL(s->r)) {
    s->phi = 0.0;
  } else {
    s->phi = rtapi_acos(c->z / s->r);
  }

  return GO_RESULT_OK;
}

/* rotation rep conversion functions */

int go_rvec_quat_convert(const go_rvec * r, go_quat * q)
{
  go_cart vec;
  go_cart uvec;
  go_real mag;
  go_real sh;

  vec.x = r->x;
  vec.y = r->y;
  vec.z = r->z;

  if (GO_RESULT_OK != go_cart_unit(&vec, &uvec)) {
    /* a zero vector */
    q->s = 1;
    q->x = q->y = q->z = 0;
    return GO_RESULT_OK;
  }

  (void) go_cart_mag(&vec, &mag);

  sincos(0.5 * mag, &sh, &(q->s));

  if (q->s >= 0) {
    q->x = uvec.x * sh;
    q->y = uvec.y * sh;
    q->z = uvec.z * sh;
  } else {
    q->s = -q->s;
    q->x = -uvec.x * sh;
    q->y = -uvec.y * sh;
    q->z = -uvec.z * sh;
  }

  return GO_RESULT_OK;
}

int go_rvec_mat_convert(const go_rvec * r, go_mat * m)
{
  go_cart vec;
  go_cart uvec;
  go_real s, c, omc;
  go_real mag;

  vec.x = r->x;
  vec.y = r->y;
  vec.z = r->z;

  if (GO_RESULT_OK != go_cart_unit(&vec, &uvec)) {
    /* a zero vector */
    m->x.x = 1, m->y.x = 0, m->z.x = 0;
    m->x.y = 0, m->y.y = 1, m->z.y = 0;
    m->x.z = 0, m->y.z = 0, m->z.z = 1;
    return GO_RESULT_OK;
  }

  (void) go_cart_mag(&vec, &mag);

  sincos(mag, &s, &c);
  omc = 1 - c;

  m->x.x = c + go_sq(uvec.x) * omc;
  m->y.x = -uvec.z * s + uvec.x * uvec.y * omc;
  m->z.x = uvec.y * s + uvec.x * uvec.z * omc;

  m->x.y = uvec.z * s + uvec.y * uvec.x * omc;
  m->y.y = c + go_sq(uvec.y) * omc;
  m->z.y = -uvec.x * s + uvec.y * uvec.z * omc;

  m->x.z = -uvec.y * s + uvec.z * uvec.x * omc;
  m->y.z = uvec.x * s + uvec.z * uvec.y * omc;
  m->z.z = c + go_sq(uvec.z) * omc;

  return GO_RESULT_OK;
}

int go_rvec_zyz_convert(const go_rvec * rvec, go_zyz * zyz)
{
  go_mat mat;
  int retval;

  retval = go_rvec_mat_convert(rvec, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_zyz_convert(&mat, zyz);
}

int go_rvec_zyx_convert(const go_rvec * rvec, go_zyx * zyx)
{
  go_mat mat;
  int retval;

  retval = go_rvec_mat_convert(rvec, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_zyx_convert(&mat, zyx);
}

int go_rvec_rpy_convert(const go_rvec * r, go_rpy * rpy)
{
  go_quat q;
  int retval;

  retval = go_rvec_quat_convert(r, &q);
  if (GO_RESULT_OK != retval) return retval;

  return go_quat_rpy_convert(&q, rpy);
}

int go_quat_rvec_convert(const go_quat * q, go_rvec * r)
{
  go_real sh;
  go_real mag;

  sh = rtapi_sqrt(go_sq(q->x) + go_sq(q->y) + go_sq(q->z));

  if (GO_ROT_SMALL(sh)) {
    r->x = 0;
    r->y = 0;
    r->z = 0;
  } else {
    mag = 2 * rtapi_atan2(sh, q->s) / sh;
    r->x = mag * q->x;
    r->y = mag * q->y;
    r->z = mag * q->z;
  }

  return GO_RESULT_OK;
}

int go_quat_mat_convert(const go_quat * q, go_mat * m)
{
  /* from space book where e1=q->x e2=q->y e3=q->z e4=q->s */
  m->x.x = 1 - 2 * (go_sq(q->y) + go_sq(q->z));
  m->y.x = 2 * (q->x * q->y - q->z * q->s);
  m->z.x = 2 * (q->z * q->x + q->y * q->s);

  m->x.y = 2 * (q->x * q->y + q->z * q->s);
  m->y.y = 1 - 2 * (go_sq(q->z) + go_sq(q->x));
  m->z.y = 2 * (q->y * q->z - q->x * q->s);

  m->x.z = 2 * (q->z * q->x - q->y * q->s);
  m->y.z = 2 * (q->y * q->z + q->x * q->s);
  m->z.z = 1 - 2 * (go_sq(q->x) + go_sq(q->y));

  return GO_RESULT_OK;
}

int go_quat_zyz_convert(const go_quat * q, go_zyz * zyz)
{
  go_mat m;
  int retval;

  retval = go_quat_mat_convert(q, &m);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyz_convert(&m, zyz);
}

int go_quat_zyx_convert(const go_quat * q, go_zyx * zyx)
{
  go_mat m;
  int retval;

  retval = go_quat_mat_convert(q, &m);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyx_convert(&m, zyx);
}

int go_quat_rpy_convert(const go_quat * q, go_rpy * rpy)
{
  go_mat m;
  int retval;

  retval = go_quat_mat_convert(q, &m);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_rpy_convert(&m, rpy);
}

int go_mat_rvec_convert(const go_mat * m, go_rvec * r)
{
  go_quat q;
  int retval;

  retval = go_mat_quat_convert(m, &q);
  if (GO_RESULT_OK != retval) return retval;

  return go_quat_rvec_convert(&q, r);
}

/*
  from space book:

  e1 = (c32 - c23) / 4*e4
  e2 = (c13 - c31) / 4*e4
  e3 = (c21 - c12) / 4*e4
  e4 = rtapi_sqrt(1 + c11 + c22 + c33) / 2

  if e4 == 0
  e1 = rtapi_sqrt(1 + c11 - c33 - c22) / 2
  e2 = rtapi_sqrt(1 + c22 - c33 - c11) / 2
  e3 = rtapi_sqrt(1 + c33 - c11 - c22) / 2

  to determine whether to take the positive or negative sqrt value
  since e4 == 0 indicates a 180* rotation then (0 x y z) = (0 -x -y -z).
  Thus some generalities can be used:
  1) find which of e1, e2, or e3 has the largest magnitude and leave it pos
  2) if e1 is largest then
  if c21 < 0 then take the negative for e2
  if c31 < 0 then take the negative for e3
  3) else if e2 is largest then
  if c21 < 0 then take the negative for e1
  if c32 < 0 then take the negative for e3
  4) else if e3 is larget then
  if c31 < 0 then take the negative for e1
  if c32 < 0 then take the negative for e2

  Note: c21 in the space book is m.x.y in this C code
*/
int go_mat_quat_convert(const go_mat * m, go_quat * q)
{
  go_real discr;
  go_real a;

  if (! go_mat_is_norm(m)) {
    return GO_RESULT_NORM_ERROR;
  }

  discr = 1.0 + m->x.x + m->y.y + m->z.z;
  if (discr < 0.0) discr = 0.0;	/* give sqrt some slack for tiny negs */
  
  q->s = 0.5 * rtapi_sqrt(discr);

  if (GO_ROT_SMALL(q->s)) {
    q->s = 0;
    discr = 1.0 + m->x.x - m->y.y - m->z.z;
    if (discr < 0.0) discr = 0.0;
    q->x = rtapi_sqrt(discr) / 2.0;
    discr = 1.0 + m->y.y - m->x.x - m->z.z;
    if (discr < 0.0) discr = 0.0;
    q->y = rtapi_sqrt(discr) / 2.0;
    discr = 1.0 + m->z.z - m->y.y - m->x.x;
    if (discr < 0.0) discr = 0.0;
    q->z = rtapi_sqrt(discr) / 2.0;

    if (q->x > q->y && q->x > q->z) {
      if (m->x.y < 0.0) {
	q->y *= -1;
      }
      if (m->x.z < 0.0) {
	q->z *= -1;
      }
    } else if (q->y > q->z) {
      if (m->x.y < 0.0) {
	q->x *= -1;
      }
      if (m->y.z < 0.0) {
	q->z *= -1;
      }
    } else {
      if (m->x.z < 0.0) {
	q->x *= -1;
      }
      if (m->y.z < 0.0) {
	q->y *= -1;
      }
    }
  } else {
    q->x = (m->y.z - m->z.y) / (a = 4 * q->s);
    q->y = (m->z.x - m->x.z) / a;
    q->z = (m->x.y - m->y.x) / a;
  }

  return go_quat_norm(q, q);
}

int go_mat_zyz_convert(const go_mat * m, go_zyz * zyz)
{
  zyz->y = rtapi_atan2(rtapi_sqrt(go_sq(m->x.z) + go_sq(m->y.z)), m->z.z);

  if (GO_ROT_SMALL(zyz->y)) {
    zyz->z = 0;
    zyz->y = 0;			/* force Y to 0 */
    zyz->zp = rtapi_atan2(-m->y.x, m->x.x);
  } else if (GO_ROT_CLOSE(zyz->y, GO_PI)) {
    zyz->z = 0;
    zyz->y = GO_PI;		/* force Y to 180 */
    zyz->zp = rtapi_atan2(m->y.x, -m->x.x);
  } else {
    zyz->z = rtapi_atan2(m->z.y, m->z.x);
    zyz->zp = rtapi_atan2(m->y.z, -m->x.z);
  }

  return GO_RESULT_OK;
}

int go_mat_zyx_convert(const go_mat * m, go_zyx * zyx)
{
  zyx->y = rtapi_atan2(-m->x.z, rtapi_sqrt(go_sq(m->x.x) + go_sq(m->x.y)));

  if (GO_ROT_CLOSE(zyx->y, GO_PI_2)) {
    zyx->z = 0;
    zyx->y = GO_PI_2;		/* force it */
    zyx->x = rtapi_atan2(m->y.x, m->y.y);
  } else if (GO_ROT_CLOSE(zyx->y, GO_PI_2)) {
    zyx->z = 0;
    zyx->y = -GO_PI_2;		/* force it */
    zyx->x = -rtapi_atan2(m->y.z, m->y.y);
  } else {
    zyx->z = rtapi_atan2(m->x.y, m->x.x);
    zyx->x = rtapi_atan2(m->y.z, m->z.z);
  }

  return GO_RESULT_OK;
}

int go_mat_rpy_convert(const go_mat * m, go_rpy * rpy)
{
  rpy->p = rtapi_atan2(-m->x.z, rtapi_sqrt(go_sq(m->x.x) + go_sq(m->x.y)));

  if (GO_ROT_CLOSE(rpy->p, GO_PI_2)) {
    rpy->r = rtapi_atan2(m->y.x, m->y.y);
    rpy->p = GO_PI_2;		/* force it */
    rpy->y = 0;
  } else if (GO_ROT_CLOSE(rpy->p, GO_PI_2)) {
    rpy->r = -rtapi_atan2(m->y.z, m->y.y);
    rpy->p = -GO_PI_2;		/* force it */
    rpy->y = 0;
  } else {
    rpy->r = rtapi_atan2(m->y.z, m->z.z);
    rpy->y = rtapi_atan2(m->x.y, m->x.x);
  }

  return GO_RESULT_OK;
}

int go_zyz_rvec_convert(const go_zyz * zyz, go_rvec * r)
{
  go_mat m;
  int retval;

  retval = go_zyz_mat_convert(zyz, &m);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_rvec_convert(&m, r);
}

int go_zyz_quat_convert(const go_zyz * zyz, go_quat * q)
{
  go_mat m;
  int retval;

  retval = go_zyz_mat_convert(zyz, &m);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_quat_convert(&m, q);
}

int go_zyz_mat_convert(const go_zyz * zyz, go_mat * m)
{
  go_real sa, sb, sg;
  go_real ca, cb, cg;

  sa = rtapi_sin(zyz->z);
  sb = rtapi_sin(zyz->y);
  sg = rtapi_sin(zyz->zp);

  ca = rtapi_cos(zyz->z);
  cb = rtapi_cos(zyz->y);
  cg = rtapi_cos(zyz->zp);

  m->x.x = ca * cb * cg - sa * sg;
  m->y.x = -ca * cb * sg - sa * cg;
  m->z.x = ca * sb;

  m->x.y = sa * cb * cg + ca * sg;
  m->y.y = -sa * cb * sg + ca * cg;
  m->z.y = sa * sb;

  m->x.z = -sb * cg;
  m->y.z = sb * sg;
  m->z.z = cb;

  return GO_RESULT_OK;
}

int go_zyz_zyx_convert(const go_zyz * zyz, go_zyx * zyx)
{
  go_mat mat;
  int retval;

  retval = go_zyz_mat_convert(zyz, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyx_convert(&mat, zyx);
}

int go_zyz_rpy_convert(const go_zyz * zyz, go_rpy * rpy)
{
  go_mat mat;
  int retval;

  retval = go_zyz_mat_convert(zyz, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_rpy_convert(&mat, rpy);
}

int go_zyx_rvec_convert(const go_zyx * zyx, go_rvec * r)
{
  go_mat mat;
  int retval;

  retval = go_zyx_mat_convert(zyx, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_rvec_convert(&mat, r);
}


int go_zyx_quat_convert(const go_zyx * zyx, go_quat * q)
{
  go_mat mat;
  int retval;

  retval = go_zyx_mat_convert(zyx, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_quat_convert(&mat, q);
}

int go_zyx_mat_convert(const go_zyx * zyx, go_mat * m)
{
  go_real sa, sb, sg;
  go_real ca, cb, cg;

  sa = rtapi_sin(zyx->z);
  sb = rtapi_sin(zyx->y);
  sg = rtapi_sin(zyx->x);

  ca = rtapi_cos(zyx->z);
  cb = rtapi_cos(zyx->y);
  cg = rtapi_cos(zyx->x);

  m->x.x = ca * cb;
  m->y.x = ca * sb * sg - sa * cg;
  m->z.x = ca * sb * cg + sa * sg;

  m->x.y = sa * cb;
  m->y.y = sa * sb * sg + ca * cg;
  m->z.y = sa * sb * cg - ca * sg;

  m->x.z = -sb;
  m->y.z = cb * sg;
  m->z.z = cb * cg;

  return GO_RESULT_OK;
}

int go_zyx_zyz_convert(const go_zyx * zyx, go_zyz * zyz)
{
  go_mat mat;
  int retval;
  
  retval = go_zyx_mat_convert(zyx, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyz_convert(&mat, zyz);
}

int go_zyx_rpy_convert(const go_zyx * zyx, go_rpy * rpy)
{
  go_mat mat;
  int retval;
  
  retval = go_zyx_mat_convert(zyx, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_rpy_convert(&mat, rpy);
}

int go_rpy_rvec_convert(const go_rpy * rpy, go_rvec * rvec)
{
  go_quat quat;
  int retval;

  retval = go_rpy_quat_convert(rpy, &quat);
  if (GO_RESULT_OK != retval) return retval;
  return go_quat_rvec_convert(&quat, rvec);
}

int go_rpy_quat_convert(const go_rpy * rpy, go_quat * quat)
{
  go_mat mat;
  int retval;

  retval = go_rpy_mat_convert(rpy, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_quat_convert(&mat, quat);
}

int go_rpy_mat_convert(const go_rpy * rpy, go_mat * m)
{
  go_real sa, sb, sg;
  go_real ca, cb, cg;

  sa = rtapi_sin(rpy->y);
  sb = rtapi_sin(rpy->p);
  sg = rtapi_sin(rpy->r);

  ca = rtapi_cos(rpy->y);
  cb = rtapi_cos(rpy->p);
  cg = rtapi_cos(rpy->r);

  m->x.x = ca * cb;
  m->y.x = ca * sb * sg - sa * cg;
  m->z.x = ca * sb * cg + sa * sg;

  m->x.y = sa * cb;
  m->y.y = sa * sb * sg + ca * cg;
  m->z.y = sa * sb * cg - ca * sg;

  m->x.z = -sb;
  m->y.z = cb * sg;
  m->z.z = cb * cg;

  return GO_RESULT_OK;
}

int go_rpy_zyz_convert(const go_rpy * rpy, go_zyz * zyz)
{
  go_mat mat;
  int retval;

  retval = go_rpy_mat_convert(rpy, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyz_convert(&mat, zyz);
}

int go_rpy_zyx_convert(const go_rpy * rpy, go_zyx * zyx)
{
  go_mat mat;
  int retval;

  retval = go_rpy_mat_convert(rpy, &mat);
  if (GO_RESULT_OK != retval) return retval;
  return go_mat_zyx_convert(&mat, zyx);
}

go_pose go_pose_this(go_real x, go_real y, go_real z,
		     go_real rs, go_real rx, go_real ry, go_real rz)
{
  go_pose pose;

  pose.tran.x = x, pose.tran.y = y, pose.tran.z = z;
  pose.rot.s = rs, pose.rot.x = rx, pose.rot.y = ry, pose.rot.z = rz;

  return pose;
}

go_cart go_cart_zero(void)
{
  go_cart cart;

  cart.x = 0, cart.y = 0, cart.z = 0;

  return cart;
}

go_quat go_quat_identity(void)
{
  go_quat quat;

  quat.s = 1, quat.x = 0, quat.y = 0, quat.z = 0;

  return quat;
}

go_pose go_pose_identity(void)
{
  go_pose pose;

  pose.tran.x = 0, pose.tran.y = 0, pose.tran.z = 0;
  pose.rot.s = 1, pose.rot.x = 0, pose.rot.y = 0, pose.rot.z = 0;

  return pose;
}

int go_pose_hom_convert(const go_pose * p, go_hom * h)
{
  h->tran = p->tran;

  return go_quat_mat_convert(&p->rot, &h->rot);
}

int go_hom_pose_convert(const go_hom * h, go_pose * p)
{
  p->tran = h->tran;

  return go_mat_quat_convert(&h->rot, &p->rot);
}

int go_cart_rvec_convert(const go_cart * cart, go_rvec * rvec)
{
  rvec->x = cart->x;
  rvec->y = cart->y;
  rvec->z = cart->z;

  return GO_RESULT_OK;
}

int go_rvec_cart_convert(const go_rvec * rvec, go_cart * cart)
{
  cart->x = rvec->x;
  cart->y = rvec->y;
  cart->z = rvec->z;

  return GO_RESULT_OK;
}

/* go_cart functions */

go_flag go_cart_cart_compare(const go_cart * v1, const go_cart * v2)
{
  if (GO_TRAN_CLOSE(v1->x, v2->x) &&
      GO_TRAN_CLOSE(v1->y, v2->y) &&
      GO_TRAN_CLOSE(v1->z, v2->z)) {
    return 1;
  }

  return 0;
}

int go_cart_cart_dot(const go_cart * v1, const go_cart * v2,
			   go_real * d)
{
  *d = v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;

  return GO_RESULT_OK;
}

int go_cart_cart_cross(const go_cart * v1, const go_cart * v2,
			     go_cart * vout)
{
  go_cart cp1, cp2;

  cp1 = *v1;
  cp2 = *v2;

  vout->x = cp1.y * cp2.z - cp1.z * cp2.y;
  vout->y = cp1.z * cp2.x - cp1.x * cp2.z;
  vout->z = cp1.x * cp2.y - cp1.y * cp2.x;

  return GO_RESULT_OK;
}

int go_cart_mag(const go_cart * v, go_real * d)
{
  *d = rtapi_sqrt(go_sq(v->x) + go_sq(v->y) + go_sq(v->z));

  return GO_RESULT_OK;
}

int go_cart_magsq(const go_cart * v, go_real * d)
{
  *d = go_sq(v->x) + go_sq(v->y) + go_sq(v->z);

  return GO_RESULT_OK;
}

go_flag go_cart_cart_par(const go_cart *v1, const go_cart *v2)
{
  go_real dot;
  go_real magsq1, magsq2;

  /*
    parallel if v1 dot v2 = mag v1 * mag v2, or more efficiently,
    sq(v1 dot v2) = magsq v1 * magsq v2
  */

  go_cart_cart_dot(v1, v2, &dot);
  go_cart_magsq(v1, &magsq1);
  go_cart_magsq(v2, &magsq2);

  return GO_TRAN_CLOSE(dot*dot, magsq1*magsq2);
}

go_flag go_cart_cart_perp(const go_cart *v1, const go_cart *v2)
{
  go_real dot;

  /* perpendicular if v1 dot v2 is 0 */

  go_cart_cart_dot(v1, v2, &dot);

  return GO_TRAN_SMALL(dot);
}

int go_cart_cart_disp(const go_cart * v1, const go_cart * v2,
			    go_real * d)
{
  *d = rtapi_sqrt(go_sq(v2->x - v1->x) + 
	    go_sq(v2->y - v1->y) + 
	    go_sq(v2->z - v1->z));

  return GO_RESULT_OK;
}

int go_cart_cart_add(const go_cart * v1, const go_cart * v2,
			   go_cart * vout)
{
  vout->x = v1->x + v2->x;
  vout->y = v1->y + v2->y;
  vout->z = v1->z + v2->z;

  return GO_RESULT_OK;
}

int go_cart_cart_sub(const go_cart * v1, const go_cart * v2,
			   go_cart * vout)
{
  vout->x = v1->x - v2->x;
  vout->y = v1->y - v2->y;
  vout->z = v1->z - v2->z;

  return GO_RESULT_OK;
}

int go_cart_scale_mult(const go_cart * v1, go_real d, go_cart * vout)
{
  vout->x = v1->x * d;
  vout->y = v1->y * d;
  vout->z = v1->z * d;

  return GO_RESULT_OK;
}

int go_cart_neg(const go_cart * v1, go_cart * vout)
{
  vout->x = -v1->x;
  vout->y = -v1->y;
  vout->z = -v1->z;

  return GO_RESULT_OK;
}

int go_cart_unit(const go_cart * v, go_cart * vout)
{
  go_real size = rtapi_sqrt(go_sq(v->x) + go_sq(v->y) + go_sq(v->z));

  if (GO_TRAN_SMALL(size)) {
    vout->x = GO_INF;
    vout->y = GO_INF;
    vout->z = GO_INF;

    return GO_RESULT_NORM_ERROR;
  }

  size = 1.0 / size;

  vout->x = v->x * size;
  vout->y = v->y * size;
  vout->z = v->z * size;

  return GO_RESULT_OK;
}

int go_cart_is_norm(const go_cart * v)
{
  return GO_TRAN_CLOSE(rtapi_sqrt(go_sq(v->x) + go_sq(v->y) + go_sq(v->z)), 1);
}

int go_cart_cart_rot(const go_cart * v1,
			   const go_cart * v2,
			   go_quat * quat)
{
  int retval;
  go_cart u1;
  go_cart u2;
  go_cart cross;
  go_real mag;
  go_real dot;
  go_real th;
  go_rvec rvec;

  /* unitize the input vectors */
  retval = go_cart_unit(v1, &u1);
  if (GO_RESULT_OK != retval) return retval;
  retval = go_cart_unit(v2, &u2);
  if (GO_RESULT_OK != retval) return retval;
  /* and cross the unit vectors to get the mutual normal */
  (void) go_cart_cart_cross(&u1, &u2, &cross);
  /* the magnitude of the mutual normal is sin theta */
  (void) go_cart_mag(&cross, &mag);
  th = rtapi_asin(mag);
  /* dot them to get parallel (1) or antiparallel (-1) */
  (void) go_cart_cart_dot(&u1, &u2, &dot);
  /* make the cross a unit vector so we can theta-ize it */
  retval = go_cart_unit(&cross, &cross);

  /* handle aligned vectors */
  if (GO_RESULT_OK != retval) {
    /* v1 and v2 are aligned, so th will be zero */
    if (dot > 0.0) {		/* would be -1 or 1 */
      /* parallel */
      quat->s = 1.0;
      quat->x = quat->y = quat->z = 0.0;
      return GO_RESULT_OK;
    }
    /* else antiparallel - set u2 to be normal to u1, 
       rotate about it PI rads */
    retval = go_cart_normal(&u1, &u2);
    if (GO_RESULT_OK != retval) return retval;
    rvec.x = GO_PI * u2.x;
    rvec.y = GO_PI * u2.y;
    rvec.z = GO_PI * u2.z;
    return go_rvec_quat_convert(&rvec, quat);
  }

  /* else not aligned */
  if (dot < 0.0) {
    th = GO_PI - th;
  }
  rvec.x = th * cross.x;
  rvec.y = th * cross.y;
  rvec.z = th * cross.z;
  return go_rvec_quat_convert(&rvec, quat);
}

int go_cart_cart_proj(const go_cart * v1, const go_cart * v2,
			    go_cart * vout)
{
  go_cart v2u;
  go_real d;
  int retval;

  retval = go_cart_unit(v2, &v2u);
  if (GO_RESULT_OK != retval) return retval;

  retval = go_cart_cart_dot(v1, &v2u, &d);
  if (GO_RESULT_OK != retval) return retval;

  return go_cart_scale_mult(&v2u, d, vout);
}

int go_cart_plane_proj(const go_cart * v, const go_cart * normal,
			     go_cart * vout)
{
  go_cart par;
  int retval;

  retval = go_cart_cart_proj(v, normal, &par);
  if (GO_RESULT_OK != retval) return retval;

  return go_cart_cart_sub(v, &par, vout);
}

int go_cart_cart_angle(const go_cart * v1, const go_cart * v2,
			     go_real * a)
{
  go_real dot, m1, m2;

  go_cart_cart_dot(v1, v2, &dot);
  go_cart_mag(v1, &m1);
  go_cart_mag(v2, &m2);

  if (m1 <= 0. || m2 <= 0.) {
    return GO_RESULT_DIV_ERROR;
  }

  dot = dot / (m1 * m2);
  if (dot > 1.)
    dot = 1.;
  else if (dot < -1.)
    dot = -1.;

  *a = rtapi_acos(dot);

  return GO_RESULT_OK;
}

int go_cart_normal(const go_cart * v, go_cart * vout)
{
  go_cart cart;
  go_real min, ymin;

  /* pick the X, Y or Z vector that is most perpendicular to 'v' */
  cart.x = 1, cart.y = 0, cart.z = 0; /* start with X */
  min = rtapi_fabs(v->x);
  ymin = rtapi_fabs(v->y);
  if (ymin < min) {
    min = ymin;			/* Y is more perp */
    cart.x = 0, cart.y = 1, cart.z = 0;
  }
  if (rtapi_fabs(v->z) < min) {
    cart.x = 0, cart.y = 0, cart.z = 1;
  }

  /* cross the most perp axis vector with 'v' to get a real perp */
  go_cart_cart_cross(v, &cart, &cart);

  /* unitize it into 'vout' and return */
  return go_cart_unit(&cart, vout);
}

int go_cart_centroid(const go_cart * varray,
			   go_integer num,
			   go_cart * centroid)
{
  go_integer i;

  if (num < 1) return GO_RESULT_BAD_ARGS;

  centroid->x = varray[0].x;
  centroid->y = varray[0].y;
  centroid->z = varray[0].z;

  for (i = 1; i < num; i++) {
    centroid->x += varray[i].x;
    centroid->y += varray[i].y;
    centroid->z += varray[i].z;
  }
  
  return go_cart_scale_mult(centroid, 1.0/((go_real) num), centroid);
}

int go_cart_centroidize(const go_cart * vinarray,
			      go_integer num,
			      go_cart * centroid,
			      go_cart * voutarray)
{
  go_integer i;
  int retval;

  retval = go_cart_centroid(vinarray, num, centroid);
  if (GO_RESULT_OK != retval) return retval;

  for (i = 0; i < num; i++) {
    (void) go_cart_cart_sub(&vinarray[i], centroid, &voutarray[i]);
  }

  return GO_RESULT_OK;
}

/*
  Another Solution of the Cubic Equation Here is another method of
  solving a cubic polynomial equation submitted independently by Paul
  A. Torres and Robert A. Warren. It is based on the idea of "completing
  the cube," by arranging matters so that three of the four terms are
  three of the four terms of a perfect cube.

  Start with the cubic equation

  x3 + ax2 + bx + c = 0.

  If a2 - 3b = 0, then the first three terms are the first three terms
  of a perfect cube, namely (x+a/3)3. Then you can "complete the cube"
  by subtracting c from both sides and adding the missing term of the
  cube a3/27 to both sides. Recalling that b = a2/3, you get

  x3 + ax2 + bx + c = 0,
  x3 + ax2 + (a2/3)x + c = 0,
  x3 + ax2 + (a2/3)x = -c,
  x3 + ax2 + (a2/3)x + a3/27 = a3/27 - c,
  (x+a/3)3 = a3/27 - c.

  By taking the cube root of the left side and the three cube roots of
  the right side, you get

  x = -a/3 + (a3/27 - c)1/3, 
  x = -a/3 + (a3/27 - c)1/3(-1+sqrt[-3])/2, 
  x = -a/3 + (a3/27 - c)1/3(-1-sqrt[-3])/2.

  These are the roots of the cubic equation that were sought.

  If a2 - 3b is nonzero, then proceed as follows. Set x = y + z, where y
  is an indeterminate and z is a function of a, b, and c, which will be
  found below. Then

  (y+z)3 + a(y+z)2 + b(y+z) + c = 0, 
  y3 + (3z+a)y2 + (3z2+2az+b)y + (z3+az2+bz+c) = 0,
  y3 + dy2 + ey + f = 0,

  where

  d = 3z + a,
  e = 3z2 + 2az + b,
  f = z3 + az2 + bz + c.

  The first three terms of this equation in y will be those of a perfect
  cube if and only if d2 - 3e is zero, which happens if and only if a2 -
  3b = 0, which cannot happen in this case, so we seemingly haven't
  gained anything. However, the last three terms of this equation in y
  will be those of a perfect cube if and only if e2 = 3df, that is if
  and only if

  (3z2+2az+b)2 = 3(3z+a)(z3+az2+bz+c),
  (a2-3b)z2 + (ab-9c)z + (b2-3ac) = 0,
  gz2 + hz + i = 0,

  where

  g = a2 - 3b,
  h = ab - 9c,
  i = b2 - 3ac.

  Since a2 - 3b is nonzero, g is nonzero, and we have a true quadratic
  equation, called the resolvent quadratic. Now we pick z to be a root
  of this quadratic equation. If the GCD of z2 + (h/g)z + (i/g) and z3 +
  az2 + bz + c as polynomials in z is not 1, then any root of the GCD is
  also a root of the original cubic equation in x. Once you have at
  least one root, the problem of finding the other roots is reduced to
  solving a quadratic or linear equation. If the GCD is 1, then neither
  value of z can make f = 0, so we can assume henceforth that f is
  nonzero. Either root z of the quadratic will do, but we must choose
  one of them. We arbitrarily pick the one with a plus sign in front of
  the radical,

  z = (-h+sqrt[h2-4gi])/(2g), 
  = (9c-ab+sqrt[81c2-54abc+12a3c+12b3-3a2b2])/(2[a2-3b]).

  Set z equal to this value in the equation for y, and divide it by f on
  both sides. Then the last three terms of the cubic in y are those of a
  perfect cube, namely (ey/[3f]+1)3, so we can complete the cube to
  solve it. We do this by subtracting y3/f from both sides, then adding
  the missing term of the cubic, (ey/[3f])3 to both sides, obtaining

  (ey/[3f]+1)3 = ([e/(3f)]3-1/f)y3,

  ey/(3f) + 1 = y([e/(3f)]3-1/f)1/3,
  ey/(3f) + 1 = y([e/(3f)]3-1/f)1/3(-1+sqrt[-3])/2,
  ey/(3f) + 1 = y([e/(3f)]3-1/f)1/3(-1-sqrt[-3])/2,

  y = 3f/[-e+(e3-27f2)1/3],
  y = 3f/[-e+(e3-27f2)1/3(-1+sqrt[-3])/2],
  y = 3f/[-e+(e3-27f2)1/3(-1-sqrt[-3])/2].

  Now you have the values of y. Add z to each to get the values of x:

  x = z + 3f/[(e3-27f2)1/3-e],
  x = z + 3f/[(e3-27f2)1/3(-1+sqrt[-3])/2-e],
  x = z + 3f/[(e3-27f2)1/3(-1-sqrt[-3])/2-e].

  These are the roots of the cubic equation that were sought.

  Example: Solve x3 + 6x2 + 9x + 6 = 0.

  a = 6,   b = 9,   c = 6.

  Then

  d = 3z + 6,   e = 3z2 + 12z + 9,   f = z3 + 6z2 + 9z + 6.

  Then

  g = a2 - 3b = 36 - 27 = 9,
  h = ab - 9c = 54 - 54 = 0,
  i = b2 - 3ac = 81 - 108 = -27,

  and the resolvent quadratic is

  9z2 - 27 = 0,
  z2 - 3 = 0,
  z = rtapi_sqrt(3).

  Then

  d = 6 + 3 rtapi_sqrt(3),
  e = 18 + 12 rtapi_sqrt(3),
  f = 24 + 12 rtapi_sqrt(3),

  and the cubic in y is

  y3 + (6+3 sqrt[3])y2 + (18+12 sqrt[3])y + (24+12 sqrt[3]) = 0. 

  Then one root is

  y = 3f/[(e3-27f2)1/3-e],
  = 3(24+12 sqrt[3])/([18+12 rtapi_sqrt(3)]3-27[24+12 rtapi_sqrt(3)]2)1/3-18-12 sqrt[3]),
  = (12+6 sqrt[3])/([9+6 rtapi_sqrt(3)]1/3-3-2 sqrt[3]),
  = (6+4 sqrt[3])/([2+rtapi_sqrt(3)]1/3-2-sqrt[3]).

  After a lot of simplification, you get

  y = -2 - rtapi_sqrt(3) - (2-sqrt[3])1/3 - (2+sqrt[3])1/3,
  x = -2 - (2-sqrt[3])1/3 - (2+sqrt[3])1/3

  (and two other roots). 
*/

go_complex go_complex_add(go_complex z1, go_complex z2)
{
  go_complex sum;

  sum.re = z1.re + z2.re;
  sum.im = z1.im + z2.im;

  return sum;
}

go_complex go_complex_sub(go_complex z1, go_complex z2)
{
  go_complex diff;

  diff.re = z1.re - z2.re;
  diff.im = z1.im - z2.im;

  return diff;
}

go_complex go_complex_mult(go_complex z1, go_complex z2)
{
  go_complex prod;

  prod.re = (z1.re * z2.re) - (z1.im * z2.im);
  prod.im = (z1.re * z2.im) + (z1.im * z2.re);

  return prod;
}

go_complex go_complex_div(go_complex z1, go_complex z2, int * result)
{
  go_complex z2c = {0, 0};
  go_real denom;

  denom = go_sq(z2.re) + go_sq(z2.im);
  if (denom < GO_REAL_EPSILON) {
    *result = GO_RESULT_DIV_ERROR;
    return z2c;
  }

  z2c.re = z2.re, z2c.im = -z2.im; /* complex conjugate */
  *result = GO_RESULT_OK;
  return go_complex_scale(go_complex_mult(z1, z2c), 1.0 / denom);
}

go_complex go_complex_scale(go_complex z, go_real scale)
{
  go_complex s;

  s.re = scale * z.re;
  s.im = scale * z.im;

  return s;
}

go_real go_complex_mag(go_complex z)
{
  return rtapi_sqrt(go_sq(z.re) + go_sq(z.im));
}

go_real go_complex_arg(go_complex z)
{
  return rtapi_atan2(z.im, z.re);
}

/*
  go_complex_rtapi_sqrt() and go_complex_cbrt() are structured so that if
  the roots of complex conjugates are taken, the first roots z1 will
  be complex conjugates.  This is due to atan2 returning args in the
  range -PI to PI.
 */

void go_complex_rtapi_sqrt(go_complex z, go_complex * z1, go_complex * z2)
{
  go_real r, th;

  r = rtapi_sqrt(go_complex_mag(z));
  th = 0.5 * go_complex_arg(z);

  z1->re = r * rtapi_cos(th);
  z1->im = r * rtapi_sin(th);
  if (NULL != z2) {
    z2->re = -z1->re;
    z2->im = -z1->im;
  }

  return;
}

void go_complex_cbrt(go_complex z, go_complex * z1, go_complex * z2, go_complex * z3)
{
  go_real r, th;

  r = go_cbrt(go_complex_mag(z));
  th = 1.0/3.0 * go_complex_arg(z);

  z1->re = r * rtapi_cos(th);
  z1->im = r * rtapi_sin(th);
  if (NULL != z2) {
    z2->re = r * rtapi_cos(th + 2.0/3.0 * GO_PI);
    z2->im = r * rtapi_sin(th + 2.0/3.0 * GO_PI);
  }
  if (NULL != z3) {
    z3->re = r * rtapi_cos(th + 4.0/3.0 * GO_PI);
    z3->im = r * rtapi_sin(th + 4.0/3.0 * GO_PI);
  }

  return;
}

int go_quadratic_solve(const go_quadratic * quad,
			     go_complex * z1,
			     go_complex * z2)
{
  go_real discr;

  /* check if b is 0 and we can reduce order */
  if (GO_SMALL(quad->b)) {
    z2->re = z2->im = 0.0;
    z1->re = -quad->a;
    z1->im = 0.0;
    return GO_RESULT_OK;
  }

  discr = go_sq(quad->a) - 4.0 * quad->b;
  if (discr < 0.0) {
    discr = rtapi_sqrt(-discr);
    z1->re = z2->re = -0.5 * quad->a;
    z1->im = 0.5 * discr;
    z2->im = -0.5 * discr;
  } else {
    discr = rtapi_sqrt(discr);
    z1->re = 0.5 * (-quad->a + discr);
    z2->re = 0.5 * (-quad->a - discr);
    z1->im = z2->im = 0.0;
  }

  return GO_RESULT_OK;
}

/*
  References on solving the cubic: 

  "Solving Cubic and Quartic Polynomials,"
  www.karlscalculus.org/pdf/cubicQuartic.pdf

  "Cubic Equations in One Formula,"
  http://mathforum.org/library/drmath/view/52668.html
*/
int go_cubic_solve(const go_cubic * cub,
			 go_complex * z1,
			 go_complex * z2,
			 go_complex * z3)
{
  go_quadratic quad;
  go_real a, b;
  go_complex z, A, B, I = {0.0, 1.0};
  go_complex u1, u2, u3;

  /* check if c is 0, and we have a root of 0 and can reduce order
     to a quadratic */
  if (GO_SMALL(cub->c)) {
    z3->re = z3->im = 0.0;
    quad.a = cub->a;
    quad.b = cub->b;
    return go_quadratic_solve(&quad, z1, z2);
  }

  a = cub->b - 1.0/3.0 * go_sq(cub->a);
  b = cub->c - 1.0/3.0 * cub->a * cub->b + 2.0/27.0 * go_cub(cub->a);
  z.re = 0.25 * go_sq(b) + 1.0/27.0 * go_cub(a), z.im = 0.0;
  go_complex_rtapi_sqrt(z, &A, &B);

  /* since z is real, the pair of rtapi_sqrt(z) is either pure real or 
     pure complex */
  A.re -= 0.5 * b;
  B.re -= 0.5 * b;
  /* subtracting b/2 keeps them pure real and in general unequal, or
     makes them complex conjugates */

  /* Now we need to take the cube root of each. If they're pure real,
     we can take the scalar cube root. */
  if (GO_SMALL(A.im)) {
    A.re = go_cbrt(A.re), A.im = 0.0;
    B.re = go_cbrt(B.re), B.im = 0.0;
  } else {
    go_complex_cbrt(A, &A, NULL, NULL);
    B.re = A.re, B.im = -A.im;	/* B and A must be complex conjugates */
  }
  u1 = go_complex_add(A, B);
  u2 = u3 = go_complex_scale(u1, -0.5);
  z = go_complex_scale(go_complex_sub(A, B), rtapi_sqrt(0.75));
  z = go_complex_mult(I, z);
  u2 = go_complex_add(u2, z);
  u3 = go_complex_sub(u3, z);

  z1->re = u1.re - 1.0/3.0 * cub->a, z1->im = u1.im;
  z2->re = u2.re - 1.0/3.0 * cub->a, z2->im = u2.im;
  z3->re = u3.re - 1.0/3.0 * cub->a, z3->im = u3.im;

  return GO_RESULT_OK;
}

/*
  http://mathforum.org/dr.math/faq/faq.cubic.equations.html

  Solving the quartic with no third-order term:

  The quartic in y must factor into two quadratics with real
  coefficients, since any complex roots must occur in conjugate
  pairs. Write

  y4 + e y2 + f y + g = (y2 + h y + j) (y2 - h y + g/j)

  Since f and g aren't zero, neither j nor h is zero either. Without
  loss of generality, we can assume h > 0 (otherwise swap the two
  factors). Then, equating coefficients of y2 and y,

  e = g/j + j - h2, f = h (g/j - j).

  so

  g/j + j = e + h2, g/j - j = f/h.

  Adding and subtracting these two equations,

  2 g/j = e + h2 + f/h, 2 j = e + h2 - f/h.

  Multiplying these together,

  4 g = e2 + 2 e h2 + h4 - f2/h2.

  Rearranging,

  h6 + 2 e h4 + (e2-4 g) h2 - f2 = 0.

  This is a cubic equation in h2, with known coefficients, since we know
  e, f, and g. We can use the solution for cubic equations shown above
  to find a positive real value of h2, whose existence is guaranteed by
  Descartes' Rule of Signs, and then take its positive square root. This
  value of h will give a value of j from 2 j = e + h2 - f/h. Once you
  know h and j, you know the quadratic factors of the quartic in
  y. Notice that you do not need all the roots h2 of the cubic, and that
  any positive real one will do. Further notice that h2 = 4 z from the
  previous section.

  Once you have factored the quartic into two quadratics, finishing the
  finding of the roots is simple, using the Quadratic Formula. Once the
  roots y are found, the corresponding x's are gotten from x = y - a/4.
*/

int go_quartic_solve(const go_quartic * quart,
			   go_complex * z1,
			   go_complex * z2,
			   go_complex * z3,
			   go_complex * z4)
{
  go_cubic cub;
  go_quadratic quad;
  go_real a2, a3, a4;
  go_real e, f, g;
  go_complex p, q, r, fc, a4c;
  int retval;

  /*
    There are several ways to solve quartic equations. They all start by
    dividing the equation by the leading coefficient, to make it of the
    form

    x4 + a x3 + b x2 + c x + d = 0. 

    There is an easy special case if d = 0, in which case you can factor
    the quartic into x times a cubic. The roots then are 0 and the roots
    of that cubic. (To solve the cubic equation, see above.)
  */
  if (GO_SMALL(quart->d)) {
    /* set one root to be 0 and solve for reduced cubic */
    z4->re = z4->im = 0.0;
    cub.a = quart->a;
    cub.b = quart->b;
    cub.c = quart->c;
    return go_cubic_solve(&cub, z1, z2, z3);
  }

  /*
    First eliminate the third-order term. Substitute x = y - a/4,
    expand, and simplify, to get

    y4 + e y2 + f y + g = 0 

    where

    e = b - 3 a2/8,
    f = c + a3/8 - a b/2,
    g = d - 3 a4/256 + a2 b/16 - a c/4. 
  */
  a2 = go_sq(quart->a);
  e = quart->b - 3.0/8.0 * a2;
  a3 = a2 * quart->a;
  f = quart->c + 1.0/8.0 * a3 - 0.5 * (quart->a * quart->b);
  a4 = a2 * a2;
  g = quart->d - 3.0/256.0 * a4 + 1.0/16.0 * a2 * quart->b - 0.25 * (quart->a * quart->c);
  /*
    At this point, there are two useful special cases.

    1. If g = 0, then, again, you can factor the quartic into y times
    a cubic. The roots of the original equation are then x = -a/4
    and the roots of that cubic with a/4 subtracted from each.

    2. If f = 0, then the quartic in y is actually a quadratic
    equation in the variable y2. Solve this using your favorite
    method, and then take the two square roots of each of the
    solutions for y2 to find the four values of y which
    work. Subtract a/4 from each to get the four roots x.

    Henceforth we can assume that d, f, and g are all nonzero.
  */
  /* Case 1 */
  if (GO_SMALL(g)) {
    a4 = -0.25 * quart->a;	/* reuse a4 as -a/4, save a var */
    z4->re = a4;
    z4->im = 0.0;
    cub.a = 0.0;
    cub.b = e;
    cub.c = f;
    retval = go_cubic_solve(&cub, z1, z2, z3);
    if (GO_RESULT_OK != retval) return retval;
    z1->re += a4;
    z2->re += a4;
    z3->re += a4;
    return GO_RESULT_OK;
  }
  /* Case 2 */
  if (GO_SMALL(f)) {
    a4 = -0.25 * quart->a;	/* reuse a4 as -a/4, save a var */
    quad.a = e;
    quad.b = g;
    retval = go_quadratic_solve(&quad, z1, z3);
    if (GO_RESULT_OK != retval) return retval;
    go_complex_rtapi_sqrt(*z1, z1, z2);
    go_complex_rtapi_sqrt(*z3, z3, z4);
    z1->re += a4;
    z2->re += a4;
    z3->re += a4;
    z4->re += a4;
    return GO_RESULT_OK;
  }

  /*
    Here is one way to proceed from this point. The related auxiliary
    cubic equation

    z3 + (e/2) z2 + ((e2-4 g)/16) z - f2/64 = 0 

    has three roots. Find those roots by solving this cubic (again,
    see above). None of these is zero because f isn't zero. Let p and
    q be the square roots of two of those roots (any choices of roots
    and signs will work), and set

    r = -f/(8 p q). 

    Then p2, q2, and r2 are the three roots of the above cubic. More
    important is the fact that the four roots of the original quartic
    are

    x = p + q + r - a/4,
    x = p - q - r - a/4,
    x = -p + q - r - a/4, and
    x = -p - q + r - a/4. 

    This solution was discovered by Leonhard Euler (1707-1783).
  */
  cub.a = 0.5 * e;
  cub.b = 1.0/16.0 * (go_sq(e) - 4.0 * g);
  cub.c = -1.0/64.0 * go_sq(f);
  retval = go_cubic_solve(&cub, z1, z2, z3);
  if (GO_RESULT_OK != retval) return retval;
  go_complex_rtapi_sqrt(*z1, &p, NULL);
  go_complex_rtapi_sqrt(*z2, &q, NULL);
  fc.re = f, fc.im = 0.0;	/* fc is complex of scalar f */
  r = go_complex_div(fc, go_complex_scale(go_complex_mult(p, q), -8.0), &retval);
  if (GO_RESULT_OK != retval) return retval;

  a4c.re = 0.25 * quart->a, a4c.im = 0.0; /* a/4 as a complex */
  *z1 = go_complex_sub(go_complex_add(go_complex_add(p, q), r), a4c);
  *z2 = go_complex_sub(go_complex_sub(go_complex_sub(p, q), r), a4c);
  *z3 = go_complex_sub(go_complex_sub(go_complex_sub(q, p), r), a4c);
  *z4 = go_complex_sub(go_complex_sub(go_complex_sub(r, p), q), a4c);

  return GO_RESULT_OK;
}

#define SIGN(a,b) ((b) >= 0.0 ? rtapi_fabs(a) : -rtapi_fabs(a))
#define MAG2(a,b) rtapi_sqrt((a)*(a)+(b)*(b))

/*!
  This computes the Householder reduction to tridiagonal form of a
  real symmetric matrix \a a. \a a will contain the orthogonal matrix
  that effects the transformation. \a d will contain the diagonal
  elements, and \a e will contain the off-diagonal elements. This is
  used by go_tridiag_ql to find the eigenvalues and eigenvectors of a
  real symmetric matrix.
 */
int go_tridiag_reduce(go_real ** a, /*< the real symmetric matrix,
					   overwritten with the diagonalizing
					   matrix  */
			    go_integer n, /*< how many rows and columns */
			    go_real * d, /*< the diagonal elements will be
					   stored here  */
			    go_real * e) /*< the off-diagonal elements
					   will be stored here, starting
					   at index 1 */
{
  go_integer l,k,j,i;
  go_real scale,hh,h,g,f;

  for (i=n-1;i>0;i--) {
    l=i-1;
    h=scale=0.0;
    if (l > 0) {
      for (k=0;k<=l;k++)
	scale += rtapi_fabs(a[i][k]);
      if (scale == 0.0)
	e[i]=a[i][l];
      else {
	for (k=0;k<=l;k++) {
	  a[i][k] /= scale;
	  h += a[i][k]*a[i][k];
	}
	f=a[i][l];
	g=(f >= 0.0 ? -rtapi_sqrt(h) : rtapi_sqrt(h));
	e[i]=scale*g;
	h -= f*g;
	a[i][l]=f-g;
	f=0.0;
	for (j=0;j<=l;j++) {
	  a[j][i]=a[i][j]/h;
	  g=0.0;
	  for (k=0;k<=j;k++)
	    g += a[j][k]*a[i][k];
	  for (k=j+1;k<=l;k++)
	    g += a[k][j]*a[i][k];
	  e[j]=g/h; 
	  f += e[j]*a[i][j];
	}
	hh=f/(h+h);
	for (j=0;j<=l;j++) { 
	  f=a[i][j];
	  e[j]=g=e[j]-hh*f;
	  for (k=0;k<=j;k++)
	    a[j][k] -= (f*e[k]+g*a[i][k]);
	}
      }
    } else
      e[i]=a[i][l];
    d[i]=h;
  }
  d[0]=0.0;
  e[0]=0.0;
  for (i=0;i<n;i++) {
    l=i-1;
    if (d[i]) {
      for (j=0;j<=l;j++) {
	g=0.0;
	for (k=0;k<=l;k++)
	  g += a[i][k]*a[k][j];
	for (k=0;k<=l;k++)
	  a[k][j] -= g*a[k][i];
      }
    }
    d[i]=a[i][i];
    a[i][i]=1.0; 
    for (j=0;j<=l;j++) a[j][i]=a[i][j]=0.0;
  }

  return GO_RESULT_OK;
}

/*!
  This computes the eigenvalues and eigenvectors of the
  tridiagonalized matrix with diagonal \a d and off-diagonal \a e,
  using QL reduction with implicit shifts. \a d, \a e and \a z should
  have been generated by a prior call to go_tridiag_reduce. \a d will
  contain the eigenvalues, and \a z will contain the eigenvectors.

  Calling example:

  go_tridiag_reduce(matrix, 3, d, e);
  go_tridiag_ql(d, e, 3, matrix);

  \a d will contain the eigenvalues of the original \a matrix, and
  the new \a matrix will contain the eigenvectors.
*/
int go_tridiag_ql(go_real * d,
			go_real * e,
			go_integer n,
			go_real ** z)
{
  go_integer m,l,iter,i,k;
  go_real s,r,p,g,f,dd,c,b;

  for (i=1;i<n;i++) e[i-1]=e[i];
  e[n-1]=0.0;
  for (l=0;l<n;l++) {
    iter=0;
    do {
      for (m=l;m<n-1;m++) {
	dd=rtapi_fabs(d[m])+rtapi_fabs(d[m+1]);
	if ((go_real)(rtapi_fabs(e[m])+dd) == dd) break;
      }
      if (m != l) {
	if (iter++ == 30) return GO_RESULT_ERROR;
	g=(d[l+1]-d[l])/(2.0*e[l]);
	r=MAG2(g,1.0);
	g=d[m]-d[l]+e[l]/(g+SIGN(r,g));
	s=c=1.0;
	p=0.0;
	for (i=m-1;i>=l;i--) {
	  f=s*e[i];
	  b=c*e[i];
	  e[i+1]=(r=MAG2(f,g));
	  if (r == 0.0) {
	    d[i+1] -= p;
	    e[m]=0.0;
	    break;
	  }
	  s=f/r;
	  c=g/r;
	  g=d[i+1]-p;
	  r=(d[i]-g)*s+2.0*c*b;
	  d[i+1]=g+(p=s*r);
	  g=c*r-b;
	  for (k=0;k<n;k++) {
	    f=z[k][i+1];
	    z[k][i+1]=s*z[k][i]+c*f;
	    z[k][i]=c*z[k][i]-s*f;
	  }
	}
	if (r == 0.0 && i >= l) continue;
	d[l] -= p;
	e[l]=g;
	e[m]=0.0;
      }
    } while (m != l);
  }

  return GO_RESULT_OK;
}

int go_cart_cart_pose(const go_cart * v1, const go_cart * v2,
			    go_cart * v1c, go_cart * v2c,
			    go_integer num, go_pose * p)
{
  go_integer t;
  go_cart c1, c2;
  go_real Sxx, Sxy, Sxz;
  go_real Syx, Syy, Syz;
  go_real Szx, Szy, Szz;
  GO_MATRIX_DECLARE(N, Nspace, 4, 4);
  go_real d[4], e[4];
  go_real eigenval;
  int retval;

  Sxx = Sxy = Sxz = 0.0;
  Syx = Syy = Syz = 0.0;
  Szx = Szy = Szz = 0.0;

  go_matrix_init(N, Nspace, 4, 4);

  retval = go_cart_centroidize(v1, num, &c1, v1c);
  if (GO_RESULT_OK != retval) return retval;
  retval = go_cart_centroidize(v2, num, &c2, v2c);
  if (GO_RESULT_OK != retval) return retval;

  for (t = 0; t < num; t++) {
    Sxx += v1c[t].x * v2c[t].x;
    Sxy += v1c[t].x * v2c[t].y;
    Sxz += v1c[t].x * v2c[t].z;

    Syx += v1c[t].y * v2c[t].x;
    Syy += v1c[t].y * v2c[t].y;
    Syz += v1c[t].y * v2c[t].z;

    Szx += v1c[t].z * v2c[t].x;
    Szy += v1c[t].z * v2c[t].y;
    Szz += v1c[t].z * v2c[t].z;
  }

  N.el[0][0] = Sxx + Syy + Szz;
  N.el[0][1] = N.el[1][0] = Syz - Szy;
  N.el[0][2] = N.el[2][0] = Szx - Sxz;
  N.el[0][3] = N.el[3][0] = Sxy - Syx;

  N.el[1][1] = Sxx - Syy - Szz;
  N.el[1][2] = N.el[2][1] = Sxy + Syx;
  N.el[1][3] = N.el[3][1] = Szx + Sxz;
  
  N.el[2][2] = -Sxx + Syy - Szz;
  N.el[2][3] = N.el[3][2] = Syz + Szy;

  N.el[3][3] = -Sxx -Syy + Szz;

  /* compute eigenvectors */
  retval = go_tridiag_reduce(N.el, 4, d, e);
  if (GO_RESULT_OK != retval) return retval;
  retval = go_tridiag_ql(d, e, 4, N.el);
  if (GO_RESULT_OK != retval) return retval;

  /* pick eigenvector associated with most positive eigenvalue */
  eigenval = d[0], t = 0;
  if (d[1] > eigenval) eigenval = d[1], t = 1;
  if (d[2] > eigenval) eigenval = d[2], t = 2;
  if (d[3] > eigenval) eigenval = d[3], t = 3;
  /* now t is the column with the eigenvector we want as our quaternion */
  p->rot.s = N.el[0][t];
  p->rot.x = N.el[1][t];
  p->rot.y = N.el[2][t];
  p->rot.z = N.el[3][t];
  retval = go_quat_norm(&p->rot, &p->rot);
  if (GO_RESULT_OK != retval) return retval;

  /* rotate left centroid, subtract right from left to get translation */
  (void) go_quat_cart_mult(&p->rot, &c1, &c1);
  (void) go_cart_cart_sub(&c2, &c1, &p->tran);

  return GO_RESULT_OK;
}

static int trilaterate(go_real x2, go_real x3, go_real y3,
			     go_real l1, go_real l2, go_real l3,
			     go_cart * p)
{
  go_real discr;

  p->x = 0.5 * (x2 - (go_sq(l2) - go_sq(l1))/x2);

  p->y =
    (go_sq(l1) - 2.0 * p->x * x3 + go_sq(x3) +
     go_sq(y3) - go_sq(l3)) / (2.0 * y3);

  discr = go_sq(l1) - go_sq(p->x) - go_sq(p->y);
  if (discr < -GO_REAL_EPSILON) {
    return GO_RESULT_DOMAIN_ERROR;
  }
  if (discr < 0.0) discr = 0.0;	/* make slightly negative numbers 0 */

  /* return z positive, caller can make negative if they want the
     point below the base */
  p->z = rtapi_sqrt(discr);

  return GO_RESULT_OK;
}

int go_cart_trilaterate(const go_cart * c1, 
			      const go_cart * c2,
			      const go_cart * c3,
			      go_real l1,
			      go_real l2,
			      go_real l3,
			      go_cart * out1,
			      go_cart * out2)
{
  go_pose P_to_B, B_to_P;
  go_mat mat;
  go_cart diff, proj;
  go_cart p1, p2, p3, out1_in_P, out2_in_P;
  int retval;

  /* transform the points from their original base frame {B} into an
     easier frame {P}, with c1 at the {P}, origin, c2 along the {P} x
     axis, and all three in the {P} xy plane */

  /* first get the transform */
  P_to_B.tran = *c1;
  go_cart_cart_sub(c2, c1, &diff);
  retval = go_cart_unit(&diff, &mat.x);
  if (GO_RESULT_OK != retval) return retval;
  go_cart_cart_sub(c3, c1, &diff);
  retval = go_cart_cart_proj(&diff, &mat.x, &proj);
  if (GO_RESULT_OK != retval) return retval;
  go_cart_cart_sub(&diff, &proj, &diff);
  retval = go_cart_unit(&diff, &mat.y);
  if (GO_RESULT_OK != retval) return retval;
  go_cart_cart_cross(&mat.x, &mat.y, &mat.z);
  retval = go_mat_quat_convert(&mat, &P_to_B.rot);
  if (GO_RESULT_OK != retval) return retval;
  retval = go_pose_inv(&P_to_B, &B_to_P);
  if (GO_RESULT_OK != retval) return retval;
  /* now transform them */
  go_pose_cart_mult(&B_to_P, c1, &p1);
  go_pose_cart_mult(&B_to_P, c2, &p2);
  go_pose_cart_mult(&B_to_P, c3, &p3);

  /* trilaterate to get the point in {P} */
  retval = trilaterate(p2.x, p3.x, p3.y, l1, l2, l3, &out1_in_P);
  if (GO_RESULT_OK != retval) return retval;
  out2_in_P.x = out1_in_P.x;
  out2_in_P.y = out1_in_P.y;
  out2_in_P.z = -out1_in_P.z;	/* the other one has a minus z */
  go_pose_cart_mult(&P_to_B, &out1_in_P, out1);
  go_pose_cart_mult(&P_to_B, &out2_in_P, out2);

  return GO_RESULT_OK;
}

/* go_rvec functions */

go_flag go_rvec_rvec_compare(const go_rvec * r1, const go_rvec * r2)
{
  if (GO_ROT_CLOSE(r1->x, r2->x) &&
      GO_ROT_CLOSE(r1->y, r2->y) &&
      GO_ROT_CLOSE(r1->z, r2->z)) {
    return 1;
  }

  return 0;
}

int go_rvec_scale_mult(const go_rvec * r, go_real s, go_rvec * rout)
{
  rout->x = r->x * s;
  rout->y = r->y * s;
  rout->z = r->z * s;

  return GO_RESULT_OK;
}

/* go_mat functions */

int go_mat_norm(const go_mat * mat, go_mat * mout)
{
  go_cart yprojx;
  int retval;

  /* unitize the X vector, which we must be able to do */
  retval = go_cart_unit(&mat->x, &mout->x);
  if (GO_RESULT_OK != retval) return retval;

  /* project Y onto X, subtract from Y to get Y perp to X, and unitize */
  retval = go_cart_cart_proj(&mat->y, &mout->x, &yprojx);
  if (GO_RESULT_OK != retval) return retval;
  go_cart_cart_sub(&mat->y, &yprojx, &mout->y);
  retval = go_cart_unit(&mout->y, &mout->y);
  if (GO_RESULT_OK != retval) return retval;

  /* cross the new X and Y to get Z */
  return go_cart_cart_cross(&mout->x, &mout->y, &mout->z);
}

go_flag go_mat_is_norm(const go_mat * m)
{
  go_cart u;

  go_cart_cart_cross(&m->x, &m->y, &u);

#define COL_IS_UNIT(r) GO_TRAN_CLOSE(go_sq((r).x) + go_sq((r).y) + go_sq((r).z), 1)
  return (COL_IS_UNIT(m->x) &&
	  COL_IS_UNIT(m->y) &&
	  COL_IS_UNIT(m->z) &&
	  go_cart_cart_compare(&u, &m->z));
#undef COL_IS_UNIT
}

int go_mat_inv(const go_mat * m, go_mat * mout)
{
  go_real cp;

  /* inverse of a rotation matrix is the transpose */

  mout->x.x = m->x.x;
  mout->y.y = m->y.y;
  mout->z.z = m->z.z;

  cp = m->x.y;
  mout->x.y = m->y.x;
  mout->y.x = cp;

  cp = m->x.z;
  mout->x.z = m->z.x;
  mout->z.x = cp;

  cp = m->y.z;
  mout->y.z = m->z.y;
  mout->z.y = cp;

  return GO_RESULT_OK;
}

int go_mat_cart_mult(const go_mat * m, const go_cart * v,
			   go_cart * vout)
{
  go_cart cp;

  cp = *v;

  vout->x = m->x.x * cp.x + m->y.x * cp.y + m->z.x * cp.z;
  vout->y = m->x.y * cp.x + m->y.y * cp.y + m->z.y * cp.z;
  vout->z = m->x.z * cp.x + m->y.z * cp.y + m->z.z * cp.z;

  return GO_RESULT_OK;
}

int go_mat_mat_mult(const go_mat * m1, const go_mat * m2, go_mat * mout)
{
  go_mat cp1, cp2;

  cp1 = *m1;
  cp2 = *m2;

  mout->x.x = cp1.x.x * cp2.x.x + cp1.y.x * cp2.x.y + cp1.z.x * cp2.x.z;
  mout->x.y = cp1.x.y * cp2.x.x + cp1.y.y * cp2.x.y + cp1.z.y * cp2.x.z;
  mout->x.z = cp1.x.z * cp2.x.x + cp1.y.z * cp2.x.y + cp1.z.z * cp2.x.z;

  mout->y.x = cp1.x.x * cp2.y.x + cp1.y.x * cp2.y.y + cp1.z.x * cp2.y.z;
  mout->y.y = cp1.x.y * cp2.y.x + cp1.y.y * cp2.y.y + cp1.z.y * cp2.y.z;
  mout->y.z = cp1.x.z * cp2.y.x + cp1.y.z * cp2.y.y + cp1.z.z * cp2.y.z;

  mout->z.x = cp1.x.x * cp2.z.x + cp1.y.x * cp2.z.y + cp1.z.x * cp2.z.z;
  mout->z.y = cp1.x.y * cp2.z.x + cp1.y.y * cp2.z.y + cp1.z.y * cp2.z.z;
  mout->z.z = cp1.x.z * cp2.z.x + cp1.y.z * cp2.z.y + cp1.z.z * cp2.z.z;

  return GO_RESULT_OK;
}

/* go_quat functions */

go_flag go_quat_quat_compare(const go_quat * q1, const go_quat * q2)
{
  if (GO_ROT_CLOSE(q1->s, q2->s) &&
      GO_TRAN_CLOSE(q1->x, q2->x) &&
      GO_TRAN_CLOSE(q1->y, q2->y) &&
      GO_TRAN_CLOSE(q1->z, q2->z)) {
    return 1;
  }

  /* note (0, x, y, z) = (0, -x, -y, -z) */
  if (! GO_ROT_SMALL(q1->s) ||
      GO_TRAN_CLOSE(q1->x, -q2->x) ||
      GO_TRAN_CLOSE(q1->y, -q2->y) ||
      GO_TRAN_CLOSE(q1->z, -q2->z)) {
    return 0;
  }

  return 1;
}

int go_quat_mag(const go_quat * quat, go_real * d)
{
  go_real sh;

  sh = rtapi_sqrt(go_sq(quat->x) + go_sq(quat->y) + go_sq(quat->z));

  *d = 2 * rtapi_atan2(sh, quat->s);

  return GO_RESULT_OK;
}

int go_quat_unit(const go_quat * q1, go_quat * qout)
{
  go_real d;

  (void) go_quat_mag(q1, &d);
  if (GO_SMALL(d)) return GO_RESULT_ERROR;
  d = 1.0 / d;

  return go_quat_scale_mult(q1, d, qout);
}

int go_quat_norm(const go_quat * q1, go_quat * qout)
{
  go_real size;

  size = rtapi_sqrt(go_sq(q1->s) + go_sq(q1->x) + go_sq(q1->y) + go_sq(q1->z));

  if (GO_ROT_SMALL(size)) {
    qout->s = 1;
    qout->x = 0;
    qout->y = 0;
    qout->z = 0;
    return GO_RESULT_NORM_ERROR;
  }
  size = 1.0 / size;

  if (q1->s >= 0) {
    qout->s = q1->s * size;
    qout->x = q1->x * size;
    qout->y = q1->y * size;
    qout->z = q1->z * size;

    return GO_RESULT_OK;
  } else {
    qout->s = -q1->s * size;
    qout->x = -q1->x * size;
    qout->y = -q1->y * size;
    qout->z = -q1->z * size;

    return GO_RESULT_OK;
  }
}

int go_quat_inv(const go_quat * q1, go_quat * qout)
{
  qout->s = q1->s;
  qout->x = -q1->x;
  qout->y = -q1->y;
  qout->z = -q1->z;

  if (!go_quat_is_norm(q1)) {
    return GO_RESULT_NORM_ERROR;
  }

  return GO_RESULT_OK;
}

go_flag go_quat_is_norm(const go_quat * q1)
{
  return GO_TRAN_CLOSE(go_sq(q1->s) + 
		       go_sq(q1->x) +
		       go_sq(q1->y) + 
		       go_sq(q1->z), 1);
}

int go_quat_scale_mult(const go_quat * q, go_real s, go_quat * qout)
{
  go_real sh;			/* sine of half angle */
  go_real ha;			/* half angle */
  go_real scale;		/* new sh / old sh */
    
  sh = rtapi_sqrt(go_sq(q->x) + go_sq(q->y) + go_sq(q->z));
  if (GO_SMALL(sh)) {
    /* zero rotation-- leave it alone */
    *qout = *q;
    return GO_RESULT_OK;
  }

  ha = rtapi_atan2(sh, q->s);
  ha *= s;
  scale = rtapi_sin(ha) / sh;

  qout->s = rtapi_cos(ha);
  qout->x = scale * q->x;
  qout->y = scale * q->y;
  qout->z = scale * q->z;

  return GO_RESULT_OK;
}

int go_quat_quat_mult(const go_quat * q1, const go_quat * q2,
			    go_quat * qout)
{
  go_quat cp1, cp2;

  if (!go_quat_is_norm(q1) || !go_quat_is_norm(q2)) {
    return GO_RESULT_NORM_ERROR;
  }

  cp1 = *q1;
  cp2 = *q2;

  qout->s = cp1.s * cp2.s - cp1.x * cp2.x - cp1.y * cp2.y - cp1.z * cp2.z;

  if (qout->s >= 0) {
    qout->x = cp1.s * cp2.x + cp1.x * cp2.s + cp1.y * cp2.z - cp1.z * cp2.y;
    qout->y = cp1.s * cp2.y - cp1.x * cp2.z + cp1.y * cp2.s + cp1.z * cp2.x;
    qout->z = cp1.s * cp2.z + cp1.x * cp2.y - cp1.y * cp2.x + cp1.z * cp2.s;
  } else {
    qout->s = -qout->s;
    qout->x = -cp1.s * cp2.x - cp1.x * cp2.s - cp1.y * cp2.z + cp1.z * cp2.y;
    qout->y = -cp1.s * cp2.y + cp1.x * cp2.z - cp1.y * cp2.s - cp1.z * cp2.x;
    qout->z = -cp1.s * cp2.z - cp1.x * cp2.y + cp1.y * cp2.x - cp1.z * cp2.s;
  }

  return GO_RESULT_OK;
}

int go_quat_cart_mult(const go_quat * q1, const go_cart * v2,
			    go_cart * vout)
{
  go_cart c;

  if (!go_quat_is_norm(q1)) {
    return GO_RESULT_NORM_ERROR;
  }

  c.x = q1->y * v2->z - q1->z * v2->y;
  c.y = q1->z * v2->x - q1->x * v2->z;
  c.z = q1->x * v2->y - q1->y * v2->x;

  vout->x = v2->x + 2 * (q1->s * c.x + q1->y * c.z - q1->z * c.y);
  vout->y = v2->y + 2 * (q1->s * c.y + q1->z * c.x - q1->x * c.z);
  vout->z = v2->z + 2 * (q1->s * c.z + q1->x * c.y - q1->y * c.x);

  return GO_RESULT_OK;
}

/* go_pose functions*/

go_flag go_pose_pose_compare(const go_pose * p1, const go_pose * p2)
{
  return (go_quat_quat_compare(&p1->rot, &p2->rot) &&
	  go_cart_cart_compare(&p1->tran, &p2->tran));
}

int go_pose_inv(const go_pose * p1, go_pose * p2)
{
  int retval;

  retval = go_quat_inv(&p1->rot, &p2->rot);
  if (GO_RESULT_OK != retval) return retval;

  retval = go_quat_cart_mult(&p2->rot, &p1->tran, &p2->tran);
  if (GO_RESULT_OK != retval) return retval;

  p2->tran.x = -p2->tran.x;
  p2->tran.y = -p2->tran.y;
  p2->tran.z = -p2->tran.z;

  return GO_RESULT_OK;
}

int go_pose_cart_mult(const go_pose * p1, const go_cart * v2, go_cart * vout)
{
  int retval;

  /* first rotate the vector */
  retval = go_quat_cart_mult(&p1->rot, v2, vout);
  if (GO_RESULT_OK != retval) return retval;

  /* then translate it */
  return go_cart_cart_add(&p1->tran, vout, vout);
}

int go_pose_pose_mult(const go_pose * p1, const go_pose * p2, go_pose * pout)
{
  go_pose out;
  int retval;

  retval = go_quat_cart_mult(&p1->rot, &p2->tran, &out.tran);
  if (GO_RESULT_OK != retval) return retval;

  retval = go_cart_cart_add(&p1->tran, &out.tran, &out.tran);
  if (GO_RESULT_OK != retval) return retval;

  retval = go_quat_quat_mult(&p1->rot, &p2->rot, &out.rot);

  *pout = out;

  return retval;
}

int go_pose_scale_mult(const go_pose * p1, go_real s, go_pose * pout)
{
  (void) go_cart_scale_mult(&p1->tran, s, &pout->tran);
  return go_quat_scale_mult(&p1->rot, s, &pout->rot);
}

int go_pose_pose_interp(go_real t1, const go_pose * p1, go_real t2, const go_pose * p2, go_real t3, go_pose * p3)
{
  go_pose pdiff;
  int retval;

  if (GO_CLOSE(t1, t2)) return GO_RESULT_ERROR;

  retval = go_pose_inv(p1, &pdiff);
  if (GO_RESULT_OK != retval) return retval;

  (void) go_pose_pose_mult(&pdiff, p2, &pdiff);
  (void) go_pose_scale_mult(&pdiff, (t3 - t1) / (t2 - t1), &pdiff);
  return go_pose_pose_mult(p1, &pdiff, p3);
}

/* homogeneous transform functions */

int go_hom_inv(const go_hom * h1, go_hom * h2)
{
  int retval;

  retval = go_mat_inv(&h1->rot, &h2->rot);
  if (GO_RESULT_OK != retval) return retval;

  retval = go_mat_cart_mult(&h2->rot, &h1->tran, &h2->tran);
  if (GO_RESULT_OK != retval) return retval;

  h2->tran.x = -h2->tran.x;
  h2->tran.y = -h2->tran.y;
  h2->tran.z = -h2->tran.z;

  return GO_RESULT_OK;
}

int go_pose_screw_mult(const go_pose * pose, const go_screw * screw, go_screw * out)
{
  go_pose poseinv;
  go_cart wxp;
  go_cart v;
  int retval;

  /*
    B    |B  . B  |
    .T = | R .  P |
    A    |A  .   A|

    B    B   A   A    A
    .v =  R ( v + w X  P )
    .    A              B

    B   B  A
    .w = R  w
    .   A
  */

  retval = go_pose_inv(pose, &poseinv);
  if (GO_RESULT_OK != retval) return retval;

  go_cart_cart_cross(&screw->w, &poseinv.tran, &wxp);
  go_cart_cart_add(&screw->v, &wxp, &v);
  go_quat_cart_mult(&pose->rot, &v, &out->v);
  go_quat_cart_mult(&pose->rot, &screw->w, &out->w);

  return GO_RESULT_OK;
}

/* line and plane functions */

int go_line_from_poGO_RESULT_direction(const go_cart * point, const go_cart * direction, go_line * line)
{
  int retval;

  retval = go_cart_unit(direction, &line->direction);
  if (GO_RESULT_OK != retval) return GO_RESULT_DIV_ERROR;

  line->point = *point;

  return GO_RESULT_OK;
}

int go_line_from_points(const go_cart * point1, const go_cart * point2, go_line * line)
{
  go_cart direction;
  int retval;

  (void) go_cart_cart_sub(point2, point1, &direction);
  retval = go_cart_unit(&direction, &line->direction);
  if (GO_RESULT_OK != retval) return GO_RESULT_DIV_ERROR;

  line->point = *point1;

  return GO_RESULT_OK;
}

int go_line_from_planes(const go_plane * plane1, const go_plane * plane2, go_line * line)
{
  enum {X, Y, Z} which;
  go_real max, ymax, denominv;

  (void) go_cart_cart_cross(&plane1->normal, &plane2->normal, &line->direction);
  if (GO_RESULT_OK != go_cart_unit(&line->direction, &line->direction)) {
    /* planes are parallel */
    return GO_RESULT_ERROR;
  }

  /* pick the cross product component with the largest magnitude,
     which will give the most robust calculations for the point
     on the line */
  max = rtapi_fabs(line->direction.x);
  which = X;
  ymax = rtapi_fabs(line->direction.y);
  if (ymax > max) {
    max = ymax;
    which = Y;
  }
  if (rtapi_fabs(line->direction.z) > max) {
    which = Z;
  }

  switch (which) {
  case X:
    denominv = 1.0/(plane1->normal.y*plane2->normal.z - 
		    plane2->normal.y*plane1->normal.z);
    line->point.y = (plane1->normal.z*plane2->d - 
		     plane2->normal.z*plane1->d)*denominv;
    line->point.z = (plane2->normal.y*plane1->d - 
		     plane1->normal.y*plane2->d)*denominv;
    line->point.x = 0;
    break;
  case Y:
    denominv = 1.0/(plane1->normal.z*plane2->normal.x - 
		    plane2->normal.z*plane1->normal.x);
    line->point.z = (plane1->normal.x*plane2->d - 
		     plane2->normal.x*plane1->d)*denominv;
    line->point.x = (plane2->normal.z*plane1->d - 
		     plane1->normal.z*plane2->d)*denominv;
    line->point.y = 0;
    break;
  default:
    denominv = 1.0/(plane1->normal.x*plane2->normal.y - 
		    plane2->normal.x*plane1->normal.y);
    line->point.x = (plane1->normal.y*plane2->d - 
		     plane2->normal.y*plane1->d)*denominv;
    line->point.y = (plane2->normal.x*plane1->d - 
		     plane1->normal.x*plane2->d)*denominv;
    line->point.z = 0;
    break;
  }

  return GO_RESULT_OK;
}

go_flag go_line_line_compare(const go_line * line1, const go_line * line2)
{
  go_cart diff;
  go_real dot;

  /* check that directions are parallel */
  go_cart_cart_dot(&line1->direction, &line2->direction, &dot);
  if (! GO_CLOSE(dot, 1)) return 0;

  /* check that points are along the direction */
  (void) go_cart_cart_sub(&line1->point, &line2->point, &diff);
  go_cart_cart_dot(&line1->direction, &diff, &dot);
  if (! GO_CLOSE(dot, 1)) return 0;

  return 1;
}

int go_line_evaluate(const go_line * line, go_real d, go_cart * point)
{
  go_cart v;

  (void) go_cart_scale_mult(&line->direction, d, &v);
  (void) go_cart_cart_add(&line->point, &v, point);

  return GO_RESULT_OK;
}

int go_poGO_RESULT_line_distance(const go_cart * point, const go_line * line, go_real * distance)
{
  *distance = rtapi_sqrt(
		   go_sq(line->direction.z * (point->y - line->point.y) -
			 line->direction.y * (point->z - line->point.z)) +
		   go_sq(line->direction.x * (point->z - line->point.z) -
			 line->direction.z * (point->x - line->point.x)) +
		   go_sq(line->direction.y * (point->x - line->point.x) -
			 line->direction.x * (point->y - line->point.y)));

  return GO_RESULT_OK;
}

int go_poGO_RESULT_line_proj(const go_cart * point, const go_line * line, go_cart * pout)
{
  go_cart vp;
  int retval;

  go_cart_cart_sub(point, &line->point, &vp);
  retval = go_cart_cart_proj(&vp, &line->direction, &vp);
  if (GO_RESULT_OK != retval) return retval;
  go_cart_cart_add(&line->point, &vp, pout);

  return GO_RESULT_OK;
}

int go_poGO_RESULT_plane_proj(const go_cart * point, const go_plane * plane, go_cart * proj)
{
  go_real denom;
  go_real k;

  denom =
    go_sq(plane->normal.x) + 
    go_sq(plane->normal.y) +
    go_sq(plane->normal.z);

  if (GO_TRAN_SMALL(denom)) return GO_RESULT_DIV_ERROR;

  k = -(plane->normal.x * point->x +
	plane->normal.y * point->y +
	plane->normal.z * point->z +
	plane->d)/denom;

  proj->x = point->x + k*plane->normal.x;
  proj->y = point->y + k*plane->normal.y;
  proj->z = point->z + k*plane->normal.z;

  return GO_RESULT_OK;
}

int go_line_plane_proj(const go_line * line, const go_plane * plane, go_line * proj)
{
  int retval;

  retval = go_cart_plane_proj(&line->direction, &plane->normal, &proj->direction);
  if (GO_RESULT_OK != retval) return retval;

  return go_poGO_RESULT_plane_proj(&line->point, plane, &proj->point);
}

int go_plane_from_poGO_RESULT_normal(const go_cart * point, const go_cart * normal, go_plane * plane)
{
  if (GO_RESULT_OK != go_cart_unit(normal, &plane->normal)) return GO_RESULT_ERROR;
  /* D = -(Ax + By + Cz) */
  plane->d = -(plane->normal.x * point->x +
	       plane->normal.y * point->y +
	       plane->normal.z * point->z);

  return GO_RESULT_OK;
}

int go_plane_from_abcd(go_real A, go_real B, go_real C, go_real D, go_plane * plane)
{
  go_real mag;

  /* get magnitude of normal vector, actually mag squared */
  mag = go_sq(A) + go_sq(B) + go_sq(C);
  if (GO_TRAN_SMALL(mag)) {
    return GO_RESULT_DIV_ERROR;
  }
  mag = 1.0 / rtapi_sqrt(mag);	/* now it's the mag multiplier */

  plane->normal.x = A * mag;
  plane->normal.y = B * mag;
  plane->normal.z = C * mag;
  plane->d = D * mag;

  return GO_RESULT_OK;
}

int go_plane_from_points(const go_cart * point1, const go_cart * point2, const go_cart * point3, go_plane * plane)
{
  go_cart v12, v23;

  /* cross vectors from 1-2, 2-3 to get normal */
  go_cart_cart_sub(point2, point1, &v12);
  go_cart_cart_sub(point3, point2, &v23);
  go_cart_cart_cross(&v12, &v23, &plane->normal);
  if (GO_RESULT_OK != go_cart_unit(&plane->normal, &plane->normal)) return GO_RESULT_ERROR;
  /* D = -(Ax + By + Cz) */
  plane->d = -(plane->normal.x * point1->x +
	       plane->normal.y * point1->y +
	       plane->normal.z * point1->z);

  return GO_RESULT_OK;
}

int go_plane_from_poGO_RESULT_line(const go_cart * point, const go_line * line, go_plane * plane)
{
  go_cart p0, p1;

  (void) go_line_evaluate(line, 0, &p0);
  (void) go_line_evaluate(line, 1, &p1);

  return go_plane_from_points(&p0, &p1, point, plane);
}

/* for planes to be the same, they must have the same normal vector
   and same 'd', so the comparison is simple */
go_flag go_plane_plane_compare(const go_plane * plane1, const go_plane * plane2)
{
  if (! go_cart_cart_compare(&plane1->normal, &plane2->normal)) return 0;
  return GO_CLOSE(plane1->d, plane2->d);
}

int go_poGO_RESULT_plane_distance(const go_cart * point, const go_plane * plane, go_real * distance)
{
  *distance = plane->normal.x * point->x +
    plane->normal.y * point->y +
    plane->normal.z * point->z +
    plane->d;

  return GO_RESULT_OK;
}

int go_plane_evaluate(const go_plane * plane, go_real u, go_real v, go_cart * point)
{
  go_cart v1, v2;		/* othogonal vectors in plane */
  go_cart p;			/* point in plane closest to origin */

  if (GO_RESULT_OK != go_cart_normal(&plane->normal, &v1)) return GO_RESULT_ERROR;
  
  (void) go_cart_cart_cross(&plane->normal, &v1, &v2);
  (void) go_cart_scale_mult(&v1, u, &v1);
  (void) go_cart_scale_mult(&v2, v, &v2);

  /* with point = k * normal,
     a*ka * y*ky + z*kz + d = 0,
     k*(1) + d = 0,
     k = -d
  */
  p.x = -plane->normal.x * plane->d;
  p.y = -plane->normal.y * plane->d;
  p.z = -plane->normal.z * plane->d;

  (void) go_cart_cart_add(&p, &v1, &p);
  (void) go_cart_cart_add(&p, &v2, point);

  return GO_RESULT_OK;
}

/*
  To get the intersect point of the line and plane, set them equal and
  solve for the line parameter d, then plug into the parameterization
  to get the point:

  A(px + d*vx) + B(py + d*vy) + C(pz + d*vz) + D = 0

  _   -(A*px + B*py + C*pz + D)
  d   ---------------------
  _     A*vx + B*vy + C*vz

  Then plug d into px + d*vx, ... to get intersect x, y and z.
*/
int go_line_plane_intersect(const go_line * line, const go_plane * plane, go_cart * point, go_real * distance)
{
  go_real num, denom;

  /* first check for line parallel to plane */
  (void) go_cart_cart_dot(&plane->normal, &line->direction, &denom);
  if (GO_SMALL(denom)) return GO_RESULT_ERROR;

  /* check for point lying in plane */
  (void) go_cart_cart_dot(&plane->normal, &line->point, &num);
  num += plane->d;
  if (GO_SMALL(num)) {
    *point = line->point;
    /* distance probably isn't exactly zero, so compute it */
    return go_poGO_RESULT_plane_distance(&line->point, plane, distance);
  }

  /* else plug in d to get intersect point */
  *distance = -num / denom;
  return go_line_evaluate(line, *distance, point);
}

/* general vector and matrix functions */

/*
  Adapted from ludcmp routine in Numerical Recipes in C, but with
  indices starting at 0 and no heap allocation.

  Given a matrix a[0..n-1][0..n-1], this routine replaces it by the LU
  decomposition of a rowwise permutation of itself. a and n are
  input. a is changed and output.  indx[0..n-1] is an output vector
  that records the row permutation effected by the partial pivoting; d
  is output as +/-1 depending on whether the number of row
  interchanges was even or odd, respectively. This routine is used in
  combination with lubksb to solve linear equations or invert a
  matrix.

  Warning! The matrix 'a' can't be declared as go_real a[n][n]. It
  needs to be an array of go_real pointers. See e.g. go_mat6_inv() for
  how to set up the 'a' matrix.
*/

static go_real go_singular_epsilon = 1.0e-15;

go_real go_get_singular_epsilon(void)
{
  return go_singular_epsilon;
}

int go_set_singular_epsilon(go_real epsilon)
{
  if (epsilon <= 0.0) return GO_RESULT_ERROR;

  go_singular_epsilon = epsilon;

  return GO_RESULT_OK;
}

int ludcmp(go_real ** a,
		 go_real * scratchrow,
		 go_integer n,
		 go_integer * indx,
		 go_real * d)
{
  go_integer i, imax, j, k;
  go_real big, dum, sum, temp;

  *d = 1.0;
  for (i = 0; i < n; i++) {
    big = 0.0;
    for (j = 0; j < n; j++)
      if ((temp = rtapi_fabs(a[i][j])) > big)
	big = temp;
    if (big < go_singular_epsilon)
      return GO_RESULT_SINGULAR;
    scratchrow[i] = 1.0 / big;
  }
  for (j = 0; j < n; j++) {
    for (i = 0; i < j; i++) {
      sum = a[i][j];
      for (k = 0; k < i; k++)
	sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
    }
    big = 0.0;
    imax = 0;
    for (i = j; i < n; i++) {
      sum = a[i][j];
      for (k = 0; k < j; k++)
	sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
      if ((dum = scratchrow[i] * rtapi_fabs(sum)) >= big) {
	big = dum;
	imax = i;
      }
    }
    if (j != imax) {
      for (k = 0; k < n; k++) {
	dum = a[imax][k];
	a[imax][k] = a[j][k];
	a[j][k] = dum;
      }
      *d = -(*d);
      scratchrow[imax] = scratchrow[j];
    }
    indx[j] = imax;
    if (rtapi_fabs(a[j][j]) < go_singular_epsilon)
      return GO_RESULT_SINGULAR;
    if (j != n - 1) {
      dum = 1.0 / (a[j][j]);
      for (i = j + 1; i < n; i++)
	a[i][j] *= dum;
    }
  }

  return GO_RESULT_OK;
}

/*
  Solves the set of n linear equations A·X = B. Here a[0..n-1][0..n-1]
  is input, not as the matrix A but rather as its LU decomposition,
  determined by the routine ludcmp. indx[0..n-1] is input as the
  permutation vector returned by ludcmp. b[0..n-1] is input as the
  right-hand side vector B, and returns with the solution vector X. a,
  n, and indx are not modified by this routine and can be left in
  place for successive calls with different right-hand sides b. This
  routine takes into account the possibility that b will begin with
  many zero elements, so it is efficient for use in matrix inversion.

  Warning! The matrix 'a' can't be declared as go_real a[n][n]. It
  needs to be an array of go_real pointers. See e.g. go_mat6_inv() for
  how to set up the 'a' matrix.
*/

int lubksb(go_real ** a,
		 go_integer n,
		 go_integer * indx,
		 go_real * b)
{
  go_integer i, ii = -1, ip, j;
  go_real sum;

  for (i = 0; i < n; i++) {
    ip = indx[i];
    sum = b[ip];
    b[ip] = b[i];
    if (ii != -1)
      for (j = ii; j <= i - 1; j++)
	sum -= a[i][j] * b[j];
    else if (sum)
      ii = i;
    b[i] = sum;
  }
  for (i = n - 1; i >= 0; i--) {
    sum = b[i];
    for (j = i + 1; j < n; j++)
      sum -= a[i][j] * b[j];
    if (rtapi_fabs(a[i][i]) < go_singular_epsilon)
      return GO_RESULT_SINGULAR;
    b[i] = sum / a[i][i];
  }

  return GO_RESULT_OK;
}

int go_cart_vector_convert(const go_cart * c,
				 go_real * v)
{
  v[0] = c->x, v[1] = c->y, v[2] = c->z;

  return GO_RESULT_OK;
}

int go_vector_cart_convert(const go_real * v,
				 go_cart * c)
{
  c->x = v[0], c->y = v[1], c->z = v[2];

  return GO_RESULT_OK;
}

int go_quat_matrix_convert(const go_quat * quat,
				 go_matrix * matrix)
{
  go_mat mat;
  int retval;

  /* check for an initialized matrix */
  if (0 == matrix->el[0]) return GO_RESULT_ERROR;
  /* check for a 3x3 matrix */
  if (matrix->rows != 3 || matrix->cols != 3) return GO_RESULT_ERROR;

  retval = go_quat_mat_convert(quat, &mat);
  if (GO_RESULT_OK != retval) return retval;

  return go_mat_matrix_convert(&mat, matrix);
}

/*            |  m.x.x   m.y.x   m.z.x  | */
/* go_mat m = |  m.x.y   m.y.y   m.z.y  | */
/*            |  m.x.z   m.y.z   m.z.z  | */
/* go_matrix mout[row][col]               */

int go_mat_matrix_convert(const go_mat * mat,
				go_matrix * matrix)
{
  /* check for an initialized matrix */
  if (0 == matrix->el[0]) return GO_RESULT_ERROR;
  /* check for a 3x3 matrix */
  if (matrix->rows != 3 || matrix->cols != 3) return GO_RESULT_ERROR;

  matrix->el[0][0] = mat->x.x, matrix->el[0][1] = mat->y.x, matrix->el[0][2] = mat->z.x;
  matrix->el[1][0] = mat->x.y, matrix->el[1][1] = mat->y.y, matrix->el[1][2] = mat->z.y;
  matrix->el[2][0] = mat->x.z, matrix->el[2][1] = mat->y.z, matrix->el[2][2] = mat->z.z;

  return GO_RESULT_OK;
}

int go_matrix_matrix_add(const go_matrix * a,
			       const go_matrix * b,
			       go_matrix * apb)
{
  go_integer row, col;

  /* check for an initialized matrix */
  if (0 == a->el[0] || 0 == b->el[0] || 0 == apb->el[0]) return GO_RESULT_ERROR;
  /* check for matching rows and cols */
  if (a->rows != b->rows || a->cols != b->cols ||
      b->rows != apb->rows || b->cols != apb->cols) return GO_RESULT_ERROR;

  for (row = 0; row < a->rows; row++) {
    for (col = 0; col < a->cols; col++) {
      apb->el[row][col] = a->el[row][col] + b->el[row][col];
    }
  }

  return GO_RESULT_OK;
}

int go_matrix_matrix_copy(const go_matrix * src,
				go_matrix * dst)
{
  go_integer row, col;

  /* check for an initialized matrix */
  if (0 == src->el[0] || 0 == dst->el[0]) return GO_RESULT_ERROR;

  /* check for matching rows and cols */
  if (src->rows != dst->rows || src->cols != dst->cols) return GO_RESULT_ERROR;

  for (row = 0; row < src->rows; row++) {
    for (col = 0; col < src->cols; col++) {
      dst->el[row][col] = src->el[row][col];
    }
  }

  return GO_RESULT_OK;
}

int go_matrix_matrix_mult(const go_matrix * a,
				const go_matrix * b,
				go_matrix * ab)
{
  go_real ** ptrin;
  go_real ** ptrout;
  go_integer row, col, i;

  /* check for an initialized matrix */
  if (0 == a->el[0] || 0 == b->el[0] || 0 == ab->el[0]) return GO_RESULT_ERROR;
  /* check for consistent rows and cols */
  if (a->cols != b->rows ||
      a->rows != ab->rows ||
      b->cols != ab->cols) return GO_RESULT_ERROR;

  if (ab == a) {
    /* destructive multiply, use a's copy space and copy back */
    ptrin = a->elcpy;
    ptrout = a->el;
  } else if (ab == b) {
    ptrin = b->elcpy;
    ptrout = b->el;
  } else {
    ptrin = ab->el;
    ptrout = NULL;
  }

  for (row = 0; row < a->rows; row++) {
    for (col = 0; col < b->cols; col++) {
      ptrin[row][col] = 0;
      for (i = 0; i < a->cols; i++) {
	ptrin[row][col] += a->el[row][i] * b->el[i][col];
      }
    }
  }

  if (NULL != ptrout) {
    for (row = 0; row < ab->rows; row++) {
      for (col = 0; col < ab->cols; col++) {
	ptrout[row][col] = ptrin[row][col];
      }
    }
  }

  return GO_RESULT_OK;
}

int go_matrix_vector_mult(const go_matrix * a,
				const go_vector * v,
				go_vector * axv)
{
  go_vector * ptrin;
  go_vector * ptrout;
  go_integer row, i;

  /* check for an initialized matrix */
  if (0 == a->el[0]) return GO_RESULT_ERROR;

  if (axv == v) {
    ptrin = a->elcpy[0];
    ptrout = axv;
  } else {
    ptrin = axv;
    ptrout = NULL;
  }

  for (row = 0; row < a->rows; row++) {
    ptrin[row] = 0;
    for (i = 0; i < a->cols; i++) {
      ptrin[row] += a->el[row][i] * v[i];
    }
  }

  if (ptrout != NULL) {
    for (row = 0; row < a->rows; row++) {
      ptrout[row] = ptrin[row];
    }
  }

  return GO_RESULT_OK;
}

/*
  The matrix-vector cross product is a matrix of the same dimension,
  whose columns are the column-wise cross products of the matrix
  and the vector. The matrices must be 3xN, the vector 3x1.
*/
int go_matrix_vector_cross(const go_matrix * a,
				 const go_vector * v,
				 go_matrix * axv)
{
  go_real ** ptrin;
  go_real ** ptrout;
  go_cart vc;			/* 'v' */
  go_cart ac;			/* a column of the 'a' matrix */
  go_cart cross;
  go_integer row, col;

  /* check for an initialized matrix */
  if (0 == a->el[0] || 0 == axv->el[0]) return GO_RESULT_ERROR;
  /* check for consistent rows and cols */
  if (a->rows != 3 ||
      axv->rows != 3 ||
      a->cols != axv->cols) return GO_RESULT_ERROR;

  if (axv == a) {
    /* destructive multiply, use a's copy space and copy back */
    ptrin = a->elcpy;
    ptrout = a->el;
  } else {
    ptrin = axv->el;
    ptrout = NULL;
  }

  /* get 'v' as a cartesian type */
  vc.x = v[0], vc.y = v[1], vc.z = v[2];

  for (col = 0; col < a->cols; col++) {
    /* pick off the col'th column as a cartesian type */
    ac.x = a->el[0][col], ac.y = a->el[1][col], ac.z = a->el[2][col];
    /* cross it with v */
    go_cart_cart_cross(&ac, &vc, &cross);
    /* make it the col'th column of axv[] */
    ptrin[0][col] = cross.x, ptrin[1][col] = cross.y, ptrin[2][col] = cross.z;
  }

  if (ptrout != NULL) {
    for (row = 0; row < a->rows; row++) {
      for (col = 0; col < a->cols; col++) {
	ptrout[row][col] = ptrin[row][col];
      }
    }
  }

  return GO_RESULT_OK;
}

int go_matrix_transpose(const go_matrix * a,
			      go_matrix * at)
{
  go_real ** ptrin;
  go_real ** ptrout;
  go_integer row, col;

  /* check for fixed matrix */
  if (0 == a->el[0] || 0 == at->el[0]) return GO_RESULT_ERROR;

  if (at == a) {
    ptrin = a->elcpy;
    ptrout = a->el;
  } else {
    ptrin = at->el;
    ptrout = NULL;
  }

  for (row = 0; row < a->rows; row++) {
    for (col = 0; col < a->cols; col++) {
      ptrin[col][row] = a->el[row][col];
    }
  }

  if (ptrout != NULL) {
    for (row = 0; row < a->rows; row++) {
      for (col = 0; col < a->cols; col++) {
	ptrout[row][col] = ptrin[row][col];
      }
    }
  }

  return GO_RESULT_OK;
}

int go_matrix_inv(const go_matrix * m, /* M x N */
			go_matrix * minv) /* N x M */
{
  go_real d;
  go_integer N, row, col;
  int retval;

  /* check for fixed matrix */
  if (0 == m->el[0] || 0 == minv->el[0]) return GO_RESULT_ERROR;

  N = m->rows;

  /* copy of m since ludcmp destroys input matrix */
  for (row = 0; row < N; row++) {
    for (col = 0; col < N; col++) {
      m->elcpy[row][col] = m->el[row][col];
    }
  }

  /* convert the copy to its LU decomposition  */
  retval = ludcmp(m->elcpy, m->v, N, m->index, &d);
  if (GO_RESULT_OK != retval) return retval;

  /* backsubstitute a column with a 1 in it to get the inverse */
  for (col = 0; col < N; col++) {
    for (row = 0; row < N; row++) {
      m->v[row] = 0.0;
    }
    m->v[col] = 1.0;
    retval = lubksb(m->elcpy, N, m->index, m->v);
    if (GO_RESULT_OK != retval) return retval;
    for (row = 0; row < N; row++) {
      minv->el[row][col] = m->v[row];
    }
  }

  return GO_RESULT_OK;
}

extern int go_mat3_inv(const go_real a[3][3],
			     go_real ainv[3][3])
{
  go_real a11a22;
  go_real a11a23;
  go_real a11a32;
  go_real a11a33;

  go_real a12a33;
  go_real a12a23;
  go_real a12a21;
  go_real a12a31;

  go_real a13a21;
  go_real a13a22;
  go_real a13a31;
  go_real a13a32;

  go_real a21a32;
  go_real a21a33;

  go_real a22a31;
  go_real a22a33;

  go_real a23a31;
  go_real a23a32;

  go_real denom;

  a11a22 = a[0][0]*a[1][1];
  a11a23 = a[0][0]*a[1][2];
  a11a32 = a[0][0]*a[2][1];
  a11a33 = a[0][0]*a[2][2];

  a12a21 = a[0][1]*a[1][0];
  a12a23 = a[0][1]*a[1][2];
  a12a31 = a[0][1]*a[2][0];
  a12a33 = a[0][1]*a[2][2];

  a13a21 = a[0][2]*a[1][0];
  a13a22 = a[0][2]*a[1][1];
  a13a31 = a[0][2]*a[2][0];
  a13a32 = a[0][2]*a[2][1];

  a21a32 = a[1][0]*a[2][1];
  a21a33 = a[1][0]*a[2][2];

  a22a31 = a[1][1]*a[2][0];
  a22a33 = a[1][1]*a[2][2];

  a23a31 = a[1][2]*a[2][0];
  a23a32 = a[1][2]*a[2][1];

  denom =
    + a11a22*a[2][2]
    - a11a23*a[2][1]
    - a12a33*a[1][0]
    + a13a21*a[2][1]
    - a13a22*a[2][0]
    + a12a23*a[2][0];

  if (GO_SMALL(denom)) {
    return GO_RESULT_SINGULAR;
  }
  
  denom = 1.0/denom;

  ainv[0][0] = (a22a33 - a23a32)*denom;
  ainv[0][1] = (a13a32 - a12a33)*denom;
  ainv[0][2] = (a12a23 - a13a22)*denom;

  ainv[1][0] = (a23a31 - a21a33)*denom;
  ainv[1][1] = (a11a33 - a13a31)*denom;
  ainv[1][2] = (a13a21 - a11a23)*denom;

  ainv[2][0] = (a21a32 - a22a31)*denom;
  ainv[2][1] = (a12a31 - a11a32)*denom;
  ainv[2][2] = (a11a22 - a12a21)*denom;

  return GO_RESULT_OK;
}

int go_mat3_mat3_mult(const go_real a[3][3],
			    const go_real b[3][3],
			    go_real axb[3][3])
{
  go_real work[3][3];
  go_integer i, j, k;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      work[i][j] = 0;
      for (k = 0; k < 3; k++) {
	work[i][j] += a[i][k] * b[k][j];
      }
    }
  }
  
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      axb[i][j] = work[i][j];

  return GO_RESULT_OK;
}

int go_mat3_vec3_mult(const go_real a[3][3],
			    const go_real v[3],
			    go_real axv[3])
{
  go_real work[3];
  go_integer i, j;

  for (i = 0; i < 3; i++) {
    work[i] = 0;
    for (j = 0; j < 3; j++) {
      work[i] += a[i][j] * v[j];
    }
  }

  for (i = 0; i < 3; i++)
    axv[i] = work[i];

  return GO_RESULT_OK;
}

extern int go_mat4_inv(const go_real a[4][4],
			     go_real ainv[4][4])
{
  go_real work[4][4];
  go_real denom;

  go_real a11a22;
  go_real a11a23;
  go_real a11a24;
  go_real a11a33;
  go_real a11a32;
  go_real a11a34;

  go_real a12a21;
  go_real a12a23;
  go_real a12a24;
  go_real a12a31;
  go_real a12a33;
  go_real a12a34;

  go_real a13a21;
  go_real a13a22;
  go_real a13a24;
  go_real a13a31;
  go_real a13a32;
  go_real a13a34;

  go_real a14a21;
  go_real a14a22;
  go_real a14a23;
  go_real a14a31;
  go_real a14a32;
  go_real a14a33;

  go_real a21a32;
  go_real a21a33;
  go_real a21a34;

  go_real a22a31;
  go_real a22a33;
  go_real a22a34;

  go_real a23a31;
  go_real a23a32;
  go_real a23a34;

  go_real a24a31;
  go_real a24a32;
  go_real a24a33;

  go_real a11a22a33;
  go_real a11a22a34;
  go_real a11a23a32;
  go_real a11a23a34;
  go_real a11a24a32;
  go_real a11a24a33;
  go_real a12a21a33;
  go_real a12a21a34;
  go_real a12a23a31;
  go_real a12a23a34;
  go_real a12a24a31;
  go_real a12a24a33;
  go_real a13a21a32;
  go_real a13a21a34;
  go_real a13a22a31;
  go_real a13a22a34;
  go_real a13a24a31;
  go_real a13a24a32;
  go_real a14a21a32;
  go_real a14a21a33;
  go_real a14a22a31;
  go_real a14a22a33;
  go_real a14a23a31;
  go_real a14a23a32;

  go_integer row, col;

  a11a22 = a[0][0]*a[1][1];
  a11a23 = a[0][0]*a[1][2];
  a11a24 = a[0][0]*a[1][3];
  a11a33 = a[0][0]*a[2][2];
  a11a32 = a[0][0]*a[2][1];
  a11a34 = a[0][0]*a[2][3];

  a12a21 = a[0][1]*a[1][0];
  a12a23 = a[0][1]*a[1][2];
  a12a24 = a[0][1]*a[1][3];
  a12a31 = a[0][1]*a[2][0];
  a12a33 = a[0][1]*a[2][2];
  a12a34 = a[0][1]*a[2][3];

  a13a21 = a[0][2]*a[1][0];
  a13a22 = a[0][2]*a[1][1];
  a13a24 = a[0][2]*a[1][3];
  a13a31 = a[0][2]*a[2][0];
  a13a32 = a[0][2]*a[2][1];
  a13a34 = a[0][2]*a[2][3];

  a14a21 = a[0][3]*a[1][0];
  a14a22 = a[0][3]*a[1][1];
  a14a23 = a[0][3]*a[1][2];
  a14a31 = a[0][3]*a[2][0];
  a14a32 = a[0][3]*a[2][1];
  a14a33 = a[0][3]*a[2][2];

  a21a32 = a[1][0]*a[2][1];
  a21a33 = a[1][0]*a[2][2];
  a21a34 = a[1][0]*a[2][3];

  a22a31 = a[1][1]*a[2][0];
  a22a33 = a[1][1]*a[2][2];
  a22a34 = a[1][1]*a[2][3];

  a23a31 = a[1][2]*a[2][0];
  a23a32 = a[1][2]*a[2][1];
  a23a34 = a[1][2]*a[2][3];

  a24a31 = a[1][3]*a[2][0];
  a24a32 = a[1][3]*a[2][1];
  a24a33 = a[1][3]*a[2][2];

  a11a22a33 = a11a22*a[2][2];
  a11a22a34 = a11a22*a[2][3];
  a11a23a32 = a11a23*a[2][1];
  a11a23a34 = a11a23*a[2][3];
  a11a24a32 = a11a24*a[2][1];
  a11a24a33 = a11a24*a[2][2];
  a12a21a33 = a12a21*a[2][2];
  a12a21a34 = a12a21*a[2][3];
  a12a23a31 = a12a23*a[2][0];
  a12a23a34 = a12a23*a[2][3];
  a12a24a31 = a12a24*a[2][0];
  a12a24a33 = a12a24*a[2][2];
  a13a21a32 = a13a21*a[2][1];
  a13a21a34 = a13a21*a[2][3];
  a13a22a31 = a13a22*a[2][0];
  a13a22a34 = a13a22*a[2][3];
  a13a24a31 = a13a24*a[2][0];
  a13a24a32 = a13a24*a[2][1];
  a14a21a32 = a14a21*a[2][1];
  a14a21a33 = a14a21*a[2][2];
  a14a22a31 = a14a22*a[2][0];
  a14a22a33 = a14a22*a[2][2];
  a14a23a31 = a14a23*a[2][0];
  a14a23a32 = a14a23*a[2][1];

  denom =
    + a12a24a33*a[3][0]
    + a13a24a31*a[3][1]
    - a12a21a33*a[3][3]
    - a13a24a32*a[3][0]
    - a14a21a32*a[3][2]
    - a12a24a31*a[3][2]
    + a14a23a32*a[3][0]
    + a13a21a32*a[3][3]
    + a14a21a33*a[3][1]
    + a12a23a31*a[3][3]
    - a14a23a31*a[3][1]
    + a12a21a34*a[3][2]
    - a12a23a34*a[3][0]
    - a13a21a34*a[3][1]
    - a13a22a31*a[3][3]
    + a14a22a31*a[3][2]
    + a13a22a34*a[3][0]
    - a14a22a33*a[3][0]
    + a11a22a33*a[3][3]
    - a11a22a34*a[3][2]
    + a11a24a32*a[3][2]
    - a11a23a32*a[3][3] 
    + a11a23a34*a[3][1]
    - a11a24a33*a[3][1];

  if (GO_SMALL(denom)) {
    return GO_RESULT_SINGULAR;
  }

  denom = 1.0/denom;

  work[0][0] = (+ a22a33*a[3][3]
		- a22a34*a[3][2]
		+ a24a32*a[3][2]
		- a23a32*a[3][3]
		+ a23a34*a[3][1]
		- a24a33*a[3][1])*denom;
  work[0][1] = (- a12a33*a[3][3]
		+ a12a34*a[3][2]
		- a13a34*a[3][1]
		+ a13a32*a[3][3]
		- a14a32*a[3][2]
		+ a14a33*a[3][1])*denom;
  work[0][2] = (+ a12a23*a[3][3]
		- a12a24*a[3][2]
		- a13a22*a[3][3]
		+ a13a24*a[3][1]
		+ a14a22*a[3][2]
		- a14a23*a[3][1])*denom;
  work[0][3] = (- a12a23a34
		+ a12a24a33
		+ a13a22a34
		- a13a24a32
		- a14a22a33
		+ a14a23a32)*denom;

  work[1][0] = (+ a23a31*a[3][3]
		- a23a34*a[3][0]
		- a21a33*a[3][3]
		+ a21a34*a[3][2]
		+ a24a33*a[3][0]
		- a24a31*a[3][2])*denom;
  work[1][1] = (+ a11a33*a[3][3]
		- a11a34*a[3][2]
		- a13a31*a[3][3]
		+ a14a31*a[3][2]
		+ a13a34*a[3][0]
		- a14a33*a[3][0])*denom;
  work[1][2] = (- a11a23*a[3][3]
		+ a11a24*a[3][2]
		+ a13a21*a[3][3]
		+ a14a23*a[3][0]
		- a14a21*a[3][2]
		- a13a24*a[3][0])*denom;
  work[1][3] = (+ a11a23a34
		- a11a24a33
		- a13a21a34
		+ a14a21a33
		- a14a23a31
		+ a13a24a31)*denom;

  work[2][0] = (+ a22a34*a[3][0]
		- a22a31*a[3][3]
		- a24a32*a[3][0]
		+ a21a32*a[3][3]
		- a21a34*a[3][1]
		+ a24a31*a[3][1])*denom;
  work[2][1] = (+ a11a34*a[3][1]
		- a11a32*a[3][3]
		- a12a34*a[3][0]
		- a14a31*a[3][1]
		+ a12a31*a[3][3]
		+ a14a32*a[3][0])*denom;
  work[2][2] = (- a11a24*a[3][1]
		- a14a22*a[3][0]
		+ a11a22*a[3][3]
		+ a12a24*a[3][0]
		+ a14a21*a[3][1]
		- a12a21*a[3][3])*denom;
  work[2][3] = (- a11a22a34
		+ a11a24a32
		+ a14a22a31
		- a12a24a31
		+ a12a21a34
		- a14a21a32)*denom;

  work[3][0] = (+ a21a33*a[3][1]
		+ a23a32*a[3][0]
		- a22a33*a[3][0]
		- a23a31*a[3][1]
		+ a22a31*a[3][2]
		- a21a32*a[3][2])*denom;
  work[3][1] = (+ a12a33*a[3][0]
		- a13a32*a[3][0]
		- a12a31*a[3][2]
		+ a13a31*a[3][1]
		+ a11a32*a[3][2]
		- a11a33*a[3][1])*denom;
  work[3][2] = (- a12a23*a[3][0]
		+ a12a21*a[3][2]
		- a13a21*a[3][1]
		+ a13a22*a[3][0]
		- a11a22*a[3][2]
		+ a11a23*a[3][1])*denom;
  work[3][3] = (+ a13a21a32
		+ a12a23a31
		- a12a21a33
		- a13a22a31
		+ a11a22a33
		- a11a23a32)*denom;

  for (row = 0; row < 4; row++)
    for (col = 0; col < 4; col++)
      ainv[row][col] = work[row][col];

  return GO_RESULT_OK;
}

int go_mat4_mat4_mult(const go_real a[4][4],
			    const go_real b[4][4],
			    go_real axb[4][4])
{
  go_real work[4][4];
  go_integer i, j, k;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      work[i][j] = 0;
      for (k = 0; k < 4; k++) {
	work[i][j] += a[i][k] * b[k][j];
      }
    }
  }
  
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      axb[i][j] = work[i][j];

  return GO_RESULT_OK;
}

int go_mat4_vec4_mult(const go_real a[4][4],
			    const go_real v[4],
			    go_real axv[4])
{
  go_real work[4];
  go_integer i, j;

  for (i = 0; i < 4; i++) {
    work[i] = 0;
    for (j = 0; j < 4; j++) {
      work[i] += a[i][j] * v[j];
    }
  }

  for (i = 0; i < 4; i++)
    axv[i] = work[i];

  return GO_RESULT_OK;
}

int go_mat6_inv(const go_real a[6][6],
		      go_real ainv[6][6])
{
  go_real cpy[6][6];
  go_real *cpyptr[6];
  go_real scratchrow[6];
  go_real v[6];
  go_real d;
  go_integer index[6];
  go_integer row, col;
  int retval;

  /* create a copy of m[][] in cpy[][], since ludcmp destroys input matrix */
  for (row = 0; row < 6; row++) {
    for (col = 0; col < 6; col++) {
      cpy[row][col] = a[row][col];
    }
    /* set up the go_real pointer array that ludcmp likes */
    cpyptr[row] = cpy[row];
  }

  /* convert the copy to its LU decomposition  */
  retval = ludcmp(cpyptr, scratchrow, 6, index, &d);
  if (GO_RESULT_OK != retval) return retval;

  /* backsubstitute a column with a 1 in it to get the inverse */
  for (col = 0; col < 6; col++) {
    for (row = 0; row < 6; row++) {
      v[row] = 0.0;
    }
    v[col] = 1.0;
    retval = lubksb(cpyptr, 6, index, v);
    if (GO_RESULT_OK != retval) return retval;
    for (row = 0; row < 6; row++) {
      ainv[row][col] = v[row];
    }
  }

  return GO_RESULT_OK;
}

int go_mat6_mat6_mult(const go_real a[6][6],
			    const go_real b[6][6],
			    go_real axb[6][6])
{
  go_real work[6][6];
  go_integer i, j, k;

  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      work[i][j] = 0;
      for (k = 0; k < 6; k++) {
	work[i][j] += a[i][k] * b[k][j];
      }
    }
  }
  
  for (i = 0; i < 6; i++)
    for (j = 0; j < 6; j++)
      axb[i][j] = work[i][j];

  return GO_RESULT_OK;
}

int go_mat6_vec6_mult(const go_real a[6][6],
			    const go_real v[6],
			    go_real axv[6])
{
  go_real work[6];
  go_integer i, j;

  for (i = 0; i < 6; i++) {
    work[i] = 0;
    for (j = 0; j < 6; j++) {
      work[i] += a[i][j] * v[j];
    }
  }

  for (i = 0; i < 6; i++)
    axv[i] = work[i];

  return GO_RESULT_OK;
}

/* recall:                          */
/*      |  m.x.x   m.y.x   m.z.x  | */
/* M =  |  m.x.y   m.y.y   m.z.y  | */
/*      |  m.x.z   m.y.z   m.z.z  | */

int go_dh_pose_convert(const go_dh * dh, go_pose * p)
{
  go_hom h;
  go_real sth, cth;		/* sin, cos theta[i] */
  go_real sal, cal;		/* sin, cos alpha[i-1] */

  sincos(dh->theta, &sth, &cth);
  sincos(dh->alpha, &sal, &cal);

  h.rot.x.x = cth, h.rot.y.x = -sth, h.rot.z.x = 0.0;
  h.rot.x.y = sth*cal, h.rot.y.y = cth*cal, h.rot.z.y = -sal;
  h.rot.x.z = sth*sal, h.rot.y.z = cth*sal, h.rot.z.z = cal;

  h.tran.x = dh->a;
  h.tran.y = -sal*dh->d;
  h.tran.z = cal*dh->d;

  return go_hom_pose_convert(&h, p);
}

int go_pose_dh_convert(const go_pose * ph, go_dh * dh)
{
  go_hom h;

  go_pose_hom_convert(ph, &h);

  dh->a = h.tran.x;
  dh->alpha = -rtapi_atan2(h.rot.z.y, h.rot.z.z);
  dh->theta = -rtapi_atan2(h.rot.y.x, h.rot.x.x);
  if (GO_ROT_SMALL(dh->alpha)) {
    dh->d = h.tran.z / rtapi_cos(dh->alpha);
  } else {
    dh->d = -h.tran.y / rtapi_sin(dh->alpha);
  }

  return GO_RESULT_OK;
}

int go_link_joint_set(const go_link * link, go_real joint, go_link * linkout)
{
  go_pose pose;
  go_rvec rvec;
  int retval;

  linkout->type = link->type;
  linkout->quantity = link->quantity;

  if (GO_LINK_DH == link->type) {
    linkout->u.dh.a = link->u.dh.a;
    linkout->u.dh.alpha = link->u.dh.alpha;
    if (GO_QUANTITY_LENGTH == link->quantity) {
      linkout->u.dh.d = joint;
      linkout->u.dh.theta = link->u.dh.theta;
    } else {
      linkout->u.dh.d = link->u.dh.d;
      linkout->u.dh.theta = joint;
    }
    return GO_RESULT_OK;
  }

  if (GO_LINK_PP == link->type) {
    pose = go_pose_identity();
    if (GO_QUANTITY_LENGTH == link->quantity) {
      pose.tran.z = joint;
      return go_pose_pose_mult(&link->u.pp.pose, &pose, &linkout->u.pp.pose);
    }
    /* else revolute */
    rvec.x = 0, rvec.y = 0, rvec.z = joint; /* rot(Z,joint) */
    retval = go_rvec_quat_convert(&rvec, &pose.rot);
    if (GO_RESULT_OK != retval) return retval;
    return go_pose_pose_mult(&link->u.pp.pose, &pose, &linkout->u.pp.pose);
  }

  if (GO_LINK_PK == link->type) {
    /*
      Our PK type is always a length joint, so link->quantity must
      be GO_QUANTITY_LENGTH. One day we may be able to handle revolute
      parallel joints, but we can't now. Let's fail if we ever get
      a revolute joint.
      FIXME-- add revolute joints to PKMs.
    */
    if (GO_QUANTITY_LENGTH != link->quantity) {
      return GO_RESULT_IMPL_ERROR;
    }
    /* else we're a prismatic joint */
    linkout->u.pk.base = link->u.pk.base;
    linkout->u.pk.platform = link->u.pk.platform;
    linkout->u.pk.d = joint;
    return GO_RESULT_OK;
  }

  /* else not a recognized link type */
  return GO_RESULT_ERROR;
}

/* this only works for serial-link manipulators */
int go_link_pose_build(const go_link * link_params, go_integer num, go_pose * pose)
{
  go_pose p;
  go_integer link;

  *pose = go_pose_identity();

  for (link = 0; link < num; link++) {
    if (GO_LINK_DH == link_params[link].type) {
      go_dh_pose_convert(&link_params[link].u.dh, &p);
      go_pose_pose_mult(pose, &p, pose);
    } else if (GO_LINK_PP == link_params[link].type) {
      go_pose_pose_mult(pose, &link_params[link].u.pp.pose, pose);
    } else {
      return GO_RESULT_ERROR;
    }
  }

  return GO_RESULT_OK;
}
