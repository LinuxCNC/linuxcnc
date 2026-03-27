/********************************************************************
* Description: inijoint_gomc.hh
*   gomc variant: takes gomc_ini_t instead of filename.
*
* License: GPL Version 2
********************************************************************/
#ifndef INIJOINT_GOMC_HH
#define INIJOINT_GOMC_HH

#include "launcher/pkg/cmodule/gomc_ini.h"

/* initializes joint modules from gomc INI handle */
extern int iniJoint(int joint, const gomc_ini_t *ini);

#endif
