/********************************************************************
* Description: rcs_exit.hh
*   This module provides a portable way to make sure multiple
*   functions are called before exiting.
*   These functions should be written to take an int  and return void.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
#ifndef RCS_EXIT_HH
#define RCS_EXIT_HH

#ifdef __cplusplus
extern "C" {
#endif

    int attach_rcs_exit_list(void (*fptr) (int));
    void rcs_cleanup(int code);
    void rcs_exit(int code);

#ifdef __cplusplus
}
#endif
#endif
