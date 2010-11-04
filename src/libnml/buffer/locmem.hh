/********************************************************************
* Description: locmem.hh
*   Implements LOCMEM which is a derived class of CMS that serves
*   primarily to provide addresses that match when matching buffer
*   names are passed to the constructor. It is useful in allowing
*   control modules to use the same inteface to communicate as would
*   be required if they were not running in the same process even
*   though to use LOCMEM they must be.
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
********************************************************************/

#ifndef LOCMEM_HH
#define LOCMEM_HH

#include "cms.hh"		// class CMS
#include "linklist.hh"		// class LinkedList

struct BUFFERS_LIST_NODE {
    void *addr;
    long size;
    char name[64];
};

class LOCMEM:public CMS {
  public:
    LOCMEM(const char *bufline, const char *procline, int set_to_server =
	0, int set_to_master = 0);
      virtual ~ LOCMEM();
    CMS_STATUS main_access(void *_local);

  protected:
    void *lm_addr;
    int buffer_id;
    BUFFERS_LIST_NODE *my_node;
    static LinkedList *buffers_list;
};

#endif
