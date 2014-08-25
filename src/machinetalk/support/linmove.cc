// example for message creation

#include <iostream>
#include <string>

#include <google/protobuf/text_format.h>

#include <machinetalk/generated/types.pb.h>
#include <machinetalk/generated/canon.pb.h>
#include <machinetalk/generated/emcclass.pb.h>
#include <machinetalk/generated/message.pb.h>

#include <json2pb.hh>

using namespace std;
using namespace google::protobuf;

enum axis_mask {
    X_AXIS= (1 << 0),
    Y_AXIS= (1 << 1),
    Z_AXIS= (1 << 2),
    A_AXIS= (1 << 3),
    B_AXIS= (1 << 4),
    C_AXIS= (1 << 5),
    U_AXIS= (1 << 6),
    V_AXIS= (1 << 7),
    W_AXIS= (1 << 8)
};


void STRAIGHT_FEED(unsigned mask,
		   pb::Container &msg,
		   int lineno,
		   double x, double y, double z,
		   double a, double b, double c,
		   double u, double v, double w)
{
    // Container type tag
    msg.set_type(pb::MT_EMC_TRAJ_LINEAR_MOVE);

    // Container has a optional line_number field.
    // use it - unused ones carry no cost in space and time
    msg.set_line_number(lineno);

    // this instantiates an optional submessage of Container
    pb::Emc_Traj_Linear_Move *m = msg.mutable_traj_linear_move();

    // the move type
    m->set_type(pb::_EMC_MOTION_TYPE_FEED);

    // fill in the optional axes as commanded by mask
    pb::EmcPose *p = m->mutable_end();
    pb::PmCartesian *t = p->mutable_tran();

    if (mask & X_AXIS) t->set_x(x);
    if (mask & Y_AXIS) t->set_y(y);
    if (mask & Z_AXIS) t->set_z(z);

    if (mask & A_AXIS) p->set_a(a);
    if (mask & B_AXIS) p->set_b(b);
    if (mask & C_AXIS) p->set_c(c);

    if (mask & U_AXIS) p->set_u(u);
    if (mask & V_AXIS) p->set_v(v);
    if (mask & W_AXIS) p->set_w(w);

    // fake canon member variable access
    m->set_vel(123.0);
    m->set_ini_maxvel(456.0);
    m->set_acc(42.0);
    m->set_feed_mode(1);
    m->set_indexrotary(0);
}

int main(int argc, char* argv[])
{
    int axes = -1;
    if (argc > 1)
	axes = atoi(argv[1]);

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;


    pb::Container c; // all messages are wrapped in a Container

    STRAIGHT_FEED(axes, c,
		  42,
		  10.0, 20.0, 30.0,
		  40.0, 50.0, 60.0,
		  70.0, 80.0, 90.0);


    // the following stunts on c are available:

    // serialize to protobuf wire format
    string wireformat;
    assert(c.SerializeToString(&wireformat));

    // generate external text representation
    string text;
    if (TextFormat::PrintToString(c, &text)) {
	cout << "Container: \n" <<  text << endl;
    } else {
	cerr << "Fail" << endl;
    }

    // convert from text representation to instance:
    // easy to create a message with an editor
    pb::Container got;
    if (!TextFormat::ParseFromString(text, &got)) {
	cerr << "Failed to parse '" << text << "'" << endl;
	return -1;
    }

    // a message instance can also be serialized to json, thanks to Pavel:
    std::string json = pb2json(got);
    cout << "Container converted to JSON:" << json  << endl;

    // and parsed back from json into a message instance with full
    // type checking:

    pb::Container fromjson;

    try{
	json2pb(fromjson, json.c_str(), strlen(json.c_str()));
    } catch (std::exception &ex) {
	printf("json2pb exception: %s\n", ex.what());
    }

    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
