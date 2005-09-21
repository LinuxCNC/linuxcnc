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

#include "config.h"
#include <stdio.h>
#include <fcntl.h>

typedef struct {
    char tag[LINELEN];
    char rest[LINELEN];
} inifile_entry;

#ifdef __cplusplus
class Inifile {
  public:
    Inifile();
    Inifile(const char *file);
    ~Inifile();

    bool open(const char *file);
    bool close();
    bool valid();
    int write();
    const char *find(const char *tag, const char *section = NULL, int num = 1);
    int section(const char *section, inifile_entry array[], int max);
    void tilde(const char *file, char *path);

  private:
    FILE * fp;
    char *after_equal(const char *string);
    char *skip_white(char *string);
    struct flock lock;
};

extern "C" {
#else
extern const char *iniFind(void *fp, const char *tag, const char *section);
#endif
#ifdef __cplusplus
}
#endif
#endif /* INIFILE_HH */
