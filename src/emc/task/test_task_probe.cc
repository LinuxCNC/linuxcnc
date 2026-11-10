/********************************************************************
* Description: test_task_probe.cc
*   Test helper for tests/task-planner: sends task one command at a time,
*   with arguments that change as little as possible, so that the test can
*   record what the planner does with each command in each task state.
*
*   It reads lines from stdin:
*
*     list                print the names of all the commands it can send
*     probe LABEL NAME    send command NAME, bracketed by two operator
*                         display messages "probe-begin LABEL NAME" and
*                         "probe-end", and print "serial N" with the
*                         serial number the command was sent with
*
*   Every message is sent only once task has taken the previous one, so
*   task's EMC_DEBUG_TASK_ISSUE output shows them in order, between the two
*   markers. The test reads that output; this helper reports nothing else.
*
*   Built into ../bin for run-in-place tests, never installed.
*
* License: GPL Version 2
* System: Linux
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "nml_intf/emc.hh"
#include "nml_intf/emc_nml.hh"
#include "nml_intf/emcglb.h"	// emc_nmlfile, emcGetArgs()
#include <emcmotcfg.h>		// EMCMOT_MAX_JOINTS
#include <inifile.hh>
#include "timeutil.hh"

static RCS_CMD_CHANNEL *cmdChannel;
static RCS_STAT_CHANNEL *statChannel;

static const EMC_STAT &stat()
{
    statChannel->peek();
    return *static_cast<EMC_STAT *>(statChannel->get_address());
}

// Send a command and wait until task has taken it. Returns its serial
// number, or -1.
static int send(RCS_CMD_MSG &cmd)
{
    if (cmdChannel->write(&cmd) != 0) {
	return -1;
    }
    const int serial = cmd.serial_number;
    const double end = linuxcnc::etime() + 10.0;
    while (linuxcnc::etime() < end) {
	if (stat().echo_serial_number - serial >= 0) {
	    return serial;
	}
	usleep(1000);
    }
    fprintf(stderr, "test_task_probe: timeout waiting for serial %d\n", serial);
    return -1;
}

static int display(const std::string &text)
{
    EMC_OPERATOR_DISPLAY msg;
    snprintf(msg.display, sizeof(msg.display), "%s", text.c_str());
    return send(msg);
}

// A joint number no machine has: the joint commands that check their
// joint number then do nothing at all.
static const int NO_JOINT = EMCMOT_MAX_JOINTS;

typedef std::function<std::unique_ptr<RCS_CMD_MSG>(const EMC_STAT &)> Maker;

// The command, built with arguments that leave things as they are where
// that is possible: the values task already has, joint NO_JOINT, spindle 0.
static std::vector<std::pair<std::string, Maker>> commands()
{
    std::vector<std::pair<std::string, Maker>> c;

#define CMD(name, T, body) \
    c.emplace_back(name, [](const EMC_STAT &s) -> std::unique_ptr<RCS_CMD_MSG> { \
	auto m = std::make_unique<T>(); \
	(void)s; \
	body; \
	return m; \
    })

    CMD("EMC_NULL", EMC_NULL, );

    // joint configuration, for a joint that does not exist
    CMD("EMC_JOINT_SET_BACKLASH", EMC_JOINT_SET_BACKLASH, m->joint = NO_JOINT);
    CMD("EMC_JOINT_SET_HOMING_PARAMS", EMC_JOINT_SET_HOMING_PARAMS, m->joint = NO_JOINT);
    CMD("EMC_JOINT_SET_FERROR", EMC_JOINT_SET_FERROR, m->joint = NO_JOINT);
    CMD("EMC_JOINT_SET_MIN_FERROR", EMC_JOINT_SET_MIN_FERROR, m->joint = NO_JOINT);
    CMD("EMC_JOINT_SET_MAX_POSITION_LIMIT", EMC_JOINT_SET_MAX_POSITION_LIMIT, m->joint = NO_JOINT);
    CMD("EMC_JOINT_SET_MIN_POSITION_LIMIT", EMC_JOINT_SET_MIN_POSITION_LIMIT, m->joint = NO_JOINT);
    CMD("EMC_JOINT_LOAD_COMP", EMC_JOINT_LOAD_COMP, m->joint = NO_JOINT);
    CMD("EMC_JOINT_HALT", EMC_JOINT_HALT, m->joint = NO_JOINT);
    CMD("EMC_JOINT_HOME", EMC_JOINT_HOME, m->joint = NO_JOINT);
    CMD("EMC_JOINT_UNHOME", EMC_JOINT_UNHOME, m->joint = NO_JOINT);
    CMD("EMC_JOINT_OVERRIDE_LIMITS", EMC_JOINT_OVERRIDE_LIMITS, m->joint = NO_JOINT);

    // jogs of a joint or axis that does not exist
    CMD("EMC_JOG_CONT", EMC_JOG_CONT, m->joint_or_axis = NO_JOINT; m->jjogmode = 1);
    CMD("EMC_JOG_INCR", EMC_JOG_INCR, m->joint_or_axis = NO_JOINT; m->jjogmode = 1);
    CMD("EMC_JOG_ABS", EMC_JOG_ABS, m->joint_or_axis = NO_JOINT; m->jjogmode = 1);
    CMD("EMC_JOG_STOP", EMC_JOG_STOP, m->joint_or_axis = NO_JOINT; m->jjogmode = 1);

    // trajectory settings, set to what they are
    CMD("EMC_TRAJ_SET_SCALE", EMC_TRAJ_SET_SCALE, m->scale = s.motion.traj.scale);
    CMD("EMC_TRAJ_SET_RAPID_SCALE", EMC_TRAJ_SET_RAPID_SCALE, m->scale = s.motion.traj.rapid_scale);
    CMD("EMC_TRAJ_SET_MAX_VELOCITY", EMC_TRAJ_SET_MAX_VELOCITY, m->velocity = s.motion.traj.maxVelocity);
    CMD("EMC_TRAJ_SET_SPINDLE_SCALE", EMC_TRAJ_SET_SPINDLE_SCALE,
	m->spindle = 0; m->scale = s.motion.spindle[0].spindle_scale);
    CMD("EMC_TRAJ_SET_FO_ENABLE", EMC_TRAJ_SET_FO_ENABLE, m->mode = s.motion.traj.feed_override_enabled);
    CMD("EMC_TRAJ_SET_FH_ENABLE", EMC_TRAJ_SET_FH_ENABLE, m->mode = s.motion.traj.feed_hold_enabled);
    CMD("EMC_TRAJ_SET_SO_ENABLE", EMC_TRAJ_SET_SO_ENABLE,
	m->spindle = 0; m->mode = s.motion.spindle[0].spindle_override_enabled);
    CMD("EMC_TRAJ_SET_VELOCITY", EMC_TRAJ_SET_VELOCITY,
	m->velocity = s.motion.traj.velocity; m->ini_maxvel = s.motion.traj.maxVelocity);
    CMD("EMC_TRAJ_SET_ACCELERATION", EMC_TRAJ_SET_ACCELERATION, m->acceleration = s.motion.traj.acceleration);
    CMD("EMC_TRAJ_SET_TELEOP_ENABLE", EMC_TRAJ_SET_TELEOP_ENABLE,
	m->enable = s.motion.traj.mode == EMC_TRAJ_MODE::TELEOP);
    CMD("EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG", EMC_TRAJ_CLEAR_PROBE_TRIPPED_FLAG, );
    CMD("EMC_MOTION_SET_DOUT", EMC_MOTION_SET_DOUT, m->index = 0; m->start = 0; m->end = 0; m->now = 1);
    CMD("EMC_MOTION_SET_AOUT", EMC_MOTION_SET_AOUT, m->index = 0; m->start = 0; m->end = 0; m->now = 1);
    CMD("EMC_MOTION_ADAPTIVE", EMC_MOTION_ADAPTIVE, m->status = s.motion.traj.adaptive_feed_enabled);
    CMD("EMC_AUX_INPUT_WAIT", EMC_AUX_INPUT_WAIT,
	m->index = 0; m->input_type = 0; m->wait_type = 0; m->timeout = 0);
    CMD("EMC_SET_DEBUG", EMC_SET_DEBUG, m->debug = s.debug);

    // spindle and coolant, each "on" followed by its "off"
    CMD("EMC_SPINDLE_SPEED", EMC_SPINDLE_SPEED, m->spindle = 0; m->speed = s.motion.spindle[0].speed);
    CMD("EMC_SPINDLE_ON", EMC_SPINDLE_ON, m->spindle = 0; m->speed = 0);
    CMD("EMC_SPINDLE_INCREASE", EMC_SPINDLE_INCREASE, m->spindle = 0);
    CMD("EMC_SPINDLE_DECREASE", EMC_SPINDLE_DECREASE, m->spindle = 0);
    CMD("EMC_SPINDLE_CONSTANT", EMC_SPINDLE_CONSTANT, m->spindle = 0);
    CMD("EMC_SPINDLE_OFF", EMC_SPINDLE_OFF, m->spindle = 0);
    CMD("EMC_SPINDLE_BRAKE_ENGAGE", EMC_SPINDLE_BRAKE_ENGAGE, m->spindle = 0);
    CMD("EMC_SPINDLE_BRAKE_RELEASE", EMC_SPINDLE_BRAKE_RELEASE, m->spindle = 0);
    CMD("EMC_COOLANT_MIST_ON", EMC_COOLANT_MIST_ON, );
    CMD("EMC_COOLANT_MIST_OFF", EMC_COOLANT_MIST_OFF, );
    CMD("EMC_COOLANT_FLOOD_ON", EMC_COOLANT_FLOOD_ON, );
    CMD("EMC_COOLANT_FLOOD_OFF", EMC_COOLANT_FLOOD_OFF, );

    // the interpreter's settings, set to what they are
    CMD("EMC_TASK_PLAN_SET_OPTIONAL_STOP", EMC_TASK_PLAN_SET_OPTIONAL_STOP,
	m->state = s.task.optional_stop_state);
    CMD("EMC_TASK_PLAN_SET_BLOCK_DELETE", EMC_TASK_PLAN_SET_BLOCK_DELETE,
	m->state = s.task.block_delete_state);
    CMD("EMC_TASK_PLAN_SYNCH", EMC_TASK_PLAN_SYNCH, );
    CMD("EMC_TASK_PLAN_OPEN", EMC_TASK_PLAN_OPEN,
	snprintf(m->file, sizeof(m->file), "%s", s.task.file));

    // A command the planner has no case for, so refused unless a state
    // accepts it without looking, and one the interpreter never queues,
    // which would make it look queued. Should it get through, it sets the
    // mode motion is in.
    CMD("EMC_TRAJ_SET_MODE", EMC_TRAJ_SET_MODE, m->mode = s.motion.traj.mode);

    // Commands that may well change task's state. The test sets the state
    // up again after each of them.
    CMD("EMC_TOOL_LOAD_TOOL_TABLE", EMC_TOOL_LOAD_TOOL_TABLE, );
    CMD("EMC_TOOL_SET_NUMBER", EMC_TOOL_SET_NUMBER, m->tool = s.io.tool.toolInSpindle);
    CMD("EMC_TOOL_SET_OFFSET", EMC_TOOL_SET_OFFSET, m->pocket = 1; m->toolno = 1; m->diameter = 0.25);
    CMD("EMC_SPINDLE_ORIENT", EMC_SPINDLE_ORIENT, m->spindle = 0);
    CMD("EMC_SPINDLE_WAIT_ORIENT_COMPLETE", EMC_SPINDLE_WAIT_ORIENT_COMPLETE,
	m->spindle = 0; m->timeout = 0.001);
    CMD("EMC_TRAJ_PROBE", EMC_TRAJ_PROBE, m->pos = s.motion.traj.position);
    CMD("EMC_TRAJ_RIGID_TAP", EMC_TRAJ_RIGID_TAP, m->pos = s.motion.traj.position);
    CMD("EMC_TASK_PLAN_OPTIONAL_STOP", EMC_TASK_PLAN_OPTIONAL_STOP, );
    CMD("EMC_TASK_PLAN_EXECUTE", EMC_TASK_PLAN_EXECUTE,
	snprintf(m->command, sizeof(m->command), "(probe)"));
    CMD("EMC_TASK_PLAN_INIT", EMC_TASK_PLAN_INIT, );
    CMD("EMC_TASK_PLAN_CLOSE", EMC_TASK_PLAN_CLOSE, );
    CMD("EMC_TASK_PLAN_RUN", EMC_TASK_PLAN_RUN, m->line = 0);
    CMD("EMC_TASK_PLAN_STEP", EMC_TASK_PLAN_STEP, );
    CMD("EMC_TASK_PLAN_PAUSE", EMC_TASK_PLAN_PAUSE, );
    CMD("EMC_TASK_PLAN_RESUME", EMC_TASK_PLAN_RESUME, );
    CMD("EMC_TASK_PLAN_REVERSE", EMC_TASK_PLAN_REVERSE, );
    CMD("EMC_TASK_PLAN_FORWARD", EMC_TASK_PLAN_FORWARD, );
    CMD("EMC_TASK_ABORT", EMC_TASK_ABORT, );
    CMD("EMC_TASK_SET_MODE(MANUAL)", EMC_TASK_SET_MODE, m->mode = EMC_TASK_MODE::MANUAL);
    CMD("EMC_TASK_SET_MODE(AUTO)", EMC_TASK_SET_MODE, m->mode = EMC_TASK_MODE::AUTO);
    CMD("EMC_TASK_SET_MODE(MDI)", EMC_TASK_SET_MODE, m->mode = EMC_TASK_MODE::MDI);
    CMD("EMC_TASK_SET_STATE(ESTOP)", EMC_TASK_SET_STATE, m->state = EMC_TASK_STATE::ESTOP);
    CMD("EMC_TASK_SET_STATE(ESTOP_RESET)", EMC_TASK_SET_STATE, m->state = EMC_TASK_STATE::ESTOP_RESET);
    CMD("EMC_TASK_SET_STATE(OFF)", EMC_TASK_SET_STATE, m->state = EMC_TASK_STATE::OFF);
    CMD("EMC_TASK_SET_STATE(ON)", EMC_TASK_SET_STATE, m->state = EMC_TASK_STATE::ON);

#undef CMD
    return c;
}

int main(int argc, char *argv[])
{
    if (emcGetArgs(argc, argv) != 0) {
	fprintf(stderr, "usage: %s [-ini INIFILE]\n", argv[0]);
	return 1;
    }
    {
	linuxcnc::IniFile inifile(emc_inifile);
	if (inifile) {
	    if (auto nml = inifile.findString("NML_FILE", "EMC")) {
		snprintf(emc_nmlfile, sizeof(emc_nmlfile), "%s", nml->c_str());
	    }
	}
    }

    // like shcom's tryNml(): the status needs a moment after connecting
    const double end = linuxcnc::etime() + 10.0;
    while (true) {
	cmdChannel = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", emc_nmlfile);
	statChannel = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", emc_nmlfile);
	if (cmdChannel->valid() && statChannel->valid() && statChannel->peek() == EMC_STAT_TYPE) {
	    break;
	}
	delete cmdChannel;
	delete statChannel;
	if (linuxcnc::etime() > end) {
	    fprintf(stderr, "test_task_probe: can't connect to task\n");
	    return 1;
	}
	usleep(100000);
    }

    const auto cmds = commands();
    std::map<std::string, const Maker *> byName;
    for (const auto &c : cmds) {
	byName[c.first] = &c.second;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
	char what[32], label[64], name[64];
	int n = sscanf(line.c_str(), "%31s %63s %63s", what, label, name);
	if (n >= 1 && strcmp(what, "list") == 0) {
	    for (const auto &c : cmds) {
		printf("%s\n", c.first.c_str());
	    }
	    printf("end\n");
	} else if (n == 3 && strcmp(what, "probe") == 0 && byName.count(name)) {
	    auto cmd = (*byName[name])(stat());
	    int serial = -1;
	    if (display(std::string("probe-begin ") + label + " " + name) >= 0) {
		serial = send(*cmd);
		display("probe-end");
	    }
	    printf("serial %d\n", serial);
	} else {
	    printf("error %s\n", line.c_str());
	}
	fflush(stdout);
    }
    return 0;
}
