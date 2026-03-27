/********************************************************************
* Description: iniaxis_gomc.hh
*   gomc variant: takes gomc_ini_t instead of filename.
*
* License: GPL Version 2
********************************************************************/
#ifndef INIAXIS_GOMC_HH
#define INIAXIS_GOMC_HH

#include "launcher/pkg/cmodule/gomc_ini.h"
#include "emc.hh"

/* initializes axis modules from gomc INI handle */
extern int iniAxis(int axis, const gomc_ini_t *ini);

extern double ext_offset_a_or_v_ratio[];

#endif
