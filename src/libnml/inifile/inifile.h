/********************************************************************
* Description: inifile.h
*   Declarations for INI file format functions
*
*   Derived from a work by Fred Proctor & Will Shackleford
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

#ifndef INIFILE_H
#define INIFILE_H

#include "global_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>		/* FILE, NULL */

    typedef struct {
	char tag[LINELEN];
	char rest[LINELEN];
    } INIFILE_ENTRY;

    extern const char *iniFind(void *fp,	/* already opened file ptr */
	const char *tag,	/* string to find */
	const char *section);	/* section it's in */

    extern int iniSection(void *fp,	/* already opened file ptr */
	const char *section,	/* section you want */
	INIFILE_ENTRY array[],	/* entries to fill */
	int max);		/* how many you can hold */

#ifdef __cplusplus
}
#endif

#endif /* INIFILE_H */
