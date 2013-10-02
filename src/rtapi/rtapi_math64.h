#ifndef RTAPI_MATH64_H
#define RTAPI_MATH64_H

#ifdef __KERNEL__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include <asm/div64.h>
static inline __u64 rtapi_div_u64_rem(__u64 dividend, __u32 divisor, __u32 *remainder)
{
    *remainder = do_div(dividend, divisor);
    return dividend;
}
static inline __u64 rtapi_div_u64(__u64 dividend, __u32 divisor)
{
    __u32 remainder;
    return rtapi_div_u64_rem(dividend, divisor, &remainder);
}
static inline __s64 rtapi_div_s64_rem(__s64 dividend, __s32 divisor, __s32 *remainder)
{
    int sgn_dividend = (dividend < 0) ^ (divisor < 0);
    int sgn_remainder = (dividend < 0);
    if(dividend < 0) dividend = -dividend;
    if(divisor < 0) divisor = -divisor;
    *remainder = sgn_remainder
        ? -do_div(dividend, divisor) : do_div(dividend, divisor);
    return sgn_dividend ? -dividend : dividend;
}
static inline __s64 rtapi_div_s64(__s64 dividend, __s32 divisor)
{
	__s32 remainder;
	return rtapi_div_s64_rem(dividend, divisor, &remainder);
}
#else
#include <linux/math64.h>
#define rtapi_div_u64_rem div_u64_rem
#define rtapi_div_u64 div_u64
#define rtapi_div_s64_rem div_s64_rem
#define rtapi_div_s64 div_s64
#endif
#else
static inline __u64 rtapi_div_u64_rem(__u64 dividend, __u32 divisor, __u32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline __u64 rtapi_div_u64(__u64 dividend, __u32 divisor) {
	return dividend / divisor;
}
static inline __s64 rtapi_div_s64_rem(__s64 dividend, __s32 divisor, __s32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline __s64 rtapi_div_s64(__s64 dividend, __s32 divisor) {
	return dividend / divisor;
}
#endif

#endif
