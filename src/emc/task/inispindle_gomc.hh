/********************************************************************
* Description: inispindle_gomc.hh
*   gomc variant: takes gomc_ini_t instead of filename.
*
* License: GPL Version 2
********************************************************************/
#ifndef INISPINDLE_GOMC_HH
#define INISPINDLE_GOMC_HH

#include "launcher/pkg/cmodule/gomc_ini.h"
#include "emc.hh"

/* initializes spindle modules from gomc INI handle */
extern int iniSpindle(int spindle, const gomc_ini_t *ini);

#endif
