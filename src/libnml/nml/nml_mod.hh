#ifndef NML_MODULE_H
#define NML_MODULE_H

/*
   nml_mod.h

   Declarations of NML class derived from RCS_MODULE_C, which include
   NML channels and an RCS_TIMER.
   */

#include "nml.hh"		// NML, NMLmsg
#include "stat_msg.hh"		// RCS_STAT_CHANNEL, RCS_STAT_MSG
#include "cmd_msg.hh"		// RCS_CMD_CHANNEL, RCS_CMD_MSG
#include "timer.hh"		// RCS_TIMER
#include "inifile.h"		// class INIFILE

#define STATE_MATCH (set_file_and_line(__FILE__,__LINE__)),stateMatch

enum RCS_STATE {
    UNINITIALIZED_STATE = -1,
    NEW_COMMAND = -2,
    NOP_STATE = -3,
    SE0 = -10,
    SE1 = -11,
    SE2 = -12,
    SE3 = -13,
    SE4 = -14,
    SE5 = -15,
    SE6 = -16,
    SE7 = -17,
    SE8 = -18,
    SE9 = -19,
    S0 = 0,
    S1 = 1,
    S2 = 2,
    S3 = 3,
    S4 = 4,
    S5 = 5,
    S6 = 6,
    S7 = 7,
    S8 = 8,
    S9 = 9,
    S10 = 10,
    S11 = 11,
    S12 = 12,
    S13 = 13,
    S14 = 14,
    S15 = 15,
    S16 = 16,
    S17 = 17,
    S18 = 18,
    S19 = 19,
    S20 = 20,
    S21 = 21,
    S22 = 22,
    S23 = 23,
    S24 = 24,
    S25 = 25,
    S26 = 26,
    S27 = 27,
    S28 = 28,
    S29 = 29,
    S30 = 30,
    S31 = 31,
    S32 = 32,
    S33 = 33,
    S34 = 34,
    S35 = 35,
    S36 = 36,
    S37 = 37,
    S38 = 38,
    S39 = 39
};

enum RCS_STATUS {
    UNINITIALIZED_STATUS = -1,
    RCS_DONE = 1,
    RCS_EXEC = 2,
    RCS_ERROR = 3
};

struct NML_SUBORDINATE_STRUCT {
  public:
    RCS_CMD_CHANNEL * commandOut;	// NML channels for commands to
    // subordinates
    RCS_STAT_CHANNEL *statusIn;	// NML channels for status from subordinates
    RCS_CMD_MSG *commandOutData;	// ptrs to NML data to be put in
    // channel
    RCS_STAT_MSG *statusInData;	// ptrs to NML data in channels
    int modification_number;
    char *name;
};

#if 0
class NML_MODULE_INI_INFO {
  public:
    NML_MODULE_INI_INFO(const char *inifile, const char *section);
      virtual ~ NML_MODULE_INI_INFO();
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

    long getLongInt(const char *, long def = -1);
    const char *getString(const char *, const char *def = NULL);
    double getDouble(const char *, double def = -1.0);
};
#endif

class NML_MODULE {
  public:
    // This section taken from RCS_MODULE_C
    void controller(void);

    virtual void DECISION_PROCESS(void);

    virtual void READ_COMM_BUFFERS(void);
    virtual void PRE_PROCESS();

    virtual void WRITE_COMM_BUFFERS(void);
    virtual void POST_PROCESS();

    // State table functions
    int stateMatch(char *_src_file, int source_line, int state, int conds =
	1);
    int stateMatch(int state, int conds = 1);
    void stateNext(int state);
    void stateNext(RCS_STATE state);

    void read_command_in();
    void read_subordinates_status();
    void write_status_out();
    void write_commands_to_subordinates();
    void setCmdChannel(RCS_CMD_CHANNEL *);

    void setStatChannel(RCS_STAT_CHANNEL *, RCS_STAT_MSG *);

    void setErrorLogChannel(NML *);
    int addSubordinate(RCS_CMD_CHANNEL *, RCS_STAT_CHANNEL *);
    int sendCommand(RCS_CMD_MSG *, int sub_num);
    int modifyCommand(RCS_CMD_MSG *, int sub_num);
    void setSelfCommand(RCS_CMD_MSG *);
    int force_command;

    void check_if_new_command(void);

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
    int status;

    int sup_req_num;
    int sup_req_num_echo;
    int command_num;
    int command_num_echo;

  private:
    int matched;		/* flag set when a state is matched, to
				   prevent fall-through to another state */
    int stateBegin;		/* flag set by controller() signifying that
				   stateMatch should init line number */
    char *source_file;
    int source_line;

  public:

      NML_MODULE(const char *inifile, const char *section);
      NML_MODULE();
      virtual ~ NML_MODULE();

    void zero_common_vars();

#if 0
    NML_MODULE_INI_INFO *ini;	// pointer to an area of data from which ini
    // file info is gathered.
#endif

    RCS_CMD_CHANNEL *commandIn;	// NML channel for command from supervisor
    RCS_STAT_CHANNEL *statusOut;	// NML channel for status to
    // supervisor
    NML *errorLog;		// NML channel for logging errors

    RCS_CMD_MSG *commandInData;	// ptr to NML data in channel
    RCS_STAT_MSG *statusOutData;	// ptr to NML data to be put in
    // channel

    int *commandLastNum;	// array of command nums saved before writes
    int *commandOutstanding;	// array of flags, 1 = command has been sent,
    // 0 = command has finished

    NML_SUBORDINATE_STRUCT **subs;	// pointer to array of pointers to
    // subordinates
    RCS_STAT_MSG **statusInData;	// ptrs to NML data in channels
    RCS_CMD_MSG **commandOutData;	// ptrs to NML data in channels

    RCS_TIMER *timer;		// synch timer

    int done;			// non-zero means stop calling controller()

    int setSubordinates(int number);
    int setLogInfo(const char *src, int l);
    int logError(const char *fmt, ...);
    int logText(const char *fmt, ...);
    int requestDisplay(const char *display);
    void stop_timing(void);
    void set_file_and_line(char *file, int line);

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

    void check_cycle_time_start();
    void check_cycle_time_end();
    void print_statistics();

    void loadDclock(double expiration);
    int checkDclock();

  protected:

    char *proc_name;
    char *temp_file;
    int temp_line;
    int numSubordinates;	// number of subordinates for this module
    double Dclock_expiration;
    double Dclock_start_time;
    int log_line;
    const char *log_src;

  public:

    // 1 if realloc works and we should use, 0 if we need to avoid lame NT
    // problem.
    int subs_allocated;
    static int use_realloc;

};

#define NML_MOD_LOG_ERROR setLogInfo(__FILE__,__LINE__); logError
extern int logTextToNML(NML *, const char *fmt, ...);

#endif
