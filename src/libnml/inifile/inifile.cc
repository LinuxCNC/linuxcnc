/*
   inifile.cc

   INI file reader

   Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
   */


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
INIFILE::INIFILE()
{
    fp = NULL;
}

INIFILE::INIFILE(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	fprintf(stderr, "can't open %s\n", path);
    }
}

INIFILE::~INIFILE()
{
    if (NULL != fp) {
	fclose(fp);
    }
}

const int INIFILE::open(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	return -1;
    }
    return 0;
}

const int INIFILE::close()
{
    int retval = 0;
    if (fp != NULL) {
	retval = fclose(fp);
	fp = NULL;
    }
    return retval;
}

const char *INIFILE::find(const char *tag, const char *section)
{
    return iniFind(fp, tag, section);
}

/*
   given 'section' and array of strings, fills strings with what was
   found in the section, one line per string. Comments and blank lines
   are omitted. 'array' is assumed to be allocated, of 'max' entries
   of size INIFILE_MAX_LINELEN.

   Returns number of entries found; 0 if section is there but no entries
   in it, or -1 if section is not there.
*/

int INIFILE::section(const char *section, INIFILE_ENTRY array[], int max)
{
    return iniSection(fp, section, array, max);
}

const int INIFILE::valid()
{
    if (NULL == fp)
	return 0;
    return 1;
}
