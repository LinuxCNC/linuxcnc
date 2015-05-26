/*
 * cabs() wrapper for hypot().
 * 
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include "rtapi_math.h"

struct complex {
	double x;
	double y;
};

double
rtapi_cabs(struct complex z)
{
	return hypot(z.x, z.y);
}
