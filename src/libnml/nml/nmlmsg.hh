/*************************************************************************
* File: nmlmsg.hh                                                        *
* Authors: Fred Proctor, Will Shackleford                                *
* Purpose: C++ Header file for the Neutral Manufacturing Language (NML). *
*          Includes:                                                     *
*                    1. NMLmsg Class.                                    *
*                    2. NMLTYPE typedef.                                 *
*************************************************************************/

#ifndef NMLMSG_HH
#define NMLMSG_HH

/* Include Files */
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

    /* This second constructor never clears the message regardless of what is
       in nmlmsg. The value of noclear is irrelevent but adding it changes
       which constructor is called. */
      NMLmsg(NMLTYPE t, long s, int noclear);

  public:
    void clear();

    static int automatically_clear;	/* controls whether NMLmsgs are set
					   to zero in the constructor. */
    NMLTYPE type;		/* Each derived type should have a unique id */
    long size;			/* The size is used so that the entire buffer 
				   is not copied unneccesarily. */

    void update(CMS *);
};

// This is just a symbol passed to the RCS Java Tools (CodeGen, RCS-Design, RCS-Diagnostis)
#define NML_DYNAMIC_LENGTH_ARRAY

#define DECLARE_NML_DYNAMIC_LENGTH_ARRAY(type, name, size) int name##_length; type name[size];

#endif /* !defined(NMLMSG_HH) */
