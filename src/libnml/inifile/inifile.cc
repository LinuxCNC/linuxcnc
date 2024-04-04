/*****************************************************************************
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
 *****************************************************************************/

#include <stdio.h>              /* FILE *, fopen(), fclose(), NULL */
#include <stdlib.h>
#include <string.h>             /* strstr() */
#include <fcntl.h>


#include "emc/linuxcnc.h"
#include "inifile.hh"

constexpr int MAX_EXTEND_LINES = 20;


IniFile::IniFile(int _errMask, FILE *_fp) : fp(_fp), errMask(_errMask)
{
    if(fp)
        LockFile();
}


/*! Opens the file for reading. If a file was already open, it is closed
   and the new one opened.

   @return true on success, false on failure */
bool
IniFile::Open(const char *file)
{
    char                        path[LINELEN] = "";

    if(IsOpen()) Close();

    TildeExpansion(file, path, sizeof(path));

    if((fp = fopen(path, "r")) == NULL)
        return(false);

    owned = true;

    if(!LockFile())
        return(false);

    return(true);
}


/*! Closes the file descriptor..

   @return true on success, false on failure */
bool
IniFile::Close()
{
    int                         rVal = 0;

    if(fp != NULL){
        lock.l_type = F_UNLCK;
        fcntl(fileno(fp), F_SETLKW, &lock);

        if(owned)
            rVal = fclose(fp);

        fp = NULL;
    }

    return(rVal == 0);
}


IniFile::ErrorCode
IniFile::Find(int *result, StrIntPair *pPair,
     const char *tag, const char *section, int num, int *lineno)
{
    auto pStr = Find(tag, section, num);
    if(!pStr){
        // We really need an ErrorCode return from Find() and should be passing
        // in a buffer. Just pick a suitable ErrorCode for now.
	if (lineno)
	    *lineno = 0;
        return(ERR_TAG_NOT_FOUND);
    }

    int tmp;
    if(sscanf(*pStr, "%i", &tmp) == 1){
        *result = tmp;
	if (lineno)
	    *lineno = lineNo;
        return(ERR_NONE);
    }

    while(pPair->pStr != NULL){
        if(strcasecmp(*pStr, pPair->pStr) == 0){
            *result = pPair->value;
	    if (lineno)
		*lineno = lineNo;
            return(ERR_NONE);
        }
        pPair++;
    }

    ThrowException(ERR_CONVERSION);
    return(ERR_CONVERSION);
}


IniFile::ErrorCode
IniFile::Find(double *result, StrDoublePair *pPair,
     const char *tag, const char *section, int num, int *lineno)
{
    auto pStr = Find(tag, section, num);
    if(!pStr){
        // We really need an ErrorCode return from Find() and should be passing
        // in a buffer. Just pick a suitable ErrorCode for now.
	if (lineno)
	    *lineno = 0;
        return(ERR_TAG_NOT_FOUND);
    }

    double tmp;
    if(sscanf(*pStr, "%lf", &tmp) == 1){
	if (lineno)
	    *lineno = lineNo;
        *result = tmp;
	if (lineno)
	    *lineno = lineNo;
        return(ERR_NONE);
    }

    while(pPair->pStr != NULL){
        if(strcasecmp(*pStr, pPair->pStr) == 0){
            *result = pPair->value;
	    if (lineno)
		*lineno = lineNo;
            return(ERR_NONE);
        }
        pPair++;
    }

    ThrowException(ERR_CONVERSION);
    return(ERR_CONVERSION);
}


/*! Finds the nth tag in section.

   @param tag Entry in the ini file to find.

   @param section The section to look for the tag.

   @param num (optionally) the Nth occurrence of the tag.

   @return pointer to the variable after the '=' delimiter, or @c NULL if not
           found */
std::optional<const char*>
IniFile::Find(const char *_tag, const char *_section, int _num, int *lineno)
{
    // WTF, return a pointer to the middle of a local buffer?
    // FIX: this is totally non-reentrant.
    static char                 line[LINELEN + 2] = "";        /* 1 for newline, 1 for NULL */

    char  eline [(LINELEN + 2) * (MAX_EXTEND_LINES + 1)];
    char* elineptr;
    char* elinenext = eline;
    int   extend_ct = 0;

    if (!_tag) {
        fprintf(stderr, "IniFile: error: Tag is not provided\n");
        return std::nullopt;
    }

    // For exceptions.
    lineNo = 0;
    tag = _tag;
    section = _section;
    num = _num;

    /* check valid file */
    if(!CheckIfOpen())
        return std::nullopt;

    /* start from beginning */
    rewind(fp);

    /* check for section first-- if it's non-NULL, then position file at
       line after [section] */
    if (section) {
        char bracketSection[LINELEN];
        snprintf(bracketSection, sizeof(bracketSection), "[%s]", section);

        /* find [section], and position fp just after it */
        while (!feof(fp)) {

            if (NULL == fgets(line, LINELEN + 1, fp)) {
                /* got to end of file without finding it */
                ThrowException(ERR_SECTION_NOT_FOUND);
                return std::nullopt;
            }

            if (HasInvalidLineEnding(line)) {
                ThrowException(ERR_CONVERSION);
                return std::nullopt;
            }

            /* got a line */
            lineNo++;

            /* strip off newline */
            int newLinePos = static_cast<int>(strlen(line)) - 1; /* newline is on back from 0 */
            if (newLinePos < 0) {
                newLinePos = 0;
            }
            if (line[newLinePos] == '\n') {
                line[newLinePos] = 0;        /* make the newline 0 */
            }

            const char* nonWhite = SkipWhite(line);
            if (!nonWhite) {
                /* blank line-- skip */
                continue;
            }

            /* not a blank line, and nonwhite is first char */
            if (strncmp(bracketSection, nonWhite, strlen(bracketSection)) != 0){
                /* not on this line */
                continue;
            }

            /* it matches-- fp is now set up for search on tag */
            break;
        }
    }

    while (!feof(fp)) {
        /* check for end of file */
        if (NULL == fgets(line, LINELEN + 1, fp)) {
            /* got to end of file without finding it */
            ThrowException(ERR_TAG_NOT_FOUND);
            return std::nullopt;
        }

        if (HasInvalidLineEnding(line)) {
            ThrowException(ERR_CONVERSION);
            return std::nullopt;
        }

        /* got a line */
        lineNo++;

        /* strip off newline */
        int newLinePos = static_cast<int>(strlen(line)) - 1; /* newline is on back from 0 */
        if (newLinePos < 0) {
            newLinePos = 0;
        }
        if (line[newLinePos] == '\n') {
            line[newLinePos] = 0;        /* make the newline 0 */
        }
        // honor backslash (\) as line-end escape
        if (newLinePos > 0 && line[newLinePos-1] == '\\') {
           newLinePos = newLinePos-1;
           line[newLinePos] = 0;
           if (!extend_ct) {
               elineptr = eline; //first time
               strncpy(elineptr,line,newLinePos);
               elinenext = elineptr + newLinePos;
           } else {
               strncpy(elinenext,line,newLinePos);
               elinenext = elinenext + newLinePos;
           }
           *elinenext = 0;
           extend_ct++;
           if (extend_ct > MAX_EXTEND_LINES) {
              fprintf(stderr,
                 "INIFILE lineno=%u:Too many backslash line extends (limit=%d)\n",
                 lineNo, MAX_EXTEND_LINES);
              ThrowException(ERR_OVER_EXTENDED);
              return std::nullopt;
           }
           continue; // get next line to extend
        } else {
            if (extend_ct) {
               strncpy(elinenext,line,newLinePos);
               elinenext = elinenext + newLinePos;
               *elinenext = 0;
            }
        }
        if (!extend_ct) {
           elineptr = line;
        }
        extend_ct = 0;

        /* skip leading whitespace */
        char* nonWhite = SkipWhite(elineptr);
        if (!nonWhite) {
            /* blank line-- skip */
            continue;
        }

        /* check for '[' char-- if so, it's a section tag, and we're out
           of our section */
        if (NULL != section && nonWhite[0] == '[') {
            ThrowException(ERR_TAG_NOT_FOUND);
            return std::nullopt;
        }

        const std::size_t tagLength = strlen(tag);
        if (strncmp(tag, nonWhite, tagLength) != 0) {
            /* not on this line */
            continue;
        }

        /* it matches the first part of the string-- if whitespace or = is
           next char then call it a match */
        const char tagEnd = nonWhite[tagLength];
        if (tagEnd == ' ' || tagEnd == '\r' || tagEnd == '\t'
            || tagEnd == '\n' || tagEnd == '=') {
            /* it matches-- return string after =, or NULL */
            if (--_num > 0) {
                /* Not looking for this one, so skip it... */
                continue;
            }
            nonWhite += tagLength;
            char* valueString = AfterEqual(nonWhite);
            /* Eliminate white space at the end of a line also. */
            if (!valueString) {
                ThrowException(ERR_TAG_NOT_FOUND);
                return std::nullopt;
            }
            char* endValueString = valueString + strlen(valueString) - 1;
            while (*endValueString == ' ' || *endValueString == '\t'
                   || *endValueString == '\r') {
                *endValueString = 0;
                endValueString--;
            }
	    if (lineno)
		*lineno = lineNo;
            return valueString;
        }
        /* else continue */
    }

    ThrowException(ERR_TAG_NOT_FOUND);
    return std::nullopt;
}

IniFile::ErrorCode
IniFile::Find(std::string *s, const char *_tag, const char *_section, int _num)
{
    auto tmp = Find(_tag, _section, _num);
    if(!tmp)
        return ERR_TAG_NOT_FOUND; // can't distinguish errors, ugh

     *s = *tmp;

    return(ERR_NONE);
}

std::optional<const char*>
IniFile::FindString(char *dest, size_t n, const char *_tag, const char *_section, int _num, int *lineno)
{
    auto res = Find(_tag, _section, _num, lineno);
    if(!res)
        return std::nullopt;
    int r = snprintf(dest, n, "%s", *res);
    if(r < 0 || (size_t)r >= n) {
        ThrowException(ERR_CONVERSION);
        return std::nullopt;
    }
    return dest;
}

std::optional<const char*>
IniFile::FindPath(char *dest, size_t n, const char *_tag, const char *_section, int _num, int *lineno)
{
    auto res = Find(_tag, _section, _num, lineno);
    if(!res)
        return std::nullopt;
    if(TildeExpansion(*res, dest, n)) {
        return std::nullopt;
    }
    return dest;
}

bool
IniFile::CheckIfOpen()
{
    if(IsOpen())
        return(true);

    ThrowException(ERR_NOT_OPEN);

    return(false);
}


bool
IniFile::LockFile()
{
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if(fcntl(fileno(fp), F_SETLK, &lock) == -1){
        if(owned)
            fclose(fp);

        fp = NULL;
        return(false);
    }

    return(true);
}


/*! Expands the tilde to $(HOME) and concatenates file to it. If the first char
    If file does not start with ~/, file will be copied into path as-is. 

   @param the input filename

   @param pointer for returning the resulting expanded name

 */
IniFile::ErrorCode
IniFile::TildeExpansion(const char *file, char *path, size_t size)
{
    int res = snprintf(path, size, "%s", file);
    if(res < 0 || (size_t)res >= size)
        return ERR_CONVERSION;

    if (strlen(file) < 2 || !(file[0] == '~' && file[1] == '/')) {
	/* no tilde expansion required, or unsupported
           tilde expansion type requested */
	return ERR_NONE;
    }

    const char *home = getenv("HOME");
    if (!home) {
        ThrowException(ERR_CONVERSION);
	return ERR_CONVERSION;
    }

    res = snprintf(path, size, "%s%s", home, file + 1);
    if(res < 0 || (size_t)res >= size) {
        ThrowException(ERR_CONVERSION);
        return ERR_CONVERSION;
    }

    return ERR_NONE;
}

void
IniFile::ThrowException(ErrorCode errCode)
{
    if(errCode & errMask){
        exception.errCode = errCode;
        exception.tag = tag;
        exception.section = section;
        exception.num = num;
        exception.lineNo = lineNo;
        throw(exception);
    }
}

/*!
 * @brief Checks if a line has an invalid line ending.
 *
 * This function examines each character in the given line and reports warnings
 * or errors related to line endings. It detects DOS-style line endings (CRLF)
 * and ambiguous carriage returns.
 * @param line To be checked for invalid line endings.
 * @return True if an invalid line ending is found, false otherwise.
 */
bool IniFile::HasInvalidLineEnding(const char *line)
{
    if (!line)
        return false;

    for (; *line; line++) {
        if (*line == '\r') {
            char c = line[1];
            if (c == '\n' || c == '\0') {
                if (!lineEndingReported) {
                    fprintf(stderr, "IniFile: warning: File contains DOS-style line endings.\n");
                    lineEndingReported = true;
                }
                continue;
            }
            fprintf(stderr, "IniFile: error: File contains ambiguous carriage returns\n");
            return true;
        }
    }
    return false;
}

/*! Ignoring any tabs, spaces or other white spaces, finds the first
   character after the '=' delimiter.

   @param string Pointer to the tag

   @return NULL or pointer to first non-white char after the delimiter

   Called By: find() and section() only. */
char*
IniFile::AfterEqual(char *string)
{
    while (*string != '\0' && *string != '=')
        ++string;

    if (*string == '=') {
        do {
            ++string;
            if (*string == '\0')
                return nullptr;
        } while (*string == ' ' || *string == '\t' || *string == '\r' || *string == '\n');
        return string;
    }
    return nullptr;
}


/*! Finds the first non-white character on a new line and returns a
   pointer. Ignores any line that starts with a comment char i.e. a ';' or 
   '#'.

   @return NULL if not found or a valid pointer.

   Called By: find() and section() only. */
char*
IniFile::SkipWhite(char *string)
{
  while (*string != ';' && *string != '#' && *string != '\0') {
    if (*string != ' ' && *string != '\t' && *string != '\r' && *string != '\n') {
      return string;
    }
    string++;
  }
  return nullptr;
}


void
IniFile::Exception::Print(FILE *fp)
{
    const char                  *msg;

    switch(errCode){
    case ERR_NONE:
        msg = "ERR_NONE";
        break;

    case ERR_NOT_OPEN:
        msg = "ERR_NOT_OPEN";
        break;

    case ERR_SECTION_NOT_FOUND:
        msg = "ERR_SECTION_NOT_FOUND";
        break;

    case ERR_TAG_NOT_FOUND:
        msg = "ERR_TAG_NOT_FOUND";
        break;

    case ERR_CONVERSION:
        msg = "ERR_CONVERSION";
        break;

    case ERR_LIMITS:
        msg = "ERR_LIMITS";
        break;

    case ERR_OVER_EXTENDED:
        msg = "ERR_OVER_EXTENDED";
        break;

    default:
        msg = "UNKNOWN";
    }

    fprintf(fp, "INIFILE: %s, section=%s, tag=%s, num=%d, lineNo=%u\n",
            msg, section, tag, num, lineNo);
}


extern "C" const char *
iniFind(FILE *fp, const char *tag, const char *section)
{
    IniFile                     f(false, fp);

    return(f.Find(tag, section).value_or(nullptr));
}

extern "C" const int
iniFindInt(FILE *fp, const char *tag, const char *section, int *result)
{
    IniFile f(false, fp);
    return(f.Find(result, tag, section));
}

extern "C" const int
iniFindDouble(FILE *fp, const char *tag, const char *section, double *result)
{
    IniFile f(false, fp);
    return(f.Find(result, tag, section));
}

extern "C" int
TildeExpansion(const char *file, char *path, size_t size)
{
  static IniFile f;
  return !f.TildeExpansion(file, path, size);
}
