/********************************************************************
* Description: inifile.cc
*   C++ INI file reader
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>		/* FILE *, fopen(), fclose(), NULL */
#include <string.h>		/* strstr() */
#include <ctype.h>		/* isspace() */
#include <stdlib.h>		/* exit() */
#include <stdarg.h>		/* va_list */

#ifdef __cplusplus
}
#endif

#include "inifile.h"
#include "inifile.hh"
#include "rcs_print.hh"

/********************************************************************
*
* Description: Constructor.
*
* Return Value: None.
*
* Side Effects: File descriptor is set to NULL, so open("file.name")
*               must be used before any search.
*
* Called By: 
*
********************************************************************/
INIFILE::INIFILE()
{
    fp = NULL;
}

/********************************************************************
*
* Description: Constructor with a file name.
*
* Return Value: None
*
* Side Effects:
*
* Called By: 
*
********************************************************************/
INIFILE::INIFILE(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	fprintf(stderr, "can't open %s\n", path);
    }
}

/********************************************************************
*
* Description: Destructor
*
* Return Value: None
*
* Side Effects: Releases the file descriptor and any memory allocated.
*
* Called By: 
*
********************************************************************/
INIFILE::~INIFILE()
{
    if (NULL != fp) {
	fclose(fp);
    }
}

/********************************************************************
*
* Description: Opens the file for reading.
*
* Return Value: 0 on success
*               -1 on failure - Either file not found, or a permissions err.
*
* Side Effects: If a file was already open, the handle is lost.
*               FIX ME - Check and free any FD first ?
*
* Called By: 
*
********************************************************************/
const int INIFILE::open(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	return -1;
    }
    return 0;
}

/********************************************************************
*
* Description: Closes the file descriptor..
*
* Return Value: 0 on success
*               -1 if an error occurred.
*
* Side Effects:
*
* Called By: 
*
********************************************************************/
const int INIFILE::close()
{
    int retval = 0;
    if (fp != NULL) {
	retval = fclose(fp);
	fp = NULL;
    }
    return retval;
}

/********************************************************************
*
* Description: Finds the tag in section.
*
* Return Value: First item after an '=' as a string.
*               NULL if not found.
*
* Side Effects:
*
* Called By: 
*
********************************************************************/
const char *INIFILE::find(const char *tag, const char *section)
{
    return iniFind(fp, tag, section);
}

/********************************************************************
*
* Description: section(const char *section, INIFILE_ENTRY array[], int max)
*               given 'section' and array of strings, fills strings
*               with what was found in the section, one line per string.
*               Comments and blank lines are omitted. 'array' is assumed
*               to be allocated, of 'max' entries of size INIFILE_MAX_LINELEN.
*
* Return Value: Returns number of entries found
*               0 if section is there but no entries in it, or
*               -1 if section is not found.
*
* Side Effects:
*
* Called By: 
*
********************************************************************/
int INIFILE::section(const char *section, INIFILE_ENTRY array[], int max)
{
    return iniSection(fp, section, array, max);
}

/********************************************************************
*
* Description: valid()
*               Reports if the file descriptor used in the constructor
*               is valid.
*
* Return Value: 1 if the fd is valid and file open.
*               0 if not valid.
*
* Side Effects:
*
* Called By: 
*
********************************************************************/
const int INIFILE::valid()
{
    if (NULL == fp) {
	return 0;
    }
    return 1;
}
