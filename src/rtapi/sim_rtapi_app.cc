#include <sys/types.h>
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

#include "config.h"

using namespace std;

#define FIFO_PATH "/tmp/rtapi_fifo"

template<class T> T DLSYM(void *handle, const string &name) {
	return reinterpret_cast<T>(dlsym(handle, name.c_str()));
}

template<class T> T DLSYM(void *handle, const char *name) {
	return reinterpret_cast<T>(dlsym(handle, name));
}

static std::map<string, void*> modules;

extern "C" int schedule(void) { return sched_yield(); }

static int instance_count = 0;

static int do_newinst_cmd(string type, string name, string arg) {
    cerr << "newinst not implemented\n";
    return -1;
}

static int do_comp_args(void *module, vector<string> args) {
    for(unsigned i=1; i < args.size(); i++) {
        string &s = args[i];
        unsigned idx = s.find('=');
        if(idx == string::npos) {
            cerr << "Cannot understand: " << s << endl;
            return -1;
        }
        string param_name(s, 0, idx);
        string param_value(s, idx+1);
        void *item=DLSYM<void*>(module, "rtapi_info_address_" + param_name);
        if(!item) {
            cerr << "Cannot understand: (NULL item) " << s << "\n";
            cerr << dlerror() << endl;
            return -1;
        }
        char **item_type=DLSYM<char**>(module, "rtapi_info_type_" + param_name);
        if(!item_type || !*item_type) {
            cerr << "Cannot understand: (NULL type) " << s << dlerror() << "\n";
            return -1;
        }
        string item_type_string = *item_type;
        cerr << "Type for item " << param_name << " is " << *item_type << endl;
        printf("item: %p\n", item);

        if(item_type_string == "l") {
            **(long**)item = strtol(param_value.c_str(), NULL, 0);
        } else if(item_type_string == "i") {
            **(int**)item = strtol(param_value.c_str(), NULL, 0);
        } else {
            cerr << "Cannot understand: " << s << " (type)\n";
            return -1;
        }
    }
    return 0;
}

static int do_load_cmd(string name, vector<string> args) {
    void *w = modules[name];
    if(w == NULL) {
        char what[LINELEN+1];
        snprintf(what, LINELEN, "%s/%s.so", EMC2_RTLIB_DIR, name.c_str());
        void *module = modules[name] = dlopen(what, RTLD_GLOBAL | RTLD_NOW);
        if(!module) {
            printf("%s: dlopen: %s\n", name.c_str(), dlerror());
            return -1;
        }
	/// XXX handle arguments
        int (*start)(void) = DLSYM<int(*)(void)>(module, "rtapi_app_main");
        if(!start) {
            printf("%s: dlsym: %s\n", name.c_str(), dlerror());
            return -1;
        }
        int result;

        result = do_comp_args(module, args);
        if(result < 0) { dlclose(module); return -1; }

        if ((result=start()) < 0) {
            printf("%s: rtapi_app_main: %d\n", name.c_str(), result);
	    return result;
        } else {
            instance_count ++;
	    return 0;
        }
    } else {
        printf("%s: already exists\n", name.c_str());
        return -1;
    }
}

static int do_unload_cmd(string name) {
    void *w = modules[name];
    if(w == NULL) {
        printf("%s: not loaded\n", name.c_str());
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

static int read_number(int fd) {
    int r = 0;
    char ch;

    while(1) {
        read(fd, &ch, 1);
        if(ch == ' ') return r;
        r = 10 * r + ch - '0';
    }
}

static string read_string(int fd) {
    int len = read_number(fd);
    char buf[len];
    read(fd, buf, len);
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
    write(fd, buf.data(), buf.size());
}

static void print_strings(vector<string> strings) {
    cout << "STRINGS:";
    for(unsigned int i=0; i<strings.size(); i++) { 
        cout << " " << strings[i];
    }
    cout << "\n";
}

static int handle_command(vector<string> args) {
    if(args.size() == 1 && args[0] == "exit") {
        unlink(FIFO_PATH);
        exit(0);
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
        cout << "Unrecognized command: ";
        print_strings(args);
        return -1;
    }
}

static int slave(int fd, vector<string> args) {
    cout << "slave\n"; fflush(stdout);
    write_strings(fd, args);
    return 0;
}

static int master(int fd, vector<string> args) {
    cout << "master\n"; fflush(stdout);
    dlopen(NULL, RTLD_GLOBAL);
    do_load_cmd("hal_lib", vector<string>()); instance_count = 0;
    if(args.size()) handle_command(args);
    do { handle_command(read_strings(fd));
        cerr << "INSTANCE COUNT:" << instance_count << endl; } while(instance_count > 0);
    return 0;
}

int main(int argc, char **argv) {
    vector<string> args;
    for(int i=1; i<argc; i++) { args.push_back(string(argv[i])); }
become_master:
    int result = mknod(FIFO_PATH, 0666 | S_IFIFO, 0);
    if(result != 0 && errno != EEXIST) {
        cout << result << " " << (result != 0) << " " << (result != EEXIST) << endl;
        perror("mknod"); exit(1);
    }
    if(result == 0) {
        int fd = open(FIFO_PATH, O_RDWR | O_EXCL);
        if(fd < 0) {
            perror("open"); exit(1);
        }
        int result = master(fd, args);
        unlink(FIFO_PATH);
        return result;
    } else {
        int fd = -1;
	for(int i=0 ; fd < 0 && i <3 ; i++) {
	    fd = open(FIFO_PATH, O_WRONLY | O_EXCL | O_NONBLOCK);
	    if(fd < 0) sleep(1);
	}
	if(fd < 0 && errno == ENXIO) { 
	    unlink(FIFO_PATH);
	    fprintf(stdout, "Waited 3 seconds for master.  giving up.\n");
	    goto become_master;
	}
        return slave(fd, args);
    }
}

