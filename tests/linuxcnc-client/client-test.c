/********************************************************************
* Description: client-test.c
*   The C client API against a running simulator.
*
*   This is the DISPLAY program of client-test.ini, so "linuxcnc"
*   starts it, waits for it and reports its exit status.  It does what a
*   UI does with the Python linuxcnc module: take the status, bring the
*   machine up, switch to MDI, run a block, and read the error channel
*   and the tool table.  Every failure is counted, so the test tells all
*   of them instead of stopping at the first.
*
* License: GPL Version 2
* System: Linux
********************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "linuxcnc_client.h"

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fflush(stdout);
    if (!ok) {
	failures++;
    }
}

static void check_result(struct linuxcnc_client *client, int result,
			 const char *what) {
    const int ok = result == 0;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
	printf("     %s\n", linuxcnc_client_last_error(client));
	failures++;
    }
    fflush(stdout);
}

/* Print whatever the error channel has, so a failed step says why. */
static void drain_errors(struct linuxcnc_client *client) {
    struct linuxcnc_error message;
    linuxcnc_error_init(&message);
    while (linuxcnc_error_poll(client, &message) > 0) {
	printf("     error channel: %s\n", message.text);
    }
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Take the status until "done" returns true, or the deadline passes. */
static int wait_status(struct linuxcnc_client *client,
		       struct linuxcnc_status *status, int (*done)(void *),
		       void *context, double timeout) {
    const double deadline = now() + timeout;
    while (now() < deadline) {
	if (linuxcnc_status_poll(client, status) != 0) {
	    return 0;
	}
	if (status->valid && done(context)) {
	    return 1;
	}
	usleep(20000);
    }
    return 0;
}

static int ready(void *context) {
    const struct linuxcnc_status *status = context;
    return status->joints > 0;
}

static int machine_on(void *context) {
    const struct linuxcnc_status *status = context;
    return status->task_state == LINUXCNC_STATE_ON;
}

static int in_mdi(void *context) {
    const struct linuxcnc_status *status = context;
    return status->task_mode == LINUXCNC_MODE_MDI;
}

static int all_homed(void *context) {
    const struct linuxcnc_status *status = context;
    for (int i = 0; i < status->joints && i < LINUXCNC_MAX_JOINTS; i++) {
	if (!status->joint[i].homed) {
	    return 0;
	}
    }
    return status->joints > 0;
}

static int moved_to_one(void *context) {
    const struct linuxcnc_status *status = context;
    return fabs(status->position.x - 1.0) < 0.001;
}

int main(int argc, char **argv) {
    /* "linuxcnc" exports INI_FILE_NAME for the program it starts as its
       DISPLAY.  A standalone client passes the INI file itself. */
    const char *config_file = getenv("INI_FILE_NAME");
    if (config_file == NULL && argc > 1 && argv[1][0] != '-') {
	config_file = argv[1];
    }
    char error[512];
    struct linuxcnc_status status;
    struct linuxcnc_error message;

    linuxcnc_status_init(&status);
    linuxcnc_error_init(&message);

    /* A missing INI must be reported, not guessed at. */
    check(linuxcnc_client_open("/nonexistent/client-test.ini", error,
			       sizeof(error)) == NULL &&
	      error[0] != '\0',
	  "a missing configuration file is refused with a message");

    /* The INI is what tells the library where the machine is; a wrong
       NML_FILE in it must fail instead of falling back somewhere else. */
    {
	const char *bad_ini = "client-test-bad-ini.ini";
	FILE *file = fopen(bad_ini, "w");
	if (file != NULL) {
	    fprintf(file, "[EMC]\nNML_FILE = /nonexistent/client-test.nml\n");
	    fclose(file);
	}
	check(linuxcnc_client_open(bad_ini, error, sizeof(error)) == NULL &&
		  error[0] != '\0',
	      "the INI decides where the machine is");
	remove(bad_ini);
    }

    struct linuxcnc_client *client =
	linuxcnc_client_open(config_file, error, sizeof(error));
    if (client == NULL) {
	printf("FAIL open: %s\n", error);
	return 1;
    }
    check(1, "open the status, command and error channels");

    /* The simulator is slow to start; a UI does the same. */
    linuxcnc_client_set_timeout(client, 30.0);

    check(linuxcnc_status_size() == sizeof(struct linuxcnc_status),
	  "status size matches the header");
    check(strcmp(linuxcnc_client_version(), "") != 0,
	  "the library reports a version");

    if (!wait_status(client, &status, ready, &status, 20.0)) {
	printf("FAIL no status from task: %s\n",
	       linuxcnc_client_last_error(client));
	return 1;
    }
    check(1, "task reports status");
    check(status.joints == 3, "three joints in the status");
    check(status.linear_units == 1.0, "linear units are millimetres");
    check(status.task_state == LINUXCNC_STATE_ESTOP ||
	      status.task_state == LINUXCNC_STATE_ESTOP_RESET,
	  "the machine starts out in e-stop");

    check_result(client,
		 linuxcnc_command_state(client, LINUXCNC_STATE_ESTOP_RESET),
		 "leave e-stop");
    check_result(client, linuxcnc_command_state(client, LINUXCNC_STATE_ON),
		 "power on");
    check(wait_status(client, &status, machine_on, &status, 30.0),
	  "task reports the machine on");
    check(status.last_command_serial > 0,
	  "the machine reports the serial of the command it took");
    drain_errors(client);

    /* Homing advances only in joint mode; task turns teleop off for it. */
    check_result(client, linuxcnc_command_teleop_enable(client, 0),
		 "leave teleop");
    check(linuxcnc_wait_complete(client, 30.0) == 1,
	  "the teleop change completes");
    check_result(client, linuxcnc_command_home(client, -1), "home all joints");
    if (!wait_status(client, &status, all_homed, &status, 30.0)) {
	printf("     joint homed flags:");
	for (int i = 0; i < status.joints && i < LINUXCNC_MAX_JOINTS; i++) {
	    printf(" %d", status.joint[i].homed);
	}
	printf("\n");
    }
    drain_errors(client);
    check(wait_status(client, &status, all_homed, &status, 0.1),
	  "task reports all joints homed");

    check_result(client, linuxcnc_command_mode(client, LINUXCNC_MODE_MDI),
		 "switch to MDI");
    check(wait_status(client, &status, in_mdi, &status, 30.0),
	  "task reports MDI mode");

    check_result(client, linuxcnc_command_mdi(client, "G0 X1"),
		 "run a block of MDI");
    check(linuxcnc_wait_complete(client, 30.0) == 1, "the block completes");
    check(linuxcnc_status_poll(client, &status) == 0 &&
	      status.command_status == LINUXCNC_COMMAND_DONE,
	  "the status reports the command done");
    check(wait_status(client, &status, moved_to_one, &status, 30.0),
	  "the machine moved to X1");
    drain_errors(client);

    /* The error channel is what a UI shows in its message window. */
    check_result(client,
		 linuxcnc_command_error_msg(client, "client test error"),
		 "send an operator error");
    int seen = 0;
    const double deadline = now() + 5.0;
    while (now() < deadline) {
	const int result = linuxcnc_error_poll(client, &message);
	if (result < 0) {
	    break;
	}
	if (result > 0 && strstr(message.text, "client test error") != NULL) {
	    seen = 1;
	    break;
	}
	usleep(20000);
    }
    check(seen, "the operator error arrives on the error channel");
    check(message.sequence == 0 &&
	      message.severity == LINUXCNC_SEVERITY_UNKNOWN &&
	      message.command_serial == 0,
	  "correlation fields are unset, as this transport cannot fill them");

    /* The tool table comes from the tool database, not from the status. */
    size_t count = 0;
    check(linuxcnc_tool_table(client, NULL, 0, &count) == 0,
	  "ask for the tool table size");
    check(count > 0, "the tool table is not empty");
    if (count > 0) {
	struct linuxcnc_tool *tools = calloc(count, sizeof(*tools));
	check(tools != NULL &&
		  linuxcnc_tool_table(client, tools, count, &count) == 0,
	      "read the tool table");
	free(tools);
    }

    /* Rejections a UI can rely on. */
    check(linuxcnc_command_mode(client, 99) == -1,
	  "an invalid mode is refused");
    check(strlen(linuxcnc_client_last_error(client)) > 0,
	  "the refusal comes with a message");
    struct linuxcnc_status too_small;
    memset(&too_small, 0, sizeof(too_small));
    too_small.size = 4;
    check(linuxcnc_status_poll(client, &too_small) == -1,
	  "a status buffer that is too small is refused");

    /* Leave the machine off; "linuxcnc" asks before quitting otherwise. */
    linuxcnc_command_abort(client);
    linuxcnc_command_state(client, LINUXCNC_STATE_ESTOP);
    linuxcnc_client_close(client);

    printf("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
