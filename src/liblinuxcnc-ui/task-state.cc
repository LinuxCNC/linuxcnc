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


int lui_command_nml_task_set_state(lui_t *lui, lui_task_state_t state) {
    EMC_TASK_SET_STATE state_msg;
    state_msg.state = (EMC_TASK_STATE_ENUM)state;
    state_msg.serial_number = ++lui->nml_serial_number;
    lui->command_nml->write(state_msg);
    return lui_command_nml_wait_received(lui);
}


int lui_estop(lui_t *lui) {
    return lui_command_nml_task_set_state(lui, lui_task_state_estop);
}

int lui_estop_reset(lui_t *lui) {
    return lui_command_nml_task_set_state(lui, lui_task_state_estop_reset);
}

int lui_machine_on(lui_t *lui) {
    return lui_command_nml_task_set_state(lui, lui_task_state_on);
}

int lui_machine_off(lui_t *lui) {
    return lui_command_nml_task_set_state(lui, lui_task_state_off);
}

