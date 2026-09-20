/********************************************************************
* Description: halui.hh
*   The HAL user interface, running inside task.
*
*   This is the whole interface task sees; everything else lives in
*   halui.cc, the way ws_server.cc holds the websocket transport.
*
* License: GPL Version 2
********************************************************************/
#ifndef TASK_HALUI_HH
#define TASK_HALUI_HH

/* Create the halui HAL component and its pins. Returns 0, or -1 on
   failure. There is nothing to configure: the pins are always there, and
   cost nothing to a machine that connects none of them.

   Call this before ini_hal_init(): halcmd's "loadusr -Wn inihal" returns as
   soon as the inihal component is ready, and the HAL files -- which may
   well "net" halui pins -- run immediately afterwards. */
int haluiInit(const char *inifile);

/* Read the input pins, write the status pins. Called once per task cycle.
   Most of the work is internally rate limited to 20 ms, which was the
   standalone halui's loop period. */
void haluiUpdate(void);

/* Release the HAL component. Safe to call if haluiInit() never ran. */
void haluiExit(void);

#endif /* TASK_HALUI_HH */
