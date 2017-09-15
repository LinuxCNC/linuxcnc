#ifndef HALSC_RT_H
#define HALSC_RT_H
/** This file, 'halsc_rt.h', contains declarations used by
    'halscope_rt.c' to implement the real-time portion of
    the HAL oscilloscope.  Declarations that are common to the
    realtime and user parts are in 'halsc_shm.h', and those used
    only by the user part are in 'halsc_usr.h'.
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

/* import the shared declarations */
#include "scope_shm.h"

/***********************************************************************
*                         TYPEDEFS AND DEFINES                         *
************************************************************************/

/* this is the master kernel space control structure */

typedef struct {
    scope_data_t *buffer;	/* ptr to buffer (kernel mapping) */
    int mult_cntr;		/* used to divide by 'mult' */
    int auto_timer;		/* delay timer for auto triggering */
    char data_len[16];		/* data size for each channel */
    void *data_addr[16];	/* pointers to data for each channel */
    hal_type_t data_type[16];	/* data type for each channel */
} scope_rt_control_t;

/***********************************************************************
*                              GLOBALS                                 *
************************************************************************/

extern scope_rt_control_t *ctrl_rt;	/* main RT control structure */
extern scope_shm_control_t *ctrl_shm;	/* shared mem control struct */

#endif /* HALSC_RT_H */
