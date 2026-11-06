// Protocol client prototype.
//
// A client written against emc.fbs alone: no NML header, no link against
// LinuxCNC, only the flatbuffers runtime and a byte stream.  It drives a
// simulator through the bridge and checks the parts that NML cannot do:
// a versioned handshake, an ack per command, and errors with a sequence
// number that replay after a reconnect.
//
// Build: tests/protocol/test.sh builds it; the file needs only the
// flatbuffers runtime and emc_generated.h.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "emc_generated.h"
#include "frame.hh"

namespace proto = linuxcnc::protocol;

namespace {

int failures = 0;

void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    std::fflush(stdout);
    if (!ok) {
	failures++;
    }
}

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Connection {
    int fd = -1;
    std::vector<uint8_t> pending;

    bool status_valid = false;
    int joints = 0;
    int homed = 0;
    int task_state = -1;
    int task_mode = -1;
    double position_x = 0.0;
    int command_status = 0;
    int last_command_serial = 0;

    bool hello_ok = false;
    int ack_serial = -1;
    bool ack_ok = false;
    uint64_t ack_error_sequence = 0;

    bool got_error = false;
    uint64_t error_sequence = 0;
    uint64_t error_command_serial = 0;
    std::string error_text;
};

/* Read one frame if one is ready within timeout_ms.  Returns the message
   kind, or -1 when nothing arrived. */
int pump(Connection &connection, int timeout_ms) {
    struct pollfd pfd = {connection.fd, POLLIN, 0};
    const int ready = ::poll(&pfd, 1, timeout_ms);
    if (ready <= 0) {
	return -1;
    }
    for (;;) {
	std::vector<uint8_t> frame;
	const int result =
	    proto::frame_recv(connection.fd, connection.pending, frame);
	if (result <= 0) {
	    return -1;
	}
	flatbuffers::Verifier verifier(frame.data(), frame.size());
	if (!proto::VerifyEnvelopeBuffer(verifier)) {
	    return -1;
	}
	const proto::Envelope *envelope = proto::GetEnvelope(frame.data());
	switch (envelope->kind()) {
	case proto::MessageKind_HELLO:
	    connection.hello_ok =
		envelope->hello() != nullptr &&
		envelope->hello()->version() != nullptr &&
		envelope->hello()->version()->protocol_major() == 1 &&
		envelope->hello()->version()->schema_version() == 1;
	    return envelope->kind();
	case proto::MessageKind_STATUS: {
	    const proto::Status *status = envelope->status();
	    if (status != nullptr) {
		connection.status_valid = status->valid();
		connection.joints = status->joints() != nullptr
					? static_cast<int>(status->joints()->size())
					: 0;
		connection.homed = 0;
		if (status->joints() != nullptr) {
		    for (const auto *joint : *status->joints()) {
			if (joint->homed()) {
			    connection.homed++;
			}
		    }
		}
		connection.task_state = status->task_state();
		connection.task_mode = status->task_mode();
		connection.command_status = static_cast<int>(status->command_status());
		connection.last_command_serial = status->last_command_serial();
		if (status->position() != nullptr) {
		    connection.position_x = status->position()->x();
		}
	    }
	    return envelope->kind();
	}
	case proto::MessageKind_ACK: {
	    const proto::Ack *ack = envelope->ack();
	    if (ack != nullptr) {
		connection.ack_serial = ack->command_serial();
		connection.ack_ok = ack->ok();
		connection.ack_error_sequence = ack->error_sequence();
	    }
	    return envelope->kind();
	}
	case proto::MessageKind_ERROR: {
	    const proto::Error *error = envelope->error();
	    if (error != nullptr) {
		connection.got_error = true;
		connection.error_sequence = error->sequence();
		connection.error_command_serial = error->command_serial();
		connection.error_text =
		    error->text() != nullptr ? error->text()->str() : "";
	    }
	    return envelope->kind();
	}
	default:
	    return envelope->kind();
	}
    }
}

template <typename Predicate>
bool wait_for(Connection &connection, Predicate done, double timeout,
	      const char *what) {
    const double deadline = now() + timeout;
    while (now() < deadline) {
	pump(connection, 20);
	if (done(connection)) {
	    return true;
	}
    }
    std::printf("     timed out waiting for %s\n", what);
    return false;
}

bool send_hello(Connection &connection, int64_t resume_from) {
    flatbuffers::FlatBufferBuilder fbb;
    const auto name = fbb.CreateString("protocol-client");
    proto::VersionBuilder version(fbb);
    version.add_protocol_major(1);
    version.add_protocol_minor(0);
    version.add_schema_version(1);
    const auto version_offset = version.Finish();
    proto::HelloBuilder hello(fbb);
    hello.add_version(version_offset);
    hello.add_client_name(name);
    hello.add_last_error_sequence(resume_from);
    const auto envelope =
	proto::CreateEnvelope(fbb, 1, proto::MessageKind_HELLO,
			      hello.Finish(), 0, 0, 0, 0);
    fbb.Finish(envelope, proto::EnvelopeIdentifier());
    return proto::frame_send(connection.fd, fbb.GetBufferPointer(), fbb.GetSize());
}

template <typename MakeBody>
int send_command(Connection &connection, proto::CommandBody body_type,
		 MakeBody make_body) {
    static int serial = 0;
    flatbuffers::FlatBufferBuilder fbb;
    const flatbuffers::Offset<void> body = make_body(fbb);
    const auto command = proto::CreateCommand(fbb, ++serial, body_type, body);
    const auto envelope =
	proto::CreateEnvelope(fbb, 1, proto::MessageKind_COMMAND, 0, 0, 0,
			      command, 0);
    fbb.Finish(envelope, proto::EnvelopeIdentifier());
    if (!proto::frame_send(connection.fd, fbb.GetBufferPointer(),
			   fbb.GetSize())) {
	return -1;
    }
    return serial;
}

template <typename MakeBody>
bool command_and_wait(Connection &connection, proto::CommandBody body_type,
		      MakeBody make_body, const char *what) {
    const int serial = send_command(connection, body_type, make_body);
    const bool answered =
	serial > 0 &&
	wait_for(
	    connection, [serial](Connection &c) { return c.ack_serial == serial; },
	    10.0, "an ack");
    check(answered && connection.ack_ok, what);
    return answered && connection.ack_ok;
}

}  // namespace

int main(int argc, char **argv) {
    std::string socket_path = "/tmp/linuxcnc-protocol.sock";
    int tcp_port = 0;
    for (int i = 1; i < argc; i++) {
	if (std::strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
	    socket_path = argv[++i];
	} else if (std::strcmp(argv[i], "--tcp") == 0 && i + 1 < argc) {
	    tcp_port = std::atoi(argv[++i]);
	}
    }

    Connection connection;
    connection.fd = tcp_port > 0 ? proto::connect_tcp(tcp_port)
				 : proto::connect_unix(socket_path);
    if (connection.fd < 0) {
	std::printf("FAIL connect\n");
	return 1;
    }

    check(send_hello(connection, -1), "send hello");
    check(wait_for(connection, [](Connection &c) { return c.hello_ok; }, 5.0,
		   "the server hello"),
	  "hello carries protocol 1, schema 1");

    check(wait_for(connection,
		   [](Connection &c) {
		       return c.status_valid && c.joints == 3;
		   },
		   20.0, "a status with three joints"),
	  "status arrives with three joints");

    command_and_wait(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, 2).Union();
	},
	"leave e-stop");
    command_and_wait(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, 4).Union();
	},
	"power on");
    check(wait_for(connection,
		   [](Connection &c) { return c.task_state == 4; }, 10.0,
		   "task state on"),
	  "status reports the machine on");

    command_and_wait(
	connection, proto::CommandBody_SetTeleop,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetTeleop(fbb, false).Union();
	},
	"leave teleop for homing");
    command_and_wait(connection, proto::CommandBody_Home,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateHome(fbb, -1).Union();
		     },
		     "home all joints");
    check(wait_for(connection,
		   [](Connection &c) { return c.homed == c.joints && c.joints > 0; },
		   20.0, "all joints homed"),
	  "status reports all joints homed");

    command_and_wait(
	connection, proto::CommandBody_SetMode,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetMode(fbb, 3).Union();
	},
	"switch to MDI");
    check(wait_for(connection,
		   [](Connection &c) { return c.task_mode == 3; }, 10.0,
		   "MDI mode"),
	  "status reports MDI mode");
    command_and_wait(connection, proto::CommandBody_Mdi,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateMdi(fbb, fbb.CreateString("G0 X1"))
			     .Union();
		     },
		     "run a block of MDI");
    check(wait_for(connection,
		   [](Connection &c) {
		       return c.position_x > 0.999 && c.position_x < 1.001;
		   },
		   10.0, "X1"),
	  "the machine moved to X1");
    std::printf("     status: state=%d mode=%d x=%.3f last_serial=%d "
		"command_status=%d homed=%d/%d errors=%llu text=%s\n",
		connection.task_state, connection.task_mode,
		connection.position_x, connection.last_command_serial,
		connection.command_status, connection.homed,
		connection.joints,
		static_cast<unsigned long long>(connection.error_sequence),
		connection.error_text.c_str());

    /* One command that cannot succeed, so the error path is exercised. */
    const int failing = send_command(
	connection, proto::CommandBody_Mdi,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateMdi(fbb, fbb.CreateString("G99999")).Union();
	});
    wait_for(
	connection, [failing](Connection &c) { return c.ack_serial == failing; },
	10.0, "the ack of the failing block");
    check(wait_for(connection, [](Connection &c) { return c.got_error; }, 10.0,
		   "an error message"),
	  "a failed block produces an error");
    check(connection.error_sequence > 0,
	  "the error carries a monotonic sequence number");
    check(connection.error_command_serial != 0,
	  "the error names the command that caused it");

    command_and_wait(connection, proto::CommandBody_Abort,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateAbort(fbb).Union();
		     },
		     "abort");
    command_and_wait(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, 1).Union();
	},
	"e-stop");

    const uint64_t seen = connection.error_sequence;
    ::close(connection.fd);

    /* Reconnect as a client that missed one message: it must get that very
       sequence again, not a fresh stream. */
    Connection replay;
    replay.fd = tcp_port > 0 ? proto::connect_tcp(tcp_port)
			     : proto::connect_unix(socket_path);
    if (replay.fd < 0) {
	std::printf("FAIL reconnect\n");
	return 1;
    }
    check(send_hello(replay, static_cast<int64_t>(seen) - 1),
	  "send hello asking for replay");
    check(wait_for(replay,
		   [seen](Connection &c) {
		       return c.got_error && c.error_sequence == seen;
		   },
		   10.0, "the replayed error"),
	  "the missed error is replayed from its sequence number");
    ::close(replay.fd);

    std::printf("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL",
		failures);
    return failures == 0 ? 0 : 1;
}
