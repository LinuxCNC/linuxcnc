// keep linker happy so TaskMod can be resolved

#include "rcs.hh"		// NML classes, nmlErrorFormat()
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"
#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"

EMC_STAT *emcStatus = new EMC_STAT;

// EMC_IO_STAT *emcIoStatus = new EMC_IO_STAT;

int emcOperatorDisplay(int, char const*, ...) {return 0;};

int emcOperatorText(int, char const*, ...) {return 0;}

// int emcOperatorError(int, char const*, ...) {return 0;}


int emcAbortCleanup(int reason, const char *message)
{
    printf("on_abort: [%d] %s\n", reason,message);
    return 0;
}

extern void emctask_quit(int sig) {};
