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

class INIFILE {
  public:
    INIFILE();
    INIFILE(const char *path);
     ~INIFILE();

    const int open(const char *path);
    const int close();
    const char *find(const char *tag, const char *section = NULL);
    int section(const char *section, INIFILE_ENTRY array[], int max);
    const int valid();

  private:
      FILE * fp;
};

#endif /* INIFILE_HH */
