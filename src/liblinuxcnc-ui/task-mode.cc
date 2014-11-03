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

#include "linuxcnc-ui-private.h"


int lui_command_nml_task_set_mode(lui_t *lui, lui_task_mode_t mode) {
    EMC_TASK_SET_MODE mode_msg;

    if (lui->status->task.mode == (EMC_TASK_MODE_ENUM)mode) {
        return 0;
    }

    mode_msg.mode = (EMC_TASK_MODE_ENUM)mode;
    return lui_send_nml_command_and_wait(lui, mode_msg);
}


int lui_mode_manual(lui_t *lui) {
    return lui_command_nml_task_set_mode(lui, lui_task_mode_manual);
}

int lui_mode_auto(lui_t *lui) {
    return lui_command_nml_task_set_mode(lui, lui_task_mode_auto);
}

int lui_mode_mdi(lui_t *lui) {
    return lui_command_nml_task_set_mode(lui, lui_task_mode_mdi);
}

