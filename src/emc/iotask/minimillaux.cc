/*
  minimillaux.cc

  Controller for NIST minimill auxiliary I/O

  Modification history:

  3-Mar-2000  FMP added setting of estopIn status
  31-Aug-1999  FMP changed to minimillaux.cc
  28-Apr-1998  FMP took out ::iniLoad(), put in iniAux()
  1-Apr-1998  FMP created from Shaver Bridgeport
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "emcio.hh"             // these decls
#include "emcglb.h"
#include "extintf.h"            // extDioSet(),Clear()
#include "iniaux.hh"            // iniAux()

// ident tag
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused))  ident[] = "$Id$";

// local functions

static int estopState = 1;

static void estopOn()
{
  estopState = 1;
}

static void estopOff()
{
  estopState = 0;
}

static int isEstop()
{
  return estopState;
}

// constructor

EMC_AUX_MODULE::EMC_AUX_MODULE()
{
  setErrorLogChannel(new NML(nmlErrorFormat, "emcError", "aux", EMC_NMLFILE));

  setCmdChannel(new RCS_CMD_CHANNEL(emcFormat, "auxCmd", "aux", EMC_NMLFILE));

  setStatChannel(new RCS_STAT_CHANNEL(emcFormat, "auxSts", "aux", EMC_NMLFILE), &auxStatus);
}

// destructor

EMC_AUX_MODULE::~EMC_AUX_MODULE(void)
{
}

void EMC_AUX_MODULE::DECISION_PROCESS(void)
{
  switch (commandInData->type)
    {
    case EMC_AUX_INIT_TYPE:
      INIT((EMC_AUX_INIT *) commandInData);
      break;

    case EMC_AUX_HALT_TYPE:
      HALT((EMC_AUX_HALT *) commandInData);
      break;

    case EMC_AUX_ABORT_TYPE:
      ABORT((EMC_AUX_ABORT *) commandInData);
      break;

    case EMC_AUX_DIO_WRITE_TYPE:
      DIO_WRITE((EMC_AUX_DIO_WRITE *) commandInData);
      break;

    case EMC_AUX_AIO_WRITE_TYPE:
      AIO_WRITE((EMC_AUX_AIO_WRITE *) commandInData);
      break;

    case EMC_AUX_ESTOP_ON_TYPE:
      ESTOP_ON((EMC_AUX_ESTOP_ON *) commandInData);
      break;

    case EMC_AUX_ESTOP_OFF_TYPE:
      ESTOP_OFF((EMC_AUX_ESTOP_OFF *) commandInData);
      break;

    default:
      REPORT_ERROR(commandInData);
      break;
    }
}

void EMC_AUX_MODULE::PRE_PROCESS(void)
{
}

void EMC_AUX_MODULE::POST_PROCESS(void)
{
  int t;
  unsigned char in;

  auxStatus.estopIn = auxStatus.estop = isEstop();

  // do digital ins, outs
  for (t = 0; t < EMC_AUX_MAX_DIN; t++)
    {
      extDioByteRead(t, &in);
      auxStatus.din[t] = in;
    }
  for (t = 0; t < EMC_AUX_MAX_DOUT; t++)
    {
      extDioByteCheck(t, &in);
      auxStatus.dout[t] = in;
    }

  // do analog ins, outs
  for (t = 0; t < EMC_AUX_MAX_AIN; t++)
    {
      extAioRead(t, &auxStatus.ain[t]);
    }
  for (t = 0; t < EMC_AUX_MAX_AOUT; t++)
    {
      extAioCheck(t, &auxStatus.aout[t]);
    }
}

void EMC_AUX_MODULE::INIT(EMC_AUX_INIT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniAux(EMC_INIFILE);

      // go into estop
      estopOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::HALT(EMC_AUX_HALT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // go into estop
      estopOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::ABORT(EMC_AUX_ABORT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // don't fool with the estop-- abort just means stop task
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::REPORT_ERROR(RCS_CMD_MSG *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      rcs_print_error("EMC_AUX_MODULE: unknown command %d\n",
                      cmdIn->type);
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::ESTOP_ON(EMC_AUX_ESTOP_ON *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // go into estop
      estopOn();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::DIO_WRITE(EMC_AUX_DIO_WRITE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // write bit in digital IO
      extDioWrite(cmdIn->index, cmdIn->value);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::AIO_WRITE(EMC_AUX_AIO_WRITE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // write bit in analog IO
      extAioWrite(cmdIn->index, cmdIn->value);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_AUX_MODULE::ESTOP_OFF(EMC_AUX_ESTOP_OFF *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // come out of estop
      estopOff();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}
