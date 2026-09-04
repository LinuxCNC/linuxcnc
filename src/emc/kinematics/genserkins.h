/********************************************************************
* Description: genserkins.h
*   Kinematics for a generalised serial kinematics machine
*
*   Derived from a work by Fred Proctor,
*   changed to work with emc2 and HAL
*
* Adapting Author: Alex Joni
* License: GPL Version 2
* System: Linux
*
*******************************************************************

  These are the forward and inverse kinematic functions for a general
  serial-link manipulator. Thanks to Herman Bruyninckx and John
  Hallam at http://www.roble.info/ for this.

  The functions are general enough to be configured for any serial
  configuration.
  The kinematics use Denavit-Hartenberg definition for the joint and
  links. The DH definitions are the ones used by John J Craig in
  "Introduction to Robotics: Mechanics and Control"
  The parameters for the manipulator are defined by hal pins.
  Currently the type of the joints is hardcoded to ANGULAR, although
  the kins support both ANGULAR and LINEAR axes.

*/

/*
  genserkins.h
*/

#ifndef GENSERKINS_H
#define GENSERKINS_H

#include <hal.h>                /* HAL data types */
#include "libposemath/gotypes.h"    /* go_result, go_integer */
#include "libposemath/gomath.h"    /* go_pose */
#include <kinematics.h>

/*!
  The maximum number of joints supported by the general serial
  kinematics. Make this at least 6; a device can have fewer than these.
*/
#define GENSER_MAX_JOINTS 6

#define GENSER_DEFAULT_MAX_ITERATIONS 100

#define PI_2 GO_PI_2

/* default DH parameters, these should be ok for a puma - at least according to Craig */
#define DEFAULT_A1 0
#define DEFAULT_ALPHA1 0
#define DEFAULT_D1 0

#define DEFAULT_A2 0
#define DEFAULT_ALPHA2 -PI_2
#define DEFAULT_D2 0

#define DEFAULT_A3 300
#define DEFAULT_ALPHA3 0
#define DEFAULT_D3 70

#define DEFAULT_A4 50
#define DEFAULT_ALPHA4 -PI_2
#define DEFAULT_D4 400

#define DEFAULT_A5 0
#define DEFAULT_ALPHA5 PI_2
#define DEFAULT_D5 0

#define DEFAULT_A6 0
#define DEFAULT_ALPHA6 -PI_2
#define DEFAULT_D6 0

typedef struct {
  go_link links[GENSER_MAX_JOINTS]; /*!< The link description of the device. */
  int link_num;                /*!< How many are actually present. */
  unsigned iterations;         /*!< How many iterations were actually used to compute the inverse kinematics. */
} genser_struct;

extern int genser_kin_size(void);

extern const char * genser_kin_get_name(void);

extern int genser_kin_num_joints(void * kins);

extern int genser_kin_fwd(void * kins,
                                const go_real *joint,
                                go_pose * world);

extern int genser_kin_inv(void * kins,
                                const go_pose * world,
                                go_real *joint);

extern int genser_kin_set_parameters(void * kins, go_link * params, int num);

extern int genser_kin_get_parameters(void * kins, go_link * params, int num);

extern int genser_kin_jac_inv(void * kins,
                                    const go_pose * pos,
                                    const go_screw * vel,
                                    const go_real * joints,
                                    go_real * jointvels);


extern int genser_kin_jac_fwd(void * kins,
                                    const go_real * joints,
                                    const go_real * jointvels,
                                    const go_pose * pos,
                                    go_screw * vel);

extern int genser_kin_fwd_interations(genser_struct * genser);


/*
  Extras, not callable using go_kin_ wrapper but if you know you have
  linked in these kinematics, go ahead and call these for your ad hoc
  purposes.
*/

/*! Returns the number of iterations used during the last call to the
  inverse kinematics functions */
extern int genser_kin_inv_iterations(genser_struct * genser);

extern int compute_jfwd(go_link * link_params,
                        int link_number,
                        go_matrix * Jfwd,
                        go_pose * T_L_0);

extern int compute_jinv(go_matrix * Jfwd,
                        go_matrix * Jinv);

/* The kinematics as functions of the parameter block (see kinematics.h):
   the DH parameters and the unrotate couplings are the table, the maths
   is the ops.  genser_links_of() fills a link description from a block,
   for a caller that wants the go_ routines directly. */
extern const kins_param_desc GENSER_PARAMS[];
extern const int GENSER_NPARAMS;
extern const kins_ops GENSER_OPS;

extern void genser_links_of(const kins_params *p, genser_struct *genser);

#endif
