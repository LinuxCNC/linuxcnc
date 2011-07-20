/********************************************************************
* Description: interp_namedparams.cc
*
* collect all code related to named parameter handling
*
* Author: mostly K. Lerman
* rewrite by Michael Haberler to use STL containers
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change: Juli 2011
********************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <boost/python.hpp>
namespace bp = boost::python;

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sstream>
#include <map>

#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "inifile.hh"

enum predefined_named_parameters {
    NP_LINE,
    NP_MOTION_MODE,
    NP_PLANE,
    NP_CCOMP,
    NP_METRIC,
    NP_IMPERIAL,
    NP_ABSOLUTE,
    NP_INCREMENTAL,
    NP_INVERSE_TIME,
    NP_UNITS_PER_MINUTE,
    NP_UNITS_PER_REV,
    NP_COORD_SYSTEM,
    NP_TOOL_OFFSET,
    NP_RETRACT_R_PLANE,
    NP_RETRACT_OLD_Z,
    NP_SPINDLE_RPM_MODE,
    NP_SPINDLE_CSS_MODE,
    NP_IJK_ABSOLUTE_MODE,
    NP_LATHE_DIAMETER_MODE,
    NP_LATHE_RADIUS_MODE,
    NP_SPINDLE_ON,
    NP_SPINDLE_CW,
    NP_MIST,
    NP_FLOOD,
    NP_SPEED_OVERRIDE,
    NP_FEED_OVERRIDE,
    NP_ADAPTIVE_FEED,
    NP_FEED_HOLD,
    NP_FEED,
    NP_RPM,
    NP_CURRENT_TOOL,
    NP_SELECTED_POCKET,
    NP_CURRENT_POCKET,
    NP_X,
    NP_Y,
    NP_Z,
    NP_A,
    NP_B,
    NP_C,
    NP_U,
    NP_V,
    NP_W,
    NP_VALUE,
    NP_CALL_LEVEL,
    NP_REMAP_LEVEL,
    NP_SELECTED_TOOL,
    NP_VALUE_RETURNED,
};

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
    int exists;
    double value;
    parameter_map_iterator pi;

    CHKS((line[*counter] != '<'),
	 NCE_BUG_FUNCTION_SHOULD_NOT_HAVE_BEEN_CALLED);
    CHP(read_name(line, counter, paramNameBuf));

    CHP(find_named_param(paramNameBuf, &exists, &value));
    if (check_exists) {
	*double_ptr = exists ? 1.0 : 0.0;
	return INTERP_OK;
    }
    if (exists) {
	*double_ptr = value;
	return INTERP_OK;
    } else {
	logNP("%s: referencing undefined named parameter '%s' level=%d",
	      name, paramNameBuf, (paramNameBuf[0] == '_') ? 0 : _setup.call_level);
	ERS(_("Named parameter #<%s> not defined"), paramNameBuf);
    }
    return INTERP_OK;
}

// if the variable is of the form '_[section]name', then treat it as
// an inifile  variable. Lookup section/name and cached the value
// as global, and read-only.
// the shortest possible ini variable is '_[s]n' .
int Interp::fetch_ini_param( const char *nameBuf, int *status, double *value)
{
    char *s;
    *status = 0;
    if ((nameBuf[0] == '_') &&
	(nameBuf[1] == '[') &&
	(strlen(nameBuf) > 4) &&
	((s = (char *) strchr(&nameBuf[3],']')) != NULL)) {

	IniFile inifile;
	const char *iniFileName;
	int retval;
	int closeBracket = s - nameBuf;

	if ((iniFileName = getenv("INI_FILE_NAME")) == NULL) {
	    logNP("warning: referencing ini parameter '%s': no ini file",nameBuf);
	    *status = 0;
	    return INTERP_OK;
	}
	if (!inifile.Open(iniFileName)) {
	    *status = 0;
	    ERS(_("cant open ini file '%s'"), iniFileName);
	}

	char capName[LINELEN];

	strncpy(capName, nameBuf, sizeof(capName));
	for (char *p = capName; *p != 0; p++)
	    *p = toupper(*p);
	capName[closeBracket] = '\0';

	if ((retval = inifile.Find( value, &capName[closeBracket+1], &capName[2])) == 0) {
	    *status = 1;
	    inifile.Close();
	} else {
	    inifile.Close();
	    *status = 0;
	    ERS(_("Named ini parameter #<%s> not found in inifile '%s': error=0x%x"),
		nameBuf, iniFileName, retval);
	}
    }
    return INTERP_OK;
}

// debug aid
int Interp::print_named_params(bool locals)
{
    context_pointer frame;
    parameter_map_iterator pi;
    int level;
    double value;

    printf("--- named parameters: -----\n");
    for (level =  locals ? _setup.call_level : 0; level >= 0; level --) {
	frame = &_setup.sub_context[level];
	for (pi = frame->named_params.begin(); pi != frame->named_params.end(); pi++) {
	    if (pi->second.attr & PA_USE_LOOKUP)  {
		CHP(lookup_named_param(pi->first, pi->second.value, &value));
	    } else {
		value = pi->second.value;
	    }
	    printf("%-2d %-20.20s %10.4f ", level, pi->first, value);
	    printf((pi->second.attr & PA_GLOBAL) ? "GLOBAL ": "LOCAL ");
	    if (pi->second.attr & PA_READONLY)
		printf("R/O ");
	    if (pi->second.attr & PA_UNSET)
		printf("UNSET ");
	    if (pi->second.attr & PA_USE_LOOKUP)
		printf("LOOKUP ");
	    if (pi->second.attr & PA_FROM_INI)
		printf("INI ");
	    printf("\n");
	}
    }
    return INTERP_OK;
}

int Interp::find_named_param(
    const char *nameBuf, //!< pointer to name to be read
    int *status,    //!< pointer to return status 1 => found
    double *value   //!< pointer to value of found parameter
    )
{
  context_pointer frame;
  parameter_map_iterator pi;
  int level;

  level = (nameBuf[0] == '_') ? 0 : _setup.call_level; // determine scope
  frame = &_setup.sub_context[level];

  pi = frame->named_params.find(nameBuf);
  if (pi == frame->named_params.end()) { // not found
      int exists;
      double inivalue;
      CHP(fetch_ini_param(nameBuf, &exists, &inivalue));
      if (exists) {
	  logNP("parameter '%s' retrieved from INI: %f",nameBuf,inivalue);
	  *value = inivalue;
	  *status = 1;
	  parameter_value param;  // cache the value
	  param.value = inivalue;
	  param.attr = PA_GLOBAL | PA_READONLY | PA_FROM_INI;
	  _setup.sub_context[0].named_params[strstore(nameBuf)] = param;
      } else {
	  *value = 0.0;
	  *status = 0;
      }
  } else {
      parameter_pointer pv = &pi->second;
      if (pv->attr & PA_UNSET)
	  logNP("warning: referencing unset variable '%s'",nameBuf);
      if (pv->attr & PA_USE_LOOKUP) {
	  CHP(lookup_named_param(nameBuf, pv->value, value));
	  *status = 1;
      } else {
	  *value = pv->value;
	  *status = 1;
      }
  }
  return INTERP_OK;
}


int Interp::store_named_param(setup_pointer settings,
    const char *nameBuf, //!< pointer to name to be written
    double value,   //!< value to be written
    int override_readonly  //!< set to true to init a r/o parameter
    )
{
  context_pointer frame;
  int level;
  parameter_map_iterator pi;

  level = (nameBuf[0] == '_') ? 0 : _setup.call_level; // determine scope
  frame = &settings->sub_context[level];

  pi = frame->named_params.find(nameBuf);
  if (pi == frame->named_params.end()) {
      ERS(_("Internal error: Could not assign #<%s>"), nameBuf);
  } else {
      parameter_pointer pv = &pi->second;

      CHKS(((pv->attr & PA_GLOBAL)  && level),
	   "BUG: variable '%s' marked global, but assigned at level %d", nameBuf, level);

      if ((pv->attr & PA_READONLY) && !override_readonly) {
	  ERS(_("Cannot assign to read-only parameter #<%s>"), nameBuf);
      } else {
	  pv->value = value;
	  pv->attr &= ~PA_UNSET;
	  logNP("store_named_parameter: level[%d] %s value=%lf",
		level, nameBuf, value);
      }
  }
  return INTERP_OK;
}


// add_parameters - a built-in prolog function
//
// handles argspec and extracts required and optional items from the
// controlling block.
//
// if preparing for an NGC file, add local variables to
// the current oword subroutine call frame
//
// if posargs == NULL:
//      add the named parameters as local variables to  the current call frame
// if posargs != NULL:
//      create a positional argument list as per argspec order
//      instead of adding local variables
//
// also, generate a kwargs style dictionary of required and optional items
// in case a Python prolog is called
//
// 1. add all requried and  present optional words.
// 2. error on missing but required words.
// 4. handle '>' as to require a positive feed.
// 5. handle '^' as to require a positive speed.
// 6. handle 'N' as to add the line number.
//
// return INTERP_ERROR and propagate appropriate message if any errors so far
// else return INTERP_OK
//
// handling '@' (positional params) is dealt with in the calling procedure

int Interp::add_parameters(setup_pointer settings,
			   block_pointer cblock,
			   char *posarglist)
{
    const char *s,*argspec, *code;
    block_pointer block;
    char missing[30],optional[30],required[30];
    char *m = missing;
    char *o = optional;
    char *r = required;
    char msg[LINELEN], tail[LINELEN];
    bool errored = false;
    remap_pointer rptr = cblock->executing_remap;

    if (!rptr) {
	ERS("BUG: add_parameters: remap_frame: executing_remap == NULL ");
    }
    code = rptr->name;

    // if any Python handlers are present, create a kwargs dict
    bool pydict = rptr->remap_py || rptr->prolog_func || rptr->epilog_func;

    memset(missing,0,sizeof(missing));
    memset(optional,0,sizeof(optional));
    memset(required,0,sizeof(required));
    memset(msg,0,sizeof(msg));
    memset(tail,0,sizeof(tail));

    s = argspec = rptr->argspec;
    CHKS((argspec == NULL),"BUG: add_parameters: argspec = NULL");

    while (*s) {
	if (isupper(*s) && !strchr(required,*s)) *r++ = tolower(*s);
	if (islower(*s) && !strchr(optional,*s)) *o++ = *s;
	s++;
    }
    o = optional;
    r = required;
    block = &CONTROLLING_BLOCK((*settings));

    logNP("add_parameters code=%s argspec=%s call_level=%d r=%s o=%s pydict=%d\n",
	    code,argspec,settings->call_level,required,optional,pydict);

#define STORE(name,value)						\
    if (pydict) {							\
	try {								\
	    cblock->kwargs[name] = value;				\
        }								\
        catch (bp::error_already_set) {					\
	    PyErr_Print();						\
	    PyErr_Clear();						\
	    ERS("add_parameters: cant add '%s' to args",name);		\
	}								\
    }									\
    if (posarglist) {							\
	char actual[LINELEN];						\
	snprintf(actual, sizeof(actual),"[%.4lf]", value);		\
	strcat(posarglist, actual);					\
	cblock->param_cnt++;						\
    } else {								\
	add_named_param(name,0);					\
	store_named_param(settings,name,value,0);			\
    }


#define PARAM(spec,name,flag,value) 	                    	\
    if ((flag)) { /* present */	                    	        \
	/* required or optional */ 	                    	\
	if (strchr(required,spec) || strchr(optional,spec)) {	\
	    STORE(name,value);					\
	}							\
    } else {							\
	if (strchr(required,spec)) { /* missing */		\
	    *m++ = spec;					\
	    errored = true;					\
	}							\
    }

    s =  rptr->argspec;
    // step through argspec in order so positional args are built
    // in the correct order
    while (*s) {
	switch (tolower(*s)) {
	case 'a' : PARAM('a',"a",block->a_flag,block->a_number); break;
	case 'b' : PARAM('b',"b",block->b_flag,block->b_number); break;
	case 'c' : PARAM('c',"c",block->c_flag,block->c_number); break;
	case 'd' : PARAM('d',"d",block->d_flag,block->d_number_float); break;
	case 'e' : PARAM('e',"e",block->e_flag,block->e_number); break;
	case 'f' : PARAM('f',"f",block->f_flag,block->f_number); break;
	case 'h' : PARAM('h',"h",block->h_flag,(double) block->h_number); break;
	case 'i' : PARAM('i',"i",block->i_flag,block->i_number); break;
	case 'j' : PARAM('j',"j",block->j_flag,block->j_number); break;
	case 'k' : PARAM('k',"k",block->k_flag,block->k_number); break;
	case 'l' : PARAM('l',"l",block->l_flag,(double) block->l_number); break;
	case 'p' : PARAM('p',"p",block->p_flag,block->p_number); break;
	case 'q' : PARAM('q',"q",block->q_flag,block->q_number); break;
	case 'r' : PARAM('r',"r",block->r_flag,block->r_number); break;
	case 's' : PARAM('s',"s",block->s_flag,block->s_number); break;
	case 't' : PARAM('t',"t",block->t_flag, (double) block->t_number); break;
	case 'u' : PARAM('u',"u",block->u_flag,block->u_number); break;
	case 'v' : PARAM('v',"v",block->v_flag,block->v_number); break;
	case 'w' : PARAM('w',"w",block->w_flag,block->w_number); break;
	case 'x' : PARAM('x',"x",block->x_flag,block->x_number); break;
	case 'y' : PARAM('y',"y",block->y_flag,block->y_number); break;
	case 'z' : PARAM('z',"z",block->z_flag,block->z_number); break;
	case '-' : break; // ignore - backwards compatibility
	default: ;
	}
	s++;
    }

    s = missing;
    if (*s) {
	strcat(tail," missing: ");
    }
    while (*s) {
	errored = true;
	char c  = toupper(*s);
	strncat(tail,&c,1);
	if (*(s+1)) strcat(tail,",");
	s++;
    }
    // special cases:
    // N...add line number
    if (strchr(required,'n')) {
	STORE("n",(double) block->n_number);
    }

    // >...require positive feed
    if (strchr(required,'>')) {
	if (settings->feed_rate > 0.0) {
	    STORE("f",settings->feed_rate);
	} else {
	    strcat(tail,"F>0,");
	    errored = true;
	}
    }
    // ^...require positive speed
    if (strchr(required,'^')) {
	if (settings->speed > 0.0) {
	    STORE("s",settings->speed);
	} else {
	    strcat(tail,"S>0,");
	    errored = true;
	}
    }

    if (errored) {
	ERS("user-defined %s:%s",
	    code, tail);
    }
    return INTERP_OK;
}


int Interp::add_named_param(
    const char *nameBuf, //!< pointer to name to be added
    int attr) //!< see PA_* defs in interp_internal.hh
{
  static char name[] = "add_named_param";
  int findStatus;
  double value;
  int level;
  parameter_value param;

  // look it up to see if already exists
  CHP(find_named_param(nameBuf, &findStatus, &value));

  if (findStatus) {
      logNP("%s: parameter:|%s| already exists", name, nameBuf);
      return INTERP_OK;
  }
  attr |= PA_UNSET;

  if (nameBuf[0] != '_') {  // local scope
      level = _setup.call_level;
  } else {
      level = 0;    // call level zero is global scope
      attr |= PA_GLOBAL;
  }
  param.value = 0.0;
  param.attr = attr;
  _setup.sub_context[level].named_params[strstore(nameBuf)] = param;
  return INTERP_OK;
}


int Interp::free_named_parameters(context_pointer frame)
{
    frame->named_params.clear();
    return INTERP_OK;
}


// just a shorthand
int Interp::init_readonly_param(
    const char *nameBuf, //!< pointer to name to be added
    double value,  //!< initial value
    int attr)       //!< see PA_* defs in interp_internal.hh
{
    // static char name[] = "init_readonly_param";
    CHKS( add_named_param((char *) nameBuf, PA_READONLY|attr),
	  "adding r/o '%s'", nameBuf);
    CHKS(store_named_param(&_setup, (char *) nameBuf, value, OVERRIDE_READONLY),
	 "storing r/o '%s' %f", nameBuf, value);
    return INTERP_OK;
}


int Interp::lookup_named_param(const char *nameBuf,
			   double index,
			   double *value)
{
    int cmd = round_to_int(index);

    switch (cmd) {

	// some active_g_codes fields

    case NP_LINE: // _line - sequence number
	*value = _setup.sequence_number;
	break;

    case NP_MOTION_MODE: // _motion_mode
	*value = _setup.motion_mode;
	break;

    case NP_PLANE: // _plane
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

    case NP_CCOMP: // _ccomp - cutter compensation
	*value =
	    (_setup.cutter_comp_side == RIGHT) ? G_42 :
	(_setup.cutter_comp_side == LEFT) ? G_41 : G_40;
	break;

    case NP_METRIC: // _metric
	*value = (_setup.length_units == CANON_UNITS_MM);
	break;

    case NP_IMPERIAL: // _imperial
	*value = (_setup.length_units == CANON_UNITS_INCHES);
	break;

    case NP_ABSOLUTE: // _absolute - distance mode
	*value = (_setup.distance_mode == MODE_ABSOLUTE);
	break;

    case NP_INCREMENTAL: // _incremental - distance mode
	*value = (_setup.distance_mode == MODE_INCREMENTAL);
	break;

    case NP_INVERSE_TIME: // _inverse_time - feed mode
	*value = (_setup.feed_mode == INVERSE_TIME);
	break;

    case NP_UNITS_PER_MINUTE: // _units_per_minute - feed mode
	*value = (_setup.feed_mode == UNITS_PER_MINUTE);
	break;

    case NP_UNITS_PER_REV: // _units_per_rev - feed mode
	*value = (_setup.feed_mode == UNITS_PER_REVOLUTION);
	break;

    case NP_COORD_SYSTEM: // _coord_system - 0-9
	*value =
	    (_setup.origin_index < 7) ? (530 + (10 * _setup.origin_index)) :
	    (584 + _setup.origin_index);
	break;

    case NP_TOOL_OFFSET: // _tool_offset
	*value =  (_setup.tool_offset.tran.x || _setup.tool_offset.tran.y || _setup.tool_offset.tran.z ||
		   _setup.tool_offset.a || _setup.tool_offset.b || _setup.tool_offset.c ||
		   _setup.tool_offset.u || _setup.tool_offset.v || _setup.tool_offset.w) ;
	break;

    case NP_RETRACT_R_PLANE: // _retract_r_plane - G98
	*value = (_setup.retract_mode == R_PLANE);
	break;

    case NP_RETRACT_OLD_Z: // _retract_old_z - G99
	*value = (_setup.retract_mode == OLD_Z);
	break;

    case NP_SPINDLE_RPM_MODE: // _spindle_rpm_mode G97
	*value = (_setup.spindle_mode == CONSTANT_RPM);
	break;

    case NP_SPINDLE_CSS_MODE: // _spindle_css_mode G96
	*value = (_setup.spindle_mode == CONSTANT_SURFACE);
	break;

    case NP_IJK_ABSOLUTE_MODE: //_ijk_absolute_mode - G90.1
	*value = (_setup.ijk_distance_mode == MODE_ABSOLUTE);
	break;

    case NP_LATHE_DIAMETER_MODE: // _lathe_diameter_mode - G7
	*value = _setup.lathe_diameter_mode;
	break;

    case NP_LATHE_RADIUS_MODE: // _lathe_radius_mode - G8
	*value = (_setup.lathe_diameter_mode == 0);
	break;

	// some active_m_codes fields

    case NP_SPINDLE_ON: // _spindle_on
	*value = (_setup.spindle_turning != CANON_STOPPED);
	break;

    case NP_SPINDLE_CW: // spindle_cw
	*value = (_setup.spindle_turning == CANON_CLOCKWISE);
	break;

    case NP_MIST: // mist
	*value = _setup.mist;
	break;

    case NP_FLOOD: // flood
	*value = _setup.flood;
	break;

    case NP_SPEED_OVERRIDE: // speed override
	*value = _setup.speed_override;
	break;

    case NP_FEED_OVERRIDE: // feed override
	*value = _setup.feed_override;
	break;

    case NP_ADAPTIVE_FEED: // adaptive feed
	*value = _setup.adaptive_feed;
	break;

    case NP_FEED_HOLD: // feed hold
	*value = _setup.feed_hold;
	break;

	// from active_settings:
    case NP_FEED: // feed
	*value = _setup.feed_rate;
	break;

    case NP_RPM: // speed (rpm)
	*value = abs(_setup.speed);
	break;

    case NP_CURRENT_TOOL:
	*value = _setup.tool_table[0].toolno;
	break;

    case NP_SELECTED_POCKET:
	*value = _setup.selected_pocket;
	break;

    case NP_CURRENT_POCKET:
	*value = _setup.current_pocket;
	break;

    case NP_SELECTED_TOOL:
	*value = _setup.selected_tool;
	break;

    case NP_X:  // current position
	*value = _setup.current_x;
	break;

    case NP_Y:  // current position
	*value = _setup.current_y;
	break;

    case NP_Z:  // current position
	*value = _setup.current_z;
	break;

    case NP_A:  // current position
	*value = _setup.AA_current;
	break;

    case NP_B:  // current position
	*value = _setup.BB_current;
	break;

    case NP_C:  // current position
	*value = _setup.CC_current;
	break;

    case NP_U:  // current position
	*value = _setup.u_current;
	break;

    case NP_V:  // current position
	*value = _setup.v_current;
	break;

    case NP_W:  // current position
	*value = _setup.w_current;
	break;

	// o-word subs may optionally have an
	// expression after endsub and return
	// this 'function return value' is accessible as '_value'
    case NP_VALUE:
	*value = _setup.return_value;
	break;

	// predicate: the last NGC procedure did/did not return a value
    case NP_VALUE_RETURNED:
	*value = _setup.value_returned;
	break;

    case NP_CALL_LEVEL:
	*value = _setup.call_level;
	break;

    case NP_REMAP_LEVEL:
	*value = _setup.remap_level;
	break;

    default:
	ERS(_("BUG: lookup_named_param(%s): unhandled index=%fn"),
	      nameBuf,index);
    }
    return INTERP_OK;
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
  init_readonly_param("_line", NP_LINE, PA_USE_LOOKUP);

  // any of G1 G2 G3 G5.2 G73 G80 G82 G83 G86 G87 G88 G89
  // value is number after 'G' mutiplied by 10 (10,20,30,52..)

  init_readonly_param("_motion_mode", NP_MOTION_MODE, PA_USE_LOOKUP);

  // G17/18/19/17.1/18.1/19.1 -> return 170/180/190/171/181/191
  init_readonly_param("_plane", NP_PLANE, PA_USE_LOOKUP);

  // return 400,410,420 depending if (G40,G41,G42) is on
  init_readonly_param("_ccomp", NP_CCOMP, PA_USE_LOOKUP);

  // 1.0 if G21 is on
  init_readonly_param("_metric", NP_METRIC, PA_USE_LOOKUP);

  // 1.0 if G20 is on
  init_readonly_param("_imperial", NP_IMPERIAL, PA_USE_LOOKUP);

  //1.0 if G90 is on
  init_readonly_param("_absolute", NP_ABSOLUTE, PA_USE_LOOKUP);

  //1.0 if G91 is on
  init_readonly_param("_incremental", NP_INCREMENTAL, PA_USE_LOOKUP);

  // 1.0 if G93 is on
  init_readonly_param("_inverse_time", NP_INVERSE_TIME, PA_USE_LOOKUP);

  // 1.0 if G94 is on
  init_readonly_param("_units_per_minute", NP_UNITS_PER_MINUTE, PA_USE_LOOKUP);

  // 1.0 if G95 is on
  init_readonly_param("_units_per_rev", NP_UNITS_PER_REV, PA_USE_LOOKUP);

  // 0..9 for G54..G59.3
  init_readonly_param("_coord_system", NP_COORD_SYSTEM, PA_USE_LOOKUP);

  // 1.0 if G43 is on
  init_readonly_param("_tool_offset", NP_TOOL_OFFSET, PA_USE_LOOKUP);

  // 1 if G98 set
  init_readonly_param("_retract_r_plane", NP_RETRACT_R_PLANE, PA_USE_LOOKUP);

  // 1 if G99 set
  init_readonly_param("_retract_old_z", NP_RETRACT_OLD_Z, PA_USE_LOOKUP);

  // really esoteric
  // init_readonly_param("_control_mode", 110, PA_USE_LOOKUP);


  // 1 if G97 is on
  init_readonly_param("_spindle_rpm_mode", NP_SPINDLE_RPM_MODE, PA_USE_LOOKUP);

  init_readonly_param("_spindle_css_mode", NP_SPINDLE_CSS_MODE, PA_USE_LOOKUP);

  // 1 if G90.1 is on
  init_readonly_param("_ijk_absolute_mode", NP_IJK_ABSOLUTE_MODE, PA_USE_LOOKUP);

  // 1 if G7 is on
  init_readonly_param("_lathe_diameter_mode", NP_LATHE_DIAMETER_MODE, PA_USE_LOOKUP);

  // 1 if G8 is on
  init_readonly_param("_lathe_radius_mode", NP_LATHE_RADIUS_MODE, PA_USE_LOOKUP);


  // the active_m_codes fields
  init_readonly_param("_spindle_on", NP_SPINDLE_ON, PA_USE_LOOKUP);
  init_readonly_param("_spindle_cw", NP_SPINDLE_CW, PA_USE_LOOKUP);

  init_readonly_param("_mist", NP_MIST, PA_USE_LOOKUP);
  init_readonly_param("_flood", NP_FLOOD, PA_USE_LOOKUP);
  init_readonly_param("_speed_override", NP_SPEED_OVERRIDE, PA_USE_LOOKUP);
  init_readonly_param("_feed_override", NP_FEED_OVERRIDE, PA_USE_LOOKUP);
  init_readonly_param("_adaptive_feed", NP_ADAPTIVE_FEED, PA_USE_LOOKUP);
  init_readonly_param("_feed_hold", NP_FEED_HOLD, PA_USE_LOOKUP);

  // active_settings
  init_readonly_param("_feed", NP_FEED, PA_USE_LOOKUP);
  init_readonly_param("_rpm", NP_RPM, PA_USE_LOOKUP);


  // tool related
  init_readonly_param("_current_tool", NP_CURRENT_TOOL, PA_USE_LOOKUP);
  init_readonly_param("_current_pocket", NP_CURRENT_POCKET, PA_USE_LOOKUP);
  init_readonly_param("_selected_pocket", NP_SELECTED_POCKET, PA_USE_LOOKUP);
  init_readonly_param("_selected_tool", NP_SELECTED_TOOL, PA_USE_LOOKUP);

  // current position - alias to #5420-#5429
  init_readonly_param("_x", NP_X, PA_USE_LOOKUP);
  init_readonly_param("_y", NP_Y, PA_USE_LOOKUP);
  init_readonly_param("_z", NP_Z, PA_USE_LOOKUP);
  init_readonly_param("_a", NP_A, PA_USE_LOOKUP);
  init_readonly_param("_b", NP_B, PA_USE_LOOKUP);
  init_readonly_param("_c", NP_C, PA_USE_LOOKUP);
  init_readonly_param("_u", NP_U, PA_USE_LOOKUP);
  init_readonly_param("_v", NP_V, PA_USE_LOOKUP);
  init_readonly_param("_w", NP_W, PA_USE_LOOKUP);

  // last (optional) endsub/return value
  init_readonly_param("_value", NP_VALUE, PA_USE_LOOKUP);

  // predicate: last NGC procedure did return a value on endsub/return
  init_readonly_param("_value_returned", NP_VALUE_RETURNED, PA_USE_LOOKUP);

  // debugging aids
  init_readonly_param("_call_level", NP_CALL_LEVEL, PA_USE_LOOKUP);
  init_readonly_param("_remap_level", NP_REMAP_LEVEL, PA_USE_LOOKUP);

  return INTERP_OK;
}
