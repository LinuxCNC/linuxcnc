#ifdef MODULE
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implemented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/string.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#ifndef __HAVE_ARCH_STRCMP	/* This flag will be defined if we do */
#define __HAVE_ARCH_STRCMP	/* have strcmp */
/* some kernels don't have strcmp */
static int strcmp(const char *cs, const char *ct)
{
    signed char __res;
    while (1) {
	if ((__res = *cs - *ct++) != 0 || !*cs++) {
	    break;
	}
    }
    return __res;
}
#endif /* __HAVE_ARCH_STRCMP */
#endif /* linux 2.4 */
#else
#include <string.h>
#endif
