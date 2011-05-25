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
* Last change:
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

    if(file)
    {
        strncpy(foundFileDirect, direct, PATH_MAX);
        fclose(file);
        return INTERP_OK;
    }

    aDir = opendir(direct);

    if(!aDir)
    {
	ERS(NCE_FILE_NOT_OPEN);
    }

    while((aFile = readdir(aDir)))
    {
        if(aFile->d_type == DT_DIR &&
           (0 != strcmp(aFile->d_name, "..")) &&
           (0 != strcmp(aFile->d_name, ".")))
        {
            char path[PATH_MAX+1];
            snprintf(path, PATH_MAX, "%s/%s", direct, aFile->d_name);
            if(INTERP_OK == findFile(path, target, foundFileDirect))
            {
	        closedir(aDir);
                return INTERP_OK;
            }
        }
    }
    closedir(aDir);
    ERS(NCE_FILE_NOT_OPEN);
}

/************************************************************************/
/*
   In the long run, this function will use a hash table or other
   fast data structure
*/
int Interp::control_save_offset( /* ARGUMENTS                   */
 int line,                   /* (o-word) line number        */
 block_pointer block,        /* pointer to a block of RS274/NGC instructions */
 setup_pointer settings)     /* pointer to machine settings */
{
  static char name[] = "control_save_offset";
  int index;

  logOword("Entered:%s for o_name:|%s|", name, block->o_name);

  if(control_find_oword(block, settings, &index) == INTERP_OK)
  {
      // already exists
      ERS(_("File:%s line:%d redefining sub: o|%s| already defined in file:%s"),
               settings->filename, settings->sequence_number,
	       block->o_name,
               settings->oword_offset[index].filename);
      //return INTERP_OK;
  }

  CHKS((settings->oword_labels >= INTERP_OWORD_LABELS),
      NCE_TOO_MANY_OWORD_LABELS);


  index = settings->oword_labels++;
  //logOword("index: %d offset: %ld", index, block->offset);

  //  settings->oword_offset[index].o_word = line;
  settings->oword_offset[index].o_word_name = block->o_name;
  settings->oword_offset[index].type = block->o_type;
  settings->oword_offset[index].offset = block->offset;
  settings->oword_offset[index].filename = strstore(settings->filename);
  settings->oword_offset[index].repeat_count = -1;

  // the sequence number has already been bumped, so save
  // the proper value
  settings->oword_offset[index].sequence_number =
    settings->sequence_number - 1;

  logOword("control_save_offset: o_word_name=%s type=%d offset=%ld filename=%s repeat_count=%d sequence_number=%d\n",
	   settings->oword_offset[index].o_word_name = block->o_name,
	   settings->oword_offset[index].type,
	   settings->oword_offset[index].offset,
	   settings->oword_offset[index].filename,
	   settings->oword_offset[index].repeat_count,
	   settings->oword_offset[index].sequence_number);

  return INTERP_OK;
}

int Interp::control_find_oword( /* ARGUMENTS                       */
  block_pointer block,      /* pointer to block */
  setup_pointer settings,   /* pointer to machine settings      */
  int *o_index)             /* the index of o-word (returned) */
{
  static char name[] = "control_find_oword";
  int i;

  logOword("Entered:%s", name);
  for(i=0; i<settings->oword_labels; i++)
    {
      if(0 == strcmp(settings->oword_offset[i].o_word_name, block->o_name))
	{
	  *o_index = i;
	  logOword("Found oword[%d]: |%s|", i, block->o_name);
	  return INTERP_OK;
	}
    }
  logOword("Unknown oword name: |%s|", block->o_name);
  ERS(NCE_UNKNOWN_OWORD_NUMBER);
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
int Interp::control_back_to( /* ARGUMENTS                       */
 block_pointer block, // pointer to block
 setup_pointer settings)   /* pointer to machine settings      */
{
  static char name[] = "control_back_to";
  int i,dct;
  char newFileName[PATH_MAX+1];
  char foundPlace[PATH_MAX+1];
  char tmpFileName[PATH_MAX+1];
  FILE *newFP;

  foundPlace[0] = 0;
  logOword("Entered:%s", name);
  for(i=0; i<settings->oword_labels; i++)
    {
      // if(settings->oword_offset[i].o_word == line)
      if(0 == strcmp(settings->oword_offset[i].o_word_name, block->o_name))
	{
	    if ((settings->filename[0] != 0) &
		(settings->file_pointer == NULL))  {
		ERS(NCE_FILE_NOT_OPEN);
	    }
          if(0 != strcmp(settings->filename,
                         settings->oword_offset[i].filename))
          {
              // open the new file...

              newFP = fopen(settings->oword_offset[i].filename, "r");

              // set the line number
              settings->sequence_number = 0;

              strcpy(settings->filename, settings->oword_offset[i].filename);

              if(newFP)
              {
                  // close the old file...
		  if (settings->file_pointer) // only close if it was open
		      fclose(settings->file_pointer);
                  settings->file_pointer = newFP;
              }
              else
              {
                  logOword("Unable to open file: %s", settings->filename);
                  ERS(NCE_UNABLE_TO_OPEN_FILE,settings->filename);
              }
          }
	  if (settings->file_pointer) // only seek if it was open
	      fseek(settings->file_pointer,
		    settings->oword_offset[i].offset, SEEK_SET);

	  settings->sequence_number =
	    settings->oword_offset[i].sequence_number;

	  return INTERP_OK;
	}
    }

  // NO o_word found

  // look for a new file
  logOword("settings->program_prefix:%s:", settings->program_prefix);
  sprintf(tmpFileName, "%s.ngc", block->o_name);

  // find subroutine by search: program_prefix, subroutines, wizard_root
  // use first file found

  // first look in the program_prefix place
  sprintf(newFileName, "%s/%s", settings->program_prefix, tmpFileName);

  newFP = fopen(newFileName, "r");
  logOword("fopen: |%s|", newFileName);

  // then look in the subroutines place
  if(!newFP)
  {
      for (dct=0; dct < MAX_SUB_DIRS; dct++) {
          if (!settings->subroutines[dct][0]) continue;
          sprintf(newFileName, "%s/%s", settings->subroutines[dct], tmpFileName);
          newFP = fopen(newFileName, "r");
          if (newFP) {
              logOword("fopen: |%s|", newFileName);
              break; // use first occurrence in dir search
          }
      }
  }


  // if not found, search the wizard tree
  if(!newFP)
  {
      int ret;
      ret = findFile(settings->wizard_root, tmpFileName, foundPlace);

      if(INTERP_OK == ret)
      {
	  // create the long name
          sprintf(newFileName, "%s/%s",
		  foundPlace, tmpFileName);
          newFP = fopen(newFileName, "r");
      }
  }

  if(newFP)
  {
      logOword("fopen: |%s| OK", newFileName);

      // close the old file...
      if (settings->file_pointer)
	  fclose(settings->file_pointer);
      settings->file_pointer = newFP;

      strcpy(settings->filename, newFileName);
  }
  else
  {
     char *dirname = get_current_dir_name();
     logOword("fopen: |%s| failed CWD:|%s|", newFileName,
              dirname);
     free(dirname);
     ERS(NCE_UNABLE_TO_OPEN_FILE,tmpFileName);
  }

  settings->skipping_o = block->o_name; // start skipping
  settings->skipping_to_sub = block->o_name; // start skipping
  settings->skipping_start = settings->sequence_number;
  //ERS(NCE_UNKNOWN_OWORD_NUMBER);

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

int Interp::convert_control_functions( /* ARGUMENTS           */
 block_pointer block,      /* pointer to a block of RS274/NGC instructions */
 setup_pointer settings)   /* pointer to machine settings                  */
{
  int status = INTERP_OK;
  int index;
  int i;
  context_pointer current_frame, new_frame, leaving_frame,  returnto_frame;
  block_pointer r_block;
  bool is_py_remap_handler; // the sub is executing on behalf of a remapped code
  bool is_remap_handler; // the sub is executing on behalf of a remapped code
  bool is_py_callable;   // the sub name is actually Python
  bool is_py_osub;  // a plain osub whose name is a python callable
  bool py_exception = false;

  logOword("convert_control_functions");

  // if there is an oword, must get the block->o_number
  // !!!KL
  if(block->o_name)
    {
      control_find_oword(block, settings, &(block->o_number));
    }
  else
    {
      block->o_number = 0;
    }

  // must skip if skipping
  if(settings->skipping_o && (0 != strcmp(settings->skipping_o, block->o_name)))
  {
      logOword("skipping to line: |%s|", settings->skipping_o);
      return INTERP_OK;
  }

  if(settings->skipping_to_sub && (block->o_type != O_sub))
  {
      logOword("skipping to sub: |%s|", settings->skipping_to_sub);
      return INTERP_OK;
  }

  logOword("o_type:%d", block->o_type);
  switch(block->o_type)
    {
    case O_none:
      // not an error because we use this to signal that we
      // are not evaluating functions
      break;

    case O_sub:
      // if the level is not zero, this is a call
      // not the definition
      // if we were skipping, no longer
      if(settings->skipping_o)
      {
          logOword("sub(o_|%s|) was skipping to here", settings->skipping_o);

          // skipping to a sub means that we must define this now
	  CHP(control_save_offset(block->o_number, block, settings));
      }

      if(settings->skipping_o)
	{
	  logOword("no longer skipping to:|%s|", settings->skipping_o);
          settings->skipping_o = NULL; // this IS our block number
	}
      settings->skipping_to_sub = NULL; // this IS our block number

      if(settings->call_level != 0)
	{
	  logOword("call:%f:%f:%f",
		   settings->parameters[1],
		   settings->parameters[2],
		   settings->parameters[3]);
	}
      else
	{
	  logOword("started a subroutine defn");
	  // a definition
	  CHKS((settings->defining_sub == 1), NCE_NESTED_SUBROUTINE_DEFN);
	  CHP(control_save_offset(block->o_number, block, settings));

          settings->skipping_o = block->o_name; // start skipping
          settings->skipping_start = settings->sequence_number;
	  settings->defining_sub = 1;
	  settings->sub_name = block->o_name;
	  logOword("will now skip to: |%s| %d", settings->sub_name,
		   block->o_number);
	}
      break;

    case O_endsub:
    case O_return:

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
	    // if (settings->sub_context[settings->call_level].position == -1) {
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

		// cleanups on return
		// if we have an epilog function - execute it, passing the
		// current call frame


		// aquire the remapped block
		r_block = &CONTROLLING_BLOCK(_setup);

		if (HAS_PYTHON_EPILOG(r_block->executing_remap)) {
		    logRemap("O_call: py epilog %s for NGC remap %s",
			     r_block->executing_remap->epilog_func,
			     r_block->executing_remap->remap_ngc);
		    status = pycall(settings,r_block,
				    r_block->executing_remap->epilog_func);
		}
		if (r_block->executing_remap)
		    ERP(remap_finished(USER_REMAP));

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
	break;

    case O_call:
	current_frame = &settings->sub_context[settings->call_level];
	new_frame = &settings->sub_context[settings->call_level + 1];

	// aquire the 'remap_frame' a.k.a controlling block
	r_block = &CONTROLLING_BLOCK(*settings);

	// determine if this sub is the  body executing on behalf of a remapped code
	// we're loosing a bit of context by funneling everything through the osub call
	// interface, which needs to be reestablished here but it's worth the generality
	is_remap_handler =
	    (r_block->executing_remap != NULL) &&
	    (((r_block->executing_remap->remap_py != NULL) &&
	      (!strcmp(r_block->executing_remap->remap_py, block->o_name))) ||
	     (((r_block->executing_remap->remap_ngc != NULL) &&
	       (!strcmp(r_block->executing_remap->remap_ngc, block->o_name)))));

	is_py_remap_handler =
	    (r_block->executing_remap != NULL) &&
	    ((r_block->executing_remap->remap_py != NULL) &&
	     (!strcmp(r_block->executing_remap->remap_py, block->o_name)));

	is_py_callable = is_pycallable(settings, block->o_name);
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
		for(int i = 0; i < block->param_cnt; i++)
		    plist.append(block->params[i]);
		block->tupleargs = bp::make_tuple(plist);
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

	// 	// if remap + NGC
	// // build kwargs for  any Python pro/epilogs if an argspec
	// // was given - add_parameters will decorate remap_kwargs as per argspec
	// block->kwargs = boost::python::dict();
	// if (rptr->argspec) {
	// 	CHKS(add_parameters(settings, block),
	// 	     "%s: add_parameters(argspec=%s) for remap body %s failed ",
	// 	     rptr->name, rptr->argspec,  REMAP_FUNC(rptr));
	// }

	// let any  Oword sub know the number of parameters
	if (!is_py_osub) {
	    CHP(add_named_param("n_args", PA_READONLY));
	    CHP(store_named_param(settings, "n_args",
				  (double )block->param_cnt,
				  OVERRIDE_READONLY));
	}

	if (HAS_PYTHON_PROLOG(r_block->executing_remap)) {
	    logRemap("O_call: py prologue %s for remap %s (%s)",
		     r_block->executing_remap->prolog_func,
		     REMAP_FUNC(r_block->executing_remap),
		     r_block->executing_remap->name);
	    status = pycall(settings, r_block,
			    r_block->executing_remap->prolog_func);
	}

	// a Python oword sub can be executed inline -
	// no control_back_to() needed

	// FIXME mah: think through queuebusters in Python oword sub

	if (is_py_osub) {
	    status = pycall(settings, block, block->o_name);
	    if (status > INTERP_MIN_ERROR) {
		logOword("O_call: vanilla osub pycall(%s) failed, unwinding",
			 block->o_name);
	    }
	    if (status == INTERP_OK && block->returned[RET_DOUBLE]) {
		logOword("O_call: vanilla osub pycall(%s) returned %lf",
			 block->o_name, block->py_returned_value);
		settings->return_value = block->py_returned_value;
	    }
	    settings->call_level--;  // drop back
	    return status;
	}

	// a Python remap handler can be executed inline too -
	// no control_back_to() needed
	if (is_py_remap_handler) {
	    r_block->tupleargs = bp::make_tuple(r_block->user_data);
	    status =  pycall(settings, r_block,
			     r_block->executing_remap->remap_py);
	    if (status >  INTERP_MIN_ERROR) {
		logRemap("O_call: pycall %s returned %s, unwinding (%s)",
			 r_block->executing_remap->remap_py,
			 interp_status(status),
			 r_block->executing_remap->name);
		settings->call_level--;
		return status;
	    }
	    if (status == INTERP_EXECUTE_FINISH) {
		if (r_block->returned[RET_USERDATA])
		    r_block->user_data = r_block->py_returned_userdata;

		if (block->call_again) {
		    settings->call_level--; // stay on current call level (1)
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (sequel), call_level=%d",
			     block->o_name, settings->call_level);
		    return INTERP_EXECUTE_FINISH;
		} else {
		    block->call_again = true;
		    // call level was increased above , now at 1
		    logRemap("O_call: %s returned INTERP_EXECUTE_FINISH (first), call_level=%d",
			     block->o_name, settings->call_level);
		    return INTERP_EXECUTE_FINISH;
		}
	    }
	    if (status == INTERP_OK) {
		if (block->call_again) {
		    block->call_again = false;
		    settings->call_level--; // compensate bump above
		}
		ERP(remap_finished(USER_REMAP));
		settings->call_level--; // and return to previous level
		return status;
	    }
	}

	if (control_back_to(block,settings) == INTERP_ERROR) {
	    settings->call_level--;
	    ERS(NCE_UNABLE_TO_OPEN_FILE,block->o_name);
	    return INTERP_ERROR;
	}
	break;

    case O_do:
      // if we were skipping, no longer
      settings->skipping_o = NULL;
      // save the loop point
      // we hit this again on loop back -- so test first
      if(INTERP_OK != control_find_oword(block, settings, &index))
      {
          CHP(control_save_offset(block->o_number, block, settings));
      }
      break;

    case O_repeat:
      // if we were skipping, no longer
      settings->skipping_o = NULL;
      status = control_find_oword(block, settings, &index);

      // test if not already seen OR
      // if seen and this is a repeat
      if((status != INTERP_OK) ||
	 (settings->oword_offset[index].type == block->o_type))	{
	  // this is the beginning of a 'repeat' loop
	  // add it to the table if not already there
	  if(status != INTERP_OK)
              CHP(control_save_offset(block->o_number, block, settings));

          // note the repeat count.  it should only be calculated at the
          // start of the repeat loop.
          control_find_oword(block, settings, &index);
          if(settings->oword_offset[index].repeat_count == -1)
              settings->oword_offset[index].repeat_count = 
                  round_to_int(settings->test_value);

	  // are we still repeating?
	  if(settings->oword_offset[index].repeat_count > 0) {
	      // execute forward
	      logOword("executing forward: [%s] in 'repeat' test value-- %g",
		       block->o_name, settings->test_value);
              // one less repeat remains
              settings->oword_offset[index].repeat_count--;
          } else {
	      // skip forward
	      logOword("skipping forward: [%s] in 'repeat'",
		       block->o_name);
	      settings->skipping_o = block->o_name;
              settings->skipping_start = settings->sequence_number;
              // cause the repeat count to be recalculated
	      // if we do this loop again
              settings->oword_offset[index].repeat_count = -1;
          }
      }
      break;

    case O_while:
      // if we were skipping, no longer
      settings->skipping_o = NULL;
      status = control_find_oword(block, settings, &index);

      // test if not already seen OR
      // if seen and this is a while (alternative is that it is a do)
      if((status != INTERP_OK) ||
	 (settings->oword_offset[index].type == block->o_type))
	{
	  // this is the beginning of a 'while' loop

	  // add it to the table if not already there
	  if(status != INTERP_OK)
	    {
               CHP(control_save_offset(block->o_number, block, settings));
	    }

	  // test the condition

	  if (settings->test_value != 0.0)
	    {
	      // true
	      // execute forward
	      logOword("executing forward: [%s] in 'while'",
		       block->o_name);
	    }
	  else
	    {
	      // false
	      // skip forward
	      logOword("skipping forward: [%s] in 'while'",
		       block->o_name);
	      settings->skipping_o = block->o_name;
              settings->skipping_start = settings->sequence_number;
	    }
	}
      else
	{
	  // this is the end of a 'do'
	  // test the condition
	    if ((settings->test_value != 0.0) && !_setup.doing_break)
	    {
	      // true
	      // loop on back
	      logOword("looping back to: [%s] in 'do while'",
		       block->o_name);
              CHP(control_back_to(block, settings));
	    }
	  else
	    {
	      // false
	      logOword("not looping back to: [%s] in 'do while'",
		       block->o_name);
	      _setup.doing_break = 0;
	    }
	}
      
      break;

    case O_if:
      if(settings->test_value != 0.0)
	{
	  //true
	  logOword("executing forward: [%s] in 'if'",
	      block->o_name);
          settings->skipping_o = NULL;
	  settings->executed_if = 1;
	}
      else
	{
	  //false
          logOword("skipping forward: [%s] in 'if'",
	      block->o_name);
          settings->skipping_o = block->o_name;
          settings->skipping_start = settings->sequence_number;
	  settings->executed_if = 0;
	}
      break;

    case O_elseif:
      if((settings->skipping_o) &&
	 (0 != strcmp(settings->skipping_o, block->o_name)))
	{
	  //!!!KL -- the if conditions here say that we were skipping
	  //!!!KL but that the target o_name is not ours.
	  //!!!KL so we should continue skipping -- that's not what
	  //!!!KL the code below says.
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
      if(settings->executed_if)
	{
	  // we have already executed, keep on skipping
          logOword("already executed, continue  "
		   "skipping forward: [%s] in 'elseif'",
	      block->o_name);
	  //settings->skipping_0 = block->o_number;
          settings->skipping_o = block->o_name;
          settings->skipping_start = settings->sequence_number;
          return INTERP_OK;
	}
      
      if(settings->test_value != 0.0)
	{
	  //true -- start executing
          logOword("start executing forward: [%s] in 'elseif'",
	      block->o_name);
	  settings->skipping_o = NULL;
	  settings->executed_if = 1;
	}
      else
	{
	  //false
          logOword("continue skipping forward: [%s] in 'elseif'",
	      block->o_name);
	}
      break;

    case O_else:
      // were we ever not skipping
      if(settings->executed_if)
	{
	  // we have already executed, skip
          logOword("already executed, "
		   "skipping forward: [%s] in 'else'",
	      block->o_name);
	  settings->skipping_o = block->o_name;
          settings->skipping_start = settings->sequence_number;
          return INTERP_OK;
	}

      if((settings->skipping_o) &&
	 (0 == strcmp(settings->skipping_o, block->o_name)))
	{
	  // we were skipping so stop skipping
          logOword("stop skipping forward: [%s] in 'else'",
	      block->o_name);
          settings->executed_if = 1;
	  settings->skipping_o = NULL;
	}
      else
	{
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
      // start skipping
      settings->skipping_o = block->o_name;
      settings->skipping_start = settings->sequence_number;
      settings->doing_break = 1;
      logOword("start skipping forward: [%s] in 'break'",
	      block->o_name);
      break;

    case O_continue:

      // if already skipping, do nothing
      if((settings->skipping_o) &&
	 (0 == strcmp(settings->skipping_o, block->o_name)))
	{
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
      if((settings->skipping_o) &&
	 (0 == strcmp(settings->skipping_o, block->o_name)))
	{
	  // we were skipping, so this is the end
	  settings->skipping_o = NULL;

	  if(settings->doing_continue)
	    {
	      settings->doing_continue = 0;

  	      // loop on back
	      logOword("looping back (continue) to: [%s] in while/repeat",
		   block->o_name);
	      CHP(control_back_to(block, settings));
	    }
	  else
	    {
	      // not doing continue, we are done
	      logOword("falling thru the complete while/repeat: [%s]",
		   block->o_name);
	      return INTERP_OK;
	    }
	}
      else
	{
	  // loop on back
	  logOword("looping back to: [%s] in 'endwhile/endrepeat'",
		   block->o_name);
	  CHP(control_back_to(block, settings));
	}
      break;

    default:
      // FIXME !!!KL should probably be an error
	status = INTERP_ERROR; // mah
      break;
    }
  return status;
  //    return INTERP_OK;
}
//========================================================================
// End of functions for control stuff (O-words)
//========================================================================
