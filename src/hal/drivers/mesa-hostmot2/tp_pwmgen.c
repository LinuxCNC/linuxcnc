//
//    Copyright (C) 2010 Andy Pugh
//	  Heavily based on pwmgen.c (C) Sebastian Kuzminsky

//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"

void hm2_tp_pwmgen_handle_pwm_frequency(hostmot2_t *hm2) {
    u32 dds;
	int deadtime;
	int i;

    if (hm2->tp_pwmgen.hal->param.pwm_frequency < 1) {
        HM2_ERR("3pwmgen.pwm_frequency %d is too low, setting to 1\n",
            hm2->tp_pwmgen.hal->param.pwm_frequency);
        hm2->tp_pwmgen.hal->param.pwm_frequency = 1;
    }


    //
    // pwm_frequency is the user's desired PWM frequency in Hz
    //
    // PWMClock is controlled by the 16-bit PWM Master Rate DDS Register:
    //     PWMClock = ClockHigh * DDS / 65536
    //
    // Counter is fixed at 11 bits wide for 3-Phase PWM.

    // The key equation is:
    //     PWMFreq = PWMClock / 2048

    // Solve for DDS:
    //     PWMFreq * (65536 * 2048) = ClockHigh * DDS
    //     DDS = (PWMFreq * 65536 * 2048) / ClockHigh
    //

    // check that the frequency is achievable
    dds = ((double)hm2->tp_pwmgen.hal->param.pwm_frequency * 65536.0 * 2048.0)
            / (double)hm2->tp_pwmgen.clock_frequency;
    if (dds < 65536) {
        hm2->tp_pwmgen.pwmgen_master_rate_dds_reg = dds;
    } else {
	// not possible so try the fastest we can
        //     PWMFreq = (ClockHigh * DDS) / (65536 * 2048)
	dds = 65535;
	hm2->tp_pwmgen.hal->param.pwm_frequency =
            ((double)hm2->tp_pwmgen.clock_frequency * 65535.0)
            / (65536.0 * 2048.0);
	HM2_ERR("max PWM frequency is %d Hz\n",
            hm2->tp_pwmgen.hal->param.pwm_frequency);
	hm2->tp_pwmgen.pwmgen_master_rate_dds_reg = dds;
    }

    // Now recalculate the deadtime.
    // This is specified in nS in the pin but 2/TPPWMCLK in the register.
    // = (2*65536*1e9 / ClockHigh*dds)nS

    for (i=0; i < hm2->tp_pwmgen.num_instances; i++) {
        if (hm2->tp_pwmgen.instance[i].hal.param.sampletime > 1){
                HM2_ERR("Max sampletime is 1 (end of PWM cycle");
                hm2->tp_pwmgen.instance[i].hal.param.sampletime = 1;
        }
        else if (hm2->tp_pwmgen.instance[i].hal.param.sampletime < 0){
                HM2_ERR("Min sampletime is 0 (beginning of PWM cycle");
                hm2->tp_pwmgen.instance[i].hal.param.sampletime = 0;
        }

        deadtime = (hm2->tp_pwmgen.instance[i].hal.param.deadzone
            * hm2->tp_pwmgen.clock_frequency * dds) / (2 * 65536 * 1e9);

        if (deadtime > 511){
                deadtime = 511;
                hm2->tp_pwmgen.instance[i].hal.param.deadzone =
                    ((double)deadtime * 2.0 * 65536.0 * 1.0e9)
                    /((double)hm2->tp_pwmgen.clock_frequency * dds);
                HM2_ERR("At this PWM frequency the maximum deadtime is %dnS\n",
                    (int)hm2->tp_pwmgen.instance[i].hal.param.deadzone);
        }
        else if (deadtime < 0) {
                HM2_ERR("Deadtime must be positive");
                deadtime = 0;
                hm2->tp_pwmgen.instance[i].hal.param.deadzone = 0;
        }

        // Now setup the control register

        hm2->tp_pwmgen.setup_reg[i] = (
             ((int)(hm2->tp_pwmgen.instance[i].hal.param.sampletime*1023) << 16)
             +((hm2->tp_pwmgen.instance[i].hal.param.faultpolarity != 0) << 15)
             +(deadtime));
	}
}


void hm2_tp_pwmgen_force_write(hostmot2_t *hm2) {
    int i;

    if (hm2->tp_pwmgen.num_instances == 0) return;

    // This sets the pwmgen_dds register and also the  deadtime/sample time
    // register
    hm2_tp_pwmgen_handle_pwm_frequency(hm2);

    // update enable and setup registers
    for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
        if (*hm2->tp_pwmgen.instance[i].hal.pin.enable)
                hm2->tp_pwmgen.enable_reg[i] = 1;
        else
            hm2->tp_pwmgen.enable_reg[i] = 0;
    }

    //write out the values to their registers
    hm2->llio->write(hm2->llio, hm2->tp_pwmgen.setup_addr, hm2->tp_pwmgen.setup_reg, (hm2->tp_pwmgen.num_instances * sizeof(u32)));
    hm2->llio->write(hm2->llio, hm2->tp_pwmgen.enable_addr, hm2->tp_pwmgen.enable_reg, (hm2->tp_pwmgen.num_instances * sizeof(u32)));
    hm2->llio->write(hm2->llio, hm2->tp_pwmgen.pwmgen_master_rate_dds_addr, &hm2->tp_pwmgen.pwmgen_master_rate_dds_reg, sizeof(u32));

    if ((*hm2->llio->io_error) != 0) return;

    for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
        hm2->tp_pwmgen.instance[i].written_faultpolarity = hm2->tp_pwmgen.instance[i].hal.param.faultpolarity;
        hm2->tp_pwmgen.instance[i].written_deadzone = hm2->tp_pwmgen.instance[i].hal.param.deadzone;
        hm2->tp_pwmgen.instance[i].written_sampletime = hm2->tp_pwmgen.instance[i].hal.param.sampletime;
    }

    hm2->tp_pwmgen.written_pwm_frequency = hm2->tp_pwmgen.hal->param.pwm_frequency;
}


//
// Update the Registers of all Three Phase pwmgen instances that need it
//

void hm2_tp_pwmgen_write(hostmot2_t *hm2) {
    int i;

    if (hm2->tp_pwmgen.num_instances == 0) return;

    // check pwm frequency
    if (hm2->tp_pwmgen.hal->param.pwm_frequency != hm2->tp_pwmgen.written_pwm_frequency) goto force_write;

    // update enable register?
    for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
        if ((hm2->tp_pwmgen.instance[i].hal.param.deadzone != hm2->tp_pwmgen.instance[i].written_deadzone)
		 || (hm2->tp_pwmgen.instance[i].hal.param.sampletime != hm2->tp_pwmgen.instance[i].written_sampletime)
		 || (hm2->tp_pwmgen.instance[i].hal.param.faultpolarity != hm2->tp_pwmgen.instance[i].written_faultpolarity))
            goto force_write;

    // The enable register is a little odd. Writing a 1 to bit0 of the
    // register is a request to enable, however the state of the bit,
    // and the physical amp-enable pin, depends on the fault status.  So
    // rather than check with a "written_enable" variable we attempt to
    // enable if there is a mismatch between the hal-pin and the enable
    // bit.

        if ((*hm2->tp_pwmgen.instance[i].hal.pin.enable != (hm2->tp_pwmgen.enable_reg[i] & 1)))
            goto force_write;
    }

    return;

force_write:
    hm2_tp_pwmgen_force_write(hm2);
}

//
// Read the fault bit for each instance
//
void hm2_tp_pwmgen_read(hostmot2_t *hm2) {
    int i;
    hm2->llio->read(hm2->llio, hm2->tp_pwmgen.enable_addr, hm2->tp_pwmgen.enable_reg, (hm2->tp_pwmgen.num_instances * sizeof(u32)));
    for (i = 0 ; i < hm2->tp_pwmgen.num_instances ; i++) {
        *hm2->tp_pwmgen.instance[i].hal.pin.fault
            = (hm2->tp_pwmgen.enable_reg[i] & 2);
    }
}


int hm2_tp_pwmgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    //
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 4, 4, 0x0003)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->tp_pwmgen.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
	    hm2_get_general_function_name(md->gtag)
	);
        return -EINVAL;
    }

    if (hm2->config.num_tp_pwmgens > md->instances) {
        HM2_ERR(
            "config.num_3pwmgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_tp_pwmgens,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_tp_pwmgens == 0) {
        return 0;
    }

    //
    // looks good, start initializing
    //


    if (hm2->config.num_tp_pwmgens == -1) {
        hm2->tp_pwmgen.num_instances = md->instances;
    } else {
        hm2->tp_pwmgen.num_instances = hm2->config.num_tp_pwmgens;
    }


    // allocate the module-global HAL shared memory

    hm2->tp_pwmgen.instance = (hm2_tp_pwmgen_instance_t *)hal_malloc(hm2->tp_pwmgen.num_instances * sizeof(hm2_tp_pwmgen_instance_t));
    if (hm2->tp_pwmgen.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->tp_pwmgen.hal = (hm2_tp_pwmgen_global_hal_t *)hal_malloc(sizeof(hm2_tp_pwmgen_global_hal_t));
    if (hm2->tp_pwmgen.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    hm2->tp_pwmgen.clock_frequency = md->clock_freq;
    hm2->tp_pwmgen.version = md->version;

    hm2->tp_pwmgen.pwm_value_addr = md->base_address + (0 * md->register_stride);
    hm2->tp_pwmgen.pwmgen_master_rate_dds_addr = md->base_address + (3 * md->register_stride);
    hm2->tp_pwmgen.enable_addr = md->base_address + (1 * md->register_stride);
    hm2->tp_pwmgen.setup_addr = md->base_address + (2 * md->register_stride);

    //Allocate some memory for the parameter registers. The value equivalent is handled in the tram section
    hm2->tp_pwmgen.setup_reg = (u32 *)kmalloc(hm2->tp_pwmgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->tp_pwmgen.setup_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }
    hm2->tp_pwmgen.enable_reg = (u32 *)kmalloc(hm2->tp_pwmgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->tp_pwmgen.enable_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail2;
    }

    // Register the PWM values with the TRAM
    r = hm2_register_tram_write_region(hm2, hm2->tp_pwmgen.pwm_value_addr, (hm2->tp_pwmgen.num_instances * sizeof(u32)), &hm2->tp_pwmgen.pwm_value_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for 3PWM Value register (%d)\n", r);
        goto fail2;
    }

    // export to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 1];

        // this hal parameter affects all the 3-Phase pwmgen instances
        r = hal_param_u32_newf(
	    HAL_RW,
	    &(hm2->tp_pwmgen.hal->param.pwm_frequency),
	    hm2->llio->comp_id,
	    "%s.3pwmgen.frequency",
	    hm2->llio->name
        );
        if (r < 0) {
            HM2_ERR("error adding pin 3pwmgen.frequency param, aborting\n");
            goto fail2;
        }

        hm2->tp_pwmgen.hal->param.pwm_frequency = 20000;
        hm2->tp_pwmgen.written_pwm_frequency = 0; // force a write


        for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.A-value", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->tp_pwmgen.instance[i].hal.pin.Avalue), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.B-value", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->tp_pwmgen.instance[i].hal.pin.Bvalue), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.C-value", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->tp_pwmgen.instance[i].hal.pin.Cvalue), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail2;
            }

            rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.enable", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->tp_pwmgen.instance[i].hal.pin.enable), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.fault", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->tp_pwmgen.instance[i].hal.pin.fault), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail2;
            }

            // parameters

            rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.scale", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->tp_pwmgen.instance[i].hal.param.scale), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding param '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.deadtime", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->tp_pwmgen.instance[i].hal.param.deadzone), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding param '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.fault-invert", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->tp_pwmgen.instance[i].hal.param.faultpolarity), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding param '%s', aborting\n", name);
                goto fail2;
            }

	    rtapi_snprintf(name, sizeof(name), "%s.3pwmgen.%02d.sample-time", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->tp_pwmgen.instance[i].hal.param.sampletime), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding param '%s', aborting\n", name);
                goto fail2;
            }


            // init hal objects
            *(hm2->tp_pwmgen.instance[i].hal.pin.enable) = 0;
            *(hm2->tp_pwmgen.instance[i].hal.pin.Avalue) = 0.0;
	    *(hm2->tp_pwmgen.instance[i].hal.pin.Bvalue) = 0.0;
	    *(hm2->tp_pwmgen.instance[i].hal.pin.Cvalue) = 0.0;

            hm2->tp_pwmgen.instance[i].hal.param.scale = 1.0;
            hm2->tp_pwmgen.instance[i].hal.param.sampletime = 0.5;
	    hm2->tp_pwmgen.instance[i].hal.param.faultpolarity = 0.0; //active low
	    hm2->tp_pwmgen.instance[i].hal.param.deadzone = 5000; // 5000nS conservative deadzone for safety

            hm2->tp_pwmgen.instance[i].written_sampletime = -666;       // force an update at the start
        }
    }

    return hm2->tp_pwmgen.num_instances;

fail2:
    kfree(hm2->tp_pwmgen.enable_reg);

fail1:
    kfree(hm2->tp_pwmgen.setup_reg);

fail0:
    hm2->tp_pwmgen.num_instances = 0;
    return r;
}

void hm2_tp_pwmgen_print_module(hostmot2_t *hm2) {
    int i;
    HM2_PRINT("3-phase PWMGen: %d\n", hm2->tp_pwmgen.num_instances);
    if (hm2->tp_pwmgen.num_instances <= 0) return;
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->tp_pwmgen.clock_frequency, hm2_hz_to_mhz(hm2->tp_pwmgen.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->tp_pwmgen.version);
    HM2_PRINT("    pwmgen_master_rate_dds: 0x%08X (%d)\n", hm2->tp_pwmgen.pwmgen_master_rate_dds_reg, hm2->tp_pwmgen.pwmgen_master_rate_dds_reg);
    HM2_PRINT("    pwmgen_master_rate_dds_addr: 0x%04X\n", hm2->tp_pwmgen.pwmgen_master_rate_dds_addr);

    for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        hw:\n");
        HM2_PRINT("    pwm_value_addr: 0x%04X\n", hm2->tp_pwmgen.pwm_value_addr);
        HM2_PRINT("            pwm_val:0x%08X\n", hm2->tp_pwmgen.pwm_value_reg[i]);
        HM2_PRINT("            enable: 0x%08X\n", hm2->tp_pwmgen.enable_reg[i]);
        HM2_PRINT("             setup: 0x%08X\n", hm2->tp_pwmgen.setup_reg[i]);
    }
}


void hm2_tp_pwmgen_prepare_tram_write(hostmot2_t *hm2) {
    int i;

    if (hm2->tp_pwmgen.num_instances <= 0) return;

    for (i = 0; i < hm2->tp_pwmgen.num_instances; i ++) {
        double scaled_Avalue;
        double scaled_Bvalue;
        double scaled_Cvalue;

        // When disabled the 3pwmgen sets both sets of drivers to zero.
        // No need to handle that here (In contrast to the conventional
        // pwmgen)

        // Check for /0 problems
        if (hm2->tp_pwmgen.instance[i].hal.param.scale ==0) {
            hm2->tp_pwmgen.instance[i].hal.param.scale = 1;
            HM2_ERR("3pwmgen scale must be greater than zero. Scale set to %i", (int)hm2->tp_pwmgen.instance[i].hal.param.scale);
        }

        scaled_Avalue = (*hm2->tp_pwmgen.instance[i].hal.pin.Avalue / hm2->tp_pwmgen.instance[i].hal.param.scale);
        scaled_Bvalue = (*hm2->tp_pwmgen.instance[i].hal.pin.Bvalue / hm2->tp_pwmgen.instance[i].hal.param.scale);
        scaled_Cvalue = (*hm2->tp_pwmgen.instance[i].hal.pin.Cvalue / hm2->tp_pwmgen.instance[i].hal.param.scale);

        if (scaled_Avalue > 1.0) scaled_Avalue = 1.0;
        else if (scaled_Avalue < -1.0) scaled_Avalue = -1.0;
        if (scaled_Bvalue > 1.0) scaled_Bvalue = 1.0;
        else if (scaled_Avalue < -1.0) scaled_Avalue = -1.0;
        if (scaled_Cvalue > 1.0) scaled_Cvalue = 1.0;
        else if (scaled_Avalue < -1.0) scaled_Avalue = -1.0;

        // duty_cycle goes from 0.0 to 1.0, and needs to be puffed out to 10 bits

        hm2->tp_pwmgen.pwm_value_reg[i] = (
            ((long)(scaled_Cvalue * (511.0) + 512.0) << 20)
            + ((long)(scaled_Bvalue * (511.0) + 512.0) << 10)
            + ((long)(scaled_Avalue * (511.0) + 512.0) << 0)
        );
    }
}


void hm2_tp_pwmgen_cleanup(hostmot2_t *hm2) {
    if (hm2->tp_pwmgen.num_instances <= 0) return;
    if (hm2->tp_pwmgen.setup_reg != NULL) {
        kfree(hm2->tp_pwmgen.setup_reg);
        hm2->tp_pwmgen.enable_reg = NULL;
    }
    if (hm2->tp_pwmgen.enable_reg != NULL) {
        kfree(hm2->tp_pwmgen.enable_reg);
        hm2->tp_pwmgen.enable_reg = NULL;
    }
    hm2->tp_pwmgen.num_instances = 0;
}
