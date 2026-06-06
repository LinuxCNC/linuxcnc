/********************************************************************
* Description:  hal_gpio.c
*               GPIO driver for Rapberry Pi and similar using the
* 		gpiod library
*
* Author: Andy Pugh
* License: GPL Version 2+
*
* Copyright (c) 2023 All rights reserved.
*
*********************************************************************
    This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the LinuxCNC project.  For more
    information, go to www.linuxcnc.org.
*/

#include "gomc_env.h"		/* cmod API */
#include "gomc_log.h"
#include <gpiod.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "config.h"             /* includes the GPIOD version 105 = 1.5 */
#if LIBGPIOD_VER >= 200
    #include <sys/stat.h>	/* to avoid following symlinks in /dev */
    #include <dirent.h>		/* to query entries in /dev */
#endif

// There isn't really any limit except for in the MP_ARRAY macros. 
#define MAX_CHAN 128

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct{
    gomc_hal_bit_t *value;
    gomc_hal_bit_t *value_not;
} hal_gpio_hal_t;

/* flags are defined such:
 * bits 0 - 4 are gpiod flags
 * OPEN_DRAIN		= BIT(0)
 * OPEN_SOURCE		= BIT(1)
 * BIAS_DISABLE 	= BIT(2)
 * PULL_DOWN		= BIT(3)
 * PULL_UP		= BIT(4)
 *
 * hal_gpio flags
 * INVERT 		= BIT(5)
 * RESET		= BIT(6)
 */

typedef struct {
    int num_lines;
    int *vals;
    int *flags;
    hal_gpio_hal_t *hal;
    struct gpiod_chip *chip;
#if LIBGPIOD_VER >= 200
    unsigned int *offsets;
    struct gpiod_line_request *lines;
#else
    struct gpiod_line_bulk *lines;
#endif
} hal_gpio_bulk_t;

typedef struct {
    // Bulk line access has to all be to the same "chip" so we have an
    // array of chips with their bulk line collections.
    gomc_hal_u32_t *reset_ns;
    int num_in_chips;
    int num_out_chips;
    hal_gpio_bulk_t *in_chips;
    hal_gpio_bulk_t *out_chips;
} hal_gpio_t;

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    const gomc_log_t *log;
    const gomc_rtapi_t *rtapi;
    int comp_id;

    int reset_active;
    long long last_reset;

    hal_gpio_t *gpio;

    char *inputs[MAX_CHAN];
    char *outputs[MAX_CHAN];
    char *invert[MAX_CHAN];
    char *reset[MAX_CHAN];
    char *opendrain[MAX_CHAN];
    char *opensource[MAX_CHAN];
    char *biasdisable[MAX_CHAN];
    char *pulldown[MAX_CHAN];
    char *pullup[MAX_CHAN];
} inst_t;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static inline long long get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void hal_gpio_read(void *arg, long period);
static void hal_gpio_write(void *arg, long period);
static void hal_gpio_reset(void *arg, long period);
static void hal_gpio_destroy(cmod_t *self);

/***********************************************************************
*                      SETUP AND EXIT CODE                             *
************************************************************************/

static int flags(inst_t *inst, char *name){
    int f = 0;
    int i;
    for (i = 0; inst->opendrain[i]; i++)
	if (strcmp(name, inst->opendrain[i]) == 0) f |= 0x1;
    for (i = 0; inst->opensource[i]; i++)
	if (strcmp(name, inst->opensource[i]) == 0) f |= 0x2;
    for (i = 0; inst->biasdisable[i]; i++)
	if (strcmp(name, inst->biasdisable[i]) == 0) f |= 0x4;
    for (i = 0; inst->pulldown[i]; i++)
	if (strcmp(name, inst->pulldown[i]) == 0) f |= 0x8;
    for (i = 0; inst->pullup[i]; i++)
	if (strcmp(name, inst->pullup[i]) == 0) f |= 0x10;
    for (i = 0; inst->invert[i]; i++)
	if (strcmp(name, inst->invert[i]) == 0) f |= 0x20;
    for (i = 0; inst->reset[i]; i++)
	if (strcmp(name, inst->reset[i]) == 0) {
	    inst->reset_active = 1;
	    f |= 0x40;
	}
    gomc_log_infof(inst->log, "hal_gpio", "line %s flags %02x", name, f);
    return f;
}

#if LIBGPIOD_VER >= 200

// used by scandir to filter file list for gpiochip devices
// rejects links (like cpiochip4 -> gpiochip0 on Pi5)
int filter(const struct dirent *entry){
    struct stat sb;
	int ret = 0;
	char path[20];
	if (snprintf(path, 19, "/dev/%s", entry->d_name) < 0)
		return 0;
	if ((lstat(path, &sb) == 0) && (!S_ISLNK(sb.st_mode)) &&
	    gpiod_is_gpiochip_device(path))
		ret = 1;
	return ret;
}

/*  API changes mean that this works differently from the previous iteration
    Now we iterate through all "chips" looking for each GPIO name in turn.
    On the initial pass we only populate the offsets array with the offset
    of each matching line
    returns the number of "bulks" required, one per chip used. */
int allocate_lines(inst_t *inst, char **names, hal_gpio_bulk_t **bulk){
    int n;
    int b = 0;
    int offset;
    char path[280];
    struct dirent **namelist;
    
// Get a list of all chips. the "filter" function identifies gpiochips
    n = scandir("/dev", &namelist, filter, alphasort);
    if (n == -1) {
	gomc_log_errorf(inst->log, "hal_gpio", "No valid gpio devices recognized by gpiod in /dev");
	return -1;
    }

    while (n--){
	gomc_log_debugf(inst->log, "hal_gpio", "hal_gpio processing chip %s", namelist[n]->d_name);
	int l = 0;

	*bulk = inst->rtapi->realloc(inst->rtapi->ctx, *bulk, sizeof(hal_gpio_bulk_t) * (b + 1));
	(*bulk)[b].offsets = NULL;
	(*bulk)[b].flags = NULL;
	(*bulk)[b].vals = NULL;

	snprintf(path, sizeof(path), "/dev/%s", namelist[n]->d_name);
	(*bulk)[b].chip = gpiod_chip_open(path);
	for (int i = 0; names[i]; i++){
	    offset = gpiod_chip_get_line_offset_from_name((*bulk)[b].chip, names[i]);
	    if (offset >= 0){
		gomc_log_debugf(inst->log, "hal_gpio", "hal_gpio processing line %s found at offset %i", names[i], offset);
		(*bulk)[b].offsets = inst->rtapi->realloc(inst->rtapi->ctx, (*bulk)[b].offsets, (l + 1) * sizeof(offset));
		(*bulk)[b].offsets[l] = offset;
		(*bulk)[b].flags = inst->rtapi->realloc(inst->rtapi->ctx, (*bulk)[b].flags, (l + 1) * sizeof(offset));
		(*bulk)[b].flags[l] = flags(inst, names[i]);
		(*bulk)[b].vals = inst->rtapi->realloc(inst->rtapi->ctx, (*bulk)[b].vals, (l + 1) * sizeof(int));
		(*bulk)[b].num_lines = ++l;
	    }
	    for (int o = 0; o < l; o++) gomc_log_debugf(inst->log, "hal_gpio", "chip %i offset %i = %i", b, o, (*bulk)[b].offsets[o]);
	}
	if (l > 0) {
	    b ++; // move to a new hal_gpio_bulk_t next loop
	} else {
	    gpiod_chip_close((*bulk)[b].chip); // not using this chip
	}
    }
    return b;
}

int setup_lines(hal_gpio_bulk_t *bulk, enum gpiod_line_direction direction, const gomc_log_t *log){
    char *consumer = "LinuxCNC";
//    unsigned int offset[1];
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    int ret;

    // Initially collect all input or output lines into one "request"
    // (formerly "bulk"), then reconfigure settings for each line

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();
    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, consumer);

    gpiod_line_settings_set_direction(settings, direction);
    gpiod_line_config_add_line_settings(line_cfg, bulk->offsets, bulk->num_lines, settings);
    bulk->lines = gpiod_chip_request_lines(bulk->chip, req_cfg, line_cfg);

    for (int i = 0; i < bulk->num_lines; i++) {
	//gpiod_line_config_reset(line_cfg);
	gpiod_line_settings_reset(settings);
	gpiod_line_settings_set_direction(settings, direction);
	if (bulk->flags[i] & 0x01) gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_OPEN_DRAIN);
	if (bulk->flags[i] & 0x02) gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_OPEN_SOURCE);
	if (bulk->flags[i] & 0x04) gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_DISABLED);
	if (bulk->flags[i] & 0x08) gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);
	if (bulk->flags[i] & 0x10) gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);

	ret = gpiod_line_config_add_line_settings(line_cfg, &bulk->offsets[i], 1, settings);
	gomc_log_debugf(log, "hal_gpio", "add_line_settings returned %d errno=%d (%s)",
			ret, errno, strerror(errno));
	if (ret) {
	    return -1;
	} else {
	    ret = gpiod_line_request_reconfigure_lines(bulk->lines, line_cfg);
	    gomc_log_debugf(log, "hal_gpio", "reconfigure returned %d errno=%d (%s)",
			    ret, errno, strerror(errno));
	    if (ret) {
		return -1;
	    }
	}
    }

    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_request_config_free(req_cfg);

    return 0;
}

#else

int build_chips_collection(inst_t *inst, char **names, hal_gpio_bulk_t **ptr){
    int c, i;
    int count = 0;
    struct gpiod_chip *temp_chip;
    struct gpiod_line *temp_line;
    char *name;

    for (i = 0; names[i]; i++){
	name = names[i];
	temp_line = gpiod_line_find(name);
	if (!temp_line) {
		gomc_log_errorf(inst->log, "hal_gpio", "The GPIO line %s can not be found", name);
		return -EINVAL;
	}
	temp_chip = gpiod_line_get_chip(temp_line);
	for (c = 0; c < count
		    && (strcmp(gpiod_chip_name((*ptr)[c].chip), gpiod_chip_name(temp_chip))
		    // max of 64 lines per bulk, so carry on to another "chip" if full
		    ||  (*ptr)[c].num_lines > 63);
		    c++){
	}

	if (c >= count){
		(count)++;
		*ptr = inst->rtapi->realloc(inst->rtapi->ctx, *ptr, sizeof(hal_gpio_bulk_t) * count);
		(*ptr)[c].chip = NULL;
		(*ptr)[c].flags = NULL;
		(*ptr)[c].vals = NULL;
		(*ptr)[c].num_lines = 0;
		(*ptr)[c].chip = gpiod_line_get_chip(temp_line);
		(*ptr)[c].lines = inst->rtapi->calloc(inst->rtapi->ctx, sizeof(*(*ptr)[c].lines));
		gpiod_line_bulk_init((*ptr)[c].lines);
		gomc_log_infof(inst->log, "hal_gpio", "added chip %s index %i", gpiod_chip_name((*ptr)[c].chip), c);
	}
	gomc_log_infof(inst->log, "hal_gpio", "adding IO line %s to chip %i", name, c);
	temp_line = gpiod_chip_find_line((*ptr)[c].chip, name);
	(*ptr)[c].num_lines++;
	(*ptr)[c].flags = inst->rtapi->realloc(inst->rtapi->ctx, (*ptr)[c].flags, (*ptr)[c].num_lines * sizeof(int));
	(*ptr)[c].flags[(*ptr)[c].num_lines - 1] = flags(inst, name);
#if LIBGPIOD_VER >= 105
	gpiod_line_set_flags(temp_line, (*ptr)[c].flags[(*ptr)[c].num_lines - 1]);
#endif
	(*ptr)[c].vals = inst->rtapi->realloc(inst->rtapi->ctx, (*ptr)[c].vals, (*ptr)[c].num_lines * sizeof(int));
	gpiod_line_bulk_add((*ptr)[c].lines, temp_line);
    }

    return count;
}

#endif

const char* get_line_name(hal_gpio_bulk_t *bulk, int index){
#if LIBGPIOD_VER >= 200
    struct gpiod_line_info *info;
    info = gpiod_chip_get_line_info(bulk->chip, bulk->offsets[index]);
    return gpiod_line_info_get_name(info);
#else
    return gpiod_line_name(gpiod_line_bulk_get_line(bulk->lines, index));
#endif
}

static void parse_argv(inst_t *inst, int argc, const char **argv) {
    int in_idx = 0, out_idx = 0, inv_idx = 0, rst_idx = 0;
    int od_idx = 0, os_idx = 0, bd_idx = 0, pd_idx = 0, pu_idx = 0;

    for (int i = 0; i < argc; i++) {
	if (strncmp(argv[i], "inputs=", 7) == 0 && in_idx < MAX_CHAN) {
	    inst->inputs[in_idx++] = (char *)argv[i] + 7;
	} else if (strncmp(argv[i], "outputs=", 8) == 0 && out_idx < MAX_CHAN) {
	    inst->outputs[out_idx++] = (char *)argv[i] + 8;
	} else if (strncmp(argv[i], "invert=", 7) == 0 && inv_idx < MAX_CHAN) {
	    inst->invert[inv_idx++] = (char *)argv[i] + 7;
	} else if (strncmp(argv[i], "reset=", 6) == 0 && rst_idx < MAX_CHAN) {
	    inst->reset[rst_idx++] = (char *)argv[i] + 6;
	} else if (strncmp(argv[i], "opendrain=", 10) == 0 && od_idx < MAX_CHAN) {
	    inst->opendrain[od_idx++] = (char *)argv[i] + 10;
	} else if (strncmp(argv[i], "opensource=", 11) == 0 && os_idx < MAX_CHAN) {
	    inst->opensource[os_idx++] = (char *)argv[i] + 11;
	} else if (strncmp(argv[i], "biasdisable=", 12) == 0 && bd_idx < MAX_CHAN) {
	    inst->biasdisable[bd_idx++] = (char *)argv[i] + 12;
	} else if (strncmp(argv[i], "pulldown=", 9) == 0 && pd_idx < MAX_CHAN) {
	    inst->pulldown[pd_idx++] = (char *)argv[i] + 9;
	} else if (strncmp(argv[i], "pullup=", 7) == 0 && pu_idx < MAX_CHAN) {
	    inst->pullup[pu_idx++] = (char *)argv[i] + 7;
	}
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;
    const gomc_hal_t *hal = env->hal;
    int retval = 0;
    int i, c;
    char hal_name[GOMC_HAL_NAME_LEN + 1];
    const char *line_name;

    inst_t *inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;
    inst->env = env;
    inst->log = env->log;
    inst->rtapi = env->rtapi;

    parse_argv(inst, argc, argv);

gomc_log_infof(inst->log, "hal_gpio", "Libgpiod is %i", LIBGPIOD_VER);

    
    int r = hal->init(hal->ctx, "hal_gpio", env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (r < 0) {
        gomc_log_errorf(inst->log, "hal_gpio", "ERROR: hal_init() failed");
        inst->rtapi->free(inst->rtapi->ctx, inst);
        return r;
    }
    inst->comp_id = r;

    // allocate shared memory for the base struct
    inst->gpio = inst->rtapi->calloc(inst->rtapi->ctx, sizeof(hal_gpio_t));
    if (inst->gpio == 0) {
        gomc_log_errorf(inst->log, "hal_gpio",
                "component: Out of Memory");
        goto fail0;
    }

    hal_gpio_t *gpio = inst->gpio;

    // Create Inputs
    gpio->num_in_chips = 0;
    gpio->num_out_chips = 0;
    gpio->in_chips = NULL; // so that realloc knows that they need malloc first time throigh
    gpio->out_chips = NULL;

#if LIBGPIOD_VER >= 200
    gpio->num_in_chips = allocate_lines(inst, inst->inputs, &gpio->in_chips);
# else
    gpio->num_in_chips = build_chips_collection(inst, inst->inputs, &gpio->in_chips);
#endif
    if (gpio->num_in_chips < 0){
	gomc_log_errorf(inst->log, "hal_gpio", "Failed to identify all specified input pins");
	goto fail0;
    }
    for (c = 0; c < gpio->num_in_chips; c++){
    // Set up the lines parameters and the bulk reads
#if LIBGPIOD_VER >= 200
	retval = setup_lines(&gpio->in_chips[c], GPIOD_LINE_DIRECTION_INPUT, inst->log);
#else
	retval = gpiod_line_request_bulk_input(gpio->in_chips[c].lines, "linuxcnc");
#endif
	if (retval < 0) {
	    gomc_log_errorf(inst->log, "hal_gpio", "Failed to register input pin collection");
	    goto fail0;
	}

	gpio->in_chips[c].hal = hal->malloc(hal->ctx, gpio->in_chips[c].num_lines * sizeof(hal_gpio_hal_t));
	for (i = 0; i < gpio->in_chips[c].num_lines; i++){
	    line_name = get_line_name(&gpio->in_chips[c], i);
	    retval += gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &(gpio->in_chips[c].hal[i].value), inst->comp_id, "hal_gpio.%s-in", line_name);
	    retval += gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &(gpio->in_chips[c].hal[i].value_not), inst->comp_id, "hal_gpio.%s-in-not", line_name);
	}
	if (retval < 0){
	    gomc_log_errorf(inst->log, "hal_gpio", "Failed to allocate GPIO input HAL pins");
	    goto fail0;
	}
	    
    }
    

    // Create Outputs

#if LIBGPIOD_VER >= 200
    gpio->num_out_chips = allocate_lines(inst, inst->outputs, &gpio->out_chips);
# else
    gpio->num_out_chips = build_chips_collection(inst, inst->outputs, &gpio->out_chips);
#endif
    if (gpio->num_in_chips < 0){
	gomc_log_errorf(inst->log, "hal_gpio", "Failed to identify all specified output pins");
	goto fail0;
    }
    for (c = 0; c < gpio->num_out_chips; c++){
	// Set up the lines parameters and the bulk reads
#if LIBGPIOD_VER >= 200
	retval = setup_lines(&gpio->out_chips[c], GPIOD_LINE_DIRECTION_OUTPUT, inst->log);
#else
	retval = gpiod_line_request_bulk_output(gpio->out_chips[c].lines, "linuxcnc", gpio->out_chips[c].vals);
#endif
	if (retval < 0){
	    gomc_log_errorf(inst->log, "hal_gpio", "Failed to register output pin collection");
	    goto fail0;
	}

	gpio->out_chips[c].hal = hal->malloc(hal->ctx, gpio->out_chips[c].num_lines * sizeof(hal_gpio_hal_t));
	for (i = 0; i < gpio->out_chips[c].num_lines; i++){
	    line_name = get_line_name(&gpio->out_chips[c], i);
	    retval += gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &(gpio->out_chips[c].hal[i].value), inst->comp_id, "hal_gpio.%s-out", line_name);
	}
	if (retval < 0){
	    gomc_log_errorf(inst->log, "hal_gpio", "Failed to allocate GPIO output HAL pins");
	    goto fail0;
	}
    }

    snprintf(hal_name, GOMC_HAL_NAME_LEN, "hal_gpio.read");
    retval += hal->export_funct(hal->ctx, hal_name, hal_gpio_read, inst, 0, 0, inst->comp_id);
    snprintf(hal_name, GOMC_HAL_NAME_LEN, "hal_gpio.write");
    retval += hal->export_funct(hal->ctx, hal_name, hal_gpio_write, inst, 0, 0, inst->comp_id);

    if (inst->reset_active){
	gpio->reset_ns = hal->malloc(hal->ctx, sizeof(gomc_hal_u32_t));
	snprintf(hal_name, GOMC_HAL_NAME_LEN, "hal_gpio.reset");
	retval += gomc_hal_param_u32_newf(hal, GOMC_HAL_RW, gpio->reset_ns, inst->comp_id, "hal_gpio.reset_ns");
	retval += hal->export_funct(hal->ctx, hal_name, hal_gpio_reset, inst, 0, 0, inst->comp_id);
    }
    if (retval < 0){
	gomc_log_errorf(inst->log, "hal_gpio", "failed to export functions");
	goto fail0;
    }
    hal->ready(hal->ctx, inst->comp_id);

    inst->cmod.Destroy = hal_gpio_destroy;
    inst->cmod.priv = inst;
    *out = &inst->cmod;
    return 0;

fail0:
    hal->exit(hal->ctx, inst->comp_id);
    inst->rtapi->free(inst->rtapi->ctx, inst);
    return -1;
}

/**************************************************************
* REALTIME PORT READ/WRITE FUNCTION                                *
**************************************************************/

static void hal_gpio_read(void *arg, long period)
{
    (void)period;
    inst_t *inst = arg;
    hal_gpio_t *gpio = inst->gpio;
    int i, c;
    for (c = 0; c < gpio->num_in_chips; c++){
#if LIBGPIOD_VER >= 200
	gpiod_line_request_get_values(gpio->in_chips[c].lines, gpio->in_chips[c].vals);
#else
	gpiod_line_get_value_bulk(gpio->in_chips[c].lines, gpio->in_chips[c].vals);
#endif
	for (i = 0; i < gpio->in_chips[c].num_lines; i++){
	   *(gpio->in_chips[c].hal[i].value) = gpio->in_chips[c].vals[i];
	   *(gpio->in_chips[c].hal[i].value_not) = ! gpio->in_chips[c].vals[i];
	}
    }
}

static void hal_gpio_write(void *arg, long period)
{
    (void)period;
    inst_t *inst = arg;
    hal_gpio_t *gpio = inst->gpio;
    int i, c;
    for (c = 0; c < gpio->num_out_chips; c++){
	for (i = 0; i < gpio->out_chips[c].num_lines; i++){
	    if (gpio->out_chips[c].flags[i] & 0x20){
		gpio->out_chips[c].vals[i] = ! *(gpio->out_chips[c].hal[i].value);
	    } else {
		gpio->out_chips[c].vals[i] = *(gpio->out_chips[c].hal[i].value);
	    }
	}
#if LIBGPIOD_VER >200
	gpiod_line_request_set_values(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#else
	gpiod_line_set_value_bulk(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#endif
    }
    // store the time for the reset function
    inst->last_reset = get_time_ns();
}

static void hal_gpio_reset(void *arg, long period)
{
    inst_t *inst = arg;
    hal_gpio_t *gpio = inst->gpio;
    int i, c;
    long long deadline;
    for (c = 0; c < gpio->num_out_chips; c++){
	for (i = 0; i < gpio->out_chips[c].num_lines; i++){
	    if ((gpio->out_chips[c].flags[i] & 0x60) == 0x60){ // inverted and reset
		gpio->out_chips[c].vals[i] = 1;
	    } else if ((gpio->out_chips[c].flags[i] & 0x60) == 0x40){ // only reset
		gpio->out_chips[c].vals[i] = 0;
	    }
	}
	if (*gpio->reset_ns > period/4) *gpio->reset_ns = period/4;
	deadline = inst->last_reset + *gpio->reset_ns;
        while(get_time_ns() < deadline) {} // busy-wait!
#if LIBGPIOD_VER > 200
	gpiod_line_request_set_values(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#else
	gpiod_line_set_value_bulk(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#endif
    }
}

static void hal_gpio_destroy(cmod_t *self) {
    inst_t *inst = self->priv;
    const gomc_hal_t *hal = inst->env->hal;
    hal_gpio_t *gpio = inst->gpio;
    int c;

    for (c = 0; c < gpio->num_in_chips; c++){
#if LIBGPIOD_VER >= 200
	gpiod_line_request_release(gpio->in_chips[c].lines);
#else
	inst->rtapi->free(inst->rtapi->ctx, gpio->in_chips[c].lines);
#endif
	gpiod_chip_close(gpio->in_chips[c].chip);
    }
    for (c = 0; c < gpio->num_out_chips; c++){
#if LIBGPIOD_VER >= 200
	gpiod_line_request_release(gpio->out_chips[c].lines);
#else
	inst->rtapi->free(inst->rtapi->ctx, gpio->out_chips[c].lines);
#endif
	gpiod_chip_close(gpio->out_chips[c].chip);
    }
    inst->rtapi->free(inst->rtapi->ctx, gpio->in_chips);
    inst->rtapi->free(inst->rtapi->ctx, gpio->out_chips);
    inst->rtapi->free(inst->rtapi->ctx, gpio);
    hal->exit(hal->ctx, inst->comp_id);
    inst->rtapi->free(inst->rtapi->ctx, inst);
}
