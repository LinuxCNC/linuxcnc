/********************************************************************
* Description: halrmt.cc
*   Simple telnet interface to LinuxCNC HAL commands (halcmd)
*
*   Derived from work by jmkasunich
*
*   Other contributors:
*     Alex Joni
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006-2008 All rights reserved.
* Copyright (c) 2026  B.Stultiens
*
* Last change:
* 2026 - Mostly rewritten
********************************************************************/

//******************************************************************
//  Using halrmt: see man halrmt(1)
//******************************************************************

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <poll.h>
#include <pthread.h>
#include <fnmatch.h>
#include <getopt.h>
#include <locale.h>

#include <string>
#include <vector>
#include <set>
#include <fmt/format.h>

#include <rtapi.h>
#include <rtapi_string.h>
#include <hal.h>
#include <inifile.hh>

#include "setps_util.h"

using namespace linuxcnc;

#define NELEM(x)    (sizeof(x) / sizeof(*(x)))

// To prevent excessive hostnames only
#define MAX_HOSTNAME_SIZE 80

static std::string helloPwd   = "EMC";       // Connect password
static std::string enablePwd  = "EMCTOO";    // Enable password
static std::string serverName = "EMCNETSVR"; // Server name written in hello response
static int port = 5006;
static std::string inifilename;
static volatile int quitloop = 0;   //  Signal to main loop to exit
static int quiet = 0;

static unsigned maxSessions = 0;    // Maximum number of sessions to allow

typedef struct {
    int sock;                       // Client socket
    std::vector<std::string> toks;  // Tokenized command
    std::string inbuf;              // Input buffer, contains all data from read()
    std::string outbuf;             // Output buffer, contains all data to write()
    std::string hostname;           // Name of the client
    std::string version;            // Client's version
    std::string inifilename;        // Local ini filename for the connection
    bool halfclosed;
    bool linked;
    bool enabled;
    bool echo;      // Echo input
    bool verbose;   // Show GET/SET xxx ACK on every command
    bool timestamp;
    bool timefmt;
    bool expand;    // Enable INI-file expansion
    bool header;    // Show a header with PIN{VAL}S, PARAM{VAL}S, SIGNAL{VAL}S, FUNCTS, THREADS
} connectionRecType;

static std::vector<connectionRecType> clients; // Client connections


typedef enum { cmdUnknown, cmdHello, cmdSet, cmdGet, cmdQuit, cmdShutdown, cmdHelp } cmdType;

typedef enum {
    hcUnknown,
    hcEcho, hcVerbose, hcEnable, hcExpand, hcHeader,
    hcTime, hcTimestamp,
    hcIni, hcIniFile,
    hcSave,
    hcPin, hcPinVal, hcPins, hcPinVals,
    hcParam, hcParamVal, hcParams, hcParamVals,
    hcSig, hcSigVal, hcSigs, hcSigVals,
    hcComp, hcComps,
    hcFunct, hcFuncts,
    hcThread, hcThreads,
    hcLoadRt, hcUnload, hcLoadUsr,
    hcLinkPS, hcLinkSP, hcNet, hcUnlinkP,
    // hcLinkPP, -- Deprecated and now no longer implemented
    hcLock, hcUnlock,
    hcNewSig, hcDelSig,
    hcSetP, hcSetS,
    hcAddF, hcDelF,
    hcStart, hcStop
} subCmdType;

typedef enum {
    rtOk,
    rtError
} cmdResponseType;

typedef struct {
    cmdType command;
    unsigned nargs; // Minimum number of arguments
    const char *name;
} commandListType;

// Must be sorted on 'name'
static const commandListType commandList[] = {
    {cmdGet,      1, "GET"     },
    {cmdHello,    2, "HELLO"   },
    {cmdHelp,     0, "HELP"    },
    {cmdQuit,     0, "QUIT"    },
    {cmdSet,      1, "SET"     },
    {cmdShutdown, 0, "SHUTDOWN"},
};

enum accessLevelType {
    alNone,   // No restrictions
    alHello,  // Must have issued successful HELLO
    alEnable  // Must have issued successful SET ENABLE
};

typedef struct {
    subCmdType command;
    const char *name;
    unsigned nget; // Minimum getter arg count (token count = nget + 2)
    unsigned nset; // minimum setter arg count (token count = nget + 2)
    cmdResponseType (*getter)(connectionRecType &);
    cmdResponseType (*setter)(connectionRecType &);
    accessLevelType acclvl; // Access level
    const char *gethelp;    // Argument list for GET
    const char *sethelp;    // Argument list for SET
} getsetListType;

static void info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void info(const char *fmt, ...)
{
    if(quiet > 0)
        return;
    va_list va;
    va_start(va, fmt);
    fprintf(stdout, "halrmt: ");
    vfprintf(stdout, fmt, va);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(va);
}

static void error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "halrmt: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(va);
}

static void xperror(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void xperror(const char *fmt, ...)
{
    int en = errno;
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "halrmt: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, ": %s\n", strerror(en));
    fflush(stderr);
    va_end(va);
}

static void mysleep(double secs)
{
    struct timespec ts, rem;
    ts.tv_sec = (time_t)secs;
    ts.tv_nsec = 1000000000l * (secs - ts.tv_sec);
retry:
    int err = nanosleep(&ts, &rem);
    if(err < 0) {
        if(EINTR == errno) {
            ts = rem;
            goto retry;
        }
        xperror("nanosleep");
    }
}

static void flushall(void)
{
    // Try to write pending data to each socket
    for(auto &c : clients) {
        if(!c.outbuf.size())
            continue;

        ssize_t res = write(c.sock, c.outbuf.c_str(), c.outbuf.size());
        if (res < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                xperror("flushall: client write()");
                // Make this connection to close/abort
                c.halfclosed = true;
                c.inbuf.clear();
                c.outbuf.clear();
            }
        } else {
            c.outbuf.erase(0, res); // Remove written data from buffer
        }
    }
}

static void reply(connectionRecType &ctx, const std::string &s)
{
    if (ctx.timestamp) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        if (ctx.timefmt) {
            ctx.outbuf.append(fmt::format("{}.{:06d}: ", tv.tv_sec, tv.tv_usec));
        } else {
            char tstr[32]; // Enough to hold 'yyyy-mm-dd hh:mm:ss.000000'
            struct tm lt;
            localtime_r(&tv.tv_sec, &lt);
            strftime(tstr, sizeof(tstr), "%F %T", &lt);
            ctx.outbuf.append(fmt::format("{}.{:06d}: ", tstr, tv.tv_usec));
        }
    }
    ctx.outbuf.append(s);
}

static void replynl(connectionRecType &ctx, const std::string &s)
{
    reply(ctx, s + "\r\n");
}

static void errornl(connectionRecType &ctx, const std::string &s)
{
    reply(ctx, "error: " + s + "\r\n");
}

static void warnnl(connectionRecType &ctx, const std::string &s)
{
    reply(ctx, "warning: " + s + "\r\n");
}

static bool to_int(const std::string &s, int &i)
{
    try {
        size_t pos;
        i = std::stoi(s, &pos);
        return pos == s.size();
    } catch (std::exception const &) {
        return false;
    }
}


static int compareNoCase(const std::string &a, const std::string &b)
{
    for (auto i = a.begin(), j = b.begin(); i != a.end() && j != b.end(); ++i, ++j) {
        int d = std::toupper((unsigned char)*i) - std::toupper((unsigned char)*j);
        if (d)
            return d; // Trivial, difference in character sequence
    }
    // Until here, all characters are the same, but we may have broken out of
    // the compare loop due to a difference in string length and both strings
    // share the same prefix.
    // Non-trivial compare, sizes differ on same string prefix:
    // a=ABC
    // b=ABC\0D
    // We cannot use the difference between character ordinal because we have
    // to account for embedded NUL characters. Therefore, we must use the sizes
    // to determine rank. Using the difference of sizes also matches equal
    // length, i.e. when the strings are equal.
    return (int)((ssize_t)a.size() - (ssize_t)b.size());
}

// Support functions
static const char *data_type(hal_type_t type)
{
    switch (type) {
    case HAL_BOOL: return "bool ";
    case HAL_REAL: return "real ";
    case HAL_S32:  return "s32  ";
    case HAL_U32:  return "u32  ";
    case HAL_SINT: return "sint ";
    case HAL_UINT: return "uint ";
    case HAL_PORT: return "port ";
    default:       return "undef";  // Shouldn't happen...
    }
}

/* Switch function for pin direction for the print_*_list functions  */
static const char *pin_data_dir(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "IN ";
    case HAL_OUT: return "OUT";
    case HAL_IO:  return "I/O";
    default:      return "???";  // Shouldn't happen...
    }
}

/* Switch function for param direction for the print_*_list functions  */
static const char *param_data_dir(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_RO: return "RO";
    case HAL_RW: return "RW";
    default:     return "??";  // Shouldn't happen...
    }
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow1(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "<==";
    case HAL_OUT: return "==>";
    case HAL_IO:  return "<=>";
    default:      return "???"; // Shouldn't happen...
    }
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow2(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "==>";
    case HAL_OUT: return "<==";
    case HAL_IO:  return "<=>";
    default:      return "???"; // Shouldn't happen...
    }
}

static std::string data_value(hal_type_t type, hal_query_value_u val)
{
    switch (type) {
    case HAL_BOOL: return val.b ? "        TRUE" : "       FALSE";
    case HAL_REAL: return fmt::format("{:12.7g}", val.r);
    case HAL_PORT: // FIXME
    case HAL_S32:
    case HAL_SINT: return fmt::format("  {:10d}", val.s);
    case HAL_U32:
    case HAL_UINT: return fmt::format("    {:08X}", val.u);
    default:       return "   undef    ";
    }
}

static std::string data_value2(hal_type_t type, hal_query_value_u val)
{
    switch (type) {
    case HAL_BOOL: return val.b ? "TRUE" : "FALSE";
    case HAL_REAL: return fmt::format("{:.7g}", val.r);
    case HAL_PORT: // FIXME
    case HAL_S32:
    case HAL_SINT: return fmt::format("{}", val.s);
    case HAL_U32:
    case HAL_UINT: return fmt::format("{}", val.u);
    default:       return "unknown_type";
    }
}

static std::optional<std::string> getIniVar(connectionRecType &ctx, const std::string &var, const std::string &section)
{
    if(ctx.inifilename.empty()) {
        return std::nullopt;
    }

    IniFile inifile(ctx.inifilename);
    if(!inifile) {
        return std::nullopt;
    }

    return inifile.findString(var, section);
}

static inline bool isEnabled(const connectionRecType &ctx)
{
    return ctx.enabled;
}

static inline bool doDisable(connectionRecType &ctx)
{
    if (!isEnabled(ctx)) {
        return false;
    }
    ctx.enabled = false;
    info("Disabled '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    return true;
}

static inline bool doEnable(connectionRecType &ctx)
{
    if (isEnabled(ctx)) {
        return false;
    }
    ctx.enabled = true;
    info("Enabled '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    return true;
}

static inline bool doUnlink(connectionRecType &ctx)
{
    if (!ctx.linked) {
        return false;
    }
    ctx.linked = false;
    info("Unlinked '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    return true;
}

static inline bool doLink(connectionRecType &ctx)
{
    if (ctx.linked) {
        return false;
    }
    ctx.linked = true;
    info("Linked '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    return true;
}

typedef struct {
    int value;
    const char *keyword;
} keywordValueType;

// Command argument keyword enum
enum {
    kwOn, kwOff,
    kwAll, kwLoad, kwConfig, kwParams, kwRun, kwTune, kwNone,
};

static int checkKeywords(const std::string &s, const keywordValueType *kv, size_t n)
{
    // Going through the list linearly is O(n), but the lists are no longer
    // than 8 so binary search does not really make sense.
    for (size_t i = 0; i < n; i++) {
        if (!compareNoCase(kv[i].keyword, s))
            return kv[i].value;
    }
    return -1;
}

static int checkOnOff(const std::string &s)
{
    static const keywordValueType kv[] = {
        {kwOn,  "ON"   },
        {kwOff, "OFF"  },
        {kwOn,  "1"    }, // Be tolerant
        {kwOff, "0"    },
        {kwOn,  "TRUE" }, // Be very tolerant
        {kwOff, "FALSE"},
        {kwOn,  "YES"  }, // Be extremely tolerant
        {kwOff, "NO"   },
    };
    return checkKeywords(s, kv, NELEM(kv));
}

static int checkLock(const std::string &s)
{
    static const keywordValueType kv[] = {
        {kwAll,    "ALL"   },
        {kwLoad,   "LOAD"  },
        {kwConfig, "CONFIG"},
        {kwParams, "PARAMS"},
        {kwRun,    "RUN"   },
        {kwTune,   "TUNE"  },
        {kwNone,   "NONE"  },
    };
    return checkKeywords(s, kv, NELEM(kv));
}

static inline const char *onOff(bool b)
{
    return b ? "ON" : "OFF";
}

static std::string onOffString(const std::string &s, bool b)
{
    return fmt::format("{} {}", s, onOff(b));
}


// Signal handler cannot call exit.
// Any exit in the signal handler would leave shared memory segments behind.
static void quitSig(int sig)
{
    (void)sig;
    quitloop = 1;
}

static pid_t halrmt_systemv_nowait(connectionRecType &ctx, const std::vector<const char *> &argv)
{
    // Need program name and a NULL
    if(argv.size() < 2) {
        errornl(ctx, "halrmt_systemv_nowait: no program or arguments");
        return -1;
    }
    if(NULL != argv[argv.size()-1]) {
        errornl(ctx, "halrmt_systemv_nowait: argument list not NULL terminated");
        return -1;
    }

    // disconnect from the HAL shmem area before forking
    hal_lib_exit();

    int err;
    pid_t pid = fork();
    switch(pid) {
    case -1: // Error
        err = errno;
        hal_lib_init(); // Reconnect
        errornl(ctx, fmt::format("halrmt_systemv_nowait: Fork failed: errno={} ({})", err, strerror(err)));
        return -1;
    case 0: // Child
        // Restore signals to default before exec
        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP,  SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        execvp(argv[0], (char * const *)argv.data());
        fprintf(stderr, "halrmt: Child execvp(%s,...) failed: %s\n", argv[0], strerror(errno));
        _exit(1);
        break;
    default: // Parent
        hal_lib_init(); // Reconnect
        break;
    }
    return pid;
}

static int halrmt_systemv(connectionRecType &ctx, const std::vector<const char *> &argv)
{
    int status;

    pid_t pid = halrmt_systemv_nowait(ctx, argv);

    do {
retry_wait:
        int err = waitpid(pid, &status, 0);  // Wait for the child to finish
        if(err < 0) {
            if(EINTR == errno) {
                goto retry_wait;
            }
            errornl(ctx, fmt::format("halrmt_systemv: waitpid({}) failed, errno={} ({})", pid, errno, strerror(errno)));
            return -1;
        }
    } while(!WIFEXITED(status) && !WIFSIGNALED(status));

    int rv = WEXITSTATUS(status);
    if(0 != rv) {
        errornl(ctx, fmt::format("{}: exit value: {}", argv[0], rv));
        return -1;
    }
    return 0;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int unloadrt_cb(hal_query_t *q, void *arg)
{
    std::vector<std::string> *comps = reinterpret_cast<std::vector<std::string> *>(arg);
    if(HAL_COMP_TYPE_REALTIME == q->comp.type) {
        comps->push_back(q->name);
    }
    return 0;
}

static int unloadrt_comp(connectionRecType &ctx, const std::string &mod_name)
{
    std::vector<const char *> argv;

#if defined(RTAPI_USPACE)
    argv.push_back(EMC2_BIN_DIR "/rtapi_app");
    argv.push_back("unload");
#else
    argv.push_back(EMC2_BIN_DIR "/linuxcnc_module_helper");
    argv.push_back("remove");
#endif
    argv.push_back(mod_name.c_str());
    argv.push_back(NULL);

    return halrmt_systemv(ctx, argv);
}

//
// It turns out that it is not portable to reset the state of getopt, so that a
// different argv list can be parsed.
// https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=192834
//
// (though that thread ends with the bug being closed as fixed in lenny, it is
// not fixed or has regressed by debian jessie)
//
static void reset_getopt_state()
{
#ifdef __GNU_LIBRARY__
    optind = 0;
#else
    optind = 1;
#endif
#ifdef HAVE_OPTRESET
    optreset = 1;
#endif
}

static std::string guess_comp_name(const std::string &prog_name)
{
    std::string name;
    size_t pos = prog_name.find_last_of('/');
    if(std::string::npos != pos) {
        name = prog_name.substr(pos + 1);
    } else {
        name = prog_name;
    }
    pos = name.find_last_of('.');
    if(std::string::npos != pos) {
        name.erase(pos);
    }
    return name;
}

static int get_all_comp_names_cb(hal_query_t *q, void *arg)
{
    std::set<std::string> *comps = reinterpret_cast<std::set<std::string> *>(arg);
    comps->insert(q->name);
    return 0;
}

static std::set<std::string> get_all_comp_names() {
    std::set<std::string> result;
    hal_query_t q = {};
    hal_list_comp(&q, get_all_comp_names_cb, &result);
    return result;
}

static void warn_newly_loaded_comps(connectionRecType &ctx, std::set<std::string> &names, const std::string &newname)
{
    auto new_names = get_all_comp_names();
    for(const auto &name : new_names) {
        if(name == newname) continue;
        if(names.find(name) == names.end()) {
            errornl(ctx, fmt::format("While waiting for '{}', component '{}' loaded.\r\n"
                                    "Did you specify the correct name via 'loadusr -Wn'?",
                                    newname, name));
            flushall();
        }
    }
    std::swap(new_names, names);
}

//
// FIXME: The loadUsr code will stall the entire process. We need to do this
// in a coroutine style for other connections to be served.
//
static int doLoadUsr(connectionRecType &ctx, const std::vector<const char *> &args)
{
    int status;

    if(args.size() < 2) {
        errornl(ctx, fmt::format("LoadUsr: Not enough arguments"));
        return -EINVAL;
    }
    if(NULL != args[args.size() - 1]) {
        errornl(ctx, fmt::format("LoadUsr: Argument list not NULL terminated"));
        return -EINVAL;
    }

    if(hal_get_lock() & HAL_LOCK_LOAD) {
        errornl(ctx, "LoadUsr: HAL is locked, loading of programs is not permitted");
        return -EPERM;
    }

    int wait_flag = 0;
    int wait_comp_flag = 0;
    int ignore_flag = 0;
    std::string new_comp_name;

    /* check for options (-w, -i, and/or -r) */
    reset_getopt_state();
    int optc;
    int argc = args.size() - 1;
    while (-1 != (optc = getopt(argc, (char * const *)args.data(), "+wWin:"))) {
        switch(optc) {
        case 'w': wait_flag = 1; break;
        case 'W': wait_comp_flag = 1; break;
        case 'i': ignore_flag = 1; break;
        case 'n': new_comp_name = optarg; break;
        default:
            errornl(ctx, fmt::format("LoadUsr: Invalid option '-{}'", isprint(optc) ? optc : '?'));
            return -EINVAL;
        }
    }
    std::vector<const char *> argv;
    for(unsigned i = optind; i < args.size(); i++) {
        argv.push_back(args[i]);
    }
    // Double check argument list
    if(argv.size() < 2) {
        errornl(ctx, fmt::format("LoadUsr: Not enough arguments for halcmd_systemv"));
        return -EINVAL;
    }
    if(NULL != argv[argv.size() - 1]) {
        errornl(ctx, fmt::format("LoadUsr: Argument list not NULL terminated for halcmd_systemv"));
        return -EINVAL;
    }

    // Get component name
    if(new_comp_name.empty()) {
        new_comp_name = guess_comp_name(argv[0]);
    }

    // Component status quo
    std::set<std::string> comp_names_pre = get_all_comp_names();

    // Create the new user process
    pid_t pid = halrmt_systemv_nowait(ctx, argv);
    if(pid < 0) {
        int err = errno;
        errornl(ctx, fmt::format("LoadUsr: Failed to execute '{}'", argv[0]));
        return -err;
    }

    bool exited = false;
    int err;
    if(wait_comp_flag) {
        bool ready = false;
        int count = 0;
        int retval = 0;
        while(!ready && !exited) {
            mysleep(0.01);
            // Program done?
            errno = 0;
            retval = waitpid(pid, &status, WNOHANG);
            err = errno;
            if (retval != 0) {
                if(EINTR == err) {
                    continue;
                }
                exited = true;
                ready = false;
                errornl(ctx, fmt::format("LoadUsr: waitpid failed '{}' ('{}') errno={}", argv[0], new_comp_name, err));
                break;
            }
            if(WIFEXITED(status)) {
                errornl(ctx, fmt::format("LoadUsr: program '{}' terminated with code {}", argv[0], (int)WEXITSTATUS(status)));
                exited = true;
                ready = false;
                break;
            }
            /* check for program becoming ready */
            hal_query_t q = {};
            int rv = hal_comp_by_name(new_comp_name.c_str(), &q);
            if(!rv && q.comp.ready) {
                ready = true;
                break;
            }
            /* pacify the user */
            count++;
            if(count >= 300 && 0 == (count % 200)) {
                replynl(ctx, fmt::format("\nWaiting for component '{}' to become ready.", new_comp_name));
                warn_newly_loaded_comps(ctx, comp_names_pre, new_comp_name);
                flushall();
            } else if(count > 200 && count % 10 == 0) {
                reply(ctx, ".");
                warn_newly_loaded_comps(ctx, comp_names_pre, new_comp_name);
                flushall();
            }
        }
        if (count > 200) {
            // terminate the ... pacifier
            replynl(ctx, "");
            flushall();
        }
        // did it work?
        if (!ready) {
            if (retval < 0) {
                errornl(ctx, fmt::format("LoadUsr: waitpid({}) failed errno={} ({})", pid, err, strerror(err)));
            } else {
                errornl(ctx, fmt::format("LoadUsr: {} exited without becoming ready", argv[0]));
            }
            return -err;
        }
    }

    if (wait_flag) {
        while(!exited) {
            int rv = waitpid(pid, &status, 0);
            err = errno;
            if (rv < 0) {
                if(EINTR == err) {
                    continue;
                }
                errornl(ctx, fmt::format("LoadUsr: waitpid({}) failed, errno={} ({})", pid, err, strerror(err)));
                return -err;
            }
            if(WIFEXITED(status)) {
                // Done...
                break;
            }
            if(WIFSIGNALED(status)) {
                errornl(ctx, fmt::format("LoadUsr: program '{}' did exit on signal {}", argv[0], WTERMSIG(status)));
                return -ESRCH;
            }
        }
        if (!ignore_flag) {
            int rv = WEXITSTATUS(status);
            if (0 != rv) {
                errornl(ctx, fmt::format("LoadUsr: program '{}' failed, returned {}", argv[0], rv));
                return -ESRCH;
            }
        }
    }
    return 0;
}

static int doLoadRt(connectionRecType &ctx, const std::vector<std::string> &args)
{
    int retval;
    std::vector<const char *> argv;

#if defined(RTAPI_USPACE)
    argv.push_back("-W");
    argv.push_back("-n");
    argv.push_back(args[0].c_str());
    argv.push_back(EMC2_BIN_DIR "/rtapi_app");
    argv.push_back("load");
    for(unsigned i = 0; i < args.size(); i++) {
        argv.push_back(args[i].c_str());
    }
    argv.push_back(NULL);
    retval = doLoadUsr(ctx, argv);
#else
    if (hal_get_lock() & HAL_LOCK_LOAD) {
        errornl(ctx, "LoadRt: HAL is locked, loading of modules is not permitted");
        return -EPERM;
    }

    // Make full module name '<path>/<name>.o'
    std::string mod_path = fmt::format("{}/{}{}", EMC2_RTLIB_DIR, args[0], MODULE_EXT);

    // Does the module exist?
    struct stat stat_buf;
    if (stat(mod_path.c_str(), &stat_buf) != 0 ) {
        errornl(ctx, fmt::format("LoadRt: Can't find module '{}' in '{}'", args[0], EMC2_RTLIB_DIR));
        return -ENOENT;
    }

    argv.push_back(EMC2_BIN_DIR "/linuxcnc_module_helper");
    argv.push_back("insert");
    for(unsigned i = 0; i < args.size(); i++) {
        argv.push_back(args[i].c_str());
    }
    argv.push_back(NULL);

    retval = halrmt_systemv(ctx, argv);
#endif

    if (0 != retval) {
        errornl(ctx, fmt::format("LoadRt: insmod '{}' failed, returned {}", args[0], retval));
        return retval;
    }
    // Join the module arguments into a single string
    std::string insmod = args[0];
    for(unsigned i = 1; i < args.size(); i++) {
        insmod += ' ';
        insmod += args[i];
    }
    // Module argument string needs to go into hal memory
    char *cptr = (char *)hal_malloc(insmod.size() + 1);
    if (!cptr) {
        errornl(ctx, "LoadRt: Failed to allocate HAL memory");
        return -ENOMEM;
    }
    strcpy(cptr, insmod.c_str());
    int rv = hal_comp_insmod_args(args[0].c_str(), cptr);
    if(rv) {
        errornl(ctx, fmt::format("LoadRt: module '{}' not loaded", args[0]));
        return -EINVAL;
    }
    return 0;
}

static int getCompInfo_cb(hal_query_t *q, void *arg)
{
    connectionRecType *ctx = reinterpret_cast<connectionRecType *>(arg);
    const std::string *pattern = reinterpret_cast<const std::string *>(q->callerdata.cpval);
    if(pattern->empty() || !fnmatch(pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
        replynl(*ctx, fmt::format("COMP {:12s} {:3d} {}",
                q->name, q->comp.comp_id, q->comp.type == HAL_COMP_TYPE_REALTIME ? "RT  " : "User"));
    }
    return 0;
}

static int getCompInfo(connectionRecType &ctx, const std::string &pattern)
{
    hal_query_t q = {};
    q.callerdata.cpval = &pattern;
    if(ctx.header) {
        replynl(ctx, fmt::format("COMP Name         Id  Type"));
    }
    return hal_list_comp(&q, getCompInfo_cb, &ctx);
}

typedef struct {
    connectionRecType *ctx;
    const std::string *pattern;
    int count;
    bool valuesOnly;
} getXinfo_t;

static int getPinInfo_cb(hal_query_t *q, void *arg)
{
    getXinfo_t *gpi = reinterpret_cast<getXinfo_t *>(arg);
    if(gpi->pattern->empty() || !fnmatch(gpi->pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
        gpi->count++;
        if(gpi->valuesOnly) {
            replynl(*gpi->ctx, fmt::format("PINVAL {:30s} {}",
                    q->name, data_value2(q->pp.type, q->pp.value)));
        } else {
            replynl(*gpi->ctx, fmt::format("PIN {:30s} {:11s} {:12s}({:3d}) {} {}",
                    q->name, data_value2(q->pp.type, q->pp.value),
                    q->pp.comp, q->pp.comp_id, data_type(q->pp.type), pin_data_dir(q->pp.dir)));
        }
    }
    return 0;
}

static std::pair<int,int> getPinInfo(connectionRecType &ctx, const std::string &pattern, bool valuesOnly)
{
    getXinfo_t gpi = { &ctx, &pattern, 0, valuesOnly };
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    if(ctx.header) {
        if(valuesOnly)
            replynl(ctx, fmt::format("PINVAL Name                           Value"));
        else
            replynl(ctx, fmt::format("PIN Name                           Value       Component   ( id) Type  Dir"));
    }
    return {hal_list_p(&q, getPinInfo_cb, &gpi), gpi.count};
}

static int getSigInfo_cb(hal_query_t *q, void *arg)
{
    getXinfo_t *gpi = reinterpret_cast<getXinfo_t *>(arg);
    if(gpi->pattern->empty() || !fnmatch(gpi->pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
        gpi->count++;
        if(gpi->valuesOnly) {
            replynl(*gpi->ctx, fmt::format("SIGNALVAL {:30s} {}",
                    q->name, data_value2(q->sig.type, q->sig.value)));
        } else {
            replynl(*gpi->ctx, fmt::format("SIGNAL {:30s} {:11s} {}",
                    q->name, data_value2(q->sig.type, q->sig.value), data_type(q->sig.type)));
        }
    }
    return 0;
}

static std::pair<int,int> getSigInfo(connectionRecType &ctx, const std::string &pattern, bool valuesOnly)
{
    getXinfo_t gpi = { &ctx, &pattern, 0, valuesOnly };
    hal_query_t q = {};
    if(ctx.header) {
        if(valuesOnly)
            replynl(ctx, fmt::format("SIGNALVAL Name                           Value"));
        else
            replynl(ctx, fmt::format("SIGNAL Name                           Value       Type"));
    }
    return {hal_list_s(&q, getSigInfo_cb, &gpi), gpi.count};
}

static int getParamInfo_cb(hal_query_t *q, void *arg)
{
    getXinfo_t *gpi = reinterpret_cast<getXinfo_t *>(arg);
    if(gpi->pattern->empty() || !fnmatch(gpi->pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
        gpi->count++;
        if(gpi->valuesOnly) {
            replynl(*gpi->ctx, fmt::format("PARAMVAL {:30s} {}",
                    q->name, data_value2(q->pp.type, q->pp.value)));
        } else {
            replynl(*gpi->ctx, fmt::format("PARAM {:30s} {:11s} {:12s}({:3d}) {} {}",
                    q->name, data_value2(q->pp.type, q->pp.value),
                    q->pp.comp, q->pp.comp_id, data_type(q->pp.type), param_data_dir(q->pp.dir)));
        }
    }
    return 0;
}

static std::pair<int,int> getParamInfo(connectionRecType &ctx, const std::string &pattern, bool valuesOnly)
{
    getXinfo_t gpi = { &ctx, &pattern, 0, valuesOnly };
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;
    if(ctx.header) {
        if(valuesOnly)
            replynl(ctx, fmt::format("PARAMVAL Name                           Value"));
        else
            replynl(ctx, fmt::format("PARAM Name                           Value       Component   ( id) Type  Dir"));
    }
    return {hal_list_p(&q, getParamInfo_cb, &gpi), gpi.count};
}

static int getFunctInfo_cb(hal_query_t *q, void *arg)
{
    connectionRecType *ctx = reinterpret_cast<connectionRecType *>(arg);
    const std::string *pattern = reinterpret_cast<const std::string *>(q->callerdata.cpval);
    if(pattern->empty() || !fnmatch(pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
        replynl(*ctx, fmt::format("FUNCT {:16s} {:12s}({:3d}) {:3d}",
                        q->name, q->funct.comp, q->funct.comp_id, q->funct.users));
    }
    return 0;
}

static int getFunctInfo(connectionRecType &ctx, const std::string &pattern)
{
    hal_query_t q = {};
    q.callerdata.cpval = reinterpret_cast<const void *>(&pattern);
    if(ctx.header) {
        replynl(ctx, "FUNCT Name             Component   ( id)   Users");
    }
    return hal_list_funct(&q, getFunctInfo_cb, &ctx);
}

static rtapi_sint getpin_sint(connectionRecType &ctx, const std::string &name)
{
    hal_query_t q = {};
    q.name = name.c_str();
    int rv = hal_get_p(&q, NULL, NULL);
    if(0 != rv) {
        errornl(ctx, fmt::format("Cannot find thread's pin '{}', error={}", name, rv));
        return 0;
    }
    return q.pp.value.s;
}
static int getThreadInfo_cb(hal_query_t *q, void *arg)
{
    connectionRecType *ctx = reinterpret_cast<connectionRecType *>(arg);
    const std::string *pattern = reinterpret_cast<const std::string *>(q->callerdata.cpval);
    if(HAL_QTYPE_THREAD == q->qtype) {
        // The thread reference
        if(pattern->empty() || !fnmatch(pattern->c_str(), q->name, FNM_NOESCAPE|FNM_CASEFOLD)) {
            rtapi_sint tp = getpin_sint(*ctx, fmt::format("{}.time", q->name));
            rtapi_sint tm = getpin_sint(*ctx, fmt::format("{}.tmax", q->name));
            replynl(*ctx, fmt::format("THREAD {:12s} {:11d} {} {}", q->name, q->thread.period, tp, tm));
        }
    } else {
        // The thread's function reference
        replynl(*ctx, fmt::format("THREADFUNCT {:16s} {:2d}", q->thread.funct, q->thread.functidx + 1));
    }
    return 0;
}

static int getThreadInfo(connectionRecType &ctx, const std::string &pattern)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    q.callerdata.cpval = reinterpret_cast<const void *>(&pattern);
    if(ctx.header) {
        replynl(ctx, "THREAD Name             Period  time  tmax");
    }
    return hal_list_thread(&q, getThreadInfo_cb, &ctx);
}

static int save_comps_cb(hal_query_t *q, void *arg)
{
    std::string *str = reinterpret_cast<std::string *>(arg);
    if(strstr(q->name, HAL_PSEUDO_COMP_PREFIX) == q->name) {
        // Those starting with '__' are fake components
        return 0;
    }
    if(HAL_COMP_TYPE_REALTIME == q->comp.type) {
        // Realtime components only
        if(!q->comp.insmod) {
            *str += fmt::format("# loadrt {} (not loaded by loadrt, no args saved)\n", q->name);
        } else {
            *str += fmt::format("loadrt {} {}\n", q->name, q->comp.insmod);
        }
    }
    return 0;
}

static void save_comps(std::string &dst)
{
    dst += "# components\n";
    hal_query_t q = {};
    hal_list_comp(&q, save_comps_cb, &dst);
}

static int save_signals_cb(hal_query_t *q, void *arg)
{
    std::string *str = reinterpret_cast<std::string *>(arg);
    *str += fmt::format("newsig {} {}\n", q->name, data_type(q->sig.type));
    return 0;
}

static void save_signals(std::string &dst)
{
    dst += "# signals\n";
    hal_query_t q = {};
    hal_list_s(&q, save_signals_cb, &dst);
}

static int save_links_cb(hal_query_t *q, void *arg)
{
    if(q->pp.signal) {
        std::string *str = reinterpret_cast<std::string *>(arg);
        const char *arrow_str = q->callerdata.sival ? data_arrow1(q->pp.dir) : "";
        *str += fmt::format("linkps {} {} {}\n", q->name, arrow_str, q->pp.signal);
    }
    return 0;
}

static void save_links(std::string &dst, int arrow)
{
    dst += "# links\n";
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    q.callerdata.sival = arrow;
    hal_list_p(&q, save_links_cb, &dst);
}

static int save_nets_pins_cb(hal_query_t *q, void *arg)
{
    std::string *str = reinterpret_cast<std::string *>(arg);
    const char *arrow_str = q->callerdata.sival ? data_arrow2(q->pp.dir) : "";
    *str += fmt::format("linksp {} {} {}\n", q->pp.signal, arrow_str, q->name);
    return 0;
}

static int save_nets_cb(hal_query_t *q, void *arg)
{
    std::string *str = reinterpret_cast<std::string *>(arg);
    *str += fmt::format("newsig {} {}\n", q->name, data_type(q->sig.type));
    hal_query_t qp = {};
    qp.name = q->name;
    qp.callerdata.sival = q->callerdata.sival;
    return hal_list_p_s(&qp, save_nets_pins_cb, arg);
}
static void save_nets(std::string &dst, int arrow)
{
    dst += "# nets\n";
    hal_query_t q = {};
    q.callerdata.sival = arrow;
    hal_list_s(&q, save_nets_cb, &dst);
}

static int save_params_cb(hal_query_t *q, void *arg)
{
    std::string *str = reinterpret_cast<std::string *>(arg);
    *str += fmt::format("setp {} {}\n", q->name, data_value(q->pp.type, q->pp.value).c_str());
    return 0;
}

static void save_params(std::string &dst)
{
    dst += "# parameter values\n";
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;
    hal_list_p(&q, save_params_cb, &dst);
}

static int save_threads_cb(hal_query_t *q, void *arg)
{
    if(HAL_QTYPE_THREAD_FUNCT == q->qtype) {
        std::string *str = reinterpret_cast<std::string *>(arg);
        *str += fmt::format("addf {} {}\n", q->thread.funct, q->name);
    }
    return 0;
}

static void save_threads(std::string &dst)
{
    dst += "# realtime thread/function links\n";
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    hal_list_thread(&q, save_threads_cb, &dst);
}

// initiate session
static int commandHello(connectionRecType &ctx)
{
    // HELLO <password> <hostname> [<version>]

    if (ctx.toks[2].size() > MAX_HOSTNAME_SIZE) {
        errornl(ctx, fmt::format("Hostname too long. Max length allowed is {}", MAX_HOSTNAME_SIZE));
        replynl(ctx, "HELLO NAK");
        return -1;
    }

    // Repeating a HELLO means we disable and unlink.
    // Linking needs to be reestablished
    doDisable(ctx);
    doUnlink(ctx);

    // On password errors, don't say too much.
    if (ctx.toks[1] != helloPwd) {
        replynl(ctx, "HELLO NAK");
        return -1;
    }

    std::string version = "1.1";
    if (auto inistring = getIniVar(ctx, "VERSION", "EMC"))
        version = *inistring;

    ctx.hostname = ctx.toks[2];

    // ctx.version defaults to "1.1" when the context is created
    if (ctx.toks.size() > 3)
        ctx.version = ctx.toks[3];

    if (ctx.version > version) {
        warnnl(ctx, fmt::format("Requested version '{}' is newer than '{}' from [EMC]VERSION", ctx.version, version));
    }

    doLink(ctx);
    replynl(ctx, fmt::format("HELLO ACK {} {}", serverName, version));
    return 0;
}

int commandQuit(connectionRecType &ctx)
{
    doDisable(ctx);
    ctx.halfclosed = true;
    ctx.inbuf.clear(); // Zap any remaining input
    info("Closing connection '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    replynl(ctx, "QUIT ACK");
    return 0;
}

int commandShutdown(connectionRecType &ctx)
{
    if (isEnabled(ctx)) {
        // A shutdown will disconnect all clients from halrmt
        info("Shutdown initiated by '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
        doDisable(ctx);
        // Shutdown is not actually performed...
        // It cannot work because there is no infrastructure in place to do a
        // remote shutdown.
        replynl(ctx, "SHUTDOWN ACK");
        return 0;
    }

    errornl(ctx, "SHUTDOWN failed, not enabled");
    replynl(ctx, "SHUTDOWN NAK");
    return -1;
}

static cmdResponseType getEcho(connectionRecType &ctx)
{
    // GET ECHO
    replynl(ctx, onOffString("ECHO", ctx.echo));
    return rtOk;
}

static cmdResponseType getVerbose(connectionRecType &ctx)
{
    // GET VERBOSE
    replynl(ctx, onOffString("VERBOSE", ctx.verbose));
    return rtOk;
}

static cmdResponseType getHeader(connectionRecType &ctx)
{
    // GET HEADER
    replynl(ctx, onOffString("HEADER", ctx.header));
    return rtOk;
}

static cmdResponseType getEnable(connectionRecType &ctx)
{
    // GET ENABLE
    replynl(ctx, onOffString("ENABLE", isEnabled(ctx)));
    return rtOk;
}

static cmdResponseType getExpand(connectionRecType &ctx)
{
    // GET EXPAND
    replynl(ctx, onOffString("EXPAND", ctx.expand));
    return rtOk;
}

static cmdResponseType getComps(connectionRecType &ctx)
{
    // GET COMPS [prefix]
    int rv;
    if(ctx.toks.size() > 2)
        rv = getCompInfo(ctx, ctx.toks[2].c_str());
    else
        rv = getCompInfo(ctx, "");
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType getPins(connectionRecType &ctx)
{
    // GET PINS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getPinInfo(ctx, ctx.toks[2], false);
    } else {
        rv = getPinInfo(ctx, "", false);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getPinVals(connectionRecType &ctx)
{
    // GET PINVALS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getPinInfo(ctx, ctx.toks[2], true);
    } else {
        rv = getPinInfo(ctx, "", true);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getSigs(connectionRecType &ctx)
{
    // GET SIGS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getSigInfo(ctx, ctx.toks[2], false);
    } else {
        rv = getSigInfo(ctx, "", false);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getSigVals(connectionRecType &ctx)
{
    // GET SIGVALS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getSigInfo(ctx, ctx.toks[2], true);
    } else {
        rv = getSigInfo(ctx, "", true);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getParams(connectionRecType &ctx)
{
    // GET PARAMS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getParamInfo(ctx, ctx.toks[2], false);
    } else {
        rv = getParamInfo(ctx, "", false);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getParamVals(connectionRecType &ctx)
{
    // GET PARAMVALS [prefix]
    std::pair<int,int> rv;
    if(ctx.toks.size() > 2) {
        rv = getParamInfo(ctx, ctx.toks[2], true);
    } else {
        rv = getParamInfo(ctx, "", true);
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getFuncts(connectionRecType &ctx)
{
    // GET FUNCTS [prefix]
    int rv;
    if(ctx.toks.size() > 2) {
        rv = getFunctInfo(ctx, ctx.toks[2]);
    } else {
        rv = getFunctInfo(ctx, "");
    }
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType getIni(connectionRecType &ctx)
{
    // GET INI <var> <section>
    auto inistring = getIniVar(ctx, ctx.toks[2], ctx.toks[3]);
    if (inistring) {
        replynl(ctx, fmt::format("INI [{}]{}={}", ctx.toks[3], ctx.toks[2], *inistring));
        return rtOk;
    } else {
        errornl(ctx, fmt::format("INI file entry '[{}]{}' not found", ctx.toks[3], ctx.toks[2]));
        return rtError;
    }
}

static cmdResponseType getIniFile(connectionRecType &ctx)
{
    // GET INIFILE
    replynl(ctx, fmt::format("INIFILE {}", ctx.inifilename));
    return rtOk;
}

static cmdResponseType getThreads(connectionRecType &ctx)
{
    // GET THREADS [prefix]
    int rv;
    if(ctx.toks.size() > 2) {
        rv = getThreadInfo(ctx, ctx.toks[2]);
    } else {
        rv = getThreadInfo(ctx, "");
    }
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType getComp(connectionRecType &ctx)
{
    // GET COMP <name>
    return 0 == getCompInfo(ctx, ctx.toks[2]) ? rtOk : rtError;
}

static cmdResponseType getPin(connectionRecType &ctx)
{
    // GET PIN <pin>
    std::pair<int,int> rv = getPinInfo(ctx, ctx.toks[2], false);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Pin '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getPinVal(connectionRecType &ctx)
{
    // GET PINVAL <pin>
    std::pair<int,int> rv = getPinInfo(ctx, ctx.toks[2], true);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Pin '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getSig(connectionRecType &ctx)
{
    // GET SIGNAL <signal>
    std::pair<int,int> rv = getSigInfo(ctx, ctx.toks[2], false);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Signal '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getSigVal(connectionRecType &ctx)
{
    // GET SIGNALVAL <signal>
    std::pair<int,int> rv = getSigInfo(ctx, ctx.toks[2], true);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Signal '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getParam(connectionRecType &ctx)
{
    // GET PARAM <param>
    std::pair<int,int> rv = getParamInfo(ctx, ctx.toks[2], false);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Parameter '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getParamVal(connectionRecType &ctx)
{
    // GET PARAMVAL <param>
    std::pair<int,int> rv = getParamInfo(ctx, ctx.toks[2], true);
    if(0 == rv.first && 0 == rv.second) {
        errornl(ctx, fmt::format("Parameter '{}' not found", ctx.toks[2]));
        return rtError;
    }
    return 0 == rv.first ? rtOk : rtError;
}

static cmdResponseType getFunct(connectionRecType &ctx)
{
    // GET FUNCT <funct>
    return 0 == getFunctInfo(ctx, ctx.toks[2]) ? rtOk : rtError;
}

static cmdResponseType getThread(connectionRecType &ctx)
{
    // GET THREAD <thread>
    return 0 == getThreadInfo(ctx, ctx.toks[2]) ? rtOk : rtError;
}

static cmdResponseType getLock(connectionRecType &ctx)
{
    // GET LOCK
    unsigned char lck = hal_get_lock();
    if(HAL_LOCK_NONE == lck) {
        replynl(ctx, "LOCK none");
    } else if(HAL_LOCK_ALL == lck) {
        replynl(ctx, "LOCK all");
    } else {
#define LOCK_ANY (HAL_LOCK_LOAD | HAL_LOCK_CONFIG | HAL_LOCK_PARAMS | HAL_LOCK_RUN)
        std::string l;
        if(lck & HAL_LOCK_LOAD)   l += " load";
        if(lck & HAL_LOCK_CONFIG) l += " config";
        if(lck & HAL_LOCK_PARAMS) l += " params";
        if(lck & HAL_LOCK_RUN)    l += " run";
        if(0 != (lck & ~LOCK_ANY)) {
            l += fmt::format(" (unknown values: lock=0x{:02x})", lck);
        }
        replynl(ctx, fmt::format("LOCK{}", l));
#undef LOCK_ANY
    }
    return rtOk;
}

static int getNet_cb(hal_query_t *q, void *arg)
{
    connectionRecType *ctx = reinterpret_cast<connectionRecType *>(arg);
    replynl(*ctx, fmt::format("{} {} {}", q->pp.signal, data_arrow1(q->pp.dir), q->name));
    return 0;
}

static cmdResponseType getNet(connectionRecType &ctx)
{
    // GET NET <signal>
    hal_query_t q = {};
    q.name = ctx.toks[2].c_str();
    int rv = hal_list_p_s(&q, getNet_cb, &ctx);
    if(0 == rv) {
        return rtOk;
    } else if(-ENOENT == rv) {
        errornl(ctx, fmt::format("Signal '{}' not found", ctx.toks[2]));
        return rtError;
    } else {
        errornl(ctx, fmt::format("Signal '{}' resulted in error={} ({})", ctx.toks[2], rv, hal_strerror(rv)));
        return rtError;
    }
}

static cmdResponseType getTimestamp(connectionRecType &ctx)
{
    // GET TIMESTAMP
    replynl(ctx, fmt::format("TIMESTAMP {} {}", onOff(ctx.timestamp), onOff(ctx.timefmt)));
    return rtOk;
}

static cmdResponseType getTime(connectionRecType &ctx)
{
    // GET TIME
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = (double)tv.tv_sec + tv.tv_usec * 1e-6;
    replynl(ctx, fmt::format("TIME {}", t));
    return rtOk;
}

static cmdResponseType setEcho(connectionRecType &ctx)
{
    // SET ECHO <ON|OFF>
    switch (checkOnOff(ctx.toks[2])) {
    default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
    case kwOn: ctx.echo = true; break;
    case kwOff: ctx.echo = false; break;
    }
    return rtOk;
}

static cmdResponseType setVerbose(connectionRecType &ctx)
{
    // SET VERBOSE <ON|OFF>
    switch (checkOnOff(ctx.toks[2])) {
    default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
    case kwOn: ctx.verbose = true; break;
    case kwOff: ctx.verbose = false; break;
    }
    return rtOk;
}

static cmdResponseType setHeader(connectionRecType &ctx)
{
    // SET HEADER <ON|OFF>
    switch (checkOnOff(ctx.toks[2])) {
    default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
    case kwOn: ctx.header = true; break;
    case kwOff: ctx.header = false; break;
    }
    return rtOk;
}

static cmdResponseType setEnable(connectionRecType &ctx)
{
    // SET ENABLE <pwd|OFF>
    if (ctx.toks[2] == enablePwd) {
        doEnable(ctx);
        return rtOk;
    }

    if (!compareNoCase(ctx.toks[2], "OFF")) {
        if (!isEnabled(ctx)) {
            errornl(ctx, "Trying to set enable to off on a non-enabled link");
            return rtError;
        }
        doDisable(ctx);
        return rtOk;
    }
    return rtError;
}

static cmdResponseType setExpand(connectionRecType &ctx)
{
    // SET EXPAND <ON|OFF>
    switch (checkOnOff(ctx.toks[2])) {
    default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
    case kwOn: ctx.expand = true; break;
    case kwOff: ctx.expand = false; break;
    }
    return rtOk;
}

static cmdResponseType setLoadRt(connectionRecType &ctx)
{
    // SET LOADRT <name> [args...]
    if(REALTIME_TYPE_UNINITIALIZED == hal_get_realtime_type()) {
        errornl(ctx, "Realtime has not been initialized.");
        return rtError;
    }

    std::vector<std::string> args;

    for(unsigned i = 2; i < ctx.toks.size(); i++) {
        args.push_back(ctx.toks[i]);
    }
    int rv = doLoadRt(ctx, args);
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType setUnload(connectionRecType &ctx)
{
    // SET UNLOAD <comp>|all
    if(REALTIME_TYPE_UNINITIALIZED == hal_get_realtime_type()) {
        errornl(ctx, "Realtime has not been initialized.");
        return rtError;
    }

    std::vector<std::string> comps;

    if("all" == ctx.toks[2]) {
        // Unload all components
        // Gather all RT components
        hal_query_t q = {};
        int rv = hal_list_comp(&q, unloadrt_cb, &comps);
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to gather component list, error={} ({})",
                        ctx.toks[2], rv, hal_strerror(rv)));
            return rtError;
        }
    } else {
        // Unload one specific component
        hal_query_t q = {};
        int rv = hal_comp_by_name(ctx.toks[2].c_str(), &q);
        if(0 != rv) {
            errornl(ctx, fmt::format("Component '{}' is not loaded, error={} ({})",
                        ctx.toks[2], rv, hal_strerror(rv)));
            return rtError;
        }
        if(HAL_COMP_TYPE_REALTIME != q.comp.type) {
            errornl(ctx, fmt::format("Component '{}' is not a real-time component", ctx.toks[2]));
            return rtError;
        }
        comps.push_back(ctx.toks[2]);
    }

    // Now unload the lot
    for(const auto &comp : comps) {
        int rv = unloadrt_comp(ctx, comp);
        if(0 != rv) {
            errornl(ctx, fmt::format("Component '{}' failed to unload, error={} ({})",
                        ctx.toks[2], rv, hal_strerror(rv)));
            return rtError;
        }
    }
    return rtOk;
}

static cmdResponseType setLoadUsr(connectionRecType &ctx)
{
    // SET LOADUSR <arg> [arg [arg...]]
    std::vector<const char *> argv;
    for(unsigned i = 2; i < ctx.toks.size(); i++) {
        argv.push_back(ctx.toks[i].c_str());
    }
    argv.push_back(NULL);
    int rv = doLoadUsr(ctx, argv);
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType linkPS(connectionRecType &ctx, const std::string &pin, const std::string &sig)
{
    if(sig.empty()) {
        int rv = hal_unlink(pin.c_str());
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to unlink '{}'", pin));
            return rtError;
        }
    } else {
        int rv = hal_link(pin.c_str(), sig.c_str());
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to link pin '{}' to signal '{}'", pin, sig));
            return rtError;
        }
    }
    return rtOk;
}

static cmdResponseType setLinkPS(connectionRecType &ctx)
{
    // SET LINKPS <pin> [<signal>]
    if(ctx.toks.size() > 3)
        return linkPS(ctx, ctx.toks[2], ctx.toks[3]);
    else
        return linkPS(ctx, ctx.toks[2], "");
}

static cmdResponseType setLinkSP(connectionRecType &ctx)
{
    // SET LINKSP <signal> <pin>
    return linkPS(ctx, ctx.toks[3], ctx.toks[2]);
}

#if 0
// This has been deprecated in halcmd for quite some time.
// No need to continue it.
static cmdResponseType setLinkPP(connectionRecType &ctx)
{
    // SET LINKPP <pin1> <pin2>
    hal_query_t p1 = {};
    hal_query_t p2 = {};
    p1.qtype = HAL_QTYPE_PIN;
    p1.name  = ctx.toks[2].c_str();
    p2.qtype = HAL_QTYPE_PIN;
    p2.name  = ctx.toks[3].c_str();
    int pin1 = hal_getref_p(&p1);
    int pin2 = hal_getref_p(&p2);
    // Check if both pins exist
    if (0 != pin1 || 0 != pin2) {
        errornl(ctx, fmt::format("Pin '{}' not found", 0 != pin1 ? ctx.toks[2] : ctx.toks[3]));
        return rtError;
    }

    // Check that both pins have the same type
    if (p1.pp.type != p2.pp.type) {
        errornl(ctx, fmt::format("Pins '{}' and '{}' are not of same type ({} != {})",
                        ctx.toks[2], ctx.toks[3], (int)p1.pp.type, (int)p2.pp.type));
        return rtError;
    }

    // Create the signal
    int rv = hal_signal_new(ctx.toks[2].c_str(), p1.pp.type);

    if (0 == rv) {
        // Link pin 1 to signal
        rv = hal_link(ctx.toks[2].c_str(), ctx.toks[2].c_str());

        if (0 == rv) {
            // Link pin 2 to signal
            rv = hal_link(ctx.toks[2].c_str(), ctx.toks[2].c_str());
            if(0 != rv) {
                errornl(ctx, fmt::format("Failed to link pin '{}' to signal '{}'", ctx.toks[3], ctx.toks[2]));
                return rtError;
            }
        } else {
            errornl(ctx, fmt::format("Failed to link pin '{}' to signal '{}'", ctx.toks[2], ctx.toks[2]));
            return rtError;
        }
    } else {
        errornl(ctx, fmt::format("Failed to create signal '{}'", ctx.toks[2]));
        return rtError;
    }
    return rtOk;
}
#endif

static cmdResponseType setNet(connectionRecType &ctx)
{
    // SET NET <signal> <pin> [pin [..]]

    hal_query_t qs = {};
    qs.name = ctx.toks[2].c_str();
    int havesig = hal_getref_s(&qs);
    if(0 != havesig && -ENOENT != havesig) {
        errornl(ctx, fmt::format("Failed to retrieve signal info for '{}' error={} ({})",
                    ctx.toks[2], havesig, hal_strerror(havesig)));
        return rtError;
    }

    // The type of the signal (to be)
    hal_type_t ts = 0 == havesig ? qs.sig.type : HAL_TYPE_UNINITIALIZED;

    for(unsigned i = 3; i < ctx.toks.size(); i++) {
        hal_query_t qp = {};
        qp.qtype = HAL_QTYPE_PIN;
        qp.name = ctx.toks[i].c_str();
        int rv = hal_getref_p(&qp);
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to retrieve pin info for '{}', error={} ({})",
                        ctx.toks[i], rv, hal_strerror(rv)));
            return rtError;
        }
        if(HAL_TYPE_UNINITIALIZED == ts) {
            ts = qp.pp.type;
        } else if(ts != qp.pp.type) {
            errornl(ctx, fmt::format("Type mismatch {} != {} ({}!={})",
                        data_type(ts), data_type(qp.pp.type), (int)ts, (int)qp.pp.type));
            return rtError;
        }
        if(HAL_OUT == qp.pp.dir && (qs.sig.writers > 0 || qs.sig.bidirs > 0)) {
            if(qs.sig.writers > 0)
                errornl(ctx, fmt::format("Cannot add OUT pin '{}', signal '{}' already has a writer", qp.name, qs.name));
            else
                errornl(ctx, fmt::format("Cannot add OUT pin '{}', signal '{}' already has bidirs", qp.name, qs.name));
            return rtError;
        } else if(HAL_IO == qp.pp.dir) {
            qs.sig.bidirs++;
            if(qs.sig.writers > 0) {
                errornl(ctx, fmt::format("Cannot add IO pin '{}', signal '{}' already has a writer", qp.name, qs.name));
                return rtError;
            }
        }
    }

    // Create the signal if it does not yet exist
    if(-ENOENT == havesig) {
        hal_query_t qp = {};
        qp.name = ctx.toks[2].c_str();
        int rv = hal_getref_p(&qp);
        if(0 == rv) {
            errornl(ctx, fmt::format("Signal name '{}' is already in use as a {} name",
                            ctx.toks[2], HAL_QTYPE_PIN == qp.qtype ? "pin" : "param"));
            return rtError;
        }
        if(0 != (rv = hal_signal_new(ctx.toks[2].c_str(), ts))) {
            errornl(ctx, fmt::format("Failed to create signal '{}', error={}", ctx.toks[2], rv));
            return rtError;
        }
    }

    // Link up all the pins
    for(unsigned i = 3; i < ctx.toks.size(); i++) {
        int rv = hal_link(ctx.toks[i].c_str(), ctx.toks[2].c_str());
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to add pin '{}' to signal '{}', error={}", ctx.toks[i], ctx.toks[2], rv));
            return rtError;
        }
    }
    return rtOk;
}

static cmdResponseType setUnlinkP(connectionRecType &ctx)
{
    // SET UNLINK <pin>
    return linkPS(ctx, ctx.toks[2], "");
}

static cmdResponseType setLock(connectionRecType &ctx)
{
    // SET LOCK {all|load|config|params|run|tune|none}
    int lck;
    if((lck = checkLock(ctx.toks[2].c_str())) < 0) {
        errornl(ctx, fmt::format("Invalid lock argument '{}'", ctx.toks[2]));
        return rtError;
    }
    unsigned char old = hal_get_lock();
    int rv;
    switch(lck) {
    case kwAll:    rv = hal_set_lock(HAL_LOCK_ALL); break;
    case kwLoad:   rv = hal_set_lock(old | HAL_LOCK_LOAD); break;
    case kwConfig: rv = hal_set_lock(old | HAL_LOCK_CONFIG); break;
    case kwParams: rv = hal_set_lock(old | HAL_LOCK_PARAMS); break;
    case kwRun:    rv = hal_set_lock(old | HAL_LOCK_RUN); break;
    case kwTune:   rv = hal_set_lock(HAL_LOCK_TUNE); break;
    case kwNone:   rv = hal_set_lock(HAL_LOCK_NONE); break;
    default:
        errornl(ctx, fmt::format("internal: Invalid lock key '{}'", lck));
        return rtError;
    }
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType setUnlock(connectionRecType &ctx)
{
    // SET UNLOCK {all|load|config|params|run|tune|none}
    int lck;
    if((lck = checkLock(ctx.toks[2].c_str())) < 0) {
        errornl(ctx, fmt::format("Invalid lock argument '{}'", ctx.toks[2]));
        return rtError;
    }
    unsigned char old = hal_get_lock();
    int rv;
    switch(lck) {
    case kwAll:    rv = hal_set_lock(HAL_LOCK_NONE); break;
    case kwLoad:   rv = hal_set_lock(old & ~HAL_LOCK_LOAD); break;
    case kwConfig: rv = hal_set_lock(old & ~HAL_LOCK_CONFIG); break;
    case kwParams: rv = hal_set_lock(old & ~HAL_LOCK_PARAMS); break;
    case kwRun:    rv = hal_set_lock(old & ~HAL_LOCK_RUN); break;
    case kwTune:   rv = hal_set_lock(old & ~HAL_LOCK_TUNE); break;
    case kwNone:   rv = 0; break;
    default:
        errornl(ctx, fmt::format("internal: Invalid lock key '{}'", lck));
        return rtError;
    }
    return 0 == rv ? rtOk : rtError;
}

static hal_type_t haltype_from_str(const std::string &str)
{
    static const struct {
        const char *name;
        hal_type_t type;
    } types[] = {
        { "bit",   HAL_BOOL },
        { "bool",  HAL_BOOL },
        { "float", HAL_REAL },
        { "real",  HAL_REAL },
        { "s32",   HAL_S32  },
        { "s64",   HAL_SINT },
        { "sint",  HAL_SINT },
        { "u32",   HAL_U32  },
        { "u64",   HAL_UINT },
        { "uint",  HAL_UINT },
    };
    for(unsigned i = 0; i < NELEM(types); i++) {
        if(str == types[i].name)
            return types[i].type;
    }
    return HAL_TYPE_UNSPECIFIED;
}

static cmdResponseType setNewSig(connectionRecType &ctx)
{
    // SET NEWSIG <signal> <type>
    hal_type_t type = haltype_from_str(ctx.toks[3]);
    if(type <= 0) {
        errornl(ctx, fmt::format("Invalid type '{}'", ctx.toks[3]));
        return rtError;
    }
    int rv = hal_signal_new(ctx.toks[2].c_str(), type);
    if(0 != rv) {
        errornl(ctx, fmt::format("Cannot create signal '{}' with type '{}', error={}", ctx.toks[2], ctx.toks[3], rv));
        return rtError;
    }
    return rtOk;
}

static int setDelSig_cb(hal_query_t *q, void *arg)
{
    std::vector<std::string> *sigs = reinterpret_cast<std::vector<std::string> *>(arg);
    sigs->push_back(q->name);
    return 0;
}

static cmdResponseType setDelSig(connectionRecType &ctx)
{
    // SET DELSIG <signal>
    if(ctx.toks[2] == "all") {
        std::vector<std::string> sigs;
        hal_query_t q = {};
        int rv = hal_list_s(&q, setDelSig_cb, &sigs);
        if(0 != rv) {
            errornl(ctx, fmt::format("Failed to gather all signal names, error={}", rv));
            return rtError;
        }
        bool fail = false;
        for(const auto &sig : sigs) {
            rv = hal_signal_delete(sig.c_str());
            if(0 != rv) {
                errornl(ctx, fmt::format("Cannot delete signal '{}', error={}", sig, rv));
                fail = true;
            }
        }
        if(fail) {
            return rtError;
        }
    } else {
        int rv = hal_signal_delete(ctx.toks[2].c_str());
        if(0 != rv) {
            errornl(ctx, fmt::format("Cannot delete signal '{}', error={}", ctx.toks[2], rv));
            return rtError;
        }
    }
    return rtOk;
}

static cmdResponseType setSetP(connectionRecType &ctx)
{
    // SET SETP <pin|param> <value>
    hal_query_t query = {};
    query.name = ctx.toks[2].c_str();
    int rv = hal_set_p(&query, setps_common_cb, ctx.toks[3].data());
    if(0 != rv) {
        errornl(ctx, fmt::format("Cannot setp '{}' to '{}', error={}", ctx.toks[2], ctx.toks[3], rv));
        return rtError;
    }
    return rtOk;
}

static cmdResponseType setSetS(connectionRecType &ctx)
{
    // SET SETS <signal> <value>
    hal_query_t query = {};
    query.name = ctx.toks[2].c_str();
    int rv = hal_set_s(&query, setps_common_cb, ctx.toks[3].data());
    if(0 != rv) {
        errornl(ctx, fmt::format("Cannot sets '{}' to '{}', error={}", ctx.toks[2], ctx.toks[3], rv));
        return rtError;
    }
    return rtOk;
}

static cmdResponseType setAddf(connectionRecType &ctx)
{
    // SET ADDF <funct> <thread> [arg]
    int cl = -1;
    if (ctx.toks.size() > 4 && !to_int(ctx.toks[4], cl)) {
        errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[4]));
        return rtError;
    }
    if(cl < -1) {
        errornl(ctx, fmt::format("Invalid argument '{}', must be >= -1", ctx.toks[4]));
        return rtError;
    }
    int rv = hal_add_funct_to_thread(ctx.toks[2].c_str(), ctx.toks[3].c_str(), cl);
    if(0 != rv) {
        errornl(ctx, fmt::format("Failed to add fucntion '{}' to thread '{}', error={}", ctx.toks[2], ctx.toks[3], rv));
        return rtError;
    }
    return rtOk;
}

static cmdResponseType setDelF(connectionRecType &ctx)
{
    // SET DELF <name> <thread>
    int rv = hal_del_funct_from_thread(ctx.toks[2].c_str(), ctx.toks[3].c_str());
    if(0 != rv) {
        errornl(ctx, fmt::format("Failed to remove function '{}' from thread '{}'", ctx.toks[2], ctx.toks[3]));
        return rtError;
    }
    return rtOk;
}

static cmdResponseType setSave(connectionRecType &ctx)
{
    // SET SAVE {all,comp,sig,link,linka,net,neta,param,thread} [filename]
    static std::set<std::string> keys = {"all","comp","sig","link","linka","net","neta","param","thread"};

    if(keys.end() == keys.find(ctx.toks[2])) {
        errornl(ctx, fmt::format("Invalid save type '{}'", ctx.toks[2]));
        return rtError;
    }
    std::string out;
    if("all" == ctx.toks[2]) {
        save_comps(out);
        save_signals(out);
        save_links(out, 0);
        save_params(out);
        save_threads(out);
    } else if("comp" == ctx.toks[2]) {
        save_comps(out);
    } else if("sig" == ctx.toks[2]) {
        save_signals(out);
    } else if("link" == ctx.toks[2]) {
        save_links(out, 0);
    } else if("linka" == ctx.toks[2]) {
        save_links(out, 1);
    } else if("net" == ctx.toks[2]) {
        save_nets(out, 0);
    } else if("neta" == ctx.toks[2]) {
        save_nets(out, 1);
    } else if("param" == ctx.toks[2]) {
        save_params(out);
    } else if("thread" == ctx.toks[2]) {
        save_threads(out);
    } else {
        errornl(ctx, fmt::format("internal: failed to match key '{}'", ctx.toks[2]));
        return rtError;
    }
    if(ctx.toks.size() > 3) {
        // Filename was specified
        int fd = creat(ctx.toks[3].c_str(), 0644);
        if(!fd) {
            errornl(ctx, fmt::format("Cannot open '{}' for write ({})", ctx.toks[2], strerror(errno)));
            return rtError;
        }
        size_t done = 0;
        while(done < out.size()) {
            ssize_t err = write(fd, out.c_str() + done, out.size() - done);
            int e = errno;
            if(err < 0) {
                if(EINTR == e)
                    continue;
                close(fd);
                errornl(ctx, fmt::format("Write to '{}' failed written={} != size={} errno={} ({})",
                            ctx.toks[2], done, out.size(), e, strerror(e)));
                return rtError;
            }
            done += err;
        }
        close(fd);
    } else {
        replynl(ctx, out);
    }
    return rtOk;
}

static cmdResponseType setStart(connectionRecType &ctx)
{
    // SET START
    if(REALTIME_TYPE_UNINITIALIZED == hal_get_realtime_type()) {
        errornl(ctx, "Realtime has not been initialized.");
        return rtError;
    }

    int rv = hal_start_threads();
    if (rv != 0) {
        errornl(ctx, fmt::format("Failed to start realtime threads, error={}", rv));
    }
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType setStop(connectionRecType &ctx)
{
    // SET STOP
    if(REALTIME_TYPE_UNINITIALIZED == hal_get_realtime_type()) {
        errornl(ctx, "Realtime has not been initialized.");
        return rtError;
    }

    int rv = hal_stop_threads();
    if (rv != 0) {
        errornl(ctx, fmt::format("Unable to stop realtime threads, error={}", rv));
    }
    return 0 == rv ? rtOk : rtError;
}

static cmdResponseType setTimestamp(connectionRecType &ctx)
{
    // SET TIMESTAMP <ON|OFF> [ON|OFF]
    switch (checkOnOff(ctx.toks[2])) {
    default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
    case kwOn: ctx.timestamp = true; break;
    case kwOff: ctx.timestamp = false; break;
    }

    ctx.timefmt = ctx.toks.size() > 3 && kwOn == checkOnOff(ctx.toks[3]);
    return rtOk;
}

static cmdResponseType setIniFile(connectionRecType &ctx)
{
    // SET INIFILE <filename>
    IniFile ini(ctx.toks[2]); // Try to open and parse
    if(!ini) {
        errornl(ctx, fmt::format("Invalid INI-file '{}'", ctx.toks[2]));
        return rtError;
    }
    ctx.inifilename = ctx.toks[2];
    return rtOk;
}

static cmdResponseType gsNotImpl(connectionRecType &)
{
    return rtError;
}

static const getsetListType getsetList[] = {
    { hcAddF,      "ADDF",      0, 2, gsNotImpl,    setAddf,      alEnable, "", "<funct> <thread> [order]" },
    { hcComp,      "COMP",      1, 0, getComp,      gsNotImpl,    alEnable, "<comp>", "" },
    { hcComps,     "COMPS",     0, 0, getComps,     gsNotImpl,    alEnable, "[pattern]", "" },
    { hcDelF,      "DELF",      0, 2, gsNotImpl,    setDelF,      alEnable, "", "<funct> <thread>" },
    { hcDelSig,    "DELSIG",    0, 1, gsNotImpl,    setDelSig,    alEnable, "", "<signal>" },
    { hcEcho,      "ECHO",      0, 1, getEcho,      setEcho,      alNone,   "", "{on|off}" },
    { hcEnable,    "ENABLE",    0, 1, getEnable,    setEnable,    alHello,  "", "<password>|{off}" },
    { hcExpand,    "EXPAND",    0, 1, getExpand,    setExpand,    alHello,  "", "{on|off}" },
    { hcFunct,     "FUNCT",     1, 0, getFunct,     gsNotImpl,    alEnable, "<funct>", "" },
    { hcFuncts,    "FUNCTS",    0, 0, getFuncts,    gsNotImpl,    alEnable, "[pattern]", "" },
    { hcHeader,    "HEADER",    0, 1, getHeader,    setHeader,    alNone,   "", "{on|off}" },
    { hcIni,       "INI",       2, 0, getIni,       gsNotImpl,    alHello,  "<var> <section>", "" },
    { hcIniFile,   "INIFILE",   0, 1, getIniFile,   setIniFile,   alHello,  "", "<filename>" },
//  { hcLinkPP,    "LINKPP",    0, 2, gsNotImpl,    setLinkPP,    alEnable, "", "<pin1> <pin2>" },
    { hcLinkPS,    "LINKPS",    0, 1, gsNotImpl,    setLinkPS,    alEnable, "", "<pin> [<signal>]" },
    { hcLinkSP,    "LINKSP",    0, 2, gsNotImpl,    setLinkSP,    alEnable, "", "<signal> <pin>" },
    { hcLoadRt,    "LOADRT",    0, 1, gsNotImpl,    setLoadRt,    alEnable, "", "<module}" },
    { hcLoadUsr,   "LOADUSR",   0, 1, gsNotImpl,    setLoadUsr,   alEnable, "", "<program>" },
    { hcLock,      "LOCK",      0, 1, getLock,      setLock,      alEnable, "", "{all|load|config|params|run|tune|none}" },
    { hcNet,       "NET",       1, 2, getNet,       setNet,       alHello,  "<signal>", "<signal> <pin> [pin [...]]" },
    { hcNewSig,    "NEWSIG",    0, 2, gsNotImpl,    setNewSig,    alEnable, "", "<signal> <type>" },
    { hcParam,     "PARAM",     1, 0, getParam,     gsNotImpl,    alEnable, "<param>", "" },
    { hcParams,    "PARAMS",    0, 0, getParams,    gsNotImpl,    alEnable, "[pattern]", "" },
    { hcParamVal,  "PARAMVAL",  1, 0, getParamVal,  gsNotImpl,    alEnable, "<param>", "" },
    { hcParamVals, "PARAMVALS", 0, 0, getParamVals, gsNotImpl,    alEnable, "[pattern]", "" },
    { hcPin,       "PIN",       1, 0, getPin,       gsNotImpl,    alEnable, "<pin>", "" },
    { hcPins,      "PINS",      0, 0, getPins,      gsNotImpl,    alEnable, "[pattern]", "" },
    { hcPinVal,    "PINVAL",    1, 0, getPinVal,    gsNotImpl,    alEnable, "<pin>", "" },
    { hcPinVals,   "PINVALS",   0, 0, getPinVals,   gsNotImpl,    alEnable, "[pattern]", "" },
    { hcSave,      "SAVE",      0, 1, gsNotImpl,    setSave,      alEnable,
      "", "{all,comp,sig,link,linka,net,neta,param,thread} [filename]" },
    { hcSetP,      "SETP",      0, 2, gsNotImpl,    setSetP,      alEnable, "", "<pin|param> <value>" },
    { hcSetS,      "SETS",      0, 2, gsNotImpl,    setSetS,      alEnable, "", "<signal> <value>" },
    { hcSig,       "SIGNAL",    1, 0, getSig,       gsNotImpl,    alEnable, "<signal>", "" },
    { hcSigs,      "SIGNALS",   0, 0, getSigs,      gsNotImpl,    alEnable, "[pattern]", "" },
    { hcSigVal,    "SIGVAL",    1, 0, getSigVal,    gsNotImpl,    alEnable, "<signal>", "" },
    { hcSigVals,   "SIGVALS",   0, 0, getSigVals,   gsNotImpl,    alEnable, "[pattern]", "" },
    { hcStart,     "START",     0, 0, gsNotImpl,    setStart,     alEnable, "", "" },
    { hcStop,      "STOP",      0, 0, gsNotImpl,    setStop,      alEnable, "", "" },
    { hcThread,    "THREAD",    1, 0, getThread,    gsNotImpl,    alEnable, "<thread>", "" },
    { hcThreads,   "THREADS",   0, 0, getThreads,   gsNotImpl,    alEnable, "[pattern]", "" },
    { hcTime,      "TIME",      0, 0, getTime,      gsNotImpl,    alNone,   "", "" },
    { hcTimestamp, "TIMESTAMP", 0, 1, getTimestamp, setTimestamp, alNone,   "", "{on|off} [{on|off}]" },
    { hcUnlinkP,   "UNLINKP",   0, 1, gsNotImpl,    setUnlinkP,   alEnable, "", "<pin>" },
    { hcUnload,    "UNLOAD",    0, 1, gsNotImpl,    setUnload,    alEnable, "", "<module>|<program>" },
    { hcUnlock,    "UNLOCK",    0, 1, gsNotImpl,    setUnlock,    alEnable, "", "{all|load|config|params|run|tune|none}" },
    { hcVerbose,   "VERBOSE",   0, 1, getVerbose,   setVerbose,   alHello,  "", "{on|off}" },
};

static int cmpGSList(const void *a, const void *b)
{
    return compareNoCase(static_cast<const getsetListType *>(a)->name, static_cast<const getsetListType *>(b)->name);
}

int commandGetSet(connectionRecType &ctx, cmdType getset)
{
    const char *sg = getset == cmdSet ? "SET" : "GET";
    if (ctx.toks.size() < 2) {
        // Missing missing parameter
        errornl(ctx, fmt::format("Must specify subcommand for {}", sg));
        replynl(ctx, fmt::format("{} NAK", sg));
        return -1;
    }

    // Find the command in the list
    getsetListType v = {};
    v.name = ctx.toks[1].c_str();
    void *_tag = bsearch(&v, getsetList, NELEM(getsetList), sizeof(getsetList[0]), cmpGSList);
    const getsetListType *tag = static_cast<const getsetListType *>(_tag);
    if (!tag) {
        errornl(ctx, fmt::format("Subcommand not found: {} {}", sg, ctx.toks[1]));
        replynl(ctx, fmt::format("{} NAK", sg));
        return -1;
    }

    if (getset == cmdGet) {
        // GET X command
        // Check for enough arguments
        if (tag->nget + 2 > ctx.toks.size()) {
            errornl(ctx, fmt::format("Too few arguments to GET {}, have {}, need {} or more",
                    tag->name, ctx.toks.size() - 2, tag->nget));
            replynl(ctx, fmt::format("GET {} NAK", tag->name));
            return -1;
        }
        cmdResponseType res = tag->getter(ctx);
        switch (res) {
        case rtOk:
            if (ctx.verbose) {
                replynl(ctx, fmt::format("GET {} ACK", tag->name));
            }
            break;
        case rtError: // Standard error response
            replynl(ctx, fmt::format("GET {} NAK", tag->name));
            break;
        default: replynl(ctx, fmt::format("internal: GET {} returned unknown value '{}'", tag->name, (int)res)); break;
        }
    } else {
        // SET X command
        // Check for enough arguments
        if (tag->nset + 2 > ctx.toks.size()) {
            errornl(ctx, fmt::format("Too few arguments to SET {}, have {}, need {} or more",
                    tag->name, ctx.toks.size() - 2, tag->nset));
            replynl(ctx, fmt::format("GET {} NAK", tag->name));
            return -1;
        }

        if (tag->acclvl >= alHello && !ctx.linked) {
            errornl(ctx, fmt::format("A HELLO must be successfully negotiated for SET {}", tag->name));
            replynl(ctx, fmt::format("SET {} NAK", tag->name));
            return -1;
        }

        if (tag->acclvl >= alEnable && !isEnabled(ctx)) {
            errornl(ctx, fmt::format("Connection must be enabled for SET {}", tag->name));
            replynl(ctx, fmt::format("SET {} NAK", tag->name));
            return -1;
        }

        cmdResponseType res = tag->setter(ctx);
        switch (res) {
        case rtOk:
            if (ctx.verbose) {
                replynl(ctx, fmt::format("SET {} ACK", tag->name));
            }
            break;
        case rtError: // Standard error response
            replynl(ctx, fmt::format("SET {} NAK", tag->name));
            break;
        default:
            replynl(ctx, fmt::format("internal: SET {} returned unknown value '{}'", tag->name, (int)res));
            break;
        }
    }
    return 0;
}

static int helpGeneral(connectionRecType &ctx)
{
    replynl(ctx,
            "Available commands:\r\n"
            "  HELLO <password> <clientname> <protocolversion>\r\n"
            "  GET <command> [parameters...]\r\n"
            "  SET <command> [parameters...]\r\n"
            "  SHUTDOWN\r\n"
            "  HELP <command>\r\n");
    return 0;
}

static int helpHello(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: HELLO <password> <clientname> [<protover>]\r\n"
            "  Where:\r\n"
            "  - password:   the connection password to allow communications with the server.\r\n"
            "  - clientname: the name of the client trying to connect.\r\n"
            "  - protover:   the version of the protocol which the client wishes to use.\r\n"
            "  With valid password, server responds with:\r\n"
            "    HELLO ACK <servername> <protover>\r\n"
            "  Where:\r\n"
            "  - ACK:        acknowledging the connection has been made.\r\n"
            "  - servername: the name of the LinuxCNC Server to which the client has connected.\r\n"
            "  - protover:   the client requested version or latest version support by server if\r\n"
            "                the client requests a version later than that supported by the server.\r\n"
            "  With invalid password, the server responds with:\r\n"
            "    HELLO NAK\r\n");
    return 0;
}

static int helpGet(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: GET <command> [parameters...]\r\n"
            "  All get commands require that a HELLO has been successfully negotiated.\r\n"
            "  Command may be one of:");
    for (unsigned i = 0; i < NELEM(getsetList); i++) {
        const getsetListType *gsl = &getsetList[i];
        if (gsl->getter == gsNotImpl)
            continue;
        replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->gethelp));
    }
    reply(ctx, "\r\n");
    return 0;
}

static int helpSet(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: SET <command> [parameters...]\r\n"
            "  All set commands require that a HELLO has been successfully negotiated.\r\n"
            "  Most set commands require exclusive control over the connection to\r\n"
            "  LinuxCNC by having successfully executed 'SET ENABLE <password>'.\r\n"
            "  Set commands not requiring any access level control:");
    for (unsigned i = 0; i < NELEM(getsetList); i++) {
        const getsetListType *gsl = &getsetList[i];
        if (gsl->setter == gsNotImpl || gsl->acclvl != alNone)
            continue;
        replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
    }
    reply(ctx, "  Set commands requiring HELLO level control:");
    reply(ctx, "\r\n");
    for (unsigned i = 0; i < NELEM(getsetList); i++) {
        const getsetListType *gsl = &getsetList[i];
        if (gsl->setter == gsNotImpl || gsl->acclvl != alHello)
            continue;
        replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
    }
    reply(ctx, "\r\n");
    replynl(ctx, "  Set commands requiring control enabled:");
    for (unsigned i = 0; i < NELEM(getsetList); i++) {
        const getsetListType *gsl = &getsetList[i];
        if (gsl->setter == gsNotImpl || gsl->acclvl != alEnable)
            continue;
        replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
    }
    reply(ctx, "\r\n");
    return 0;
}

static int helpQuit(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: QUIT\r\n"
            "  The quit command disconnects the client from the server.\r\n"
            "  the command has no parameters and no requirements to have negotiated\r\n"
            "  a hello, or be in control.\r\n");
    return 0;
}

static int helpShutdown(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: SHUTDOWN\r\n"
            "  The shutdown command terminates the connections with all clients.\r\n"
            "  No shutdown of LinuxCNC is performed or attempted. It is not possible\r\n"
            "  for halrmt to perform any clean shudown procedure.\r\n"
            "  The command has no parameters, and can only be issued on the connection\r\n"
            "  that has enable control.\r\n");
    return 0;
}

static int helpHelp(connectionRecType &ctx)
{
    replynl(ctx,
            "Usage: HELP HELP\r\n"
            "  You need help if you need help on help to help you with help.\r\n"
            "  It may be time to look into another line of work where computers are\r\n"
            "  no requirement. Computers will only remind you of the necessary evil\r\n"
            "  they represent and cause severe helplessness for those requesting help.\r\n");
    return 0;
}

static int commandHelp(connectionRecType &ctx)
{
    if (ctx.toks.size() < 2)
        return helpGeneral(ctx);
    std::string s = ctx.toks[1];
    std::transform(s.begin(), s.end(), s.begin(), [](char c) { return std::toupper((unsigned char)c); });
    if (s == "HELLO")    return helpHello(ctx);
    if (s == "GET")      return helpGet(ctx);
    if (s == "SET")      return helpSet(ctx);
    if (s == "QUIT")     return helpQuit(ctx);
    if (s == "SHUTDOWN") return helpShutdown(ctx);
    if (s == "HELP")     return helpHelp(ctx);
    errornl(ctx, fmt::format("{} is not a valid command. ({})", ctx.toks[1], s));
    return 0;
}

static int cmpCmdList(const void *a, const void *b)
{
    return compareNoCase(static_cast<const commandListType *>(a)->name, static_cast<const commandListType *>(b)->name);
}

static inline bool isidchar(int ch)
{
    return (::isascii(ch & 0xff) && ::isalnum(ch & 0xff)) || '_' == ch;
}

static std::string resolveIni(connectionRecType &ctx, const std::string &section, const std::string &variable)
{
    if(ctx.inifilename.empty())
        return "";

    IniFile ini(ctx.inifilename);
    if(!ini)
        return "";

    if(auto str = ini.findString(variable, section))
        return *str;
    return "";
}

static void iniExpand(connectionRecType &ctx, std::string &line)
{
    if(!ctx.expand)
        return;
    std::string newline;
    std::string section;
    std::string variable;
    enum { ST_START, ST_BOPEN, ST_BCLOSE, ST_POPEN, ST_VAR } state = ST_START;
    for(const auto &ch : line) {
        switch(state) {
        default:
        case ST_START:
            if('[' == ch) {
                state = ST_BOPEN;
            } else {
                newline += ch;
            }
            break;
        case ST_BOPEN: // Seen '['
            if(']' == ch) {
                state = ST_BCLOSE;
            } else {
                if(!isidchar(ch)) {
                    // Invalid char in section name.
                    newline += '[' + section + ch;
                    section.clear();
                    state = ST_START;
                } else {
                    section += ch;
                }
            }
            break;
        case ST_BCLOSE: // Seen '[section]'
            if('(' == ch) {
                state = ST_POPEN;
            } else {
                if(!isidchar(ch)) {
                    // Invalid char starting variable
                    newline += '[' + section + ']' + ch;
                    variable.clear();
                    section.clear();
                    state = ST_START;
                } else {
                    state = ST_VAR;
                    variable += ch;
                }
            }
            break;
        case ST_VAR: // Seen '[section]x'
            if(!isidchar(ch)) {
                if(isspace(ch & 0xff)) {
                    // Have '[section]variable' --> resolve
                    newline += resolveIni(ctx, section, variable);
                } else {
                    // Invalid termination --> restore
                    newline += '[' + section + ']' + variable;
                }
                newline += ch;
                section.clear();
                variable.clear();
                state = ST_START;
            } else {
                variable += ch;
            }
            break;
        case ST_POPEN: // Seen '[section]('
            if(!isidchar(ch)) {
                if(')' == ch) {
                    // Have '[section](variable)' --> resolve
                    newline += resolveIni(ctx, section, variable);
                } else {
                    // Invalid termination --> restore
                    newline += '[' + section + "](" + variable + ch;
                }
                section.clear();
                variable.clear();
                state = ST_START;
            } else {
                variable += ch;
            }
            break;
        }
    }

    // We can terminate on end-of-string in any state
    // Repair any intermediate
    switch(state) {
    default:
    case ST_START:  break;
    case ST_BOPEN:  newline += '[' + section; break;
    case ST_BCLOSE: newline += '[' + section + ']'; break;
    case ST_POPEN:  newline += '[' + section + "](" + variable; break;
    case ST_VAR:    newline += resolveIni(ctx, section, variable); break;
    }
    line = newline;
}

static int parseCommand(connectionRecType &ctx, std::string &line)
{
    static const char toksep[] = " \t\n\r";

    size_t pos = 0;
    ctx.toks.clear(); // Clear any previous commands/tokens
    iniExpand(ctx, line);
    while (pos < line.size()) {
        // trim left
        size_t lft = line.find_first_not_of(toksep, pos);
        if (lft == std::string::npos) {
            // No non-whitespace characters found to delimit the start
            pos = line.size(); // All white-space until end
            continue;
        }
        // Now have:
        //   arbitrary  string with spaces
        //   ^
        //   lft
        size_t rgt = line.find_first_of(toksep, lft);
        if (rgt == std::string::npos) {
            // No whitespace characters found to delimit the end
            // The current range [lft,size()) has the token
            rgt = line.size();
        }
        // Now have:
        //   arbitrary  string with spaces
        //   ^        ^
        //   lft      rgt
        ctx.toks.push_back(line.substr(lft, rgt - lft));
        pos = rgt;
    }

    if (ctx.toks.size() < 1)
        return 0;

    // Find the command in the list
    commandListType v = {cmdUnknown, 0, ctx.toks[0].c_str()};
    void *_tag = bsearch(&v, commandList, NELEM(commandList), sizeof(commandList[0]), cmpCmdList);
    const commandListType *tag = static_cast<const commandListType *>(_tag);
    if (!tag) {
        errornl(ctx, fmt::format("Command '{}' not found", ctx.toks[0]));
        return 0;
    }

    if (ctx.toks.size() < tag->nargs + 1) {
        errornl(ctx, fmt::format("Too few arguments for '{}' command", ctx.toks[0]));
        return 0;
    }

    // Only echo when on, linked and never on HELLO commands
    if (ctx.echo && ctx.linked && tag->command != cmdHello)
        replynl(ctx, line);

    switch (tag->command) {
    case cmdHello: commandHello(ctx); break;
    case cmdSet:
    case cmdGet:   commandGetSet(ctx, tag->command); break;
    case cmdQuit:  commandQuit(ctx); break;
    case cmdHelp:  commandHelp(ctx); break;
    case cmdShutdown:
        if (!commandShutdown(ctx)) {
            return -1;
        }
        break;
    default:
        replynl(ctx, fmt::format("internal: Invalid command ID '{}' on name='{}'", (int)tag->command, tag->name));
        break;
    }
    return 0;
}

/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

static int initSocket()
{
    int optval = 1;
    int err;
    int sockfd;
    struct sockaddr_in6 address = {};

    sockfd = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
        xperror("socket()");
        return -1;
    }
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    // Using AF_INET6 will also allow IPv4 to connect (v4-mapped-on-v6)
    address.sin6_family = AF_INET6;
    address.sin6_addr = IN6ADDR_ANY_INIT;
    address.sin6_port = htons(port);
    err = bind(sockfd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    if (err) {
        close(sockfd);
        xperror("bind()");
        return -1;
    }

    err = listen(sockfd, 5);
    if (err) {
        close(sockfd);
        xperror("listen()");
        return -1;
    }

    return sockfd;
}

static void closeClient(connectionRecType &ctx)
{
    if (ctx.sock < 0)
        return;
    info("Connection terminated '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
    close(ctx.sock);
    ctx.sock = -1;
}

static int sockMain(int svrfd)
{
    std::vector<char> rdbuf(1600); // Declare at top, no need to realloc each round in the loop
    std::vector<struct pollfd> pfds(2);
    bool shutdown = false;

    while (!quitloop && (clients.size() || !shutdown)) {
        // Make sure we have room for clients and server socket
        pfds.resize(clients.size() + 1);

        // Setup poll data for all sockets
        pfds[0].fd = svrfd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        int pto = -1;    // Default to infinite poll timeout
        if (shutdown)
            pto = 0;    // Immediate return in shutdown mode

        for (size_t i = 0; i < clients.size(); i++) {
            // Close connection when about to shutdown and all is flushed. All
            // clients should already have halfclosed set and the input buffer
            // cleared.
            if (!clients[i].outbuf.size() && shutdown) {
                closeClient(clients[i]);
                // poll will ignore this entry
            }
            pfds[i + 1].fd = clients[i].sock;
            pfds[i + 1].events = POLLPRI | (!clients[i].halfclosed ? POLLIN : 0) | (clients[i].outbuf.size() ? POLLOUT : 0);
            pfds[i + 1].revents = 0;

            // If there is still a full line of data in an input buffer then
            // make poll return asap. It will still check the descriptors for
            // I/O activity.
            if (clients[i].inbuf.find_first_of("\r\n") != std::string::npos)
                pto = 0;
        }

        // Wait until something happens
        int err = poll(pfds.data(), pfds.size(), pto);
        if (err < 0) {
            if (EINTR == errno) {
                continue;
            } else {
                error("poll() returned errno=%d (%s), quiting", errno, strerror(errno));
                return EXIT_FAILURE;
            }
        }

        // Check server socket first
        if (pfds[0].revents & POLLERR) {
            // This should never happen...famous...last...words...
            error("Server socket received an error on poll, quiting");
            return EXIT_FAILURE;
        } else if (pfds[0].revents & POLLIN) {
            // POLLIN on a listen socket means new connection available
            int cfd;
            struct sockaddr_in6 csa;
            socklen_t csal = sizeof(csa);
            cfd = accept4(svrfd, reinterpret_cast<struct sockaddr *>(&csa), &csal, SOCK_CLOEXEC | SOCK_NONBLOCK);
            if (cfd < 0) {
                switch (errno) {
                // The usual errors that should make us retry:
                case EAGAIN:
                case ECONNABORTED:
                    break;
                // Network related errors should be treated as EAGAIN according
                // to accept(2) manual
                case ENETDOWN:
                case EPROTO:
                case ENOPROTOOPT:
                case EHOSTDOWN:
                case ENONET:
                case EHOSTUNREACH:
                case EOPNOTSUPP:
                case ENETUNREACH:
                    mysleep(1.0); // Don't busy loop waiting for the net to be up again
                    break;
                // Various restartable errors
                case ETIMEDOUT: // Linux kernel thingy, apparently
                case ERESTART:  // During trace (in accept(2) manual named ERESTARTSYS)
                    break;
                default:
                    // Any other error will be fatal
                    xperror("accept()");
                    return EXIT_FAILURE;
                }
            } else {
                if (!maxSessions || clients.size() < maxSessions) {
                    // The accept has set non-blocking and close-on-exec.
                    // Disable Nagle's algo
                    int nd = 1;
                    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
                    connectionRecType cr = {};
                    cr.sock = cfd;
                    cr.hostname = fmt::format("Default-{}", cfd);
                    cr.version = "1.1";
                    cr.echo = true;
                    cr.inifilename = inifilename; // Inherit global default
                    clients.push_back(cr);
                    char addr[INET6_ADDRSTRLEN] = {};
                    inet_ntop(AF_INET6, &csa.sin6_addr, addr, sizeof(addr));
                    info("New connection from %s:%d", addr, ntohs(csa.sin6_port));
                } else {
                    close(cfd);
                }
            }
        }

        // Note that we can have one extra client already added but it does not
        // have a pfds entry. That happens when we accept a new connection
        // above. However, we still need to check all other clients for
        // read/write.
        for (size_t i = 0; i < clients.size() && i < pfds.size() - 1; i++) {
            if (pfds[i + 1].revents & POLLERR) {
                // An error on a socket means we make it to close/abort
                clients[i].halfclosed = true;
                clients[i].inbuf.clear();
                clients[i].outbuf.clear();
            }

            if (pfds[i + 1].revents & POLLPRI) {
                ssize_t res = recv(clients[i].sock, rdbuf.data(), rdbuf.size(), MSG_OOB);
                if (res < 0) {
                    if (errno != EINTR && errno != EAGAIN) {
                        xperror("recv() OOB data");
                    }
                } else {
                    info("Received %zd bytes of OOB data (using telnet?), ignoring", res);
                }
            }

            if (pfds[i + 1].revents & POLLOUT) {
                // Writable...
                ssize_t res = write(clients[i].sock, clients[i].outbuf.c_str(), clients[i].outbuf.size());
                if (res < 0) {
                    if (errno != EINTR && errno != EAGAIN) {
                        xperror("client write()");
                        // Make this connection to close/abort
                        clients[i].halfclosed = true;
                        clients[i].inbuf.clear();
                        clients[i].outbuf.clear();
                    }
                } else {
                    clients[i].outbuf.erase(0, res); // Remove written data from buffer
                }
            }

            if (pfds[i + 1].revents & POLLIN) {
                // Readable...
                ssize_t len = read(clients[i].sock, rdbuf.data(), rdbuf.size());
                if (len < 0) {
                    if (errno != EAGAIN && errno != EINTR) {
                        xperror("client read()");
                        // Make this connection to close but still try flush output
                        clients[i].halfclosed = true;
                        clients[i].inbuf.clear();
                    }
                } else if (0 == len) {
                    // Socket closed
                    info("Read close '%s' (%d)", clients[i].hostname.c_str(), clients[i].sock);
                    clients[i].halfclosed = true;
                } else {
                    // Append the read data
                    clients[i].inbuf.append(rdbuf.data(), len);
                }
            }

            if (clients[i].halfclosed && !clients[i].inbuf.size() && !clients[i].outbuf.size()) {
                // The input is closed and no more data to handle or write.
                // Terminate the connection.
                closeClient(clients[i]);
            }
        }

        // Remove fully closed clients
        for (auto i = clients.begin(); i != clients.end();) {
            if (-1 == i->sock) {
                i = clients.erase(i);
            } else {
                ++i;
            }
        }

        // Handle input, one line at a time for each client (round robin)
        for (auto i = clients.begin(); i != clients.end(); ++i) {
            // Split into lines and send to parser
            size_t nl = i->inbuf.find_first_of("\r\n");
            if (nl != std::string::npos) {
                // Isolate the line and remove from input buffer
                std::string line = i->inbuf.substr(0, nl);
                // and remove any trailing \r\n too
                size_t enl = i->inbuf.find_first_not_of("\r\n", nl);
                i->inbuf.erase(0, enl);
                if (line.size()) {
                    // Parse the line if there is something on it
                    // It returns non-zero when the shutdown has been executed
                    if (parseCommand(*i, line)) {
                        // We may still have data in the outbuf that needs to
                        // be flushed. Any remaining output will still be sent
                        // and then the client will be fully closed and removed.
                        shutdown = true;
                        // Halfclose all clients and zap remaining input.
                        for (auto c = clients.begin(); c != clients.end(); ++c) {
                            c->halfclosed = true;
                            c->inbuf.clear();
                        }
                    }
                }
            }
            // Else: Not a finished line, must wait for rest
        }
    }

    return 0;
}

static void usage(void)
{
    static const char usage_str[] =
        "Usage: halrmt [options]\n"
        "Options:\n"
        "  -h,--help             This help\n"
        "  -p,--port <num>       Listen on port 'num' (default=%d)\n"
        "  -n,--name <str>       Set this server's name to 'str' (default=%s)\n"
        "  -w,--connectpw <pwd>  Set connect/hello password to 'pwd' (default=%s)\n"
        "  -e,--enablepw <pwd>   Set enable password to 'pwd' (default=%s)\n"
        "  -s,--sessions <num>   Restrict number of session to 'num' (default=%u) (0 for no limit)\n"
        "  -i,--ini <file>       Use 'file' as inifile (default=env[INI_FILE_NAME])\n"
        "  -q,--quiet            Don't print informational messages\n"
        "\n"
        "Session commands: Connect and use 'help', 'help get' and 'help set' for details.\n"
    ;
    printf(usage_str, port, serverName.c_str(), helloPwd.c_str(), enablePwd.c_str(), maxSessions);
}

static bool check_white(const std::string &s)
{
    for (const char &c : s) {
        if (std::iscntrl((unsigned char)c) || std::isspace((unsigned char)c))
            return true;
    }
    return false;
}

static struct option longopts[] = {
    {"help",      0, NULL, 'h'},
    {"name",      1, NULL, 'n'},
    {"port",      1, NULL, 'p'},
    {"connectpw", 1, NULL, 'w'},
    {"enablepw",  1, NULL, 'e'},
    {"sessions",  1, NULL, 's'},
    {"ini",       1, NULL, 'i'},
    {"quiet",     0, NULL, 'q'},
    {}
};

int main(int argc, char **argv)
{
    int optc;
    int lose = 0;
    char *eptr;

    /* set default level of output - 'quiet' */
    rtapi_set_msg_level(RTAPI_MSG_ERR);
    /* set default for other options */
    // process halrmt command line args
    while(-1 != (optc = getopt_long(argc, argv, "e:hi:n:p:qs:w:", longopts, NULL))) {
        switch(optc) {
        case 'h':
            usage();
            return EXIT_FAILURE;

        case 'e':
            enablePwd = optarg;
            if (!compareNoCase(enablePwd, "OFF")) {
                error("Invalid enable password. You cannot use 'OFF' as enable password");
                lose++;
            } else {
                if (check_white(enablePwd)) {
                    error("You cannot have control or space characters in the enable password");
                    lose++;
                }
            }
            break;
        case 'i': {
            inifilename = optarg;
            IniFile ini(inifilename); // Parse file to see it alright (and cache it)
            if(!ini)
                lose++;
            break; }
        case 'q':
            quiet++;
            break;
        case 'n':
            serverName = optarg;
            if (check_white(serverName)) {
                error("You cannot have control or space characters in the server name");
                lose++;
            }
            break;
        case 'w':
            helloPwd = optarg;
            if (check_white(helloPwd)) {
                error("You cannot have control or space characters in the password");
                lose++;
            }
            break;
        case 's':
            maxSessions = strtoul(optarg, &eptr, 0);
            if (eptr == optarg || *eptr) {
                error("Invalid maxSessions '%s', must be integer in range [0,...)", optarg);
                lose++;
            }
            break;
        case 'p':
            port = strtol(optarg, &eptr, 0);
            if (eptr == optarg || *eptr || port <= 0 || port >= 65535) {
                error("Invalid port '%s', must be integer in range [1,65534]", optarg);
                lose++;
            }
            break;

        default:
            lose++;
            break;
        }
    }

    if(inifilename.empty()) {
        const char *cptr = getenv("INI_FILE_NAME");
        if(!cptr) {
            info("No inifile available");
        } else {
            inifilename = cptr;
            IniFile ini(inifilename);
            if(!ini)
                lose++;
        }
    }

    if(lose)
        return EXIT_FAILURE;

    // Initialize the listen socket
    int sockfd = initSocket();
    if (sockfd < 0) {
        return EXIT_FAILURE;
    }

    // Get us access to HAL
    int rv = hal_lib_init();
    if(0 != rv) {
        close(sockfd);
        return EXIT_FAILURE;
    }

    // Make sure we quit when requested
    signal(SIGINT,  quitSig);
    signal(SIGQUIT, quitSig);
    signal(SIGTERM, quitSig);
    signal(SIGHUP,  SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    rv = sockMain(sockfd);

    // Disconnect from HAL shared memory
    hal_lib_exit();

    return rv;
}
// vim: ts=4 sw=4 et
