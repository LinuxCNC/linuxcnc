// C++ example for:
// encoding a message
// convert to text format
// parse from encoded buffer

#include <iostream>
#include <fstream>
#include <string>

#include <google/protobuf/text_format.h>

#include <machinetalk/generated/types.pb.h>
#include <machinetalk/generated/value.pb.h>
#include <machinetalk/generated/message.pb.h>
#include <machinetalk/generated/object.pb.h>

#include <json2pb.hh>

using namespace pb;
using namespace std;
using namespace google::protobuf;

int main(int argc, char* argv[])
{

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    string buffer;
    Container container, got;
    Pin *pin;
    Value *value;

    // type-tag the container:
    container.set_type(MT_HALUPDATE);
    container.set_serial(56789);
    container.set_rsvp(NONE);


    // add repeated submessage(s)
    pin = container.add_pin();
    pin->set_type(HAL_S32);
    pin->set_name("foo.1.bar");
    pin->set_hals32(4711);

    value = container.add_value();
    value->set_type(DOUBLE);
    value->set_v_double(3.14159);

    // ready to serialize to wire format.
    if (argc == 1) {

	// just dump encoded string to stdout
	if (!container.SerializeToOstream(&cout)) {
	    cerr << "Failed to serialize container." << endl;
	    exit(1);
	}
    } else {
	cout << "Container converted to JSON:"  << endl;
	std::string json = pb2json(container);
	cout << json << endl;

	Container c;
	try{
	    json2pb(c, json.c_str(), strlen(json.c_str()));
	} catch (std::exception &ex) {
	    printf("json2pb exception: %s\n", ex.what());
	}
	// printf("c.DebugString=%s\n",c.DebugString().c_str());

	if (TextFormat::PrintToString(c, &buffer)) {
	    cout << "FromJson: \n" <<  buffer << endl;
	} else {
	    cerr << "Fail" << endl;
	}
	// generate external text representation
	if (TextFormat::PrintToString(container, &buffer)) {
	    cout << "Container: \n" <<  buffer << endl;
	} else {
	    cerr << "Fail" << endl;
	}

	// and convert back from wire format:
	if (!TextFormat::ParseFromString(buffer, &got)) {
	    cerr << "Failed to parse Container " << endl;
	    return -1;
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
