/*
  DISCLAIMER:
  This software was produced by the National Institute of Standards
  and Technology (NIST), an agency of the U.S. government, and by statute is
  not subject to copyright in the United States.  Recipients of this software
  assume all responsibility associated with its operation, modification,
  maintenance, and subsequent redistribution.

  See NIST Administration Manual 4.09.07 b and Appendix I. 
*/

/*
  genserkins.c

  These are the forward and inverse kinematic functions for a general
  serial-link manipulator. Thanks to Herman Bruyninckx and John
  Hallam at http://www.roble.info/ for this.
*/

#include <math.h>
#include "gotypes.h"		/* go_result, go_integer */
#include "gomath.h"		/* go_pose */
//#include "gokin.h"		/* go_kin_type */
#include "genserkins.h"		/* these decls */
#include "kinematics.h"

#include "rtapi.h"
#include "rtapi_app.h"

#include "hal.h"
struct haldata {
    hal_float_t *a[6];
    hal_float_t *alpha[6];
    hal_float_t *d[6];
    genser_struct *kins;
} *haldata = 0;

#define A(i) (*(haldata->a[i]))
#define ALPHA(i) (*(haldata->alpha[i]))
#define D(i) (*(haldata->d[i]))

#define KINS_PTR (haldata->kins)

#if GENSER_MAX_JOINTS < 6
#error GENSER_MAX_JOINTS must be at least 6; fix genserkins.h
#endif

enum {GENSER_DEFAULT_MAX_ITERATIONS = 100};

int genser_kin_init(void)
{
//  genser_struct * genser = (genser_struct *) KINS_PTR;
  genser_struct * genser = KINS_PTR;
  int t;

  /* init them all and make them revolute joints */
  for (t = 0; t < GENSER_MAX_JOINTS; t++) {
    genser->links[t].u.dh.a = A(t);
    genser->links[t].u.dh.alpha = ALPHA(t);
    genser->links[t].u.dh.d = D(t);
    genser->links[t].u.dh.theta = 0;
    genser->links[t].type = GO_LINK_DH;
    genser->links[t].quantity = GO_QUANTITY_ANGLE;
  }

  /* set a select few to make it PUMA-like */
/* FIXME-AJ: remove, they are inited above to HAL data
  genser->links[1].u.dh.alpha = -GO_PI_2;

  genser->links[2].u.dh.a = 0.300;
  genser->links[2].u.dh.d = 0.070;

  genser->links[3].u.dh.a = 0.050;
  genser->links[3].u.dh.alpha = -GO_PI_2;
  genser->links[3].u.dh.a = 0.400;

  genser->links[4].u.dh.alpha = GO_PI_2;
  genser->links[6].u.dh.alpha = -GO_PI_2;
*/
  genser->link_num = 6;
  genser->iterations = 0;
  genser->max_iterations = GENSER_DEFAULT_MAX_ITERATIONS;

  return GO_RESULT_OK;
}
/*
int genser_kin_num_joints(void * kins)
{
  genser_struct * genser = (genser_struct *) kins;

  return genser->link_num;
}
*/

/*
int genser_kin_set_parameters(void * kins, go_link * params, int num)
{
  genser_struct * genser = (genser_struct *) kins;
  int t;

  if (num > GENSER_MAX_JOINTS) return GO_RESULT_BAD_ARGS;

  for (t = 0; t < num; t++) {
    // we only handle serial-type link params 
    if (params[t].type != GO_LINK_DH &&
	params[t].type != GO_LINK_PP) return GO_RESULT_BAD_ARGS;
    genser->links[t] = params[t];
  }
  genser->link_num = num;

  return GO_RESULT_OK;
}
*/

/*
int genser_kin_get_parameters(void * kins, go_link * params, int num)
{
  genser_struct * genser = (genser_struct *) kins;
  int t;

  // check if they have enough space to hold the params 
  if (num < genser->link_num) return GO_RESULT_BAD_ARGS;

  for (t = 0; t < genser->link_num; t++) {
    params[t] = genser->links[t];
  }

  return GO_RESULT_OK;
}
*/

static int compute_jfwd(go_link * link_params, int link_number, go_matrix * Jfwd, go_pose * T_L_0)
{
  GO_MATRIX_DECLARE(Jv, Jvstg, 3, GENSER_MAX_JOINTS);
  GO_MATRIX_DECLARE(Jw, Jwstg, 3, GENSER_MAX_JOINTS);
  GO_MATRIX_DECLARE(R_i_ip1, R_i_ip1stg, 3, 3);
  GO_MATRIX_DECLARE(scratch, scratchstg, 3, GENSER_MAX_JOINTS);
  GO_MATRIX_DECLARE(R_inv, R_invstg, 3, 3);
  go_pose pose;
  go_quat quat;
  go_vector P_ip1_i[3];
  int row, col;

  /* init matrices to possibly smaller size */
  go_matrix_init(Jv, Jvstg, 3, link_number);
  go_matrix_init(Jw, Jwstg, 3, link_number);
  go_matrix_init(R_i_ip1, R_i_ip1stg, 3, 3);
  go_matrix_init(scratch, scratchstg, 3, link_number);
  go_matrix_init(R_inv, R_invstg, 3, 3);

  Jv.el[0][0] = 0, Jv.el[1][0] = 0, Jv.el[2][0] = (GO_QUANTITY_LENGTH == link_params[0].quantity ? 1 : 0);
  Jw.el[0][0] = 0, Jw.el[1][0] = 0, Jw.el[2][0] = (GO_QUANTITY_ANGLE == link_params[0].quantity ? 1 : 0);

  /* initialize inverse rotational transform */
  if (GO_LINK_DH == link_params[0].type) {
    go_dh_pose_convert(&link_params[0].u.dh, &pose);
  } else if (GO_LINK_PP == link_params[0].type) {
    pose = link_params[0].u.pp.pose;
  } else {
    return GO_RESULT_IMPL_ERROR;
  }

  *T_L_0 = pose;

  for (col = 1; col < link_number; col++) {
    /* T_ip1_i */
    if (GO_LINK_DH == link_params[col].type) {
      go_dh_pose_convert(&link_params[col].u.dh, &pose);
    } else if (GO_LINK_PP == link_params[col].type) {
      pose = link_params[col].u.pp.pose;
    } else {
      return GO_RESULT_IMPL_ERROR;
    }

    go_cart_vector_convert(&pose.tran, P_ip1_i);
    go_quat_inv(&pose.rot, &quat);
    go_quat_matrix_convert(&quat, &R_i_ip1);

    /* Jv */
    go_matrix_vector_cross(&Jw, P_ip1_i, &scratch);
    go_matrix_matrix_add(&Jv, &scratch, &scratch);
    go_matrix_matrix_mult(&R_i_ip1, &scratch, &Jv);
    Jv.el[0][col] = 0, Jv.el[1][col] = 0, Jv.el[2][col] = (GO_QUANTITY_LENGTH == link_params[col].quantity ? 1 : 0);
    /* Jw */
    go_matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
    Jw.el[0][col] = 0, Jw.el[1][col] = 0, Jw.el[2][col] = (GO_QUANTITY_ANGLE == link_params[col].quantity ? 1 : 0);
    if (GO_LINK_DH == link_params[col].type) {
      go_dh_pose_convert(&link_params[col].u.dh, &pose);
    } else if (GO_LINK_PP == link_params[col].type) {
      pose = link_params[col].u.pp.pose;
    } else {
      return GO_RESULT_IMPL_ERROR;
    }
    go_pose_pose_mult(T_L_0, &pose, T_L_0);
  }

  /* rotate back into {0} frame */
  go_quat_matrix_convert(&T_L_0->rot, &R_inv);
  go_matrix_matrix_mult(&R_inv, &Jv, &Jv);
  go_matrix_matrix_mult(&R_inv, &Jw, &Jw);

  /* put Jv atop Jw in J */
  for (row = 0; row < 6; row++) {
    for (col = 0; col < link_number; col++) {
      if (row < 3) {
	Jfwd->el[row][col] = Jv.el[row][col];
      } else {
	Jfwd->el[row][col] = Jw.el[row - 3][col];
      }
    }
  }

  return GO_RESULT_OK;
}

static int compute_jinv(go_matrix * Jfwd, go_matrix * Jinv)
{
  int retval;

  /* compute inverse, or pseudo-inverse */
  if (Jfwd->rows == Jfwd->cols) {
    retval = go_matrix_inv(Jfwd, Jinv);
    if (GO_RESULT_OK != retval) return retval;
  } else if (Jfwd->rows < Jfwd->cols) {
    /* underdetermined, optimize on smallest sum of square of speeds */
    /* JT(JJT)inv */
    GO_MATRIX_DECLARE(JT, JTstg, GENSER_MAX_JOINTS, 6);
    GO_MATRIX_DECLARE(JJT, JJTstg, 6, 6);
    
    go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
    go_matrix_init(JJT, JJTstg, Jfwd->rows, Jfwd->rows);
    go_matrix_transpose(Jfwd, &JT);
    go_matrix_matrix_mult(Jfwd, &JT, &JJT);
    retval = go_matrix_inv(&JJT, &JJT);
    if (GO_RESULT_OK != retval) return retval;
    go_matrix_matrix_mult(&JT, &JJT, Jinv);
  } else {
    /* overdetermined, do least-squares best fit */
    /* (JTJ)invJT */
    GO_MATRIX_DECLARE(JT, JTstg, GENSER_MAX_JOINTS, 6);
    GO_MATRIX_DECLARE(JTJ, JTJstg, GENSER_MAX_JOINTS, GENSER_MAX_JOINTS);
    
    go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
    go_matrix_init(JTJ, JTJstg, Jfwd->cols, Jfwd->cols);
    go_matrix_transpose(Jfwd, &JT);
    go_matrix_matrix_mult(&JT, Jfwd, &JTJ);
    retval = go_matrix_inv(&JTJ, &JTJ);
    if (GO_RESULT_OK != retval) return retval;
    go_matrix_matrix_mult(&JTJ, &JT, Jinv);
  }

  return GO_RESULT_OK;
}

int genser_kin_jac_inv(void * kins,
			     const go_pose * pos,
			     const go_screw * vel,
			     const go_real * joints,
			     go_real * jointvels)
{
  genser_struct * genser = (genser_struct *) kins;
  GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
  GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
  go_pose T_L_0;
  go_link linkout[GENSER_MAX_JOINTS];
  go_real vw[6];
  int link;
  int retval;

  go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
  go_matrix_init(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);

  for (link = 0; link < genser->link_num; link++) {
    retval = go_link_joint_set(&genser->links[link], joints[link], &linkout[link]);
    if (GO_RESULT_OK != retval) return retval;
  }
  retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
  if (GO_RESULT_OK != retval) return retval;
  retval = compute_jinv(&Jfwd, &Jinv);
  if (GO_RESULT_OK != retval) return retval;

  vw[0] = vel->v.x;
  vw[1] = vel->v.y;
  vw[2] = vel->v.z;
  vw[3] = vel->w.x;
  vw[4] = vel->w.y;
  vw[5] = vel->w.z;

  return go_matrix_vector_mult(&Jinv, vw, jointvels);
}

int genser_kin_jac_fwd(void * kins,
			     const go_real * joints,
			     const go_real * jointvels,
			     const go_pose * pos,
			     go_screw * vel)
{
  genser_struct * genser = (genser_struct *) kins;
  GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
  go_pose T_L_0;
  go_link linkout[GENSER_MAX_JOINTS];
  go_real vw[6];
  int link;
  int retval;

  go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);

  for (link = 0; link < genser->link_num; link++) {
    retval = go_link_joint_set(&genser->links[link], joints[link], &linkout[link]);
    if (GO_RESULT_OK != retval) return retval;
  }

  retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
  if (GO_RESULT_OK != retval) return retval;

  go_matrix_vector_mult(&Jfwd, jointvels, vw);
  vel->v.x = vw[0];
  vel->v.y = vw[1];
  vel->v.z = vw[2];
  vel->w.x = vw[3];
  vel->w.y = vw[4];
  vel->w.z = vw[5];

  return GO_RESULT_OK;
}

int kinematicsForward(const double * joint,
                      EmcPose * world,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
/* int genser_kin_fwd(void * kins,
			 const go_real *joints,
			 go_pose * pos) */
{
  genser_struct * genser = KINS_PTR;
  go_link linkout[GENSER_MAX_JOINTS];
  go_pose * pos;
  go_zyx zyx;
  
  int link;
  int retval;

  for (link = 0; link < genser->link_num; link++) {
    retval = go_link_joint_set(&genser->links[link], joint[link], &linkout[link]);
    if (GO_RESULT_OK != retval) return retval;
  }

    // AJ: convert from emc2 coords (XYZABC - which are actually ZYX euler angles)
    //     to go angles (quaternions)
  pos = (go_pose *) hal_malloc(sizeof(go_pose));
  zyx.z = world->c;
  zyx.y = world->b;
  zyx.x = world->a;
  
  go_zyx_quat_convert(&zyx, &pos->rot);
  pos->tran.x = world->tran.x;
  pos->tran.y = world->tran.y;
  pos->tran.z = world->tran.z;

  retval = go_link_pose_build(linkout, genser->link_num, pos);
  if (GO_RESULT_OK != retval) return retval;

  return GO_RESULT_OK;
}

int genser_kin_inv(void  * kins,
			 const go_pose * pos,
			 go_real * joints)
{
  genser_struct * genser = (genser_struct *) kins;
  GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
  GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
  go_pose T_L_0;
  go_real dvw[6];
  go_real jest[GENSER_MAX_JOINTS];
  go_real dj[GENSER_MAX_JOINTS];
  go_pose pest, pestinv, Tdelta;
  go_rvec rvec;
  go_link linkout[GENSER_MAX_JOINTS];
  int link;
  int smalls;
  int retval;

  go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
  go_matrix_init(Jinv, Jinv_stg, genser->link_num, 6);

  /* jest[] is a copy of joints[], which is the joint estimate */
  for (link = 0; link < genser->link_num; link++) {
    jest[link] = joints[link];
  }

  for (genser->iterations = 0; genser->iterations < genser->max_iterations; genser->iterations++) {
    /* pest is the resulting pose estimate given joint estimate */
    genser_kin_fwd(kins, jest, &pest);
    go_pose_inv(&pest, &pestinv);
    go_pose_pose_mult(&pestinv, pos, &Tdelta);
    dvw[0] = Tdelta.tran.x;
    dvw[1] = Tdelta.tran.y;
    dvw[2] = Tdelta.tran.z;
    go_quat_rvec_convert(&Tdelta.rot, &rvec);
    dvw[3] = rvec.x;
    dvw[4] = rvec.y;
    dvw[5] = rvec.z;

    /* update the Jacobians */
    for (link = 0; link < genser->link_num; link++) {
      go_link_joint_set(&genser->links[link], jest[link], &linkout[link]);
    }
    retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
    if (GO_RESULT_OK != retval) return retval;
    retval = compute_jinv(&Jfwd, &Jinv);
    if (GO_RESULT_OK != retval) return retval;

    /* push the Cartesian velocity vector through the inverse Jacobian */
    go_matrix_vector_mult(&Jinv, dvw, dj);

    /* check for small joint increments, if so we're done */
    for (link = 0, smalls = 0; link < genser->link_num; link++) {
      if (GO_QUANTITY_LENGTH == linkout[link].quantity) {
	if (GO_TRAN_SMALL(dj[link])) smalls++;
      } else {
	if (GO_ROT_SMALL(dj[link])) smalls++;
      }
    }
    if (smalls == genser->link_num) {
      /* converged, copy jest[] out */
      for (link = 0; link < genser->link_num; link++) {
	joints[link] = jest[link];
      }
      return GO_RESULT_OK;
    }
    /* else keep iterating */
    for (link = 0; link < genser->link_num; link++) {
      jest[link] += dj[link];
    }
  } /* for (iterations) */

  return GO_RESULT_ERROR;
}

/*
  Extras, not callable using go_kin_ wrapper but if you know you have
  linked in these kinematics, go ahead and call these for your ad hoc
  purposes.
*/

int genser_kin_inv_iterations(genser_struct * genser)
{
  return genser->iterations;
}

int genser_kin_inv_set_max_iterations(genser_struct * genser, int i)
{
  if (i <= 0) return GO_RESULT_ERROR;
  genser->max_iterations = i;
  return GO_RESULT_OK;
}

int genser_kin_inv_get_max_iterations(genser_struct * genser)
{
  return genser->max_iterations;
}


int kinematicsHome(EmcPose * world,
                   double * joint,
                   KINEMATICS_FORWARD_FLAGS * fflags,
                   KINEMATICS_INVERSE_FLAGS * iflags)
{
  /* use joints, set world */
  return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

int comp_id;

int rtapi_app_main(void) {
    int res=0,i;
    
    comp_id = hal_init("genserkins");
    if (comp_id < 0) return comp_id;
    
    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata) goto error;

    for (i=0; i< 6; i++) {
	if((res = hal_pin_float_newf( HAL_IO, &(haldata->a[i]), comp_id, "genserkins.A-%d", i)) < 0) goto error;
	if((res = hal_pin_float_newf( HAL_IO, &(haldata->alpha[i]), comp_id, "genserkins.ALPHA-%d", i)) < 0) goto error;
	if((res = hal_pin_float_newf( HAL_IO, &(haldata->d[i]), comp_id, "genserkins.D-%d", i)) < 0) goto error;
    }

    KINS_PTR = hal_malloc(sizeof(genser_struct));
    if (KINS_PTR == NULL) goto error;
    
    A(0) = DEFAULT_A1;
    A(1) = DEFAULT_A2;
    A(2) = DEFAULT_A3;
    A(3) = DEFAULT_A4;
    A(4) = DEFAULT_A5;
    A(5) = DEFAULT_A6;
    ALPHA(0) = DEFAULT_ALPHA1;
    ALPHA(1) = DEFAULT_ALPHA2;
    ALPHA(2) = DEFAULT_ALPHA3;
    ALPHA(3) = DEFAULT_ALPHA4;
    ALPHA(4) = DEFAULT_ALPHA5;
    ALPHA(5) = DEFAULT_ALPHA6;
    D(0) = DEFAULT_D1;
    D(1) = DEFAULT_D2;
    D(2) = DEFAULT_D3;
    D(3) = DEFAULT_D4;
    D(4) = DEFAULT_D5;
    D(5) = DEFAULT_D6;
    
    hal_ready(comp_id);
    return 0;
    
error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
