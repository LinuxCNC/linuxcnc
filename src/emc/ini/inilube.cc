/********************************************************************
* Description: inilube.cc
*    INI file initialization for lube controller
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

extern "C" {
#include <stdio.h>              // NULL
#include <stdlib.h>             // atol()
}

#include "emc.hh"
#include "inifile.hh"
#include "inilube.hh"           // these decls
#include "emcglb.h"             // LUBE_SENSE_INDEX, etc

// inifile ref'ed by iniLube(), loadLube()
static Inifile *lubeInifile = 0;

/*
  loadLube()

  Loads ini file params for lube from [EMCIO] section

  LUBE_SENSE_INDEX <int>     dio input point for lube level sense
  LUBE_SENSE_POLARITY <0,1>  polarity for lube level sense input
  LUBE_WRITE_INDEX <int>     dio output point for lube pump
  LUBE_WRITE_POLARITY <0,1>  polarity for lube pump output

  calls:

  emcLubeSetSenseIndex(int index);
  emcLubeSetSensePolarity(int polarity);
  emcLubeSetWriteIndex(int index);
  emcLubeSetWritePolarity(int polarity);
  */

static int loadLube()
{
  int retval = 0;
  const char *inistring;
  int i;

  if (NULL != (inistring = lubeInifile->find("LUBE_SENSE_INDEX", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcLubeSetSenseIndex(i))
            {
              printf("bad return value from emcLubeSetSenseIndex\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for LUBE_SENSE_INDEX: %s\n",
                          inistring);
        }
    }

  if (NULL != (inistring = lubeInifile->find("LUBE_SENSE_POLARITY", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcLubeSetSensePolarity(i))
            {
              printf("bad return value from emcLubeSetSensePolarity\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for LUBE_SENSE_POLARITY: %s\n",
                          inistring);
        }
    }

  if (NULL != (inistring = lubeInifile->find("LUBE_WRITE_INDEX", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcLubeSetWriteIndex(i))
            {
              printf("bad return value from emcLubeSetWriteIndex\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for LUBE_WRITE_INDEX: %s\n",
                          inistring);
        }
    }
  // else ignore omission

  if (NULL != (inistring = lubeInifile->find("LUBE_WRITE_POLARITY", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcLubeSetWritePolarity(i))
            {
              printf("bad return value from emcLubeSetWritePolarity\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for LUBE_WRITE_POLARITY: %s\n",
                          inistring);
        }
    }

  // else ignore omission

  return retval;
}

/*
  iniLube(const char *filename)

  Loads ini file parameters for lube controller, from [EMCIO] section
 */
int iniLube(const char *filename)
{
  int retval = 0;

  lubeInifile = new Inifile;

  if (lubeInifile->open(filename) == false)
    {
      return -1;
    }

  // load lube values
  if (0 != loadLube())
    {
      retval = -1;
    }

  // close the inifile
  lubeInifile->close();
  delete lubeInifile;

  return retval;
}

// functions to set global variables

int emcLubeSetSenseIndex(int index)
{
  LUBE_SENSE_INDEX = index;

  return 0;
}

int emcLubeSetSensePolarity(int polarity)
{
  LUBE_SENSE_POLARITY = polarity;

  return 0;
}

int emcLubeSetWriteIndex(int index)
{
  LUBE_WRITE_INDEX = index;

  return 0;
}

int emcLubeSetWritePolarity(int polarity)
{
  LUBE_WRITE_POLARITY = polarity;

  return 0;
}
