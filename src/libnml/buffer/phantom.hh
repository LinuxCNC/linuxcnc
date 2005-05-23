/********************************************************************
* Description: phantom.hh
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/

#ifndef PHANTOM_HH
#define PHANTOM_HH

#include "cms.hh"

class PHANTOMMEM:public CMS {
  public:
    PHANTOMMEM(char *bufline, char *procline);
      virtual ~ PHANTOMMEM();
    virtual CMS_STATUS main_access(void *_local);
};

#endif
