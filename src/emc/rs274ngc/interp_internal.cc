/********************************************************************
* Description: interp_internal.cc
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

/****************************************************************************/

/*! close_and_downcase

Returned Value: int
   If any of the following errors occur, this returns the error code shown.
   Otherwise, it returns INTERP_OK.
   1. A left parenthesis is found inside a comment:
      NCE_NESTED_COMMENT_FOUND
   2. The line ends before an open comment is closed:
      NCE_UNCLOSED_COMMENT_FOUND
   3. A newline character is found that is not followed by null:
      NCE_NULL_MISSING_AFTER_NEWLINE

Side effects: See below

Called by:  read_text

To simplify handling upper case letters, spaces, and tabs, this
function removes spaces and and tabs and downcases everything on a
line which is not part of a comment.

Comments are left unchanged in place. Comments are anything
enclosed in parentheses. Nested comments, indicated by a left
parenthesis inside a comment, are illegal.

The line must have a null character at the end when it comes in.
The line may have one newline character just before the end. If
there is a newline, it will be removed.

Although this software system detects and rejects all illegal characters
and illegal syntax, this particular function does not detect problems
with anything but comments.

We are treating RS274 code here as case-insensitive and spaces and
tabs as if they have no meaning. [RS274D, page 6] says spaces and tabs
are to be ignored by control.

The KT and NGC manuals say nothing about case or spaces and tabs.

*/

int Interp::close_and_downcase(char *line)       //!< string: one line of NC code
{
  static char name[] = "close_and_downcase";
  int m;
  int n;
  int comment;
  char item;
  comment = 0;
  for (n = 0, m = 0; (item = line[m]) != (char) NULL; m++) {
    if (comment) {
      line[n++] = item;
      if (item == ')') {
        comment = 0;
      } else if (item == '(')
        ERM(NCE_NESTED_COMMENT_FOUND);
    } else if ((item == ' ') || (item == '\t') || (item == '\r'));
    /* don't copy blank or tab or CR */
    else if (item == '\n') {    /* don't copy newline            *//* but check null follows        */
      CHK((line[m + 1] != 0), NCE_NULL_MISSING_AFTER_NEWLINE);
    } else if ((64 < item) && (item < 91)) {    /* downcase upper case letters */
      line[n++] = (32 + item);
    } else if (item == '(') {   /* comment is starting */
      comment = 1;
      line[n++] = item;
    } else {
      line[n++] = item;         /* copy anything else */
    }
  }
  CHK((comment), NCE_UNCLOSED_COMMENT_FOUND);
  line[n] = 0;
  return INTERP_OK;
}


/****************************************************************************/

/*! enhance_block

Returned Value:
   If any of the following errors occur, this returns the error shown.
   Otherwise, it returns INTERP_OK.
   1. A g80 is in the block, no modal group 0 code that uses axes
      is in the block, and one or more axis values is given:
      NCE_CANNOT_USE_AXIS_VALUES_WITH_G80
   2. A g92 is in the block and no axis value is given:
      NCE_ALL_AXES_MISSING_WITH_G92
   3. One g-code from group 1 and one from group 0, both of which can use
      axis values, are in the block:
      NCE_CANNOT_USE_TWO_G_CODES_THAT_BOTH_USE_AXIS_VALUES
   4. A g-code (other than 0 or 1, for which we are allowing all axes
      missing) from group 1 which can use axis values is in the block,
      but no axis value is given: NCE_ALL_AXES_MISSING_WITH_MOTION_CODE
   5. Axis values are given, but there is neither a g-code in the block
      nor an active previously given modal g-code that uses axis values:
      NCE_CANNOT_USE_AXIS_VALUES_WITHOUT_A_G_CODE_THAT_USES_THEM

Side effects:
   The value of motion_to_be in the block is set.

Called by: parse_line

If there is a g-code for motion in the block (in g_modes[1]),
set motion_to_be to that. Otherwise, if there is an axis value in the
block and no g-code to use it (any such would be from group 0 in
g_modes[0]), set motion_to_be to be the last motion saved (in
settings->motion mode).

This also make the checks described above.

*/

int Interp::enhance_block(block_pointer block,   //!< pointer to a block to be checked 
                         setup_pointer settings)        //!< pointer to machine settings      
{
  static char name[] = "enhance_block";
  int axis_flag;
  int mode_zero_covets_axes;
  int mode0;
  int mode1;

  axis_flag = ((block->x_flag == ON) || (block->y_flag == ON) ||
               (block->z_flag == ON) || (block->a_flag == ON) ||
               (block->b_flag == ON) || (block->c_flag == ON) );
  mode0 = block->g_modes[0];
  mode1 = block->g_modes[1];
  mode_zero_covets_axes =
    ((mode0 == G_10) || (mode0 == G_28) || (mode0 == G_30)
     || (mode0 == G_92));

  if (mode1 != -1) {
    if (mode1 == G_80) {
      CHK((axis_flag && (!mode_zero_covets_axes)),
          NCE_CANNOT_USE_AXIS_VALUES_WITH_G80);
      CHK(((!axis_flag) && (mode0 == G_92)), NCE_ALL_AXES_MISSING_WITH_G92);
    } else {
      CHK(mode_zero_covets_axes,
          NCE_CANNOT_USE_TWO_G_CODES_THAT_BOTH_USE_AXIS_VALUES);
      CHK(((!axis_flag) && (mode1 != G_0) && (mode1 != G_1)),
          NCE_ALL_AXES_MISSING_WITH_MOTION_CODE);
    }
    block->motion_to_be = mode1;
  } else if (mode_zero_covets_axes) {   /* other 3 can get by without axes but not G92 */
    CHK(((!axis_flag) && (block->g_modes[0] == G_92)),
        NCE_ALL_AXES_MISSING_WITH_G92);
  } else if (axis_flag) {
    CHK(((settings->motion_mode == -1)
         || (settings->motion_mode == G_80)),
        NCE_CANNOT_USE_AXIS_VALUES_WITHOUT_A_G_CODE_THAT_USES_THEM);
    block->motion_to_be = settings->motion_mode;
  }
  return INTERP_OK;
}


/****************************************************************************/

/*! init_block

Returned Value: int (INTERP_OK)

Side effects:
   Values in the block are reset as described below.

Called by: parse_line

This system reuses the same block over and over, rather than building
a new one for each line of NC code. The block is re-initialized before
each new line of NC code is read.

The block contains many slots for values which may or may not be present
on a line of NC code. For some of these slots, there is a flag which
is turned on (at the time time value of the slot is read) if the item
is present.  For slots whose values are to be read which do not have a
flag, there is always some excluded range of values. Setting the
initial value of these slot to some number in the excluded range
serves to show that a value for that slot has not been read.

The rules for the indicators for slots whose values may be read are:
1. If the value may be an arbitrary real number (which is always stored
   internally as a double), a flag is needed to indicate if a value has
   been read. All such flags are initialized to OFF.
   Note that the value itself is not initialized; there is no point in it.
2. If the value must be a non-negative real number (which is always stored
   internally as a double), a value of -1.0 indicates the item is not present.
3. If the value must be an unsigned integer (which is always stored
   internally as an int), a value of -1 indicates the item is not present.
   (RS274/NGC does not use any negative integers.)
4. If the value is a character string (only the comment slot is one), the
   first character is set to 0 (NULL).

*/

int Interp::init_block(block_pointer block)      //!< pointer to a block to be initialized or reset
{
  int n;
  block->a_flag = OFF;
  block->b_flag = OFF;
  block->c_flag = OFF;
  block->comment[0] = 0;
  block->d_number = -1;
  block->f_number = -1.0;
  for (n = 0; n < 15; n++) {
    block->g_modes[n] = -1;
  }
  block->h_number = -1;
  block->i_flag = OFF;
  block->j_flag = OFF;
  block->k_flag = OFF;
  block->l_number = -1;
  block->line_number = -1;
  block->motion_to_be = -1;
  block->m_count = 0;
  for (n = 0; n < 11; n++) {
    block->m_modes[n] = -1;
  }
  block->user_m = 0;
  block->p_number = -1.0;
  block->q_number = -1.0;
  block->r_flag = OFF;
  block->s_number = -1.0;
  block->t_number = -1;
  block->x_flag = OFF;
  block->y_flag = OFF;
  block->z_flag = OFF;

  block->o_type = O_none;
  block->o_number = 0;

  return INTERP_OK;
}


/****************************************************************************/

/*! parse_line

Returned Value: int
   If any of the following functions returns an error code,
   this returns that code.
     init_block
     read_items
     enhance_block
     check_items
   Otherwise, it returns INTERP_OK.

Side effects:
   One RS274 line is read into a block and the block is checked for
   errors. System parameters may be reset.

Called by:  Interp::read

*/

int Interp::parse_line(char *line,       //!< array holding a line of RS274 code  
                      block_pointer block,      //!< pointer to a block to be filled     
                      setup_pointer settings)   //!< pointer to machine settings         
{
  static char name[] = "parse_line";
  int status;

  CHP(init_block(block));
  CHP(read_items(block, line, settings->parameters));

  if(settings->skipping_o == 0)
  {
    CHP(enhance_block(block, settings));
    CHP(check_items(block, settings));
  }
  return INTERP_OK;
}

/****************************************************************************/

/*! precedence

Returned Value: int
  This returns an integer representing the precedence level of an_operator

Side Effects: None

Called by: read_real_expression

To add additional levels of operator precedence, edit this function.

*/

int Interp::precedence(int an_operator)
{
  switch(an_operator)
    {
      case RIGHT_BRACKET:
	return 1;

      case AND2:
      case EXCLUSIVE_OR:
      case NON_EXCLUSIVE_OR:
	return 2;

      case LT:
      case EQ:
      case NE:
      case LE:
      case GE:
      case GT:
	return 3;

      case MINUS:
      case PLUS:
	return 4;

      case NO_OPERATION:
      case DIVIDED_BY:
      case MODULO:
      case TIMES:
	return 5;

      case POWER:
	return 6;
    }
  // should never happen
  return 0;
}


/****************************************************************************/

/*! set_probe_data

Returned Value: int (INTERP_OK)

Side effects:
  The current position is set.
  System parameters for probe position are set.

Called by:  Interp::read

*/

int Interp::set_probe_data(setup_pointer settings)       //!< pointer to machine settings
{

  settings->current_x = GET_EXTERNAL_POSITION_X();
  settings->current_y = GET_EXTERNAL_POSITION_Y();
  settings->current_z = GET_EXTERNAL_POSITION_Z();
  settings->AA_current = GET_EXTERNAL_POSITION_A();
  settings->BB_current = GET_EXTERNAL_POSITION_B();
  settings->CC_current = GET_EXTERNAL_POSITION_C();
  settings->parameters[5061] = GET_EXTERNAL_PROBE_POSITION_X();
  settings->parameters[5062] = GET_EXTERNAL_PROBE_POSITION_Y();
  settings->parameters[5063] = GET_EXTERNAL_PROBE_POSITION_Z();
  settings->parameters[5064] = GET_EXTERNAL_PROBE_POSITION_A();
  settings->parameters[5065] = GET_EXTERNAL_PROBE_POSITION_B();
  settings->parameters[5066] = GET_EXTERNAL_PROBE_POSITION_C();
  settings->parameters[5067] = GET_EXTERNAL_PROBE_VALUE();
  return INTERP_OK;
}

