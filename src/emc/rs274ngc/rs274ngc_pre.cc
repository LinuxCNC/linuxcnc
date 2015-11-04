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

#include "rtapi.h"
#include "inifile.hh"		// INIFILE
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"	// interpreter private definitions
#include "interp_queue.hh"
#include "rs274ngc_interp.hh"

#include "units.h"

extern char * _rs274ngc_errors[];

const char *Interp::interp_status(int status) {
    static char statustext[50];
    static const char *msgs[] = { "INTERP_OK", "INTERP_EXIT",
	    "INTERP_EXECUTE_FINISH", "INTERP_ENDFILE", "INTERP_FILE_NOT_OPEN",
	    "INTERP_ERROR" };
    sprintf(statustext, "%s%s%d", ((status >= INTERP_OK) && (status
	    <= INTERP_ERROR)) ? msgs[status] : "unknown interpreter error",
	    (status > INTERP_MIN_ERROR) ? " - error: " : " - ", status);
    return statustext;
}

extern struct _inittab builtin_modules[];
int trace;
static char savedError[LINELEN+1];

Interp::Interp()
    : log_file(stderr),
      _setup(setup_struct())
{
    _setup.init_once = 1;  
    init_named_parameters();  // need this before Python init.
 
    if (!PythonPlugin::instantiate(builtin_modules)) {  // factory
	Error("Interp ctor: cant instantiate Python plugin");
	return;
    }

    try {
	// this import will register the C++->Python converter for Interp
	bp::object interp_module = bp::import("interpreter");
	
	// use a boost::cref to avoid per-call instantiation of the
	// Interp Python wrapper (used for the 'self' parameter in handlers)
	// since interp.init() may be called repeatedly this would create a new
	// wrapper instance on every init(), abandoning the old one and all user attributes
	// tacked onto it, so make sure this is done exactly once
	_setup.pythis =  boost::python::object(boost::cref(this));
	
	// alias to 'interpreter.this' for the sake of ';py, .... ' comments
	// besides 'this', eventually use proper instance names to handle
	// several instances 
	bp::scope(interp_module).attr("this") =  _setup.pythis;

	// make "this" visible without importing interpreter explicitly
	bp::object retval;
	python_plugin->run_string("from interpreter import this", retval, false);
    }
    catch (bp::error_already_set) {
	std::string exception_msg;
	if (PyErr_Occurred()) {
	    exception_msg = handle_pyerror();
	} else
	    exception_msg = "unknown exception";
	bp::handle_exception();
	PyErr_Clear();
	Error("PYTHON: exception during 'this' export:\n%s\n",exception_msg.c_str());
    }
}


Interp::~Interp() {

    if(log_file) {
        if(log_file != stderr)
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
    if (flags & LOG_FILENAME) {
        fprintf(log_file, "%s:%d: ",file,line);
    }

    va_start(ap, fmt);
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
    logOword("close()");
    // be "lazy" only if we're not aborting a call in progress
    // in which case we need to reset() the call stack
    // this does not reset the filename properly 
    if(_setup.use_lazy_close) //  && (_setup.call_level == 0)) 
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
  block_pointer eblock = &EXECUTING_BLOCK(_setup);
  extern const char *call_statenames[];
  extern const char *call_typenames[];
  extern const char *o_ops[];

  if (NULL != command) {
    MDImode = 1;
    status = read(command);
    if (status != INTERP_OK) {
	// if (status > INTERP_MIN_ERROR) 
	//     _setup.remap_level = 0;
	return status;
    }
  }
  logDebug("execute:%s %s='%s' mdi_int=%d o_type=%s o_name=%s cl=%d rl=%d type=%s state=%s",
	   MDImode ? "MDI" : "auto",
	   command ? "command" : "line",
	   command ? command : _setup.linetext,
	    _setup.mdi_interrupt, o_ops[eblock->o_type], eblock->o_name,
	   _setup.call_level,_setup.remap_level, 
	   eblock->call_type < 0 ? "*unset*" : call_typenames[eblock->call_type], 
	   call_statenames[_setup.call_state]);

  // process control functions -- will skip if skipping
  if ((eblock->o_name != 0) || _setup.mdi_interrupt)  {
      status = convert_control_functions(eblock, &_setup);
      CHP(status); // relinquish control if INTERP_EXCUTE_FINISH, INTERP_ERROR etc
      
      // let MDI code call subroutines.
      // !!!KL not clear what happens if last execution failed while in
      // !!!KL a subroutine

      // NOTE: the last executed file will still be open, because "close"
      // is really a lazy close.
    
      // we had an INTERP_OK, so no need to set up another call to finish after sync()
      if (_setup.mdi_interrupt) {
	  _setup.mdi_interrupt = false;
	  MDImode = 1;
      }
      logDebug("!!!KL Open file is:%s:", _setup.filename);
      logDebug("MDImode = %d", MDImode);
      while(MDImode && _setup.call_level) // we are still in a subroutine
      {
          status = read(0);  // reads from current file and calls parse
	  if (status > INTERP_MIN_ERROR)
	      CHP(status);
          status = execute();  // special handling for mdi errors
          if (status != INTERP_OK) {
	      if (status == INTERP_EXECUTE_FINISH) {
		  _setup.mdi_interrupt = true;
	      }
	      CHP(status);
          }
      }
      _setup.mdi_interrupt = false;
     if (MDImode)
	  FINISH();
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

  // logDebug("_setup.named_parameter_occurrence = %d",
  //          _setup.named_parameter_occurrence);
  for (n = 0; n < _setup.named_parameter_occurrence; n++)
  {  // copy parameter settings from parameter buffer into parameter table

      logDebug("storing param:|%s|", _setup.named_parameters[n]);
      CHP(store_named_param(&_setup, _setup.named_parameters[n],
                          _setup.named_parameter_values[n]));
  }
  _setup.named_parameter_occurrence = 0;

  if (_setup.line_length != 0) {        /* line not blank */

      // at this point we have a parsed block
      // if items are to be remapped the flow is as follows:
      //
      // 1. push this block onto the remap stack because this might take several
      //    interp invcocations to finish, while other blocks will be parsed and
      //    executed by the oword subs. The top-of-stack block is the 'current
      //    remapped block' or CONTROLLING_BLOCK.
      //
      // 2. execute the remap stack top level block, ticking off all items which are done.
      //
      // 3. when a remap operation is encountered, this will result in a call like so:
      //   'o<replacement>call'.
      //
      //   this replacement call is parsed with read() into _setup.blocks[0] by the
      //   corresponding routine (see e.g. handling of T in interp_execute.cc)
      //   through calling into convert_remapped_code()
      //
      // 4. The oword call code might execute an optional prologue handler which is called
      //    when the subroutine environment is set up (parameters set, execution of
      //    body to begin). This is the way to set local named parameters e.g. for canned cycles.
      //
      // 5. The oword endsub/return code might call an epilogue handler
      //   which finishes any work at the Python level on endsub/return, and thereafter
      //   calls back into remap_finished().
      //
      // 6. The execution stops after parsing, and returns with an indication of the
      //   execution phase. We use negative values of enum steps to distinguish them
      //   from normal INTERP_* type codes which are all >= 0.
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
      if (eblock->remappings.size() > 0) {
	  std::set<int>::iterator it;
	  int next_remap = *eblock->remappings.begin();
	  logRemap("found remap %d in '%s', level=%d filename=%s line=%d",
		  next_remap,_setup.blocktext,_setup.call_level,_setup.filename,_setup.sequence_number);


	  CHP(enter_remap());
	  block_pointer cblock = &CONTROLLING_BLOCK(_setup);
	  cblock->phase = next_remap;
	  // execute up to the first remap including read() of its handler
	  // this also sets cblock->executing_remap
	  status = execute_block(cblock, &_setup);
#if 0
	  // this is too naive a test and needs improving (aka: not segfault). 
	  // It needs to kick in only for  new codes, not remapped ones, for which
	  // recursion just means 'use builtin semantics'
	  // add some kind of 'is_remapped_builtin()' macro or test method

	  // detect a remapping recursion.
	  // since each remapped item pushes a new block onto the remap stack, we walk
	  // the remap stack searching for an identical remap below the TOS
	  for (int i = _setup.remap_level - 1; i > 0; i--) {
	      if (_setup.blocks[i].executing_remap == cblock->executing_remap) {
		  ERS("recursive remapping for %s detected", cblock->executing_remap->name);
	      }
	  }
#endif
	  // All items up to the first remap item have been executed.
	  // The remap item procedure call has been parsed into _setup.blocks[0],
	  // the EXECUTING_BLOCK.
	  // after parsing a handler, execute_block() either fails to toplevel or
	  // returns the negative value of phase (to distinguish them from INTERP_* codes which are all >= 0)

	  if (status < 0) {

	      // the remap phase indicator was returned.
	      // sanity:
	      if (cblock->remappings.find(- status) == cblock->remappings.end()) {
		  ERS("BUG: execute_block: got %d - not in remappings() !! (next_remap=%d)",- status,next_remap);
	      }
	      logRemap("inital phase %d",-status);
	      if (MDImode) {
		  // need to trigger execution of parsed _setup.block1 here
		  // replicate MDI oword execution code here
		  if ((eblock->o_name != 0) ||
		      (_setup.mdi_interrupt)) { 

		      status = convert_control_functions(eblock, &_setup);
		      // a prolog might yield INTERP_EXECUTE_FINISH too
		      if (status == INTERP_EXECUTE_FINISH) 
			  _setup.mdi_interrupt = true;
		      CHP(status);
		      if (_setup.mdi_interrupt) {
			  _setup.mdi_interrupt = false;
			  MDImode = 1;
		      }
		      status = INTERP_OK;
		      while(MDImode && _setup.call_level) { // we are still in a subroutine
			  CHP(read(0));  // reads from current file and calls parse
			  status = execute();  // special handling for mdi errors
			  if (status == INTERP_EXECUTE_FINISH) 
			      _setup.mdi_interrupt = true;
			  CHP(status);
		      }
		      _setup.mdi_interrupt = false;
		      // at this point the MDI execution of a remapped block is complete.
		      logRemap("MDI remap execution complete status=%s\n",interp_status(status));
		      write_g_codes(eblock, &_setup);
		      write_m_codes(eblock, &_setup);
		      write_settings(&_setup);
		      return INTERP_OK;
		  }
	      } else {
		  // this should get the osub going
		  status = execute(0);
		  CHP(status);
		  // when this is done, blocks[0] will be executed as per standard case
		  // on endsub/return and g_codes/m_codes/settings recorded there.
	      }
	      if ((status != INTERP_OK) &&
		  (status != INTERP_EXECUTE_FINISH) && (status != INTERP_EXIT))
		  ERP(status);
	  } else {
	      CHP(status);
	  }
      } else {
	  // standard case: unremapped block execution
	  status = execute_block(eblock, &_setup);

	  write_g_codes(eblock, &_setup);
	  write_m_codes(eblock, &_setup);
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
        unwind_call(status, __FILE__,__LINE__,__FUNCTION__);
    }
    return status;
}

int Interp::execute()
{
  return Interp::execute(0);
}

int Interp::execute(const char *command, int line_number)
{
    int status;

    if(command && line_number)
        _setup.sequence_number = line_number;
    status = Interp::execute(command);
    if (status > INTERP_MIN_ERROR) {
	unwind_call(status, __FILE__,__LINE__,__FUNCTION__);
	logDebug("<-- execute(): error returned, clearing remap and call stack");
    }
    if ((_setup.call_level == 0) &&
	(status == INTERP_EXECUTE_FINISH) &&
	(_setup.mdi_interrupt)) {
	logDebug(" execute() clearing mdi_interrupt");
	_setup.mdi_interrupt = false;  // seems to work ok! FIXME mah is this needed?
    }
    return status;
}

// when a remapping sub finishes, the oword return/endsub handling code
// calls back in here to continue block execution
int Interp::remap_finished(int phase)
{
    int next_remap,status;
    block_pointer cblock = &CONTROLLING_BLOCK(_setup);

    logRemap("remap_finished phase=%d remap_level=%d call_level=%d filename=%s",
	  phase, _setup.remap_level,_setup.call_level,_setup.filename);

    // the controlling block had a remapped item, which just finished and the
    // oword return/endsub code called in here.
    if (phase < 0) {
	// paranoia.
	if (cblock->remappings.find(-phase) == cblock->remappings.end()) {
	    ERS("remap_finished: got %d - not in cblock.remappings!",phase);
	}
	// done with this phase.
	cblock->remappings.erase(-phase);
	// check the controlling block for the next remapped item
	std::set<int>::iterator it  = cblock->remappings.begin();

	if (it != cblock->remappings.end()) {
	    next_remap = *it;
	    cblock->phase = next_remap;

	    logRemap("starting phase %d  (remap_level=%d call_level=%d)",next_remap,_setup.remap_level,_setup.call_level);

	    // this will execute up to the next remap, and return
	    // after parsing the handler with read()
	    // so blocks[0] is armed (meaning: a osub call is parsed, but not executed yet)
	    status = execute_block(cblock, &_setup);
	    logRemap("phase %d started,  execute_block() returns %d", next_remap, status);

	    if (status < 0) {
		// a remap was parsed, get the block going
		return execute(0);
	    } else
		return status;
	} else {
	    if (cblock->remappings.size()) {
		ERS("BUG - remappings not empty");
	    }
	    // execution of controlling block finished executing a remap, and it contains no more
	    // remapped items. Execute any leftover items.
	    logRemap("no more remaps in controlling_block found (remap_level=%d call_level=%d), remappings size=%zd, dropping",
		     _setup.remap_level,_setup.call_level,cblock->remappings.size());

	    status = execute_block(cblock,  &_setup);

	    if ((status < 0) ||  (status > INTERP_MIN_ERROR)) {
		// status < 0 is a bug; might happen if find_remappings() failed to indicate the next remap
		logRemap("executing block leftover items: %s status=%s  remap_level=%d call_level=%d (failing)",
			 status < 0 ? "BUG":"ERROR", interp_status(status),_setup.remap_level,_setup.call_level);
		if (status < 0)
		    ERS("BUG - check find_remappings()!! status=%d nesting=%d",status,_setup.remap_level);
	    } else {
		// we're done with this remapped block.
		// execute_block may return INTERP_EXECUTE_FINISH if a probe, input or toolchange
		// command was executed.
		// not sure what INTERP_ENDFILE & INTERP_EXIT really mean here.
		// if ((status == INTERP_OK) || (status == INTERP_ENDFILE) || (status == INTERP_EXIT) || (status == INTERP_EXECUTE_FINISH)) {
		// leftover items finished. Drop a remapping level.

		CHP(leave_remap());
		logRemap("executing block leftover items complete, status=%s  remap_level=%d call_level=%d tc=%d probe=%d input=%d mdi_interrupt=%d  line=%d backtoline=%d",
			 interp_status(status),_setup.remap_level,_setup.call_level,_setup.toolchange_flag,
			_setup.probe_flag,_setup.input_flag,_setup.mdi_interrupt,_setup.sequence_number,
			 cblock->line_number);
	    }
	}
	return status;
    } else {
	// "should not happen"
	ERS("BUG: remap_finished(): phase=%d nesting=%d",
	    phase, _setup.remap_level);
    }
    return INTERP_OK;
}


// examine a block for an active items which are remapped
// insert all remapped item phases into block.remapping set
// return number of remaps found
int Interp::find_remappings(block_pointer block, setup_pointer settings)
{
    if (block->f_flag && remapping("F")) {
	if (remap_in_progress("F"))
	    CONTROLLING_BLOCK(*settings).builtin_used = true;
	else
	    block->remappings.insert(STEP_SET_FEED_RATE);
    }
    if (block->s_flag && remapping("S")) {
	if (remap_in_progress("S"))
	    CONTROLLING_BLOCK(*settings).builtin_used = true;
	else
	    block->remappings.insert(STEP_SET_SPINDLE_SPEED);
    }
    if (block->t_flag && remapping("T")) {
	if (remap_in_progress("T"))
	    CONTROLLING_BLOCK(*settings).builtin_used = true;
	else
	    block->remappings.insert(STEP_PREPARE);
    }
    // User defined M-Codes in group 5
    if (IS_USER_MCODE(block,settings,5))
	block->remappings.insert(STEP_M_5);

    // User defined M-Codes in group 6 (including M6, M61)
    // call the remap procedure if it the code in that group is remapped unless:
    // it's an M6 or M61 and a remap is in progress
    // (recursion case)
    if (IS_USER_MCODE(block,settings,6) &&  
	!(((block->m_modes[6] == 6) && remap_in_progress("M6")) ||
	  ((block->m_modes[6] == 61) && remap_in_progress("M61")))) {  
	block->remappings.insert(STEP_M_6); // then call the remap procedure
    } // else we get the builtin behaviour
    
    // User defined M-Codes in group 7
    if (IS_USER_MCODE(block,settings,7))
	block->remappings.insert(STEP_M_7);

    // User defined M-Codes in group 8
    if (IS_USER_MCODE(block,settings,8))
	block->remappings.insert(STEP_M_8);

    // User defined M-Codes in group 9
    if (IS_USER_MCODE(block,settings,9))
	block->remappings.insert(STEP_M_9);

    // User defined M-Codes in group 10
    if (IS_USER_MCODE(block,settings,10))
	block->remappings.insert(STEP_M_10);

    // User-defined motion codes (G0 to G3, G33, G73, G76, G80 to G89)
    // as modified (possibly) by G53.
    int mode = block->g_modes[GM_MOTION];
    if ((mode != -1) && IS_USER_GCODE(mode))
	block->remappings.insert(STEP_MOTION);
    
    // this makes it possible to call remapped codes like cycles:
    // G84.2 x1 y2 
    // x3
    // will execute 'G84.2 x1 y2', then 'G84.2 x3 y2'
    // provided the remap function explicitly sets motion_mode like so:

    // def g842(self,**words):
    //     ....
    //     self.motion_mode = 842
    //     return INTERP_OK

    mode = block->motion_to_be;
    if ((mode != -1) && IS_USER_GCODE(mode)) {
	block->remappings.insert(STEP_MOTION);
    }

    // User defined M-Codes in group 4 (stopping)
    if (IS_USER_MCODE(block,settings,4)) {

	if (remap_in_progress("M0") ||
	    remap_in_progress("M1") ||
	    remap_in_progress("M60"))  { // detect recursion case

	    // these require real work.
	    // remap_in_progress("M2") ||
	    // remap_in_progress("M60")
	    CONTROLLING_BLOCK(*settings).builtin_used = true;
	} else {
	    block->remappings.insert(STEP_MGROUP4);
	}
    }
    return block->remappings.size();
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

  // interpreter shutdown Python hook
  if (python_plugin->is_callable(NULL, DELETE_FUNC)) {

      bp::object retval, tupleargs, kwargs;
      bp::list plist;

      plist.append(_setup.pythis); // self
      tupleargs = bp::tuple(plist);
      kwargs = bp::dict();

      python_plugin->call(NULL, DELETE_FUNC, tupleargs, kwargs, retval);
      if (python_plugin->plugin_status() == PLUGIN_EXCEPTION) {
	  ERM("pycall(%s):\n%s", INIT_FUNC,
	      python_plugin->last_exception().c_str());
	  // this likely wont make it to the UI's any more so bark on stderr
	  fprintf(stderr, "%s\n",savedError);
      }
  }

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
  IniFile::ErrorCode r;

  INIT_CANON();

  iniFileName = getenv("INI_FILE_NAME");

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
  _setup.value_returned = 0;
  _setup.remap_level = 0; // remapped blocks stack index
  _setup.call_state = CS_NORMAL;

  // default arc radius tolerances
  // we'll try to override these from the ini file below
  _setup.center_arc_radius_tolerance_inch = CENTER_ARC_RADIUS_TOLERANCE_INCH;
  _setup.center_arc_radius_tolerance_mm = CENTER_ARC_RADIUS_TOLERANCE_MM;

  if(iniFileName != NULL) {

      IniFile inifile;
      if (inifile.Open(iniFileName) == false) {
          fprintf(stderr,"Unable to open inifile:%s:\n", iniFileName);
      } else {

          const char *inistring;

          inifile.Find(&_setup.tool_change_at_g30, "TOOL_CHANGE_AT_G30", "EMCIO");
          inifile.Find(&_setup.tool_change_quill_up, "TOOL_CHANGE_QUILL_UP", "EMCIO");
          inifile.Find(&_setup.tool_change_with_spindle_on, "TOOL_CHANGE_WITH_SPINDLE_ON", "EMCIO");
          inifile.Find(&_setup.a_axis_wrapped, "WRAPPED_ROTARY", "AXIS_3");
          inifile.Find(&_setup.b_axis_wrapped, "WRAPPED_ROTARY", "AXIS_4");
          inifile.Find(&_setup.c_axis_wrapped, "WRAPPED_ROTARY", "AXIS_5");
          inifile.Find(&_setup.random_toolchanger, "RANDOM_TOOLCHANGER", "EMCIO");
          inifile.Find(&_setup.feature_set, "FEATURES", "RS274NGC");

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
		  log_file = stderr;
		  logDebug( "(%d): Unable to open log file:%s, using stderr",
			  getpid(), inistring);
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
            char expandinistring[LINELEN];
            if (inifile.TildeExpansion(inistring,expandinistring,sizeof(expandinistring))) {
                   logDebug("TildeExpansion failed for: %s",inistring);
            }
            if (realpath(expandinistring, _setup.program_prefix) == NULL){
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
                 _setup.subroutines[dct] = NULL;
            }

            strcpy(tmpdirs,inistring);
            nextdir = strtok(tmpdirs,":");  // first token
            dct = 0;
            while (1) {
                char tmp_path[PATH_MAX];
                char expandnextdir[LINELEN];
                if (inifile.TildeExpansion(nextdir,expandnextdir,sizeof(expandnextdir))) {
                   logDebug("TildeExpansion failed for: %s",nextdir);
                }
                if (realpath(expandnextdir, tmp_path) == NULL){
                   //realpath didn't find the directory
                   logDebug("realpath failed to find subroutines[%d]:%s:",dct,nextdir);
                    _setup.subroutines[dct] = NULL;
                } else {
		    _setup.subroutines[dct] = strstore(tmp_path);
                    logDebug("program prefix[%d]:%s",dct,_setup.subroutines[dct]);
		    dct++;
                }
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
          // subroutine to execute on aborts - for instance to retract
          // toolchange HAL pins
          if (NULL != (inistring = inifile.Find("ON_ABORT_COMMAND", "RS274NGC"))) {
	      _setup.on_abort_command = strstore(inistring);
              logDebug("_setup.on_abort_command=%s", _setup.on_abort_command);
          } else {
	      _setup.on_abort_command = NULL;
          }

	  // initialize the Python plugin singleton
          if (NULL != (inistring = inifile.Find("TOPLEVEL", "PYTHON"))) {
	      int status = python_plugin->configure(iniFileName,"PYTHON");
	      if (status != PLUGIN_OK) {
		  Error("Python plugin configure() failed, status = %d", status);
	      }
	  }
 
	  int n = 1;
	  int lineno = -1;
	  _setup.g_remapped.clear();
	  _setup.m_remapped.clear();
	  _setup.remaps.clear();
	  while (NULL != (inistring = inifile.Find("REMAP", "RS274NGC",
						   n, &lineno))) {

	      CHP(parse_remap( inistring,  lineno));
	      n++;
	  }

          // if exist and within bounds, apply ini file arc tolerances
          // limiting figures are defined in interp_internal.hh

          r = inifile.Find(
              &_setup.center_arc_radius_tolerance_inch,
              MIN_CENTER_ARC_RADIUS_TOLERANCE_INCH,
              CENTER_ARC_RADIUS_TOLERANCE_INCH,
              "CENTER_ARC_RADIUS_TOLERANCE_INCH",
              "RS274NGC"
          );
          if ((r != IniFile::ERR_NONE) && (r != IniFile::ERR_TAG_NOT_FOUND)) {
              Error("invalid [RS275NGC]CENTER_ARC_RADIUS_TOLERANCE_INCH in ini file\n");
          }

          r = inifile.Find(
              &_setup.center_arc_radius_tolerance_mm,
              MIN_CENTER_ARC_RADIUS_TOLERANCE_MM,
              CENTER_ARC_RADIUS_TOLERANCE_MM,
              "CENTER_ARC_RADIUS_TOLERANCE_MM",
              "RS274NGC"
          );
          if ((r != IniFile::ERR_NONE) && (r != IniFile::ERR_TAG_NOT_FOUND)) {
              Error("invalid [RS275NGC]CENTER_ARC_RADIUS_TOLERANCE_MM in ini file\n");
          }

	  // ini file g52/g92 offset persistence default setting
	  inifile.Find(&_setup.disable_g92_persistence,
		       "DISABLE_G92_PERSISTENCE",
		       "RS274NGC");

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

  // Restore G92 offset if DISABLE_G92_PERSISTENCE not set in .ini file.
  // This can't be done with the static _required_parameters[], where
  // the .vars file contents would reflect that setting, so instead
  // edit the restored parameters here.
  if (_setup.disable_g92_persistence)
      // Persistence disabled:  clear g92 parameters
      for (k = 5210; k < 5220; k++)
	  pars[k] = 0;

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
//  _setup.tool_offset_index = 1;  // unused - removed, mah
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
  _setup.spindle_mode = CONSTANT_RPM;
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
  _setup.offset_map.clear();

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

  // call __init__(self) once in toplevel module if defined
  // once fully set up and sync()ed
  if ((iniFileName != NULL) && _setup.init_once && PYUSABLE ) {

      // initialize any python global predefined named parameters
      // walk the namedparams module for callables and add their names as predefs
      try {
	  bp::object npmod =  python_plugin->main_namespace[NAMEDPARAMS_MODULE];
	  bp::dict predef_dict = bp::extract<bp::dict>(npmod.attr("__dict__"));
	  bp::list iterkeys = (bp::list) predef_dict.iterkeys();
	  for (int i = 0; i < bp::len(iterkeys); i++)  {
	      std::string key = bp::extract<std::string>(iterkeys[i]);
	      bp::object value = predef_dict[key];
	      if (PyCallable_Check(value.ptr())) {
		  CHP(init_python_predef_parameter(key.c_str()));
	      }
	  }
      }
      catch (bp::error_already_set) {
	  std::string exception_msg;
	  bool unexpected = false;
	  // KeyError is ok - this means the namedparams module doesnt exist
	  if (!PyErr_ExceptionMatches(PyExc_KeyError)) {
	      // something else, strange
	      exception_msg = handle_pyerror();
	      unexpected = true;
	  }
	  bp::handle_exception();
	  PyErr_Clear();
	  CHKS(unexpected, "exception adding Python predefined named parameter: %s", exception_msg.c_str());
      }

      if (python_plugin->is_callable(NULL, INIT_FUNC)) {

	  bp::object retval, tupleargs, kwargs;
	  bp::list plist;

	  plist.append(_setup.pythis); // self
	  tupleargs = bp::tuple(plist);
	  kwargs = bp::dict();

	  python_plugin->call(NULL, INIT_FUNC, tupleargs, kwargs, retval);
	  CHKS(python_plugin->plugin_status() == PLUGIN_EXCEPTION,
	       "pycall(%s):\n%s", INIT_FUNC,
	       python_plugin->last_exception().c_str());
      }
  }
  _setup.init_once = 0;
  
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

  logOword("open()");
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

int Interp::read_inputs(setup_pointer settings)
{
    // logDebug("read_inputs probe=%d input=%d toolchange=%d",
    // 	     settings->probe_flag, settings->toolchange_flag, settings->input_flag);
    if (settings->probe_flag) {
	CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
	     NCE_QUEUE_IS_NOT_EMPTY_AFTER_PROBING);
	set_probe_data(&_setup);
	settings->probe_flag = false;
    }
    if (settings->toolchange_flag) {
	CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
	     _("Queue is not empty after tool change"));
	refresh_actual_position(&_setup);
	load_tool_table();
	settings->toolchange_flag = false;
    }
    // always track toolchanger-fault and toolchanger-reason codes
    settings->parameters[5600] = GET_EXTERNAL_TC_FAULT();
    settings->parameters[5601] = GET_EXTERNAL_TC_REASON();

    if (settings->input_flag) {
	CHKS((GET_EXTERNAL_QUEUE_EMPTY() == 0),
	     NCE_QUEUE_IS_NOT_EMPTY_AFTER_INPUT);
	if (settings->input_digital) { // we are checking for a digital input
	    settings->parameters[5399] =
		GET_EXTERNAL_DIGITAL_INPUT(settings->input_index,
					   (settings->parameters[5399] != 0.0));
	} else { // checking for analog input
	    settings->parameters[5399] =
		GET_EXTERNAL_ANALOG_INPUT(settings->input_index, settings->parameters[5399]);
	}
	settings->input_flag = false;
    }
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

  // this input reading code is in the wrong place. It should be executed
  // in sync(), not here. This would make correct parameter values available 
  // without doing a read() (e.g. from Python).
  // Unfortunately synch() isnt called in preview (gcodemodule)

#if 0
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
#endif

  // Support for restartable Python handlers during Auto mode
  // Conceptually a O_call or O_endsub/O_return block might need to be executed several
  // times in a row until it finally returns INTERP_OK, the reason being that all Python
  // procedures might 'yield INTERP_EXECUTE_FINISH' or execute a queue buster an arbitrary number
  // of times. So they need to be called again post-sync and post-read-input possibly several times.
  // 
  // the task readahead logic assumes a block execution may result in a single INTERP_EXECUTE_FINISH
  // and readahead is started therafter immediately. Modifying the readahead logic would be a massive
  // change. Therefore we use the trick to suppress reading the next block as required, which means
  // we will get several calls to execute() in a row which are used to finish the handlers. This is
  // needed for remapped codes which might involve up to three Python handlers, and Python oword subs.
  // Note this is not an issue for NGC oword procedures. The call/return logic will set _setup.call_state to
  // CS_REEXEC_PROLOG, CS_REEXEC_PYBODY, CS_REEXEC_EPILOG or CS_REEXEC_PYOSUB before returning, which 
  // also indicates the point which handler needs to be restarted
  // 
  // We use the following conditions to 'skip reading the next block and stay on the same block' 
  // until done as follows:
  // 
  // 1. block.o_type = O_call and
  //    block.call_type in {CT_PYTHON_OWORD_SUB, CT_REMAP} and
  //    _setup.call_state > CS_NORMAL
  // 
  // 2. block.o_type in {O_endsub, O_return} and
  //    block.call_type in {CT_PYTHON_OWORD_SUB, CT_REMAP} and
  //    _setup.call_state > CS_NORMAL
  // 
  // handlers eventually return INTERP_OK, which sets _setup.call_state to CS_NORMAL. Then
  // normal readahead continues.
  // A call frame is tagged with the eblock->call_type since this potentially needs to persist across
  // several blocks. Inside the execute_call()/execute_return() logic we use the frame call type
  // to decide what to do.
  // The handler reexec code will call read_inputs() just before continuation.
   
  block_pointer eblock = &EXECUTING_BLOCK(_setup);

  if ((_setup.call_state > CS_NORMAL) && 
      (eblock->call_type > CT_NGC_OWORD_SUB)  && 
      ((eblock->o_type == O_call) ||
       (eblock->o_type == O_return) ||
       (eblock->o_type == O_endsub))) {

      logDebug("read(): skipping read");
      _setup.line_length = 0;
      _setup.linetext[0] = 0;
      return INTERP_OK;
  }
  _setup.call_state = CS_NORMAL;
  CHP(read_inputs(&_setup));


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

  read_status =
    read_text(command, _setup.file_pointer, _setup.linetext,
              _setup.blocktext, &_setup.line_length);

  if (read_status == INTERP_ERROR && _setup.skipping_to_sub) {
    _setup.skipping_to_sub = NULL;
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

	// mah: FIXME test this - no idea what this is about; o_number is history
        if (EXECUTING_BLOCK(_setup).o_type != O_none)  {
            // Clear o_type, this isn't line isn't a command...
            EXECUTING_BLOCK(_setup).o_type = 0;
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
	unwind_call(status, __FILE__,__LINE__,__FUNCTION__);
    }
    return status;
}

// Reset interpreter state and  terminate a call in progress by
// falling back to toplevel in a controlled way. Idempotent.
int Interp::unwind_call(int status, const char *file, int line, const char *function)
{
    logDebug("unwind_call: call_level=%d status=%s from %s %s:%d",
	     _setup.call_level, interp_status(status), function, file, line);

    for(; _setup.call_level > 0; _setup.call_level--) {
	int i;
	context * sub = _setup.sub_context + _setup.call_level - 1;
	free_named_parameters(&_setup.sub_context[_setup.call_level]);
	if(sub->subName) {
	    logDebug("unwind_call leaving sub '%s'", sub->subName);
	    sub->subName = 0;
	}

	for(i=0; i<INTERP_SUB_PARAMS; i++) {
	    _setup.parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
		sub->saved_params[i];
	}

	// When called from Interp::close via Interp::reset, this one is NULL
	if (!_setup.file_pointer) continue;

	// some frames may not have a filename and hence a position to seek to
	// on return, like Python handlers
	// needed to make sure this works in rs274 -n 0 (continue on error) mode
	if (sub->filename && sub->filename[0]) {
	    if(0 != strcmp(_setup.filename, sub->filename)) {
		fclose(_setup.file_pointer);
		_setup.file_pointer = fopen(sub->filename, "r");
		logDebug("unwind_call: reopening '%s' at %ld",
			 sub->filename, sub->position);
		strcpy(_setup.filename, sub->filename);
	    }
	    fseek(_setup.file_pointer, sub->position, SEEK_SET);
	}
	_setup.sequence_number = sub->sequence_number;
	logDebug("unwind_call: setting sequence number=%d from frame %d",
		_setup.sequence_number,_setup.call_level);

    }
    // call_level == 0 here.
 
    if(_setup.sub_name) {
	logDebug("unwind_call: exiting current sub '%s'\n", _setup.sub_name);
	_setup.sub_name = 0;
    }
    _setup.remap_level = 0; // reset remapping stack
    _setup.defining_sub = 0;
    _setup.skipping_o = 0;
    _setup.skipping_to_sub = 0;
    _setup.offset_map.clear();
    _setup.mdi_interrupt = false;

    qc_reset();
    return INTERP_OK;
}

int Interp::read() {
  return read(0);
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
  _setup.call_state = CS_NORMAL;
  //!!!KL According to the comment,
  //!!!KL this should not be here because this is for
  //!!!KL more than one line.
  //!!!KL But the comment seems wrong -- it is only called at open, close,
  //!!!KL init times which should affect the more global structure.
  //!!!KL (also called by external -- but probably OK)
  //
  // initialization stuff for subroutines and control structures
    _setup.linetext[0] = 0;
    _setup.blocktext[0] = 0;
    _setup.line_length = 0;

    unwind_call(INTERP_OK, __FILE__,__LINE__,__FUNCTION__);
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
        if (k > variable) {
          fclose(infile);
          ERS(NCE_PARAMETER_FILE_OUT_OF_ORDER);
        } else if (k == variable) {
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
  char line[PATH_MAX];
  int variable;
  double value;
  int required;                 // number of next required parameter
  int index;                    // index into _required_parameters
  int k;

  if(access(filename, F_OK)==0) 
  {
    // rename as .bak
    int r;
    r = snprintf(line, sizeof(line), "%s%s", filename, RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX);
    CHKS((r >= (int)sizeof(line)), NCE_CANNOT_CREATE_BACKUP_FILE);
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
        if (k > variable) {
          fclose(infile);
          fclose(outfile);
          ERS(NCE_PARAMETER_FILE_OUT_OF_ORDER);
        } else if (k == variable) {
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

  // read_inputs(&_setup); // input/probe/toolchange 

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

void Interp::setError(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    vsnprintf(savedError, LINELEN, fmt, ap);

    va_end(ap);
}

const char *Interp::getSavedError()
{
    return savedError;
}

// set error message text without going through printf format interpretation
int Interp::setSavedError(const char *msg)
{
    savedError[0] = '\0';
    strncpy(savedError, msg, LINELEN);
    return INTERP_OK;
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

char * Interp::error_text(int error_code,        //!< code number of error
                         char *error_text,      //!< char array to copy error text into  
                         size_t max_size)  //!< maximum number of characters to copy
{
    if(error_code == INTERP_ERROR)
    {
        strncpy(error_text, savedError, max_size);
        error_text[max_size-1] = 0;

        return error_text;
    }

    error_text[0] = 0;
    return error_text;
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

char *Interp::file_name(char *file_name,        //!< string: to copy file name into
                        size_t max_size)   //!< maximum number of characters to copy
{
  if (strlen(_setup.filename) < ((size_t) max_size))
    strcpy(file_name, _setup.filename);
  else
    file_name[0] = 0;
  return file_name;
}

/***********************************************************************/

/*! Interp::line_length

Returned Value: the length of the most recently read line

Side Effects: none

Called By: external programs

*/

size_t Interp::line_length()
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

char *Interp::line_text(char *line_text,        //!< string: to copy line into
                        size_t max_size)   //!< maximum number of characters to copy
{
  size_t n;
  char *the_text;

  the_text = _setup.linetext;
  for (n = 0; n < (max_size - 1); n++) {
    if (the_text[n] != 0)
      line_text[n] = the_text[n];
    else
      break;
  }
  line_text[n] = 0;
  return line_text;
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

char *Interp::stack_name(int stack_index,       //!< index into stack of function names
                         char *function_name,   //!< string: to copy function name into
                         size_t max_size)  //!< maximum number of characters to copy
{
  size_t n;
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
  return function_name;
}

/***********************************************************************/

/* Interp::ini_load()

Returned Value: INTERP_OK, RS274NGC_ERROR

Side Effects:
   An INI file containing values for global variables is used to
   update the globals

Called By:
   emctask before calling Interp::init()

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
        if (_parameter_file_name[LINELEN-1] != '\0') {
            logDebug("%s:[RS274NGC]PARAMETER_FILE is too long (max len %d)", filename, LINELEN-1);
            inifile.Close();
            _parameter_file_name[0] = '\0';
            return -1;
        }
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

int Interp::enter_remap(void)
{
    _setup.remap_level++;
    if (_setup.remap_level == MAX_NESTED_REMAPS) {
	_setup.remap_level = 0;
	ERS("maximum nesting of remapped blocks execeeded");
    }

    // push onto block stack
    CONTROLLING_BLOCK(_setup) = EXECUTING_BLOCK(_setup);
    CONTROLLING_BLOCK(_setup).breadcrumbs = 0; // clear trail
    // set later but tested for in remap_in_progress()
    CONTROLLING_BLOCK(_setup).executing_remap = NULL;

    // remember the line where remap was discovered
    if (_setup.remap_level == 1) {
	logRemap("enter_remap: toplevel - saved_line_number=%d",_setup.sequence_number);
	CONTROLLING_BLOCK(_setup).saved_line_number  =
	    _setup.sequence_number;
    } else {
	logRemap("enter_remap into %d - saved_line_number=%d",
		 _setup.remap_level,
		 EXECUTING_BLOCK(_setup).saved_line_number);
	CONTROLLING_BLOCK(_setup).saved_line_number  =
	    EXECUTING_BLOCK(_setup).saved_line_number;
    }
    _setup.sequence_number = 0;
    return INTERP_OK;
}

int Interp::leave_remap(void)
{
    // restore the line number where remap was found
    if (_setup.remap_level == 1) {
	// dropping to top level, so pass onto _setup
	_setup.sequence_number = CONTROLLING_BLOCK(_setup).saved_line_number;
	logRemap("leave_remap into toplevel, restoring seqno=%d",_setup.sequence_number);

    } else {
	// just dropping a nesting level
	EXECUTING_BLOCK(_setup).saved_line_number =
	    CONTROLLING_BLOCK(_setup).saved_line_number ;
	logRemap("leave_remap from %d propagate saved_line_number=%d",
		 _setup.remap_level,
		 EXECUTING_BLOCK(_setup).saved_line_number);
    }
    _setup.blocks[_setup.remap_level].executing_remap = NULL;
    _setup.remap_level--; // drop one nesting level
    if (_setup.remap_level < 0) {
	ERS("BUG: remap_level < 0 : %d",_setup.remap_level);
    }
    return INTERP_OK;
}

int Interp::on_abort(int reason, const char *message)
{
    logDebug("on_abort reason=%d message='%s'", reason, message);

    reset();
    _setup.mdi_interrupt = false;

    // clear in case set by an interrupted remapped procedure
    // if set, may cause a "Queue is not empty after tool change" error
    _setup.toolchange_flag = false;
    _setup.probe_flag = false;
    _setup.input_flag = false;

    if (_setup.on_abort_command == NULL)
	return -1;

    char cmd[LINELEN];

    snprintf(cmd,sizeof(cmd), "%s [%d]",_setup.on_abort_command, reason);
    int status = execute(cmd);

    ERP(status);
    return status;
}

// spun out from interp_o_word so we can use it to test ngc file accessibility during
// config file parsing (REMAP... ngc=<basename>)
FILE *Interp::find_ngc_file(setup_pointer settings,const char *basename, char *foundhere )
{
    FILE *newFP;
    char tmpFileName[PATH_MAX+1];
    char newFileName[PATH_MAX+1];
    char foundPlace[PATH_MAX+1];
    int  dct;

    // look for a new file
    sprintf(tmpFileName, "%s.ngc", basename);

    // find subroutine by search: program_prefix, subroutines, wizard_root
    // use first file found

    // first look in the program_prefix place
    sprintf(newFileName, "%s/%s", settings->program_prefix, tmpFileName);
    newFP = fopen(newFileName, "r");

    // then look in the subroutines place
    if (!newFP) {
	for (dct = 0; dct < MAX_SUB_DIRS; dct++) {
	    if (!settings->subroutines[dct])
		continue;
	    sprintf(newFileName, "%s/%s", settings->subroutines[dct], tmpFileName);
	    newFP = fopen(newFileName, "r");
	    if (newFP) {
		// logOword("fopen: |%s|", newFileName);
		break; // use first occurrence in dir search
	    }
	}
    }
    // if not found, search the wizard tree
    if (!newFP) {
	int ret;
	ret = findFile(settings->wizard_root, tmpFileName, foundPlace);

	if (INTERP_OK == ret) {
	    // create the long name
	    sprintf(newFileName, "%s/%s",
		    foundPlace, tmpFileName);
	    newFP = fopen(newFileName, "r");
	}
    }
    if (foundhere && (newFP != NULL)) 
	strcpy(foundhere, newFileName);
    return newFP;
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

context_struct::context_struct()
: position(0), sequence_number(0), filename(""), subName(""),
context_status(0), call_type(0), py_return_type(0), py_returned_double(0),
py_returned_int(0)
{
    memset(saved_params, 0, sizeof(saved_params));
    memset(saved_g_codes, 0, sizeof(saved_g_codes));
    memset(saved_m_codes, 0, sizeof(saved_m_codes));
    memset(saved_settings, 0, sizeof(saved_settings));
}
