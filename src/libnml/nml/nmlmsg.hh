/********************************************************************
* Description: nmlmsg.hh
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

#ifndef NMLMSG_HH
#define NMLMSG_HH

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>		/* size_t */

#ifdef __cplusplus
};
#endif

/* Definitions from other Header files. */

class CMS;			/* Use only partial definition to avoid */
				/* depending on cms.hh */

#ifndef NMLTYPE_TYPEDEFED
#define NMLTYPE_TYPEDEFED
typedef long NMLTYPE;		/* Also defined in nml.hh */
#endif

/* Class NMLmsg */
/* Base class for all types that can be written to NML. */
/* The constructor is protected so that to users cannot send messages */
/*  without deriving their own classes from NMLmsg.  */
/* Derived classes should pass the type and size to the NMLmsg constructor. */
/*  and define their own update function. */
class NMLmsg {
  protected:
    NMLmsg(NMLTYPE t, long s);
    NMLmsg(NMLTYPE t, size_t s);

  public:
    void clear();

    NMLTYPE type;		/* Each derived type should have a unique id */
    long size;			/* The size is used so that the entire buffer 
				   is not copied unneccesarily. */

    void update(CMS *);
};

// This is just a symbol passed to the RCS Java Tools
#define NML_DYNAMIC_LENGTH_ARRAY

#define DECLARE_NML_DYNAMIC_LENGTH_ARRAY(type, name, size) int name##_length; type name[size];
/* The above macro, when supplied with the following : 

  DECLARE_NML_DYNAMIC_LENGTH_ARRAY(foo_type, bar, 666)

gives:

   int bar_length; foo_type bar[20];
*/

#endif /* !defined(NMLMSG_HH) */
