/********************************************************************
* Description: rs274ngc.hh
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef RS274NGC_HH
#define RS274NGC_HH

/**********************/
/* INCLUDE DIRECTIVES */
/**********************/

#include <stdio.h>
#include "interp_internal.hh"
#include "emc.hh"
#include "canon.hh"

typedef setup *setup_pointer;
// pointer to function that reads
typedef int (*read_function_pointer) (char *, int *, block_pointer, double *);

extern const int _gees[];
extern const int _ems[];
extern const int _required_parameters[];

extern const read_function_pointer _readers[];
extern setup _setup;

/*************************************************************************/
/*

Interface functions to call to tell the interpreter what to do.
Return values indicate status of execution.
These functions may change the state of the interpreter.

*/

// close the currently open NC code file
extern int rs274ngc_close();

// execute a line of NC code
#ifndef NOT_OLD_EMC_INTERP_COMPATIBLE
extern int rs274ngc_execute(const char *command = 0);
#else
extern int rs274ngc_execute();
#endif

// stop running
extern int rs274ngc_exit();

// get ready to run
extern int rs274ngc_init();

// load a tool table
extern int rs274ngc_load_tool_table();

// open a file of NC code
extern int rs274ngc_open(const char *filename);

// read the mdi or the next line of the open NC code file
extern int rs274ngc_read(const char *mdi = 0);

// reset yourself
extern int rs274ngc_reset();

// restore interpreter variables from a file
extern int rs274ngc_restore_parameters(const char *filename);

// save interpreter variables to file
extern int rs274ngc_save_parameters(const char *filename,
                                    const double parameters[]);

// synchronize your internal model with the external world
extern int rs274ngc_synch();


/*************************************************************************/
/*

Interface functions to call to get information from the interpreter.
If a function has a return value, the return value contains the information.
If a function returns nothing, information is copied into one of the
arguments to the function. These functions do not change the state of
the interpreter.

*/

// copy active G codes into array [0]..[11]
extern void rs274ngc_active_g_codes(int *codes);

// copy active M codes into array [0]..[6]
extern void rs274ngc_active_m_codes(int *codes);

// copy active F, S settings into array [0]..[2]
extern void rs274ngc_active_settings(double *settings);

// copy the text of the error message whose number is error_code into the
// error_text array, but stop at max_size if the text is longer.
extern void rs274ngc_error_text(int error_code, char *error_text,
                                int max_size);

// copy the name of the currently open file into the file_name array,
// but stop at max_size if the name is longer
extern void rs274ngc_file_name(char *file_name, int max_size);

// return the length of the most recently read line
extern int rs274ngc_line_length();

// copy the text of the most recently read line into the line_text array,
// but stop at max_size if the text is longer
extern void rs274ngc_line_text(char *line_text, int max_size);

// return the current sequence number (how many lines read)
extern int rs274ngc_sequence_number();

// copy the function name from the stack_index'th position of the
// function call stack at the time of the most recent error into
// the function name string, but stop at max_size if the name is longer
extern void rs274ngc_stack_name(int stack_index, char *function_name,
                                int max_size);

// Get the parameter file name from the ini file.
extern int rs274ngc_ini_load(const char *filename);

static inline int rs274ngc_line()
{
  return rs274ngc_sequence_number();
}

static inline const char *rs274ngc_command()
{
  static char buf[100];
  rs274ngc_line_text(buf, 100);
  return buf;
}

static inline const char *rs274ngc_file()
{
  static char buf[100];
  rs274ngc_file_name(buf, 100);
  return buf;
}

#endif
