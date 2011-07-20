
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


void foo_init()
{
    printf("foo init\n");
}

int
main (int argc, char **argv)
{
    int reload = 0;
    char *modpath = NULL;
    char *module = NULL;
    char *internmod = NULL;
    int addpath = 0;
    int index;
    int c;
    bp::object retval;
    char *callablefunc = NULL;
    char *callablemod = NULL;
    char *xcallable = NULL;

    PythonPlugin pp;

    opterr = 0;

    while ((c = getopt (argc, argv, "rM:m:i:aC:c:x:")) != -1) {
	switch (c)  {
	case 'r':
	    reload = 1;
	    break;
	case 'M':
	    modpath = optarg;
	    break;
	case 'm':
	    module = optarg;
	    break;
	case 'i':
	    internmod = optarg;
	    break;
	case 'a':
	    addpath = 1;
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
    // printf ("aflag = %d, bflag = %d, cvalue = %s\n",
    // 	    aflag, bflag, cvalue);
    CHK(pp.setup(modpath,module,reload) != PLUGIN_OK, "setup failed\n");
    if (internmod)
	CHK(pp.add_inittab_entry("foo", foo_init),"add_inittab(%s) failed\n",internmod);
    CHK(pp.initialize(addpath) != PLUGIN_OK, "initialize failed\n");

    if (callablefunc) {
	bool result = pp.is_callable(callablemod, callablefunc);
	printf("callable(%s.%s) = %d\n",callablemod, callablefunc,result);
    }

    if (xcallable) {
	bp::tuple tupleargs;
	bp::dict kwargs;
	kwargs['x'] = 10;
	kwargs['y'] = 20;
	tupleargs = bp::make_tuple(3,2,1,"foo");
	CHK(pp.call(callablemod,xcallable,tupleargs,kwargs,retval), "call(%s.%s): fail",callablemod,xcallable);

    }

    for (index = optind; index < argc; index++) {
	CHK(pp.run_string( argv[index], retval)  != PLUGIN_OK, "runstring(%s) failed\n", argv[index] );

    }
    return 0;
}
