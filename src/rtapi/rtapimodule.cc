// RTAPI (really ULAPI) Python bindings
//
// RT logger
// basic introspection

// mah@wheezy:~/linuxcnc-ubc3/src$ python
// Python 2.7.6 (default, Jan 11 2014, 17:06:02)
// [GCC 4.8.2] on linux2
// Type "help", "copyright", "credits" or "license" for more information.
// >>> import rtapi
// >>> rtapi.kernel_type()
// 'rtpreempt'
// >>> rtapi.flavor_name
// 'rt-preempt'
// >>>
//
// NB: this assumes realtime is running.
//
// Michael Haberler 11/2013

#include <cstddef>  // std::size_t
#include <string>
using namespace std;

#include <boost/python.hpp>
using namespace boost::python;

#include "rtapi.h"
#include "rtapi_compat.h"

static string whitespaces (" \t\f\v\n\r");

static string chopws(const string &t, const string &ws)
{
    string str = t;
    size_t found;
    found = str.find_last_not_of(ws);
    if (found != string::npos)
	str.erase(found+1);
    else
	str.clear();            // str is all whitespace
    return str;
}

static int rtapi_module_id;

// if this process doesnt happen to have a HAL module initialized already
// (which causes an ulapi autoload), autoload ulapi by initializing
// a dummy RTAPI module
void init_rtapi(void) {
    if (!ulapi_loaded()) {
	char module_name[30];
	snprintf(module_name, sizeof(module_name), "pyrtapi%d", getpid());
	rtapi_module_id = rtapi_init(module_name);
    }
    if (!ulapi_loaded())
        throw std::runtime_error( "RTAPI: Realtime not running" );
}

static const char *kernel_type(void)
{
    if (kernel_is_rtai())
	return "rtai";
    if (kernel_is_xenomai())
	return "xenomai";
    if (kernel_is_rtpreempt())
	return "rtpreempt";
    return "posix";
}

class RTAPILogger {
private:

public:
    int level;

    RTAPILogger() : level(RTAPI_MSG_ALL) { rtapi_set_logtag("pyrtapi"); }
    RTAPILogger(int l) : level(l)        { rtapi_set_logtag("pyrtapi"); }
    RTAPILogger(const char *t) : level(RTAPI_MSG_ALL) { rtapi_set_logtag(t); }
    RTAPILogger(int l, const char *t) : level(l)      { rtapi_set_logtag(t); }

#pragma GCC diagnostic ignored "-Wformat-security"
    void write(string line) {
	string trimmed = chopws(line, whitespaces);
	if (trimmed.size())
	    rtapi_print_msg(level, trimmed.c_str());
    }
#pragma GCC diagnostic warning "-Wformat-security"
    void flush(void) {}
    // void del(void) { }
    // void enter(void) {}
    // void exit(bp::object type, bp::object value, bp::object traceback) {}
    const char *get_tag(void) { return rtapi_get_logtag(); }
    void set_tag(const char *tag) { rtapi_set_logtag(tag); }

};

extern flavor_ptr flavor;  // ulapi_autoload.c

BOOST_PYTHON_MODULE(rtapi) {

    init_rtapi();
    flavor = flavor_byid(global_data->rtapi_thread_flavor);

    scope().attr("MSG_NONE")  = (int) RTAPI_MSG_NONE;
    scope().attr("MSG_ERR")  = (int) RTAPI_MSG_ERR;
    scope().attr("MSG_WARN")  = (int) RTAPI_MSG_WARN;
    scope().attr("MSG_INFO")  = (int) RTAPI_MSG_INFO;
    scope().attr("MSG_DBG")  = (int) RTAPI_MSG_DBG;
    scope().attr("MSG_ALL")  = (int) RTAPI_MSG_ALL;

    // introspection
    scope().attr("rtlevel")  = global_data->rt_msg_level;
    scope().attr("userlevel")  = global_data->user_msg_level;
    scope().attr("instance")  = global_data->instance_id;
    scope().attr("instance_name")  = global_data->instance_name;
    scope().attr("flavor_name")  = flavor->name;
    scope().attr("flavor_id")  = global_data->rtapi_thread_flavor;
    scope().attr("build_sys")  = flavor->build_sys;
    scope().attr("flavor_flags")  = flavor->flags;
    scope().attr("git_tag")  = rtapi_switch->git_version;

    // flavor flag bits
    scope().attr("DOES_IO")  = (int) FLAVOR_DOES_IO;
    scope().attr("KERNEL_BUILD")  = (int) FLAVOR_KERNEL_BUILD;
    scope().attr("RTAPI_DATA_IN_SHM")  = (int) FLAVOR_RTAPI_DATA_IN_SHM;


    def("kernel_type", &kernel_type);


    // duck-type a File object so one can do this:
    // self.rtapi = RTAPILogger(MSG_ERR,"logtag")
    // print >> self.rtapi, "some message"

    class_<RTAPILogger>("RTAPILogger")

	.add_property("level", &RTAPILogger::level)
	.add_property("tag", &RTAPILogger::get_tag, &RTAPILogger::set_tag)

	.def(init<int>())
	.def(init<int, char *>())
	.def(init<char *>())
	.def("write", &RTAPILogger::write)
	.def("flush", &RTAPILogger::flush)
	;
}
