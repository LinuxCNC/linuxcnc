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
* $Revision$
* $Author$
* $Date$
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

/*
The return values OK, EXIT, EXECUTE_FINISH, and ENDFILE represent
normal, non-error return conditions. FILE_NOT_OPEN is the first
(of perhaps many) value that represents an error result. INTERP_MIN_ERROR
is therefore the index of the last non-error return value.
*/

#define INTERP_MIN_ERROR 3

/*
  Modification history:

  $Log$
  Revision 1.2  2005/07/08 14:11:12  yabosukz
  fix some more bugz

  Revision 1.1  2005/06/14 05:12:33  mshaver
  Adding interp.hh and interp_return.hh which contain the declarations of interpreter public functions and interpreter public return code values respectively

*/

#endif				/* INTERP_RETURN_H */
