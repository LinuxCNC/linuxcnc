/********************************************************************
* Description: _inifile.c
*   C INI file reader
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/

#include "inifile.h"
#include <stdio.h>		/* FILE *, fopen(), fclose(), NULL */
#include <string.h>		/* strlen(), etc. */
#include <ctype.h>		/* isspace() */

/********************************************************************
*
* Description: const char *afterequal(const char *string)
*		Ignoring any tabs, spaces or other white spaces,
*		finds the first character after an '='.
*
* Return Value: NULL or pointer to first non-white char after an '='
*
* Side Effects: None.
*
* Called By: iniFind() and iniSection()
*
********************************************************************/
static const char *afterequal(const char *string)
{
    const char *spot = string;	/* non-reentrant */

    for (;;) {
	if (*spot == '=') {
	    /* = is there-- return next non-white, or NULL if not there */
	    for (;;) {
		spot++;
		if (0 == *spot) {
		    /* ran out */
		    return NULL;
		} else if (*spot != ' ' && *spot != '\t' && *spot != '\r'
		    && *spot != '\n') {
		    /* matched! */
		    return spot;
		} else {
		    /* keep going for the text */
		    continue;
		}
	    }
	} else if (*spot == 0) {
	    /* end of string */
	    return NULL;
	} else {
	    /* haven't seen '=' yet-- keep going */
	    spot++;
	    continue;
	}
    }
}

/* returns ptr to first non-white in string, or NULL if it's all white */
/********************************************************************
*
* Description: char *skipwhite(char *string)
*		Finds the first non-white character on a new line
*		and returns a pointer.
*		Ignores any line that starts with a comment char
*		i.e. a ';' or '#'.
*
* Return Value: NULL if not found or a valid pointer.
*
* Side Effects: None.
*
* Called By: findSection() iniFind() iniSection()
*
********************************************************************/
static char *skipwhite(char *string)
{
    for (;;) {
	if (*string == 0) {
	    return NULL;
	}

	if ((*string == ';') || (*string == '#')) {
	    return NULL;
	}

	if (*string != ' ' && *string != '\t' && *string != '\r'
	    && *string != '\n') {
	    return string;
	}

	string++;
    }
}

/********************************************************************
*
* Description: findSection(void *fp, const char *section)
*		Positions the pointer to the first line after a
*		[section] tag.
*
* Return Value: Returns zero on success or -1 if tag not found or EOF reached.
*
* Side Effects: 
*
* Called By: iniSection()
*
********************************************************************/

int findSection(void *fp, const char *section)
{
    static char line[LINELEN + 2];	/* 1 for newline, 1 for NULL */
    static char bracketsection[LINELEN + 2];
    char *nonwhite;

    /* check valid file */
    if (NULL == fp) {
	return -1;
    }

    /* start from beginning */
    rewind((FILE *) fp);

    /* if section is NULL, we're already positioned */
    if (NULL == section) {
	return 0;
    }

    /* wrap section in brackets, so it matches */
    sprintf(bracketsection, "[%s]", section);

    /* find [section], and position fp just after it */
    while (!feof((FILE *) fp)) {

	if (NULL == fgets(line, LINELEN + 1, (FILE *) fp)) {
	    /* got to end of file without finding it */
	    return -1;
	}

	/* got a line-- check it for real data, not comment or blank line */
	if (NULL == (nonwhite = skipwhite(line))) {
	    /* blank line-- skip it */
	    continue;
	}

	/* not a blank line-- compare with section tag */
	if (0 != strncmp(bracketsection, nonwhite, strlen(bracketsection))) {
	    /* not on this line */
	    continue;
	}

	/* else it matches-- fp is now set up for search on tag */
	return 0;
    }

    /* didn't find it */
    return -1;
}

/********************************************************************
*
* Description: char *iniFind(void *fp, const char *tag, const char *section)
*		Locates the start of a string associated with a tag.
*
*		 e.g., in a file that contains
*
*		  PRINTER=fred
*
*		   iniFind("PRINTER");   would return "fred", and
*		   iniFind("printer");   would return NULL.
*
* Return Value: NULL if not found or a string.
*
* Side Effects: The file pointer is undefined upon return.
*
* Called By: inivar.c inifile.cc usrmotintf.c
*
********************************************************************/
const char *iniFind(void *fp, const char *tag, const char *section)
{
    static char line[LINELEN + 2];	/* 1 for newline, 1 for NULL */
    static char bracketsection[LINELEN + 2];
    char *nonwhite;
    int newlinepos;		/* position of newline to strip */
    int len;
    char tagend;
    char *value_string;
    char *end_value_string;
    /* check valid file */
    if (NULL == fp)
	return NULL;

    /* start from beginning */
    rewind((FILE *) fp);

    /* check for section first-- if it's non-NULL, then position file at line
       after [section] */
    if (NULL != section) {
	sprintf(bracketsection, "[%s]", section);

	/* find [section], and position fp just after it */

	while (!feof((FILE *) fp)) {

	    if (NULL == fgets(line, LINELEN + 1, (FILE *) fp)) {
		/* got to end of file without finding it */
		return NULL;
	    }
	    /* got a line */

	    /* strip off newline */
	    newlinepos = strlen(line) - 1;	/* newline is on back from 0 */
	    if (newlinepos < 0) {
		newlinepos = 0;
	    }
	    if (line[newlinepos] == '\n') {
		line[newlinepos] = 0;	/* make the newline 0 */
	    }

	    if (NULL == (nonwhite = skipwhite(line))) {
		/* blank line-- skip */
		continue;
	    }

	    /* not a blank line, and nonwhite is first char */
	    if (0 !=
		strncmp(bracketsection, nonwhite, strlen(bracketsection))) {
		/* not on this line */
		continue;
	    }

	    /* it matches-- fp is now set up for search on tag */
	    break;
	}
    }
    while (!feof((FILE *) fp)) {
	/* check for end of file */
	if (NULL == fgets(line, LINELEN + 1, (FILE *) fp)) {
	    /* got to end of file without finding it */
	    return NULL;
	}

	/* got a line */

	/* strip off newline */
	newlinepos = strlen(line) - 1;	/* newline is on back from 0 */
	if (newlinepos < 0) {
	    newlinepos = 0;
	}
	if (line[newlinepos] == '\n') {
	    line[newlinepos] = 0;	/* make the newline 0 */
	}

	/* skip leading whitespace */
	if (NULL == (nonwhite = skipwhite(line))) {
	    /* blank line-- skip */
	    continue;
	}

	/* check for '[' char-- if so, it's a section tag, and we're out of
	   our section */
	if (NULL != section && nonwhite[0] == '[') {
	    return NULL;
	}

	len = strlen(tag);

	if (0 != strncmp(tag, nonwhite, len)) {
	    /* not on this line */
	    continue;
	}

	/* it matches the first part of the string-- if whitespace or = is
	   next char then call it a match */
	tagend = nonwhite[len];
	if (tagend == ' ' || tagend == '\r' || tagend == '\t'
	    || tagend == '\n' || tagend == '=') {
	    /* it matches-- return string after =, or NULL */
	    nonwhite += len;
	    /* Cast is needed  because we are discarding the const. */
	    value_string = (char *) afterequal(nonwhite);
	    /* Eliminate white space at the end of a line also. */
	    if (NULL == value_string) {
		return NULL;
	    }
	    end_value_string = value_string + strlen(value_string) - 1;
	    while (*end_value_string == ' ' || *end_value_string == '\t'
		|| *end_value_string == '\r') {
		*end_value_string = 0;
		end_value_string--;
	    }
	    return value_string;
	}
	/* else continue */
    }

    return NULL;
}

/*

   Returns number of entries found; 0 if section is there but no entries
   in it, or -1 if section is not there.
*/

/********************************************************************
*
* Description: iniSection(void *fp, const char *section, INIFILE_ENTRY array[], int max)
*		Given 'section' and array of strings, fills strings with
*		what was found in the section, one line per string.
*		Comments and blank lines are omitted. 'array' is assumed
*		to be allocated, of 'max' entries of size 
*		INIFILE_MAX_LINELEN.
*
* Return Value: Number of entries found, zero if none found, or
*		-1 if no section.
*
* Side Effects:
*
* Called By: inifile.cc
*
********************************************************************/
int iniSection(void *fp, const char *section, INIFILE_ENTRY array[], int max)
{
    static char line[LINELEN + 2];	/* 1 for newline, 1 for NULL */
    int count = 0;
    char *nonwhite;
    int newlinepos;
    const char *entry;

    if (NULL == fp) {
	return -1;
    }

    /* position at section */
    if (-1 == findSection(fp, section)) {
	/* didn't find it */
	return -1;
    }

    /* found section-- start loading lines */

    while (!feof((FILE *) fp) && count < max) {
	if (NULL == fgets(line, LINELEN + 1, (FILE *) fp)) {
	    /* got to end of file without finding it */
	    return count;
	}

	/* got a line-- check for blank */
	if (NULL == (nonwhite = skipwhite(line))) {
	    continue;
	}

	/* check for new section-- if so, we're done */
	if ('[' == *nonwhite) {
	    return count;
	}

	/* strip off newline */
	newlinepos = strlen(line) - 1;	/* newline is one back from 0 */
	if (newlinepos < 0) {
	    newlinepos = 0;
	}
	if (line[newlinepos] == '\n') {
	    line[newlinepos] = 0;	/* make the newline 0 */
	}

	/* it's a valid line-- copy into entry struct */

	/* read first tag */
	sscanf(line, "%s", array[count].tag);

	/* copy everything after equal to the rest */

	entry = afterequal(line);
	if (NULL != entry) {
	    strcpy(array[count].rest, afterequal(line));
	} else {
	    array[count].rest[0] = 0;	/* make it the empty string */
	}
	count++;
    }

    /* got to end of file */
    return count;
}
