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
********************************************************************/

#ifndef INIFILE_HH
#define INIFILE_HH

#include <inifile.h>

#ifndef __cplusplus
#warning Inclusion of <inifile.hh> from C programs is deprecated.  Include <inifile.h> instead.
#endif

#ifdef __cplusplus
class IniFile {
public:
    typedef enum {
        ERR_NONE                = 0x00,
        ERR_NOT_OPEN            = 0x01,
        ERR_SECTION_NOT_FOUND   = 0x02,
        ERR_TAG_NOT_FOUND       = 0x04,
        ERR_CONVERSION          = 0x08,
        ERR_LIMITS              = 0x10,
    } ErrorCode;

    class Exception {
    public:
        ErrorCode               errCode;
        const char *            tag;
        const char *            section;
        int                     num;
        unsigned int            lineNo;

        void                    Print(FILE *fp=stderr);
    };


                                IniFile(int errMask=0, FILE *fp=NULL);
                                ~IniFile(void){ Close(); }

    bool                        Open(const char *file);
    bool                        Close(void);
    bool                        IsOpen(void){ return(fp != NULL); }
    ErrorCode                   Find(int *result, int min, int max,
                                     const char *tag,const char *section,
                                     int num=1);
    ErrorCode                   Find(int *result, const char *tag,
                                     const char *section=NULL, int num = 1);
    ErrorCode                   Find(double *result, double min, double max,
                                     const char *tag,const char *section,
                                     int num=1);
    ErrorCode                   Find(double *result, const char *tag,
                                     const char *section=NULL, int num = 1,
				     int *lineno = NULL);
    const char *                Find(const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL);
    const char *                FindString(char *dest, size_t n,
				     const char *tag, const char *section=NULL,
				     int num = 1, int *lineno = NULL);
    const char *                FindPath(char *dest, size_t n,
				     const char *tag, const char *section=NULL,
				     int num = 1, int *lineno = NULL);
    void                        EnableExceptions(int _errMask){
                                    errMask = _errMask;
                                }

    ErrorCode                   TildeExpansion(const char *file, char *path,
					       size_t n);

protected:
    struct StrIntPair {
        const char             *pStr;
        int                     value;
    };

    struct StrDoublePair {
        const char              *pStr;
        double                   value;
    };


    ErrorCode                   Find(double *result, StrDoublePair *,
                                     const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL);
    ErrorCode                   Find(int *result, StrIntPair *,
                                     const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL);


private:
    FILE                        *fp;
    struct flock                lock;
    bool                        owned;

    Exception                   exception;
    int                         errMask;

    unsigned int                lineNo;
    const char *                tag;
    const char *                section;
    int                         num;

    bool                        CheckIfOpen(void);
    bool                        LockFile(void);
    void                        ThrowException(ErrorCode);
    char                        *AfterEqual(const char *string);
    char                        *SkipWhite(const char *string);
};
#endif


#endif
