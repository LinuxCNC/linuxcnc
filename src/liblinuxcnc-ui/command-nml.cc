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


int lui_send_nml_command_and_wait(lui_t *lui, RCS_CMD_MSG &nml_command) {
    lui->command_nml->write(&nml_command);
    lui->nml_serial_number = nml_command.serial_number;
    return lui_command_wait(lui);
}


void lui_set_command_wait_mode(lui_t *lui, lui_command_wait_mode_t wait_mode) {
    lui->command_wait_mode = wait_mode;
}


int lui_command_wait(lui_t *lui) {
    switch (lui->command_wait_mode) {
        case lui_command_wait_mode_received:
            return lui_command_wait_received(lui);

        case lui_command_wait_mode_done:
            return lui_command_wait_done(lui);

        default:
            fprintf(stderr, "lui: invalid wait mode %d, setting to lui_command_wait_done\n", lui->command_wait_mode);
            lui->command_wait_mode = lui_command_wait_mode_done;
            return lui_command_wait_done(lui);
    }
}


//
// Wait for the most recent command sent to be to be acknowledged by Task,
// indicating that Task received the message.
//
int lui_command_wait_received(lui_t *lui) {
    struct timeval end, now;
    int serial_number;

    serial_number = lui->nml_serial_number;

    gettimeofday(&now, NULL);
    timeradd(&now, &lui->command_nml_receive_timeout, &end);

    do {
	lui_status_nml_update(lui);

	if (lui->status->echo_serial_number >= serial_number) {
	    return 0;
	}

	usleep(1000);
        gettimeofday(&now, NULL);
    } while (timercmp(&now, &end, <));

    // timeout
    return -1;
}


//
// Wait for Task to acknowledge receipt of the most recent command sent,
// then wait for Task's status to be "Done", indicating it's finished doing
// something.
//
// We use this as an imperfect proxy for "Task is finished executing the
// command indicated  by our current serial number".
//
int lui_command_wait_done(lui_t *lui) {
    int r;
    struct timeval end, now;

    // first wait for the command to be received
    r = lui_command_wait_received(lui);
    if (r != 0) {
        return -1;
    }

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
