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

#include <emc/protocol/emc_generated.h>

#include <flatbuffers/idl.h>
#include "frame.hh"

namespace proto = linuxcnc::protocol;

namespace {

int failures = 0;

/* Optional: render every received frame as JSON through the schema's
   reflection tables, so a test or a person can read the wire without a
   decoder per message. Disabled unless --bfbs is given. */
flatbuffers::Parser *json_parser = nullptr;

void dump_json(const uint8_t *data, size_t len) {
    (void)len;
    if (json_parser == nullptr) {
	return;
    }
    std::string text;
    const char *error = flatbuffers::GenerateText(*json_parser, data, &text);
    if (error != nullptr) {
	std::printf("json error: %s\n", error);
	return;
    }
    std::printf("json %s\n", text.c_str());
    std::fflush(stdout);
}
/* One counter for every command type: a per-template static would hand out
   duplicate serials and each wait would match the wrong ack. */
int client_serial_counter = 0;

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
    int hello_joints = 0;
    int hello_axes = 0;
    int ack_serial = -1;
    bool ack_ok = false;
    proto::AckState ack_state = proto::AckState_REFUSED;
    int received_serial = -1;
    int ack_choice = -1;
    uint64_t ack_error_sequence = 0;

    double op_jog_increment = 0.0;
    double op_jog_rate = 0.0;
    std::string op_program;
    std::string op_pending_mdi;
    std::string op_focus;

    bool got_view = false;
    size_t view_bytes = 0;
    uint32_t request_id = 0;
    proto::RequestKind request_kind = proto::RequestKind_CONFIRM;
    std::string request_text;
    std::vector<std::string> request_options;

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
	dump_json(frame.data(), frame.size());
	const proto::Envelope *envelope = proto::GetEnvelope(frame.data());
	switch (envelope->kind()) {
	case proto::MessageKind_HELLO:
	    connection.hello_ok =
		envelope->hello() != nullptr &&
		envelope->hello()->version() != nullptr &&
		envelope->hello()->version()->protocol_major() == 1 &&
		envelope->hello()->version()->schema_version() == 2;
	    if (envelope->hello() != nullptr) {
		connection.hello_joints = envelope->hello()->joints();
		connection.hello_axes = envelope->hello()->axes();
	    }
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
		if (status->operator_() != nullptr) {
		    connection.op_jog_increment = status->operator_()->jog_increment();
		    connection.op_jog_rate = status->operator_()->jog_rate();
		    connection.op_program = status->operator_()->program_file() != nullptr
						? status->operator_()->program_file()->str()
						: "";
		    connection.op_pending_mdi =
			status->operator_()->pending_mdi() != nullptr
			    ? status->operator_()->pending_mdi()->str()
			    : "";
		    connection.op_focus = status->operator_()->focus_client() != nullptr
					      ? status->operator_()->focus_client()->str()
					      : "";
		}
		connection.command_status = static_cast<int>(status->command_status());
		connection.last_command_serial = status->last_command_serial();
		if (status->position() != nullptr) {
		    connection.position_x = status->position()->x();
		}
	    }
	    return envelope->kind();
	}
	case proto::MessageKind_VIEW: {
	    const proto::View *view = envelope->view();
	    connection.got_view = true;
	    connection.view_bytes = view != nullptr && view->data() != nullptr
					? view->data()->size()
					: 0;
	    return envelope->kind();
	}
	case proto::MessageKind_REQUEST: {
	    const proto::Request *request = envelope->request();
	    if (request != nullptr) {
		connection.request_id = request->id();
		connection.request_kind = request->kind();
		connection.request_text =
		    request->text() != nullptr ? request->text()->str() : "";
		connection.request_options.clear();
		if (request->options() != nullptr) {
		    for (const auto *option : *request->options()) {
			connection.request_options.push_back(
			    option != nullptr ? option->str() : "");
		    }
		}
	    }
	    return envelope->kind();
	}
	case proto::MessageKind_ACK: {
	    const proto::Ack *ack = envelope->ack();
	    if (ack != nullptr) {
		connection.ack_serial = ack->command_serial();
		connection.ack_ok = ack->ok();
		connection.ack_state = ack->state();
		if (ack->state() == proto::AckState_RECEIVED) {
		    connection.received_serial = ack->command_serial();
		}
		connection.ack_choice = ack->choice();
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

bool send_hello(Connection &connection, int64_t resume_from,
		bool observe = false, const char *name = "protocol-client") {
    flatbuffers::FlatBufferBuilder fbb;
    const auto client_name = fbb.CreateString(name);
    proto::VersionBuilder version(fbb);
    version.add_protocol_major(1);
    version.add_protocol_minor(0);
    version.add_schema_version(2);
    const auto version_offset = version.Finish();
    proto::HelloBuilder hello(fbb);
    hello.add_version(version_offset);
    hello.add_client_name(client_name);
    hello.add_last_error_sequence(resume_from);
    hello.add_poll_ms(20);
    hello.add_observe(observe);
    const auto envelope =
	proto::CreateEnvelope(fbb, 2, proto::MessageKind_HELLO,
			      hello.Finish(), 0, 0, 0, 0);
    fbb.Finish(envelope, proto::EnvelopeIdentifier());
    return proto::frame_send(connection.fd, fbb.GetBufferPointer(), fbb.GetSize());
}

template <typename MakeBody>
int send_command(Connection &connection, proto::CommandBody body_type,
		 MakeBody make_body) {
    const int serial = ++client_serial_counter;
    flatbuffers::FlatBufferBuilder fbb;
    const flatbuffers::Offset<void> body = make_body(fbb);
    const auto command = proto::CreateCommand(fbb, serial, body_type, body);
    const auto envelope =
	proto::CreateEnvelope(fbb, 2, proto::MessageKind_COMMAND, 0, 0, 0,
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
	    connection,
	    [serial](Connection &c) {
		return c.ack_serial == serial &&
		       (c.ack_state == proto::AckState_DONE ||
			c.ack_state == proto::AckState_ERROR ||
			c.ack_state == proto::AckState_REFUSED);
	    },
	    10.0, "an ack");
    check(answered && connection.ack_state == proto::AckState_DONE, what);
    return answered && connection.ack_state == proto::AckState_DONE;
}

}  // namespace

int main(int argc, char **argv) {
    std::string socket_path = "/tmp/linuxcnc-protocol.sock";
    std::string bfbs_path;
    int tcp_port = 0;
    for (int i = 1; i < argc; i++) {
	if (std::strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
	    socket_path = argv[++i];
	} else if (std::strcmp(argv[i], "--tcp") == 0 && i + 1 < argc) {
	    tcp_port = std::atoi(argv[++i]);
	} else if (std::strcmp(argv[i], "--bfbs") == 0 && i + 1 < argc) {
	    bfbs_path = argv[++i];
	}
    }

    flatbuffers::Parser parser;
    if (!bfbs_path.empty()) {
	std::FILE *file = std::fopen(bfbs_path.c_str(), "rb");
	if (file == nullptr) {
	    std::printf("FAIL cannot open %s\n", bfbs_path.c_str());
	    return 1;
	}
	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);
	std::vector<uint8_t> schema(static_cast<size_t>(size));
	const size_t read = std::fread(schema.data(), 1, schema.size(), file);
	std::fclose(file);
	if (read != schema.size() || !parser.Deserialize(schema.data(), schema.size())) {
	    std::printf("FAIL cannot load the schema\n");
	    return 1;
	}
	json_parser = &parser;
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
	  "hello carries protocol 1, schema 2");
    check(connection.hello_joints == 3 && connection.hello_axes == 3,
	  "hello reports a three-joint machine");

    check(wait_for(connection,
		   [](Connection &c) {
		       return c.status_valid && c.joints == 3;
		   },
		   20.0, "a status with three joints"),
	  "status arrives with three joints");

    const int estop_reset = send_command(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, proto::TaskState_ESTOP_RESET).Union();
	});
    check(wait_for(connection,
		   [estop_reset](Connection &c) {
		       return c.ack_serial == estop_reset &&
			      c.ack_state == proto::AckState_DONE;
		   },
		   10.0, "the ack of leaving e-stop"),
	  "leave e-stop");
    check(connection.received_serial == estop_reset,
	  "a command is acknowledged as received before it is done");
    command_and_wait(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, proto::TaskState_ON).Union();
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
	    return proto::CreateSetMode(fbb, proto::TaskMode_MDI).Union();
	},
	"switch to MDI");
    check(wait_for(connection,
		   [](Connection &c) { return c.task_mode == 3; }, 10.0,
		   "MDI mode"),
	  "status reports MDI mode");
    command_and_wait(
	connection, proto::CommandBody_SetJog,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetJog(fbb, 0.5, 2.0).Union();
	},
	"set the jog increment and rate");
    command_and_wait(
	connection, proto::CommandBody_SelectProgram,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSelectProgram(fbb, fbb.CreateString("test.ngc"))
		.Union();
	},
	"select a program");
    command_and_wait(connection, proto::CommandBody_ClaimFocus,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateClaimFocus(fbb, true).Union();
		     },
		     "claim focus");
    check(wait_for(connection,
		   [](Connection &c) {
		       return c.op_jog_increment == 0.5 && c.op_jog_rate == 2.0 &&
			      c.op_program == "test.ngc" &&
			      c.op_focus == "protocol-client";
		   },
		   10.0, "operator state from the core"),
	  "operator state is read back from the core");

    /* View state is opaque to the core: store a blob, read it back on the
       next connection that uses the same client name. */
    command_and_wait(
	connection, proto::CommandBody_SetView,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    const uint8_t blob[] = {'p', 'a', 'g', 'e', '2'};
	    return proto::CreateSetView(fbb, fbb.CreateVector(blob, sizeof(blob)))
		.Union();
	},
	"store the view state");

    /* A run that names a different program than the loaded one is refused. */
    const int wrong_program = send_command(
	connection, proto::CommandBody_Mdi,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateMdi(fbb, fbb.CreateString("G0 X1"),
				    fbb.CreateString("other.ngc"), false)
		.Union();
	});
    wait_for(connection,
	     [wrong_program](Connection &c) { return c.ack_serial == wrong_program; },
	     10.0, "the refusal of a stale run");
    check(wait_for(connection,
		   [](Connection &c) {
		       return c.error_text.find("not 'other.ngc'") !=
			      std::string::npos;
		   },
		   5.0, "the stale-run refusal text"),
	  "a run naming another program is refused");
    check(connection.ack_state == proto::AckState_REFUSED,
	  "a refused run never reports done");

    /* A run that asks for confirmation: the core asks this client, and the
       block is issued only after the answer. */
    const int confirmed = send_command(
	connection, proto::CommandBody_Mdi,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateMdi(fbb, fbb.CreateString("G0 X2"), 0, true)
		.Union();
	});
    check(wait_for(connection, [](Connection &c) { return c.request_id != 0; },
		   10.0, "the confirmation request"),
	  "the core asks before it runs a confirmed block");
    const uint32_t request_id = connection.request_id;
    send_command(connection, proto::CommandBody_Answer,
		 [request_id](flatbuffers::FlatBufferBuilder &fbb) {
		     return proto::CreateAnswer(fbb, request_id, true).Union();
		 });
    check(wait_for(connection,
		   [confirmed](Connection &c) { return c.ack_serial == confirmed; },
		   20.0, "the ack after answering") &&
	      connection.ack_ok,
	  "the block runs after the operator answers");
    check(wait_for(connection,
		   [](Connection &c) { return c.position_x > 1.999; }, 10.0, "X2"),
	  "the confirmed block moved the machine");

    command_and_wait(connection, proto::CommandBody_Mdi,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateMdi(fbb, fbb.CreateString("G0 X1"), 0,
						 false)
			     .Union();
		     },
		     "run a block of MDI");
    check(wait_for(connection,
		   [](Connection &c) {
		       return c.position_x > 0.999 && c.position_x < 1.001;
		   },
		   10.0, "X1"),
	  "the machine moved to X1");
    check(wait_for(connection,
		   [](Connection &c) { return c.op_pending_mdi == "G0 X1"; }, 10.0,
		   "the queued MDI line"),
	  "the core keeps the MDI line the operator queued");
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
	    return proto::CreateMdi(fbb, fbb.CreateString("G99999"), 0, false)
		.Union();
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

    /* Interactive G-code: the program asks a question with choices, the
       client in focus shows it, and the answer comes back as a value. */
    const int asked = send_command(
	connection, proto::CommandBody_AskOperator,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    std::vector<flatbuffers::Offset<flatbuffers::String>> options{
		fbb.CreateString("Continue"), fbb.CreateString("Abort")};
	    return proto::CreateAskOperator(
		       fbb, fbb.CreateString("Change the tool now?"),
		       fbb.CreateVector(options))
		.Union();
	});
    check(wait_for(connection,
		   [](Connection &c) {
		       return c.request_kind == proto::RequestKind_CHOICE &&
			      c.request_options.size() == 2;
		   },
		   10.0, "the program's question"),
	  "a program question reaches the client in focus");
    const uint32_t question = connection.request_id;
    send_command(connection, proto::CommandBody_Answer,
		 [question](flatbuffers::FlatBufferBuilder &fbb) {
		     return proto::CreateAnswer(fbb, question, true, 1).Union();
		 });
    check(wait_for(connection,
		   [asked](Connection &c) { return c.ack_serial == asked; }, 10.0,
		   "the answered question") &&
	      connection.ack_ok && connection.ack_choice == 1,
	  "the answer comes back to the program as a value");

    /* A connection that declared observe reads everything and commands
       nothing; the refusal says so instead of dropping the frame. */
    Connection observer;
    observer.fd = tcp_port > 0 ? proto::connect_tcp(tcp_port)
			       : proto::connect_unix(socket_path);
    check(observer.fd >= 0 &&
	      send_hello(observer, -1, true, "protocol-observer"),
	  "observer sends hello");
    check(wait_for(observer,
		   [](Connection &c) { return c.hello_ok && c.status_valid; },
		   10.0, "the observer's status"),
	  "observer receives status");
    const int refused = send_command(
	observer, proto::CommandBody_SetMode,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetMode(fbb, proto::TaskMode_MANUAL).Union();
	});
    wait_for(observer, [refused](Connection &c) { return c.ack_serial == refused; },
	     10.0, "the observer's ack");
    check(!observer.ack_ok &&
	      observer.ack_state == proto::AckState_REFUSED,
	  "an observe connection is refused a command");
    check(observer.got_error &&
	      observer.error_text.find("observe") != std::string::npos,
	  "the refusal says which declaration refused it");
    ::close(observer.fd);

    /* Focus is arbitrated between connections that may command at all. */
    Connection rival;
    rival.fd = tcp_port > 0 ? proto::connect_tcp(tcp_port)
			    : proto::connect_unix(socket_path);
    check(rival.fd >= 0 && send_hello(rival, -1, false, "protocol-rival"),
	  "a second commanding client connects");
    check(wait_for(rival, [](Connection &c) { return c.status_valid; }, 10.0,
		   "the rival's status"),
	  "the rival reads status");
    const int focus_refusal = send_command(
	rival, proto::CommandBody_ClaimFocus,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateClaimFocus(fbb, true).Union();
	});
    wait_for(rival, [focus_refusal](Connection &c) { return c.ack_serial == focus_refusal; },
	     10.0, "the focus refusal");
    check(!rival.ack_ok && rival.ack_state == proto::AckState_REFUSED,
	  "focus is arbitrated by the core");
    check(wait_for(rival,
		   [](Connection &c) {
		       return c.error_text.find("focus is held by") !=
			      std::string::npos;
		   },
		   5.0, "the focus refusal text"),
	  "the focus refusal names the holder");
    ::close(rival.fd);

    command_and_wait(connection, proto::CommandBody_Abort,
		     [](flatbuffers::FlatBufferBuilder &fbb) {
			 return proto::CreateAbort(fbb).Union();
		     },
		     "abort");
    command_and_wait(
	connection, proto::CommandBody_SetState,
	[](flatbuffers::FlatBufferBuilder &fbb) {
	    return proto::CreateSetState(fbb, proto::TaskState_ESTOP).Union();
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
    check(wait_for(replay,
		   [](Connection &c) { return c.got_view && c.view_bytes == 5; },
		   10.0, "the stored view"),
	  "the view state comes back to the name that stored it");
    ::close(replay.fd);

    std::printf("%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL",
		failures);
    return failures == 0 ? 0 : 1;
}
