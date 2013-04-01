// C++ example for:
// encoding a message
// convert to text format
// parse from encoded buffer

#include <iostream>
#include <fstream>
#include <string>
#include <google/protobuf/text_format.h>

#include <protobuf/generated/types.pb.h>
#include <protobuf/generated/value.pb.h>
#include <protobuf/generated/message.pb.h>

#define PB_DEBUG
#include "container.h"

#undef PB2JSON
#ifdef PB2JSON
#include <protobuf/pb2json/pb2json.h>
#endif

using namespace std;
using namespace google::protobuf;


int main(int argc, char* argv[])
{

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    string buffer;
    Container container, got;
    Command *command;
    Originator *origin;
    Object *object;
    Pin *pin;
    size_t payload_size;

    // prepare a submessage for reuse
    origin = new Originator();
    origin->set_origin(PROCESS);
    origin->set_name("gladevcp");
    origin->set_id(234);

    // generate a submessage on the fly:
    command = container.mutable_command();

    command->set_op(REPORT);
    command->set_serial(56789);
    command->set_rsvp(NONE);

    // plug in the prepared origin submessage
    command->set_allocated_origin(origin);

    // add optional submessage(s)
    object = command->add_args();
    object->set_type(HAL_PIN);

    pin = object->mutable_pin();
    pin->set_type(HAL_S32);
    pin->set_name("foo.1.bar");
    pin->set_hals32(4711);

    // fill in the header
    payload_size = command->ByteSize();
    finish_container_header(&container, payload_size, MT_COMMAND);

    // ready to serialize to wire format.
    if (argc == 1) {

	// just dump encoded string to stdout
	if (!container.SerializeToOstream(&cout)) {
	    cerr << "Failed to serialize container." << endl;
	    exit(1);
	}
    } else {

#ifdef PB2JSON

	cout << "Container converted to JSON:"  << endl;
	char *json = pb2json(container);
	cout << json << endl;
	free(json);

#endif
	cout << "varint_size(MT_COMMAND):" <<  varint_size(MT_COMMAND) << endl;
	cout << "payload_size:" <<  payload_size << endl;

	// generate external text representation
	if (TextFormat::PrintToString(container, &buffer)) {
	    cout << "Container: \n" <<  buffer << endl;
	} else {
	    cerr << "Fail" << endl;
	}

	// and convert back from wire format:
	if (!TextFormat::ParseFromString(buffer, &got)) {
	    cerr << "Failed to parse Container message" << endl;
	    return -1;
	} else {
	    cout << "type: "     << got.type() << endl;
	    cout << "length: "   << got.length() << endl;
	}

	if (TextFormat::PrintToString(got, &buffer)) {
	    cout << "FromWire: \n" <<  buffer << endl;
	} else {
	    cerr << "Fail" << endl;
	}
    }
    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
