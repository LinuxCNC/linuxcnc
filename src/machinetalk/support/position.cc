// C++ example for:
// encoding a message
// convert to text format
// parse from encoded buffer

#include <iostream>
#include <fstream>
#include <string>

#include <google/protobuf/text_format.h>

#include <machinetalk/generated/message.pb.h>
#include <json2pb.hh>

using namespace pb;
using namespace std;
using namespace google::protobuf;

int main(int argc, char* argv[])
{

    GOOGLE_PROTOBUF_VERIFY_VERSION;
    Container c;
    Preview *p;
    string buffer, text;

    c.set_type(MT_PREVIEW);

    p = c.add_preview();

    p->set_type(PV_STRAIGHT_FEED);
    p->set_line_number(4711);
    p->set_kins(KT_JOINT);
    p->set_axismask(1<<1|1<<8);
    Position *pos = p->mutable_pos();

    pos->set_x(42.0);
    pos->set_a(3.14);

    if (TextFormat::PrintToString(c, &text)) {
	cout << "PrintToString: \n" <<  text << endl;
    } else {
	cerr << "Fail" << endl;
    }

    if (!c.SerializeToString(&buffer)) {
	    cerr << "Failed to serialize Preview." << endl;
	    exit(1);
    }

    // read back
    Container q;

    q.ParseFromString(buffer);

    // const Reflection* qr = q.GetReflection();
    // const Descriptor* qd = q.GetDescriptor();
    // const FieldDescriptor* start_field = qd->FindFieldByName("x1");


    // if (qr->HasField(q, start_field)) {

    // 	cerr << "has start." << endl;
    // 	// const Descriptor* descriptor = q.start.GetDescriptor();
    // 	// const FieldDescriptor* start_field = descriptor->FindFieldByName("x1");

    // 	// cerr <<  r->GetDouble(q, _field, 1) << endl;
    // 	// cerr <<  r->GetDouble(q, start_field, 8) << endl;
    // 	// cerr <<  r->GetDouble(q, start_field, 2) << endl;

    // }


    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
