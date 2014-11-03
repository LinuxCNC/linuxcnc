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
#include <string.h>

#include "linuxcnc-ui-private.h"


int lui_send_mdi_command(lui_t *lui, const char *mdi) {
    EMC_TASK_PLAN_EXECUTE task_plan_execute_msg;

    if (lui_get_task_mode(lui) != lui_task_mode_mdi) {
        return -1;
    }

    // If the length of the passed-in MDI command plus the terminating NULL
    // is bigger than the size of the NML command message buffer, then we
    // error out here.
    if (strlen(mdi) + 1 > sizeof(task_plan_execute_msg.command)) {
        return -1;
    }

    // It's safe to use strcpy here because we just verified that the
    // command will fit.
    strcpy(task_plan_execute_msg.command, mdi);

    return lui_send_nml_command_and_wait(lui, task_plan_execute_msg);
}
