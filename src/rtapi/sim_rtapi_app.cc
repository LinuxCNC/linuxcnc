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
#include <errno.h>
#include <malloc.h>

#include <sys/mman.h>
#include <execinfo.h>
#include <sys/prctl.h>
#if defined(RTAPI_XENOMAI_USER)
#include <grp.h>
#include <rtdk.h>
#endif

#include "rtapi.h"
#include "hal.h"
#include "hal/hal_priv.h"

//extern "C" int sim_rtapi_run_threads(int fd);
static int harden_rt();

using namespace std;

#define SOCKET_PATH "\0/tmp/rtapi_fifo"

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

extern "C" int schedule(void) { return sched_yield(); }

static int msglevel = RTAPI_MSG_WARN;
static int force_exit = 0;

static struct rusage rusage;
static unsigned long minflt, majflt;


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
        char what[LINELEN+1];
        snprintf(what, LINELEN, "%s/%s.so", EMC2_RTLIB_DIR, name.c_str());
        void *module = modules[name] = dlopen(what, RTLD_GLOBAL | RTLD_LAZY);
        if(!module) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlopen: %s\n", name.c_str(), dlerror());
            return -1;
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
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %d\n", name.c_str(), result);
	    modules.erase(modules.find(name));
	    return result;
        } else {
	    return 0;
        }
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: already exists\n", name.c_str());
        return -1;
    }
}

static int do_unload_cmd(string name) {
    void *w = modules[name];
    if(w == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: not loaded\n", name.c_str());
	return -1;
    } else {
        int (*stop)(void) = DLSYM<int(*)(void)>(w, "rtapi_app_exit");
	if(stop) stop();
	modules.erase(modules.find(name));
        dlclose(w);
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
        force_exit = 1;
        return 0;
    } else if(args.size() == 1 && args[0] == "ping") {
        return 0;
    } else if(args.size() >= 2 && args[0] == "load") {
        string name = args[1];
        args.erase(args.begin());
        return do_load_cmd(name, args);
    } else if(args.size() == 2 && args[0] == "msglevel") {
	msglevel = atoi(args[1].c_str());
	rtapi_print_msg(RTAPI_MSG_DBG, "rtapi_app: msglevel set to %d\n",msglevel);
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

static int master(int fd, vector<string> args) {

    int retval;

    pid_t pid = fork();
    if (pid > 0) { // parent
	exit(0);
    }
    if (pid < 0) { // fork failed
	perror("fork");
	exit(1);
    }
    // child:
    if ((retval = harden_rt()))
	exit(retval);

    dlopen(NULL, RTLD_GLOBAL);
    // do_load_cmd("hal_lib", vector<string>());
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

    //    do_unload_cmd("hal_lib"); // assure destructors called

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
			    "rtapi_app_main: mlockall() failed: %d '%s'\n",
			    errno,strerror(errno)); 
	    return 1;
	}
#endif

	/* Turn off malloc trimming.*/
	if (!mallopt(M_TRIM_THRESHOLD, -1)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, "rtapi_app_main: mallopt(M_TRIM_THRESHOLD, -1) failed\n");
	    return 1;
	}
	/* Turn off mmap usage. */
	if (!mallopt(M_MMAP_MAX, 0)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, "rtapi_app_main: mallopt(M_MMAP_MAX, -1) failed\n");
	    return 1;
	}
	buf = static_cast<char *>(malloc(PRE_ALLOC_SIZE));
	if (buf == NULL) {
	    rtapi_print_msg(RTAPI_MSG_WARN, "rtapi_app_main: malloc(PRE_ALLOC_SIZE) failed\n");
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

void backtrace_handler(int sig, siginfo_t *si, void *uctx)
{
    void *bt[32];
    int nentries;
 
#if defined(RTAPI_XENOMAI_USER)
    if (sig == SIGXCPU) 
	rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app %d: Xenomai switched RT task to secondary domain\n",
			getpid());
    else
#endif
	rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app %d: signal %d - '%s' received\n",
			getpid(), sig, strsignal(sig));

    nentries = backtrace(bt,sizeof(bt) / sizeof(bt[0]));
    backtrace_symbols_fd(bt,nentries,fileno(stderr));
    if (sig != SIGXCPU)
	abort();
    exit(sig);
}

static void
exit_handler(void)
{
    struct rusage rusage;
	
    getrusage(RUSAGE_SELF, &rusage);
    rtapi_print_msg(RTAPI_MSG_DBG, "rtapi_app_main %d: %ld page faults, %ld page reclaims\n",
		    getpid(), rusage.ru_majflt - majflt , rusage.ru_minflt - minflt);
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
			"rtapi_app_main: setrlimit: %s - core dumps may be truncated or non-existant\n",
			strerror(errno));

    // even when setuid root
    if (prctl(PR_SET_DUMPABLE, 1) < 0)
	rtapi_print_msg(RTAPI_MSG_WARN, 
			"prctl(PR_SET_DUMPABLE) failed: no core dumps will be created - %d - %s\n",
			errno, strerror(errno));

    configure_memory();    

    if (getrusage(RUSAGE_SELF, &rusage)) {
	rtapi_print_msg(RTAPI_MSG_WARN, 
			"rtapi_app_main: getrusage(RUSAGE_SELF) failed: %d '%s'\n",
			errno,strerror(errno)); 
    } else {
	minflt = rusage.ru_minflt;
	majflt = rusage.ru_majflt;

	if (atexit(exit_handler)) {
	    rtapi_print_msg(RTAPI_MSG_WARN, 
			    "rtapi_app_main: atexit() failed: %d '%s'\n",
			    errno,strerror(errno)); 
	}
    }

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

#if defined(RTAPI_XENOMAI_USER)
    // check if this user is member of group xenomai, and fail miserably if not
    int numgroups;
    gid_t *grouplist;

    struct group *gp = getgrnam("xenomai");
    if (gp == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_app_main: the group 'xenomai' does not exist - xenomai userland support missing?\n");
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
			"rtapi_app_main: getgroups() failed: %d - %s\n",
			errno, strerror(errno));
	exit(1);
    }
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "rtapi_app_main: this user is not member of group xenomai\n"
		    "please 'sudo adduser <username>  xenomai', logout and login again\n");
    exit(1);

 is_xenomai_member:
    sigaction(SIGXCPU, &sig_act, (struct sigaction *) NULL);
    rt_print_auto_init(1);
#endif

#if defined(BUILD_DRIVERS) && (defined(__x86_64) || defined(i386))

    // this is a bit of a shotgun approach and should be made more selective
    // however, due to serial invocations of rtapi_app during setup it is not
    // guaranteed the process executing e.g. hal_parport's rtapi_app_main is
    // the same process which starts the RT threads, causing hal_parport
    // thread functions to fail on inb/outb

    if (iopl(3) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_app: cannot gain I/O privileges - forgot 'sudo make setuid'?\n");
	return -EPERM;
    }
#endif
    return 0;
}

int main(int argc, char **argv)
{

    vector<string> args;
    for(int i=1; i<argc; i++) { args.push_back(string(argv[i])); }

become_master:
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd == -1) { perror("socket"); exit(1); }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    struct sockaddr_un addr = { AF_UNIX, SOCKET_PATH };
    int result = bind(fd, (sockaddr*)&addr, sizeof(addr));

    if(result == 0) {
        int result = listen(fd, 10);
        if(result != 0) { perror("listen"); exit(1); }
        result = master(fd, args);
        unlink(SOCKET_PATH);
        return result;
    } else if(errno == EADDRINUSE) {
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
            fprintf(stderr, "Waited 3 seconds for master.  giving up.\n");
            close(fd);
            goto become_master;
        }
        if(result < 0) { perror("connect"); exit(1); }
        return slave(fd, args);
    } else {
        perror("bind"); exit(1);
    }
}
