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

int Interp::execute_call(block_pointer block, // pointer to block
			 setup_pointer settings, int what) 
{
    context_pointer current_frame, new_frame;
    block_pointer cblock;
    bool is_py_remap_handler; // the sub is executing on behalf of a remapped code
    bool is_remap_handler; // the sub is executing on behalf of a remapped code
    bool is_py_callable;   // the sub name is actually Python
    bool is_py_osub;  // a plain osub whose name is a python callable
    int status = INTERP_OK;
    int i;
    bool py_exception = false;


    current_frame = &settings->sub_context[settings->call_level];
    new_frame = &settings->sub_context[settings->call_level + 1];

    // aquire the 'remap_frame' a.k.a controlling block
    cblock = &CONTROLLING_BLOCK(*settings);

    // determine if this sub is the  body executing on behalf of a remapped code
    // we're loosing a bit of context by funneling everything through the osub call
    // interface, which needs to be reestablished here but it's worth the generality
    is_remap_handler =
	(cblock->executing_remap != NULL) &&
	(((cblock->executing_remap->remap_py != NULL) &&
	  (!strcmp(cblock->executing_remap->remap_py, block->o_name))) ||
	 (((cblock->executing_remap->remap_ngc != NULL) &&
	   (!strcmp(cblock->executing_remap->remap_ngc, block->o_name)))));

    is_py_callable = is_pycallable(settings, is_remap_handler ? REMAP_MODULE : OWORD_MODULE, block->o_name);
    is_py_osub = is_py_callable && ! is_remap_handler;

    is_py_remap_handler =
	(cblock->executing_remap != NULL) &&
	((cblock->executing_remap->remap_py != NULL) &&
	 (!strcmp(cblock->executing_remap->remap_py, block->o_name)));
	

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
	    logDebug("O_call: vanilla osub pycall(%s), preparing positional args", block->o_name);
	    py_exception = false;
	    try {
		// call with list of positional parameters
		// no saving needed - this is call by value
		bp::list plist;
		plist.append(settings->pythis); // self
		for(int i = 0; i < block->param_cnt; i++)
		    plist.append(block->params[i]);
		block->tupleargs = bp::tuple(plist);
		block->kwargs = bp::dict();
	    }
	    catch (bp::error_already_set) {
		if (PyErr_Occurred()) {
		    PyErr_Print();
		}
		bp::handle_exception();
		PyErr_Clear();
		py_exception = true;
	    }
	    if (py_exception) {
		CHKS(py_exception,"O_call: Py exception preparing arguments for %s",
		     block->o_name);
	    }
	} else {
	    for(i = 0; i < INTERP_SUB_PARAMS; i++)	{
		current_frame->saved_params[i] =
		    settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM];
		settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM] =
		    block->params[i];
	    }
	}

	// if the previous file was NULL, mark positon as -1 so as not to
	// reopen it on return.
	if (settings->file_pointer == NULL) {
	    current_frame->position = -1;
	} else {
	    current_frame->position = ftell(settings->file_pointer);
	}

	// save the previous filename
	logOword("Duping |%s|", settings->filename);
	current_frame->filename = strstore(settings->filename);
	current_frame->sequence_number = settings->sequence_number;
	logOword("(in call)set params[%d] return file:%s offset:%ld",
		 settings->call_level,
		 current_frame->filename,
		 current_frame->position);

	// set the new subName
	new_frame->subName = block->o_name;
	settings->call_level++;

	// let any  Oword sub know the number of parameters
	if (!(is_py_osub || is_py_remap_handler)) {
	    CHP(add_named_param("n_args", PA_READONLY));
	    CHP(store_named_param(settings, "n_args",
				  (double )block->param_cnt,
				  OVERRIDE_READONLY));
	}
	// fall through on purpose 

    case FINISH_PROLOG:
	if (HAS_PYTHON_PROLOG(cblock->executing_remap)) {
	    if (cblock->reexec_prolog) 
		settings->call_level--; // compensate bump above
	    logRemap("O_call: prolog %s for remap %s (%s)  cl=%d rl=%d",
		     cblock->executing_remap->prolog_func,
		     REMAP_FUNC(cblock->executing_remap),
		     cblock->executing_remap->name,
		     settings->call_level,
		     settings->remap_level);
	    status = pycall(settings, cblock,
			    REMAP_MODULE,
			    cblock->executing_remap->prolog_func,
			    PY_PROLOG);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: prolog pycall(%s) failed, unwinding",
			 cblock->executing_remap->prolog_func);
		ERP(status);
	    }

#ifndef NOTYET
	    CHKS(status == INTERP_EXECUTE_FINISH,"continuation of prolog functions not supported yet");
#else
	    if (status == INTERP_EXECUTE_FINISH) {
		if (cblock->reexec_prolog) {
		    logRemap("O_call: prolog %s returned INTERP_EXECUTE_FINISH (sequel),  cl=%d rl=%d",
			     cblock->executing_remap->prolog_func, settings->call_level,settings->remap_level);
		    return INTERP_EXECUTE_FINISH;
		} else {
		    cblock->reexec_prolog = true;
		    logRemap("O_call: epilog %s returned INTERP_EXECUTE_FINISH (first), cl=%d rl=%d",
			     cblock->executing_remap->prolog_func,settings->call_level,settings->remap_level);
		    return INTERP_EXECUTE_FINISH;
		}
	    }
	    if (status == INTERP_OK) {
		if (cblock->reexec_prolog) {
		    cblock->reexec_prolog = false;
		}
	    }
#endif
	}
	// fall through on purpose 

    case FINISH_OWORDSUB:
	// a Python oword sub can be executed inline -
	// no control_back_to() needed
	// might want to cache this in an offset struct eventually
	if (is_py_osub) {
	    status = pycall(settings, block,
			    OWORD_MODULE,
			    block->o_name, PY_OWORDCALL);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: vanilla osub pycall(%s) failed, unwinding",
			 block->o_name);
	    }
	    if (status == INTERP_OK && block->returned[RET_DOUBLE]) {
		logOword("O_call: vanilla osub pycall(%s) returned %lf",
			 block->o_name, block->py_returned_value);
		settings->return_value = block->py_returned_value;
		settings->value_returned = 1;
	    } else {
		settings->return_value = 0.0;
		settings->value_returned = 0;
	    }
	    settings->call_level--;  // drop back
	    return status;
	}
	// fall through on purpose 

    case FINISH_BODY:
	// a Python remap handler can be executed inline too -
	// no control_back_to() needed
	if (is_py_remap_handler) {
	    
	    cblock->tupleargs = bp::make_tuple(settings->pythis); // ,cblock->user_data);

 	    status =  pycall(settings, cblock,
			     REMAP_MODULE,
			     cblock->executing_remap->remap_py,
			     PY_REMAP);

	    if (status >  INTERP_MIN_ERROR) {
		logRemap("O_call: pycall %s returned %s, unwinding (%s)",
			 cblock->executing_remap->remap_py,
			 interp_status(status),
			 cblock->executing_remap->name);
		settings->call_level--;  // unwind this call
		// a failing remap handler is expected to set an appropriate error message
		// which is why we just return status
		return status; 
	    }
#if 0
	    if (status == INTERP_EXECUTE_FINISH) {
		settings->restart_from_snapshot = true;
		getcontext(&settings->snapshot);
		fprintf(stderr, "post getcontext");
		return status;
	    }
#endif
	    if (status == INTERP_EXECUTE_FINISH) {
		if (cblock->reexec_body) {
		    settings->call_level--; // stay on current call level 
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (sequel), call_level=%d",
			     block->o_name, settings->call_level);
		    return INTERP_EXECUTE_FINISH;
		} else {
		    cblock->reexec_body = true;
		    // call level was increased above , now at 1
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (first), call_level=%d",
			     block->o_name, settings->call_level); //FIXME block->o_name can be NULL
		    return INTERP_EXECUTE_FINISH;
		}
	    }

	    // this condition corresponds to an endsub/return. 
	    // signal that this remap is done and drop call level.
	    if (status == INTERP_OK) {
		if (cblock->reexec_body) {
		    cblock->reexec_body = false;
		    settings->call_level--; // compensate bump above
		    if (!cblock->returned[RET_STOPITERATION]) {
			// the user executed 'yield INTERP_OK' which is fine but keeps
			// the generator object hanging around (I think)
			// unsure how to do this - not like so: cblock->generator_next.del();
		    }
		    logRemap("O_call: %s returned INTERP_OK, finishing continuation, StopIteration=%d cl=%d rl=%d",
			     block->o_name,(int) cblock->returned[RET_STOPITERATION],
			     settings->call_level, settings->remap_level);
		}
		logRemap("O_call: %s returning INTERP_OK",    block->o_name);
		settings->call_level--; // and return to previous level
		ERP(remap_finished(-cblock->phase));
		return status;
	    }
	    logRemap("O_call: %s OOPS STATUS=%d", block->o_name, status);

	}
	logRemap("O_call: %s STATUS=%d", block->o_name, status);

	if (control_back_to(block,settings) == INTERP_ERROR) {
	    settings->call_level--;
	    ERS(NCE_UNABLE_TO_OPEN_FILE,block->o_name);
	    return INTERP_ERROR;
	}

    } // end case
    return status;
}

int Interp::execute_return(block_pointer block, // pointer to block
			   setup_pointer settings, int what)   // pointer to machine settings
{

    int status = INTERP_OK;
    int i;
    block_pointer cblock;   
    context_pointer  leaving_frame,  returnto_frame;

    switch (what) {
    case NORMAL_RETURN:
	// this case is executed only for bona-fide NGC subs
	// subs whose name is a Py callable are handled inline during O_call

	// if level is not zero, in a call
	// otherwise in a defn
	// if we were skipping, no longer
	if (settings->skipping_o) {
	    logOword("case O_%s -- no longer skipping to:|%s|",
		     (block->o_type == O_endsub) ? "endsub" : "return",
		     settings->skipping_o);
	    settings->skipping_o = NULL;
	}
	if (settings->call_level != 0) {
	    //  some shorthands to make this readable
	    leaving_frame = &settings->sub_context[settings->call_level];
	    returnto_frame = &settings->sub_context[settings->call_level - 1];

	    // we call the epilog handler while the current frame is still alive
	    // so we can inspect it from the epilog

	    // aquire the remapped block
	    cblock = &CONTROLLING_BLOCK(_setup);
	}

    case FINISH_EPILOG:
	if (settings->call_level != 0) {

	    if (HAS_PYTHON_EPILOG(cblock->executing_remap)) {
		logRemap("O_endsub/return: epilog %s for NGC remap %s cl=%d rl=%d",
			 cblock->executing_remap->epilog_func,
			 cblock->executing_remap->remap_ngc,settings->call_level,
			 settings->remap_level);
		status = pycall(settings,cblock,
				REMAP_MODULE,
				cblock->executing_remap->epilog_func,
				PY_EPILOG);
		if (status > INTERP_MIN_ERROR) {
		    logRemap("O_endsub/return: epilog %s failed, unwinding",
			     cblock->executing_remap->epilog_func);
		    ERP(status);
		}
		if (status == INTERP_EXECUTE_FINISH) {
		    if (cblock->reexec_epilog) {
			logRemap("O_endsub/return: epilog %s returned INTERP_EXECUTE_FINISH (sequel),  cl=%d rl=%d",
				 cblock->executing_remap->epilog_func, settings->call_level,settings->remap_level);
			return INTERP_EXECUTE_FINISH;
		    } else {
			cblock->reexec_epilog = true;
			logRemap("O_endsub/return: epilog %s returned INTERP_EXECUTE_FINISH (first), cl=%d rl=%d",
				 cblock->executing_remap->epilog_func,settings->call_level,settings->remap_level);
			return INTERP_EXECUTE_FINISH;
		    }
		}
		if (status == INTERP_OK) {
		    if (cblock->reexec_epilog) {
			cblock->reexec_epilog = false;
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
		    restore_context(settings, settings->call_level + 1);
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
	    if (block->o_type == O_endsub) {
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
    block_pointer cblock;
    bool is_py_remap_handler; // the sub is executing on behalf of a remapped code
    offset_pointer op = NULL;

    logOword("convert_control_functions");

    // aquire the 'remap_frame' a.k.a controlling block
    cblock = &CONTROLLING_BLOCK(*settings);
    is_py_remap_handler =
	(cblock->executing_remap != NULL) &&
	((cblock->executing_remap->remap_py != NULL) &&
	 (!strcmp(cblock->executing_remap->remap_py, block->o_name)));

    // must skip if skipping
    if (settings->skipping_o && (0 != strcmp(settings->skipping_o, block->o_name)))  {
	logOword("skipping to line: |%s|", settings->skipping_o);
	return INTERP_OK;
    }

    if (settings->skipping_to_sub && (block->o_type != O_sub)) {
	logOword("skipping to sub: |%s|", settings->skipping_to_sub);
	return INTERP_OK;
    }

    logOword("o_type:  %d\n", block->o_type);
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
	return execute_return(block,settings, NORMAL_RETURN);
	break;

#if 0
	// this case is executed only for bona-fide NGC subs
	// subs whose name is a Py callable are handled inline during O_call

	// if level is not zero, in a call
	// otherwise in a defn
	// if we were skipping, no longer
	if (settings->skipping_o) {
	    logOword("case O_%s -- no longer skipping to:|%s|",
		     (block->o_type == O_endsub) ? "endsub" : "return",
		     settings->skipping_o);
	    settings->skipping_o = NULL;
	}
	if (settings->call_level != 0) {
	    //  some shorthands to make this readable
	    leaving_frame = &settings->sub_context[settings->call_level];
	    returnto_frame = &settings->sub_context[settings->call_level - 1];

	    // we call the epilog handler while the current frame is still alive
	    // so we can inspect it from the epilog

	    // aquire the remapped block
	    cblock = &CONTROLLING_BLOCK(_setup);

	    if (HAS_PYTHON_EPILOG(cblock->executing_remap)) {
		logRemap("O_endsub/return: epilog %s for NGC remap %s cl=%d rl=%d",
			 cblock->executing_remap->epilog_func,
			 cblock->executing_remap->remap_ngc,settings->call_level,
			 settings->remap_level);
		status = pycall(settings,cblock,
				REMAP_MODULE,
				cblock->executing_remap->epilog_func,
				PY_EPILOG);
		if (status > INTERP_MIN_ERROR) {
		    logRemap("O_endsub/return: epilog %s failed, unwinding",
			     cblock->executing_remap->epilog_func);
		    ERP(status);
		}
		if (status == INTERP_EXECUTE_FINISH) {
		    if (cblock->reexec_epilog) {
			logRemap("O_endsub/return: epilog %s returned INTERP_EXECUTE_FINISH (sequel),  cl=%d rl=%d",
				 cblock->executing_remap->epilog_func, settings->call_level,settings->remap_level);
			return INTERP_EXECUTE_FINISH;
		    } else {
			cblock->reexec_epilog = true;
			logRemap("O_endsub/return: epilog %s returned INTERP_EXECUTE_FINISH (first), cl=%d rl=%d",
				 cblock->executing_remap->epilog_func,settings->call_level,settings->remap_level);
			return INTERP_EXECUTE_FINISH;
		    }
		}
		if (status == INTERP_OK) {
		    if (cblock->reexec_epilog) {
			cblock->reexec_epilog = false;
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
		    restore_context(settings, settings->call_level + 1);
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
	    if (block->o_type == O_endsub) {
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
	}
#endif
	break;

    case O_call:
	return  execute_call(block, settings, NORMAL_CALL);
	break;
#if 0
	current_frame = &settings->sub_context[settings->call_level];
	new_frame = &settings->sub_context[settings->call_level + 1];

	// aquire the 'remap_frame' a.k.a controlling block
	cblock = &CONTROLLING_BLOCK(*settings);

	// determine if this sub is the  body executing on behalf of a remapped code
	// we're loosing a bit of context by funneling everything through the osub call
	// interface, which needs to be reestablished here but it's worth the generality
	is_remap_handler =
	    (cblock->executing_remap != NULL) &&
	    (((cblock->executing_remap->remap_py != NULL) &&
	      (!strcmp(cblock->executing_remap->remap_py, block->o_name))) ||
	     (((cblock->executing_remap->remap_ngc != NULL) &&
	       (!strcmp(cblock->executing_remap->remap_ngc, block->o_name)))));

        is_py_callable = is_pycallable(settings, is_remap_handler ? REMAP_MODULE : OWORD_MODULE, block->o_name);
	is_py_osub = is_py_callable && ! is_remap_handler;

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
	    logDebug("O_call: vanilla osub pycall(%s), preparing positional args", block->o_name);
	    py_exception = false;
	    try {
		// call with list of positional parameters
		// no saving needed - this is call by value
		bp::list plist;
		plist.append(settings->pythis); // self
		for(int i = 0; i < block->param_cnt; i++)
		    plist.append(block->params[i]);
		block->tupleargs = bp::tuple(plist);
		block->kwargs = bp::dict();
	    }
	    catch (bp::error_already_set) {
		if (PyErr_Occurred()) {
		    PyErr_Print();
		}
		bp::handle_exception();
		PyErr_Clear();
		py_exception = true;
	    }
	    if (py_exception) {
		CHKS(py_exception,"O_call: Py exception preparing arguments for %s",
		     block->o_name);
	    }
	} else {
	    for(i = 0; i < INTERP_SUB_PARAMS; i++)	{
		current_frame->saved_params[i] =
		    settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM];
		settings->parameters[i + INTERP_FIRST_SUBROUTINE_PARAM] =
		    block->params[i];
	    }
	}

	// if the previous file was NULL, mark positon as -1 so as not to
	// reopen it on return.
	if (settings->file_pointer == NULL) {
	    current_frame->position = -1;
	} else {
	    current_frame->position = ftell(settings->file_pointer);
	}

	// save the previous filename
	logOword("Duping |%s|", settings->filename);
	current_frame->filename = strstore(settings->filename);
	current_frame->sequence_number = settings->sequence_number;
	logOword("(in call)set params[%d] return file:%s offset:%ld",
		 settings->call_level,
		 current_frame->filename,
		 current_frame->position);

	// set the new subName
	new_frame->subName = block->o_name;
	settings->call_level++;

	// let any  Oword sub know the number of parameters
	if (!(is_py_osub || is_py_remap_handler)) {
	    CHP(add_named_param("n_args", PA_READONLY));
	    CHP(store_named_param(settings, "n_args",
				  (double )block->param_cnt,
				  OVERRIDE_READONLY));
	}

	if (HAS_PYTHON_PROLOG(cblock->executing_remap)) {
	    if (cblock->reexec_prolog) 
		settings->call_level--; // compensate bump above
	    logRemap("O_call: prolog %s for remap %s (%s)  cl=%d rl=%d",
		     cblock->executing_remap->prolog_func,
		     REMAP_FUNC(cblock->executing_remap),
		     cblock->executing_remap->name,
		     settings->call_level,
		     settings->remap_level);
	    status = pycall(settings, cblock,
			    REMAP_MODULE,
			    cblock->executing_remap->prolog_func,
			    PY_PROLOG);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: prolog pycall(%s) failed, unwinding",
			 cblock->executing_remap->prolog_func);
		ERP(status);
	    }

#ifndef NOTYET
	    CHKS(status == INTERP_EXECUTE_FINISH,"continuation of prolog functions not supported yet");
#else
	    if (status == INTERP_EXECUTE_FINISH) {
		if (cblock->reexec_prolog) {
		    logRemap("O_call: prolog %s returned INTERP_EXECUTE_FINISH (sequel),  cl=%d rl=%d",
			     cblock->executing_remap->prolog_func, settings->call_level,settings->remap_level);
		    return INTERP_EXECUTE_FINISH;
		} else {
		    cblock->reexec_prolog = true;
		    logRemap("O_call: epilog %s returned INTERP_EXECUTE_FINISH (first), cl=%d rl=%d",
			     cblock->executing_remap->prolog_func,settings->call_level,settings->remap_level);
		    return INTERP_EXECUTE_FINISH;
		}
	    }
	    if (status == INTERP_OK) {
		if (cblock->reexec_prolog) {
		    cblock->reexec_prolog = false;
		}
	    }
#endif
	}
	// a Python oword sub can be executed inline -
	// no control_back_to() needed
	// might want to cache this in an offset struct eventually
	if (is_py_osub) {
	    status = pycall(settings, block,
			    OWORD_MODULE,
			    block->o_name, PY_OWORDCALL);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: vanilla osub pycall(%s) failed, unwinding",
			 block->o_name);
	    }
	    if (status == INTERP_OK && block->returned[RET_DOUBLE]) {
		logOword("O_call: vanilla osub pycall(%s) returned %lf",
			 block->o_name, block->py_returned_value);
		settings->return_value = block->py_returned_value;
		settings->value_returned = 1;
	    } else {
		settings->return_value = 0.0;
		settings->value_returned = 0;
	    }
	    settings->call_level--;  // drop back
	    return status;
	}

	// a Python remap handler can be executed inline too -
	// no control_back_to() needed
	if (is_py_remap_handler) {
	    
	    cblock->tupleargs = bp::make_tuple(settings->pythis); // ,cblock->user_data);

 	    status =  pycall(settings, cblock,
			     REMAP_MODULE,
			     cblock->executing_remap->remap_py,
			     PY_REMAP);

	    if (status >  INTERP_MIN_ERROR) {
		logRemap("O_call: pycall %s returned %s, unwinding (%s)",
			 cblock->executing_remap->remap_py,
			 interp_status(status),
			 cblock->executing_remap->name);
		settings->call_level--;  // unwind this call
		// a failing remap handler is expected to set an appropriate error message
		// which is why we just return status
		return status; 
	    }
#if 0
	    if (status == INTERP_EXECUTE_FINISH) {
		settings->restart_from_snapshot = true;
		getcontext(&settings->snapshot);
		fprintf(stderr, "post getcontext");
		return status;
	    }
#endif
	    if (status == INTERP_EXECUTE_FINISH) {
		if (cblock->reexec_body) {
		    settings->call_level--; // stay on current call level 
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (sequel), call_level=%d",
			     block->o_name, settings->call_level);
		    return INTERP_EXECUTE_FINISH;
		} else {
		    cblock->reexec_body = true;
		    // call level was increased above , now at 1
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (first), call_level=%d",
			     block->o_name, settings->call_level); //FIXME block->o_name can be NULL
		    return INTERP_EXECUTE_FINISH;
		}
	    }

	    // this condition corresponds to an endsub/return. 
	    // signal that this remap is done and drop call level.
	    if (status == INTERP_OK) {
		if (cblock->reexec_body) {
		    cblock->reexec_body = false;
		    settings->call_level--; // compensate bump above
		    if (!cblock->returned[RET_STOPITERATION]) {
			// the user executed 'yield INTERP_OK' which is fine but keeps
			// the generator object hanging around (I think)
			// unsure how to do this - not like so: cblock->generator_next.del();
		    }
		    logRemap("O_call: %s returned INTERP_OK, finishing continuation, StopIteration=%d cl=%d rl=%d",
			     block->o_name,(int) cblock->returned[RET_STOPITERATION],
			     settings->call_level, settings->remap_level);
		}
		logRemap("O_call: %s returning INTERP_OK",    block->o_name);
		settings->call_level--; // and return to previous level
		ERP(remap_finished(-cblock->phase));
		return status;
	    }
	    logRemap("O_call: %s OOPS STATUS=%d", block->o_name, status);

	}
	logRemap("O_call: %s STATUS=%d", block->o_name, status);

	if (control_back_to(block,settings) == INTERP_ERROR) {
	    settings->call_level--;
	    ERS(NCE_UNABLE_TO_OPEN_FILE,block->o_name);
	    return INTERP_ERROR;
	}
	break;
#endif 
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
