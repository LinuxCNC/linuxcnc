/*
  bridgeportlube.cc

  Controller for Bridgeport lube controller

  Modification history:

  19-May-2000  FMP added lubeOn,Off(), isLubeOn(); control code for these
  31-Aug-1999  FMP changed to bridgeportlube.cc
  10-Feb-1999  FMP just a test
  28-Apr-1998  FMP took out ::iniLoad(), put in iniLube()
  1-Apr-1998  FMP changed from emclube.cc to shvlube.cc
  24-Mar-1998  FMP changed ERROR() to REPORT_ERROR() due to conflict
  in VC++
  15-Dec-1997  FMP created from emcaux.cc
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "emcio.hh"             // these decls
#include "emcglb.h"
#include "extintf.h"            // extDioSet(),Clear()
#include "inilube.hh"           // iniLube()

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// local functions

static void lubeOn()
{
  extDioWrite(LUBE_WRITE_INDEX, LUBE_WRITE_POLARITY);
}

static void lubeOff()
{
  extDioWrite(LUBE_WRITE_INDEX, ! LUBE_WRITE_POLARITY);
}

static int isLubeOn()
{
  int f;

  extDioCheck(LUBE_WRITE_INDEX, &f);

  return f == LUBE_WRITE_POLARITY;
}

// returns 0 if lube level is low, 1 if OK
static int lubeLevel()
{
  int lube;

  extDioRead(LUBE_SENSE_INDEX, &lube);

  return lube == LUBE_SENSE_POLARITY;
}

// constructor

EMC_LUBE_MODULE::EMC_LUBE_MODULE()
{
  setErrorLogChannel(new NML(nmlErrorFormat, "emcError", "lube", EMC_NMLFILE));

  setCmdChannel(new RCS_CMD_CHANNEL(emcFormat, "lubeCmd", "lube", EMC_NMLFILE));

  setStatChannel(new RCS_STAT_CHANNEL(emcFormat, "lubeSts", "lube", EMC_NMLFILE), &lubeStatus);
}

// destructor

EMC_LUBE_MODULE::~EMC_LUBE_MODULE(void)
{
}

void EMC_LUBE_MODULE::DECISION_PROCESS(void)
{
  switch (commandInData->type)
    {
    case EMC_LUBE_INIT_TYPE:
      INIT((EMC_LUBE_INIT *) commandInData);
      break;

    case EMC_LUBE_HALT_TYPE:
      HALT((EMC_LUBE_HALT *) commandInData);
      break;

    case EMC_LUBE_ABORT_TYPE:
      ABORT((EMC_LUBE_ABORT *) commandInData);
      break;

    case EMC_LUBE_ON_TYPE:
      LUBE_ON((EMC_LUBE_ON *) commandInData);
      break;

    case EMC_LUBE_OFF_TYPE:
      LUBE_OFF((EMC_LUBE_OFF *) commandInData);
      break;

    default:
      REPORT_ERROR(commandInData);
      break;
    }
}

void EMC_LUBE_MODULE::PRE_PROCESS(void)
{
}

void EMC_LUBE_MODULE::POST_PROCESS(void)
{
  lubeStatus.on = isLubeOn();
  lubeStatus.level = lubeLevel();
}

void EMC_LUBE_MODULE::INIT(EMC_LUBE_INIT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniLube(EMC_INIFILE);

      // turn lube off
      lubeOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_LUBE_MODULE::HALT(EMC_LUBE_HALT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn lube off
      lubeOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_LUBE_MODULE::ABORT(EMC_LUBE_ABORT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn lube off
      lubeOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_LUBE_MODULE::REPORT_ERROR(RCS_CMD_MSG *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      rcs_print_error("EMC_LUBE_MODULE: unknown command %d\n",
                      cmdIn->type);
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_LUBE_MODULE::LUBE_ON(EMC_LUBE_ON *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn lube on
      lubeOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_LUBE_MODULE::LUBE_OFF(EMC_LUBE_OFF *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn lube off
      lubeOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}
