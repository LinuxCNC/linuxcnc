//
// Copyright (C) 2014 Sebastian Kuzminsky
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
// Boston, MA  02110-1301, USA.
//

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "linuxcnc-ui-private.h"


int lui_status_nml_update(lui_t *lui) {
    NMLTYPE type;

    if (
        (0 == lui)
        || (0 == lui->status)
        || (0 == lui->status_nml)
	|| !lui->status_nml->valid()
    ) {
	return -1;
    }

    switch (type = lui->status_nml->peek()) {
    case -1:
	// error on CMS channel
	return -1;
	break;

    case 0:		// no new data (old data is still valid)
    case EMC_STAT_TYPE:	// new data
	// status buffer is valid
	break;

    default:
	return -1;
	break;
    }

    for(int i=0; i<9; i++)
    {
        const EMC_AXIS_STAT &info = lui->status->motion.axis[i];
        int v = 0;
        if(info.minHardLimit) v |= 1;
        if(info.maxHardLimit) v |= 2;
        if(info.minSoftLimit) v |= 4;
        if(info.maxSoftLimit) v |= 8;
        lui->shadow_limit[i] = v;
        lui->shadow_homed[i] = info.homed;
        lui->shadow_joint_commanded[i] = info.output;
        lui->shadow_joint_actual[i] = info.input;
    }

    for(int i=0; i<EMC_AXIS_MAX; i++) {
    }
    return 0;
}
