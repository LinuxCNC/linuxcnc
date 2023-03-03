/*
 * limit_axis.c
 *
 * Limit axis to certain limits at varying inputs
 *   For example on a spindle with C rotation, to avoid hitting a
 *   gantry when the height is above Z-10
 *    - Use Z axis as feedback
 *    - Use 2 ranges (0,-10) (-10, -40)
 *    - When Z is above 10, C rotation is limited to range 0
 *    - When Z is below 10, C rotation is limited to range 1
 *
 *   Inputs
 *     - num_axis = Number of Axis Min 1, Max 10 to control (Default 1)
 *     - num_range = Number of Ranges per axis Min 1, Max 10 (Default 2)
 *
 *   Usage:
 *   loadrt limit_axis num_axis=# num_range=#
 *
 *   Caveats
 *     - Searches ranges from 0 to 9 and will take the first range that matches
 *     - Ranges are inclusive min_range <= feedback <= max_range
 *     - Max can not be less than minimum for any group
 *     - Sticky indicates, to not check other ranges if the feedback is still in the current range
 *     - Enable allows for a range to be turned on/off, for cases such as avoiding a tool changer, but allowing sometimes
 *
 */




#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "rtapi_errno.h"
#include "rtapi_string.h"
#include "limits.h"
#include "math.h"

/* module information */
MODULE_AUTHOR("Chad Woitas");
MODULE_DESCRIPTION("Limit Axis");
MODULE_LICENSE("GPL");
#define MODNAME "Axis Limit"
#define MAX_CHAN 10
static int num_axis;	/* number of axis to control */
static int default_num_axis = 1;
static int default_num_range = 2;
static int num_range; /* number of positions per axis*/

RTAPI_MP_INT(num_axis, "Number of Axis");
RTAPI_MP_INT(num_range, "Number of Positions per Axis");


typedef struct {
    hal_float_t *min_limit;
    hal_float_t *max_limit;
    hal_float_t *min_range;
    hal_float_t *max_range;
    hal_bit_t   *range_error;     // Error in Range inputs, used as a flag
    hal_bit_t   *limit_error;     // Error in Range inputs, used as a flag
    hal_bit_t   *sticky;          // Improve perfomance time, no other ranges are checked while in limits of this range
    hal_bit_t   *enable;          // Enable/Disable this range
} limit_axis;

typedef struct {
    hal_float_t *min_limit_default; // All Limits Default to this if not assigned
    hal_float_t *max_limit_default; // All Limits Default to this if not assigned
    hal_bit_t   *default_error;     // Error in Default inputs, used as a flag
    hal_float_t *min_output;        // Current Limit
    hal_float_t *max_output;        // Current Limit
    hal_u32_t   *current_range;     // Current Range that the limit corresponds to
    hal_float_t *fb;                // Feedback from joint
    hal_bit_t   *no_range_error;    // Current feedback does not have a limit

    limit_axis *limit_axis_array[MAX_CHAN];
} limit_axis_pins;

static limit_axis_pins *limitAxisPins[MAX_CHAN];
static int comp_id;
static int export_pins(int num, limit_axis_pins *addr, char* prefix);
static void update(void *arg, long period);

int rtapi_app_main(void){
  int ret = -1;

  if (!num_axis){
    num_axis = default_num_axis;
  }

  if (!num_range){
    num_range = default_num_range;
  }

  if(num_range < 0 || num_range > MAX_CHAN){
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: Invalid Range: %i\n", MODNAME, num_range);
    return -1;
  }

  if(num_axis < 0 || num_axis > MAX_CHAN){
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: Invalid Axis: %i\n", MODNAME, num_axis);
    return -1;
  }

  comp_id = hal_init("limit_axis");
  if (comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", MODNAME);
    return -1;
  }

  for(int n=0; n<num_axis; n++) {
    limitAxisPins[n] = hal_malloc(sizeof (limit_axis_pins));

    if (limitAxisPins[n] == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", MODNAME);
      hal_exit(comp_id);
      return -1;
    }

    char buf[HAL_NAME_LEN + 1];
    rtapi_snprintf(buf, sizeof(buf), "alim.%d", n);
    ret = export_pins(num_range, limitAxisPins[n], buf);
    if (ret != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: %d var export failed\n", MODNAME, n);
      hal_exit(comp_id);
      return -1;
    }
  }

  hal_ready(comp_id);
  return 0;
}

static int export_pins(int num, limit_axis_pins *addr, char* prefix){
  int ret = 0;
  ret += hal_pin_float_newf(HAL_IN,  &(addr->min_limit_default), comp_id, "%s.min-limit-default", prefix);
  ret += hal_pin_float_newf(HAL_IN,  &(addr->max_limit_default), comp_id, "%s.max-limit-default", prefix);
  ret += hal_pin_bit_newf(HAL_OUT,   &(addr->default_error),     comp_id, "%s.error-default",     prefix);
  ret += hal_pin_bit_newf(HAL_OUT,   &(addr->no_range_error),    comp_id, "%s.error-no-range",    prefix);
  ret += hal_pin_float_newf(HAL_OUT, &(addr->min_output),        comp_id, "%s.min-output",        prefix);
  ret += hal_pin_float_newf(HAL_OUT, &(addr->max_output),        comp_id, "%s.max-output",        prefix);
  ret += hal_pin_float_newf(HAL_IN,  &(addr->fb),                comp_id, "%s.fb",                prefix);
  ret += hal_pin_u32_newf(HAL_OUT,   &(addr->current_range),     comp_id, "%s.current-range",     prefix);

  for(int i=0; i<num; i++){
    addr->limit_axis_array[i] = hal_malloc(sizeof(limit_axis));

    if (addr->limit_axis_array[i]  == 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", MODNAME);
      hal_exit(comp_id);
      return -1;
    }

    ret += hal_pin_float_newf(HAL_IN, &(addr->limit_axis_array[i]->min_limit), comp_id, "%s.%i.min-limit", prefix, i);
    ret += hal_pin_float_newf(HAL_IN, &(addr->limit_axis_array[i]->max_limit), comp_id, "%s.%i.max-limit", prefix, i);
    ret += hal_pin_float_newf(HAL_IN, &(addr->limit_axis_array[i]->min_range), comp_id, "%s.%i.min-range", prefix, i);
    ret += hal_pin_float_newf(HAL_IN, &(addr->limit_axis_array[i]->max_range), comp_id, "%s.%i.max-range", prefix, i);
    ret += hal_pin_bit_newf(HAL_IN,   &(addr->limit_axis_array[i]->enable),    comp_id, "%s.%i.enable", prefix, i);
    ret += hal_pin_bit_newf(HAL_IN,   &(addr->limit_axis_array[i]->sticky),    comp_id, "%s.%i.sticky", prefix, i);
    ret += hal_pin_bit_newf(HAL_OUT,  &(addr->limit_axis_array[i]->range_error), comp_id, "%s.%i.error-range", prefix, i);
    ret += hal_pin_bit_newf(HAL_OUT,  &(addr->limit_axis_array[i]->limit_error), comp_id, "%s.%i.error-limit", prefix, i);

    *addr->limit_axis_array[i]->enable = 1;
    *addr->limit_axis_array[i]->sticky = 1;
  }

  // Export update function to hal for this axis
  int use_floating_point = 1;
  int no_reentrant = 0;
  char buf[HAL_NAME_LEN + 1];
  rtapi_snprintf(buf, sizeof(buf), "%s.update", prefix);

  ret += hal_export_funct(buf, update, addr, use_floating_point, no_reentrant, comp_id);

  // Report any error on exporting pins
  if (ret != 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: Export Limit Axis pins Failed\n", MODNAME);
  }

  return ret;
}

static void update(void *arg, long period) {
  limit_axis_pins *axis;
  axis = arg;

  // Check limits
  if (*axis->max_limit_default < *axis->min_limit_default){
    if (! *axis->default_error){
      *axis->default_error = 1;
      rtapi_print_msg(RTAPI_MSG_ERR,
                      "%s: Error in default settings! Min: %f > Max: %f \n",
                      MODNAME,
                      *axis->max_limit_default,
                      *axis->min_limit_default);

    }
    // Skip updating this axis.
    return;
  }

  // If we made it to here clear the error
  *axis->default_error = 0;

  // Check sticky bit
  if(*axis->limit_axis_array[*axis->current_range]->sticky){
    if (*axis->limit_axis_array[*axis->current_range]->min_range <= *axis->fb &&
        *axis->fb <= *axis->limit_axis_array[*axis->current_range]->max_range &&
        *axis->limit_axis_array[*axis->current_range]->enable) {

      *axis->max_output = *axis->limit_axis_array[*axis->current_range]->max_limit;
      *axis->min_output = *axis->limit_axis_array[*axis->current_range]->min_limit;

      return;
    }
  }

  // Check all ranges
  for (int i=0; i<num_range; i++){
    if (*axis->limit_axis_array[i]->max_range < *axis->limit_axis_array[i]->min_range){

      if(!*axis->limit_axis_array[*axis->current_range]->enable){
        continue;
      }

      // Throw Error and skip this range
      if (! *axis->limit_axis_array[i]->range_error ){
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: Error in range %d settings! Min: %f > Max: %f \n",
                        MODNAME,
                        i,
                        *axis->limit_axis_array[i]->max_range,
                        *axis->limit_axis_array[i]->min_range);
        *axis->limit_axis_array[i]->range_error = 1;
      }
      continue;
    }
    // If we made it to here clear the error
    *axis->limit_axis_array[i]->range_error = 0;

    if (*axis->limit_axis_array[i]->max_limit < *axis->limit_axis_array[i]->min_limit){
      // Throw Error and skip this range
      if (! *axis->limit_axis_array[i]->limit_error ){
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: Error in limit %d settings! Min: %f > Max: %f \n",
                        MODNAME,
                        i,
                        *axis->limit_axis_array[i]->max_limit,
                        *axis->limit_axis_array[i]->min_limit);
        *axis->limit_axis_array[i]->limit_error = 1;
      }
      continue;
    }
    // If we made it to here clear the error
    *axis->limit_axis_array[i]->limit_error = 0;

    // Check position, take first range that it fits in.
    if(*axis->limit_axis_array[i]->min_range <= *axis->fb && *axis->fb <= *axis->limit_axis_array[i]->max_range) {
      *axis->max_output = *axis->limit_axis_array[i]->max_limit;
      *axis->min_output = *axis->limit_axis_array[i]->min_limit;
      // Found our limits, stop looking
      *axis->no_range_error = 0;
      if (i != *axis->current_range){
        rtapi_print_msg(RTAPI_MSG_INFO, "%s: Switching to Range %d\n", MODNAME, i);
        *axis->current_range = i;
      }
      return;
    }
  }

  // If we've looked through all our ranges, then it's not there
  if (!*axis->no_range_error){
    *axis->no_range_error = 1;
    rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: No Range found corresponding to feedback", MODNAME);

  }

}

