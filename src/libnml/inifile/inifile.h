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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>		/* FILE, NULL */

#define INIFILE_MAX_LINELEN 256	/* max number of chars in a line */

    typedef struct {
	char tag[INIFILE_MAX_LINELEN];
	char line[INIFILE_MAX_LINELEN];
    } inifile_line;

    extern const char *iniFind(void *fp,	/* already opened file ptr */
	const char *tag,	/* string to find */
	const char *section);	/* section it's in */

    extern int iniSection(void *fp,	/* already opened file ptr */
	const char *section,	/* section you want */
	inifile_line array[],	/* entries to fill */
	int max);		/* how many you can hold */
    extern int iniFill(void *fp, 
	inifile_line array[]);

    extern int findSection(void *fp, const char *section);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "inifile.hh"
#endif /* C++ part */

#endif /* INIFILE_H */
