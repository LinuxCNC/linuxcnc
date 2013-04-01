// Python bindings for hal group/member_* methods
//
// Michael Haberler 2/2013


#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <map>

#include "hal.h"
#include "hal_priv.h"

namespace bp = boost::python;

static int comp_id;

static void hal_startup(void)
{
    char msg[LINELEN];
    char name[HAL_NAME_LEN + 1];
    int retval;

    rtapi_snprintf(name, sizeof(name), "pygroup%d", getpid());
    comp_id = hal_init(name);
    if (comp_id < 0) {
	rtapi_snprintf(msg, sizeof(msg), "hal_init(%s) failed: %s",
		       name, strerror(-comp_id));
        PyErr_SetString(PyExc_RuntimeError, msg);
        throw boost::python::error_already_set();
	//FIXME	throw std::runtime_error("parameter does not exist: " + std::string(varname));
    }
    retval = hal_ready(comp_id);
    if (retval < 0) {
	rtapi_snprintf(msg, sizeof(msg), "hal_ready(%s,%d) failed: %s",
		       name, comp_id, strerror(-retval));
        PyErr_SetString(PyExc_RuntimeError, msg);
        throw boost::python::error_already_set();
    }
}

__attribute__((destructor))
static void end (void)
{
    if (comp_id > 0)
	hal_exit(comp_id);
}

class HalGroup  {

private:
    hal_group_t *hg;

public:
    HalGroup() : hg(NULL) {}

    // attach to existing ring
    HalGroup(const char *name) : hg(NULL) {
	printf("--HalGroup ctor\n");
	// int retval;

	// if ((retval = hal_ring_attach(name, &rb))) {
	//     char msg[LINELEN];
	//     rtapi_snprintf(msg, sizeof(msg), "hal_ring_attach(): no such ring: '%s': %s",
	// 		   name, strerror(-retval));
	//     PyErr_SetString(PyExc_NameError, msg);
	//     throw boost::python::error_already_set();
	// }
    }
    ~HalGroup()  {
	printf("--HalGroup dtor\n");
    }

    int show() { printf("--show\n"); return 4711; }
    // int get_reader()       { return rb->reader; }
    // void set_reader(int r) { rb->reader = r; }
};

static  HalGroup *HalGroupFactory(const char *name) { return new HalGroup(name); }

// TCurrency* TCurrency_from_Foo( const Foo& ) { return new TCurrency(); }
// static boost::python::object pythis =  boost::python::object(boost::cref(this));

// 	// alias to 'interpreter.this' for the sake of ';py, .... ' comments
// 	// besides 'this', eventually use proper instance names to handle
// 	// several instances
// 	bp::scope(interp_module).attr("this") =  _setup.pythis;

//typedef std::map<const char *, HalGroup > GroupMap;
// class GroupMap {

// public:
//     GroupMap() {}
//     ~GroupMap() {}
//     void clear() {}
// }

using namespace boost::python;


typedef std::string Key;
typedef double Val;
typedef std::map<Key,Val> Map;
#include "map_item.hpp"

BOOST_PYTHON_MODULE(group) {

    scope().attr("__doc__") = "HAL Group and Member support";

    class_<Map,boost::noncopyable>("Map",no_init)
	.def("__len__", &Map::size)
	.def("__getitem__", &map_item<Key,Val>().get,
	     return_value_policy<copy_non_const_reference>() )

	.def("__setitem__", &map_item<Key,Val>().set)
	.def("__delitem__", &map_item<Key,Val>().del)
	.def("clear", &Map::clear)
	.def("__contains__", &map_item<Key,Val>().in)
	.def("has_key", &map_item<Key,Val>().in)
	.def("keys", &map_item<Key,Val>().keys)
	.def("values", &map_item<Key,Val>().values)
	.def("items", &map_item<Key,Val>().items)
	;

    class_<HalGroup>("HalGroup")
	.def("show", &HalGroup::show)
	;

    hal_startup();
    scope().attr("comp_id") = comp_id;

    def("newgroup", HalGroupFactory, return_value_policy<manage_new_object>());
    // def("",  );
    // static GroupMap *gm = new GroupMap();
    scope().attr("groups") = bp::object(boost::cref(new Map()));
}
