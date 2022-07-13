/********************************************************************
* Description: interp_return.hh
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2005 All rights reserved.
*
* Last change:
*
* This file declares the public interpreter return values. An
* interpreter may extend this list with return values that are
* used internally within the interpreters own code, but these
* constitute the minimum set.
********************************************************************/
#ifndef INTERP_RETURN_H
#define INTERP_RETURN_H

enum InterpReturn {
    INTERP_OK = 0,
    INTERP_EXIT = 1,
    INTERP_EXECUTE_FINISH = 2,
    INTERP_ENDFILE = 3,
    INTERP_FILE_NOT_OPEN = 4,
    INTERP_ERROR = 5,
};

/*
The return values OK, EXIT, EXECUTE_FINISH, and ENDFILE represent
normal, non-error return conditions. FILE_NOT_OPEN is the first
value that represents an error result. INTERP_MIN_ERROR
is therefore the index of the last non-error return value.
*/

static const InterpReturn INTERP_MIN_ERROR = INTERP_ENDFILE;

#endif				/* INTERP_RETURN_H */
