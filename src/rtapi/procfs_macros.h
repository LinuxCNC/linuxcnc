/*
 * Copyright (C) 1999-2014 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_PROC_FS_H
#define _RTAI_PROC_FS_H

extern struct proc_dir_entry *rtai_proc_root;

#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define PROC_READ_FUN(read_fun_name)			\
    int read_fun_name(struct seq_file *pf, void *v)

#define PROC_READ_WRITE_OPEN_OPS(rtai_proc_fops,		\
				 read_fun_name, write_fun_name)	\
    								\
    static int read_fun_name##_open(struct inode *inode,	\
				    struct file *file) {	\
	return single_open(file, read_fun_name, NULL);		\
    }								\
    								\
    static const struct file_operations rtai_proc_fops = {	\
	.owner = THIS_MODULE,					\
	.open = read_fun_name##_open,				\
	.read = seq_read,					\
	.write = write_fun_name,				\
	.llseek = seq_lseek,					\
	.release = single_release				\
    };

#define PROC_READ_OPEN_OPS(rtai_proc_fops, read_fun_name)		\
    PROC_READ_WRITE_OPEN_OPS(rtai_proc_fops, read_fun_name, NULL)

static inline void *CREATE_PROC_ENTRY(const char *name, umode_t mode,
				      void *parent,
				      const struct file_operations *proc_fops)
{
    return !parent ? proc_mkdir(name, NULL) :
	proc_create(name, mode, parent, proc_fops);
}

#define SET_PROC_READ_ENTRY(entry, read_fun)  do { } while(0)

#define PROC_PRINT_VARS 

#define PROC_PRINT(fmt, args...)  \
	do { seq_printf(pf, fmt, ##args); } while(0)

#define PROC_PRINT_RETURN do { goto done; } while(0)

#define PROC_PRINT_DONE do { return 0; } while(0)

#endif  /* !_RTAI_PROC_FS_H */
