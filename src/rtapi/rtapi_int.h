#ifndef RTAPI_INT_H
#define RTAPI_INT_H

#ifndef __KERNEL__
# include <stdint.h>
#endif

/** Provide fixed length types of the form __u8, __s32, etc.  These
    can be used in both kernel and user space.  There are also types
    without the leading underscores, but they work in kernel space
    only.  Since we have a simulator that runs everything in user
    space, the non-underscore types should NEVER be used.
*/
#if defined(BUILD_SYS_USER_DSO)
#define __KERNEL_STRICT_NAMES
# include <linux/types.h>

# include <string.h>
typedef __u8		u8;
typedef __u16		u16;
typedef __u32		u32;
typedef __u64		u64;
typedef __s8		s8;
typedef __s16		s16;
typedef __s32		s32;
typedef __s64		s64;
#define __iomem		/* Nothing */
#else
# include <asm/types.h>

#ifdef HAVE_CK
# include <ck_limits.h>
#endif

#endif


#endif // RTAPI_INT_H
