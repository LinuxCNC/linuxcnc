/*    Copyright (C) 2013 GP Orcullo
 *
 *    Portions of this code is based on stepgen.c
 *    by John Kasunich, Copyright (C) 2003-2007
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

#include "rtapi_math.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "hal_spi.h"

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#if !defined(TARGET_PLATFORM_RASPBERRY)
#error "This driver is for the Raspberry Pi platform only"
#endif

#define MODNAME "hal_spi"
#define PREFIX "spi"

MODULE_AUTHOR("GP Orcullo");
MODULE_DESCRIPTION("Driver for Raspberry Pi PICnc");
MODULE_LICENSE("GPL v2");

static int stepwidth = 1;
RTAPI_MP_INT(stepwidth, "Step width in 1/BASEFREQ");

static long pwmfreq = 1000;
RTAPI_MP_LONG(pwmfreq, "PWM frequency in Hz");

typedef struct {
	hal_float_t *position_cmd[NUMAXES],
	            *position_fb[NUMAXES],
	            *pwm_duty;
	hal_bit_t   *pin_out[5],
	            *pin_in[5],
	            *ready;
	hal_float_t scale[NUMAXES],
	            maxaccel[NUMAXES],
	            pwm_scale;
} spi_data_t;

static spi_data_t *spi_data;

static int comp_id;
static const char *modname = MODNAME;
static const char *prefix = PREFIX;

volatile unsigned *gpio, *spi;

volatile int32_t txBuf[BUFSIZE], rxBuf[BUFSIZE+1];
static u32 pwm_period = 0;

static double dt = 0,				/* update_freq period in seconds */
	      recip_dt = 0,			/* reciprocal of period, avoids divides */
	      scale_inv[NUMAXES] = { 1.0 },	/* inverse of scale */
	      old_vel[NUMAXES] = { 0 },
	      old_pos[NUMAXES] = { 0 },
	      old_scale[NUMAXES] = { 0 },
	      max_vel;
static long old_dtns = 0;			/* update_freq funct period in nsec */
static s32 accum_diff = 0,
           old_count[NUMAXES] = { 0 };
static s64 accum[NUMAXES] = { 0 };		/* 64 bit DDS accumulator */

static void read_spi(void *arg, long period);
static void write_spi(void *arg, long period);
static void update(void *arg, long period);
void transfer_data();
static void reset_board();
static int map_gpio();
static void setup_gpio();
static void restore_gpio();

int rtapi_app_main(void) {
	char name[HAL_NAME_LEN + 1];
	int n, retval;

	/* initialise driver */
	comp_id = hal_init(modname);
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n",
		        modname);
		return -1;
	}

	/* allocate shared memory */
	spi_data = hal_malloc(sizeof(spi_data_t));
	if (spi_data == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n",
		        modname);
		hal_exit(comp_id);
		return -1;
	}

	/* configure board */
	retval = map_gpio();
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: cannot map GPIO memory\n", modname);
		return retval;
	}

	setup_gpio();
	reset_board();

	pwm_period = (40000000ul/pwmfreq) - 1;	/* PeripheralClock/pwmfreq - 1 */

	txBuf[0] = 0x4746433E;			/* this is config data (>CFG) */
	txBuf[1] = stepwidth;
	txBuf[2] = pwm_period;
	transfer_data();			/* send config data */

	max_vel = BASEFREQ/(2.0 * stepwidth);	/* calculate velocity limit */

	/* export pins and parameters */
	for (n=0; n<NUMAXES; n++) {
		retval = hal_pin_float_newf(HAL_IN, &(spi_data->position_cmd[n]),
		        comp_id, "%s.%01d.position-cmd", prefix, n);
		if (retval < 0) goto error;
		*(spi_data->position_cmd[n]) = 0.0;

		retval = hal_pin_float_newf(HAL_OUT, &(spi_data->position_fb[n]),
		        comp_id, "%s.%01d.position-fb", prefix, n);
		if (retval < 0) goto error;
		*(spi_data->position_fb[n]) = 0.0;

		retval = hal_param_float_newf(HAL_RW, &(spi_data->scale[n]),
		        comp_id, "%s.%01d.scale", prefix, n);
		if (retval < 0) goto error;
		spi_data->scale[n] = 1.0;

		retval = hal_param_float_newf(HAL_RW, &(spi_data->maxaccel[n]),
		        comp_id, "%s.%01d.maxaccel", prefix, n);
		if (retval < 0) goto error;
		spi_data->maxaccel[n] = 1.0;
	}

	for (n=0; n < (5); n++) {
		retval = hal_pin_bit_newf(HAL_IN, &(spi_data->pin_out[n]),
		        comp_id, "%s.pin.%01d.out", prefix, n);
		if (retval < 0) goto error;
		*(spi_data->pin_out[n]) = 0;

		retval = hal_pin_bit_newf(HAL_OUT, &(spi_data->pin_in[n]),
		        comp_id, "%s.pin.%01d.in", prefix, n);
		if (retval < 0) goto error;
		*(spi_data->pin_in[n]) = 0;
	}

	retval = hal_pin_float_newf(HAL_IN, &(spi_data->pwm_duty), comp_id,
	        "%s.pwm-duty", prefix);
	if (retval < 0) goto error;
	*(spi_data->pwm_duty) = 0.0;

	retval = hal_pin_bit_newf(HAL_OUT, &(spi_data->ready), comp_id,
	        "%s.ready", prefix);
	if (retval < 0) goto error;
	*(spi_data->ready) = 0;

	retval = hal_param_float_newf(HAL_RW, &(spi_data->pwm_scale), comp_id,
	        "%s.pwm-scale", prefix);
	if (retval < 0) goto error;
	spi_data->pwm_scale = 1.0;
error:
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: pin export failed with err=%i\n",
		        modname, retval);
		hal_exit(comp_id);
		return -1;
	}

	/* export functions */
	rtapi_snprintf(name, sizeof(name), "%s.read", prefix);
	retval = hal_export_funct(name, read_spi, spi_data, 1, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: read function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	rtapi_snprintf(name, sizeof(name), "%s.write", prefix);
	/* no FP operations */
	retval = hal_export_funct(name, write_spi, spi_data, 0, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: write function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	rtapi_snprintf(name, sizeof(name), "%s.update", prefix);
	retval = hal_export_funct(name, update, spi_data, 1, 0, comp_id);
	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		        "%s: ERROR: update function export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);
	hal_ready(comp_id);
	return 0;
}

void rtapi_app_exit(void) {
	restore_gpio();
	munmap((void *)gpio,BLOCK_SIZE);
	munmap((void *)spi,BLOCK_SIZE);
	hal_exit(comp_id);
}

static void read_spi(void *arg, long period) {
	int i;
	spi_data_t *spi = (spi_data_t *)arg;

	/* skip loading velocity command */
	txBuf[0] = 0x444D4300;

	/* send request */
	BCM2835_GPCLR0 = (1l << 14);

	/* wait until ready, signal active low */
	while (BCM2835_GPLEV0 & (1l << 15));

	transfer_data();

	/* clear request, active low */
	BCM2835_GPSET0 = (1l << 14);

	/* sanity check */
	if (((u32)rxBuf[1] >> 8) == (rxBuf[BUFSIZE] & 0xffffff) &&
		((u32)rxBuf[1] >> 8) == 0x444D43)	/* CMD */
		*(spi->ready) = 1;
	else
		*(spi->ready) = 0;

	/* check for change in period */
	if (period != old_dtns) {
		old_dtns = period;
		dt = period * 0.000000001;
		recip_dt = 1.0 / dt;
	}

	/* check for scale change */
	for (i = 0; i < NUMAXES; i++) {
		if (spi->scale[i] != old_scale[i]) {
			old_scale[i] = spi->scale[i];
			/* scale must not be 0 */
			if ((spi->scale[i] < 1e-20) && (spi->scale[i] > -1e-20))
				spi->scale[i] = 1.0;
			scale_inv[i] = (1.0 / (1L << STEPBIT)) / spi->scale[i];
		}
	}

	/* update outputs */
	for (i = 0; i < NUMAXES; i++) {
		/* the DDS uses 32 bit counter, this code converts
		   that counter into 64 bits */
		accum_diff = get_position(i) - old_count[i];
		old_count[i] = get_position(i);
		accum[i] += accum_diff;

		*(spi->position_fb[i]) = (float)(accum[i]) * scale_inv[i];
	}

	/* update input status */
	*(spi->pin_in[0]) = (get_inputs() & 0b000000100000) ? 1 : 0;
	*(spi->pin_in[1]) = (get_inputs() & 0b000001000000) ? 1 : 0;
	*(spi->pin_in[2]) = (get_inputs() & 0b000010000000) ? 1 : 0;
	*(spi->pin_in[3]) = (get_inputs() & 0b000100000000) ? 1 : 0;
	*(spi->pin_in[4]) = (get_inputs() & 0b001000000000) ? 1 : 0;
}

static void write_spi(void *arg, long period) {
	transfer_data();
}

static void update(void *arg, long period) {
	int i;
	spi_data_t *spi = (spi_data_t *)arg;
	float duty;
	double max_accl, vel_cmd, dv, new_vel,
	       dp, pos_cmd, curr_pos, match_accl, match_time, avg_v,
	       est_out, est_cmd, est_err;

	for (i = 0; i < NUMAXES; i++) {
		/* set internal accel limit to its absolute max, which is
		   zero to full speed in one thread period */
		max_accl = max_vel * recip_dt;

		/* check for user specified accel limit parameter */
		if (spi->maxaccel[i] <= 0.0) {
			/* set to zero if negative */
			spi->maxaccel[i] = 0.0;
		} else {
			/* parameter is non-zero, compare to max_accl */
			if ((spi->maxaccel[i] * rtapi_fabs(spi->scale[i])) > max_accl) {
				/* parameter is too high, lower it */
				spi->maxaccel[i] = max_accl / rtapi_fabs(spi->scale[i]);
			} else {
				/* lower limit to match parameter */
				max_accl = spi->maxaccel[i] * rtapi_fabs(spi->scale[i]);
			}
		}

		/* calculate position command in counts */
		pos_cmd = *(spi->position_cmd[i]) * spi->scale[i];
		/* calculate velocity command in counts/sec */
		vel_cmd = (pos_cmd - old_pos[i]) * recip_dt;
		old_pos[i] = pos_cmd;

		/* apply frequency limit */
		if (vel_cmd > max_vel) {
			vel_cmd = max_vel;
		} else if (vel_cmd < -max_vel) {
			vel_cmd = -max_vel;
		}

		/* determine which way we need to ramp to match velocity */
		if (vel_cmd > old_vel[i])
			match_accl = max_accl;
		else
			match_accl = -max_accl;

		/* determine how long the match would take */
		match_time = (vel_cmd - old_vel[i]) / match_accl;
		/* calc output position at the end of the match */
		avg_v = (vel_cmd + old_vel[i]) * 0.5;
		curr_pos = (double)(accum[i]) * (1.0 / (1L << STEPBIT));
		est_out = curr_pos + avg_v * match_time;
		/* calculate the expected command position at that time */
		est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * dt);
		/* calculate error at that time */
		est_err = est_out - est_cmd;

		if (match_time < dt) {
			/* we can match velocity in one period */
			if (rtapi_fabs(est_err) < 0.0001) {
				/* after match the position error will be acceptable */
				/* so we just do the velocity match */
				new_vel = vel_cmd;
			} else {
				/* try to correct position error */
				new_vel = vel_cmd - 0.5 * est_err * recip_dt;
				/* apply accel limits */
				if (new_vel > (old_vel[i] + max_accl * dt)) {
					new_vel = old_vel[i] + max_accl * dt;
				} else if (new_vel < (old_vel[i] - max_accl * dt)) {
					new_vel = old_vel[i] - max_accl * dt;
				}
			}
		} else {
			/* calculate change in final position if we ramp in the
			opposite direction for one period */
			dv = -2.0 * match_accl * dt;
			dp = dv * match_time;
			/* decide which way to ramp */
			if (rtapi_fabs(est_err + dp * 2.0) < rtapi_fabs(est_err)) {
				match_accl = -match_accl;
			}
			/* and do it */
			new_vel = old_vel[i] + match_accl * dt;
		}

		/* apply frequency limit */
		if (new_vel > max_vel) {
			new_vel = max_vel;
		} else if (new_vel < -max_vel) {
			new_vel = -max_vel;
		}

		old_vel[i] = new_vel;
		/* calculate new velocity cmd */
		update_velocity(i, (new_vel * VELSCALE));
	}

	/* update rpi output, active low */
	BCM2835_GPCLR0 = (*(spi->pin_out[3]) ? 1l : 0) << 23 ;	/* GPIO23 */
	BCM2835_GPSET0 = (*(spi->pin_out[3]) ? 0 : 1l) << 23 ;
	BCM2835_GPCLR0 = (*(spi->pin_out[4]) ? 1l : 0) << 24 ;	/* GPIO24 */
	BCM2835_GPSET0 = (*(spi->pin_out[4]) ? 0 : 1l) << 24 ;

	/* update pic32 output */
	txBuf[1+NUMAXES] = (*(spi->pin_out[0]) ? 1l : 0) << 11 |
	        (*(spi->pin_out[1]) ? 1l : 0) << 12 |
	        (*(spi->pin_out[2]) ? 1l : 0) << 14 ;

	/* update pwm */
	duty = *spi->pwm_duty * spi->pwm_scale * 0.01;
	if (duty < 0.0) duty = 0.0;
	if (duty > 1.0) duty = 1.0;

	duty = 1.0 - duty;		/* pwm output is active low */

	txBuf[2+NUMAXES] = (duty * (1.0 + pwm_period));

	/* this is a command (>CMD) */
	txBuf[0] = 0x444D433E;
}

void transfer_data() {
	char *buf;
	int i;

	/* activate transfer */
	BCM2835_SPICS = SPI_CS_TA;

	/* send txBuf */
	buf = (char *)txBuf;
	for (i=0; i<SPIBUFSIZE; i++) {
		BCM2835_SPIFIFO = *buf++;
	}

	/* wait until transfer is finished */
	while (!(BCM2835_SPICS & SPI_CS_DONE));

	/* clear DONE bit */
	BCM2835_SPICS = SPI_CS_DONE;

	/* read buffer */
	buf = (char *)rxBuf + 3;	/* ignore 1st byte and align data */
	for (i=0; i<SPIBUFSIZE; i++) {
		*buf++ = BCM2835_SPIFIFO;
	}
}

void reset_board() {
	u32 x,i;

	/* GPIO 7 is configured as a tri-state output pin */

	/* set as output GPIO 7 */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (7*3));
	x |= (0b001 << (7*3));
	BCM2835_GPFSEL0 = x;

	/* board reset is active low */
	for (i=0; i<0x10000; i++)
		BCM2835_GPCLR0 = (1l << 7);

	/* wait until the board is ready */
	for (i=0; i<0x300000; i++)
		BCM2835_GPSET0 = (1l << 7);

	/* reset GPIO 7 back to input */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (7*3));
	BCM2835_GPFSEL0 = x;
}

int map_gpio() {
	int fd;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't open /dev/mem \n",modname);
		return -1;
	}

	/* mmap GPIO */
	gpio = mmap(
	        NULL,
	        BLOCK_SIZE,
	        PROT_READ|PROT_WRITE,
	        MAP_SHARED,
	        fd,
	        BCM2835_GPIO_BASE);

	if (gpio == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't map gpio\n",modname);
		close(fd);
		return -1;
	}

	/* mmap SPI */
	spi = mmap(
	        NULL,
	        BLOCK_SIZE,
	        PROT_READ|PROT_WRITE,
	        MAP_SHARED,
	        fd,
	        BCM2835_SPI_BASE);

	close(fd);

	if (spi == MAP_FAILED) {
		rtapi_print_msg(RTAPI_MSG_ERR,"%s: can't map spi\n",modname);
		return -1;
	}

	return 0;
}

void setup_gpio() {
	u32 x;

	/* disable UART pins and use as gpio */

	/* data ready GPIO 15, input */
	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (5*3));
	BCM2835_GPFSEL1 = x;

	/* data request GPIO 14, output */
	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (4*3));
	x |= (0b001 << (4*3));
	BCM2835_GPFSEL1 = x;

	/* clear request, active low */
	BCM2835_GPSET0 = (1l << 14);

	/* GPIO 23 24, output */
	x = BCM2835_GPFSEL2;
	x &= ~(0b111 << (3*3) | 0b111 << (4*3));
	x |= (0b001 << (3*3) | 0b001 << (4*3));
	BCM2835_GPFSEL2 = x;

	/* clear GPIO 23 24, active low */
	BCM2835_GPSET0 = (1l << 23 | 1l << 24);

	/* reset GPIO 7, tri-state, hi-Z */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (7*3));
	BCM2835_GPFSEL0 = x;

	/* change SPI pins */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (9*3));
	x |=   0b100 << (9*3);
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	x |= (0b100 << (0*3) | 0b100 << (1*3));
	BCM2835_GPFSEL1 = x;

	/* set up SPI */
	BCM2835_SPICLK = SPICLKDIV;

	BCM2835_SPICS = 0;

	/* clear FIFOs */
	BCM2835_SPICS |= SPI_CS_CLEAR_RX | SPI_CS_CLEAR_TX;

	/* clear done bit */
	BCM2835_SPICS |= SPI_CS_DONE;
}

void restore_gpio() {
	u32 x;

	/* restore UART */
	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (4*3) | 0b111 << (5*3));
	x |= (0b100 << (4*3) | 0b100 << (5*3));
	BCM2835_GPFSEL1 = x;

	/* change all used pins back to inputs */

	/* GPIO 7 */
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (7*3));
	BCM2835_GPFSEL0 = x;

	/* GPIO 23 24 */
	x = BCM2835_GPFSEL2;
	x &= ~(0b111 << (3*3) | 0b111 << (4*3));
	BCM2835_GPFSEL2 = x;

	/* change SPI pins to inputs*/
	x = BCM2835_GPFSEL0;
	x &= ~(0b111 << (9*3));
	BCM2835_GPFSEL0 = x;

	x = BCM2835_GPFSEL1;
	x &= ~(0b111 << (0*3) | 0b111 << (1*3));
	BCM2835_GPFSEL1 = x;
}
