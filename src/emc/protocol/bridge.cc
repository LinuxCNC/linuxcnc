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

volatile sig_atomic_t stop_requested = 0;

void on_signal(int) {
    stop_requested = 1;
}

struct ErrorRecord {
    uint64_t sequence;
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

void push_error(proto::Severity severity, proto::ErrorSource source,
		int command_serial, const std::string &text) {
    ErrorRecord rec;
    rec.sequence = ++last_sequence;
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
	proto::Severity severity = proto::Severity_INFO;
	proto::ErrorSource source = proto::ErrorSource_TASK;
	switch (type) {
	case EMC_OPERATOR_ERROR_TYPE:
	    severity = proto::Severity_ERROR;
	    source = proto::ErrorSource_OPERATOR;
	    std::strncpy(text, static_cast<EMC_OPERATOR_ERROR *>(address)->error,
			 sizeof(text) - 1);
	    break;
	case EMC_OPERATOR_TEXT_TYPE:
	    source = proto::ErrorSource_OPERATOR;
	    std::strncpy(text, static_cast<EMC_OPERATOR_TEXT *>(address)->text,
			 sizeof(text) - 1);
	    break;
	case EMC_OPERATOR_DISPLAY_TYPE:
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
	push_error(severity, source, command_serial, text);
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

    std::vector<flatbuffers::Offset<proto::Joint>> joints;
    const int joint_count =
	std::max(0, std::min(s.motion.traj.joints, EMCMOT_MAX_JOINTS));
    for (int i = 0; i < joint_count; i++) {
	const EMC_JOINT_STAT &joint = s.motion.joint[i];
	proto::JointBuilder builder(fbb);
	builder.add_joint(joint.joint);
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
    builder.add_task_mode(static_cast<int>(s.task.mode));
    builder.add_task_state(static_cast<int>(s.task.state));
    builder.add_exec_state(static_cast<int>(s.task.execState));
    builder.add_interp_state(static_cast<int>(s.task.interpState));
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
    version.add_schema_version(1);
    const auto version_offset = version.Finish();
    proto::HelloBuilder hello(fbb);
    hello.add_version(version_offset);
    hello.add_client_name(name);
    hello.add_last_error_sequence(static_cast<int64_t>(last_sequence));
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 1, proto::MessageKind_HELLO,
					       hello.Finish(), 0, 0, 0, 0));
}

bool send_error(int fd, const ErrorRecord &rec) {
    flatbuffers::FlatBufferBuilder fbb;
    auto text = fbb.CreateString(rec.text);
    proto::ErrorBuilder error(fbb);
    error.add_sequence(rec.sequence);
    error.add_severity(rec.severity);
    error.add_source(rec.source);
    error.add_command_serial(rec.command_serial);
    error.add_text(text);
    error.add_timestamp(rec.timestamp);
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 1, proto::MessageKind_ERROR, 0,
					       0, error.Finish(), 0, 0));
}

bool send_ack(int fd, int command_serial, bool ok, uint64_t error_sequence) {
    flatbuffers::FlatBufferBuilder fbb;
    proto::AckBuilder ack(fbb);
    ack.add_command_serial(command_serial);
    ack.add_ok(ok);
    ack.add_error_sequence(error_sequence);
    return send_envelope(fd, fbb,
			 proto::CreateEnvelope(fbb, 1, proto::MessageKind_ACK, 0,
					       0, 0, 0, ack.Finish()));
}

bool send_status(int fd) {
    if (!refresh_status()) {
	return true;	// nothing new to say yet is not an error
    }
    flatbuffers::FlatBufferBuilder fbb;
    return send_envelope(
	fd, fbb,
	proto::CreateEnvelope(fbb, 1, proto::MessageKind_STATUS, 0,
			      make_status(fbb), 0, 0, 0));
}

/* Write one command and wait until task has taken it.  Returns the serial
   task echoed, or -1.  The error sequence of the last message seen while
   waiting is reported back so the ack can point at it. */
template <typename Message>
int issue_command(Message &message, uint64_t *error_sequence) {
    if (cmd_channel == nullptr || !cmd_channel->valid()) {
	return -1;
    }
    if (cmd_channel->write(&message) != 0) {
	return -1;
    }
    const int serial = message.serial_number;
    const double deadline = linuxcnc::etime() + kCommandTimeout;
    while (linuxcnc::etime() < deadline) {
	refresh_status();
	poll_errors(serial);
	if (have_status && status.echo_serial_number - serial >= 0) {
	    if (!ring.empty() && ring.back().command_serial == serial) {
		*error_sequence = ring.back().sequence;
	    }
	    return serial;
	}
	linuxcnc::esleep(kPollInterval);
    }
    return -1;
}

void handle_command(Client &client, const proto::Command *command) {
    const int client_serial = command == nullptr ? 0 : command->client_serial();
    uint64_t error_sequence = 0;
    int serial = -1;

    if (command == nullptr || command->body_type() == proto::CommandBody_NONE) {
	send_ack(client.fd, client_serial, false, 0);
	return;
    }

    switch (command->body_type()) {
    case proto::CommandBody_SetMode: {
	EMC_TASK_SET_MODE message;
	message.mode = static_cast<EMC_TASK_MODE>(command->body_as_SetMode()->mode());
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_SetState: {
	EMC_TASK_SET_STATE message;
	message.state =
	    static_cast<EMC_TASK_STATE>(command->body_as_SetState()->state());
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_Mdi: {
	EMC_TASK_PLAN_EXECUTE message;
	copy_text(message.command, sizeof(message.command),
		  command->body_as_Mdi()->block()->c_str());
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_SetTeleop: {
	EMC_TRAJ_SET_TELEOP_ENABLE message;
	message.enable = command->body_as_SetTeleop()->enable();
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_Home: {
	EMC_JOINT_HOME message;
	message.joint = command->body_as_Home()->joint();
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_JogStop: {
	EMC_JOG_STOP message;
	message.jjogmode = command->body_as_JogStop()->joint_mode();
	message.joint_or_axis = command->body_as_JogStop()->index();
	serial = issue_command(message, &error_sequence);
	break;
    }
    case proto::CommandBody_Abort: {
	EMC_TASK_ABORT message;
	serial = issue_command(message, &error_sequence);
	break;
    }
    default:
	break;
    }

    send_ack(client.fd, client_serial, serial >= 0, error_sequence);
}

void handle_hello(Client &client, const proto::Hello *hello) {
    client.hello_done = true;
    client.name = hello != nullptr && hello->client_name() != nullptr
		      ? hello->client_name()->str()
		      : "unnamed";
    const int64_t resume =
	hello != nullptr ? hello->last_error_sequence() : int64_t(-1);

    send_hello(client.fd);

    if (resume < 0) {
	client.cursor = last_sequence;
    } else if (!ring.empty() &&
	       static_cast<uint64_t>(resume) + 1 < ring.front().sequence) {
	/* Too old to replay from the ring: say so, then carry on from now. */
	ErrorRecord gap;
	gap.sequence = last_sequence;
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

void handle_frame(Client &client, const uint8_t *data, size_t len) {
    flatbuffers::Verifier verifier(data, len);
    if (!proto::VerifyEnvelopeBuffer(verifier)) {
	return;
    }
    const proto::Envelope *envelope = proto::GetEnvelope(data);
    if (envelope->schema_version() != 1) {
	push_error(proto::Severity_FATAL, proto::ErrorSource_PROTOCOL, 0,
		   "client with an incompatible schema version");
	return;
    }
    switch (envelope->kind()) {
    case proto::MessageKind_HELLO:
	handle_hello(client, envelope->hello());
	break;
    case proto::MessageKind_COMMAND:
	if (client.hello_done) {
	    handle_command(client, envelope->command());
	}
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
    double next_status = 0.0;

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
		handle_frame(*client, frame.data(), frame.size());
	    }
	    if (!alive) {
		delete client;
		clients.erase(clients.begin() + static_cast<long>(i));
		i--;
	    }
	}

	poll_errors(0);

	const double now = linuxcnc::etime();
	if (now >= next_status) {
	    next_status = now + status_ms / 1000.0;
	    if (refresh_status()) {
		for (Client *client : clients) {
		    if (!client->hello_done) {
			continue;
		    }
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
				      fbb, 1, proto::MessageKind_STATUS, 0,
				      make_status(fbb), 0, 0, 0));
		}
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
