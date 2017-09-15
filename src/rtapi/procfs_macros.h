//    Copyright 2003 John Kasunich
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef PROCFS_MACROS_H
#define PROCFS_MACROS_H
/***********************************************************************
*                        PROC_FS MACROS                                *
************************************************************************/
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>

/* proc print macros - Contributed by: Erwin Rol (erwin@muffin.org)
   and shamelessly ripped from rtai_proc_fs.h, part of the RTAI
   project.  See http://www.rtai.org for more details.

   macro that holds the local variables that
   we use in the PROC_PRINT_* macros. We have
   this macro so we can add variables with out
   changing the users of this macro, of course
   only when the names don't colide!
*/

#define PROC_PRINT_VARS                                 \
    off_t pos = 0;                                      \
    off_t begin = 0;                                    \
    int len = 0			/* no ";" */

/* macro that prints in the procfs read buffer.
   this macro expects the function arguments to be
   named as follows.
   static int FOO(char *page, char **start,
                off_t off, int count, int *eof, void *data)  */

#define PROC_PRINT(fmt,args...)                         \
    len += sprintf(page + len , fmt, ##args);           \
    pos += len;                                         \
    if(pos < off) {                                     \
        len = 0;                                        \
        begin = pos;                                    \
    }                                                   \
    if(pos > off + count)                               \
        goto done;

/* macro to leave the read function from another
   place than at the end.   */
#define PROC_PRINT_RETURN                              \
    *eof = 1;                                          \
    goto done			// no ";"

/* macro that should only used once at the end of the
   read function, to return from another place in the
   read function use the PROC_PRINT_RETURN macro.   */
#define PROC_PRINT_DONE                                 \
        *eof = 1;                                       \
    done:                                               \
        *start = page + (off - begin);                  \
        len -= (off - begin);                           \
        if(len > count)                                 \
            len = count;                                \
        if(len < 0)                                     \
            len = 0;                                    \
        return len		// no ";"

#endif /* CONFIG_PROC_FS */
#endif /* PROCFS_MACROS_H */
