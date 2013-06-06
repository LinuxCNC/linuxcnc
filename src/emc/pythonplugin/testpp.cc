// test harness for python_plugin

#include "python_plugin.hh"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHK(bad, fmt, ...)					       \
    do {							       \
	if (bad) {						       \
	    fprintf(stderr,fmt, ## __VA_ARGS__);		       \
	    exit(1);						       \
	}							       \
    } while(0)

const char *strstore(const char *s) { return strdup(s); }
extern PythonPlugin *python_plugin;

void analyze(const char *what,bp::object retval)
{
    PyObject *res_str = PyObject_Str(retval.ptr());
    Py_XDECREF(res_str);
    printf("analyze: %s returned '%s' - '%s'\n",what,
	   PyString_AsString(res_str),
	   retval.ptr()->ob_type->tp_name);
}

void exercise(PythonPlugin *pp,const char *mod, const char*func,  bp::tuple tupleargs,   bp::dict kwargs)
{

    bp::object r;

    bool callable = pp->is_callable(mod,func);
    printf("callable(%s%s%s) = %s\n",mod ? mod : "", mod ? ".":"",
	   func, callable ? "TRUE": "FALSE");


    int status = pp->call(mod,func, tupleargs,kwargs,r);
    switch (status) {
    case PLUGIN_EXCEPTION:
	printf("call(%s%s%s): exception='%s' status = %d\n",
	       mod ? mod : "", mod ? ".":"",func,pp->last_exception().c_str(), status);
	break;
    case PLUGIN_OK:
	printf("call(%s%s%s): OK\n",  mod ? mod : "", mod ? ".":"",func);
	analyze( func,r);
	break;
    default:
	printf("call(%s%s%s): status = %d\n", mod ? mod : "", mod ? ".":"",func,status);
    }
}

void run(PythonPlugin *pp,const char *cmd, bool as_file)
{
    bp::object r;
    int status = pp->run_string(cmd,r, as_file);
    printf("run_string(%s): status = %d\n", cmd,status);
    analyze(cmd,r);
}

void foo_init()
{
    printf("foo init\n");
}
void bar_init()
{
    printf("bar init\n");
}

const char *inifile = "test.ini";
const char *section = "PYTHON";
struct _inittab builtin_modules[] = {
    { (char *) "foo", foo_init },
    { (char *) "bar", bar_init },
    // any others...
    { NULL, NULL }
};

int builtins;
bool as_file = false;
int
main (int argc, char **argv)
{

    int index;
    int c;
    bp::object retval, pythis;
    char *callablefunc = NULL;
    char *callablemod = NULL;
    char *xcallable = NULL;
    int status;

    opterr = 0;

    while ((c = getopt (argc, argv, "fbi:C:c:x:")) != -1) {
	switch (c)  {
	case 'f':
	    as_file = true;
	    break;
	case 'b':
	    builtins++;
	    break;
	case 'i':
	    inifile = optarg;
	    break;
	case 'C':
	    callablemod = optarg;
	    break;
	case 'c':
	    callablefunc = optarg;
	    break;
	case 'x':
	    xcallable = optarg;
	    break;
	case '?':
	    if (optopt == 'c')
		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
	    else if (isprint (optopt))
		fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	    else
		fprintf (stderr,
			 "Unknown option character `\\x%x'.\n",
			 optopt);
	    return 1;
	default:
	    abort ();
	}
    }

    if (!PythonPlugin::instantiate(builtins ? builtin_modules : NULL)) {
	fprintf (stderr,"instantiate: cant instantiate Python plugin");
    }

    // creates the singleton instance
    status = python_plugin->configure(inifile, section);

    if (status != PLUGIN_OK) {
	fprintf (stderr, "configure failed: %d\n",status);
    }

    // PythonPlugin two = python_plugin; // this fails since copy constructor is private.
    // PythonPlugin &second = PythonPlugin::getInstance();  // returns the singleton instance

    printf("status = %d\n", python_plugin->plugin_status());
    bp::tuple tupleargs,nulltupleargs;
    bp::dict kwargs,nullkwargs;
    kwargs['x'] = 10;
    kwargs['y'] = 20;
    tupleargs = bp::make_tuple(3,2,1,"foo");

    exercise(python_plugin,NULL,"func",nulltupleargs, nullkwargs);

    system("/usr/bin/touch testmod.py");

    exercise(python_plugin,NULL,"badfunc",tupleargs,kwargs);
    exercise(python_plugin,NULL,"retstring",tupleargs,kwargs);
    exercise(python_plugin,NULL,"retdouble",tupleargs,kwargs);
    // exercise(python_plugin,"submod","subfunc");

    for (index = optind; index < argc; index++) {
	run(python_plugin, argv[index], as_file);
    }
    return 0;
}
