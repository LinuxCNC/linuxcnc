#include "setup_signals.h"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>


int setup_signals(const sa_sigaction_t handler, ...)
{
    sigset_t sigmask;
    sigfillset(&sigmask);

    // SIGSEGV,SIGBUS,SIGILL,SIGFPE delivered via sigaction if handler given
    if (handler != NULL) {
	struct sigaction sig_act;
	sigemptyset( &sig_act.sa_mask );
	sig_act.sa_sigaction = handler;
	sig_act.sa_flags   = SA_SIGINFO;
	sigaction(SIGSEGV, &sig_act, (struct sigaction *) NULL);
	sigaction(SIGBUS,  &sig_act, (struct sigaction *) NULL);
	sigaction(SIGILL,  &sig_act, (struct sigaction *) NULL);
	sigaction(SIGFPE,  &sig_act, (struct sigaction *) NULL);

	// if they go through sigaction, block delivery through normal handler
	sigdelset(&sigmask, SIGSEGV);
	sigdelset(&sigmask, SIGBUS);
	sigdelset(&sigmask, SIGILL);
	sigdelset(&sigmask, SIGFPE);
	if (sigprocmask(SIG_SETMASK, &sigmask, NULL) == -1)
	    perror("sigprocmask");
    } // else fail miserably, the default way: dump core

    // now explicitly turn on the signals delivered via  signalfd()
    // sigset of all the signals that we're interested in
    // these we want delivered via signalfd()
    int retval;
    retval = sigemptyset(&sigmask);        assert(retval == 0);

    va_list ap;
    int signo;
    va_start(ap, handler);
    do {
	signo = va_arg(ap, int);
	if (signo < 0 ) break;
	retval = sigaddset(&sigmask, signo);  assert(retval == 0);
    } while (1);
    va_end(ap);
    return signalfd(-1, &sigmask, 0);
}
