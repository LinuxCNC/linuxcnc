/********************************************************************
* Description: interp_namedparams.cc
*
* collect all code related to named parameter handling
*
* Author: mostl K. Lerman, some by Michael Haberler
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sstream>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"


/****************************************************************************/

/*! read_named_parameter

Returned Value: int
   If read_integer_value returns an error code, this returns that code.
   If any of the following errors occur, this returns the error code shown.
   Otherwise, this returns INTERP_OK.
   1. The first character read is not a <:
      NCE_BUG_FUNCTION_SHOULD_NOT_HAVE_BEEN_CALLED
   2. The named parameter string is not terminated by >:
      NCE_NAMED_PARAMETER_NOT_TERSINATED
   3. The named parameter has not been defined before use:
      NCE_NAMED_PARAMETER_NOT_DEFINED

Side effects:
   The value of the given parameter is put into what double_ptr points at.
   The counter is reset to point to the first character after the
   characters which make up the value.

Called by:  read_parameter

This attempts to read the value of a parameter out of the line,
starting at the index given by the counter.

According to the RS274/NGC manual [NCMS, p. 62], the characters following
# may be any "parameter expression". Thus, the following are legal
and mean the same thing (the value of the parameter whose number is
stored in parameter 2):
  ##2
  #[#2]


ADDED by K. Lerman
Named parameters are now supported.
#<_abcd> is a parameter with name "abcd" of global scope
#<abce> is a named parameter of local scope.

*/

int Interp::read_named_parameter(
    char *line,   //!< string: line of RS274/NGC code being processed
    int *counter, //!< pointer to a counter for position on the line
    double *double_ptr,   //!< pointer to double to be read
    double *parameters,   //!< array of system parameters
    bool check_exists)    //!< test for existence, not value
{
  static char name[] = "read_named_parameter";

  char paramNameBuf[LINELEN+1];
  int level;
  int i;

  struct named_parameters_struct *nameList;

  CHKS((line[*counter] != '<'),
      NCE_BUG_FUNCTION_SHOULD_NOT_HAVE_BEEN_CALLED);

  CHP(read_name(line, counter, paramNameBuf));

  // now look it up
  if(paramNameBuf[0] != '_') // local scope
  {
      level = _setup.call_level;
  }
  else
  {
      // call level zero is global scope
      level = 0;
  }

  nameList = &_setup.sub_context[level].named_parameters;

  for(i=0; i<nameList->named_parameter_used_size; i++)
  {
      if(0 == strcmp(nameList->named_parameters[i], paramNameBuf))
      {
          if (check_exists) {
              *double_ptr = 1.0;
	  } else {
	      if (nameList->named_param_attr[i] & PA_USE_LOOKUP) {
		  lookup_named_param(paramNameBuf, nameList->named_param_values[i], double_ptr);
	      } else {
		  *double_ptr = nameList->named_param_values[i];
	      }
	  }
          return INTERP_OK;
      }
  }

  *double_ptr = 0.0;

  if(check_exists) return INTERP_OK;

  logDebug("%s: level[%d] param:|%s| returning not defined", name, level,
           paramNameBuf);
  ERS(_("Named parameter #<%s> not defined"), paramNameBuf);
}

int Interp::find_named_param(
    char *nameBuf, //!< pointer to name to be read
    int *status,    //!< pointer to return status 1 => found
    double *value   //!< pointer to value of found parameter
    )
{
    //static char name[] = "find_named_param";
  struct named_parameters_struct *nameList;

  int level;
  int i;

  // now look it up
  if(nameBuf[0] != '_') // local scope
  {
      level = _setup.call_level;
  }
  else
  {
      // call level zero is global scope
      level = 0;
  }

  nameList = &_setup.sub_context[level].named_parameters;

  for(i=0; i<nameList->named_parameter_used_size; i++)
  {
      if(0 == strcmp(nameList->named_parameters[i], nameBuf))
      {
	  if (nameList->named_param_attr[i] & PA_UNSET) {
	      printf("warning: referencing unassigned variable '%s'\n",nameBuf);
	  }
	  if (nameList->named_param_attr[i] & PA_USE_LOOKUP) {
	      *status = lookup_named_param(nameBuf, nameList->named_param_values[i], value);
	  } else {
	      *value = nameList->named_param_values[i];
	      *status = 1;
	  }
          return INTERP_OK;
      }
  }

  *value = 0.0;
  *status = 0;

  return INTERP_OK;
}



int Interp::store_named_param(
    char *nameBuf, //!< pointer to name to be written
    double value,   //!< value to be written
    int override_readonly  //!< set to true to init a r/o parameter
    )
{
  struct named_parameters_struct *nameList;

  int level;
  int i;

  // now look it up
  if(nameBuf[0] != '_') // local scope
  {
      level = _setup.call_level;
  }
  else
  {
      // call level zero is global scope
      level = 0;
  }

  nameList = &_setup.sub_context[level].named_parameters;

  logDebug("store_named_parameter: nameList[%d]=%p storing:|%s|", level,
           nameList, nameBuf);
  logDebug("store_named_parameter: named_parameter_used_size=%d",
           nameList->named_parameter_used_size);


  for(i=0; i<nameList->named_parameter_used_size; i++)
  {
#if 0
      logDebug("store_named_parameter: named_parameter[%d]=|%s|",
               i, nameList->named_parameters[i]);
#endif
      if(0 == strcmp(nameList->named_parameters[i], nameBuf))
      {
	  CHKS((nameList->named_param_attr[i] & PA_GLOBAL) && level,
	       "BUG: variable marked global, but assigned at level %d",level);
	  if ((nameList->named_param_attr[i] & PA_READONLY) && !override_readonly) {
	      ERS(_("Cannot assign to read-only parameter #<%s>"), nameBuf);
	  } else {
	      nameList->named_param_values[i] = value;
	      nameList->named_param_attr[i] &= ~(PA_UNSET);
	      logDebug("store_named_parameter: level[%d] %s value=%lf",
		       level, nameBuf, value);

	      return INTERP_OK;
	  }
      }
  }

  logDebug("%s: param:|%s| returning not defined", "store_named_param",
           nameBuf);

  ERS(_("Internal error: Could not assign #<%s>"), nameBuf);
}


int Interp::add_named_param(
    char *nameBuf, //!< pointer to name to be added
    int attr //!< see PA_* defs in interp_internal.hh
    )
{
  static char name[] = "add_named_param";
  struct named_parameters_struct *nameList;
  int findStatus;
  double value;
  int level;
  char *dup;

  // look it up to see if already exists
  CHP(find_named_param(nameBuf, &findStatus, &value));

  if(findStatus)
  {
      logDebug("%s: parameter:|%s| already exists", name, nameBuf);
      return INTERP_OK;
  }
  attr |= PA_UNSET;

  // must do an add
  if(nameBuf[0] != '_') // local scope
  {
      level = _setup.call_level;
  }
  else
  {
      // call level zero is global scope
      level = 0;
      attr |= PA_GLOBAL;
  }
  nameList = &_setup.sub_context[level].named_parameters;

  if(nameList->named_parameter_used_size >=
     nameList->named_parameter_alloc_size)
  {
      // must realloc space
      nameList->named_parameter_alloc_size += NAMED_PARAMETERS_ALLOC_UNIT;

      logDebug("realloc space level[%d] size:%d",
               level, nameList->named_parameter_alloc_size);

      nameList->named_parameters =
          (char **)realloc((void *)nameList->named_parameters,
                      sizeof(char *)*nameList->named_parameter_alloc_size);

      nameList->named_param_values =
          (double *)realloc((void *)nameList->named_param_values,
                      sizeof(double)*nameList->named_parameter_alloc_size);

      nameList->named_param_attr = (unsigned char *)realloc((void *)nameList->named_param_attr,
              sizeof(nameList->named_param_attr[0])*nameList->named_parameter_alloc_size);

      if((nameList->named_parameters == 0) ||
         (nameList->named_param_values == 0))
      {
          ERS(NCE_OUT_OF_MEMORY);
      }
  }

  dup = strdup(nameBuf);
  if(dup == 0)
  {
      ERS(NCE_OUT_OF_MEMORY);
  }
  logDebug("%s strdup[%p]:|%s|", name, dup, dup);
  nameList->named_parameters[nameList->named_parameter_used_size] = dup;
  nameList->named_param_attr[nameList->named_parameter_used_size] = attr;
  nameList->named_parameter_used_size++;

  return INTERP_OK;
}


int Interp::free_named_parameters( // ARGUMENTS
    int level,      // level to free
    setup_pointer settings)   // pointer to machine settings
{
    struct named_parameters_struct *nameList;
    int i;

    nameList = &settings->sub_context[level].named_parameters;

    for(i=0; i<nameList->named_parameter_used_size; i++)
    {
        free(nameList->named_parameters[i]);
    }
    nameList->named_parameter_used_size = 0;

    return INTERP_OK;
}

// just a shorthand
int Interp::init_readonly_param(
    const char *nameBuf, //!< pointer to name to be added
    double value,  //!< initial value
    int attr)       //!< see PA_* defs in interp_internal.hh
{
    CHP( add_named_param((char *) nameBuf, PA_READONLY|attr));
    CHP(store_named_param((char *) nameBuf, value, OVERRIDE_READONLY));

    //printf("(DEBUG, %s: = #<%s>)\n",nameBuf,nameBuf);
    return INTERP_OK;
}

int Interp::lookup_named_param(char *nameBuf,
			   double index,
			   double *value
			       )
{
    int cmd = round_to_int(index);

    switch (cmd) {

	// some active_g_codes fields

    case 10: // _line - sequence number
	*value = _setup.sequence_number;
	break;

    case 20: // _motion_mode
	*value = _setup.motion_mode;
	break;

    case 30: // _plane
	switch(_setup.plane) {
	case CANON_PLANE_XY:
	    *value = G_17;
	    break;
	case CANON_PLANE_XZ:
	    *value = G_18;
	    break;
	case CANON_PLANE_YZ:
	    *value = G_19;
	    break;
	case CANON_PLANE_UV:
	    *value = G_17_1;
	    break;
	case CANON_PLANE_UW:
	    *value = G_18_1;
	    break;
	case CANON_PLANE_VW:
	    *value = G_19_1;
	    break;
	}
	break;

    case 40: // _ccomp - cutter compensation
	*value =
	    (_setup.cutter_comp_side == RIGHT) ? G_42 :
	(_setup.cutter_comp_side == LEFT) ? G_41 : G_40;
	break;

    case 50: // _metric
	*value = (_setup.length_units == CANON_UNITS_MM);
	break;

    case 51: // _imperial
	*value = (_setup.length_units == CANON_UNITS_INCHES);
	break;

    case 60: // _absolute - distance mode
	*value = (_setup.distance_mode == MODE_ABSOLUTE);
	break;

    case 61: // _incremental - distance mode
	*value = (_setup.distance_mode == MODE_INCREMENTAL);
	break;

    case 70: // _inverse_time - feed mode
	*value = (_setup.feed_mode == INVERSE_TIME);
	break;

    case 71: // _units_per_minute - feed mode
	*value = (_setup.feed_mode == UNITS_PER_MINUTE);
	break;

    case 72: // _units_per_rev - feed mode
	*value = (_setup.feed_mode == UNITS_PER_REVOLUTION);
	break;

    case 80: // _coord_system - 0-9
	*value =
	    (_setup.origin_index < 7) ? (530 + (10 * _setup.origin_index)) :
	    (584 + _setup.origin_index);
	break;

    case 90: // _tool_offset
	*value =  (_setup.tool_offset.tran.x || _setup.tool_offset.tran.y || _setup.tool_offset.tran.z ||
		   _setup.tool_offset.a || _setup.tool_offset.b || _setup.tool_offset.c ||
		   _setup.tool_offset.u || _setup.tool_offset.v || _setup.tool_offset.w) ;
	break;

    case 100: // _retract_r_plane - G98
	*value = (_setup.retract_mode == R_PLANE);
	break;

    case 101: // _retract_old_z - G99
	*value = (_setup.retract_mode == OLD_Z);
	break;

    case 120: // _spindle_rpm_mode G97
	*value = (_setup.spindle_mode == CONSTANT_RPM);
	break;

    case 121: // _spindle_css_mode G96
	*value = (_setup.spindle_mode == CONSTANT_SURFACE);
	break;

    case 130: //_ijk_absolute_mode - G90.1
	*value = (_setup.ijk_distance_mode == MODE_ABSOLUTE);
	break;

    case 140: // _lathe_diameter_mode - G7
	*value = _setup.lathe_diameter_mode;
	break;

    case 141: // _lathe_radius_mode - G8
	*value = (_setup.lathe_diameter_mode == 0);
	break;

	// some active_m_codes fields

    case 150: // _spindle_on
	*value = (_setup.spindle_turning != CANON_STOPPED);
	break;

    case 151: // spindle_cw
	*value = (_setup.spindle_turning == CANON_CLOCKWISE);
	break;

    case 160: // mist
	*value = _setup.mist;
	break;

    case 170: // flood
	*value = _setup.flood;
	break;

    case 180: // speed override
	*value = _setup.speed_override;
	break;

    case 181: // feed override
	*value = _setup.feed_override;
	break;

    case 190: // adaptive feed
	*value = _setup.adaptive_feed;
	break;

    case 200: // feed hold
	*value = _setup.feed_hold;
	break;

	// from active_settings:
    case 210: // feed
	*value = _setup.feed_rate;
	break;

    case 220: // speed (rpm)
	*value = abs(_setup.speed);
	break;

    case 240:  // current position
	*value = _setup.current_x;
	break;

    case 241:  // current position
	*value = _setup.current_y;
	break;

    case 242:  // current position
	*value = _setup.current_z;
	break;

    case 243:  // current position
	*value = _setup.AA_current;
	break;

    case 244:  // current position
	*value = _setup.BB_current;
	break;

    case 245:  // current position
	*value = _setup.CC_current;
	break;

    case 246:  // current position
	*value = _setup.u_current;
	break;

    case 247:  // current position
	*value = _setup.v_current;
	break;

    case 248:  // current position
	*value = _setup.w_current;
	break;

    default:
	printf("---BUG: lookup_named_param(%s) UNHANDLED INDEX=%f \n",
	       nameBuf,index);
	return 0;
    }
    return 1;
}

int Interp::init_named_parameters()
{
// version       major   minor      Note
// ------------ -------- ---------- -------------------------------------
// M.N.m         M.N     0.m        normal format
// M.N.m~xxx     M.N     0.m        pre-release format
  const char *pkgversion = PACKAGE_VERSION;  //examples: 2.4.6, 2.5.0~pre
  const char *version_major = "_vmajor";// named_parameter name (use lower case)
  const char *version_minor = "_vminor";// named_parameter name (use lower case)
  double vmajor=0.0, vminor=0.0;
  sscanf(pkgversion, "%lf%lf", &vmajor, &vminor);

  init_readonly_param(version_major,vmajor,0);
  init_readonly_param(version_minor,vminor,0);

  // params tagged with PA_USE_LOOKUP will call the lookup_named_param()
  // method. The value is used as a index for the switch() statement.

  // the active_g_codes fields

  // I guess this is the line number
  init_readonly_param("_line", 10, PA_USE_LOOKUP);

  // any of G1 G2 G3 G5.2 G73 G80 G82 G83 G86 G87 G88 G89
  // value is number after 'G' mutiplied by 10 (10,20,30,52..)

  init_readonly_param("_motion_mode", 20, PA_USE_LOOKUP);

  // G17/18/19/17.1/18.1/19.1 -> return 170/180/190/171/181/191
  init_readonly_param("_plane", 30, PA_USE_LOOKUP);

  // return 400,410,420 depending if (G40,G41,G42) is on
  init_readonly_param("_ccomp", 40, PA_USE_LOOKUP);

  // 1.0 if G21 is on
  init_readonly_param("_metric", 50, PA_USE_LOOKUP);

  // 1.0 if G20 is on
  init_readonly_param("_imperial", 51, PA_USE_LOOKUP);

  //1.0 if G90 is on
  init_readonly_param("_absolute", 60, PA_USE_LOOKUP);

  //1.0 if G91 is on
  init_readonly_param("_incremental", 61, PA_USE_LOOKUP);

  // 1.0 if G93 is on
  init_readonly_param("_inverse_time", 70, PA_USE_LOOKUP);

  // 1.0 if G94 is on
  init_readonly_param("_units_per_minute", 71, PA_USE_LOOKUP);

  // 1.0 if G95 is on
  init_readonly_param("_units_per_rev", 72, PA_USE_LOOKUP);

  // 0..9 for G54..G59.3
  init_readonly_param("_coord_system", 80, PA_USE_LOOKUP);

  // 1.0 if G43 is on
  init_readonly_param("_tool_offset", 90, PA_USE_LOOKUP);

  // 1 if G98 set
  init_readonly_param("_retract_r_plane", 100, PA_USE_LOOKUP);

  // 1 if G99 set
  init_readonly_param("_retract_old_z", 101, PA_USE_LOOKUP);

  // really esoteric
  // init_readonly_param("_control_mode", 110, PA_USE_LOOKUP);


  // 1 if G97 is on
  init_readonly_param("_spindle_rpm_mode", 120, PA_USE_LOOKUP);

  init_readonly_param("_spindle_css_mode", 121, PA_USE_LOOKUP);

  // 1 if G90.1 is on
  init_readonly_param("_ijk_absolute_mode", 130, PA_USE_LOOKUP);

  // 1 if G7 is on
  init_readonly_param("_lathe_diameter_mode", 140, PA_USE_LOOKUP);

  // 1 if G8 is on
  init_readonly_param("_lathe_radius_mode", 141, PA_USE_LOOKUP);


  // the active_m_codes fields
  init_readonly_param("_spindle_on", 150, PA_USE_LOOKUP);
  init_readonly_param("_spindle_cw", 151, PA_USE_LOOKUP);

  init_readonly_param("_mist", 160, PA_USE_LOOKUP);
  init_readonly_param("_flood", 170, PA_USE_LOOKUP);
  init_readonly_param("_speed_override", 180, PA_USE_LOOKUP);
  init_readonly_param("_feed_override", 181, PA_USE_LOOKUP);
  init_readonly_param("_adaptive_feed", 190, PA_USE_LOOKUP);
  init_readonly_param("_feed_hold", 200, PA_USE_LOOKUP);

  // active_settings
  init_readonly_param("_feed", 210, PA_USE_LOOKUP);
  init_readonly_param("_rpm", 220, PA_USE_LOOKUP);

  // current position - alias to #5420-#5429
  init_readonly_param("_x", 240, PA_USE_LOOKUP);
  init_readonly_param("_y", 241, PA_USE_LOOKUP);
  init_readonly_param("_z", 242, PA_USE_LOOKUP);
  init_readonly_param("_a", 243, PA_USE_LOOKUP);
  init_readonly_param("_b", 244, PA_USE_LOOKUP);
  init_readonly_param("_c", 245, PA_USE_LOOKUP);
  init_readonly_param("_u", 246, PA_USE_LOOKUP);
  init_readonly_param("_v", 247, PA_USE_LOOKUP);
  init_readonly_param("_w", 248, PA_USE_LOOKUP);

  return INTERP_OK;
}
