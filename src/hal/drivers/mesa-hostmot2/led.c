
//
//    Copyright (C) 2010 Andy Pugh

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

// Onboard LED driver for the Mesa FPGA cards





#include "hal/drivers/mesa-hostmot2/hostmot2.h"

int hm2_led_parse_md(hostmot2_t *hm2, int md_index) {

    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    //
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 1, 4, 0x0000)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }


    // LEDs were enumerated during llio setup
    
    if (hm2->llio->num_leds == 0 || hm2->config.num_leds == 0) return 0;

    if (hm2->config.num_leds > hm2->llio->num_leds) {
        hm2->config.num_leds = hm2->llio->num_leds;
        HM2_ERR( "There are only %d LEDs on this board type, defaulting to %d\n",
                hm2->llio->num_leds, hm2->config.num_leds );
    }
    else if (hm2->config.num_leds == -1) {
        hm2->config.num_leds = hm2->llio->num_leds;
    }

    //
    // looks good, start initializing
    //


    // allocate the module-global HAL shared memory
    hm2->led.instance = (hm2_led_instance_t *)hm2->llio->hal->malloc(hm2->llio->hal->ctx, hm2->config.num_leds * sizeof(hm2_led_instance_t));
    if (hm2->led.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }
    hm2->led.led_reg = (uint32_t *)hm2->llio->rtapi->calloc(hm2->llio->rtapi->ctx,  sizeof(uint32_t));
    if (hm2->led.led_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->led.led_addr = md->base_address;

    // export to HAL
    {
        int i;
        char name[256];
        for (i = 0 ; i < hm2->config.num_leds ; i++) {
            snprintf(name, sizeof(name), "%s.led.CR%02d", hm2->llio->name, i + 1 );
            r = gomc_hal_pin_bit_newf(hm2->llio->hal, GOMC_HAL_IN, &(hm2->led.instance[i].led), hm2->llio->comp_id, name);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }
        }
        return 1;

    fail1:

        hm2->llio->rtapi->free(hm2->llio->rtapi->ctx, hm2->led.led_reg);

    fail0:
        return r;

    }
}

void hm2_led_write(hostmot2_t *hm2) {
    uint32_t regval = 0;
    int i;

    for (i = 0 ; i < hm2->config.num_leds; i++ ) {
        if (*hm2->led.instance[i].led) {
            regval |= 1 << (31 - i);
        }
    }

    if (regval != hm2->led.written_buff) {
        *hm2->led.led_reg = regval;
        hm2->led.written_buff = regval;
        hm2->llio->write(hm2->llio, hm2->led.led_addr, hm2->led.led_reg, sizeof(uint32_t));
    }
}

void hm2_led_cleanup(hostmot2_t *hm2) {
    if (hm2->led.led_reg != NULL) {
	hm2->llio->rtapi->free(hm2->llio->rtapi->ctx, hm2->led.led_reg);
	hm2->led.led_reg = NULL;
    }
}
