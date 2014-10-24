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
    fatal_if(r != 0, "Error: failed to update Status buffer\n");

    actual_mode = lui_get_task_mode(lui);
    printf("task mode is %d\n", actual_mode);
    fatal_if(actual_mode != expected_mode,
        "Error: expected mode %d, got mode %d\n", expected_mode, actual_mode);
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

void test_task_introspect(lui_t *lui)
{
    printf("introspecting interpreter\n");
    size_t count;
    int *gcodes = lui_get_active_gcodes(lui, &count);
    size_t count1 = lui_get_active_gcodes_count(lui);
    fatal_if(count1 != count,
        "lui_get_acive_gcodes_count inconsistent with lui_get_active_gcodes (count=%zd count1=%zd)\n",
        count, count1);
    for(size_t i=0; i<count; i++) {
        if(gcodes[i] == -1) continue;
        printf("G%d.%d ", gcodes[i]/10, gcodes[i]%10);
        fatal_if(gcodes[i] != *lui_get_active_gcodes_idx(lui, i),
            "lui_get_active_gcodes_idx inconsistent with lui_get_active_gcodes");
    }

    putchar('\n');
    int *mcodes = lui_get_active_mcodes(lui, &count);
    for(size_t i=0; i<count; i++) {
        if(mcodes[i] == -1) continue;
        printf("M%d ", mcodes[i]);
    }
    putchar('\n');

    lui_tool_info_t *tinfo = lui_get_tool_table_idx(lui, 1);
    printf("T%d Z%f D%f\n", tinfo->toolno, tinfo->offset.z, tinfo->diameter);
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
    test_task_introspect(lui);

    lui_estop(lui);
    lui_free(lui);
    printf("kthxbye\n");

    return 0;
}
