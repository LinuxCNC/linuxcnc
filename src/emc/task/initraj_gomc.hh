/********************************************************************
* Description: initraj_gomc.hh
*   gomc variant: takes gomc_ini_t instead of filename.
*
* License: GPL Version 2
********************************************************************/
#ifndef INITRAJ_GOMC_HH
#define INITRAJ_GOMC_HH

#include "launcher/pkg/cmodule/gomc_ini.h"

/* initializes traj module from gomc INI handle */
extern int iniTraj(const gomc_ini_t *ini);

#endif
