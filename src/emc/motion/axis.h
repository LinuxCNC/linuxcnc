#ifndef AXIS_H
#define AXIS_H

#include "rtapi_bool.h"
#include "hal.h"
#include "gomc_hal.h"
#include "gomc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque per-instance axis state (allocated by axis_inst_new, freed by axis_inst_free). */
typedef struct axis_inst axis_inst_t;

axis_inst_t *axis_inst_new(void);
void axis_inst_free(axis_inst_t *ai);

void axis_init_all(axis_inst_t *ai);
void axis_initialize_external_offsets(axis_inst_t *ai);
int axis_init_hal_io(axis_inst_t *ai, const gomc_hal_t *hal, const gomc_log_t *log,
                     int comp_id, const char *pin_prefix);

void axis_handle_jogwheels(axis_inst_t *ai, bool motion_teleop_flag, bool motion_enable_flag, bool homing_is_active);
bool axis_plan_external_offsets(axis_inst_t *ai, double servo_period, bool motion_enable_flag, bool all_homed);
void axis_check_constraints(axis_inst_t *ai, double pos[], int failing_axes[]);

void axis_jog_cont(axis_inst_t *ai, int axis_num, double vel, long servo_period);
void axis_jog_incr(axis_inst_t *ai, int axis_num, double offset, double vel, long servo_period);
void axis_jog_abs(axis_inst_t *ai, int axis_num, double offset, double vel);
bool axis_jog_abort_all(axis_inst_t *ai, bool immediate);
bool axis_jog_abort(axis_inst_t *ai, int axis_num, bool immediate);
bool axis_jog_is_active(axis_inst_t *ai);

void axis_output_to_hal(axis_inst_t *ai, double *pcmd_p[]);

void axis_set_max_pos_limit(axis_inst_t *ai, int axis_num, double maxLimit);
void axis_set_min_pos_limit(axis_inst_t *ai, int axis_num, double minLimit);
void axis_set_vel_limit(axis_inst_t *ai, int axis_num, double vel);
void axis_set_acc_limit(axis_inst_t *ai, int axis_num, double acc);
void axis_set_ext_offset_vel_limit(axis_inst_t *ai, int axis_num, double vel);
void axis_set_ext_offset_acc_limit(axis_inst_t *ai, int axis_num, double acc);
void axis_set_locking_joint(axis_inst_t *ai, int axis_num, int joint);

double axis_get_min_pos_limit(axis_inst_t *ai, int axis_num);
double axis_get_max_pos_limit(axis_inst_t *ai, int axis_num);
double axis_get_vel_limit(axis_inst_t *ai, int axis_num);
double axis_get_acc_limit(axis_inst_t *ai, int axis_num);
int axis_get_locking_joint(axis_inst_t *ai, int axis_num);
double axis_get_compound_velocity(axis_inst_t *ai);
double axis_get_ext_offset_curr_pos(axis_inst_t *ai, int axis_num);

double axis_get_teleop_vel_cmd(axis_inst_t *ai, int axis_num);

void axis_sync_teleop_tp_to_carte_pos(axis_inst_t *ai, int extfactor, double *pcmd_p[]);
void axis_sync_carte_pos_to_teleop_tp(axis_inst_t *ai, int extfactor, double *pcmd_p[]);
void axis_apply_ext_offsets_to_carte_pos(axis_inst_t *ai, int extfactor, double *pcmd_p[]);

int axis_update_coord_with_bound(axis_inst_t *ai, double *pcmd_p[], double servo_period);

int axis_calc_motion(axis_inst_t *ai, double servo_period);


#ifdef __cplusplus
}
#endif
#endif /* AXIS_H */
