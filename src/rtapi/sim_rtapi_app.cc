/* Copyright (C) 2006-2008 Jeff Epler <jepler@unpythonic.net>
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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <sys/time.h>
#include <sys/resource.h>
#include <linux/capability.h>
#include <sys/io.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <malloc.h>
#include <assert.h>
#include <syslog.h>
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <limits.h>

#include <sys/mman.h>
#include <execinfo.h>
#include <sys/prctl.h>
#if defined(RTAPI_XENOMAI_USER)  // FIXME BUILD_XENOMAI_USER
#include <grp.h>
#include <rtdk.h>
#endif

#include "rtapi.h"
#include "rtapi_global.h"
#include "rtapi_support.h"
#include "hal.h"
#include "hal/hal_priv.h"

#define BACKTRACE_SIZE 1000

using namespace std;

// X will be replaced by '\0' post-snprintf
#define SOCKET_PATH "X/tmp/rtapi_fifo:%d"

/* Pre-allocation size. Must be enough for the whole application life to avoid
 * pagefaults by new memory requested from the system. */
#define PRE_ALLOC_SIZE		(30 * 1024 * 1024)

template<class T> T DLSYM(void *handle, const string &name) {
	return (T)(dlsym(handle, name.c_str()));
}

template<class T> T DLSYM(void *handle, const char *name) {
	return (T)(dlsym(handle, name));
}

static std::map<string, void*> modules;

static struct rusage rusage;
static unsigned long minflt, majflt;

static int harden_rt(void);
static int usr_msglevel = RTAPI_MSG_INFO ; 
static int rt_msglevel = RTAPI_MSG_INFO ; 

static int halsize = 262000; //FIXME
static int instance_id; 
static flavor_ptr flavor;
static const char *rtlibpath = EMC2_RTLIB_DIR;
static int use_drivers = 0;
static int foreground;

static rtapi_switch_t *rtsw; // dont collide with rtapi_switch in hal_lib.so
static global_data_t *gd;    // dont collide with global_data in instance.so

#if 1
// trap: try to collide on purpose - NB: externs
// if there's symbol leakage, this should result in a segfault
rtapi_switch_t *rtapi_switch = NULL;
global_data_t *global_data = NULL;
#endif

//ringbuffer_t rtapi_message_buffer;   // rtapi_message ring access strcuture
static int force_exit = 0;

static int init_actions(int instance, int hal_size, int rtlevel, int userlevel);
static void exit_actions(void);

static int do_newinst_cmd(string type, string name, string arg) {
    void *module = modules["hal_lib"];
    if(!module) {
        rtapi_print_msg(RTAPI_MSG_ERR,
		  "newinst: hal_lib is required, but not loaded\n");
        return -1;
    }

    hal_comp_t *(*find_comp_by_name)(char*) =
        DLSYM<hal_comp_t*(*)(char *)>(module, "halpr_find_comp_by_name");
    if(!find_comp_by_name) {
        rtapi_print_msg(RTAPI_MSG_ERR,
		  "newinst: halpr_find_comp_by_name not found\n");
        return -1;
    }

    hal_comp_t *comp = find_comp_by_name((char*)type.c_str());
    if(!comp) {
        rtapi_print_msg(RTAPI_MSG_ERR,
		  "newinst: component %s not found\n", type.c_str());
        return -1;
    }

    return comp->make((char*)name.c_str(), (char*)arg.c_str());
}

static int do_one_item(char item_type_char, const string &param_name, const string &param_value, void *vitem, int idx=0) {
    char *endp;
    switch(item_type_char) {
        case 'l': {
            long *litem = *(long**) vitem;
            litem[idx] = strtol(param_value.c_str(), &endp, 0);
	    if(*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
			  "`%s' invalid for parameter `%s'",
			  param_value.c_str(), param_name.c_str());
                return -1;
            }
	    return 0;
        }
        case 'i': {
            int *iitem = *(int**) vitem;
            iitem[idx] = strtol(param_value.c_str(), &endp, 0);
	    if(*endp) {
                rtapi_print_msg(RTAPI_MSG_ERR,
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
            rtapi_print_msg(RTAPI_MSG_ERR,
		      "%s: Invalid type character `%c'\n",
		      param_name.c_str(), item_type_char);
            return -1;
        }
    return 0;
}

void remove_quotes(string &s) {
    s.erase(remove_copy(s.begin(), s.end(), s.begin(), '"'), s.end());
}

static int do_comp_args(void *module, vector<string> args) {
    for(unsigned i=1; i < args.size(); i++) {
        string &s = args[i];
	remove_quotes(s);
        size_t idx = s.find('=');
        if(idx == string::npos) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid paramter `%s'\n",
		      s.c_str());
            return -1;
        }
        string param_name(s, 0, idx);
        string param_value(s, idx+1);
        void *item=DLSYM<void*>(module, "rtapi_info_address_" + param_name);
        if(!item) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		      "Unknown parameter `%s'\n", s.c_str());
            return -1;
        }
        char **item_type=DLSYM<char**>(module, "rtapi_info_type_" + param_name);
        if(!item_type || !*item_type) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		      "Unknown parameter `%s' (type information missing)\n",
		      s.c_str());
            return -1;
        }
        string item_type_string = *item_type;

        if(item_type_string.size() > 1) {
            int a, b;
            char item_type_char;
            int r = sscanf(item_type_string.c_str(), "%d-%d%c",
                    &a, &b, &item_type_char);
            if(r != 3) {
                rtapi_print_msg(RTAPI_MSG_ERR,
			  "Unknown parameter `%s' (corrupt array type information): %s\n",
			  s.c_str(), item_type_string.c_str());
                return -1;
            }
            size_t idx = 0;
            int i = 0;
            while(idx != string::npos) {
                if(i == b) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
			      "%s: can only take %d arguments\n",
			      s.c_str(), b);
                    return -1;
                }
                size_t idx1 = param_value.find(",", idx);
                string substr(param_value, idx, idx1 - idx);
                int result = do_one_item(item_type_char, s, substr, item, i);
                if(result != 0) return result;
                i++;
                idx = idx1 == string::npos ? idx1 : idx1 + 1;
            }
        } else {
            char item_type_char = item_type_string[0];
            int result = do_one_item(item_type_char, s, param_value, item);
            if(result != 0) return result;
        }
    }
    return 0;
}

static int do_load_cmd(string name, vector<string> args) {
    void *w = modules[name];
    if(w == NULL) {
        char what[PATH_MAX];

	if (module_path(flavor, what,
			rtlibpath, name.c_str(),
			flavor->mod_ext)) {
	    rtapi_print_msg(RTAPI_MSG_ERR, 
			    "%s: cannot locate module %s:  %s \n", 
			    what,name.c_str(),
			    strerror(errno));
            return -1;
	}

        void *module = modules[name] = dlopen(what, RTLD_GLOBAL |RTLD_LAZY);
        if(!module) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlopen: %s\n", name.c_str(), dlerror());
            return -1;
        }

	// retrieve the address of rtapi_switch_struct
	// so rtapi functions can be called and members
	// access 
	// NB: the reference may NOT be called rtapi_switch or 
	// it might hide rtapi_switch definitions in modules
	if (rtsw == NULL) {

	    rtapi_get_handle_t rtapi_get_handle;
    	    dlerror();
	    rtapi_get_handle = (rtapi_get_handle_t) dlsym(module, "rtapi_get_handle");
	    if (rtapi_get_handle != NULL) {
		rtsw = rtapi_get_handle();
		assert(rtsw != NULL);
		rtapi_print_msg(RTAPI_MSG_DBG, 
				"rtapi_app: handle:%d retrieved %s %s\n", 
				instance_id,
				rtsw->thread_flavor_name,
				rtsw->git_version);
	    }
	}

	/// XXX handle arguments
        int (*start)(void) = DLSYM<int(*)(void)>(module, "rtapi_app_main");
        if(!start) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", name.c_str(), dlerror());
            return -1;
        }
        int result;

        result = do_comp_args(module, args);
        if(result < 0) { dlclose(module); return -1; }

        if ((result=start()) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app_main(%s): %d %s\n", 
		      name.c_str(), result, strerror(-result));
	    modules.erase(modules.find(name));
	    return result;
        }
	rtapi_print_msg(RTAPI_MSG_DBG, "%s: loaded from %s\n", name.c_str(), what);

	// retrieve the address of the global segment
	// this is set only after rtapi_app_main returns sucessfully
	if (gd == NULL) {
	    get_global_handle_t get_global_handle;

	    dlerror();
	    get_global_handle = (get_global_handle_t) dlsym(module, "get_global_handle");
	    if (get_global_handle != NULL) {
		gd = get_global_handle();
		assert(gd != NULL);
		rtapi_print_msg(RTAPI_MSG_DBG,
				"rtapi_app:%d global_data=%p retrieved\n",
				instance_id, gd);

		// rtapi_ringbuffer_init(&gd->error_ring, &rtapi_message_buffer);
		// rtapi_message_buffer.header->refcount++;
		// start rtapi_message_buffer reading thread here
		//

	    }
	}
	return 0;
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: already loaded\n", name.c_str());
    return -1;
}

static int do_unload_cmd(string name) {
    void *w = modules[name];
    if(w == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unload: '%s' not loaded\n", name.c_str());
	return -1;
    } else {
        int (*stop)(void) = DLSYM<int(*)(void)>(w, "rtapi_app_exit");
	if(stop) stop();
	modules.erase(modules.find(name));
        dlclose(w);
	rtapi_print_msg(RTAPI_MSG_DBG, "instance:%d '%s' unloaded\n", 
		  instance_id, name.c_str());
    }
    return 0;
}

struct ReadError : std::exception {};
struct WriteError : std::exception {};

static int read_number(int fd) {
    int r = 0, neg=1;
    char ch;

    while(1) {
        int res = read(fd, &ch, 1);
        if(res != 1) return -1;
        if(ch == '-') neg = -1;
        else if(ch == ' ') return r * neg;
        else r = 10 * r + ch - '0';
    }
}

static string read_string(int fd) {
    int len = read_number(fd);
    char buf[len];
    if(read(fd, buf, len) != len) throw ReadError();
    return string(buf, len);
}

static vector<string> read_strings(int fd) {
    vector<string> result;
    int count = read_number(fd);
    for(int i=0; i<count; i++) {
        result.push_back(read_string(fd));
    }
    return result;
}

static void write_number(string &buf, int num) {
    char numbuf[10];
    sprintf(numbuf, "%d ", num);
    buf = buf + numbuf;
}

static void write_string(string &buf, string s) {
    write_number(buf, s.size());
    buf += s;
}

static void write_strings(int fd, vector<string> strings) {
    string buf;
    write_number(buf, strings.size());
    for(unsigned int i=0; i<strings.size(); i++) {
        write_string(buf, strings[i]);
    }
    if(write(fd, buf.data(), buf.size()) != (ssize_t)buf.size()) throw WriteError();
}

static int handle_command(vector<string> args) {
    if(args.size() == 0) { return 0; }
    if(args.size() == 1 && args[0] == "exit") {
	exit_actions(); // XXX FIXME child only
        force_exit = 1;
        return 0;
    } else if(args.size() == 1 && args[0] == "ping") {
        return 0;
    } else if(args.size() >= 2 && args[0] == "load") {
        string name = args[1];
        args.erase(args.begin());
        return do_load_cmd(name, args);
    } else if(args.size() == 2 && args[0] == "rtlevel") {
	rt_msglevel = atoi(args[1].c_str());
	rtapi_set_msg_level(rt_msglevel);
	if (gd != NULL)
	    gd->rt_msg_level = rt_msglevel;
	rtapi_print_msg(RTAPI_MSG_DBG, "instance:%d RT msglevel set to %d (%s)\n",
		  instance_id, rt_msglevel,
		  gd == NULL ? "local" : "global");
        return 0;
    } else if(args.size() == 2 && args[0] == "usrlevel") {
	usr_msglevel = atoi(args[1].c_str());
	if (gd != NULL)
	    gd->user_msg_level = rt_msglevel;
	rtapi_print_msg(RTAPI_MSG_DBG, "instance:%d User msglevel set to %d (%s)\n",
		  instance_id, usr_msglevel,
		  gd == NULL ? "local" : "global");
        return 0;
    } else if(args.size() == 2 && args[0] == "unload") {
        return do_unload_cmd(args[1]);
    } else if(args.size() == 3 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], "");
    } else if(args.size() == 4 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], args[3]);
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR,
		  "Unrecognized command starting with %s\n",
		  args[0].c_str());
        return -1;
    }
}

static int slave(int fd, vector<string> args) {
    try {
        write_strings(fd, args);
    }
    catch (WriteError &e) {
        rtapi_print_msg(RTAPI_MSG_ERR,
		  "rtapi_app: failed to write to master: %s\n", strerror(errno));
    }

    int result = read_number(fd);
    return result;
}

// shut down the stack in proper order
static void exit_actions()
{
    do_unload_cmd("hal_lib");
    do_unload_cmd("rtapi");
    //    do_unload_cmd("instance");
    gd = NULL;
}

static int init_actions(int instance, int hal_size, int rtlevel, int userlevel)
{
    vector<string> args;
    string arg;
    char buff[100];

    args.push_back(string("rtapi"));
    snprintf(buff, sizeof(buff), "rtapi_instance=%d", instance);
    args.push_back(string(buff));
    snprintf(buff, sizeof(buff), "hal_size=%d",  hal_size);
    args.push_back(string(buff));
    snprintf(buff, sizeof(buff), "rt_msg_level=%d", rtlevel);
    args.push_back(string(buff));
    snprintf(buff, sizeof(buff), "user_msg_level=%d", userlevel);
    args.push_back(string(buff));

    return do_load_cmd("rtapi", args);
}

static char proctitle[20];

static int master(size_t  argc, char **argv, int fd, vector<string> args) {

    int retval;

    if (!foreground) {
	pid_t pid = fork();
	if (pid > 0) { // parent
	    exit(0);
	}
	if (pid < 0) { // fork failed
	    perror("fork");
	    exit(1);
	}
    }

    // set new process name
    snprintf(proctitle, sizeof(proctitle), "rtapi:%d",instance_id);
    size_t argv0_len = strlen(argv[0]);
    size_t procname_len = strlen(proctitle);
    size_t max_procname_len = (argv0_len > procname_len) ? (procname_len) : (argv0_len);
    
    strncpy(argv[0], proctitle, max_procname_len);
    memset(&argv[0][max_procname_len], '\0', argv0_len - max_procname_len);
    
    for (size_t i = 1; i < argc; i++) {
	memset(argv[i], '\0', strlen(argv[i]));
    }
    //rtapi_openlog(proctitle, rt_msglevel );
    rtapi_set_logtag("rtapi_app");
    rtapi_set_msg_level(rt_msglevel);
    rtapi_print_msg(RTAPI_MSG_INFO, "master:%d started", instance_id);

    // child:
    if ((retval = harden_rt())) {
	fprintf(stderr, "rtapi_app:%d failed to setup realtime environment - 'sudo make setuid' missing?\n", instance_id);
	exit(retval);
    }
    // dlopen(NULL, RTLD_GLOBAL);


    if (init_actions(instance_id, halsize, rt_msglevel, usr_msglevel)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "load(instance) failed\n");
	exit(1);
    }
    assert(gd != NULL);
    
    gd->rtapi_app_pid = getpid();

    if(args.size()) { 
        int result = handle_command(args);
        if(result != 0) return result;
        if(force_exit) return 0;
    }
    do {
        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t len = sizeof(client_addr);

        int fd1 = accept(fd, (sockaddr*)&client_addr, &len);
        if(fd1 < 0) {
            perror("accept");
            return -1;
        } else {
            int result;
            try {
                result = handle_command(read_strings(fd1));
            } catch (ReadError &e) {
                rtapi_print_msg(RTAPI_MSG_ERR,
			  "rtapi_app: failed to read from slave: %s\n", strerror(errno));
                close(fd1);
                continue;
            }
            string buf;
            write_number(buf, result);
            if(write(fd1, buf.data(), buf.size()) != (ssize_t)buf.size()) {
                rtapi_print_msg(RTAPI_MSG_ERR,
			  "rtapi_app: failed to write to slave: %s\n", strerror(errno));
            };
            close(fd1);
        }
    } while(!force_exit);

    if (gd)
	gd->rtapi_app_pid = 0;

    return 0;
}

static int configure_memory(void) {
	unsigned int i, pagesize;
	char *buf;

#ifndef RTAPI_POSIX  // Realtime tweak requires privs
	/* Lock all memory. This includes all current allocations (BSS/data)
	 * and future allocations. */
	if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, 
		      "mlockall() failed: %d '%s'\n",
		      errno,strerror(errno)); 
	    return 1;
	}
#endif

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


extern "C" void 
backtrace_handler(int sig, siginfo_t *si, void *uctx)
{
    void *buffer[BACKTRACE_SIZE];
    int j, nptrs;
    char **strings;

    if ((flavor->id == RTAPI_XENOMAI_USER_ID) &&
	(sig == SIGXCPU))
	rtapi_print_msg(RTAPI_MSG_ERR, 
		  "rtapi_app:%d: Xenomai switched RT task to secondary domain\n",
		  instance_id);
    else
	rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app:%d: signal %d - '%s' received\n",
		  instance_id, sig, strsignal(sig));

    nptrs = backtrace(buffer, BACKTRACE_SIZE);

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,"backtrace_symbols(): %s",
		  strerror(errno));
    } else {
	for (j = 0; j < nptrs; j++) {
	    char *s =  strings[j];
	    if (s && strlen(s)) 
		rtapi_print_msg(RTAPI_MSG_ERR,s);
	}
	free(strings);
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app:%d exiting%s", 
	      instance_id,
	      sig == SIGSEGV ? ", core dumped" : "");
    //rtapi_closelog();

    if (gd)
	gd->rtapi_app_pid = 0;

    if (sig == SIGSEGV) {
	sleep(1); // let syslog drain or we might loose
      	abort();  // some important stuff
    }
    exit(sig);
}

static void
exit_handler(void)
{
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    if ((rusage.ru_majflt - majflt) > 0) {
	// RTAPI already shut down here
	rtapi_print_msg(RTAPI_MSG_WARN,"rtapi_app_main %d: %ld page faults, %ld page reclaims\n",
		  getpid(), rusage.ru_majflt - majflt , rusage.ru_minflt - minflt);
    }
}

static int harden_rt()
{
    struct sigaction sig_act;

    // enable core dumps
    struct rlimit core_limit;
    core_limit.rlim_cur = RLIM_INFINITY;
    core_limit.rlim_max = RLIM_INFINITY;
    
    if (setrlimit(RLIMIT_CORE, &core_limit) < 0)
	rtapi_print_msg(RTAPI_MSG_WARN, 
		  "setrlimit: %s - core dumps may be truncated or non-existant\n",
		  strerror(errno));

    // even when setuid root
    if (prctl(PR_SET_DUMPABLE, 1) < 0)
	rtapi_print_msg(RTAPI_MSG_WARN, 
		  "prctl(PR_SET_DUMPABLE) failed: no core dumps will be created - %d - %s\n",
		  errno, strerror(errno));

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

    if (!foreground)
	setsid(); // Detach from the parent session

    sigemptyset( &sig_act.sa_mask );
    sig_act.sa_handler = SIG_IGN; 
    sig_act.sa_sigaction = NULL;

    // prevent stopping of RT threads by ^Z
    sigaction(SIGTSTP, &sig_act, (struct sigaction *) NULL); 

    sig_act.sa_sigaction = backtrace_handler;
    sig_act.sa_flags   = SA_SIGINFO;

    sigaction(SIGSEGV, &sig_act, (struct sigaction *) NULL);
    sigaction(SIGILL,  &sig_act, (struct sigaction *) NULL);
    sigaction(SIGFPE,  &sig_act, (struct sigaction *) NULL);

#if defined(RTAPI_XENOMAI_USER)  // FIXME BUILD_XENOMAI_USER
    if (flavor->id ==  RTAPI_XENOMAI_USER_ID) {
	// check if this user is member of group xenomai, and fail miserably if not
	int numgroups;
	gid_t *grouplist;

	struct group *gp = getgrnam("xenomai");
	if (gp == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "the group 'xenomai' does not exist - xenomai userland support missing?\n");
	    if (gd)
		gd->rtapi_app_pid = 0;
	    exit(1);
	}
	numgroups = getgroups(0,NULL);
	grouplist = (gid_t *) calloc( numgroups, sizeof(gid_t));
	if (getgroups( numgroups, grouplist) != -1) {
	    for (int i = 0; i < numgroups; i++) {
		if (grouplist[i] == gp->gr_gid) {
		    free(grouplist);
		    goto is_xenomai_member;
		}
	    }
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "getgroups() failed: %d - %s\n",
			    errno, strerror(errno));
	    if (gd)
		gd->rtapi_app_pid = 0;
	    exit(1);
	}
	rtapi_print_msg(RTAPI_MSG_ERR,"this user is not member of group xenomai");
	rtapi_print_msg(RTAPI_MSG_ERR,
			"please 'sudo adduser <username>  xenomai', logout and login again");
	if (gd)
	    gd->rtapi_app_pid = 0;
	exit(1);

    is_xenomai_member:
	sigaction(SIGXCPU, &sig_act, (struct sigaction *) NULL);
	rt_print_auto_init(1);
    }
#endif

#if defined(__x86_64) || defined(i386)

    // this is a bit of a shotgun approach and should be made more selective
    // however, due to serial invocations of rtapi_app during setup it is not
    // guaranteed the process executing e.g. hal_parport's rtapi_app_main is
    // the same process which starts the RT threads, causing hal_parport
    // thread functions to fail on inb/outb
    if (use_drivers || (flavor->flags & FLAVOR_DOES_IO)) {
	if (iopl(3) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "cannot gain I/O privileges - forgot 'sudo make setuid'?\n");
	    return -EPERM;
	}
    }
#endif
    return 0;
}

#define RTPRINTBUFFERLEN 1024
void foreground_rtapi_msg_handler(msg_level_t level, const char *fmt,
				  va_list ap) {
    char buf[RTPRINTBUFFERLEN];
    vsnprintf(buf, RTPRINTBUFFERLEN, fmt, ap);
    fprintf(stderr, buf);
}

static void usage(int argc, char **argv) 
{
    printf("Usage:  %s [options]\n", argv[0]);
}

static struct option long_options[] = {
    {"help",  no_argument,          0, 'h'},
    {"foreground",  no_argument,    0, 'F'},
    {"halsize",  required_argument, 0, 'H'},
    {"usrmsglevel", required_argument, 0, 'u'},
    {"rtmsglevel", required_argument, 0, 'r'},
    {"instance", required_argument, 0, 'I'},
    {"flavor",   required_argument, 0, 'f'},
    {"rtpath",   required_argument, 0, 'R'},
    {"drivers",   required_argument, 0, 'D'},
    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    struct sockaddr_un addr = { AF_UNIX, "" };
    int c;

    while (1) {
	int option_index = 0;
	int curind = optind;
	c = getopt_long (argc, argv, "hH:m:I:f:r:u:R:NF",
			 long_options, &option_index);
	if (c == -1)
	    break;

	switch (c)	{

	case 'D':
	    use_drivers = 1;
	    break;

	case 'F':
	    foreground = 1;
	    rtapi_set_msg_handler(foreground_rtapi_msg_handler);
	    break;

	case 'H':
	    halsize = atoi(optarg);
	    break;

	case 'u':
	    usr_msglevel = atoi(optarg);
	    break;

     	case 'r':
	    rt_msglevel = atoi(optarg);
	    break;

     	case 'R':
	    rtlibpath = strdup(optarg);
	    break;

	case 'I':
	    instance_id = atoi(optarg);
	    break;

	case 'f':
	    if ((flavor = flavor_byname(optarg)) == NULL) {
		fprintf(stderr, "no such flavor: '%s' -- valid flavors are:\n", optarg);
		flavor_ptr f = flavors;
		while (f->name) {
		    fprintf(stderr, "\t%s\n", f->name);
		    f++;
		} 
		exit(1);
	    }
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

    if (flavor == NULL)
	flavor = default_flavor();
    assert(flavor != NULL);

    if ((use_drivers || (flavor->flags & FLAVOR_DOES_IO)) && !getuid()) {
	fprintf(stderr, "rtapi_app: need to 'sudo make setuid' to access I/O\n");
	exit(1);
    }

    snprintf(addr.sun_path, sizeof(addr.sun_path), 
	     SOCKET_PATH, instance_id);
    addr.sun_path[0] = '\0';

    vector<string> args;
    if (optind < argc) {
	while (optind < argc) {
	    args.push_back(string(argv[optind++]));
	}
    }


become_master:
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) { perror("socket"); exit(1); }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    //struct sockaddr_un addr = { AF_UNIX, socket_path };
    int result = bind(fd, (sockaddr*)&addr, sizeof(addr));

    // if the bind succeeded, nobody is listening on the socket,
    // so become master
    if(result == 0) {
        int result = listen(fd, 10);
        if(result != 0) { perror("listen"); exit(1); }
        result = master(argc, argv, fd, args);
        unlink(SOCKET_PATH);
	rtapi_print_msg(RTAPI_MSG_INFO, "master:%d exit %d\n", 
		  instance_id, result);
        return result;
    } else if(errno == EADDRINUSE) {
	// the master is already running, so become slave
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);
        gettimeofday(&t1, NULL);
        for(int i=0; i < 3 || (t1.tv_sec < 3 + t0.tv_sec) ; i++) {
            result = connect(fd, (sockaddr*)&addr, sizeof(addr));
            if(result == 0) break;
            if(i==0) srand48(t0.tv_sec ^ t0.tv_usec);
            usleep(lrand48() % 100000);
            gettimeofday(&t1, NULL);
        }
        if(result < 0 && errno == ECONNREFUSED) { 
            unlink(SOCKET_PATH);
	    rtapi_print_msg(RTAPI_MSG_WARN, 
			    "slave:%d:  Waited 3 seconds for master.  giving up.",
			    instance_id);
            close(fd);
            goto become_master;
        }
        if(result < 0) { perror("connect"); exit(1); }
        return slave(fd, args);
    } else {
	rtapi_print_msg(RTAPI_MSG_WARN, 
		   "instance:%d:  bind failed: %s", 
		   instance_id, strerror(errno));
        perror("bind"); exit(1);
    }
}
