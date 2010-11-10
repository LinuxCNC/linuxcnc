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
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "config.h"

#include "rtapi.h"
#include "hal.h"
#include "hal/hal_priv.h"

extern "C" int sim_rtapi_run_threads(int fd);

using namespace std;

#define SOCKET_PATH "\0/tmp/rtapi_fifo"

template<class T> T DLSYM(void *handle, const string &name) {
	return (T)(dlsym(handle, name.c_str()));
}

template<class T> T DLSYM(void *handle, const char *name) {
	return (T)(dlsym(handle, name));
}

static std::map<string, void*> modules;

extern "C" int schedule(void) { return sched_yield(); }

static int instance_count = 0;
static int force_exit = 0;

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
                    "Unknown parameter `%s' (corrupt array type information)\n",
                    s.c_str());
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
	    return result;
        } else {
            instance_count ++;
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
        instance_count --;
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
    } else if(args.size() >= 2 && args[0] == "load") {
        string name = args[1];
        args.erase(args.begin());
        return do_load_cmd(name, args);
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
    dlopen(NULL, RTLD_GLOBAL);
    do_load_cmd("hal_lib", vector<string>()); instance_count = 0;
    if(args.size()) { 
        int result = handle_command(args);
        if(result != 0) return result;
        if(force_exit || instance_count == 0) return 0;
    }
    do {
        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t len = sizeof(client_addr);

	sim_rtapi_run_threads(fd);

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
    } while(!force_exit && instance_count > 0);

    return 0;
}

int main(int argc, char **argv) {
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
