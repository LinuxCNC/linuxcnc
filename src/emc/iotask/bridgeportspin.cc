/*
  bridgeportspin.cc

  Controller for Bridgeport spindle I/O

  Modification history:
  
  9-Nov-2002 P.C. Spindle speed passed to spindle functions via NML.
  31-Aug-1999  FMP changed to bridgeportspin.cc
  1-Oct-1998  FMP added enabled to spindle status, setting it to non-zero if
  spindleDirection is non-zero.
  1-Apr-1998  FMP changed from emcspin.cc to shvspin.cc
  24-Mar-1998  FMP changed ERROR() to REPORT_ERROR() due to conflict
  in VC++
  11-Dec-1997  FMP created from nce stuff
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "emcio.hh"             // these decls
#include "emcglb.h"
#include "extintf.h"            // extDioSet(),Clear()
#include "inispin.hh"           // iniSpindle()

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// local functions for acting on spindle

static void spindleForward(double speed)
{
  extDioWrite(SPINDLE_REVERSE_INDEX, ! SPINDLE_REVERSE_POLARITY);
  extDioWrite(SPINDLE_FORWARD_INDEX, SPINDLE_FORWARD_POLARITY);
}

static void spindleReverse(double speed)
{
  extDioWrite(SPINDLE_FORWARD_INDEX, ! SPINDLE_FORWARD_POLARITY);
  extDioWrite(SPINDLE_REVERSE_INDEX, SPINDLE_REVERSE_POLARITY);
}

static void spindleStop()
{
  extDioWrite(SPINDLE_FORWARD_INDEX, ! SPINDLE_FORWARD_POLARITY);
  extDioWrite(SPINDLE_REVERSE_INDEX, ! SPINDLE_REVERSE_POLARITY);
}

static void spindleBrake(int on)
{
  if (on)
    {
      extDioWrite(SPINDLE_BRAKE_INDEX, SPINDLE_BRAKE_POLARITY);
    }
  else
    {
      extDioWrite(SPINDLE_BRAKE_INDEX, ! SPINDLE_BRAKE_POLARITY);
    }
}

static void spindleIncrease(double speed)
{
  extDioWrite(SPINDLE_DECREASE_INDEX, ! SPINDLE_DECREASE_POLARITY);
  extDioWrite(SPINDLE_INCREASE_INDEX, SPINDLE_INCREASE_POLARITY);
}

static void spindleDecrease(double speed)
{
  extDioWrite(SPINDLE_INCREASE_INDEX, ! SPINDLE_INCREASE_POLARITY);
  extDioWrite(SPINDLE_DECREASE_INDEX, SPINDLE_DECREASE_POLARITY);
}

static void spindleConstant()
{
  extDioWrite(SPINDLE_INCREASE_INDEX, ! SPINDLE_INCREASE_POLARITY);
  extDioWrite(SPINDLE_DECREASE_INDEX, ! SPINDLE_DECREASE_POLARITY);
}

static int isSpindleDirection()
{
  int f, r;

  extDioCheck(SPINDLE_FORWARD_INDEX, &f);
  extDioCheck(SPINDLE_REVERSE_INDEX, &r);

  // spindle is interlocked so forward wins
  if (f == SPINDLE_FORWARD_POLARITY)
    {
      return 1;
    }

  if (r == SPINDLE_REVERSE_POLARITY)
    {
      return -1;
    }

  return 0;
}

static int isSpindleIncrease()
{
  int i;

  extDioCheck(SPINDLE_INCREASE_INDEX, &i);

  return i == SPINDLE_INCREASE_POLARITY;
}

static int isSpindleDecrease()
{
  int d;

  extDioCheck(SPINDLE_DECREASE_INDEX, &d);

  return d == SPINDLE_DECREASE_POLARITY;
}

static int isSpindleBrake()
{
  int b;

  extDioCheck(SPINDLE_BRAKE_INDEX, &b);

  if (b == SPINDLE_BRAKE_POLARITY)
    {
      return 1;
    }

  return 0;
}

// constructor

EMC_SPINDLE_MODULE::EMC_SPINDLE_MODULE()
{
  setErrorLogChannel(new NML(nmlErrorFormat, "emcError", "spindle", EMC_NMLFILE));

  setCmdChannel(new RCS_CMD_CHANNEL(emcFormat, "spindleCmd", "spindle", EMC_NMLFILE));

  setStatChannel(new RCS_STAT_CHANNEL(emcFormat, "spindleSts", "spindle", EMC_NMLFILE), &spindleStatus);
}

// destructor

EMC_SPINDLE_MODULE::~EMC_SPINDLE_MODULE(void)
{
}

void EMC_SPINDLE_MODULE::DECISION_PROCESS(void)
{
  switch (commandInData->type)
    {
    case EMC_SPINDLE_INIT_TYPE:
      INIT((EMC_SPINDLE_INIT *) commandInData);
      break;

    case EMC_SPINDLE_HALT_TYPE:
      HALT((EMC_SPINDLE_HALT *) commandInData);
      break;

    case EMC_SPINDLE_ABORT_TYPE:
      ABORT((EMC_SPINDLE_ABORT *) commandInData);
      break;

    case EMC_SPINDLE_FORWARD_TYPE:
      SPINDLE_FORWARD((EMC_SPINDLE_FORWARD *) commandInData);
      break;

    case EMC_SPINDLE_REVERSE_TYPE:
      SPINDLE_REVERSE((EMC_SPINDLE_REVERSE *) commandInData);
      break;

    case EMC_SPINDLE_STOP_TYPE:
      SPINDLE_STOP((EMC_SPINDLE_STOP *) commandInData);
      break;

    case EMC_SPINDLE_INCREASE_TYPE:
      SPINDLE_INCREASE((EMC_SPINDLE_INCREASE *) commandInData);
      break;

    case EMC_SPINDLE_DECREASE_TYPE:
      SPINDLE_DECREASE((EMC_SPINDLE_DECREASE *) commandInData);
      break;

    case EMC_SPINDLE_CONSTANT_TYPE:
      SPINDLE_CONSTANT((EMC_SPINDLE_CONSTANT *) commandInData);
      break;

    case EMC_SPINDLE_BRAKE_RELEASE_TYPE:
      SPINDLE_BRAKE_RELEASE((EMC_SPINDLE_BRAKE_RELEASE *) commandInData);
      break;

    case EMC_SPINDLE_BRAKE_ENGAGE_TYPE:
      SPINDLE_BRAKE_ENGAGE((EMC_SPINDLE_BRAKE_ENGAGE *) commandInData);
      break;

    default:
      REPORT_ERROR(commandInData);
      break;
    }
}

void EMC_SPINDLE_MODULE::PRE_PROCESS(void)
{
}

void EMC_SPINDLE_MODULE::POST_PROCESS(void)
{
  // sync status vars with sensors
//  spindleStatus.speed = 0.0;    // no speed sensor
  spindleStatus.direction = isSpindleDirection();
  spindleStatus.brake = isSpindleBrake();
  spindleStatus.increasing = isSpindleIncrease() ? 1 :
    isSpindleDecrease() ? -1 : 0;
  spindleStatus.enabled = (spindleStatus.direction != 0);
}

void EMC_SPINDLE_MODULE::INIT(EMC_SPINDLE_INIT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniSpindle(EMC_INIFILE);

      // stop spindle, brake it, make constant
      spindleStop();
      spindleBrake(1);
      spindleConstant();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::HALT(EMC_SPINDLE_HALT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // stop spindle, brake it, make constant
      spindleStop();
      spindleBrake(1);
      spindleConstant();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::ABORT(EMC_SPINDLE_ABORT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      // stop spindle, brake it, make constant
      spindleStop();
      spindleBrake(1);
      spindleConstant();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::REPORT_ERROR(RCS_CMD_MSG *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      rcs_print_error("EMC_SPINDLE_MODULE: unknown command %d\n",
                      cmdIn->type);
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_FORWARD(EMC_SPINDLE_FORWARD *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleStatus.speed = cmdIn->speed;
      spindleForward(cmdIn->speed);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_REVERSE(EMC_SPINDLE_REVERSE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleStatus.speed = cmdIn->speed;
      spindleReverse(cmdIn->speed);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_STOP(EMC_SPINDLE_STOP *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleStatus.speed = 0.0;
      spindleStop();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_INCREASE(EMC_SPINDLE_INCREASE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleIncrease(cmdIn->speed);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_DECREASE(EMC_SPINDLE_DECREASE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleDecrease(cmdIn->speed);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_CONSTANT(EMC_SPINDLE_CONSTANT *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleConstant();

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_BRAKE_RELEASE(EMC_SPINDLE_BRAKE_RELEASE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleBrake(0);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_SPINDLE_MODULE::SPINDLE_BRAKE_ENGAGE(EMC_SPINDLE_BRAKE_ENGAGE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      spindleBrake(1);

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}
