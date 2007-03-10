/********************************************************************
* Description: inifile.hh
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

#ifndef INIFILE_HH
#define INIFILE_HH

#include <stdio.h>
#include <fcntl.h>


#ifdef __cplusplus
class IniFile {
public:
    typedef enum {
        ERR_NONE,
        ERR_NOT_OPEN,
        ERR_SECTION_NOT_FOUND,
        ERR_TAG_NOT_FOUND,
        ERR_CONVERSION,
        ERR_LIMITS,
    } ErrorCode;

    class Exception {
    public:
        ErrorCode               errCode;
        const char *            tag;
        const char *            section;
        int                     num;
        unsigned int            lineNo;

        void                    Print(FILE *fp);
    };


                                IniFile(bool throwExcp=false, FILE *fp=NULL);
                                ~IniFile(void){ Close(); }

    bool                        Open(const char *file);
    bool                        Close(void);
    bool                        IsOpen(void){ return(fp != NULL); }
    ErrorCode                   Find(int *result, const char *tag,
                                     const char *section=NULL, int num = 1);
    ErrorCode                   Find(double *result, const char *tag,
                                     const char *section=NULL, int num = 1);
    ErrorCode                   Find(bool *result, const char *tag,
                                     const char *section=NULL, int num = 1);
    const char *                Find(const char *tag, const char *section=NULL,
                                     int num = 1);
    void                        EnableExceptions(bool enable){
                                    throwException = enable;
                                }


private:
    FILE                        *fp;
    struct flock                lock;
    bool                        owned;

    Exception                   exception;
    bool                        throwException;

    unsigned int                lineNo;
    const char *                tag;
    const char *                section;
    int                         num;

    bool                        CheckIfOpen(void);
    bool                        LockFile(void);
    void                        TildeExpansion(const char *file, char *path);
    void                        ThrowException(ErrorCode);
    char                        *AfterEqual(const char *string);
    char                        *SkipWhite(const char *string);
};
#else
extern const char *iniFind(FILE *fp, const char *tag, const char *section);
#endif


#endif
