/********************************************************************
* Description:  units.h
*               Unit conversion macros and constants
*
* Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
*    
* Copyright (c) 2005 All rights reserved.
*
* Last change: 
********************************************************************/

/* macros for converting internal (mm/deg) units to external units */
#define TO_EXT_LEN(mm) ((mm) * GET_EXTERNAL_LENGTH_UNITS())
#define TO_EXT_ANG(deg) ((deg) * GET_EXTERNAL_ANGLE_UNITS())

/* macros for converting external units to internal (mm/deg) units */
#define FROM_EXT_LEN(ext) ((ext) / GET_EXTERNAL_LENGTH_UNITS())
#define FROM_EXT_ANG(ext) ((ext) / GET_EXTERNAL_ANGLE_UNITS())

/* macros for converting internal (mm/deg) units to program units */
#define TO_PROG_LEN(mm) ((mm) / (_setup.length_units == CANON_UNITS_INCHES ? 25.4 : _setup.length_units == CANON_UNITS_CM ? 10.0 : 1.0))
#define TO_PROG_ANG(deg) (deg)

/* macros for converting program units to internal (mm/deg) units */
#define FROM_PROG_LEN(prog) ((prog) * (_setup.length_units == CANON_UNITS_INCHES ? 25.4 : _setup.length_units == CANON_UNITS_CM ? 10.0 : 1.0))
#define FROM_PROG_ANG(prog) (prog)

/* macros for converting between user units (ini file) and program units (g code) */
#define USER_TO_PROGRAM_LEN(u) (TO_PROG_LEN(FROM_EXT_LEN(u)))
#define PROGRAM_TO_USER_LEN(p) (TO_EXT_LEN(FROM_PROG_LEN(p)))

#define USER_TO_PROGRAM_ANG(u) (TO_PROG_ANG(FROM_EXT_ANG(u)))
#define PROGRAM_TO_USER_ANG(p) (TO_EXT_ANG(FROM_PROG_ANG(p)))


