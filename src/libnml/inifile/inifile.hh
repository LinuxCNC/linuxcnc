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
#include "global_defs.h"

typedef struct {
    char tag[LINELEN];
    char rest[LINELEN];
} inifile_entry;

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

  private:
    FILE * fp;
    char *after_equal(const char *string);
    char *skip_white(char *string);
    struct flock lock;
};

#endif /* INIFILE_HH */
