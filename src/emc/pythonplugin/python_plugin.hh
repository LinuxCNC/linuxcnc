#ifndef PYTHON_PLUGIN_HH
#define PYTHON_PLUGIN_HH

#include <boost/python.hpp>
namespace bp = boost::python;

#include <vector>
#include <string>
#include <sys/types.h>


extern std::string handle_pyerror();


// PY_EXCEPTION: both exception_msg and error_msg are set
// PY_ERROR:  error_msg is set
enum pp_status   {
    PLUGIN_NO_SECTION = -1,
    PLUGIN_NO_INIFILE = -2,
    PLUGIN_BAD_INIFILE = -3,
    PLUGIN_NO_TOPLEVEL = -4,
    PLUGIN_BAD_PATH = -5,
    PLUGIN_STAT_FAILED = -6,
    PLUGIN_INITTAB_FAILED = -7 ,
    PLUGIN_PYTHON_ALREADY_INITIALIZED = -8,
    PLUGIN_EXCEPTION_DURING_PATH_PREPEND = -9,
    PLUGIN_EXCEPTION_DURING_PATH_APPEND = -10,
    PLUGIN_INIT_EXCEPTION = -11,
    PLUGIN_PYTHON_NOT_INITIALIZED = -12,
    PLUGIN_PATH_TOO_LONG = -13,

    // errors < OK make run_string(), is_callable(), and call() fail immediately
    // a reload might clear the error
    PLUGIN_OK = 0,
    PLUGIN_NO_CALLABLE = 1,
    PLUGIN_EXCEPTION = 2
};

class PythonPlugin {
public:
    // factory method
    static PythonPlugin *instantiate(struct _inittab *inittab = NULL);
    int configure(const char *iniFilename = NULL, const char *section = NULL);
    bool is_callable(const char *module, const char *funcname);
    int call(const char *module,const char *callable,
	     bp::object tupleargs, bp::object kwargs, bp::object &retval);
    int run_string(const char *cmd, bp::object &retval, bool as_file = false);
    int call_method(bp::object method, bp::object &retval);

    int plugin_status() { return status; };
    bool usable() { return (status >= PLUGIN_OK); }
    int initialize();
    std::string last_exception() { return exception_msg; };
    std::string last_errmsg() { return error_msg; };
    bp::object main_namespace;

private:
    PythonPlugin(struct _inittab *inittab);       // nb: no public constructor
    PythonPlugin(const PythonPlugin &) {};        // not copyable
    PythonPlugin & operator=(const PythonPlugin&) { return *this; };  // not assignable
    ~PythonPlugin() {};

    int reload();
    std::vector<std::string> inittab_entries;
    int status;
    time_t module_mtime;                  // toplevel module - last modification time
    bool reload_on_change;                // auto-reload if toplevel module was changed
    const char *ini_filename;
    const char *section;
    struct _inittab *inittab_pointer;
    const char *toplevel;          // toplevel script
    //    const char *plugin_dir;               // directory prefix
    const char *abs_path;                 // normalized path to toplevel module
    std::string exception_msg;
    std::string error_msg;
    int log_level;
};

#endif
