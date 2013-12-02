// Interpreter internals - Python bindings
// Michael Haberler 7/2011
//

#include <boost/python.hpp>
namespace bp = boost::python;

#include "rs274ngc.hh"

static bp::object pmcartesian_str( PmCartesian &c) {
    return  bp::object("PmCartesian(x=%.4f y=%.4f z=%.4f)" %
		       bp::make_tuple(c.x,c.y,c.z));
}

static bp::object emcpose_2_obj ( EmcPose &p) {
    return  bp::object("x=%.4f y=%.4f z=%.4f a=%.4f b=%.4f c=%.4f u=%.4f v=%.4f w=%.4f" %
		       bp::make_tuple(p.tran.x,p.tran.y,p.tran.z,
				      p.a,p.b,p.c,p.u,p.v,p.w));
}
static bp::object emcpose_str( EmcPose &p) {
    return  bp::object("EmcPose(" + emcpose_2_obj(p) + ")");
}

static void  set_x(EmcPose &p, double value) { p.tran.x = value; }
static void  set_y(EmcPose &p, double value) { p.tran.y = value; }
static void  set_z(EmcPose &p, double value) { p.tran.z = value; }
static double get_x(EmcPose &p) { return p.tran.x; }
static double get_y(EmcPose &p) { return p.tran.y; }
static double get_z(EmcPose &p) { return p.tran.z; }

static bp::object tool_str( CANON_TOOL_TABLE &t) {
    return  bp::object("Tool(T%d D%.4f I%.4f J%.4f Q%d offset: " %
		       bp::make_tuple(t.toolno,  t.diameter,
				      t.frontangle,t.backangle, t.orientation) +
		       emcpose_2_obj(t.offset) + ")");
}

static void tool_zero( CANON_TOOL_TABLE &t) {
    t.toolno = -1;
    ZERO_EMC_POSE(t.offset);
    t.diameter = 0.0;
    t.frontangle = 0.0;
    t.backangle = 0.0;
    t.orientation = 0;
}
static void carte_zero( PmCartesian &c) { 
    c.x = 0.0;
    c.y = 0.0;
    c.z = 0.0;
}

static void pose_zero( EmcPose &p) { ZERO_EMC_POSE(p); }


void export_EmcTypes()
{
    using namespace boost::python;
    using namespace boost;

    // PmCartesian and EmcPose make sense to be instantiable 
    // within Python, since some canon calls take these as parameter
    class_<PmCartesian, noncopyable>("PmCartesian","EMC cartesian postition")
	.def_readwrite("x",&PmCartesian::x)
	.def_readwrite("y",&PmCartesian::y)
	.def_readwrite("z",&PmCartesian::z)
	.def("__str__", &pmcartesian_str)
	.def("zero", &carte_zero)
	;

    class_<EmcPose>("EmcPose","EMC pose")
	.def_readwrite("tran",&EmcPose::tran)
	.add_property("x", &get_x, &set_x)
	.add_property("y", &get_y, &set_y)
	.add_property("z", &get_z, &set_z)
	.def_readwrite("a",&EmcPose::a)
	.def_readwrite("b",&EmcPose::b)
	.def_readwrite("c",&EmcPose::c)
	.def_readwrite("u",&EmcPose::u)
	.def_readwrite("v",&EmcPose::v)
	.def_readwrite("w",&EmcPose::w)
	.def("__str__", &emcpose_str)
	.def("zero", &pose_zero)
	;

    // leave CANON_TOOL_TABLE copyable/assignable because assignment is
    // used a lot when fiddling with tooltable entries
    class_<CANON_TOOL_TABLE >("CANON_TOOL_TABLE","Tool description" ,no_init)
	.def_readwrite("toolno", &CANON_TOOL_TABLE::toolno)
	.def_readwrite("offset", &CANON_TOOL_TABLE::offset)
	.def_readwrite("diameter", &CANON_TOOL_TABLE::diameter)
	.def_readwrite("frontangle", &CANON_TOOL_TABLE::frontangle)
	.def_readwrite("backangle", &CANON_TOOL_TABLE::backangle)
	.def_readwrite("orientation", &CANON_TOOL_TABLE::orientation)
	.def("__str__", &tool_str)
	.def("zero", &tool_zero)
	;
}
