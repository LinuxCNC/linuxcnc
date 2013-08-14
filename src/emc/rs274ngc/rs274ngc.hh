/********************************************************************
* Description: rs274ngc.hh
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef RS274NGC_HH
#define RS274NGC_HH

#include "interp_base.hh"



/**********************/
/* INCLUDE DIRECTIVES */
/**********************/

#include <stdio.h>
#include "canon.hh"
#include "emc.hh"
#include "debugflags.h"

typedef struct setup_struct setup;
#ifndef JAVA_DIAG_APPLET
typedef setup *setup_pointer;
#endif
typedef struct block_struct block;
#ifndef JAVA_DIAG_APPLET
typedef block *block_pointer;
#endif

typedef struct remap_struct remap;
typedef remap *remap_pointer;

typedef struct context_struct context;
typedef context *context_pointer;

typedef struct offset_struct offset;
typedef offset *offset_pointer;

// Declare class so that we can use it in the typedef.
class Interp;
typedef int (Interp::*read_function_pointer) (char *, int *, block_pointer, double *);

#define DBG(level,fmt,args...)                  \
    do {                                        \
      if (level < _setup.loggingLevel) {	\
	fprintf(stderr,fmt, ## args);		\
      }                                         \
    } while (0)

// print to if RS274NGC/LOG_LEVEL > 1:

#define MSG(fmt,args...)                        \
    do {                                        \
      DBG(0, fmt, ##args);                      \
    } while (0)


#undef DEBUG_EMC


#define _logDebug(mask,dlflags,level, fmt, args...)	\
    do {						\
	if (((mask & _setup.debugmask) &&		\
	     (level < _setup.loggingLevel)) ||		\
            (mask & EMC_DEBUG_UNCONDITIONAL)) {		\
	    doLog(dlflags,				\
		  __FILE__,				\
		  __LINE__ ,				\
		  fmt "\n",				\
		  ## args);				\
        }						\
    } while(0)

//#define logDebug(fmt, args...)  _logDebug(EMC_DEBUG_INTERP,LOG_FILENAME,1,fmt, ## args)
#define logDebug(fmt, args...)  _logDebug(EMC_DEBUG_INTERP,0,1,fmt, ## args)

#define logConfig(fmt, args...) _logDebug(EMC_DEBUG_CONFIG,0,1,fmt, ## args)
#define logOword(fmt, args...)  _logDebug(EMC_DEBUG_OWORD,0,1,fmt, ## args)
#define logRemap(fmt, args...)  _logDebug(EMC_DEBUG_REMAP,0,1,fmt, ## args)
#define logPy(fmt, args...)     _logDebug(EMC_DEBUG_PYTHON,0,1,fmt, ## args)
#define logNP(fmt, args...)     _logDebug(EMC_DEBUG_NAMEDPARAM,0,1,fmt, ## args)

// log always
#define Log(fmt, args...)       _logDebug(EMC_DEBUG_UNCONDITIONAL,LOG_PROCESSID|LOG_FILENAME,-1,fmt, ## args)
#define Error(fmt, args...)       _logDebug(EMC_DEBUG_UNCONDITIONAL,0,-1,fmt, ## args)



#endif
