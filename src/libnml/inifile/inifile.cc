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

extern "C" {
#include <stdio.h>		/* FILE *, fopen(), fclose(), NULL */
#include <string.h>		/* strstr() */
#include <ctype.h>		/* isspace() */
#include <stdlib.h>		/* exit() */
#include <stdarg.h>		/* va_list */
#include <fcntl.h>
}
#include "inifile.hh"
#include "rcs_print.hh"

/*! File descriptor is set to NULL, so open("file.name") must be used
   before any search.

   @return None. */
Inifile::Inifile()
{
    fp = NULL;
}

/*! Constructor with a file name.

   @param file File name to open.

   @return None */
Inifile::Inifile(const char *file)
{
    if (open(file) == false) {
	fprintf(stderr, "can't open %s\n", file);
    }
}

/*! Releases the file descriptor and any memory allocated.

   @return None */
Inifile::~Inifile()
{
    close();
}

/*! Opens the file for reading. If a file was already open, it is closed
   and the new one opened.

   @return true on success, false on failure */
bool Inifile::open(const char *file)
{
    /*! \todo fixme Need to do tilde expansion here */
    if ((fp = fopen(file, "r")) == NULL) {
	return false;
    }
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fileno(fp), F_SETLK, &lock) == -1) {
	fprintf(stderr, "Unable to lock file %s\n", file);
	fclose(fp);
	fp = NULL;
	return false;
    }
    return true;
}

/*! Closes the file descriptor..

   @return true on success, false on failure */
bool Inifile::close()
{
    int tmp = 0;
    if (fp != NULL) {
	lock.l_type = F_UNLCK;
	fcntl(fileno(fp), F_SETLKW, &lock);
	tmp = fclose(fp);
	fp = NULL;
    }
    return ((tmp == 0) ? true : false);
}

/*! Reports if the file is open and valid.

   @return true if data is valid */
bool Inifile::valid()
{
    return ((fp != NULL) ? true : false);
}

/*! Writes the contents of memory back to the file.

    @return Error code if write failed. */
int Inifile::write()
{
    int tmp = 0;
    if (fp == NULL) {
	return -1;		/* error - File closed. */
    }
    /* Write file to disk
       Set tmp to the err code if write fails */
    return tmp;
};

/*! Finds the nth tag in section.

   @param tag Entry in the ini file to find.

   @param section The section to look for the tag.

   @param num (optionally) the Nth occurrence of the tag.

   @return pointer to the the variable after the '=' delimiter */
const char *Inifile::find(const char *tag, const char *section, int num)
{
    static char line[LINELEN + 2] = "";	/* 1 for newline, 1 for NULL */
    char bracketsection[LINELEN + 2] = "";
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

    /* check for section first-- if it's non-NULL, then position file at
       line after [section] */
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

	    if (NULL == (nonwhite = skip_white(line))) {
		/* blank line-- skip */
		continue;
	    }

	    /* not a blank line, and nonwhite is first char */
	    if (strncmp(bracketsection, nonwhite, strlen(bracketsection))
		!= 0) {
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
	if (NULL == (nonwhite = skip_white(line))) {
	    /* blank line-- skip */
	    continue;
	}

	/* check for '[' char-- if so, it's a section tag, and we're out
	   of our section */
	if (NULL != section && nonwhite[0] == '[') {
	    return NULL;
	}

	len = strlen(tag);
	if (strncmp(tag, nonwhite, len) != 0) {
	    /* not on this line */
	    continue;
	}

	if (--num > 0) {
	    /* Not looking for the first one, so skip it... */
	    continue;
	}

	/* it matches the first part of the string-- if whitespace or = is
	   next char then call it a match */
	tagend = nonwhite[len];
	if (tagend == ' ' || tagend == '\r' || tagend == '\t'
	    || tagend == '\n' || tagend == '=') {
	    /* it matches-- return string after =, or NULL */
	    nonwhite += len;
	    value_string = after_equal(nonwhite);
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

/*! given 'section' and array of strings, fills strings with what was
   found in the section, one line per string. Comments and blank lines are 
   omitted. 'array' is assumed to be allocated, of 'max' entries of size
   LINELEN.

   @param section The setion to read.

   @param array[] array to fill

   @param max Maximum entries to read.

   @return number of entries found 0 if section is there but no entries in 
   it, or -1 if section is not found. */
int Inifile::section(const char *section, inifile_entry array[], int max)
{
    char line[LINELEN + 2];	/* 1 for newline, 1 for NULL */
    char bracketsection[LINELEN + 2];
    char *nonwhite;

    /* check valid file */
    if (NULL == fp) {
	return -1;
    }

    /* start from beginning */
    rewind((FILE *) fp);

    /* if section is NULL, we're already positioned */
    if (section == NULL) {
	return 0;
    }

    /* wrap section in brackets, so it matches */
    sprintf(bracketsection, "[%s]", section);

    /* find [section], and position fp just after it */
    while (!feof((FILE *) fp)) {

	if (fgets(line, LINELEN + 1, (FILE *) fp) == NULL) {
	    /* got to end of file without finding it */
	    return -1;
	}

	/* got a line-- check it for real data, not comment or blank line */
	if ((nonwhite = skip_white(line)) == NULL) {
	    /* blank line-- skip it */
	    continue;
	}

	/* not a blank line-- compare with section tag */
	if (strncmp(bracketsection, nonwhite, strlen(bracketsection)) != 0) {
	    /* not on this line */
	    continue;
	}

	/* else it matches-- fp is now set up for search on tag */
	return 0;
    }

    /* didn't find it */
    return -1;
}

/*! Ignoring any tabs, spaces or other white spaces, finds the first
   character after the '=' delimiter.

   @param string Pointer to the tag

   @return NULL or pointer to first non-white char after the delimiter

   Called By: find() and section() only. */
char *Inifile::after_equal(const char *string)
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
		    return (char *) spot;
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

/*! Finds the first non-white character on a new line and returns a
   pointer. Ignores any line that starts with a comment char i.e. a ';' or 
   '#'.

   @return NULL if not found or a valid pointer.

   Called By: find() and section() only. */
char *Inifile::skip_white(char *string)
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
