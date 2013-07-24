//    Copyright 2004-2010, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef NML_MODULE_H
#define NML_MODULE_H

/*
   nml_mod.h

   Declarations of NML class derived from RCS_MODULE_C, which include
   NML channels and an RCS_TIMER.
   */

/*
   Modification history:

     9-Sep-1997 WPS eliminated LOAL_LOCAL_VARIABLES, UNLOAD_LOCAL_VARIABLES,
    READ_CHILD_BUFFERS, WRITE_CHILD_BUFFERS, and DETERMINE_CHILD_STATUS
   3-Apr-1997 WPS made NML_MODULE no longer the subordinate of RCS_MODULE_C
    and brought those functions, variables from RCS_MODULE_C we want in
    here directly.
    7-Mar-1997 WPS changed header files to 8.3 format.
   11-Feb-1997 WPS modified --  calc_avg_time, update_line_history, and
   stop_timing all use exec_history but fail to check to
   see if it is initialized.
   I override them here minimize changes to rcs_module.cc
   3-Dec-1996  FMP changed NML to RCS_CMD/STAT_CHANNEL for commandIn,
   statusOut, etc.
   112696  hui, changed the base class for commandInData, etc., from NMLmsg
   to RCS_CMD_MSG and RCS_STAT_MSG.
   29-Jul-1996  FMP moved NML_ERROR, NML_TEXT, NML_DISPLAY, and NML_STATUS
   into nml_emc.hh
   29-Jul-1996  FMP added NML_TEXT and NML_DISPLAY classes; added member
   functions for logText and requestDisplay
   10-Jun-1996  Fred Proctor added commandOutstanding, commandLastNum arrays;
   NML_STATUS struct
   5-Jun-1996  Fred Proctor added errorLog, logError()
   29-Apr-1996  Fred Proctor moved 'done' from RCS_MODULE_C to here.
   16-Apr-1996  Fred Proctor added NMLmsg *'s
   5-Apr-1996  Fred Proctor created
    */

#include "rcs.hh"
#include "nml.hh"		// NML, NMLmsg
#include "stat_msg.hh"		// RCS_STAT_CHANNEL, RCS_STAT_MSG
#include "cmd_msg.hh"		// RCS_CMD_CHANNEL, RCS_CMD_MSG
#include "timer.hh"		// RCS_TIMER
#include "inifile.hh"		// class INIFILE




#define STATE_MATCH (set_file_and_line(__FILE__,__LINE__)),stateMatch


struct NML_SUBORDINATE_STRUCT
{
public:
  RCS_CMD_CHANNEL * commandOut;	// NML channels for commands to subordinates
  RCS_STAT_CHANNEL *statusIn;	// NML channels for status from subordinates
  RCS_CMD_MSG *commandOutData;	// ptrs to NML data to be put in channel
  RCS_STAT_MSG *statusInData;	// ptrs to NML data in channels
  int modification_number;
  char *name;
};


/*! \todo Another #if 0 */
#if 0
class RCS_EXPORT NML_MODULE_INI_INFO
{
public:
  NML_MODULE_INI_INFO (const char *inifile, const char *section);
    virtual ~ NML_MODULE_INI_INFO ();
  INIFILE *inif;
  char nml_file[256];
  char ini_file[256];
  char ini_section[80];
  char module_name[80];
  char cmd_buf_name[80];
  char stat_buf_name[80];
  char err_buf_name[80];
  INIFILE_ENTRY entries[32];
  int num_entries;

  long getLongInt (const char *, long def = -1);
  const char *getString (const char *, const char *def = NULL);
  double getDouble (const char *, double def = -1.0);
};
#endif

class NML_MODULE
{
public:
  // This section taken from RCS_MODULE_C
  void controller (void);

  virtual void DECISION_PROCESS (void);

  virtual void READ_COMM_BUFFERS (void);
  virtual void PRE_PROCESS ();


  virtual void WRITE_COMM_BUFFERS (void);
  virtual void POST_PROCESS ();

  // State table functions
  int stateMatch (char *_src_file, int source_line, int state, int conds = 1);
  int stateMatch (int state, int conds = 1);
  void stateNext (int state);
  void stateNext (RCS_STATE state);

  void read_command_in ();
  void read_subordinates_status ();
  void write_status_out ();
  void write_commands_to_subordinates ();
  void setCmdChannel (RCS_CMD_CHANNEL *);

  void setStatChannel (RCS_STAT_CHANNEL *, RCS_STAT_MSG *);

  void setErrorLogChannel (NML *);
  int addSubordinate (RCS_CMD_CHANNEL *, RCS_STAT_CHANNEL *);
  int sendCommand (RCS_CMD_MSG *, int sub_num);
  int modifyCommand (RCS_CMD_MSG *, int sub_num);
  void setSelfCommand (RCS_CMD_MSG *);
  int force_command;

  void check_if_new_command (void);

/*! \todo Another #if 0 */
#if 0
  RCS_EXEC_HISTORY_STRUCT exec_history;	// Exec History
  RCS_EXEC_STATUS_STRUCT exec_status;

  RCS_RUN_COMMAND_STRUCT run_command;	// Run Command
  RCS_RUN_STATUS_STRUCT run_status;
#endif

  long cycle_start;		// Data
  long cycle_stop;

  int command_time_averaged;
  int new_command_sequence;
  int new_line_num_sequence;
  int new_sup_request;

  long delta_clock;
  long command_current_time;

  int pause_status;
  int command;
  int last_line;

  int execute;
  int command_time;
  RCS_STATE state;
  RCS_STATUS status;

  int sup_req_num;
  int sup_req_num_echo;
  int command_num;
  int command_num_echo;

private:
  int matched;			/* flag set when a state is matched, to
				   prevent fall-through to another state */
  int stateBegin;		/* flag set by controller() signifying
				   that stateMatch should init line number */
  char *source_file;
  int source_line;

public:

    NML_MODULE (const char *inifile, const char *section);
    NML_MODULE ();
    virtual ~ NML_MODULE ();

  void zero_common_vars ();

/*! \todo Another #if 0 */
#if 0
  NML_MODULE_INI_INFO *ini;	// pointer to an area of data from which ini file info is gathered.
#endif

  RCS_CMD_CHANNEL *commandIn;	// NML channel for command from supervisor
  RCS_STAT_CHANNEL *statusOut;	// NML channel for status to supervisor
  NML *errorLog;		// NML channel for logging errors

  RCS_CMD_MSG *commandInData;	// ptr to NML data in channel
  RCS_STAT_MSG *statusOutData;	// ptr to NML data to be put in channel

  int *commandLastNum;		// array of command nums saved before writes
  int *commandOutstanding;	// array of flags, 1 = command has been sent,
  // 0 = command has finished

  NML_SUBORDINATE_STRUCT **subs;	// pointer to array of pointers to subordinates
  RCS_STAT_MSG **statusInData;	// ptrs to NML data in channels
  RCS_CMD_MSG **commandOutData;	// ptrs to NML data in channels

  RCS_TIMER *timer;		// synch timer

  int done;			// non-zero means stop calling controller()

  int setSubordinates (int number);
  int setLogInfo (const char *src, int l);
  int logError (const char *fmt, ...) __attribute__((format(printf,2,3)));
  int logText (const char *fmt, ...) __attribute__((format(printf,2,3)));
  int requestDisplay (const char *display);
  void stop_timing (void);
  void set_file_and_line (char *file, int line);

  int commands_received;
  int commands_executed;
  int cycles;
  int cycles_executing;
  int cycles_executing_completed_commands;
  int cycles_executing_this_command;
  int last_command_completed_serial_number;
  double expected_cycle_time;
  double start_run_time;
  double last_start_run_time;
  double stop_run_time;
  double total_run_time;
  double min_run_time;
  double max_run_time;
  double start_cycle_time;
  double min_cycle_time;
  double max_cycle_time;
  double last_cycle_time;

  void check_cycle_time_start ();
  void check_cycle_time_end ();
  void print_statistics ();

  void loadDclock (double expiration);
  int checkDclock ();

protected:

  char *proc_name;
  char *temp_file;
  int temp_line;
  int numSubordinates;		// number of subordinates for this module
  double Dclock_expiration;
  double Dclock_start_time;
  int log_line;
  const char *log_src;

public:

  // 1 if realloc works and we should use, 0 if we need to avoid lame NT problem.
  int subs_allocated;
  static int use_realloc;


};

#define NML_MOD_LOG_ERROR setLogInfo(__FILE__,__LINE__); logError
extern int logTextToNML (NML *, const char *fmt, ...) __attribute__((format(printf,2,3)));

#endif
