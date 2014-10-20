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


//
// Wait for our current serial number to be echoed back from Task,
// indicating that Task received our message.
//
// lui calls this after each time it sends a message to Task, before
// returning to the caller.
//
int lui_command_nml_wait_received(lui_t *lui) {
    struct timeval end, now;

    gettimeofday(&now, NULL);
    timeradd(&now, &lui->command_nml_receive_timeout, &end);

    do {
	lui_status_nml_update(lui);

	if (lui->status->echo_serial_number == lui->nml_serial_number) {
	    return 0;
	}

	usleep(1000);
        gettimeofday(&now, NULL);
    } while (timercmp(&now, &end, <));

    // timeout
    return -1;
}


//
// Wait for Task's status to be "Done", indicating it's finished doing
// something.  We use this as an imperfect proxy for "Task is finished
// executing the command indicated  by our current serial number".
//
int lui_command_nml_wait_done(lui_t *lui) {
    struct timeval end, now;

    gettimeofday(&now, NULL);
    timeradd(&now, &lui->command_nml_receive_timeout, &end);

    do {
        lui_status_nml_update(lui);

        if (lui->status->status == RCS_DONE) {
            return 0;
        } else if (lui->status->status == RCS_ERROR) {
            return -1;
        }

        usleep(1000);
        gettimeofday(&now, NULL);
    } while (timercmp(&now, &end, <));

    // timeout
    return -1;
}

