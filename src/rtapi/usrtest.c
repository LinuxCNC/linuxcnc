/********************************************************************
* Description: Test routines to verify the ioctl calls work.
*               This file will be removed prior to merging with the
*               main branch (if it ever happens)
* Author: Paul Corner
* Created at: Sat Nov 29 17:26:12 UTC 2003
* Computer: Morphix 
* System: Linux 2.4.22-adeos on i686
*    
* Copyright (c) 2003 root  All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>		/* for blocking when needed */
#include <sys/ioctl.h>
#include "rtapi.h"

int main(void)
{

    int msg_level=0;
    int fd;
    int ret;

    fd = open("/dev/rtapi", S_IWUSR);
    if (fd < 0) {
	printf("\n No /dev/rtapi %i\n", fd);
    } else {
	printf("\n/dev/rtapi opened %i\n", fd);
    }
    ret=ioctl(fd, RTAPI_IOC_GET_MSG, &msg_level);
    printf("\nmessage level: %i\nioctl call returned: %i\n", msg_level, ret);
    msg_level++;
    ret=ioctl(fd, RTAPI_IOC_SET_MSG, &msg_level);
    printf("message level set to: %i\nioctl call returned: %i\n", msg_level, ret);

/*for (;;) {
	sched_yield();
}*/

    close(fd);
    return 0;
}
