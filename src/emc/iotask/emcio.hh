#ifndef EMCIO_HH
#define EMCIO_HH

/*
  emcio.hh

  Decls for EMC control modules in discrete I/O hierarchy

  Modification history:

  23-Apr-1998  FMP took out iniLoad(), used iniTool() instead
  24-Mar-1998  FMP changed ERROR() to REPORT_ERROR() due to conflict
  in VC++
  20-Feb-1998  FMP added spindle brake release,engage in tool module
  11-Dec-1997  FMP created from nce stuff
  */

#include "emc.hh"               // EMC NML
#include "nml_mod.hh"           // NML_MODULE
#include "inifile.h"            // INIFILE

// how long an inifile name can be
#define EMCIO_INIFILE_NAME_LEN 256

// default ini file name
#define DEFAULT_EMCIO_INIFILE_NAME "emc.ini"

// this should be defined in main file, set by argv
extern char EMCIO_INIFILE[EMCIO_INIFILE_NAME_LEN];

// EMC dicrete I/O control module classes

// top level is tool controller
class EMC_TOOL_MODULE: public NML_MODULE
{
 public:

  EMC_TOOL_MODULE(void);
  ~EMC_TOOL_MODULE(void);

  void DECISION_PROCESS(void);
  void PRE_PROCESS(void);
  void POST_PROCESS(void);

  /*
    EMC tool controller decomposes higher-level I/O commands into
    sequences of actions for spindle, coolant, and auxiliary controllers.

    Tool commands are handled directly.

    Spindle commands are given as on (with a signed speed) or off; they
    are converted to brake-on,off, spindle forward,reverse for the lower
    level spindle controller.

    Coolant functions are checked for estop-on status, then passed to
    the coolant controller.

    Auxiliary (estop) functions are checked so that when coming out of
    estop, all systems are in a safe state (spindle off, brake on, coolant
    off).
    */

  // generic functions
  void INIT(EMC_TOOL_INIT *);
  void HALT(EMC_TOOL_HALT *);
  void ABORT(EMC_TOOL_ABORT *);
  void REPORT_ERROR(RCS_CMD_MSG *);

  // tool functions
  void TOOL_PREPARE(EMC_TOOL_PREPARE *);
  void TOOL_LOAD(EMC_TOOL_LOAD *);
  void TOOL_UNLOAD(EMC_TOOL_UNLOAD *);
  void TOOL_LOAD_TOOL_TABLE(EMC_TOOL_LOAD_TOOL_TABLE *);
  void TOOL_SET_OFFSET(EMC_TOOL_SET_OFFSET *);

  // spindle functions
  void SPINDLE_ON(EMC_SPINDLE_ON *);
  void SPINDLE_OFF(EMC_SPINDLE_OFF *);
  void SPINDLE_INCREASE(EMC_SPINDLE_INCREASE *);
  void SPINDLE_DECREASE(EMC_SPINDLE_DECREASE *);
  void SPINDLE_CONSTANT(EMC_SPINDLE_CONSTANT *);
  void SPINDLE_BRAKE_RELEASE(EMC_SPINDLE_BRAKE_RELEASE *);
  void SPINDLE_BRAKE_ENGAGE(EMC_SPINDLE_BRAKE_ENGAGE *);
  void SPINDLE_ENABLE(EMC_SPINDLE_ENABLE *);
  void SPINDLE_DISABLE(EMC_SPINDLE_DISABLE *);

  // coolant functions
  void COOLANT_MIST_ON(EMC_COOLANT_MIST_ON *);
  void COOLANT_MIST_OFF(EMC_COOLANT_MIST_OFF *);
  void COOLANT_FLOOD_ON(EMC_COOLANT_FLOOD_ON *);
  void COOLANT_FLOOD_OFF(EMC_COOLANT_FLOOD_OFF *);

  // auxiliary functions
  void AUX_ESTOP_ON(EMC_AUX_ESTOP_ON *);
  void AUX_ESTOP_OFF(EMC_AUX_ESTOP_OFF *);
  void AUX_DIO_WRITE(EMC_AUX_DIO_WRITE *);
  void AUX_AIO_WRITE(EMC_AUX_AIO_WRITE *);

  // lube functions
  void LUBE_ON(EMC_LUBE_ON *);
  void LUBE_OFF(EMC_LUBE_OFF *);

  // subordinate ids
  int spindleSubNum;
  int coolantSubNum;
  int auxSubNum;
  int lubeSubNum;

  // aggregate status information
  EMC_IO_STAT ioStatus;

  // local data
  double deltaClock;            // load and decrement for time delays
};

// spindle controller is subordinate to tool controller
class EMC_SPINDLE_MODULE: public NML_MODULE
{
 public:

  EMC_SPINDLE_MODULE(void);
  ~EMC_SPINDLE_MODULE(void);

  void DECISION_PROCESS(void);
  void PRE_PROCESS(void);
  void POST_PROCESS(void);

  void INIT(EMC_SPINDLE_INIT *);
  void HALT(EMC_SPINDLE_HALT *);
  void ABORT(EMC_SPINDLE_ABORT *);
  void REPORT_ERROR(RCS_CMD_MSG *);

  // spindle functions
  void SPINDLE_FORWARD(EMC_SPINDLE_FORWARD *);
  void SPINDLE_REVERSE(EMC_SPINDLE_REVERSE *);
  void SPINDLE_ON(EMC_SPINDLE_ON *);
  void SPINDLE_STOP(EMC_SPINDLE_STOP *);
  void SPINDLE_INCREASE(EMC_SPINDLE_INCREASE *);
  void SPINDLE_DECREASE(EMC_SPINDLE_DECREASE *);
  void SPINDLE_CONSTANT(EMC_SPINDLE_CONSTANT *);
  void SPINDLE_BRAKE_RELEASE(EMC_SPINDLE_BRAKE_RELEASE *);
  void SPINDLE_BRAKE_ENGAGE(EMC_SPINDLE_BRAKE_ENGAGE *);
  void SPINDLE_ENABLE(EMC_SPINDLE_ENABLE *);
  void SPINDLE_DISABLE(EMC_SPINDLE_DISABLE *);

  // status information
  EMC_SPINDLE_STAT spindleStatus;
};

// coolant controller is subordinate to tool controller
class EMC_COOLANT_MODULE: public NML_MODULE
{
 public:

  EMC_COOLANT_MODULE(void);
  ~EMC_COOLANT_MODULE(void);

  void DECISION_PROCESS(void);
  void PRE_PROCESS(void);
  void POST_PROCESS(void);

  void INIT(EMC_COOLANT_INIT *);
  void HALT(EMC_COOLANT_HALT *);
  void ABORT(EMC_COOLANT_ABORT *);
  void REPORT_ERROR(RCS_CMD_MSG *);

  // coolant functions
  void MIST_ON(EMC_COOLANT_MIST_ON *);
  void MIST_OFF(EMC_COOLANT_MIST_OFF *);
  void FLOOD_ON(EMC_COOLANT_FLOOD_ON *);
  void FLOOD_OFF(EMC_COOLANT_FLOOD_OFF *);

  // status information
  EMC_COOLANT_STAT coolantStatus;
};

// auxiliary controller is subordinate to tool controller
class EMC_AUX_MODULE: public NML_MODULE
{
 public:

  EMC_AUX_MODULE(void);
  ~EMC_AUX_MODULE(void);

  void DECISION_PROCESS(void);
  void PRE_PROCESS(void);
  void POST_PROCESS(void);

  void INIT(EMC_AUX_INIT *);
  void HALT(EMC_AUX_HALT *);
  void ABORT(EMC_AUX_ABORT *);
  void REPORT_ERROR(RCS_CMD_MSG *);

  // aux functions
  void ESTOP_ON(EMC_AUX_ESTOP_ON *);
  void ESTOP_OFF(EMC_AUX_ESTOP_OFF *);
  void DIO_WRITE(EMC_AUX_DIO_WRITE *);
  void AIO_WRITE(EMC_AUX_AIO_WRITE *);

  // status information
  EMC_AUX_STAT auxStatus;
};

// lube controller is subordinate to tool controller
class EMC_LUBE_MODULE: public NML_MODULE
{
 public:

  EMC_LUBE_MODULE(void);
  ~EMC_LUBE_MODULE(void);

  void DECISION_PROCESS(void);
  void PRE_PROCESS(void);
  void POST_PROCESS(void);

  void INIT(EMC_LUBE_INIT *);
  void HALT(EMC_LUBE_HALT *);
  void ABORT(EMC_LUBE_ABORT *);
  void REPORT_ERROR(RCS_CMD_MSG *);

  // lube functions
  void LUBE_ON(EMC_LUBE_ON *);
  void LUBE_OFF(EMC_LUBE_OFF *);

  // status information
  EMC_LUBE_STAT lubeStatus;
};

#endif // EMCIO_HH
