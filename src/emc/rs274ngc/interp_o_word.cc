/********************************************************************
* Description: interp_o_word.cc
*
*
* Author: Kenneth Lerman
* License: GPL Version 2
* System: Linux
*    
* Copyright 2005 All rights reserved.
*
* Last change: Michael Haberler 7/2011
*
********************************************************************/

#include <boost/python.hpp>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"

namespace bp = boost::python;

//========================================================================
// Functions for control stuff (O-words)
//========================================================================

/*
  Given the root of a directory tree and a file name,
  find the path to the file, if any.
*/

int Interp::findFile( // ARGUMENTS
		     char *direct,  // the directory to start looking in
		     char *target,  // the name of the file to find
		     char *foundFileDirect) // where to store the result
{
    FILE *file;
    DIR *aDir;
    struct dirent *aFile;
    char targetPath[PATH_MAX+1];

    snprintf(targetPath, PATH_MAX, "%s/%s", direct, target);
    file = fopen(targetPath, "r");
    if (file) {
        strncpy(foundFileDirect, direct, PATH_MAX);
        fclose(file);
        return INTERP_OK;
    }
    aDir = opendir(direct);
    if (!aDir) {
	ERS(NCE_FILE_NOT_OPEN);
    }

    while ((aFile = readdir(aDir))) {
        if (aFile->d_type == DT_DIR &&
	    (0 != strcmp(aFile->d_name, "..")) &&
	    (0 != strcmp(aFile->d_name, "."))) {

            char path[PATH_MAX+1];
            snprintf(path, PATH_MAX, "%s/%s", direct, aFile->d_name);
            if (INTERP_OK == findFile(path, target, foundFileDirect)) {
	        closedir(aDir);
                return INTERP_OK;
            }
        }
    }
    closedir(aDir);
    ERS(NCE_FILE_NOT_OPEN);
}


/*
 *  this now uses STL maps for offset access
 */
int Interp::control_save_offset(block_pointer block,        /* pointer to a block of RS274/NGC instructions */
				setup_pointer settings)     /* pointer to machine settings */
{
  static char name[] = "control_save_offset";
  offset_pointer op = NULL;

  logOword("Entered:%s for o_name:|%s|", name, block->o_name);

  if (control_find_oword(block, settings, &op) == INTERP_OK) {
      // already exists
      ERS(_("File:%s line:%d redefining sub: o|%s| already defined in file:%s"),
	  settings->filename, settings->sequence_number,
	  block->o_name,
	  op->filename);
  }
  offset new_offset;

  new_offset.type = block->o_type;
  new_offset.offset = block->offset;
  new_offset.filename = strstore(settings->filename);
  new_offset.repeat_count = -1;
  // the sequence number has already been bumped, so save
  // the proper value
  new_offset.sequence_number = settings->sequence_number - 1;
  settings->offset_map[block->o_name] = new_offset;

  return INTERP_OK;
}


int Interp::control_find_oword(block_pointer block,      // pointer to block
			       setup_pointer settings,   // pointer to machine settings
			       offset_pointer *op)       // pointer to offset descriptor
{
    static char name[] = "control_find_oword";
    offset_map_iterator it;

    it = settings->offset_map.find(block->o_name);
    if (it != settings->offset_map.end()) {
	*op =  &it->second;
	return INTERP_OK;
    } else {
	logOword("%s: Unknown oword name: |%s|", name, block->o_name);
	ERS(NCE_UNKNOWN_OWORD_NUMBER);
    }
}

#if 0
int Interp::execute_call(setup_pointer settings, int what) 
{
    context_pointer previous_frame, new_frame;
    block_pointer cblock,eblock;
    bool is_py_remap_handler; // the sub is executing on behalf of a remapped code
    bool is_remap_handler; // the sub is executing on behalf of a remapped code
    bool is_py_callable;   // the sub name is actually Python
    bool is_py_osub;  // a plain osub whose name is a python callable
    int status = INTERP_OK;
    int i;
    bool py_exception = false;
    extern const char * _entrynames[];

    previous_frame = &settings->sub_context[settings->call_level];
    new_frame = &settings->sub_context[settings->call_level + 1];

    // aquire the 'remap_frame' a.k.a controlling block
    cblock = &CONTROLLING_BLOCK(*settings);
    eblock = &EXECUTING_BLOCK(*settings);

    logOword("execute_call %s name=%s", _entrynames[what], 
	     what > NORMAL_RETURN? new_frame->subName : eblock->o_name);

    // determine if this sub is the  body executing on behalf of a remapped code
    // we're loosing a bit of context by funneling everything through the osub call
    // interface, which needs to be reestablished here but it's worth the generality
    is_remap_handler =
	(what > FINISH_OWORDSUB) || // restarting a Python handler which yielded INTERP_EXECUTE_FINISH
	(cblock->executing_remap != NULL) &&
	(((cblock->executing_remap->remap_py != NULL) &&
	  (!strcmp(cblock->executing_remap->remap_py, eblock->o_name))) ||
	 (((cblock->executing_remap->remap_ngc != NULL) &&
	   (!strcmp(cblock->executing_remap->remap_ngc, eblock->o_name)))));

    is_py_callable = 	(what > NORMAL_RETURN) || // already determined on initial call
	is_pycallable(settings, is_remap_handler ? REMAP_MODULE : OWORD_MODULE, eblock->o_name);
    is_py_osub = is_py_callable && ! is_remap_handler;

    is_py_remap_handler = (what ==  FINISH_BODY) || 
	((cblock->executing_remap != NULL) &&
	((cblock->executing_remap->remap_py != NULL) &&
	 (!strcmp(cblock->executing_remap->remap_py, eblock->o_name))));
	

    switch (what) {
    case NORMAL_CALL:
	// copy parameters from context
	// save old values of parameters
	// save current file position in context
	// if we were skipping, no longer
	if (settings->skipping_o) {
	    logOword("case O_call -- no longer skipping to:|%s|",
		     settings->skipping_o);
	    settings->skipping_o = NULL;
	}
	if (settings->call_level >= INTERP_SUB_ROUTINE_LEVELS) {
	    ERS(NCE_TOO_MANY_SUBROUTINE_LEVELS);
	}
	if (is_py_osub) {
	    logDebug("O_call: vanilla osub pycall(%s), preparing positional args", eblock->o_name);
	    py_exception = false;

// moved to execute_pycall

	    // try {
	    // 	// call with list of positional parameters
	    // 	// no saving needed - this is call by value
	    // 	bp::list plist;
	    // 	plist.append(settings->pythis); // self
	    // 	for(int i = 0; i < eblock->param_cnt; i++)
	    // 	    plist.append(eblock->params[i]);
	    // 	eblock->tupleargs = bp::tuple(plist);
	    // 	eblock->kwargs = bp::dict();
	    // }
	    // catch (bp::error_already_set) {
	    // 	if (PyErr_Occurred()) {
	    // 	    PyErr_Print();
	    // 	}
	    // 	bp::handle_exception();
	    // 	PyErr_Clear();
	    // 	py_exception = true;
	    // }
	    // if (py_exception) {
	    // 	CHKS(py_exception,"O_call: Py exception preparing arguments for %s",
	    // 	     eblock->o_name);
	    // }
	} else {
	    for(i = 0; i < INTERP_SUB_PARAMS; i++)	{
		previous_frame->saved_params[i] =
		    settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM];
		settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM] =
		    eblock->params[i];
	    }
	}

	// if the previous file was NULL, mark positon as -1 so as not to
	// reopen it on return.
	if (settings->file_pointer == NULL) {
	    previous_frame->position = -1;
	} else {
	    previous_frame->position = ftell(settings->file_pointer);
	}

	// save return location
	previous_frame->filename = strstore(settings->filename);
	previous_frame->sequence_number = settings->sequence_number;
	logOword("return location: %s:%d offset=%ld cl=%d", 
		 previous_frame->filename,
		 previous_frame->sequence_number,
		 previous_frame->position,
		 settings->call_level);
		 

	// set the new subName
	new_frame->subName = eblock->o_name;
	settings->call_level++;

	// let any  Oword sub know the number of parameters
	if (!(is_py_osub || is_py_remap_handler)) {
	    CHP(add_named_param("n_args", PA_READONLY));
	    CHP(store_named_param(settings, "n_args",
				  (double )eblock->param_cnt,
				  OVERRIDE_READONLY));
	}
	// fall through on purpose 

    case FINISH_PROLOG:
	if (HAS_PYTHON_PROLOG(cblock->executing_remap)) {
	    logRemap("O_call: prolog %s for remap %s (%s)  cl=%d rl=%d",
		     cblock->executing_remap->prolog_func,
		     REMAP_FUNC(cblock->executing_remap),
		     cblock->executing_remap->name,
		     settings->call_level,
		     settings->remap_level);
	    status = pycall(settings, new_frame, // cblock,
			    REMAP_MODULE,
			    cblock->executing_remap->prolog_func,
			    PY_PROLOG);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: prolog pycall(%s) failed, unwinding",
			 cblock->executing_remap->prolog_func);
		ERP(status);
	    }
	    if ((status == INTERP_OK) &&
		(new_frame->call_phase == FINISH_PROLOG)) { // finally done after restart
		new_frame->call_phase = NONE;
		logRemap("O_call: prolog %s done after restart cl=%d rl=%d",
			 cblock->executing_remap->prolog_func, settings->call_level,settings->remap_level);
	    }
	    if (status == INTERP_EXECUTE_FINISH) {  // cede control and mark restart point
		new_frame->call_phase = FINISH_PROLOG;  
		logRemap("O_call: prolog %s returned INTERP_EXECUTE_FINISH, mark restart cl=%d rl=%d",
			 cblock->executing_remap->prolog_func, settings->call_level,settings->remap_level);
		return INTERP_EXECUTE_FINISH;
	    }
	}
	// fall through on purpose 


// now in execute_pycall

    // case FINISH_OWORDSUB:
    // 	// a Python oword sub can be executed inline -
    // 	// no control_back_to() needed
    // 	// might want to cache this in an offset struct eventually
    // 	if (is_py_osub) {
    // 	    status = pycall(settings, eblock,
    // 			    OWORD_MODULE,
    // 			    eblock->o_name, PY_OWORDCALL);

    // 	    //FIXME make restartable with yield
    // 	    if (status > INTERP_MIN_ERROR) {
    // 		logOword("O_call: vanilla osub pycall(%s) failed, unwinding",
    // 			 eblock->o_name);
    // 	    }
    // 	    if (status == INTERP_OK && (eblock->returned == RET_DOUBLE)) {
    // 		logOword("O_call: vanilla osub pycall(%s) returned %lf",
    // 			 eblock->o_name, eblock->py_returned_value);
    // 		settings->return_value = eblock->py_returned_value;
    // 		settings->value_returned = 1;
    // 	    } else {
    // 		settings->return_value = 0.0;
    // 		settings->value_returned = 0;
    // 	    }
    // 	    settings->call_level--;  // drop back
    // 	    return status;
    // 	}
	// fall through on purpose 

    case FINISH_BODY:
	// a Python remap handler can be executed inline too -
	// no control_back_to() needed
	if (is_py_remap_handler) {
	    
	    new_frame->tupleargs = bp::make_tuple(settings->pythis); 

 	    status =  pycall(settings, new_frame,
			     REMAP_MODULE,
			     cblock->executing_remap->remap_py,
			     PY_BODY);

	    if (status >  INTERP_MIN_ERROR) {
		logRemap("O_call: pycall %s returned %s, unwinding (%s)",
			 cblock->executing_remap->remap_py,
			 interp_status(status),
			 cblock->executing_remap->name);
		settings->call_level--;  // unwind this call
		// a failing remap handler is expected to set an appropriate error message
		// which is why we just return status without a message
		return status; 
	    }

	    if (status == INTERP_EXECUTE_FINISH) {  // cede control and mark restart point
		new_frame->call_phase = FINISH_BODY;  
		logRemap("O_call: body %s returned INTERP_EXECUTE_FINISH, mark restart cl=%d rl=%d",
			 cblock->executing_remap->remap_py, settings->call_level,settings->remap_level);
		return INTERP_EXECUTE_FINISH;
	    }
	    // this condition corresponds to an endsub/return of a Python osub
	    // signal that this remap is done and drop call level.
	    if (status == INTERP_OK) {
		if (new_frame->call_phase == FINISH_BODY) { // finally done after restart
		    new_frame->call_phase = NONE;
		    // settings->call_level--; // compensate bump above
		    if (new_frame->returned == RET_STOPITERATION) {
			// the user executed 'yield INTERP_OK' which is fine but keeps
			// the generator object hanging around (I think)
			// unsure how to do this - not like so: cblock->generator_next.del();
		    }
		    logRemap("O_call: %s done after restart, StopIteration=%d cl=%d rl=%d",
			     cblock->executing_remap->remap_py,(int) new_frame->returned == RET_STOPITERATION,
			     settings->call_level, settings->remap_level);
		}
		logRemap("O_call: %s returning INTERP_OK", cblock->executing_remap->remap_py);
		 settings->call_level--; // and return to previous level
		ERP(remap_finished(-cblock->phase));
		return status;
	    }
	    logRemap("O_call: %s OOPS STATUS=%d", eblock->o_name, status);

	}
	logRemap("O_call: %s STATUS=%d  cl=%d rl=%d", eblock->o_name, status, settings->call_level,settings->remap_level);

	if (control_back_to(eblock, settings) == INTERP_ERROR) {
	    settings->call_level--;
	    ERS(NCE_UNABLE_TO_OPEN_FILE,eblock->o_name);
	    return INTERP_ERROR;
	}

    } // end case
    return status;
}

int Interp::execute_return(setup_pointer settings, int what)   // pointer to machine settings
{

    int status = INTERP_OK;
    int i;
    block_pointer cblock,eblock;   
    context_pointer  leaving_frame,  returnto_frame;

    logOword("execute_return what=%d", what);

    cblock = &CONTROLLING_BLOCK(*settings);
    eblock = &EXECUTING_BLOCK(*settings);
    //  some shorthands to make this readable
    leaving_frame = &settings->sub_context[settings->call_level];
    returnto_frame = &settings->sub_context[settings->call_level - 1];

    switch (what) {
    case NORMAL_RETURN:
	// this case is executed only for bona-fide NGC subs
	// subs whose name is a Py callable are handled inline during O_call

	// if level is not zero, in a call
	// otherwise in a defn
	// if we were skipping, no longer
	if (settings->skipping_o) {
	    logOword("case O_%s -- no longer skipping to:|%s|",
		     (eblock->o_type == O_endsub) ? "endsub" : "return",
		     settings->skipping_o);
	    settings->skipping_o = NULL;
	}

    case FINISH_EPILOG:
	if (settings->call_level != 0) {
	    // we call the epilog handler while the current frame is still alive
	    // so we can inspect it from the epilog

	    if (HAS_PYTHON_EPILOG(cblock->executing_remap)) {
		// FIXME not necessarily an NGC remap!
		logRemap("O_endsub/return: epilog %s for NGC remap %s cl=%d rl=%d",
			 cblock->executing_remap->epilog_func,
			 cblock->executing_remap->remap_ngc,settings->call_level,
			 settings->remap_level);
		status = pycall(settings,leaving_frame,
				REMAP_MODULE,
				cblock->executing_remap->epilog_func,
				PY_EPILOG);
		if (status > INTERP_MIN_ERROR) {
		    logRemap("O_endsub/return: epilog %s failed, unwinding",
			     cblock->executing_remap->epilog_func);
		    ERP(status);
		}
		if (status == INTERP_EXECUTE_FINISH) {
		    leaving_frame->call_phase = FINISH_EPILOG;  
		    logRemap("O_endsub/return: epilog %s  INTERP_EXECUTE_FINISH, mark for restart, cl=%d rl=%d",
			     cblock->executing_remap->epilog_func, 
			     settings->call_level,settings->remap_level);
		    return INTERP_EXECUTE_FINISH;
		}
		if (status == INTERP_OK) {
		    if (leaving_frame->call_phase == FINISH_EPILOG) {
			leaving_frame->call_phase = NONE;  
			logRemap("O_call: epilog %s done after restart cl=%d rl=%d",
				 cblock->executing_remap->epilog_func, 
				 settings->call_level,settings->remap_level);
		    }
		}
	    }

	    // free local variables
	    free_named_parameters(leaving_frame);
	    leaving_frame->subName = NULL;

	    // drop one call level.
	    settings->call_level--;

	    // restore subroutine parameters.
	    for(i = 0; i < INTERP_SUB_PARAMS; i++) {
		settings->parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
		    returnto_frame->saved_params[i];
	    }

	    // file at this level was marked as closed, so dont reopen.
	    if (returnto_frame->position == -1) {
		settings->file_pointer = NULL;
		strcpy(settings->filename, "");
	    } else {
		logOword("seeking to: %ld", returnto_frame->position);
		if(settings->file_pointer == NULL) {
		    ERS(NCE_FILE_NOT_OPEN);
		}
		//!!!KL must open the new file, if changed
		if (0 != strcmp(settings->filename, returnto_frame->filename))  {
		    fclose(settings->file_pointer);
		    settings->file_pointer = fopen(returnto_frame->filename, "r");
		    strcpy(settings->filename, returnto_frame->filename);
		}
		fseek(settings->file_pointer, returnto_frame->position, SEEK_SET);
		settings->sequence_number = returnto_frame->sequence_number;

		// cleanups on return:

		// if this was a remap we're done
		if (cblock->executing_remap)
		    ERP(remap_finished(-cblock->phase));

		// a valid previous context was marked by an M73 as auto-restore
		if ((leaving_frame->context_status &
		     (CONTEXT_RESTORE_ON_RETURN|CONTEXT_VALID)) ==
		    (CONTEXT_RESTORE_ON_RETURN|CONTEXT_VALID)) {

		    // NB: this means an M71 invalidate context will prevent an
		    // auto-restore on return/endsub
		    restore_settings(settings, settings->call_level + 1);
		}
		// always invalidate on leaving a context so we dont accidentially
		// 'run into' a valid context when growing the stack upwards again
		leaving_frame->context_status &=  ~CONTEXT_VALID;
	    }

	    settings->sub_name = 0;
	    if (returnto_frame->subName)  {
		settings->sub_name = returnto_frame->subName;
	    } else {
		settings->sub_name = NULL;
	    }
	} else {
	    // a definition
	    if (eblock->o_type == O_endsub) {
		CHKS((settings->defining_sub != 1), NCE_NOT_IN_SUBROUTINE_DEFN);
		// no longer skipping or defining
		if (settings->skipping_o)  {
		    logOword("case O_endsub in defn -- no longer skipping to:|%s|",
			     settings->skipping_o);
		    settings->skipping_o = NULL;
		}
		settings->defining_sub = NULL;
		settings->sub_name = NULL;
	    }
	} // end case FINISH_EPILOG:
    } // end case
    return status;
}
#endif 

//
// TESTME!!! MORE THOROUGHLY !!!KL
//
// In the past, calls had to be to predefined subs
//
// Now they don't. Do things in the following sequence:
// 1 -- if o_word is already defined, just go back to it, else
// 2 -- if there is a file with the name of the o_word,
//             open it and start skipping (as in 3, below)
// 3 -- skip to the o_word (will be an error if not found)
//
int Interp::control_back_to( block_pointer block, // pointer to block
			     setup_pointer settings)   // pointer to machine settings
{
    static char name[] = "control_back_to";
    char newFileName[PATH_MAX+1];
    char foundPlace[PATH_MAX+1];
    char tmpFileName[PATH_MAX+1];
    FILE *newFP;
    offset_map_iterator it;
    offset_pointer op;

    foundPlace[0] = 0;
    logOword("Entered:%s %s", name,block->o_name);

    it = settings->offset_map.find(block->o_name);
    if (it != settings->offset_map.end()) {
	op = &it->second;
	if ((settings->filename[0] != 0) &
	    (settings->file_pointer == NULL))  {
	    ERS(NCE_FILE_NOT_OPEN);
	}
	if (0 != strcmp(settings->filename,
			op->filename)) {
	    // open the new file...
	    newFP = fopen(op->filename, "r");
	    // set the line number
	    settings->sequence_number = 0;
	    strcpy(settings->filename, op->filename);

	    if (newFP) {
		// close the old file...
		if (settings->file_pointer) // only close if it was open
		    fclose(settings->file_pointer);
		settings->file_pointer = newFP;
	    } else {
		logOword("Unable to open file: %s", settings->filename);
		ERS(NCE_UNABLE_TO_OPEN_FILE,settings->filename);
	    }
	}
	if (settings->file_pointer) { // only seek if it was open
	    fseek(settings->file_pointer,
		  op->offset, SEEK_SET);
	}
	settings->sequence_number = op->sequence_number;
	return INTERP_OK;
    }
    newFP = find_ngc_file(settings, block->o_name, newFileName);

    if (newFP) {
	logOword("fopen: |%s| OK", newFileName);

	// close the old file...
	if (settings->file_pointer)
	    fclose(settings->file_pointer);
	settings->file_pointer = newFP;
	strcpy(settings->filename, newFileName);
    } else {
	char *dirname = get_current_dir_name();
	logOword("fopen: |%s| failed CWD:|%s|", newFileName,
		 dirname);
	free(dirname);
	ERS(NCE_UNABLE_TO_OPEN_FILE,tmpFileName);
    }

    settings->skipping_o = block->o_name; // start skipping
    settings->skipping_to_sub = block->o_name; // start skipping
    settings->skipping_start = settings->sequence_number;
    return INTERP_OK;
}

static const char *o_ops[] = {
    "O_none",
    "O_sub",
    "O_endsub",
    "O_call",
    "O_do",
    "O_while",
    "O_if",
    "O_elseif",
    "O_else",
    "O_endif",
    "O_break",
    "O_continue",
    "O_endwhile",
    "O_return",
    "O_repeat",
    "O_endrepeat",
    "O_remap",
    "O_pycall",
    "O_continue_call",
};
static const char *call_phases[] = {
    "CS_START",
    "CS_CALL_PROLOG",
    "CS_CONTINUE_PROLOG",
    "CS_CALL_BODY",
    "CS_CONTINUE_BODY",
    "CS_CALL_EPILOG",
    "CS_CONTINUE_EPILOG",
    "CS_CALL_PYBODY",
    "CS_CONTINUE_PYBODY",
    "CS_CALL_PY_OSUB",
    "CS_CONTINUE_PY_OSUB"
    "CS_DONE"
};
// Propagate an error up the stack as with ERP if the result of 'call' is not
// INTERP_OK or INTERP_EXECUTE_FINISH
#define CHE(call)                                                  \
    do {                                                           \
	int CHP__status = (call);                                  \
        if (CHP__status > INTERP_MIN_ERROR) {			   \
	    ERP(CHP__status);                                      \
        }                                                          \
    } while(0)


// call handling FSM. 
// when called, call frame is already established
// O_return/O_endsub and finishing an Python osub will leave_context()
// currentframe->call_phase holds the state
int Interp::call_fsm(setup_pointer settings, int event) 
{
    int status = INTERP_OK;
    context_pointer active_frame = &settings->sub_context[settings->call_level];
    context_pointer previous_frame = &settings->sub_context[settings->call_level-1];
    block_pointer eblock = &EXECUTING_BLOCK(*settings);
    bp::list plist;

    do {
	logOword("call_fsm event=%s state=%s cl=%d rl=%d", 
		 o_ops[event], call_phases[active_frame->state],
		 settings->call_level, settings->remap_level);

	switch (active_frame->state) {

	case CS_CALL_PY_OSUB: // first time around
	    CHKS(event != O_pycall,"baaad..");
	    settings->return_value = 0.0;
	    settings->value_returned = 0;
	    plist.append(settings->pythis); // self
	    for(int i = 0; i < eblock->param_cnt; i++)
		plist.append(eblock->params[i]);
	    active_frame->tupleargs = bp::tuple(plist);
	    active_frame->kwargs = bp::dict();
	    active_frame->subName = eblock->o_name; 
	    CHP(status = pycall(settings, active_frame, OWORD_MODULE,
				active_frame->subName, PY_OWORDCALL));
	    switch ((status = handler_returned(settings, active_frame, active_frame->subName, true))) {
	    case INTERP_OK:
		active_frame->state = CS_DONE;
		settings->sequence_number = previous_frame->sequence_number;  //FIXTHIS
		CHP(leave_context(settings,false));
		break;
	    case INTERP_EXECUTE_FINISH:
		active_frame->state = CS_CONTINUE_PY_OSUB;
		settings->skip_read = true;  // 'stay on same line' 
		eblock->o_type = O_continue_call; // dont enter_context()
	    }
	    break;
	
	case CS_CONTINUE_PY_OSUB: // continuation post synch()
	    CHKS(event != O_continue_call,"baaad..");
	    CHP(read_inputs(settings));  // update toolchange/input/probe data
	    CHP(pycall(settings, active_frame, OWORD_MODULE,
		       active_frame->subName, PY_FINISH_OWORDCALL));
	    switch ((status = handler_returned(settings, active_frame, active_frame->subName, true))) {
	    case INTERP_OK:
		active_frame->state = CS_DONE;
		settings->skip_read = false; 
		settings->sequence_number = previous_frame->sequence_number; //FIXTHIS
		CHP(leave_context(settings,false));
		break;
	    default: ;
	    }
	    break;

	default:
	    logDebug("unhandled state %s event %s cl=%d rl=%d", 
		 o_ops[event], call_phases[active_frame->state],
		 settings->call_level, settings->remap_level);
	    status = INTERP_ERROR;
	}
    } while ((status == INTERP_OK) && (active_frame->state != CS_DONE));

    return status;
}
#if 0
//int nextstate(remap_pointer remap, int call_phase, 
int Interp::execute_remap(setup_pointer settings, int call_phase) 
{
    int status = INTERP_OK;
    block_pointer cblock = &CONTROLLING_BLOCK(*settings);
    remap_pointer remap = cblock->executing_remap;
    bp::list plist;
    context_pointer active_frame;

    do {
	logRemap("execute_remap %s call_phase=%s cl=%d rl=%d", 
		 remap->name, call_phases[call_phase],
		 settings->call_level, settings->remap_level);
	switch (call_phase) {
	case CS_CALL_PROLOG:
	
	    CHP(enter_context(settings));
	    active_frame = &settings->sub_context[settings->call_level];
	    
	    if (remap->remap_py || remap->prolog_func || remap->epilog_func) {
		CHKS(!PYUSABLE, "%s (remapped) uses Python functions, but the Python plugin is not available", 
		     remap->name);
		plist.append(settings->pythis);   //self
		active_frame->tupleargs = bp::tuple(plist);
		active_frame->kwargs = bp::dict();
	    }
	    if (remap->argspec && (strchr(remap->argspec, '@') == NULL)) {
		// add_parameters will decorate kwargs as per argspec
		// if named local parameters specified
		CHP(add_parameters(settings, cblock, NULL));
	    }
	    if (remap->prolog_func) { // initial call
		status = pycall(settings, active_frame, REMAP_MODULE,
				     remap->prolog_func, PY_PROLOG);
		if (status > INTERP_MIN_ERROR) // assume errormsg set in pycall
		    return status;
		switch (status = handler_returned(settings, active_frame, remap->prolog_func, false)) {
		case INTERP_EXECUTE_FINISH:
		    active_frame->call_phase = CS_CONTINUE_PROLOG;
		    // we keep reexcuting the same block until we get OK, or fail
		    settings->skip_read = true;  // 'stay on same line'   
		    return  INTERP_EXECUTE_FINISH;
		    break;
		case INTERP_OK:
		    call_phase = (remap->remap_py ? CS_CALL_PYBODY : CS_CALL_BODY);
		    break;
		default:
		    CHP(status);
		}   
	    } else {
		call_phase =  (remap->remap_py ? CS_CALL_PYBODY : CS_CALL_BODY);
	    }
	    break;

	case CS_CONTINUE_PROLOG:
	    active_frame = &settings->sub_context[settings->call_level];
	    CHP(read_inputs(settings));  // update toolchange/input/probe data
	    status = pycall(settings, active_frame, REMAP_MODULE,
			    remap->prolog_func, PY_FINISH_PROLOG);

	    switch (status = handler_returned(settings, active_frame, remap->prolog_func, false)) {
	    case INTERP_EXECUTE_FINISH:
		call_phase = active_frame->call_phase = CS_CONTINUE_PROLOG;
		settings->skip_read = true;  
		return  INTERP_EXECUTE_FINISH;
		break;
	    case INTERP_OK:
		call_phase = (remap->remap_py ? CS_CALL_PYBODY : CS_CALL_BODY);
		break;
	    default:
		CHP(status);
	    }   
	    break;
	
	case CS_CALL_PYBODY : 
	    status = pycall(settings, active_frame, REMAP_MODULE,
			    remap->remap_py, PY_BODY);
	    switch (status = handler_returned(settings, active_frame, remap->remap_py, false)) {
	    case INTERP_EXECUTE_FINISH:
		call_phase = active_frame->call_phase = CS_CONTINUE_PYBODY;
		settings->skip_read = true; 
		return  INTERP_EXECUTE_FINISH;
		break;
	    case INTERP_OK:
		call_phase = (remap->epilog_func ? CS_CALL_EPILOG : CS_START);
		break;
	    default:
		CHP(status);
	    }   

	    break;

	case CS_CONTINUE_PYBODY:
	    active_frame = &settings->sub_context[settings->call_level];
	    CHP(read_inputs(settings));
	    status = pycall(settings, active_frame, REMAP_MODULE,
			    remap->prolog_func, PY_FINISH_BODY);

	    switch (status = handler_returned(settings, active_frame, remap->prolog_func, false)) {
	    case INTERP_EXECUTE_FINISH:
		call_phase = active_frame->call_phase = CS_CONTINUE_PYBODY;
		settings->skip_read = true;  // 'stay on same line'   
		return  INTERP_EXECUTE_FINISH;
		break;
	    case INTERP_OK:
		call_phase = (remap->epilog_func ? CS_CALL_EPILOG : CS_START);
		break;
	    default:
		CHP(status);
	    }   
	    break;    

	case CS_CALL_EPILOG:
	    status = pycall(settings, active_frame, REMAP_MODULE,
			    remap->epilog_func, PY_EPILOG);
	    switch (status = handler_returned(settings, active_frame, remap->epilog_func, false)) {
	    case INTERP_EXECUTE_FINISH:
		call_phase = active_frame->call_phase = CS_CONTINUE_EPILOG;
		settings->skip_read = true;  
		return  INTERP_EXECUTE_FINISH;
		break;
	    case INTERP_OK:
		call_phase = CS_START; // done
		break;
	    default:
		CHP(status);
	    }   
	    break;

	case CS_CONTINUE_EPILOG:
	    active_frame = &settings->sub_context[settings->call_level];
	    CHP(read_inputs(settings));  
	    status = pycall(settings, active_frame, REMAP_MODULE,
			    remap->epilog_func, PY_FINISH_EPILOG);

	    switch (status = handler_returned(settings, active_frame, remap->epilog_func, false)) {
	    case INTERP_EXECUTE_FINISH:
		call_phase = active_frame->call_phase = CS_CONTINUE_EPILOG;
		settings->skip_read = true;
		return  INTERP_EXECUTE_FINISH;
		break;
	    case INTERP_OK:
		call_phase =  CS_START; // done
		break;
	    default:
		CHP(status);
	    }   
	    break;    

	case CS_CALL_BODY:
	    logDebug("CS_CALL_BODY done for now");
	    call_phase = active_frame->call_phase = CS_START;
	    break;
	    
	}
    } while ((status == INTERP_OK) && (call_phase != CS_START));

    ERP(remap_finished(-cblock->phase));  // finish remap
    CHP(leave_context(settings,true));    // finish call
    settings->skip_read = false; 
    return status;
}
#endif 

int Interp::handler_returned( setup_pointer settings,  context_pointer active_frame, 
			      const char *name, bool osub)
{
    int status = INTERP_OK;
    
    switch (active_frame->py_return_type) {
    case RET_YIELD:
	// yield <integer> was executed
	CHP(active_frame->py_returned_int);
	break;

    case RET_STOPITERATION:  // a bare 'return' in a generator - treat as INTERP_OK
    case RET_NONE:
	break;
	
    case RET_DOUBLE: 
	if (osub) { // float values are ok for osubs
	    settings->return_value = active_frame->py_returned_double;
	    settings->value_returned = 1;
	} else {
	    ERS("handler_returned: %s returned double: %f - invalid", 
			name, active_frame->py_returned_double);
	}
	break;

    case RET_INT: 
	if (osub) { // let's be liberal with types - widen to double return value
	    settings->return_value = (double) active_frame->py_returned_int;
	    settings->value_returned = 1;
	} else 
	    return active_frame->py_returned_int;
    }
    return status;
}
#if 0

// execute a Python function when 'o<funcname> call' is encountered
// and funcname is a Python callable.
// A Python function may  yield INTERP_EXECUTE_FINISH to indicate
// it executed a queuebuster, possibly several times in a row.
// example:
//     yield self.execute("M66 P0 L0",lineno())
// this requires:
// - the procedure must be restarted after synch()
// - no new read() shall be executed until we get INTERP_OK
// - post-synch, we need to update HAL input/probe/toolchange data.
// we therefore signal skipping reads until done, and handle
// reading input/probe/toolchange data  after restart here
int Interp::execute_pycall(setup_pointer settings, const char *name, int call_phase) 
{
    int status;
    context_pointer active_frame, previous_frame;
    block_pointer eblock = &EXECUTING_BLOCK(*settings);
    bp::list plist;

    logOword("execute_pycall %s call_phase=%s cl=%d", name, 
	     call_phases[call_phase],settings->call_level);

    settings->return_value = 0.0;
    settings->value_returned = 0;

    switch (call_phase) {

    case CS_CALL_PY_OSUB: // first time around: establish new call frame
	CHP(enter_context(settings));
	active_frame = &settings->sub_context[settings->call_level];
	previous_frame = &settings->sub_context[settings->call_level-1];
	previous_frame->sequence_number = settings->sequence_number;
	plist.append(settings->pythis); // self
	for(int i = 0; i < eblock->param_cnt; i++)
	    plist.append(eblock->params[i]);
	active_frame->tupleargs = bp::tuple(plist);
	active_frame->kwargs = bp::dict();
	active_frame->subName = name; 
	status = pycall(settings, active_frame, OWORD_MODULE,
			name, PY_OWORDCALL);
	break;

    case CS_CONTINUE_PY_OSUB:
	// continuation post synch() - refer to existing frame
	active_frame = &settings->sub_context[settings->call_level];
	previous_frame = &settings->sub_context[settings->call_level-1];
	CHP(read_inputs(settings));  // update toolchange/input/probe data
	status = pycall(settings, active_frame, OWORD_MODULE,
			name, PY_FINISH_OWORDCALL);
	break;

    default:
	ERM("O_pycall: yikes - call_phase=%d cl=%d", call_phase,settings->call_level);
	status = INTERP_ERROR;
    }

    if (status > INTERP_MIN_ERROR) {
	settings->skip_read = false;
	return status;
    }
    if ((status = handler_returned(settings, active_frame, name, true)) == INTERP_EXECUTE_FINISH) {
	active_frame->call_phase = CS_CONTINUE_PY_OSUB;
	// we keep reexcuting the same block until we get OK, or fail
	// read() will still set probe/toolchange/input params
	settings->skip_read = true;  // 'stay on same line'   
	return  INTERP_EXECUTE_FINISH;
    }
    active_frame->call_phase = CS_START; // done
    settings->sequence_number = previous_frame->sequence_number;
    settings->skip_read = false;  // proceed
    CHP(leave_context(settings,false));
    return status;
}

#endif 
// prepare a new call frame.
int Interp::enter_context(setup_pointer settings, int call_type, int start_state) 
{
    settings->call_level++;
    if (settings->call_level >= INTERP_SUB_ROUTINE_LEVELS) {
	ERS(NCE_TOO_MANY_SUBROUTINE_LEVELS);
    }
    context_pointer frame = &settings->sub_context[settings->call_level];
    frame->py_returned_int = 0;
    frame->py_returned_double = 0.0;
    frame->py_return_type = -1;
    frame->frame_type = call_type;
    frame->state = start_state;
    return INTERP_OK;
}

int Interp::leave_context(setup_pointer settings, bool restore) 
{
    context_pointer active_frame = &settings->sub_context[settings->call_level];

    free_named_parameters(active_frame);
    active_frame->subName = NULL;

    if (restore && 
	((active_frame->context_status &
    	 (CONTEXT_RESTORE_ON_RETURN|CONTEXT_VALID)) ==
	 (CONTEXT_RESTORE_ON_RETURN|CONTEXT_VALID))) {
	// a valid previous context was marked by an M73 as auto-restore

    	// NB: this means an M71 invalidate context will prevent an
    	// auto-restore on return/endsub
    	restore_settings(settings, settings->call_level + 1);
    }
    // always invalidate on leaving a context so we dont accidentially
    // 'run into' a valid context when growing the stack upwards again
    active_frame->context_status &=  ~CONTEXT_VALID;
    settings->call_level--;  // drop back
    return INTERP_OK;
}

/************************************************************************/
/* convert_control_functions

Returned Value: int (INTERP_OK)
Side effects:
   Changes the flow of control.

Called by: execute_block

Calls: control_skip_to
       control_back_to
       control_save_offset
*/

int Interp::convert_control_functions(block_pointer block, // pointer to a block of RS274/NGC instructions
				      setup_pointer settings)  // pointer to machine settings
{
    int status = INTERP_OK;
    // block_pointer eblock = &EXECUTING_BLOCK(*settings);
    // block_pointer cblock = &CONTROLLING_BLOCK(*settings);
    // context_pointer current_frame = &settings->sub_context[settings->call_level];

    offset_pointer op = NULL;

    logOword("convert_control_functions %s", o_ops[block->o_type]);

    // must skip if skipping
    if (settings->skipping_o && (0 != strcmp(settings->skipping_o, block->o_name)))  {
	logOword("skipping to line: |%s|", settings->skipping_o);
	return INTERP_OK;
    }
    if (settings->skipping_to_sub && (block->o_type != O_sub)) {
	logOword("skipping to sub: |%s|", settings->skipping_to_sub);
	return INTERP_OK;
    }

    switch (block->o_type) {

    case O_none:
	// not an error because we use this to signal that we
	// are not evaluating functions
	break;

    case O_sub:
	// if the level is not zero, this is a call
	// not the definition
	// if we were skipping, no longer
	if (settings->skipping_o) {
	    logOword("sub(o_|%s|) was skipping to here", settings->skipping_o);

	    // skipping to a sub means that we must define this now
	    CHP(control_save_offset( block, settings));
	}

	if (settings->skipping_o) {
	    logOword("no longer skipping to:|%s|", settings->skipping_o);
	    settings->skipping_o = NULL; // this IS our block number
	}
	settings->skipping_to_sub = NULL; // this IS our block number

	if (settings->call_level != 0) {
	    logOword("call:%f:%f:%f",
		     settings->parameters[1],
		     settings->parameters[2],
		     settings->parameters[3]);
	} else {
	    logOword("started a subroutine defn: %s",block->o_name);
	    // a definition
	    CHKS((settings->defining_sub == 1), NCE_NESTED_SUBROUTINE_DEFN);
	    CHP(control_save_offset( block, settings));

	    settings->skipping_o = block->o_name; // start skipping
	    settings->skipping_start = settings->sequence_number;
	    settings->defining_sub = 1;
	    settings->sub_name = block->o_name;
	    logOword("will now skip to: |%s|", settings->sub_name);
	}
	break;

    case O_endsub:
    case O_return:
	// this will also leave_context() including appropriate actions
	CHP(call_fsm(settings, block->o_type));
	break;


	// cases:
	// 'freshly parsed' ngc call - tag in read()
	// 'freshly parsed' pyosub call tag in read_o()
	// 'freshly parsed' remap - tag in convert_remapped_code
	//    -- on execution, enter frame, mark frame, call fsm 
	// the NGC body will be a second call frame!
	// call continuation of py handlers because skip_read = true: 
	//      --- how to mark continuation? set o_type to O_continue_call?
	//      --- do this every time a py handler gets IEF!
	// O_endsub/return - consider callframe type to see 

    case O_call:
	CHP(enter_context(settings, block->o_type, CS_CALL_BODY));
	CHP(call_fsm(settings, block->o_type));
	break;

    case O_pycall:
	CHP(enter_context(settings, block->o_type, CS_CALL_PY_OSUB));
	CHP(call_fsm(settings, block->o_type));
	break;

    case O_remap:
	CHP(enter_context(settings, block->o_type, CS_CALL_PROLOG));
	CHP(call_fsm(settings, block->o_type));
	break;

    case O_continue_call:
	CHP(call_fsm(settings, block->o_type));
	break;

    case O_do:
	// if we were skipping, no longer
	settings->skipping_o = NULL;
	// save the loop point
	// we hit this again on loop back -- so test first
	if(INTERP_OK != control_find_oword(block, settings, &op)) // &index))
	    {
		// save offset if not found in offset table
		CHP(control_save_offset( block, settings));
	    }
	break;

    case O_repeat:
	// if we were skipping, no longer
	settings->skipping_o = NULL;
	status = control_find_oword(block, settings, &op); // &index);

	// test if not already seen OR
	// if seen and this is a repeat
	if ((status != INTERP_OK) ||
	    (op->type == block->o_type))	{
	    // this is the beginning of a 'repeat' loop
	    // add it to the table if not already there
	    if(status != INTERP_OK)
		CHP(control_save_offset( block, settings));

	    // note the repeat count.  it should only be calculated at the
	    // start of the repeat loop.
	    control_find_oword(block, settings, &op); // &index);
	    if(op->repeat_count == -1)
		op->repeat_count =
		    round_to_int(settings->test_value);

	    // are we still repeating?
	    if(op->repeat_count > 0) {
		// execute forward
		logOword("executing forward: [%s] in 'repeat' test value-- %g",
			 block->o_name, settings->test_value);
		// one less repeat remains
		op->repeat_count--;
	    } else {
		// skip forward
		logOword("skipping forward: [%s] in 'repeat'",
			 block->o_name);
		settings->skipping_o = block->o_name;
		settings->skipping_start = settings->sequence_number;
		// cause the repeat count to be recalculated
		// if we do this loop again
		op->repeat_count = -1;
	    }
	}
	break;

    case O_while:
	// if we were skipping, no longer
	settings->skipping_o = NULL;
	status = control_find_oword(block, settings, &op); //  &index);

	// test if not already seen OR
	// if seen and this is a while (alternative is that it is a do)
	if ((status != INTERP_OK) ||
	    (op->type == block->o_type))  {

	    // this is the beginning of a 'while' loop
	    // add it to the table if not already there
	    if (status != INTERP_OK)
		CHP(control_save_offset( block, settings));

	    // test the condition
	    if (settings->test_value != 0.0) // true - execute forward
		logOword("executing forward: [%s] in 'while'",
			 block->o_name);
	    else  {
		// false -  skip forward
		logOword("skipping forward: [%s] in 'while'",
			 block->o_name);
		settings->skipping_o = block->o_name;
		settings->skipping_start = settings->sequence_number;
	    }
	} else {
	    // this is the end of a 'do'
	    // test the condition
	    if ((settings->test_value != 0.0) && !settings->doing_break) {
		// true - loop on back
		logOword("looping back to: [%s] in 'do while'",
			 block->o_name);
		CHP(control_back_to(block, settings));
	    } else {
		// false
		logOword("not looping back to: [%s] in 'do while'",
			 block->o_name);
 	    settings->doing_break = 0;
	    }
	}
	break;

    case O_if:
	if (settings->test_value != 0.0) {
	    //true
	    logOword("executing forward: [%s] in 'if'",
		     block->o_name);
	    settings->skipping_o = NULL;
	    settings->executed_if = 1;
	} else {
	    //false
	    logOword("skipping forward: [%s] in 'if'",
		     block->o_name);
	    settings->skipping_o = block->o_name;
	    settings->skipping_start = settings->sequence_number;
	    settings->executed_if = 0;
	}
	break;

    case O_elseif:
	if ((settings->skipping_o) &&
	    (0 != strcmp(settings->skipping_o, block->o_name)))  {

	    //!!!KL -- the if conditions here say that we were skipping
	    //!!!KL but that the target o_name is not ours.
	    //!!!KL so we should continue skipping -- that's not what
	    //!!!KL the code below says.

	    // mah: so?
#if 0
	    // we were not skipping -- start skipping
	    logOword("start skipping forward: [%s] in 'elseif'",
		     block->o_name);
	    settings->skipping_o = block->o_name;
	    settings->skipping_start = settings->sequence_number;
	    return INTERP_OK;
#else
	    // we were skipping -- continue skipping
	    logOword("continue skipping forward: [%s] in 'elseif'",
		     block->o_name);
	    return INTERP_OK;
#endif
	}

	// we were skipping
	// but were we ever not skipping
	if (settings->executed_if) {
	    // we have already executed, keep on skipping
	    logOword("already executed, continue  "
		     "skipping forward: [%s] in 'elseif'",
		     block->o_name);
	    settings->skipping_o = block->o_name;
	    settings->skipping_start = settings->sequence_number;
	    return INTERP_OK;
	}

	if (settings->test_value != 0.0) {
	    //true -- start executing
	    logOword("start executing forward: [%s] in 'elseif'",
		     block->o_name);
	    settings->skipping_o = NULL;
	    settings->executed_if = 1;
	} else {
	    //false
	    logOword("continue skipping forward: [%s] in 'elseif'",
		     block->o_name);
	}
	break;

    case O_else:
	// were we ever not skipping
	if (settings->executed_if) {
	    // we have already executed, skip
	    logOword("already executed, "
		     "skipping forward: [%s] in 'else'",
		     block->o_name);
	    settings->skipping_o = block->o_name;
	    settings->skipping_start = settings->sequence_number;
	    return INTERP_OK;
	}

	if ((settings->skipping_o) &&
	    (0 == strcmp(settings->skipping_o, block->o_name))) {
	    // we were skipping so stop skipping
	    logOword("stop skipping forward: [%s] in 'else'",
		     block->o_name);
	    settings->executed_if = 1;
	    settings->skipping_o = NULL;
	} else	{
	    // we were not skipping -- so skip
	    logOword("start skipping forward: [%s] in 'else'",
		     block->o_name);
	}
	break;

    case O_endif:
	// stop skipping if we were
	settings->skipping_o = NULL;
	logOword("stop skipping forward: [%s] in 'endif'",
		 block->o_name);
	// the KEY -- outside if clearly must have executed
	// or this would not have executed
	settings->executed_if = 1;
	break;

    case O_break:
<<<<<<< HEAD
      // start skipping
      settings->skipping_o = block->o_name;
      settings->skipping_start = settings->sequence_number;
      settings->doing_break = 1;
      logOword("start skipping forward: [%s] in 'break'",
	      block->o_name);
      break;
||||||| parent of cbbe0b9... interp_o_word.cc: reformat
      // start skipping
      settings->skipping_o = block->o_name;
      settings->skipping_start = settings->sequence_number;
      //settings->doing_break = 1;
      logOword("start skipping forward: [%s] in 'break'",
	      block->o_name);
      break;
=======
	// start skipping
	settings->skipping_o = block->o_name;
	settings->skipping_start = settings->sequence_number;
	//settings->doing_break = 1;
	logOword("start skipping forward: [%s] in 'break'",
		 block->o_name);
	break;
>>>>>>> cbbe0b9... interp_o_word.cc: reformat

    case O_continue:
	// if already skipping, do nothing
	if ((settings->skipping_o) &&
	    (0 == strcmp(settings->skipping_o, block->o_name))) {
	    logOword("already skipping: [%s] in 'continue'",
		     block->o_name);
	    return INTERP_OK;
	}
	// start skipping
	settings->skipping_o = block->o_name;
	settings->skipping_start = settings->sequence_number;
	settings->doing_continue = 1;
	logOword("start skipping forward: [%s] in 'continue'",
		 block->o_name);
	break;

    case O_endrepeat:
    case O_endwhile:
	// end of a while loop
	logOword("endwhile: skipping_o:%s", settings->skipping_o);
	if ((settings->skipping_o) &&
	    (0 == strcmp(settings->skipping_o, block->o_name))) {
	    // we were skipping, so this is the end
	    settings->skipping_o = NULL;

	    if (settings->doing_continue) {
		settings->doing_continue = 0;

		// loop on back
		logOword("looping back (continue) to: [%s] in while/repeat",
			 block->o_name);
		CHP(control_back_to(block, settings));
	    } else {
		// not doing continue, we are done
		logOword("falling thru the complete while/repeat: [%s]",
			 block->o_name);
		return INTERP_OK;
	    }
	} else {
	    // loop on back
	    logOword("looping back to: [%s] in 'endwhile/endrepeat'",
		     block->o_name);
	    CHP(control_back_to(block, settings));
	}
	break;

    default:
	// FIXME !!!KL should probably be an error
	return INTERP_ERROR;
	break;
    }
    // return status;
    return INTERP_OK;
}
//========================================================================
// End of functions for control stuff (O-words)
//========================================================================
