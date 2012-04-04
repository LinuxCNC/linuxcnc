/********************************************************************
* Description: driver.cc
*   Drives the interpreter from a menu based user interface.
*
* 13-Oct-2000 WPS changed gets to fgets and moved external canon variable
* definitions to canon.hh. (This may be temporary.)
* Early March 2007 MGS adapted this to emc2
*
*   Derived from a work by Tom Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2007 All rights reserved.
*
* Last change:
*
********************************************************************/

#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include "rs274ngc_return.hh"
#include "inifile.hh"		// INIFILE
#include "canon.hh"		// _parameter_file_name
#include "config.h"		// LINELEN
#include "tool_parse.h"
#include <stdio.h>    /* gets, etc. */
#include <stdlib.h>   /* exit       */
#include <string.h>   /* strcpy     */
#include <getopt.h>
#include <stdarg.h>
#include <string>

#include <readline/readline.h>
#include <readline/history.h>
#include <glob.h>
#include <wordexp.h>

InterpBase *pinterp;
#define interp_new (*pinterp)
const char *prompt = "READ => ";
const char *history = "~/.rs274";
#define RS274_HISTORY "RS274_HISTORY"

#define active_settings  interp_new.active_settings
#define active_g_codes   interp_new.active_g_codes
#define active_m_codes   interp_new.active_m_codes
#define error_text	 interp_new.error_text
#define interp_execute	 interp_new.execute
#define file_name	 interp_new.file_name
#define interp_init	 interp_new.init
#define stack_name	 interp_new.stack_name
#define line_text	 interp_new.line_text
#define line_length	 interp_new.line_length
#define sequence_number  interp_new.sequence_number
#define interp_close     interp_new.close
#define interp_exit      interp_new.exit
#define interp_open      interp_new.open
#define interp_read	 interp_new.read
#define interp_load_tool_table interp_new.load_tool_table
#define interp_set_loglevel interp_new.set_loglevel
#define interp_task_init interp_new.task_init

/*

This file contains the source code for an emulation of using the six-axis
rs274 interpreter from the EMC system.

*/

/*********************************************************************/

/* report_error

Returned Value: none

Side effects: an error message is printed on stderr

Called by:
  interpret_from_file
  interpret_from_keyboard
  main

This

1. calls error_text to get the text of the error message whose
code is error_code and prints the message,

2. calls line_text to get the text of the line on which the
error occurred and prints the text, and

3. if print_stack is on, repeatedly calls stack_name to get
the names of the functions on the function call stack and prints the
names. The first function named is the one that sent the error
message.


*/

void report_error( /* ARGUMENTS                            */
 int error_code,   /* the code number of the error message */
 int print_stack)  /* print stack if ON, otherwise not     */
{
  char interp_error_text_buf[LINELEN];
  int k;

  error_text(error_code, interp_error_text_buf, 5); /* for coverage of code */
  error_text(error_code, interp_error_text_buf, LINELEN);
  fprintf(stderr, "%s\n",
          ((interp_error_text_buf[0] == 0) ? "Unknown error, bad error code" : interp_error_text_buf));
  line_text(interp_error_text_buf, LINELEN);
  fprintf(stderr, "%s\n", interp_error_text_buf);
  if (print_stack == ON)
    {
      for (k = 0; ; k++)
        {
          stack_name(k, interp_error_text_buf, LINELEN);
          if (interp_error_text_buf[0] != 0)
            fprintf(stderr, "%s\n", interp_error_text_buf);
          else
            break;
        }
    }
}


void initialize_readline ()
{
    wordexp_t p;
    const char *s;

    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = "rs274";
 
    if ((s = getenv(RS274_HISTORY)))
	history = s;
    // tilde-expand 
    if (wordexp(history, &p, WRDE_SHOWERR|WRDE_UNDEF )) {
	perror("wordexp");
    } else {
	history = strdup(p.we_wordv[0]);
    }
    if (history)
	read_history(history);
}

/***********************************************************************/

/* interpret_from_keyboard

Returned Value: int (0)

Side effects:
  Lines of NC code entered by the user are interpreted.

Called by:
  interpret_from_file
  main

This prompts the user to enter a line of rs274 code. When the user
hits <enter> at the end of the line, the line is executed.
Then the user is prompted to enter another line.

Any canonical commands resulting from executing the line are printed
on the monitor (stdout).  If there is an error in reading or executing
the line, an error message is printed on the monitor (stderr).

To exit, the user must enter "quit" (followed by a carriage return).

*/

int interpret_from_keyboard(  /* ARGUMENTS                 */
			    int block_delete,            /* switch which is ON or OFF */
			    int print_stack)             /* option which is ON or OFF */
{
    char *line;
    int status;

    initialize_readline ();
      
    for(; ;)
	{
	    line = readline ( prompt);
	    if (!line || strcmp (line, "quit") == 0) {
		if (history)
		    write_history(history);
		return 0;
	    }
	    if (*line)
		add_history(line);
	    status = interp_read(line);
	    if ((status == INTERP_EXECUTE_FINISH) && (block_delete == ON));
	    else if (status == INTERP_ENDFILE);
	    else if ((status != INTERP_EXECUTE_FINISH) &&
		     (status != INTERP_OK))
		report_error(status, print_stack);
	    else
		{
		    status = interp_execute();
		    if ((status == INTERP_EXIT) ||
			(status == INTERP_EXECUTE_FINISH));
		    else if (status != INTERP_OK)
			report_error(status, print_stack);
		}
	}
}

/*********************************************************************/

/* interpret_from_file

Returned Value: int (0 or 1)
   If any of the following errors occur, this returns 1.
   Otherwise, it returns 0.
   1. interp_read returns something other than INTERP_OK or
      INTERP_EXECUTE_FINISH, no_stop is off, and the user elects
      not to continue.
   2. interp_execute returns something other than INTERP_OK,
      EXIT, or INTERP_EXECUTE_FINISH, no_stop is off, and the user
      elects not to continue.

Side Effects:
   An open NC-program file is interpreted.

Called By:
   main

This emulates the way the EMC system uses the interpreter.

If the do_next argument is 1, this goes into MDI mode if an error is
found. In that mode, the user may (1) enter code or (2) enter "quit" to
get out of MDI. Once out of MDI, this asks the user whether to continue
interpreting the file.

If the do_next argument is 0, an error does not stop interpretation.

If the do_next argument is 2, an error stops interpretation.

*/

int interpret_from_file( /* ARGUMENTS                  */
 int do_next,            /* what to do if error        */
 int block_delete,       /* switch which is ON or OFF  */
 int print_stack)        /* option which is ON or OFF  */
{
  int status=0;
  char line[LINELEN];

  SET_BLOCK_DELETE(block_delete);

  for(; ;)
    {
      status = interp_read();
      if ((status == INTERP_EXECUTE_FINISH) && (block_delete == ON))
        continue;
      else if (status == INTERP_ENDFILE)
        break;
      if ((status != INTERP_OK) &&    // should not be EXIT
          (status != INTERP_EXECUTE_FINISH))
        {
          report_error(status, print_stack);
          if (do_next == 2) /* 2 means stop */
            {
              status = 1;
              break;
            }
          else if (do_next == 1) /* 1 means MDI */
            {
              fprintf(stderr, "starting MDI\n");
              interpret_from_keyboard(block_delete, print_stack);
              fprintf(stderr, "continue program? y/n =>");
              if (!fgets(line, LINELEN, stdin) || line[0] != 'y')
                {
                  status = 1;
                  break;
                }
              else
                continue;
            }
          else /* if do_next == 0 -- 0 means continue */
            continue;
        }
      status = interp_execute();
      if ((status != INTERP_OK) &&
          (status != INTERP_EXIT) &&
          (status != INTERP_EXECUTE_FINISH))
        {
          report_error(status, print_stack);
          status = 1;
          if (do_next == 1) /* 1 means MDI */
            {
              fprintf(stderr, "starting MDI\n");
              interpret_from_keyboard(block_delete, print_stack);
              fprintf(stderr, "continue program? y/n =>");
              if (!fgets(line, LINELEN, stdin) || line[0] != 'y')
                break;
            }
          else if (do_next == 2) /* 2 means stop */
            break;
        }
      else if (status == INTERP_EXIT)
        return 0;
    }
  return ((status == 1) ? 1 : 0);
}

/************************************************************************/

/* read_tool_file

Returned Value: int
  Returns 0 for success, nonzero for failure.  Failures can be caused by:
  1. The file named by the user cannot be opened.
  2. Any error detected by loadToolTable()

Side Effects:
  Values in the tool table of the machine setup are changed,
  as specified in the file.

Called By: main
*/

int read_tool_file(  /* ARGUMENTS         */
 const char * tool_file_name)   /* name of tool file */
{
  char buffer[1000];

  if (tool_file_name[0] == 0) /* ask for name if given name is empty string */
    {
      fprintf(stderr, "name of tool file => ");
      if(!fgets(buffer, 1000, stdin)) return 1;
      buffer[strlen(buffer) - 1] = 0;
      tool_file_name = buffer;
    }

  return loadToolTable(tool_file_name, _tools, 0, 0, 0);
}

/************************************************************************/

/* designate_parameter_file

Returned Value: int
  If any of the following errors occur, this returns 1.
  Otherwise, it returns 0.
  1. The file named by the user cannot be opened.

Side Effects:
  The name of a parameter file given by the user is put in the
  parameter_file_name string.

Called By: main

*/

int designate_parameter_file(char * parameter_file_name)
{
  FILE * test_port;

  fprintf(stderr, "name of parameter file => ");
  if(!fgets(parameter_file_name, PARAMETER_FILE_NAME_LENGTH, stdin))
    return 1;
  parameter_file_name[strlen(parameter_file_name) - 1] = 0;
  test_port = fopen(parameter_file_name, "r");
  if (test_port == NULL)
    {
      fprintf(stderr, "Cannot open %s\n", parameter_file_name);
      return 1;
    }
  fclose(test_port);
  return 0;
}

/************************************************************************/

/* adjust_error_handling

Returned Value: int (0)

Side Effects:
  The values of print_stack and do_next are set.

Called By: main

This function allows the user to set one or two aspects of error handling.

By default the driver does not print the function stack in case of error.
This function always allows the user to turn stack printing on if it is off
or to turn stack printing off if it is on.

When interpreting from the keyboard, the driver always goes ahead if there
is an error.

When interpreting from a file, the default behavior is to stop in case of
an error. If the user is interpreting from a file (indicated by args being
2 or 3), this lets the user change what it does on an error.

If the user has not asked for output to a file (indicated by args being 2),
the user can choose any of three behaviors in case of an error (1) continue,
(2) stop, (3) go into MDI mode. This function allows the user to cycle among
the three.

If the user has asked for output to a file (indicated by args being 3),
the user can choose any of two behaviors in case of an error (1) continue,
(2) stop. This function allows the user to toggle between the two.

*/

int adjust_error_handling(
 int args,
 int * print_stack,
 int * do_next)
{
  char buffer[80];
  int choice;

  for(;;)
    {
      fprintf(stderr, "enter a number:\n");
      fprintf(stderr, "1 = done with error handling\n");
      fprintf(stderr, "2 = %sprint stack on error\n",
              ((*print_stack == ON) ? "do not " : ""));
      if (args == 3)
        {
          if (*do_next == 0) /* 0 means continue */
            fprintf(stderr,
                    "3 = stop on error (do not continue)\n");
          else /* if do_next == 2 -- 2 means stopping on error */
            fprintf(stderr,
                    "3 = continue on error (do not stop)\n");
        }
      else if (args == 2)
        {
          if (*do_next == 0) /* 0 means continue */
            fprintf(stderr,
                    "3 = mdi on error (do not continue or stop)\n");
          else if (*do_next == 1) /* 1 means MDI */
            fprintf(stderr,
                    "3 = stop on error (do not mdi or continue)\n");
          else /* if do_next == 2 -- 2 means stopping on error */
            fprintf(stderr,
                    "3 = continue on error (do not stop or mdi)\n");
        }
      fprintf(stderr, "enter choice => ");
      if (!fgets(buffer, 80, stdin))
        break;
      if (sscanf(buffer, "%d", &choice) != 1)
        continue;
      if (choice == 1)
        break;
      else if (choice == 2)
        *print_stack = ((*print_stack == OFF) ? ON : OFF);
      else if ((choice == 3) && (args == 3))
        *do_next = ((*do_next == 0) ? 2 : 0);
      else if ((choice == 3) && (args == 2))
        *do_next = ((*do_next == 2) ? 0 : (*do_next + 1));
    }
  return 0;
}

/************************************************************************/

/* main

The executable exits with either 0 (under all conditions not listed
below) or 1 (under the following conditions):
1. A fatal error occurs while interpreting from a file.
2. Read_tool_file fails.
3. An error occurs in interp_init.

***********************************************************************

Here are three ways in which the rs274 executable may be called.
Any other sort of call to the executable will cause an error message
to be printed and the interpreter will not run. Other executables
may be called similarly.

1. If the rs274 stand-alone executable is called with no arguments,
input is taken from the keyboard, and an error in the input does not
cause the rs274 executable to exit.

EXAMPLE:

1A. To interpret from the keyboard, enter:

rs274

***********************************************************************

2. If the executable is called with one argument, the argument is
taken to be the name of an NC file and the file is interpreted as
described in the documentation of interpret_from_file.


EXAMPLES:

2A. To interpret the file "cds.abc" and read the results on the
screen, enter:

rs274 cds.abc

2B. To interpret the file "cds.abc" and print the results in the file
"cds.prim", enter:

rs274 cds.abc > cds.prim

***********************************************************************

Whichever way the executable is called, this gives the user several
choices before interpretation starts

1 = start interpreting
2 = choose parameter file
3 = read tool file ...
4 = turn block delete switch ON
5 = adjust error handling...

Interpretation starts when option 1 is chosen. Until that happens, the
user is repeatedly given the five choices listed above.  Item 4
toggles between "turn block delete switch ON" and "turn block delete
switch OFF".  See documentation of adjust_error_handling regarding
what option 5 does.

User instructions are printed to stderr (with fprintf) so that output
can be redirected to a file. When output is redirected and user
instructions are printed to stdout (with printf), the instructions get
redirected and the user does not see them.

*/
int _task = 0; // control preview behaviour when remapping

int main (int argc, char ** argv)
{
  int status;
  int choice;
  int do_next; /* 0=continue, 1=mdi, 2=stop */
  int block_delete;
  char buffer[80];
  int tool_flag;
  int gees[ACTIVE_G_CODES];
  int ems[ACTIVE_M_CODES];
  double sets[ACTIVE_SETTINGS];
  char default_name[] = "/etc/emc2/sample-configs/sim/sim.var";
  int print_stack;
  int go_flag;
  char *inifile = NULL;
  int log_level = -1;
  std::string interp;

  do_next = 2;  /* 2=stop */
  block_delete = OFF;
  print_stack = OFF;
  tool_flag = 0;
  strcpy(_parameter_file_name, default_name);
  _outfile = stdout; /* may be reset below */
  go_flag = 0;

  while(1) {
      int c = getopt(argc, argv, "p:t:v:bsn:gi:l:T");
      if(c == -1) break;

      switch(c) {
          case 'p': interp = optarg; break;
          case 't': read_tool_file(optarg); tool_flag=1; break;
          case 'v': strcpy(_parameter_file_name, optarg); break;
          case 'b': block_delete = (block_delete == OFF) ? ON : OFF; break;
          case 's': print_stack = (print_stack == OFF) ? ON : OFF; break;
          case 'n': do_next = atoi(optarg); break;
          case 'l': log_level = atoi(optarg); break;
          case 'g': go_flag = !go_flag; break;
          case 'i': inifile = optarg; break;
          case 'T': _task = 1; break;
          case '?': default: goto usage;
      }
  }

  if (argc - optind > 3)
    {
usage:
      fprintf(stderr,
            "Usage: %s [-p interp.so] [-t tool.tbl] [-v var-file.var] [-n 0|1|2]\n"
            "          [-b] [-s] [-g] [input file [output file]]\n"
            "\n"
            "    -p: Specify the pluggable interpreter to use\n"
            "    -t: Specify the .tbl (tool table) file to use\n"
            "    -v: Specify the .var (parameter) file to use\n"
            "    -n: Specify the continue mode:\n"
            "           0: continue\n"
            "           1: enter MDI mode\n"
            "           2: stop (default)\n"
            "    -b: Toggle the 'block delete' flag (default: OFF)\n"
            "    -s: Toggle the 'print stack' flag (default: OFF)\n"
            "    -g: Toggle the 'go (batch mode)' flag (default: OFF)\n"
            "    -i: specify the .ini file (default: no ini file)\n"
            "    -T: call task_init()\n"
            "    -l: specify the log_level (default: -1)\n"
            , argv[0]);
      exit(1);
    }

  if(!interp.empty()) {
    pinterp = interp_from_shlib(interp.c_str());
  }
  if(!pinterp) pinterp = new Interp;

  for(; !go_flag ;)
    {
      fprintf(stderr, "enter a number:\n");
      fprintf(stderr, "1 = start interpreting\n");
      fprintf(stderr, "2 = choose parameter file ...\n");
      fprintf(stderr, "3 = read tool file ...\n");
      fprintf(stderr, "4 = turn block delete switch %s\n",
              ((block_delete == OFF) ? "ON" : "OFF"));
      fprintf(stderr, "5 = adjust error handling...\n");
      fprintf(stderr, "enter choice => ");
      if (!fgets(buffer, 80, stdin))
        break;
      if (sscanf(buffer, "%d", &choice) != 1)
        continue;
      if (choice == 1)
        break;
      else if (choice == 2)
        {
          if (designate_parameter_file(_parameter_file_name) != 0)
            exit(1);
        }
      else if (choice == 3)
        {
          if (read_tool_file("") != 0)
            exit(1);
          tool_flag = 1;
        }
      else if (choice == 4)
        block_delete = ((block_delete == OFF) ? ON : OFF);
      else if (choice == 5)
        adjust_error_handling(argc, &print_stack, &do_next);
    }
  fprintf(stderr, "executing\n");
  if (tool_flag == 0)
    {
      if (read_tool_file(EMC2_DEFAULT_TOOLTABLE) != 0)
        exit(1);
    }

  // Skip past arguments used up by getopt() */
  argc = argc - optind + 1;
  argv = argv + optind - 1;

  if (argc == 3)
    {
      _outfile = fopen(argv[2], "w");
      if (_outfile == NULL)
        {
          fprintf(stderr, "could not open output file %s\n", argv[2]);
          exit(1);
        }
    }
  if (inifile!= 0) {
      setenv("INI_FILE_NAME",inifile,1);
  } else
      unsetenv("INI_FILE_NAME");

  if ((status = interp_init()) != INTERP_OK)
    {
      report_error(status, print_stack);
      exit(1);
    }

  if (log_level != -1)
      interp_set_loglevel(log_level);


  if (argc == 1)
    status = interpret_from_keyboard(block_delete, print_stack);
  else /* if (argc == 2 or argc == 3) */
    {
      status = interp_open(argv[1]);
      if (status != INTERP_OK) /* do not need to close since not open */
        {
          report_error(status, print_stack);
          exit(1);
        }
      status = interpret_from_file(do_next, block_delete, print_stack);
      file_name(buffer, 5);  /* called to exercise the function */
      file_name(buffer, 79); /* called to exercise the function */
      interp_close();
    }
  line_length();         /* called to exercise the function */
  sequence_number();     /* called to exercise the function */
  active_g_codes(gees);  /* called to exercise the function */
  active_m_codes(ems);   /* called to exercise the function */
  active_settings(sets); /* called to exercise the function */
  interp_exit(); /* saves parameters */
  exit(status);
}

/***********************************************************************/

int  emcOperatorError(int id, const char *fmt, ...)
{
    va_list ap;

    if (id)
	fprintf(stderr,"[%d] ", id);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return 0;
}
