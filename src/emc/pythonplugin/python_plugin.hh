#ifndef PYTHON_PLUGIN_HH
#define PYTHON_PLUGIN_HH

#include <boost/python.hpp>
namespace bp = boost::python;

#include <vector>
#include <string>
#include <sys/types.h>
// #include <sys/stat.h>

// PY_EXCEPTION: both exception_msg and error_msg are set
// PY_ERROR:  error_msg is set

enum py_retcode {PLUGIN_OK=0, PLUGIN_EXCEPTION=1, PLUGIN_ERROR=2, PLUGIN_NOTCALLABLE=3};

enum pymod_stat {PYMOD_NONE=0, PYMOD_FAILED=1,PYMOD_OK=2};

class PythonPlugin {


public:
    PythonPlugin(int loglevel = 0);
    ~PythonPlugin();
    int setup(const char *modpath, const char *module, bool reload_if_changed = false);
    int add_inittab_entry(const char *mod_name, void (*mod_init)());
    int initialize(bool reload = false);
    int run_string(const char *cmd, bp::object &retval);
    int call(const char *module,const char *callable,
	     bp::object tupleargs, bp::object kwargs, bp::object &retval);
    bool is_callable(const char *module, const char *funcname);
    int plugin_status();

    std::string last_exception();
    std::string last_errmsg();

private:
    int reload();
    std::string handle_pyerror();

    std::vector<std::string> modules;
    bool python_exception;
    int module_status;
    bool reload_on_change;   // auto-reload if toplevel module was changed
    const char *py_dir;      // plugin directory
    const char *module;   // toplevel module
    char module_path[PATH_MAX];
    time_t     module_mtime;  // top level module - last modification time
    bp::object module_namespace;

    std::string exception_msg;
    std::string error_msg;
    int log_level;
};

#endif
