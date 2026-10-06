/********************************************************************
 * Description: interpl.cc
 *   Definition of the global interpreter list.
 *
 *   The list itself is a header-only template in interpl.hh; the tie to
 *   the NML message classes lives in interpl_nml.hh.
 *
 *   Derived from a work by Fred Proctor & Will Shackleford
 *
 * Author:
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2004 All rights reserved.
 ********************************************************************/

#include "interpl_nml.hh"

NML_INTERP_LIST interp_list;
