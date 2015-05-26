/********************************************************************
* Description: gomath.h
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

#ifndef GO_MATH_H
#define GO_MATH_H

#include <stddef.h>		/* sizeof */
#include "rtapi_math.h"		/* M_PI */
#include <float.h>		/* FLT,DBL_MIN,MAX,EPSILON */
#include "gotypes.h"		/* go_integer,real */

/*! Returns the square of \a x. */
#define go_sq(x) ((x)*(x))
/*! Returns the cube of \a x.  */
#define go_cub(x) ((x)*(x)*(x))
/*! Returns \a x to the fourth power. */
#define go_qua(x) ((x)*(x)*(x)*(x))

/*! Returns the sine and cosine of \a x (in radians) in \a s and \a c,
  respectively. Implemented as a single call when supported, to speed
  up the calculation of the two values. */
extern void go_sincos(go_real x, go_real * s, go_real * c);

/*! Returns the cube root of \a x. */
extern go_real go_cbrt(go_real x);

#ifdef M_PI
/*! The value of Pi. */
#define GO_PI M_PI
#else
/*! The value of Pi. */
#define GO_PI 3.14159265358979323846
#endif
/*! The value of twice Pi. */
#define GO_2_PI (2.0*GO_PI)

#ifdef M_PI_2
/*! The value of half of Pi. */
#define GO_PI_2 M_PI_2
#else
/*! The value of half of Pi. */
#define GO_PI_2 1.57079632679489661923
#endif

#ifdef M_PI_4
/*! The value of one-fourth of Pi. */
#define GO_PI_4 M_PI_4
#else
/*! The value of one-fourth of Pi. */
#define GO_PI_4 0.78539816339744830962
#endif

/*! Returns \a rad in radians as its value in degrees. */
#define GO_TO_DEG(rad) ((rad)*57.295779513082323)
/*! Returns \a deg in degrees as its value in radians. */
#define GO_TO_RAD(deg) ((deg)*0.0174532925199432952)

/*! How close translational quantities must be to be equal. */
#define GO_TRAN_CLOSE(x,y) (rtapi_fabs((x)-(y)) < GO_REAL_EPSILON)
/*! How small a translational quantity must be to be zero. */
#define GO_TRAN_SMALL(x) (rtapi_fabs(x) < GO_REAL_EPSILON)

/*! How close rotational quantities must be to be equal. */
#define GO_ROT_CLOSE(x,y) (rtapi_fabs((x)-(y)) < GO_REAL_EPSILON)
/*! How small a rotational quantity must be to be zero. */
#define GO_ROT_SMALL(x) (rtapi_fabs(x) < GO_REAL_EPSILON)

/*! How close general quantities must be to be equal. Use this when
  you have something other than translational or rotational quantities,
  otherwise use one of \a GO_TRAN,ROT_CLOSE. */
#define GO_CLOSE(x,y) (rtapi_fabs((x)-(y)) < GO_REAL_EPSILON)
/*! How small a general quantity must be to be zero. Use this when
  you have something other than a translational or rotational quantity,
  otherwise use one of \a GO_TRAN,ROT_SMALL. */
#define GO_SMALL(x) (rtapi_fabs(x) < GO_REAL_EPSILON)

/*! A point or vector in Cartesian coordinates. */
typedef struct {
  go_real x;
  go_real y;
  go_real z;
} go_cart;

/*! A point or vector in spherical coordinates, with \a phi as 
  the angle down from the zenith, not up from the XY plane. */
typedef struct {
  go_real theta;
  go_real phi;
  go_real r;
} go_sph;

/*! A point or vector in cylindrical coordinates. */
typedef struct {
  go_real theta;
  go_real r;
  go_real z;
} go_cyl;

/*! A rotation vector, whose direction points along the axis of positive
  rotation, and whose magnitude is the amount of rotation around this
  axis, in radians. */
typedef struct {
  go_real x;
  go_real y;
  go_real z;
} go_rvec;

/*            |  m.x.x   m.y.x   m.z.x  | */
/* go_mat m = |  m.x.y   m.y.y   m.z.y  | */
/*            |  m.x.z   m.y.z   m.z.z  | */

/*! A rotation matrix. */
typedef struct {
  go_cart x;			/*!< X unit vector */
  go_cart y;			/*!< Y unit vector */
  go_cart z;			/*!< Z unit vector */
} go_mat;

/*! A quaternion. \a s is the cosine of the half angle of rotation,
  and the \a xyz elements comprise the vector that points in the direction
  of positive rotation and whose magnitude is the sine of the half
  angle of rotation. */
typedef struct {
  go_real s;
  go_real x;
  go_real y;
  go_real z;
} go_quat;

/*! ZYZ Euler angles. \a z is the amount of the first rotation around the
  Z axis. \a y is the amount of the second rotation around the new \a Y
  axis. \a zp is the amount of the third rotation around the new \a Z axis. */
typedef struct {
  go_real z;
  go_real y;
  go_real zp;
} go_zyz;

/*! ZYX Euler angles. \a z is the amount of the first rotation around the
  Z axis. \a y is the amount of the second rotation around the new \a Y
  axis. \a x is the amount of the third rotation around the new \a X axis. */
typedef struct {
  go_real z;
  go_real y;
  go_real x;
} go_zyx;

/*! Roll-pitch-yaw angles. \a r is the amount of the first rotation
  (roll) around the X axis. \a p is the amount of the second rotation
  (pitch) around the original Y axis.  \a y is the amount of the third
  rotation (yaw) around the original Z axis. */
typedef struct {
  go_real r;
  go_real p;
  go_real y;
} go_rpy;

/*!
  A \a go_pose represents the Cartesian position vector and quaternion
  orientation of a frame.
*/
typedef struct {
  go_cart tran;
  go_quat rot;
} go_pose;

/*!
  A \a go_screw represents the linear- and angular velocity vectors
  of a frame. \a v is the Cartesian linear velocity vector.
  \a w is the Cartesian angular velocity vector, the instantaneous
  vector about which the frame is rotating.
*/
typedef struct {
  go_cart v;
  go_cart w;
} go_screw;

/*! Convenience function that returns a \a go_pose given individual
  elements. */
extern go_pose
go_pose_this(go_real x, go_real y, go_real z,
	     go_real rs, go_real rx, go_real ry, go_real rz);

/*! Returns the zero vector. */
extern go_cart
go_cart_zero(void);

/*! Returns the identity (zero) quaternion, i.e., no rotation. */
extern go_quat
go_quat_identity(void);

/*! Returns the identity pose, no translation or rotation. */
extern go_pose
go_pose_identity(void);

typedef struct {
  go_cart tran;
  go_mat rot;
} go_hom;

/* lines, planes and related functions */

/*!
  Lines are represented in point-direction form 
  (point p, direction v) as
  (x - px)/vx = (y - py)/vy = (z - pz)vz
*/
typedef struct {
  go_cart point;
  go_cart direction;		/* always a unit vector */
} go_line;

/*!
  Given a plane as Ax + By + Cz + D = 0, the normal vector
  \a normal is the Cartesian vector (A,B,C), and the number
  \a d is the value D.

  Planes have a handedness, given by the direction of the normal
  vector, so two planes that appear coincident may be different by the
  direction of their anti-parallel normal vectors.
*/
typedef struct {
  go_cart normal;
  go_real d;
} go_plane;

/*! Fills in \a line given \a point and \a direction. Returns GO_RESULT_OK
  if \a direction is non-zero, otherwise GO_RESULT_ERROR. */
extern int go_line_from_point_direction(const go_cart * point, const go_cart * direction, go_line * line);

/*! Fills in \a line given two points. Returns GO_RESULT_OK if the points
  are different, otherwise GO_RESULT_ERROR. */
extern int go_line_from_points(const go_cart * point1, const go_cart * point2, go_line * line);

/*! Fill in \a line with the intersection of the two planes. Returns GO_RESULT_OK if the planes are not parallel, otherwise GO_RESULT_ERROR. */
extern int go_line_from_planes(const go_plane * plane1, const go_plane * plane2, go_line * line);

/*! Returns non-zero if the lines are the same, otherwise zero. */
extern go_flag go_line_line_compare(const go_line * line1, const go_line * line2);

/*! Fills in \a point with the point located distance \a d along \a line */
extern int go_line_evaluate(const go_line * line, go_real d, go_cart * point);

/*! Fills in \a distance with the distance from \a point to \a line */
extern int go_point_line_distance(const go_cart * point, const go_line * line, go_real * distance);

/*! Fills in \a pout with the nearest point on \a line to \a point */
extern int go_point_line_proj(const go_cart * point, const go_line * line, go_cart * pout);

/*! Fills in \a proj with the projection of \a point onto \a plane */
extern int go_point_plane_proj(const go_cart * point, const go_plane * plane, go_cart * proj);

/*! Fills in \a proj with the projection of \a line onto \a plane */
extern int go_line_plane_proj(const go_line * line, const go_plane * plane, go_line * proj);

/*! Fills in \a plane give a \a point on the plane and the normal \a direction. */
extern int go_plane_from_point_normal(const go_cart * point, const go_cart * normal, go_plane * plane);

/*! Fills in \a plane given the A, B, C and D values in the canonical
  form Ax + By + Cz + D = 0. Returns GO_RESULT_OK
  if not all of A, B and C are zero, otherwise GO_RESULT_ERROR. */
extern int go_plane_from_abcd(go_real A, go_real B, go_real C, go_real D, go_plane * plane);

/*! Fills in \a plane given three points. Returns GO_RESULT_OK
  if the points are distinct, otherwise GO_RESULT_ERROR. */
extern int go_plane_from_points(const go_cart * point1, const go_cart * point2, const go_cart * point3, go_plane * plane);

/*! Fills in \a plane given a \a point on the plane and a \a line
  in the plane. Returns GO_RESULT_OK if the point is not on the line,
  GO_RESULT_ERROR otherwise. */
extern int go_plane_from_point_line(const go_cart * point, const go_line * line, go_plane * plane);

/*! Returns non-zero if the planes are coincident and have the same
  normal direction, otherwise zero. */
extern go_flag go_plane_plane_compare(const go_plane * plane1, const go_plane * plane2);

/*! Fills in the \a distance from the \a point to the \a plane.  */
extern int go_point_plane_distance(const go_cart * point, const go_plane * plane, go_real * distance);

/*! Fills in \a point with the point located distances \a u and \a v along
  some othogonal planar coordinate system in \a plane */
extern int go_plane_evaluate(const go_plane * plane, go_real u, go_real v, go_cart * point);

/*! Fills in \a point with the intersection point of
 \a line with \a plane, and \a distance with the distance along the
 line to the intersection point. Returns GO_RESULT_ERROR if the line
 is parallel to the plane and not lying in the plane, otherwise
 GO_RESULT_OK. */
extern int go_line_plane_intersect(const go_line * line, const go_plane * plane, go_cart * point, go_real * distance);

/*
  struct arguments are passed to functions as const pointers since the
  speed is at least as fast for all but structs of one or two elements.
*/

/* translation rep conversion functions */

extern int go_cart_sph_convert(const go_cart *, go_sph *);
extern int go_cart_cyl_convert(const go_cart *, go_cyl *);
extern int go_sph_cart_convert(const go_sph *, go_cart *);
extern int go_sph_cyl_convert(const go_sph *, go_cyl *);
extern int go_cyl_cart_convert(const go_cyl *, go_cart *);
extern int go_cyl_sph_convert(const go_cyl *, go_sph *);

/* rotation rep conversion functions */

extern int go_rvec_quat_convert(const go_rvec *, go_quat *);
extern int go_rvec_mat_convert(const go_rvec *, go_mat *);
extern int go_rvec_zyz_convert(const go_rvec *, go_zyz *);
extern int go_rvec_zyx_convert(const go_rvec *, go_zyx *);
extern int go_rvec_rpy_convert(const go_rvec *, go_rpy *);

extern int go_quat_rvec_convert(const go_quat *, go_rvec *);
extern int go_quat_mat_convert(const go_quat *, go_mat *);
extern int go_quat_zyz_convert(const go_quat *, go_zyz *);
extern int go_quat_zyx_convert(const go_quat *, go_zyx *);
extern int go_quat_rpy_convert(const go_quat *, go_rpy *);

extern int go_mat_rvec_convert(const go_mat *, go_rvec *);
extern int go_mat_quat_convert(const go_mat *, go_quat *);
extern int go_mat_zyz_convert(const go_mat *, go_zyz *);
extern int go_mat_zyx_convert(const go_mat *, go_zyx *);
extern int go_mat_rpy_convert(const go_mat *, go_rpy *);

extern int go_zyz_rvec_convert(const go_zyz *, go_rvec *);
extern int go_zyz_quat_convert(const go_zyz *, go_quat *);
extern int go_zyz_mat_convert(const go_zyz *, go_mat *);
extern int go_zyz_zyx_convert(const go_zyz *, go_zyx *);
extern int go_zyz_rpy_convert(const go_zyz *, go_rpy *);

extern int go_zyx_rvec_convert(const go_zyx *, go_rvec *);
extern int go_zyx_quat_convert(const go_zyx *, go_quat *);
extern int go_zyx_mat_convert(const go_zyx *, go_mat *);
extern int go_zyx_zyz_convert(const go_zyx *, go_zyz *);
extern int go_zyx_rpy_convert(const go_zyx *, go_rpy *);

extern int go_rpy_rvec_convert(const go_rpy *, go_rvec *);
extern int go_rpy_quat_convert(const go_rpy *, go_quat *);
extern int go_rpy_mat_convert(const go_rpy *, go_mat *);
extern int go_rpy_zyz_convert(const go_rpy *, go_zyz *);
extern int go_rpy_zyx_convert(const go_rpy *, go_zyx *);

/* combined rep conversion functions */

extern int go_pose_hom_convert(const go_pose *, go_hom *);
extern int go_hom_pose_convert(const go_hom *, go_pose *);

/* misc conversion functions */
/*!
  go_cart_rvec_convert and go_rvec_cart_convert convert between
  Cartesian vectors and rotation vectors. The conversion is trivial
  but keeps types distinct.
*/
extern int go_cart_rvec_convert(const go_cart * cart, go_rvec * rvec);
extern int go_rvec_cart_convert(const go_rvec * rvec, go_cart * cart);

/* translation functions, that work only with the preferred
   go_cart type. Other types must be converted to go_cart
   to use these, e.g., there's no go_sph_cyl_compare() */

extern go_flag go_cart_cart_compare(const go_cart *, const go_cart *);
extern int go_cart_cart_dot(const go_cart *, const go_cart *,
				  go_real *);
extern int go_cart_cart_cross(const go_cart *, const go_cart *,
				    go_cart *);
extern int go_cart_mag(const go_cart *, go_real *);
extern int go_cart_magsq(const go_cart *, go_real *);
extern go_flag go_cart_cart_par(const go_cart *, const go_cart *);
extern go_flag go_cart_cart_perp(const go_cart *, const go_cart *);

/*! Places the Cartesian displacement between two vectors \a v1 and
 \a v2 in \a disp, returning \a GO_RESULT_OK. */
extern int go_cart_cart_disp(const go_cart * v1,
				   const go_cart * v2,
				   go_real * disp);

extern int go_cart_cart_add(const go_cart *, const go_cart *,
				  go_cart *);
extern int go_cart_cart_sub(const go_cart *, const go_cart *,
				  go_cart *);
extern int go_cart_scale_mult(const go_cart *, go_real, go_cart *);
extern int go_cart_neg(const go_cart *, go_cart *);
extern int go_cart_unit(const go_cart *, go_cart *);

/*!
  Given two non-zero vectors \a v1 and \a v2, fill in \a quat with
  the minimum rotation that brings \a v1 to \a v2.
 */
extern int go_cart_cart_rot(const go_cart * v1,
				  const go_cart * v2,
				  go_quat * quat);

/*!
  Project vector \a v1 onto \a v2, with the resulting vector placed
  into \a vout. Returns GO_RESULT_OK if it can be done, otherwise
  something else.
*/
extern int go_cart_cart_proj(const go_cart * v1, const go_cart * v2,
				   go_cart * vout);
extern int go_cart_plane_proj(const go_cart *, const go_cart *,
				    go_cart *);
extern int go_cart_cart_angle(const go_cart *, const go_cart *,
				    go_real *);
/*!
  go_cart_normal finds one of the infinite vectors perpendicular
  to \a v, putting the result in \a vout.
 */
extern int go_cart_normal(const go_cart * v, go_cart * vout);
extern int go_cart_centroid(const go_cart * varray,
				  go_integer num,
				  go_cart * centroid);
extern int go_cart_centroidize(const go_cart * vinarray,
				     go_integer num,
				     go_cart * centroid,
				     go_cart * voutarray);
extern int go_cart_cart_pose(const go_cart *, const go_cart *,
				   go_cart *, go_cart *,
				   go_integer, go_pose *);

/*!
  Returns the Cartesian point \a p whose distances from three other points
  \a c1, \a c2 and \a c3 are \a l1, \a l2 and \a l3, respectively. In
  general there are 0, 1 or two points possible. If no point is possible,
  this returns GO_RESULT_ERROR, otherwise the points are returned in \a
  p1 and \a p2, which may be the same point.
*/

int go_cart_trilaterate(const go_cart * c1, 
			      const go_cart * c2,
			      const go_cart * c3,
			      go_real l1,
			      go_real l2,
			      go_real l3,
			      go_cart * p1,
			      go_cart * p2);

/* quat functions */

extern go_flag go_quat_quat_compare(const go_quat *, const go_quat *);
extern int go_quat_mag(const go_quat *, go_real *);

/*!
  go_quat_unit takes a quaternion rotation \a q and converts it into
  a unit rotation about the same axis, \a qout.
*/
extern int go_quat_unit(const go_quat * q, go_quat * qout);
extern int go_quat_norm(const go_quat *, go_quat *);
extern int go_quat_inv(const go_quat *, go_quat *);
extern go_flag go_quat_is_norm(const go_quat *);
extern int go_quat_scale_mult(const go_quat *, go_real, go_quat *);
extern int go_quat_quat_mult(const go_quat *, const go_quat *,
				   go_quat *);
extern int go_quat_cart_mult(const go_quat *, const go_cart *,
				   go_cart *);

/* rotation vector functions */

extern go_flag go_rvec_rvec_compare(const go_rvec * r1, const go_rvec * r2);
extern int go_rvec_scale_mult(const go_rvec *, go_real, go_rvec *);

/* rotation matrix functions */

/*        |  m.x.x   m.y.x   m.z.x  |   */
/*   M =  |  m.x.y   m.y.y   m.z.y  |   */
/*        |  m.x.z   m.y.z   m.z.z  |   */

/*!
  Normalizes rotation matrix \a m so that all columns are mutually
  perpendicular unit vectors, placing the result in \a mout.
*/
extern int go_mat_norm(const go_mat * m, go_mat * mout);

extern go_flag go_mat_is_norm(const go_mat *);
extern int go_mat_inv(const go_mat *, go_mat *);
extern int go_mat_cart_mult(const go_mat *, const go_cart *, go_cart *);
extern int go_mat_mat_mult(const go_mat *, const go_mat *, go_mat *);

/* pose functions*/

extern go_flag go_pose_pose_compare(const go_pose *, const go_pose *);
extern int go_pose_inv(const go_pose *, go_pose *);
extern int go_pose_cart_mult(const go_pose *, const go_cart *, go_cart *);
extern int go_pose_pose_mult(const go_pose *, const go_pose *, go_pose *);
extern int go_pose_scale_mult(const go_pose *, go_real, go_pose *);

/*! Given two times \a t1 and \a t2, and associated poses \a p1 and \a
  p2, interpolates (or extrapolates) to find pose \a p3 at time \a
  t3. The result is stored in \a p3. Returns GO_RESULT_OK if \a t1 and
  \a t2 are distinct and \a p1 and \a p2 are valid poses, otherwise it
  can't interpolate and returns a relevant error. */
extern int
go_pose_pose_interp(go_real t1,
		    const go_pose * p1,
		    go_real t2, 
		    const go_pose * p2,
		    go_real t3, 
		    go_pose * p3);

/* homogeneous transform functions */

extern int go_hom_inv(const go_hom *, go_hom *);

/* screw functions */

/*! Given \a pose transformation from frame A to B, and a screw \a screw
  in frame A, transform the screw into frame B and place in \a out. */
extern int go_pose_screw_mult(const go_pose * pose, const go_screw * screw, go_screw * out);

/* declarations for general MxN matrices */

/*!
  Declare a matrix variable \a m with \a rows rows and \a cols columns.
  Allocates \a rows X \a columns of space in \a mspace.
*/

typedef go_real go_vector;

typedef struct {
  go_integer rows;
  go_integer cols;
  go_real ** el;
  go_real ** elcpy;
  go_real * v;
  go_integer * index;
} go_matrix;

#define GO_MATRIX_DECLARE(M,Mspace,_rows,_cols) \
go_matrix M = {0, 0, 0, 0, 0, 0}; \
struct { \
  go_real * el[_rows]; \
  go_real * elcpy[_rows]; \
  go_real stg[_rows][_cols]; \
  go_real stgcpy[_rows][_cols]; \
  go_real v[_rows]; \
  go_integer index[_rows]; \
} Mspace

#define go_matrix_init(M,Mspace,_rows,_cols) \
M.el = Mspace.el;				    \
M.elcpy = Mspace.elcpy;				    \
for (M.rows = 0; M.rows < (_rows); M.rows++) { \
  M.el[M.rows] = Mspace.stg[M.rows];	    \
  M.elcpy[M.rows] = Mspace.stgcpy[M.rows];	    \
} \
M.rows = (_rows); \
M.cols = (_cols); \
M.v = Mspace.v; \
M.index = Mspace.index

extern go_real
go_get_singular_epsilon(void);

extern int
go_set_singular_epsilon(go_real epsilon);

extern int
ludcmp(go_real ** a,
       go_real * scratchrow,
       go_integer n,
       go_integer * indx,
       go_real * d);

extern int
lubksb(go_real ** a,
       go_integer n,
       go_integer * indx,
       go_real * b);

/* MxN matrix, Mx1 vector functions */

extern int
go_cart_vector_convert(const go_cart * c,
		       go_vector * v);
extern int
go_vector_cart_convert(const go_real * v,
		       go_cart * c);

extern int
go_quat_matrix_convert(const go_quat * quat,
		      go_matrix * matrix);

extern int
go_mat_matrix_convert(const go_mat * mat,
		      go_matrix * matrix);

extern int
go_matrix_matrix_add(const go_matrix * a,
		     const go_matrix * b,
		     go_matrix * apb);

extern int
go_matrix_matrix_copy(const go_matrix * src,
		      go_matrix * dst);

extern int
go_matrix_matrix_mult(const go_matrix * a,
		      const go_matrix * b,
		      go_matrix * ab);

extern int
go_matrix_vector_mult(const go_matrix * a,
		      const go_vector * v,
		      go_vector * av);

/*!
  The matrix-vector cross product is a matrix of the same dimension,
  whose columns are the column-wise cross products of the matrix
  and the vector. The matrices must be 3xN, the vector 3x1.
*/
extern int
go_matrix_vector_cross(const go_matrix * a,
		       const go_vector * v,
		       go_matrix * axv);

extern int
go_matrix_transpose(const go_matrix * a,
		    go_matrix * at);

extern int
go_matrix_inv(const go_matrix * a,
	      go_matrix * ainv);

/* Square matrix functions, where matN is an NxN matrix, and vecN
   is an Nx1 vector */

/* Optimized 3x3 functions */
extern int go_mat3_inv(const go_real a[3][3],
			     go_real ainv[3][3]);
extern int go_mat3_mat3_mult(const go_real a[3][3],
				   const go_real b[3][3],
				   go_real axb[3][3]);
extern int go_mat3_vec3_mult(const go_real a[3][3],
				   const go_real v[3],
				   go_real axv[3]);

/* Optimized 4x4 functions */
extern int go_mat4_inv(const go_real a[4][4],
			     go_real ainv[4][4]);
extern int go_mat4_mat4_mult(const go_real a[4][4],
				   const go_real b[4][4],
				   go_real axb[4][4]);
extern int go_mat4_vec4_mult(const go_real a[4][4],
				   const go_real v[4],
				   go_real axv[4]);

/*!
  Given a 6x6 matrix \a a, computes the inverse and returns it in
  \a ainv. Leaves \a a untouched. Returns GO_RESULT_OK if there is an
  inverse, else GO_RESULT_SINGULAR if the matrix is singular.
*/
extern int go_mat6_inv(const go_real a[6][6],
			     go_real ainv[6][6]);

/*!
  Given two 6x6 matrices \a a and \a b, multiplies them and returns
  the result in \a axb. Leaves \a a and \a b untouched.
  Returns GO_RESULT_OK.
*/
extern int go_mat6_mat6_mult(const go_real a[6][6],
				   const go_real b[6][6],
				   go_real axb[6][6]);

/*!
  Given a 6x6 matrix \a a and a 6x1 vector \a v, multiplies them
  and returns the result in \a axv. Leaves \a a and \a v untouched.
  Returns GO_RESULT_OK.
*/
extern int go_mat6_vec6_mult(const go_real a[6][6],
				   const go_real v[6],
				   go_real axv[6]);

/* Denavit-Hartenberg to pose conversions */

/*
  The link frams is assumed to be

  | i-1
  |    T
  |   i

  that is, elements of the link frame expressed wrt the
  previous link frame.
*/

/*!
  These DH parameters follow the convention in John J. Craig,
  _Introduction to Robotics: Mechanics and Control_.
*/
typedef struct {
  go_real a;			/*< a[i-1] */
  go_real alpha;		/*< alpha[i-1] */
  /* either d or theta are the free variable, depending on quantity */
  go_real d;			/*< d[i] */
  go_real theta;		/*< theta[i] */
} go_dh;

/*!
  PK parameters are used for parallel kinematic mechanisms, and
  represent the Cartesian positions of the ends of the link in the
  stationary base frame and the moving platform frame. Currently this
  only supports prismatic links.
 */
typedef struct {
  go_cart base;			/*< position of fixed end in base frame */
  go_cart platform;		/*< position of moving end in platform frame  */
  go_real d;			/*< the length of the link */
} go_pk;

/*!
  PP parameters represent the pose of the link with respect to the
  previous link. Revolute joints rotate about the Z axis, prismatic
  joints slide along the Z axis.
 */
typedef struct {
  go_pose pose;		/*< the pose of the link wrt to the previous link */
} go_pp;

/*! Types of link parameter representations  */
enum {
  GO_LINK_DH = 1,		/*< for Denavit-Hartenberg params  */
  GO_LINK_PK,			/*< for parallel kinematics  */
  GO_LINK_PP			/*< for serial kinematics */
};

#define go_link_to_string(L)		\
(L) == GO_LINK_DH ? "DH" :		\
(L) == GO_LINK_PK ? "PK" :		\
(L) == GO_LINK_PP ? "PP" : "None"

/*!
  This is the generic link structure for PKM sliding/cable links and 
  serial revolute/prismatic links.
 */
typedef struct {
  union {
    go_dh dh; /*< if you have DH params and don't want to convert to PP */
    go_pk pk; /*< if you have a parallel machine, e.g., hexapod or robot crane */
    go_pp pp; /*< if you have a serial machine, e.g., an industrial robot  */
  } u;
  go_flag type;			/*< one of GO_LINK_DH,PK,PP  */
  go_flag quantity;		/*< one of GO_QUANTITY_LENGTH,ANGLE */
} go_link;

/*!
  Converts DH parameters in \a dh to their pose equivalent, stored
  in \a pose.
 */
extern int go_dh_pose_convert(const go_dh * dh, go_pose * pose);

/*!
  Converts \a pose to the equivalent DH parameters, stored in \a dh.
  Warning! Conversion from these DH parameters back to a pose via \a
  go_dh_pose_convert will NOT in general result in the same
  pose. Poses have 6 degrees of freedom, DH parameters have 4, and
  conversion to DH parameters loses some information. The source of
  this information loss is the convention imposed on DH parameters for
  choice of X-Y-Z axis directions. With poses, there is no such
  convention, and poses are thus freer than DH parameters.
 */
extern int go_pose_dh_convert(const go_pose * pose, go_dh * dh);

/*!
  Fixes the link in \a link to its value when the joint
  variable is \a joint, storing the result in \a linkout.
*/
extern int go_link_joint_set(const go_link * link, go_real joint, go_link * linkout);

/*!
  Takes the link description of the device in \a links, and the number
  of these in \a num, and builds the pose of the device and stores in
  \a pose. \a links should have the value of the free link parameter
  filled in with the current joint value, e.g., with a prior call to
  go_link_joint_set.
*/
extern int go_link_pose_build(const go_link * links, go_integer num, go_pose * pose);

typedef struct {
  go_real re;
  go_real im;
} go_complex;

extern go_complex go_complex_add(go_complex z1, go_complex z2);
extern go_complex go_complex_sub(go_complex z1, go_complex z2);
extern go_complex go_complex_mult(go_complex z1, go_complex z2);
extern go_complex go_complex_div(go_complex z1, go_complex z2, int * result);
extern go_complex go_complex_scale(go_complex z, go_real scale);
extern go_real go_complex_mag(go_complex z);
extern go_real go_complex_arg(go_complex z);
extern void go_complex_sqrt(go_complex z, go_complex * z1, go_complex * z2);
extern void go_complex_cbrt(go_complex z, go_complex * z1, go_complex * z2, go_complex * z3);

typedef struct {
  /* leading coefficient is 1, x^2 + ax + b = 0 */
  go_real a;
  go_real b;
} go_quadratic;

typedef struct {
  /* leading coefficient is 1, x^3 + ax^2 + bx + c = 0 */
  go_real a;
  go_real b;
  go_real c;
} go_cubic;

typedef struct {
  /* leading coefficient is 1, x^4 + ax^3 + bx^2 + cx + d = 0 */
  go_real a;
  go_real b;
  go_real c;
  go_real d;
} go_quartic;

extern int go_quadratic_solve(const go_quadratic * quad,
				    go_complex * z1,
				    go_complex * z2);

extern int go_cubic_solve(const go_cubic * cub,
				go_complex * z1,
				go_complex * z2,
				go_complex * z3);

extern int go_quartic_solve(const go_quartic * quart,
				  go_complex * z1,
				  go_complex * z2,
				  go_complex * z3,
				  go_complex * z4);

extern int go_tridiag_reduce(go_real ** a,
				   go_integer n,
				   go_real * d,
				   go_real * e);

extern int go_tridiag_ql(go_real * d,
			       go_real * e,
			       go_integer n,
			       go_real ** z);

#endif /* GO_MATH_H */

