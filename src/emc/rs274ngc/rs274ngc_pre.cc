/********************************************************************
* Description: rs274ngc_pre.cc
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
********************************************************************/
/* rs274ngc.cc

This rs274ngc.cc file contains the source code for (1) the kernel of
several rs274ngc interpreters and (2) two of the four sets of interface
functions declared in canon.hh:
1. interface functions to call to tell the interpreter what to do.
   These all return a status value.
2. interface functions to call to get information from the interpreter.

Kernel functions call each other. A few kernel functions are called by
interface functions.

Interface function names all begin with "Interp::".

Error handling is by returning a status value of either a non-error
code (INTERP_OK, INTERP_EXIT, etc.) or some specific error code
from each function where there is a possibility of error.  If an error
occurs, processing is always stopped, and control is passed back up
through the function call hierarchy to an interface function; the
error code is also passed back up. The stack of functions called is
also recorded. The external program calling an interface function may
then handle the error further as it sees fit.

Since returned values are usually used as just described to handle the
possibility of errors, an alternative method of passing calculated
values is required. In general, if function A needs a value for
variable V calculated by function B, this is handled by passing a
pointer to V from A to B, and B calculates and sets V.

There are a lot of functions named read_XXXX. All such functions read
characters from a string using a counter. They all reset the counter
to point at the character in the string following the last one used by
the function. The counter is passed around from function to function
by using pointers to it. The first character read by each of these
functions is expected to be a member of some set of characters (often
a specific character), and each function checks the first character.

This version of the interpreter not saving input lines. A list of all
lines will be needed in future versions to implement loops, and
probably for other purposes.

This version does not use any additional memory as it runs. No
memory is allocated by the source code.

This version does not suppress superfluous commands, such as a command
to start the spindle when the spindle is already turning, or a command
to turn on flood coolant, when flood coolant is already on.  When the
interpreter is being used for direct control of the machining center,
suppressing superfluous commands might confuse the user and could be
dangerous, but when it is used to translate from one file to another,
suppression can produce more concise output. Future versions might
include an option for suppressing superfluous commands.

****************************************************************************/
#include <boost/python.hpp>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <libintl.h>
#include <set>
#include <stdexcept>

#include "inifile.hh"		// INIFILE
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"	// interpreter private definitions
#include "interp_queue.hh"
#include "rs274ngc_interp.hh"
//#include "rs274ngc_errors.cc"

#include "units.h"

extern char * _rs274ngc_errors[];


const char *Interp::interp_status(int status) {
    static char statustext[50];
    static const char *msgs[] = { "INTERP_OK", "INTERP_EXIT",
	    "INTERP_EXECUTE_FINISH", "INTERP_ENDFILE", "INTERP_FILE_NOT_OPEN",
	    "INTERP_ERROR" };
    sprintf(statustext, "%s%s%d", ((status >= INTERP_OK) && (status
	    <= INTERP_ERROR)) ? msgs[status] : "unknown interpreter error",
	    (status >= INTERP_MIN_ERROR) ? " - error: " : " - ", status);
    return statustext;
}

int trace;

Interp::Interp() 
    : log_file(0)
{
    _setup.pymodule_stat = PYMOD_NONE;
    init_named_parameters();  // need this before Python init
    if (trace) fprintf(stderr,"---> new Interp() pid=%d\"",getpid());
}


Interp::~Interp() {
    if (trace) fprintf(stderr,"---> del Interp() pid=%d\"",getpid());

    if(log_file) {
	fclose(log_file);
	log_file = 0;
    }
}

void Interp::doLog(unsigned int flags, const char *file, int line,
		   const char *fmt, ...)
{
    struct timeval tv;
    struct tm *tm;
    va_list ap;

    va_start(ap, fmt);
    if (flags & LOG_TIME) {
	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	fprintf(log_file, "%04d%02d%02d-%02d:%02d:%02d.%03ld ",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec,
		tv.tv_usec/1000);
    }
    if (flags & LOG_PID) {
	fprintf(log_file, "%4d ",getpid());
    }
    if (flags & LOG_FILENAME) {fprintf(log_file, "%s:%d: ",file,line);
    }
    vfprintf(log_file, fmt, ap);
    fflush(log_file);
    va_end(ap);
}

/****************************************************************************/

/*

The functions in this section of this file are functions for
external programs to call to tell the rs274ngc interpreter
what to do. They are declared in rs274ngc.hh.

*/

/***********************************************************************/

/*! Interp::close

Returned Value: int (INTERP_OK)

Side Effects:
   The NC-code file is closed if open.
   The _setup world model is reset.

Called By: external programs

*/

int Interp::close()
{
  if(_setup.use_lazy_close)
    {
      _setup.lazy_closing = 1;
      return INTERP_OK;
    }

  if (_setup.file_pointer != NULL) {
    fclose(_setup.file_pointer);
    _setup.file_pointer = NULL;
    _setup.percent_flag = false;
  }
  reset();

  return INTERP_OK;
}

const char *remaps[] = {"NONE","T_REMAP","M6_REMAP","M61_REMAP",
			"M_USER_REMAP","G_USER_REMAP"};


/***********************************************************************/

/*! Interp::execute

Returned Value: int)
   If execute_block returns INTERP_EXIT, this returns that.
   If execute_block returns INTERP_EXECUTE_FINISH, this returns that.
   If execute_block returns an error code, this returns that code.
   Otherwise, this returns INTERP_OK.

Side Effects:
   Calls to canonical machining commands are made.
   The interpreter variables are changed.
   At the end of the program, the file is closed.
   If using a file, the active G codes and M codes are updated.

Called By: external programs

This executes a previously parsed block.

*/

int Interp::_execute(const char *command)
{
  int status;
  int n;
  int MDImode = 0;

  logDebug("execute: command=%s mdi_interrupt=%d",command,_setup.mdi_interrupt);

  if (NULL != command) {
    MDImode = 1;
    status = read(command);
    if (status != INTERP_OK) {
	if (status > INTERP_MIN_ERROR) {
	    logRemap("-- clearing remap stack (current level=%d) due to read(%s) status %s MDImode=%d",
		    _setup.remap_level,command, interp_status(status),MDImode);
	    _setup.remap_level = 0;
	}
      return status;
    }
  }

  logDebug("MDImode = %d",MDImode);
  logDebug("Interp::execute(%s)", command);
  // process control functions -- will skip if skipping
  //  if (_setup.block1.o_number != 0)
  if ((EXECUTING_BLOCK(_setup).o_number != 0) ||
      (EXECUTING_BLOCK(_setup).o_name != 0) ||
      (_setup.mdi_interrupt))
    {
      logDebug("Convert control functions");
      CHP(convert_control_functions(&(EXECUTING_BLOCK(_setup)), &_setup));

#if 1
      // let MDI code call subroutines.
      // !!!KL not clear what happens if last execution failed while in
      // !!!KL a subroutine

      // NOTE: the last executed file will still be open, because "close"
      // is really a lazy close.
    
      if (_setup.mdi_interrupt) {
	  _setup.mdi_interrupt = false;
	  MDImode = 1;
      }
      logDebug("!!!KL Open file is:%s:", _setup.filename);
      logDebug("MDImode = %d", MDImode);
      while(MDImode && _setup.call_level) // we are still in a subroutine
      {
          status = read(0);  // reads from current file and calls parse
          if (status != INTERP_OK)
	    {
		if (status > INTERP_MIN_ERROR) {
		    logRemap("-- clearing remap stack (current level=%d) due to read(0) status %s, blocktext='%s' MDImode=%d",
			    _setup.remap_level, interp_status(status), _setup.blocktext,MDImode);
		    _setup.remap_level = 0;
		}
		return status;
	    }
          status = execute();  // special handling for mdi errors
          if (status != INTERP_OK) {
		if (status == INTERP_EXECUTE_FINISH) {
		    _setup.mdi_interrupt = true;
		} else {
		    if (status > INTERP_MIN_ERROR) {
			logRemap("-- clearing remap stack (current level=%d) due to execute() status %s, blocktext='%s' MDImode=%d",
				_setup.remap_level, interp_status(status), _setup.blocktext,MDImode);
			_setup.remap_level = 0;
		    }
		    reset();
		}
               CHP(status);
          }
      }
      _setup.mdi_interrupt = false;
#endif
      return INTERP_OK;
    }

  // skip if skipping
  if(_setup.skipping_o)
    {
      logDebug("skipping to: %s", _setup.skipping_o);
      return INTERP_OK;
    }

  for (n = 0; n < _setup.parameter_occurrence; n++)
  {  // copy parameter settings from parameter buffer into parameter table
    _setup.parameters[_setup.parameter_numbers[n]]
          = _setup.parameter_values[n];
  }

  logDebug("_setup.named_parameter_occurrence = %d",
           _setup.named_parameter_occurrence);
  for (n = 0; n < _setup.named_parameter_occurrence; n++)
  {  // copy parameter settings from parameter buffer into parameter table

      logDebug("storing param:|%s|", _setup.named_parameters[n]);
      CHP(store_named_param(&_setup, _setup.named_parameters[n],
                          _setup.named_parameter_values[n]));

    // free the string
      logDebug("freeing param[%d]:|%s|:%p", n, _setup.named_parameters[n],
               _setup.named_parameters[n]);
    free(_setup.named_parameters[n]);
  }

  _setup.named_parameter_occurrence = 0;

  if (_setup.line_length != 0) {        /* line not blank */
      int next_remap = next_remapping(&(EXECUTING_BLOCK(_setup)), &_setup);

      // at this point we have a parsed block
      // if items are to be remapped the flow is as follows:
      //
      // 1. push this block onto the remap stack because this might take several
      //    interp invcocations to finish, while other blocks will be parsed and
      //    executed by the oword subs. The top-of-stack block is the 'current
      //    remapped block' or CONTROLLING_BLOCK.
      //
      // 2. execute the remap stack top level block, ticking off (zeroing) all items which are done.
      //
      // 3. when a remap operation is encountered, this will result in a call like so:
      //   'o<replacement>call ..params..' OR a call to a Python function if so defined.
      //
      //   this replacement call is parsed with read() into _setup.blocks[0] by the
      //   corresponding routine (see e.g. handling of T in interp_execute.cc)
      //   through calling into execute_handler()
      //
      // 4. execute_handler() sets an optional prologue handler which is called
      //    when the subroutine environment is set up (parameters set, execution of
      //    body to begin). This is the way to set local named parameters e.g. for canned cycles.
      //
      // 5. execute_handler() also sets up an epilogue handler for the replacement sub
      //   which finishes any work at the C or Python level on endsub/return, and thereafter a callback
      //   into remap_finished().
      //
      // 6. The execution stops after parsing, and returns with an indication which
      //   remapping operation was parsed.
      //
      //   NB: it is important that a convert_xxx function encountering a remapped
      //   item does not return a INTERP_* type code but the negative value of the
      //   the remapping operation so the right thing can be done after parsing.
      //
      // 7. In MDI mode, we have to kick execution by replicating code from above
      //   to get the osub call going.
      //
      // 8. In Auto mode, we do an initial execute(0) to get things going, thereafer
      //   task will do it for us.
      //
      // 9. When a replacment sub finishes, remap_finished() continues execution of
      //   the current remapped block until done.
      //
      if (next_remap != NO_REMAP) {
	  logRemap("found remap %s in '%s', level=%d filename=%s line=%d",
		  remaps[next_remap],_setup.blocktext,_setup.call_level,_setup.filename,_setup.sequence_number);

          _setup.remap_level++;
	  if (_setup.remap_level == MAX_NESTED_REMAPS) {
	      _setup.remap_level = 0;
	      ERS("maximum nesting of remapped blocks execeeded");
	      return INTERP_ERROR;
	  }


	  // push onto block stack
	  CONTROLLING_BLOCK(_setup) = EXECUTING_BLOCK(_setup);
	  CONTROLLING_BLOCK(_setup).breadcrumbs = 0; // clear trail

	  // remember the line where remap was discovered
	  if (_setup.remap_level == 1) {
	      CONTROLLING_BLOCK(_setup).line_number  = _setup.sequence_number;
	  } else {
	      CONTROLLING_BLOCK(_setup).line_number  = EXECUTING_BLOCK(_setup).line_number;
	  }

	  logRemap("enter remap nesting (now level %d) remapline=%d", _setup.remap_level,CONTROLLING_BLOCK(_setup).line_number);

	  // execute up to the first remap including read() of its handler
	  status = execute_block(&(CONTROLLING_BLOCK(_setup)), &_setup);

	  // All items up to the first remap item have been executed.
	  // The remap item procedure call has been parsed into _setup.blocks[0],
	  // the EXECUTING_BLOCK.
	  // after parsing a handler, execute_block() either fails to toplevel or
	  // returns the negative value of the handler ID it parsed.
	  switch (status) {
	  case -T_REMAP:
	  case -M6_REMAP:
	  case -M61_REMAP:
	  case -M_USER_REMAP:
	  case -G_USER_REMAP:
	      logRemap("handler armed: %s",remaps[-status]);
	      if (MDImode) {
		  // need to trigger execution of parsed _setup.block1 here
		  // replicate MDI oword execution code here
		  if ((EXECUTING_BLOCK(_setup).o_number != 0) ||
		      (EXECUTING_BLOCK(_setup).o_name != 0) ||
		      (_setup.mdi_interrupt)) {
		      CHP(convert_control_functions(&(EXECUTING_BLOCK(_setup)), &_setup));
		      if (_setup.mdi_interrupt) {
			  _setup.mdi_interrupt = false;
			  MDImode = 1;
		      }
		      status = INTERP_OK;
		      while(MDImode && _setup.call_level) { // we are still in a subroutine
			  status = read(0);  // reads from current file and calls parse
			  if (status != INTERP_OK) {
			      return status;
			  }
			  status = execute();  // special handling for mdi errors
			  if (status != INTERP_OK) {
			      if (status == INTERP_EXECUTE_FINISH) {
				  _setup.mdi_interrupt = true;
			      } else {
				  logRemap("execute() returned %s, RESETTING!\n",interp_status(status));
				  reset();
			      }
			      CHP(status);
			  }
		      }
		      _setup.mdi_interrupt = false;
		      // at this point the MDI execution of a remapped block is complete.
		      logRemap("MDI remap execution complete status=%s\n",interp_status(status));
		      write_g_codes(&(EXECUTING_BLOCK(_setup)), &_setup);
		      write_m_codes(&(EXECUTING_BLOCK(_setup)), &_setup);
		      write_settings(&_setup);
		      return INTERP_OK;
		  }
	      } else {
		  // this should get the osub going
		  status = execute(0);
		  // when this is done, blocks[0] will be executed as per standard case
		  // on endsub/return and g_codes/m_codes/settings recorded there.
	      }
	      if ((status != INTERP_OK) &&
		  (status != INTERP_EXECUTE_FINISH) && (status != INTERP_EXIT))
		  ERP(status);
	      break;
	  default:
	      logRemap("execute_block status = %d",status);
	  }
      } else {
	  // standard case: unremapped block execution
	  status = execute_block(&(EXECUTING_BLOCK(_setup)), &_setup);

	  write_g_codes(&(EXECUTING_BLOCK(_setup)), &_setup);
	  write_m_codes(&(EXECUTING_BLOCK(_setup)), &_setup);
	  write_settings(&_setup);

	  if ((status == INTERP_EXIT) &&
	      (_setup.remap_level > 0) &&
	      (_setup.call_level > 0)) {
	      // an M2 was encountered while executing a handler.
	      logRemap("standard case status=%s remap_level=%d call_level=%d blocktext='%s' MDImode=%d",
		      interp_status(status),_setup.remap_level,_setup.call_level, _setup.blocktext,MDImode);
	      logRemap("_setup.filename = %s, fn[0]=%s, fn[1]=%s",
		      _setup.filename,
		      _setup.sub_context[0].filename,
		      _setup.sub_context[1].filename);


	  }
      }
    if ((status != INTERP_OK) &&
        (status != INTERP_EXECUTE_FINISH) && (status != INTERP_EXIT))
      ERP(status);
  } else                        /* blank line is OK */
    status = INTERP_OK;
  return status;
}


int Interp::execute(const char *command) 
{
    int status;
    if ((status = _execute(command)) > INTERP_MIN_ERROR) {
	unwind_call(status, __FILE__,__LINE__);
    }
    return status;
}

int Interp::execute(const char *command, int line_number)
{
    int status;

    _setup.sequence_number = line_number;
    logDebug("--> execute(%s) remap remap_level=%d call_level=%d  mdi_interrupt=%d",
	    command == NULL ? "NULL" : command,
	    _setup.remap_level,_setup.call_level,_setup.mdi_interrupt);
    status = Interp::execute(command);
    logDebug("<-- execute() remap remap_level=%d call_level=%d status=%s mdi_interrupt=%d",
	    _setup.remap_level,_setup.call_level,interp_status(status),_setup.mdi_interrupt);
    if ((_setup.call_level == 0) &&
	(status == INTERP_EXECUTE_FINISH) &&
	(_setup.mdi_interrupt)) {
	logRemap("<-- execute() clearing mdi_interrupt");
	_setup.mdi_interrupt = false;  // seems to work ok!
    }
    return status;
}

// when a remapping sub finishes, the oword return/endsub handling code
// calls back in here to continue block execution
int Interp::remap_finished(int finished_remap)
{
    int next_remap,status;

    logRemap("remap_finished finished=%s remap_level=%d call_level=%d filename=%s",
	  remaps[finished_remap],_setup.remap_level,_setup.call_level,_setup.filename);

    switch (finished_remap) {
    case T_REMAP:
    case M6_REMAP:
    case M61_REMAP:
    case M_USER_REMAP:
    case G_USER_REMAP:
	// the controlling block had a remapped item, which just finished and the
	// epilogue handler called in here.
	// check the current block for the next remapped item
	next_remap = next_remapping(&(CONTROLLING_BLOCK(_setup)), &_setup);
	if (next_remap) {
	    logRemap("arming %s",remaps[next_remap]);

	    // this will execute up to the next remap, and return
	    // after parsing the handler with read()
	    // so blocks[0] is armed (meaning: a osub call is parsed, but not executed yet)
	    status = execute_block(&(CONTROLLING_BLOCK(_setup)),
				   &_setup);
	    logRemap("post-arming: execute_block() returns %d",status);

	    if (status < 0) {
		// a remap was parsed, get the block going
		return execute(0);
	    } else
		return status;
	} else {
	    // execution of controlling block finished executing a remap, and it contains no more
	    // remapped items. Execute any leftover items.
	    logRemap("no more remaps in controlling_block found (nesting=%d call_level=%d), dropping",
		    _setup.remap_level,_setup.call_level);

	    status = execute_block(&(CONTROLLING_BLOCK(_setup)),
				   &_setup);

	    if ((status < 0) ||  (status > INTERP_MIN_ERROR)) {
		// status < 0 is a bug; might happen if next_remapping() failed to indicate the next remap
		logRemap("executing block leftover items: %s status=%s  nesting=%d (failing)",
			status < 0 ? "BUG":"ERROR", interp_status(status),_setup.remap_level);
		int level = _setup.remap_level;
		_setup.remap_level = 0;
		if (status < 0)
		    ERS("BUG - check next_remapping() status=%d nesting=%d",status,level);
	    } else {
		// we're done with this remapped block.
		// execute_block may return INTERP_EXECUTE_FINISH if a probe, input or toolchange
		// command was executed.
		// not sure what INTERP_ENDFILE & INTERP_EXIT really mean here.
		// if ((status == INTERP_OK) || (status == INTERP_ENDFILE) || (status == INTERP_EXIT) || (status == INTERP_EXECUTE_FINISH)) {
		// leftover items finished. Drop a remapping level.
		logRemap("executing block leftover items complete, status=%s  remap_level=%d tc=%d probe=%d input=%d mdi_interrupt=%d  line=%d backtoline=%d",
			interp_status(status),_setup.remap_level,_setup.toolchange_flag,
			_setup.probe_flag,_setup.input_flag,_setup.mdi_interrupt,_setup.sequence_number,
			CONTROLLING_BLOCK(_setup).line_number);

		// restore the line number where remap was found
		if (_setup.remap_level == 1) {
		    // dropping to top level
		    _setup.sequence_number = CONTROLLING_BLOCK(_setup).line_number;
		} else {
		    // just dropping a nesting level
		    EXECUTING_BLOCK(_setup).line_number = CONTROLLING_BLOCK(_setup).line_number ;
		}
		_setup.remap_level--; // drop one nesting level
		if (_setup.remap_level < 0) {
		    Log("BUG: remap_level %d (<0) after dropping!!",
			    _setup.remap_level);
		    ERS("BUG: remap_level < 0");
		}
	    }
	}
	return status;
	break;

    default: ;
	// "should not happen"
	Log("BUG: remap_finished(): finished_remap=%d nesting=%d",
	    finished_remap, _setup.remap_level);
    }
    return INTERP_OK;
}


// examine a block in execution sequence for an active item which is
// remapped to an oword subroutine.
// execution sequence as described in:
// http://www.linuxcnc.org/docs/2.4/html/gcode_overview.html#sec:Order-of-Execution
// return NO_REMAP if no remapped item found.
// return remap_op of the first remap in execution sequence.
int Interp::next_remapping(block_pointer block, setup_pointer settings)
{
    int exec_phase, mode;

    for (exec_phase = 1; exec_phase < 31; exec_phase++) {
	switch (exec_phase) {
	case 1: // Comment (including message)
	case 2: // Set feed rate mode (G93, G94).
	case 3: // Set feed rate (F).
	case 4: // Set spindle speed (S).
	    break;
	case 5: // Select tool (T).
	    if (block->t_flag && todo(STEP_PREPARE) &&
		remapping("T"))
		return T_REMAP;
	    break;
	case 6: // User defined M-Codes in group 5
	    if (IS_USER_MCODE(block,settings,5) && todo(STEP_M_5))
		return M_USER_REMAP;
	    break;
	case 7: // HAL pin I/O (M62-M68)
	case 8: // User defined M-Codes in group 6 which are NOT M6 or M61
	    if (IS_USER_MCODE(block,settings,6) && todo(STEP_M_6))
		return M_USER_REMAP;
	    break;
	case 9: // Change tool (M6), Set Tool Number (M61)
	    if ((block->m_modes[6] == 6) && todo(STEP_M_6) &&
		remapping("M6"))
		return M6_REMAP;
	    if ((block->m_modes[6] == 61) && todo(STEP_M_6) &&
		remapping("M61"))
		return M61_REMAP;
	    break;
	case 10: // User defined M-Codes in group 7
	    if (IS_USER_MCODE(block,settings,7) && todo(STEP_M_7))
		return M_USER_REMAP;
	    break;

	case 11: // Spindle on or off (M3, M4, M5).
	case 12: // Save State (M70, M73), Restore State (M72), Invalidate State (M71)
	case 13: // User defined M-Codes in group 8
	    if (IS_USER_MCODE(block,settings,8) && todo(STEP_M_8))
		return M_USER_REMAP;
	    break;
	case 14: // Coolant on or off (M7, M8, M9).
	    break;
	case 15: // User defined M-Codes in group 9
	    if (IS_USER_MCODE(block,settings,9) && todo(STEP_M_9))
		return M_USER_REMAP;
	    break;
	case 16: // Enable or disable overrides (M48, M49,M50,M51,M52,M53).
	case 17: // User defined M-Codes in group 10
	    if (IS_USER_MCODE(block,settings,10)  && todo(STEP_M_10))
		return M_USER_REMAP;
	    break;
        case 18: // User-defined Commands (M100-M199)
	case 19: // Dwell (G4).
	case 20: // Set active plane (G17, G18, G19).
	case 21: // Set length units (G20, G21).
	case 22: // Cutter radius compensation on or off (G40, G41, G42)
	case 23: // Cutter length compensation on or off (G43, G49)
	case 24: // Coordinate system selection (G54, G55, G56, G57, G58, G59, G59.1, G59.2, G59.3).
	case 25: // Set path control mode (G61, G61.1, G64)
	case 26: // Set distance mode (G90, G91).
	case 27: // Set retract mode (G98, G99).
	case 28: // Go to reference location (G28, G30) or change coordinate system data (G10) or set axis offsets (G92, G92.1, G92.2, G94).
	    break;
	case 29: // Perform motion (G0 to G3, G33, G73, G76, G80 to G89), as modified (possibly) by G53.
	    mode = block->g_modes[GM_MOTION];
	    if ((mode != -1) && IS_USER_GCODE(mode) &&
		todo(STEP_MOTION)) {
		return G_USER_REMAP;
	    }
	    break;
	case 30: // Stop (M0, M1, M2, M30, M60).
	    break;
	    // when adding cases, dont forget to increase exec_phase upper limit
	}
    }
    return NO_REMAP;
}

/***********************************************************************/

/*! Interp::exit

Returned Value: int (INTERP_OK)

Side Effects: See below

Called By: external programs

The system parameters are saved to a file and some parts of the world
model are reset. If GET_EXTERNAL_PARAMETER_FILE_NAME provides a
non-empty file name, that name is used for the file that is
written. Otherwise, the default parameter file name is used.

*/

int Interp::exit()
{
  char file_name[LINELEN];

  GET_EXTERNAL_PARAMETER_FILE_NAME(file_name, (LINELEN - 1));
  save_parameters(((file_name[0] ==
                             0) ?
                            RS274NGC_PARAMETER_FILE_NAME_DEFAULT :
                            file_name), _setup.parameters);
  reset();

  return INTERP_OK;
}

void Interp::set_loglevel(int level) { _setup.loggingLevel = level; }


/***********************************************************************/

/*! rs274_ngc_init

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, this returns INTERP_OK.
   1. Interp::restore_parameters returns an error code.

Side Effects:
   Many values in the _setup structure are reset.
   A USE_LENGTH_UNITS canonical command call is made.
   A SET_FEED_REFERENCE canonical command call is made.
   A SET_ORIGIN_OFFSETS canonical command call is made.
   An INIT_CANON call is made.

Called By: external programs

Currently we are running only in CANON_XYZ feed_reference mode.  There
is no command regarding feed_reference in the rs274 language (we
should try to get one added). The initialization routine, therefore,
always calls SET_FEED_REFERENCE(CANON_XYZ).

*/

int Interp::init()
{
  int k;                        // starting index in parameters of origin offsets
  char filename[LINELEN];
  double *pars;                 // short name for _setup.parameters

  char *iniFileName;

  INIT_CANON();

  iniFileName = getenv("INI_FILE_NAME");

  // logDebug("Interp.init() called pid=%d ini=%s\n",getpid(),iniFileName);

  // the default log file
  _setup.loggingLevel = 0;
  _setup.tool_change_at_g30 = 0;
  _setup.tool_change_quill_up = 0;
  _setup.tool_change_with_spindle_on = 0;
  _setup.a_axis_wrapped = 0;
  _setup.b_axis_wrapped = 0;
  _setup.c_axis_wrapped = 0;
  _setup.random_toolchanger = 0;
  _setup.a_indexer = 0;
  _setup.b_indexer = 0;
  _setup.c_indexer = 0;
  _setup.return_value = 0;
  _setup.remap_level = 0; // remapped blocks stack index


  // not clear -- but this is fn is called a second time without an INI.
  if(NULL == iniFileName)
  {
      // log_file not yet setup here
      if (trace) fprintf(stderr,"INI_FILE_NAME not found pid=%d\n",getpid());
  }
  else
  {
      if (trace) fprintf(stderr,"Interp.init(%d) getenv(INI_FILE_NAME)=%s\n",
			 getpid(),iniFileName);
      IniFile inifile;

      if (trace) fprintf(stderr,"iniFileName:%s:\n", iniFileName);

      if (inifile.Open(iniFileName) == false) {
          fprintf(stderr,"Unable to open inifile:%s:\n", iniFileName);
      }
      else
      {
          const char *inistring;


          inifile.Find(&_setup.tool_change_at_g30, "TOOL_CHANGE_AT_G30", "EMCIO");
          inifile.Find(&_setup.tool_change_quill_up, "TOOL_CHANGE_QUILL_UP", "EMCIO");
          inifile.Find(&_setup.tool_change_with_spindle_on, "TOOL_CHANGE_WITH_SPINDLE_ON", "EMCIO");
          inifile.Find(&_setup.a_axis_wrapped, "WRAPPED_ROTARY", "AXIS_3");
          inifile.Find(&_setup.b_axis_wrapped, "WRAPPED_ROTARY", "AXIS_4");
          inifile.Find(&_setup.c_axis_wrapped, "WRAPPED_ROTARY", "AXIS_5");
          inifile.Find(&_setup.random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");

          inifile.Find(&_setup.a_indexer, "LOCKING_INDEXER", "AXIS_3");
          inifile.Find(&_setup.b_indexer, "LOCKING_INDEXER", "AXIS_4");
          inifile.Find(&_setup.c_indexer, "LOCKING_INDEXER", "AXIS_5");
          inifile.Find(&_setup.orient_offset, "ORIENT_OFFSET", "RS274NGC");

          inifile.Find(&_setup.debugmask, "DEBUG", "EMC");

	  _setup.debugmask |= EMC_DEBUG_UNCONDITIONAL;

          if(NULL != (inistring = inifile.Find("LOG_LEVEL", "RS274NGC")))
          {
              _setup.loggingLevel = atol(inistring);
          }

	  // default the log_file to stderr.
          if(NULL != (inistring = inifile.Find("LOG_FILE", "RS274NGC")))
          {
	      if ((log_file = fopen(inistring, "a"))  == NULL) {
		  logDebug( "(%d): Unable to open log file:%s, using stderr",
			  getpid(), inistring);
		  log_file = stderr;
	      }
          } else {
	      log_file = stderr;
	  }
          _setup.use_lazy_close = 1;

	  _setup.wizard_root[0] = 0;
          if(NULL != (inistring = inifile.Find("WIZARD_ROOT", "WIZARD")))
          {
	    logDebug("[WIZARD]WIZARD_ROOT:%s", inistring);
            if (realpath(inistring, _setup.wizard_root) == NULL) {
        	//realpath didn't find the file
		logDebug("realpath failed to find wizard_root:%s:", inistring);
            }
          }
          logDebug("_setup.wizard_root:%s:", _setup.wizard_root);

	  _setup.program_prefix[0] = 0;
          if(NULL != (inistring = inifile.Find("PROGRAM_PREFIX", "DISPLAY")))
          {
	    // found it
            if (realpath(inistring, _setup.program_prefix) == NULL){
        	//realpath didn't find the file
		logDebug("realpath failed to find program_prefix:%s:", inistring);
            }
            logDebug("program prefix:%s: prefix:%s:",
		     inistring, _setup.program_prefix);
          }
          else
          {
	      logDebug("PROGRAM_PREFIX not found");
          }
          logDebug("_setup.program_prefix:%s:", _setup.program_prefix);


          if(NULL != (inistring = inifile.Find("SUBROUTINE_PATH", "RS274NGC")))
          {
            // found it
            int dct;
            char* nextdir;
            char tmpdirs[PATH_MAX+1];

            for (dct=0; dct < MAX_SUB_DIRS; dct++) {
                 _setup.subroutines[dct][0] = 0;
            }

            strcpy(tmpdirs,inistring);
            nextdir = strtok(tmpdirs,":");  // first token
            dct = 0;
            while (1) {
                if (realpath(nextdir,_setup.subroutines[dct]) == NULL){
                   //realpath didn't find the file
                   logDebug("realpath failed to find subroutines[%d]:%s:",dct,nextdir);
                    _setup.subroutines[dct][0] = 0;
                } else {
                    logDebug("program prefix[%d]:%s",dct,_setup.subroutines[dct]);
                }
                dct++;
                if (dct >= MAX_SUB_DIRS) {
                   logDebug("too many entries in SUBROUTINE_PATH, max=%d", MAX_SUB_DIRS);
                   break;
                }
                nextdir = strtok(NULL,":");
                if (nextdir == NULL) break; // no more tokens
             }
          }
          else
          {
              logDebug("SUBROUTINE_PATH not found");
          }
          logDebug("_setup.subroutines:%p:", _setup.subroutines);


          // subroutine to execute on aborts - for instance to retract
          // toolchange HAL pins
          if (NULL != (inistring = inifile.Find("ON_ABORT_COMMAND", "RS274NGC"))) {
	      _setup.on_abort_command = strdup(inistring);
              logDebug("_setup.on_abort_command=%s", _setup.on_abort_command);
          } else {
	      _setup.on_abort_command = NULL;
          }

	  // not used yet
	  if (NULL != (inistring = inifile.Find("PY_RELOAD_AFTER_ERROR", "RS274NGC")))
	      _setup.py_reload_on_error = (atoi(inistring) > 0);
	  else
	      _setup.py_reload_on_error = false;

	  if (NULL != (inistring = inifile.Find("PYDIR", "RS274NGC")))
	      _setup.pydir = strdup(inistring);
	  else
	      _setup.pydir = NULL;

          if (NULL != (inistring = inifile.Find("PYIMPORT", "RS274NGC"))) {
	      _setup.pymodule = strdup(inistring);
	      int status;
	      if ((status = init_python(&(_setup))) != INTERP_OK) {
		  logDebug("PYIMPORT: import of module %s failed",_setup.pymodule);
	      }
          } else {
	      _setup.pymodule = NULL;
          }


	  // parse options of the form:
	  // REMAP= M420 modalgroup=6 argspec=pq prolog=setnamedvars ngc=m43.ngc epilog=ignore_retvalue
	  // REMAP= M421 modalgroup=6 argspec=- prolog=setnamedvars python=m43func epilog=ignore_retvalue
	  int n = 1;
	  int lineno = -1;
	  while (NULL != (inistring = inifile.Find("REMAP", "RS274NGC", n, &lineno))) {

	      char iniline[LINELEN];
	      char *argv[MAX_REMAPOPTS];
	      int   argc = 0;
	      const char *code;
	      remap_pointer r;
	      bool errored = false;
	      int g1 = 0, g2 = 0;
	      int mcode = -1;

	      if ((r = (remap_pointer) malloc(sizeof(remap))) == NULL) {
		  Error("cant malloc remap_struct");
		  continue;
	      }
	      memset((void *)r, 0, sizeof(remap));
	      r->modal_group = -1; // mark as unset, required param for m/g
	      strcpy(iniline, inistring);
	      char* token = strtok((char *) iniline, " \t");

	      while( token != NULL && argc < MAX_REMAPOPTS - 1) {
		  argv[argc++] = token;
		  token = strtok( NULL, " \t" );
	      }
	      argv[argc] = NULL;
	      code = strstore(argv[0]);
	      r->name = code;

	      for (int i = 1; i < argc; i++) {
		  int kwlen = 0;
		  char *kw = argv[i];
		  char *arg = strchr(argv[i],'=');
		  if (arg != NULL) {
		      kwlen = arg - argv[i];
		      arg++;
		      if (!strlen(arg)) { // 'kw='
			  Error("option '%s' - zero length value: %d:REMAP = %s",
				kw,lineno,inistring);
			  errored = true;
			  continue;
		      }
		  } else { // 'kw'
		      Error("option '%s' - missing '=<value>: %d:REMAP = %s",
			    kw,lineno,inistring);
		      errored = true;
		      continue;;
		  }
		  if (!strncasecmp(kw,"modalgroup",kwlen)) {
		      r->modal_group = atoi(arg);
		      continue;
		  }
		  if (!strncasecmp(kw,"argspec",kwlen)) {
		      size_t pos = strspn (arg,
					   "ABCDEFGHIJKMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz-");
		      if (pos != strlen(arg)) {
			  Error("argspec: illegal word '%c' - %d:REMAP = %s",
				arg[pos],lineno,inistring);
			  errored = true;
			  continue;
		      }
		      r->argspec = strstore(arg);
		      continue;
		  }
		  if (!strncasecmp(kw,"prolog",kwlen)) {
		      r->prolog_func = strstore(arg);
		      continue;
		  }
		  if (!strncasecmp(kw,"epilog",kwlen)) {
		      r->epilog_func = strstore(arg);
		      continue;
		  }
		  if (!strncasecmp(kw,"ngc",kwlen)) {
		      if (r->remap_py) {
			  Error("cant remap to an ngc file and a Python function: -  %d:REMAP = %s",
				lineno,inistring);
			  errored = true;
			  continue;
		      }
		      r->remap_ngc = strstore(arg);
		      continue;
		  }
		  if (!strncasecmp(kw,"python",kwlen)) {
		      if (r->remap_ngc ) {
			  Error("cant remap to an ngc file and a Python function: -  %d:REMAP = %s",
				lineno,inistring);
			  errored = true;
			  continue;
		      }
		      if (!is_pycallable(&_setup,arg)) {
			  Error("'%s' is not a Python callable function - %d:REMAP = %s",
				arg,lineno,inistring);
			  errored = true;
			  continue;
		      }
		      r->remap_py = strstore(arg);
		      continue;
		  }
		  Error("unrecognized option '%*s' in  %d:REMAP = %s",
			kwlen,kw,lineno,inistring);
	      }
	      if (errored) {
		  free(r);
		  goto done;
	      }
	      char key[10];

	      if (remapping(code)) {
		  Error("code '%s' already remapped : %d:REMAP = %s",
			code,lineno,inistring);
		  free(r);
		  goto done;
	      }

	      // it is an error not to define a remap function to call.
	      if ((r->remap_ngc == NULL) && (r->remap_py == NULL)) {
		  Error("code '%s' - no remap function given, use either 'python=<function>' or 'ngc=<basename>' : %d:REMAP = %s",
			code,lineno,inistring);
		  free(r);
		  goto done;
	      }

	      // if an argspec is given, the builtin prolog add_parameters()
	      // is called automatically.
	      // for ngc files, this adds local variables as per argspec
	      // for Python, adds variables to the  kwargs dict
	      if (r->argspec) {
		  r->builtin_prolog = &Interp::add_parameters;
	      }

#define CHECK(bad, fmt)						 \
	      do {						 \
		  if (bad) {					 \
		      Log(fmt);				 \
		      goto done;				 \
		  }						 \
	      } while(0)

	      switch (towlower(*code)) {

	      case 't':
		  CHECK((strlen(code) > 1),"T remap - only single letter code allowed");
		  CHECK((r->modal_group != -1), "T remap - modal group setting ignored - fixed sequencing");

		  // prepare has a default builtin epilog for ngc files
		  // which sets the pocket to a nonnegative return value
		  // and aborts on negative values

		  // can be overridden by epilog=pyfunc
		  r->op = T_REMAP;
		  if (r->epilog_func) {
		      // finish_user_command just ends a remap in progress
		      // without any side effects
		      r->builtin_epilog = &Interp::finish_user_command;
		  } else {
		      r->builtin_epilog = &Interp::finish_t_command;
		  }
		  _setup.remaps["T"] = r;
		  break;

	      case 's':
		  CHECK(strlen(code) > 1,"S remap - only single letter code allowed");
		  CHECK((r->modal_group != -1), "S remap - modal group setting ignored - fixed sequencing");

		  r->op = S_REMAP;
		  _setup.remaps["S"] = r;
		  r->builtin_epilog = &Interp::finish_user_command;
		  break;

	      case 'f':
		  CHECK(strlen(code) > 1,"F remap - only single letter code allowed");
		  CHECK((r->modal_group != -1), "F remap - modal group setting ignored - fixed sequencing");

		  r->op = F_REMAP;
		  _setup.remaps["F"] = r;
		  r->builtin_epilog = &Interp::finish_user_command;
		  break;

	      case 'm':
		  if (sscanf(code + 1, "%d", &mcode) == 1) {
		      // change_tool, set tool number have default builtin epilogs for ngc files
		      // which can be overridden by a py epilog
		      if (r->epilog_func) {
			  r->op = M_USER_REMAP;
			  r->builtin_epilog = &Interp::finish_user_command;
		      } else {
			  switch (mcode) {
			  case 6:
			      r->builtin_epilog = &Interp::finish_m6_command ;
			      r->op = M6_REMAP;
			      break;
			  case 61:
			      r->builtin_epilog = &Interp::finish_m61_command;
			      r->op = M61_REMAP;
			      break;
			  default:
			      r->op = M_USER_REMAP;
			      r->builtin_epilog =  &Interp::finish_user_command;
			  }
		      }
		      _setup.remaps[code] = r;
		      _setup.m_remapped[mcode] = 1;
		  } else {
		      Error("parsing M-code: expecting integer like 'M420', got '%s' : %d:REMAP = %s",
			    code,lineno,inistring);
		      goto done;
		  }
		  if (r->modal_group == -1) {
		      Error("code '%s' : no modalgroup=<int> given : %d:REMAP = %s",
			    code,lineno,inistring);
		      goto done;
		  }
		  if (!M_MODE_OK(r->modal_group)) {
			  Error("code '%s' : invalid modalgroup=<int> given (currently valid: 5..10) : %d:REMAP = %s",
			    code,lineno,inistring);
		      goto done;
		  }
		  break;
	      case 'g':
		  r->op = G_USER_REMAP;
		  r->builtin_epilog =  &Interp::finish_user_command;

		  if (!G_MODE_OK(r->modal_group)) {
		      Error("code '%s' : %s modalgroup=<int> given, def : %d:REMAP = %s",
			    code,
			    r->modal_group == -1 ? "no" : "invalid",
			    lineno,
			    inistring);
		      goto done;
		  }
		  if (sscanf(code + 1, "%d.%d", &g1, &g2) == 2) {
		      int n = g1 * 10 + g2;
		      sprintf(key,"G%d",n );
		      code = strstore(key);
		      _setup.g_remapped[n] = 1;
		      _setup.remaps[code] = r;
		  } else
		      if (sscanf(code + 1, "%d", &g1) == 1) {
			  sprintf(key,"G%d", g1 * 10);
			  code = strstore(key);
			  _setup.remaps[code] = r;
			  _setup.g_remapped[g1*10] = 1;
		      } else {
			  Error("parsing G-code: expecting code like 'G89' or 'G88.7', got '%s' : %d:REMAP = %s",
				code,lineno,inistring);
		      }
		  if (r->modal_group == -1) {
		      Error("code '%s' : no modalgroup=<int> given : %d:REMAP = %s",
			    code,lineno,inistring);
		      goto done;
		  }
		  if (!G_MODE_OK(r->modal_group)) {
			  Error("code '%s' : invalid modalgroup=<int> given (currently valid: 1) : %d:REMAP = %s",
			    code,lineno,inistring);
		      goto done;
		  }
		  break;
	      default:
		  Log("REMAP BUG=%s %d:REMAP = %s",
		      code,lineno,inistring);
	      }
	      logRemap("%d: REMAP=%s line=%s",
		       lineno, code, inistring);

	  done:
	      n++;
	      continue;
	  }
	  print_remaps();

          // close it
          inifile.Close();
      }
  }

  _setup.length_units = GET_EXTERNAL_LENGTH_UNIT_TYPE();
  USE_LENGTH_UNITS(_setup.length_units);
  GET_EXTERNAL_PARAMETER_FILE_NAME(filename, LINELEN);
  if (filename[0] == 0)
    strcpy(filename, RS274NGC_PARAMETER_FILE_NAME_DEFAULT);
  CHP(restore_parameters(filename));
  pars = _setup.parameters;
  _setup.origin_index = (int) (pars[5220] + 0.0001);
  if(_setup.origin_index < 1 || _setup.origin_index > 9) {
      _setup.origin_index = 1;
      pars[5220] = 1.0;
  }

  k = (5200 + (_setup.origin_index * 20));
  _setup.origin_offset_x = USER_TO_PROGRAM_LEN(pars[k + 1]);
  _setup.origin_offset_y = USER_TO_PROGRAM_LEN(pars[k + 2]);
  _setup.origin_offset_z = USER_TO_PROGRAM_LEN(pars[k + 3]);
  _setup.AA_origin_offset = USER_TO_PROGRAM_ANG(pars[k + 4]);
  _setup.BB_origin_offset = USER_TO_PROGRAM_ANG(pars[k + 5]);
  _setup.CC_origin_offset = USER_TO_PROGRAM_ANG(pars[k + 6]);
  _setup.u_origin_offset = USER_TO_PROGRAM_LEN(pars[k + 7]);
  _setup.v_origin_offset = USER_TO_PROGRAM_LEN(pars[k + 8]);
  _setup.w_origin_offset = USER_TO_PROGRAM_LEN(pars[k + 9]);

  SET_G5X_OFFSET(_setup.origin_index,
                 _setup.origin_offset_x ,
                 _setup.origin_offset_y ,
                 _setup.origin_offset_z ,
                 _setup.AA_origin_offset,
                 _setup.BB_origin_offset,
                 _setup.CC_origin_offset,
                 _setup.u_origin_offset ,
                 _setup.v_origin_offset ,
                 _setup.w_origin_offset);

  if (pars[5210]) {
      _setup.axis_offset_x = USER_TO_PROGRAM_LEN(pars[5211]);
      _setup.axis_offset_y = USER_TO_PROGRAM_LEN(pars[5212]);
      _setup.axis_offset_z = USER_TO_PROGRAM_LEN(pars[5213]);
      _setup.AA_axis_offset = USER_TO_PROGRAM_ANG(pars[5214]);
      _setup.BB_axis_offset = USER_TO_PROGRAM_ANG(pars[5215]);
      _setup.CC_axis_offset = USER_TO_PROGRAM_ANG(pars[5216]);
      _setup.u_axis_offset = USER_TO_PROGRAM_LEN(pars[5217]);
      _setup.v_axis_offset = USER_TO_PROGRAM_LEN(pars[5218]);
      _setup.w_axis_offset = USER_TO_PROGRAM_LEN(pars[5219]);
  } else {
      _setup.axis_offset_x = 0.0;
      _setup.axis_offset_y = 0.0;
      _setup.axis_offset_z = 0.0;
      _setup.AA_axis_offset = 0.0;
      _setup.BB_axis_offset = 0.0;
      _setup.CC_axis_offset = 0.0;
      _setup.u_axis_offset = 0.0;
      _setup.v_axis_offset = 0.0;
      _setup.w_axis_offset = 0.0;
  }

  SET_G92_OFFSET(_setup.axis_offset_x ,
                 _setup.axis_offset_y ,
                 _setup.axis_offset_z ,
                 _setup.AA_axis_offset,
                 _setup.BB_axis_offset,
                 _setup.CC_axis_offset,
                 _setup.u_axis_offset ,
                 _setup.v_axis_offset ,
                 _setup.w_axis_offset);

  _setup.rotation_xy = pars[k+10];
  SET_XY_ROTATION(pars[k+10]);
  SET_FEED_REFERENCE(CANON_XYZ);
//_setup.active_g_codes initialized below
//_setup.active_m_codes initialized below
//_setup.active_settings initialized below
//_setup.block1 does not need initialization
  _setup.blocktext[0] = 0;
//_setup.current_slot set in Interp::synch
//_setup.current_x set in Interp::synch
//_setup.current_y set in Interp::synch
//_setup.current_z set in Interp::synch
  _setup.cutter_comp_side = false;
  _setup.arc_not_allowed = false;
  _setup.cycle_il_flag = false;
  _setup.distance_mode = MODE_ABSOLUTE;
  _setup.ijk_distance_mode = MODE_INCREMENTAL;  // backwards compatability
  _setup.feed_mode = UNITS_PER_MINUTE;
//_setup.feed_override set in Interp::synch
//_setup.feed_rate set in Interp::synch
  _setup.filename[0] = 0;
  _setup.file_pointer = NULL;
//_setup.flood set in Interp::synch
  _setup.tool_offset_index = 1;
//_setup.length_units set in Interp::synch
  _setup.line_length = 0;
  _setup.linetext[0] = 0;
//_setup.mist set in Interp::synch
  _setup.motion_mode = G_80;
//_setup.origin_index set above
//_setup.parameters set above
//_setup.parameter_occurrence does not need initialization
//_setup.parameter_numbers does not need initialization
//_setup.parameter_values does not need initialization
//_setup.percent_flag does not need initialization
//_setup.plane set in Interp::synch
  _setup.probe_flag = false;
  _setup.toolchange_flag = false;
  _setup.input_flag = false;
  _setup.input_index = -1;
  _setup.input_digital = false;
  _setup.program_x = 0.;   /* for cutter comp */
  _setup.program_y = 0.;   /* for cutter comp */
  _setup.program_z = 0.;   /* for cutter comp */
  _setup.cutter_comp_firstmove = true;
//_setup.retract_mode does not need initialization
//_setup.selected_tool_slot set in Interp::synch
  _setup.sequence_number = 0;   /*DOES THIS NEED TO BE AT TOP? */
//_setup.speed set in Interp::synch
  _setup.speed_feed_mode = CANON_INDEPENDENT;
//_setup.speed_override set in Interp::synch
//_setup.spindle_turning set in Interp::synch
//_setup.stack does not need initialization
//_setup.stack_index does not need initialization
   ZERO_EMC_POSE(_setup.tool_offset);
//_setup.tool_max set in Interp::synch
//_setup.tool_table set in Interp::synch
//_setup.traverse_rate set in Interp::synch
//_setup.adaptive_feed set in Interp::synch
//_setup.feed_hold set in Interp::synch

  // initialization stuff for subroutines and control structures
  _setup.call_level = 0;
  _setup.defining_sub = 0;
  _setup.skipping_o = 0;
  _setup.oword_labels = 0;

  _setup.lathe_diameter_mode = false;
  _setup.parameters[5599] = 1.0; // enable (DEBUG, ) output

  memcpy(_readers, default_readers, sizeof(default_readers));

  long axis_mask = GET_EXTERNAL_AXIS_MASK();
  if(!(axis_mask & AXIS_MASK_X)) _readers[(int)'x'] = 0;
  if(!(axis_mask & AXIS_MASK_Y)) _readers[(int)'y'] = 0;
  if(!(axis_mask & AXIS_MASK_Z)) _readers[(int)'z'] = 0;
  if(!(axis_mask & AXIS_MASK_A)) _readers[(int)'a'] = 0;
  if(!(axis_mask & AXIS_MASK_B)) _readers[(int)'b'] = 0;
  if(!(axis_mask & AXIS_MASK_C)) _readers[(int)'c'] = 0;
  if(!(axis_mask & AXIS_MASK_U)) _readers[(int)'u'] = 0;
  if(!(axis_mask & AXIS_MASK_V)) _readers[(int)'v'] = 0;
  if(!(axis_mask & AXIS_MASK_W)) _readers[(int)'w'] = 0;

  synch(); //synch first, then update the interface


  write_g_codes((block_pointer) NULL, &_setup);
  write_m_codes((block_pointer) NULL, &_setup);
  write_settings(&_setup);

  init_tool_parameters();
  // Synch rest of settings to external world
  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::load_tool_table

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, this returns INTERP_OK.
   1. _setup.tool_max is larger than CANON_TOOL_MAX: NCE_TOOL_MAX_TOO_LARGE

Side Effects:
   _setup.tool_table[] is modified.

Called By:
   Interp::synch
   external programs

This function calls the canonical interface function GET_EXTERNAL_TOOL_TABLE
to load the whole tool table into the _setup.

The CANON_TOOL_MAX is an upper limit for this software. The
_setup.tool_max is intended to be set for a particular machine.

*/

int Interp::load_tool_table()
{
  int n;

  CHKS((_setup.pockets_max > CANON_POCKETS_MAX), NCE_POCKET_MAX_TOO_LARGE);
  for (n = 0; n < _setup.pockets_max; n++) {
    _setup.tool_table[n] = GET_EXTERNAL_TOOL_TABLE(n);
  }
  for (; n < CANON_POCKETS_MAX; n++) {
    _setup.tool_table[n].toolno = -1;
    ZERO_EMC_POSE(_setup.tool_table[n].offset);
    _setup.tool_table[n].diameter = 0;
    _setup.tool_table[n].orientation = 0;
    _setup.tool_table[n].frontangle = 0;
    _setup.tool_table[n].backangle = 0;
  }
  set_tool_parameters();
  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::open

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise it returns INTERP_OK.
   1. A file is already open: NCE_A_FILE_IS_ALREADY_OPEN
   2. The name of the file is too long: NCE_FILE_NAME_TOO_LONG
   3. The file cannot be opened: NCE_UNABLE_TO_OPEN_FILE

Side Effects: See below

Called By: external programs

The file is opened for reading and _setup.file_pointer is set.
The file name is copied into _setup.filename.
The _setup.sequence_number, is set to zero.
Interp::reset() is called, changing several more _setup attributes.

The manual [NCMS, page 3] discusses the use of the "%" character at the
beginning of a "tape". It is not clear whether it is intended that
every NC-code file should begin with that character.

In the following, "uses percents" means the first non-blank line
of the file must consist of nothing but the percent sign, with optional
leading and trailing white space, and there must be a second line
of the same sort later on in the file. If a file uses percents,
execution stops at the second percent line. Any lines after the
second percent line are ignored.

In this interpreter (recalling that M2 and M30 always ends execution):
1. If execution of a file is ended by M2 or M30 (not necessarily on
the last line of the file), then it is optional that the file
uses percents.
2. If execution of a file is not ended by M2 or M30, then it is
required that the file uses percents.

If the file being opened uses percents, this function turns on the
_setup.percent flag, reads any initial blank lines, and reads the
first line with the "%". If not, after reading enough to determine
that, this function puts the file pointer back at the beginning of the
file.

*/

int Interp::open(const char *filename) //!< string: the name of the input NC-program file
{
  char *line;
  int index;
  int length;

  if(_setup.use_lazy_close && _setup.lazy_closing)
    {
      _setup.use_lazy_close = 0; // so that close will work
      close();
      _setup.use_lazy_close = 1;
      _setup.lazy_closing = 0;
    }

  CHKS((_setup.file_pointer != NULL), NCE_A_FILE_IS_ALREADY_OPEN);
  CHKS((strlen(filename) > (LINELEN - 1)), NCE_FILE_NAME_TOO_LONG);
  _setup.file_pointer = fopen(filename, "r");
  CHKS((_setup.file_pointer == NULL), NCE_UNABLE_TO_OPEN_FILE, filename);
  line = _setup.linetext;
  for (index = -1; index == -1;) {      /* skip blank lines */
    CHKS((fgets(line, LINELEN, _setup.file_pointer) ==
         NULL), NCE_FILE_ENDED_WITH_NO_PERCENT_SIGN);
    length = strlen(line);
    if (length == (LINELEN - 1)) {   // line is too long. need to finish reading the line to recover
      for (; fgetc(_setup.file_pointer) != '\n';);      // could look for EOF
      ERS(NCE_COMMAND_TOO_LONG);
    }
    for (index = (length - 1);  // index set on last char
         (index >= 0) && (isspace(line[index])); index--);
  }
  if (line[index] == '%') {
    for (index--; (index >= 0) && (isspace(line[index])); index--);
    if (index == -1) {
      _setup.percent_flag = true;
      _setup.sequence_number = 1;       // We have already read the first line
      // and we are not going back to it.
    } else {
      fseek(_setup.file_pointer, 0, SEEK_SET);
      _setup.percent_flag = false;
      _setup.sequence_number = 0;       // Going back to line 0
    }
  } else {
    fseek(_setup.file_pointer, 0, SEEK_SET);
    _setup.percent_flag = false;
    _setup.sequence_number = 0; // Going back to line 0
  }
  strcpy(_setup.filename, filename);
  reset();
  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::read

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, this returns:
       a. INTERP_ENDFILE if the only non-white character on the line is %,
       b. INTERP_EXECUTE_FINISH if the first character of the
          close_and_downcased line is a slash, and
       c. INTERP_OK otherwise.
   1. The command and_setup.file_pointer are both NULL: INTERP_FILE_NOT_OPEN
   2. The probe_flag is true but the HME command queue is not empty:
      NCE_QUEUE_IS_NOT_EMPTY_AFTER_PROBING
   3. If read_text (which gets a line of NC code from file) or parse_line
     (which parses the line) returns an error code, this returns that code.

Side Effects:
   _setup.sequence_number is incremented.
   The executing block is filled with data.

Called By: external programs

This reads a line of NC-code from the command string or, (if the
command string is NULL) from the currently open file. The
_setup.line_length will be set by read_text. This will be zero if the
line is blank or consists of nothing but a slash. If the length is not
zero, this parses the line into the _setup.block1.

*/

int Interp::_read(const char *command)  //!< may be NULL or a string to read
{
  static char name[] = "Interp::read";
  int read_status;

  if (_setup.probe_flag) {
    CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
        NCE_QUEUE_IS_NOT_EMPTY_AFTER_PROBING);
    set_probe_data(&_setup);
    _setup.probe_flag = false;
  }
  if (_setup.toolchange_flag) {
    CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
         _("Queue is not empty after tool change"));
    refresh_actual_position(&_setup);
    load_tool_table();
    _setup.toolchange_flag = false;
  }
  // always track toolchanger-fault and toolchanger-reason codes
  _setup.parameters[5600] = GET_EXTERNAL_TC_FAULT();
  _setup.parameters[5601] = GET_EXTERNAL_TC_REASON();

  if (_setup.input_flag) {
    CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
        NCE_QUEUE_IS_NOT_EMPTY_AFTER_INPUT);
    if (_setup.input_digital) { // we are checking for a digital input
	_setup.parameters[5399] =
	    GET_EXTERNAL_DIGITAL_INPUT(_setup.input_index,
				      (_setup.parameters[5399] != 0.0));
    } else { // checking for analog input
	_setup.parameters[5399] =
	    GET_EXTERNAL_ANALOG_INPUT(_setup.input_index, _setup.parameters[5399]);
    }
    _setup.input_flag = false;
  }
  CHKN(((command == NULL) && (_setup.file_pointer == NULL)),
      INTERP_FILE_NOT_OPEN);

  _setup.parameters[5420] = _setup.current_x;
  _setup.parameters[5421] = _setup.current_y;
  _setup.parameters[5422] = _setup.current_z;
  _setup.parameters[5423] = _setup.AA_current;
  _setup.parameters[5424] = _setup.BB_current;
  _setup.parameters[5425] = _setup.CC_current;
  _setup.parameters[5426] = _setup.u_current;
  _setup.parameters[5427] = _setup.v_current;
  _setup.parameters[5428] = _setup.w_current;

  if(_setup.file_pointer)
  {
      EXECUTING_BLOCK(_setup).offset = ftell(_setup.file_pointer);
  }
  EXECUTING_BLOCK(_setup).breadcrumbs = 0; // clear trail

  read_status =
    read_text(command, _setup.file_pointer, _setup.linetext,
              _setup.blocktext, &_setup.line_length);

  if (read_status == INTERP_ERROR && _setup.skipping_to_sub) {
      // free(_setup.skipping_to_sub);
    _setup.skipping_to_sub = 0;
  }

  if(command)logDebug("%s:[cmd]:|%s|", name, command);
  else logDebug("%s:|%s|", name, _setup.linetext);

  if ((read_status == INTERP_EXECUTE_FINISH)
      || (read_status == INTERP_OK)) {
    if (_setup.line_length != 0) {
	CHP(parse_line(_setup.blocktext, &(EXECUTING_BLOCK(_setup)), &_setup));
    }

    else // Blank line (zero length)
    {
          /* RUM - this case reached when the block delete '/' character 
             is used, or READ_FULL_COMMENT is false and a comment is the
             only content of a line. 
             If a block o-type is in effect, block->o_number needs to be 
             incremented to allow o-extensions to work. 
             Note that the the block is 'refreshed' by init_block(),
             not created new, so this is a legal operation on block1. */
        if (EXECUTING_BLOCK(_setup).o_type != O_none)
        {
            // Clear o_type, this isn't line isn't a command...
            EXECUTING_BLOCK(_setup).o_type = 0;
            // increment o_number
	    EXECUTING_BLOCK(_setup).o_number++;
        }
    }
  } else if (read_status == INTERP_ENDFILE);
  else
    ERP(read_status);
  return read_status;
}

int Interp::read(const char *command) 
{
    int status;
    if ((status = _read(command)) > INTERP_MIN_ERROR) {
	unwind_call(status, __FILE__,__LINE__);
    }
    return status;
}

// Reset interpreter state and  terminate a call in progress by
// falling back to toplevel in a controlled way. Idempotent.
// The input line (_setup.linetext,_setup.blocktext, _setup.line_length) 
// is left untouched for inspection post-error. This is only
// cleared in reset().
int Interp::unwind_call(int status, const char *file, int line)
{
    logDebug("unwind_call call_level=%d status=%d from %s:%d\n",
	    _setup.call_level, status, file, line);

    for(; _setup.call_level > 0; _setup.call_level--) {
	int i;
	context * sub = _setup.sub_context + _setup.call_level - 1;
	free_named_parameters(_setup.call_level, &_setup);
	if(sub->subName) {
	    logDebug("unwind_call leaving sub '%s'\n", sub->subName);
	    free(sub->subName);
	    sub->subName = 0;
	}

	for(i=0; i<INTERP_SUB_PARAMS; i++) {
	    _setup.parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
		sub->saved_params[i];
	}

	// When called from Interp::close via Interp::reset, this one is NULL
	if (!_setup.file_pointer) continue;

	if(0 != strcmp(_setup.filename, sub->filename)) {
	    fclose(_setup.file_pointer);
	    _setup.file_pointer = fopen(sub->filename, "r");
	    logDebug("unwind_call: reopening '%s' at %ld\n",
		    sub->filename, sub->position);
	    strcpy(_setup.filename, sub->filename);
	}

	fseek(_setup.file_pointer, sub->position, SEEK_SET);

	_setup.sequence_number = sub->sequence_number;
	logDebug("unwind_call: setting sequence number=%d from frame %d\n",
		_setup.sequence_number,_setup.call_level);

    }

    if(_setup.sub_name) {
	logDebug("unwind_call exiting current sub '%s'\n", _setup.sub_name);
	free(_setup.sub_name);
	_setup.sub_name = 0;
    }
    _setup.call_level = 0;
    _setup.defining_sub = 0;
    _setup.skipping_o = 0;
    _setup.skipping_to_sub = 0;
    _setup.oword_labels = 0;

    _setup.mdi_interrupt = false;

    qc_reset();
    return INTERP_OK;
}

/***********************************************************************/

/*! Interp::reset

Returned Value: int (INTERP_OK)

Side Effects: See below

Called By:
   external programs
   Interp::close
   Interp::exit
   Interp::open

This function resets the parts of the _setup model having to do with
reading and interpreting one line. It does not affect the parts of the
model dealing with a file being open; Interp::open and Interp::close
do that.

There is a hierarchy of resetting the interpreter. Each of the following
calls does everything the ones above it do.

Interp::reset()
Interp::close()
Interp::init()

In addition, Interp::synch and Interp::restore_parameters (both of
which are called by Interp::init) change the model.

*/

int Interp::reset()
{
  //!!!KL According to the comment,
  //!!!KL this should not be here because this is for
  //!!!KL more than one line.
  //!!!KL But the comment seems wrong -- it is only called at open, close,
  //!!!KL init times which should affect the more global structure.
  //!!!KL (also called by external -- but probably OK)
  //
  // initialization stuff for subroutines and control structures

  for(; _setup.call_level > 0; _setup.call_level--) {
    int i;
    context * sub = _setup.sub_context + _setup.call_level - 1;
    free_named_parameters(&_setup.sub_context[_setup.call_level]);
    if(sub->subName) {
	// FIXME mah free(sub->subName);
      sub->subName = 0;
    }

    for(i=0; i<INTERP_SUB_PARAMS; i++) {
      _setup.parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
        sub->saved_params[i];
    }

    // When called from Interp::close this one is NULL
    if (!_setup.file_pointer) continue;

    if(0 != strcmp(_setup.filename, sub->filename)) {
      fclose(_setup.file_pointer);
      _setup.file_pointer = fopen(sub->filename, "r");

      strcpy(_setup.filename, sub->filename);
    }

    fseek(_setup.file_pointer, sub->position, SEEK_SET);

    _setup.sequence_number = sub->sequence_number;
  }
  if(_setup.sub_name) {
      //free(_setup.sub_name);
    _setup.sub_name = 0;
  }
  _setup.call_level = 0;
  _setup.defining_sub = 0;
  _setup.skipping_o = 0;
  _setup.oword_labels = 0;

  _setup.mdi_interrupt = false;
  _setup.return_value = 0;

  qc_reset();

  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::restore_parameters

Returned Value:
  If any of the following errors occur, this returns the error code shown.
  Otherwise it returns INTERP_OK.
  1. The parameter file cannot be opened for reading: NCE_UNABLE_TO_OPEN_FILE
  2. A parameter index is out of range: NCE_PARAMETER_NUMBER_OUT_OF_RANGE
  3. The parameter file is not in increasing order:
     NCE_PARAMETER_FILE_OUT_OF_ORDER

Side Effects: See below

Called By:
  external programs
  Interp::init

This function restores the parameters from a file, modifying the
parameters array. Usually parameters is _setup.parameters. The file
contains lines of the form:

<variable number> <value>

e.g.

5161 10.456

The variable numbers must be in increasing order, and certain
parameters must be included, as given in the _required_parameters
array. These are the axis offsets, the origin index (5220), and nine
sets of origin offsets. Any parameter not given a value in the file
has its value set to zero.

*/
int Interp::restore_parameters(const char *filename)   //!< name of parameter file to read  
{
  FILE *infile;
  char line[256];
  int variable;
  double value;
  int required;                 // number of next required parameter
  int index;                    // index into _required_parameters
  double *pars;                 // short name for _setup.parameters
  int k;

  // it's OK if the parameter file doesn't exist yet
  // it'll be created in due course with some default values
  if(access(filename, F_OK) == -1)
      return INTERP_OK;
  // open original for reading
  infile = fopen(filename, "r");
  CHKS((infile == NULL), _("Unable to open parameter file: '%s'"), filename);

  pars = _setup.parameters;
  k = 0;
  index = 0;
  required = _required_parameters[index++];
  while (feof(infile) == 0) {
    if (fgets(line, 256, infile) == NULL) {
      break;
    }
    // try for a variable-value match in the file
    if (sscanf(line, "%d %lf", &variable, &value) == 2) {
      CHKS(((variable <= 0)
           || (variable >= RS274NGC_MAX_PARAMETERS)),
          NCE_PARAMETER_NUMBER_OUT_OF_RANGE);
      for (; k < RS274NGC_MAX_PARAMETERS; k++) {
        if (k > variable)
          ERS(NCE_PARAMETER_FILE_OUT_OF_ORDER);
        else if (k == variable) {
          pars[k] = value;
          if (k == required)
            required = _required_parameters[index++];
          k++;
          break;
        } else                  // if (k < variable)
        {
          if (k == required)
            required = _required_parameters[index++];
          pars[k] = 0;
        }
      }
    }
  }
  fclose(infile);
  for (; k < RS274NGC_MAX_PARAMETERS; k++) {
    pars[k] = 0;
  }
  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::save_parameters

Returned Value:
  If any of the following errors occur, this returns the error code shown.
  Otherwise it returns INTERP_OK.
  1. The existing file cannot be renamed:  NCE_CANNOT_CREATE_BACKUP_FILE
  2. The renamed file cannot be opened to read: NCE_CANNOT_OPEN_BACKUP_FILE
  3. The new file cannot be opened to write: NCE_CANNOT_OPEN_VARIABLE_FILE
  4. A parameter index is out of range: NCE_PARAMETER_NUMBER_OUT_OF_RANGE
  5. The renamed file is out of order: NCE_PARAMETER_FILE_OUT_OF_ORDER

Side Effects: See below

Called By:
   external programs
   Interp::exit

A file containing variable-value assignments is updated. The old
version of the file is saved under a different name.  For each
variable-value pair in the old file, a line is written in the new file
giving the current value of the variable.  File lines have the form:

<variable number> <value>

e.g.

5161 10.456

If a required parameter is missing from the input file, this does not
complain, but does write it in the output file.

*/
int Interp::save_parameters(const char *filename,      //!< name of file to write
                             const double parameters[]) //!< parameters to save   
{
  FILE *infile;
  FILE *outfile;
  char line[256];
  int variable;
  double value;
  int required;                 // number of next required parameter
  int index;                    // index into _required_parameters
  int k;

  if(access(filename, F_OK)==0) 
  {
    // rename as .bak
    strcpy(line, filename);
    strcat(line, RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX);
    CHKS((rename(filename, line) != 0), NCE_CANNOT_CREATE_BACKUP_FILE);

    // open backup for reading
    infile = fopen(line, "r");
    CHKS((infile == NULL), NCE_CANNOT_OPEN_BACKUP_FILE);
  } else {
    // it's OK if the parameter file doesn't exist yet
    // it will now be created with a default list of parameters
    infile = fopen("/dev/null", "r");
  }
  // open original for writing
  outfile = fopen(filename, "w");
  CHKS((outfile == NULL), NCE_CANNOT_OPEN_VARIABLE_FILE);

  k = 0;
  index = 0;
  required = _required_parameters[index++];
  while (feof(infile) == 0) {
    if (fgets(line, 256, infile) == NULL) {
      break;
    }
    // try for a variable-value match
    if (sscanf(line, "%d %lf", &variable, &value) == 2) {
      CHKS(((variable <= 0)
           || (variable >= RS274NGC_MAX_PARAMETERS)),
          NCE_PARAMETER_NUMBER_OUT_OF_RANGE);
      for (; k < RS274NGC_MAX_PARAMETERS; k++) {
        if (k > variable)
          ERS(NCE_PARAMETER_FILE_OUT_OF_ORDER);
        else if (k == variable) {
          sprintf(line, "%d\t%f\n", k, parameters[k]);
          fputs(line, outfile);
          if (k == required)
            required = _required_parameters[index++];
          k++;
          break;
        } else if (k == required)       // know (k < variable)
        {
          sprintf(line, "%d\t%f\n", k, parameters[k]);
          fputs(line, outfile);
          required = _required_parameters[index++];
        }
      }
    }
  }
  fclose(infile);
  for (; k < RS274NGC_MAX_PARAMETERS; k++) {
    if (k == required) {
      sprintf(line, "%d\t%f\n", k, parameters[k]);
      fputs(line, outfile);
      required = _required_parameters[index++];
    }
  }
  fclose(outfile);
  return INTERP_OK;
}

/***********************************************************************/

/*! Interp::synch

Returned Value: int (INTERP_OK)

Side Effects:
   sets the value of many attribute of _setup by calling various
   GET_EXTERNAL_xxx functions.

Called By:
   Interp::init
   external programs

This function gets the _setup world model in synch with the rest of
the controller.

*/

int Interp::synch()
{

  char file_name[LINELEN];

  _setup.control_mode = GET_EXTERNAL_MOTION_CONTROL_MODE();
  _setup.AA_current = GET_EXTERNAL_POSITION_A();
  _setup.BB_current = GET_EXTERNAL_POSITION_B();
  _setup.CC_current = GET_EXTERNAL_POSITION_C();
  _setup.current_pocket = GET_EXTERNAL_TOOL_SLOT();
  _setup.current_x = GET_EXTERNAL_POSITION_X();
  _setup.current_y = GET_EXTERNAL_POSITION_Y();
  _setup.current_z = GET_EXTERNAL_POSITION_Z();
  _setup.u_current = GET_EXTERNAL_POSITION_U();
  _setup.v_current = GET_EXTERNAL_POSITION_V();
  _setup.w_current = GET_EXTERNAL_POSITION_W();
  _setup.feed_rate = GET_EXTERNAL_FEED_RATE();
  _setup.flood = GET_EXTERNAL_FLOOD();
  _setup.length_units = GET_EXTERNAL_LENGTH_UNIT_TYPE();
  _setup.mist = GET_EXTERNAL_MIST();
  _setup.plane = GET_EXTERNAL_PLANE();
  _setup.selected_pocket = GET_EXTERNAL_SELECTED_TOOL_SLOT();
  _setup.speed = GET_EXTERNAL_SPEED();
  _setup.spindle_turning = GET_EXTERNAL_SPINDLE();
  _setup.pockets_max = GET_EXTERNAL_POCKETS_MAX();
  _setup.traverse_rate = GET_EXTERNAL_TRAVERSE_RATE();
  _setup.feed_override = GET_EXTERNAL_FEED_OVERRIDE_ENABLE();
  _setup.speed_override = GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE();
  _setup.adaptive_feed = GET_EXTERNAL_ADAPTIVE_FEED_ENABLE();
  _setup.feed_hold = GET_EXTERNAL_FEED_HOLD_ENABLE();

  GET_EXTERNAL_PARAMETER_FILE_NAME(file_name, (LINELEN - 1));
  save_parameters(((file_name[0] ==
                             0) ?
                            RS274NGC_PARAMETER_FILE_NAME_DEFAULT :
                            file_name), _setup.parameters);

  load_tool_table();   /*  must set  _setup.tool_max first */

  return INTERP_OK;
}

/***********************************************************************/
/***********************************************************************/

/*

The functions in this section are to extract information from the
interpreter.

*/

/***********************************************************************/

/*! Interp::active_g_codes

Returned Value: none

Side Effects: copies active G codes into the codes array

Called By: external programs

See documentation of write_g_codes.

*/

void Interp::active_g_codes(int *codes)        //!< array of codes to copy into
{
  int n;

  for (n = 0; n < ACTIVE_G_CODES; n++) {
    codes[n] = _setup.active_g_codes[n];
  }
}

/***********************************************************************/

/*! Interp::active_m_codes

Returned Value: none

Side Effects: copies active M codes into the codes array

Called By: external programs

See documentation of write_m_codes.

*/

void Interp::active_m_codes(int *codes)        //!< array of codes to copy into
{
  int n;

  for (n = 0; n < ACTIVE_M_CODES; n++) {
    codes[n] = _setup.active_m_codes[n];
  }
}

/***********************************************************************/

/*! Interp::active_settings

Returned Value: none

Side Effects: copies active F, S settings into array

Called By: external programs

See documentation of write_settings.

*/

void Interp::active_settings(double *settings) //!< array of settings to copy into
{
  int n;

  for (n = 0; n < ACTIVE_SETTINGS; n++) {
    settings[n] = _setup.active_settings[n];
  }
}

static char savedError[LINELEN+1];
void Interp::setError(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    vsnprintf(savedError, LINELEN, fmt, ap);

    va_end(ap);
}

/***********************************************************************/

/*! Interp::error_text

Returned Value: none

Side Effects: see below

Called By: external programs

This copies the error string whose index in the _rs274ngc_errors array
is error_code into the error_text array -- unless the error_code is
an out-of-bounds index or the length of the error string is not less
than max_size, in which case an empty string is put into the
error_text. The length of the error_text array should be at least
max_size.

*/

void Interp::error_text(int error_code,        //!< code number of error                
                         char *error_text,      //!< char array to copy error text into  
                         int max_size)  //!< maximum number of characters to copy
{
    if(error_code == INTERP_ERROR)
    {
        strncpy(error_text, savedError, max_size);
        error_text[max_size-1] = 0;

        return;
    }

    error_text[0] = 0;
}

/***********************************************************************/

/*! Interp::file_name

Returned Value: none

Side Effects: see below

Called By: external programs

This copies the _setup.filename (the name of the currently open
file) into the file_name array -- unless the name is not shorter than
max_size, in which case a null string is put in the file_name array.


*/

void Interp::file_name(char *file_name,        //!< string: to copy file name into      
                        int max_size)   //!< maximum number of characters to copy
{
  if (strlen(_setup.filename) < ((size_t) max_size))
    strcpy(file_name, _setup.filename);
  else
    file_name[0] = 0;
}

/***********************************************************************/

/*! Interp::line_length

Returned Value: the length of the most recently read line

Side Effects: none

Called By: external programs

*/

int Interp::line_length()
{
  return _setup.line_length;
}

/***********************************************************************/

/*! Interp::line_text

Returned Value: none

Side Effects: See below

Called By: external programs

This copies at most (max_size - 1) non-null characters of the most
recently read line into the line_text string and puts a NULL after the
last non-null character.

*/

void Interp::line_text(char *line_text,        //!< string: to copy line into           
                        int max_size)   //!< maximum number of characters to copy
{
  int n;
  char *the_text;

  the_text = _setup.linetext;
  for (n = 0; n < (max_size - 1); n++) {
    if (the_text[n] != 0)
      line_text[n] = the_text[n];
    else
      break;
  }
  line_text[n] = 0;
}

/***********************************************************************/

/*! Interp::sequence_number

Returned Value: the current interpreter sequence number (how many
lines read since the last time the sequence number was reset to zero,
which happens only when Interp::init or Interp::open is called).

Side Effects: none

Called By: external programs

*/

int Interp::sequence_number()
{
  return _setup.sequence_number;
}

/***********************************************************************/

/*! Interp::stack_name

Returned Value: none

Side Effects: see below

Called By: external programs

This copies at most (max_size - 1) non-null characters from the
string whose index in the _setup.stack array is stack_index into the
function_name string and puts a NULL after the last non-null character --
unless the stack_index is an out-of-bounds index, in which case an
empty string is put into the function_name.

This function is intended to be used several times in a row to get the
stack of function calls that existed when the most recent error
occurred. It should be called first with stack_index equal to 0,
next with stack_index equal to 1, and so on, stopping when an
empty string is returned for the name.

*/

void Interp::stack_name(int stack_index,       //!< index into stack of function names  
                         char *function_name,   //!< string: to copy function name into
                         int max_size)  //!< maximum number of characters to copy
{
  int n;
  char *the_name;

  if ((stack_index > -1) && (stack_index < STACK_LEN)) {
    the_name = _setup.stack[stack_index];
    for (n = 0; n < (max_size - 1); n++) {
      if (the_name[n] != 0)
        function_name[n] = the_name[n];
      else
        break;
    }
    function_name[n] = 0;
  } else
    function_name[0] = 0;
}

/***********************************************************************/

/* Interp::ini_load()

Returned Value: INTERP_OK, RS274NGC_ERROR

Side Effects:
   An INI file containing values for global variables is used to
   update the globals

Called By:
   Interp::init()

The file looks like this:

[RS274NGC]
VARIABLE_FILE = rs274ngc.var

*/


int Interp::ini_load(const char *filename)
{
    IniFile inifile;
    const char *inistring;

    // open it
    if (inifile.Open(filename) == false) {
        logDebug("Unable to open inifile:%s:", filename);
	return -1;
    }

    logDebug("Opened inifile:%s:", filename);


    if (NULL != (inistring = inifile.Find("PARAMETER_FILE", "RS274NGC"))) {
	// found it
	strncpy(_parameter_file_name, inistring, LINELEN);
        logDebug("found PARAMETER_FILE:%s:", _parameter_file_name);
    } else {
	// not found, leave RS274NGC_PARAMETER_FILE alone
        logDebug("did not find PARAMETER_FILE");
    }

    // close it
    inifile.Close();

    return 0;
}

int Interp::init_tool_parameters()
{
  if (_setup.random_toolchanger) {
     // random_toolchanger: tool at startup expected
    _setup.parameters[5400] = _setup.tool_table[0].toolno;
    _setup.parameters[5401] = _setup.tool_table[0].offset.tran.x;
    _setup.parameters[5402] = _setup.tool_table[0].offset.tran.y;
    _setup.parameters[5403] = _setup.tool_table[0].offset.tran.z;
    _setup.parameters[5404] = _setup.tool_table[0].offset.a;
    _setup.parameters[5405] = _setup.tool_table[0].offset.b;
    _setup.parameters[5406] = _setup.tool_table[0].offset.c;
    _setup.parameters[5407] = _setup.tool_table[0].offset.u;
    _setup.parameters[5408] = _setup.tool_table[0].offset.v;
    _setup.parameters[5409] = _setup.tool_table[0].offset.w;
    _setup.parameters[5410] = _setup.tool_table[0].diameter;
    _setup.parameters[5411] = _setup.tool_table[0].frontangle;
    _setup.parameters[5412] = _setup.tool_table[0].backangle;
    _setup.parameters[5413] = _setup.tool_table[0].orientation;
  } else {
    // non random_toolchanger: no tool at startup, one-time init
    if (_setup.tool_table[0].toolno == -1) {
      default_tool_parameters();
    }
  }
  return 0;
}

int Interp::default_tool_parameters()
{
  _setup.parameters[5400] =  0; // toolno
  _setup.parameters[5401] =  0; // x offset
  _setup.parameters[5402] =  0; // y offset RESERVED
  _setup.parameters[5403] =  0; // z offset
  _setup.parameters[5404] =  0; // a offset RESERVED
  _setup.parameters[5405] =  0; // b offset RESERVED
  _setup.parameters[5406] =  0; // c offset RESERVED
  _setup.parameters[5407] =  0; // u offset RESERVED
  _setup.parameters[5408] =  0; // v offset RESERVED
  _setup.parameters[5409] =  0; // w offset RESERVED
  _setup.parameters[5410] =  0; // diameter
  _setup.parameters[5411] =  0; // frontangle
  _setup.parameters[5412] =  0; // backangle
  _setup.parameters[5413] =  0; // orientation
  return 0;
}

int Interp::set_tool_parameters()
{
  // invoke to set tool parameters for current tool (pocket==0)
  // when a tool is absent, set default (zero offset) tool parameters

  if ((! _setup.random_toolchanger) && (_setup.tool_table[0].toolno <= 0)) {
    default_tool_parameters();
    return 0;
  }
  _setup.parameters[5400] = _setup.tool_table[0].toolno;
  _setup.parameters[5401] = _setup.tool_table[0].offset.tran.x;
  _setup.parameters[5402] = _setup.tool_table[0].offset.tran.y;
  _setup.parameters[5403] = _setup.tool_table[0].offset.tran.z;
  _setup.parameters[5404] = _setup.tool_table[0].offset.a;
  _setup.parameters[5405] = _setup.tool_table[0].offset.b;
  _setup.parameters[5406] = _setup.tool_table[0].offset.c;
  _setup.parameters[5407] = _setup.tool_table[0].offset.u;
  _setup.parameters[5408] = _setup.tool_table[0].offset.v;
  _setup.parameters[5409] = _setup.tool_table[0].offset.w;
  _setup.parameters[5410] = _setup.tool_table[0].diameter;
  _setup.parameters[5411] = _setup.tool_table[0].frontangle;
  _setup.parameters[5412] = _setup.tool_table[0].backangle;
  _setup.parameters[5413] = _setup.tool_table[0].orientation;

  return 0;
}


int Interp::on_abort(int reason, const char *message)
{
    // int i;

    logDebug("on_abort reason=%d message='%s' remap_level=%d call_level=%d mdi_interrupt=%d tc=%d probe=%d input=%d",
	    reason, message,_setup.remap_level,_setup.call_level,_setup.mdi_interrupt
	    ,_setup.toolchange_flag,
	    _setup.probe_flag,_setup.input_flag);

    reset();
    _setup.mdi_interrupt = false;

    // clear in case set by an interrupted remapped procedure
    // if set, may cause a "Queue is not empty after tool change" error
    _setup.toolchange_flag = false;
    _setup.probe_flag = false;
    _setup.input_flag = false;

    // reset remapping stack
    _setup.remap_level = 0;
    // reset call level if a sub aborted
    _setup.call_level = 0;

    if (_setup.on_abort_command == NULL)
	return -1;

    char cmd[LINELEN];

    snprintf(cmd,sizeof(cmd), "%s [%d]",_setup.on_abort_command, reason);
    int status = execute(cmd);

    ERP(status);
    return status;
}

static std::set<std::string> stringtable;

const char *strstore(const char *s)
{
    using namespace std;

    if (s == NULL)
        throw invalid_argument("strstore(): NULL argument");
    pair< set<string>::iterator, bool > pair = stringtable.insert(s);
    return string(*pair.first).c_str();
}

remap_pointer Interp::g_remapping(int number)
{
    char key[10];
    snprintf(key, sizeof(key), "g%d", number);
    return remapping(key);
}


remap_pointer Interp::m_remapping(int number)
{
    char key[10];
    snprintf(key, sizeof(key), "m%d", number);
    return remapping(key);
}

remap_pointer Interp::remapping(const char *code)
{
    std::map<const char *,remap_pointer>::iterator n =
	_setup.remaps.find(code);
    if (n !=  _setup.remaps.end())
	return n->second;
    else
	return NULL;
}

// debug aid
void Interp::print_remap(const char *key)
{
    if (!key)
	return;
    remap_pointer r = remapping(key);
    if (r) {
	logRemap("----- remap '%s' :",key);
	logRemap("argspec = '%s'", r->argspec);
	logRemap("modalgroup = %d", r->modal_group);
	logRemap("builtin_prolog = %p",(void *)r->builtin_prolog);
	logRemap("prolog_func = %s",
		 (r->prolog_func ? r->prolog_func : ""));

	logRemap("remap_py = %s",
		 (r->remap_py ? r->remap_py : ""));
	logRemap("remap_ngc = %s",
		 (r->remap_ngc ? r->remap_ngc : ""));
	logRemap("epilog_func = %s",
		 (r->epilog_func ? r->epilog_func : ""));
	logRemap("builtin_epilog = %p",(void *)r->builtin_epilog);

    } else {
	logRemap("print_remap: no such remap: '%s'",key);
    }
}

void Interp::print_remaps(void)
{
    std::map<const char *,remap_pointer>::iterator n =
	_setup.remaps.begin();

    logRemap("-----  remaps:");
    for ( ; n  != _setup.remaps.end(); ++n ) {
	print_remap(n->first);
    }
    logRemap("-------------");
}


