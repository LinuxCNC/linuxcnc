/*
  bridgeporttool.cc

  Top-level controller for Bridgeport discrete I/O hierarchy

  Modification history:

  9-Nov-2002  P.C. Added Spindle Speed to EMC_SPINDLE_ON NML message.
  1-Jun-2001  FMP added handling of EMC_SET_DEBUG in decision process
  31-Aug-1999  FMP changed to bridgeporttool.cc
  22-Feb-1999  FMP added EMC_TOOL_SET_OFFSET, saveToolTable()
  19-Feb-1999  FMP took out setSubordinates()
  5-Oct-1998  FMP added check of estop for lube on
  1-Oct-1998 FMP removed check of estop in SPINDLE_OFF(), since it left
  the spindle on when estop triggered
  14-May-1998 WPS removed call to setSubordinates() from constructor
  23-Apr-1998  FMP took out iniLoad(), called iniTool()
  1-Apr-1998  FMP changed from emctool.cc to shvtool.cc
  24-Mar-1998  FMP changed ERROR() to REPORT_ERROR() due to conflict
  in VC++
  20-Feb-1998  FMP added brake release,engage pass-through
  7-Jan-1998  FMP added heartbeat
  11-Dec-1997  FMP created from nce stuff
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "canon.hh"             // CANON_TOOL_TABLE
#include "emcio.hh"             // these decls
#include "emcglb.h"             // global TOOL_TABLE_FILE
#include "initool.hh"           // iniTool()

// ident tag
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// shortcuts to NML module subordinate statuses
#define spindleStatus ((EMC_SPINDLE_STAT *) statusInData[spindleSubNum])
#define coolantStatus ((EMC_COOLANT_STAT *) statusInData[coolantSubNum])
#define auxStatus ((EMC_AUX_STAT *) statusInData[auxSubNum])
#define lubeStatus ((EMC_LUBE_STAT *) statusInData[lubeSubNum])

/*
  load the tool table from file filename into toolTable[] array.
  Array is CANON_TOOL_MAX + 1 entries, since 0 is included.

  If filename is "", use global established from ini file
  */
static int loadToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
{
  int t;
  FILE *fp;
  char buffer[CANON_TOOL_ENTRY_LEN];
  const char *name;

  // check filename
  if (filename[0] == 0)
    {
      name = TOOL_TABLE_FILE;
    }
  else
    {
      // point to name provided
      name = filename;
    }

  // open tool table file
  if (NULL == (fp = fopen(name, "r")))
    {
      // can't open file
      return -1;
    }

  // clear out tool table
  for (t = 0; t <= CANON_TOOL_MAX; t++)
    {
      // unused tools are 0, 0.0, 0.0
      toolTable[t].id = 0;
      toolTable[t].length = 0.0;
      toolTable[t].diameter = 0.0;
    }

  /*
     Override 0's with codes from tool file

     File format is:

     <header>
     <pocket # 0..CANON_TOOL_MAX> <FMS id> <length> <diameter>
     ...

     */

  // read and discard header
  if (NULL == fgets(buffer, 256, fp))
    {
      // nothing in file at all
      fclose(fp);
      return -1;
    }

  while (!feof(fp))
    {
      int pocket;
      int id;
      double length;
      double diameter;

      // just read pocket, ID, and length offset
      if (NULL == fgets(buffer, CANON_TOOL_ENTRY_LEN, fp))
        {
          break;
        }

      if (4 != sscanf(buffer, "%d %d %lf %lf",
                      &pocket, &id, &length, &diameter))
        {
          // bad entry-- skip
          continue;
        }
      else
        {
          if (pocket < 0 || pocket > CANON_TOOL_MAX)
            {
              continue;
            }
          else
            {
              toolTable[pocket].id = id;
              toolTable[pocket].length = length;
              toolTable[pocket].diameter = diameter;
            }
        }
    }

  // close the file
  fclose(fp);

  return 0;
}

/*
  save the tool table to file filename from toolTable[] array.
  Array is CANON_TOOL_MAX + 1 entries, since 0 is included.

  If filename is "", use global established from ini file
  */
static int saveToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
{
  int pocket;
  FILE *fp;
  const char *name;

  // check filename
  if (filename[0] == 0)
    {
      name = TOOL_TABLE_FILE;
    }
  else
    {
      // point to name provided
      name = filename;
    }

  // open tool table file
  if (NULL == (fp = fopen(name, "w")))
    {
      // can't open file
      return -1;
    }

  // write header
  fprintf(fp, "POC\tFMS\tLEN\t\tDIAM\n");

  for (pocket = 1; pocket <= CANON_TOOL_MAX; pocket++)
    {
      fprintf(fp, "%d\t%d\t%f\t%f\n",
              pocket,
              toolTable[pocket].id,
              toolTable[pocket].length,
              toolTable[pocket].diameter);
    }

  // close the file
  fclose(fp);

  return 0;
}

// constructor

EMC_TOOL_MODULE::EMC_TOOL_MODULE()
{
  EMC_TOOL_INIT initMsg;

  setErrorLogChannel(new NML(nmlErrorFormat, "emcError", "tool", EMC_NMLFILE));

  setCmdChannel(new RCS_CMD_CHANNEL(emcFormat, "toolCmd", "tool", EMC_NMLFILE));

  setStatChannel(new RCS_STAT_CHANNEL(emcFormat, "toolSts", "tool", EMC_NMLFILE), &ioStatus);

  spindleSubNum = addSubordinate(new RCS_CMD_CHANNEL(emcFormat, "spindleCmd", "tool", EMC_NMLFILE), new RCS_STAT_CHANNEL(emcFormat, "spindleSts", "tool", EMC_NMLFILE));

  coolantSubNum = addSubordinate(new RCS_CMD_CHANNEL(emcFormat, "coolantCmd", "tool", EMC_NMLFILE), new RCS_STAT_CHANNEL(emcFormat, "coolantSts", "tool", EMC_NMLFILE));

  auxSubNum = addSubordinate(new RCS_CMD_CHANNEL(emcFormat, "auxCmd", "tool", EMC_NMLFILE), new RCS_STAT_CHANNEL(emcFormat, "auxSts", "tool", EMC_NMLFILE));

  lubeSubNum = addSubordinate(new RCS_CMD_CHANNEL(emcFormat, "lubeCmd", "tool", EMC_NMLFILE), new RCS_STAT_CHANNEL(emcFormat, "lubeSts", "tool", EMC_NMLFILE));

  // local data
  deltaClock = 0.0;

  // stuff an INIT to get things going
  initMsg.serial_number = 1;    // comes up as 0
  commandIn->write(initMsg);
}

// destructor

EMC_TOOL_MODULE::~EMC_TOOL_MODULE(void)
{
}

void EMC_TOOL_MODULE::DECISION_PROCESS(void)
{
  switch (commandInData->type)
    {
    case EMC_SET_DEBUG_TYPE:
      /* this will be done "immediately", without a state table, since
	 it's trivial. We have to make sure to manipulate the state
	 table vars as we would any command */
      EMC_DEBUG = ((EMC_SET_DEBUG *) commandInData)->debug;
      ioStatus.debug = EMC_DEBUG;
      status = RCS_DONE;
      stateNext(S0);
      break;

    case EMC_TOOL_INIT_TYPE:
      INIT((EMC_TOOL_INIT *) commandInData);
      break;

    case EMC_TOOL_HALT_TYPE:
      HALT((EMC_TOOL_HALT *) commandInData);
      break;

    case EMC_TOOL_ABORT_TYPE:
      ABORT((EMC_TOOL_ABORT *) commandInData);
      break;

    case EMC_TOOL_PREPARE_TYPE:
      TOOL_PREPARE((EMC_TOOL_PREPARE *) commandInData);
      break;

    case EMC_TOOL_LOAD_TYPE:
      TOOL_LOAD((EMC_TOOL_LOAD *) commandInData);
      break;

    case EMC_TOOL_UNLOAD_TYPE:
      TOOL_UNLOAD((EMC_TOOL_UNLOAD *) commandInData);
      break;

    case EMC_TOOL_LOAD_TOOL_TABLE_TYPE:
      TOOL_LOAD_TOOL_TABLE((EMC_TOOL_LOAD_TOOL_TABLE *) commandInData);
      break;

    case EMC_TOOL_SET_OFFSET_TYPE:
      TOOL_SET_OFFSET((EMC_TOOL_SET_OFFSET *) commandInData);
      break;

    case EMC_SPINDLE_ON_TYPE:
      SPINDLE_ON((EMC_SPINDLE_ON *) commandInData);
      break;

    case EMC_SPINDLE_OFF_TYPE:
      SPINDLE_OFF((EMC_SPINDLE_OFF *) commandInData);
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

    case EMC_COOLANT_MIST_ON_TYPE:
      COOLANT_MIST_ON((EMC_COOLANT_MIST_ON *) commandInData);
      break;

    case EMC_COOLANT_MIST_OFF_TYPE:
      COOLANT_MIST_OFF((EMC_COOLANT_MIST_OFF *) commandInData);
      break;

    case EMC_COOLANT_FLOOD_ON_TYPE:
      COOLANT_FLOOD_ON((EMC_COOLANT_FLOOD_ON *) commandInData);
      break;

    case EMC_COOLANT_FLOOD_OFF_TYPE:
      COOLANT_FLOOD_OFF((EMC_COOLANT_FLOOD_OFF *) commandInData);
      break;

    case EMC_AUX_ESTOP_ON_TYPE:
      AUX_ESTOP_ON((EMC_AUX_ESTOP_ON *) commandInData);
      break;

    case EMC_AUX_ESTOP_OFF_TYPE:
      AUX_ESTOP_OFF((EMC_AUX_ESTOP_OFF *) commandInData);
      break;

    case EMC_AUX_DIO_WRITE_TYPE:
      AUX_DIO_WRITE((EMC_AUX_DIO_WRITE *) commandInData);
      break;

    case EMC_AUX_AIO_WRITE_TYPE:
      AUX_AIO_WRITE((EMC_AUX_AIO_WRITE *) commandInData);
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

void EMC_TOOL_MODULE::PRE_PROCESS(void)
{
}

void EMC_TOOL_MODULE::POST_PROCESS(void)
{
  // copy in the subordinate status
  ioStatus.spindle = *spindleStatus;
  ioStatus.coolant = *coolantStatus;
  ioStatus.aux = *auxStatus;
  ioStatus.lube = *lubeStatus;

  // update the heartbeat
  ioStatus.heartbeat++;
}

void EMC_TOOL_MODULE::INIT(EMC_TOOL_INIT *cmdIn)
{
  EMC_SPINDLE_INIT spindleInitMsg;
  EMC_COOLANT_INIT coolantInitMsg;
  EMC_AUX_INIT auxInitMsg;
  EMC_LUBE_INIT lubeInitMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniTool(EMC_INIFILE);
      loadToolTable(TOOL_TABLE_FILE, ioStatus.tool.toolTable);

      // send commands to subordinates
      sendCommand(&spindleInitMsg, spindleSubNum);
      sendCommand(&coolantInitMsg, coolantSubNum);
      sendCommand(&auxInitMsg, auxSubNum);
      sendCommand(&lubeInitMsg, lubeSubNum);

      status = RCS_EXEC;
      stateNext(S1);

    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE &&
                       coolantStatus->status == RCS_DONE &&
                       auxStatus->status == RCS_DONE &&
                       lubeStatus->status == RCS_DONE))
    {
      // all are done
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR ||
                       coolantStatus->status == RCS_ERROR ||
                       auxStatus->status == RCS_ERROR ||
                       lubeStatus->status == RCS_ERROR))
    {
      // a subordinate reported an error
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1))
    {
      // one or more are still executing-- keep waiting
    }
}

void EMC_TOOL_MODULE::HALT(EMC_TOOL_HALT *cmdIn)
{
  EMC_SPINDLE_HALT spindleHaltMsg;
  EMC_COOLANT_HALT coolantHaltMsg;
  EMC_AUX_HALT auxHaltMsg;
  EMC_LUBE_HALT lubeHaltMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleHaltMsg, spindleSubNum);
      sendCommand(&coolantHaltMsg, coolantSubNum);
      sendCommand(&auxHaltMsg, auxSubNum);
      sendCommand(&lubeHaltMsg, lubeSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE &&
                       coolantStatus->status == RCS_DONE &&
                       auxStatus->status == RCS_DONE &&
                       lubeStatus->status == RCS_DONE))
    {
      // both are done

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR ||
                       coolantStatus->status == RCS_ERROR ||
                       auxStatus->status == RCS_ERROR ||
                       lubeStatus->status == RCS_ERROR))
    {
      // a subordinate reported an error

      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1))
    {
      // one or more are still executing-- keep waiting
    }
}

void EMC_TOOL_MODULE::ABORT(EMC_TOOL_ABORT *cmdIn)
{
  EMC_SPINDLE_ABORT spindleAbortMsg;
  EMC_COOLANT_ABORT coolantAbortMsg;
  EMC_AUX_ABORT auxAbortMsg;
  EMC_LUBE_ABORT lubeAbortMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleAbortMsg, spindleSubNum);
      sendCommand(&coolantAbortMsg, coolantSubNum);
      sendCommand(&auxAbortMsg, auxSubNum);
      sendCommand(&lubeAbortMsg, lubeSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE &&
                       coolantStatus->status == RCS_DONE &&
                       auxStatus->status == RCS_DONE &&
                       lubeStatus->status == RCS_DONE))
    {
      // both are done

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR ||
                       coolantStatus->status == RCS_ERROR ||
                       auxStatus->status == RCS_ERROR ||
                       lubeStatus->status == RCS_ERROR))
    {
      // a subordinate reported an error

      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1))
    {
      // one or more are still executing-- keep waiting
    }
}

void EMC_TOOL_MODULE::REPORT_ERROR(RCS_CMD_MSG *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      rcs_print_error("EMC_TOOL_MODULE: unknown command %d\n",
                      cmdIn->type);
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_TOOL_MODULE::TOOL_PREPARE(EMC_TOOL_PREPARE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      ioStatus.tool.toolPrepped = cmdIn->tool;

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_TOOL_MODULE::TOOL_LOAD(EMC_TOOL_LOAD *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      ioStatus.tool.toolInSpindle = ioStatus.tool.toolPrepped;
      ioStatus.tool.toolPrepped = 0;

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_TOOL_MODULE::TOOL_UNLOAD(EMC_TOOL_UNLOAD *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      ioStatus.tool.toolInSpindle = 0;

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_TOOL_MODULE::TOOL_LOAD_TOOL_TABLE(EMC_TOOL_LOAD_TOOL_TABLE *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      if (0 != loadToolTable(cmdIn->file, ioStatus.tool.toolTable))
        {
          status = RCS_ERROR;
        }
      else
        {
          status = RCS_DONE;
        }
      stateNext(S0);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

void EMC_TOOL_MODULE::TOOL_SET_OFFSET(EMC_TOOL_SET_OFFSET *cmdIn)
{
  if (STATE_MATCH(NEW_COMMAND))
    {
      if (cmdIn->tool < 0 || cmdIn->tool > CANON_TOOL_MAX)
        {
          status = RCS_ERROR;
          stateNext(S0);
        }
      else
        {
          // good tool number, so record it and save it
          ioStatus.tool.toolTable[cmdIn->tool].length = cmdIn->length;
          ioStatus.tool.toolTable[cmdIn->tool].diameter = cmdIn->diameter;

          if (0 != saveToolTable(TOOL_TABLE_FILE, ioStatus.tool.toolTable))
            {
              status = RCS_ERROR;
            }
          else
            {
              status = RCS_DONE;
            }
          stateNext(S0);
        }
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
}

// spindle functions

/*
  Spindle on means:

  1. If we're in estop, do nothing, report error
  2. Take off brake, wait SPINDLE_ON_WAIT
  3. If speed < 0, call for reverse
  4. If speed > 0, call for forward
  5. If speed = 0, call for stop
  */
void EMC_TOOL_MODULE::SPINDLE_ON(EMC_SPINDLE_ON *cmdIn)
{
  EMC_SPINDLE_FORWARD forwardMsg;
  EMC_SPINDLE_REVERSE reverseMsg;
  EMC_SPINDLE_BRAKE_RELEASE brakeReleaseMsg;
  EMC_SPINDLE_STOP stopMsg;

  if (STATE_MATCH(NEW_COMMAND,
                  auxStatus->estop))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(NEW_COMMAND,
                       ! auxStatus->estop))
    {
      // take brake off
      sendCommand(&brakeReleaseMsg, spindleSubNum);
      // load delta clock
      deltaClock = SPINDLE_ON_WAIT;

      status = RCS_EXEC;
      stateNext(S1);            // wait for time delay
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1))
    {
      // wait for clock to expire
      if (deltaClock <= 0.0)
        {
          stateNext(S2);
        }
      else
        {
          deltaClock -= EMC_IO_CYCLE_TIME;
        }
    }
  else if (STATE_MATCH(S2))
    {
      if (cmdIn->speed < 0.0)
        {
          reverseMsg.speed = cmdIn->speed;
          sendCommand(&reverseMsg, spindleSubNum);
        }
      else if (cmdIn->speed > 0.0)
        {
          forwardMsg.speed = cmdIn->speed;
	  sendCommand(&forwardMsg, spindleSubNum);
        }
      else
        {
          sendCommand(&stopMsg, spindleSubNum);
        }

      stateNext(S3);
    }
  else if (STATE_MATCH(S3,
                       spindleStatus->status == RCS_DONE))
    {
      // done
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S3,
                       spindleStatus->status == RCS_ERROR))
    {
      // error
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S3,
                       spindleStatus->status == RCS_EXEC))
    {
      // still running
    }
}

/*
  Spindle off means:

  1. Set speed to 0 internally
  2. Set forward and reverse to off
  3. Wait SPINDLE_OFF_WAIT
  4. Apply brake
  */
void EMC_TOOL_MODULE::SPINDLE_OFF(EMC_SPINDLE_OFF *cmdIn)
{
  EMC_SPINDLE_STOP stopMsg;
  EMC_SPINDLE_BRAKE_ENGAGE brakeEngageMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      // turn spindle off
      sendCommand(&stopMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);            // wait on clock
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working on the stop
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      // load delta clock
      deltaClock = SPINDLE_OFF_WAIT;
      stateNext(S2);
    }
  else if (STATE_MATCH(S2))
    {
      if (deltaClock <= 0.0)
        {
          stateNext(S3);
        }
      else
        {
          deltaClock -= EMC_IO_CYCLE_TIME;
        }
    }
  else if (STATE_MATCH(S3))
    {
      // time's up; put on brake
      sendCommand(&brakeEngageMsg, spindleSubNum);

      stateNext(S4);
    }
  else if (STATE_MATCH(S4,
                       spindleStatus->status == RCS_DONE))
    {
      // done
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S4,
                       spindleStatus->status == RCS_ERROR))
    {
      // error
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S4,
                       spindleStatus->status == RCS_EXEC))
    {
      // still running
    }
}

void EMC_TOOL_MODULE::SPINDLE_INCREASE(EMC_SPINDLE_INCREASE *cmdIn)
{
  EMC_SPINDLE_INCREASE spindleIncreaseMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleIncreaseMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::SPINDLE_DECREASE(EMC_SPINDLE_DECREASE *cmdIn)
{
  EMC_SPINDLE_DECREASE spindleDecreaseMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleDecreaseMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::SPINDLE_CONSTANT(EMC_SPINDLE_CONSTANT *cmdIn)
{
  EMC_SPINDLE_CONSTANT spindleConstantMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleConstantMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::SPINDLE_BRAKE_RELEASE(EMC_SPINDLE_BRAKE_RELEASE *cmdIn)
{
  EMC_SPINDLE_BRAKE_RELEASE spindleBrakeReleaseMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleBrakeReleaseMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::SPINDLE_BRAKE_ENGAGE(EMC_SPINDLE_BRAKE_ENGAGE *cmdIn)
{
  EMC_SPINDLE_BRAKE_ENGAGE spindleBrakeEngageMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&spindleBrakeEngageMsg, spindleSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       spindleStatus->status == RCS_EXEC))
    {
      // still working
    }
}

// coolant functions
void EMC_TOOL_MODULE::COOLANT_MIST_ON(EMC_COOLANT_MIST_ON *cmdIn)
{
  EMC_COOLANT_MIST_ON mistOnMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&mistOnMsg, coolantSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::COOLANT_MIST_OFF(EMC_COOLANT_MIST_OFF *cmdIn)
{
  EMC_COOLANT_MIST_OFF mistOffMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&mistOffMsg, coolantSubNum);
      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_EXEC))
    {
      // still working
    }
  else
    {
      printf("mist off %d %d\n", state, coolantStatus->status);
    }
}

void EMC_TOOL_MODULE::COOLANT_FLOOD_ON(EMC_COOLANT_FLOOD_ON *cmdIn)
{
  EMC_COOLANT_FLOOD_ON floodOnMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&floodOnMsg, coolantSubNum);
      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::COOLANT_FLOOD_OFF(EMC_COOLANT_FLOOD_OFF *cmdIn)
{
  EMC_COOLANT_FLOOD_OFF floodOffMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&floodOffMsg, coolantSubNum);
      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       coolantStatus->status == RCS_EXEC))
    {
      // still working
    }
  else
    {
      printf("flood off %d %d\n", state, coolantStatus->status);
    }
}

// auxiliary functions
void EMC_TOOL_MODULE::AUX_ESTOP_ON(EMC_AUX_ESTOP_ON *cmdIn)
{
  EMC_AUX_ESTOP_ON estopOnMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&estopOnMsg, auxSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::AUX_ESTOP_OFF(EMC_AUX_ESTOP_OFF *cmdIn)
{
  EMC_AUX_ESTOP_OFF estopOffMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&estopOffMsg, auxSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::AUX_DIO_WRITE(EMC_AUX_DIO_WRITE *cmdIn)
{
  EMC_AUX_DIO_WRITE dioWriteMsg = *cmdIn;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&dioWriteMsg, auxSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::AUX_AIO_WRITE(EMC_AUX_AIO_WRITE *cmdIn)
{
  EMC_AUX_AIO_WRITE aioWriteMsg = *cmdIn;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&aioWriteMsg, auxSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_EXEC))
    {
      // still working
    }
}

// lube functions
void EMC_TOOL_MODULE::LUBE_ON(EMC_LUBE_ON *cmdIn)
{
  EMC_LUBE_ON lubeOnMsg;

  if (STATE_MATCH(NEW_COMMAND,
                  auxStatus->estop))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(NEW_COMMAND,
                       ! auxStatus->estop))
    {
      sendCommand(&lubeOnMsg, lubeSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_EXEC))
    {
      // still working
    }
}

void EMC_TOOL_MODULE::LUBE_OFF(EMC_LUBE_OFF *cmdIn)
{
  EMC_LUBE_OFF lubeOffMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&lubeOffMsg, lubeSubNum);

      status = RCS_EXEC;
      stateNext(S1);
    }
  else if (STATE_MATCH(S0))
    {
      // idle
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_DONE))
    {
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_ERROR))
    {
      status = RCS_ERROR;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       lubeStatus->status == RCS_EXEC))
    {
      // still working
    }
}
