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
#include "rcs_print.hh"

/*********************************************************************
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
IniFile::IniFile()
{
    fp = NULL;
}

/*!*******************************************************************
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
IniFile::IniFile(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	fprintf(stderr, "can't open %s\n", path);
    }
}

/*!*******************************************************************
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
IniFile::~IniFile()
{
    if (NULL != fp) {
	fclose(fp);
    }
}

/*!*******************************************************************
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
const int IniFile::open(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	return -1;
    }
    return 0;
}

/*!******************************************************************
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
const int IniFile::close()
{
    int retval = 0;
    if (fp != NULL) {
	retval = fclose(fp);
	fp = NULL;
    }
    return retval;
}

/*! Locates a tag in [section] of file
  \param tag The tag to be searched for
  \param section the section to search in
  \return pointer to the first non-white char after an = or NULL if not found or eof reached.

/*
* Side Effects:
*
* Called By: 
*
*/
const char *IniFile::find(const char *tag, const char *section)
{
    return iniFind(fp, tag, section);
}

/*! Given 'section' and array of strings, fills array[] with the contents
    of the section, one line per string. Comments and blank lines are
    omitted. 'array' is assumed to be allocated, of 'max' entries of
    size INIFILE_MAX_LINELEN.
\param section to use
\param array[] Array of 'max' lines of INIFILE_MAX_LINE_LEN
\param max Maximum number of lines to copy in to the array

\return Number of entries found - 0 if section is there but no entries
	 in it, or -1 if section is not found.
*/
int IniFile::section(const char *section, INIFILE_ENTRY array[], int max)
{
    return iniSection(fp, section, array, max);
}

/*!
    Fills the array with the contents of the file. One line per
    array member. 'array' is assumed to be dynamically allocated
    and will grow if it is not large enough to accomodate all the
    lines.
    
    Side Effects: Memory allocated to array[] may grow or shrink.
		  Use the return value to index the array.

    \param array[] of iniile_lines allocated with malloc()
    \return Number of entries found, zero if none found, or -1 on error.
*/
int IniFile::fill(INIFILE_ENTRY array[])
{
    if (array == NULL) {
	return -1;
    
    return iniFill(fp, array);
}

/*! Reports if the file descriptor used in the constructor is valid.

\return TRUE if the fd is valid and file open or FALSE if not valid.

*/
const bool IniFile::valid()
{
    if (fp == NULL) {
	return false;
    }
    return true;
}
