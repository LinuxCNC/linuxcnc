/*
  bridgeportcool.cc

  Controller for Bridgeport coolant I/O

  Modification history:

  31-Aug-1999  FMP changed to bridgeportcool.cc
  1-Apr-1998  FMP changed from emccool.cc to shvcool.cc
  24-Mar-1998  FMP changed ERROR() to REPORT_ERROR() due to conflict
  in VC++
  11-Dec-1997  FMP created from nce stuff
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "emcio.hh"             // these decls
#include "emcglb.h"
#include "extintf.h"            // extDioSet(),Clear()
#include "inicool.hh"           // iniCoolant()

// ident tag
/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused))  ident[] = "$Id$";

// local functions

static void mistOn()
{
  extDioWrite(MIST_COOLANT_INDEX, MIST_COOLANT_POLARITY);
}

static void mistOff()
{
  extDioWrite(MIST_COOLANT_INDEX, ! MIST_COOLANT_POLARITY);
}

static void floodOn()
{
  extDioWrite(FLOOD_COOLANT_INDEX, FLOOD_COOLANT_POLARITY);
}

static void floodOff()
{
  extDioWrite(FLOOD_COOLANT_INDEX, ! FLOOD_COOLANT_POLARITY);
}

static int isMistOn()
{
  int m;

  extDioCheck(MIST_COOLANT_INDEX, &m);

  return m == MIST_COOLANT_POLARITY;
}

static int isFloodOn()
{
  int f;

  extDioCheck(FLOOD_COOLANT_INDEX, &f);

  return f == FLOOD_COOLANT_POLARITY;
}

// constructor

EMC_COOLANT_MODULE::EMC_COOLANT_MODULE()
{
  setErrorLogChannel(new NML(nmlErrorFormat, "emcError", "coolant", EMC_NMLFILE));

  setCmdChannel(new RCS_CMD_CHANNEL(emcFormat, "coolantCmd", "coolant", EMC_NMLFILE));

  setStatChannel(new RCS_STAT_CHANNEL(emcFormat, "coolantSts", "coolant", EMC_NMLFILE), &coolantStatus);
}

// destructor

EMC_COOLANT_MODULE::~EMC_COOLANT_MODULE(void)
{
}

void EMC_COOLANT_MODULE::DECISION_PROCESS(void)
{
  switch (commandInData->type)
    {
    case EMC_COOLANT_INIT_TYPE:
      INIT((EMC_COOLANT_INIT *) commandInData);
      break;

    case EMC_COOLANT_HALT_TYPE:
      HALT((EMC_COOLANT_HALT *) commandInData);
      break;

    case EMC_COOLANT_ABORT_TYPE:
      ABORT((EMC_COOLANT_ABORT *) commandInData);
      break;

    case EMC_COOLANT_MIST_ON_TYPE:
      MIST_ON((EMC_COOLANT_MIST_ON *) commandInData);
      break;

    case EMC_COOLANT_MIST_OFF_TYPE:
      MIST_OFF((EMC_COOLANT_MIST_OFF *) commandInData);
      break;

    case EMC_COOLANT_FLOOD_ON_TYPE:
      FLOOD_ON((EMC_COOLANT_FLOOD_ON *) commandInData);
      break;

    case EMC_COOLANT_FLOOD_OFF_TYPE:
      FLOOD_OFF((EMC_COOLANT_FLOOD_OFF *) commandInData);
      break;

    default:
      REPORT_ERROR(commandInData);
      break;
    }
}

void EMC_COOLANT_MODULE::PRE_PROCESS(void)
{
}

void EMC_COOLANT_MODULE::POST_PROCESS(void)
{
  // sync status with sensors
  coolantStatus.mist = isMistOn();
  coolantStatus.flood = isFloodOn();
}

void EMC_COOLANT_MODULE::INIT(EMC_COOLANT_INIT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniCoolant(EMC_INIFILE);

      // turn coolants off
      mistOff();
      floodOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::HALT(EMC_COOLANT_HALT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn coolants off
      mistOff();
      floodOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::ABORT(EMC_COOLANT_ABORT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn coolants off
      mistOff();
      floodOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::REPORT_ERROR(RCS_CMD_MSG *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      rcs_print_error("EMC_COOLANT_MODULE: unknown command %d\n",
                      cmdIn->type);
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::MIST_ON(EMC_COOLANT_MIST_ON *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      mistOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::MIST_OFF(EMC_COOLANT_MIST_OFF *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      mistOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::FLOOD_ON(EMC_COOLANT_FLOOD_ON *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      floodOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_COOLANT_MODULE::FLOOD_OFF(EMC_COOLANT_FLOOD_OFF *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      floodOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}
