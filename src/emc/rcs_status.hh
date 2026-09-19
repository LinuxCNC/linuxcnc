/********************************************************************
 * Description: rcs_status.hh
 *   Execution status of a command, as reported back by task and IO.
 *
 *   This enum was originally declared in libnml's <rcs/rcs.hh>, which
 *   is otherwise nothing but forward declarations. It carries no NML
 *   machinery of its own, so it lives here; libnml includes this header
 *   for RCS_STAT_MSG::status, and goes away without taking it along.
 *
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef LINUXCNC_RCS_STATUS_HH
#define LINUXCNC_RCS_STATUS_HH

enum class RCS_STATUS : int {           /* Originally from nml_mod.hh */
    UNINITIALIZED = -1,
    DONE = 1,
    EXEC = 2,
    ERROR = 3
};

#endif
