/********************************************************************
* Description: The ioctl routines in kernel space.
* Author: Paul Corner
* Created at: Mon Dec  1 12:13:14 UTC 2003
* Computer: Morphix 
* System: Linux 2.4.22-adeos on i686
*    
* Copyright (c) 2003 Morphix User  All rights reserved.
*
********************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "rtapi.h"

static char *module_name = MODULE_NAME;
static int dev_major = 0;
/* The struct containing information about our /dev node.
   All we need to do is add pointers to the functions that are implimented,
   else leave them as NULL. */
struct file_operations hello_fops = {
    /* For a detailed discussion on each field, refer to Linux Device Drivers 
       2nd edition by Alessandro Rubini - Pages 63 to 66 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
  owner:THIS_MODULE,
#endif
  read:NULL,			/* Retrieves data from the 'file' and returns 
				   a byte count or negative error code */
  write:NULL,			/* Write data from user space to the 'file' - 
				   returns a byte count negative error code */
  ioctl:command_handler,	/* The 'command' handler - See asm/ioctls.h
				   and bits/ioctls.h for reserved numbers */
    /* From the ioctl man page - Call with ioctl(fd, request, *argp) where fd 
       is the file descriptor from an earlier open() command, request is an
       integer command code, and argp is an optional (void ?) pointer in user 
       space memory, argp could also be a single int value - It would be up
       to the command handler to decide which. If this is a pointer, it is
       then used by copy_to_user() and copy_from_user() to transfer specific
       data between the user and this module. See p134-136 of LDD. (Side
       note) A sane replacement for emc's CommandHandler() ? */
  open:NULL,			/* Called when ever the 'file' is opened from 
				   user space - If not implimented, then
				   'file' is always opened without any
				   errors. */
  release:NULL,		/* Called when the 'file' is finally closed */
};

int rtapi_dev_init(void)
{

    rtapi_print_msg(RTAPI_MSG_INFO, "\n Hello World\n");

    /* Register the /dev node */
    dev_major = register_chrdev(RTAPI_MAJOR, module_name, &hello_fops);
    if (dev_major > 0) {
    return RTAPI_SUCCESS;
    }
	rtapi_print_msg(RTAPI_MSG_ERR, "Can not register /dev/$s\n",
	    module_name);
	return -1;
}


void rtapi_dev_clean(void)
{
    rtapi_print_msg(RTAPI_MSG_INFO, "\n Goodbye cruel World\n");

    /* Unregister the /dev node... */
    unregister_chrdev(dev_major, module_name);
    return;
}

int command_handler(struct inode *inode, struct file *filp, unsigned int cmd,
    unsigned long argp)
{
    int err = 0, ret = 0, msg_level;
    rtapi_print_msg(RTAPI_MSG_INFO, "HELLO Command handler\n");

    /* don't even decode wrong cmds: better returning ENOTTY than EFAULT */
    if (_IOC_TYPE(cmd) != RTAPI_IOC_MAGIC)
	return -ENOTTY;
    if (_IOC_NR(cmd) > RTAPI_IOC_MAXNR)
	return -ENOTTY;

    /* The type is a bitmask, and VERIFY_WRITE catches R/W transfers. Note
       that the type is user-oriented, while verify_area is kernel-oriented,
       so the concept of "read" and "write" is reversed */
    if (_IOC_DIR(cmd) & _IOC_READ)
	/* Check to see if argp is valid for read/write commands */
	err = !access_ok(VERIFY_WRITE, (void *) argp, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
	err = !access_ok(VERIFY_READ, (void *) argp, _IOC_SIZE(cmd));
    if (err)
	return -EFAULT;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_IOC Command switch\n");

    switch (cmd) {

    case RTAPI_IOC_GET_MSG:
    msg_level=rtapi_get_msg_level();
    ret = __put_user(msg_level, (int *) argp);
  	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_IOC_GET_MSG %i\n", msg_level);
	break;
    case RTAPI_IOC_SET_MSG:
    ret = __get_user(msg_level, (int *) argp);
	rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_IOC_SET_MSG %i\n", msg_level);
    rtapi_set_msg_level(msg_level);
	break;

//        ret = __get_user(foo, (int *) arg);
/* Possible method of operation:
 Get the data from user space.
  Get mutex on the RT/Kernel shared memory (blocking)
  Find the tail of a linked list.
  Add to the tail.
  Release the mutex.
Alternative...
 Check to see if the RT thread is active.
 Abort if active.
 Get config data from user space.
 Get mutex. (blocking ?)
 Copy data to config space.
 Release mutex.
 Signal that configs have changed.
*/

/*	break;
    case RTAPI_IOC_FOO:
	ret = copy_from_user(&new_set, (int *) argp, sizeof(new_set));
	rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI_IOC_FOO %x\n", argp);
    new_set.offset=2;
    strcpy(new_set.name,"FooBar");
	ret = copy_to_user((int *) argp, &new_set, sizeof(new_set));
    
	break;
*/
    default:			/* redundant, as cmd was checked against
				   MAXNR */
	return -ENOTTY;		/* BUT.. Just in case something went wrong ! */
    }

    return ret;
}
