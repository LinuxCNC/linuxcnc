#ifndef PYTHON_PLUGIN_HH
#define PYTHON_PLUGIN_HH

#include <boost/python.hpp>
namespace bp = boost::python;

#include <vector>
#include <string>
#include <sys/types.h>


// PY_EXCEPTION: both exception_msg and error_msg are set
// PY_ERROR:  error_msg is set
enum pp_status   {
    PLUGIN_NO_SECTION = -1,
    PLUGIN_NO_INIFILE = -2,
    PLUGIN_BAD_INIFILE = -3,
    PLUGIN_NO_MODULE_BASENAME = -4,
    PLUGIN_BAD_PATH = -5,
    PLUGIN_STAT_FAILED = -6,
    PLUGIN_INITTAB_FAILED = -7 ,
    PLUGIN_PYTHON_ALREADY_INITIALIZED = -8,
    PLUGIN_EXCEPTION_DURING_PATH_APPEND = -9,
    PLUGIN_INIT_EXCEPTION = -10,
    PLUGIN_NO_PLUGIN_DIR = -11,
    // errors < OK make run_string(), is_callable(), and call() fail immediately
    // a reload might clear the error
    PLUGIN_OK = 0,
    PLUGIN_NO_CALLABLE = 1,
    PLUGIN_EXCEPTION = 2
};

class PythonPlugin {
public:
    static PythonPlugin *getInstance(const char *iniFilename = NULL,
				     const char *section = NULL,
				     struct _inittab *inittab = NULL);
    bool is_callable(const char *module, const char *funcname);
    int call(const char *module,const char *callable,
	     bp::object tupleargs, bp::object kwargs, bp::object &retval);
    int run_string(const char *cmd, bp::object &retval, bool as_file = false);

    int plugin_status();
    void initialize(bool reload = false);
    std::string last_exception();
    std::string last_errmsg();

private:
    PythonPlugin(const char *iniFilename,         // no public constructor
		 const char *section,
		 struct _inittab *inittab);
    PythonPlugin(const PythonPlugin &) {};        // not copyable
    PythonPlugin & operator=(const PythonPlugin&) { return *this; };  // not assignable
    ~PythonPlugin();                              // no public destructor


    static PythonPlugin *instance;

    int reload();
    std::string handle_pyerror();

    std::vector<std::string> inittab_entries;
    int status;
    time_t module_mtime;                  // toplevel module - last modification time
    bool reload_on_change;                // auto-reload if toplevel module was changed
    const char *ini_filename;
    const char *section;
    struct _inittab *inittab_pointer;
    const char *module_basename;          // toplevel module
    const char *plugin_dir;               // directory prefix
    const char *abs_path;                 // normalized path to toplevel module, ProgramName
    bp::object module_namespace;
    std::string exception_msg;
    std::string error_msg;
    int log_level;
};

#endif
