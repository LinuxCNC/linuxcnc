// keep linker happy so TaskMod can be resolved

#include "rcs.hh"		// NML classes, nmlErrorFormat()
#include "emc.hh"		// EMC NML
#include "emc_nml.hh"

static EMC_STAT dummy_emcstat;
EMC_STAT *emcStatus = &dummy_emcstat;
