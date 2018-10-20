/********************************************************************
* Description: shcom.hh
*   Headers for common functions for NML calls
*
*   Derived from a work by Fred Proctor & Will Shackleford
*   Further derived from work by jmkasunich, Alex Joni
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2007 All rights reserved.
*
* Last change:
********************************************************************/

#ifndef SHCOM_HH
#define SHCOM_HH

#include "emc_nml.hh"
#include "nml_oi.hh"            // NML_ERROR_LEN

#define CLOSE(a,b,eps) ((a)-(b) < +(eps) && (a)-(b) > -(eps))
#define LINEAR_CLOSENESS 0.0001
#define ANGULAR_CLOSENESS 0.0001
#define INCH_PER_MM (1.0/25.4)
#define CM_PER_MM 0.1
#define GRAD_PER_DEG (100.0/90.0)
#define RAD_PER_DEG TO_RAD	// from posemath.h
#define DEFAULT_PATH "../../nc_files/"

#define JOGTELEOP 0
#define JOGJOINT  1

enum LINEAR_UNIT_CONVERSION {
    LINEAR_UNITS_CUSTOM = 1,
    LINEAR_UNITS_AUTO,
    LINEAR_UNITS_MM,
    LINEAR_UNITS_INCH,
    LINEAR_UNITS_CM
};
extern LINEAR_UNIT_CONVERSION linearUnitConversion;

enum ANGULAR_UNIT_CONVERSION {
    ANGULAR_UNITS_CUSTOM = 1,
    ANGULAR_UNITS_AUTO,
    ANGULAR_UNITS_DEG,
    ANGULAR_UNITS_RAD,
    ANGULAR_UNITS_GRAD
};
extern ANGULAR_UNIT_CONVERSION angularUnitConversion;

// the current command numbers, set up updateStatus(), used in main()
extern int emcCommandSerialNumber;

// the NML channels to the EMC task
extern RCS_CMD_CHANNEL *emcCommandBuffer;
extern RCS_STAT_CHANNEL *emcStatusBuffer;
// EMC_STAT *emcStatus;

// the NML channel for errors
extern NML *emcErrorBuffer;
extern char error_string[NML_ERROR_LEN];
extern char operator_text_string[NML_TEXT_LEN];
extern char operator_display_string[NML_DISPLAY_LEN];
extern char defaultPath[80]; 

// default value for timeout, 0 means wait forever
extern double emcTimeout;

enum EMC_UPDATE_TYPE {
    EMC_UPDATE_NONE = 1,
    EMC_UPDATE_AUTO
};
extern EMC_UPDATE_TYPE emcUpdateType;

enum EMC_WAIT_TYPE {
    EMC_WAIT_RECEIVED = 2,
    EMC_WAIT_DONE
};
extern EMC_WAIT_TYPE emcWaitType;

// programStartLine is the saved valued of the line that
// sendProgramRun(int line) sent
extern int programStartLine;

extern void strupr(char *s);
extern int emcTaskNmlGet();
extern int emcErrorNmlGet();
extern int tryNml(double retry_time=10.0, double retry_interval=1.0);
extern int updateStatus();
extern int updateError();
extern int emcCommandWaitReceived();
extern int emcCommandWaitDone();
extern int emcCommandSend(RCS_CMD_MSG & cmd);
extern double convertLinearUnits(double u);
extern double convertAngularUnits(double u);
extern int sendDebug(int level);
extern int sendEstop();
extern int sendEstopReset();
extern int sendMachineOn();
extern int sendMachineOff();
extern int sendManual();
extern int sendAuto();
extern int sendMdi();
extern int sendOverrideLimits(int jnum);
extern int sendJogStop(int jnum, int jjogmode);
extern int sendJogCont(int jnum, int jjogmode, double speed);
extern int sendJogIncr(int jnum, int jjogmode, double speed, double incr);
extern int sendMistOn();
extern int sendMistOff();
extern int sendFloodOn();
extern int sendFloodOff();
extern int sendLubeOn();
extern int sendLubeOff();
extern int sendSpindleForward(int spindle);
extern int sendSpindleReverse(int spindle);
extern int sendSpindleOff(int spindle);
extern int sendSpindleIncrease(int spindle);
extern int sendSpindleDecrease(int spindle);
extern int sendSpindleConstant(int spindle);
extern int sendBrakeEngage(int spindle);
extern int sendBrakeRelease(int spindle);
extern int sendAbort();
extern int sendHome(int jnum);
extern int sendUnHome(int jnum);
extern int sendFeedOverride(double override);
extern int sendRapidOverride(double override);
extern int sendMaxVelocity(double velocity);
extern int sendSpindleOverride(int spindle, double override);
extern int sendTaskPlanInit();
extern int sendProgramOpen(char *program);
extern int sendProgramRun(int line);
extern int sendProgramPause();
extern int sendProgramResume();
extern int sendSetOptionalStop(bool state);
extern int sendProgramStep();
extern int sendMdiCmd(const char *mdi);
extern int sendLoadToolTable(const char *file);
extern int sendToolSetOffset(int tool, double length, double diameter);
extern int sendJointSetBacklash(int jnum, double backlash);
extern int sendJointEnable(int joint, int val);
extern int sendJointLoadComp(int joint, const char *file, int type);
extern int sendSetTeleopEnable(int enable);
extern int sendClearProbeTrippedFlag();
extern int sendProbe(double x, double y, double z);
extern int iniLoad(const char *filename);
extern int checkStatus();

#endif				/* ifndef SHCOM_HH */
