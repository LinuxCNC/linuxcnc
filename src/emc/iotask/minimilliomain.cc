/*
  minimilliomain.cc

  Main controller for minimill I/O: tool, estop auxiliary

  Modification history:

  8-Nov-1999 WPS If'd out RCS_TIMER::wait() calls.
  1-Oct-1999  FMP took out lookup of PARPORT_IO_ADDRESS since no one uses it
  3-Sep-1999  FMP looked for PARPORT_IO_ADDRESS in EMCIO section first;
  used EMC_DEBUG to inhibit version printing
  31-Aug-1999  FMP changed to minimilliomain.cc; took out -nml option
  15-Jun-1999  FMP set EMC_DEBUG in iniLoad()
  7-Aug-1998  FMP changed extInit/Quit() to extDioInit/Quit()
  6-Jul-1998  FMP added Argc,Argv copies; call to emcGetArgs()
  2-Apr-1998  FMP created from emciomain, with no spindle
  */

#include <string.h>             // strcpy()
#include <stdlib.h>             // exit()
#include <signal.h>             // SIGINT, signal()
#include "rcs.hh"               // RCS_TIMER
#include "emcglb.h"             // EMC_IO_CYCLE_TIME, EMC_NMLFILE
#include "emcio.hh"             // EMC_IO_INIFILE, EMC_TOOL_MODULE
#include "extintf.h"            // extInit(), extQuit()

// ident tag
/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

// command line args-- global so that other modules can access
int Argc;
char **Argv;

// flag signifying main loop is to terminate
static int done;

// signal handler for ^C
static void quit(int sig)
{
  done = 1;
}

static int iniLoad(const char *filename)
{
  INIFILE inifile;
  const char *inistring;
  char version[INIFILE_MAX_LINELEN];
  double saveDouble;

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

  if (EMC_DEBUG & EMC_DEBUG_VERSIONS) {
    if (NULL != (inistring = inifile.find("VERSION", "EMC"))) {
      // print version
      sscanf(inistring, "$Revision: %s", version);
      rcs_print("Version:  %s\n", version);
    }
    else {
      // not found, not fatal
      rcs_print("Version:  (not found)\n");
    }

    if (NULL != (inistring = inifile.find("MACHINE", "EMC"))) {
      // print machine
      rcs_print("Machine:  %s\n", inistring);
    }
    else {
      // not found, not fatal
      rcs_print("Machine:  (not found)\n");
    }
  }

  if (NULL != (inistring = inifile.find("NML_FILE", "EMC"))) {
    // copy to global
    strcpy(EMC_NMLFILE, inistring);
  }
  else {
    // not found, use default
  }

  saveDouble = EMC_IO_CYCLE_TIME;
  if (NULL != (inistring = inifile.find("CYCLE_TIME", "EMCIO"))) {
    if (1 == sscanf(inistring, "%lf", &EMC_IO_CYCLE_TIME)) {
      // found it
    }
    else {
      // found, but invalid
      EMC_IO_CYCLE_TIME = saveDouble;
      rcs_print("invalid [EMCIO] CYCLE_TIME in %s (%s); using default %f\n",
                filename, inistring, EMC_IO_CYCLE_TIME);
    }
  }
  else {
    // not found, using default
    rcs_print("[EMCIO] CYCLE_TIME not found in %s; using default %f\n",
              filename, EMC_IO_CYCLE_TIME);
  }

  // close it
  inifile.close();

  return 0;
}

/*
  syntax: a.out {<inifile>} {<nmlfile>}
  */
int main(int argc, char *argv[])
{
  EMC_TOOL_MODULE *tool;
  EMC_AUX_MODULE *aux;
  RCS_TIMER *timer;

  // copy command line args
  Argc = argc;
  Argv = argv;

  // set print destination to stdout, for console apps
  set_rcs_print_destination(RCS_PRINT_TO_STDOUT);

  // process command line args, indexing argv[] from [1]
  if (0 != emcGetArgs(argc, argv)) {
    rcs_print_error("error in argument list\n");
    exit(1);
  }

  // get configuration information
  iniLoad(EMC_INIFILE);

  // init external IO here, since everyone uses it
  if (0 != extDioInit(EMC_INIFILE)) {
    rcs_print_error("can't initialize IO-- privilege problems?\n");
    exit(1);
  }

  // get controllers to run
  tool = new EMC_TOOL_MODULE();
  aux = new EMC_AUX_MODULE();

  // get timer
  timer = new RCS_TIMER(EMC_IO_CYCLE_TIME);

  // set the SIGINT handler
  signal(SIGINT, quit);

  // enter main loop
  while (!done) {
    // run controller logic
    tool->controller();
    aux->controller();

    // wait on timer
    if (EMC_IO_CYCLE_TIME > 0.0) {
#if defined(LINUX_KERNEL_2_2)
      // work around bug in gettimeofday() by running off nominal time
      esleep(EMC_IO_CYCLE_TIME);
#else
      timer->wait();
#endif
    }
  }

  // clean up timer
  delete timer;

  // clean up controller
  delete aux;
  delete tool;

  // quit external IO here, since everyone uses it
  extDioQuit();

  exit(0);
}
