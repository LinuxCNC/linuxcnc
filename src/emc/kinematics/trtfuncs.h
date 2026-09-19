/********************************************************************
* Description: trtfuncs.h
*   The two table-rotary tilting geometries, xyzac and xyzbc, written
*   once in trtfuncs.c for the two modules that load them.
*
* License: GPL Version 2
********************************************************************/
#ifndef __LINUXCNC_TRTFUNCS_H
#define __LINUXCNC_TRTFUNCS_H

#include "kins_module.h"


// xyzac,xyzbc (trtfuncs.c): one geometry table, the maths of each machine
extern const kins_param_desc TRT_PARAMS[];
extern const int TRT_NPARAMS;
extern const kins_ops XYZAC_OPS;
extern const kins_ops XYZBC_OPS;

#endif
