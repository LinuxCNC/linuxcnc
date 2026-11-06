/********************************************************************
* Description: linuxcnc_client.cc
*   C ABI client library for LinuxCNC's status, commands and errors.
*
*   The functions here are the C spelling of the Python "linuxcnc"
*   module: linuxcnc.stat, linuxcnc.command and linuxcnc.error_channel.
*   The status and command members are copied field for field from the
*   NML status message, so a UI that reads one can read the other.
*
*   The header is the contract.  How the library reaches task is not:
*   today it is NML, and replacing NML must not change any function or
*   structure in linuxcnc_client.h.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2026 All rights reserved.
********************************************************************/

#include "linuxcnc_client.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <exception>
#include <new>
#include <string>

#include <inifile.hh>

#include "config.h"
#include "nml_intf/emc.hh"
#include "nml_intf/emc_nml.hh"
#include "nml_intf/emccfg.h"
#include "nml_intf/modal_state.hh"
#include "timeutil.hh"
#include "libnml/nml/nml.hh"
#include "libnml/nml/nml_oi.hh"
#include "tooldata/tooldata.hh"

#ifdef TOOL_NML
/* The tool table of a TOOL_NML build lives in the status message, which
   is a different layout and a different way to reach it.  The Python
   module has the same restriction. */
#error "linuxcnc_client needs the mmap tool table (configure without --with-tool-nml)"
#endif

/* A change in any of these means linuxcnc_client.h has to change with it;
   failing to compile here is cheaper than a silent truncation at run time. */
static_assert(LINUXCNC_MAX_JOINTS == EMCMOT_MAX_JOINTS,
	      "joint array size differs from EMCMOT_MAX_JOINTS");
static_assert(LINUXCNC_MAX_AXES == EMCMOT_MAX_AXIS,
	      "axis array size differs from EMCMOT_MAX_AXIS");
static_assert(LINUXCNC_MAX_SPINDLES == EMCMOT_MAX_SPINDLES,
	      "spindle array size differs from EMCMOT_MAX_SPINDLES");
static_assert(LINUXCNC_MAX_DIO == EMCMOT_MAX_DIO,
	      "digital io array size differs from EMCMOT_MAX_DIO");
static_assert(LINUXCNC_MAX_AIO == EMCMOT_MAX_AIO,
	      "analog io array size differs from EMCMOT_MAX_AIO");
static_assert(LINUXCNC_MAX_MISC_ERROR == EMCMOT_MAX_MISC_ERROR,
	      "misc error array size differs from EMCMOT_MAX_MISC_ERROR");
static_assert(LINUXCNC_MAX_GCODES == ACTIVE_G_CODES,
	      "gcodes array size differs from ACTIVE_G_CODES");
static_assert(LINUXCNC_MAX_MCODES == ACTIVE_M_CODES,
	      "mcodes array size differs from ACTIVE_M_CODES");
static_assert(LINUXCNC_MAX_SETTINGS == ACTIVE_SETTINGS,
	      "settings array size differs from ACTIVE_SETTINGS");
static_assert(LINUXCNC_MAX_CALL_STACK == EMC_MAX_CALL_STACK,
	      "call stack size differs from EMC_MAX_CALL_STACK");
static_assert(LINUXCNC_MAX_SWITCHKINS_TYPES == SWITCHKINS_MAX_TYPES,
	      "switchkins array size differs from SWITCHKINS_MAX_TYPES");
static_assert(LINUXCNC_MAX_STATE_FIELDS == GM_FIELD_MAX_FIELDS,
	      "state tag field count differs from GM_FIELD_MAX_FIELDS");
static_assert(LINUXCNC_MAX_STATE_FLOAT_FIELDS == GM_FIELD_FLOAT_MAX_FIELDS,
	      "state tag float field count differs from GM_FIELD_FLOAT_MAX_FIELDS");
static_assert(LINUXCNC_TEXT_LEN == LINELEN,
	      "text length differs from LINELEN");
static_assert(LINUXCNC_MAX_TOOLS == CANON_POCKETS_MAX,
	      "tool table size differs from CANON_POCKETS_MAX");
static_assert(sizeof(((EmcCallFrame *)nullptr)->filename) == LINUXCNC_FILENAME_LEN,
	      "call frame filename length differs from the header");
static_assert(sizeof(((StateTag *)nullptr)->filename) == LINUXCNC_TAG_FILENAME_LEN,
	      "state tag filename length differs from the header");
static_assert(sizeof(((EmcCallFrame *)nullptr)->subname) == LINUXCNC_SUBNAME_LEN,
	      "call frame subroutine name length differs from the header");

namespace {

constexpr double kDefaultCommandTimeout = 5.0;
constexpr double kPollInterval = 0.01;

void set_error(struct linuxcnc_client *client, const char *format, ...);
void clear_error(struct linuxcnc_client *client);

}  /* namespace */

/* The definition of the opaque handle.  It is private to this file; the
   header only names it. */
struct linuxcnc_client {
    RCS_STAT_CHANNEL *stat = nullptr;
    RCS_CMD_CHANNEL *cmd = nullptr;
    NML *error = nullptr;
    EMC_STAT status;
    bool has_status = false;
    bool tool_mmap_tried = false;
    int serial = 0;
    double timeout = kDefaultCommandTimeout;
    std::string last_error;

    ~linuxcnc_client() {
	delete error;
	delete cmd;
	delete stat;
    }
};

namespace {

void set_error(struct linuxcnc_client *client, const char *format, ...) {
    if (client == nullptr) {
	return;
    }
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    client->last_error = buffer;
}

void clear_error(struct linuxcnc_client *client) {
    if (client != nullptr) {
	client->last_error.clear();
    }
}

/* Copy with a hard bound and a terminating NUL, so a long message in a
   fixed-size NML field cannot run past the caller's buffer. */
void copy_text(char *dst, size_t len, const char *src) {
    if (dst == nullptr || len == 0) {
	return;
    }
    if (src == nullptr) {
	src = "";
    }
    size_t n = std::strlen(src);
    if (n >= len) {
	n = len - 1;
    }
    std::memmove(dst, src, n);
    dst[n] = '\0';
}

void pose_to_c(struct linuxcnc_pose *out, const EmcPose &pose) {
    out->x = pose.tran.x;
    out->y = pose.tran.y;
    out->z = pose.tran.z;
    out->a = pose.a;
    out->b = pose.b;
    out->c = pose.c;
    out->u = pose.u;
    out->v = pose.v;
    out->w = pose.w;
}

void tool_to_c(struct linuxcnc_tool *out, const CANON_TOOL_TABLE &tool) {
    out->toolno = tool.toolno;
    out->pocketno = tool.pocketno;
    pose_to_c(&out->offset, tool.offset);
    out->diameter = tool.diameter;
    out->frontangle = tool.frontangle;
    out->backangle = tool.backangle;
    out->orientation = tool.orientation;
}

/* emcError carries the EMC_OPERATOR_* messages as well as the generic NML
   ones, so it needs both format tables. */
int error_format(NMLTYPE type, void *buffer, CMS *cms) {
    if (nmlErrorFormat(type, buffer, cms) == 1) {
	return 1;
    }
    return emcFormat(type, buffer, cms);
}

int refresh_status(struct linuxcnc_client *client) {
    if (client == nullptr || client->stat == nullptr) {
	set_error(client, "status channel is not open");
	return -1;
    }
    if (!client->stat->valid()) {
	set_error(client, "status channel is invalid");
	return -1;
    }
    if (client->stat->peek() == -1) {
	set_error(client, "status read failed");
	return -1;
    }
    if (client->stat->peek() == EMC_STAT_TYPE) {
	EMC_STAT *status =
	    static_cast<EMC_STAT *>(client->stat->get_address());
	if (status == nullptr) {
	    set_error(client, "status channel returned no data");
	    return -1;
	}
	/* A status message carries the size its writer was built with.  If
	   it differs from ours, task and this client are from different
	   builds and every member after the change would be misread. */
	const int64_t expected = static_cast<int64_t>(sizeof(EMC_STAT));
	if (status->size != expected) {
	    set_error(client,
		      "status message is %lld bytes, this client expects %lld; "
		      "task and client are from different builds",
		      static_cast<long long>(status->size),
		      static_cast<long long>(expected));
	    return -1;
	}
	std::memcpy(static_cast<void *>(&client->status),
		    static_cast<const void *>(status), sizeof(EMC_STAT));
	client->has_status = true;
    }
    return 0;
}

void fill_status(const struct linuxcnc_client *client,
		 struct linuxcnc_status *out) {
    /* Value-initialisation, not memset: the struct holds doubles, and
       zeroing bytes is flagged as a portability problem. */
    *out = linuxcnc_status{};
    out->size = static_cast<uint32_t>(sizeof(*out));
    out->version = LINUXCNC_STATUS_VERSION;
    if (!client->has_status) {
	return;
    }
    out->valid = 1;

    const EMC_STAT &s = client->status;

    out->command_status = static_cast<int>(s.status);
    out->last_command_serial = s.echo_serial_number;

    out->task_mode = static_cast<int>(s.task.mode);
    out->task_state = static_cast<int>(s.task.state);
    out->exec_state = static_cast<int>(s.task.execState);
    out->interp_state = static_cast<int>(s.task.interpState);
    out->call_level = s.task.callLevel;
    for (int i = 0; i < LINUXCNC_MAX_CALL_STACK; i++) {
	const EmcCallFrame &frame = s.task.callStack[i];
	copy_text(out->call_stack[i].filename,
		  sizeof(out->call_stack[i].filename), frame.filename);
	copy_text(out->call_stack[i].subname,
		  sizeof(out->call_stack[i].subname), frame.subname);
	out->call_stack[i].line = frame.line;
    }
    out->motion_line = s.task.motionLine;
    out->current_line = s.task.currentLine;
    out->read_line = s.task.readLine;
    out->optional_stop = s.task.optional_stop_state ? 1 : 0;
    out->block_delete = s.task.block_delete_state ? 1 : 0;
    out->input_timeout = s.task.input_timeout ? 1 : 0;
    out->task_paused = s.task.task_paused;
    out->interpreter_errcode = s.task.interpreter_errcode;
    out->program_units = static_cast<int>(s.task.programUnits);
    out->queued_mdi_commands = s.task.queuedMDIcommands;
    out->delay_left = s.task.delayLeft;
    copy_text(out->file, sizeof(out->file), s.task.file);
    copy_text(out->command, sizeof(out->command), s.task.command);
    copy_text(out->ini_filename, sizeof(out->ini_filename),
	      s.task.ini_filename);
    pose_to_c(&out->g5x_offset, s.task.g5x_offset);
    out->g5x_index = s.task.g5x_index;
    pose_to_c(&out->g92_offset, s.task.g92_offset);
    out->rotation_xy = s.task.rotation_xy;
    pose_to_c(&out->g68_offset, s.task.g68_offset);
    for (int i = 0; i < 9; i++) {
	out->g68_rotation[i] = s.task.g68_rotation[i];
    }
    out->g68_active = s.task.g68_active;
    pose_to_c(&out->tool_offset, s.task.toolOffset);
    for (int i = 0; i < LINUXCNC_MAX_GCODES; i++) {
	out->gcodes[i] = s.task.activeGCodes[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_MCODES; i++) {
	out->mcodes[i] = s.task.activeMCodes[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_SETTINGS; i++) {
	out->settings[i] = s.task.activeSettings[i];
    }
    out->taskbeat = s.task.taskbeat;

    const EMC_TRAJ_STAT &traj = s.motion.traj;
    out->linear_units = traj.linearUnits;
    out->angular_units = traj.angularUnits;
    out->cycle_time = traj.cycleTime;
    out->joints = traj.joints;
    out->spindles = traj.spindles;
    out->axis_mask = traj.axis_mask;
    out->motion_mode = static_cast<int>(traj.mode);
    out->enabled = traj.enabled ? 1 : 0;
    out->inpos = traj.inpos ? 1 : 0;
    out->queue = traj.queue;
    out->active_queue = traj.activeQueue;
    out->queue_full = traj.queueFull ? 1 : 0;
    out->motion_id = traj.id;
    out->paused = traj.paused ? 1 : 0;
    out->single_stepping = traj.single_stepping ? 1 : 0;
    out->feedrate = traj.scale;
    out->rapidrate = traj.rapid_scale;
    out->velocity = traj.velocity;
    out->acceleration = traj.acceleration;
    out->max_velocity = traj.maxVelocity;
    out->max_acceleration = traj.maxAcceleration;
    out->probe_tripped = traj.probe_tripped ? 1 : 0;
    out->probing = traj.probing ? 1 : 0;
    out->probe_val = traj.probeval;
    out->kinematics_type = traj.kinematics_type;
    out->motion_type = traj.motion_type;
    out->distance_to_go = traj.distance_to_go;
    out->current_vel = traj.current_vel;
    out->feed_override_enabled = traj.feed_override_enabled ? 1 : 0;
    out->adaptive_feed_enabled = traj.adaptive_feed_enabled ? 1 : 0;
    out->feed_hold_enabled = traj.feed_hold_enabled ? 1 : 0;
    pose_to_c(&out->position, traj.position);
    pose_to_c(&out->actual_position, traj.actualPosition);
    pose_to_c(&out->dtg, traj.dtg);
    pose_to_c(&out->probed_position, traj.probedPosition);
    out->switchkins_type = traj.switchkins_type;
    out->switchkins_seq = traj.switchkins_seq;
    out->switchkins_changed = traj.switchkins_changed ? 1 : 0;
    for (int i = 0; i < LINUXCNC_MAX_SWITCHKINS_TYPES; i++) {
	out->switchkins_flags[i] = traj.switchkins_flags[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_STATE_FLOAT_FIELDS; i++) {
	out->tag.fields_float[i] = traj.tag.fields_float[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_STATE_FIELDS; i++) {
	out->tag.fields[i] = traj.tag.fields[i];
    }
    out->tag.packed_flags = traj.tag.packed_flags;
    out->tag.flags = traj.tag.flags.to_ullong();
    copy_text(out->tag.filename, sizeof(out->tag.filename),
	      traj.tag.filename);

    out->num_extra_joints = s.motion.numExtraJoints;
    out->on_soft_limit = s.motion.on_soft_limit;
    out->external_offsets_applied = s.motion.external_offsets_applied;
    out->jogging_active = s.motion.jogging_active ? 1 : 0;
    out->homing_active = s.motion.homing_active ? 1 : 0;
    out->homing_runs = s.motion.homing_runs;
    out->heartbeat = s.motion.heartbeat;
    out->motion_debug = s.motion.debug;
    pose_to_c(&out->eoffset_pose, s.motion.eoffset_pose);
    for (int i = 0; i < LINUXCNC_MAX_DIO; i++) {
	out->din[i] = s.motion.synch_di[i];
	out->dout[i] = s.motion.synch_do[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_AIO; i++) {
	out->ain[i] = s.motion.analog_input[i];
	out->aout[i] = s.motion.analog_output[i];
    }
    for (int i = 0; i < LINUXCNC_MAX_MISC_ERROR; i++) {
	out->misc_error[i] = s.motion.misc_error[i];
    }

    for (int i = 0; i < LINUXCNC_MAX_JOINTS; i++) {
	const EMC_JOINT_STAT &joint = s.motion.joint[i];
	struct linuxcnc_joint &out_joint = out->joint[i];
	out_joint.joint = joint.joint;
	out_joint.joint_type = joint.jointType;
	out_joint.units = joint.units;
	out_joint.backlash = joint.backlash;
	out_joint.min_position_limit = joint.minPositionLimit;
	out_joint.max_position_limit = joint.maxPositionLimit;
	out_joint.max_ferror = joint.maxFerror;
	out_joint.min_ferror = joint.minFerror;
	out_joint.ferror_current = joint.ferrorCurrent;
	out_joint.ferror_high_mark = joint.ferrorHighMark;
	out_joint.output = joint.output;
	out_joint.input = joint.input;
	out_joint.velocity = joint.velocity;
	out_joint.inpos = joint.inpos;
	out_joint.homing = joint.homing;
	out_joint.homed = joint.homed;
	out_joint.fault = joint.fault;
	out_joint.enabled = joint.enabled;
	out_joint.min_soft_limit = joint.minSoftLimit;
	out_joint.max_soft_limit = joint.maxSoftLimit;
	out_joint.min_hard_limit = joint.minHardLimit;
	out_joint.max_hard_limit = joint.maxHardLimit;
	out_joint.override_limits = joint.overrideLimits;
    }

    for (int i = 0; i < LINUXCNC_MAX_AXES; i++) {
	const EMC_AXIS_STAT &axis = s.motion.axis[i];
	struct linuxcnc_axis &out_axis = out->axis[i];
	out_axis.axis = axis.axis;
	out_axis.min_position_limit = axis.minPositionLimit;
	out_axis.max_position_limit = axis.maxPositionLimit;
	out_axis.velocity = axis.velocity;
    }

    for (int i = 0; i < LINUXCNC_MAX_SPINDLES; i++) {
	const EMC_SPINDLE_STAT &spindle = s.motion.spindle[i];
	struct linuxcnc_spindle &out_spindle = out->spindle[i];
	out_spindle.speed = spindle.speed;
	out_spindle.spindle_scale = spindle.spindle_scale;
	out_spindle.css_maximum = spindle.css_maximum;
	out_spindle.css_factor = spindle.css_factor;
	out_spindle.state = spindle.state;
	out_spindle.direction = spindle.direction;
	out_spindle.brake = spindle.brake;
	out_spindle.increasing = spindle.increasing;
	out_spindle.enabled = spindle.enabled;
	out_spindle.orient_state = spindle.orient_state;
	out_spindle.orient_fault = spindle.orient_fault;
	out_spindle.spindle_override_enabled =
	    spindle.spindle_override_enabled ? 1 : 0;
	out_spindle.homed = spindle.homed ? 1 : 0;
    }

    out->tool_in_spindle = s.io.tool.toolInSpindle;
    out->pocket_prepped = s.io.tool.pocketPrepped;
    out->tool_from_pocket = s.io.tool.toolFromPocket;
    tool_to_c(&out->tool_table_current, s.io.tool.toolTableCurrent);
    out->mist = s.io.coolant.mist;
    out->flood = s.io.coolant.flood;
    out->estop = s.io.aux.estop;
    out->io_fault = s.io.fault;
    out->io_reason = s.io.reason;
    out->io_debug = s.io.debug;

    out->debug = s.debug;
}

double command_timeout(const struct linuxcnc_client *client,
		       double timeout) {
    if (timeout >= 0.0) {
	return timeout;
    }
    return client->timeout;
}

int wait_for_echo(struct linuxcnc_client *client, int serial,
		  double timeout_seconds) {
    const double deadline = linuxcnc::etime() + timeout_seconds;
    while (linuxcnc::etime() < deadline) {
	if (refresh_status(client) != 0) {
	    return -1;
	}
	if (client->has_status && client->status.echo_serial_number - serial >= 0) {
	    return 1;
	}
	linuxcnc::esleep(kPollInterval);
    }
    set_error(client, "timed out waiting for command %d to be taken", serial);
    return 0;
}

int wait_for_done(struct linuxcnc_client *client, int serial,
		  double timeout_seconds) {
    const double deadline = linuxcnc::etime() + timeout_seconds;
    while (linuxcnc::etime() < deadline) {
	if (refresh_status(client) != 0) {
	    return -1;
	}
	if (client->has_status) {
	    const int difference = client->status.echo_serial_number - serial;
	    if (difference > 0) {
		return 1;
	    }
	    if (difference == 0 &&
		    (client->status.status == RCS_STATUS::DONE ||
		     client->status.status == RCS_STATUS::ERROR)) {
		return 1;
	    }
	}
	linuxcnc::esleep(kPollInterval);
    }
    set_error(client, "timed out waiting for command %d to finish", serial);
    return 0;
}

template <typename Message>
int send_command(struct linuxcnc_client *client, Message &message) {
    if (client == nullptr || client->cmd == nullptr) {
	set_error(client, "command channel is not open");
	return -1;
    }
    if (!client->cmd->valid()) {
	set_error(client, "command channel is invalid");
	return -1;
    }

    clear_error(client);
    if (client->cmd->write(&message) != 0) {
	set_error(client, "command write failed");
	return -1;
    }
    client->serial = message.serial_number;
    return wait_for_echo(client, client->serial, client->timeout) == 1 ? 0 : -1;
}

bool copy_command_text(char *dst, size_t len, const char *src,
		       struct linuxcnc_client *client, const char *what) {
    if (src == nullptr) {
	set_error(client, "%s is null", what);
	return false;
    }
    const size_t source_len = std::strlen(src);
    if (source_len >= len) {
	set_error(client, "%s is too long (max %zu bytes)", what, len - 1);
	return false;
    }
    std::memcpy(dst, src, source_len + 1);
    return true;
}

}  /* namespace */

namespace {

std::string directory_of(const std::string &path) {
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos) {
	return ".";
    }
    if (slash == 0) {
	return "/";
    }
    return path.substr(0, slash);
}

bool is_absolute(const std::string &path) {
    return !path.empty() && path[0] == '/';
}

/* The machine's INI says where its transport lives.  That is a detail of
   this library; the header only names the INI. */
std::string transport_file_from_ini(const char *config_file,
				    struct linuxcnc_client *client) {
    std::string ini_path;
    if (config_file != nullptr && config_file[0] != '\0') {
	ini_path = config_file;
    } else if (const char *from_env = std::getenv("INI_FILE_NAME");
	       from_env != nullptr && from_env[0] != '\0') {
	ini_path = from_env;
    } else {
	/* No INI to read: the launcher's own answer, or the default. */
	if (const char *from_env = std::getenv("NMLFILE");
	    from_env != nullptr && from_env[0] != '\0') {
	    return from_env;
	}
	return DEFAULT_EMC_NMLFILE != nullptr ? DEFAULT_EMC_NMLFILE : "";
    }

    const linuxcnc::IniFile ini(ini_path);
    if (!ini) {
	set_error(client, "cannot read the INI file %s", ini_path.c_str());
	return "";
    }
    std::optional<std::string> transport = ini.findString("NML_FILE", "LINUXCNC");
    if (!transport) {
	transport = ini.findString("NML_FILE", "EMC");
    }
    if (!transport || transport->empty()) {
	return DEFAULT_EMC_NMLFILE != nullptr ? DEFAULT_EMC_NMLFILE : "";
    }
    if (is_absolute(*transport)) {
	return *transport;
    }
    /* The launcher runs from the INI directory, so a relative name is
       relative to the INI, not to this process. */
    return directory_of(ini_path) + "/" + *transport;
}

}  /* namespace */

extern "C" struct linuxcnc_client *linuxcnc_client_open(const char *config_file,
							char *error,
							size_t error_len) {
    struct linuxcnc_client *client = nullptr;
    try {
	client = new linuxcnc_client();

	const std::string path = transport_file_from_ini(config_file, client);
	if (path.empty()) {
	    if (client->last_error.empty()) {
		set_error(client, "no transport to reach the machine with");
	    }
	    copy_text(error, error_len, client->last_error.c_str());
	    delete client;
	    return nullptr;
	}

	client->stat =
	    new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", path.c_str());
	if (!client->stat->valid()) {
	    set_error(client, "cannot open emcStatus from %s", path.c_str());
	    copy_text(error, error_len, client->last_error.c_str());
	    delete client;
	    return nullptr;
	}

	client->cmd =
	    new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", path.c_str());
	if (!client->cmd->valid()) {
	    set_error(client, "cannot open emcCommand from %s", path.c_str());
	    copy_text(error, error_len, client->last_error.c_str());
	    delete client;
	    return nullptr;
	}

	client->error =
	    new NML(error_format, "emcError", "xemc", path.c_str());
	if (!client->error->valid()) {
	    set_error(client, "cannot open emcError from %s", path.c_str());
	    copy_text(error, error_len, client->last_error.c_str());
	    delete client;
	    return nullptr;
	}
    } catch (const std::exception &exception) {
	copy_text(error, error_len, exception.what());
	delete client;
	return nullptr;
    } catch (...) {
	copy_text(error, error_len, "unknown exception opening LinuxCNC");
	delete client;
	return nullptr;
    }

    copy_text(error, error_len, "");
    return client;
}

extern "C" void linuxcnc_client_close(struct linuxcnc_client *client) {
    delete client;
}

extern "C" const char *
linuxcnc_client_last_error(const struct linuxcnc_client *client) {
    if (client == nullptr) {
	return "";
    }
    return client->last_error.c_str();
}

extern "C" const char *linuxcnc_client_version(void) {
    return PACKAGE_VERSION;
}

extern "C" uint32_t linuxcnc_client_status_version(void) {
    return LINUXCNC_STATUS_VERSION;
}

extern "C" uint32_t linuxcnc_status_size(void) {
    return static_cast<uint32_t>(sizeof(struct linuxcnc_status));
}

extern "C" uint32_t linuxcnc_error_size(void) {
    return static_cast<uint32_t>(sizeof(struct linuxcnc_error));
}

extern "C" void linuxcnc_status_init(struct linuxcnc_status *status) {
    if (status == nullptr) {
	return;
    }
    *status = linuxcnc_status{};
    status->size = static_cast<uint32_t>(sizeof(*status));
    status->version = LINUXCNC_STATUS_VERSION;
}

extern "C" int
linuxcnc_client_set_timeout(struct linuxcnc_client *client, double seconds) {
    if (client == nullptr) {
	return -1;
    }
    client->timeout = seconds >= 0.0 ? seconds : kDefaultCommandTimeout;
    return 0;
}

extern "C" int linuxcnc_status_poll(struct linuxcnc_client *client,
				    struct linuxcnc_status *status) {
    if (client == nullptr) {
	return -1;
    }
    if (status == nullptr) {
	set_error(client, "status output is null");
	return -1;
    }
    if (status->size < sizeof(struct linuxcnc_status)) {
	set_error(client, "status buffer of %u bytes is smaller than %zu",
		  status->size, sizeof(struct linuxcnc_status));
	return -1;
    }
    clear_error(client);
    if (refresh_status(client) != 0) {
	return -1;
    }
    fill_status(client, status);
    return 0;
}

extern "C" void linuxcnc_error_init(struct linuxcnc_error *error) {
    if (error == nullptr) {
	return;
    }
    *error = linuxcnc_error{};
    error->size = static_cast<uint32_t>(sizeof(*error));
    error->version = LINUXCNC_ERROR_VERSION;
    error->severity = LINUXCNC_SEVERITY_UNKNOWN;
}

extern "C" int linuxcnc_error_poll(struct linuxcnc_client *client,
				   struct linuxcnc_error *error) {
    if (client == nullptr || client->error == nullptr) {
	set_error(client, "error channel is not open");
	return -1;
    }
    if (error == nullptr || error->size < sizeof(*error)) {
	set_error(client, "error buffer is null or too small");
	return -1;
    }
    if (!client->error->valid()) {
	set_error(client, "error channel is invalid");
	return -1;
    }
    clear_error(client);

    const NMLTYPE type = client->error->read();
    if (type == 0) {
	return 0;
    }
    if (type == static_cast<NMLTYPE>(-1)) {
	set_error(client, "error channel read failed");
	return -1;
    }

    void *address = client->error->get_address();
    if (address == nullptr) {
	set_error(client, "error channel returned no message");
	return -1;
    }

    /* The NML channel has no sequence number, no severity and no link back
       to the command that caused the message.  Leave those 0 rather than
       making something up; see the comment on struct linuxcnc_error. */
    error->version = LINUXCNC_ERROR_VERSION;
    error->severity = LINUXCNC_SEVERITY_UNKNOWN;
    error->command_serial = 0;
    error->sequence = 0;

    switch (type) {
    case EMC_OPERATOR_ERROR_TYPE:
	error->kind = LINUXCNC_ERROR_OPERATOR_ERROR;
	copy_text(error->text, sizeof(error->text),
		  static_cast<EMC_OPERATOR_ERROR *>(address)->error);
	break;
    case EMC_OPERATOR_TEXT_TYPE:
	error->kind = LINUXCNC_ERROR_OPERATOR_TEXT;
	copy_text(error->text, sizeof(error->text),
		  static_cast<EMC_OPERATOR_TEXT *>(address)->text);
	break;
    case EMC_OPERATOR_DISPLAY_TYPE:
	error->kind = LINUXCNC_ERROR_OPERATOR_DISPLAY;
	copy_text(error->text, sizeof(error->text),
		  static_cast<EMC_OPERATOR_DISPLAY *>(address)->display);
	break;
    case NML_ERROR_TYPE:
	error->kind = LINUXCNC_ERROR_ERROR;
	copy_text(error->text, sizeof(error->text),
		  static_cast<NML_ERROR *>(address)->error);
	break;
    case NML_TEXT_TYPE:
	error->kind = LINUXCNC_ERROR_TEXT;
	copy_text(error->text, sizeof(error->text),
		  static_cast<NML_TEXT *>(address)->text);
	break;
    case NML_DISPLAY_TYPE:
	error->kind = LINUXCNC_ERROR_DISPLAY;
	copy_text(error->text, sizeof(error->text),
		  static_cast<NML_DISPLAY *>(address)->display);
	break;
    default:
	error->kind = LINUXCNC_ERROR_OTHER;
	snprintf(error->text, sizeof(error->text),
		 "unrecognized error message %d", static_cast<int>(type));
	break;
    }
    return 1;
}

extern "C" int linuxcnc_wait_complete(struct linuxcnc_client *client,
				      double timeout) {
    if (client == nullptr || client->serial <= 0) {
	set_error(client, "no command is waiting");
	return -1;
    }
    clear_error(client);
    if (wait_for_done(client, client->serial, command_timeout(client, timeout))
	< 0) {
	return -1;
    }
    return wait_for_done(client, client->serial,
			 command_timeout(client, timeout));
}

extern "C" int
linuxcnc_command_serial(const struct linuxcnc_client *client) {
    return client == nullptr ? 0 : client->serial;
}

extern "C" int linuxcnc_tool_table(struct linuxcnc_client *client,
				   struct linuxcnc_tool *tools,
				   size_t capacity, size_t *count) {
    if (client == nullptr || count == nullptr) {
	set_error(client, "invalid tool table output");
	return -1;
    }
    clear_error(client);
    if (!client->tool_mmap_tried) {
	client->tool_mmap_tried = true;
	tool_mmap_user();
    }

    const int last = tooldata_last_index_get() + 1;
    const size_t total = last > 0 ? static_cast<size_t>(last) : 0;
    *count = total;
    if (tools == nullptr || capacity == 0) {
	return 0;
    }
    if (total > capacity) {
	set_error(client, "tool table capacity %zu is smaller than %zu",
		  capacity, total);
	return -1;
    }
    for (size_t i = 0; i < total; i++) {
	CANON_TOOL_TABLE tool = tooldata_entry_init();
	if (tooldata_get(&tool, static_cast<int>(i)) != IDX_OK) {
	    set_error(client, "cannot read tool table entry %zu", i);
	    return -1;
	}
	tool_to_c(&tools[i], tool);
    }
    return 0;
}

extern "C" int linuxcnc_command_debug(struct linuxcnc_client *client,
				      int level) {
    EMC_SET_DEBUG message;
    message.debug = level;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_teleop_enable(struct linuxcnc_client *client, int enable) {
    EMC_TRAJ_SET_TELEOP_ENABLE message;
    message.enable = enable != 0;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_traj_mode(struct linuxcnc_client *client,
					  int mode) {
    EMC_TRAJ_SET_MODE message;
    message.mode = static_cast<EMC_TRAJ_MODE>(mode);
    return send_command(client, message);
}

extern "C" int linuxcnc_command_mode(struct linuxcnc_client *client,
				     int mode) {
    switch (mode) {
    case LINUXCNC_MODE_MANUAL:
    case LINUXCNC_MODE_AUTO:
    case LINUXCNC_MODE_MDI:
	break;
    default:
	set_error(client, "invalid task mode %d", mode);
	return -1;
    }
    EMC_TASK_SET_MODE message;
    message.mode = static_cast<EMC_TASK_MODE>(mode);
    return send_command(client, message);
}

extern "C" int linuxcnc_command_state(struct linuxcnc_client *client,
				      int state) {
    switch (state) {
    case LINUXCNC_STATE_ESTOP:
    case LINUXCNC_STATE_ESTOP_RESET:
    case LINUXCNC_STATE_OFF:
    case LINUXCNC_STATE_ON:
	break;
    default:
	set_error(client, "invalid task state %d", state);
	return -1;
    }
    EMC_TASK_SET_STATE message;
    message.state = static_cast<EMC_TASK_STATE>(state);
    return send_command(client, message);
}

extern "C" int linuxcnc_command_mdi(struct linuxcnc_client *client,
				    const char *block) {
    EMC_TASK_PLAN_EXECUTE message;
    if (!copy_command_text(message.command, sizeof(message.command), block,
			   client, "MDI block")) {
	return -1;
    }
    return send_command(client, message);
}

extern "C" int linuxcnc_command_program_open(struct linuxcnc_client *client,
					     const char *path) {
    EMC_TASK_PLAN_CLOSE close_message;
    if (send_command(client, close_message) != 0) {
	return -1;
    }
    EMC_TASK_PLAN_OPEN open_message;
    if (!copy_command_text(open_message.file, sizeof(open_message.file), path,
			   client, "program path")) {
	return -1;
    }
    return send_command(client, open_message);
}

extern "C" int linuxcnc_command_program_close(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_CLOSE message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_run(struct linuxcnc_client *client,
					 int line) {
    EMC_TASK_PLAN_RUN message;
    message.line = line;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_pause(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_PAUSE message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_resume(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_RESUME message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_step(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_STEP message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_reverse(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_REVERSE message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_auto_forward(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_FORWARD message;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_reset_interpreter(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_INIT message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_abort(struct linuxcnc_client *client) {
    EMC_TASK_ABORT message;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_task_plan_synch(struct linuxcnc_client *client) {
    EMC_TASK_PLAN_SYNCH message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_feedrate(struct linuxcnc_client *client,
					 double scale) {
    EMC_TRAJ_SET_SCALE message;
    message.scale = scale;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_rapidrate(struct linuxcnc_client *client,
					  double scale) {
    EMC_TRAJ_SET_RAPID_SCALE message;
    message.scale = scale;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_maxvel(struct linuxcnc_client *client,
				       double velocity) {
    EMC_TRAJ_SET_MAX_VELOCITY message;
    message.velocity = velocity;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_spindle_override(struct linuxcnc_client *client,
				  double scale, int spindle) {
    EMC_TRAJ_SET_SPINDLE_SCALE message;
    message.spindle = spindle;
    message.scale = scale;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_spindle_on(struct linuxcnc_client *client,
					   double speed, int spindle,
					   int wait_for_speed) {
    EMC_SPINDLE_ON message;
    message.spindle = spindle;
    message.speed = speed;
    message.wait_for_spindle_at_speed = wait_for_speed;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_spindle_off(struct linuxcnc_client *client,
					    int spindle) {
    EMC_SPINDLE_OFF message;
    message.spindle = spindle;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_spindle_increase(struct linuxcnc_client *client,
				  int spindle) {
    EMC_SPINDLE_INCREASE message;
    message.spindle = spindle;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_spindle_decrease(struct linuxcnc_client *client,
				  int spindle) {
    EMC_SPINDLE_DECREASE message;
    message.spindle = spindle;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_spindle_constant(struct linuxcnc_client *client,
				  int spindle) {
    EMC_SPINDLE_CONSTANT message;
    message.spindle = spindle;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_spindle_brake(struct linuxcnc_client *client,
					      int spindle, int engage) {
    if (engage != 0) {
	EMC_SPINDLE_BRAKE_ENGAGE message;
	message.spindle = spindle;
	return send_command(client, message);
    }
    EMC_SPINDLE_BRAKE_RELEASE message;
    message.spindle = spindle;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_tool_offset(struct linuxcnc_client *client, int toolno,
			     double z, double x, double diameter,
			     double frontangle, double backangle,
			     int orientation) {
    EMC_TOOL_SET_OFFSET message;
    message.toolno = toolno;
    message.offset.tran.z = z;
    message.offset.tran.x = x;
    message.diameter = diameter;
    message.frontangle = frontangle;
    message.backangle = backangle;
    message.orientation = orientation;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_load_tool_table(struct linuxcnc_client *client,
				 const char *file) {
    EMC_TOOL_LOAD_TOOL_TABLE message;
    if (!copy_command_text(message.file, sizeof(message.file),
			   file == nullptr ? "" : file, client,
			   "tool table path")) {
	return -1;
    }
    return send_command(client, message);
}

extern "C" int linuxcnc_command_mist(struct linuxcnc_client *client, int on) {
    if (on != 0) {
	EMC_COOLANT_MIST_ON message;
	return send_command(client, message);
    }
    EMC_COOLANT_MIST_OFF message;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_flood(struct linuxcnc_client *client, int on) {
    if (on != 0) {
	EMC_COOLANT_FLOOD_ON message;
	return send_command(client, message);
    }
    EMC_COOLANT_FLOOD_OFF message;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_override_limits(struct linuxcnc_client *client, int joint) {
    EMC_JOINT_OVERRIDE_LIMITS message;
    message.joint = joint;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_home(struct linuxcnc_client *client,
				     int joint) {
    EMC_JOINT_HOME message;
    message.joint = joint;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_unhome(struct linuxcnc_client *client,
				       int joint) {
    EMC_JOINT_UNHOME message;
    message.joint = joint;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_jog_stop(struct linuxcnc_client *client,
					 int jog_mode, int index) {
    EMC_JOG_STOP message;
    message.jjogmode = jog_mode;
    message.joint_or_axis = index;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_jog_continuous(struct linuxcnc_client *client, int jog_mode,
				int index, double velocity) {
    EMC_JOG_CONT message;
    message.jjogmode = jog_mode;
    message.joint_or_axis = index;
    message.vel = velocity;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_jog_increment(struct linuxcnc_client *client, int jog_mode,
			       int index, double velocity, double increment) {
    EMC_JOG_INCR message;
    message.jjogmode = jog_mode;
    message.joint_or_axis = index;
    message.vel = velocity;
    message.incr = increment;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_optional_stop(struct linuxcnc_client *client, int on) {
    EMC_TASK_PLAN_SET_OPTIONAL_STOP message;
    message.state = on != 0;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_block_delete(struct linuxcnc_client *client, int on) {
    EMC_TASK_PLAN_SET_BLOCK_DELETE message;
    message.state = on != 0;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_set_min_limit(struct linuxcnc_client *client,
					      int joint, double limit) {
    EMC_JOINT_SET_MIN_POSITION_LIMIT message;
    message.joint = joint;
    message.limit = limit;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_set_max_limit(struct linuxcnc_client *client,
					      int joint, double limit) {
    EMC_JOINT_SET_MAX_POSITION_LIMIT message;
    message.joint = joint;
    message.limit = limit;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_feed_override(struct linuxcnc_client *client, int on) {
    EMC_TRAJ_SET_FO_ENABLE message;
    message.mode = on != 0 ? 1 : 0;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_spindle_override(struct linuxcnc_client *client, int on,
				      int spindle) {
    EMC_TRAJ_SET_SO_ENABLE message;
    message.spindle = spindle;
    message.mode = on != 0 ? 1 : 0;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_feed_hold(struct linuxcnc_client *client, int on) {
    EMC_TRAJ_SET_FH_ENABLE message;
    message.mode = on != 0 ? 1 : 0;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_adaptive_feed(struct linuxcnc_client *client, int on) {
    EMC_MOTION_ADAPTIVE message;
    message.status = on != 0 ? 1 : 0;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_digital_output(struct linuxcnc_client *client,
				    int index, int on) {
    EMC_MOTION_SET_DOUT message;
    message.index = static_cast<unsigned char>(index);
    message.start = on != 0 ? 1 : 0;
    message.now = 1;
    return send_command(client, message);
}

extern "C" int
linuxcnc_command_set_analog_output(struct linuxcnc_client *client, int index,
				   double value) {
    EMC_MOTION_SET_AOUT message;
    message.index = static_cast<unsigned char>(index);
    message.start = value;
    message.now = 1;
    return send_command(client, message);
}

extern "C" int linuxcnc_command_error_msg(struct linuxcnc_client *client,
					  const char *text) {
    EMC_OPERATOR_ERROR message;
    if (!copy_command_text(message.error, sizeof(message.error), text, client,
			   "operator error")) {
	return -1;
    }
    return send_command(client, message);
}

extern "C" int linuxcnc_command_text_msg(struct linuxcnc_client *client,
					 const char *text) {
    EMC_OPERATOR_TEXT message;
    if (!copy_command_text(message.text, sizeof(message.text), text, client,
			   "operator text")) {
	return -1;
    }
    return send_command(client, message);
}

extern "C" int linuxcnc_command_display_msg(struct linuxcnc_client *client,
					    const char *text) {
    EMC_OPERATOR_DISPLAY message;
    if (!copy_command_text(message.display, sizeof(message.display), text,
			   client, "operator display")) {
	return -1;
    }
    return send_command(client, message);
}
