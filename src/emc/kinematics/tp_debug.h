/********************************************************************
* Description: tc_debug.h
*
*
* Author: Robert W. Ellenberg
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2013 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef TP_DEBUG_H
#define TP_DEBUG_H

#ifdef TP_DEBUG
//Kludge because I didn't know any better at the time
//FIXME replace these with better names?
#define tp_debug_print(...) rtapi_print(__VA_ARGS__)
#define STATIC

#else

#define tp_debug_print(...) 
#define STATIC static

#endif

#ifdef TC_DEBUG
#define tc_debug_print(...) rtapi_print(__VA_ARGS__)
#else
#define tc_debug_print(...) 
#endif

#endif
