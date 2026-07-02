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
#include <grp.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include <sys/resource.h>
#include <sys/mman.h>
#ifdef __linux__
#include <malloc.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

#include <fmt/format.h>
#include <boost/lockfree/queue.hpp>

#include "rtapi.h"
#include <hal.h>
#include "uspace_common.h"

static RtapiApp &App();

//Indicates if this instance is a master
static bool is_master = false;

#ifdef __linux__
// detect_preempt_rt() inspects uname for the PREEMPT_RT marker.  Used only
// for diagnostic warning at startup; callers must not gate behavior on
// the kernel string, since SCHED_FIFO on a PREEMPT_DYNAMIC kernel is still
// useful (better than SCHED_OTHER, worse than PREEMPT_RT).
static bool detect_preempt_rt() {
    struct utsname u;
    if(uname(&u) < 0) return 0;
    return strcasestr(u.version, "PREEMPT RT") != NULL
        || strcasestr(u.version, "PREEMPT_RT") != NULL;
}
#else
static bool detect_preempt_rt() {
    return false;
}
#endif

#ifdef __linux__
// detect_preempt_dynamic() inspects uname for the PREEMPT_DYNAMIC marker.
static bool detect_preempt_dynamic() {
    struct utsname u;
    if(uname(&u) < 0) return false;
    return strcasestr(u.version, "PREEMPT DYNAMIC") != NULL
        || strcasestr(u.version, "PREEMPT_DYNAMIC") != NULL;
}
#else
static bool detect_preempt_dynamic() {
    return false;
}
#endif

#if defined(USPACE_RTAI) || defined(USPACE_XENOMAI) || defined(USPACE_XENOMAI_EVL)
static bool has_setuid_root() {
    return geteuid() == 0;
}
#endif

#if defined(USPACE_XENOMAI) || defined(USPACE_XENOMAI_EVL)
static bool is_current_user_in_gid(gid_t target_gid) {
    int ngroups = getgroups(0, NULL);
    if (ngroups < 0) {
        perror("getgroups size failed");
        return false;
    }

    gid_t *groups = (gid_t *)malloc(ngroups * sizeof(gid_t));
    if (groups == NULL) {
        perror("malloc failed");
        return false;
    }

    ngroups = getgroups(ngroups, groups);
    if (ngroups < 0) {
        perror("getgroups get failed");
        free(groups);
        return false;
    }

    bool found = false;
    for (int i = 0; i < ngroups; i++) {
        if (groups[i] == target_gid) {
            found = true;
            break;
        }
    }

    free(groups);
    return found;
}
#endif

#ifdef USPACE_RTAI
// FIXME: detect_rtai_lxrt relays on setuid root
static bool detect_rtai_lxrt() {
    if(!has_setuid_root()) return false;
    struct utsname u;
    uname(&u);
    return strcasestr (u.release, "-rtai") != NULL;
}
#else
static bool detect_rtai_lxrt() {
    return false;
}
#endif
#ifdef USPACE_XENOMAI
static bool detect_xenomai() {
    //Running xenomai has /proc/xenomai
    struct stat sb;
    if (stat("/proc/xenomai", &sb) != 0) {
        //No xenomai
        return false;
    }

    if (has_setuid_root()) {
        //xenomai and setuid, all fine
        return true;
    }

    //Get allowed xenomai group
    //Set to xenomai by /etc/init.d/xenomai from libxenomai1 debian package
    int gid = 0;
    FILE *fp = fopen("/sys/module/xenomai/parameters/allowed_group", "r");
    if (fp == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: detect_xenomai fopen failed: %m\n");
        return false;
    }

    int ret = fscanf(fp, "%i", &gid);
    if (ret != 1) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: detect_xenomai fscanf failed: %m\n");
        fclose(fp);
        return false;
    }
    fclose(fp);

    if (gid == 0) {
        if(is_master){
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Warning: Xenomai kernel running but xenomai\n"
                "  access in /sys/module/xenomai/parameters/allowed_group\n"
                "  is set to root and not setuid root.\n"
                "  Fix: enable /etc/init.d/xenomai (preferred) or 'sudo make setuid'\n");
        }
        return false;
    }

    if(!is_current_user_in_gid(gid)){
        if(is_master){
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Warning: Xenomai kernel running but current user not\n"
                "  in xenomai group (gid: %i) or setuid root\n"
                "  Fix: 'sudo adduser $USER xenomai' (preferred) or 'sudo make setuid'\n", gid);
        }
        return false;
    }

    return true;
}
#else
static bool detect_xenomai() {
    return false;
}
#endif
#ifdef USPACE_XENOMAI_EVL
static bool detect_xenomai_evl() {
    //Running xenomai evl has /dev/evl but no /proc/xenomai
    struct stat sb;
    if(stat("/dev/evl/control", &sb) != 0){
        //No EVL
        return false;
    }
    if(has_setuid_root()){
        //EVL and setuid, all fine
        return true;
    }

    //Check if current user is in group of /dev/evl/control
    //Set to evl by /usr/lib/udev/rules.d/90-libevl.rules from libevl debian package
    //Note: There are more files in /dev/evl which need to have the proper
    //access rights set. However, we check only one of them.
    if (sb.st_gid == 0) {
        if(is_master){
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Warning: Xenomai4 EVL kernel running but /dev/evl/control\n"
                "  access is set to root and not setuid root.\n"
                "  Fix: enable enable 90-libevl.rules (preferred) or 'sudo make setuid'\n");
        }
        return false;
    }

    if(!is_current_user_in_gid(sb.st_gid)){
        if(is_master){
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Warning: Xenomai4 EVL kernel running but current user not\n"
                "  in evl group (gid: %i) or setuid root\n"
                "  Fix: 'sudo adduser $USER evl' (preferred) or 'sudo make setuid'\n", sb.st_gid);
        }
        return false;
    }

    return true;
}
#else
static bool detect_xenomai_evl() {
    return false;
}
#endif

static bool detect_force(){
    const char *force = getenv("LINUXCNC_FORCE_REALTIME");
    if(force != NULL && atoi(force) != 0){
        return true;
    }else{
        return false;
    }
}

#ifdef __linux__
// Diagnostic helper: report cap_effective state for a single capability.
// Returns "yes", "no", or "unknown" if libcap could not introspect.
static const char *cap_effective_str(cap_t caps, cap_value_t cap) {
    if (!caps) return "unknown";
    cap_flag_value_t v;
    if (cap_get_flag(caps, cap, CAP_EFFECTIVE, &v) != 0) return "unknown";
    return v == CAP_SET ? "yes" : "no";
}
#endif

static void report_sched_fifo_error(int sched_err){
    // Surface the actual reason so the user does not have to guess
    // between "no caps", "stock kernel", or "wrong rlimits" (issue
    // #3928).  errno comes from the SCHED_FIFO probe in
    // can_set_sched_fifo(); cap state comes from libcap.
    const char *nice_s = "unknown";
    const char *lock_s = "unknown";
#ifdef __linux__
    cap_t caps = cap_get_proc();
    if(caps == NULL){
        rtapi_print_msg(RTAPI_MSG_ERR, "cap_get_proc failed: %m\n");
    }else{
        nice_s = cap_effective_str(caps, CAP_SYS_NICE);
        lock_s = cap_effective_str(caps, CAP_IPC_LOCK);
        cap_free(caps);
    }
#endif
    rtapi_print_msg(RTAPI_MSG_ERR,
        "Note: realtime scheduling unavailable "
        "(sched_setscheduler SCHED_FIFO: %s).\n"
        "  Process capabilities: cap_sys_nice=%s cap_ipc_lock=%s.\n"
        "  Falling back to POSIX non-realtime.\n"
        "  Fix: 'sudo make setcap' (preferred) or 'sudo make setuid' "
        "on rtapi_app.\n"
        "  Override (testing only): set LINUXCNC_FORCE_REALTIME=1.\n",
        sched_err ? strerror(sched_err) : "denied",
        nice_s, lock_s);
}

// Success-probe for realtime scheduling: briefly try to set SCHED_FIFO on
// the calling thread and restore the previous policy.  Succeeds when the
// process holds CAP_SYS_NICE (file caps or setuid root) or has a matching
// RLIMIT_RTPRIO.  Works on any kernel, so the probe also covers the
// PREEMPT_RT-vs-stock distinction implicitly: if we can actually get
// SCHED_FIFO, the platform can deliver realtime, regardless of how.
// Only report an error if this check fails in master mode to not spam the user.
static bool can_set_sched_fifo(void) {
    struct sched_param old_param, probe_param;
    int old_policy = sched_getscheduler(0);
    if(old_policy < 0) {
        if(is_master) report_sched_fifo_error(errno);
        return false;
    }
    if(sched_getparam(0, &old_param) < 0) {
        if(is_master) report_sched_fifo_error(errno);
        return false;
    }

    memset(&probe_param, 0, sizeof(probe_param));
    probe_param.sched_priority = sched_get_priority_min(SCHED_FIFO);
    if(sched_setscheduler(0, SCHED_FIFO, &probe_param) < 0) {
        if(is_master) report_sched_fifo_error(errno);
        return false;
    }

    // Best-effort restore; if this fails we are still on SCHED_FIFO at
    // minimum priority, which is no worse than where we started.
    sched_setscheduler(0, old_policy, &old_param);
    return true;
}

// rtapi_get_realtime_type() reports whether this process can actually run
// realtime code and returns the type it should run.
// This matches the convention used by JACK, PipeWire,
// rtkit, Xenomai, and Klipper: surface the observed capability, not
// kernel metadata.  The old setuid-root stat check has been removed; it
// stat()ed EMC2_BIN_DIR/rtapi_app rather than the running binary (breaking
// wrapper-based installs like NixOS /run/wrappers) and silently masked
// LINUXCNC_FORCE_REALTIME (see issue #3928).
rtapi_realtime_type_t rtapi_get_realtime_type(void){
    static rtapi_realtime_type_t cached = REALTIME_TYPE_UNINITIALIZED;
    if(cached != REALTIME_TYPE_UNINITIALIZED){
        return cached;
    }

    if(!detect_force() && !can_set_sched_fifo()){
        cached = REALTIME_TYPE_NONE;
        return cached;
    }

    if(detect_rtai_lxrt()){
        cached = REALTIME_TYPE_LXRT;
        return cached;
    }
    if(detect_xenomai()){
        cached = REALTIME_TYPE_XENOMAI;
        return cached;
    }
    if(detect_xenomai_evl()){
        cached = REALTIME_TYPE_XENOMAI_EVL;
        return cached;
    }
    if(detect_preempt_rt()){
        cached = REALTIME_TYPE_PREEMPT_RT;
        return cached;
    }

    if(detect_force()){
        //Use REALTIME_TYPE_PREEMPT_DYNAMIC / REALTIME_TYPE_UNKNOWN only if forced
        //This is not recommended
        if(detect_preempt_dynamic()){
            cached = REALTIME_TYPE_PREEMPT_DYNAMIC;
        }else{
            if(is_master){
                rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_get_realtime_type: Realtime type unknown but SCHED_FIFO available\n");
            }
            cached = REALTIME_TYPE_UNKNOWN;
        }
    }else{
        if(is_master){
            rtapi_print_msg(RTAPI_MSG_ERR,
                "Note: realtime scheduling unavailable.\n"
                "  Falling back to POSIX non-realtime.\n"
                "  Override (testing only): set LINUXCNC_FORCE_REALTIME=1.\n");
        }
        cached = REALTIME_TYPE_NONE;
    }
    return cached;
}

int rtapi_is_realtime() {
    return rtapi_get_realtime_type() > REALTIME_TYPE_NONE;
}

struct message_t {
    msg_level_t level;
    char msg[1024 - sizeof(level)];
};

static boost::lockfree::queue<message_t, boost::lockfree::capacity<128>> rtapi_msg_queue;

static pthread_t queue_thread;
static void *queue_function(void * /*arg*/) {
    RtapiApp::set_namef("rtapi_app:mesg");
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

    int (*invoke_make)(const char *, const char *, const char *);
    invoke_make = DLSYM<int (*)(const char *, const char *, const char *)>(module, "hal_comp_invoke_make");
    if(!invoke_make) {
        rtapi_print_msg(RTAPI_MSG_ERR, "newinst: hal_comp_invoke_make not found\n");
        return -1;
    }

    int rv = invoke_make(type.c_str(), name.c_str(), arg.c_str());
    if(rv) {
        if(-ENOENT == rv)
            rtapi_print_msg(RTAPI_MSG_ERR, "newinst: component %s not found\n", type.c_str());
        else if(-ENOEXEC == rv)
            rtapi_print_msg(RTAPI_MSG_ERR, "newinst: component %s does not support instantiation\n", type.c_str());
        else
            rtapi_print_msg(RTAPI_MSG_ERR, "newinst: component %s returned error %d\n", type.c_str(), rv);
        return -1;
    }

    return 0;
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

static std::string remove_quotes(const std::string &s) {
    std::string ret;
    std::remove_copy(s.begin(), s.end(), std::back_inserter(ret), '"');
    return ret;
}

static int do_comp_args(void *module, const std::vector<std::string> &args) {
    for (size_t i = 0; i < args.size(); i++) {
        std::string s = remove_quotes(args[i]);
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
        //Sanitize the name
        if (name.find("/") != std::string::npos || name.find("..") != std::string::npos) {
            rtapi_print_msg(
                RTAPI_MSG_ERR,
                "%s: Not allowed as module name. Slashes or with \"..\" (even /a..b/) are not allowed.\n",
                name.c_str()
            );
            return -1;
        }
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

// Human-readable name of a realtime type, suitable for display by callers
// such as 'realtime verify' and the latency tools.
static const char *realtime_type_name(rtapi_realtime_type_t type) {
    switch(type) {
        case REALTIME_TYPE_UNINITIALIZED:   return "Uninitialized";
        case REALTIME_TYPE_NONE:            return "No realtime";
        case REALTIME_TYPE_UNKNOWN:         return "Unknown (SCHED_FIFO)";
        case REALTIME_TYPE_PREEMPT_DYNAMIC: return "Preempt Dynamic";
        case REALTIME_TYPE_PREEMPT_RT:      return "Preempt RT";
        case REALTIME_TYPE_RTAI:            return "RTAI";
        case REALTIME_TYPE_LXRT:            return "LXRT";
        case REALTIME_TYPE_XENOMAI:         return "Xenomai";
        case REALTIME_TYPE_XENOMAI_EVL:     return "Xenomai EVL";
    }
    return "BUG: unknown realtime type";
}

static int do_check_rt_cmd(std::string &out) {
    out = realtime_type_name(rtapi_get_realtime_type());
    return rtapi_is_realtime() == 0;
}

/*
 * Fully checked send/recv
 * Will retry on EINTR, so to abort a send_data/recv_data on a
 * signal, change to something like while(remaining > 0 && !exit_flag)
 * and set exit_flag in signal handler
 */
static ssize_t send_data(int fd, const void *buf, size_t n, int flags) {
    if(n > SSIZE_MAX){
        errno=EFBIG;
        return -1;
    }
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t n_rem = n;
    while (n_rem > 0) {
        ssize_t n_ret = send(fd, ptr, n_rem, flags);
        if (n_ret == -1) {
            if (errno == EINTR) {
                // Retry
            } else {
                return -1; // Other error, fail
            }
        } else if (n_ret == 0) {
            return (n - n_rem); // No more data
        } else {
            ptr += n_ret;
            n_rem -= n_ret;
        }
    }
    return n; // All sent
}

static ssize_t recv_data(int fd, void *buf, size_t n, int flags) {
    if(n > SSIZE_MAX){
        errno=EFBIG;
        return -1;
    }
    uint8_t *ptr = (uint8_t *)buf;
    size_t n_rem = n;
    while (n_rem > 0) {
        ssize_t n_ret = recv(fd, ptr, n_rem, flags);
        if (n_ret == -1) {
            if (errno == EINTR) {
                // Retry
            } else {
                return -1; // Other error, fail
            }
        } else if (n_ret == 0) {
            return (n - n_rem); // No more data
        } else {
            ptr += n_ret;
            n_rem -= n_ret;
        }
    }
    return n; // All read
}

/*
 * Protocol:
 * 
 * client->master: std::vector<std::string> args
 * master processes the args and returns result and stdout string
 * master->client: int result, std::string out
 *
 * Packing:
 * args are serialized as:
 * uint16_t size (full package size including the size field)
 * uint16_t n_args
 * n_args times:
 * {
 *      uint16_t arg_size
 *      char[arg_size] argument
 * }
 * 
 * result is serialized as:
 * uint16_t size (full package size including the size field)
 * int result
 * uint16_t out_size
 * char[out_size] out
 */

static void push_uint16(std::vector<char> &buf, uint16_t value) {
    buf.push_back((char)(0xff & (value >> 0)));
    buf.push_back((char)(0xff & (value >> 8)));
}

static uint16_t get_uint16(const std::vector<char> &buf, size_t idx) {
    //at() will check index and throw std::out_of_range
    return ((uint16_t)(unsigned char)buf.at(idx)) | ((uint16_t)(unsigned char)buf.at(idx + 1) << 8);
}

static void push_int(std::vector<char> &buf, int value) {
    for (size_t i = 0; i < sizeof(int); i++) {
        buf.push_back((char)(0xff & (value >> i * 8)));
    }
}

static int get_int(const std::vector<char> &buf, size_t idx) {
    //at() will check index and throw std::out_of_range
    int ret = 0;
    for (size_t i = 0; i < sizeof(int); i++) {
        ret |= ((int)(unsigned char)buf.at(idx + i) << i * 8);
    }
    return ret;
}

static bool send_result(int fd, int result, const std::string &out) {
    //Calculate size
    size_t buff_size = sizeof(uint16_t) + sizeof(int) + sizeof(uint16_t) + out.size();

    //Check uint16_t conversion of the total size; out.size() is bounded by it
    if (buff_size > std::numeric_limits<uint16_t>::max()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: send_result: result too big, size = %li!\n", buff_size);
        return false;
    }

    //Serialize
    std::vector<char> buf;
    buf.reserve(buff_size);
    push_uint16(buf, (uint16_t)buff_size);
    push_int(buf, result);
    push_uint16(buf, (uint16_t)out.size());
    buf.insert(buf.end(), out.begin(), out.end());
    if (buf.size() != buff_size) {
        rtapi_print_msg(
            RTAPI_MSG_ERR, "rtapi_app: Bug send_result: buf.size() %li != buff_size %li\n", buf.size(), buff_size
        );
        return false;
    }

    //Send
    ssize_t res = send_data(fd, buf.data(), buf.size(), 0);
    if (res != (ssize_t)buf.size()) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: send_result failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: send_result failed, sent only %li of %li bytes\n", res, buf.size()
            );
        }
        return false;
    }
    return true;
}

static bool recv_result(int fd, int *result, std::string &out) {
    //Get size
    uint16_t tmp;
    ssize_t res = recv_data(fd, &tmp, sizeof(uint16_t), 0);
    if (res != sizeof(uint16_t)) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_result 1 failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: recv_result 1 failed, recv only %li of %li bytes\n", res, sizeof(uint16_t)
            );
        }
        return false;
    }
    if (tmp < sizeof(uint16_t)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_result: bad frame size %u\n", tmp);
        return false;
    }
    size_t buff_size = tmp - sizeof(uint16_t); //Size already consumed

    //Get data
    std::vector<char> buf(buff_size);
    res = recv_data(fd, buf.data(), buff_size, 0);
    if (res != (ssize_t)buff_size) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_result 2 failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: recv_result 2 failed, recv only %li of %li bytes\n", res, buff_size
            );
        }
        return false;
    }

    //Deserialize
    try {
        size_t idx = 0;
        *result = get_int(buf, idx);
        idx += sizeof(int);
        size_t out_size = get_uint16(buf, idx);
        idx += sizeof(uint16_t);
        //Bound checked, unpack argument
        auto start = buf.begin() + idx;
        auto end = start + out_size;
        if (end > buf.end()) {
            throw std::out_of_range("recv_result: out size not in buffer range");
        }
        out = std::string(start, end);
        idx += out_size;
        if (idx != buff_size) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: Bug recv_result: idx %li != buff_size %li\n", idx, buff_size);
            return false;
        }
    } catch (std::out_of_range &e) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: Bug recv_result: %s\n", e.what());
        return false;
    }

    return true;
}

static bool recv_args(int fd, std::vector<std::string> &args) {
    //Get size
    uint16_t tmp;
    ssize_t res = recv_data(fd, &tmp, sizeof(uint16_t), 0);
    if (res != sizeof(uint16_t)) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_args 1 failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: recv_args 1 failed, recv only %li of %li bytes\n", res, sizeof(uint16_t)
            );
        }
        return false;
    }
    if (tmp < sizeof(uint16_t)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_args: bad frame size %u\n", tmp);
        return false;
    }
    size_t buff_size = tmp - sizeof(uint16_t); //Size already consumed

    //Get data
    std::vector<char> buf(buff_size);
    res = recv_data(fd, buf.data(), buff_size, 0);
    if (res != (ssize_t)buff_size) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: recv_args 2 failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: recv_args 2 failed, recv only %li of %li bytes\n", res, buff_size
            );
        }
        return false;
    }

    //Deserialize
    try {
        size_t idx = 0;
        size_t n_args = get_uint16(buf, idx);
        args.resize(n_args);
        idx += sizeof(uint16_t);
        for (size_t i = 0; i < n_args; i++) {
            size_t arg_size = get_uint16(buf, idx);
            idx += sizeof(uint16_t);
            //Bound checked, unpack argument
            auto start = buf.begin() + idx;
            auto end = start + arg_size;
            if (end > buf.end()) {
                throw std::out_of_range("recv_args: arg size not in buffer range");
            }
            args[i] = std::string(start, end);
            idx += arg_size;
        }
        if (idx != buff_size) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: Bug recv_args: idx %li != buff_size %li\n", idx, buff_size);
            return false;
        }
    } catch (std::out_of_range &e) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: Bug recv_args: %s\n", e.what());
        return false;
    }

    return true;
}

static bool send_args(int fd, const std::vector<std::string> &args) {
    //Calculate size
    size_t buff_size = 0;
    buff_size += 2 * sizeof(uint16_t);
    for (size_t i = 0; i < args.size(); i++) {
        buff_size += sizeof(uint16_t);
        buff_size += args[i].size();
    }

    //Check uint16_t conversions
    //Buffer size is > sum(args[i].size()) so they don't need a separate check
    if (buff_size > std::numeric_limits<uint16_t>::max()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: send_args: args too big, size = %li!\n", buff_size);
        return false;
    }
    //Edge case: One could in theory send many size zero args
    if (args.size() > std::numeric_limits<uint16_t>::max()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: send_args: arg count too big, size = %li!\n", args.size());
        return false;
    }

    //Serialize
    std::vector<char> buf;
    buf.reserve(buff_size);
    push_uint16(buf, (uint16_t)buff_size);
    push_uint16(buf, (uint16_t)args.size());
    for (size_t i = 0; i < args.size(); i++) {
        push_uint16(buf, (uint16_t)args[i].size());
        buf.insert(buf.end(), args[i].begin(), args[i].end());
    }
    if (buf.size() != buff_size) {
        rtapi_print_msg(
            RTAPI_MSG_ERR, "rtapi_app: Bug send_args: buf.size() %li != buff_size %li\n", buf.size(), buff_size
        );
        return false;
    }

    //Send
    ssize_t res = send_data(fd, buf.data(), buf.size(), 0);
    if (res != (ssize_t)buf.size()) {
        if (res == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: send_args failed: %s\n", strerror(errno));
        } else {
            rtapi_print_msg(
                RTAPI_MSG_ERR, "rtapi_app: send_args failed, sent only %li of %li bytes\n", res, buf.size()
            );
        }
        return false;
    }
    return true;
}

static int handle_command(const std::vector<std::string> &args, std::string &out) {
    if (args.size() == 0) {
        return 0;
    }
    if (args.size() == 1 && args[0] == "start") {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: start received while running\n");
        return 0;
    } else if (args.size() == 1 && args[0] == "ping") {
        //Just return success
        return 0;
    } else if (args.size() == 1 && args[0] == "exit") {
        force_exit = 1;
        return 0;
    } else if (args.size() >= 2 && args[0] == "load") {
        std::string name = args[1];
        std::vector<std::string> args_rem(args.begin() + 2, args.end());
        return do_load_cmd(name, args_rem);
    } else if (args.size() == 2 && args[0] == "unload") {
        return do_unload_cmd(args[1]);
    } else if (args.size() == 3 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], "");
    } else if (args.size() == 4 && args[0] == "newinst") {
        return do_newinst_cmd(args[1], args[2], args[3]);
    } else if (args.size() == 2 && args[0] == "debug") {
        return do_debug_cmd(args[1]);
    } else if (args.size() == 1 && args[0] == "check_rt") {
        return do_check_rt_cmd(out);
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: unrecognized command starting with %s\n", args[0].c_str());
        return -1;
    }
}

static int slave(int fd, const std::vector<std::string> &args) {
    if (!send_args(fd, args)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to write to master\n");
        return -1;
    }

    int result = -1;
    std::string out;
    if (!recv_result(fd, &result, out)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to read from master\n");
        return -1;
    } else {
        if (out.length() > 0) {
            printf("%s\n", out.c_str());
        }
        return result;
    }
}

//Processes incoming command on socket
//This function blocks on accept until a client connects
//return: true if master should continue
//        false if master should exit
static bool master_process_socket_command(int fd) {
    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t len = sizeof(client_addr);
    int fd1 = accept(fd, (sockaddr *)&client_addr, &len);
    if (fd1 < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to accept connection from slave: %s\n", strerror(errno));
        return true; //If there is a socket error, just continue, no need to check errno
    } else {
        int result;
        std::vector<std::string> args;
        std::string out;

        //Set timeout, so master doesn't hang forever
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(fd1, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: setsockopt timeout failed: %s\n", strerror(errno));
            close(fd1);
            return true; //If there is a socket error, just continue
        }

        if (!recv_args(fd1, args)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to read from slave\n");
            close(fd1);
            return true; //If there is a socket error, just continue
        }

        result = handle_command(args, out);

        if (!send_result(fd1, result, out)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: failed to write to slave\n");
        }
        close(fd1);
    }
    return !force_exit;
}

static pthread_t main_thread{};

static int master(int fd) {
    is_master = true;
    main_thread = pthread_self();
    int result;
    if ((result = pthread_create(&queue_thread, nullptr, &queue_function, nullptr)) != 0) {
        errno = result;
        perror("pthread_create (queue function)");
        return -1;
    }
    do_load_cmd("hal_lib", std::vector<std::string>());
    App(); // force rtapi_app to be created
    //Process commands as long as master should not exit
    while(master_process_socket_command(fd));
    do_unload_cmd("hal_lib");
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

static bool get_fifo_path_to_addr(struct sockaddr_un *addr) {
    const std::string s = get_fifo_path();
    if (s.empty()) {
        return false;
    }
    if (s.size() + 2 > sizeof(addr->sun_path)) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "rtapi_app: rtapi fifo path is too long (arch limit %zd): %s\n",
            sizeof(sockaddr_un::sun_path),
            s.c_str()
        );
        return false;
    }
    //See: https://www.man7.org/linux/man-pages/man7/unix.7.html abstract
    //sun_path[0] is a null byte ('\0')
    addr->sun_path[0] = 0;
    strncpy(addr->sun_path + 1, s.c_str(), sizeof(addr->sun_path) - 2);
    return true;
}

static double diff_timespec(const struct timespec *time1, const struct timespec *time0) {
    return (double)(time1->tv_sec - time0->tv_sec) + (double)(time1->tv_nsec - time0->tv_nsec) / 1000000000.0;
}

#ifdef __linux__
static void raise_net_admin_ambient(void);
#endif

static int create_socket(){
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return fd;
    }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    return fd;
}

static int start_master(int fd){
    int result = listen(fd, 10);
    if (result != 0) {
        perror("listen");
        return 1;
    }
    //Demonize
    pid_t pid = fork();
    if (pid < 0){
        perror("fork");
        return 1;
    }
    if(pid == 0){
        setsid(); // create a new session if we can...
        result = master(fd);
        exit(result);
    }else{
        return 0;
    }
}

static int run_slave_cmd(struct sockaddr_un *addr, int fd, const std::vector<std::string> &args){
    int result = -1;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_MONOTONIC, &now);
    srand48(start.tv_sec ^ start.tv_nsec);
    while (diff_timespec(&now, &start) < 3.0) {
        result = connect(fd, (sockaddr *)addr, sizeof(*addr));
        if (result == 0)
            break;

        usleep((useconds_t)(lrand48() % 100000) + 100); //Random sleep min 100us max 100100us
        clock_gettime(CLOCK_MONOTONIC, &now);
    }
    if (result < 0 && errno == ECONNREFUSED) {
        fprintf(stderr, "Waited 3 seconds for master.  giving up.\n");
        close(fd);
        return 1;
    }
    if (result < 0) {
        fprintf(stderr, "connect %s: %s\n", addr->sun_path, strerror(errno));
        return 1;
    }
    return slave(fd, args);
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
    uid_t ruid = getuid();
    uid_t euid = geteuid();
    WithRoot::init(ruid, euid);
    if (setresuid(euid, euid, ruid) != 0) {
        perror("setresuid");
        abort();
    }
#ifdef __linux__
    setfsuid(ruid);
    raise_net_admin_ambient();
#endif
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }

    struct sockaddr_un addr;
    memset(&addr, 0x0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (!get_fifo_path_to_addr(&addr))
        exit(1);

    int fd = create_socket();
    if (fd < 0) {
        exit(1);
    }

    // plus one because we use the abstract namespace, it will show up in
    // /proc/net/unix prefixed with an @
    int result = bind(fd, (sockaddr *)&addr, sizeof(addr));

    if (result == 0) {
        //If exit is called and master is not running, just give a warning
        if (args.size() == 1 && args[0] == "exit") {
            rtapi_print_msg(RTAPI_MSG_ERR, "rtapi_app: exit received while not running\n");
            return 0;
        }
        //If ping is called and master is not running, return an error
        if (args.size() == 1 && args[0] == "ping") {
            return 1;
        }
        //If check_rt is called and master is not running, do not start master
        //execute and return
        //This is needed for the verify command in the realtime script
        //If master is started, this starts up the whole realtime chain
        //and halrun -U is needed to exit again
        if (args.size() == 1 && args[0] == "check_rt") {
            std::string out;
            int ret = do_check_rt_cmd(out);
            if (out.length() > 0) {
                printf("%s\n", out.c_str());
            }
            return ret;
        }
        //Start a master on start command
        if (args.size() == 1 && args[0] == "start") {
            result = start_master(fd);
            if (result != 0) {
                exit(result);
            }
            //Need to close and reopen the socket
            //It is already bound and master is using it
            close(fd);
            int fd = create_socket();
            if (fd < 0) {
                exit(1);
            }
            //Ping master to make shure it is running
            //before returning
            return run_slave_cmd(&addr, fd, {"ping"});
        }else{
            fprintf(stderr, "WARNING: Deprecated: No active realtime environment found. Use \"realtime start\" to start one.\n"
                "  A realtime environment is started automatically.\n"
                "  If this appears while using halcmd: Use halrun instead.\n"
                "  halcmd should only be used with an already running realtime environment.\n"
                "  halrun creates a realtime environment and tears it down at exit.\n");
            result = start_master(fd);
            if (result != 0) {
                exit(result);
            }
            //Need to close and reopen the socket
            //It is already bound and master is using it
            close(fd);
            int fd = create_socket();
            if (fd < 0) {
                exit(1);
            }
            //Run command
            //This makes also shure it is running before returning
            return run_slave_cmd(&addr, fd, args);
        }
    } else if (errno == EADDRINUSE) {
        return run_slave_cmd(&addr, fd, args);
    } else {
        perror("bind");
        exit(1);
    }
}

static inline void write_string(int fd, const char *str) {
    (void)!write(fd, str, strlen(str));
}

static void signal_handler(int sig, siginfo_t * /*si*/, void * /*uctx*/) {
    //Read: https://www.man7.org/linux/man-pages/man7/signal-safety.7.html
    bool doAbort = true;
    switch (sig) {
    case SIGXCPU:
        write_string(STDERR_FILENO, "rtapi_app: SIGXCPU - aborting\n");
        break;
    case SIGSEGV:
        write_string(STDERR_FILENO, "rtapi_app: SIGSEGV - aborting\n");
        break;
    case SIGILL:
        write_string(STDERR_FILENO, "rtapi_app: SIGILL - aborting\n");
        break;
    case SIGFPE:
        write_string(STDERR_FILENO, "rtapi_app: SIGFPE - aborting\n");
        break;
    case SIGTERM:
        write_string(STDERR_FILENO, "rtapi_app: SIGTERM - shutting down\n");
        doAbort = false; //TERM is a user signal, no need for a coredump
        break;
    case SIGINT:
        write_string(STDERR_FILENO, "rtapi_app: SIGINT - shutting down\n");
        doAbort = false; //INT is a user signal, no need for a coredump
        break;
    default:
        write_string(STDERR_FILENO, "rtapi_app: UNKNOWN - aborting\n");
        break;
    }

    //Write remaining messages
    rtapi_msg_queue.consume_all([](const message_t &m) {
        write_string(STDERR_FILENO, m.msg);
    });

    if (doAbort) {
        //Call abort to generate a coredump if enabled
        //To enable coredumps for setuid applications:
        //echo 1 | sudo tee /proc/sys/fs/suid_dumpable #rtapi_app is setuid
        //In general:
        //ulimit -c unlimited or coredumpctl
        abort();
    }
    _exit(128 + sig); //128+n: Fatal error signal "n"
}

const static size_t PRE_ALLOC_SIZE = 1024 * 1024 * 32;
const static struct rlimit unlimited = {RLIM_INFINITY, RLIM_INFINITY};
static void configure_memory() {
    // Best-effort raise of the soft cap to the hard cap.  Needs
    // CAP_SYS_RESOURCE; absent that, CAP_IPC_LOCK lets mlockall succeed
    // regardless of the rlimit.  Only warn when neither path is open,
    // i.e. mlockall is actually going to fail.
    bool have_ipc_lock = false;
#ifdef __linux__
    cap_t caps = cap_get_proc();
    if (caps) {
        cap_flag_value_t v;
        if (cap_get_flag(caps, CAP_IPC_LOCK, CAP_EFFECTIVE, &v) == 0)
            have_ipc_lock = (v == CAP_SET);
        cap_free(caps);
    }
#endif
    struct rlimit limit;
    if (getrlimit(RLIMIT_MEMLOCK, &limit) == 0) {
        limit.rlim_cur = limit.rlim_max;
        if (setrlimit(RLIMIT_MEMLOCK, &limit) < 0 && !have_ipc_lock)
            rtapi_print_msg(RTAPI_MSG_WARN,
                "setrlimit(RLIMIT_MEMLOCK) failed and CAP_IPC_LOCK not held: %s. "
                "mlockall will likely fail.\n", strerror(errno));
    }

    int res = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (res < 0)
        rtapi_print_msg(RTAPI_MSG_WARN,
            "mlockall failed: %s. Realtime latency may suffer.\n",
            strerror(errno));

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

static void harden_rt() {
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
    if (iopl(3) < 0) {
        rtapi_print_msg(
            RTAPI_MSG_ERR,
            "iopl() failed: %s\n"
            "cannot gain I/O privileges - "
            "missing CAP_SYS_RAWIO or using secure boot? - "
            "parallel port access is not allowed\n",
            strerror(errno)
        );
    }
#endif

    struct sigaction sig_act = {};
#ifdef __linux__
    // Best-effort raise of RTPRIO/CORE soft caps.  Setting these to
    // RLIM_INFINITY requires CAP_SYS_RESOURCE, which neither setuid root
    // nor file capabilities grant by default.  Without it, threads still
    // get SCHED_FIFO via CAP_SYS_NICE; the rlimit just gates how high
    // they can go.  Don't fail harden_rt() when it can't be raised.
    if (setrlimit(RLIMIT_RTPRIO, &unlimited) < 0)
        rtapi_print_msg(RTAPI_MSG_DBG,
            "setrlimit(RLIMIT_RTPRIO): %s\n", strerror(errno));

    if (setrlimit(RLIMIT_CORE, &unlimited) < 0)
        rtapi_print_msg(
            RTAPI_MSG_WARN, "setrlimit: %s - core dumps may be truncated or non-existent\n", strerror(errno)
        );

    // even when running with elevated capabilities
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

    sigaction(SIGXCPU, &sig_act, (struct sigaction *)NULL);
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
        ssize_t r = write(fd, "\0\0\0\0", 4);
        if (r != 4) {
            rtapi_print_msg(RTAPI_MSG_WARN, "failed to write to /dev/cpu_dma_latency: %s\n", strerror(errno));
        }
        // deliberately leak fd until program exit
    }
#endif /* __linux__ */
}

static RtapiApp *makeDllApp(const std::string &dllName, int policy) {
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

#ifdef __linux__
// Raise CAP_NET_ADMIN into the ambient set so it survives execve() into
// child processes (iptables, ip6tables) launched by HAL drivers like
// hm2_eth.  Linux file capabilities on rtapi_app give cap_net_admin in
// the permitted+effective sets but not inheritable/ambient, so without
// this iptables runs cap-less and fails with EPERM.  No-op when the cap
// is not held (e.g. running unprivileged).
static void raise_net_admin_ambient(void) {
    cap_t caps = cap_get_proc();
    if (!caps) return;

    cap_value_t cap = CAP_NET_ADMIN;
    cap_flag_value_t v;
    if (cap_get_flag(caps, cap, CAP_PERMITTED, &v) == 0 && v == CAP_SET) {
        if (cap_set_flag(caps, CAP_INHERITABLE, 1, &cap, CAP_SET) == 0
                && cap_set_proc(caps) == 0) {
            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE,
                      CAP_NET_ADMIN, 0, 0) != 0
                    && geteuid() != 0) {
                rtapi_print_msg(RTAPI_MSG_WARN,
                    "rtapi_app: PR_CAP_AMBIENT_RAISE(CAP_NET_ADMIN) "
                    "failed: %s; iptables-using drivers may not work "
                    "under file caps.\n", strerror(errno));
            }
        }
    }
    cap_free(caps);
}
#endif

static RtapiApp *makeApp() {
    RtapiApp *app;
    rtapi_realtime_type_t rt_type = rtapi_get_realtime_type();
    if (rt_type == REALTIME_TYPE_NONE) {
        app = makeDllApp("liblinuxcnc-uspace-posix.so.0", SCHED_OTHER);
    } else {
        WithRoot r;
        harden_rt();
        if (rt_type == REALTIME_TYPE_XENOMAI_EVL) {
            app = makeDllApp("liblinuxcnc-uspace-xenomai-evl.so.0", SCHED_FIFO);
        } else if (rt_type == REALTIME_TYPE_XENOMAI) {
            app = makeDllApp("liblinuxcnc-uspace-xenomai.so.0", SCHED_FIFO);
        } else if (rt_type == REALTIME_TYPE_LXRT) {
            app = makeDllApp("liblinuxcnc-uspace-rtai.so.0", SCHED_FIFO);
        } else if (rt_type == REALTIME_TYPE_PREEMPT_RT || rt_type == REALTIME_TYPE_PREEMPT_DYNAMIC || rt_type == REALTIME_TYPE_UNKNOWN) {
            // SCHED_FIFO available but no Xenomai/RTAI backend.  Warn if the
            // kernel is not PREEMPT_RT: SCHED_FIFO still beats SCHED_OTHER,
            // but latency on a PREEMPT_DYNAMIC stock kernel can be tens of
            // milliseconds, which will surprise users who expect the same
            // bounds as a PREEMPT_RT or Xenomai setup.
            if (rt_type == REALTIME_TYPE_PREEMPT_DYNAMIC || rt_type == REALTIME_TYPE_UNKNOWN) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "Note: SCHED_FIFO available but kernel is not PREEMPT_RT.  "
                    "Latency may be unbounded; install a PREEMPT_RT kernel "
                    "for hard realtime guarantees.\n");
            }
            app = makeDllApp("liblinuxcnc-uspace-posix.so.0", SCHED_FIFO);
        } else {
            app = nullptr;
            rtapi_print_msg(RTAPI_MSG_ERR, "Bug: rt_type = %i in not handled\n", rt_type);
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

void rtapi_task_self_resync(void) {
    App().task_self_resync();
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
