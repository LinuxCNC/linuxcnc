/*
  iniaux.cc

  INI file initialization for aux controller

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */


extern "C" {
#include <stdio.h>              // NULL, sscanf
}

#include "emc.hh"
#include "inifile.h"
#include "iniaux.hh"            // these decls
#include "emcglb.h"             // ESTOP_SENSE_INDEX, etc

// inifile ref'ed by iniAux(), loadAux()
static INIFILE *auxInifile = 0;

/*
  loadAux()

  Loads ini file params for aux from [EMCIO] section

  ESTOP_SENSE_INDEX <int>      dio point for mist aux on/off
  ESTOP_WRITE_INDEX <int>     dio point for flood aux on/off

  ESTOP_SENSE_POLARITY <0,1>   polarity for mist on
  ESTOP_WRITE_POLARITY <0,1>  polarity for flood on

  calls:

  emcAuxEstopSetSenseIndex(int index);
  emcAuxEstopSetWriteIndex(int index);

  emcAuxEstopSetSensePolarity(int polarity);
  emcAuxEstopSetWritePolarity(int polarity);
  */

static int loadAux()
{
  int retval = 0;
  const char *inistring;
  int i;

  if (NULL != (inistring = auxInifile->find("ESTOP_SENSE_INDEX", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcAuxEstopSetSenseIndex(i))
            {
              printf("bad return value from emcAuxEstopSetSenseIndex\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for ESTOP_SENSE_INDEX: %s\n",
                          inistring);
        }
    }
  // else ignore omission

  if (NULL != (inistring = auxInifile->find("ESTOP_WRITE_INDEX", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcAuxEstopSetWriteIndex(i))
            {
              printf("bad return value from emcAuxEstopSetWriteIndex\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for ESTOP_WRITE_INDEX: %s\n",
                          inistring);
        }
    }
  // else ignore omission

  if (NULL != (inistring = auxInifile->find("ESTOP_SENSE_POLARITY", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcAuxEstopSetSensePolarity(i))
            {
              printf("bad return value from emcAuxEstopSetSensePolarity\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for ESTOP_SENSE_POLARITY: %s\n",
                          inistring);
        }
    }
  // else ignore omission

  if (NULL != (inistring = auxInifile->find("ESTOP_WRITE_POLARITY", "EMCIO")))
    {
      if (1 == sscanf(inistring, "%d", &i))
        {
          // found, and valid
          if (0 != emcAuxEstopSetWritePolarity(i))
            {
              printf("bad return value from emcAuxEstopSetWritePolarity\n");
              retval = -1;
            }
        }
      else
        {
          // found, but invalid, so warn
          printf("invalid inifile value for ESTOP_WRITE_POLARITY: %s\n",
                          inistring);
        }
    }
  // else ignore omission

  return retval;
}

/*
  iniAux(const char *filename)

  Loads ini file parameters for aux controller, from [EMCIO] section
 */
int iniAux(const char *filename)
{
  int retval = 0;

  auxInifile = new INIFILE;

  if (-1 == auxInifile->open(filename))
    {
      return -1;
    }

  // load aux values
  if (0 != loadAux())
    {
      retval = -1;
    }

  // close the inifile
  auxInifile->close();
  delete auxInifile;

  return retval;
}

// implementations of functions to set ini file global variables

int emcAuxEstopSetSenseIndex(int index)
{
  ESTOP_SENSE_INDEX = index;

  return 0;
}

int emcAuxEstopSetWriteIndex(int index)
{
  ESTOP_WRITE_INDEX = index;

  return 0;
}

int emcAuxEstopSetSensePolarity(int polarity)
{
  ESTOP_SENSE_POLARITY = polarity;

  return 0;
}

int emcAuxEstopSetWritePolarity(int polarity)
{
  ESTOP_WRITE_POLARITY = polarity;

  return 0;
}
