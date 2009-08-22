/********************************************************************
* Description: gotypes.h
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


#ifndef GO_TYPES_H
#define GO_TYPES_H

#include <float.h>		/* DBL_MAX, FLOAT_MAX */

/*!
  GO_RESULT symbols run through a small range of values, on the
  order of tens, suitable for a byte.  GO_RESULT_OK is zero for easy
  detection of error conditions, e.g., if (result) { handle error }
*/
enum {
  GO_RESULT_OK = 0,
  GO_RESULT_IGNORED,		/* action can't be done, ignored */
  GO_RESULT_BAD_ARGS,		/* arguments bad, e.g., null pointer */
  GO_RESULT_RANGE_ERROR,	/* supplied range value out of bounds */
  GO_RESULT_DOMAIN_ERROR,	/* resulting domain out of bounds */
  GO_RESULT_ERROR,		/* action can't be done, a problem */
  GO_RESULT_IMPL_ERROR,		/* function not implemented */
  GO_RESULT_NORM_ERROR,		/* a value is expected to be normalized */
  GO_RESULT_DIV_ERROR,		/* divide by zero */
  GO_RESULT_SINGULAR,		/* a matrix is singular */
  GO_RESULT_NO_SPACE,		/* no space for append operation */
  GO_RESULT_EMPTY,		/* data structure is empty */
  GO_RESULT_BUG			/* a bug in Go, e.g., unknown case */
};

#define go_result_to_string(r)				\
(r) == GO_RESULT_OK ? "Ok" :				\
(r) == GO_RESULT_IGNORED ? "Ignored" :			\
(r) == GO_RESULT_BAD_ARGS ? "Bad Args" :		\
(r) == GO_RESULT_RANGE_ERROR ? "Range Error" :		\
(r) == GO_RESULT_DOMAIN_ERROR ? "Domain Error" :	\
(r) == GO_RESULT_ERROR ? "General Error" :		\
(r) == GO_RESULT_IMPL_ERROR ? "Implementation Error" :	\
(r) == GO_RESULT_NORM_ERROR ? "Norm Error" :		\
(r) == GO_RESULT_DIV_ERROR ? "Div Error" :		\
(r) == GO_RESULT_SINGULAR ? "Singular" :		\
(r) == GO_RESULT_NO_SPACE ? "No Space" :		\
(r) == GO_RESULT_EMPTY ? "Empty" :			\
(r) == GO_RESULT_BUG ? "Bug" : "?"

/*!
  Joints are characterized by the quantities they affect, such as
  length for linear joints and angle for rotary joints.
*/
enum {
  GO_QUANTITY_NONE = 0,
  GO_QUANTITY_LENGTH,
  GO_QUANTITY_ANGLE
};

#define go_quantity_to_string(q)		\
(q) == GO_QUANTITY_LENGTH ? "Length" :		\
(q) == GO_QUANTITY_ANGLE ? "Angle" : "None"

/* go_real: float, long double, default double; GO_INF is defined as
 the associated max value from float.h */

/*
  In IEEE floating point,

  FLT_MIN = 1.175494e-38, FLT_EPSILON 1.192093e-07
  DBL_MIN = 2.225074e-308, DBL_EPSILON 2.220446e-16
*/

#if defined(GO_REAL_FLOAT)
typedef float go_real;
#define GO_REAL go_real_float
extern int go_real_float;
#define GO_REAL_MIN FLT_MIN
#define GO_REAL_MAX FLT_MAX
#define GO_REAL_EPSILON (1.0e-4)
#define GO_INF FLT_MAX

#elif defined(GO_REAL_LONG_DOUBLE)
typedef long double go_real;
#define GO_REAL go_real_long_double
extern int go_real_long_double;
#define GO_REAL_MIN DBL_MIN
#define GO_REAL_MAX DBL_MAX
#define GO_REAL_EPSILON (1.0e-10)
#define GO_INF DBL_MAX

#else
#define GO_REAL_DOUBLE
typedef double go_real;
#define GO_REAL go_real_double
extern int go_real_double;
#define GO_REAL_MIN DBL_MIN
#define GO_REAL_MAX DBL_MAX
#define GO_REAL_EPSILON (1.0e-7)
#define GO_INF DBL_MAX
#endif

/* go_integer: short, long, long long, default int */

#if defined(GO_INTEGER_SHORT)
typedef short int go_integer;
#define GO_INTEGER go_integer_short
extern int go_integer_short;
#if defined(SHRT_MAX)
#define GO_INTEGER_MAX SHRT_MAX
#endif

#elif defined(GO_INTEGER_LONG)
typedef long int go_integer;
#define GO_INTEGER go_integer_long
extern int go_integer_long;
#if defined(LONG_MAX)
#define GO_INTEGER_MAX LONG_MAX
#endif

#elif defined(GO_INTEGER_LONG_LONG)
typedef long long int go_integer;
#define GO_INTEGER go_integer_long_long
extern int go_integer_long_long;
#if defined(LONG_MAX)
#define GO_INTEGER_MAX LONG_MAX
#endif

#else
#define GO_INTEGER_INT
typedef int go_integer;
#define GO_INTEGER go_integer_int
extern int go_integer_int;
#if defined(INT_MAX)
#define GO_INTEGER_MAX INT_MAX
#endif
#endif

/* go_flag: unsigned short, unsigned int, default unsigned char */

#if defined(GO_FLAG_USHORT)
typedef unsigned short go_flag;
#define GO_FLAG go_flag_ushort
extern int go_flag_ushort;

#elif defined(GO_FLAG_UINT)
typedef unsigned int go_flag;
#define GO_FLAG go_flag_uint
extern int go_flag_uint;

#else
#define GO_FLAG_UCHAR
typedef unsigned char go_flag;
#define GO_FLAG go_flag_uchar
extern int go_flag_uchar;
#endif

#endif /* GO_TYPES_H */
