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

#include <string.h>
#include "linuxcnc-ui-private.h"


int lui_program_get_line(lui_t *lui) {
    return lui->line;
}


int lui_program_pause(lui_t *lui) {
    EMC_TASK_PLAN_PAUSE msg;
    return lui_send_nml_command_and_wait(lui, msg);
}

int lui_program_resume(lui_t *lui) {
    EMC_TASK_PLAN_RESUME msg;
    return lui_send_nml_command_and_wait(lui, msg);
}


int lui_program_run(lui_t *lui, int line) {
    EMC_TASK_PLAN_RUN msg;
    char *task_file;
    int r;

    // first reopen program if it's not open
    task_file = lui_get_file(lui);
    if ((task_file == NULL) || (task_file[0] == (char)0)) {
        r = lui_program_open(lui, lui->program);
        if (r != 0) {
            return r;
        }
    }

    lui->line = line;

    msg.line = line;
    return lui_send_nml_command_and_wait(lui, msg);
}


int lui_program_open(lui_t *lui, const char *file) {
    EMC_TASK_PLAN_OPEN msg;

    if ((file == NULL) || (file[0] == (char)0)) {
        return -1;
    }

    // If the length of the passed-in filename the terminating NULL is
    // bigger than the size of the NML command message buffer, then we
    // error out here.
    if (strlen(file) + 1 > sizeof(msg.file)) {
        return -1;
    }

    // Automatically switch Task into Auto mode, if needed.
    if (lui_get_task_mode(lui) != lui_task_mode_auto) {
        lui_mode_auto(lui);
    }

    // It's safe to use strcpy here because we just verified that the
    // file name will fit.
    strcpy(msg.file, file);

    // Cache the filename inside the lui_t so we can reopen it if the user
    // asks to re-run it later.
    // FIXME: maybe this should be cached down in Task instead?  Or not
    //     cached at all and make the UI handle re-running files?  Not sure.
    strcpy(lui->program, file);

    return lui_send_nml_command_and_wait(lui, msg);
}


int lui_program_step(lui_t *lui) {
    EMC_TASK_PLAN_STEP msg;
    char *task_file;
    int r;

    // first reopen program if it's not open
    task_file = lui_get_file(lui);
    if ((task_file == NULL) || (task_file[0] == (char)0)) {
        r = lui_program_open(lui, lui->program);
        if (r != 0) {
            return r;
        }
    }

    return lui_send_nml_command_and_wait(lui, msg);
}
