// Protocol bridge prototype: serves the client protocol in front of task.
//
// It reads the NML status and error channels and writes commands, exactly as
// the Python module and the C client library do today, but what it offers to
// clients is the schema in emc.fbs over a pluggable byte stream.  This is the
// seam the NML replacement can move behind without changing clients.
//
// It is a prototype: status carries the fields a UI needs first, and the
// transport is a unix socket or loopback TCP.  A WebSocket adapter plugs into
// the same frame layer; nothing above frame.hh changes when one is added.
//
// Build (from src/): see the recipe in the surrounding documentation, or run
// tests/protocol/test.sh, which builds it with the tree's include paths.

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "nml_intf/emc.hh"
#include "nml_intf/emc_nml.hh"
#include "nml_intf/emccfg.h"
#include "libnml/nml/nml.hh"
#include "libnml/nml/nml_oi.hh"
#include "emc/timeutil.hh"

#include <emc/protocol/emc_generated.h>
#include "frame.hh"

namespace proto = linuxcnc::protocol;

namespace {

constexpr size_t kRingSize = 1024;
constexpr double kCommandTimeout = 5.0;
constexpr double kPollInterval = 0.005;
constexpr int kMaxClients = 8;
constexpr int kSchemaVersion = 2;
constexpr int kMinPollMs = 10;

volatile sig_atomic_t stop_requested = 0;

void on_signal(int) {
    stop_requested = 1;
}

struct ErrorRecord {
    uint64_t sequence;
    proto::OperatorKind kind;
    proto::Severity severity;
    proto::ErrorSource source;
    int command_serial;
    std::string text;
    double timestamp;
};

struct Client {
    int fd = -1;
    std::vector<uint8_t> pending;
    bool hello_done = false;
    uint64_t cursor = 0;
    std::string name;
    bool observe = false;
    bool stop_on_disconnect = true;
    int poll_ms = 0;
    double next_status = 0.0;

    ~Client() {
	if (fd >= 0) {
	    ::close(fd);
	}
    }
};

RCS_STAT_CHANNEL *stat_channel = nullptr;
RCS_CMD_CHANNEL *cmd_channel = nullptr;
NML *error_channel = nullptr;

EMC_STAT status;
bool have_status = false;

/* Operator state lives here because this process is standing in for task.
   A UI never keeps its own copy of these; it reads them back from Status. */
struct OperatorState {
    int selected_joint = -1;
    int selected_axis = -1;
    double jog_increment = 0.0;
    double jog_rate = 0.0;
    std::string pending_mdi;
    std::string program_file;
    std::string focus_client;
};

OperatorState operator_state;

/* View state: opaque per client name, stored and returned, never read. */
std::map<std::string, std::vector<uint8_t>> views;

struct PendingRequest {
    std::string client_name;     // who asked
    int client_serial = 0;
    bool is_choice = false;
    bool ask_focused = false;    // send the question to the focus holder
    double deadline = 0.0;
    std::string block;
    std::string expect_program;
};

std::map<uint32_t, PendingRequest> pending_requests;
uint32_t next_request_id = 1;
std::deque<ErrorRecord> ring;
uint64_t last_sequence = 0;

void copy_text(char *dst, size_t len, const char *src) {
    if (dst == nullptr || len == 0) {
	return;
    }
    const char *from = src != nullptr ? src : "";
    size_t n = std::strlen(from);
    if (n >= len) {
	n = len - 1;
    }
    std::memcpy(dst, from, n);
    dst[n] = '\0';
}

void fail(const char *what) {
    std::fprintf(stderr, "protocol-bridge: %s\n", what);
    std::exit(1);
}

/* emcError carries the operator messages as well as the generic NML ones. */
int error_format(NMLTYPE type, void *buffer, CMS *cms) {
    if (nmlErrorFormat(type, buffer, cms) == 1) {
	return 1;
    }
    return emcFormat(type, buffer, cms);
}

bool open_channels(const char *nml_file) {
    const char *path = (nml_file != nullptr && nml_file[0] != '\0')
			   ? nml_file
			   : DEFAULT_EMC_NMLFILE;
    if (path == nullptr || path[0] == '\0') {
	return false;
    }
    stat_channel = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", path);
    if (!stat_channel->valid()) {
	return false;
    }
    cmd_channel = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", path);
    if (!cmd_channel->valid()) {
	return false;
    }
    error_channel = new NML(error_format, "emcError", "xemc", path);
    return error_channel->valid();
}

bool refresh_status() {
    if (stat_channel == nullptr || !stat_channel->valid()) {
	return false;
    }
    if (stat_channel->peek() != EMC_STAT_TYPE) {
	return false;
    }
    EMC_STAT *incoming = static_cast<EMC_STAT *>(stat_channel->get_address());
    if (incoming == nullptr) {
	return false;
    }
    std::memcpy(static_cast<void *>(&status), static_cast<const void *>(incoming),
		sizeof(EMC_STAT));
    have_status = true;
    return true;
}

void push_error(proto::OperatorKind kind, proto::Severity severity,
		proto::ErrorSource source, int command_serial,
		const std::string &text) {
    ErrorRecord rec;
    rec.sequence = ++last_sequence;
    rec.kind = kind;
    rec.severity = severity;
    rec.source = source;
    rec.command_serial = command_serial;
    rec.text = text;
    rec.timestamp = linuxcnc::wall_etime();
    ring.push_back(rec);
    while (ring.size() > kRingSize) {
	ring.pop_front();
    }
}

/* Everything the error channel has, one sequence number each.  This runs on
   the bridge, so every client reads the same stream through its own cursor
   instead of racing for one destructively-read queue. */
void poll_errors(int command_serial) {
    if (error_channel == nullptr || !error_channel->valid()) {
	return;
    }
    for (;;) {
	const NMLTYPE type = error_channel->read();
	if (type == 0 || type == static_cast<NMLTYPE>(-1)) {
	    return;
	}
	void *address = error_channel->get_address();
	char text[512] = {0};
	if (address == nullptr) {
	    return;
	}
	proto::OperatorKind kind = proto::OperatorKind_ERROR;
	proto::Severity severity = proto::Severity_INFO;
	proto::ErrorSource source = proto::ErrorSource_TASK;
	switch (type) {
	case EMC_OPERATOR_ERROR_TYPE:
	    kind = proto::OperatorKind_ERROR;
	    severity = proto::Severity_ERROR;
	    source = proto::ErrorSource_OPERATOR;
	    std::strncpy(text, static_cast<EMC_OPERATOR_ERROR *>(address)->error,
			 sizeof(text) - 1);
	    break;
	case EMC_OPERATOR_TEXT_TYPE:
	    kind = proto::OperatorKind_TEXT;
	    source = proto::ErrorSource_OPERATOR;
	    std::strncpy(text, static_cast<EMC_OPERATOR_TEXT *>(address)->text,
			 sizeof(text) - 1);
	    break;
	case EMC_OPERATOR_DISPLAY_TYPE:
	    kind = proto::OperatorKind_DISPLAY;
	    source = proto::ErrorSource_OPERATOR;
	    std::strncpy(text, static_cast<EMC_OPERATOR_DISPLAY *>(address)->display,
			 sizeof(text) - 1);
	    break;
	case NML_ERROR_TYPE:
	    severity = proto::Severity_ERROR;
	    std::strncpy(text, static_cast<NML_ERROR *>(address)->error,
			 sizeof(text) - 1);
	    break;
	case NML_TEXT_TYPE:
	    std::strncpy(text, static_cast<NML_TEXT *>(address)->text,
			 sizeof(text) - 1);
	    break;
	case NML_DISPLAY_TYPE:
	    std::strncpy(text, static_cast<NML_DISPLAY *>(address)->display,
			 sizeof(text) - 1);
	    break;
	default:
	    std::snprintf(text, sizeof(text), "unhandled error message %d",
			  static_cast<int>(type));
	    break;
	}
	push_error(kind, severity, source, command_serial, text);
    }
}

flatbuffers::Offset<proto::Pose> make_pose(flatbuffers::FlatBufferBuilder &fbb,
					   const EmcPose &pose) {
    proto::PoseBuilder builder(fbb);
    builder.add_x(pose.tran.x);
    builder.add_y(pose.tran.y);
    builder.add_z(pose.tran.z);
    builder.add_a(pose.a);
    builder.add_b(pose.b);
    builder.add_c(pose.c);
    builder.add_u(pose.u);
    builder.add_v(pose.v);
    builder.add_w(pose.w);
    return builder.Finish();
}

flatbuffers::Offset<proto::Status> make_status(flatbuffers::FlatBufferBuilder &fbb) {
    const EMC_STAT &s = status;

    /* Everything nested is built before the Status table opens: creating a
       table inside another open table trips flatbuffers' nesting check. */
    const auto position = make_pose(fbb, s.motion.traj.position);
    const auto actual_position = make_pose(fbb, s.motion.traj.actualPosition);
    const auto task_file = fbb.CreateString(s.task.file);
    const auto pending_mdi = fbb.CreateString(operator_state.pending_mdi);
    const auto program_file = fbb.CreateString(operator_state.program_file);
    const auto focus_client = fbb.CreateString(operator_state.focus_client);
    const auto op = proto::CreateOperatorState(
	fbb, operator_state.selected_joint, operator_state.selected_axis,
	operator_state.jog_increment, operator_state.jog_rate, pending_mdi,
	program_file, focus_client);

    std::vector<flatbuffers::Offset<proto::Joint>> joints;
    const int joint_count =
	std::max(0, std::min(s.motion.traj.joints, EMCMOT_MAX_JOINTS));
    for (int i = 0; i < joint_count; i++) {
	const EMC_JOINT_STAT &joint = s.motion.joint[i];
	proto::JointBuilder builder(fbb);
	builder.add_joint(joint.joint);
	builder.add_joint_type(static_cast<proto::JointType>(joint.jointType));
	builder.add_homed(joint.homed != 0);
	builder.add_enabled(joint.enabled != 0);
	builder.add_fault(joint.fault != 0);
	builder.add_position(joint.input);
	builder.add_velocity(joint.velocity);
	builder.add_min_position_limit(joint.minPositionLimit);
	builder.add_max_position_limit(joint.maxPositionLimit);
	joints.push_back(builder.Finish());
    }
    const auto joint_vector = fbb.CreateVector(joints);

    std::vector<flatbuffers::Offset<proto::Axis>> axes;
    for (int i = 0; i < EMCMOT_MAX_AXIS; i++) {
	if ((s.motion.traj.axis_mask & (1 << i)) == 0) {
	    continue;
	}
	const EMC_AXIS_STAT &axis = s.motion.axis[i];
	proto::AxisBuilder builder(fbb);
	builder.add_axis(axis.axis);
	builder.add_velocity(axis.velocity);
	builder.add_min_position_limit(axis.minPositionLimit);
	builder.add_max_position_limit(axis.maxPositionLimit);
	axes.push_back(builder.Finish());
    }
    const auto axis_vector = fbb.CreateVector(axes);

    proto::StatusBuilder builder(fbb);
    builder.add_valid(have_status);
    builder.add_task_mode(static_cast<proto::TaskMode>(static_cast<int>(s.task.mode)));
    builder.add_task_state(static_cast<proto::TaskState>(static_cast<int>(s.task.state)));
    builder.add_exec_state(static_cast<proto::ExecState>(static_cast<int>(s.task.execState)));
    builder.add_interp_state(
	static_cast<proto::InterpState>(static_cast<int>(s.task.interpState)));
    builder.add_traj_mode(static_cast<proto::TrajMode>(static_cast<int>(s.motion.traj.mode)));
    builder.add_units(
	static_cast<proto::ProgramUnits>(static_cast<int>(s.task.programUnits)));
    builder.add_last_command_serial(s.echo_serial_number);
    builder.add_command_status(static_cast<proto::CommandStatus>(
	static_cast<int>(s.status)));
    builder.add_task_file(task_file);
    builder.add_task_line(s.task.currentLine);
    builder.add_position(position);
    builder.add_actual_position(actual_position);
    builder.add_linear_units(s.motion.traj.linearUnits);
    builder.add_feedrate(s.motion.traj.scale);
    builder.add_rapidrate(s.motion.traj.rapid_scale);
    builder.add_joints(joint_vector);
    builder.add_axes(axis_vector);
    builder.add_operator_(op);
    builder.add_heartbeat(s.motion.heartbeat);
    builder.add_timestamp(linuxcnc::wall_etime());
    return builder.Finish();
}

bool send_envelope(int fd, flatbuffers::FlatBufferBuilder &fbb,
		   flatbuffers::Offset<proto::Envelope> envelope) {
    fbb.Finish(envelope, proto::EnvelopeIdentifier());
    return proto::frame_send(fd, fbb.GetBufferPointer(), fbb.GetSize());
}

bool send_hello(int fd) {
    flatbuffers::FlatBufferBuilder fbb;
    const auto name = fbb.CreateString("protocol-bridge");
    proto::VersionBuilder version(fbb);
    version.add_protocol_major(1);
    version.add_protocol_minor(0);
    version.add_schema_version(2);
    const auto version_offset = version.Finish();
    proto::HelloBuilder hello(fbb);
    hello.add_version(version_offset);
    hello.add_client_name(name);
    hello.add_last_error_sequence(static_cast<int64_t>(last_sequence));
    hello.add_joints(static_cast<uint8_t>(have_status
					      ? std::min(status.motion.traj.joints,
							 EMCMOT_MAX_JOINTS)
					      : 0));
    hello.add_spindles(static_cast<uint8_t>(have_status
						? std::min(status.motion.traj.spindles,
							   EMCMOT_MAX_SPINDLES)
						: 0));
    int axis_count = 0;
    if (have_status) {
	for (int i = 0; i < EMCMOT_MAX_AXIS; i++) {
	    if ((status.motion.traj.axis_mask & (1 << i)) != 0) {
		axis_count++;
	    }
	}
    }
    hello.add_axes(static_cast<uint8_t>(axis_count));
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 2, proto::MessageKind_HELLO,
					       hello.Finish(), 0, 0, 0, 0, 0, 0));
}

bool send_error(int fd, const ErrorRecord &rec) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(rec.text);
    proto::ErrorBuilder error(fbb);
    error.add_sequence(rec.sequence);
    error.add_kind(rec.kind);
    error.add_severity(rec.severity);
    error.add_source(rec.source);
    error.add_command_serial(rec.command_serial);
    error.add_text(text);
    error.add_timestamp(rec.timestamp);
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 2, proto::MessageKind_ERROR, 0,
					       0, error.Finish(), 0, 0, 0, 0));
}

bool send_ack(int fd, int command_serial, proto::AckState state,
	      uint64_t error_sequence = 0, int choice = -1) {
    flatbuffers::FlatBufferBuilder fbb;
    proto::AckBuilder ack(fbb);
    ack.add_command_serial(command_serial);
    ack.add_ok(state == proto::AckState_RECEIVED ||
	       state == proto::AckState_DONE);
    ack.add_state(state);
    ack.add_choice(choice);
    ack.add_error_sequence(error_sequence);
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 2, proto::MessageKind_ACK, 0,
					       0, 0, 0, ack.Finish(), 0, 0));
}

bool send_view(int fd, const std::vector<uint8_t> &data) {
    flatbuffers::FlatBufferBuilder fbb;
    const auto blob = fbb.CreateVector(data);
    return send_envelope(
	fd, fbb,
	proto::CreateEnvelope(fbb, 2, proto::MessageKind_VIEW, 0, 0, 0, 0, 0,
			      proto::CreateView(fbb, blob), 0));
}

bool send_request(int fd, uint32_t id, const std::string &text,
		  uint32_t timeout_ms,
		  const std::vector<std::string> &options = {}) {
    flatbuffers::FlatBufferBuilder fbb;
    const auto reason = fbb.CreateString(text);
    std::vector<flatbuffers::Offset<flatbuffers::String>> strings;
    for (const std::string &option : options) {
	strings.push_back(fbb.CreateString(option));
    }
    const auto option_vector = fbb.CreateVector(strings);
    return send_envelope(
	fd, fbb,
	proto::CreateEnvelope(
	    fbb, 2, proto::MessageKind_REQUEST, 0, 0, 0, 0, 0, 0,
	    proto::CreateRequest(fbb, id,
				 options.empty() ? proto::RequestKind_CONFIRM
						 : proto::RequestKind_CHOICE,
				 reason, option_vector, false, timeout_ms)));
}

bool send_status(int fd) {
    if (!refresh_status()) {
	return true;	// nothing new to say yet is not an error
    }
    flatbuffers::FlatBufferBuilder fbb;
    return send_envelope(
	fd, fbb,
	proto::CreateEnvelope(fbb, 2, proto::MessageKind_STATUS, 0,
			      make_status(fbb), 0, 0, 0, 0, 0));
}

/* Write one command and wait until task has taken it.  Returns the serial
   task echoed, or -1.  The error sequence of the last message seen while
   waiting is reported back so the ack can point at it. */
template <typename Message>
/* Hand one command to task and report its life: `received` once task has it,
   then `done` or `error` when it settles.  The client no longer has to guess
   the difference by polling the echo counter. */
int issue_command(int fd, int client_serial, Message &message,
		  uint64_t *error_sequence) {
    if (cmd_channel == nullptr || !cmd_channel->valid()) {
	send_ack(fd, client_serial, proto::AckState_REFUSED);
	return -1;
    }
    if (cmd_channel->write(&message) != 0) {
	send_ack(fd, client_serial, proto::AckState_REFUSED);
	return -1;
    }
    const int serial = message.serial_number;
    send_ack(fd, client_serial, proto::AckState_RECEIVED);
    const double deadline = linuxcnc::etime() + kCommandTimeout;
    while (linuxcnc::etime() < deadline) {
	refresh_status();
	poll_errors(serial);
	if (have_status && status.echo_serial_number - serial >= 0) {
	    if (!ring.empty() && ring.back().command_serial == serial) {
		*error_sequence = ring.back().sequence;
		send_ack(fd, client_serial, proto::AckState_ERROR,
			 *error_sequence);
	    } else {
		send_ack(fd, client_serial, proto::AckState_DONE);
	    }
	    return serial;
	}
	linuxcnc::esleep(kPollInterval);
    }
    send_ack(fd, client_serial, proto::AckState_ERROR);
    return -1;
}

Client *find_client(std::vector<Client *> &clients, const std::string &name) {
    for (Client *candidate : clients) {
	if (candidate->name == name) {
	    return candidate;
	}
    }
    return nullptr;
}

void handle_command(Client &client, const proto::Command *command,
		    std::vector<Client *> &clients) {
    const int client_serial = command == nullptr ? 0 : command->client_serial();
    uint64_t error_sequence = 0;

    if (command == nullptr || command->body_type() == proto::CommandBody_NONE) {
	send_ack(client.fd, client_serial, proto::AckState_REFUSED);
	return;
    }

    switch (command->body_type()) {
    case proto::CommandBody_SetMode: {
	EMC_TASK_SET_MODE message;
	message.mode = static_cast<EMC_TASK_MODE>(command->body_as_SetMode()->mode());
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_SetState: {
	EMC_TASK_SET_STATE message;
	message.state =
	    static_cast<EMC_TASK_STATE>(command->body_as_SetState()->state());
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_SetJog: {
	operator_state.jog_increment = command->body_as_SetJog()->increment();
	operator_state.jog_rate = command->body_as_SetJog()->rate();
	send_ack(client.fd, client_serial, proto::AckState_DONE);
	return;
    }
    case proto::CommandBody_SelectProgram: {
	const auto *path = command->body_as_SelectProgram()->path();
	operator_state.program_file = path != nullptr ? path->str() : "";
	send_ack(client.fd, client_serial, proto::AckState_DONE);
	return;
    }
    case proto::CommandBody_SetView: {
	const auto *data = command->body_as_SetView()->data();
	std::vector<uint8_t> blob;
	if (data != nullptr) {
	    blob.assign(data->begin(), data->end());
	}
	views[client.name] = std::move(blob);
	send_ack(client.fd, client_serial, proto::AckState_DONE);
	return;
    }
    case proto::CommandBody_Answer: {
	const uint32_t id = command->body_as_Answer()->id();
	const bool accept = command->body_as_Answer()->accept();
	auto it = pending_requests.find(id);
	if (it == pending_requests.end() ||
	    it->second.client_name != client.name) {
	    send_ack(client.fd, client_serial, proto::AckState_REFUSED);
	    return;
	}
	PendingRequest pending = it->second;
	pending_requests.erase(it);
	if (pending.is_choice) {
	    int choice = command->body_as_Answer()->choice();
	    if (choice < 0) {
		choice = -1;
	    }
	    Client *asker = find_client(clients, pending.client_name);
	    if (asker != nullptr) {
		send_ack(asker->fd, pending.client_serial,
			 choice >= 0 ? proto::AckState_DONE : proto::AckState_REFUSED,
			 0, choice);
	    }
	    send_ack(client.fd, client_serial, proto::AckState_DONE);
	    return;
	}
	if (!accept) {
	    ErrorRecord rec;
	    rec.sequence = ++last_sequence;
	    rec.kind = proto::OperatorKind_ERROR;
	    rec.severity = proto::Severity_WARNING;
	    rec.source = proto::ErrorSource_OPERATOR;
	    rec.command_serial = pending.client_serial;
	    rec.text = "the operator refused to run the block";
	    rec.timestamp = linuxcnc::wall_etime();
	    ring.push_back(rec);
	    while (ring.size() > kRingSize) {
		ring.pop_front();
	    }
	    send_error(client.fd, rec);
	    client.cursor = rec.sequence;
	    send_ack(client.fd, pending.client_serial, proto::AckState_REFUSED, rec.sequence);
	    return;
	}
	EMC_TASK_PLAN_EXECUTE message;
	copy_text(message.command, sizeof(message.command),
		  pending.block.c_str());
	uint64_t error_sequence = 0;
	issue_command(client.fd, pending.client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_AskOperator: {
	const auto *prompt = command->body_as_AskOperator()->prompt();
	std::vector<std::string> options;
	const auto *list = command->body_as_AskOperator()->options();
	if (list != nullptr) {
	    for (const auto *option : *list) {
		options.push_back(option != nullptr ? option->str() : "");
	    }
	}
	/* The question goes to the operator in focus; without a focus holder it
	   goes back to the client that asked. */
	Client *target = client.name.empty()
			     ? &client
			     : find_client(clients, operator_state.focus_client);
	if (target == nullptr || operator_state.focus_client.empty()) {
	    target = &client;
	}
	PendingRequest pending;
	pending.client_name = client.name;
	pending.client_serial = client_serial;
	pending.is_choice = true;
	pending.deadline = linuxcnc::etime() + 30.0;
	const uint32_t id = next_request_id++;
	pending_requests[id] = pending;
	send_request(target->fd, id,
		     prompt != nullptr ? prompt->str() : "the program asks",
		     30000, options);
	return;
    }
    case proto::CommandBody_ClaimFocus: {
	const bool take = command->body_as_ClaimFocus()->take();
	if (take && !operator_state.focus_client.empty() &&
	    operator_state.focus_client != client.name) {
	    ErrorRecord rec;
	    rec.sequence = ++last_sequence;
	    rec.kind = proto::OperatorKind_ERROR;
	    rec.severity = proto::Severity_WARNING;
	    rec.source = proto::ErrorSource_PROTOCOL;
	    rec.command_serial = client_serial;
	    rec.text = "focus is held by " + operator_state.focus_client;
	    rec.timestamp = linuxcnc::wall_etime();
	    ring.push_back(rec);
	    while (ring.size() > kRingSize) {
		ring.pop_front();
	    }
	    send_error(client.fd, rec);
	    client.cursor = rec.sequence;
	    send_ack(client.fd, client_serial, proto::AckState_REFUSED, rec.sequence);
	    return;
	}
	operator_state.focus_client = take ? client.name : std::string();
	send_ack(client.fd, client_serial, proto::AckState_DONE);
	return;
    }
    case proto::CommandBody_Mdi: {
	const auto *expect = command->body_as_Mdi()->expect_program();
	const std::string expected = expect != nullptr ? expect->str() : "";
	if (!expected.empty() && expected != operator_state.program_file) {
	    ErrorRecord rec;
	    rec.sequence = ++last_sequence;
	    rec.kind = proto::OperatorKind_ERROR;
	    rec.severity = proto::Severity_ERROR;
	    rec.source = proto::ErrorSource_PROTOCOL;
	    rec.command_serial = client_serial;
	    rec.text = "the loaded program is '" + operator_state.program_file +
		       "', not '" + expected + "'";
	    rec.timestamp = linuxcnc::wall_etime();
	    ring.push_back(rec);
	    while (ring.size() > kRingSize) {
		ring.pop_front();
	    }
	    send_error(client.fd, rec);
	    client.cursor = rec.sequence;
	    send_ack(client.fd, client_serial, proto::AckState_REFUSED, rec.sequence);
	    return;
	}
	if (command->body_as_Mdi()->confirm()) {
	    const auto *block = command->body_as_Mdi()->block();
	    PendingRequest pending;
	    pending.client_name = client.name;
	    pending.client_serial = client_serial;
	    pending.deadline = linuxcnc::etime() + 30.0;
	    pending.block = block != nullptr ? block->str() : "";
	    pending.expect_program = expected;
	    const uint32_t id = next_request_id++;
	    pending_requests[id] = pending;
	    send_request(client.fd, id, "run '" + pending.block + "'?", 30000);
	    return;
	}
	EMC_TASK_PLAN_EXECUTE message;
	const auto *block = command->body_as_Mdi()->block();
	copy_text(message.command, sizeof(message.command),
		  block != nullptr ? block->c_str() : "");
	operator_state.pending_mdi = block != nullptr ? block->str() : "";
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_SetTeleop: {
	EMC_TRAJ_SET_TELEOP_ENABLE message;
	message.enable = command->body_as_SetTeleop()->enable();
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_Home: {
	EMC_JOINT_HOME message;
	message.joint = command->body_as_Home()->joint();
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_JogStop: {
	EMC_JOG_STOP message;
	message.jjogmode = static_cast<int>(command->body_as_JogStop()->target());
	message.joint_or_axis = command->body_as_JogStop()->index();
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    case proto::CommandBody_Abort: {
	EMC_TASK_ABORT message;
	issue_command(client.fd, client_serial, message, &error_sequence);
	return;
    }
    default:
	send_ack(client.fd, client_serial, proto::AckState_REFUSED);
	return;
    }
}

void handle_hello(Client &client, const proto::Hello *hello) {
    client.hello_done = true;
    client.name = hello != nullptr && hello->client_name() != nullptr
		      ? hello->client_name()->str()
		      : "unnamed";
    const int64_t resume =
	hello != nullptr ? hello->last_error_sequence() : int64_t(-1);
    client.observe = hello != nullptr && hello->observe();
    client.stop_on_disconnect =
	hello == nullptr || hello->stop_on_disconnect();
    const int requested = hello != nullptr ? hello->poll_ms() : 0;
    /* A client that asserts nothing takes the server rate; one that asks for
       a faster tick than the bridge can keep is held at the floor rather than
       refused, so a stale client cannot make the machine stutter. */
    client.poll_ms = requested <= 0 ? 0 : std::max(requested, kMinPollMs);
    client.next_status = 0.0;

    send_hello(client.fd);
    if (auto it = views.find(client.name); it != views.end()) {
	send_view(client.fd, it->second);
    }

    if (resume < 0) {
	client.cursor = last_sequence;
    } else if (!ring.empty() &&
	       static_cast<uint64_t>(resume) + 1 < ring.front().sequence) {
	/* Too old to replay from the ring: say so, then carry on from now. */
	ErrorRecord gap;
	gap.sequence = last_sequence;
	gap.kind = proto::OperatorKind_ERROR;
	gap.severity = proto::Severity_WARNING;
	gap.source = proto::ErrorSource_PROTOCOL;
	gap.command_serial = 0;
	gap.text = "error replay gap: older messages are gone";
	gap.timestamp = linuxcnc::wall_etime();
	send_error(client.fd, gap);
	client.cursor = last_sequence;
    } else {
	client.cursor = static_cast<uint64_t>(resume);
	for (const ErrorRecord &rec : ring) {
	    if (rec.sequence > client.cursor) {
		send_error(client.fd, rec);
		client.cursor = rec.sequence;
	    }
	}
    }
    send_status(client.fd);
}

void handle_frame(Client &client, const uint8_t *data, size_t len,
		  std::vector<Client *> &clients) {
    flatbuffers::Verifier verifier(data, len);
    if (!proto::VerifyEnvelopeBuffer(verifier)) {
	return;
    }
    const proto::Envelope *envelope = proto::GetEnvelope(data);
    if (envelope->schema_version() != kSchemaVersion) {
	push_error(proto::OperatorKind_ERROR, proto::Severity_FATAL,
		   proto::ErrorSource_PROTOCOL, 0,
		   "client with an incompatible schema version");
	return;
    }
    switch (envelope->kind()) {
    case proto::MessageKind_HELLO:
	handle_hello(client, envelope->hello());
	break;
    case proto::MessageKind_COMMAND:
	if (!client.hello_done) {
	    break;
	}
	if (client.observe) {
	    const int serial = envelope->command() != nullptr
				   ? envelope->command()->client_serial()
				   : 0;
	    ErrorRecord rec;
	    rec.sequence = ++last_sequence;
	    rec.kind = proto::OperatorKind_ERROR;
	    rec.severity = proto::Severity_ERROR;
	    rec.source = proto::ErrorSource_PROTOCOL;
	    rec.command_serial = serial;
	    rec.text = "this connection declared observe and may not command";
	    rec.timestamp = linuxcnc::wall_etime();
	    ring.push_back(rec);
	    while (ring.size() > kRingSize) {
		ring.pop_front();
	    }
	    send_error(client.fd, rec);
	    client.cursor = rec.sequence;
	    send_ack(client.fd, serial, proto::AckState_REFUSED, rec.sequence);
	    break;
	}
	handle_command(client, envelope->command(), clients);
	break;
    default:
	break;
    }
}

}  // namespace

int main(int argc, char **argv) {
    const char *nml_file = nullptr;
    std::string socket_path = "/tmp/linuxcnc-protocol.sock";
    int tcp_port = 0;
    int status_ms = 50;

    for (int i = 1; i < argc; i++) {
	if (std::strcmp(argv[i], "--nml-file") == 0 && i + 1 < argc) {
	    nml_file = argv[++i];
	} else if (std::strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
	    socket_path = argv[++i];
	} else if (std::strcmp(argv[i], "--tcp") == 0 && i + 1 < argc) {
	    tcp_port = std::atoi(argv[++i]);
	} else if (std::strcmp(argv[i], "--status-ms") == 0 && i + 1 < argc) {
	    status_ms = std::atoi(argv[++i]);
	} else {
	    std::fprintf(stderr,
			 "usage: %s [--nml-file FILE] [--socket PATH] "
			 "[--tcp PORT] [--status-ms N]\n",
			 argv[0]);
	    return 2;
	}
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    if (!open_channels(nml_file)) {
	fail("cannot open the NML channels");
    }

    const int listener =
	tcp_port > 0 ? proto::listen_tcp(tcp_port) : proto::listen_unix(socket_path);
    if (listener < 0) {
	fail("cannot listen");
    }
    proto::set_nonblocking(listener);

    std::vector<Client *> clients;

    while (!stop_requested) {
	std::vector<struct pollfd> fds;
	fds.push_back({listener, POLLIN, 0});
	for (Client *client : clients) {
	    short events = POLLIN;
	    fds.push_back({client->fd, events, 0});
	}

	const int ready = ::poll(fds.data(), fds.size(), 10);
	if (ready < 0 && errno != EINTR) {
	    break;
	}

	if (ready > 0 && (fds[0].revents & POLLIN) != 0) {
	    const int fd = ::accept(listener, nullptr, nullptr);
	    if (fd >= 0 && clients.size() < kMaxClients) {
		proto::set_nonblocking(fd);
		Client *client = new Client();
		client->fd = fd;
		clients.push_back(client);
	    } else if (fd >= 0) {
		::close(fd);
	    }
	}

	for (size_t i = 0; i < clients.size(); i++) {
	    Client *client = clients[i];
	    const short revents = ready > 0 ? fds[i + 1].revents : 0;
	    if ((revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
		continue;
	    }
	    bool alive = true;
	    for (;;) {
		std::vector<uint8_t> frame;
		const int result =
		    proto::frame_recv(client->fd, client->pending, frame);
		if (result < 0) {
		    alive = false;
		    break;
		}
		if (result == 0) {
		    break;
		}
		handle_frame(*client, frame.data(), frame.size(), clients);
	    }
	    if (!alive) {
		if (operator_state.focus_client == client->name) {
		    operator_state.focus_client.clear();
		}
		if (client->hello_done && client->stop_on_disconnect &&
		    !client->observe) {
		    /* What a GUI expects when its window disappears: nothing
		       keeps moving because the socket closed. */
		    EMC_TASK_ABORT abort_message;
		    if (cmd_channel != nullptr && cmd_channel->valid()) {
			cmd_channel->write(&abort_message);
		    }
		}
		delete client;
		clients.erase(clients.begin() + static_cast<long>(i));
		i--;
	    }
	}

	poll_errors(0);

	const double now = linuxcnc::etime();
	for (auto it = pending_requests.begin(); it != pending_requests.end();) {
	    if (now < it->second.deadline) {
		++it;
		continue;
	    }
	    /* Nobody answered: refuse rather than run blind. */
	    for (Client *client : clients) {
		if (client->name != it->second.client_name) {
		    continue;
		}
		ErrorRecord rec;
		rec.sequence = ++last_sequence;
		rec.kind = proto::OperatorKind_ERROR;
		rec.severity = proto::Severity_WARNING;
		rec.source = proto::ErrorSource_PROTOCOL;
		rec.command_serial = it->second.client_serial;
		rec.text = "no answer to the confirmation; the block was not run";
		rec.timestamp = linuxcnc::wall_etime();
		ring.push_back(rec);
		while (ring.size() > kRingSize) {
		    ring.pop_front();
		}
		send_error(client->fd, rec);
		client->cursor = rec.sequence;
		send_ack(client->fd, it->second.client_serial,
			 proto::AckState_REFUSED, rec.sequence);
	    }
	    it = pending_requests.erase(it);
	}

	if (refresh_status()) {
	    for (Client *client : clients) {
		if (!client->hello_done || now < client->next_status) {
		    continue;
		}
		const int period = client->poll_ms > 0 ? client->poll_ms : status_ms;
		client->next_status = now + period / 1000.0;
		/* replay errors this client has not seen */
		for (const ErrorRecord &rec : ring) {
		    if (rec.sequence > client->cursor) {
			send_error(client->fd, rec);
			client->cursor = rec.sequence;
		    }
		}
		flatbuffers::FlatBufferBuilder fbb;
		send_envelope(client->fd, fbb,
			      proto::CreateEnvelope(
				  fbb, kSchemaVersion, proto::MessageKind_STATUS, 0,
				  make_status(fbb), 0, 0, 0, 0, 0));
	    }
	}
    }

    for (Client *client : clients) {
	delete client;
    }
    ::close(listener);
    if (tcp_port == 0) {
	::unlink(socket_path.c_str());
    }
    return 0;
}
