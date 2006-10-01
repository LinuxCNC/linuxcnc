#ifdef MODULE
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implimented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/errno.h>
#else
#include <errno.h>
#endif

