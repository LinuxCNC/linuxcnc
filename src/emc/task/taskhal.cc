/********************************************************************
* Description: taskhal.cc
*   HAL pins exported by Task ("task.*").
*
*   These are the halui pins that motion and iocontrol do not already
*   provide. They are computed the same way halui computes them, but
*   directly from Task's own status, once per Task cycle.
*
* License: GPL Version 2
* System: Linux
********************************************************************/

#include <math.h>
#include <string.h>
#include <hal.h>
#include <posemath.h>		// TO_RAD
#include <ctype.h>
#include <inifile.hh>
#include "taskhal.hh"
#include "nml_intf/emcglb.h"		// emc_inifile
#include "logutil.hh"

#define TASKHAL "task"

using namespace linuxcnc;

static const char axis_letters[] = "xyzabcuvw";

struct taskhal_str {
    hal_real_t units_per_mm;

    hal_bool_t mode_is_manual;
    hal_bool_t mode_is_auto;
    hal_bool_t mode_is_mdi;
    hal_bool_t mode_is_joint;

    hal_bool_t program_is_idle;
    hal_bool_t program_is_running;
    hal_bool_t program_is_paused;
    hal_bool_t program_os_is_on;
    hal_bool_t program_bd_is_on;

    hal_real_t mv_value;
    hal_real_t fo_value;
    hal_real_t ro_value;
    hal_real_t so_value[EMCMOT_MAX_SPINDLES];

    hal_bool_t joint_on_soft_min_limit[EMCMOT_MAX_JOINTS];
    hal_bool_t joint_on_soft_max_limit[EMCMOT_MAX_JOINTS];
    hal_bool_t joint_override_limits[EMCMOT_MAX_JOINTS];

    hal_real_t axis_pos_feedback[EMCMOT_MAX_AXIS];
    hal_real_t axis_pos_relative[EMCMOT_MAX_AXIS];

    hal_real_t tool_diameter;
};

static int comp_id = -1;
static taskhal_str *taskhal_data;
static int num_joints;
static int num_spindles;
static int axis_mask;

#define CHK(x) do { \
        int rv = (x); \
        if (rv < 0) { \
            log_error(TASKHAL ": cannot create HAL pin\n"); \
            hal_exit(comp_id); \
            comp_id = -1; \
            return rv; \
        } \
    } while (0)

int emcTaskHalInit(int joints)
{
    num_joints = joints;

    // The spindles and axes motion is set up with, read the same way as
    // loadTraj() in ini/initraj.cc does: motion is not up yet.
    {
        linuxcnc::IniFile inifile(emc_inifile);
        num_spindles = 1;
        axis_mask = (1 << 0) | (1 << 1) | (1 << 2);
        if (inifile) {
            num_spindles = inifile.findIntV("SPINDLES", "TRAJ", 1);
            if (auto coord = inifile.findString("COORDINATES", "TRAJ")) {
                static const std::string axes{"XYZABCUVW"};
                axis_mask = 0;
                for (auto c : *coord) {
                    size_t n = axes.find_first_of(toupper(c & 0xff));
                    if (n != std::string::npos) {
                        axis_mask |= 1 << n;
                    }
                }
            }
        }
    }

    if (num_joints < 0 || num_joints > EMCMOT_MAX_JOINTS ||
        num_spindles < 0 || num_spindles > EMCMOT_MAX_SPINDLES) {
        log_error(TASKHAL ": bad joint ({}) or spindle ({}) count\n",
                  num_joints, num_spindles);
        return -1;
    }

    comp_id = hal_init(TASKHAL);
    if (comp_id < 0) {
        log_error(TASKHAL ": hal_init() failed\n");
        return -1;
    }

    taskhal_data = (taskhal_str *) hal_malloc(sizeof(taskhal_str));
    if (taskhal_data == NULL) {
        log_error(TASKHAL ": hal_malloc() failed\n");
        hal_exit(comp_id);
        comp_id = -1;
        return -1;
    }
    taskhal_str *d = taskhal_data;
    memset(d, 0, sizeof(*d));

    CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->units_per_mm, 0.0, TASKHAL ".machine.units-per-mm"));

    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->mode_is_manual, 0, TASKHAL ".mode.is-manual"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->mode_is_auto, 0, TASKHAL ".mode.is-auto"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->mode_is_mdi, 0, TASKHAL ".mode.is-mdi"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->mode_is_joint, 0, TASKHAL ".mode.is-joint"));

    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->program_is_idle, 0, TASKHAL ".program.is-idle"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->program_is_running, 0, TASKHAL ".program.is-running"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->program_is_paused, 0, TASKHAL ".program.is-paused"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->program_os_is_on, 0, TASKHAL ".program.optional-stop.is-on"));
    CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->program_bd_is_on, 0, TASKHAL ".program.block-delete.is-on"));

    CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->mv_value, 0.0, TASKHAL ".max-velocity.value"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->fo_value, 0.0, TASKHAL ".feed-override.value"));
    CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->ro_value, 0.0, TASKHAL ".rapid-override.value"));
    for (int s = 0; s < num_spindles; s++) {
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->so_value[s], 0.0, TASKHAL ".spindle.%d.override.value", s));
    }

    for (int j = 0; j < num_joints; j++) {
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->joint_on_soft_min_limit[j], 0, TASKHAL ".joint.%d.on-soft-min-limit", j));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->joint_on_soft_max_limit[j], 0, TASKHAL ".joint.%d.on-soft-max-limit", j));
        CHK(hal_pin_new_bool(comp_id, HAL_OUT, &d->joint_override_limits[j], 0, TASKHAL ".joint.%d.override-limits", j));
    }

    for (int a = 0; a < EMCMOT_MAX_AXIS; a++) {
        if (!(axis_mask & (1 << a))) { continue; }
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->axis_pos_feedback[a], 0.0, TASKHAL ".axis.%c.pos-feedback", axis_letters[a]));
        CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->axis_pos_relative[a], 0.0, TASKHAL ".axis.%c.pos-relative", axis_letters[a]));
    }

    CHK(hal_pin_new_real(comp_id, HAL_OUT, &d->tool_diameter, 0.0, TASKHAL ".tool.diameter"));

    hal_ready(comp_id);
    return 0;
}

void emcTaskHalExit()
{
    if (comp_id >= 0) {
        hal_exit(comp_id);
        comp_id = -1;
    }
    taskhal_data = NULL;
}

// the loaded tool's entry comes with the status, as halui reads it
static double tool_diameter(const EMC_TOOL_STAT &tool)
{
    const CANON_TOOL_TABLE &loaded = tool.toolTableCurrent;
    if (tool.toolInSpindle != 0 && loaded.toolno == tool.toolInSpindle) {
        return loaded.diameter;
    }
    return 0.0;
}

void emcTaskHalUpdate(const EMC_STAT &stat)
{
    taskhal_str *d = taskhal_data;
    if (!d) {
        return;
    }
    const EMC_TASK_STAT &task = stat.task;
    const EMC_TRAJ_STAT &traj = stat.motion.traj;

    hal_set_real(d->units_per_mm, traj.linearUnits);

    hal_set_bool(d->mode_is_manual, task.mode == EMC_TASK_MODE::MANUAL);
    hal_set_bool(d->mode_is_auto, task.mode == EMC_TASK_MODE::AUTO);
    hal_set_bool(d->mode_is_mdi, task.mode == EMC_TASK_MODE::MDI);
    hal_set_bool(d->mode_is_joint, traj.mode == EMC_TRAJ_MODE::FREE);

    hal_set_bool(d->program_is_idle, task.interpState == EMC_TASK_INTERP::IDLE);
    hal_set_bool(d->program_is_running, task.interpState == EMC_TASK_INTERP::READING ||
                                        task.interpState == EMC_TASK_INTERP::WAITING);
    hal_set_bool(d->program_is_paused, task.interpState == EMC_TASK_INTERP::PAUSED);
    hal_set_bool(d->program_os_is_on, task.optional_stop_state);
    hal_set_bool(d->program_bd_is_on, task.block_delete_state);

    hal_set_real(d->mv_value, traj.maxVelocity);
    hal_set_real(d->fo_value, traj.scale);
    hal_set_real(d->ro_value, traj.rapid_scale);
    for (int s = 0; s < num_spindles; s++) {
        hal_set_real(d->so_value[s], stat.motion.spindle[s].spindle_scale);
    }

    for (int j = 0; j < num_joints; j++) {
        const EMC_JOINT_STAT &joint = stat.motion.joint[j];
        hal_set_bool(d->joint_on_soft_min_limit[j], joint.minSoftLimit);
        hal_set_bool(d->joint_on_soft_max_limit[j], joint.maxSoftLimit);
        hal_set_bool(d->joint_override_limits[j], joint.overrideLimits);
    }

    // the relative position: the offset chain taken off in reverse, the
    // tool offset, G5x, the XY rotation, G92 and the tilted work plane
    const EmcPose &act = traj.actualPosition;
    double rel[EMCMOT_MAX_AXIS];
    double rx = act.tran.x - task.g5x_offset.tran.x - task.toolOffset.tran.x;
    double ry = act.tran.y - task.g5x_offset.tran.y - task.toolOffset.tran.y;
    double rz = act.tran.z - task.g5x_offset.tran.z - task.toolOffset.tran.z;
    {
        double t = -task.rotation_xy * TO_RAD;
        double x = rx * cos(t) - ry * sin(t);
        double y = ry * cos(t) + rx * sin(t);
        rx = x - task.g92_offset.tran.x;
        ry = y - task.g92_offset.tran.y;
        rz -= task.g92_offset.tran.z;
    }
    if (task.g68_active) {
        const double *r = task.g68_rotation;
        double x = rx - task.g68_offset.tran.x;
        double y = ry - task.g68_offset.tran.y;
        double z = rz - task.g68_offset.tran.z;
        rx = r[0]*x + r[3]*y + r[6]*z;
        ry = r[1]*x + r[4]*y + r[7]*z;
        rz = r[2]*x + r[5]*y + r[8]*z;
    }
    rel[0] = rx;
    rel[1] = ry;
    rel[2] = rz;
    rel[3] = act.a - task.g5x_offset.a - task.g92_offset.a - task.toolOffset.a;
    rel[4] = act.b - task.g5x_offset.b - task.g92_offset.b - task.toolOffset.b;
    rel[5] = act.c - task.g5x_offset.c - task.g92_offset.c - task.toolOffset.c;
    rel[6] = act.u - task.g5x_offset.u - task.g92_offset.u - task.toolOffset.u;
    rel[7] = act.v - task.g5x_offset.v - task.g92_offset.v - task.toolOffset.v;
    rel[8] = act.w - task.g5x_offset.w - task.g92_offset.w - task.toolOffset.w;

    const double fb[EMCMOT_MAX_AXIS] = {
        act.tran.x, act.tran.y, act.tran.z, act.a, act.b, act.c, act.u, act.v, act.w
    };
    for (int a = 0; a < EMCMOT_MAX_AXIS; a++) {
        if (!(axis_mask & (1 << a))) { continue; }
        hal_set_real(d->axis_pos_feedback[a], fb[a]);
        hal_set_real(d->axis_pos_relative[a], rel[a]);
    }

    hal_set_real(d->tool_diameter, tool_diameter(stat.io.tool));
}
