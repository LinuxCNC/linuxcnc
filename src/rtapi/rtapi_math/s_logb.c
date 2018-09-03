#if !defined(__ppc__)
/* @(#)s_logb.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: s_logb.c,v 1.8 1995/05/10 20:47:50 jtc Exp $";
#endif

/*
 * double logb(x)
 * IEEE 754 logb. Included to pass IEEE test suite. Not recommend.
 * Use ilogb instead.
 */

#include "rtapi_math.h"
#include "mathP.h"

#ifdef __STDC__
	double rtapi_logb(double x)
#else
	double rtapi_logb(x)
	double x;
#endif
{
	int32_t lx,ix;
	EXTRACT_WORDS(ix,lx,x);
	ix &= 0x7fffffff;			/* high |x| */
	if((ix|lx)==0) return -1.0/fabs(x);
	if(ix>=0x7ff00000) return x*x;
	if((ix>>=20)==0) 			/* IEEE 754 logb */
		return -1022.0; 
	else
		return (double) (ix-1023); 
}
#endif /* !__ppc__ */
