/*
   inifile.cc

   INI file reader

   Modification history:

   18-Dec-1997  FMP split out C code into _inifile.c
   7-Nov-1997  FMP fixed bug in afterequal so that it terminates at a 0
   25-Jul-1996  FMP added find_section() and ::section()
   11-Jul-1996  Fred Proctor made sure ini_find() returned NULL if a
   section were provided and no match was detected when the section
   was left; fixed bug which required the last line to have a newline
   */

extern "C" {

#include <stdio.h>		/* FILE, fopen(), fclose(), NULL */
#include <string.h>		/* strstr() */
#include <ctype.h>		/* isspace() */
#include <stdlib.h>		/* exit() */
#include <stdarg.h>		/* va_list */

}
#include "inifile.hh"
IniFile::IniFile()
{
    fp = NULL;
}

IniFile::IniFile(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	fprintf(stderr, "can't open %s\n", path);
    }
}

IniFile::~IniFile()
{
    if (NULL != fp) {
	fclose(fp);
    }
}

const int IniFile::open(const char *path)
{
    if (NULL == (fp = fopen(path, "r"))) {
	return -1;
    }
    return 0;
}

const int IniFile::close()
{
    int retval = 0;
    if (fp != NULL) {
	retval = fclose(fp);
	fp = NULL;
    }
    return retval;
}

const char *IniFile::find(const char *tag, const char *section)
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

int IniFile::section(const char *section, INIFILE_ENTRY array[], int max)
{
    return iniSection(fp, section, array, max);
}

const int IniFile::valid()
{
    if (NULL == fp)
	return 0;
    return 1;
}
