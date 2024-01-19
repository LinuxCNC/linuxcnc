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
#include <string>
#include <boost/lexical_cast.hpp>

#ifndef __cplusplus
#warning Inclusion of <inifile.hh> from C programs is deprecated.  Include <inifile.h> instead.
#endif

#ifdef __cplusplus
#include <fcntl.h>
#include <optional>
class IniFile {
public:
    enum ErrorCode {
        ERR_NONE                = 0x00,
        ERR_NOT_OPEN            = 0x01,
        ERR_SECTION_NOT_FOUND   = 0x02,
        ERR_TAG_NOT_FOUND       = 0x04,
        ERR_CONVERSION          = 0x08,
        ERR_LIMITS              = 0x10,
        ERR_OVER_EXTENDED       = 0x20,
    };

    class Exception {
    public:
        ErrorCode               errCode;
        const char *            tag;
        const char *            section;
        int                     num;
        unsigned int            lineNo;

        void                    Print(FILE *fp=stderr);
    };


                                explicit IniFile(int errMask = 0, FILE *fp = nullptr);
                                ~IniFile(){ Close(); }

    bool                        Open(const char *file);
    bool                        Close();
    bool                        IsOpen(){ return(fp != nullptr); }

    std::optional<const char*>  Find(const char *tag, const char *section = nullptr,
                                     int num = 1, int *lineno = nullptr);

    template<typename T>
    ErrorCode                   Find(T *result, T min, T max,
                                     const char *tag,const char *section,
                                     int num=1);

    template<typename T>
    ErrorCode                   Find(T *result,
                                     const char *tag,const char *section,
                                     int num=1);

    ErrorCode                   Find(std::string *s,
                                     const char *tag,const char *section,
                                     int num=1);

    std::optional<const char*>  FindString(char *dest, size_t n,
				     const char *tag, const char *section = nullptr,
				     int num = 1, int *lineno = nullptr);
    std::optional<const char*>  FindPath(char *dest, size_t n,
				     const char *tag, const char *section = nullptr,
				     int num = 1, int *lineno = nullptr);
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
                                     const char *tag, const char *section = nullptr,
                                     int num = 1, int *lineno = nullptr);
    ErrorCode                   Find(int *result, StrIntPair *,
                                     const char *tag, const char *section = nullptr,
                                     int num = 1, int *lineno = nullptr);


private:
    FILE                        *fp;
    struct flock                lock{};
    bool                        owned{false};

    Exception                   exception{};
    int                         errMask;

    unsigned int                lineNo{};
    const char *                tag{};
    const char *                section{};
    int                         num{};
    bool                        lineEndingReported{false};

    bool                        CheckIfOpen();
    bool                        LockFile();
    bool                        HasInvalidLineEnding(const char *line);
    void                        ThrowException(ErrorCode);
    char                        *AfterEqual(char *string);
    char                        *SkipWhite(char *string);
};

template<typename T>
IniFile::ErrorCode IniFile::Find(T *result, T min, T max,
                                 const char *_tag,const char *_section,
                                 int _num)
{
    ErrorCode errCode;
    T tmp;
    if((errCode = Find(&tmp, _tag, _section, _num)) != ERR_NONE)
        return(errCode);

    if((tmp > max) || (tmp < min)) {
        ThrowException(ERR_LIMITS);
        return(ERR_LIMITS);
    }

    *result = tmp;

    return(ERR_NONE);
}

template<typename T>
IniFile::ErrorCode IniFile::Find(T *result, const char *_tag,
                                 const char *_section, int _num)
{
    ErrorCode errCode;
    std::string tmp;
    if((errCode = Find(&tmp, _tag, _section, _num)) != ERR_NONE)
        return(errCode);

    try {
        *result = boost::lexical_cast<T>(tmp);
    } catch (boost::bad_lexical_cast &) {
        ThrowException(ERR_CONVERSION);
        return(ERR_CONVERSION);
    }

    return(ERR_NONE);
}
#endif


#endif
