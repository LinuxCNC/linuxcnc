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
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_return.hh"
#include "interp_internal.hh"

//========================================================================
// Functions for control stuff (O-words)
//========================================================================

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

  //logDebug("Entered:%s", name);

  if(control_find_oword(line, settings, &index) == INTERP_OK)
  {
      // already exists
      return INTERP_OK;
  }

  CHK((settings->oword_labels >= INTERP_OWORD_LABELS),
      NCE_TOO_MANY_OWORD_LABELS);


  index = settings->oword_labels++;
  //logDebug("index: %d offset: %ld", index, block->offset);

  settings->oword_offset[index].o_word = line;
  settings->oword_offset[index].type = block->o_type;
  settings->oword_offset[index].offset = block->offset;

  // the sequence number has already been bumped, so save
  // the proper value
  settings->oword_offset[index].sequence_number =
    settings->sequence_number - 1;

  return INTERP_OK;
}

#if 0
static int control_skip_to( /* ARGUMENTS                                   */
 int line,                 /* (o-word) line number                         */
 setup_pointer settings)   /* pointer to machine settings                  */
{
}
#endif

int Interp::control_find_oword( /* ARGUMENTS                       */
  int line,                 /* (o-word) line number             */
  setup_pointer settings,   /* pointer to machine settings      */
  int *o_index)             /* the index of o-word (returned) */
{
  static char name[] = "control_find_oword";
  int i;

  //logDebug("Entered:%s", name);
  for(i=0; i<settings->oword_labels; i++)
    {
      if(settings->oword_offset[i].o_word == line)
	{
	  *o_index = i;
	  return INTERP_OK;
	}
    }
  logDebug("Unknown oword number: %d", line);
  ERP(NCE_UNKNOWN_OWORD_NUMBER);
}

int Interp::control_back_to( /* ARGUMENTS                       */
 int line,                 /* (o-word) line number             */
 setup_pointer settings)   /* pointer to machine settings      */
{
  static char name[] = "control_back_to";
  int i;

  //logDebug("Entered:%s", name);
  for(i=0; i<settings->oword_labels; i++)
    {
      if(settings->oword_offset[i].o_word == line)
	{
          if(settings->file_pointer == NULL)
          {
            ERP(NCE_FILE_NOT_OPEN);
          }
	  fseek(settings->file_pointer,
		settings->oword_offset[i].offset, SEEK_SET);

	  settings->sequence_number =
	    settings->oword_offset[i].sequence_number;

	  return INTERP_OK;
	}
    }
  ERP(NCE_UNKNOWN_OWORD_NUMBER);
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
  static char name[] = "convert_control_functions";
  int status;
  int index;
  int i;

  // must skip if skipping
  if(settings->skipping_o && (settings->skipping_o != block->o_number))
    {
      logDebug("skipping to line: %d", settings->skipping_o);
      return INTERP_OK;
    }

  logDebug("o_type:%d", block->o_type);
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
      settings->skipping_o = 0;
      if(settings->call_level != 0)
	{
	  logDebug("call:%f:%f:%f",
		   settings->parameters[1],
		   settings->parameters[2],
		   settings->parameters[3]);
	}
      else
	{
	  logDebug("started a subroutine defn");
	  // a definition
	  CHK((settings->defining_sub == 1), NCE_NESTED_SUBROUTINE_DEFN);
	  CHP(control_save_offset(block->o_number, block, settings));
	  settings->skipping_o = block->o_number;
	  logDebug("will now skip to: %d", block->o_number);
	  settings->defining_sub = 1;
	}
      break;
    case O_endsub:
      // if level is not zero, in a call
      // otherwise in a defn
      // if we were skipping, no longer
      settings->skipping_o = 0;
      if(settings->call_level != 0)
	{
	  // in a call -- must do a return
          // restore old values of parameters
          // restore file position from context

          free_named_parameters(settings->call_level, settings);
          settings->call_level--;

          for(i=0; i<INTERP_SUB_PARAMS; i++)
	    {
              settings->parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
	        settings->sub_context[settings->call_level].saved_params[i];
	    }

	  logDebug("seeking to: %ld",
		   settings->sub_context[settings->call_level].position);

          if(settings->file_pointer == NULL)
          {
            ERP(NCE_FILE_NOT_OPEN);
          }

	  fseek(settings->file_pointer,
		settings->sub_context[settings->call_level].position,
		SEEK_SET);

	  settings->sequence_number =
	    settings->sub_context[settings->call_level].sequence_number;

	}
      else
	{
	  // a definition
	  CHK((settings->defining_sub != 1), NCE_NOT_IN_SUBROUTINE_DEFN);
	  // no longer skipping or defining
	  settings->skipping_o = 0;
	  settings->defining_sub = 0;
	}
      break;
    case O_call:
      // copy parameters from context
      // save old values of parameters
      // save current file position in context
      // if we were skipping, no longer
      settings->skipping_o = 0;
      if(settings->call_level >= INTERP_SUB_ROUTINE_LEVELS)
	{
	  ERP(NCE_TOO_MANY_SUBROUTINE_LEVELS);
	}

      for(i=0; i<INTERP_SUB_PARAMS; i++)
	{
	  settings->sub_context[settings->call_level].saved_params[i] =
	    settings->parameters[i+INTERP_FIRST_SUBROUTINE_PARAM];

	  settings->parameters[i+INTERP_FIRST_SUBROUTINE_PARAM] =
	    block->params[i];

          if(settings->file_pointer == NULL)
          {
            ERP(NCE_FILE_NOT_OPEN);
          }
	  settings->sub_context[settings->call_level].position =
	    ftell(settings->file_pointer);

	  settings->sub_context[settings->call_level].sequence_number
	    = settings->sequence_number;

	}
      logDebug("(in call)set params[%d] return offset:%ld",
	       settings->call_level,
	       settings->sub_context[settings->call_level].position);

      settings->call_level++;

      CHP(control_back_to(block->o_number, settings));
      break;
    case O_do:
      // if we were skipping, no longer
      settings->skipping_o = 0;
      // save the loop point
      CHP(control_save_offset(block->o_number, block, settings));
      break;

    case O_while:
      // if we were skipping, no longer
      settings->skipping_o = 0;
      status = control_find_oword(block->o_number, settings, &index);

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

	  if(settings->test_value != 0.0)
	    {
	      // true
	      // execute forward
	      logDebug("executing forward: [o%d] in 'while'",
		       block->o_number);
	    }
	  else
	    {
	      // false
	      // skip forward
	      logDebug("skipping forward: [o%d] in 'while'",
		       block->o_number);
	      settings->skipping_o = block->o_number;
	    }
	}
      else
	{
	  // this is the end of a 'do'
	  // test the condition
	  if(settings->test_value != 0.0)
	    {
	      // true
	      // loop on back
	      logDebug("looping back to: [o%d] in 'do while'",
		       block->o_number);
              CHP(control_back_to(block->o_number, settings));
	    }
	  else
	    {
	      // false
	      logDebug("not looping back to: [o%d] in 'do while'",
		       block->o_number);
	    }
	}
      
      break;

    case O_if:
      if(settings->test_value != 0.0)
	{
	  //true
	  logDebug("executing forward: [o%d] in 'if'",
	      block->o_number);
          settings->skipping_o = 0;
	  settings->executed_if = 1;
	}
      else
	{
	  //false
          logDebug("skipping forward: [o%d] in 'if'",
	      block->o_number);
          settings->skipping_o = block->o_number;
	  settings->executed_if = 0;
	}
      break;

    case O_elseif:
      if(settings->skipping_o != block->o_number)
	{
	  // we were not skipping -- start skipping
          logDebug("start skipping forward: [o%d] in 'elseif'",
	      block->o_number);
	  settings->skipping_o = block->o_number;
          return INTERP_OK;
	}

      // we were skipping
      // but were we ever not skipping
      if(settings->executed_if)
	{
	  // we have already executed, keep on skipping
          logDebug("already executed, continue  "
		   "skipping forward: [o%d] in 'elseif'",
	      block->o_number);
	  //settings->skipping_0 = block->o_number;
          return INTERP_OK;
	}
      
      if(settings->test_value != 0.0)
	{
	  //true -- start executing
          logDebug("start executing forward: [o%d] in 'elseif'",
	      block->o_number);
	  settings->skipping_o = 0;
	  settings->executed_if = 1;
	}
      else
	{
	  //false
          logDebug("continue skipping forward: [o%d] in 'elseif'",
	      block->o_number);
	}
      break;

    case O_else:
      // were we ever not skipping
      if(settings->executed_if)
	{
	  // we have already executed, skip
          logDebug("already executed, "
		   "skipping forward: [o%d] in 'else'",
	      block->o_number);
	  settings->skipping_o = block->o_number;
          return INTERP_OK;
	}

      if(settings->skipping_o == block->o_number)
	{
	  // we were skipping so stop skipping
          logDebug("stop skipping forward: [o%d] in 'else'",
	      block->o_number);
          settings->executed_if = 1;
	  settings->skipping_o = 0;
	}
      else
	{
          // we were not skipping -- so skip
          logDebug("start skipping forward: [o%d] in 'else'",
	      block->o_number);
	}
      break;

    case O_endif:
      // stop skipping if we were
      settings->skipping_o = 0;
      logDebug("stop skipping forward: [o%d] in 'endif'",
	      block->o_number);
      // the KEY -- outside if clearly must have executed
      // or this would not have executed
      settings->executed_if = 1;
      break;

    case O_break:
      // start skipping
      settings->skipping_o = block->o_number;
      //settings->doing_break = 1;
      logDebug("start skipping forward: [o%d] in 'break'",
	      block->o_number);
      break;

    case O_continue:

      // if already skipping, do nothing
      if(settings->skipping_o == block->o_number)
	{
	  logDebug("already skipping: [o%d] in 'continue'",
		   block->o_number);
	  return INTERP_OK;
	}
      // start skipping
      settings->skipping_o = block->o_number;
      settings->doing_continue = 1;
      logDebug("start skipping forward: [o%d] in 'continue'",
	      block->o_number);
      break;

    case O_endwhile:
      // end of a while loop
      if(settings->skipping_o == block->o_number)
	{
	  // we were skipping, so this is the end
	  settings->skipping_o = 0;

	  if(settings->doing_continue)
	    {
	      settings->doing_continue = 0;

  	      // loop on back
	      logDebug("looping back (continue) to: [o%d] in 'while endwhile'",
		   block->o_number);
	      CHP(control_back_to(block->o_number, settings));
	    }
	  else
	    {
	      // not doing continue, we are done
	      logDebug("falling thru the complete while: [o%d] in 'endwhile'",
		   block->o_number);
	      return INTERP_OK;
	    }
	}
      else
	{
	  // loop on back
	  logDebug("looping back to: [o%d] in 'while endwhile'",
		   block->o_number);
	  CHP(control_back_to(block->o_number, settings));
	}

      break;

    case O_return:
      if(settings->call_level == 0)
      {
          // this is in the definition
          break;
      }

      // if we were skipping, no longer
      settings->skipping_o = 0;

      // in a call -- must do a return
      // restore old values of parameters
      // restore file position from context

      free_named_parameters(settings->call_level, settings);
      settings->call_level--;

      for(i=0; i<INTERP_SUB_PARAMS; i++)
	{
	  settings->parameters[i] =
	    settings->sub_context[settings->call_level].saved_params[i];
	}

      logDebug("seeking to: %ld",
	       settings->sub_context[settings->call_level].position);

      if(settings->file_pointer == NULL)
      {
        ERP(NCE_FILE_NOT_OPEN);
      }

      fseek(settings->file_pointer,
	    settings->sub_context[settings->call_level].position, SEEK_SET);

      settings->sequence_number =
	settings->sub_context[settings->call_level].sequence_number;

      break;

    default:
      //!!!KL should probably be an error
      break;
    }
    return INTERP_OK;
}
//========================================================================
// End of functions for control stuff (O-words)
//========================================================================
