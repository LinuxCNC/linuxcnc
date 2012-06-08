
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

#include <linux/slab.h>

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hostmot2.h"
#include "modules.h"
#include "watchdog.h"

// timeout_s = (timer_counts + 1) / clock_hz
// (timeout_s * clock_hz) - 1 = timer_counts
// (timeout_ns * (1 s/1e9 ns) * clock_hz) - 1 = timer_counts
static void force_write(hostmot2_t *hm2, void *void_module) {
    hm2_module_t *wd_module = void_module;
    hm2_watchdog_t *wd_data = (hm2_watchdog_t*) wd_module->data;
    u64 tmp;

    if (wd_data->instance[0].enable == 0) {
        // watchdog is disabled, MSb=1 is secret handshake with FPGA
        wd_data->timer_reg[0] = 0x80000000;
    } else {
        tmp = (wd_data->instance[0].hal.param.timeout_ns * ((double)wd_data->clock_frequency / (double)(1000 * 1000 * 1000))) - 1;
        if (tmp < 0x80000000) {
            wd_data->timer_reg[0] = tmp;
        } else {
            // truncate watchdog timeout
            tmp = 0x7FFFFFFF;
            wd_data->timer_reg[0] = tmp;
            wd_data->instance[0].hal.param.timeout_ns = (tmp + 1) / ((double)wd_data->clock_frequency / (double)(1000 * 1000 * 1000));
            HM2_ERR("requested watchdog timeout is out of range, setting it to max: %u ns\n", wd_data->instance[0].hal.param.timeout_ns);
        }
    }

    // set the watchdog timeout (we'll check for i/o errors later)
    hm2->llio->write(hm2->llio, wd_data->timer_addr, wd_data->timer_reg, (wd_data->num_instances * sizeof(u32)));
    wd_data->instance[0].written_timeout_ns = wd_data->instance[0].hal.param.timeout_ns;
    wd_data->instance[0].written_enable = wd_data->instance[0].enable;

    // re-warn the user if their requested timeout is too short
    wd_data->instance[0].warned_about_short_timeout = 0;

    // clear the has-bit bit
    hm2->llio->write(hm2->llio, wd_data->status_addr, wd_data->status_reg, sizeof(u32));
}

// if the user has changed the timeout, sync it out to the watchdog
static void write(hostmot2_t *hm2, void *void_module) {
    hm2_module_t *wd_module = void_module;
    hm2_watchdog_t *wd_data;

    if (wd_module == NULL) return;
    wd_data = wd_module->data;
    if ((wd_data->instance[0].hal.param.timeout_ns == wd_data->instance[0].written_timeout_ns) &&
        (wd_data->instance[0].enable == wd_data->instance[0].written_enable)) {
        return;
    }
    force_write(hm2, wd_module);
}

static void read(hostmot2_t *hm2, void *void_module) {
    hm2_module_t *wd_module = void_module;
    hm2_watchdog_t *wd_data;

    // if there is no watchdog, then there's nothing to do
    if (wd_module == NULL) return;
    wd_data = wd_module->data;

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
    hm2->llio->read(hm2->llio, wd_data->status_addr, wd_data->status_reg, (wd_data->num_instances * sizeof(u32)));
    if ((*hm2->llio->io_error) != 0) return;
    if (wd_data->status_reg[0] & 0x1) {
        HM2_PRINT("Watchdog has bit! (set the .has-bit pin to False to resume)\n");
        *wd_data->instance[0].hal.pin.has_bit = 1;
        hm2->llio->needs_reset = 1;
    }
}

static void cleanup(hostmot2_t *hm2, void *void_module) {
    hm2_module_t *wd_module = hm2_find_module(hm2, HM2_GTAG_WATCHDOG);
    hm2_watchdog_t *wd_data;

    if (wd_module == NULL) return;
    wd_data = wd_module->data;
    if (wd_data->status_reg != NULL) kfree(wd_data->status_reg);
    if (wd_data->reset_reg != NULL) kfree(wd_data->reset_reg);
    if (wd_data->timer_reg != NULL) kfree(wd_data->timer_reg);
}

void hm2_watchdog_print_module(hostmot2_t *hm2) {
    hm2_module_t *wd_module = hm2_find_module(hm2, HM2_GTAG_WATCHDOG);
    hm2_watchdog_t *wd_data;
    int i;

    // if there is no watchdog, then there's nothing to do
    if (wd_module == NULL) {
        HM2_PRINT("Watchdog: %d\n", 0);
        return;
    }
    wd_data = wd_module->data;

    HM2_PRINT("Watchdog: %d\n", wd_data->num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", wd_data->clock_frequency, hm2_hz_to_mhz(wd_data->clock_frequency));
    HM2_PRINT("    version: %d\n", wd_data->version);
    HM2_PRINT("    timer_addr: 0x%04X\n", wd_data->timer_addr);
    HM2_PRINT("    status_addr: 0x%04X\n", wd_data->status_addr);
    HM2_PRINT("    reset_addr: 0x%04X\n", wd_data->reset_addr);
    for (i = 0; i < wd_data->num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        param timeout_ns = %u\n", wd_data->instance[i].hal.param.timeout_ns);
        HM2_PRINT("        pin has_bit = %d\n", (*wd_data->instance[i].hal.pin.has_bit));
        HM2_PRINT("        reg timer = 0x%08X\n", wd_data->timer_reg[i]);
    }
}

// this is the function exported to HAL
// it keeps the watchdog from biting us for a while
static void hm2_pet_watchdog(void *void_hm2, long period_ns) {
    hostmot2_t *hm2 = void_hm2;
    hm2_module_t *wd_module = hm2_find_module(hm2, HM2_GTAG_WATCHDOG);
    hm2_watchdog_t *wd_data;

    // if there is no watchdog, then there's nothing to do
    if (wd_module == NULL) return;
    wd_data = wd_module->data;

    // if there are comm problems, wait for the user to fix it
    if ((*hm2->llio->io_error) != 0) return;

    // if the requested timeout is dangerously short compared to the petting-period, warn the user once
    if (wd_data->instance[0].hal.param.timeout_ns < (1.5 * period_ns)) {
        if (0 == wd_data->instance[0].warned_about_short_timeout) {
            wd_data->instance[0].warned_about_short_timeout = 1;
            HM2_PRINT(
                "Watchdog timeout (%u ns) is dangerously short compared to pet_watchdog() period (%ld ns)\n",
                wd_data->instance[0].hal.param.timeout_ns, period_ns
            );
        }
    }

    // if the watchdog has bit, wait for the user to reset it
    if (*wd_data->instance[0].hal.pin.has_bit) return;

    // petting the watchdog wakes it up, and now we can't stop or it will bite!
    wd_data->instance[0].enable = 1;

    if (hm2->llio->needs_reset) {
        // user has cleared the bit
        HM2_PRINT("trying to recover from IO error or Watchdog bite\n");

        // reset the watchdog status
        wd_data->status_reg[0] = 0;

        // write all settings out to the FPGA
        force_write(hm2, wd_module);
        if ((*hm2->llio->io_error) != 0) {
            HM2_PRINT("error recovery failed\n");
            return;
        }
        HM2_PRINT("error recover successful!\n");

        hm2->llio->needs_reset = 0;
    }

    // reset the watchdog timer
    // FIXME: write just 1 byte
    hm2->llio->write(hm2->llio, wd_data->reset_addr, wd_data->reset_reg, (wd_data->num_instances * sizeof(u32)));
}

int hm2_watchdog_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    hm2_module_t *module;
    hm2_watchdog_t *wd_data;
    int r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 3, 4, 0)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2_find_module(hm2, HM2_GTAG_WATCHDOG) != NULL) {
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

    module = (hm2_module_t*) kmalloc(sizeof(hm2_module_t), GFP_KERNEL);
    memset(module, 0, sizeof(hm2_module_t));
    list_add(&module->list, &hm2->modules);
    module->read = read;
    module->write = write;
    module->force_write = force_write;
    module->cleanup = cleanup;
    module->type = HM2_GTAG_WATCHDOG;

    wd_data = (hm2_watchdog_t *)kmalloc(sizeof(hm2_watchdog_t), GFP_KERNEL);
    memset(wd_data, 0, sizeof(hm2_watchdog_t));

    module->data = wd_data;
    wd_data->num_instances = 1;

    wd_data->instance = (hm2_watchdog_instance_t *)hal_malloc(wd_data->num_instances * sizeof(hm2_watchdog_instance_t));
    if (wd_data->instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    wd_data->clock_frequency = md->clock_freq;
    wd_data->version = md->version;

    wd_data->timer_addr = md->base_address + (0 * md->register_stride);
    wd_data->status_addr = md->base_address + (1 * md->register_stride);
    wd_data->reset_addr = md->base_address + (2 * md->register_stride);


    // 
    // allocate memory for register buffers
    //

    wd_data->status_reg = (u32 *)kmalloc(wd_data->num_instances * sizeof(u32), GFP_KERNEL);
    if (wd_data->status_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    wd_data->reset_reg = (u32 *)kmalloc(wd_data->num_instances * sizeof(u32), GFP_KERNEL);
    if (wd_data->reset_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }

    wd_data->timer_reg = (u32 *)kmalloc(wd_data->num_instances * sizeof(u32), GFP_KERNEL);
    if (wd_data->timer_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail2;
    }


    //
    // export to HAL
    //

    // pins
    r = hal_pin_bit_newf(
        HAL_IO,
        &(wd_data->instance[0].hal.pin.has_bit),
        hm2->llio->comp_id,
        "%s.watchdog.has_bit",
        hm2->llio->name
    );
    if (r < 0) {
        HM2_ERR("error adding pin, aborting\n");
        r = -EINVAL;
        goto fail3;
    }

    // params
    r = hal_param_u32_newf(
        HAL_RW,
        &(wd_data->instance[0].hal.param.timeout_ns),
        hm2->llio->comp_id,
        "%s.watchdog.timeout_ns",
        hm2->llio->name
    );
    if (r < 0) {
        HM2_ERR("error adding param, aborting\n");
        r = -EINVAL;
        goto fail3;
    }


    // the function
    {
        char name[HAL_NAME_LEN + 1];

        rtapi_snprintf(name, sizeof(name), "%s.pet_watchdog", hm2->llio->name);
        r = hal_export_funct(name, hm2_pet_watchdog, hm2, 0, 0, hm2->llio->comp_id);
        if (r != 0) {
            HM2_ERR("error %d exporting pet_watchdog function %s\n", r, name);
            r = -EINVAL;
            goto fail3;
        }
    }


    //
    // initialize the watchdog
    //

    *wd_data->instance[0].hal.pin.has_bit = 0;
    wd_data->instance[0].hal.param.timeout_ns = 5 * 1000 * 1000;  // default timeout is 5 milliseconds
    wd_data->instance[0].enable = 0;  // the first pet_watchdog will turn it on

    wd_data->instance[0].warned_about_short_timeout = 0;

    wd_data->reset_reg[0] = 0x5a000000;
    wd_data->status_reg[0] = 0;


    return wd_data->num_instances;


fail3:
    kfree(wd_data->timer_reg);

fail2:
    kfree(wd_data->reset_reg);

fail1:
    kfree(wd_data->status_reg);

fail0:
    wd_data->num_instances = 0;
    return r;
}
