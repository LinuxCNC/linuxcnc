/*
  minimilltool.cc

  Top-level controller for NIST minimill discrete I/O hierarchy

  Modification history:

  1-Jun-2001  FMP added handling of EMC_SET_DEBUG in decision process
  31-Aug-1999  FMP changed to minimilltool.cc
  22-Feb-1999  FMP added EMC_TOOL_SET_OFFSET, saveToolTable()
  19-Feb-1999  FMP took out setSubordinates()
  23-Apr-1998  FMP took out iniLoad(), called iniTool()
  2-Apr-1998  FMP took out spindle stuff altogether
  1-Apr-1998  FMP created from Shaver Bridgeport
  */

#include "rcs.hh"               // NML
#include "emc.hh"               // EMC NML
#include "canon.hh"             // CANON_TOOL_TABLE
#include "emcio.hh"             // these decls
#include "emcglb.h"             // global TOOL_TABLE_FILE[]
#include "initool.hh"           // iniTool()

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// shortcuts to NML module subordinate statuses
#define auxStatus ((EMC_AUX_STAT *) statusInData[auxSubNum])

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

  auxSubNum = addSubordinate(new RCS_CMD_CHANNEL(emcFormat, "auxCmd", "tool", EMC_NMLFILE), new RCS_STAT_CHANNEL(emcFormat, "auxSts", "tool", EMC_NMLFILE));

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
  ioStatus.aux = *auxStatus;

  // update the heartbeat
  ioStatus.heartbeat++;
}

void EMC_TOOL_MODULE::INIT(EMC_TOOL_INIT *cmdIn)
{
  EMC_AUX_INIT auxInitMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      // load params from ini file
      iniTool(EMC_INIFILE);
      loadToolTable(TOOL_TABLE_FILE, ioStatus.tool.toolTable);

      // send commands to subordinates
      sendCommand(&auxInitMsg, auxSubNum);

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
      // all are done
      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
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
  EMC_AUX_HALT auxHaltMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&auxHaltMsg, auxSubNum);

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
      // both are done

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
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
  EMC_AUX_ABORT auxAbortMsg;

  if (STATE_MATCH(NEW_COMMAND))
    {
      sendCommand(&auxAbortMsg, auxSubNum);

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
      // both are done

      status = RCS_DONE;
      stateNext(S0);
    }
  else if (STATE_MATCH(S1,
                       auxStatus->status == RCS_ERROR))
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
