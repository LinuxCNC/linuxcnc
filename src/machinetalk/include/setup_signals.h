#ifndef _SETUP_SIGNALS_H
#define _SETUP_SIGNALS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <sys/signalfd.h>

    typedef void (*sa_sigaction_t)(int sig, siginfo_t *si, void *uctx);

    // setup signal disposition via signalfd for a list of signals
    // the signal list MUST terminated by -1
    // optionally arm a sa_sigaction type handler for SIGSEGV/SIGILL/SIGFPE/SIGBUS,
    // which cannot be delivered via signalfd
    // if handler is NULL, use default action - dump core

    int setup_signals(const sa_sigaction_t handler, ...);

#if 0
    // sa_sigaction_t handler example:
    static void handler(int sig, siginfo_t *si, void *uctx)
    {
	fprintf(stderr, "signal %d - '%s' received, dumping core (current dir=%s)",
		sig, strsignal(sig), get_current_dir_name());
	closelog_async(); // let syslog drain
	sleep(1);
	sleep(1);
	// reset handler for current signal to default
	signal(sig, SIG_DFL);
	// and re-raise so we get a proper core dump and stacktrace
	kill(getpid(), sig);
	sleep(1);
    }
#endif

#ifdef __cplusplus
}
#endif
#endif //_SETUP_SIGNALS_H
