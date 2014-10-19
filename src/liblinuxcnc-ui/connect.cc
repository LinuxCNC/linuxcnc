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

#include <stdio.h>
#include <unistd.h>

#include "linuxcnc-ui-private.h"


static int lui_connect_to_command_nml(lui_t *lui) {
    lui->command_nml = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", emc_nmlfile);
    if (!lui->command_nml->valid()) {
        delete lui->command_nml;
        lui->command_nml = NULL;
        return -1;
    }
    return 0;
}


static int lui_connect_to_status_nml(lui_t *lui) {
    int stat_type;
    int pass = 0;

    lui->status_nml = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", emc_nmlfile);
    if (!lui->status_nml->valid()) {
        fprintf(stderr, "lui->status_nml->valid() is false\n");
    }

    if (!lui->status_nml->valid()) {
        goto cleanup;
    }

    // wait a while for Task to send us a Status message
    do {
        stat_type = lui->status_nml->peek();

        if (stat_type == -1) {
            printf("lui->status_nml->peek() returned error (-1)\n");
            goto cleanup;
        } else if (stat_type == EMC_STAT_TYPE) {
            break;
        } else if (stat_type == 0) {
            // no new Status message, let's try waiting a little longer
        } else {
            printf("lui->status_nml->peek() returned unknown value %d\n", stat_type);
            goto cleanup;
        }

        // if we get here, stat_type must be 0 and we need to wait for a status update
        pass ++;
        usleep(10 * 1000);
    } while (pass < 1000);

    if (stat_type != EMC_STAT_TYPE) {
        printf("never got a Status message (stat_type=%d pass=%d)\n",
            stat_type, pass);
        goto cleanup;
    }

    lui->status = (EMC_STAT *)lui->status_nml->get_address();

    return 0;

cleanup:
    delete lui->status_nml;
    lui->status_nml = 0;
    lui->status = 0;
    return -1;
}


static int lui_connect_to_error_nml(lui_t *lui) {
    lui->error_nml = new NML(nmlErrorFormat, "emcError", "xemc", emc_nmlfile);
    if (!lui->error_nml->valid()) {
        delete lui->error_nml;
        lui->error_nml = NULL;
        return -1;
    }
    return 0;
}


int lui_connect(lui_t *lui) {
    int r;

    if ((emc_debug & EMC_DEBUG_NML) == 0) {
        // inhibit diag messages
	set_rcs_print_destination(RCS_PRINT_TO_NULL);
    }

    r = lui_connect_to_command_nml(lui);
    if (r != 0) {
        // nope
        fprintf(stderr, "failed to connect to Command NML\n");
        return -1;
    }

    r = lui_connect_to_status_nml(lui);
    if (r != 0) {
        // nope
        fprintf(stderr, "failed to connect to Status NML\n");
        return -1;
    }

    r = lui_connect_to_error_nml(lui);
    if (r != 0) {
        // nope
        fprintf(stderr, "failed to connect to Error NML\n");
        return -1;
    }

    return 0;
}

