/*
  emcsvr.cc

  Network server for EMC NML

  Modification history:

  25-Jan-2001 WPS added string.h and stdlib.h
  21-Jul-2000  FMP added #include <math.h> for fabs()
  8-Jun-2000 WPS added code so that it would try the connections for
  10 seconds before printing any errors.
  1-Oct-1999  FMP added emcGetArgs, DEBUG to suppress version printing
  12-Apr-1999 WPS changed emcError to use emcFormat instead of nmlErrorFormat
  and to be a RCS_CMD_CHANNEL
  18-Mar-1998  FMP added tool command, status channels since emcpanel
  uses them directly
  4-Mar-1998  FMP added -nml command line arg
  15-Jan-1998  FMP added version printing niceties
  15-Jan-1998  WPS updated with new emc-style buffer names, and forgot
  to edit the modification history
  17-Oct-1997  FMP added ident tag
  */

#include <stdio.h>              // sscanf()
#include <math.h>               // fabs()
#include <stdlib.h>		// exit()
#include <string.h>		// strncpy()


#include "rcs.hh"               // RCS_CMD_CHANNEL, RCS_STAT_CHANNEL, etc.
#include "emc.hh"               // EMC NML
#include "emcglb.h"             // emcGetArgs(), EMC_NMLFILE

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

static int iniLoad(const char *filename)
{
  INIFILE inifile;
  const char *inistring;

  // open it
  if (-1 == inifile.open(filename)) {
    return -1;
  }

  if (NULL != (inistring = inifile.find("DEBUG", "EMC"))) {
    // copy to global
    if (1 != sscanf(inistring, "%i", &EMC_DEBUG)) {
      EMC_DEBUG = 0;
    }
  }
  else {
    // not found, use default
    EMC_DEBUG = 0;
  }
  if(EMC_DEBUG & EMC_DEBUG_RCS)
    {
      //set_rcs_print_flag(PRINT_EVERYTHING);
      max_rcs_errors_to_print=-1;
    }


  if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
    // copy to global
    strcpy(EMC_NMLFILE, inistring);
  }
  else {
    // not found, use default
  }

  // close it
  inifile.close();

  return 0;
}

static RCS_CMD_CHANNEL *emcCommandChannel=NULL;
static RCS_STAT_CHANNEL *emcStatusChannel=NULL;
static NML *emcErrorChannel=NULL;
static RCS_CMD_CHANNEL *toolCommandChannel=NULL;
static RCS_STAT_CHANNEL *toolStatusChannel=NULL;
static RCS_CMD_CHANNEL *auxCommandChannel=NULL;
static RCS_STAT_CHANNEL *auxStatusChannel=NULL;
static RCS_CMD_CHANNEL *lubeCommandChannel=NULL;
static RCS_STAT_CHANNEL *lubeStatusChannel=NULL;
static RCS_CMD_CHANNEL *spindleCommandChannel=NULL;
static RCS_STAT_CHANNEL *spindleStatusChannel=NULL;
static RCS_CMD_CHANNEL *coolantCommandChannel=NULL;
static RCS_STAT_CHANNEL *coolantStatusChannel=NULL;

int main(int argc, char *argv[])
{
  double start_time;

  // process command line args
  if (0 != emcGetArgs(argc, argv)) {
    rcs_print_error("Error in argument list\n");
    exit(1);
  }

  // get configuration information
  iniLoad(EMC_INIFILE);

  set_rcs_print_destination(RCS_PRINT_TO_NULL);

  start_time = etime();

  while(fabs(etime() - start_time) < 10.0 &&
        ( emcCommandChannel == NULL || emcStatusChannel == NULL || toolCommandChannel == NULL ||
          toolStatusChannel == NULL || emcErrorChannel == NULL)
        )
    {
      if(NULL == emcCommandChannel)
        {
          emcCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == emcStatusChannel)
        {
          emcStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == emcErrorChannel)
        {
          emcErrorChannel = new NML(nmlErrorFormat, "emcError", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == toolCommandChannel)
        {
          toolCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == toolStatusChannel)
        {
          toolStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == spindleCommandChannel)
        {
          spindleCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "spindleCmd", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == spindleStatusChannel)
        {
          spindleStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "spindleSts", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == auxCommandChannel)
        {
          auxCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "auxCmd", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == auxStatusChannel)
        {
          auxStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "auxSts", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == coolantCommandChannel)
        {
          coolantCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "coolantCmd", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == coolantStatusChannel)
        {
          coolantStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "coolantSts", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == lubeCommandChannel)
        {
          lubeCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "lubeCmd", "emcsvr", EMC_NMLFILE);
        }
      if(NULL == lubeStatusChannel)
        {
          lubeStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "lubeSts", "emcsvr", EMC_NMLFILE);
        }

      if(!emcCommandChannel->valid())
        {
          delete emcCommandChannel;
          emcCommandChannel = NULL;
        }
      if(!emcStatusChannel->valid())
        {
          delete emcStatusChannel;
          emcStatusChannel = NULL;
        }
      if(!emcErrorChannel->valid())
        {
          delete emcErrorChannel;
          emcErrorChannel = NULL;
        }
      if(!toolCommandChannel->valid())
        {
          delete toolCommandChannel;
          toolCommandChannel = NULL;
        }
      if(!toolStatusChannel->valid())
        {
          delete toolStatusChannel;
          toolStatusChannel = NULL;
        }
      if(!auxCommandChannel->valid())
        {
          delete auxCommandChannel;
          auxCommandChannel = NULL;
        }
      if(!auxStatusChannel->valid())
        {
          delete auxStatusChannel;
          auxStatusChannel = NULL;
        }
      if(!coolantCommandChannel->valid())
        {
          delete coolantCommandChannel;
          coolantCommandChannel = NULL;
        }
      if(!coolantStatusChannel->valid())
        {
          delete coolantStatusChannel;
          coolantStatusChannel = NULL;
        }
      if(!lubeCommandChannel->valid())
        {
          delete lubeCommandChannel;
          lubeCommandChannel = NULL;
        }
      if(!lubeStatusChannel->valid())
        {
          delete lubeStatusChannel;
          lubeStatusChannel = NULL;
        }
      if(!spindleCommandChannel->valid())
        {
          delete spindleCommandChannel;
          spindleCommandChannel = NULL;
        }
      if(!spindleStatusChannel->valid())
        {
          delete spindleStatusChannel;
          spindleStatusChannel = NULL;
        }
      esleep(0.200);
    }

  set_rcs_print_destination(RCS_PRINT_TO_STDERR);

  if(NULL == emcCommandChannel)
    {
      emcCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == emcStatusChannel)
    {
      emcStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == emcErrorChannel)
    {
      emcErrorChannel = new NML(nmlErrorFormat, "emcError", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == toolCommandChannel)
    {
      toolCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == toolStatusChannel)
    {
      toolStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "toolSts", "emcsvr", EMC_NMLFILE);
    }

  if(NULL == spindleCommandChannel)
    {
      spindleCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "spindleCmd", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == spindleStatusChannel)
    {
      spindleStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "spindleSts", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == auxCommandChannel)
    {
      auxCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "auxCmd", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == auxStatusChannel)
    {
      auxStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "auxSts", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == coolantCommandChannel)
    {
      coolantCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "coolantCmd", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == coolantStatusChannel)
    {
      coolantStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "coolantSts", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == lubeCommandChannel)
    {
      lubeCommandChannel = new RCS_CMD_CHANNEL(emcFormat, "lubeCmd", "emcsvr", EMC_NMLFILE);
    }
  if(NULL == lubeStatusChannel)
    {
      lubeStatusChannel = new RCS_STAT_CHANNEL(emcFormat, "lubeSts", "emcsvr", EMC_NMLFILE);
    }


  run_nml_servers();

  return 0;
}


