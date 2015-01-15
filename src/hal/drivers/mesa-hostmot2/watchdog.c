
//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
//
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




// this is the function exported to HAL
// it keeps the watchdog from biting us for a while
static void hm2_pet_watchdog(void *void_hm2, long period_ns) {
    hostmot2_t *hm2 = void_hm2;
    static int print_warning = 1;

    if (print_warning) {
        HM2_PRINT("The hm2 pet_watchdog function is no longer needed.\n");
        HM2_PRINT("The hm2 write function now pets the watchdog.\n");
        print_warning = 0;
    }
}


void hm2_watchdog_process_tram_read(hostmot2_t *hm2) {
    // if there is no watchdog, then there's nothing to do
    if (hm2->watchdog.num_instances == 0) return;

    // if there are comm problems, wait for the user to fix it
    if ((*hm2->llio->io_error) != 0) return;

    // if we've already noticed the board needs to be reset, don't re-read
    // the watchdog has-bit bit
    // Note: we check needs_reset instead of the has-bit pin here, because
    // has-bit might be cleared by the user at any time, so using it here
    // would cause a race condition between this function and pet_watchdog
    if (hm2->llio->needs_reset != 0) return;

    // last time we were here, everything was fine
    // see if the watchdog has bit since then
    if (hm2->watchdog.status_reg[0] & 0x1) {
        HM2_PRINT("Watchdog has bit! (set the .has-bit pin to False to resume)\n");
        *hm2->watchdog.instance[0].hal.pin.has_bit = 1;
        hm2->llio->needs_reset = 1;
    }
}


int hm2_watchdog_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 3, 4, 0)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->watchdog.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }


    // 
    // special sanity checks for watchdog
    //

    if (md->instances != 1) {
        HM2_PRINT("MD declares %d watchdogs!  only using the first one...\n", md->instances);
    }


    // 
    // looks good, start initializing
    // 


    hm2->watchdog.num_instances = 1;

    hm2->watchdog.instance = (hm2_watchdog_instance_t *)hal_malloc(hm2->watchdog.num_instances * sizeof(hm2_watchdog_instance_t));
    if (hm2->watchdog.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->watchdog.clock_frequency = md->clock_freq;
    hm2->watchdog.version = md->version;

    hm2->watchdog.timer_addr = md->base_address + (0 * md->register_stride);
    hm2->watchdog.status_addr = md->base_address + (1 * md->register_stride);
    hm2->watchdog.reset_addr = md->base_address + (2 * md->register_stride);

    r = hm2_register_tram_read_region(hm2, hm2->watchdog.status_addr, (hm2->watchdog.num_instances * sizeof(u32)), &hm2->watchdog.status_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for watchdog (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->watchdog.reset_addr, sizeof(u32), &hm2->watchdog.reset_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for watchdog (%d)!\n", r);
        goto fail0;
    }

    // 
    // allocate memory for register buffers
    //

    hm2->watchdog.timer_reg = (u32 *)kmalloc(hm2->watchdog.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->watchdog.timer_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    //
    // export to HAL
    //

    // pins
    r = hal_pin_bit_newf(
        HAL_IO,
        &(hm2->watchdog.instance[0].hal.pin.has_bit),
        hm2->llio->comp_id,
        "%s.watchdog.has_bit",
        hm2->llio->name
    );
    if (r < 0) {
        HM2_ERR("error adding pin, aborting\n");
        r = -EINVAL;
        goto fail1;
    }

    // params
    r = hal_param_u32_newf(
        HAL_RW,
        &(hm2->watchdog.instance[0].hal.param.timeout_ns),
        hm2->llio->comp_id,
        "%s.watchdog.timeout_ns",
        hm2->llio->name
    );
    if (r < 0) {
        HM2_ERR("error adding param, aborting\n");
        r = -EINVAL;
        goto fail1;
    }


    // the function
    {
        char name[HAL_NAME_LEN + 1];

        rtapi_snprintf(name, sizeof(name), "%s.pet_watchdog", hm2->llio->name);
        r = hal_export_funct(name, hm2_pet_watchdog, hm2, 0, 0, hm2->llio->comp_id);
        if (r != 0) {
            HM2_ERR("error %d exporting pet_watchdog function %s\n", r, name);
            r = -EINVAL;
            goto fail1;
        }
    }


    //
    // initialize the watchdog
    //

    *hm2->watchdog.instance[0].hal.pin.has_bit = 0;
    hm2->watchdog.instance[0].hal.param.timeout_ns = 5 * 1000 * 1000;  // default timeout is 5 milliseconds
    hm2->watchdog.instance[0].enable = 0;  // the first pet_watchdog will turn it on

    return hm2->watchdog.num_instances;

fail1:
    kfree(hm2->watchdog.timer_reg);

fail0:
    hm2->watchdog.num_instances = 0;
    return r;
}


void hm2_watchdog_print_module(hostmot2_t *hm2) {
    int i;
    HM2_PRINT("Watchdog: %d\n", hm2->watchdog.num_instances);
    if (hm2->watchdog.num_instances <= 0) return;
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->watchdog.clock_frequency, hm2_hz_to_mhz(hm2->watchdog.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->watchdog.version);
    HM2_PRINT("    timer_addr: 0x%04X\n", hm2->watchdog.timer_addr);
    HM2_PRINT("    status_addr: 0x%04X\n", hm2->watchdog.status_addr);
    HM2_PRINT("    reset_addr: 0x%04X\n", hm2->watchdog.reset_addr);
    for (i = 0; i < hm2->watchdog.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        param timeout_ns = %u\n", hm2->watchdog.instance[i].hal.param.timeout_ns);
        HM2_PRINT("        pin has_bit = %d\n", (*hm2->watchdog.instance[i].hal.pin.has_bit));
        HM2_PRINT("        reg timer = 0x%08X\n", hm2->watchdog.timer_reg[i]);
    }
}


void hm2_watchdog_cleanup(hostmot2_t *hm2) {
    if (hm2->watchdog.num_instances <= 0) return;
    if (hm2->watchdog.timer_reg != NULL) kfree(hm2->watchdog.timer_reg);
}


void hm2_watchdog_prepare_tram_write(hostmot2_t *hm2) {
    if (hm2->watchdog.num_instances <= 0) return;
    hm2->watchdog.reset_reg[0] = 0x5a000000;
}


// timeout_s = (timer_counts + 1) / clock_hz
// (timeout_s * clock_hz) - 1 = timer_counts
// (timeout_ns * (1 s/1e9 ns) * clock_hz) - 1 = timer_counts
void hm2_watchdog_force_write(hostmot2_t *hm2) {
    u64 tmp;

    if (hm2->watchdog.num_instances != 1) return;

    if (hm2->watchdog.instance[0].enable == 0) {
        // watchdog is disabled, MSb=1 is secret handshake with FPGA
        hm2->watchdog.timer_reg[0] = 0x80000000;
    } else {
#ifdef __KERNEL__
	/* some gccs don't include 64-bit integer functions until
         linking with gcc, but kbuild links directly with ld; instead,
         use kernel-supplied functions; in this case, replace
         __udivdi3 with do_div */
        tmp = ((u64)hm2->watchdog.instance[0].hal.param.timeout_ns *
               hm2->watchdog.clock_frequency);
        do_div(tmp,(u64)(1000 * 1000 * 1000)) - 1;
#else
        tmp = ((u64)hm2->watchdog.instance[0].hal.param.timeout_ns *
	       hm2->watchdog.clock_frequency / (1000 * 1000 * 1000)) - 1;
#endif
        if (tmp < 0x80000000) {
            hm2->watchdog.timer_reg[0] = tmp;
        } else {
            // truncate watchdog timeout
            tmp = 0x7FFFFFFF;
            hm2->watchdog.timer_reg[0] = tmp;
            hm2->watchdog.instance[0].hal.param.timeout_ns = (tmp + 1) / ((double)hm2->watchdog.clock_frequency / (double)(1000 * 1000 * 1000));
            HM2_ERR("requested watchdog timeout is out of range, setting it to max: %u ns\n", hm2->watchdog.instance[0].hal.param.timeout_ns);
        }
    }

    // set the watchdog timeout (we'll check for i/o errors later)
    hm2->llio->write(hm2->llio, hm2->watchdog.timer_addr, hm2->watchdog.timer_reg, (hm2->watchdog.num_instances * sizeof(u32)));
    hm2->watchdog.instance[0].written_timeout_ns = hm2->watchdog.instance[0].hal.param.timeout_ns;
    hm2->watchdog.instance[0].written_enable = hm2->watchdog.instance[0].enable;

    // clear the has-bit bit
    hm2->llio->write(hm2->llio, hm2->watchdog.status_addr, hm2->watchdog.status_reg, sizeof(u32));
}


// if the user has changed the timeout, sync it out to the watchdog
void hm2_watchdog_write(hostmot2_t *hm2, long period_ns) {
    if (hm2->watchdog.num_instances != 1) return;

    // if there are comm problems, wait for the user to fix it
    if ((*hm2->llio->io_error) != 0) return;

    // if the watchdog has bit, wait for the user to reset it
    if (*hm2->watchdog.instance[0].hal.pin.has_bit) return;

    // writing to the watchdog wakes it up, and now we can't stop or it will bite!
    hm2->watchdog.instance[0].enable = 1;

    if (hm2->llio->needs_reset) {
        // user has cleared the bit
        HM2_PRINT("trying to recover from IO error or Watchdog bite\n");

        // reset the watchdog status
        hm2->watchdog.status_reg[0] = 0;

        // write all settings out to the FPGA
        hm2_force_write(hm2);
        if ((*hm2->llio->io_error) != 0) {
            HM2_PRINT("error recovery failed\n");
            return;
        }
        HM2_PRINT("error recover successful!\n");

        hm2->llio->needs_reset = 0;
    }

    if (
        (hm2->watchdog.instance[0].hal.param.timeout_ns == hm2->watchdog.instance[0].written_timeout_ns)
        &&
        (hm2->watchdog.instance[0].enable == hm2->watchdog.instance[0].written_enable)
    ) {
        return;
    }

    // if the requested timeout is dangerously short compared to the petting-period, warn the user once
    if (hm2->watchdog.instance[0].hal.param.timeout_ns < (1.5 * period_ns)) {
        HM2_PRINT(
            "Watchdog timeout (%u ns) is dangerously short compared to hm2_write() period (%ld ns)\n",
            hm2->watchdog.instance[0].hal.param.timeout_ns,
            period_ns
        );
    }

    hm2_watchdog_force_write(hm2);
}

