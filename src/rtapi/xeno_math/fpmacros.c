/***********************************************************************
**  File:  fpmacros.c
**   
**  Contains:  C source code for implementations of floating-point
**             functions which involve float format numbers, as
**             defined in header <fp.h>.  In particular, this file
**             contains implementations of functions
**              __fpclassify(d,f), __isnormal(d,f), __isfinite(d,f),
**             __isnan(d,f), and __signbit(d,f).  This file targets
**             PowerPC platforms.
**            
**  Written by:   Robert A. Murley, Ali Sazegari
**   
**  Copyright:   c 2001 by Apple Computer, Inc., all rights reserved
**   
**  Change History (most recent first):
**
**     07 Jul 01   ram      First created from fpfloatfunc.c, fp.c,
**							classify.c and sign.c in MathLib v3 Mac OS9.
**            
***********************************************************************/

#include     "fpP.h"

#define SIGN_MASK 0x80000000
#define NSIGN_MASK 0x7fffffff
#define FEXP_MASK 0x7f800000
#define FFRAC_MASK 0x007fffff

/***********************************************************************
   long int __fpclassifyf(float x) returns the classification code of the
   argument x, as defined in <fp.h>.
   
   Exceptions:  INVALID signaled if x is a signaling NaN; in this case,
                the FP_QNAN code is returned.
   
   Calls:  none
***********************************************************************/

long int __fpclassifyf ( float x )
{
   unsigned long int iexp;
   
   union {
      unsigned long int lval;
      float fval;
   } z;
   
   z.fval = x;
   iexp = z.lval & FEXP_MASK;                 /* isolate float exponent */ 
   
   if (iexp == FEXP_MASK) {                   /* NaN or INF case */
      if ((z.lval & 0x007fffff) == 0)
         return (long int) FP_INFINITE;
      else if ((z.lval & 0x00400000) != 0)
         return (long int) FP_QNAN;
      else
         return (long int) FP_SNAN;
   }
   
   if (iexp != 0)                             /* normal float */
      return (long int) FP_NORMAL;
      
   if (x == 0.0)
      return (long int) FP_ZERO;             /* zero */
   else
      return (long int) FP_SUBNORMAL;        /* must be subnormal */
}
   

/***********************************************************************
      Function __fpclassify,                                                 
      Implementation of classify of a double number for the PowerPC.          
                                                                              
   Exceptions:  INVALID signaled if x is a signaling NaN; in this case,
                the FP_QNAN code is returned.
   
   Calls:  none
***********************************************************************/

long int __fpclassify ( double arg )
{
	register unsigned long int exponent;
      union
            {
            dHexParts hex;
            double dbl;
            } x;
      
	x.dbl = arg;
	
	exponent = x.hex.high & dExpMask;
	if ( exponent == dExpMask )
		{
		if ( ( ( x.hex.high & dHighMan ) | x.hex.low ) == 0 )
			return (long int) FP_INFINITE;
		else
            	return ( x.hex.high & 0x00080000 ) ? FP_QNAN : FP_SNAN; 
		}
	else if ( exponent != 0)
		return (long int) FP_NORMAL;
	else {
		if ( arg == 0.0 )
			return (long int) FP_ZERO;
		else
			return (long int) FP_SUBNORMAL;
		}
}


/***********************************************************************
   long int __isnormalf(float x) returns nonzero if and only if x is a
   normalized float number and zero otherwise.
   
   Exceptions:  INVALID is raised if x is a signaling NaN; in this case,
                zero is returned.
   
   Calls:  none
***********************************************************************/

long int __isnormalf ( float x )
{
   unsigned long int iexp;
   union {
      unsigned long int lval;
      float fval;
   } z;
   
   z.fval = x;
   iexp = z.lval & FEXP_MASK;                 /* isolate float exponent */
   return ((iexp != FEXP_MASK) && (iexp != 0));
}
   

long int __isnorma ( double x )
{
	return ( __fpclassify ( x ) == FP_NORMAL ); 
}


/***********************************************************************
   long int __isfinitef(float x) returns nonzero if and only if x is a
   finite (normal, subnormal, or zero) float number and zero otherwise.
   
   Exceptions:  INVALID is raised if x is a signaling NaN; in this case,
                zero is returned.
   
   Calls:  none
***********************************************************************/

long int __isfinitef ( float x )
{   
   union {
      unsigned long int lval;
      float fval;
   } z;
   
   z.fval = x;
   return ((z.lval & FEXP_MASK) != FEXP_MASK);
}
   
long int __isfinite ( double x )
{
	return ( __fpclassify ( x ) >= FP_ZERO ); 
}



/***********************************************************************
   long int __isnanf(float x) returns nonzero if and only if x is a
   NaN and zero otherwise.
   
   Exceptions:  INVALID is raised if x is a signaling NaN; in this case,
                nonzero is returned.
   
   Calls:  none
***********************************************************************/

long int __isnanf ( float x )
{   
   union {
      unsigned long int lval;
      float fval;
   } z;
   
   z.fval = x;
   return (((z.lval&FEXP_MASK) == FEXP_MASK) && ((z.lval&FFRAC_MASK) != 0));
}

long int __isnan ( double x )
{
	long int class = __fpclassify(x);
	return ( ( class == FP_SNAN ) || ( class == FP_QNAN ) ); 
}


/***********************************************************************
   long int __signbitf(float x) returns nonzero if and only if the sign
   bit of x is set and zero otherwise.
   
   Exceptions:  INVALID is raised if x is a signaling NaN.
   
   Calls:  none
***********************************************************************/

long int __signbitf ( float x )
{   
   union {
      unsigned long int lval;
      float fval;
   } z;
   
   z.fval = x;
   return ((z.lval & SIGN_MASK) != 0);
}


/***********************************************************************
      Function sign of a double.                                              
      Implementation of sign bit for the PowerPC.                             
   
   Calls:  none
***********************************************************************/

long int __signbit ( double arg )
{
      union
            {
            dHexParts hex;
            double dbl;
            } x;
      long int sign;

      x.dbl = arg;
      sign = ( ( x.hex.high & dSgnMask ) == dSgnMask ) ? 1 : 0;
      return sign;
}


