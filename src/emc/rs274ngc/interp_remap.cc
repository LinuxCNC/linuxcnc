/********************************************************************
 * Description: interp_remap.cc
 *
 *  Remapping support
 *
 * Author: Michael Haberler
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2011 All rights reserved.
 *
 ********************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "rs274ngc_interp.hh"
#include "interp_internal.hh"

namespace bp = boost::python;


bool Interp::has_user_mcode(setup_pointer settings,block_pointer block)
{
    unsigned i;
    for(i = 0; i < sizeof(block->m_modes)/sizeof(int); i++) {
	if (block->m_modes[i] == -1)
	    continue;
	if (M_REMAPPABLE(block->m_modes[i]) &&
	    settings->m_remapped[block->m_modes[i]])
	    return true;
    }
    return false;
}

int Interp::convert_remapped_code(block_pointer block,
				  setup_pointer settings,
				  int phase,
				  char letter,
				  int number)
{
    remap_pointer remap;
    char key[2];
    int status;
    block_pointer cblock;
    bp::list plist;
    char cmd[LINELEN];

    if (number == -1)
	logRemap("convert_remapped_code '%c'", letter);
    else
	logRemap("convert_remapped_code '%c%d'", letter, number);

    switch (toupper(letter)) {
    case 'M':
	remap = settings->m_remapped[number];
	break;
    case 'G':
	remap = settings->g_remapped[number];
	break;
    default:
	key[0] = letter;
	key[1] = '\0';
	remap = remapping((const char *)key);
    }
    CHKS((remap == NULL), "BUG: convert_remapped_code: no remapping");

    settings->sequence_number = 1; // FIXME not sure..

    // some remapped handlers may need c/c++ or Python code to
    // setup environment before, and finish work after doing theirs.
    // That's what prolog and epilog functions are for.
    // Statically these are described in the remap descriptor (read from ini).

    // Since a remap is always executed in the context of a block,
    // block now contains fields which hold dynamic remap information.
    // information. Some of these fields are initialized here -
    // conceptually the block stack is also a 'remap frame stack'.

    // the O_call code will pick up the static descriptor and
    // dynamic information through the block and call any prolog
    // function before passing control to the actual handler procedure.

    // On the corresponding O_endsub/O_return, any epilog function
    // will be executed, doing any work not doable in an NGC file.
    // the remap_* fields are  essentially hidden parameters to the call
    // (we do not want to expose boost.python objects in method paramters -
    // this impacts too much code)

    // Note that even Python-remapped execution is pulled through the
    // oword mechanism - so no duplication of handler calling code
    // is needed.

    snprintf(cmd, sizeof(cmd),"O <%s> call ", REMAP_FUNC(remap));

    // the controlling block holds all dynamic remap information.
    cblock = &CONTROLLING_BLOCK(*settings);
    cblock->executing_remap = remap; // the current descriptor
    cblock->py_returned_userdata = 0;
    cblock->user_data = 0;
    cblock->param_cnt = 0;

    // build positional args for any Python pro/epilogs here
    cblock->tupleargs = bp::make_tuple(plist);

    // build kwargs for  any Python pro/epilogs if an argspec
    // was given - add_parameters will decorate remap_kwargs as per argspec
    cblock->kwargs = boost::python::dict();
    if (remap->argspec) {
	// we're inserting locals into a callframe which isnt used yet
	// NB: this assumes read() doesnt clear the callframe
	settings->call_level++;
	// create a positional argument list instead of local variables
	// if user specified 'posargs=true'
	CHP(add_parameters(settings, cblock,
			   remap->posarg ? &cmd[strlen(cmd)] : NULL));
	settings->call_level--;
    }

    if ((_setup.debugmask & EMC_DEBUG_REMAP) &&
	(_setup.loggingLevel > 2)) {
	logRemap("convert_remapped_code(%s)", cmd);
    }

    // good to go, pass to o-word call handling mechanism
    status = read(cmd);
    CHKS(status != INTERP_OK,
	 "convert_remapped_code: inital read returned %s",
	 interp_status(status));
    return(- phase);
}



// this looks up a remapping by unnormalized code (like G88.1)
remap_pointer Interp::remapping(const char *code)
{
    //logRemap("lookup remapping(code=%s)",code);
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
	logRemap("prolog_func = %s",
		 (r->prolog_func ? r->prolog_func : ""));
	logRemap("remap_py = %s",
		 (r->remap_py ? r->remap_py : ""));
	logRemap("remap_ngc = %s",
		 (r->remap_ngc ? r->remap_ngc : ""));
	logRemap("epilog_func = %s",
		 (r->epilog_func ? r->epilog_func : ""));
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

// parse options of the form:
// REMAP= M420 modalgroup=6 argspec=pq prolog=setnamedvars ngc=m43.ngc epilog=ignore_retvalue
// REMAP= M421 modalgroup=6 argspec=- prolog=setnamedvars python=m43func epilog=ignore_retvalue

int Interp::parse_remap(const char *inistring, int lineno)
{

    char iniline[LINELEN];
    char *argv[MAX_REMAPOPTS];
    int   argc = 0;
    const char *code;
    remap_pointer r;
    bool errored = false;
    int g1 = 0, g2 = 0;
    int mcode = -1;
    int gcode = -1;
    char *s;

    if ((r = (remap_pointer) malloc(sizeof(remap))) == NULL) {
	Error("cant malloc remap_struct");
	return INTERP_ERROR;
    }
    memset((void *)r, 0, sizeof(remap));
    r->modal_group = -1; // mark as unset, required param for m/g
    strcpy(iniline, inistring);
    // strip trailing comments
    if ((s = strchr(iniline, '#')) != NULL) {
	*s = '\0';
    }
    s = strtok((char *) iniline, " \t");

    while( s != NULL && argc < MAX_REMAPOPTS - 1) {
	argv[argc++] = s;
	s = strtok( NULL, " \t" );
    }
    if (argc == MAX_REMAPOPTS) {
	Error("parse_remap: too many arguments (max %d)", MAX_REMAPOPTS);
	goto fail;
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
				 "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnpqrstuvwxyz>^");
	    if (pos != strlen(arg)) {
		Error("argspec: illegal word '%c' - %d:REMAP = %s",
		      arg[pos],lineno,inistring);
		errored = true;
		continue;
	    }
	    r->argspec = strstore(arg);
	    continue;
	}
	if (!strncasecmp(kw,"posargs",kwlen)) {
	    if (strcasecmp(arg,"true")) {
		Error("posarg: '%s': unrecognized option - %d:REMAP = %s",
		      arg,lineno,inistring);
		errored = true;
		continue;
	    }
	    r->posarg = true;
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
	goto fail;
    }

    if (remapping(code)) {
	Error("code '%s' already remapped : %d:REMAP = %s",
	      code,lineno,inistring);
	goto fail;
    }

    // it is an error not to define a remap function to call.
    if ((r->remap_ngc == NULL) && (r->remap_py == NULL)) {
	Error("code '%s' - no remap function given, use either 'python=<function>' or 'ngc=<basename>' : %d:REMAP = %s",
	      code,lineno,inistring);
	goto fail;
    }

#define CHECK(bad, fmt, ...)			\
    do {					\
	if (bad) {				\
	    Log(fmt, ## __VA_ARGS__);		\
	    goto fail;				\
	}					\
    } while(0)

    switch (towlower(*code)) {

    case 't':
    case 's':
    case 'f':
	CHECK((strlen(code) > 1),"%d: %c remap - only single letter code allowed", lineno, *code);
	CHECK((r->modal_group != -1), "%d: %c remap - modal group setting ignored - fixed sequencing", lineno, *code);
	_setup.remaps[code] = r;
	break;

    case 'm':
	if (sscanf(code + 1, "%d", &mcode) == 1) {
	    _setup.remaps[code] = r;
	    _setup.m_remapped[mcode] = r;
	} else {
	    Error("parsing M-code: expecting integer like 'M420', got '%s' : %d:REMAP = %s",
		  code,lineno,inistring);
	    goto fail;
	}
	if (r->modal_group == -1) {
	    Error("code '%s' : no modalgroup=<int> given : %d:REMAP = %s",
		  code,lineno,inistring);
	    goto fail;
	}
	if (!M_MODE_OK(r->modal_group)) {
	    Error("code '%s' : invalid modalgroup=<int> given (currently valid: 5..10) : %d:REMAP = %s",
		  code,lineno,inistring);
	    goto fail;
	}
	break;
    case 'g':

	// code may be G88.1 or so - normalize to use 'G881' instead
	// (multiply by 10, no dots)
	if (sscanf(code + 1, "%d.%d", &g1, &g2) == 2) {
	    gcode = g1 * 10 + g2;
	}
	if (( gcode == -1) &&  (sscanf(code + 1, "%d", &gcode) != 1)) {
	    Error("code '%s' : cant parse G-code : %d:REMAP = %s",
		  code, lineno, inistring);
	    goto fail;
	}
	if (!G_MODE_OK(r->modal_group)) {
	    Error("code '%s' : %s modalgroup=<int> given, def : %d:REMAP = %s",
		  argv[0],
		  r->modal_group == -1 ? "no" : "invalid",
		  lineno,
		  inistring);
	    goto fail;
	}
	if (r->modal_group == -1) {
	    Error("code '%s' : no modalgroup=<int> given : %d:REMAP = %s",
		  code,lineno,inistring);
	    goto fail;
	}
	if (!G_MODE_OK(r->modal_group)) {
	    Error("code '%s' : invalid modalgroup=<int> given (currently valid: 1) : %d:REMAP = %s",
		  code,lineno,inistring);
	    goto fail;
	}
	_setup.remaps[code] = r;
	_setup.g_remapped[gcode] = r;
	break;

    default:
	Log("REMAP BUG=%s %d:REMAP = %s",
	    code,lineno,inistring);
    }
    // logRemap("success: %d: REMAP=%s line=%s",
    // 	     lineno, code, inistring);

    return INTERP_OK;

 fail:
    free(r);
    return INTERP_ERROR;
}
