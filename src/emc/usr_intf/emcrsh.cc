/********************************************************************
* Description: emcrsh.cc  (source for linuxcncrsh)
*   Extended telnet based LinuxCNC interface
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2006-2008,2026 All rights reserved.
*
* Last change:
* Feb-2026  B.Stultiens  As good as completely rewritten
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <fmt/format.h>

#include "shcom.hh"
#include "emcglb.h"
#include "inifile.hh"
#include "timer.hh"

/*
 * Using linuxcncrsh: see man page linuxcncrsh.1
 */

#define NELEM(x) (sizeof(x) / sizeof(*(x)))

// To prevent excessive hostnames only
#define MAX_HOSTNAME_SIZE 80

// These are for JOINT_WAIT_HOMED
#define JOINT_WAIT_HOMED_SLEEP          0.1 // Sleeps time between checks
#define JOINT_WAIT_HOMED_TIMEOUT       10.0 // Default timeout waiting for homing
#define JOINT_WAIT_HOMED_TIMEOUT_WARN  30.0 // Timeout level to warn about stall
#define JOINT_WAIT_HOMED_TIMEOUT_MAX  300.0 // Max timeout level to accept

// Maximum wait time in WAIT_HEARTBEAT to see real-time move along
#define WAIT_HEARTBEAT_TIMEOUT 1.0

// Maximum number of heartbeats to wait for EMC_TASK_STATE to change
#define WAIT_TASK_STATE_CHANGE_MAX 25

typedef enum { cmdUnknown, cmdHello, cmdSet, cmdGet, cmdQuit, cmdShutdown, cmdHelp } cmdType;

typedef enum {
	scUnknown,
	// These you can always run
	scEcho,
	scTimestamp,
	// From here you need a successful HELLO to run the setter
	scEnable,
	scDisable,
	scVerbose,
	scTime,
	scClients,
	scPlat,
	scCommProt, // Deprecated
	// From here you need to be enabled to run the setter
	scIniFile,
	scIni,
	scDebug,
	scUpdate, // Deprecated
	scUpdateStatus, // Deprecated
	scWaitMode,
	scWait,
	scWaitHeartbeat,
	scTimeout,
	scError,
	scOperatorDisplay,
	scOperatorText,
	scEStop,
	// From here the machine must be in the ON state
	scMachine,
	scMode,
	scMist,
	scFlood,
	scSpindle,
	scSpindleOverride,
	scBrake,
	scTool,
	scToolOffset,
	scLoadToolTable,
	scJog,
	scJogIncr,
	scJogStop,
	scFeedOverride,
	scAbsCmdPos,
	scAbsActPos,
	scRelCmdPos,
	scRelActPos,
	scPosOffset,
	scJointPos,
	scJointHome,
	scJointUnhome,
	scJointWaitHomed,
	scJointLimit,
	scJointFault,
	scJointHomed,
	scJointType,
	scJointUnits,
	scJointVelocity,
	scAxisVelocity,
	scMDI,
	scTaskPlanInit,
	scOpen,
	scRun,
	scPause,
	scResume,
	scStep,
	scAbort,
	scProgram,
	scProgramLine,
	scProgramStatus,
	scProgramCodes,
	scProgramUnits,
	scProgramLinearUnits,
	scProgramAngularUnits,
	scUserLinearUnits,
	scUserAngularUnits,
	scDisplayLinearUnits,
	scDisplayAngularUnits,
	scLinearUnitConversion,
	scAngularUnitConversion,
	scProbe,
	scProbeClear,
	scProbeTripped,
	scProbeValue,
	scTeleopEnable,
	scKinematicsType,
	scOverrideLimits,
	scOptionalStop,
} subCmdType;

typedef enum { rtOk = 0, rtError } cmdResponseType;


typedef struct {
	int sock;						// Client socket
	std::vector<std::string> toks;	// Tokenized command
	std::string third;				// Not tokenized rest after second token (to prevent double space deletions in filenames)
	std::string inbuf;				// Input buffer, contains all data from read()
	std::string outbuf;				// Output buffer, contains all data to write()
	std::string hostname;			// Name of the client
	std::string version;			// Client's version
	std::string progname;			// Filename of (g-code) program
	EMC_WAIT_TYPE waitmode;			// Local to the connection to allow others to intervene
	double cmdtimeout;				// Command timeout (infinite when <= 0.0)
	double usrtimeout;				// Timeout as set by SET TIMEOUT
	int serial;						// The serial number of the last command sent
	bool halfclosed;				// Set when the socket input/receive is closed
	bool linked;					// Received HELLO and valid password
	bool enabled;					// The connection can perform all SET commands
	bool echo;						// Echo received commands
	bool verbose;					// When set, send ACK after successful GET/SET commands
	bool timestamp;					// Prefix answers with timestamp
	bool timefmt;					// Print human readable date/time instead of seconds since epoch
	int commMode;					// Deprecated
	int commProt;					// Deprecated
} connectionRecType;

static int port = 5007;
static std::string helloPwd = "EMC";
static std::string enablePWD = "EMCTOO";
static std::string serverName = "EMCNETSVR";
static unsigned maxSessions = 0;
static int quiet = 0;	// Set to suppress 'informational' messages to stdout
static std::vector<connectionRecType> clients;
static std::vector<std::string> searchPath;
static std::string activeSetter;	// Set when a remote command is being processed

#define POLLTIMEOUT_MAX 100	// 100ms max interval for command done update checks
static int polltimeout = 0;	// Set to a progressive value while a setter is active

static inline bool setterActive() { return activeSetter.size() > 0; }

static void setterClear(connectionRecType &ctx)
{
	activeSetter.clear();
	ctx.cmdtimeout = 0.0;
	ctx.serial = 0;
	polltimeout = 0;
}

static void setterSet(connectionRecType &ctx, const char *tag)
{
	activeSetter = tag;
	// An 'infinite' timeout is about 10 years ;-)
	ctx.cmdtimeout = etime() + (ctx.usrtimeout > 0.0 ? ctx.usrtimeout : (double)(86400*365*10));
	ctx.serial = emcCommandSerialNumber;
	polltimeout = 1;
}

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
	alNone,	  // No restrictions
	alHello,  // Must have issued successful HELLO
	alEnable, // Must have issued successful SET ENABLE
	alMachOn  // Must be in state ESTOP OFF and MACHINE ON
};

typedef struct {
	subCmdType command;
	const char *name;
	unsigned nget; // Minimum getter arg count (token count = nget + 2)
	unsigned nset; // minimum setter arg count (token count = nget + 2)
	cmdResponseType (*getter)(connectionRecType &);
	cmdResponseType (*setter)(connectionRecType &);
	accessLevelType acclvl; // Access level
	bool remote;			// The setter sends a command via NML
	const char *gethelp;	// Argument list for GET
	const char *sethelp;	// Argument list for SET
} getsetListType;

static struct option longopts[] = {
	{"help",      0, NULL, 'h'},
	{"port",      1, NULL, 'p'},
	{"name",      1, NULL, 'n'},
	{"sessions",  1, NULL, 's'},
	{"connectpw", 1, NULL, 'w'},
	{"enablepw",  1, NULL, 'e'},
	{"path",      1, NULL, 'd'},
	{"quiet",     0, NULL, 'q'},
	{}
};

static void info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void info(const char *fmt, ...)
{
	if(quiet)
		return;
	va_list va;
	va_start(va, fmt);
	fprintf(stdout, "linuxcncrsh: ");
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
	fprintf(stderr, "linuxcncrsh: ");
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
	fprintf(stderr, "linuxcncrsh: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, ": %s\n", strerror(en));
	fflush(stderr);
	va_end(va);
}

static void cleanup()
{
	// Wait until current message has been received
	if (emcStatusBuffer) {
		// Can't use printf and friends in signal handlers.
		// See signal-safety(7).
		static const char errmsg[] = "linuxcncrsh: cleanup(): emcCommandWaitReceived() timed out\n";
		emcTimeout = 10.0; // Or it may block forever
		if (emcCommandWaitReceived()) {
			ssize_t dummy __attribute__((unused));
			// Note: 'sizeof(errmsg)-1' to skip the trailing '\0'
			dummy = write(2, errmsg, sizeof(errmsg) - 1);
		}
	}

	// FIXME: The delete of the buffers calls the destructors and they may not
	// be signal-safe.
	// Fortunately, we call _exit() from the signal, so we will not be seeing
	// too much of messed up main application data and-what-not.
	//
	// The more correct way would be to set a flag in the signal handler, break
	// out of the main loop and then clean up and exit. But that might not be
	// entirely fool proof either because we may have been waiting for some NML
	// communication or machine action to finish.

	// clean up NML buffers
	if (emcErrorBuffer) {
		delete emcErrorBuffer;
		emcErrorBuffer = NULL;
	}

	if (emcStatusBuffer) {
		delete emcStatusBuffer;
		emcStatusBuffer = NULL;
		emcStatus = NULL;
	}

	if (emcCommandBuffer) {
		delete emcCommandBuffer;
		emcCommandBuffer = NULL;
	}

	// Valgrind report: deleting the NML buffer stuff leaves three allocated
	// memory segments as leaked. Not sure if we should bother.
}

static void set_nonblock(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	if (fl < 0) {
		xperror("fcntl(F_GETFL)");
		return;
	}
	fl |= O_NONBLOCK;
	fl = fcntl(fd, F_SETFL, fl);
	if (fl < 0) {
		xperror("fcntl(F_SETFL)");
	}
}

static int initSocket()
{
	int optval = 1;
	int err;
	int sockfd;
	struct sockaddr_in6 address = {};

	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
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

	set_nonblock(sockfd);

	return sockfd;
}

static void sigQuit(int /*sig*/)
{
	// FIXME: Terminating here may cause unintended side effects for in-progress stuff
	cleanup();
	_exit(0);
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

static bool to_double(const std::string &s, double &d)
{
	try {
		size_t pos;
		d = std::stod(s, &pos);
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

static std::optional<std::string> getIniVar(const std::string &var, const std::string &section)
{
	IniFile inifile;
	if (!inifile.Open(emc_inifile))
		return nullptr;

	auto inistr = inifile.Find(var.c_str(), section.c_str());
	inifile.Close();
	return inistr;
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

static inline const char *onOff(bool b)
{
	return b ? "ON" : "OFF";
}

static std::string onOffString(const std::string &s, bool b)
{
	return fmt::format("{} {}", s, onOff(b));
}

static inline const char *yesNo(bool b)
{
	return b ? "YES" : "NO";
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
	if (auto inistring = getIniVar("VERSION", "EMC"))
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

typedef struct {
	int value;
	const char *keyword;
} keywordValueType;

// Command argument keyword enum
enum {
	kwOn, kwOff,
	kwReceived, kwDone, kwNone,
	/*kwNone,*/ kwAuto, kwManual,
	/*kwAuto,*/ kwMDI,
	kwForward, kwReverse, kwIncrease, kwDecrease, kwConstant, /*kwOff,*/
	kwInch, kwmm, kwcm, /*kwAuto,*/ kwCustom,
	kwDeg, kwRad, kwGrad, /*kwAuto,*/ /*kwCustom,*/
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
		{kwOn,  "1"	   }, // Be tolerant
		{kwOff, "0"	   },
		{kwOn,  "TRUE" }, // Be very tolerant
		{kwOff, "FALSE"},
		{kwOn,  "YES"  }, // Be extremely tolerant
		{kwOff, "NO"   },
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkReceivedDoneNone(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwReceived, "RECEIVED"},
		{kwDone,	 "DONE"	   },
		{kwNone,	 "NONE"	   },
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkNoneAuto(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwNone, "NONE"},
		{kwAuto, "AUTO"},
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkManualAutoMDI(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwManual, "MANUAL"},
		{kwAuto,   "AUTO"  },
		{kwMDI,    "MDI"   },
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkSpindleCmd(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwForward,  "FORWARD" },
		{kwReverse,  "REVERSE" },
		{kwIncrease, "INCREASE"},
		{kwDecrease, "DECREASE"},
		{kwConstant, "CONSTANT"},
		{kwOff,      "OFF"     },
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkConversionStr(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwInch,   "INCH"  },
		{kwmm,     "MM"	   },
		{kwcm,     "CM"	   },
		{kwAuto,   "AUTO"  },
		{kwCustom, "CUSTOM"},
	};
	return checkKeywords(s, kv, NELEM(kv));
}

static int checkAngularConversionStr(const std::string &s)
{
	static const keywordValueType kv[] = {
		{kwDeg,    "DEG"   },
		{kwRad,    "RAD"   },
		{kwGrad,   "GRAD"  },
		{kwAuto,   "AUTO"  },
		{kwCustom, "CUSTOM"},
	};
	return checkKeywords(s, kv, NELEM(kv));
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

static cmdResponseType setDisable(connectionRecType &ctx)
{
	// SET DISABLE [sockfd]
	int cl = ctx.sock;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], cl)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}

	connectionRecType *conn = nullptr;
	for (auto &i : clients) {
		if (cl == i.sock) {
			conn = &i;
			break;
		}
	}
	if (!conn) {
		errornl(ctx, fmt::format("Client '{}' not available", cl));
		return rtError;
	}

	if (!isEnabled(*conn)) {
		errornl(ctx, fmt::format("Client '{}' is not enabled", conn->sock));
		return rtError;
	}

	if (ctx.sock == conn->sock) {
		replynl(ctx, "DISABLE SELF");
	} else {
		replynl(ctx, fmt::format("DISABLE {} {}", conn->sock, conn->hostname));
	}
	doDisable(*conn);
	return rtOk;
}

static cmdResponseType setEnable(connectionRecType &ctx)
{
	// SET ENABLE <pwd|OFF>
	if (ctx.toks[2] == enablePWD) {
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

static cmdResponseType setDebug(connectionRecType &ctx)
{
	// SET DEBUG <level>
	int level;
	if (!to_int(ctx.toks[2], level)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (sendDebug(level))
		return rtError;
	return rtOk;
}

static cmdResponseType setWaitMode(connectionRecType &ctx)
{
	// SET WAIT_MODE <RECEIVED|DONE>
	switch (checkReceivedDoneNone(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{received,done}}", ctx.toks[2])); return rtError;
	case kwReceived: ctx.waitmode = EMC_WAIT_RECEIVED; break;
	case kwDone: ctx.waitmode = EMC_WAIT_DONE; break;
	case kwNone:
		errornl(ctx, "Setting 'SET WAIT_MODE' to 'NONE' has been removed as it may cause commands to be lost.");
		return rtError;
	}
	return rtOk;
}

//
// Wait for a number of real-time thread heartbeats.
// The heartbeat is incremented every time the motion-controller runs in
// servo-thread.
static cmdResponseType doWaitHeartbeat(connectionRecType &ctx, int periods, double timeout)
{
	if (timeout <= 0.0) {
		replynl(ctx, fmt::format("internal: doWaitHeartbeat(): timeout={} and should have been > 0.0", timeout));
		return rtError;
	}

	if (periods < 1) {
		replynl(ctx, fmt::format("internal: doWaitHeartbeat(): periods={} and should have been > 0", timeout));
		return rtError;
	}

	static double sleeptime = -1.0;
	if (sleeptime < 0.0) {
		// No need to read the INI file on every invocation. The SERVO_PERIOD
		// does never change in a running system.
		std::string sp = "10000000"; // 10ms expressed in ns
		if (auto spp = getIniVar("SERVO_PERIOD", "EMCMOT")) {
			sp = *spp;
		}
		sleeptime = 10e6; // 10ms in ns
		if (!to_double(sp, sleeptime)) {
			// This would mean an invalid [EMCMOT]SERVO_PERIOD value...
			sleeptime = 10e6; // 10ms in ns
		} else if (sleeptime <= 0.0) {
			replynl(
				ctx,
				fmt::format("internal: [EMCMOT]SERVO_PERIOD was read as '{}' (calculated={}) and should be > 0", sp, sleeptime));
			sleeptime = -1.0;
			return rtError;
		}
		sleeptime /= 1e9; // Convert to seconds
	}
	if (sleeptime * periods > timeout) {
		errornl(ctx,
				fmt::format("Invalid periods '{}', waiting time of {:.3f} seconds would exceed timeout of {:.3f} seconds",
							periods,
							sleeptime * periods,
							timeout));
		return rtError;
	}

	updateStatus(); // Find out where the heartbeat is right now
	// This only works with 64-bit heartbeat type
	static_assert(std::is_same_v<decltype(emcStatus->motion.heartbeat), uint64_t>);
	uint64_t oldhb = emcStatus->motion.heartbeat;
	while (timeout > 0.0) {
		esleep(sleeptime);
		updateStatus(); // See if the heartbeat changed
		if (emcStatus->motion.heartbeat - oldhb >= (unsigned)periods)
			return rtOk;
		timeout -= sleeptime;
	}
	return rtError;
}

static cmdResponseType setJointWaitHomed(connectionRecType &ctx)
{
	// SET JOINT_WAIT_HOMED [joint] [timeout]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (joint >= 0 && !emcStatus->motion.joint[joint].homing && !emcStatus->motion.joint[joint].homed) {
		errornl(ctx, fmt::format("Joint '{}' is not homed and not in the process of homing.", joint));
		return rtError;
	}

	double timeout = JOINT_WAIT_HOMED_TIMEOUT;
	if (ctx.toks.size() > 3 && !to_double(ctx.toks[3], timeout)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[3]));
		return rtError;
	}
	if (timeout < 0.0 || timeout > JOINT_WAIT_HOMED_TIMEOUT_MAX) {
		errornl(ctx, fmt::format("Invalid timeout '{}', valid range [0.0,{:.1f}]", ctx.toks[3], JOINT_WAIT_HOMED_TIMEOUT_MAX));
		return rtError;
	}
	if (timeout > JOINT_WAIT_HOMED_TIMEOUT_WARN) {
		warnnl(ctx, "Timeout above 30 seconds. The remote shell and all other remote connections may stall for this period.");
	}

	double st = etime();
	while (timeout > 0.0) {
		updateStatus(); // Get fresh values about joints' status
		if (-1 == joint) {
			bool allhome = true;
			for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
				if (!emcStatus->motion.joint[i].homing)
					continue; // Don't care about joints which are not homing
				if (!emcStatus->motion.joint[i].homed)
					allhome = false;
			}
			if (allhome) {
				//replynl(ctx, fmt::format("JOINT_WAIT_HOMED finished in {:.3f} seconds", etime() - st));
				// Wait one heartbeat to allow teleop to become set in the
				// motion controller.
				return doWaitHeartbeat(ctx, 1, WAIT_HEARTBEAT_TIMEOUT);
			}
		} else {
			if (emcStatus->motion.joint[joint].homed && !emcStatus->motion.joint[joint].homing) {
				//replynl(ctx, fmt::format("JOINT_WAIT_HOMED finished in {:.3f} seconds", etime() - st));
				// Wait one heartbeat to allow teleop to become set in the
				// motion controller.
				return doWaitHeartbeat(ctx, 1, WAIT_HEARTBEAT_TIMEOUT);
			}
		}
		double now = etime();
		esleep(JOINT_WAIT_HOMED_SLEEP);
		timeout -= etime() - now;
	}
	if (joint >= 0)
		errornl(ctx, fmt::format("Joint '{}' still not homed after {:.1f} seconds", joint, etime() - st));
	else
		errornl(ctx, fmt::format("One or more joints are still not homed after {:.1f} seconds", etime() - st));
	return rtError;
}

static cmdResponseType setMachine(connectionRecType &ctx)
{
	// SET MACHINE <ON|OFF>
	bool machon = false;
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (setterActive()) {
			errornl(ctx, "Cannot issue 'SET MACHINE ON' while another command is being processed");
			return rtError;
		}
		if (sendMachineOn())
			return rtError;
		machon = true;
		break;
	case kwOff:
		if (sendMachineOff())
			return rtError;
		break;
	}
	// Setting machine on/off takes a moment
	// We need to wait for the motion controller to catch up
	// and set/clear the motion enable flag internally.
	for(unsigned toc = 0; toc < WAIT_TASK_STATE_CHANGE_MAX; toc++) {
		// Wait for the motion control to run
		if (rtOk != doWaitHeartbeat(ctx, 1, WAIT_HEARTBEAT_TIMEOUT)) {
			// Bad sign,... can't wait for a heartbeat
			errornl(ctx, fmt::format("Waiting for motion controller heartbeat while running 'SET MACHINE {}' failed",
								onOff(machon)));
			return rtError;
		}
		updateStatus();
		if ((!machon && EMC_TASK_STATE::ON != emcStatus->task.state) ||
			( machon && EMC_TASK_STATE::ON == emcStatus->task.state)) {
			// Machine went on or off, all fine
			return rtOk;
		}
	}
	errornl(ctx, fmt::format("Waiting for 'SET MACHINE {}' failed", onOff(machon)));
	return rtError;
}

static cmdResponseType setEStop(connectionRecType &ctx)
{
	// SET ESTOP <ON|OFF>
	bool estopon = false;
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (sendEstop())
			return rtError;
		estopon = true;
		break;
	case kwOff:
		if (setterActive()) {
			errornl(ctx, "Cannot issue 'SET ESTOP OFF' while another command is being processed");
			return rtError;
		}
		if (sendEstopReset())
			return rtError;
		break;
	}

	// Setting/clearing ESTOP takes a moment
	for(unsigned toc = 0; toc < WAIT_TASK_STATE_CHANGE_MAX; toc++) {
		// Wait for the motion control to run
		if (rtOk != doWaitHeartbeat(ctx, 1, WAIT_HEARTBEAT_TIMEOUT)) {
			// Bad sign,... can't wait for a heartbeat
			errornl(ctx, fmt::format("Waiting for motion controller heartbeat while running 'SET ESTOP {}' failed",
								onOff(estopon)));
			return rtError;
		}
		updateStatus();
		if ((!estopon && EMC_TASK_STATE::ESTOP != emcStatus->task.state) ||
			( estopon && EMC_TASK_STATE::ESTOP == emcStatus->task.state)) {
			// System went into or out of ESTOP, all fine
			return rtOk;
		}
	}
	errornl(ctx, fmt::format("Waiting for 'SET ESTOP {}' failed", onOff(estopon)));
	return rtError;
}

static cmdResponseType setWait(connectionRecType &ctx)
{
	// *****
	// FIXME: Should be done in poll loop
	// *****
	// SET WAIT <RECEIVED|DONE|NONE>
	int oldserial = emcCommandSerialNumber;
	emcCommandSerialNumber = ctx.serial;
	cmdResponseType rv = rtOk;

	switch (checkReceivedDoneNone(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{received,done,none}}", ctx.toks[2])); return rtError;
	case kwReceived:
		if (emcCommandWaitReceived())
			rv = rtError;
		break;
	case kwDone:
		if (emcCommandWaitDone())
			rv = rtError;
		break;
	case kwNone:
		// 'none' ignored
		break;
	}
	emcCommandSerialNumber = oldserial;
	return rv;
}

static cmdResponseType setWaitHeartbeat(connectionRecType &ctx)
{
	// SET WAIT_HEARTBEAT [<periods>]
	int periods = 1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], periods)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (periods < 1) {
		errornl(ctx, fmt::format("Invalid periods '{}', valid range [1,...)", periods));
		return rtError;
	}

	return doWaitHeartbeat(ctx, periods, WAIT_HEARTBEAT_TIMEOUT);
}

static cmdResponseType setTimeout(connectionRecType &ctx)
{
	// SET TIMEOUT <float>
	double timeout;
	if (!to_double(ctx.toks[2], timeout)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", timeout));
		return rtError;
	}
	ctx.usrtimeout = timeout;
	return rtOk;
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

static cmdResponseType setUpdate(connectionRecType &ctx)
{
	// SET UPDATE <NONE|AUTO>
	switch (checkNoneAuto(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{none,auto}}", ctx.toks[2])); return rtError;
	case kwNone: emcUpdateType = EMC_UPDATE_NONE; break;
	case kwAuto: emcUpdateType = EMC_UPDATE_AUTO; break;
	}
	return rtOk;
}

static cmdResponseType setUpdateStatus(connectionRecType &)
{
	// SET UPDATE_STATUS
	return updateStatus() ? rtError : rtOk;
}

static cmdResponseType setMode(connectionRecType &ctx)
{
	// SET MODE <MANUAL|AUTO|MDI>
	switch (checkManualAutoMDI(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{manual,auto,mdi}}", ctx.toks[2])); return rtError;
	case kwManual:
		if (sendManual())
			return rtError;
		break;
	case kwAuto:
		if (sendAuto())
			return rtError;
		break;
	case kwMDI:
		if (sendMdi())
			return rtError;
		break;
	}
	return rtOk;
}

static cmdResponseType setMist(connectionRecType &ctx)
{
	// SET MIST <ON|OFF>
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (sendMistOn())
			return rtError;
		break;
	case kwOff:
		if (sendMistOff())
			return rtError;
		break;
	}
	return rtOk;
}

static cmdResponseType setFlood(connectionRecType &ctx)
{
	// SET FLOOD <ON|OFF>
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (sendFloodOn())
			return rtError;
		break;
	case kwOff:
		if (sendFloodOff())
			return rtError;
		break;
	}
	return rtOk;
}

static cmdResponseType setSpindle(connectionRecType &ctx)
{
	// SET SPINDLE <FORWARD|REVERSE|INCREASE|DECREASE|CONSTANT|OFF> [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	int cmd = checkSpindleCmd(ctx.toks[2]);
	int spindle = 0;
	if (ctx.toks.size() > 3) {
		if (!to_int(ctx.toks[3], spindle)) {
			errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[3]));
			return rtError;
		}
		if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
			errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
			return rtError;
		}
	}

	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (n != spindle && spindle != -1)
			continue;

		switch (cmd) {
		case kwForward:
			if (sendSpindleForward(n))
				return rtError;
			break;

		case kwReverse:
			if (sendSpindleReverse(n))
				return rtError;
			break;

		case kwIncrease:
			if (sendSpindleIncrease(n))
				return rtError;
			break;

		case kwDecrease:
			if (sendSpindleDecrease(n))
				return rtError;
			break;

		case kwConstant:
			if (sendSpindleConstant(n))
				return rtError;
			break;

		case kwOff:
			if (sendSpindleOff(n))
				return rtError;
			break;

		default:
			errornl(ctx,
					fmt::format("Invalid argument '{}', must be {{forward,reverse,increase,decrease,constant,off}}", ctx.toks[2]));
			return rtError;
		}
	}

	return rtOk;
}

static cmdResponseType setBrake(connectionRecType &ctx)
{
	// SET BRAKE <ON|OFF> [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	int onoff = checkOnOff(ctx.toks[2]);
	int spindle = 0;
	if (ctx.toks.size() > 3) {
		if (!to_int(ctx.toks[3], spindle)) {
			errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[3]));
			return rtError;
		}
		if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
			errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
			return rtError;
		}
	}
	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (n != spindle && spindle != -1)
			continue;

		switch (onoff) {
		case kwOn:
			if (sendBrakeEngage(n))
				return rtError;
			break;

		case kwOff:
			if (sendBrakeRelease(n))
				return rtError;
			break;

		default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
		}
	}

	return rtOk;
}

static std::optional<std::string> findFilename(const std::string &filename)
{
	if (filename.size() < 1)
		return nullptr;

	struct stat sb;
	if (filename[0] == '/') { // Absolute path
		return !stat(filename.c_str(), &sb) ? filename : nullptr;
	}

	// Relative name, try the search path
	for (const auto &path : searchPath) {
		std::string filepath = path + '/' + filename;
		if (!stat(filepath.c_str(), &sb)) {
			return filepath;
		}
	}

	// Last resort, try the filename 'as is'
	return !stat(filename.c_str(), &sb) ? filename : nullptr;
}

static cmdResponseType setLoadToolTable(connectionRecType &ctx)
{
	// SET LOAD_TOOL_TABLE <filename>
	if (ctx.third.size() < 1) {
		errornl(ctx, "Empty filename");
		return rtError;
	}

	auto pathp = findFilename(ctx.third);
	if (!pathp) {
		errornl(ctx, fmt::format("File '{}' not found", ctx.third));
		return rtError;
	}

	// FIXME: This should be PATH_MAX, but the interface glue
	// EMC_TOOL_LOAD_TOOL_TABLE is limited to LINELEN.
	size_t ll = sizeof(EMC_TOOL_LOAD_TOOL_TABLE::file);
	if (pathp->size() >= ll) {
		errornl(ctx, fmt::format("Filename path '{}' too long, got {} bytes, max {}", *pathp, pathp->size(), ll - 1));
		return rtError;
	}

	if (sendLoadToolTable(pathp->c_str()))
		return rtError;
	return rtOk;
}

static cmdResponseType setToolOffset(connectionRecType &ctx)
{
	// SET TOOL_OFFSET <tool> <length> <dia>
	int tool;
	double length, diameter;
	if (!to_int(ctx.toks[2], tool)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (!to_double(ctx.toks[3], length)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[3]));
		return rtError;
	}
	if (!to_double(ctx.toks[4], diameter)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[4]));
		return rtError;
	}

	if (sendToolSetOffset(tool, length, diameter))
		return rtError;
	return rtOk;
}

static cmdResponseType setOverrideLimits(connectionRecType &ctx)
{
	// SET OVERRIDE_LIMITS <ON|OFF>
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (sendOverrideLimits(0))
			return rtError;
		break;
	case kwOff:
		if (sendOverrideLimits(-1))
			return rtError;
	}
	return rtOk;
}

static cmdResponseType setMDI(connectionRecType &ctx)
{
	// SET MDI <whatever>
	if (sendMdiCmd(ctx.third.c_str()))
		return rtError;
	return rtOk;
}

static cmdResponseType setJointHome(connectionRecType &ctx)
{
	// SET JOINT_HOME [joint]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}

	// joint == -1 means "Home All", any other negative is wrong
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (sendHome(joint))
		return rtError;
	return rtOk;
}

static cmdResponseType setJointUnhome(connectionRecType &ctx)
{
	// SET JOINT_UNHOME [joint]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}

	// joint == -1 means "Unhome all joints",
	// joint == -2 means "Unhome joints marked as VOLATILE_HOME",
	// any other negative value is an error.
	if (joint < -2 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-2,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (sendUnHome(joint))
		return rtError;

	return rtOk;
}

static int axisLetter(char ch)
{
	// Note: Removed P=3 and R=4; Changed W=5 into W=8; Added U=6 and V=7;
	// On a 9-axis system we have XYZ, ABC and UVW
	switch (std::toupper((unsigned char)ch)) {
	case 'X': return 0;
	case 'Y': return 1;
	case 'Z': return 2;

	case 'A': return 3;
	case 'B': return 4;
	case 'C': return 5;

	case 'U': return 6;
	case 'V': return 7;
	case 'W': return 8;

	default: return -1;
	}
}

static cmdResponseType setJogStop(connectionRecType &ctx)
{
	// SET JOG_STOP <joint|axis>
	int jogmode = JOGJOINT;
	int joint;
	if (!to_int(ctx.toks[2], joint)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		joint = axisLetter(ctx.toks[2].data()[0]);
		jogmode = JOGTELEOP;
		if (joint < 0 || joint >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << joint) & emcStatus->motion.traj.axis_mask)) {
			errornl(ctx, fmt::format("Invalid axis '{}', not configured", ctx.toks[2]));
			return rtError;
		}
	} else if (joint < 0 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}' valid range [0,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	if (sendJogStop(joint, jogmode))
		return rtError;
	return rtOk;
}

static cmdResponseType setJog(connectionRecType &ctx)
{
	// SET JOG <joint|axis> <speed>
	int jogmode = JOGJOINT;
	int joint;
	if (!to_int(ctx.toks[2], joint)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		joint = axisLetter(ctx.toks[2].data()[0]);
		jogmode = JOGTELEOP;
		if (joint < 0 || joint >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << joint) & emcStatus->motion.traj.axis_mask)) {
			errornl(ctx, fmt::format("Invalid axis '{}', not configured", ctx.toks[2]));
			return rtError;
		}
	} else if (joint < 0 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [0,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	double speed;
	if (!to_double(ctx.toks[3], speed))
		return rtError;

	if (sendJogCont(joint, jogmode, speed))
		return rtError;
	return rtOk;
}

static cmdResponseType setFeedOverride(connectionRecType &ctx)
{
	// SET FEED_OVERRIDE <float>
	double pct;
	if (!to_double(ctx.toks[2], pct)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[2]));
		return rtError;
	}
	if (sendFeedOverride(pct / 100.0))
		return rtError;
	return rtOk;
}

static cmdResponseType setJogIncr(connectionRecType &ctx)
{
	// SET JOG_INCR <joint|axis> <speed> <incr>
	int jogmode = JOGJOINT;
	int joint;
	if (!to_int(ctx.toks[2], joint)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		joint = axisLetter(ctx.toks[2].data()[0]);
		jogmode = JOGTELEOP;
		if (joint < 0 || joint >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << joint) & emcStatus->motion.traj.axis_mask)) {
			errornl(ctx, fmt::format("Invalid axis '{}', not configured", ctx.toks[2]));
			return rtError;
		}
	} else if (joint < 0 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [0,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	double speed;
	if (!to_double(ctx.toks[3], speed))
		return rtError;

	double incr;
	if (!to_double(ctx.toks[4], incr))
		return rtError;

	if (sendJogIncr(joint, jogmode, speed, incr))
		return rtError;
	return rtOk;
}

static cmdResponseType setTaskPlanInit(connectionRecType &)
{
	// SET TASK_PLAN_INIT
	if (sendTaskPlanInit() != 0)
		return rtError;
	return rtOk;
}

static cmdResponseType setOpen(connectionRecType &ctx)
{
	// SET OPEN <filename>
	if (ctx.third.size() < 1) {
		errornl(ctx, "Empty filename");
		return rtError;
	}

	auto pathp = findFilename(ctx.third);
	if (!pathp) {
		errornl(ctx, fmt::format("File '{}' not found", ctx.third));
		return rtError;
	}

	ctx.progname = *pathp;
	if (sendProgramOpen(pathp->c_str()))
		return rtError;

	return rtOk;
}

static cmdResponseType setRun(connectionRecType &ctx)
{
	// SET RUN [linenr]
	if (ctx.toks.size() == 2) {
		// run from beginning
		if (sendProgramRun(0))
			return rtError;
		return rtOk;
	}

	// run from line number
	int line;
	if (!to_int(ctx.toks[2], line)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (sendProgramRun(line))
		return rtError;
	return rtOk;
}

static cmdResponseType setPause(connectionRecType &ctx)
{
	// SET PAUSE
	if (ctx.waitmode != EMC_WAIT_RECEIVED) {
		errornl(ctx, "Sending 'SET PAUSE' while not in 'WAIT_MODE RECEIVED' will stall forever");
		return rtError;
	}
	if (sendProgramPause())
		return rtError;
	return rtOk;
}

static cmdResponseType setResume(connectionRecType &ctx)
{
	// SET RESUME
	if (ctx.waitmode != EMC_WAIT_DONE) {
		warnnl(ctx, "Sending 'SET RESUME' while not in 'WAIT_MODE DONE' may return before the system resumes");
		return rtError;
	}
	if (sendProgramResume())
		return rtError;
	return rtOk;
}

static cmdResponseType setStep(connectionRecType &)
{
	// SET STEP
	// FIXME: How should ctx.waitmode be set??
	if (sendProgramStep())
		return rtError;
	return rtOk;
}

static cmdResponseType setAbort(connectionRecType &)
{
	// SET ABORT
	if (sendAbort())
		return rtError;
	return rtOk;
}

static cmdResponseType setLinearUnitConversion(connectionRecType &ctx)
{
	// SET LINEAR_UNIT_CONVERSION <INCH|MM|CM|AUTO|CUSTOM>
	switch (checkConversionStr(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{inch,mm,cm,auto,custom}}", ctx.toks[2])); return rtError;
	case kwInch: linearUnitConversion = LINEAR_UNITS_INCH; break;
	case kwmm: linearUnitConversion = LINEAR_UNITS_MM; break;
	case kwcm: linearUnitConversion = LINEAR_UNITS_CM; break;
	case kwAuto: linearUnitConversion = LINEAR_UNITS_AUTO; break;
	case kwCustom: linearUnitConversion = LINEAR_UNITS_CUSTOM; break;
	}
	return rtOk;
}

static cmdResponseType setAngularUnitConversion(connectionRecType &ctx)
{
	// SET ANGULAR_UNIT_CONVERSION <DEG|RAD|GRAD|AUTO|CUSTOM>
	switch (checkAngularConversionStr(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{deg,rad,grad,auto,custom}}", ctx.toks[2])); return rtError;
	case kwDeg: angularUnitConversion = ANGULAR_UNITS_DEG; break;
	case kwRad: angularUnitConversion = ANGULAR_UNITS_RAD; break;
	case kwGrad: angularUnitConversion = ANGULAR_UNITS_GRAD; break;
	case kwAuto: angularUnitConversion = ANGULAR_UNITS_AUTO; break;
	case kwCustom: angularUnitConversion = ANGULAR_UNITS_CUSTOM; break;
	}
	return rtOk;
}

static cmdResponseType setTeleopEnable(connectionRecType &ctx)
{
	// SET TELEOP_ENABLE <ON|OFF>
	switch (checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
		if (sendSetTeleopEnable(1))
			return rtError;
		break;
	case kwOff:
		if (sendSetTeleopEnable(0))
			return rtError;
		break;
	}
	return rtOk;
}

static cmdResponseType setProbe(connectionRecType &ctx)
{
	// SET PROBE <float> <float> <float>
	double x, y, z;
	if (!to_double(ctx.toks[2], x)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[2]));
		return rtError;
	}
	if (!to_double(ctx.toks[3], y)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[3]));
		return rtError;
	}
	if (!to_double(ctx.toks[4], z)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be floating point", ctx.toks[4]));
		return rtError;
	}

	// sendProbe() is a nonfunctional disaster. You need a lot of information
	// about _all_ axes and you need both velocity, acceleration and jerk
	// parameters. These are not easy to get but are available in the canon
	// interface. Therefore, just send it as an MDI command and let the
	// interpreter handle all the juicy stuff.
	std::string p = fmt::format("g38.2 x{:f} y{:f} z{:f}", x, y, z);
	if (sendMdiCmd(p.c_str()))
		return rtError;
	return rtOk;
}

static cmdResponseType setProbeClear(connectionRecType &)
{
	// SET PROBE_CLEAR
	if (sendClearProbeTrippedFlag() != 0)
		return rtError;
	return rtOk;
}

static cmdResponseType setSpindleOverride(connectionRecType &ctx)
{
	// SET SPINDLE_OVERRIDE <percent> [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	double pct;
	if (!to_double(ctx.toks[2], pct)) {
		return rtError;
	}
	// Validate percent range
	if (pct < 0.0 || pct > 100.0) {
		errornl(ctx, fmt::format("invalid: {} (valid: 0-100)", pct));
		return rtError;
	}

	int spindle = 0;
	if (ctx.toks.size() > 3 && !to_int(ctx.toks[3], spindle)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[3]));
		return rtError;
	}
	if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
		errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
		return rtError;
	}

	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (spindle == -1 || n == spindle) {
			if (sendSpindleOverride(n, pct / 100.0))
				return rtError;
		}
	}

	return rtOk;
}

static cmdResponseType setOptionalStop(connectionRecType &ctx)
{
	// SET OPTIONAL_STOP <value>
	int value;
	switch (value = checkOnOff(ctx.toks[2])) {
	default: errornl(ctx, fmt::format("Invalid argument '{}', must be {{on,off}}", ctx.toks[2])); return rtError;
	case kwOn:
	case kwOff: break;
	}
	if (sendSetOptionalStop(value == kwOn))
		return rtError;
	return rtOk;
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

static cmdResponseType getEnable(connectionRecType &ctx)
{
	// GET ENABLE
	replynl(ctx, onOffString("ENABLE", isEnabled(ctx)));
	return rtOk;
}

static cmdResponseType getClients(connectionRecType &ctx)
{
	// GET CLIENTS
	for (const auto &c : clients) {
		replynl(ctx, fmt::format("CLIENTS {} '{}'{}{}",
					c.sock,
					c.hostname,
					c.linked ? " LINKED" : "",
					isEnabled(c) ? " ENABLED" : ""));
	}
	return rtOk;
}

static cmdResponseType getCommProt(connectionRecType &ctx)
{
	// GET COMM_PROT
	warnnl(ctx, "GET COMM_PROT has been deprecated. It has no function.");
	replynl(ctx, fmt::format("COMM_PROT {}", ctx.version));
	return rtOk;
}

static cmdResponseType getDebug(connectionRecType &ctx)
{
	// GET DEBUG
	replynl(ctx, fmt::format("DEBUG 0x{:08x}", emcStatus->debug));
	return rtOk;
}

static cmdResponseType getWaitMode(connectionRecType &ctx)
{
	// GET WAIT_MODE
	switch (ctx.waitmode) {
	case EMC_WAIT_RECEIVED: replynl(ctx, "WAIT_MODE RECEIVED"); break;
	case EMC_WAIT_DONE: replynl(ctx, "WAIT_MODE DONE"); break;
	default: replynl(ctx, fmt::format("internal: Invalid ctx.waitmode '{}'", (int)ctx.waitmode)); return rtError;
	}
	return rtOk;
}

static cmdResponseType getPlat(connectionRecType &ctx)
{
	// GET PLAT
	replynl(ctx, "PLAT Linux");
	return rtOk;
}

static cmdResponseType getEStop(connectionRecType &ctx)
{
	// GET ESTOP
	replynl(ctx, onOffString("ESTOP", emcStatus->task.state == EMC_TASK_STATE::ESTOP));
	return rtOk;
}

static cmdResponseType getTimeout(connectionRecType &ctx)
{
	// GET TIMEOUT
	replynl(ctx, fmt::format("TIMEOUT {:f}", ctx.usrtimeout));
	return rtOk;
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
	replynl(ctx, fmt::format("TIME {}", etime()));
	return rtOk;
}

static cmdResponseType getUpdate(connectionRecType &ctx)
{
	// GET UPDATE
	switch (emcUpdateType) {
	case EMC_UPDATE_NONE: replynl(ctx, "UPDATE NONE"); break;
	case EMC_UPDATE_AUTO: replynl(ctx, "UPDATE AUTO"); break;
	default: replynl(ctx, "UPDATE UNKNOWN"); break;
	}

	return rtOk;
}

static cmdResponseType getError(connectionRecType &ctx)
{
	// GET ERROR
	if (updateError()) {
		replynl(ctx, "internal: updateError(): bad status from LinuxCNC");
		return rtError;
	}

	// FIXME: static buffer 'error_string'
	if (!error_string[0]) {
		replynl(ctx, "ERROR OK");
	} else {
		replynl(ctx, fmt::format("ERROR {}", error_string));
		error_string[0] = 0;
	}
	return rtOk;
}

static cmdResponseType getOperatorDisplay(connectionRecType &ctx)
{
	// GET OPERATOR_DISPLAY
	if (updateError()) {
		replynl(ctx, "internal: updateError(): bad status from LinuxCNC");
		return rtError;
	}

	// FIXME: static buffer 'operator_display_string'
	if (!operator_display_string[0])
		replynl(ctx, "OPERATOR_DISPLAY OK");
	else {
		replynl(ctx, fmt::format("OPERATOR_DISPLAY {}", operator_display_string));
		operator_display_string[0] = 0;
	}
	return rtOk;
}

static cmdResponseType getOperatorText(connectionRecType &ctx)
{
	// GET OPERATOR_TEXT
	if (updateError()) {
		replynl(ctx, "internal: updateError(): bad status from LinuxCNC");
		return rtError;
	}

	// FIXME: static buffer 'operator_text_string'
	if (!operator_text_string[0])
		replynl(ctx, "OPERATOR_TEXT OK");
	else {
		replynl(ctx, fmt::format("OPERATOR_TEXT {}", operator_text_string));
		operator_text_string[0] = 0;
	}
	return rtOk;
}

static cmdResponseType getMachine(connectionRecType &ctx)
{
	// GET MACHINE
	replynl(ctx, onOffString("MACHINE",emcStatus->task.state == EMC_TASK_STATE::ON));
	return rtOk;
}

static cmdResponseType getMode(connectionRecType &ctx)
{
	// GET MODE
	switch (emcStatus->task.mode) {
	case EMC_TASK_MODE::MANUAL: replynl(ctx, "MODE MANUAL"); break;
	case EMC_TASK_MODE::AUTO: replynl(ctx, "MODE AUTO"); break;
	case EMC_TASK_MODE::MDI: replynl(ctx, "MODE MDI"); break;
	default:
		errornl(ctx, fmt::format("internal: GET MODE: unrecognised EMC_TASK_MODE value '{}'", (int)emcStatus->task.mode));
		replynl(ctx, "MODE ?");
	}
	return rtOk;
}

static cmdResponseType getOpen(connectionRecType &ctx)
{
	// GET OPEN
	if (ctx.progname.size() < 1)
		replynl(ctx, "OPEN NONE");
	else
		replynl(ctx, fmt::format("OPEN {}", ctx.progname));
	return rtOk;
}

static cmdResponseType getMist(connectionRecType &ctx)
{
	// GET MIST
	replynl(ctx, onOffString("MIST", !!emcStatus->io.coolant.mist));
	return rtOk;
}

static cmdResponseType getFlood(connectionRecType &ctx)
{
	// GET FLOOD
	replynl(ctx, onOffString("FLOOD", !!emcStatus->io.coolant.flood));
	return rtOk;
}

static cmdResponseType getSpindle(connectionRecType &ctx)
{
	// GET SPINDLE [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	int spindle = 0;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], spindle)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
		errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
		return rtError;
	}

	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (n != spindle && spindle != -1)
			continue;

		if (emcStatus->motion.spindle[n].increasing > 0)
			replynl(ctx, fmt::format("SPINDLE {} INCREASE", n));
		else if (emcStatus->motion.spindle[n].increasing < 0)
			replynl(ctx, fmt::format("SPINDLE {} DECREASE", n));
		else if (emcStatus->motion.spindle[n].direction > 0)
			replynl(ctx, fmt::format("SPINDLE {} FORWARD", n));
		else if (emcStatus->motion.spindle[n].direction < 0)
			replynl(ctx, fmt::format("SPINDLE {} REVERSE", n));
		else
			replynl(ctx, fmt::format("SPINDLE {} OFF", n));
	}

	return rtOk;
}

static cmdResponseType getBrake(connectionRecType &ctx)
{
	// GET BRAKE [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	int spindle = 0;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], spindle)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
		errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
		return rtError;
	}

	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (n != spindle && spindle != -1)
			continue;
		replynl(ctx, fmt::format("BRAKE {} {}", n, onOff(!!emcStatus->motion.spindle[n].brake)));
	}

	return rtOk;
}

static cmdResponseType getTool(connectionRecType &ctx)
{
	// GET TOOL
	replynl(ctx, fmt::format("TOOL {}", emcStatus->io.tool.toolInSpindle));
	return rtOk;
}

static cmdResponseType getToolOffset(connectionRecType &ctx)
{
	// GET TOOL_OFFSET
	replynl(ctx, fmt::format("TOOL_OFFSET {:f}", emcStatus->task.toolOffset.tran.z));
	return rtOk;
}

static cmdResponseType printPose(connectionRecType &ctx, int axis, const std::string &pfx, const EmcPose &pose)
{
	switch (axis) {
	default: return rtError;

	case -1: // output all axes
		replynl(ctx,
				fmt::format("{} {:f} {:f} {:f} {:f} {:f} {:f} {:f} {:f} {:f}",
							pfx,
							pose.tran.x,
							pose.tran.y,
							pose.tran.z,
							pose.a,
							pose.b,
							pose.c,
							pose.u,
							pose.v,
							pose.w));
		break;

	case 0: replynl(ctx, fmt::format("{} X {:f}", pfx, pose.tran.x)); break;
	case 1: replynl(ctx, fmt::format("{} Y {:f}", pfx, pose.tran.y)); break;
	case 2: replynl(ctx, fmt::format("{} Z {:f}", pfx, pose.tran.z)); break;
	case 3: replynl(ctx, fmt::format("{} A {:f}", pfx, pose.a)); break;
	case 4: replynl(ctx, fmt::format("{} B {:f}", pfx, pose.b)); break;
	case 5: replynl(ctx, fmt::format("{} C {:f}", pfx, pose.c)); break;
	case 6: replynl(ctx, fmt::format("{} U {:f}", pfx, pose.u)); break;
	case 7: replynl(ctx, fmt::format("{} V {:f}", pfx, pose.v)); break;
	case 8: replynl(ctx, fmt::format("{} W {:f}", pfx, pose.w)); break;
	}
	return rtOk;
}

static cmdResponseType getAbsCmdPos(connectionRecType &ctx)
{
	// GET ABS_CMD_POS [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	return printPose(ctx, axis, "ABS_CMD_POS", emcStatus->motion.traj.position);
}

static cmdResponseType getAxisVelocity(connectionRecType &ctx)
{
	// GET AXIS_VELOCITY [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	EmcPose pose = {
		{
			emcStatus->motion.axis[0].velocity,
			emcStatus->motion.axis[1].velocity,
			emcStatus->motion.axis[2].velocity
		},
		emcStatus->motion.axis[3].velocity,
		emcStatus->motion.axis[4].velocity,
		emcStatus->motion.axis[5].velocity,
		emcStatus->motion.axis[6].velocity,
		emcStatus->motion.axis[7].velocity,
		emcStatus->motion.axis[8].velocity,
	};

	return printPose(ctx, axis, "AXIS_VELOCITY", pose);
}

static cmdResponseType getAbsActPos(connectionRecType &ctx)
{
	// GET ABS_ACT_POS [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	return printPose(ctx, axis, "ABS_ACT_POS", emcStatus->motion.traj.actualPosition);
}

static cmdResponseType getRelCmdPos(connectionRecType &ctx)
{
	// GET REL_CMD_POS [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	const EmcPose *pos = &emcStatus->motion.traj.position;
	const EmcPose *g5x = &emcStatus->task.g5x_offset;
	const EmcPose *g92 = &emcStatus->task.g92_offset;
	EmcPose pose = {
		{
			pos->tran.x - g5x->tran.x - g92->tran.x,
			pos->tran.y - g5x->tran.y - g92->tran.y,
			pos->tran.z - g5x->tran.z - g92->tran.z,
		 },
		pos->a - g5x->a - g92->a,
		pos->b - g5x->b - g92->b,
		pos->c - g5x->c - g92->c,
		pos->u - g5x->u - g92->u,
		pos->v - g5x->v - g92->v,
		pos->w - g5x->w - g92->w,
	};
	return printPose(ctx, axis, "REL_CMD_POS", pose);
}

static cmdResponseType getRelActPos(connectionRecType &ctx)
{
	// GET REL_ACT_POS [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', valid range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	const EmcPose *pos = &emcStatus->motion.traj.actualPosition;
	const EmcPose *g5x = &emcStatus->task.g5x_offset;
	const EmcPose *g92 = &emcStatus->task.g92_offset;
	EmcPose pose = {
		{
			pos->tran.x - g5x->tran.x - g92->tran.x,
			pos->tran.y - g5x->tran.y - g92->tran.y,
			pos->tran.z - g5x->tran.z - g92->tran.z,
		 },
		pos->a - g5x->a - g92->a,
		pos->b - g5x->b - g92->b,
		pos->c - g5x->c - g92->c,
		pos->u - g5x->u - g92->u,
		pos->v - g5x->v - g92->v,
		pos->w - g5x->w - g92->w,
	};
	return printPose(ctx, axis, "REL_ACT_POS", pose);
}

static cmdResponseType getJointPos(connectionRecType &ctx)
{
	// GET JOINT_POS [joint]
	int joint = -1; // Return all axes by default
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	if (joint == -1) {
		std::string jp = "JOINT_POS";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			jp += fmt::format(" {:f}", emcStatus->motion.joint[i].input);
		}
		replynl(ctx, jp);
		return rtOk;
	}

	replynl(ctx, fmt::format("JOINT_POS {} {:f}", joint, emcStatus->motion.joint[joint].input));
	return rtOk;
}

static cmdResponseType getPosOffset(connectionRecType &ctx)
{
	// GET POS_OFFSET [axis]
	int axis = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], axis)) {
		if (1 != ctx.toks[2].size()) // If letter, than just one char
			return rtError;
		axis = axisLetter(ctx.toks[2].data()[0]);
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
			errornl(ctx, fmt::format("Invalid axis '{}', must be one of XYZABCUVW", ctx.toks[2]));
			return rtError;
		}
		if (!((1 << axis) & emcStatus->motion.traj.axis_mask)) {
			warnnl(ctx, fmt::format("Axis '{}', not configured", ctx.toks[2]));
		}
	}
	if (axis < -1 || axis >= EMCMOT_MAX_AXIS) {
		errornl(ctx, fmt::format("Invalid axis '{}', valid range [-1,{}]", ctx.toks[2], EMCMOT_MAX_AXIS - 1));
		return rtError;
	}

	const EmcPose *g5x = &emcStatus->task.g5x_offset;
	const EmcPose *g92 = &emcStatus->task.g92_offset;
	EmcPose pose = {
		{
			convertLinearUnits(g5x->tran.x + g92->tran.x),
			convertLinearUnits(g5x->tran.y + g92->tran.y),
			convertLinearUnits(g5x->tran.z + g92->tran.z),
		 },
		convertLinearUnits(g5x->a + g92->a),
		convertLinearUnits(g5x->b + g92->b),
		convertLinearUnits(g5x->c + g92->c),
		convertLinearUnits(g5x->u + g92->u),
		convertLinearUnits(g5x->v + g92->v),
		convertLinearUnits(g5x->w + g92->w),
	};
	return printPose(ctx, axis, "POS_OFFSET", pose);
}

static const char *jointLimit(int joint)
{
	if (emcStatus->motion.joint[joint].minHardLimit)
		return "MINHARD";
	else if (emcStatus->motion.joint[joint].minSoftLimit)
		return "MINSOFT";
	else if (emcStatus->motion.joint[joint].maxSoftLimit)
		return "MAXSOFT";
	else if (emcStatus->motion.joint[joint].maxHardLimit)
		return "MAXHARD";
	else
		return "OK";
}

static cmdResponseType getJointLimit(connectionRecType &ctx)
{
	// GET JOINT_LIMIT [joint]

	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (joint >= 0) {
		std::string lim;
		replynl(ctx, fmt::format("JOINT_LIMIT {} {}", joint, jointLimit(joint)));
	} else {
		std::string jl = "JOINT_LIMIT";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			jl += ' ';
			jl += jointLimit(i);
		}
		replynl(ctx, jl);
	}
	return rtOk;
}

static cmdResponseType getJointVelocity(connectionRecType &ctx)
{
	// GET JOINT_VELOCITY [joint]

	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (joint >= 0) {
		std::string lim;
		replynl(ctx, fmt::format("JOINT_VELOCITY {} {:f}", joint, emcStatus->motion.joint[joint].velocity));
	} else {
		std::string jl = "JOINT_VELOCITY";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			jl += fmt::format(" {:f}", emcStatus->motion.joint[joint].velocity);
		}
		replynl(ctx, jl);
	}
	return rtOk;
}

static cmdResponseType getJointFault(connectionRecType &ctx)
{
	// GET JOINT_FAULT [joint]

	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (joint >= 0) {
		replynl(ctx, fmt::format("JOINT_FAULT {} {}", joint, emcStatus->motion.joint[joint].fault ? "FAULT" : "OK"));
	} else {
		std::string flt = "JOINT_FAULT";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			flt += emcStatus->motion.joint[i].fault ? " FAULT" : " OK";
		}
		replynl(ctx, flt);
	}
	return rtOk;
}

static cmdResponseType getOverrideLimits(connectionRecType &ctx)
{
	// GET OVERRIDE_LIMITS
	replynl(ctx, onOffString("OVERRIDE_LIMITS", emcStatus->motion.joint[0].overrideLimits));
	return rtOk;
}

static cmdResponseType getJointHomed(connectionRecType &ctx)
{
	// GET JOINT_HOMED [joint]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}
	if (joint >= 0) {
		replynl(ctx, fmt::format("JOINT_HOMED {} {}", joint, yesNo(emcStatus->motion.joint[joint].homed)));
	} else {
		std::string jh = "JOINT_HOMED";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			jh += ' ';
			jh += yesNo(emcStatus->motion.joint[i].homed);
		}
		replynl(ctx, jh);
	}
	return rtOk;
}

static cmdResponseType getProgram(connectionRecType &ctx)
{
	// GET PROGRAM
	if (emcStatus->task.file[0] != 0)
		replynl(ctx, fmt::format("PROGRAM {}", emcStatus->task.file));
	else
		replynl(ctx, "PROGRAM NONE");
	return rtOk;
}

static cmdResponseType getProgramLine(connectionRecType &ctx)
{
	// GET PROGRAM_LINE
	int lineNo;

	if ((programStartLine < 0) || (emcStatus->task.readLine < programStartLine))
		lineNo = emcStatus->task.readLine;
	else if (emcStatus->task.currentLine > 0)
		if ((emcStatus->task.motionLine > 0) && (emcStatus->task.motionLine < emcStatus->task.currentLine))
			lineNo = emcStatus->task.motionLine;
		else
			lineNo = emcStatus->task.currentLine;
	else
		lineNo = 0;
	replynl(ctx, fmt::format("PROGRAM_LINE {}", lineNo));
	return rtOk;
}

static cmdResponseType getProgramStatus(connectionRecType &ctx)
{
	// GET PROGRAM_STATUS
	switch (emcStatus->task.interpState) {
	case EMC_TASK_INTERP::READING:
	case EMC_TASK_INTERP::WAITING: replynl(ctx, "PROGRAM_STATUS RUNNING"); break;
	case EMC_TASK_INTERP::PAUSED: replynl(ctx, "PROGRAM_STATUS PAUSED"); break;
	default: replynl(ctx, "PROGRAM_STATUS IDLE"); break;
	}
	return rtOk;
}

static cmdResponseType getProgramCodes(connectionRecType &ctx)
{
	// GET PROGRAM_CODES
	std::string pc = "PROGRAM_CODES";
	for (int i = 1; i < ACTIVE_G_CODES; i++) {
		int code = emcStatus->task.activeGCodes[i];
		if (code == -1)
			continue;
		if (code % 10)
			pc += fmt::format(" G{:.1f}", (double)code / 10.0);
		else
			pc += fmt::format(" G{}", code / 10);
	}
	pc += fmt::format(" F{:.0f}", emcStatus->task.activeSettings[1]);
	pc += fmt::format(" S{:.0f}", fabs(emcStatus->task.activeSettings[2]));
	replynl(ctx, pc);
	return rtOk;
}

static const std::string jointType(unsigned type)
{
	switch (type) {
	case EMC_LINEAR: return "LINEAR";
	case EMC_ANGULAR: return "ANGULAR";
	default: return "CUSTOM";
	}
}

static cmdResponseType getJointType(connectionRecType &ctx)
{
	// GET JOINT_TYPE [joint]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	if (joint >= 0) {
		replynl(ctx, fmt::format("JOINT_TYPE {} {}", joint, jointType(emcStatus->motion.joint[joint].jointType)));
	} else {
		std::string jt = "JOINT_TYPE";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			jt += ' ';
			jt += jointType(emcStatus->motion.joint[i].jointType);
		}
		replynl(ctx, jt);
	}
	return rtOk;
}

static const std::string jointTypeUnits(unsigned type, double units)
{
	switch (type) {
	case EMC_LINEAR:
		if (CLOSE(units, 1.0, LINEAR_CLOSENESS))
			return "MM";
		else if (CLOSE(units, INCH_PER_MM, LINEAR_CLOSENESS))
			return "INCH";
		else if (CLOSE(units, CM_PER_MM, LINEAR_CLOSENESS))
			return "CM";
		else
			return "CUSTOM";
	case EMC_ANGULAR:
		if (CLOSE(units, 1.0, ANGULAR_CLOSENESS))
			return "DEG";
		else if (CLOSE(units, RAD_PER_DEG, ANGULAR_CLOSENESS))
			return "RAD";
		else if (CLOSE(units, GRAD_PER_DEG, ANGULAR_CLOSENESS))
			return "GRAD";
		else
			return "CUSTOM";
	default: return "CUSTOM";
	}
}

static cmdResponseType getJointUnits(connectionRecType &ctx)
{
	// GET JOINT_UNITS [joint]
	int joint = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], joint)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (joint < -1 || joint >= emcStatus->motion.traj.joints) {
		errornl(ctx, fmt::format("Invalid joint '{}', valid range [-1,{}]", joint, emcStatus->motion.traj.joints - 1));
		return rtError;
	}

	if (joint >= 0) {
		const std::string ju = jointTypeUnits(emcStatus->motion.joint[joint].jointType, emcStatus->motion.joint[joint].units);
		replynl(ctx, fmt::format("JOIN_UNITS {} {}", joint, ju));
	} else {
		std::string ju = "JOINT_UNITS";
		for (int i = 0; i < emcStatus->motion.traj.joints; i++) {
			ju += ' ';
			ju += jointTypeUnits(emcStatus->motion.joint[i].jointType, emcStatus->motion.joint[i].units);
		}
		replynl(ctx, ju);
	}
	return rtOk;
}

static const char *programUnits(CANON_UNITS u)
{
	switch(u) {
	case CANON_UNITS_INCHES: return "INCH";
	case CANON_UNITS_MM: return "MM";
	case CANON_UNITS_CM: return "CM";
	default: return "CUSTOM";
	}
}

static cmdResponseType getProgramLinearUnits(connectionRecType &ctx)
{
	// GET PROGRAM_LINEAR_UNITS
	replynl(ctx, fmt::format("PROGRAM_UNITS {}", programUnits(emcStatus->task.programUnits)));
	return rtOk;
}

static cmdResponseType getProgramAngularUnits(connectionRecType &ctx)
{
	// GET PROGRAM_ANGULAR_UNITS
	// The program is always in degrees...
	replynl(ctx, "PROGRAM_ANGULAR_UNITS DEG");
	return rtOk;
}

static cmdResponseType getUserLinearUnits(connectionRecType &ctx)
{
	// GET USER_LINEAR_UNITS
	const char *ulu = "CUSTOM";
	if (CLOSE(emcStatus->motion.traj.linearUnits, 1.0, LINEAR_CLOSENESS))
		ulu = "MM";
	else if (CLOSE(emcStatus->motion.traj.linearUnits, INCH_PER_MM, LINEAR_CLOSENESS))
		ulu = "INCH";
	else if (CLOSE(emcStatus->motion.traj.linearUnits, CM_PER_MM, LINEAR_CLOSENESS))
		ulu = "CM";

	replynl(ctx, fmt::format("USER_LINEAR_UNITS {}", ulu));
	return rtOk;
}

static cmdResponseType getUserAngularUnits(connectionRecType &ctx)
{
	// GET USER_ANGULAR_UNITS
	const char *uau = "CUSTOM";
	if (CLOSE(emcStatus->motion.traj.angularUnits, 1.0, ANGULAR_CLOSENESS))
		uau = "DEG";
	else if (CLOSE(emcStatus->motion.traj.angularUnits, RAD_PER_DEG, ANGULAR_CLOSENESS))
		uau = "RAD";
	else if (CLOSE(emcStatus->motion.traj.angularUnits, GRAD_PER_DEG, ANGULAR_CLOSENESS))
		uau = "GRAD";

	replynl(ctx, fmt::format("USER_ANGULAR_UNITS {}", uau));
	return rtOk;
}

static cmdResponseType getDisplayLinearUnits(connectionRecType &ctx)
{
	// GET DISPLAY_LINEAR_UNITS
	const char *dlu = "CUSTOM";
	switch (linearUnitConversion) {
	case LINEAR_UNITS_INCH: dlu = "INCH"; break;
	case LINEAR_UNITS_MM:   dlu = "MM"; break;
	case LINEAR_UNITS_CM:   dlu = "CM"; break;
	case LINEAR_UNITS_AUTO: dlu = programUnits(emcStatus->task.programUnits); break;
	default: /* CUSTOM */ break;
	}
	replynl(ctx, fmt::format("DISPLAY_LINEAR_UNITS {}", dlu));
	return rtOk;
}

static cmdResponseType getDisplayAngularUnits(connectionRecType &ctx)
{
	// GET DISPLAY_ANGULAR_UNITS
	const char *dau = "CUSTOM";
	switch (angularUnitConversion) {
	case ANGULAR_UNITS_AUTO:
	case ANGULAR_UNITS_DEG:  dau = "DEG"; break;
	case ANGULAR_UNITS_RAD:  dau = "RAD"; break;
	case ANGULAR_UNITS_GRAD: dau = "GRAD"; break;
	default: /* CUSTOM */ break;
	}
	replynl(ctx, fmt::format("DISPLAY_ANGULAR_UNITS {}", dau));
	return rtOk;
}

static cmdResponseType getLinearUnitConversion(connectionRecType &ctx)
{
	// GET LINEAR_UNIT_CONVERSION
	const char *luc = "CUSTOM";
	switch (linearUnitConversion) {
	case LINEAR_UNITS_INCH: luc = "INCH"; break;
	case LINEAR_UNITS_MM:   luc = "MM"; break;
	case LINEAR_UNITS_CM:   luc = "CM"; break;
	case LINEAR_UNITS_AUTO: luc = "AUTO"; break;
	default: /* CUSTOM */ break;
	}
	replynl(ctx, fmt::format("LINEAR_UNIT_CONVERSION {}", luc));
	return rtOk;
}

static cmdResponseType getAngularUnitConversion(connectionRecType &ctx)
{
	// GET ANGULAR_UNIT_CONVERSION
	const char *auc = "CUSTOM";
	switch (angularUnitConversion) {
	case ANGULAR_UNITS_DEG:  auc = "DEG"; break;
	case ANGULAR_UNITS_RAD:  auc = "RAD"; break;
	case ANGULAR_UNITS_GRAD: auc = "GRAD"; break;
	case ANGULAR_UNITS_AUTO: auc = "AUTO"; break;
	default: /* CUSTOM */ break;
	}
	replynl(ctx, fmt::format("ANGULAR_UNIT_CONVERSION {}", auc));
	return rtOk;
}

static cmdResponseType getProbeValue(connectionRecType &ctx)
{
	// GET PROBE_VALUE
	replynl(ctx, fmt::format("PROBE_VALUE {}", emcStatus->motion.traj.probeval));
	return rtOk;
}

static cmdResponseType getProbeTripped(connectionRecType &ctx)
{
	// GET PROBE_TRIPPED
	replynl(ctx, fmt::format("PROBE_TRIPPED {}", yesNo(emcStatus->motion.traj.probe_tripped)));
	return rtOk;
}

static cmdResponseType getTeleopEnable(connectionRecType &ctx)
{
	// GET TELEOP_ENABLE
	replynl(ctx, onOffString("TELEOP_ENABLE", emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP));
	return rtOk;
}

static cmdResponseType getKinematicsType(connectionRecType &ctx)
{
	// GET KINEMATICS_TYPE
	replynl(ctx, fmt::format("KINEMATICS_TYPE {}", emcStatus->motion.traj.kinematics_type));
	return rtOk;
}

static cmdResponseType getFeedOverride(connectionRecType &ctx)
{
	// GET FEED_OVERRIDE
	double percent = emcStatus->motion.traj.scale * 100.0;
	replynl(ctx, fmt::format("FEED_OVERRIDE {:f}", percent));
	return rtOk;
}

static cmdResponseType getIni(connectionRecType &ctx)
{
	// GET INI <var> <section>
	auto inistring = getIniVar(ctx.toks[2], ctx.toks[3]);
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
	replynl(ctx, fmt::format("INIFILE {}", emc_inifile));
	return rtOk;
}

static cmdResponseType getSpindleOverride(connectionRecType &ctx)
{
	// GET SPINDLE_OVERRIDE [spindle]
	// handle no spindle present
	if (emcStatus->motion.traj.spindles == 0) {
		errornl(ctx, "no spindles configured");
		return rtError;
	}

	int spindle = -1;
	if (ctx.toks.size() > 2 && !to_int(ctx.toks[2], spindle)) {
		errornl(ctx, fmt::format("Invalid argument '{}', must be integer", ctx.toks[2]));
		return rtError;
	}
	if (spindle < -1 || spindle >= emcStatus->motion.traj.spindles) {
		errornl(ctx, fmt::format("Invalid spindle '{}', valid range [-1,{}]", spindle, emcStatus->motion.traj.spindles - 1));
		return rtError;
	}

	// walk all spindles
	for (int n = 0; n < emcStatus->motion.traj.spindles; n++) {
		// process this spindle?
		if (n != spindle && spindle != -1)
			continue;

		double percent = emcStatus->motion.spindle[n].spindle_scale * 100.0;
		replynl(ctx, fmt::format("SPINDLE_OVERRIDE {} {}", n, percent));
	}

	return rtOk;
}

static cmdResponseType getOptionalStop(connectionRecType &ctx)
{
	// GET OPTIONAL_STOP
	replynl(ctx, onOffString("OPTIONAL_STOP", emcStatus->task.optional_stop_state));
	return rtOk;
}

static cmdResponseType gsNotImpl(connectionRecType &)
{
	return rtError;
}

static cmdResponseType gsIgnored(connectionRecType &)
{
	return rtOk;
}

// Important: this list must be sorted on the name field string.
// Note: The following are allowed to be sent, even though a
// command is being processed:
// - SET ESTOP ON
// - SET MACHINE OFF
// - SET ABORT
// There is an additional check in setEStop() and setMachine()
static const getsetListType getsetList[] = {
	{scAbort,					"ABORT",					0, 0, gsNotImpl,				setAbort,
	 alMachOn, false, "", "" },
	{scAbsActPos,				"ABS_ACT_POS",				0, 0, getAbsActPos,				gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scAbsCmdPos,				"ABS_CMD_POS",				0, 0, getAbsCmdPos,				gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scAngularUnitConversion,	"ANGULAR_UNIT_CONVERSION",	0, 1, getAngularUnitConversion,	setAngularUnitConversion,
	 alMachOn, false, "", "{deg|rad|grad|auto}" },
	{scAxisVelocity,			"AXIS_VELOCITY",			0, 0, getAxisVelocity,			gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scBrake,					"BRAKE",					0, 1, getBrake,					setBrake,
	 alMachOn, true, "[<spindle>|-1]", "{on|off} [<spindle>|-1]" },
	{scClients,					"CLIENTS",					0, 0, getClients,				gsNotImpl,
	 alHello,  false, "", "" },
	{scCommProt,				"COMM_PROT",				0, 1, getCommProt,				gsNotImpl,
	 alHello,  false,"", "" },
	{scDebug,					"DEBUG",					0, 1, getDebug,					setDebug,
	 alEnable, true, "", "<value>" },
	{scDisable,					"DISABLE",					0, 0, gsNotImpl,				setDisable,
	 alHello,  false, "", "" },
	{scDisplayAngularUnits,		"DISPLAY_ANGULAR_UNITS",	0, 0, getDisplayAngularUnits,	gsNotImpl,
	 alMachOn, false, "", "" },
	{scDisplayLinearUnits,		"DISPLAY_LINEAR_UNITS",		0, 0, getDisplayLinearUnits,	gsNotImpl,
	 alMachOn, false, "", "" },
	{scEcho,					"ECHO",						0, 1, getEcho,					setEcho,
	 alNone,   false, "", "{on|off}" },
	{scEnable,					"ENABLE",					0, 1, getEnable,				setEnable,
	 alHello,  false, "", "<password>|{off}" },
	{scError,					"ERROR",					0, 0, getError,					gsNotImpl,
	 alEnable, false, "", "" },
	{scEStop,					"ESTOP",					0, 1, getEStop,					setEStop,
	 alEnable, false, "", "{on|off}" },
	{scFeedOverride,			"FEED_OVERRIDE",			0, 1, getFeedOverride,			setFeedOverride,
	 alMachOn, true, "", "<percent>" },
	{scFlood,					"FLOOD",					0, 1, getFlood,					setFlood,
	 alMachOn, true, "", "{on|off}" },
	{scJointHome,				"HOME",						0, 1, gsNotImpl,				setJointHome, // Alias
	 alMachOn, true, "", "[<joint>|-1]" },
	{scIni,						"INI",						2, 0, getIni,					gsIgnored,
	 alHello,  false, "<var> <section>", "" },
	{scIniFile,					"INIFILE",					0, 0, getIniFile,				gsIgnored,
	 alHello,  false, "", "" },
	{scJog,						"JOG",						0, 2, gsNotImpl,				setJog,
	 alMachOn, true, "", "<joint>|<axis> <speed>" },
	{scJogIncr,					"JOG_INCR",					0, 3, gsNotImpl,				setJogIncr,
	 alMachOn, true, "", "<joint>|<axis> <speed>" },
	{scJogStop,					"JOG_STOP",					0, 1, gsNotImpl,				setJogStop,
	 alMachOn, true, "", "<joint>|<axis>" },
	{scJointFault,				"JOINT_FAULT",				0, 0, getJointFault,			gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointHome,				"JOINT_HOME",				0, 1, gsNotImpl,				setJointHome,
	 alMachOn, true, "", "[<joint>|-1]" },
	{scJointHomed,				"JOINT_HOMED",				0, 0, getJointHomed,			gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointLimit,				"JOINT_LIMIT",				0, 0, getJointLimit,			gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointPos,				"JOINT_POS",				0, 0, getJointPos,				gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointType,				"JOINT_TYPE",				0, 0, getJointType,				gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointUnhome,				"JOINT_UNHOME",				0, 1, gsNotImpl,				setJointUnhome,
	 alMachOn, true, "", "[<joint>-1]" },
	{scJointUnits,				"JOINT_UNITS",				0, 0, getJointUnits,			gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointVelocity,			"JOINT_VELOCITY",			0, 0, getJointVelocity,			gsNotImpl,
	 alMachOn, false, "[<joint>|-1]", "" },
	{scJointWaitHomed,			"JOINT_WAIT_HOMED",			0, 1, gsNotImpl,				setJointWaitHomed,
	 alMachOn, false, "", "[<joint>|-1] [<timeout>]" },
	{scKinematicsType,			"KINEMATICS_TYPE",			0, 0, getKinematicsType,		gsNotImpl,
	 alMachOn, false, "", "" },
	{scLinearUnitConversion,	"LINEAR_UNIT_CONVERSION",	0, 1, getLinearUnitConversion,	setLinearUnitConversion,
	 alMachOn, false, "", "{inch|mm|cm|auto}" },
	{scLoadToolTable,			"LOAD_TOOL_TABLE",			0, 1, gsNotImpl,				setLoadToolTable,
	 alMachOn, true, "", "<filename>" },
	{scMachine,					"MACHINE",					0, 1, getMachine,				setMachine,
	 alEnable, false, "", "{on|off}" },
	{scMDI,						"MDI",						0, 1, gsNotImpl,				setMDI,
	 alMachOn, true, "", "<string>" },
	{scMist,					"MIST",						0, 1, getMist,					setMist,
	 alMachOn, true, "", "{on|off}" },
	{scMode,					"MODE",						0, 1, getMode,					setMode,
	 alMachOn, true, "", "{manual|auto|mdi}" },
	{scOpen,					"OPEN",						0, 1, getOpen,					setOpen,
	 alMachOn, true, "", "<filename>" },
	{scOperatorDisplay,			"OPERATOR_DISPLAY",			0, 0, getOperatorDisplay,		gsNotImpl,
	 alEnable, false, "", "" },
	{scOperatorText,			"OPERATOR_TEXT",			0, 0, getOperatorText,			gsNotImpl,
	 alEnable, false, "", "" },
	{scOptionalStop,			"OPTIONAL_STOP",			0, 1, getOptionalStop,			setOptionalStop,
	 alMachOn, true, "", "{on|off}" },
	{scOverrideLimits,			"OVERRIDE_LIMITS",			0, 1, getOverrideLimits,		setOverrideLimits,
	 alMachOn, true, "", "{on|off}" },
	{scPause,					"PAUSE",					0, 0, gsNotImpl,				setPause,
	 alMachOn, true, "", "" },
	{scPlat,					"PLAT",						0, 0, getPlat,					gsNotImpl,
	 alHello,  false, "", "" },
	{scPosOffset,				"POS_OFFSET",				0, 0, getPosOffset,				gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scProbe,					"PROBE",					0, 3, gsIgnored,				setProbe,
	 alMachOn, true, "", "<x> <y> <z>" },
	{scProbeClear,				"PROBE_CLEAR",				0, 0, gsIgnored,				setProbeClear,
	 alMachOn, true, "", "" },
	{scProbeTripped,			"PROBE_TRIPPED",			0, 0, getProbeTripped,			gsNotImpl,
	 alMachOn, false, "", "" },
	{scProbeValue,				"PROBE_VALUE",				0, 0, getProbeValue,			gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgram,					"PROGRAM",					0, 0, getProgram,				gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramAngularUnits,		"PROGRAM_ANGULAR_UNITS",	0, 0, getProgramAngularUnits,   gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramCodes,			"PROGRAM_CODES",			0, 0, getProgramCodes,			gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramLine,				"PROGRAM_LINE",				0, 0, getProgramLine,			gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramLinearUnits,		"PROGRAM_LINEAR_UNITS",		0, 0, getProgramLinearUnits,	gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramStatus,			"PROGRAM_STATUS",			0, 0, getProgramStatus,			gsNotImpl,
	 alMachOn, false, "", "" },
	{scProgramUnits,			"PROGRAM_UNITS",			0, 0, getProgramLinearUnits,	gsNotImpl,
	 alMachOn, false, "", "" }, // Alias
	{scRelActPos,				"REL_ACT_POS",				0, 0, getRelActPos,				gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scRelCmdPos,				"REL_CMD_POS",				0, 0, getRelCmdPos,				gsNotImpl,
	 alMachOn, false, "[<axis>|-1]", "" },
	{scResume,					"RESUME",					0, 0, gsNotImpl,				setResume,
	 alMachOn, true, "", "" },
	{scRun,						"RUN",						0, 0, gsNotImpl,				setRun,
	 alMachOn, true, "", "[<startline>]" },
	{scSpindle,					"SPINDLE",					0, 1, getSpindle,				setSpindle,
	 alMachOn, true, "[<spindle>|-1]", "{forward|reverse|increase|decrease|constant|off} [<spindle>|-1]"},
	{scSpindleOverride,			"SPINDLE_OVERRIDE",			0, 2, getSpindleOverride,		setSpindleOverride,
	 alMachOn, true, "", "<percent> [<spindle>|-1]" },
	{scStep,					"STEP",						0, 0, gsNotImpl,				setStep,
	 alMachOn, true, "", "" },
	{scTaskPlanInit,			"TASK_PLAN_INIT",			0, 0, gsNotImpl,				setTaskPlanInit,
	 alMachOn, true, "", "" },
	{scTeleopEnable,			"TELEOP_ENABLE",			0, 1, getTeleopEnable,			setTeleopEnable,
	 alMachOn, true, "", "{on|off}" },
	{scTime,					"TIME",						0, 0, getTime,					gsNotImpl,
	 alHello,  false, "", "" },
	{scTimeout,					"TIMEOUT",					0, 1, getTimeout,				setTimeout,
	 alEnable, false, "", "<timeout>" },
	{scTimestamp,				"TIMESTAMP",				0, 1, getTimestamp,				setTimestamp,
	 alNone,   false, "", "{on|off} [{on|off}]" },
	{scTool,					"TOOL",						0, 0, getTool,					gsNotImpl,
	 alMachOn, false, "", "" },
	{scToolOffset,				"TOOL_OFFSET",				0, 3, getToolOffset,			setToolOffset,
	 alMachOn, true, "", "<tool> <length> <diameter>" },
	{scUpdate,					"UPDATE",					0, 1, getUpdate,				setUpdate,
	 alEnable, false, "", "{none|auto}" },
	{scUpdateStatus,			"UPDATE_STATUS",			0, 0, gsNotImpl,				setUpdateStatus,
	 alEnable, false, "", "" },
	{scUserAngularUnits,		"USER_ANGULAR_UNITS",		0, 0, getUserAngularUnits,		gsNotImpl,
	 alMachOn, false, "", "" },
	{scUserLinearUnits,			"USER_LINEAR_UNITS",		0, 0, getUserLinearUnits,		gsNotImpl,
	 alMachOn, false, "", "" },
	{scVerbose,					"VERBOSE",					0, 1, getVerbose,				setVerbose,
	 alHello,  false, "", "{on|off}" },
	{scWait,					"WAIT",						0, 1, gsIgnored,				setWait,
	 alEnable, false, "", "{received|done}" },
	{scWaitHeartbeat,			"WAIT_HEARTBEAT",			0, 0, gsIgnored,				setWaitHeartbeat,
	 alMachOn, false, "", "<periods>" },
	{scWaitMode,				"WAIT_MODE",				0, 1, getWaitMode,				setWaitMode,
	 alEnable, false, "", "{received|done}" },
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

	// Update status for both GET and SET. GET only displays data, but some SET
	// commands use the emcStatus for decisions. Better make it up-to-date.
	// We don't care about the setting. We need it, very much.
	// if (emcUpdateType == EMC_UPDATE_AUTO)
	updateStatus();

	if (getset == cmdGet) {
		// GET X command
		// Check for enough arguments
		if (tag->nget + 2 > ctx.toks.size()) {
			errornl(
				ctx,
				fmt::format("Too few arguments to GET {}, have {}, need {} or more", tag->name, ctx.toks.size() - 2, tag->nget));
			replynl(ctx, fmt::format("GET {} NAK", tag->name));
			return -1;
		}
		cmdResponseType res = tag->getter(ctx);
		switch (res) {
		case rtOk: break;
		case rtError: // Standard error response
			replynl(ctx, fmt::format("GET {} NAK", tag->name));
			break;
		default: replynl(ctx, fmt::format("internal: GET {} returned unknown value '{}'", tag->name, (int)res)); break;
		}
	} else {
		// SET X command
		if (setterActive() && tag->remote) {
			errornl(ctx, fmt::format("Cannot issue SET {} command while SET {} command is still active",
							tag->name, activeSetter));
			return -1;
		}

		// Check for enough arguments
		if (tag->nset + 2 > ctx.toks.size()) {
			errornl(
				ctx,
				fmt::format("Too few arguments to SET {}, have {}, need {} or more", tag->name, ctx.toks.size() - 2, tag->nset));
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

		if (tag->acclvl >= alMachOn && emcStatus->task.state != EMC_TASK_STATE::ON) {
			// Extra check in the event of an undetected change in Machine state
			// resulting in sending a set command when the machine state is off.
			// This condition is detected and appropriate error messages are
			// generated, however erratic behavior has been seen when doing certain
			// set commands when the Machine state is other than 'ON'
			errornl(ctx, fmt::format("Machine must be 'ON' for SET {}", tag->name));
			replynl(ctx, fmt::format("SET {} NAK", tag->name));
			return -1;
		}

		cmdResponseType res = tag->setter(ctx);
		switch (res) {
		case rtOk:
			if(tag->remote) {
				setterSet(ctx, tag->name);
				// Replying ACK/NAK is postponed until DONE
				// See sockMain() for handling below.
			} else if (ctx.verbose) {
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
		// A shutdown will disconnect all clients from linuxcncrsh
		info("Shutdown initiated by '%s' (%d)", ctx.hostname.c_str(), ctx.sock);
		doDisable(ctx);
		// This works fine when linuxcncrsh is the DISPLAY program in the INI
		// file because the linuxcnc wrapper script will execute it and we
		// terminate after a shutdown.
		// However, this does /not/ work when you have another DISPLAY program
		// and run linuxcncrsh on the side.
		// FIXME: There is currently no infrastructure to remotely shutdown the
		// whole LinuxCNC stack of programs if the DISPLAY program is anything
		// else than linuxcncrsh.
		replynl(ctx, "SHUTDOWN ACK");
		return 0;
	}

	errornl(ctx, "SHUTDOWN failed, not enabled");
	replynl(ctx, "SHUTDOWN NAK");
	return -1;
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
			"Usage: HELLO <password> <clientname> <protover>\r\n"
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
		if (gsl->getter == gsNotImpl || gsl->getter == gsIgnored)
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
			"  Additionally, some set commands require the machine to be in the ON state.\r\n"
			"  Set commands not requiring any access level control:");
	for (unsigned i = 0; i < NELEM(getsetList); i++) {
		const getsetListType *gsl = &getsetList[i];
		if (gsl->setter == gsNotImpl || gsl->setter == gsIgnored || gsl->acclvl != alNone)
			continue;
		replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
	}
	reply(ctx, "  Set commands requiring HELLO level control:");
	reply(ctx, "\r\n");
	for (unsigned i = 0; i < NELEM(getsetList); i++) {
		const getsetListType *gsl = &getsetList[i];
		if (gsl->setter == gsNotImpl || gsl->setter == gsIgnored || gsl->acclvl != alHello)
			continue;
		replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
	}
	reply(ctx, "\r\n");
	replynl(ctx, "  Set commands requiring control enabled:");
	for (unsigned i = 0; i < NELEM(getsetList); i++) {
		const getsetListType *gsl = &getsetList[i];
		if (gsl->setter == gsNotImpl || gsl->setter == gsIgnored || gsl->acclvl != alEnable)
			continue;
		replynl(ctx, fmt::format("    {} {}", gsl->name, gsl->sethelp));
	}
	reply(ctx, "\r\n");
	replynl(ctx, "  Set commands requiring control enabled and machine in the ON state:");
	for (unsigned i = 0; i < NELEM(getsetList); i++) {
		const getsetListType *gsl = &getsetList[i];
		if (gsl->setter == gsNotImpl || gsl->setter == gsIgnored || gsl->acclvl != alMachOn)
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
			"  The shutdown command terminates the connections with all clients\r\n"
			"  and initiates a shutdown of LinuxCNC if it is set as the DISPLAY\r\n"
			"  application in the INI file.\r\n"
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

static int parseCommand(connectionRecType &ctx, std::string &line)
{
	static const char toksep[] = " \t\n\r";

	size_t pos = 0;
	ctx.toks.clear(); // Clear any previous commands/tokens
	ctx.third.clear();
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
		if (2 == ctx.toks.size()) {
			// Save the rest of the string as possible filename
			ctx.third = line.substr(lft);
		}
		ctx.toks.push_back(std::move(line.substr(lft, rgt - lft)));
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

static bool setterDoneUpdate(connectionRecType &ctx)
{
	double now = etime();
	if (ctx.cmdtimeout < now) {
		// Timeout
		errornl(ctx, fmt::format("command 'SET {}' timed out", activeSetter));
		return true;
	}

	updateStatus();
	int serial_diff = emcStatus->echo_serial_number - ctx.serial;

	if (serial_diff > 0) { // We've past beyond our command
		return true;
	}

	if (!serial_diff) { // We're at our command
		if (ctx.waitmode == EMC_WAIT_RECEIVED) {
			return true;
		}

		switch (emcStatus->status) {
		case RCS_STATUS::EXEC: // Still busy executing command
			break;

		case RCS_STATUS::ERROR: // The command failed
		case RCS_STATUS::DONE:	// The command finished
			return true;

		default: // Default should never happen...
			error("setterDoneUpdate(): unknown emcStatus->status=%d", (int)emcStatus->status);
			return false;
		}
	}
	return false;
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

	while (clients.size() || !shutdown) {
		// Make sure we have room for clients and server socket
		pfds.resize(clients.size() + 1);

		// Setup poll data for all sockets
		pfds[0].fd = svrfd;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;
		int pto = -1;	// Default to infinite poll timeout
		if (shutdown)
			pto = 0;	// Immediate return in shutdown mode
		else if (setterActive())
			pto = polltimeout;	// Timeout to count

		// Add progressive waiting
		polltimeout *= 2;
		if(polltimeout > POLLTIMEOUT_MAX)
			polltimeout = POLLTIMEOUT_MAX;

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
			// But only test for clients not waiting.
			if (clients[i].cmdtimeout <= 0.0 && clients[i].inbuf.find_first_of("\r\n") != std::string::npos)
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
			cfd = accept(svrfd, reinterpret_cast<struct sockaddr *>(&csa), &csal);
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
					esleep(1); // Don't busy loop waiting for the net to be up again
					break;
				// Various restartable errors
				case ETIMEDOUT: // Linux kernel thingy, apparently
				case ERESTART:	// During trace (in accept(2) manual named ERESTARTSYS)
					break;
				default:
					// Any other error will be fatal
					xperror("accept()");
					return EXIT_FAILURE;
				}
			} else {
				if (!maxSessions || clients.size() < maxSessions) {
					// Enter non-blocking mode and disable Nagle's algo
					set_nonblock(cfd);
					int nd = 1;
					setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
					connectionRecType cr = {};
					cr.sock = cfd;
					cr.hostname = fmt::format("Default-{}", cfd);
					cr.version = "1.1";
					cr.echo = true;
					cr.waitmode = EMC_WAIT_DONE;
					//cr.timestamp= true;
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
			// See if there is an active command outstanding
			if (setterActive() && i->cmdtimeout > 0.0) {
				// This client has issued a setter and we're waiting
				if (!setterDoneUpdate(*i))
					continue;	// This client is still waiting
				// Done, now we know what to reply
				if (emcStatus->status == RCS_STATUS::ERROR) {
					replynl(*i, fmt::format("SET {} NAK", activeSetter));
				} else if (i->verbose) {
					replynl(*i, fmt::format("SET {} ACK", activeSetter));
				}
				setterClear(*i);
			}

			// Non-waiting connections may proceed unimpeded. They cannot
			// execute SET commands that will queue if there is an active SET
			// command and will be informed if they try to.

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

// Default: always search "${HOME}/nc_files/"
static std::string getDefaultPath()
{
	std::string defpath;
	if (const char *home = getenv("HOME")) {
		defpath = home;
	}
	return defpath + "/nc_files";
}

static void usage(char *pname)
{
	printf("Usage: \n"
		   "  %s [Options] [-- LinuxCNC_Options]\n"
		   "Options:\n"
		   "   -h,--help             This help\n"
		   "   -p,--port <num>       Listen on port 'num' (default=%d)\n"
		   "   -n,--name <str>       Set this server's name to 'str' (default=%s)\n"
		   "   -w,--connectpw <pwd>  Set connect/hello password to 'pwd' (default=%s)\n"
		   "   -e,--enablepw <pwd>   Set enable password to 'pwd' (default=%s)\n"
		   "   -s,--sessions <num>   Restrict number of session to 'num' (default=%u) (0 for no limit)\n"
		   "   -d,--path <path>      Set (colon-separated) search path to 'path' (default=%s)\n"
		   "   -q,--quiet            Don't print informational messages\n"
		   "LinuxCNC_Options:\n"
		   "   -ini <INI file>       (default=%s)\n",
		   pname,
		   port,
		   serverName.c_str(),
		   helloPwd.c_str(),
		   enablePWD.c_str(),
		   maxSessions,
		   getDefaultPath().c_str(),
		   emc_inifile);
}

static bool check_white(const std::string &s)
{
	for (const char &c : s) {
		if (std::iscntrl((unsigned char)c) || std::isspace((unsigned char)c))
			return true;
	}
	return false;
}

int main(int argc, char *argv[])
{
	int opt;
	int lose = 0;
	char *eptr;
	size_t pos;
	std::string s;

	// initialize default values
	emcWaitType = EMC_WAIT_NEVER;	// Handled in the poll loop
	emcCommandSerialNumber = 0;
	emcTimeout = 0.0;
	emcUpdateType = EMC_UPDATE_AUTO;	// Ignored
	linearUnitConversion = LINEAR_UNITS_AUTO;
	angularUnitConversion = ANGULAR_UNITS_AUTO;
	emcCommandBuffer = NULL;
	emcStatusBuffer = NULL;
	emcErrorBuffer = NULL;
	emcStatus = NULL;
	// FIXME: Get rid of static buffers
	memset(error_string, 0, sizeof(error_string));
	memset(operator_text_string, 0, sizeof(operator_text_string));
	memset(operator_display_string, 0, sizeof(operator_display_string));
	programStartLine = 0;

	searchPath.push_back(getDefaultPath());

	// process local command line args
	while ((opt = getopt_long(argc, argv, "he:n:p:s:w:d:q", longopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return EXIT_FAILURE;
		case 'e':
			enablePWD = optarg;
			if (!compareNoCase(enablePWD, "OFF")) {
				error("Invalid enable password. You cannot use 'OFF' as enable password");
				lose++;
			} else {
				if (check_white(enablePWD)) {
					error("You cannot have control or space characters in the enable password");
					lose++;
				}
			}
			break;
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
		case 'd':
			searchPath.clear(); // Remove the default
			s = optarg;
			if (s.back() == ':') {
				error("Trailing ':' in path ignored");
			}
			pos = 0;
			while (pos < s.size()) {
				size_t colon = s.find_first_of(':', pos);
				if (std::string::npos == colon) {
					// No colon found. Pretend it was there at the end.
					colon = s.size();
				}
				std::string tmp = s.substr(pos, colon - pos);
				if (!tmp.size()) {
					error("Empty search path entry ignored");
				} else {
					if (tmp.front() != '/') {
						error("'%s' not an absolute path. May not be useful.", tmp.c_str());
					}
					searchPath.push_back(std::move(tmp));
				}
				pos = colon + 1;
			}
			break;
		case 'p':
			port = strtol(optarg, &eptr, 0);
			if (eptr == optarg || *eptr || port <= 0 || port >= 65535) {
				error("Invalid port '%s', must be integer in range [1,65534]", optarg);
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
		default:
			lose++;
			break;
		}
	}

	if (lose)
		return EXIT_FAILURE;

	// process LinuxCNC command line args
	// Note: '--' may be used to separate cmd line args
	//       optind is index of next arg to process
	//       make argv[optind] zeroth arg
	argc = argc - optind + 1;
	argv = argv + optind - 1;
	if (emcGetArgs(argc, argv) != 0) {
		error("Error in emc argument list");
		exit(EXIT_FAILURE);
	}
	// get configuration information
	iniLoad(emc_inifile);
	// initialize telnet socket
	int sockfd = initSocket();
	if (sockfd < 0) {
		exit(EXIT_FAILURE);
	}
	// init NML
	if (tryNml() != 0) {
		error("Can't connect to LinuxCNC");
		cleanup();
		exit(EXIT_FAILURE);
	}
	// get current serial number, and save it for restoring when we quit
	// so as not to interfere with real operator interface
	updateStatus();
	emcCommandSerialNumber = emcStatus->echo_serial_number;

	// Attach our quit function
	signal(SIGINT, sigQuit);
	signal(SIGQUIT, sigQuit);
	signal(SIGTERM, sigQuit);

	int rv = sockMain(sockfd);
	cleanup();
	return rv;
}

// vim: ts=4 sw=4
