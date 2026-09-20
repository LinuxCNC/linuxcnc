/********************************************************************
* Description: halui.cc (task)
*   The HAL user interface, running inside task.
*
*   This was the standalone halui program (src/emc/usr_intf/halui.cc, now
*   deleted) with its NML transport taken out. The pin table, the edge
*   detection in check_hal_changes() and the status mirroring in
*   modify_hal_pins() are the same code, so that behaviour -- including the
*   quirks the tests in tests/halui pin down -- is unchanged.
*
*   What is different, and why:
*
*   - No NML. Commands go into the task-internal queue (cmd_queue.hh), so
*     they pass through emcTaskPlan() exactly like a command from a UI.
*     Nothing waits for an echo: inside task, waiting for task to answer
*     is a deadlock, since it is this thread that would have to answer.
*
*   - MDI commands become a state machine, see sendMdiCommand() below.
*
*   - Status is read straight out of the live EMC_STAT. halui sees it after
*     task has finished the cycle's status update, which is the same
*     picture an NML client gets, one cycle earlier.
*
*   - The pin scan still runs at 20 ms, the standalone halui's loop period.
*     The task cycle is typically 1 ms, and jog pins send a command per
*     change, so running the whole scan every cycle would put up to a
*     thousand commands a second through task's single command slot.
*     estop-activate, machine-off, abort and program-stop are checked every
*     cycle regardless.
*
*   halui is always there: there is nothing to switch on, and a machine
*   that connects none of the pins pays for a pin scan every 20 ms.
*
* Derived from the standalone halui.cc by Alex Joni, which this replaces.
* License: GPL Version 2
********************************************************************/

#include "logutil.hh"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include <deque>

#include <hal.h>		/* access to HAL functions/definitions */
#include <rtapi.h>		/* rtapi_print_msg */
#include "rcs_status.hh"
#include <posemath.h>		// PM_POSE, TO_RAD
#include "nml_intf/emc.hh"		// EMC NML
#include "nml_intf/emc_nml.hh"
#include "nml_intf/emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "nml_intf/emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include <inifile.hh>
#include "timeutil.hh"
#include <rtapi_string.h>
#include "tooldata/tooldata.hh"
#include "usr_intf/mapini.hh"
#include "usr_intf/unitenum.hh"

#include "halui.hh"
#include "cmd_queue.hh"

using namespace linuxcnc;

/* Using halui: see the man page */

static int axis_mask = 0;
#define JOGJOINT  1
#define JOGTELEOP 0

#define MDI_MAX 64

#define HAL_FIELDS \
    FIELD(bool,machine_on) /* pin for setting machine On */ \
    FIELD(bool,machine_off) /* pin for setting machine Off */ \
    FIELD(bool,machine_is_on) /* pin for machine is On/Off */ \
    FIELD(bool,estop_activate) /* pin for activating EMC ESTOP  */ \
    FIELD(bool,estop_reset) /* pin for resetting ESTOP */ \
    FIELD(bool,estop_is_activated) /* pin for status ESTOP is activated */ \
\
    FIELD(bool,mode_manual) /* pin for requesting manual mode */ \
    FIELD(bool,mode_is_manual) /* pin for manual mode is on */ \
    FIELD(bool,mode_auto) /* pin for requesting auto mode */ \
    FIELD(bool,mode_is_auto) /* pin for auto mode is on */ \
    FIELD(bool,mode_mdi) /* pin for requesting mdi mode */ \
    FIELD(bool,mode_is_mdi) /* pin for mdi mode is on */ \
    FIELD(bool,mode_teleop) /* pin for requesting teleop mode */ \
    FIELD(bool,mode_is_teleop) /* pin for teleop mode is on */ \
    FIELD(bool,mode_joint) /* pin for requesting joint mode */ \
    FIELD(bool,mode_is_joint) /* pin for joint mode is on */ \
\
    FIELD(bool,mist_on) /* pin for starting mist */ \
    FIELD(bool,mist_off) /* pin for stopping mist */ \
    FIELD(bool,mist_is_on) /* pin for mist is on */ \
    FIELD(bool,flood_on) /* pin for starting flood */ \
    FIELD(bool,flood_off) /* pin for stopping flood */ \
    FIELD(bool,flood_is_on) /* pin for flood is on */ \
\
    FIELD(bool,program_is_idle) /* pin for notifying user that program is idle */ \
    FIELD(bool,program_is_running) /* pin for notifying user that program is running */ \
    FIELD(bool,halui_mdi_is_running) /* pin for notifying user that halui MDI commands is running */ \
    FIELD(bool,program_is_paused) /* pin for notifying user that program is paused */ \
    FIELD(bool,program_run) /* pin for running program */ \
    FIELD(bool,program_pause) /* pin for pausing program */ \
    FIELD(bool,program_resume) /* pin for resuming program */ \
    FIELD(bool,program_step) /* pin for running one line of the program */ \
    FIELD(bool,program_stop) /* pin for stopping the program */ \
    FIELD(bool,program_os_on) /* pin for setting optional stop on */ \
    FIELD(bool,program_os_off) /* pin for setting optional stop off */ \
    FIELD(bool,program_os_is_on) /* status pin that optional stop is on */ \
    FIELD(bool,program_bd_on) /* pin for setting block delete on */ \
    FIELD(bool,program_bd_off) /* pin for setting block delete off */ \
    FIELD(bool,program_bd_is_on) /* status pin that block delete is on */ \
\
    FIELD(uint,tool_number) /* pin for current selected tool */ \
    FIELD(real,tool_length_offset_x) /* current applied x tool-length-offset */ \
    FIELD(real,tool_length_offset_y) /* current applied y tool-length-offset */ \
    FIELD(real,tool_length_offset_z) /* current applied z tool-length-offset */ \
    FIELD(real,tool_length_offset_a) /* current applied a tool-length-offset */ \
    FIELD(real,tool_length_offset_b) /* current applied b tool-length-offset */ \
    FIELD(real,tool_length_offset_c) /* current applied c tool-length-offset */ \
    FIELD(real,tool_length_offset_u) /* current applied u tool-length-offset */ \
    FIELD(real,tool_length_offset_v) /* current applied v tool-length-offset */ \
    FIELD(real,tool_length_offset_w) /* current applied w tool-length-offset */ \
    FIELD(real,tool_diameter) /* current tool diameter (0 if no tool) */ \
\
    ARRAY(bool,spindle_start,EMCMOT_MAX_SPINDLES+1) /* pin for starting the spindle */ \
    ARRAY(bool,spindle_stop,EMCMOT_MAX_SPINDLES+1) /* pin for stopping the spindle */ \
    ARRAY(bool,spindle_is_on,EMCMOT_MAX_SPINDLES+1) /* status pin for spindle is on */ \
    ARRAY(bool,spindle_forward,EMCMOT_MAX_SPINDLES+1) /* pin for making the spindle go forward */ \
    ARRAY(bool,spindle_runs_forward,EMCMOT_MAX_SPINDLES+1) /* status pin for spindle running forward */ \
    ARRAY(bool,spindle_reverse,EMCMOT_MAX_SPINDLES+1) /* pin for making the spindle go reverse */ \
    ARRAY(bool,spindle_runs_backward,EMCMOT_MAX_SPINDLES+1) /* status pin for spindle running backward */ \
    ARRAY(bool,spindle_increase,EMCMOT_MAX_SPINDLES+1) /* pin for making the spindle go faster */ \
    ARRAY(bool,spindle_decrease,EMCMOT_MAX_SPINDLES+1) /* pin for making the spindle go slower */ \
\
    ARRAY(bool,spindle_brake_on,EMCMOT_MAX_SPINDLES) /* pin for activating spindle-brake */ \
    ARRAY(bool,spindle_brake_off, EMCMOT_MAX_SPINDLES) /* pin for deactivating spindle/brake */ \
    ARRAY(bool,spindle_brake_is_on, EMCMOT_MAX_SPINDLES) /* status pin that tells us if brake is on */ \
\
    ARRAY(bool,joint_home,EMCMOT_MAX_JOINTS+1) /* pin for homing one joint */ \
    ARRAY(bool,joint_unhome,EMCMOT_MAX_JOINTS+1) /* pin for unhoming one joint */ \
    ARRAY(bool,joint_is_homed,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is homed */ \
    ARRAY(bool,joint_on_soft_min_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the software min limit */ \
    ARRAY(bool,joint_on_soft_max_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the software max limit */ \
    ARRAY(bool,joint_on_hard_min_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the hardware min limit */ \
    ARRAY(bool,joint_on_hard_max_limit,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the hardware max limit */ \
    ARRAY(bool,joint_override_limits,EMCMOT_MAX_JOINTS+1) /* status pin that the joint is on the hardware max limit */ \
    ARRAY(bool,joint_has_fault,EMCMOT_MAX_JOINTS+1) /* status pin that the joint has a fault */ \
    FIELD(uint,joint_selected) /* status pin for the joint selected */ \
    FIELD(uint,axis_selected) /* status pin for the axis selected */ \
\
    ARRAY(bool,joint_nr_select,EMCMOT_MAX_JOINTS) /* nr. of pins to select a joint */ \
    ARRAY(bool,axis_nr_select,EMCMOT_MAX_AXIS) /* nr. of pins to select a axis */ \
\
    ARRAY(bool,joint_is_selected,EMCMOT_MAX_JOINTS) /* nr. of status pins for joint selected */ \
    ARRAY(bool,axis_is_selected,EMCMOT_MAX_AXIS) /* nr. of status pins for axis selected */ \
\
    ARRAY(real,axis_pos_commanded,EMCMOT_MAX_AXIS+1) /* status pin for commanded cartesian position */ \
    ARRAY(real,axis_pos_feedback,EMCMOT_MAX_AXIS+1) /* status pin for actual cartesian position */ \
    ARRAY(real,axis_pos_relative,EMCMOT_MAX_AXIS+1) /* status pin for relative cartesian position */ \
\
    FIELD(real,jjog_speed) /* pin for setting the jog speed (halui internal) */ \
    ARRAY(bool,jjog_minus,EMCMOT_MAX_JOINTS+1) /* pin to jog in positive direction */ \
    ARRAY(bool,jjog_plus,EMCMOT_MAX_JOINTS+1) /* pin to jog in negative direction */ \
    ARRAY(real,jjog_analog,EMCMOT_MAX_JOINTS+1) /* pin for analog jogging (-1..0..1) */ \
    ARRAY(real,jjog_increment,EMCMOT_MAX_JOINTS+1) /* Incremental jogging */ \
    ARRAY(bool,jjog_increment_plus,EMCMOT_MAX_JOINTS+1) /* Incremental jogging, positive direction */ \
    ARRAY(bool,jjog_increment_minus,EMCMOT_MAX_JOINTS+1) /* Incremental jogging, negative direction */ \
\
    FIELD(real,ajog_speed) /* pin for setting the jog speed (halui internal) */ \
    ARRAY(bool,ajog_minus,EMCMOT_MAX_AXIS+1) /* pin to jog in positive direction */ \
    ARRAY(bool,ajog_plus,EMCMOT_MAX_AXIS+1) /* pin to jog in negative direction */ \
    ARRAY(real,ajog_analog,EMCMOT_MAX_AXIS+1) /* pin for analog jogging (-1..0..1) */ \
    ARRAY(real,ajog_increment,EMCMOT_MAX_AXIS+1) /* Incremental jogging */ \
    ARRAY(bool,ajog_increment_plus,EMCMOT_MAX_AXIS+1) /* Incremental jogging, positive direction */ \
    ARRAY(bool,ajog_increment_minus,EMCMOT_MAX_AXIS+1) /* Incremental jogging, negative direction */ \
\
    FIELD(real,jjog_deadband) /* pin for setting the jog analog deadband (where not to move) */ \
    FIELD(real,ajog_deadband) /* pin for setting the jog analog deadband (where not to move) */ \
\
    FIELD(sint,mv_counts) /* pin for the Max Velocity counting */ \
    FIELD(bool,mv_count_enable) /* pin for the Max Velocity counting enable */ \
    FIELD(bool,mv_direct_value) /* pin for enabling direct value option instead of counts */ \
    FIELD(real,mv_scale) /* scale for the Max Velocity counting */ \
    FIELD(real,mv_value) /* current Max Velocity value */ \
    FIELD(bool,mv_increase) /* pin for increasing the MV (+=scale) */ \
    FIELD(bool,mv_decrease) /* pin for decreasing the MV (-=scale) */ \
\
    FIELD(sint,fo_counts) /* pin for the Feed Override counting */ \
    FIELD(bool,fo_count_enable) /* pin for the Feed Override counting enable */ \
    FIELD(bool,fo_direct_value) /* pin for enabling direct value option instead of counts  */ \
    FIELD(real,fo_scale) /* scale for the Feed Override counting */ \
    FIELD(real,fo_value) /* current Feed Override value */ \
    FIELD(bool,fo_increase) /* pin for increasing the FO (+=scale) */ \
    FIELD(bool,fo_decrease) /* pin for decreasing the FO (-=scale) */ \
    FIELD(bool,fo_reset) /* pin for resetting Feed Override */ \
\
    FIELD(sint,ro_counts) /* pin for the Feed Override counting */ \
    FIELD(bool,ro_count_enable) /* pin for the Feed Override counting enable */ \
    FIELD(bool,ro_direct_value) /* pin for enabling direct value option instead of counts  */ \
    FIELD(real,ro_scale) /* scale for the Feed Override counting */ \
    FIELD(real,ro_value) /* current Feed Override value */ \
    FIELD(bool,ro_increase) /* pin ror increasing the FO (+=scale) */ \
    FIELD(bool,ro_decrease) /* pin for decreasing the FO (-=scale) */ \
    FIELD(bool,ro_reset) /* pin for resetting Feed Override */ \
\
    ARRAY(sint,so_counts,EMCMOT_MAX_SPINDLES+1) /* pin for the Spindle Speed Override counting */ \
    ARRAY(bool,so_count_enable,EMCMOT_MAX_SPINDLES+1) /* pin for the Spindle Speed Override counting enable */ \
    ARRAY(bool,so_direct_value,EMCMOT_MAX_SPINDLES+1) /* pin for enabling direct value option instead of counts */ \
    ARRAY(real,so_scale,EMCMOT_MAX_SPINDLES+1) /* scale for the Spindle Speed Override counting */ \
    ARRAY(real,so_value,EMCMOT_MAX_SPINDLES+1) /* current Spindle speed Override value */ \
    ARRAY(bool,so_increase,EMCMOT_MAX_SPINDLES+1) /* pin for increasing the SO (+=scale) */ \
    ARRAY(bool,so_decrease,EMCMOT_MAX_SPINDLES+1) /* pin for decreasing the SO (-=scale) */ \
    ARRAY(bool,so_reset,EMCMOT_MAX_SPINDLES+1) /* pin for resetting Spindle Speed Override */ \
\
    FIELD(bool,home_all) /* pin for homing all joints in sequence */ \
    FIELD(bool,abort) /* pin for aborting */ \
    ARRAY(bool,mdi_commands,MDI_MAX) \
\
    FIELD(real,units_per_mm) \

struct PTR {
    template<class T>
    struct field { typedef T type; };
};

template<class T> struct NATIVE {};
template<> struct NATIVE<hal_bool_t> { typedef rtapi_bool type; };
template<> struct NATIVE<hal_sint_t> { typedef rtapi_sint type; };
template<> struct NATIVE<hal_uint_t> { typedef rtapi_uint type; };
template<> struct NATIVE<hal_real_t> { typedef rtapi_real type; };
struct VALUE {
    template<class T> struct field { typedef typename NATIVE<T>::type type; };
};

template<class T>
struct halui_str_base
{
#define FIELD(t,f) typename T::template field<hal_##t##_t>::type f;
#define ARRAY(t,f,n) typename T::template field<hal_##t##_t>::type f[n];
HAL_FIELDS
#undef FIELD
#undef ARRAY
};

typedef halui_str_base<PTR> halui_str;
typedef halui_str_base<VALUE> local_halui_str;

static halui_str *halui_data;
static local_halui_str old_halui_data;

static char *mdi_commands[MDI_MAX];
static int num_mdi_commands=0;
static int have_home_all = 0;

static int comp_id = -1;				/* HAL component ID */

static int num_axes = 0; //number of axes, taken from the INI [TRAJ] section
static int num_joints = 3; //number of joints, taken from the INI [KINS] section
static int num_spindles = 1; // number of spindles, [TRAJ]SPINDLES

static double maxFeedOverride=1;
static double maxMaxVelocity=1;
static double minSpindleOverride=0.0;
static double maxSpindleOverride=1.0;
static EMC_TASK_MODE halui_old_mode = EMC_TASK_MODE::MANUAL;
static int halui_sent_mdi = 0;


// How often the pin scan runs, the standalone halui's loop period.
static const double HALUI_PERIOD = 0.020;

static double last_pass = 0.0;

// MDI state machine, see sendMdiCommand()
static std::deque<int> mdi_pending;
static taskcmd::Ticket mdi_mode_ticket = 0;
static taskcmd::Ticket mdi_exec_ticket = 0;

// Queue a command for task to execute on one of the next cycles. This is
// where the standalone halui wrote to the NML command channel and then
// waited for task to echo the serial number back.
//
// coalesce_key is passed for jogs only: see cmd_queue.hh. Returns 0 like
// the NML send it replaces, -1 if the queue is full.
template <class T>
static int emcCommandSend(const T &cmd, unsigned long coalesce_key = 0)
{
    if (taskcmd::push(cmd, coalesce_key) == 0) {
        rtapi_print("halui: %s: command queue full, dropping a command\n", __func__);
        return -1;
    }
    return 0;
}

// One key per jog target, so that a new jog for a joint or axis replaces
// the one still waiting for it rather than queueing behind it.
static unsigned long jog_key(int ja, int jjogmode)
{
    return 1 + (unsigned long)(jjogmode ? 1 : 0) * 1024 + (unsigned long)ja;
}

static LINEAR_UNIT_CONVERSION linearUnitConversion = LINEAR_UNITS_AUTO;
static ANGULAR_UNIT_CONVERSION angularUnitConversion = ANGULAR_UNITS_AUTO;

#define CLOSE(a,b,eps) ((a)-(b) < +(eps) && (a)-(b) > -(eps))
#define LINEAR_CLOSENESS 0.0001
#define ANGULAR_CLOSENESS 0.0001
#define CM_PER_MM 0.1
#define GRAD_PER_DEG (100.0/90.0)
#define RAD_PER_DEG TO_RAD	// from posemath.h

static int halui_export_pin_IN_bool(hal_bool_t *pin, const char *name)
{
    int retval;
    retval = hal_pin_new_bool(comp_id, HAL_IN, pin, 0, "%s", name);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}

static int halui_export_pin_IN_sint(hal_sint_t *pin, const char *name)
{
    int retval;
    retval = hal_pin_new_sint(comp_id, HAL_IN, pin, 0, "%s", name);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}

static int halui_export_pin_IN_real(hal_real_t *pin, const char *name)
{
    int retval;
    retval = hal_pin_new_real(comp_id, HAL_IN, pin, 0.0, "%s", name);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}


static int halui_export_pin_OUT_bool(hal_bool_t *pin, const char *name)
{
    int retval;
    retval = hal_pin_new_bool(comp_id, HAL_OUT, pin, 0, "%s", name);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"HALUI: ERROR: halui pin %s export failed with err=%i\n", name, retval);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}


/********************************************************************
*
* Description: halui_hal_init(void)
*
* Side Effects: Exports HAL pins.
*
* Called By: main
********************************************************************/
#define CHK(x) do { \
        int rv = (x); \
        if(rv < 0) \
            return rv; \
    } while(0)

int halui_hal_init(void)
{
    /* STEP 1: initialise the hal component */
    comp_id = hal_init("halui");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HALUI: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for halui data */
    halui_data = (halui_str *) hal_malloc(sizeof(halui_str));
    if (halui_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"HALUI: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /* STEP 3a: export the out-pin(s) */

    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->units_per_mm), 0.0, "halui.machine.units-per-mm"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->machine_is_on), "halui.machine.is-on"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->estop_is_activated), "halui.estop.is-activated"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mode_is_manual), "halui.mode.is-manual"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mode_is_auto), "halui.mode.is-auto"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mode_is_mdi), "halui.mode.is-mdi"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mode_is_teleop), "halui.mode.is-teleop"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mode_is_joint), "halui.mode.is-joint"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->mist_is_on), "halui.mist.is-on"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->flood_is_on), "halui.flood.is-on"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->program_is_idle), "halui.program.is-idle"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->program_is_running), "halui.program.is-running"));
    
    if (num_mdi_commands > 0) {
        CHK(halui_export_pin_OUT_bool(&(halui_data->halui_mdi_is_running), "halui.halui-mdi-is-running"));
    }
    
    CHK(halui_export_pin_OUT_bool(&(halui_data->program_is_paused), "halui.program.is-paused"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->program_os_is_on), "halui.program.optional-stop.is-on"));
    CHK(halui_export_pin_OUT_bool(&(halui_data->program_bd_is_on), "halui.program.block-delete.is-on"));

    for (int spindle = 0; spindle < num_spindles; spindle++){
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->spindle_is_on[spindle]), 0, "halui.spindle.%i.is-on", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->spindle_runs_forward[spindle]), 0, "halui.spindle.%i.runs-forward", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->spindle_runs_backward[spindle]), 0, "halui.spindle.%i.runs-backward", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->spindle_brake_is_on[spindle]), 0, "halui.spindle.%i.brake-is-on", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_start[spindle]), 0, "halui.spindle.%i.start", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_stop[spindle]), 0, "halui.spindle.%i.stop", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_forward[spindle]), 0, "halui.spindle.%i.forward", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_reverse[spindle]), 0, "halui.spindle.%i.reverse", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_increase[spindle]), 0, "halui.spindle.%i.increase", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_decrease[spindle]), 0, "halui.spindle.%i.decrease", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_brake_on[spindle]), 0, "halui.spindle.%i.brake-on", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->spindle_brake_off[spindle]), 0, "halui.spindle.%i.brake-off", spindle));
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->so_value[spindle]), 0.0, "halui.spindle.%i.override.value", spindle));
        CHK(hal_pin_new_sint(comp_id, HAL_IN, &(halui_data->so_counts[spindle]), 0, "halui.spindle.%i.override.counts", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->so_count_enable[spindle]), 1, "halui.spindle.%i.override.count-enable", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->so_direct_value[spindle]), 0, "halui.spindle.%i.override.direct-value", spindle));
        CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->so_scale[spindle]), 0.0, "halui.spindle.%i.override.scale", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->so_increase[spindle]), 0, "halui.spindle.%i.override.increase", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->so_decrease[spindle]), 0, "halui.spindle.%i.override.decrease", spindle));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->so_reset[spindle]), 0, "halui.spindle.%i.override.reset", spindle));
    }

    for (int joint = 0; joint < num_joints ; joint++) {
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_is_homed[joint]), 0, "halui.joint.%d.is-homed", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_is_selected[joint]), 0, "halui.joint.%d.is-selected", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_soft_min_limit[joint]), 0, "halui.joint.%d.on-soft-min-limit", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_soft_max_limit[joint]), 0, "halui.joint.%d.on-soft-max-limit", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_hard_min_limit[joint]), 0, "halui.joint.%d.on-hard-min-limit", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_hard_max_limit[joint]), 0, "halui.joint.%d.on-hard-max-limit", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_override_limits[joint]), 0, "halui.joint.%d.override-limits", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_has_fault[joint]), 0, "halui.joint.%d.has-fault", joint));
    }

    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_soft_min_limit[num_joints]), 0, "halui.joint.selected.on-soft-min-limit"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_soft_max_limit[num_joints]), 0, "halui.joint.selected.on-soft-max-limit"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_hard_min_limit[num_joints]), 0, "halui.joint.selected.on-hard-min-limit"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_on_hard_max_limit[num_joints]), 0, "halui.joint.selected.on-hard-max-limit"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_override_limits[num_joints]), 0, "halui.joint.selected.override-limits"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_has_fault[num_joints]), 0, "halui.joint.selected.has-fault"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->joint_is_homed[num_joints]), 0, "halui.joint.selected.is-homed"));

    bool first_axis = true;
    for (int axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        if ( !(axis_mask & (1 << axis_num)) ) { continue; }
        char c = "xyzabcuvw"[axis_num];
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &(halui_data->axis_is_selected[axis_num]), 0, "halui.axis.%c.is-selected", c));
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->axis_pos_commanded[axis_num]), 0.0, "halui.axis.%c.pos-commanded", c));
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->axis_pos_feedback[axis_num]), 0.0, "halui.axis.%c.pos-feedback", c));
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->axis_pos_relative[axis_num]), 0.0, "halui.axis.%c.pos-relative", c));
        if (first_axis) {
            // at startup, indicate first item is selected:
            hal_set_bool(halui_data->joint_is_selected[0], 1);
            hal_set_bool(halui_data->axis_is_selected[axis_num], 1);
        }
        first_axis = false;
    }

    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->mv_value), 0.0, "halui.max-velocity.value"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->fo_value), 0.0, "halui.feed-override.value"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->ro_value), 0.0, "halui.rapid-override.value"));
    CHK(hal_pin_new_uint(comp_id, HAL_OUT, &(halui_data->joint_selected), 0, "halui.joint.selected"));
    CHK(hal_pin_new_uint(comp_id, HAL_OUT, &(halui_data->axis_selected), 0, "halui.axis.selected"));
    CHK(hal_pin_new_uint(comp_id, HAL_OUT, &(halui_data->tool_number), 0, "halui.tool.number"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_x), 0.0, "halui.tool.length_offset.x"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_y), 0.0, "halui.tool.length_offset.y"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_z), 0.0, "halui.tool.length_offset.z"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_a), 0.0, "halui.tool.length_offset.a"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_b), 0.0, "halui.tool.length_offset.b"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_c), 0.0, "halui.tool.length_offset.c"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_u), 0.0, "halui.tool.length_offset.u"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_v), 0.0, "halui.tool.length_offset.v"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_length_offset_w), 0.0, "halui.tool.length_offset.w"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &(halui_data->tool_diameter), 0.0, "halui.tool.diameter"));

    /* STEP 3b: export the in-pin(s) */

    CHK(halui_export_pin_IN_bool(&(halui_data->machine_on), "halui.machine.on"));
    CHK(halui_export_pin_IN_bool(&(halui_data->machine_off), "halui.machine.off"));
    CHK(halui_export_pin_IN_bool(&(halui_data->estop_activate), "halui.estop.activate"));
    CHK(halui_export_pin_IN_bool(&(halui_data->estop_reset), "halui.estop.reset"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mode_manual), "halui.mode.manual"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mode_auto), "halui.mode.auto"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mode_mdi), "halui.mode.mdi"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mode_teleop), "halui.mode.teleop"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mode_joint), "halui.mode.joint"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mist_on), "halui.mist.on"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mist_off), "halui.mist.off"));
    CHK(halui_export_pin_IN_bool(&(halui_data->flood_on), "halui.flood.on"));
    CHK(halui_export_pin_IN_bool(&(halui_data->flood_off), "halui.flood.off"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_run), "halui.program.run"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_pause), "halui.program.pause"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_resume), "halui.program.resume"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_step), "halui.program.step"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_stop), "halui.program.stop"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_os_on), "halui.program.optional-stop.on"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_os_off), "halui.program.optional-stop.off"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_bd_on), "halui.program.block-delete.on"));
    CHK(halui_export_pin_IN_bool(&(halui_data->program_bd_off), "halui.program.block-delete.off"));

    CHK(halui_export_pin_IN_sint(&(halui_data->mv_counts), "halui.max-velocity.counts"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mv_count_enable), "halui.max-velocity.count-enable"));
    hal_set_bool(halui_data->mv_count_enable, 1);
    CHK(halui_export_pin_IN_bool(&(halui_data->mv_direct_value), "halui.max-velocity.direct-value"));
    CHK(halui_export_pin_IN_real(&(halui_data->mv_scale), "halui.max-velocity.scale"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mv_increase), "halui.max-velocity.increase"));
    CHK(halui_export_pin_IN_bool(&(halui_data->mv_decrease), "halui.max-velocity.decrease"));

    CHK(halui_export_pin_IN_sint(&(halui_data->fo_counts), "halui.feed-override.counts"));
    CHK(halui_export_pin_IN_bool(&(halui_data->fo_count_enable), "halui.feed-override.count-enable"));
    hal_set_bool(halui_data->fo_count_enable, 1);
    CHK(halui_export_pin_IN_bool(&(halui_data->fo_direct_value), "halui.feed-override.direct-value"));
    CHK(halui_export_pin_IN_real(&(halui_data->fo_scale), "halui.feed-override.scale"));
    CHK(halui_export_pin_IN_bool(&(halui_data->fo_increase), "halui.feed-override.increase"));
    CHK(halui_export_pin_IN_bool(&(halui_data->fo_decrease), "halui.feed-override.decrease"));
    CHK(halui_export_pin_IN_bool(&(halui_data->fo_reset), "halui.feed-override.reset"));

    CHK(halui_export_pin_IN_sint(&(halui_data->ro_counts), "halui.rapid-override.counts"));
    CHK(halui_export_pin_IN_bool(&(halui_data->ro_count_enable), "halui.rapid-override.count-enable"));
    hal_set_bool(halui_data->ro_count_enable, 1);
    CHK(halui_export_pin_IN_bool(&(halui_data->ro_direct_value), "halui.rapid-override.direct-value"));
    CHK(halui_export_pin_IN_real(&(halui_data->ro_scale), "halui.rapid-override.scale"));
    CHK(halui_export_pin_IN_bool(&(halui_data->ro_increase), "halui.rapid-override.increase"));
    CHK(halui_export_pin_IN_bool(&(halui_data->ro_decrease), "halui.rapid-override.decrease"));
    CHK(halui_export_pin_IN_bool(&(halui_data->ro_reset), "halui.rapid-override.reset"));

    if (have_home_all) {
        CHK(halui_export_pin_IN_bool(&(halui_data->home_all), "halui.home-all"));
    }

    CHK(halui_export_pin_IN_bool(&(halui_data->abort), "halui.abort"));

    for (int joint = 0; joint < num_joints ; joint++) {
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->joint_home[joint]), 0, "halui.joint.%d.home", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->joint_unhome[joint]), 0, "halui.joint.%d.unhome", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->joint_nr_select[joint]), 0, "halui.joint.%d.select", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_plus[joint]), 0, "halui.joint.%d.plus", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_minus[joint]), 0, "halui.joint.%d.minus", joint));
        CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->jjog_analog[joint]), 0.0, "halui.joint.%d.analog", joint));
        CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->jjog_increment[joint]), 0.0, "halui.joint.%d.increment", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_increment_plus[joint]), 0, "halui.joint.%d.increment-plus", joint));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_increment_minus[joint]), 0, "halui.joint.%d.increment-minus", joint));
    }

    for (int axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        char c = "xyzabcuvw"[axis_num];
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->axis_nr_select[axis_num]), 0, "halui.axis.%c.select", c));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_plus[axis_num]), 0, "halui.axis.%c.plus", c));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_minus[axis_num]), 0, "halui.axis.%c.minus", c));
        CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->ajog_analog[axis_num]), 0.0, "halui.axis.%c.analog", c));
        CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->ajog_increment[axis_num]), 0.0, "halui.axis.%c.increment", c));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_increment_plus[axis_num]), 0, "halui.axis.%c.increment-plus", c));
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_increment_minus[axis_num]), 0, "halui.axis.%c.increment-minus", c));
    }

    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->joint_home[num_joints]), 0, "halui.joint.selected.home"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->joint_unhome[num_joints]), 0, "halui.joint.selected.unhome"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_plus[num_joints]), 0, "halui.joint.selected.plus"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_minus[num_joints]), 0, "halui.joint.selected.minus"));
    CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->jjog_increment[num_joints]), 0.0, "halui.joint.selected.increment"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_increment_plus[num_joints]), 0, "halui.joint.selected.increment-plus"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->jjog_increment_minus[num_joints]), 0, "halui.joint.selected.increment-minus"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_plus[EMCMOT_MAX_AXIS]), 0, "halui.axis.selected.plus"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_minus[EMCMOT_MAX_AXIS]), 0, "halui.axis.selected.minus"));
    CHK(hal_pin_new_real(comp_id, HAL_IN, &(halui_data->ajog_increment[EMCMOT_MAX_AXIS]), 0.0, "halui.axis.selected.increment"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_increment_plus[EMCMOT_MAX_AXIS]), 0, "halui.axis.selected.increment-plus"));
    CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->ajog_increment_minus[EMCMOT_MAX_AXIS]), 0, "halui.axis.selected.increment-minus"));

    CHK(halui_export_pin_IN_real(&(halui_data->jjog_speed), "halui.joint.jog-speed"));
    CHK(halui_export_pin_IN_real(&(halui_data->jjog_deadband), "halui.joint.jog-deadband"));

    CHK(halui_export_pin_IN_real(&(halui_data->ajog_speed), "halui.axis.jog-speed"));
    CHK(halui_export_pin_IN_real(&(halui_data->ajog_deadband), "halui.axis.jog-deadband"));

    for (int n = 0; n < num_mdi_commands; n++) {
        CHK(hal_pin_new_bool(comp_id, HAL_IN, &(halui_data->mdi_commands[n]), 0, "halui.mdi-command-%02d", n));
    }

    hal_ready(comp_id);
    return 0;
}

static int sendMachineOn()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ON;
    return emcCommandSend(state_msg);
}

static int sendMachineOff()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::OFF;
    return emcCommandSend(state_msg);
}

static int sendEstop()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ESTOP;
    return emcCommandSend(state_msg);
}

static int sendEstopReset()
{
    EMC_TASK_SET_STATE state_msg;

    state_msg.state = EMC_TASK_STATE::ESTOP_RESET;
    return emcCommandSend(state_msg);
}

static int sendManual()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE::MANUAL) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE::MANUAL;
    return emcCommandSend(mode_msg);
}

static int sendAuto()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE::AUTO) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE::AUTO;
    return emcCommandSend(mode_msg);
}

static int sendMdi()
{
    EMC_TASK_SET_MODE mode_msg;

    if (emcStatus->task.mode == EMC_TASK_MODE::MDI) {
        return 0;
    }

    mode_msg.mode = EMC_TASK_MODE::MDI;
    return emcCommandSend(mode_msg);
}

// Start an [HALUI]MDI_COMMAND. The standalone halui could block here: it
// sent the switch to MDI, waited for task to echo it, checked the mode and
// only then sent the command itself. Inside task nothing may block, so the
// sequence becomes a state machine, driven by service_mdi() below:
//
//   press -> queue SET_MODE(MDI) -> task reaches MDI -> queue EXECUTE
//
// What the pins do is unchanged, including the two known quirks the tests
// pin down: halui-mdi-is-running goes true at the press, before task has
// accepted anything, and a command task refuses leaves it stuck until the
// next command finishes DONE.
static void sendMdiCommand(int n)
{
    if (!halui_sent_mdi) {
        // There is currently no MDI command from halui executing, we're
        // currently starting the first one.  Record what the Task mode is,
        // so we can restore it when all the MDI commands finish.
        halui_old_mode = emcStatus->task.mode;
    }

    halui_sent_mdi = 1;

    if (num_mdi_commands > 0) {
        hal_set_bool(halui_data->halui_mdi_is_running, halui_sent_mdi);
    }

    mdi_pending.push_back(n);

    // switch to MDI mode if needed
    if (emcStatus->task.mode != EMC_TASK_MODE::MDI && mdi_mode_ticket == 0) {
        EMC_TASK_SET_MODE mode_msg;
        mode_msg.mode = EMC_TASK_MODE::MDI;
        mdi_mode_ticket = taskcmd::push(mode_msg);
    }
}

// Issue the MDI commands waiting for task to be in MDI mode, or give up on
// them if task did not go there. Runs every task cycle.
static void service_mdi()
{
    if (mdi_pending.empty()) {
        return;
    }

    if (emcStatus->task.mode == EMC_TASK_MODE::MDI) {
        mdi_mode_ticket = 0;
        while (!mdi_pending.empty()) {
            EMC_TASK_PLAN_EXECUTE emc_task_plan_execute_msg;
            rtapi_strxcpy(emc_task_plan_execute_msg.command,
                          mdi_commands[mdi_pending.front()]);
            mdi_exec_ticket = taskcmd::push(emc_task_plan_execute_msg);
            mdi_pending.pop_front();
        }
        return;
    }

    if (!taskcmd::pending(mdi_mode_ticket)) {
        // Task has been through the mode change and is not in MDI, so it
        // refused it -- while a program runs, for instance. Drop the
        // commands, exactly as the standalone halui did when its mode
        // check failed. halui_sent_mdi deliberately stays set: the pins
        // recover on the next command that ends DONE.
        rtapi_print("halui: %s: could not switch task to mdi, dropping %d command(s)\n",
                    __func__, (int)mdi_pending.size());
        mdi_pending.clear();
        mdi_mode_ticket = 0;
    }
}

// True once the MDI command halui started has actually been issued by task
// (not merely queued), which is what the standalone halui knew by waiting
// for the echo before it returned from sendMdiCommand().
static bool mdi_settled()
{
    return halui_sent_mdi
        && mdi_pending.empty()
        && !taskcmd::pending(mdi_mode_ticket)
        && !taskcmd::pending(mdi_exec_ticket);
}

// The standalone halui waited here for task to finish the mode change,
// up to 60 seconds. Inside task that wait is the deadlock it sounds like:
// task cannot finish anything while halui holds the loop.
static int sendTeleop()
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = 1;
    return emcCommandSend(emc_set_teleop_enable_msg);
}

static int sendJoint()
{
    EMC_TRAJ_SET_TELEOP_ENABLE emc_set_teleop_enable_msg;

    emc_set_teleop_enable_msg.enable = 0;
    return emcCommandSend(emc_set_teleop_enable_msg);
}

static int sendMistOn()
{
    EMC_COOLANT_MIST_ON emc_coolant_mist_on_msg;

    return emcCommandSend(emc_coolant_mist_on_msg);
}

static int sendMistOff()
{
    EMC_COOLANT_MIST_OFF emc_coolant_mist_off_msg;

    return emcCommandSend(emc_coolant_mist_off_msg);
}

static int sendFloodOn()
{
    EMC_COOLANT_FLOOD_ON emc_coolant_flood_on_msg;

    return emcCommandSend(emc_coolant_flood_on_msg);
}

static int sendFloodOff()
{
    EMC_COOLANT_FLOOD_OFF emc_coolant_flood_off_msg;

    return emcCommandSend(emc_coolant_flood_off_msg);
}

// programStartLine is the saved valued of the line that
// sendProgramRun(int line) sent
static int programStartLine = 0;

static int sendProgramRun(int line)
{
    EMC_TASK_PLAN_RUN emc_task_plan_run_msg;

    if (0 == emcStatus->task.file[0]) {
	return -1; // no program open
    }
    // save the start line, to compare against active line later
    programStartLine = line;

    emc_task_plan_run_msg.line = line;
    sendAuto();
    return emcCommandSend(emc_task_plan_run_msg);
}

static int sendProgramPause()
{
    EMC_TASK_PLAN_PAUSE emc_task_plan_pause_msg;

    return emcCommandSend(emc_task_plan_pause_msg);
}

static int sendSetOptionalStop(bool state)
{
    EMC_TASK_PLAN_SET_OPTIONAL_STOP emc_task_plan_set_optional_stop_msg;

    emc_task_plan_set_optional_stop_msg.state = state;
    return emcCommandSend(emc_task_plan_set_optional_stop_msg);
}

static int sendSetBlockDelete(bool state)
{
    EMC_TASK_PLAN_SET_BLOCK_DELETE emc_task_plan_set_block_delete_msg;

    emc_task_plan_set_block_delete_msg.state = state;
    return emcCommandSend(emc_task_plan_set_block_delete_msg);
}


static int sendProgramResume()
{
    EMC_TASK_PLAN_RESUME emc_task_plan_resume_msg;

    return emcCommandSend(emc_task_plan_resume_msg);
}

static int sendProgramStep()
{
    EMC_TASK_PLAN_STEP emc_task_plan_step_msg;

    return emcCommandSend(emc_task_plan_step_msg);
}

static int sendSpindleForward(int spindle)
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    emc_spindle_on_msg.spindle = spindle;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed = fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = +1;
    }
    return emcCommandSend(emc_spindle_on_msg);
}

static int sendSpindleReverse(int spindle)
{
    EMC_SPINDLE_ON emc_spindle_on_msg;
    emc_spindle_on_msg.spindle = spindle;
    if (emcStatus->task.activeSettings[2] != 0) {
	emc_spindle_on_msg.speed =
	    -1 * fabs(emcStatus->task.activeSettings[2]);
    } else {
	emc_spindle_on_msg.speed = -1;
    }
    return emcCommandSend(emc_spindle_on_msg);
}

static int sendSpindleOff(int spindle)
{
    EMC_SPINDLE_OFF emc_spindle_off_msg;
    emc_spindle_off_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_off_msg);
}

static int sendSpindleIncrease(int spindle)
{
    EMC_SPINDLE_INCREASE emc_spindle_increase_msg;
    emc_spindle_increase_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_increase_msg);
}

static int sendSpindleDecrease(int spindle)
{
    EMC_SPINDLE_DECREASE emc_spindle_decrease_msg;
    emc_spindle_decrease_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_decrease_msg);
}

static int sendSpindleConstant(int spindle)
{
    EMC_SPINDLE_CONSTANT emc_spindle_constant_msg;
    emc_spindle_constant_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_constant_msg);
}

static int sendBrakeEngage(int spindle)
{
    EMC_SPINDLE_BRAKE_ENGAGE emc_spindle_brake_engage_msg;
    emc_spindle_brake_engage_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_brake_engage_msg);
}

static int sendBrakeRelease(int spindle)
{
    EMC_SPINDLE_BRAKE_RELEASE emc_spindle_brake_release_msg;
    emc_spindle_brake_release_msg.spindle = spindle;
    return emcCommandSend(emc_spindle_brake_release_msg);
}

static int sendHome(int joint)
{
    EMC_JOINT_HOME emc_joint_home_msg;

    emc_joint_home_msg.joint = joint;
    return emcCommandSend(emc_joint_home_msg);
}

static int sendUnhome(int joint)
{
    EMC_JOINT_UNHOME emc_joint_unhome_msg;

    emc_joint_unhome_msg.joint = joint;
    return emcCommandSend(emc_joint_unhome_msg);
}

static int sendAbort()
{
    EMC_TASK_ABORT task_abort_msg;

    return emcCommandSend(task_abort_msg);
}


static void sendJogStop(int ja, int jjogmode)
{
    EMC_JOG_STOP emc_jog_stop_msg;

    if (   ( (jjogmode == JOGJOINT) && (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || ( (jjogmode == JOGTELEOP ) && (emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return;
    }

    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) { rtapi_print("halui: unexpected_1 %d\n",ja); return; }
    if ( !jjogmode &&  (ja < 0))                     { rtapi_print("halui: unexpected_2 %d\n",ja); return; }
    if ( !jjogmode && !(axis_mask & (1 << ja)) )     { rtapi_print("halui: unexpected_3 %d\n",ja); return; }

    emc_jog_stop_msg.jjogmode = jjogmode;
    emc_jog_stop_msg.joint_or_axis = ja;
    emcCommandSend(emc_jog_stop_msg, jog_key(ja, jjogmode));
}


static void sendJogCont(int ja, double speed, int jjogmode)
{
    EMC_JOG_CONT emc_jog_cont_msg;

    if (emcStatus->task.state != EMC_TASK_STATE::ON) { return; }
    if (   ( (jjogmode == JOGJOINT) && (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || ( (jjogmode == JOGTELEOP ) && (emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return;
    }

    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) { rtapi_print("halui: unexpected_4 %d\n",ja); return; }
    if ( !jjogmode &&  (ja < 0))                     { rtapi_print("halui: unexpected_5 %d\n",ja); return; }
    if ( !jjogmode && !(axis_mask & (1 << ja)) )     { rtapi_print("halui: unexpected_6 %d\n",ja); return; }

    emc_jog_cont_msg.jjogmode = jjogmode;
    emc_jog_cont_msg.joint_or_axis = ja;
    emc_jog_cont_msg.vel = speed / 60.0;

    emcCommandSend(emc_jog_cont_msg, jog_key(ja, jjogmode));
}

static void sendJogIncr(int ja, double speed, double incr, int jjogmode)
{
    EMC_JOG_INCR emc_jog_incr_msg;

    if (emcStatus->task.state != EMC_TASK_STATE::ON) { return; }
    if (   ( (jjogmode == JOGJOINT) && (emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP) )
        || ( (jjogmode == JOGTELEOP ) && (emcStatus->motion.traj.mode != EMC_TRAJ_MODE::TELEOP) )
       ) {
       return;
    }

    if (  jjogmode &&  (ja < 0 || ja >= num_joints)) { rtapi_print("halui: unexpected_7 %d\n",ja); return; }
    if ( !jjogmode &&  (ja < 0))                     { rtapi_print("halui: unexpected_8 %d\n",ja); return; }
    if ( !jjogmode && !(axis_mask & (1 << ja)) )     { rtapi_print("halui: unexpected_9 %d\n",ja); return; }

    emc_jog_incr_msg.jjogmode = jjogmode;
    emc_jog_incr_msg.joint_or_axis = ja;
    emc_jog_incr_msg.vel = speed / 60.0;
    emc_jog_incr_msg.incr = incr;

    emcCommandSend(emc_jog_incr_msg);
}

static int sendFeedOverride(double override)
{
    EMC_TRAJ_SET_SCALE emc_traj_set_scale_msg;

    if (override < 0.0) {
	override = 0.0;
    }

    if (override > maxFeedOverride) {
	override = maxFeedOverride;
    }

    emc_traj_set_scale_msg.scale = override;
    return emcCommandSend(emc_traj_set_scale_msg);
}

static int sendRapidOverride(double override)
{
    EMC_TRAJ_SET_RAPID_SCALE emc_traj_set_scale_msg;

    if (override < 0.0) {
	override = 0.0;
    }

    if (override > 1.0) {
	override = 1.0;
    }

    emc_traj_set_scale_msg.scale = override;
    return emcCommandSend(emc_traj_set_scale_msg);
}

static int sendMaxVelocity(double velocity)
{
    EMC_TRAJ_SET_MAX_VELOCITY mv;

    if (velocity < 0.0) {
        velocity = 0.0;
    }

    if (velocity > maxMaxVelocity) {
        velocity = maxMaxVelocity;
    }

    mv.velocity = velocity;
    return emcCommandSend(mv);
}

static int sendSpindleOverride(int spindle, double override)
{
    EMC_TRAJ_SET_SPINDLE_SCALE emc_traj_set_spindle_scale_msg;

    if (override < minSpindleOverride) {
	override = minSpindleOverride;
    }

    if (override > maxSpindleOverride) {
	override = maxSpindleOverride;
    }

    emc_traj_set_spindle_scale_msg.spindle = spindle;
    emc_traj_set_spindle_scale_msg.scale = override;
    return emcCommandSend(emc_traj_set_spindle_scale_msg);
}

static int haluiIniLoad(const char *filename)
{
    IniFile inifile(filename);

    if (!inifile) {
	return -1;
    }

    if (auto inival = inifile.findReal("MAX_FEED_OVERRIDE", "DISPLAY")) {
        if (*inival > 0.0) {
            maxFeedOverride =  *inival;
        }
    }

    if(!inifile.isSet("MAX_LINEAR_VELOCITY", "TRAJ") && !inifile.isSet("MAX_VELOCITY", "AXIS_X"))
        maxMaxVelocity = 1.0;

    if (auto inival = inifile.findReal("MIN_SPINDLE_OVERRIDE", "DISPLAY")) {
        if (*inival > 0.0) {
            minSpindleOverride = *inival;
        }
    }

    if (auto inival = inifile.findReal("MAX_SPINDLE_OVERRIDE", "DISPLAY")) {
        if (*inival > 0.0) {
            maxSpindleOverride = *inival;
        }
    }

    num_axes = 0;
    axis_mask = 0;
    if (auto coord = inifile.findString("COORDINATES", "TRAJ")) {
        static std::string axes{"XYZABCUVW"};
        for (auto c : *coord) {
            size_t pos = axes.find(std::toupper(c & 0xff));
            if (std::string::npos != pos) {
                num_axes++;
                axis_mask |= 1 << pos;
            }
            // else we could warn...
        }
    }
    if (num_axes ==0) {
        log_info("halui: no [TRAJ]COORDINATES specified, enabling all axes\n");
        num_axes = EMCMOT_MAX_AXIS;
        axis_mask = (1 << EMCMOT_MAX_AXIS) - 1;
    }

    if (auto inival = inifile.findSInt("JOINTS", "KINS")) {
        if (*inival > 0) {
            num_joints = *inival;
        }
    }

    if (auto inival = inifile.findSInt("SPINDLES", "TRAJ")) {
        if (*inival > 0) {
            num_spindles = *inival;
        }
    }

    if (inifile.isSet("HOME_SEQUENCE", "JOINT_0")) {
        have_home_all = 1;
    }

    if (auto v = mapLinearUnits(inifile, "LINEAR_UNITS", "DISPLAY")) {
        linearUnitConversion = *v;
    }
    if (auto v = mapAngularUnits(inifile, "ANGULAR_UNITS", "DISPLAY")) {
        angularUnitConversion = *v;
    }

    while(num_mdi_commands < MDI_MAX) {
        auto mc = inifile.findString(num_mdi_commands+1, "MDI_COMMAND", "HALUI");
        if (!mc) break;
        mdi_commands[num_mdi_commands++] = strdup(mc->c_str());
    }

    return 0;
}

static void hal_init_pins()
{
    int joint;
    int axis_num;
    int spindle;

    hal_set_bool(halui_data->machine_on, old_halui_data.machine_on = 0);
    hal_set_bool(halui_data->machine_off, old_halui_data.machine_off = 0);

    hal_set_bool(halui_data->estop_activate, old_halui_data.estop_activate = 0);
    hal_set_bool(halui_data->estop_reset, old_halui_data.estop_reset = 0);


    for (joint=0; joint < num_joints; joint++) {
	hal_set_bool(halui_data->joint_home[joint], old_halui_data.joint_home[joint] = 0);
	hal_set_bool(halui_data->joint_unhome[joint], old_halui_data.joint_unhome[joint] = 0);
	hal_set_bool(halui_data->joint_nr_select[joint], old_halui_data.joint_nr_select[joint] = 0);
	hal_set_bool(halui_data->jjog_minus[joint], old_halui_data.jjog_minus[joint] = 0);
	hal_set_bool(halui_data->jjog_plus[joint], old_halui_data.jjog_plus[joint] = 0);
	hal_set_real(halui_data->jjog_analog[joint], old_halui_data.jjog_analog[joint] = 0.0);
	hal_set_real(halui_data->jjog_increment[joint], old_halui_data.jjog_increment[joint] = 0.0);
	hal_set_bool(halui_data->jjog_increment_plus[joint], old_halui_data.jjog_increment_plus[joint] = 0);
	hal_set_bool(halui_data->jjog_increment_minus[joint], old_halui_data.jjog_increment_minus[joint] = 0);
    }

    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        if ( !(axis_mask & (1 << axis_num)) ) { continue; }
        hal_set_bool(halui_data->axis_nr_select[axis_num], old_halui_data.axis_nr_select[axis_num] = 0);
	hal_set_bool(halui_data->ajog_minus[axis_num], old_halui_data.ajog_minus[axis_num] = 0);
	hal_set_bool(halui_data->ajog_plus[axis_num], old_halui_data.ajog_plus[axis_num] = 0);
	hal_set_real(halui_data->ajog_analog[axis_num], old_halui_data.ajog_analog[axis_num] = 0);
	hal_set_real(halui_data->ajog_increment[axis_num], old_halui_data.ajog_increment[axis_num] = 0.0);
	hal_set_bool(halui_data->ajog_increment_plus[axis_num], old_halui_data.ajog_increment_plus[axis_num] = 0);
	hal_set_bool(halui_data->ajog_increment_minus[axis_num], old_halui_data.ajog_increment_minus[axis_num] = 0);
    }

    hal_set_bool(halui_data->joint_home[num_joints], old_halui_data.joint_home[num_joints] = 0);
    hal_set_bool(halui_data->jjog_minus[num_joints], old_halui_data.jjog_minus[num_joints] = 0);
    hal_set_bool(halui_data->jjog_plus[num_joints], old_halui_data.jjog_plus[num_joints] = 0);
    hal_set_real(halui_data->jjog_increment[num_joints], old_halui_data.jjog_increment[num_joints] = 0.0);
    hal_set_bool(halui_data->jjog_increment_plus[num_joints], old_halui_data.jjog_increment_plus[num_joints] = 0);
    hal_set_bool(halui_data->jjog_increment_minus[num_joints], old_halui_data.jjog_increment_minus[num_joints] = 0);
    hal_set_real(halui_data->jjog_deadband, 0.2);
    hal_set_real(halui_data->jjog_speed, 0);
    hal_set_bool(halui_data->ajog_minus[EMCMOT_MAX_AXIS], old_halui_data.ajog_minus[EMCMOT_MAX_AXIS] = 0);
    hal_set_bool(halui_data->ajog_plus[EMCMOT_MAX_AXIS], old_halui_data.ajog_plus[EMCMOT_MAX_AXIS] = 0);
    hal_set_real(halui_data->ajog_increment[EMCMOT_MAX_AXIS], old_halui_data.ajog_increment[EMCMOT_MAX_AXIS] = 0.0);
    hal_set_bool(halui_data->ajog_increment_plus[EMCMOT_MAX_AXIS], old_halui_data.ajog_increment_plus[EMCMOT_MAX_AXIS] = 0);
    hal_set_bool(halui_data->ajog_increment_minus[EMCMOT_MAX_AXIS], old_halui_data.ajog_increment_minus[EMCMOT_MAX_AXIS] = 0);
    hal_set_real(halui_data->ajog_deadband, 0.2);
    hal_set_real(halui_data->ajog_speed, 0);

    hal_set_uint(halui_data->joint_selected, 0); // select joint 0 by default
    hal_set_uint(halui_data->axis_selected, 0); // select no axis by default

    hal_set_real(halui_data->fo_scale, old_halui_data.fo_scale = 0.1); //sane default
    hal_set_real(halui_data->ro_scale, old_halui_data.ro_scale = 0.1); //sane default
    for (spindle = 0; spindle < num_spindles; spindle++){
        hal_set_real(halui_data->so_scale[spindle], old_halui_data.so_scale[spindle] = 0.1); //sane default
        hal_set_bool(halui_data->so_increase[spindle], old_halui_data.so_increase[spindle] = 0);
        hal_set_bool(halui_data->so_decrease[spindle], old_halui_data.so_decrease[spindle] = 0);
        hal_set_bool(halui_data->spindle_increase[spindle], old_halui_data.spindle_increase[spindle] = 0);
        hal_set_bool(halui_data->spindle_decrease[spindle], old_halui_data.spindle_decrease[spindle] = 0);
    }
}

static int check_bit_changed(bool halpin, bool &newpin)
{
    if (halpin != newpin) {
	newpin = halpin;
	return halpin;
    }
    return 0;
}

static void copy_hal_data(const halui_str &i, local_halui_str &j)
{
#define FIELD(t,f) do { \
        if(i.f) { \
            j.f = hal_get_##t(i.f); \
        } else { j.f = 0; } \
    } while(0);
#define ARRAY(t,f,n) do { \
        for (int x = 0; x < n; x++) { \
            if(i.f[x]) { \
                j.f[x] = hal_get_##t(i.f[x]); \
            } else { j.f[x] = 0; } \
        } \
    } while (0);
    HAL_FIELDS
#undef FIELD
#undef ARRAY
}


// Returns true if any of halui.JA.N.plus, halui.JA.N.minus, or
// halui.JA.N.analog are true (where JA is joint or axis and
// N is the passed-in joint/axis number). Otherwise, returns false.
static bool jogging_joint(local_halui_str &hal, int joint) {
    return (hal.jjog_plus[joint] || hal.jjog_minus[joint] || hal.jjog_analog[joint]);
}
static bool jogging_axis(local_halui_str &hal, int axis_num) {
    return (hal.ajog_plus[axis_num] || hal.ajog_minus[axis_num] || hal.ajog_analog[axis_num]);
}


// Returns true if any of halui.JA.selected.plus,
// halui.JA.selected.minus, or halui.JA.selected.analog are true.
// JA == joint or axis as appropriate
// Otherwise, returns false.
static bool jogging_selected_joint(local_halui_str &hal) {
    return (hal.jjog_plus[num_joints] || hal.jjog_minus[num_joints]);
}
static bool jogging_selected_axis(local_halui_str &hal) {
    return (hal.ajog_plus[EMCMOT_MAX_AXIS] || hal.ajog_minus[EMCMOT_MAX_AXIS]);
}


// The pins where a 20 ms sampling period is the wrong answer. They are
// checked every task cycle; check_bit_changed() updates old_halui_data, so
// the full pass below does not see the same edge a second time.
static void check_safety_pins()
{
    if (check_bit_changed(hal_get_bool(halui_data->estop_activate),
                          old_halui_data.estop_activate) != 0)
	sendEstop();

    if (check_bit_changed(hal_get_bool(halui_data->machine_off),
                          old_halui_data.machine_off) != 0)
	sendMachineOff();

    if (check_bit_changed(hal_get_bool(halui_data->abort),
                          old_halui_data.abort) != 0)
	sendAbort();

    if (check_bit_changed(hal_get_bool(halui_data->program_stop),
                          old_halui_data.program_stop) != 0)
	sendAbort();
}

// this function looks if any of the hal pins has changed
// and sends appropriate messages if so
static void check_hal_changes()
{
    rtapi_sint counts;
    int jselect_changed, joint;
    int aselect_changed, axis_num;
    rtapi_bool bit;
    int js;
    rtapi_real floatt;
    int jjog_speed_changed;
    int ajog_speed_changed;

    local_halui_str new_halui_data_mutable;
    copy_hal_data(*halui_data, new_halui_data_mutable);
    const local_halui_str &new_halui_data = new_halui_data_mutable;

    //check if machine_on pin has changed (the rest work exactly the same)
    if (check_bit_changed(new_halui_data.machine_on, old_halui_data.machine_on) != 0)
	sendMachineOn();                //send MachineOn NML command

    if (check_bit_changed(new_halui_data.machine_off, old_halui_data.machine_off) != 0)
	sendMachineOff();

    if (check_bit_changed(new_halui_data.estop_activate, old_halui_data.estop_activate) != 0)
	sendEstop();

    if (check_bit_changed(new_halui_data.estop_reset, old_halui_data.estop_reset) != 0)
	sendEstopReset();

    if (check_bit_changed(new_halui_data.mode_manual, old_halui_data.mode_manual) != 0)
	sendManual();

    if (check_bit_changed(new_halui_data.mode_auto, old_halui_data.mode_auto) != 0)
	sendAuto();

    if (check_bit_changed(new_halui_data.mode_mdi, old_halui_data.mode_mdi) != 0)
	sendMdi();

    if (check_bit_changed(new_halui_data.mode_teleop, old_halui_data.mode_teleop) != 0)
	sendTeleop();

    if (check_bit_changed(new_halui_data.mode_joint, old_halui_data.mode_joint) != 0)
	sendJoint();

    if (check_bit_changed(new_halui_data.mist_on, old_halui_data.mist_on) != 0)
	sendMistOn();

    if (check_bit_changed(new_halui_data.mist_off, old_halui_data.mist_off) != 0)
	sendMistOff();

    if (check_bit_changed(new_halui_data.flood_on, old_halui_data.flood_on) != 0)
	sendFloodOn();

    if (check_bit_changed(new_halui_data.flood_off, old_halui_data.flood_off) != 0)
	sendFloodOff();

    if (check_bit_changed(new_halui_data.program_run, old_halui_data.program_run) != 0)
	sendProgramRun(0);

    if (check_bit_changed(new_halui_data.program_pause, old_halui_data.program_pause) != 0)
	sendProgramPause();

    if (check_bit_changed(new_halui_data.program_os_on, old_halui_data.program_os_on) != 0)
	sendSetOptionalStop(ON);

    if (check_bit_changed(new_halui_data.program_os_off, old_halui_data.program_os_off) != 0)
	sendSetOptionalStop(OFF);

    if (check_bit_changed(new_halui_data.program_bd_on, old_halui_data.program_bd_on) != 0)
	sendSetBlockDelete(ON);

    if (check_bit_changed(new_halui_data.program_bd_off, old_halui_data.program_bd_off) != 0)
	sendSetBlockDelete(OFF);

    if (check_bit_changed(new_halui_data.program_resume, old_halui_data.program_resume) != 0)
	sendProgramResume();

    if (check_bit_changed(new_halui_data.program_step, old_halui_data.program_step) != 0)
	sendProgramStep();

    if (check_bit_changed(new_halui_data.program_stop, old_halui_data.program_stop) != 0)
	sendAbort();

    //max-velocity stuff
    counts = new_halui_data.mv_counts;
    if (counts != old_halui_data.mv_counts) {
        if (new_halui_data.mv_count_enable) {
            if (new_halui_data.mv_direct_value) {
                sendMaxVelocity(counts * new_halui_data.mv_scale);
            } else {
                sendMaxVelocity( new_halui_data.mv_value + (counts - old_halui_data.mv_counts) *
                    new_halui_data.mv_scale);
            }
        }
        old_halui_data.mv_counts = counts;
    }

    //feed-override stuff
    counts = new_halui_data.fo_counts;
    if (counts != old_halui_data.fo_counts) {
        if (new_halui_data.fo_count_enable) {
            if (new_halui_data.fo_direct_value) {
                sendFeedOverride(counts * new_halui_data.fo_scale);
            } else {
                sendFeedOverride( new_halui_data.fo_value + (counts - old_halui_data.fo_counts) *
                    new_halui_data.fo_scale);
            }
        }
        old_halui_data.fo_counts = counts;
    }

    //rapid-override stuff
    counts = new_halui_data.ro_counts;
    if (counts != old_halui_data.ro_counts) {
        if (new_halui_data.ro_count_enable) {
            if (new_halui_data.ro_direct_value) {
                sendRapidOverride(counts * new_halui_data.ro_scale);
            } else {
                sendRapidOverride( new_halui_data.ro_value + (counts - old_halui_data.ro_counts) *
                    new_halui_data.ro_scale);
            }
        }
        old_halui_data.ro_counts = counts;
    }

    //spindle-override stuff
    for (int spindle = 0; spindle < num_spindles; spindle++){
		counts = new_halui_data.so_counts[spindle];
		if (counts != old_halui_data.so_counts[spindle]) {
			if (new_halui_data.so_count_enable[spindle]) {
				if (new_halui_data.so_direct_value[spindle]) {
					sendSpindleOverride(spindle, counts * new_halui_data.so_scale[spindle]);
				} else {
					sendSpindleOverride(spindle, new_halui_data.so_value[spindle] + (counts - old_halui_data.so_counts[spindle]) *
						new_halui_data.so_scale[spindle]);
				}
			}
			old_halui_data.so_counts[spindle] = counts;
		}
    }

    if (check_bit_changed(new_halui_data.mv_increase, old_halui_data.mv_increase) != 0)
        sendMaxVelocity(new_halui_data.mv_value + new_halui_data.mv_scale);
    if (check_bit_changed(new_halui_data.mv_decrease, old_halui_data.mv_decrease) != 0)
        sendMaxVelocity(new_halui_data.mv_value - new_halui_data.mv_scale);

    if (check_bit_changed(new_halui_data.fo_increase, old_halui_data.fo_increase) != 0)
        sendFeedOverride(new_halui_data.fo_value + new_halui_data.fo_scale);
    if (check_bit_changed(new_halui_data.fo_decrease, old_halui_data.fo_decrease) != 0)
        sendFeedOverride(new_halui_data.fo_value - new_halui_data.fo_scale);
    if (check_bit_changed(new_halui_data.fo_reset, old_halui_data.fo_reset) != 0)
        sendFeedOverride( 1.0 );

    if (check_bit_changed(new_halui_data.ro_increase, old_halui_data.ro_increase) != 0)
        sendRapidOverride(new_halui_data.ro_value + new_halui_data.ro_scale);
    if (check_bit_changed(new_halui_data.ro_decrease, old_halui_data.ro_decrease) != 0)
        sendRapidOverride(new_halui_data.ro_value - new_halui_data.ro_scale);
    if (check_bit_changed(new_halui_data.ro_reset, old_halui_data.ro_reset) != 0)
        sendRapidOverride( 1.0 );

	// spindle stuff
    for (int spindle = 0; spindle < num_spindles; spindle++){
		if (check_bit_changed(new_halui_data.so_increase[spindle], old_halui_data.so_increase[spindle]) != 0)
			sendSpindleOverride(spindle, new_halui_data.so_value[spindle] + new_halui_data.so_scale[spindle]);
		if (check_bit_changed(new_halui_data.so_decrease[spindle], old_halui_data.so_decrease[spindle]) != 0)
			sendSpindleOverride(spindle, new_halui_data.so_value[spindle] - new_halui_data.so_scale[spindle]);
                if (check_bit_changed(new_halui_data.so_reset[spindle], old_halui_data.so_reset[spindle]) != 0)
                        sendSpindleOverride(spindle, 1.0 );

		if (check_bit_changed(new_halui_data.spindle_start[spindle], old_halui_data.spindle_start[spindle]) != 0)
		sendSpindleForward(spindle);

		if (check_bit_changed(new_halui_data.spindle_stop[spindle], old_halui_data.spindle_stop[spindle]) != 0)
		sendSpindleOff(spindle);

		if (check_bit_changed(new_halui_data.spindle_forward[spindle], old_halui_data.spindle_forward[spindle]) != 0)
		sendSpindleForward(spindle);

		if (check_bit_changed(new_halui_data.spindle_reverse[spindle], old_halui_data.spindle_reverse[spindle]) != 0)
		sendSpindleReverse(spindle);

		bit = new_halui_data.spindle_increase[spindle];
		if (bit != old_halui_data.spindle_increase[spindle]) {
		if (bit != 0)
			sendSpindleIncrease(spindle);
		if (bit == 0)
			sendSpindleConstant(spindle);
		old_halui_data.spindle_increase[spindle]= bit;
		}

		bit = new_halui_data.spindle_decrease[spindle];
		if (bit != old_halui_data.spindle_decrease[spindle]) {
		if (bit != 0)
			sendSpindleDecrease(spindle);
		if (bit == 0)
			sendSpindleConstant(spindle);
		old_halui_data.spindle_decrease[spindle]= bit;
		}

		if (check_bit_changed(new_halui_data.spindle_brake_on[spindle], old_halui_data.spindle_brake_on[spindle]) != 0)
		sendBrakeEngage(spindle);

		if (check_bit_changed(new_halui_data.spindle_brake_off[spindle], old_halui_data.spindle_brake_off[spindle]) != 0)
		sendBrakeRelease(spindle);
    }

	if (check_bit_changed(new_halui_data.abort, old_halui_data.abort) != 0)
	sendAbort();

	if (check_bit_changed(new_halui_data.home_all, old_halui_data.home_all) != 0)
	sendHome(-1);

// joint stuff (selection, homing..)
    jselect_changed = -1; // flag to see if the selected joint changed

    // if the jog-speed changes while in a continuous jog, we want to
    // re-start the jog with the new speed
    if (fabs(old_halui_data.jjog_speed - new_halui_data.jjog_speed) > 0.00001) {
        old_halui_data.jjog_speed = new_halui_data.jjog_speed;
        jjog_speed_changed = 1;
    } else {
        jjog_speed_changed = 0;
    }
// axis stuff (selection, homing..)
    aselect_changed = -1; // flag to see if the selected joint changed

    // if the jog-speed changes while in a continuous jog, we want to
    // re-start the jog with the new speed
    if (fabs(old_halui_data.ajog_speed - new_halui_data.ajog_speed) > 0.00001) {
        old_halui_data.ajog_speed = new_halui_data.ajog_speed;
        ajog_speed_changed = 1;
    } else {
        ajog_speed_changed = 0;
    }

    for (joint=0; joint < num_joints; joint++) {
	if (check_bit_changed(new_halui_data.joint_home[joint], old_halui_data.joint_home[joint]) != 0)
	    sendHome(joint);

	if (check_bit_changed(new_halui_data.joint_unhome[joint], old_halui_data.joint_unhome[joint]) != 0)
	    sendUnhome(joint);

	bit = new_halui_data.jjog_minus[joint];
	if ((bit != old_halui_data.jjog_minus[joint]) || (bit && jjog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(joint,-new_halui_data.jjog_speed,JOGJOINT);
	    else
		sendJogStop(joint,JOGJOINT);
	    old_halui_data.jjog_minus[joint] = bit;
	}

	bit = new_halui_data.jjog_plus[joint];
	if ((bit != old_halui_data.jjog_plus[joint]) || (bit && jjog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(joint,new_halui_data.jjog_speed,JOGJOINT);
	    else
		sendJogStop(joint,JOGJOINT);
	    old_halui_data.jjog_plus[joint] = bit;
	}

	floatt = new_halui_data.jjog_analog[joint];
	bit = (fabs(floatt) > new_halui_data.jjog_deadband);
	if ((floatt != old_halui_data.jjog_analog[joint]) || (bit && jjog_speed_changed)) {
	    if (bit)
		sendJogCont(joint,(new_halui_data.jjog_speed) * (new_halui_data.jjog_analog[joint]),JOGJOINT);
	    else
		sendJogStop(joint,JOGJOINT);
	    old_halui_data.jjog_analog[joint] = floatt;
	}

	bit = new_halui_data.jjog_increment_plus[joint];
	if (bit != old_halui_data.jjog_increment_plus[joint]) {
	    if (bit)
		sendJogIncr(joint, new_halui_data.jjog_speed, new_halui_data.jjog_increment[joint],JOGJOINT);
	    old_halui_data.jjog_increment_plus[joint] = bit;
	}

	bit = new_halui_data.jjog_increment_minus[joint];
	if (bit != old_halui_data.jjog_increment_minus[joint]) {
	    if (bit)
		sendJogIncr(joint, new_halui_data.jjog_speed, -(new_halui_data.jjog_increment[joint]),JOGJOINT);
	    old_halui_data.jjog_increment_minus[joint] = bit;
	}

	// check to see if another joint has been selected
	bit = new_halui_data.joint_nr_select[joint];
	if (bit != old_halui_data.joint_nr_select[joint]) {
	    if (bit != 0) {
		hal_set_uint(halui_data->joint_selected, joint);
		jselect_changed = joint; // flag that we changed the selected joint
	    }
	    old_halui_data.joint_nr_select[joint] = bit;
	}

    }

    if (jselect_changed >= 0) {
	for (joint = 0; joint < num_joints; joint++) {
	    if (joint != jselect_changed) {
		hal_set_bool(halui_data->joint_is_selected[joint], 0);
                if (jogging_selected_joint(old_halui_data) && !jogging_joint(old_halui_data, joint)) {
                    sendJogStop(joint,JOGJOINT);
                }
            } else {
		hal_set_bool(halui_data->joint_is_selected[joint], 1);
                if (hal_get_bool(halui_data->jjog_plus[num_joints])) {
                    sendJogCont(joint, new_halui_data.jjog_speed,JOGJOINT);
                } else if (hal_get_bool(halui_data->jjog_minus[num_joints])) {
                    sendJogCont(joint, -new_halui_data.jjog_speed,JOGJOINT);
                }
	    }
	}
    }

    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        if ( !(axis_mask & (1 << axis_num)) ) { continue; }
	bit = new_halui_data.ajog_minus[axis_num];
	if ((bit != old_halui_data.ajog_minus[axis_num]) || (bit && ajog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(axis_num,-new_halui_data.ajog_speed,JOGTELEOP);
	    else
		sendJogStop(axis_num,JOGTELEOP);
	    old_halui_data.ajog_minus[axis_num] = bit;
	}

	bit = new_halui_data.ajog_plus[axis_num];
	if ((bit != old_halui_data.ajog_plus[axis_num]) || (bit && ajog_speed_changed)) {
	    if (bit != 0)
		sendJogCont(axis_num,new_halui_data.ajog_speed,JOGTELEOP);
	    else
		sendJogStop(axis_num,JOGTELEOP);
	    old_halui_data.ajog_plus[axis_num] = bit;
	}

	floatt = new_halui_data.ajog_analog[axis_num];
	bit = (fabs(floatt) > new_halui_data.ajog_deadband);
	if ((floatt != old_halui_data.ajog_analog[axis_num]) || (bit && ajog_speed_changed)) {
	    if (bit)
		sendJogCont(axis_num,(new_halui_data.ajog_speed) * (new_halui_data.ajog_analog[axis_num]),JOGTELEOP);
	    else
		sendJogStop(axis_num,JOGTELEOP);
	    old_halui_data.ajog_analog[axis_num] = floatt;
	}

	bit = new_halui_data.ajog_increment_plus[axis_num];
	if (bit != old_halui_data.ajog_increment_plus[axis_num]) {
	    if (bit)
		sendJogIncr(axis_num, new_halui_data.ajog_speed, new_halui_data.ajog_increment[axis_num],JOGTELEOP);
	    old_halui_data.ajog_increment_plus[axis_num] = bit;
	}

	bit = new_halui_data.ajog_increment_minus[axis_num];
	if (bit != old_halui_data.ajog_increment_minus[axis_num]) {
	    if (bit)
		sendJogIncr(axis_num, new_halui_data.ajog_speed, -(new_halui_data.ajog_increment[axis_num]),JOGTELEOP);
	    old_halui_data.ajog_increment_minus[axis_num] = bit;
	}

	// check to see if another axis has been selected
	bit = new_halui_data.axis_nr_select[axis_num];
	if (bit != old_halui_data.axis_nr_select[axis_num]) {
	    if (bit != 0) {
		hal_set_uint(halui_data->axis_selected, axis_num);
		aselect_changed = axis_num; // flag that we changed the selected axis
	    }
	    old_halui_data.axis_nr_select[axis_num] = bit;
	}
    }

    if (aselect_changed >= 0) {
    for (axis_num = 0; axis_num < EMCMOT_MAX_AXIS; axis_num++) {
        if ( !(axis_mask & (1 << axis_num)) ) { continue; }
	    if (axis_num != aselect_changed) {
		hal_set_bool(halui_data->axis_is_selected[axis_num], 0);
                if (jogging_selected_axis(old_halui_data) && !jogging_axis(old_halui_data, axis_num)) {
                    sendJogStop(axis_num,JOGTELEOP);
                }
            } else {
		hal_set_bool(halui_data->axis_is_selected[axis_num], 1);
                if (hal_get_bool(halui_data->ajog_plus[num_axes])) {
                    sendJogCont(axis_num, new_halui_data.ajog_speed,JOGTELEOP);
                } else if (hal_get_bool(halui_data->ajog_minus[num_axes])) {
                    sendJogCont(axis_num, -new_halui_data.ajog_speed,JOGTELEOP);
                }
	    }
	}
    }

    if (check_bit_changed(new_halui_data.joint_home[num_joints], old_halui_data.joint_home[num_joints]) != 0)
	sendHome(new_halui_data.joint_selected);

    if (check_bit_changed(new_halui_data.joint_unhome[num_joints], old_halui_data.joint_unhome[num_joints]) != 0)
	sendUnhome(new_halui_data.joint_selected);

    bit = new_halui_data.jjog_minus[num_joints];
    js = new_halui_data.joint_selected;
    if ((bit != old_halui_data.jjog_minus[num_joints]) || (bit && jjog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js, -new_halui_data.jjog_speed,JOGJOINT);
	else
	    sendJogStop(js,JOGJOINT);
	old_halui_data.jjog_minus[num_joints] = bit;
    }

    bit = new_halui_data.jjog_plus[num_joints];
    js = new_halui_data.joint_selected;
    if ((bit != old_halui_data.jjog_plus[num_joints]) || (bit && jjog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js,new_halui_data.jjog_speed,JOGJOINT);
	else
	    sendJogStop(js,JOGJOINT);
	old_halui_data.jjog_plus[num_joints] = bit;
    }

    bit = new_halui_data.jjog_increment_plus[num_joints];
    js = new_halui_data.joint_selected;
    if (bit != old_halui_data.jjog_increment_plus[num_joints]) {
	if (bit)
	    sendJogIncr(js, new_halui_data.jjog_speed, new_halui_data.jjog_increment[num_joints],JOGJOINT);
	old_halui_data.jjog_increment_plus[num_joints] = bit;
    }

    bit = new_halui_data.jjog_increment_minus[num_joints];
    js = new_halui_data.joint_selected;
    if (bit != old_halui_data.jjog_increment_minus[num_joints]) {
	if (bit)
	    sendJogIncr(js, new_halui_data.jjog_speed, -(new_halui_data.jjog_increment[num_joints]),JOGJOINT);
	old_halui_data.jjog_increment_minus[num_joints] = bit;
    }

    bit = new_halui_data.ajog_minus[EMCMOT_MAX_AXIS];
    js = new_halui_data.axis_selected;
    if ((bit != old_halui_data.ajog_minus[EMCMOT_MAX_AXIS]) || (bit && ajog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js, -new_halui_data.ajog_speed,JOGTELEOP);
	else
	    sendJogStop(js,JOGTELEOP);
	old_halui_data.ajog_minus[EMCMOT_MAX_AXIS] = bit;
    }

    bit = new_halui_data.ajog_plus[EMCMOT_MAX_AXIS];
    js = new_halui_data.axis_selected;
    if ((bit != old_halui_data.ajog_plus[EMCMOT_MAX_AXIS]) || (bit && ajog_speed_changed)) {
        if (bit != 0)
	    sendJogCont(js,new_halui_data.ajog_speed,JOGTELEOP);
	else
	    sendJogStop(js,JOGTELEOP);
	old_halui_data.ajog_plus[EMCMOT_MAX_AXIS] = bit;
    }

    bit = new_halui_data.ajog_increment_plus[EMCMOT_MAX_AXIS];
    js = new_halui_data.axis_selected;
    if (bit != old_halui_data.ajog_increment_plus[EMCMOT_MAX_AXIS]) {
	if (bit)
	    sendJogIncr(js, new_halui_data.ajog_speed, new_halui_data.ajog_increment[EMCMOT_MAX_AXIS],JOGTELEOP);
	old_halui_data.ajog_increment_plus[EMCMOT_MAX_AXIS] = bit;
    }

    bit = new_halui_data.ajog_increment_minus[EMCMOT_MAX_AXIS];
    js = new_halui_data.axis_selected;
    if (bit != old_halui_data.ajog_increment_minus[EMCMOT_MAX_AXIS]) {
	if (bit)
	    sendJogIncr(js, new_halui_data.ajog_speed, -(new_halui_data.ajog_increment[EMCMOT_MAX_AXIS]),JOGTELEOP);
	old_halui_data.ajog_increment_minus[EMCMOT_MAX_AXIS] = bit;
    }

    for(int n = 0; n < num_mdi_commands; n++) {
        if (check_bit_changed(new_halui_data.mdi_commands[n], old_halui_data.mdi_commands[n]) != 0)
            sendMdiCommand(n);
    }
}

// this function looks at the received NML status message
// and modifies the appropriate HAL pins
static void modify_hal_pins()
{
    int joint;
    int spindle;

    hal_set_bool(halui_data->machine_is_on, emcStatus->task.state == EMC_TASK_STATE::ON);
    hal_set_bool(halui_data->estop_is_activated, emcStatus->task.state == EMC_TASK_STATE::ESTOP);

    // Decide once whether an ongoing halui MDI command has finished: the mode
    // restore below may refresh emcStatus, and Task may echo it while still EXEC.
    bool mdi_finished = mdi_settled() && emcStatus->status == RCS_STATUS::DONE;

    if (mdi_settled()) { // we have an ongoing MDI command
	if (mdi_finished) { //which seems to have finished
	    switch (halui_old_mode) {
		case EMC_TASK_MODE::MANUAL: sendManual();break;
		case EMC_TASK_MODE::MDI: break;
		case EMC_TASK_MODE::AUTO: sendAuto();break;
		default: sendManual();break;
	    }
	}
    }
	

    hal_set_bool(halui_data->mode_is_manual, emcStatus->task.mode == EMC_TASK_MODE::MANUAL);
    hal_set_bool(halui_data->mode_is_auto,   emcStatus->task.mode == EMC_TASK_MODE::AUTO);
    hal_set_bool(halui_data->mode_is_mdi,    emcStatus->task.mode == EMC_TASK_MODE::MDI);
    hal_set_bool(halui_data->mode_is_teleop, emcStatus->motion.traj.mode == EMC_TRAJ_MODE::TELEOP);
    hal_set_bool(halui_data->mode_is_joint,  emcStatus->motion.traj.mode == EMC_TRAJ_MODE::FREE);

    hal_set_bool(halui_data->program_is_paused,  emcStatus->task.interpState == EMC_TASK_INTERP::PAUSED);
    hal_set_bool(halui_data->program_is_running, emcStatus->task.interpState == EMC_TASK_INTERP::READING ||
                                                 emcStatus->task.interpState == EMC_TASK_INTERP::WAITING);
    hal_set_bool(halui_data->program_is_idle,    emcStatus->task.interpState == EMC_TASK_INTERP::IDLE);
    
    if (num_mdi_commands>0){
		// we wants initialize program_is_idle and mode_is_mdi before halui_sent_mdi
		if (mdi_settled()) { // we have an ongoing MDI command
			if (mdi_finished) { //which seems to have finished
			halui_sent_mdi = 0;
			mdi_exec_ticket = 0;
			// the standalone halui slept 2x20ms around a status
			// re-read here; in task the status is already current
			}
		}
		hal_set_bool(halui_data->halui_mdi_is_running, halui_sent_mdi);
	}


    
    hal_set_bool(halui_data->program_os_is_on, emcStatus->task.optional_stop_state);
    hal_set_bool(halui_data->program_bd_is_on, emcStatus->task.block_delete_state);

    hal_set_real(halui_data->mv_value, emcStatus->motion.traj.maxVelocity);
    hal_set_real(halui_data->fo_value, emcStatus->motion.traj.scale); //feedoverride from 0 to 1 for 100%
    hal_set_real(halui_data->ro_value, emcStatus->motion.traj.rapid_scale); //rapid override from 0 to 1 for 100%

    hal_set_bool(halui_data->mist_is_on, emcStatus->io.coolant.mist);
    hal_set_bool(halui_data->flood_is_on, emcStatus->io.coolant.flood);

    hal_set_uint(halui_data->tool_number, emcStatus->io.tool.toolInSpindle);
    hal_set_real(halui_data->tool_length_offset_x, emcStatus->task.toolOffset.tran.x);
    hal_set_real(halui_data->tool_length_offset_y, emcStatus->task.toolOffset.tran.y);
    hal_set_real(halui_data->tool_length_offset_z, emcStatus->task.toolOffset.tran.z);
    hal_set_real(halui_data->tool_length_offset_a, emcStatus->task.toolOffset.a);
    hal_set_real(halui_data->tool_length_offset_b, emcStatus->task.toolOffset.b);
    hal_set_real(halui_data->tool_length_offset_c, emcStatus->task.toolOffset.c);
    hal_set_real(halui_data->tool_length_offset_u, emcStatus->task.toolOffset.u);
    hal_set_real(halui_data->tool_length_offset_v, emcStatus->task.toolOffset.v);
    hal_set_real(halui_data->tool_length_offset_w, emcStatus->task.toolOffset.w);

    if (emcStatus->io.tool.toolInSpindle == 0) {
        hal_set_real(halui_data->tool_diameter, 0.0);
    } else {
        int idx;
        for (idx = 0; idx <= tooldata_last_index_get(); idx ++) { // note <=
            CANON_TOOL_TABLE tdata;
            if (tooldata_get(&tdata,idx) != IDX_OK) {
                fprintf(stderr,"UNEXPECTED idx %s %d\n",__FILE__,__LINE__);
            }
            if (tdata.toolno == emcStatus->io.tool.toolInSpindle) {
                hal_set_real(halui_data->tool_diameter, tdata.diameter);
                break;
            }
        }
        if (idx == CANON_POCKETS_MAX) {
            // didn't find the tool
            hal_set_real(halui_data->tool_diameter, 0.0);
        }
    }

    for (spindle = 0; spindle < num_spindles; spindle++){
        hal_set_bool(halui_data->spindle_is_on[spindle], (emcStatus->motion.spindle[spindle].enabled));
        hal_set_bool(halui_data->spindle_runs_forward[spindle], (emcStatus->motion.spindle[spindle].direction == 1));
        hal_set_bool(halui_data->spindle_runs_backward[spindle], (emcStatus->motion.spindle[spindle].direction == -1));
        hal_set_bool(halui_data->spindle_brake_is_on[spindle], emcStatus->motion.spindle[spindle].brake);
        hal_set_real(halui_data->so_value[spindle], emcStatus->motion.spindle[spindle].spindle_scale); //spindle-speed-override from 0 to 1 for 100%
    }

    for (joint=0; joint < num_joints; joint++) {
	hal_set_bool(halui_data->joint_is_homed[joint], emcStatus->motion.joint[joint].homed);
	hal_set_bool(halui_data->joint_on_soft_min_limit[joint], emcStatus->motion.joint[joint].minSoftLimit);
	hal_set_bool(halui_data->joint_on_soft_max_limit[joint], emcStatus->motion.joint[joint].maxSoftLimit);
	hal_set_bool(halui_data->joint_on_hard_min_limit[joint], emcStatus->motion.joint[joint].minHardLimit);
	hal_set_bool(halui_data->joint_on_hard_max_limit[joint], emcStatus->motion.joint[joint].maxHardLimit);
	hal_set_bool(halui_data->joint_override_limits[joint], emcStatus->motion.joint[joint].overrideLimits);
	hal_set_bool(halui_data->joint_has_fault[joint], emcStatus->motion.joint[joint].fault);
    }

    if (axis_mask & 0x0001) {
      hal_set_real(halui_data->axis_pos_commanded[0], emcStatus->motion.traj.position.tran.x);
      hal_set_real(halui_data->axis_pos_feedback[0], emcStatus->motion.traj.actualPosition.tran.x);
      double x = emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.toolOffset.tran.x;
      double y = emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.toolOffset.tran.y;
      x = x * cos(-emcStatus->task.rotation_xy * TO_RAD) - y * sin(-emcStatus->task.rotation_xy * TO_RAD);
      hal_set_real(halui_data->axis_pos_relative[0], x - emcStatus->task.g92_offset.tran.x);
    }

    if (axis_mask & 0x0002) {
      hal_set_real(halui_data->axis_pos_commanded[1], emcStatus->motion.traj.position.tran.y);
      hal_set_real(halui_data->axis_pos_feedback[1], emcStatus->motion.traj.actualPosition.tran.y);
      double x = emcStatus->motion.traj.actualPosition.tran.x - emcStatus->task.g5x_offset.tran.x - emcStatus->task.toolOffset.tran.x;
      double y = emcStatus->motion.traj.actualPosition.tran.y - emcStatus->task.g5x_offset.tran.y - emcStatus->task.toolOffset.tran.y;
      y = y * cos(-emcStatus->task.rotation_xy * TO_RAD) + x * sin(-emcStatus->task.rotation_xy * TO_RAD);
      hal_set_real(halui_data->axis_pos_relative[1], y - emcStatus->task.g92_offset.tran.y);
    }

    if (axis_mask & 0x0004) {
      hal_set_real(halui_data->axis_pos_commanded[2], emcStatus->motion.traj.position.tran.z);
      hal_set_real(halui_data->axis_pos_feedback[2], emcStatus->motion.traj.actualPosition.tran.z);
      hal_set_real(halui_data->axis_pos_relative[2], emcStatus->motion.traj.actualPosition.tran.z - emcStatus->task.g5x_offset.tran.z - emcStatus->task.g92_offset.tran.z - emcStatus->task.toolOffset.tran.z);
    }

    if (axis_mask & 0x0008) {
      hal_set_real(halui_data->axis_pos_commanded[3], emcStatus->motion.traj.position.a);
      hal_set_real(halui_data->axis_pos_feedback[3], emcStatus->motion.traj.actualPosition.a);
      hal_set_real(halui_data->axis_pos_relative[3], emcStatus->motion.traj.actualPosition.a - emcStatus->task.g5x_offset.a - emcStatus->task.g92_offset.a - emcStatus->task.toolOffset.a);
    }

    if (axis_mask & 0x0010) {
      hal_set_real(halui_data->axis_pos_commanded[4], emcStatus->motion.traj.position.b);
      hal_set_real(halui_data->axis_pos_feedback[4], emcStatus->motion.traj.actualPosition.b);
      hal_set_real(halui_data->axis_pos_relative[4], emcStatus->motion.traj.actualPosition.b - emcStatus->task.g5x_offset.b - emcStatus->task.g92_offset.b - emcStatus->task.toolOffset.b);
    }

    if (axis_mask & 0x0020) {
      hal_set_real(halui_data->axis_pos_commanded[5], emcStatus->motion.traj.position.c);
      hal_set_real(halui_data->axis_pos_feedback[5], emcStatus->motion.traj.actualPosition.c);
      hal_set_real(halui_data->axis_pos_relative[5], emcStatus->motion.traj.actualPosition.c - emcStatus->task.g5x_offset.c - emcStatus->task.g92_offset.c - emcStatus->task.toolOffset.c);
    }

    if (axis_mask & 0x0040) {
      hal_set_real(halui_data->axis_pos_commanded[6], emcStatus->motion.traj.position.u);
      hal_set_real(halui_data->axis_pos_feedback[6], emcStatus->motion.traj.actualPosition.u);
      hal_set_real(halui_data->axis_pos_relative[6], emcStatus->motion.traj.actualPosition.u - emcStatus->task.g5x_offset.u - emcStatus->task.g92_offset.u - emcStatus->task.toolOffset.u);
    }

    if (axis_mask & 0x0080) {
      hal_set_real(halui_data->axis_pos_commanded[7], emcStatus->motion.traj.position.v);
      hal_set_real(halui_data->axis_pos_feedback[7], emcStatus->motion.traj.actualPosition.v);
      hal_set_real(halui_data->axis_pos_relative[7], emcStatus->motion.traj.actualPosition.v - emcStatus->task.g5x_offset.v - emcStatus->task.g92_offset.v - emcStatus->task.toolOffset.v);
    }

    if (axis_mask & 0x0100) {
      hal_set_real(halui_data->axis_pos_commanded[8], emcStatus->motion.traj.position.w);
      hal_set_real(halui_data->axis_pos_feedback[8], emcStatus->motion.traj.actualPosition.w);
      hal_set_real(halui_data->axis_pos_relative[8], emcStatus->motion.traj.actualPosition.w - emcStatus->task.g5x_offset.w - emcStatus->task.g92_offset.w - emcStatus->task.toolOffset.w);
    }

    rtapi_uint joint_selected = hal_get_uint(halui_data->joint_selected);
    hal_set_bool(halui_data->joint_is_homed[num_joints], emcStatus->motion.joint[joint_selected].homed);
    hal_set_bool(halui_data->joint_on_soft_min_limit[num_joints], emcStatus->motion.joint[joint_selected].minSoftLimit);
    hal_set_bool(halui_data->joint_on_soft_max_limit[num_joints], emcStatus->motion.joint[joint_selected].maxSoftLimit);
    hal_set_bool(halui_data->joint_on_hard_min_limit[num_joints], emcStatus->motion.joint[joint_selected].minHardLimit);
    hal_set_bool(halui_data->joint_override_limits[num_joints], emcStatus->motion.joint[joint_selected].overrideLimits);
    hal_set_bool(halui_data->joint_on_hard_max_limit[num_joints], emcStatus->motion.joint[joint_selected].maxHardLimit);
    hal_set_bool(halui_data->joint_has_fault[num_joints], emcStatus->motion.joint[joint_selected].fault);

}




/********************************************************************
*
* The three entry points task uses. See halui.hh.
*
********************************************************************/

int haluiInit(const char *filename)
{
    if (0 != haluiIniLoad(filename)) {
        log_error("halui: iniLoad error\n");
        return -1;
    }

    if (0 != halui_hal_init()) {
        log_error("halui: hal_init error\n");
        // the export helpers hal_exit() the component themselves
        comp_id = -1;
        return -1;
    }

    hal_init_pins();
    return 0;
}

void haluiUpdate(void)
{
    if (comp_id < 0) {
        return;
    }

    static bool task_start_synced = false;
    if (!task_start_synced) {
        // wait for task to establish nonzero linearUnits
        if (emcStatus->motion.traj.linearUnits != 0) {
            // set once at startup, no changes are expected:
            hal_set_real(halui_data->units_per_mm, emcStatus->motion.traj.linearUnits);
            task_start_synced = true;
        }
    }

    // every cycle: the pins that should not wait for the next pass
    check_safety_pins();

    const double now = etime();
    const bool due = (now - last_pass) >= HALUI_PERIOD;

    if (due) {
        last_pass = now;
        check_hal_changes(); //if anything changed send commands
    }

    // every cycle, and after check_hal_changes() so that a button pressed
    // in this pass is acted on in it
    service_mdi();

    if (due) {
        modify_hal_pins(); //if status changed modify HAL too
    }
}

void haluiExit(void)
{
    if (comp_id < 0) {
        return;
    }

    //don't forget the big HAL sin ;)
    hal_exit(comp_id);
    comp_id = -1;
}
