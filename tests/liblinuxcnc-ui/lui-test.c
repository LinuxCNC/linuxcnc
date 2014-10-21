//
// This is lui-test, a test program for liblinuxcnc-ui
//
// Copyright (C) 2014 Sebastian Kuzminsky <seb@highlab.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "linuxcnc-ui.h"

#define fatal_if(cond, message, ...) do { \
    if(cond) { printf(message, ## __VA_ARGS__); exit(1); } \
} while(0)

void verify_state(lui_t *lui, lui_task_state_t expected_state) {
    int r;
    lui_task_state_t actual_state;

    r = lui_status_nml_update(lui);
    fatal_if(r != 0, "Error: failed to update Status buffer\n");

    actual_state = lui_get_task_state(lui);
    printf("task state is %d\n", actual_state);
    fatal_if(actual_state != expected_state,
        "Error: expected state %d, got state %d\n", expected_state, actual_state);
}


void verify_mode(lui_t *lui, lui_task_mode_t expected_mode) {
    int r;
    lui_task_mode_t actual_mode;

    r = lui_status_nml_update(lui);
    if (r != 0) {
        printf("Error: failed to update Status buffer\n");
        exit(1);
    }

    actual_mode = lui_get_task_mode(lui);
    printf("task mode is %d\n", actual_mode);
    if (actual_mode != expected_mode) {
        printf("Error: expected mode %d, got mode %d\n", expected_mode, actual_mode);
        exit(1);
    }
}


void test_task_state(lui_t *lui) {
    int r;

    //
    // start state is Estop
    //

    verify_state(lui, lui_task_state_estop);


    //
    // from the Estop state, the only thing that changes state is Estop Reset
    //

    printf("*** testing transitions from the Estop state\n");

    printf("sending Estop to linuxcnc\n");
    r = lui_estop(lui);
    fatal_if(r != 0, "Error: failed to send Estop to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop);


    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);


    //
    // in the "Estop Reset" state:
    //     "Estop" takes you to "Estop"
    //     "Machine On" takes you to "Machine On"
    //     everything else leaves you in "Estop Reset"
    //

    printf("*** testing transitions from the Estop Reset state\n");

    verify_state(lui, lui_task_state_estop_reset);

    printf("sending Estop to linuxcnc\n");
    r = lui_estop(lui);
    fatal_if(r != 0, "Error: failed to send Estop to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop);

    // go back to Estop Reset so we can keep testing it
    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_on);


    //
    // in the "Machine On" state:
    //     "Estop" takes you to "Estop"
    //     "Machine Off" takes you to "Estop Reset"
    //     everything else leaves you in "Machine On"
    //

    printf("*** testing transitions from the Machine On state\n");

    verify_state(lui, lui_task_state_on);

    printf("sending Estop to linuxcnc\n");
    r = lui_estop(lui);
    fatal_if(r != 0, "Error: failed to send Estop to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop);

    // go back to Machine On so we can keep testing it
    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);

    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_on);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_on);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_estop_reset);

    // go back to Machine On so we can keep testing it
    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_on);

    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    lui_command_nml_wait_done(lui);
    verify_state(lui, lui_task_state_on);
}


void test_task_mode(lui_t *lui) {
    int r;

    // We start out in Manual
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> Manual\n");
    r = lui_mode_manual(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Manual to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> Auto\n");
    r = lui_mode_auto(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Auto to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> Auto\n");
    r = lui_mode_auto(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Auto to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> MDI\n");
    r = lui_mode_mdi(lui);
    if (r != 0) {
        printf("Error: failed to send Mode MDI to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> MDI\n");
    r = lui_mode_mdi(lui);
    if (r != 0) {
        printf("Error: failed to send Mode MDI to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> Auto\n");
    r = lui_mode_auto(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Auto to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> Manual\n");
    r = lui_mode_manual(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Manual to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> MDI\n");
    r = lui_mode_mdi(lui);
    if (r != 0) {
        printf("Error: failed to send Mode MDI to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> Manual\n");
    r = lui_mode_manual(lui);
    if (r != 0) {
        printf("Error: failed to send Mode Manual to linuxcnc!\n");
        exit(1);
    }
    lui_command_nml_wait_done(lui);
    verify_mode(lui, lui_task_mode_manual);

}


int main(int argc, char *argv[]) {
    lui_t *lui;
    int r;

    lui = lui_new();
    if (lui == NULL) {
        fprintf(stderr, "failed to allocate lui\n");
        return 1;
    }

    r = lui_connect(lui);
    if (r != 0) {
        fprintf(stderr, "failed to connect lui to linuxcnc\n");
        return 1;
    }


    test_task_state(lui);
    test_task_mode(lui);


    lui_estop(lui);
    lui_free(lui);
    printf("kthxbye\n");

    return 0;
}
