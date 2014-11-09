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
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "linuxcnc-ui.h"

#define fatal_if(cond, message, ...) do { \
    if(cond) { printf(message, ## __VA_ARGS__); exit(1); } \
} while(0)

void verify_lube(lui_t *lui, int expected_lube) {
    int r;
    int actual_lube;

    printf("verifying lube=%d\n", expected_lube);

    r = lui_status_nml_update(lui);
    fatal_if(r != 0, "Error: failed to update Status buffer\n");

    actual_lube = lui_get_lube(lui);
    fatal_if(actual_lube != expected_lube, "Error: lube is %d, expected %d\n", actual_lube, expected_lube);
}

void verify_coolant(lui_t *lui, int expected_flood, int expected_mist) {
    int r;
    int actual_flood, actual_mist;

    printf("verifying flood=%d, mist=%d\n", expected_flood, expected_mist);

    r = lui_status_nml_update(lui);
    fatal_if(r != 0, "Error: failed to update Status buffer\n");

    actual_flood = lui_get_flood(lui);
    fatal_if(actual_flood != expected_flood, "Error: flood is %d, expected %d\n", actual_flood, expected_flood);

    actual_mist = lui_get_mist(lui);
    fatal_if(actual_mist != expected_mist, "Error: mist is %d, expected %d\n", actual_mist, expected_mist);
}

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


void verify_traj_mode(lui_t *lui, lui_traj_mode_t expected_traj_mode) {
    int r;
    lui_traj_mode_t actual_traj_mode;

    struct timeval start, end, now;
    struct timeval timeout = { 5, 0 };

    gettimeofday(&start, NULL);
    now = start;
    timeradd(&start, &timeout, &end);

    do {
        r = lui_status_nml_update(lui);
        fatal_if(r != 0, "Error: failed to update Status buffer\n");

        actual_traj_mode = lui_get_traj_mode(lui);
        if (actual_traj_mode == expected_traj_mode) {
            timersub(&now, &start, &end);
            printf("traj mode became the expected (%d) after %d.%06d seconds\n", actual_traj_mode, end.tv_sec, end.tv_usec);
            return;
        }

        usleep(1000);
        gettimeofday(&now, NULL);
    } while (timercmp(&now, &end, <));

    printf("Error: did not see expected traj mode %d within the timeout of %d.%06d seconds\n", expected_traj_mode, timeout.tv_sec, timeout.tv_usec);
    exit(1);
}


void expect_error(lui_t *lui, lui_error_t expected_err_type, const char *expected_msg) {
    const char *actual_msg;
    lui_error_t actual_err_type;
    int r;

    r = lui_error(lui, &actual_err_type, &actual_msg);
    fatal_if(r != 0, "error getting error\n");

    fatal_if(actual_err_type != expected_err_type, "expected error type %d, got %d\n", expected_err_type, actual_err_type);
    fatal_if(strcmp(actual_msg, expected_msg) != 0, "expected error message [%s], got [%s]\n", expected_msg, actual_msg);

    printf("got expected error type %d: %s\n", actual_err_type, actual_msg);
}


void drain_errors(lui_t *lui) {
    const char *msg;
    lui_error_t err_type;
    int r;

    while (lui_error(lui, &err_type, &msg) == 0) {
        switch (err_type) {
            case lui_no_error:
                return;

            case lui_unknown_error:
                printf("lui reports unknown error type\n");
                break;

            case lui_operator_error:
            case lui_operator_text:
            case lui_operator_display:
            case lui_nml_error:
            case lui_nml_text:
            case lui_nml_display:
                printf("got error type %d from linuxcnc:\n", err_type);
                printf("**********\n");
                printf("%s", msg);
                if (msg[strlen(msg)] != '\n') {
                    printf("\n");
                }
                printf("**********\n");
                break;

            default:
                printf("got unknown error type %d\n", err_type);
                break;
        }
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
    verify_state(lui, lui_task_state_estop);


    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
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
    verify_state(lui, lui_task_state_estop);

    // go back to Estop Reset so we can keep testing it
    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop_reset);


    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
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
    verify_state(lui, lui_task_state_estop);

    // go back to Machine On so we can keep testing it
    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop_reset);

    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    verify_state(lui, lui_task_state_on);


    printf("sending Estop Reset to linuxcnc\n");
    r = lui_estop_reset(lui);
    fatal_if(r != 0, "Error: failed to send Estop Reset to linuxcnc!\n");
    verify_state(lui, lui_task_state_on);


    printf("sending Machine Off to linuxcnc\n");
    r = lui_machine_off(lui);
    fatal_if(r != 0, "Error: failed to send Machine Off to linuxcnc!\n");
    verify_state(lui, lui_task_state_estop_reset);

    // go back to Machine On so we can keep testing it
    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    verify_state(lui, lui_task_state_on);

    printf("sending Machine On to linuxcnc\n");
    r = lui_machine_on(lui);
    fatal_if(r != 0, "Error: failed to send Machine On to linuxcnc!\n");
    verify_state(lui, lui_task_state_on);
}


void test_task_mode(lui_t *lui) {
    int r;

    // We start out in Manual
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> Manual\n");
    r = lui_mode_manual(lui);
    fatal_if(r != 0, "Error: failed to send Mode Manual to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> Auto\n");
    r = lui_mode_auto(lui);
    fatal_if(r != 0, "Error: failed to send Mode Auto to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> Auto\n");
    r = lui_mode_auto(lui);
    fatal_if(r != 0, "Error: failed to send Mode Auto to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> MDI\n");
    r = lui_mode_mdi(lui);
    fatal_if(r != 0, "Error: failed to send Mode MDI to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> MDI\n");
    r = lui_mode_mdi(lui);
    fatal_if(r != 0, "Error: failed to send Mode MDI to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> Auto\n");
    r = lui_mode_auto(lui);
    fatal_if(r != 0, "Error: failed to send Mode Auto to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_auto);

    printf("Auto -> Manual\n");
    r = lui_mode_manual(lui);
    fatal_if(r != 0, "Error: failed to send Mode Manual to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_manual);

    printf("Manual -> MDI\n");
    r = lui_mode_mdi(lui);
    fatal_if(r != 0, "Error: failed to send Mode MDI to linuxcnc!\n");
    verify_mode(lui, lui_task_mode_mdi);

    printf("MDI -> Manual\n");
    r = lui_mode_manual(lui);
    fatal_if(r != 0, "Error: failed to send Mode Manual to linuxcnc!\n");
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


void test_coolant(lui_t *lui) {
    verify_coolant(lui, 0, 0);
    printf("flood on\n");
    lui_coolant_flood_on(lui);
    verify_coolant(lui, 1, 0);
    printf("mist on\n");
    lui_coolant_mist_on(lui);
    verify_coolant(lui, 1, 1);
    printf("flood off\n");
    lui_coolant_flood_off(lui);
    verify_coolant(lui, 0, 1);
    printf("mist off\n");
    lui_coolant_mist_off(lui);
    verify_coolant(lui, 0, 0);
}


void test_lube(lui_t *lui) {
    // lube starts out on, since the machine is out of estop
    verify_lube(lui, 1);

    printf("lube off\n");
    lui_lube_off(lui);
    verify_lube(lui, 0);

    printf("lube on\n");
    lui_lube_on(lui);
    verify_lube(lui, 1);
}


void test_jog_mode(lui_t *lui) {
    // Make sure Task is in Manual mode, because that's the only Task mode
    // that allows jogging.
    lui_mode_manual(lui);
    verify_mode(lui, lui_task_mode_manual);

    // when Task goes to Manual mode, it places Motion in Free mode (aka joint mode)
    verify_traj_mode(lui, lui_traj_mode_free);

    printf("teleop mode on\n");
    lui_jog_mode_teleop(lui);
    verify_traj_mode(lui, lui_traj_mode_teleop);

    printf("joint mode on\n");
    lui_jog_mode_joint(lui);
    verify_traj_mode(lui, lui_traj_mode_free);
}


void test_program(lui_t *lui) {
    int r;
    const char *e;

    lui_estop_reset(lui);
    lui_machine_on(lui);
    lui_mode_auto(lui);

    drain_errors(lui);

    printf("opening a missing program\n");
    r = lui_program_open(lui, "missing.ngc");
    fatal_if(r == 0, "opening a missing gcode file succeeded?!\n");

    expect_error(lui, lui_operator_error, "Unable to open file <missing.ngc>");
    expect_error(lui, lui_operator_error, "can't open missing.ngc");
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

    lui_set_command_wait_mode(lui, lui_command_wait_mode_done);


    test_task_state(lui);
    test_task_mode(lui);
    test_task_introspect(lui);
    test_coolant(lui);
    test_lube(lui);
    test_jog_mode(lui);
    test_program(lui);

    lui_estop(lui);
    lui_free(lui);
    printf("kthxbye\n");

    return 0;
}
