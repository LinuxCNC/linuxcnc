/********************************************************************
* Description: emclcd.cc
*   EMC interface for LCD displays
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Eric H. Johnson
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2007 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

/*******************************************************************
*
* This program implements a limited user interface on an LCD
* display and keypad, such as the MX4 series made by Matrix Orbital.
* http://www.matrixorbital.com
*
* The layout is for a 4 line by 20 character display.
*
* Supported interfaces include Serial and USB.
*
********************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "config.h"		// Standard path definitions
#include "rcs_print.hh"
#include "sockets.h"		// TCP/IP common socket functions
#include "shcom.hh"		// Common NML messaging routines

#define DEFAULT_SERVER		"localhost"
#define DEFAULT_PORT            13666
#define MAX_NAME_LENGTH         20
#define MAX_PRIORITY_LENGTH     12
#define MAX_STR_LENGTH          20

typedef enum { unmm, uncm, unInch, unEncoder } unitType;

typedef enum { rsIdle, rsPause, rsRun } runType;

typedef enum {
  hbIgnore = -1, hbOn, hbOff, hbOpen} heartbeatType;

typedef enum {
  blIgnore = -1, blOn, blOff, blToggle, blOpen, blBlink, blFlash} backlightType;

typedef enum {
  cuIgnore = -1, cuOn, cuOff, cuUnder, cuBlock} cursorType;

typedef enum {
  wtString, wtTitle, wtHbar, wtVbar, wtIcon, wtScroller, wtFrame, wtNum} widgetType;

typedef enum {
  kmExclusive, kmShared} keyModeType;

typedef enum {
  rmConnect, rmSuccess, rmHuh, rmListen, rmIgnore, rmKey, rmMenuEvent, rmUnknown} respMsgType;

typedef enum {
  meSelect, meUpdate, mePlus, meMinus, meEnter, meLeave, meUnknown} menuEventType;

typedef enum {
  ktLeftPress, ktLeftRelease, ktRightPress, ktRightRelease, ktUpPress, ktUpRelease,
  ktDownPress, ktDownRelease, ktMenuPress, ktMenuRelease, ktStartPress, ktStartRelease,
  ktPausePress, ktPauseRelease, ktStopPress, ktStopRelease, ktStepPress, ktStepRelease, 
  ktTestPress, ktTestRelease, ktHelpPress, ktHelpRelease,
  ktNextPress, ktNextRelease, ktPrevPress, ktPrevRelease, 
  ktEnterPress, ktEnterRelease, ktUnknown} keyType;

typedef enum {
  cpVersion, cpProtocol, cpLCD, cpWidth, cpHeight, cpCellWidth, cpCellHeight, cpUnknown
  } connectParmType;

typedef enum {
  jtCont, jtStep1, jtStep01, jtStep001, jtStep0001 } jogModeType;

typedef struct screenDef {
  char name[MAX_NAME_LENGTH];
  int wid, hgt;
  char priority[MAX_PRIORITY_LENGTH];
  heartbeatType heartbeat;
  backlightType backlight;
  int duration;
  int timeout;
  cursorType cursor;
  int cursor_x;
  int cursor_y;
} screenDef;

typedef struct widgetDef {
  char screenName[MAX_NAME_LENGTH];
  char widgetName[MAX_NAME_LENGTH];
  widgetType type;
  int x, y;
  char text[MAX_STR_LENGTH];
} widgetDef;

typedef struct keyDef {
  char key;
  keyModeType mode;
  keyType Action;
} keyDef;

typedef struct connectRec {
  char version[12];
  char protocol[12];
  int lcdWidth, lcdHeight;
  int cellWidth, cellHeight;
} connectRec;

char attrs[][12] = {
  "-name", "-wid", "-hgt", "-priority", "-heartbeat", "-backlight",
  "-duration", "-timeout", "-cursor", "-cursor_x", "-cursor_y"
};

char hbStrs[][6] = {
  "on", "off", "open"
};

char blStrs[][8] = {
  "on", "off", "toggle", "open", "blink", "flash"
};

char cuStrs[][6] = {
  "on", "off", "under", "block"
};

char typeStrs[][10] = {
  "string", "title", "hbar", "vbar", "icon", "scroller", "frame", "num"
};

char keyModes[][8] = {
  "shared", "excl"
};

char rspMsgs[][10] = {
  "connect", "success", "huh?", "listen", "ignore", "key", "menuevent", ""
};

char eventMsgs[][10] = {
  "select", "update", "plus", "minus", "enter", "leave", " "
};

char connectStrs[][12] = {
  "LCDproc", "protocol", "lcd", "wid", "hgt", "cellwid", "cellhgt", ""};

typedef enum {
  stStartup, stMain, stOpen, stMain2, stUnknown} screenType;

#define SCREEN_COUNT 4

screenDef screens[] = {
  { "startup",     -1, -1, "foreground", hbOff, blIgnore, -1, -1, cuIgnore, -1, -1 },
  { "main",        -1, -1, "background", hbOff, blIgnore, -1, -1, cuIgnore, -1, -1 },
  { "open",        -1, -1, "background", hbOff, blIgnore, -1, -1, cuIgnore, -1, -1 },
  { "main2",       -1, -1, "background", hbOff, blIgnore, -1, -1, cuIgnore, -1, -1 }
};

#define WIDGET_COUNT 29
widgetDef widgets[] = {
  { "startup", "su1", wtString, 1, 1, ""},                 // 0
  { "startup", "su2", wtString, 1, 2, "System Starting"},  // 1
  { "startup", "su3", wtString, 1, 3, "Please Wait"},      // 2
  { "startup", "su4", wtString, 14, 4, "V2.1.0"},          // 3

  { "main",    "m1",  wtString, 1, 2, "X "},               // 4
  { "main",    "m2",  wtString, 1, 3, "Y "},               // 5
  { "main",    "m3",  wtString, 11, 2, " Z"},              // 6
  { "main",    "m4",  wtString, 3, 2, "   0.00"},          // 7
  { "main",    "m5",  wtString, 3, 3, "   0.00"},          // 8
  { "main",    "m6",  wtString, 14, 2, "   0.00"},         // 9
  { "main",    "m7",  wtString, 15, 1, " Idle"},           // 10
  { "main",    "m8",  wtString, 1, 1, "<no program>"},     // 11
  { "main",    "m9",  wtString, 15, 3, "  100"},           // 12
  { "main",    "m10", wtString, 1, 4, "Jog "},             // 13
  { "main",    "m11", wtString, 6, 4, "Cont"},             // 14
  { "main",    "m12", wtHbar,   11, 4, "0"},               // 15

  { "open",    "o1",  wtString, 1, 1, "3D_Chips"},         // 16
  { "open",    "o2",  wtString, 1, 2, "arcspiral"},        // 17
  { "open",    "o3",  wtString, 1, 3, "cds"},              // 18
  { "open",    "o4",  wtString, 1, 4, "comp"},             // 19

  { "main2",   "ma1", wtString, 1, 1, "File: "},           // 20
  { "main2",   "ma2", wtString, 1, 2, "Speed: "},          // 21
  { "main2",   "ma3", wtString, 1, 3, "Power: "},          // 22
  { "main2",   "ma4", wtString, 1, 4, "Pieces: "},         // 23
  { "main2",   "ma5", wtString, 7, 1, "<none>"},           // 24
  { "main2",   "ma6", wtString, 8, 2, "100%"},             // 25
  { "main2",   "ma7", wtString, 8, 3, "100/100%"},         // 26
  { "main2",   "ma8", wtString, 9, 4, "  0"},              // 27
  { "main2",   "ma9", wtString, 18, 4, "Del"}              // 28
};

#define KEY_COUNT 28

keyDef keys[] = {
  { 'L', kmShared, ktLeftPress },    // Left Arrow
  { 'l', kmShared, ktLeftRelease },
  { 'R', kmShared, ktRightPress },   // Right Arrow
  { 'r', kmShared, ktRightRelease },
  { 'F', kmShared, ktUpPress },      // Up Arrow
  { 'f', kmShared, ktUpRelease },
  { 'B', kmShared, ktDownPress },    // Down Arrow
  { 'b', kmShared, ktDownRelease },
  { 'M', kmShared, ktMenuPress },    // Menu
  { 'm', kmShared, ktMenuRelease },
  { 'S', kmShared, ktStartPress },   // Start
  { 's', kmShared, ktStartRelease },
  { 'P', kmShared, ktPausePress },   // Pause / Resume
  { 'p', kmShared, ktPauseRelease },
  { 'A', kmShared, ktStopPress },    // Stop - Abort
  { 'a', kmShared, ktStopRelease },
  { 'I', kmShared, ktStepPress },    // Step - Increment
  { 'i', kmShared, ktStepRelease },
  { 'T', kmShared, ktTestPress },    // Test
  { 't', kmShared, ktTestRelease },
  { 'H', kmShared, ktHelpPress },    // Help
  { 'h', kmShared, ktHelpRelease },
  { 'U', kmShared, ktNextPress },    // Next
  { 'u', kmShared, ktNextRelease },
  { 'D', kmShared, ktPrevPress },    // Previous
  { 'd', kmShared, ktPrevRelease },
  { 'E', kmShared, ktEnterPress },   // Enter
  { 'e', kmShared, ktEnterRelease }
};

int sockfd = -1;
int err;
int len;
char *testMsg = "hello\n";
char buffer[255];
char *bufptr;
int screensInitialized = -1;
screenType curScreen = stStartup;
connectRec lcdParms;
char *delims = " \n\r";
char menu1[20] = "\0";
char menu2[20] = "\0";
char programName[25] = "<none>";
int totalSteps;
runType runStatus = rsIdle;
unitType units = unmm;
double conversion = 25.4;
float jogSpeed = 20.0;
jogModeType jogMode = jtCont;
int jogging = 0;
int feedOverride = 100;

static void thisQuit()
{
    EMC_NULL emc_null_msg;

    if (emcStatusBuffer != 0) {
	// wait until current message has been received
	emcCommandWaitReceived(emcCommandSerialNumber);
    }

    if (emcCommandBuffer != 0) {
	// send null message to reset serial number to original
	emc_null_msg.serial_number = saveEmcCommandSerialNumber;
	emcCommandBuffer->write(emc_null_msg);
    }
    // clean up NML buffers

    if (emcErrorBuffer != 0) {
	delete emcErrorBuffer;
	emcErrorBuffer = 0;
    }

    if (emcStatusBuffer != 0) {
	delete emcStatusBuffer;
	emcStatusBuffer = 0;
	emcStatus = 0;
    }

    if (emcCommandBuffer != 0) {
	delete emcCommandBuffer;
	emcCommandBuffer = 0;
    }

    exit(0);
}

static void sigQuit(int sig)
{
  sockClose(sockfd);
  thisQuit();
}

int preciseSleep(double sleepTime)
{
  struct timespec tv;
  int rval;

  tv.tv_sec = (time_t) sleepTime;
  tv.tv_nsec = (long) ((sleepTime - tv.tv_sec) * 1e+9);

  while (1) {
    rval = nanosleep(&tv, &tv);
    if (rval == 0)
      return 0;
    if (errno == EINTR)
      continue;
    else return(rval);
    }
  return(0);
}

static char *extractFileName(char *s)
{
  int i, len, start;
  static char fname[25] = "\0";

  len = strlen(s);
  start = len;
  if (start <= 0) return NULL;
  while (s[start] != '/' && (start>0)) start--;
  if (s[start] == '/') start++;
  for (i=start;i<(len-4);i++)
    fname[i-start] = s[i];
  return fname;
}

static char *widgetSetStr(int widgetNo, char *newStr, char *oldStr)
{
  if (strcmp(newStr, oldStr) == 0) return newStr;

  sockPrintf(sockfd, "widget_set %s %s %d %d {%s}\n", 
    widgets[widgetNo].screenName,
    widgets[widgetNo].widgetName,
    widgets[widgetNo].x,
    widgets[widgetNo].y,
    newStr);
  return newStr;
}

static int widgetSetHbar(int widgetNo, int newVal, int oldVal)
{
  char numBuf[12];

  if (oldVal == newVal) return newVal;

  sprintf(numBuf, "%d", newVal);
  sockPrintf(sockfd, "widget_set %s %s %d %d %s\n", 
    widgets[widgetNo].screenName,
    widgets[widgetNo].widgetName,
    widgets[widgetNo].x,
    widgets[widgetNo].y, 
    numBuf);
  return newVal;
}

static void intScreenSet(char *screen, char *attr,  int value)
{
  if (value != -1) {
    sockPrintf(sockfd, "screen_set %s %s %d\n", screen, attr, value);
    }
}

static void strScreenSet(char *screen, char *attr, char *s)
{
  if (s != "") {
    sockPrintf(sockfd, "screen_set %s %s %s\n", screen, attr, s);
    }
}

static void heartbeatScreenSet(char *screen, heartbeatType hb)
{
  if (hb != hbIgnore) {
    sockPrintf(sockfd, "screen_set %s %s %s\n", screen, attrs[4], hbStrs[hb]);
    }
}

static void backlightScreenSet(char *screen, backlightType bl)
{
  if (bl != blIgnore) {
    sockPrintf(sockfd, "screen_set %s %s %s\n", screen, attrs[5], blStrs[bl]);
    }
}

static void cursorScreenSet(char *screen, cursorType cu)
{
  if (cu != cuIgnore) {
    sockPrintf(sockfd, "screen_set %s %s %s\n", screen, attrs[8], cuStrs[cu]);
    }
}

static int createScreens()
{
  int i;

  for (i=0;i<SCREEN_COUNT;i++) {
    sockPrintf(sockfd, "screen_add %s\n", screens[i].name);
    intScreenSet(screens[i].name, attrs[1], screens[i].wid);
    intScreenSet(screens[i].name, attrs[2], screens[i].hgt);
    strScreenSet(screens[i].name, attrs[3], screens[i].priority);
    heartbeatScreenSet(screens[i].name, screens[i].heartbeat);
    backlightScreenSet(screens[i].name, screens[i].backlight);
    intScreenSet(screens[i].name, attrs[6], screens[i].duration);
    intScreenSet(screens[i].name, attrs[7], screens[i].timeout);
    cursorScreenSet(screens[i].name, screens[i].cursor);
    intScreenSet(screens[i].name, attrs[9], screens[i].cursor_x);
    intScreenSet(screens[i].name, attrs[10], screens[i].cursor_y);
    }
  return(0);
}

static int createWidgets()
{
  int i;

  for (i=0; i<WIDGET_COUNT; i++) {
    sockPrintf(sockfd, "widget_add %s %s %s\n", widgets[i].screenName,
      widgets[i].widgetName, typeStrs[widgets[i].type]);
    switch (widgets[i].type) {
      case wtString:
        widgetSetStr(i, widgets[i].text, ""); 
        break;
      case wtTitle: break;
      case wtHbar: 
        widgetSetStr(i, widgets[i].text, "");
        break;
      case wtVbar: break;
      case wtIcon: break;
      case wtScroller: break;
      case wtFrame: break;
      case wtNum: break;
      }
    }
  return(0);
}

static int createKeys()
{
  int i;

  for (i=0; i<KEY_COUNT; i++) {
    sockPrintf(sockfd, "client_add_key %s %c\n", keyModes[keys[i].mode], keys[i].key);
    }
  return(0);
}

static int loadFiles()
{
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  char namebuf[25];
  
  if ((dp = opendir(EMC2_NCFILES_DIR)) == NULL) {
    printf("Cannot open directory: %s\n", EMC2_NCFILES_DIR);
    return -1;
    }

  chdir(EMC2_NCFILES_DIR);
  while ((entry = readdir(dp)) != NULL) {
    lstat(entry->d_name, &statbuf);
    if (!S_ISDIR(statbuf.st_mode)) {
      if (strstr(entry->d_name, "ngc") != NULL) {
        strncpy(namebuf, entry->d_name, (strlen(entry->d_name)<24)?strlen(entry->d_name)-4:24);
        namebuf[strlen(entry->d_name) - 4] = '\0';
        sockPrintf(sockfd, "menu_add_item open %s action {%s}\n", namebuf, namebuf);
        sockPrintf(sockfd, "menu_set_item open %s -menu_result quit\n", namebuf);
        }
      }
    }
  return 0;
}

static void menuSetInt(char *menu, char *item, int value)
{
  sockPrintf(sockfd, "menu_set_item %s %s -value {%d}\n", menu, item, value);
}

static void menuSetMin(char *menu, char *item, int value)
{
  sockPrintf(sockfd, "menu_set_item %s %s -minvalue {%d}\n", menu, item, value);
}

static void menuSetMax(char *menu, char *item, int value)
{
  sockPrintf(sockfd, "menu_set_item %s %s -maxvalue {%d}\n", menu, item, value);
}

static int createMenus()
{
  sockSendString(sockfd, "client_set name {Main Menu}\n");
  sockSendString(sockfd, "menu_add_item {} File menu {File}\n");
  sockSendString(sockfd, "menu_add_item File open menu {Open}\n");
  sockSendString(sockfd, "menu_add_item File reload action {Reload}\n");
  sockSendString(sockfd, "menu_add_item File delete action {Delete}\n");

  sockSendString(sockfd, "menu_add_item {} View menu {View}\n");
  sockSendString(sockfd, 
    "menu_add_item View units_ring ring {Units} -strings {mm\tcm\tinch}\n");
  sockSendString(sockfd, 
    "menu_add_item View coord_ring ring {Coord} -strings {abs\trel}\n");

  sockSendString(sockfd, "menu_add_item {} Run menu {Run}\n");
  sockSendString(sockfd, "menu_add_item Run feed_slider slider {Feed Override} -mintext {0} -maxtext {125} -value {50}\n");
  sockSendString(sockfd, "menu_add_item Run laser_slider slider {Laser Override} -mintext {0} -maxtext {100} -value {50}\n");

  sockSendString(sockfd, "menu_add_item {} Jog menu {Jog}\n");
  sockSendString(sockfd, "menu_add_item Jog jog_speed numeric {Speed}\n");
  sockSendString(sockfd, "menu_add_item Jog jog_mode ring {Mode} -strings {Cont\t1.0\t0.1\t0.01\t0.001}\n");

  sockSendString(sockfd, "menu_add_item {} setup menu {Setup}\n");

  sockSendString(sockfd, "menu_add_item setup display menu {Main Display}\n");
  sockSendString(sockfd, "menu_add_item display posdisplay action {Positions}\n");
  sockSendString(sockfd, "menu_add_item display sumdisplay action {Summary}\n");
  sockSendString(sockfd, "menu_set_item display posdisplay -next _quit_\n");
  sockSendString(sockfd, "menu_set_item display sumdisplay -next _quit_\n");

  sockSendString(sockfd, "menu_add_item setup netname alpha {Network Name}\n");
  sockSendString(sockfd, "menu_add_item setup tcpip menu {TCP/IP}\n");
  sockSendString(sockfd, "menu_add_item tcpip addrtype ring {Addr Type} -strings {Auto\tManual}\n");
  sockSendString(sockfd, "menu_add_item tcpip address ip {IP Address} -v6 false -value {192.168.1.250}\n");
  sockSendString(sockfd, "menu_add_item tcpip netmask ip {Subnet mask} -v6 false -value {255.255.255.0}\n");
  sockSendString(sockfd, "menu_add_item tcpip gateway ip {Gateway} -v6 false -value {192.168.1.1}\n");
  sockSendString(sockfd, "menu_add_item tcpip dns1 ip {Primary DNS} -v6 false -value {192.168.1.1}\n");
  sockSendString(sockfd, "menu_add_item tcpip dns2 ip {Secondary DNS} -v6 false -value {0.0.0.0}\n");

  sockSendString(sockfd, "menu_add_item setup passwords menu {Set Passwords}\n");
  sockSendString(sockfd, "menu_add_item passwords connect alpha {Connect}\n");
  sockSendString(sockfd, "menu_add_item passwords enable alpha {Enable}\n");
  sockSendString(sockfd, "menu_set_item passwords connect -password_char {*}\n");
  sockSendString(sockfd, "menu_set_item passwords enable -password_char {*}\n");

  sockSendString(sockfd, "menu_add_item setup language menu {Set Language}\n");
  sockSendString(sockfd, "menu_add_item language english action {English}\n");
  sockSendString(sockfd, "menu_add_item language spanish action {Espanol}\n");
  sockSendString(sockfd, "menu_add_item language french action {Francais}\n");
  sockSendString(sockfd, "menu_add_item language german action {Deutch}\n");
  sockSendString(sockfd, "menu_add_item language italian action {Italiano}\n");

  sockSendString(sockfd, "menu_add_item {} utility menu {Utilities}\n");
  sockSendString(sockfd, "menu_add_item utility home ring {Home} -strings {all\tX\tY\tZ}\n");
  sockSendString(sockfd, "menu_add_item utility limits ring {Limits} -strings {On\tOff}\n");
  sockSendString(sockfd, "menu_add_item utility laser ring {Laser} -strings {Off\tOn}\n");

  loadFiles();
  menuSetInt("Jog", "jog_speed", 25);
  menuSetMin("Jog", "jog_speed", 0);
  menuSetMax("Jog", "jog_speed", 100);
  sockSendString(sockfd, "menu_set_main {}\n");
  return(0);
}

static screenType setNewScreen(screenType screenNo)
{
  static char *background = "background";
  static char *foreground = "foreground";

  strScreenSet(screens[curScreen].name, attrs[3], background);
  strScreenSet(screens[screenNo].name, attrs[3], foreground);
  return(screenNo);
}

static screenType lookupScreen(char *s)
{
  screenType i = stStartup;
  int temp;

  while (i < stUnknown) {
    if (strcmp(screens[i].name, s) == 0) return i;
    temp = i;
    temp++;
    i = (screenType) temp;
    }
  return i;
}

static respMsgType lookupRsp(char *s)
{
  respMsgType i = rmConnect;
  int temp;
  
  while (i < rmUnknown) {
    if (strcmp(rspMsgs[i], s) == 0) return i;
    temp = i;
    temp++;
    i = (respMsgType) temp;
    }
  return i;
}

static menuEventType lookupEvent(char *s)
{
  menuEventType i = meSelect;
  int temp;
  
  while (i < meUnknown) {
    if (strcmp(eventMsgs[i], s) == 0) return i;
    temp = i;
    temp++;
    i = (menuEventType) temp;
    }
  return i;
}

static keyType lookupKey(char key)
{
  keyType i = ktLeftPress;
  int temp;

  while (i < KEY_COUNT) {
    if (key == keys[i].key) return i;
    temp = i;
    temp++;
    i = (keyType) temp;
    }
  return i;
}

static connectParmType lookupConnect(char *s)
{
  connectParmType i = cpVersion;
  int temp;
  
  while (i < cpUnknown) {
    if (strcmp(connectStrs[i], s) == 0) return i;
    temp = i;
    temp++;
    i = (connectParmType) temp;
    }
  return i;
}

#define PROG_WIDGET1 11
#define PROG_WIDGET2 24

static int openProgram(char *s)
{
  char fname[255];

  sprintf(fname, "%s/%s.ngc", EMC2_NCFILES_DIR, s);
//  sprintf(fname, "../../nc_files/%s.ngc\n", s);
  widgetSetStr(PROG_WIDGET1, s, "");
  widgetSetStr(PROG_WIDGET2, s, "");
  if (sendTaskPlanInit() != 0) return -1;
  sendAuto();
  return sendProgramOpen(fname);
}

#define JOGMODE_WIDGET 14

static void displayJogMode(jogModeType mode)
{
  switch (mode) {
    case jtCont: widgetSetStr(JOGMODE_WIDGET, "Cont", ""); break;
    case jtStep1: 
      if (units == unmm)
        widgetSetStr(JOGMODE_WIDGET, "10.0", "");
      else
        widgetSetStr(JOGMODE_WIDGET, "1.0", "");
      break;
    case jtStep01: 
      if (units == unmm)
        widgetSetStr(JOGMODE_WIDGET, "1.0", "");
      else
        widgetSetStr(JOGMODE_WIDGET, "0.1", "");
      break;
    case jtStep001: 
      if (units == unmm)
        widgetSetStr(JOGMODE_WIDGET, "0.1", "");
      else
        widgetSetStr(JOGMODE_WIDGET, "0.01", "");
      break;
    case jtStep0001: 
      if (units == unmm)
        widgetSetStr(JOGMODE_WIDGET, "0.01", "");
      else
        widgetSetStr(JOGMODE_WIDGET, "0.001", "");
      break;
    default: widgetSetStr(JOGMODE_WIDGET, "Cont", "");
    }
}

static int selectEvent()
{
  char *pch;

  pch = strtok(NULL, delims);
  if (strcmp(menu2, "open") == 0)
    if (openProgram(pch) == -1) {
      printf("Failed to open program\n"); 
      return -1;
      }
  if (strcmp(pch, "posdisplay") == 0)
    curScreen = setNewScreen(stMain);
  if (strcmp(pch, "sumdisplay") == 0)
    curScreen = setNewScreen(stMain2);

  return 0;
}

static int updateEvent()
{
  char *pch;
  int value;

  pch = strtok(NULL, delims);
  if (strcmp(pch, "units_ring") == 0) 
    {
      value = atoi(strtok(NULL, delims));
      switch (value) {
        case 0: 
          linearUnitConversion = LINEAR_UNITS_MM; 
          units = unmm;
          conversion = 25.4;
          break;
        case 1: 
          linearUnitConversion = LINEAR_UNITS_CM; 
          units = uncm;
          conversion = 2.54;
          break;
        case 2: 
          linearUnitConversion = LINEAR_UNITS_INCH; 
          units = unInch;
          conversion = 1.0;
          break;
        }
    }
  if (strcmp(pch, "jog_mode") == 0) 
    {
      value = atoi(strtok(NULL, delims));
      switch (value) {
        case 0: jogMode = jtCont; break;
        case 1: jogMode = jtStep1; break;
        case 2: jogMode = jtStep01; break;
        case 3: jogMode = jtStep001; break;
        case 4: jogMode = jtStep0001; break;
        default: jogMode = jtCont;
        }
      displayJogMode(jogMode);
    }
  if (strcmp(pch, "jog_speed") == 0)
    {
      value = atoi(strtok(NULL, delims));
      jogSpeed = value;
    }

  return 0;
}

static int plusMinusEvent()
{
  char *pch;
  int value;

  printf("menuevent minus\n");
  pch = strtok(NULL, delims);
  if (strcmp(pch, "feed_slider") == 0) {
    value = atoi(strtok(NULL, delims));
    sendFeedOverride(((double) value) / 100.0);
    }
  if (strcmp(pch, "laser_slider") == 0) {
    value = atoi(strtok(NULL, delims));
    }

  return 0;
}

static int enterEvent()
{
  char *pch;

  pch = strtok(NULL, delims);
  strcpy(menu1, menu2);
  strcpy(menu2, pch);
  printf("menuevent enter %s\n", pch);

  return 0;
}

static int leaveEvent()
{
  char *pch;

  pch = strtok(NULL, delims);
  printf("menuevent leave %s\n", pch);

  return 0;
}

static void doMenuEvent(char *s)
{
  switch (lookupEvent(s)) {
    case meSelect: selectEvent(); break;
    case meUpdate: updateEvent(); break;
    case mePlus: plusMinusEvent(); break;
    case meMinus: plusMinusEvent(); break;
    case meEnter: enterEvent(); break;
    case meLeave: leaveEvent(); break;
    case meUnknown: ;
    }
}

static void parseConnect()
{
  char *pch;

  pch = strtok(NULL, delims);
  while (pch != NULL) {
    switch (lookupConnect(pch)) {
      case cpVersion: strcpy(lcdParms.version, strtok(NULL, delims)); break;
      case cpProtocol: strcpy(lcdParms.protocol, strtok(NULL, delims)); break;
      case cpLCD: break;
      case cpWidth: 
        pch = strtok(NULL, delims);
        sscanf(pch, "%d", &lcdParms.lcdWidth);
        break;
      case cpHeight: 
        pch = strtok(NULL, delims);
        sscanf(pch, "%d", &lcdParms.lcdHeight);
        break;
      case cpCellWidth: 
        pch = strtok(NULL, delims);
        sscanf(pch, "%d", &lcdParms.cellWidth);
        break;
      case cpCellHeight: 
        pch = strtok(NULL, delims);
        sscanf(pch, "%d", &lcdParms.cellHeight);
        break;
      case cpUnknown: printf("Unknow parameter: %s\n", pch);
      }
    pch = strtok(NULL, delims);
    }
//  sockSendString(sockfd, "info\n");
}

static int jog(int axis, float speed)
{
  float stepSize;

  if (units == unmm) stepSize = 10.0;
  else stepSize = 1.0;
  if ((axis < 0) || (axis > 5)) return -1;
  if (runStatus == rsIdle) {
    sendManual();
    switch (jogMode) {
      case jtCont: 
        if (jogging == 1) {  // toggle if driver does not support key down / key up
          jogging = 0;
          return sendJogStop(axis);
          }
        else {
          jogging = 1;
          return sendJogCont(axis, speed);
          }
      case jtStep1: return sendJogIncr(axis, speed, stepSize/conversion); break;
      case jtStep01: return sendJogIncr(axis, speed, (stepSize/10.0)/conversion); break;
      case jtStep001: return sendJogIncr(axis, speed, (stepSize/100.0)/conversion); break;
      case jtStep0001: return sendJogIncr(axis, speed, (stepSize/1000.0)/conversion); break;
      default: return -1;
      }
    }
  else return 0;
}

static int jogStop(int axis)
{
  if ((axis < 0) || (axis > 5)) return -1;
  return sendJogStop(axis);
}

static int leftKey()
{
  return jog(0, -jogSpeed);
}

static int leftKeyRelease()
{
  return jogStop(0);
}

static int rightKey()
{
  return jog(0, jogSpeed);
}

static int rightKeyRelease()
{
  return jogStop(0);
}

static int upKey()
{
  return jog(1, jogSpeed);
}

static int upKeyRelease()
{
  return jogStop(1);
}

static int downKey()
{
  return jog(1, -jogSpeed);
}

static int downKeyRelease()
{
  return jogStop(1);
}

static int startKey()
{
  if (runStatus == rsIdle)
    return sendProgramRun(0);
  else return -1;
}

static int pauseKey()
{
  if (runStatus == rsRun)
    return sendProgramPause();
  else
    if (runStatus == rsPause)
      return sendProgramResume();
    else return -1;
}

static int stopKey()
{
  return sendAbort();
}

static int stepKey()
{
  return sendProgramStep();
}

static int nextKey()
{
  int temp;

  if (curScreen == stMain) {
    temp = jogMode;
    temp++;
    temp %= 5;
    menuSetInt("Jog", "jog_mode", temp);
    jogMode = (jogModeType) temp;
    displayJogMode(jogMode);
    }
  return 0;
}

static int prevKey()
{
  int temp;

  if (curScreen == stMain) {
    temp = jogMode;
    temp--;
    if (temp < 0) temp = 4;
    menuSetInt("Jog", "jog_mode", temp);
    jogMode = (jogModeType) temp;
    displayJogMode(jogMode);
    }
  return 0;
}

static int enterKey()
{
  sendHome(0);
  sendHome(1);
  sendHome(2);
  return 0;
}

static int doKey(keyType k)
{
  switch (k) {
    case ktLeftPress:
      leftKey();
      break;
    case ktLeftRelease: 
      leftKeyRelease();
      break;
    case ktRightPress: 
      rightKey();
      break;
    case ktRightRelease: 
      rightKeyRelease();
      break;
    case ktUpPress: 
      upKey();
      break;
    case ktUpRelease: 
      upKeyRelease();
      break;
    case ktDownPress: 
      downKey();
      break;
    case ktDownRelease: 
      downKeyRelease();
      break;
    case ktMenuPress: break;
    case ktMenuRelease: break;
    case ktStartPress: 
      startKey();
      break; 
    case ktStartRelease: break;
    case ktPausePress: 
      pauseKey();
      break;
    case ktPauseRelease: break;
    case ktStopPress: 
      stopKey();
      break;
    case ktStopRelease: break;
    case ktStepPress:
      stepKey();
      break;
    case ktStepRelease: break;
    case ktTestPress: break;
    case ktTestRelease: break;
    case ktHelpPress: break;
    case ktHelpRelease: break;
    case ktNextPress: 
      nextKey();
      break;
    case ktNextRelease: break;
    case ktPrevPress: 
      prevKey();
      break;
    case ktPrevRelease: break;
    case ktEnterPress: 
      enterKey();
      break;
    case ktEnterRelease: break;
    case ktUnknown: ;
    }
  return 0;
}

static int parseLine()
{
  char *pch;

  pch = strtok(buffer, delims);
  switch (lookupRsp(pch)) {
    case rmConnect: parseConnect(); break;
    case rmSuccess: ; break;
    case rmHuh:
      pch = strtok(NULL, delims);
      printf("Error: %s\n", pch); 
      break;
    case rmListen:
      pch = strtok(NULL, delims);
      printf("listening to %s\n", pch); 
      curScreen = lookupScreen(pch);
      break;
    case rmIgnore: 
      pch = strtok(NULL, delims);
      printf("ignoring %s\n", pch); 
      break;
    case rmKey: 
      pch = strtok(NULL, delims);
      printf("key entered %s\n", pch);
      doKey(lookupKey(pch[0]));
      break;
    case rmMenuEvent: 
      pch = strtok(NULL, delims);
      printf("menuevent %s\n", pch);
      doMenuEvent(pch);
      break;
    case rmUnknown: printf("unknown command %s\n", buffer);
    }
  return 0;
}

static int startup()
{
  return sockConnect(DEFAULT_SERVER, DEFAULT_PORT);
}

static int initScreens()
{
  screensInitialized = createScreens();
  screensInitialized = createWidgets();
  screensInitialized = createKeys();
  screensInitialized = createMenus();
  setNewScreen(stMain);
  printf("Screens created.\n");
  return 0;
}

static int stepCount(char *fileName)
{
  FILE *f;
  char buffer[80];
  int len;

  sprintf(buffer, "wc -l < %s", fileName);
  f = popen(buffer, "r");
  memset(buffer, '\0', sizeof(buffer));
  if (f != NULL) {
    len = fread(buffer, sizeof(char), 79, f);
    if (len > 0) printf("result = %s\n", buffer);
    else printf("Nothing to read\n");
    }
  pclose(f);
  return atoi(buffer);
}

#define STATUSWIDGET 10
#define FEED_OVERRIDE_WIDGET 12
#define JOG_WIDGET 13

static void slowLoop()
{
  char *fname;
  char buf[8];
  static char status[8] = "";
  static int oldFeedOverride = 0;

  if (emcStatus->task.file[0] != 0) {
    fname = extractFileName(emcStatus->task.file);
    if (strcmp(fname, programName) != 0) {
      strcpy(programName, widgetSetStr(PROG_WIDGET1, fname, programName));
      strcpy(programName, widgetSetStr(PROG_WIDGET2, fname, programName));
      totalSteps = stepCount(emcStatus->task.file);
      }
    }

  feedOverride = (int)floor(emcStatus->motion.traj.scale * 100.0 + 0.5);
  if (feedOverride != oldFeedOverride) {
    menuSetInt("Run", "feed_slider", feedOverride);
    oldFeedOverride = feedOverride;
    sprintf(buf, "%5d", feedOverride);
    widgetSetStr(FEED_OVERRIDE_WIDGET, buf, "");
    }

  switch (emcStatus->task.interpState) {
      case EMC_TASK_INTERP_READING:
      case EMC_TASK_INTERP_WAITING: 
        strcpy(status, widgetSetStr(STATUSWIDGET, "  Run", status));
        if (runStatus != rsRun)
          widgetSetStr(JOG_WIDGET, "Step", "");
        runStatus = rsRun;
        break;
      case EMC_TASK_INTERP_PAUSED: 
        strcpy(status, widgetSetStr(STATUSWIDGET, "Pause", status));
        runStatus = rsPause;
        break;
      default: 
        strcpy(status, widgetSetStr(STATUSWIDGET, " Idle", status));
        if (runStatus != rsIdle) {
          widgetSetStr(JOG_WIDGET, "Jog ", "");
          displayJogMode(jogMode);
          }
        runStatus = rsIdle;
        break;
    }
}

#define XPOSWIDGET 7
#define YPOSWIDGET 8
#define ZPOSWIDGET 9
#define LINENOWIDGET 14
#define PROGRESSWIDGET 15

static void updatePositions()
{
  static char oldXStr[12] = "";
  static char oldYStr[12] = "";
  static char oldZStr[12] = "";
  char numStr[12];
  int lineNo;
  static int oldLineNo;
  int stepPct;
  static int oldPct = 0;

  if (units == unmm)
    sprintf(numStr, "%7.2f", emcStatus->motion.traj.actualPosition.tran.x * conversion);
  else
    sprintf(numStr, "%7.3f", emcStatus->motion.traj.actualPosition.tran.x * conversion);
  widgetSetStr(XPOSWIDGET, numStr, oldXStr);   
  if (units == unmm)
    sprintf(numStr, "%7.2f", emcStatus->motion.traj.actualPosition.tran.y * conversion);
  else
    sprintf(numStr, "%7.3f", emcStatus->motion.traj.actualPosition.tran.y * conversion);
  widgetSetStr(YPOSWIDGET, numStr, oldYStr);
  if (units == unmm)
    sprintf(numStr, "%6.2f", emcStatus->motion.traj.actualPosition.tran.z * conversion);
  else
    sprintf(numStr, "%6.2f", emcStatus->motion.traj.actualPosition.tran.z * conversion);
  widgetSetStr(ZPOSWIDGET, numStr, oldZStr);   

  if ((programStartLine< 0) || (emcStatus->task.readLine < programStartLine))
    lineNo = emcStatus->task.readLine;
  else
    if (emcStatus->task.currentLine > 0)
      if ((emcStatus->task.motionLine > 0) && 
        (emcStatus->task.motionLine < emcStatus->task.currentLine))
	  lineNo = emcStatus->task.motionLine;
      else lineNo = emcStatus->task.currentLine;
    else lineNo = 0;
  if (runStatus == rsIdle)
    oldPct = widgetSetHbar(PROGRESSWIDGET, 0, oldPct);
  else
    if (lineNo != oldLineNo) {
      sprintf(numStr, "%d", lineNo);
      widgetSetStr(LINENOWIDGET, numStr, "");
      oldLineNo = lineNo;

      stepPct = (int)(((double)lineNo / (double)totalSteps) * 50.0);
      oldPct = widgetSetHbar(PROGRESSWIDGET, stepPct, oldPct);
      }
}

static void fastLoop()
{
  if (emcUpdateType == EMC_UPDATE_AUTO) updateStatus();
  updatePositions();
}

static void update()
{
  static int tics = 0;

  // Wait for screens to initialize before updating
  if (screensInitialized == -1) return; 

  if ((tics % 25) == 0) fastLoop();
  if (tics == 105) {
    slowLoop();
    tics = 0;
    }
  tics++;
}

static int sockMain()
{

  sockfd = startup();
  if (sockfd == -1) {
    printf("Unable to open socket\n");
    return -1;
    }
  else { 
    sendEstopReset();
    sendMachineOn();
    sockSendString(sockfd, "hello\n");
    while ((len = sockRecvString(sockfd, buffer, sizeof(buffer) - 1)) >= 0) {
      if (len == 0) {
        preciseSleep(0.01);
        update();
        }
      else {
        parseLine();
        memset(buffer, 0, sizeof(buffer));
        if (screensInitialized == -1) 
          screensInitialized = initScreens();
        }
      } 
    }
  return 0;
}

static void initMain()
{
    emcWaitType = EMC_WAIT_RECEIVED;
    emcCommandSerialNumber = 0;
    saveEmcCommandSerialNumber = 0;
    emcTimeout = 0.0;
    emcUpdateType = EMC_UPDATE_AUTO;
    linearUnitConversion = LINEAR_UNITS_MM; 
    angularUnitConversion = ANGULAR_UNITS_AUTO;
    emcCommandBuffer = 0;
    emcStatusBuffer = 0;
    emcStatus = 0;

    emcErrorBuffer = 0;
    error_string[LINELEN] = 0;
    operator_text_string[LINELEN] = 0;
    operator_display_string[LINELEN] = 0;
    programStartLine = 0;
}

int main(int argc, char *argv[])
{
    initMain();
    // process command line args
    if (emcGetArgs(argc, argv) != 0) {
	rcs_print_error("error in argument list\n");
	exit(1);
    }
    // get configuration information
    iniLoad(EMC_INIFILE);
    // init NML
    if (tryNml() != 0) {
	rcs_print_error("can't connect to emc\n");
	thisQuit();
	exit(1);
    }
    // get current serial number, and save it for restoring when we quit
    // so as not to interfere with real operator interface
    updateStatus();
    emcCommandSerialNumber = emcStatus->echo_serial_number;
    saveEmcCommandSerialNumber = emcStatus->echo_serial_number;

    // attach our quit function to exit
//    Tcl_CreateExitHandler(thisQuit, (ClientData) 0);

    // attach our quit function to SIGINT
    signal(SIGINT, sigQuit);
    sockMain();

    // TclX_Main(argc, argv, Tcl_AppInit);

    return 0;
}
