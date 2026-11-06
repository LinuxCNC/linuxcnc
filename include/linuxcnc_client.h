/********************************************************************
* Description: linuxcnc_client.h
*   C client API for LinuxCNC's status, commands and error channel.
*
*   This header is the client interface a non-Python UI uses to talk to
*   task and motion.  It carries the same commands and the same status
*   as the Python "linuxcnc" module, so a UI or a language binding
*   written against either one maps field for field onto the other.
*
*   Nothing below the API is promised: the transport is not part of the
*   interface, and the NML channels in use today are an implementation
*   detail that may be replaced without changing this header.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2026 All rights reserved.
********************************************************************/

#ifndef LINUXCNC_CLIENT_H
#define LINUXCNC_CLIENT_H

#include <stddef.h>
#include <stdint.h>

// pi-lens-ignore: no-reserved-identifiers — __cplusplus is the standard-mandated
// macro for C++ linkage, not an identifier this header declares
#ifdef __cplusplus
extern "C" {
#endif

/* The C++ guard below uses __cplusplus, the macro the language standard
   mandates for this purpose; it is not an identifier this header declares. */
/* Compile-time array sizes.  They are the maxima in include/emcmotcfg.h and
   friends, so a status buffer never needs to be resized for a machine. */
#define LINUXCNC_MAX_JOINTS 16
#define LINUXCNC_MAX_AXES 9
#define LINUXCNC_MAX_SPINDLES 8
#define LINUXCNC_MAX_DIO 64
#define LINUXCNC_MAX_AIO 64
#define LINUXCNC_MAX_MISC_ERROR 64
#define LINUXCNC_MAX_GCODES 17
#define LINUXCNC_MAX_MCODES 10
#define LINUXCNC_MAX_SETTINGS 5
#define LINUXCNC_MAX_CALL_STACK 10
#define LINUXCNC_MAX_SWITCHKINS_TYPES 9
#define LINUXCNC_MAX_STATE_FIELDS 9
#define LINUXCNC_MAX_STATE_FLOAT_FIELDS 11

#define LINUXCNC_TEXT_LEN 255	/* LINELEN */
#define LINUXCNC_SUBNAME_LEN 64
#define LINUXCNC_FILENAME_LEN 255	/* LINELEN */
#define LINUXCNC_TAG_FILENAME_LEN 256
#define LINUXCNC_MAX_TOOLS 1001
#define LINUXCNC_ERROR_TEXT_LEN 256	/* NML_ERROR_LEN */

/* Versions of the layouts below.  A version is bumped whenever a member is
   changed or removed; a member may be appended behind the current ones
   without bumping it. */
#define LINUXCNC_STATUS_VERSION 1
#define LINUXCNC_ERROR_VERSION 1

/* EMC_TASK_MODE */
enum linuxcnc_task_mode {
    LINUXCNC_MODE_MANUAL = 1,
    LINUXCNC_MODE_AUTO = 2,
    LINUXCNC_MODE_MDI = 3
};

/* EMC_TASK_STATE */
enum linuxcnc_task_state {
    LINUXCNC_STATE_ESTOP = 1,
    LINUXCNC_STATE_ESTOP_RESET = 2,
    LINUXCNC_STATE_OFF = 3,
    LINUXCNC_STATE_ON = 4
};

/* EMC_TRAJ_MODE */
enum linuxcnc_traj_mode {
    LINUXCNC_TRAJ_MODE_FREE = 1,
    LINUXCNC_TRAJ_MODE_COORD = 2,
    LINUXCNC_TRAJ_MODE_TELEOP = 3
};

/* What the index of the jog commands refers to.  The values are the ones
   the jog messages carry. */
enum linuxcnc_jog_mode {
    LINUXCNC_JOG_AXIS = 0,
    LINUXCNC_JOG_JOINT = 1
};

/* How the machine reports the last command it was given. */
enum linuxcnc_command_status {
    LINUXCNC_COMMAND_UNINITIALIZED = -1,
    LINUXCNC_COMMAND_DONE = 1,
    LINUXCNC_COMMAND_EXEC = 2,
    LINUXCNC_COMMAND_ERROR = 3
};

/* Kind of message in struct linuxcnc_error. */
enum linuxcnc_error_kind {
    LINUXCNC_ERROR_OPERATOR_ERROR = 1,
    LINUXCNC_ERROR_OPERATOR_TEXT = 2,
    LINUXCNC_ERROR_OPERATOR_DISPLAY = 3,
    LINUXCNC_ERROR_ERROR = 4,
    LINUXCNC_ERROR_TEXT = 5,
    LINUXCNC_ERROR_DISPLAY = 6,
    LINUXCNC_ERROR_OTHER = 7
};

/* Severity of an error message.  The NML error channel does not carry one,
   so today every message is LINUXCNC_SEVERITY_UNKNOWN; a replacement that
   does will fill it in without changing this header. */
enum linuxcnc_error_severity {
    LINUXCNC_SEVERITY_UNKNOWN = 0,
    LINUXCNC_SEVERITY_INFO = 1,
    LINUXCNC_SEVERITY_WARNING = 2,
    LINUXCNC_SEVERITY_ERROR = 3,
    LINUXCNC_SEVERITY_FATAL = 4
};

/* A position: three cartesian axes, three rotary axes, three extra axes. */
struct linuxcnc_pose {
    double x, y, z;
    double a, b, c;
    double u, v, w;
};

/* One entry of the tool table, as the tool database holds it. */
struct linuxcnc_tool {
    int32_t toolno;
    int32_t pocketno;
    struct linuxcnc_pose offset;
    double diameter;
    double frontangle;
    double backangle;
    int32_t orientation;
};

/* EMC_JOINT_STAT */
struct linuxcnc_joint {
    int32_t joint;
    int32_t joint_type;		/* EMC_JOINT_LINEAR or EMC_JOINT_ANGULAR */
    double units;		/* units per mm */
    double backlash;
    double min_position_limit;
    double max_position_limit;
    double max_ferror;
    double min_ferror;
    double ferror_current;
    double ferror_high_mark;
    double output;
    double input;
    double velocity;
    int32_t inpos;
    int32_t homing;
    int32_t homed;
    int32_t fault;
    int32_t enabled;
    int32_t min_soft_limit;
    int32_t max_soft_limit;
    int32_t min_hard_limit;
    int32_t max_hard_limit;
    int32_t override_limits;
};

/* EMC_AXIS_STAT */
struct linuxcnc_axis {
    int32_t axis;
    double min_position_limit;
    double max_position_limit;
    double velocity;
};

/* EMC_SPINDLE_STAT */
struct linuxcnc_spindle {
    double speed;		/* in RPM, negative means reverse */
    double spindle_scale;	/* spindle override */
    double css_maximum;
    double css_factor;
    int32_t state;
    int32_t direction;		/* 0 stopped, 1 forward, -1 reverse */
    int32_t brake;		/* 0 released, 1 engaged */
    int32_t increasing;		/* 1 increasing, -1 decreasing, 0 neither */
    int32_t enabled;
    int32_t orient_state;
    int32_t orient_fault;
    int32_t spindle_override_enabled;
    int32_t homed;
};

/* One frame of the subroutine call stack of the move motion is running. */
struct linuxcnc_call_frame {
    char filename[LINUXCNC_FILENAME_LEN];	/* file holding the call site */
    char subname[LINUXCNC_SUBNAME_LEN];		/* subroutine called there */
    int32_t line;				/* line of the call site */
};

/* Interpreter state of the move motion is running, one per motion segment. */
struct linuxcnc_state_tag {
    double fields_float[LINUXCNC_MAX_STATE_FLOAT_FIELDS];
    int32_t fields[LINUXCNC_MAX_STATE_FIELDS];
    uint64_t packed_flags;
    uint64_t flags;
    char filename[LINUXCNC_TAG_FILENAME_LEN];
};

/* A snapshot of what task, motion and IO report.  The members are grouped
   the way EMC_STAT groups them, and carry the same meaning. */
struct linuxcnc_status {
    /* Size of this structure as the caller knows it, and the layout version
       the library fills in.  Call linuxcnc_status_init() before the first
       poll; linuxcnc_status_poll() refuses a buffer smaller than its own. */
    uint32_t size;
    uint32_t version;
    int32_t valid;		/* 0 until the first status has been taken */
    int32_t command_status;	/* enum linuxcnc_command_status */
    int32_t last_command_serial;	/* serial of the last command taken */

    /* EMC_TASK_STAT */
    int32_t task_mode;		/* enum linuxcnc_task_mode */
    int32_t task_state;		/* enum linuxcnc_task_state */
    int32_t exec_state;
    int32_t interp_state;
    int32_t call_level;
    int32_t motion_line;
    int32_t current_line;
    int32_t read_line;
    int32_t optional_stop;
    int32_t block_delete;
    int32_t input_timeout;
    int32_t task_paused;
    int32_t interpreter_errcode;
    int32_t program_units;	/* CANON_UNITS_* */
    int32_t queued_mdi_commands;
    double delay_left;
    char file[LINUXCNC_TEXT_LEN + 1];
    char command[LINUXCNC_TEXT_LEN + 1];
    char ini_filename[LINUXCNC_TEXT_LEN + 1];
    struct linuxcnc_call_frame call_stack[LINUXCNC_MAX_CALL_STACK];
    struct linuxcnc_pose g5x_offset;
    int32_t g5x_index;
    struct linuxcnc_pose g92_offset;
    double rotation_xy;
    struct linuxcnc_pose g68_offset;
    double g68_rotation[9];
    int32_t g68_active;
    struct linuxcnc_pose tool_offset;
    int32_t gcodes[LINUXCNC_MAX_GCODES];
    int32_t mcodes[LINUXCNC_MAX_MCODES];
    double settings[LINUXCNC_MAX_SETTINGS];
    uint64_t taskbeat;

    /* EMC_TRAJ_STAT */
    double linear_units;	/* units per mm */
    double angular_units;	/* units per degree */
    double cycle_time;
    int32_t joints;		/* number of joints in the status */
    int32_t spindles;		/* number of spindles in the status */
    int32_t axis_mask;
    int32_t motion_mode;	/* enum linuxcnc_traj_mode */
    int32_t enabled;
    int32_t inpos;
    int32_t queue;
    int32_t active_queue;
    int32_t queue_full;
    int32_t motion_id;
    int32_t paused;
    int32_t single_stepping;
    double feedrate;		/* feed override */
    double rapidrate;		/* rapid override */
    double velocity;
    double acceleration;
    double max_velocity;
    double max_acceleration;
    int32_t probe_tripped;
    int32_t probing;
    int32_t probe_val;
    int32_t kinematics_type;
    int32_t motion_type;
    double distance_to_go;
    double current_vel;
    int32_t feed_override_enabled;
    int32_t adaptive_feed_enabled;
    int32_t feed_hold_enabled;
    struct linuxcnc_pose position;
    struct linuxcnc_pose actual_position;
    struct linuxcnc_pose dtg;
    struct linuxcnc_pose probed_position;
    int32_t switchkins_type;
    int32_t switchkins_seq;
    int32_t switchkins_changed;
    int32_t switchkins_flags[LINUXCNC_MAX_SWITCHKINS_TYPES];
    struct linuxcnc_state_tag tag;

    /* EMC_MOTION_STAT, the part around the arrays */
    int32_t num_extra_joints;
    int32_t on_soft_limit;
    int32_t external_offsets_applied;
    int32_t jogging_active;
    int32_t homing_active;
    uint32_t homing_runs;
    uint64_t heartbeat;
    int32_t motion_debug;
    struct linuxcnc_pose eoffset_pose;
    int32_t din[LINUXCNC_MAX_DIO];
    int32_t dout[LINUXCNC_MAX_DIO];
    double ain[LINUXCNC_MAX_AIO];
    double aout[LINUXCNC_MAX_AIO];
    int32_t misc_error[LINUXCNC_MAX_MISC_ERROR];
    struct linuxcnc_joint joint[LINUXCNC_MAX_JOINTS];
    struct linuxcnc_axis axis[LINUXCNC_MAX_AXES];
    struct linuxcnc_spindle spindle[LINUXCNC_MAX_SPINDLES];

    /* EMC_IO_STAT */
    int32_t tool_in_spindle;
    int32_t pocket_prepped;
    int32_t tool_from_pocket;
    struct linuxcnc_tool tool_table_current;
    int32_t mist;
    int32_t flood;
    int32_t estop;
    int32_t io_fault;
    int32_t io_reason;
    int32_t io_debug;

    int32_t debug;		/* copy of the EMC_DEBUG global */
};

/* One message of the error channel.  Only kind and text are filled in by
   the NML transport; the rest are the fields a replacement needs so that
   error messages can be correlated with the command that caused them and
   replayed to a client that reconnects.  They are 0 ("not provided") until
   the transport can supply them, and must not be invented by the caller. */
struct linuxcnc_error {
    /* Size of this structure as the caller knows it, and the layout version
       the library fills in; see linuxcnc_error_init(). */
    uint32_t size;
    uint32_t version;
    int32_t kind;		/* enum linuxcnc_error_kind */
    int32_t severity;		/* enum linuxcnc_error_severity */
    int32_t command_serial;	/* serial of the command that caused it */
    uint64_t sequence;		/* per-machine, never reused, monotonic */
    char text[LINUXCNC_ERROR_TEXT_LEN];
};

/* An open connection to a running LinuxCNC.  One client is not safe to use
   from more than one thread at a time. */
struct linuxcnc_client;

/* Connect to the machine described by "config_file", its INI file.  The
   library reads from it whatever it needs to reach the machine; what that
   is does not appear here, and a machine that is reached another way reads
   the same argument as its endpoint.  A NULL or empty name uses the INI the
   launcher exported as INI_FILE_NAME.  On failure NULL is returned and
   "error" holds the reason. */
struct linuxcnc_client *linuxcnc_client_open(const char *config_file,
					     char *error, size_t error_len);

/* Close the connection and release the channels.  NULL is accepted. */
void linuxcnc_client_close(struct linuxcnc_client *client);

/* Why the last call on this client failed.  Never NULL; empty when the last
   call succeeded. */
const char *linuxcnc_client_last_error(const struct linuxcnc_client *client);

/* The version string of the library and of the status layout it fills. */
const char *linuxcnc_client_version(void);
uint32_t linuxcnc_client_status_version(void);

/* sizeof(struct linuxcnc_status) and sizeof(struct linuxcnc_error) as the
   library was built. */
uint32_t linuxcnc_status_size(void);
uint32_t linuxcnc_error_size(void);

/* Prepare a status buffer for polling. */
void linuxcnc_status_init(struct linuxcnc_status *status);

/* Seconds a command waits for task to take it, and the default of
   linuxcnc_wait_complete().  A negative value restores the default of 5. */
int linuxcnc_client_set_timeout(struct linuxcnc_client *client, double seconds);

/* Take the latest status.  Returns 0 on success, -1 on failure.  Every
   command below also takes the status, so "last_command_serial" tells
   whether the machine has taken the command. */
int linuxcnc_status_poll(struct linuxcnc_client *client,
			 struct linuxcnc_status *status);

/* Prepare an error buffer for polling. */
void linuxcnc_error_init(struct linuxcnc_error *error);

/* Take the next message of the error channel.  Returns 1 with the message
   filled in, 0 when the channel is empty, -1 on failure.  The NML channel
   hands a message to exactly one reader, so with two clients attached each
   message arrives at one of them only. */
int linuxcnc_error_poll(struct linuxcnc_client *client,
			struct linuxcnc_error *error);

/* Wait until task reports the last command as done or failed.  "timeout" is
   in seconds, negative selects the command timeout.  Returns 1 when it is
   done, 0 on timeout, -1 on failure. */
int linuxcnc_wait_complete(struct linuxcnc_client *client, double timeout);

/* The serial number of the last command sent, 0 before the first one. */
int linuxcnc_command_serial(const struct linuxcnc_client *client);

/* Read the tool table.  "count" receives its size; pass tools == NULL or
   capacity == 0 to only ask for it.  Returns 0 on success, -1 on failure. */
int linuxcnc_tool_table(struct linuxcnc_client *client,
			struct linuxcnc_tool *tools, size_t capacity,
			size_t *count);

/* Every command returns 0 when the machine has taken it, -1 otherwise (see
   linuxcnc_client_last_error), and fills
   linuxcnc_status::last_command_serial the next time the status is taken.
   Angles and lengths are in machine units and are not converted. */

int linuxcnc_command_debug(struct linuxcnc_client *client, int level);
int linuxcnc_command_teleop_enable(struct linuxcnc_client *client, int enable);
int linuxcnc_command_traj_mode(struct linuxcnc_client *client, int mode);

int linuxcnc_command_mode(struct linuxcnc_client *client, int mode);
int linuxcnc_command_state(struct linuxcnc_client *client, int state);
int linuxcnc_command_mdi(struct linuxcnc_client *client, const char *block);

int linuxcnc_command_program_open(struct linuxcnc_client *client,
				  const char *path);
int linuxcnc_command_program_close(struct linuxcnc_client *client);
int linuxcnc_command_auto_run(struct linuxcnc_client *client, int line);
int linuxcnc_command_auto_pause(struct linuxcnc_client *client);
int linuxcnc_command_auto_resume(struct linuxcnc_client *client);
int linuxcnc_command_auto_step(struct linuxcnc_client *client);
int linuxcnc_command_auto_reverse(struct linuxcnc_client *client);
int linuxcnc_command_auto_forward(struct linuxcnc_client *client);
int linuxcnc_command_reset_interpreter(struct linuxcnc_client *client);
int linuxcnc_command_abort(struct linuxcnc_client *client);
int linuxcnc_command_task_plan_synch(struct linuxcnc_client *client);

int linuxcnc_command_feedrate(struct linuxcnc_client *client, double scale);
int linuxcnc_command_rapidrate(struct linuxcnc_client *client, double scale);
int linuxcnc_command_maxvel(struct linuxcnc_client *client, double velocity);
int linuxcnc_command_spindle_override(struct linuxcnc_client *client,
				      double scale, int spindle);

int linuxcnc_command_spindle_on(struct linuxcnc_client *client, double speed,
				int spindle, int wait_for_speed);
int linuxcnc_command_spindle_off(struct linuxcnc_client *client, int spindle);
int linuxcnc_command_spindle_increase(struct linuxcnc_client *client,
				      int spindle);
int linuxcnc_command_spindle_decrease(struct linuxcnc_client *client,
				      int spindle);
int linuxcnc_command_spindle_constant(struct linuxcnc_client *client,
				      int spindle);
int linuxcnc_command_spindle_brake(struct linuxcnc_client *client,
				   int spindle, int engage);

int linuxcnc_command_tool_offset(struct linuxcnc_client *client, int toolno,
				 double z, double x, double diameter,
				 double frontangle, double backangle,
				 int orientation);
int linuxcnc_command_load_tool_table(struct linuxcnc_client *client,
				     const char *file);

int linuxcnc_command_mist(struct linuxcnc_client *client, int on);
int linuxcnc_command_flood(struct linuxcnc_client *client, int on);
int linuxcnc_command_override_limits(struct linuxcnc_client *client,
				     int joint);

int linuxcnc_command_home(struct linuxcnc_client *client, int joint);
int linuxcnc_command_unhome(struct linuxcnc_client *client, int joint);
int linuxcnc_command_jog_stop(struct linuxcnc_client *client, int jog_mode,
			      int index);
int linuxcnc_command_jog_continuous(struct linuxcnc_client *client,
				    int jog_mode, int index, double velocity);
int linuxcnc_command_jog_increment(struct linuxcnc_client *client,
				   int jog_mode, int index, double velocity,
				   double increment);

int linuxcnc_command_set_optional_stop(struct linuxcnc_client *client, int on);
int linuxcnc_command_set_block_delete(struct linuxcnc_client *client, int on);
int linuxcnc_command_set_min_limit(struct linuxcnc_client *client, int joint,
				   double limit);
int linuxcnc_command_set_max_limit(struct linuxcnc_client *client, int joint,
				   double limit);
int linuxcnc_command_set_feed_override(struct linuxcnc_client *client, int on);
int linuxcnc_command_set_spindle_override(struct linuxcnc_client *client,
					  int on, int spindle);
int linuxcnc_command_set_feed_hold(struct linuxcnc_client *client, int on);
int linuxcnc_command_set_adaptive_feed(struct linuxcnc_client *client, int on);
int linuxcnc_command_set_digital_output(struct linuxcnc_client *client,
					int index, int on);
int linuxcnc_command_set_analog_output(struct linuxcnc_client *client,
				       int index, double value);

int linuxcnc_command_error_msg(struct linuxcnc_client *client,
			       const char *text);
int linuxcnc_command_text_msg(struct linuxcnc_client *client,
			      const char *text);
int linuxcnc_command_display_msg(struct linuxcnc_client *client,
				 const char *text);

// pi-lens-ignore: no-reserved-identifiers — __cplusplus is the standard-mandated
// macro for C++ linkage, not an identifier this header declares
#ifdef __cplusplus
}
#endif

#endif /* LINUXCNC_CLIENT_H */
