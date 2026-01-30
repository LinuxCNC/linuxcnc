/********************************************************************
* Description:  hal_gpio.c
*               GPIO driver for Rapberry Pi and similar using the
* 		gpiod library
*
* Author: Andy Pugh
* License: GPL Version 2+
*
* Copyright (c) 2023 / 2025 All rights reserved.
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

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_slab.h"  	/* kmalloc() */
#include "hal.h"		/* HAL public API decls */
#include <gpiod.h>
#include <string.h>
#include "config.h"             /* includes the GPIOD version 105 = 1.5 */
#if LIBGPIOD_VER >= 200
    #include <sys/stat.h>	/* to avoid following symlinks in /dev */
    #include <dirent.h>		/* to query entries in /dev */
#endif

MODULE_AUTHOR("Andy Pugh");
MODULE_DESCRIPTION("GPIO driver using gpiod / libgpiod");
MODULE_LICENSE("GPL");

static unsigned long ns2tsc_factor;
#define ns2tsc(x) (((x) * (unsigned long long)ns2tsc_factor) >> 12)

// There isn't really any limit except for in the MP_ARRAY macros. 
#define MAX_CHAN 128

char *inputs[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(inputs, MAX_CHAN, "list of pins to use for input");
char *outputs[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(outputs, MAX_CHAN, "list of pins to use for output");
char *invert[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(invert, MAX_CHAN, "set as inverted");
char *reset[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(reset, MAX_CHAN, "add to reset list");
char *opendrain[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(opendrain, MAX_CHAN, "set OPEN_DRAIN flag");
char *opensource[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(opensource, MAX_CHAN, "set OPEN_SOURCE flag");
char *biasdisable[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(biasdisable, MAX_CHAN, "set BIAS_DISABLE flag");
char *pulldown[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(pulldown, MAX_CHAN, "set BIAS_PULL_DOWN flag");
char *pullup[MAX_CHAN];
RTAPI_MP_ARRAY_STRING(pullup, MAX_CHAN, "set BIAS_PULL_UP flag");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct{
    hal_bit_t *value;
    hal_bit_t *value_not;
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
    hal_u32_t *reset_ns;
    int num_in_chips;
    int num_out_chips;
    hal_gpio_bulk_t *in_chips;
    hal_gpio_bulk_t *out_chips;
} hal_gpio_t;

void rtapi_app_exit(void);

static int comp_id;
static hal_gpio_t *gpio;
static int reset_active;
static long long last_reset;

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static void hal_gpio_read(void *arg, long period);
static void hal_gpio_write(void *arg, long period);
static void hal_gpio_reset(void *arg, long period);

/***********************************************************************
*                      SETUP AND EXIT CODE                             *
************************************************************************/

int flags(char *name){
    int f = 0;
    int i;
    for (i = 0; opendrain[i]; i++)
	if (strcmp(name, opendrain[i]) == 0) f |= 0x1;
    for (i = 0; opensource[i]; i++)
	if (strcmp(name, opensource[i]) == 0) f |= 0x2;
    for (i = 0; biasdisable[i]; i++)
	if (strcmp(name, biasdisable[i]) == 0) f |= 0x4;
    for (i = 0; pulldown[i]; i++)
	if (strcmp(name, pulldown[i]) == 0) f |= 0x8;
    for (i = 0; pullup[i]; i++)
	if (strcmp(name, pullup[i]) == 0) f |= 0x10;
    for (i = 0; invert[i]; i++)
	if (strcmp(name, invert[i]) == 0) f |= 0x20;
    for (i = 0; reset[i]; i++)
	if (strcmp(name, reset[i]) == 0) {
	    reset_active = 1;
	    f |= 0x40;
	}
    rtapi_print_msg(RTAPI_MSG_INFO,"line %s flags %02x\n", name, f);
    return f;
}

#if LIBGPIOD_VER >= 200

// used by scandir to filter file list for gpiochip devices
// rejects links (like cpiochip4 -> gpiochip0 on Pi5)
int filter(const struct dirent *entry){
    struct stat sb;
	int ret = 0;
	char path[20];
	if (rtapi_snprintf(path, 19, "/dev/%s", entry->d_name) < 0)
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
int allocate_lines(char **names, hal_gpio_bulk_t **bulk){
    int n;
    int b = 0;
    int offset;
    char path[32];
    struct dirent **namelist;
    
// Get a list of all chips. the "filter" function identifies gpiochips
    n = scandir("/dev", &namelist, filter, alphasort);
    if (n == -1) {
	rtapi_print_msg(RTAPI_MSG_ERR, "No valid gpio devices recognized by gpiod in /dev");
	return -1;
    }

    while (n--){
	rtapi_print_msg(RTAPI_MSG_DBG, "hal_gpio processing chip %s\n", namelist[n]->d_name);
	int l = 0;

	*bulk = rtapi_krealloc(*bulk, sizeof(hal_gpio_bulk_t) * (b + 1), RTAPI_GFP_KERNEL);
	(*bulk)[b].offsets = NULL;
	(*bulk)[b].flags = NULL;
	(*bulk)[b].vals = NULL;

	rtapi_snprintf(path, sizeof(path), "/dev/%s", namelist[n]->d_name);
	(*bulk)[b].chip = gpiod_chip_open(path);
	for (int i = 0; names[i]; i++){
	    offset = gpiod_chip_get_line_offset_from_name((*bulk)[b].chip, names[i]);
	    if (offset >= 0){
		rtapi_print_msg(RTAPI_MSG_DBG, "hal_gpio processing line %s found at offset %i\n", names[i], offset);
		(*bulk)[b].offsets = rtapi_krealloc((*bulk)[b].offsets, (l + 1) * sizeof(offset), RTAPI_GFP_KERNEL);
		(*bulk)[b].offsets[l] = offset;
		(*bulk)[b].flags = rtapi_krealloc((*bulk)[b].flags, (l + 1) * sizeof(offset), RTAPI_GFP_KERNEL);
		(*bulk)[b].flags[l] = flags(names[i]);
		(*bulk)[b].vals = rtapi_krealloc((*bulk)[b].vals, (l + 1) * sizeof(int), RTAPI_GFP_KERNEL);
		(*bulk)[b].num_lines = ++l;
	    }
	    for (int o = 0; o < l; o++) rtapi_print_msg(RTAPI_MSG_DBG, "chip %i offset %i = %i\n", b, o, (*bulk)[b].offsets[o]);
	}
	if (l > 0) {
	    b ++; // move to a new hal_gpio_bulk_t next loop
	} else {
	    gpiod_chip_close((*bulk)[b].chip); // not using this chip
	}
    }
    return b;
}

int setup_lines(hal_gpio_bulk_t *bulk, enum gpiod_line_direction direction){
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
	rtapi_print_msg(RTAPI_MSG_DBG, "add_line_settings returned %d errno=%d (%s)\n",
			ret, errno, strerror(errno));
	if (ret) {
	    return -1;
	} else {
	    ret = gpiod_line_request_reconfigure_lines(bulk->lines, line_cfg);
	    rtapi_print_msg(RTAPI_MSG_DBG, "reconfigure returned %d errno=%d (%s)\n",
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

int build_chips_collection(char **names, hal_gpio_bulk_t **ptr){
    int c, i;
    int count = 0;
    struct gpiod_chip *temp_chip;
    struct gpiod_line *temp_line;
    char *name;

    for (i = 0; names[i] && strlen(names[i]); i++){
	name = names[i];
	temp_line = gpiod_line_find(name);
	if (!temp_line) {
		rtapi_print_msg(RTAPI_MSG_ERR, "The GPIO line %s can not be found\n", name);
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
		*ptr = rtapi_krealloc(*ptr, sizeof(hal_gpio_bulk_t) * count, RTAPI_GFP_KERNEL);
		(*ptr)[c].chip = NULL;
		(*ptr)[c].flags = NULL;
		(*ptr)[c].vals = NULL;
		(*ptr)[c].num_lines = 0;
		(*ptr)[c].chip = gpiod_line_get_chip(temp_line);
		(*ptr)[c].lines = rtapi_kmalloc(sizeof(*(*ptr)[c].lines), RTAPI_GFP_KERNEL);
		gpiod_line_bulk_init((*ptr)[c].lines);
		rtapi_print_msg(RTAPI_MSG_INFO, "hal_gpio: added chip %s index %i\n", gpiod_chip_name((*ptr)[c].chip), c);
	}
	rtapi_print_msg(RTAPI_MSG_INFO, "hal_gpio: adding IO line %s to chip %i\n", name, c);
	temp_line = gpiod_chip_find_line((*ptr)[c].chip, name);
	(*ptr)[c].num_lines++;
	(*ptr)[c].flags = rtapi_krealloc((*ptr)[c].flags, (*ptr)[c].num_lines * sizeof(int), RTAPI_GFP_KERNEL);
	(*ptr)[c].flags[(*ptr)[c].num_lines - 1] = flags(name);
#if LIBGPIOD_VER >= 105
	gpiod_line_set_flags(temp_line, (*ptr)[c].flags[(*ptr)[c].num_lines - 1]);
#endif
	(*ptr)[c].vals = rtapi_krealloc((*ptr)[c].vals, (*ptr)[c].num_lines * sizeof(int), RTAPI_GFP_KERNEL);
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

int rtapi_app_main(void){
    int retval = 0;
    int i, c;
    char hal_name[HAL_NAME_LEN];
    const char *line_name;

rtapi_print_msg(RTAPI_MSG_INFO, "Libgpiod is %i\n", LIBGPIOD_VER);

#ifdef __KERNEL__
    // this calculation fits in a 32-bit unsigned
    // as long as CPUs are under about 6GHz
    ns2tsc_factor = (cpu_khz << 6) / 15625ul;
#else
    ns2tsc_factor = 1ll<<12;
#endif
    
    comp_id = hal_init("hal_gpio");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: ERROR: hal_init() failed\n");
        goto fail0;
    }

    // allocate shared memory for the base struct
    gpio = rtapi_kmalloc(sizeof(hal_gpio_t), RTAPI_GFP_KERNEL);
    if (gpio == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "hal_gpio component: Out of Memory\n");
        goto fail0;
    }

    // Create Inputs
    gpio->num_in_chips = 0;
    gpio->num_out_chips = 0;
    gpio->in_chips = NULL; // so that realloc knows that they need malloc first time throigh
    gpio->out_chips = NULL;

#if LIBGPIOD_VER >= 200
    gpio->num_in_chips = allocate_lines(inputs, &gpio->in_chips);
# else
    gpio->num_in_chips = build_chips_collection(inputs, &gpio->in_chips);
#endif
    if (gpio->num_in_chips < 0){
	rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: Failed to identify all specified input pins\n");
	goto fail0;
    }
    for (c = 0; c < gpio->num_in_chips; c++){
    // Set up the lines parameters and the bulk reads
#if LIBGPIOD_VER >= 200
	retval = setup_lines(&gpio->in_chips[c], GPIOD_LINE_DIRECTION_INPUT);
#else
	retval = gpiod_line_request_bulk_input(gpio->in_chips[c].lines, "linuxcnc");
#endif
	if (retval < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: Failed to register input pin collection\n");
	    goto fail0;
	}

	gpio->in_chips[c].hal = hal_malloc(gpio->in_chips[c].num_lines * sizeof(hal_gpio_hal_t));
	for (i = 0; i < gpio->in_chips[c].num_lines; i++){
	    line_name = get_line_name(&gpio->in_chips[c], i);
	    retval += hal_pin_bit_newf(HAL_OUT, &(gpio->in_chips[c].hal[i].value), comp_id, "hal_gpio.%s-in", line_name);
	    retval += hal_pin_bit_newf(HAL_OUT, &(gpio->in_chips[c].hal[i].value_not), comp_id, "hal_gpio.%s-in-not", line_name);
	}
	if (retval < 0){
	    rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: Failed to allocate GPIO input HAL pins\n");
	    goto fail0;
	}
	    
    }
    

    // Create Outputs

#if LIBGPIOD_VER >= 200
    gpio->num_out_chips = allocate_lines(outputs, &gpio->out_chips);
# else
    gpio->num_out_chips = build_chips_collection(outputs, &gpio->out_chips);
#endif
    if (gpio->num_in_chips < 0){
	rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: Failed to identify all specified output pins\n");
	goto fail0;
    }
    for (c = 0; c < gpio->num_out_chips; c++){
	// Set up the lines parameters and the bulk reads
#if LIBGPIOD_VER >= 200
	retval = setup_lines(&gpio->out_chips[c], GPIOD_LINE_DIRECTION_OUTPUT);
#else
	retval = gpiod_line_request_bulk_output(gpio->out_chips[c].lines, "linuxcnc", gpio->out_chips[c].vals);
#endif
	if (retval < 0){
	    rtapi_print_msg(RTAPI_MSG_ERR, "Failed to register output pin collection\n");
	    goto fail0;
	}

	gpio->out_chips[c].hal = hal_malloc(gpio->out_chips[c].num_lines * sizeof(hal_gpio_hal_t));
	for (i = 0; i < gpio->out_chips[c].num_lines; i++){
	    line_name = get_line_name(&gpio->out_chips[c], i);
	    retval += hal_pin_bit_newf(HAL_IN, &(gpio->out_chips[c].hal[i].value), comp_id, "hal_gpio.%s-out", line_name);
	}
	if (retval < 0){
	    rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: Failed to allocate GPIO output HAL pins\n");
	    goto fail0;
	}
    }

    rtapi_snprintf(hal_name, HAL_NAME_LEN, "hal_gpio.read");
    retval += hal_export_funct(hal_name, hal_gpio_read, gpio, 0, 0, comp_id);
    rtapi_snprintf(hal_name, HAL_NAME_LEN, "hal_gpio.write");
    retval += hal_export_funct(hal_name, hal_gpio_write, gpio, 0, 0, comp_id);

    if (reset_active){
	gpio->reset_ns = hal_malloc(sizeof(hal_u32_t));
	rtapi_snprintf(hal_name, HAL_NAME_LEN, "hal_gpio.reset");
	retval += hal_param_u32_newf(HAL_RW, gpio->reset_ns, comp_id, "hal_gpio.reset_ns");
	retval += hal_export_funct(hal_name, hal_gpio_reset, gpio, 0, 0, comp_id);
    }
    if (retval < 0){
	rtapi_print_msg(RTAPI_MSG_ERR, "hal_gpio: failed to export functions\n");
	goto fail0;
    }
    hal_ready(comp_id);
    return 0;

fail0:
    rtapi_app_exit();
    return -1;

}

/**************************************************************
* REALTIME PORT READ/WRITE FUNCTION                                *
**************************************************************/

static void hal_gpio_read(void *arg, __attribute__((unused)) long period)
{
    hal_gpio_t *gpio = arg;
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

static void hal_gpio_write(void *arg, __attribute__((unused)) long period)
{
    hal_gpio_t *gpio = arg;
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
    // store the time (in CPU clocks) for the reset function
    last_reset = rtapi_get_clocks();
}

static void hal_gpio_reset(void *arg, long period)
{
    hal_gpio_t *gpio = arg;
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
	deadline = last_reset + ns2tsc(*gpio->reset_ns);
        while(rtapi_get_clocks() < deadline) {} // busy-wait!
#if LIBGPIOD_VER > 200
	gpiod_line_request_set_values(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#else
	gpiod_line_set_value_bulk(gpio->out_chips[c].lines, gpio->out_chips[c].vals);
#endif
    }
}

void rtapi_app_exit(void) {
    int c;

    for (c = 0; c < gpio->num_in_chips; c++){
#if LIBGPIOD_VER >= 200
	gpiod_line_request_release(gpio->in_chips[c].lines);
#else
	rtapi_kfree(gpio->in_chips[c].lines);
#endif
	gpiod_chip_close(gpio->in_chips[c].chip);
    }
    for (c = 0; c < gpio->num_out_chips; c++){
#if LIBGPIOD_VER >= 200
	gpiod_line_request_release(gpio->out_chips[c].lines);
#else
	rtapi_kfree(gpio->out_chips[c].lines);
#endif
	gpiod_chip_close(gpio->out_chips[c].chip);
    }
    rtapi_kfree(gpio->in_chips);
    rtapi_kfree(gpio->out_chips);
    rtapi_kfree(gpio);
    hal_exit(comp_id);
}

