/* Copyright (C) 2006-2008 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2012-2014 Michael Haberler <license@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * TODO: on setuid and capabilites(7)
 *
 * right now this program runs as setuid root
 * it might be possible to drop the wholesale root privs by using
 * capabilites(7). in particular:
 *
 * CAP_SYS_RAWIO   open /dev/mem and /dev/kmem & Perform I/O port operations
 * CAP_SYS_NICE    set real-time scheduling policies, set CPU affinity
 * CAP_SYS_MODULE  Load  and  unload  kernel  modules
 *
 * NB:  Capabilities are a per-thread attribute,
 * so this might need to be done on a per-thread basis
 * see also CAP_SETPCAP, CAP_INHERITABLE and 'inheritable set'
 *
 * see also:
 * http://stackoverflow.com/questions/13183327/drop-root-uid-while-retaining-cap-sys-nice
 * http://stackoverflow.com/questions/12141420/losing-capabilities-after-setuid
 */

#include "config.h"


#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sys/resource.h>
#include <linux/capability.h>
#include <sys/io.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <malloc.h>
#include <assert.h>
#include <syslog_async.h>
#include <limits.h>
#include <sys/prctl.h>
#include <inifile.h>

#include <czmq.h>
#include <google/protobuf/text_format.h>

#include <machinetalk/generated/message.pb.h>
#include <pbutil.hh>  // note_printf(pb::Container &c, const char *fmt, ...)

using namespace google::protobuf;

#include "rtapi.h"
#include "rtapi_global.h"
#include "rtapi_compat.h"
#include "rtapi_export.h"
#include "hal.h"
#include "hal_priv.h"
#include "rtapi/shmdrv/shmdrv.h"

#include "mk-backtrace.h"
#include "setup_signals.h"
#include "mk-zeroconf.hh"

#define BACKGROUND_TIMER 1000

using namespace std;

/* Pre-allocation size. Must be enough for the whole application life to avoid
 * pagefaults by new memory requested from the system. */
#define PRE_ALLOC_SIZE		(30 * 1024 * 1024)

template<class T> T DLSYM(void *handle, const string &name) {
    return (T)(dlsym(handle, name.c_str()));
}

template<class T> T DLSYM(void *handle, const char *name) {
    return (T)(dlsym(handle, name));
}
typedef int (*hal_call_usrfunct_t)(const char *name,
				   const int argc,
				   const char **argv,
				   int *ureturn);
static hal_call_usrfunct_t call_usrfunct;

static std::map<string, void*> modules;
static std::vector<string> loading_order;
static void remove_module(std::string name);

static struct rusage rusage;
static unsigned long minflt, majflt;
static int instance_id;
flavor_ptr flavor;
static int use_drivers = 0;
static int foreground;
static int debug;
static int signal_fd;
static bool interrupted;
static bool trap_signals = true;
int shmdrv_loaded;
long page_size;
static const char *progname;
static const char *z_uri;
static int z_port;
static uuid_t process_uuid;
static char process_uuid_str[40];
static register_context_t *rtapi_publisher;
static const char *service_uuid;

#ifdef NOTYET
static int remote = 0; // announce and bind a TCP socket
static const char *ipaddr = "127.0.0.1";
static const char *z_uri_dsn;
#endif

static const char *interfaces;
static const char *inifile;
static FILE *inifp;

#ifdef NOTYET
static AvahiCzmqPoll *av_loop;
#endif

// the following two variables, despite extern, are in fact private to rtapi_app
// in the sense that they are not visible in the RT space (the namespace 
// of dlopen'd modules); these are supposed to be 'ships in the night'
// relative to any symbols exported by rtapi_app.
//
// global_data is set in attach_global_segment() which was already 
// created by rtapi_msgd
// rtapi_switch is set once rtapi.so has been loaded by calling the 
// rtapi_get_handle() method in rtapi.so.
// Once set, rtapi methods in rtapi.so can be called normally through
// the rtapi_switch redirection (see rtapi.h).

// NB: do _not_ call any rtapi_* methods before these variables are set
// except for rtapi_msg* and friends (those do not go through the rtapi_switch).
rtapi_switch_t *rtapi_switch;
global_data_t *global_data; 

static int init_actions(int instance);
static void exit_actions(int instance);
static int harden_rt(void);
static void rtapi_app_msg_handler(msg_level_t level, const char *fmt, va_list ap);
static void stderr_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap);

static int do_one_item(char item_type_char,
		       const string &param_name,
		       const string &param_value,
		       void *vitem,
		       int idx,
		       pb::Container &pbreply)
{
    char *endp;
    switch(item_type_char) {
    case 'l': {
	long *litem = *(long**) vitem;
	litem[idx] = strtol(param_value.c_str(), &endp, 0);
	if(*endp) {
	    note_printf(pbreply, "`%s' invalid for parameter `%s'",
			param_value.c_str(), param_name.c_str());
	    return -1;
	}
	return 0;
    }
    case 'i': {
	int *iitem = *(int**) vitem;
	iitem[idx] = strtol(param_value.c_str(), &endp, 0);
	if(*endp) {
	    note_printf(pbreply,
			"`%s' invalid for parameter `%s'",
			param_value.c_str(), param_name.c_str());
	    return -1;
	}
	return 0;
    }
    case 's': {
	char **sitem = *(char***) vitem;
	sitem[idx] = strdup(param_value.c_str());
	return 0;
    }
    default:
	note_printf(pbreply,
		    "%s: Invalid type character `%c'\n",
		    param_name.c_str(), item_type_char);
	return -1;
    }
    return 0;
}

void remove_quotes(string &s)
{
    s.erase(remove_copy(s.begin(), s.end(), s.begin(), '"'), s.end());
}

static int do_module_args(void *module,
			  pbstringarray_t args,
			  const string &symprefix,
			  pb::Container &pbreply)
{
    for(int i = 0; i < args.size(); i++) {
        string s(args.Get(i));
	remove_quotes(s);
        size_t idx = s.find('=');
        if(idx == string::npos) {
	    note_printf(pbreply, "Invalid parameter `%s'",
			s.c_str());
            return -1;
        }
        string param_name(s, 0, idx);
        string param_value(s, idx+1);
        void *item = DLSYM<void*>(module,
				  symprefix +
				  "address_" +
				  param_name);
        if (!item) {
	    note_printf(pbreply,
			"Unknown parameter `%s'",
			s.c_str());
            return -1;
        }
	dlerror();
        char **item_type = DLSYM<char**>(module,
					 symprefix +
					 "type_" +
					 param_name);
        if (!item_type || !*item_type) {
	    const char *err = dlerror();
	    if (err)
		note_printf(pbreply, "BUG: %s:", err);
	    note_printf(pbreply,
			"Unknown parameter `%s' (type information missing)",
			s.c_str());
            return -1;
        }
        string item_type_string = *item_type;

        if (item_type_string.size() > 1) {
            int a, b;
            char item_type_char;
            int r = sscanf(item_type_string.c_str(), "%d-%d%c",
			   &a, &b, &item_type_char);
            if(r != 3) {
		note_printf(pbreply,
			    "Unknown parameter `%s'"
			    " (corrupt array type information): %s",
			    s.c_str(), item_type_string.c_str());
                return -1;
            }
            size_t idx = 0;
            int i = 0;
            while(idx != string::npos) {
                if(i == b) {
		    note_printf(pbreply,
				"%s: can only take %d arguments",
				s.c_str(), b);
                    return -1;
                }
                size_t idx1 = param_value.find(",", idx);
                string substr(param_value, idx, idx1 - idx);
                int result = do_one_item(item_type_char, s, substr, item, i, pbreply);
                if(result != 0) return result;
                i++;
                idx = idx1 == string::npos ? idx1 : idx1 + 1;
            }
        } else {
            char item_type_char = item_type_string[0];
            int result = do_one_item(item_type_char, s, param_value, item, 0, pbreply);
            if (result != 0) return result;
        }
    }
    return 0;
}

// kthreads:
// only instance args are exported in sysfs, module params are not
// see RTAPI_IP_MODEin src/rtapi/rtapi.h
// therefore, if we see a param on kthreads newinst, we just
// overwrite the previous value via sysfs
static int do_kmodinst_args(const string &comp,
			  pbstringarray_t args,
			  pb::Container &pbreply)
{
    for (int i = 0; i < args.size(); i++) {
        string s(args.Get(i));
	remove_quotes(s);
        size_t idx = s.find('=');
        if(idx == string::npos) {
	    note_printf(pbreply, "Invalid parameter `%s'",
			s.c_str());
            return -1;
        }
        string param_name(s, 0, idx);
        string param_value(s, idx+1);

	// ls /sys/module/brd/parameters/
	// max_part  rd_nr  rd_size

	string path = "/sys/module/" + comp + "/parameters/" + param_name;
	struct stat sb;
	if (stat(path.c_str(), &sb) < 0) {
	    // if param_name is an instance param, it's exported in sysfs
	    note_printf(pbreply, "newinst '%s': no such instance parameter '%s'",
			comp.c_str(),
			param_name.c_str());
	    return -ENOENT;
	}
	int retval = procfs_cmd(path.c_str(), param_value.c_str());
	if (retval < 0) {
	    note_printf(pbreply, "newinst %s: setting param %s to %s failed:  %d - %s",
			comp.c_str(),
			param_name.c_str(),
			param_value.c_str(),
			retval,
			strerror(-retval));
	    return retval;
	}
    }
    return 0;
}

static const char **pbargv(const pbstringarray_t &args)
{
    const char **argv, **s;
    s = argv = (const char **) calloc(sizeof(char *), args.size() + 1);
    for (int i = 0; i < args.size(); i++) {
	*s++ = args.Get(i).c_str();
    }
    *s = NULL;
    return argv;
}

static void usrfunct_error(const int retval,
			   const string &func,
			   pbstringarray_t args,
			   pb::Container &pbreply)
{
    if (retval >= 0) return;
    string s = pbconcat(args);
    note_printf(pbreply, "hal_call_usrfunct(%s,%s) failed: %d - %s",
		func.c_str(), s.c_str(), retval, strerror(-retval));
}

// split arg array into key=value, others
static void separate_kv(pbstringarray_t &kvpairs,
		    pbstringarray_t &leftovers,
		    const pbstringarray_t &args)
{
    for(int i = 0; i < args.size(); i++) {
        string s(args.Get(i));
	remove_quotes(s);
        if (s.find('=') == string::npos)
	    leftovers.Add()->assign(s);
	else
	    kvpairs.Add()->assign(s);
    }
}

static int do_newinst_cmd(int instance,
			  string comp,
			  string instname,
			  pbstringarray_t args,
			  pb::Container &pbreply)
{
    int retval = -1;


    if (kernel_threads(flavor)) {
	string s = pbconcat(args);
	retval = do_kmodinst_args(comp,args,pbreply);
	if (retval) return retval;
	return procfs_cmd(PROCFS_RTAPICMD,"call newinst %s %s %s",
			  comp.c_str(),
			  instname.c_str(),
			  s.c_str());
    } else {
	if (call_usrfunct == NULL) {
	    pbreply.set_retcode(1);
	    pbreply.add_note("this HAL library version does not support user functions - version problem?");
	    return -1;
	}
	void *w = modules[comp];
	if (w == NULL) {
	    // if newinst via halcmd, it should have been automatically loaded already
	    note_printf(pbreply,
			"newinst: component '%s' not loaded",
			comp.c_str());
	    return -1;
	}
	dlerror();
	string s = pbconcat(args);

	pbstringarray_t kvpairs, leftovers;

	separate_kv(kvpairs, leftovers, args);

	// set the instance parameters
	retval = do_module_args(w, kvpairs, RTAPI_IP_SYMPREFIX, pbreply);
	if (retval < 0) {
	    note_printf(pbreply,
			"passing args for '%s' failed: '%s'",
			instname.c_str(), s.c_str());
	    return retval;
	}
	rtapi_print_msg(RTAPI_MSG_DBG,
			"%s: instargs='%s'\n",__FUNCTION__,
			s.c_str());

	// massage the argv for the newinst user function,
	// and call it
	pbstringarray_t a;
	a.Add()->assign(comp);
	a.Add()->assign(instname);
	a.MergeFrom(leftovers);
	const char **argv = pbargv(a); // pass non-kv pairs only
	int ureturn = 0;
	retval = call_usrfunct("newinst", a.size(), argv, &ureturn );
	if (argv) free(argv);
	if (retval == 0) retval = ureturn;
	usrfunct_error(retval, "newinst", args, pbreply);
    }
    return retval;
}

static int do_delinst_cmd(int instance,
			  string instname,
			  pb::Container &pbreply)
{
    int retval = -1;
    string s;


    if (kernel_threads(flavor)) {
	return procfs_cmd(PROCFS_RTAPICMD,"call delinst %s", instname.c_str());
    } else {
	if (call_usrfunct == NULL) {
	    pbreply.set_retcode(1);
	    pbreply.add_note("this HAL library version does not support user functions - version problem?");
	    return -1;
	}
	pbstringarray_t a;
	a.Add()->assign(instname);
	const char **argv = pbargv(a);
	int ureturn = 0;
	retval = call_usrfunct("delinst", a.size(), argv, &ureturn);
	if (argv) free(argv);
	if (retval == 0) retval = ureturn;
	usrfunct_error(retval, "delinst", a, pbreply);
    }
    return retval;
 }

static int do_callfunc_cmd(int instance,
			   string func,
			   pbstringarray_t args,
			   pb::Container &pbreply)
{
    int retval = -1;

    if (kernel_threads(flavor)) {
	string s = pbconcat(args);
	return procfs_cmd(PROCFS_RTAPICMD,"call %s %s", func.c_str(), s.c_str());
    } else {
	if (call_usrfunct == NULL) {
	    pbreply.set_retcode(1);
	    pbreply.add_note("this HAL library version does not support user functions - version problem?");
	    return -1;
	}
	const char **argv = pbargv(args);
	int ureturn = 0;
	retval = call_usrfunct(func.c_str(),
			       args.size(),
			       argv,
			       &ureturn);
	if (argv) free(argv);
	if (retval == 0) retval = ureturn;
	usrfunct_error(retval, func, args, pbreply);
    }
    return retval;
}



static int do_load_cmd(int instance,
		       string name,
		       pbstringarray_t args,
		       pb::Container &pbreply)
{
    void *w = modules[name];
    char module_name[PATH_MAX];
    void *module;
    int retval;

    if (w == NULL) {
	if (kernel_threads(flavor)) {
	    string cmdargs = pbconcat(args, " ", "'");
	    retval = run_module_helper("insert %s %s", name.c_str(), cmdargs.c_str());
	    if (retval) {
		note_printf(pbreply, "couldnt insmod %s - see dmesg\n", name.c_str());
	    } else {
		modules[name] = (void *) -1;  // so 'if (modules[name])' works
		loading_order.push_back(name);
	    }
	    return retval;
	} else {
	    strncpy(module_name, (name + flavor->mod_ext).c_str(),
		    PATH_MAX);
	    module = modules[name] = dlopen(module_name, RTLD_GLOBAL |RTLD_NOW);
	    if (!module) {
		string errmsg(dlerror());

		// rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlopen: %s\n",
		// 		name.c_str(), errmsg.c_str());
		const char *rpath = rtapi_get_rpath();
		note_printf(pbreply, "%s: dlopen: %s",
			    __FUNCTION__, errmsg.c_str());
		note_printf(pbreply, "rpath=%s",	rpath == NULL ? "" : rpath);
		if (rpath)
		    free((void *)rpath);
		return -1;
	    }
	    // retrieve the address of rtapi_switch_struct
	    // so rtapi functions can be called and members
	    // accessed
	    if (rtapi_switch == NULL) {
		rtapi_get_handle_t rtapi_get_handle;
		dlerror();
		rtapi_get_handle = (rtapi_get_handle_t) dlsym(module,
							      "rtapi_get_handle");
		if (rtapi_get_handle != NULL) {
		    rtapi_switch = rtapi_get_handle();
		    assert(rtapi_switch != NULL);
		}
	    }
	    /// XXX handle arguments
	    int (*start)(void) = DLSYM<int(*)(void)>(module, "rtapi_app_main");
	    if(!start) {
		note_printf(pbreply, "%s: dlsym: %s\n",
			    name.c_str(), dlerror());
		return -1;
	    }
	    int result;

	    result = do_module_args(module, args, RTAPI_MP_SYMPREFIX, pbreply);
	    if(result < 0) { dlclose(module); return -1; }

	    // need to call rtapi_app_main with as root
	    // RT thread creation and hardening requires this
	    if ((result = start()) < 0) {
		note_printf(pbreply, "rtapi_app_main(%s): %d %s\n",
			    name.c_str(), result, strerror(-result));
		modules.erase(modules.find(name));
		return result;
	    }
	    loading_order.push_back(name);

	    rtapi_print_msg(RTAPI_MSG_DBG, "%s: loaded from %s\n",
			    name.c_str(), module_name);
	    return 0;
	}
    } else {
	note_printf(pbreply, "%s: already loaded\n", name.c_str());
	return -1;
    }
}

 static int do_unload_cmd(int instance, string name, pb::Container &reply)
{
    void *w = modules[name];
    int retval = 0;

    if (w == NULL) {
	note_printf(reply, "unload: '%s' not loaded\n",
		    name.c_str());
	return -1;
    } else {
	if (kernel_threads(flavor)) {
	    retval = run_module_helper("remove %s", name.c_str());
	    if (retval) {
		note_printf(reply,  "couldnt rmmod %s - see dmesg\n",
				name.c_str());
		return retval;
	    } else {
		modules.erase(modules.find(name));
		remove_module(name);
	    }
	} else {
	    int (*stop)(void) = DLSYM<int(*)(void)>(w, "rtapi_app_exit");
	    if (stop)
		stop();
	    modules.erase(modules.find(name));
	    remove_module(name);
	    dlclose(w);
	}
    }
    rtapi_print_msg(RTAPI_MSG_DBG, " '%s' unloaded\n", name.c_str());
    return retval;
}

// shut down the stack in reverse loading order
static void exit_actions(int instance)
{
    pb::Container reply;
    size_t index = loading_order.size() - 1;
    for(std::vector<std::string>::reverse_iterator rit = loading_order.rbegin();
	rit != loading_order.rend(); ++rit, --index) {
	do_unload_cmd(instance, *rit, reply);
    }
}

static int init_actions(int instance)
{
    int retval;
    char moddir[PATH_MAX];

    get_rtapi_config(moddir,"MODULES",PATH_MAX);

    if (kernel_threads(flavor)) {
	// kthreads cant possibly run without shmdrv, so bail
	// also, cannot load it here because rtapi_msgd already needs this
	// so it'd be too late here
	if (!is_module_loaded("shmdrv")) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "shmdrv not loaded");
	    return -1;
	}
	// leftovers or running session?
	if (is_module_loaded("rtapi")) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi already loaded");
	    return -1;
	}
	if (is_module_loaded("hal_lib")) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "hal_lib already loaded");
	    return -1;
	}
	char *m = strtok(moddir, "\t ");
	while (m != NULL) {
	    char cmdline[PATH_MAX];
	    if (!strcmp(m, "rtapi")) {
		snprintf(cmdline, sizeof(cmdline),
			 "insert %s rtapi_instance=%d", m, instance_id);
	    } else {
		snprintf(cmdline, sizeof(cmdline), "insert %s", m);
	    }

	    rtapi_print_msg(RTAPI_MSG_DBG, "running '%s'", cmdline);
	    retval = run_module_helper(cmdline);
	    if (retval) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"linuxcnc_module_helper '%s' failed - see dmesg\n",
				cmdline);
		return retval;
	    } else
		rtapi_print_msg(RTAPI_MSG_DBG, "'%s' loaded\n", m);
	    m = strtok(NULL,  "\t ");
	}
    }
    pb::Container reply;
    retval =  do_load_cmd(instance, "rtapi", pbstringarray_t(), reply);
    if (retval)
	return retval;
    if ((retval = do_load_cmd(instance, "hal_lib", pbstringarray_t(), reply)))
	return retval;

    if (!kernel_threads(flavor)) {
	// resolve the "hal_call_usrfunct" for later - callfunc, newinst & delinst need it
	void *hallib = modules["hal_lib"];
	dlerror();
	call_usrfunct = (hal_call_usrfunct_t) dlsym(hallib, "hal_call_usrfunct");

	if (call_usrfunct == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "cant resolve 'hal_call_usrfunct' in hal_lib - version problem?");
	    char *s = dlerror();
	    if (s)
		rtapi_print_msg(RTAPI_MSG_ERR, "dlsym(hal_call_usrfunct): '%s'", s);
	    return -1;
	}
    }
    return 0;
}


static int attach_global_segment()
{
    int retval = 0;
    int globalkey = OS_KEY(GLOBAL_KEY, instance_id);
    int size = 0;
    int tries = 10; // 5 sec deadline for msgd/globaldata to come up

    shm_common_init();
    do {
	retval = shm_common_new(globalkey, &size,
				instance_id, (void **) &global_data, 0);
	if (retval < 0) {
	    tries--;
	    if (tries == 0) {
		syslog_async(LOG_ERR,
		       "rtapi_app:%d: ERROR: cannot attach global segment key=0x%x %s\n",
		       instance_id, globalkey, strerror(-retval));
		return retval;
	    }
	    struct timespec ts = {0, 500 * 1000 * 1000}; //ms
	    nanosleep(&ts, NULL);
	}
    } while (retval < 0);

    if (size != sizeof(global_data_t)) {
	syslog_async(LOG_ERR,
	       "rtapi_app:%d global segment size mismatch: expect %zu got %d\n",
	       instance_id, sizeof(global_data_t), size);
	return -EINVAL;
    }

    tries = 10;
    while  (global_data->magic !=  GLOBAL_READY) {
	tries--;
	if (tries == 0) {
	    syslog_async(LOG_ERR,
		   "rtapi_app:%d: ERROR: global segment magic not changing to ready: magic=0x%x\n",
		   instance_id, global_data->magic);
	    return -EINVAL;
	}
	struct timespec ts = {0, 500 * 1000 * 1000}; //ms
	nanosleep(&ts, NULL);
    }
    return retval;
}


// handle commands from zmq socket
static int rtapi_request(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    zmsg_t *r = zmsg_recv(poller->socket);
    char *origin = zmsg_popstr (r);
    zframe_t *request_frame  = zmsg_pop (r);
    static bool force_exit = false;

    pb::Container pbreq, pbreply;

    if (!pbreq.ParseFromArray(zframe_data(request_frame),
			      zframe_size(request_frame))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "cant decode request from %s (size %zu)",
			origin ? origin : "NULL",
			zframe_size(request_frame));
	zmsg_destroy(&r);
	return 0;
    }
    if (debug) {
	string buffer;
	if (TextFormat::PrintToString(pbreq, &buffer)) {
	    fprintf(stderr, "request: %s\n",buffer.c_str());
	}
    }

    pbreply.set_type(pb::MT_RTAPI_APP_REPLY);

    switch (pbreq.type()) {
    case pb::MT_RTAPI_APP_PING:
	char buffer[LINELEN];
	snprintf(buffer, sizeof(buffer),
		 "pid=%d flavor=%s gcc=%s git=%s",
		 getpid(),flavor->name,  __VERSION__, GIT_VERSION);
	pbreply.add_note(buffer);
	pbreply.set_retcode(0);
	break;

    case pb::MT_RTAPI_APP_EXIT:
	assert(pbreq.has_rtapicmd());
	exit_actions(pbreq.rtapicmd().instance());
	force_exit = true;
	pbreply.set_retcode(0);
	break;

    case pb::MT_RTAPI_APP_CALLFUNC:

	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_func());
	assert(pbreq.rtapicmd().has_instance());
	pbreply.set_retcode(do_callfunc_cmd(pbreq.rtapicmd().instance(),
					      pbreq.rtapicmd().func(),
					      pbreq.rtapicmd().argv(),
					      pbreply));
	break;

    case pb::MT_RTAPI_APP_NEWINST:
	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_comp());
	assert(pbreq.rtapicmd().has_instname());
	assert(pbreq.rtapicmd().has_instance());
	pbreply.set_retcode(do_newinst_cmd(pbreq.rtapicmd().instance(),
					   pbreq.rtapicmd().comp(),
					   pbreq.rtapicmd().instname(),
					   pbreq.rtapicmd().argv(),
					   pbreply));
	break;

    case pb::MT_RTAPI_APP_DELINST:

	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_instname());
	assert(pbreq.rtapicmd().has_instance());
	pbreply.set_retcode(do_delinst_cmd(pbreq.rtapicmd().instance(),
					   pbreq.rtapicmd().instname(),
					   pbreply));
	break;


    case pb::MT_RTAPI_APP_LOADRT:
	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_modname());
	assert(pbreq.rtapicmd().has_instance());
	pbreply.set_retcode(do_load_cmd(pbreq.rtapicmd().instance(),
					pbreq.rtapicmd().modname(),
					pbreq.rtapicmd().argv(),
					pbreply));
	break;

    case pb::MT_RTAPI_APP_UNLOADRT:
	assert(pbreq.rtapicmd().has_modname());
	assert(pbreq.rtapicmd().has_instance());

	pbreply.set_retcode(do_unload_cmd(pbreq.rtapicmd().instance(),
					  pbreq.rtapicmd().modname(),
					  pbreply));
	break;

    case pb::MT_RTAPI_APP_LOG:
	assert(pbreq.has_rtapicmd());
	if (pbreq.rtapicmd().has_rt_msglevel()) {
	    global_data->rt_msg_level = pbreq.rtapicmd().rt_msglevel();
	}
	if (pbreq.rtapicmd().has_user_msglevel()) {
	    global_data->user_msg_level = pbreq.rtapicmd().user_msglevel();
	}
	pbreply.set_retcode(0);
	break;

    case pb::MT_RTAPI_APP_NEWTHREAD:
	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_threadname());
	assert(pbreq.rtapicmd().has_threadperiod());
	assert(pbreq.rtapicmd().has_cpu());
	assert(pbreq.rtapicmd().has_use_fp());
	assert(pbreq.rtapicmd().has_instance());
	assert(pbreq.rtapicmd().has_flags());

	if (kernel_threads(flavor)) {
	    int retval =  procfs_cmd(PROCFS_RTAPICMD,"newthread %s %d %d %d %d",
				     pbreq.rtapicmd().threadname().c_str(),
				     pbreq.rtapicmd().threadperiod(),
				     pbreq.rtapicmd().use_fp(),
				     pbreq.rtapicmd().cpu(),
				     pbreq.rtapicmd().flags());
	    pbreply.set_retcode(retval < 0 ? retval:0);

	} else {
	    void *w = modules["hal_lib"];
	    if (w == NULL) {
		pbreply.add_note("hal_lib not loaded");
		pbreply.set_retcode(-1);
		break;
	    }
	    int (*create_thread)(const hal_threadargs_t*) =
		DLSYM<int(*)(const hal_threadargs_t*)>(w, "hal_create_xthread");
	    if (create_thread == NULL) {
		pbreply.add_note("symbol 'hal_create_thread' not found in hal_lib");
		pbreply.set_retcode(-1);
		break;
	    }
	    hal_threadargs_t args;
	    args.name = pbreq.rtapicmd().threadname().c_str();
	    args.period_nsec = pbreq.rtapicmd().threadperiod();
	    args.uses_fp = pbreq.rtapicmd().use_fp();
	    args.cpu_id = pbreq.rtapicmd().cpu();
	    args.flags = (rtapi_thread_flags_t) pbreq.rtapicmd().flags();

	    int retval = create_thread(&args);
	    if (retval < 0) {
		pbreply.add_note("hal_create_xthread() failed, see log");
	    }
	    pbreply.set_retcode(retval);
	}
	break;

    case pb::MT_RTAPI_APP_DELTHREAD:
	assert(pbreq.has_rtapicmd());
	assert(pbreq.rtapicmd().has_threadname());
	assert(pbreq.rtapicmd().has_instance());

	if (kernel_threads(flavor)) {
	    int retval =  procfs_cmd(PROCFS_RTAPICMD, "delthread %s",
					   pbreq.rtapicmd().threadname().c_str());
	    pbreply.set_retcode(retval < 0 ? retval:0);
	} else {
	    void *w = modules["hal_lib"];
	    if (w == NULL) {
		pbreply.add_note("hal_lib not loaded");
		pbreply.set_retcode(-1);
		break;
	    }
	    int (*delete_thread)(const char *) =
		DLSYM<int(*)(const char *)>(w,"hal_thread_delete");
	    if (delete_thread == NULL) {
		pbreply.add_note("symbol 'hal_thread_delete' not found in hal_lib");
		pbreply.set_retcode(-1);
		break;
	    }
	    int retval = delete_thread(pbreq.rtapicmd().threadname().c_str());
	    pbreply.set_retcode(retval);
	}
	break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR,
			"unkown command type %d)",
			(int) pbreq.type());
	zmsg_destroy(&r);
	return 0;


    }
    // log accumulated notes
    for (int i = 0; i < pbreply.note_size(); i++) {
	rtapi_print_msg(pbreply.retcode() ? RTAPI_MSG_ERR : RTAPI_MSG_DBG,
			pbreply.note(i).c_str());
    }

    // TODO: extract + attach error message

    size_t reply_size = pbreply.ByteSize();
    zframe_t *reply = zframe_new (NULL, reply_size);
    if (!pbreply.SerializeWithCachedSizesToArray(zframe_data (reply))) {
	zframe_destroy(&reply);
	rtapi_print_msg(RTAPI_MSG_ERR,
			"cant serialize to %s (type %d size %zu)",
			origin ? origin : "NULL",
			pbreply.type(),
			reply_size);
    } else {
	if (debug) {
	    string buffer;
	    if (TextFormat::PrintToString(pbreply, &buffer)) {
		fprintf(stderr, "reply: %s\n",buffer.c_str());
	    }
	}
	assert(zstr_sendm (poller->socket, origin) == 0);
	if (zframe_send (&reply, poller->socket, 0)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "cant serialize to %s (type %d size %zu)",
			    origin ? origin : "NULL",
			    pbreply.type(),
			    zframe_size(reply));
	}
    }
    free(origin);
    zmsg_destroy(&r);
    if (force_exit) // terminate the zloop
	return -1;
    return 0;
}

static void btprint(const char *prefix, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    rtapi_msg_handler_t  print = rtapi_get_msg_handler();
    print(RTAPI_MSG_ERR, fmt, args);
    va_end(args);
}

// handle signals delivered via sigaction - not all signals
// can be dealt with through signalfd(2)
// log, try to do something sane, and dump core
static void sigaction_handler(int sig, siginfo_t *si, void *uctx)
{

    switch (sig) {
    case SIGXCPU:
        // should not happen - must be handled in RTAPI if enabled
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: BUG: SIGXCPU should be handled in RTAPI");
	// NB: fall through

    default:
	rtapi_print_msg(RTAPI_MSG_ERR,
			"signal %d - '%s' received, dumping core (current dir=%s)",
			sig, strsignal(sig), get_current_dir_name());
	backtrace("", "rtapi_app", btprint, 3);
	if (global_data)
	    global_data->rtapi_app_pid = 0;

	closelog_async(); // let syslog_async drain
        sleep(1);
	// reset handler for current signal to default
        signal(sig, SIG_DFL);
	// and re-raise so we get a proper core dump and stacktrace
	kill(getpid(), sig);
	sleep(1);
        break;
    }
    // not reached
}


// handle signals delivered synchronously in the event loop
// by polling signal_fd
// log, start shutdown and flag end of the event loop
static int s_handle_signal(zloop_t *loop, zmq_pollitem_t *poller, void *arg)
{
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"BUG: read(signal_fd): %s", strerror(errno));
	return 0;
    }
    switch (fdsi.ssi_signo) {
	// see machinetalk/lib/setup_signals for handled signals
    case SIGINT:
    case SIGQUIT:
    case SIGKILL:
    case SIGTERM:
	rtapi_print_msg(RTAPI_MSG_INFO,
			"signal %d - '%s' received, exiting",
			fdsi.ssi_signo, strsignal(fdsi.ssi_signo));
	exit_actions(instance_id);
	interrupted = true; // make mainloop exit
	if (global_data)
	    global_data->rtapi_app_pid = 0;
	return -1;

    default:
	// this should be handled either above or in sigaction_handler
	rtapi_print_msg(RTAPI_MSG_ERR, "BUG: unhandled signal %d - '%s' received\n",
			fdsi.ssi_signo, strsignal(fdsi.ssi_signo));
    }
    return 0;
}

static int
s_handle_timer(zloop_t *loop, int  timer_id, void *args)
{
    if (global_data->rtapi_msgd_pid == 0) {
	// cant log this via rtapi_print, since msgd is gone
	syslog_async(LOG_ERR,"rtapi_msgd went away, exiting\n");
	exit_actions(instance_id);
	if (global_data)
	    global_data->rtapi_app_pid = 0;
	exit(EXIT_FAILURE);
    }
    return 0;
}


static int mainloop(size_t  argc, char **argv)
{
    int retval;
    unsigned i;
    static char proctitle[20];

    // set new process name
    snprintf(proctitle, sizeof(proctitle), "rtapi:%d",instance_id);
    size_t argv0_len = strlen(argv[0]);
    size_t procname_len = strlen(proctitle);
    size_t max_procname_len = (argv0_len > procname_len) ?
	(procname_len) : (argv0_len);

    strncpy(argv[0], proctitle, max_procname_len);
    memset(&argv[0][max_procname_len], '\0', argv0_len - max_procname_len);

    for (i = 1; i < argc; i++)
	memset(argv[i], '\0', strlen(argv[i]));

    backtrace_init(proctitle);

    // set this thread's name so it can be identified in ps/top as
    // rtapi:<instance>
    if (prctl(PR_SET_NAME, argv[0]) < 0) {
	syslog_async(LOG_ERR,	"rtapi_app: prctl(PR_SETNAME,%s) failed: %s\n",
	       argv[0], strerror(errno));
    }

    // attach global segment which rtapi_msgd already prepared
    if ((retval = attach_global_segment()) != 0) {
	syslog_async(LOG_ERR, "%s: FATAL - failed to attach to global segment\n",
	       argv[0]);
	exit(retval);
    }

    // make sure rtapi_msgd's pid is valid and msgd is running, 
    // in case we caught a leftover shmseg
    // otherwise log messages would vanish
    if ((global_data->rtapi_msgd_pid == 0) ||
	kill(global_data->rtapi_msgd_pid, 0) != 0) {
	syslog_async(LOG_ERR,"%s: rtapi_msgd pid invalid: %d, exiting\n",
	       argv[0], global_data->rtapi_msgd_pid);
	exit(EXIT_FAILURE);
    }

    // from here on it is safe to use rtapi_print() and friends as 
    // the error ring is now set up and msgd is logging it
    rtapi_set_logtag("rtapi_app");
    rtapi_set_msg_level(global_data->rt_msg_level);

    // obtain handle on flavor descriptor as detected by rtapi_msgd
    flavor = flavor_byid(global_data->rtapi_thread_flavor);
    if (flavor == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"FATAL - invalid flavor id: %d\n",
			global_data->rtapi_thread_flavor);
	global_data->rtapi_app_pid = 0;
	exit(EXIT_FAILURE);
    }

    // make sure we're setuid root when we need to
    if (use_drivers || (flavor->flags & FLAVOR_DOES_IO)) {
	if (geteuid() != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "rtapi_app:%d need to"
			    " 'sudo make setuid' to access I/O?\n",
			    instance_id);
	    global_data->rtapi_app_pid = 0;
	    exit(EXIT_FAILURE);
	}
    }

    // assorted RT incantations - memory locking, prefaulting etc
    if ((retval = harden_rt())) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_app:%d failed to setup "
			"realtime environment - 'sudo make setuid' missing?\n", 
			instance_id);
	global_data->rtapi_app_pid = 0;
	exit(retval);
    }

    // load rtapi and hal_lib
    if (init_actions(instance_id)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"init_actions() failed\n");
	global_data->rtapi_app_pid = 0;
	exit(1);
    }

    // block all signal delivery through signal handler
    // since we're using signalfd()
    // do this here so child threads inherit the sigmask
    if (trap_signals) {
	signal_fd = setup_signals(sigaction_handler, SIGINT, SIGQUIT, SIGKILL, SIGTERM, -1);
	assert(signal_fd > -1);
    }

    // suppress default handling of signals in zctx_new()
    // since we're using signalfd()
    zsys_handler_set(NULL);

    zctx_t *z_context = zctx_new ();
    void *z_command = zsocket_new (z_context, ZMQ_ROUTER);
    {
	char z_ident[30];
	snprintf(z_ident, sizeof(z_ident), "rtapi_app%d", getpid());
	zsocket_set_identity(z_command, z_ident);
	zsocket_set_linger(z_command, 1000); // wait for last reply to drain
    }

#ifdef NOTYET
    // determine interface to bind to if remote option set
    if ((remote || z_uri)  && interfaces) {
	char ifname[LINELEN], ip[LINELEN];
	// rtapi_print_msg(RTAPI_MSG_INFO, "rtapi_app: ifpref='%s'\n",interfaces);
	if (parse_interface_prefs(interfaces,  ifname, ip, NULL) == 0) {
	    rtapi_print_msg(RTAPI_MSG_INFO, "rtapi_app: using preferred interface %s/%s\n",
			    ifname, ip);
	    ipaddr = strdup(ip);
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: INTERFACES='%s'"
			    " - cant determine preferred interface, using %s/%s\n",
			    interfaces, ifname, ipaddr);
	}
	if (z_uri == NULL) { // not given on command line - finalize the URI
	    char uri[LINELEN];
	    snprintf(uri, sizeof(uri), "tcp://%s:*" , ipaddr);
	    z_uri = strdup(uri);
	}

	if ((z_port = zsocket_bind(z_command, z_uri)) == -1) {
	    rtapi_print_msg(RTAPI_MSG_ERR,  "cannot bind '%s' - %s\n",
			    z_uri, strerror(errno));
	    global_data->rtapi_app_pid = 0;
	    exit(EXIT_FAILURE);
	} else {
	    z_uri_dsn = zsocket_last_endpoint(z_command);
	    rtapi_print_msg(RTAPI_MSG_DBG,  "rtapi_app: command RPC socket on '%s'\n",
			    z_uri_dsn);
	}
    }
#endif
    {	// always bind the IPC socket
	char uri[LINELEN];
	snprintf(uri, sizeof(uri), ZMQIPC_FORMAT,
		 RUNDIR, instance_id, "rtapi", service_uuid);
	mode_t prev = umask(S_IROTH | S_IWOTH | S_IXOTH);
	if ((z_port = zsocket_bind(z_command, uri )) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,  "cannot bind IPC socket '%s' - %s\n",
			    uri, strerror(errno));
	    global_data->rtapi_app_pid = 0;
	    exit(EXIT_FAILURE);
	}
	rtapi_print_msg(RTAPI_MSG_ERR,"accepting commands at %s\n",uri);
	umask(prev);
    }
    zloop_t *z_loop = zloop_new();
    assert(z_loop);
    zloop_set_verbose(z_loop, debug);

    zmq_pollitem_t signal_poller = { 0, signal_fd, ZMQ_POLLIN };
    if (trap_signals)
	zloop_poller (z_loop, &signal_poller, s_handle_signal, NULL);

    zmq_pollitem_t command_poller = { z_command, 0, ZMQ_POLLIN };
    zloop_poller(z_loop, &command_poller, rtapi_request, NULL);

    zloop_timer (z_loop, BACKGROUND_TIMER, 0, s_handle_timer, NULL);

#ifdef NOTYET
    // no remote rtapi service yet
    if (remote) {
	if (!(av_loop = avahi_czmq_poll_new(z_loop))) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app:%d: zeroconf: Failed to create avahi event loop object.",
			    instance_id);
	    return -1;
	} else {
	    char name[255];
	    snprintf(name, sizeof(name), "RTAPI service on %s pid %d", ipaddr, getpid());
	    rtapi_publisher = zeroconf_service_announce(name,
							MACHINEKIT_DNSSD_SERVICE_TYPE,
							RTAPI_DNSSD_SUBTYPE,
							z_port,
							(char *)z_uri_dsn,
							service_uuid,
							process_uuid_str,
							"rtapi", NULL,
							av_loop);
	    if (rtapi_publisher == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app:%d: failed to start zeroconf publisher",
				instance_id);
		return -1;
	    }
	}
    }
#endif

    // report success
    rtapi_print_msg(RTAPI_MSG_INFO, "rtapi_app:%d ready flavor=%s gcc=%s git=%s",
		    instance_id, flavor->name,  __VERSION__, GIT_VERSION);

    // the RT stack is now set up and good for use
    global_data->rtapi_app_pid = getpid();

    // main loop
    do {
	retval = zloop_start(z_loop);
    } while  (!(retval || interrupted));

    rtapi_print_msg(RTAPI_MSG_INFO,
		    "exiting mainloop (%s)\n",
		    interrupted ? "interrupted": "by remote command");

    // stop the service announcement
    zeroconf_service_withdraw(rtapi_publisher);

    // shutdown zmq context
    zctx_destroy(&z_context);

    // exiting, so deregister our pid, which will make rtapi_msgd exit too
    global_data->rtapi_app_pid = 0;
    return 0;
}

static int configure_memory(void)
{
    unsigned int i, pagesize;
    char *buf;

    if (global_data->rtapi_thread_flavor != RTAPI_POSIX_ID) {
	// Realtime tweak requires privs
	/* Lock all memory. This includes all current allocations (BSS/data)
	 * and future allocations. */
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, 
			    "mlockall() failed: %d '%s'\n",
			    errno,strerror(errno));
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "For more information, see "
			    "http://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?LockedMemory\n");
	    return 1;
	}
    }

    /* Turn off malloc trimming.*/
    if (!mallopt(M_TRIM_THRESHOLD, -1)) {
	rtapi_print_msg(RTAPI_MSG_WARN,
			"mallopt(M_TRIM_THRESHOLD, -1) failed\n");
	return 1;
    }
    /* Turn off mmap usage. */
    if (!mallopt(M_MMAP_MAX, 0)) {
	rtapi_print_msg(RTAPI_MSG_WARN,
			"mallopt(M_MMAP_MAX, -1) failed\n");
	return 1;
    }
    buf = static_cast<char *>(malloc(PRE_ALLOC_SIZE));
    if (buf == NULL) {
	rtapi_print_msg(RTAPI_MSG_WARN, "malloc(PRE_ALLOC_SIZE) failed\n");
	return 1;
    }
    pagesize = sysconf(_SC_PAGESIZE);
    /* Touch each page in this piece of memory to get it mapped into RAM */
    for (i = 0; i < PRE_ALLOC_SIZE; i += pagesize) {
	/* Each write to this buffer will generate a pagefault.
	 * Once the pagefault is handled a page will be locked in
	 * memory and never given back to the system. */
	buf[i] = 0;
    }
    /* buffer will now be released. As Glibc is configured such that it
     * never gives back memory to the kernel, the memory allocated above is
     * locked for this process. All malloc() and new() calls come from
     * the memory pool reserved and locked above. Issuing free() and
     * delete() does NOT make this locking undone. So, with this locking
     * mechanism we can build C++ applications that will never run into
     * a major/minor pagefault, even with swapping enabled. */
    free(buf);

    return 0;
}

static void
exit_handler(void)
{
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    if ((rusage.ru_majflt - majflt) > 0) {
	// RTAPI already shut down here
	rtapi_print_msg(RTAPI_MSG_WARN,
			"rtapi_app_main %d: %ld page faults, %ld page reclaims\n",
			getpid(), rusage.ru_majflt - majflt,
			rusage.ru_minflt - minflt);
    }
}

static int harden_rt()
{
    // enable core dumps
    struct rlimit core_limit;
    core_limit.rlim_cur = RLIM_INFINITY;
    core_limit.rlim_max = RLIM_INFINITY;

    if (setrlimit(RLIMIT_CORE, &core_limit) < 0)
	rtapi_print_msg(RTAPI_MSG_WARN, 
			"setrlimit: %s - core dumps may be truncated or non-existant\n",
			strerror(errno));

    // even when setuid root
    // note this may not be enough
    // echo 1 >
    // might be needed to have setuid programs dump core
    if (prctl(PR_SET_DUMPABLE, 1) < 0)
	rtapi_print_msg(RTAPI_MSG_WARN, 
			"prctl(PR_SET_DUMPABLE) failed: no core dumps will be created - %d - %s\n",
			errno, strerror(errno));
    FILE *fd;
    if ((fd = fopen("/proc/sys/fs/suid_dumpable","r")) != NULL) {
	int flag;
	if ((fscanf(fd, "%d", &flag) == 1) && (flag == 0)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "rtapi:%d: cannot create core dumps - /proc/sys/fs/suid_dumpable contains 0",
			    instance_id);
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "you might have to run 'echo 1 > /proc/sys/fs/suid_dumpable'"
			    " as root to enable rtapi_app core dumps");
	}
	fclose(fd);
    }

    configure_memory();

    if (getrusage(RUSAGE_SELF, &rusage)) {
	rtapi_print_msg(RTAPI_MSG_WARN, 
			"getrusage(RUSAGE_SELF) failed: %d '%s'\n",
			errno,strerror(errno));
    } else {
	minflt = rusage.ru_minflt;
	majflt = rusage.ru_majflt;
	if (atexit(exit_handler)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, 
			    "atexit() failed: %d '%s'\n",
			    errno,strerror(errno));
	}
    }

    if (flavor->id == RTAPI_XENOMAI_ID) {
	int retval = user_in_xenomai_group();

	switch (retval) {
	case 1:
	    // {
	    // 	gid_t xg = xenomai_gid();
	    // 	do_setuid();
	    // 	if (setegid(xg))
	    // 	    rtapi_print_msg(RTAPI_MSG_ERR,
	    // 			    "setegid(%d): %s", xg, strerror(errno));
	    // 	undo_setuid();
	    // 	rtapi_print_msg(RTAPI_MSG_ERR,
	    // 			"xg=%d egid now %d", xg, getegid());
	    // }
	    break;
	case 0:
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "this user is not member of group xenomai");
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "please 'sudo adduser <username>  xenomai',"
			    " logout and login again");
	    return -1;

	default:
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "cannot determine if this user is a member of group xenomai: %s",
			    strerror(-retval));
	    return -1;
	}
    }

#if defined(__x86_64__) || defined(__i386__)

    // this is a bit of a shotgun approach and should be made more selective
    // however, due to serial invocations of rtapi_app during setup it is not
    // guaranteed the process executing e.g. hal_parport's rtapi_app_main is
    // the same process which starts the RT threads, causing hal_parport
    // thread functions to fail on inb/outb
    if (use_drivers || (flavor->flags & FLAVOR_DOES_IO)) {
	if (iopl(3) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "cannot gain I/O privileges - "
			    "forgot 'sudo make setuid'?\n");
	    return -EPERM;
	}
    }
#endif
    return 0;
}


static void usage(int argc, char **argv) 
{
    printf("Usage:  %s [options]\n", argv[0]);
}

static struct option long_options[] = {
    {"help",  no_argument,          0, 'h'},
    {"foreground",  no_argument,    0, 'F'},
    {"nosighdlr",   no_argument,    0, 'G'},
    {"instance", required_argument, 0, 'I'},
    {"ini",      required_argument, 0, 'i'},     // default: getenv(INI_FILE_NAME)
    {"drivers",   required_argument, 0, 'D'},
    {"uri",    required_argument,    0, 'U'},
    {"debug",        no_argument,    0, 'd'},
    {"svcuuid",   required_argument, 0, 'R'},
    {"interfaces",required_argument, 0, 'n'},
    {"stderr",    no_argument,       0, 's'},
    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    int c;
    progname = argv[0];
    inifile =  getenv("MACHINEKIT_INI");
    int syslog_async_option = LOG_NDELAY;
    int syslog_async_delay = 1000;

    uuid_generate_time(process_uuid);
    uuid_unparse(process_uuid, process_uuid_str);

    rtapi_set_msg_handler(rtapi_app_msg_handler);

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "ShH:m:I:sf:r:U:NFdR:n:i:",
			 long_options, &option_index);
	if (c == -1)
	    break;

	switch (c)	{
	case 'G':
	    trap_signals = false; // ease debugging with gdb
	    break;

	case 'd':
	    debug++;
	    break;

	case 'D':
	    use_drivers = 1;
	    break;

	case 'F':
	    foreground = 1;
	    rtapi_set_msg_handler(stderr_rtapi_msg_handler);
	    break;

	case 'i':
	    inifile = strdup(optarg);
	    break;

	case 'I':
	    instance_id = atoi(optarg);
	    break;

	case 'f':
	    if ((flavor = flavor_byname(optarg)) == NULL) {
		fprintf(stderr, "no such flavor: '%s' -- valid flavors are:\n", 
			optarg);
		flavor_ptr f = flavors;
		while (f->name) {
		    fprintf(stderr, "\t%s\n", f->name);
		    f++;
		}
		exit(1);
	    }
	    break;

	case 'U':
	    z_uri = optarg;
	    break;

	case 'n':
	    interfaces = strdup(optarg);
	    break;

	case 'R':
	    service_uuid = strdup(optarg);
	    break;
	case 's':
	    syslog_async_option |= LOG_PERROR;
	    syslog_async_delay = 0;
	    break;
	case '?':
	    if (optopt)  fprintf(stderr, "bad short opt '%c'\n", optopt);
	    else  fprintf(stderr, "bad long opt \"%s\"\n", argv[curind]);
	    //usage(argc, argv);
	    exit(1);
	    break;

	default:
	    usage(argc, argv);
	    exit(0);
	}
    }

    openlog_async(argv[0], syslog_async_option, LOG_LOCAL1);
    setlogmask_async(LOG_UPTO(LOG_DEBUG));
    // tune async syslog buffers:  max buffer size; 0 delay for stdout, else 1s
    tunelog_async(99,syslog_async_delay);

    if (trap_signals && (getenv("NOSIGHDLR") != NULL))
	trap_signals = false;

    if (inifile && ((inifp = fopen(inifile,"r")) == NULL)) {
	fprintf(stderr,"rtapi_app: cant open inifile '%s'\n", inifile);
    }

    // must have a MKUUID one way or the other
    if (service_uuid == NULL) {
	const char *s;
	if ((s = iniFind(inifp, "MKUUID", "MACHINEKIT"))) {
	    service_uuid = strdup(s);
	}
    }

    if (service_uuid == NULL) {
	fprintf(stderr, "rtapi: no service UUID (-R <uuid> or environment MKUUID) present\n");
	exit(-1);
    }

#ifdef NOTYET
    iniFindInt(inifp, "REMOTE", "MACHINEKIT", &remote);
    if (remote && (interfaces == NULL)) {
	const char *s;
	if ((s = iniFind(inifp, "INTERFACES", "MACHINEKIT"))) {
	    interfaces = strdup(s);
	}
    }
#endif

    // the actual checking for setuid happens in harden_rt() (if needed)
    if (getuid() != 0) {
	pid_t pid1;
	pid_t pid2;
	int status;
	if ((pid1 = fork())) {
	    waitpid(pid1, &status, 0);
	    exit(status);
	} else if (!pid1) {
	    if ((pid2 = fork())) {
		exit(0);
	    } else if (!pid2) {
		setsid(); // Detach from the parent session
	    } else {
		exit(1);
	    }
	} else {
	    exit(1);
	}
    } else {
	// dont run as root XXX
    }
    exit(mainloop(argc, argv));
}


// normally rtapi_app will log through the message ringbuffer in the
// global data segment. This isnt available initially, and during shutdown,
// so switch to direct syslog during these time windows so we dont
// loose log messages, even if they cant go through the ringbuffer
static void rtapi_app_msg_handler(msg_level_t level, const char *fmt,
				  va_list ap)
{
    // during startup the global segment might not be
    // available yet, so use stderr until then
    if (global_data) {
	vs_ring_write(level, fmt, ap);
    } else {
	vsyslog_async(rtapi2syslog(level), fmt, ap);
    }
}

// use this handler if -F/--foreground was given
static void stderr_rtapi_msg_handler(msg_level_t level,
				     const char *fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
}

static void remove_module(std::string name)
{
    std::vector<string>::iterator invalid;
    invalid = remove( loading_order.begin(), loading_order.end(), name );
}
