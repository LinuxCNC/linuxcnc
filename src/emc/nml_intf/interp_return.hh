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

#define INTERP_OK 0
#define INTERP_EXIT 1
#define INTERP_EXECUTE_FINISH 2
#define INTERP_ENDFILE 3
#define INTERP_FILE_NOT_OPEN 4
#define INTERP_ERROR 5

/*
The return values OK, EXIT, EXECUTE_FINISH, and ENDFILE represent
normal, non-error return conditions. FILE_NOT_OPEN is the first
value that represents an error result. INTERP_MIN_ERROR
is therefore the index of the last non-error return value.
*/

#define INTERP_MIN_ERROR 3

#endif				/* INTERP_RETURN_H */
