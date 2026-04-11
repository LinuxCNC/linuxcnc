/* Copyright (C) 2006-2014 Jeff Epler <jepler@unpythonic.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <linuxcnc.h>

#ifdef __linux__
#include <sys/fsuid.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <signal.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include <sys/resource.h>
#include <sys/mman.h>
#ifdef __linux__
#include <malloc.h>
#include <sys/prctl.h>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

#include <fmt/format.h>
#include <boost/lockfree/queue.hpp>

#include "rtapi.h"
#include <hal.h>
#include "hal/hal_priv.h"
#include "uspace_common.h"

static RtapiApp &App();

struct message_t {
    msg_level_t level;
    char msg[1024 - sizeof(level)];
};

static boost::lockfree::queue<message_t, boost::lockfree::capacity<128>> rtapi_msg_queue;

static pthread_t queue_thread;
static void *queue_function(void * /*arg*/) {
    set_namef("rtapi_app:mesg");
    // note: can't use anything in this function that requires App() to exist
    // but it's OK to use functions that aren't safe for realtime (that's the
    // point of running this in a thread)
    while (1) {
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
        rtapi_msg_queue.consume_all([](const message_t &m) {
            fputs(m.msg, m.level == RTAPI_MSG_ALL ? stdout : stderr);
        });
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        struct timespec ts = {0, 10000000};
        rtapi_clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL, NULL);
    }
    return nullptr;
}

static int sim_rtapi_run_threads(int fd, int (*callback)(int fd));

template <class T> T DLSYM(void *handle, const std::string &name) {
    return (T)(dlsym(handle, name.c_str()));
}

template <class T> T DLSYM(void *handle, const char *name) {
    return (T)(dlsym(handle, name));
}

static std::map<std::string, void *> modules;

static int instance_count = 0;
static int force_exit = 0;

static int do_newinst_cmd(const std::string &type, const std::string &name, const std::string &arg) {
    void *module = modules["hal_lib"];
    if (!module) {
        rtapi_print_msg(RTAPI_MSG_ERR, "newinst: hal_lib is required, but not loaded\n");
        return -1;
    }

    hal_comp_t *(*find_comp_by_name)(char *) = DLSYM<hal_comp_t *(*)(char *)>(module, "halpr_find_comp_by_name");
    if (!find_comp_by_name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "newinst: halpr_find_comp_by_name not found\n");
        return -1;
    }

    hal_comp_t *comp = find_comp_by_name((char *)type.c_str());
    if (!comp) {
        rtapi_print_msg(RTAPI_MSG_ERR, "newinst: component %s not found\n", type.c_str());
        return -1;
    }

    return comp->make((char *)name.c_str(), (char *)arg.c_str());
}

static int do_one_item(
    char item_type_char, const std::string &param_name, const std::string &param_value, void *vitem, int idx = 0
) {
    char *endp;
    switch (item_type_char) {
    case 'l': {
        long *litem = *(long **)vitem;
        litem[idx] = strtol(param_value.c_str(), &endp, 0);
        if (*endp) {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "`%s' invalid for parameter `%s'\n", param_value.c_str(), param_name.c_str()
            );
            return -1;
        }
        return 0;
    }
    case 'i': {
        int *iitem = *(int **)vitem;
        iitem[idx] = strtol(param_value.c_str(), &endp, 0);
        if (*endp) {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "`%s' invalid for parameter `%s'\n", param_value.c_str(), param_name.c_str()
            );
            return -1;
        }
        return 0;
    }
    case 's': {
        char **sitem = *(char ***)vitem;
        sitem[idx] = strdup(param_value.c_str());
        return 0;
    }
    default:
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: Invalid type character `%c'\n", param_name.c_str(), item_type_char);
        return -1;
    }
    return 0;
}

static void remove_quotes(std::string &s) {
    s.erase(remove_copy(s.begin(), s.end(), s.begin(), '"'), s.end());
}

static int do_comp_args(void *module, std::vector<std::string> args) {
    for (unsigned i = 1; i < args.size(); i++) {
        std::string &s = args[i];
        remove_quotes(s);
        size_t idx = s.find('=');
        if (idx == std::string::npos) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Invalid parameter `%s'\n", s.c_str());
            return -1;
        }
        std::string param_name(s, 0, idx);
        std::string param_value(s, idx + 1);
        void *item = DLSYM<void *>(module, "rtapi_info_address_" + param_name);
        if (!item) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Unknown parameter `%s'\n", s.c_str());
            return -1;
        }
        char **item_type = DLSYM<char **>(module, "rtapi_info_type_" + param_name);
        if (!item_type || !*item_type) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Unknown parameter `%s' (type information missing)\n", s.c_str());
            return -1;
        }

        int *max_size_ptr = DLSYM<int *>(module, "rtapi_info_size_" + param_name);

        char item_type_char = **item_type;
        if (max_size_ptr) {
            int max_size = *max_size_ptr;
            size_t idx = 0;
            int i = 0;
            while (idx != std::string::npos) {
                if (i == max_size) {
                    rtapi_print_msg(RTAPI_MSG_ERR, "%s: can only take %d arguments\n", s.c_str(), max_size);
                    return -1;
                }
                size_t idx1 = param_value.find(",", idx);
                std::string substr(param_value, idx, idx1 - idx);
                int result = do_one_item(item_type_char, s, substr, item, i);
                if (result != 0)
                    return result;
                i++;
                idx = idx1 == std::string::npos ? idx1 : idx1 + 1;
            }
        } else {
            int result = do_one_item(item_type_char, s, param_value, item);
            if (result != 0)
                return result;
        }
    }
    return 0;
}

static int do_load_cmd(const std::string &name, const std::vector<std::string> &args) {
    void *w = modules[name];
    if (w == NULL) {
        std::string what;
        what = fmt::format("{}/{}.so", EMC2_RTLIB_DIR, name);
        void *module = modules[name] = dlopen(what.c_str(), RTLD_GLOBAL | RTLD_NOW);
        if (!module) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlopen: %s\n", name.c_str(), dlerror());
            modules.erase(name);
            return -1;
        }
        /// XXX handle arguments
        int (*start)(void) = DLSYM<int (*)(void)>(module, "rtapi_app_main");
        if (!start) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", name.c_str(), dlerror());
            dlclose(module);
            modules.erase(name);
            return -1;
        }
        if(!DLSYM<void(*)(void)>(module, "rtapi_app_exit")) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: component is missing rtapi_app_exit\n", name.c_str());
            dlclose(module);
            modules.erase(name);
            return -1;
        }
        int result;

        result = do_comp_args(module, args);
        if (result < 0) {
            dlclose(module);
            modules.erase(name);
            return -1;
        }

        if ((result = start()) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %s (%d)\n", name.c_str(), strerror(-result), result);
            dlclose(module);
            modules.erase(name);
            return result;
        } else {
            instance_count++;
            return 0;
        }
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: already exists\n", name.c_str());
        return -1;
    }
}

static int do_unload_cmd(const std::string &name) {
    void *w = modules[name];
    if (w == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: not loaded\n", name.c_str());
        return -1;
    } else {
        void (*stop)(void) = DLSYM<void(*)(void)>(w, "rtapi_app_exit");
        if (stop)
            stop();
        modules.erase(modules.find(name));
        dlclose(w);
        instance_count--;
    }
    return 0;
}

static int do_debug_cmd(const std::string &value) {
    try {
        int new_level = stoi(value);
        if (new_level < 0 || new_level > 5) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Debug level must be >=0 and <= 5\n");
            return -EINVAL;
        }
        return rtapi_set_msg_level(new_level);
    } catch (std::invalid_argument &e) {
        //stoi will throw an exception if parsing is not possible
        rtapi_print_msg(RTAPI_MSG_ERR, "Debug level is not a number\n");
        return -EINVAL;
    }
}

struct ReadError : std::exception {};
struct WriteError : std::exception {};

static int read_number(int fd) {
    int r = 0, neg = 1;
    char ch;

    while (1) {
        int res = read(fd, &ch, 1);
        if (res != 1)
            return -1;
        if (ch == '-')
            neg = -1;
        else if (ch == ' ')
            return r * neg;
        else
            r = 10 * r + ch - '0';
    }
}

static std::string read_string(int fd) {
    int len = read_number(fd);
    if (len < 0)
        throw ReadError();
    if (!len)
        return std::string();
    std::string str(len, 0);
    if (read(fd, str.data(), len) != len)
        throw ReadError();
    return str;
}

static std::vector<std::string> read_strings(int fd) {
    std::vector<std::string> result;
    int count = read_number(fd);
    if (count < 0)
        return result;
    for (int i = 0; i < count; i++) {
        result.push_back(read_string(fd));
    }
    return result;
}

static void write_number(std::string &buf, int num) {
    buf = buf + fmt::format("{} ", num);
}

static void write_string(std::string &buf, const std::string &s) {
    write_number(buf, s.size());
    buf += s;
}

static void write_strings(int fd, const std::vector<std::string> &strings) {
    std::string buf;
    write_number(buf, strings.size());
    for (unsigned int i = 0; i < strings.size(); i++) {
        write_string(buf, strings[i]);
    }
    if (write(fd, buf.data(), buf.size()) != (ssize_t)buf.size())
        throw WriteError();
}

static int handle_command(std::vector<std::string> args) {
    if (args.size() == 0) {
        return 0;
    }
    if (args.size() == 1 && args[0] == "exit") {
        force_exit = 1;
        return 0;
    } else if (args.size() >= 2 && args[0] == "load") {
        std::string name = args[1];
        args.erase(args.begin());
        return do_load_cmd(name, args);
    } else if (args.size() == 2 && args[0] == "unload") {
        return do_unload_cmd(args[1]);
    } else if (args.size() == 3 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], "");
    } else if (args.size() == 4 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], args[3]);
    } else if (args.size() == 2 && args[0] == "debug") {
        return do_debug_cmd(args[1]);
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "Unrecognized command starting with %s\n", args[0].c_str());
        return -1;
    }
}

static int slave(int fd, const std::vector<std::string> &args) {
    try {
        write_strings(fd, args);
    } catch (WriteError &e) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to write to master: %s\n", strerror(errno));
    }

    int result = read_number(fd);
    return result;
}

static int callback(int fd) {
    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t len = sizeof(client_addr);
    int fd1 = accept(fd, (sockaddr *)&client_addr, &len);
    if (fd1 < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to accept connection from slave: %s\n", strerror(errno));
        return -1;
    } else {
        int result;
        try {
            result = handle_command(read_strings(fd1));
        } catch (ReadError &e) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to read from slave: %s\n", strerror(errno));
            close(fd1);
            return -1;
        }
        std::string buf;
        write_number(buf, result);
        if (write(fd1, buf.data(), buf.size()) != (ssize_t)buf.size()) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to write to slave: %s\n", strerror(errno));
        };
        close(fd1);
    }
    return !force_exit && instance_count > 0;
}

static pthread_t main_thread{};

static int master(int fd, const std::vector<std::string> &args) {
    main_thread = pthread_self();
    int result;
    if ((result = pthread_create(&queue_thread, nullptr, &queue_function, nullptr)) != 0) {
        errno = result;
        perror("pthread_create (queue function)");
        return -1;
    }
    do_load_cmd("hal_lib", std::vector<std::string>());
    instance_count = 0;
    App(); // force rtapi_app to be created
    if (args.size()) {
        result = handle_command(args);
        if (result != 0)
            goto out;
        if (force_exit || instance_count == 0)
            goto out;
    }
    sim_rtapi_run_threads(fd, callback);
out:
    pthread_cancel(queue_thread);
    pthread_join(queue_thread, nullptr);
    rtapi_msg_queue.consume_all([](const message_t &m) {
        fputs(m.msg, m.level == RTAPI_MSG_ALL ? stdout : stderr);
    });
    return result;
}

static std::string get_fifo_path() {
    std::string s;
    if (getenv("RTAPI_FIFO_PATH")) {
        s = getenv("RTAPI_FIFO_PATH");
    } else if (getenv("HOME")) {
        s = std::string(getenv("HOME")) + "/.rtapi_fifo";
    } else {
        rtapi_print_msg(
            RTAPI_MSG_ERR, "rtapi_app: RTAPI_FIFO_PATH and HOME are unset.  rtapi fifo creation is unsafe.\n"
        );
    }
    return s;
}

static int get_fifo_path(char *buf, size_t bufsize) {
    int len;
    const std::string s = get_fifo_path();
    if (s.empty()) {
        return -1;
    }
    if (s.size() + 1 > sizeof(sockaddr_un::sun_path)) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "rtapi_app: rtapi fifo path is too long (arch limit %zd): %s\n",
            sizeof(sockaddr_un::sun_path),
            s.c_str()
        );
        return -1;
    }
    len = snprintf(buf + 1, bufsize - 1, "%s", s.c_str());
    return len;
}

int main(int argc, char **argv) {
    if (getuid() == 0) {
        char *fallback_uid_str = getenv("RTAPI_UID");
        int fallback_uid = fallback_uid_str ? atoi(fallback_uid_str) : 0;
        if (fallback_uid == 0) {
            // Cppcheck cannot see EMC2_BIN_DIR when RTAPI is defined, but that
            // doesn't happen in uspace.
            fprintf(
                stderr,
                "Refusing to run as root without fallback UID specified\n"
                "To run under a debugger with I/O, use e.g.,\n"
                // cppcheck-suppress unknownMacro
                "    sudo env RTAPI_UID=`id -u` RTAPI_FIFO_PATH=$HOME/.rtapi_fifo gdb " EMC2_BIN_DIR "/rtapi_app\n"
            );
            exit(1);
        }
        if (setreuid(fallback_uid, 0) != 0) {
            perror("setreuid");
            abort();
        }
        fprintf(stderr, "Running with fallback_uid.  getuid()=%d geteuid()=%d\n", getuid(), geteuid());
    }
    ruid = getuid();
    euid = geteuid();
    if (setresuid(euid, euid, ruid) != 0) {
        perror("setresuid");
        abort();
    }
#ifdef __linux__
    setfsuid(ruid);
#endif
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }

become_master:
    int len = 0;
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(1);
    }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    struct sockaddr_un addr;
    memset(&addr, 0x0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if ((len = get_fifo_path(addr.sun_path, sizeof(addr.sun_path))) < 0)
        exit(1);

    // plus one because we use the abstract namespace, it will show up in
    // /proc/net/unix prefixed with an @
    int result = ::bind(fd, (sockaddr *)&addr, len + sizeof(addr.sun_family) + 1);

    if (result == 0) {
        //If called in master mode with exit command, no need to start master
        //and exit again
        if (args.size() == 1 && args[0] == "exit") {
            return 0;
        }
        int result = listen(fd, 10);
        if (result != 0) {
            perror("listen");
            exit(1);
        }
        setsid(); // create a new session if we can...
        result = master(fd, args);
        return result;
    } else if (errno == EADDRINUSE) {
        struct timeval t0, t1;
        gettimeofday(&t0, NULL);
        gettimeofday(&t1, NULL);
        for (int i = 0; i < 3 || (t1.tv_sec < 3 + t0.tv_sec); i++) {
            result = connect(fd, (sockaddr *)&addr, len + sizeof(addr.sun_family) + 1);
            if (result == 0)
                break;
            if (i == 0)
                srand48(t0.tv_sec ^ t0.tv_usec);
            usleep(lrand48() % 100000);
            gettimeofday(&t1, NULL);
        }
        if (result < 0 && errno == ECONNREFUSED) {
            fprintf(stderr, "Waited 3 seconds for master.  giving up.\n");
            close(fd);
            goto become_master;
        }
        if (result < 0) {
            fprintf(stderr, "connect %s: %s", addr.sun_path, strerror(errno));
            exit(1);
        }
        return slave(fd, args);
    } else {
        perror("bind");
        exit(1);
    }
}


/* These structs hold data associated with objects like tasks, etc. */
/* Task handles are pointers to these structs.                      */

struct rtapi_module {
    int magic;
};

#define MODULE_MAGIC 30812
#define SHMEM_MAGIC 25453

#define MAX_MODULES 64
#define MODULE_OFFSET 32768

static void signal_handler(int sig, siginfo_t * /*si*/, void * /*uctx*/) {
    switch (sig) {
    case SIGXCPU:
        // should not happen - must be handled in RTAPI if enabled
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: BUG: SIGXCPU received - exiting\n");
        exit(0);
        break;

    case SIGTERM:
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: SIGTERM - shutting down\n");
        exit(0);
        break;

    default: // pretty bad
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: caught signal %d - dumping core\n", sig);
        sleep(1); // let syslog drain
        signal(sig, SIG_DFL);
        // for reasons unknown raise(sig); doesn't lead to core dump file
        // but this will
        kill(getpid(), sig);
        break;
    }
    exit(1);
}

const static size_t PRE_ALLOC_SIZE = 1024 * 1024 * 32;
const static struct rlimit unlimited = {RLIM_INFINITY, RLIM_INFINITY};
static void configure_memory() {
    int res = setrlimit(RLIMIT_MEMLOCK, &unlimited);
    if (res < 0)
        perror("setrlimit");

    res = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (res < 0)
        perror("mlockall");

#ifdef __linux__
    /* Turn off malloc trimming.*/
    if (!mallopt(M_TRIM_THRESHOLD, -1)) {
        rtapi_print_msg(RTAPI_MSG_WARN, "mallopt(M_TRIM_THRESHOLD, -1) failed\n");
    }
    /* Turn off mmap usage. */
    if (!mallopt(M_MMAP_MAX, 0)) {
        rtapi_print_msg(RTAPI_MSG_WARN, "mallopt(M_MMAP_MAX, -1) failed\n");
    }
#endif
    /*
     * The following code seems pointless, but there is a non-observable effect
     * in the allocation and loop.
     *
     * The malloc() is forced to set brk() because mmap() allocation is
     * disabled in a call to mallopt() above. All touched pages become resident
     * and locked in the loop because of above mlockall() call (see notes in
     * mlockall(2)). The mallopt() trim setting prevents the brk() from being
     * reduced after free(), effectively creating an open space for future
     * allocations that will not generate page faults.
     *
     * The qualifier 'volatile' on the buffer pointer is required because newer
     * clang would remove the malloc(), for()-loop and free() completely.
     * Marking 'buf' volatile ensures that the code will remain in place.
     */
    volatile char *buf = static_cast<volatile char *>(malloc(PRE_ALLOC_SIZE));
    if (buf == NULL) {
        rtapi_print_msg(RTAPI_MSG_WARN, "malloc(PRE_ALLOC_SIZE) failed\n");
        return;
    }
    long pagesize = sysconf(_SC_PAGESIZE);
    /* Touch each page in this piece of memory to get it mapped into RAM */
    for (size_t i = 0; i < PRE_ALLOC_SIZE; i += pagesize) {
        /* Each write to this buffer will generate a pagefault.
             * Once the pagefault is handled a page will be locked in
             * memory and never given back to the system. */
        buf[i] = 0;
    }
    free((void *)buf);
}

static int harden_rt() {
    if (!rtapi_is_realtime())
        return -EINVAL;

    WITH_ROOT;
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
    if (iopl(3) < 0) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "iopl() failed: %s\n"
            "cannot gain I/O privileges - "
            "forgot 'sudo make setuid' or using secure boot? -"
            "parallel port access is not allowed\n",
            strerror(errno)
        );
    }
#endif

    struct sigaction sig_act = {};
#ifdef __linux__
    // enable realtime
    if (setrlimit(RLIMIT_RTPRIO, &unlimited) < 0) {
        rtapi_print_msg(RTAPI_MSG_WARN, "setrlimit(RTLIMIT_RTPRIO): %s\n", strerror(errno));
        return -errno;
    }

    // enable core dumps
    if (setrlimit(RLIMIT_CORE, &unlimited) < 0)
        rtapi_print_msg(
            RTAPI_MSG_WARN, "setrlimit: %s - core dumps may be truncated or non-existent\n", strerror(errno)
        );

    // even when setuid root
    if (prctl(PR_SET_DUMPABLE, 1) < 0)
        rtapi_print_msg(
            RTAPI_MSG_WARN,
            "prctl(PR_SET_DUMPABLE) failed: no core dumps will be created - %d - %s\n",
            errno,
            strerror(errno)
        );
#endif /* __linux__ */

    configure_memory();

    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_handler = SIG_IGN;
    sig_act.sa_sigaction = NULL;

    // prevent stopping of RT threads by ^Z
    sigaction(SIGTSTP, &sig_act, (struct sigaction *)NULL);

    sig_act.sa_sigaction = signal_handler;
    sig_act.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sig_act, (struct sigaction *)NULL);
    sigaction(SIGILL, &sig_act, (struct sigaction *)NULL);
    sigaction(SIGFPE, &sig_act, (struct sigaction *)NULL);
    sigaction(SIGTERM, &sig_act, (struct sigaction *)NULL);
    sigaction(SIGINT, &sig_act, (struct sigaction *)NULL);

#ifdef __linux__
    int fd = open("/dev/cpu_dma_latency", O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        rtapi_print_msg(RTAPI_MSG_WARN, "failed to open /dev/cpu_dma_latency: %s\n", strerror(errno));
    } else {
        int r;
        r = write(fd, "\0\0\0\0", 4);
        if (r != 4) {
            rtapi_print_msg(RTAPI_MSG_WARN, "failed to write to /dev/cpu_dma_latency: %s\n", strerror(errno));
        }
        // deliberately leak fd until program exit
    }
#endif /* __linux__ */
    return 0;
}

static RtapiApp *makeDllApp(std::string dllName, int policy) {
    void *dll = nullptr;
    dll = dlopen(dllName.c_str(), RTLD_NOW);
    if (!dll) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return nullptr;
    }
    auto fn = reinterpret_cast<RtapiApp *(*)(int policy)>(dlsym(dll, "make"));
    if (!fn) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        return nullptr;
    }
    auto result = fn(policy);
    if (!result) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        return nullptr;
    }
    return result;
}

static RtapiApp *makeApp() {
    RtapiApp *app;
    if (euid != 0 || harden_rt() < 0) {
        app = makeDllApp("libuspace-posix.so.0", SCHED_OTHER);
    } else {
        WithRoot r;
        if (detect_xenomai_evl()) {
            app = makeDllApp("libuspace-xenomai-evl.so.0", SCHED_FIFO);
        } else if (detect_xenomai()) {
            app = makeDllApp("libuspace-xenomai.so.0", SCHED_FIFO);
        } else if (detect_rtai()) {
            app = makeDllApp("libuspace-rtai.so.0", SCHED_FIFO);
        } else {
            app = makeDllApp("libuspace-posix.so.0", SCHED_FIFO);
        }
    }

    if (!app) {
        throw std::invalid_argument("Could not load rtapi dll");
    } else {
        return app;
    }
}
RtapiApp &App() {
    static RtapiApp *app = makeApp();
    return *app;
}

int rtapi_prio_highest(void) {
    return App().prio_highest();
}

int rtapi_prio_lowest(void) {
    return App().prio_lowest();
}

int rtapi_prio_next_higher(int prio) {
    return App().prio_next_higher(prio);
}

int rtapi_prio_next_lower(int prio) {
    return App().prio_next_lower(prio);
}

long rtapi_clock_set_period(long nsecs) {
    return App().clock_set_period(nsecs);
}

int rtapi_task_new(void (*taskcode)(void *), void *arg, int prio, int owner, unsigned long int stacksize, int uses_fp) {
    return App().task_new(taskcode, arg, prio, owner, stacksize, uses_fp);
}

int rtapi_task_delete(int id) {
    return App().task_delete(id);
}

int rtapi_task_start(int task_id, unsigned long period_nsec) {
    int ret = App().task_start(task_id, period_nsec);
    if (ret != 0) {
        errno = -ret;
        perror("rtapi_task_start()");
    }
    return ret;
}

int rtapi_task_pause(int task_id) {
    return App().task_pause(task_id);
}

int rtapi_task_resume(int task_id) {
    return App().task_resume(task_id);
}

int rtapi_task_self() {
    return App().task_self();
}

long long rtapi_task_pll_get_reference(void) {
    return App().task_pll_get_reference();
}

int rtapi_task_pll_set_correction(long value) {
    return App().task_pll_set_correction(value);
}

void rtapi_wait(void) {
    App().wait();
}

void rtapi_outb(unsigned char byte, unsigned int port) {
    App().do_outb(byte, port);
}

unsigned char rtapi_inb(unsigned int port) {
    return App().do_inb(port);
}

long int simple_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}

int sim_rtapi_run_threads(int fd, int (*callback)(int fd)) {
    return App().run_threads(fd, callback);
}

long long rtapi_get_time() {
    return App().do_get_time();
}

void default_rtapi_msg_handler(msg_level_t level, const char *fmt, va_list ap) {
    if (main_thread && pthread_self() != main_thread) {
        message_t m;
        m.level = level;
        vsnprintf(m.msg, sizeof(m.msg), fmt, ap);
        rtapi_msg_queue.push(m);
    } else {
        vfprintf(level == RTAPI_MSG_ALL ? stdout : stderr, fmt, ap);
    }
}

long int rtapi_delay_max() {
    return 10000;
}

void rtapi_delay(long ns) {
    if (ns > rtapi_delay_max())
        ns = rtapi_delay_max();
    App().do_delay(ns);
}

const unsigned long ONE_SEC_IN_NS = 1000000000;
void rtapi_timespec_advance(struct timespec &result, const struct timespec &src, unsigned long nsec) {
    time_t sec = src.tv_sec;
    while (nsec >= ONE_SEC_IN_NS) {
        ++sec;
        nsec -= ONE_SEC_IN_NS;
    }
    nsec += src.tv_nsec;
    if (nsec >= ONE_SEC_IN_NS) {
        ++sec;
        nsec -= ONE_SEC_IN_NS;
    }
    result.tv_sec = sec;
    result.tv_nsec = nsec;
}

int rtapi_open_as_root(const char *filename, int mode) {
    WITH_ROOT;
    int r = open(filename, mode);
    if (r < 0)
        return -errno;
    return r;
}

int rtapi_spawn_as_root(
    pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]
) {
    return posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

int rtapi_spawnp_as_root(
    pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]
) {
    return posix_spawnp(pid, path, file_actions, attrp, argv, envp);
}
