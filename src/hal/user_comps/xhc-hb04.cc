/*
   XHC-HB04 Wireless MPG pendant LinuxCNC HAL module for LinuxCNC

   Copyright (C) 2013 Frederic Rible (frible@teaser.fr)
   Copyright (C) 2013 Rene Hopf (renehopf@mac.com)
   Copyright (C) 2014 Marius Alksnys (marius.alksnys@gmail.com)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <libusb.h>
#include <unistd.h>
#include <stdarg.h>

#include <hal.h>
#include <inifile.hh>

const char *modname = "xhc-hb04";
int hal_comp_id;
const char *section = "XHC-HB04";
bool simu_mode = true;

typedef struct {
	char pin_name[256];
	unsigned int code;
} xhc_button_t;

typedef enum {
	axis_off = 0x00,
	axis_x = 0x11,
	axis_y = 0x12,
	axis_z = 0x13,
	axis_a = 0x18,
	axis_spindle = 0x14,
	axis_feed = 0x15
} xhc_axis_t;

static unsigned char _button_step = 0;
#define NB_MAX_BUTTONS 32

typedef struct {
	hal_float_t *x_wc, *y_wc, *z_wc, *a_wc;
	hal_float_t *x_mc, *y_mc, *z_mc, *a_mc;

	hal_float_t *feedrate_override, *feedrate;
	hal_float_t *spindle_override, *spindle_rps;

	hal_bit_t *button_pin[NB_MAX_BUTTONS];

    hal_bit_t *jog_enable_off;
	hal_bit_t *jog_enable_x;
	hal_bit_t *jog_enable_y;
	hal_bit_t *jog_enable_z;
	hal_bit_t *jog_enable_a;
	hal_bit_t *jog_enable_feedrate;
	hal_bit_t *jog_enable_spindle;
	hal_float_t *jog_scale;
	hal_s32_t *jog_counts, *jog_counts_neg;

	hal_float_t *jog_velocity;
	hal_float_t *jog_max_velocity;
	hal_float_t *jog_increment;
	hal_bit_t *jog_plus_x, *jog_plus_y, *jog_plus_z, *jog_plus_a;
	hal_bit_t *jog_minus_x, *jog_minus_y, *jog_minus_z, *jog_minus_a;
} xhc_hal_t;

typedef struct {
	xhc_hal_t *hal;
	int step;
	xhc_axis_t axis;
	xhc_button_t buttons[NB_MAX_BUTTONS];
	unsigned char button_code;

	// Variables for velocity computation
	hal_s32_t last_jog_counts;
	struct timeval last_tv;
} xhc_t;

static xhc_t xhc;

static int do_exit = 0;
static int do_reconnect = 0;

struct libusb_transfer *transfer_in  = NULL;
unsigned char in_buf[32];
void cb_transfer_in(struct libusb_transfer *transfer);
void setup_asynch_transfer(libusb_device_handle *dev_handle);

extern "C" const char *
iniFind(FILE *fp, const char *tag, const char *section)
{
    IniFile                     f(false, fp);

    return(f.Find(tag, section));
}

int xhc_encode_float(float v, unsigned char *buf)
{
	unsigned int int_v = round(fabs(v) * 10000.0);
	unsigned short int_part = int_v / 10000;
	unsigned short fract_part = int_v % 10000;
	if (v < 0) fract_part = fract_part | 0x8000;
	*(short *)buf = int_part;
	*((short *)buf+1) = fract_part;
	return 4;
}

int xhc_encode_s16(int v, unsigned char *buf)
{
	*(short *)buf = v;
	return 2;
}

void xhc_display_encode(xhc_t *xhc, unsigned char *data, int len)
{
	unsigned char buf[6*7];
	unsigned char *p = buf;
	int i;
	int packet;

	assert(len == 6*8);

	memset(buf, 0, sizeof(buf));

	*p++ = 0xFE;
	*p++ = 0xFD;
	*p++ = 0x0C;

	if (xhc->axis == axis_a) p += xhc_encode_float(round(1000 * *(xhc->hal->a_wc)) / 1000, p);
	else p += xhc_encode_float(round(1000 * *(xhc->hal->x_wc)) / 1000, p);
	p += xhc_encode_float(round(1000 * *(xhc->hal->y_wc)) / 1000, p);
	p += xhc_encode_float(round(1000 * *(xhc->hal->z_wc)) / 1000, p);
	if (xhc->axis == axis_a) p += xhc_encode_float(round(1000 * *(xhc->hal->a_mc)) / 1000, p);
	else p += xhc_encode_float(round(1000 * *(xhc->hal->x_mc)) / 1000, p);
	p += xhc_encode_float(round(1000 * *(xhc->hal->y_mc)) / 1000, p);
	p += xhc_encode_float(round(1000 * *(xhc->hal->z_mc)) / 1000, p);
	p += xhc_encode_s16(round(100.0 * *(xhc->hal->feedrate_override)), p);
	p += xhc_encode_s16(round(100.0 * *(xhc->hal->spindle_override)), p);
	p += xhc_encode_s16(round(60.0 * *(xhc->hal->feedrate)), p);
	p += xhc_encode_s16(round(60.0 * *(xhc->hal->spindle_rps)), p);

	switch (xhc->step) {
	case 1:
		buf[35] = 0x01;
		break;
	case 10:
		buf[35] = 0x03;
		break;
	case 100:
		buf[35] = 0x08;
		break;
	case 1000:
		buf[35] = 0x0A;
		break;
	}

	// Multiplex to 6 USB transactions

	p = buf;
	for (packet=0; packet<6; packet++) {
		for (i=0; i<8; i++) {
			if (i == 0) data[i+8*packet] = 6;
			else data[i+8*packet] = *p++;
		}
	}
}

void xhc_set_display(libusb_device_handle *dev_handle, xhc_t *xhc)
{
	unsigned char data[6*8];
	int packet;

	xhc_display_encode(xhc, data, sizeof(data));

	for (packet=0; packet<6; packet++) {
		int r = libusb_control_transfer(dev_handle, 0x21, 0x09, 0x0306, 0x00, data+8*packet, 8, 0);
		if (r < 0) {
			do_reconnect = 1;
		}
	}
}

void hexdump(unsigned char *data, int len)
{
	int i;

	for (i=0; i<len; i++) printf("%02X ", data[i]);
}

void linuxcnc_simu(xhc_hal_t *hal)
{
	static int last_jog_counts;

	if (*(hal->jog_counts) != last_jog_counts) {
		float delta = (*(hal->jog_counts) - last_jog_counts) * *(hal->jog_scale);
		if (*(hal->jog_enable_x)) {
			*(hal->x_mc) += delta;
			*(hal->x_wc) += delta;
		}

		if (*(hal->jog_enable_y)) {
			*(hal->y_mc) += delta;
			*(hal->y_wc) += delta;
		}

		if (*(hal->jog_enable_z)) {
			*(hal->z_mc) += delta;
			*(hal->z_wc) += delta;
		}

		if (*(hal->jog_enable_a)) {
			*(hal->a_mc) += delta;
			*(hal->a_wc) += delta;
		}

		if (*(hal->jog_enable_spindle)) {
			*(hal->spindle_override) += (*hal->jog_counts) * 0.01;
			if (*(hal->spindle_override) > 1) *(hal->spindle_override) = 1;
			if (*(hal->spindle_override) < 0) *(hal->spindle_override) = 0;
			*(hal->spindle_rps) = 25000.0/60.0 * *(hal->spindle_override);
		}

		if (*(hal->jog_enable_feedrate)) {
			*(hal->feedrate_override) += (*hal->jog_counts) * 0.01;
			if (*(hal->feedrate_override) > 1) *(hal->feedrate_override) = 1;
			if (*(hal->feedrate_override) < 0) *(hal->feedrate_override) = 0;
			*(hal->feedrate) = 3000.0 * *(hal->feedrate_override);
		}

		last_jog_counts = *(hal->jog_counts);
	}
}

void compute_velocity(xhc_t *xhc)
{
	timeval now, delta_tv;
	gettimeofday(&now, NULL);

	if (xhc->last_tv.tv_sec == 0) xhc->last_tv = now;
	timersub(&now, &xhc->last_tv, &delta_tv);
	float elapsed = delta_tv.tv_sec + 1e-6f*delta_tv.tv_usec;
	if (elapsed <= 0) return;

	float delta_pos = (*(xhc->hal->jog_counts) - xhc->last_jog_counts) * *(xhc->hal->jog_scale);
	float velocity = *(xhc->hal->jog_max_velocity) * 60.0f * *(xhc->hal->jog_scale);
	float k = 0.05f;

	if (delta_pos) {
		*(xhc->hal->jog_velocity) = (1 - k) * *(xhc->hal->jog_velocity) + k * velocity;
		*(xhc->hal->jog_increment) = fabs(delta_pos);
		*(xhc->hal->jog_plus_x) = (delta_pos > 0) && *(xhc->hal->jog_enable_x);
		*(xhc->hal->jog_minus_x) = (delta_pos < 0) && *(xhc->hal->jog_enable_x);
		*(xhc->hal->jog_plus_y) = (delta_pos > 0) && *(xhc->hal->jog_enable_y);
		*(xhc->hal->jog_minus_y) = (delta_pos < 0) && *(xhc->hal->jog_enable_y);
		*(xhc->hal->jog_plus_z) = (delta_pos > 0) && *(xhc->hal->jog_enable_z);
		*(xhc->hal->jog_minus_z) = (delta_pos < 0) && *(xhc->hal->jog_enable_z);
		*(xhc->hal->jog_plus_a) = (delta_pos > 0) && *(xhc->hal->jog_enable_a);
		*(xhc->hal->jog_minus_a) = (delta_pos < 0) && *(xhc->hal->jog_enable_a);
		xhc->last_jog_counts = *(xhc->hal->jog_counts);
		xhc->last_tv = now;
	}
	else {
		*(xhc->hal->jog_velocity) = (1 - k) * *(xhc->hal->jog_velocity);
		if (elapsed > 0.25) {
			*(xhc->hal->jog_velocity) = 0;
			*(xhc->hal->jog_plus_x) = 0;
			*(xhc->hal->jog_minus_x) = 0;
			*(xhc->hal->jog_plus_y) = 0;
			*(xhc->hal->jog_minus_y) = 0;
			*(xhc->hal->jog_plus_z) = 0;
			*(xhc->hal->jog_minus_z) = 0;
			*(xhc->hal->jog_plus_a) = 0;
			*(xhc->hal->jog_minus_a) = 0;
		}
	}
 }

void cb_response_in(struct libusb_transfer *transfer)
{
	int i;

	if (transfer->actual_length > 0) {
		if (simu_mode) hexdump(in_buf, transfer->actual_length);

		xhc.button_code = in_buf[1];
		xhc.axis = (xhc_axis_t)in_buf[3];

		if (_button_step && xhc.button_code == _button_step) {
			xhc.step = 10*xhc.step;
			if (xhc.step > 1000 || xhc.step <= 0) xhc.step = 1;
		}

		*(xhc.hal->jog_scale) = xhc.step * 0.001f;
		*(xhc.hal->jog_counts) += ((char)in_buf[4]);
		*(xhc.hal->jog_counts_neg) = - *(xhc.hal->jog_counts);
		*(xhc.hal->jog_enable_off) = (xhc.axis == axis_off);
		*(xhc.hal->jog_enable_x) = (xhc.axis == axis_x);
		*(xhc.hal->jog_enable_y) = (xhc.axis == axis_y);
		*(xhc.hal->jog_enable_z) = (xhc.axis == axis_z);
		*(xhc.hal->jog_enable_a) = (xhc.axis == axis_a);
		*(xhc.hal->jog_enable_feedrate) = (xhc.axis == axis_feed);
		*(xhc.hal->jog_enable_spindle) = (xhc.axis == axis_spindle);

		for (i=0; i<NB_MAX_BUTTONS; i++) {
			if (!xhc.hal->button_pin[i]) continue;
			*(xhc.hal->button_pin[i]) = (xhc.button_code == xhc.buttons[i].code);
			if (simu_mode && *(xhc.hal->button_pin[i]))  printf("%s pressed", xhc.buttons[i].pin_name);
		}

		if (simu_mode) printf("\n");
	}

	setup_asynch_transfer(transfer->dev_handle);
}

void setup_asynch_transfer(libusb_device_handle *dev_handle)
{
	transfer_in  = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer( transfer_in, dev_handle, (0x1 | LIBUSB_ENDPOINT_IN),
		in_buf, sizeof(in_buf),
		cb_response_in, NULL, 0); // no user data
	libusb_submit_transfer(transfer_in);
}

static void quit(int sig)
{
	do_exit = 1;
}

static int hal_pin_simu(char *pin_name, void **ptr, int s)
{
	printf("Creating pin: %s\n", pin_name);
	*ptr = calloc(s, 1);
	return 0;
}

int _hal_pin_float_newf(hal_pin_dir_t dir, hal_float_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
	char pin_name[256];
    va_list args;
    va_start(args,fmt);
	vsprintf(pin_name, fmt, args);
	va_end(args);

    if (simu_mode) {
    	return hal_pin_simu(pin_name, ( void**)data_ptr_addr, sizeof(*data_ptr_addr));
    }
    else {
    	return hal_pin_float_new(pin_name, dir, data_ptr_addr, comp_id);
    }
}

int _hal_pin_s32_newf(hal_pin_dir_t dir, hal_s32_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
	char pin_name[256];
    va_list args;
    va_start(args,fmt);
	vsprintf(pin_name, fmt, args);
	va_end(args);

    if (simu_mode) {
    	return hal_pin_simu(pin_name, ( void**)data_ptr_addr, sizeof(*data_ptr_addr));
    }
    else {
    	return hal_pin_s32_new(pin_name, dir, data_ptr_addr, comp_id);
    }
}

int _hal_pin_bit_newf(hal_pin_dir_t dir, hal_bit_t ** data_ptr_addr, int comp_id, const char *fmt, ...)
{
	char pin_name[256];
    va_list args;
    va_start(args,fmt);
	vsprintf(pin_name, fmt, args);
	va_end(args);

    if (simu_mode) {
    	return hal_pin_simu(pin_name, ( void**)data_ptr_addr, sizeof(*data_ptr_addr));
    }
    else {
    	return hal_pin_bit_new(pin_name, dir, data_ptr_addr, comp_id);
    }
}

static void hal_setup()
{
	int r, i;

	if (!simu_mode) {
		hal_comp_id = hal_init(modname);
		if (hal_comp_id < 1) {
			fprintf(stderr, "%s: ERROR: hal_init failed\n", modname);
			exit(1);
		}

		xhc.hal = (xhc_hal_t *)hal_malloc(sizeof(xhc_hal_t));
		if (xhc.hal == NULL) {
			fprintf(stderr, "%s: ERROR: unable to allocate HAL shared memory\n", modname);
			exit(1);
		}
	}
	else {
		xhc.hal = (xhc_hal_t *)calloc(sizeof(xhc_hal_t), 1);
	}

    r = 0;

    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->x_mc), hal_comp_id, "%s.x.pos-absolute", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->y_mc), hal_comp_id, "%s.y.pos-absolute", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->z_mc), hal_comp_id, "%s.z.pos-absolute", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->a_mc), hal_comp_id, "%s.a.pos-absolute", modname);

    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->x_wc), hal_comp_id, "%s.x.pos-relative", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->y_wc), hal_comp_id, "%s.y.pos-relative", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->z_wc), hal_comp_id, "%s.z.pos-relative", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->a_wc), hal_comp_id, "%s.a.pos-relative", modname);

    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->feedrate), hal_comp_id, "%s.feed-value", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->feedrate_override), hal_comp_id, "%s.feed-override", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->spindle_rps), hal_comp_id, "%s.spindle-rps", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->spindle_override), hal_comp_id, "%s.spindle-override", modname);

	for (i=0; i<NB_MAX_BUTTONS; i++) {
		if (!xhc.buttons[i].pin_name[0]) continue;
		r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->button_pin[i]), hal_comp_id, "%s.%s", modname, xhc.buttons[i].pin_name);
		if (strcmp("button-step", xhc.buttons[i].pin_name) == 0) _button_step = xhc.buttons[i].code;
	}
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_off), hal_comp_id, "%s.jog.enable-off", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_x), hal_comp_id, "%s.jog.enable-x", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_y), hal_comp_id, "%s.jog.enable-y", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_z), hal_comp_id, "%s.jog.enable-z", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_a), hal_comp_id, "%s.jog.enable-a", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_feedrate), hal_comp_id, "%s.jog.enable-feed-override", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_enable_spindle), hal_comp_id, "%s.jog.enable-spindle-override", modname);

    r |= _hal_pin_float_newf(HAL_OUT, &(xhc.hal->jog_scale), hal_comp_id, "%s.jog.scale", modname);
    r |= _hal_pin_s32_newf(HAL_OUT, &(xhc.hal->jog_counts), hal_comp_id, "%s.jog.counts", modname);
    r |= _hal_pin_s32_newf(HAL_OUT, &(xhc.hal->jog_counts_neg), hal_comp_id, "%s.jog.counts-neg", modname);

    r |= _hal_pin_float_newf(HAL_OUT, &(xhc.hal->jog_velocity), hal_comp_id, "%s.jog.velocity", modname);
    r |= _hal_pin_float_newf(HAL_IN, &(xhc.hal->jog_max_velocity), hal_comp_id, "%s.jog.max-velocity", modname);
    r |= _hal_pin_float_newf(HAL_OUT, &(xhc.hal->jog_increment), hal_comp_id, "%s.jog.increment", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_plus_x), hal_comp_id, "%s.jog.plus-x", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_minus_x), hal_comp_id, "%s.jog.minus-x", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_plus_y), hal_comp_id, "%s.jog.plus-y", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_minus_y), hal_comp_id, "%s.jog.minus-y", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_plus_z), hal_comp_id, "%s.jog.plus-z", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_minus_z), hal_comp_id, "%s.jog.minus-z", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_plus_a), hal_comp_id, "%s.jog.plus-a", modname);
    r |= _hal_pin_bit_newf(HAL_OUT, &(xhc.hal->jog_minus_a), hal_comp_id, "%s.jog.minus-a", modname);

	return;
}

int read_ini_file(char *filename)
{
	FILE *fd = fopen(filename, "r");
	const char *bt;
	int nb_buttons = 0;
	if (!fd) {
		perror(filename);
		return -1;
	}

	IniFile f(false, fd);

	while ( (bt = f.Find("BUTTON", section, nb_buttons+1)) && nb_buttons < NB_MAX_BUTTONS) {
		if (sscanf(bt, "%x:%s", &xhc.buttons[nb_buttons].code, xhc.buttons[nb_buttons].pin_name) !=2 ) {
			fprintf(stderr, "%s: syntax error\n", bt);
			return -1;
		}
		nb_buttons++;
	}

	return 0;
}

#define STRINGIFY_IMPL(S) #S
#define STRINGIFY(s) STRINGIFY_IMPL(s)

static void Usage(char *name)
{
	fprintf(stderr, "%s version %s by Frederic RIBLE (frible@teaser.fr)\n", name, STRINGIFY(VERSION));
    fprintf(stderr, "Usage: %s [-I ini-file] [-h] [-H]\n", name);
    fprintf(stderr, " -I ini-file: configuration file defining the MPG keyboard layout\n");
    fprintf(stderr, " -h: usage\n");
    fprintf(stderr, " -H: run in real-time HAL mode (run in simulation mode by default)\n");
}

int main (int argc,char **argv)
{
	libusb_device **devs;
    libusb_device_handle *dev_handle;
	libusb_context *ctx = NULL;
	int r;
	ssize_t cnt;
#define MAX_WAIT_SECS 10
	int wait_secs = 0;

    int opt;

    while ((opt = getopt(argc, argv, "HhI:")) != -1) {
        switch (opt) {
        case 'I':
            if (read_ini_file(optarg)) exit(EXIT_FAILURE);
            break;
        case 'H':
        	simu_mode = false;
        	break;
        default:
        	Usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
	if (simu_mode) hal_setup();
	xhc.step = 1;

	signal(SIGINT, quit);
	signal(SIGTERM, quit);

	while (!do_exit) {
    	//on reconnect wait for device to be gone
    	if (do_reconnect == 1) {
    		sleep(5);
    		do_reconnect = 0;
    	}
    
		r = libusb_init(&ctx);

		if(r < 0) {
			perror("libusb_init");
			return 1;
		}
		libusb_set_debug(ctx, 3);

		printf("%s: waiting for XHC-HB04 device\n",modname);

		do {
			cnt = libusb_get_device_list(ctx, &devs);
			if (cnt < 0) {
				perror("libusb_get_device_list");
				return 1;
			}

			dev_handle = libusb_open_device_with_vid_pid(ctx, 0x10CE, 0xEB70);
			libusb_free_device_list(devs, 1);
			if (dev_handle == NULL) {
				wait_secs++;
				if (wait_secs >= MAX_WAIT_SECS/2) {
					printf("%s: waiting for XHC-HB04 device (%d)\n",modname,wait_secs);
				}
				if (wait_secs > MAX_WAIT_SECS) {
					printf("%s: MAX_WAIT_SECS exceeded, exiting\n",modname);
					exit(1);
				}
				sleep(1);
			}
		} while(dev_handle == NULL && !do_exit);
        if (!simu_mode) {
			hal_setup();
			hal_ready(hal_comp_id);
		}

		printf("%s: found XHC-HB04 device\n",modname);

		if (dev_handle) {
			if 	(libusb_kernel_driver_active(dev_handle, 0) == 1) {
				libusb_detach_kernel_driver(dev_handle, 0);
			}

			r = libusb_claim_interface(dev_handle, 0);
			if (r < 0) {
				perror("libusb_claim_interface");
				return 1;
			}
		}

		if (dev_handle) {
			setup_asynch_transfer(dev_handle);
			xhc_set_display(dev_handle, &xhc);
		}

		if (dev_handle) {
			while (!do_exit && !do_reconnect) {
				struct timeval tv;
				tv.tv_sec  = 0;
				tv.tv_usec = 30000;
				r = libusb_handle_events_timeout(NULL, &tv);
				compute_velocity(&xhc);
				if (simu_mode) linuxcnc_simu(xhc.hal);
				xhc_set_display(dev_handle, &xhc);
			}
			printf("%s: connection lost, cleaning up\n",modname);
			libusb_cancel_transfer(transfer_in);
			libusb_free_transfer(transfer_in);
			libusb_release_interface(dev_handle, 0);
			libusb_close(dev_handle);
		}
		else {
			while (!do_exit) usleep(70000);
		}
		libusb_exit(ctx);
	}
}
