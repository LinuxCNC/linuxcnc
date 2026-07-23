
//
//    Copyright (C) 2007-2009 Sebastian Kuzminsky
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

#include <rtapi_slab.h>

#include <rtapi.h>
#include <rtapi_string.h>
#include <rtapi_math.h>

#include <hal.h>

#include "hostmot2.h"


// cribbed from stepgen.c by PCW 9/25/19


// 
// read position and velocity to figure out where the galvo has gotten and how fast its swinging
// 

void hm2_xy2mod_process_tram_read(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->xy2mod.num_instances; i ++) {
    	hm2_xy2mod_instance_t *s = &hm2->xy2mod.instance[i];
        rtapi_u32 posx = hm2->xy2mod.posx_reg[i];
        rtapi_u32 posy = hm2->xy2mod.posy_reg[i];
        rtapi_u32 velx = hm2->xy2mod.velx_reg[i];
        rtapi_u32 vely = hm2->xy2mod.vely_reg[i];
	rtapi_u32 mode = hm2->xy2mod.mode_reg[i];
	rtapi_u32 status = hm2->xy2mod.status_reg[i];

        // those tricky users are always trying to get us to divide by zero
        rtapi_real posx_scale = hal_get_real(s->hal.pin.posx_scale);
        if (fabs(posx_scale) < 1e-6) {
            if (posx_scale >= 0.0) {
                posx_scale = hal_set_real(s->hal.pin.posx_scale, 1.0);
                HM2_ERR("xy2mod %d position_scalex is too close to 0, resetting to 1.0\n", i);
            } else {
                posx_scale = hal_set_real(s->hal.pin.posx_scale, -1.0);
                HM2_ERR("xy2mod %d position_scalxe is too close to 0, resetting to -1.0\n", i);
            }
        }

        rtapi_real posy_scale = hal_get_real(s->hal.pin.posy_scale);
        if (fabs(posy_scale) < 1e-6) {
            if (posy_scale >= 0.0) {
                posy_scale = hal_set_real(s->hal.pin.posy_scale, 1.0);
                HM2_ERR("xy2mod %d position_scaley is too close to 0, resetting to 1.0\n", i);
            } else {
                posy_scale = hal_set_real(s->hal.pin.posy_scale, -1.0);
                HM2_ERR("xy2mod %d position_scaley is too close to 0, resetting to -1.0\n", i);
            }
        }
	

        // The xy2mod position Registers are a 16.16 bit fixed-point
        // representation of the current galvanometer position.
        // The fractional part gives accurate velocity at low speeds, and
        // sub bit position feedback

	// need to "unscale" the position registers to return to machine units 

        hal_set_real(s->hal.pin.posx_fb, ((double)(int32_t)(posx) / 2147483647.0) / posx_scale);
        hal_set_real(s->hal.pin.posy_fb, ((double)(int32_t)(posy) / 2147483647.0) / posy_scale);

	// need to "unscale" the velocity registers to return to machine units 

	hal_set_real(s->hal.pin.velx_fb, ((double)(int32_t)velx) / (posx_scale * (256*2147483647.0/(double)hm2->xy2mod.clock_frequency)));
	hal_set_real(s->hal.pin.vely_fb, ((double)(int32_t)vely) / (posy_scale * (256*2147483647.0/(double)hm2->xy2mod.clock_frequency)));

	hal_set_bool(s->hal.pin.posx_overflow, mode & (1 << 6));
	hal_set_bool(s->hal.pin.posy_overflow, mode & (1 << 7));
	hal_set_bool(s->hal.pin.velx_overflow, mode & (1 << 8));
	hal_set_bool(s->hal.pin.vely_overflow, mode & (1 << 9));
	hal_set_ui32(s->hal.pin.status, status & 0x000FFFFF);


    }
}



static void hm2_xy2mod_instance_write(hostmot2_t *hm2, int i) {
    double stepsx_cmd;
    double stepsy_cmd;
    double steps_per_secx_cmd;
    double steps_per_secy_cmd;
    double steps_per_sec2x_cmd;
    double steps_per_sec2y_cmd;
    rtapi_u32 laccx_reg;
    rtapi_u32 laccy_reg;
    rtapi_u32 lvelx_reg;
    rtapi_u32 lvely_reg;
    rtapi_u32 lposx_reg;
    rtapi_u32 lposy_reg;
    rtapi_u32 istride;

    hm2_xy2mod_instance_t *s = &hm2->xy2mod.instance[i];
    istride = i * sizeof(rtapi_u32);
    	
    if (hal_get_bool(s->hal.pin.enable)) {
        // position setting
        rtapi_real posx_scale = hal_get_real(s->hal.pin.posx_scale);
        rtapi_real posy_scale = hal_get_real(s->hal.pin.posy_scale);
        rtapi_real posx_cmd = hal_get_real(s->hal.pin.posx_cmd);
        rtapi_real posy_cmd = hal_get_real(s->hal.pin.posy_cmd);
        stepsx_cmd = (posx_cmd * 2147483647.0) / posx_scale;
        stepsy_cmd = (posy_cmd * 2147483647.0) / posy_scale;
        // the double cast here is intentional.  (uint32_t)(-1.0) is undefined in
        // C (and in practice it gives the undesired value 0 on arm systems), but
        // (uint32_t)(int32-t)(-1.0) is defined and gives the desired value on all
        // systems.
        if (posx_cmd != s->prev_posx_cmd) {
	    lposx_reg = (int32_t)(stepsx_cmd);
            hm2->xy2mod.posx_reg[i] = lposx_reg;
        //  HM2_PRINT("StepsX register set to: %f\n",stepsx_cmd);
        //  HM2_PRINT("POSX register set to:0x%08X\n",lposx_reg);
            hm2->llio->write(hm2->llio, hm2->xy2mod.posx_addr + istride, &hm2->xy2mod.posx_reg[i], sizeof(rtapi_u32));
	    s->prev_posx_cmd = posx_cmd;
        }

        if (posy_cmd != s->prev_posy_cmd) {
            lposy_reg = (int32_t)(stepsy_cmd);
            hm2->xy2mod.posy_reg[i] = lposy_reg;
        //  HM2_PRINT("Stepsy register set to: %fhalcmd\n",stepsy_cmd);
        //  HM2_PRINT("POSY register set to:0x%08X\n",lposy_reg);
            hm2->llio->write(hm2->llio, hm2->xy2mod.posy_addr + istride, &hm2->xy2mod.posy_reg[i], sizeof(rtapi_u32));
            s->prev_posy_cmd = posy_cmd;
        }

        // velocity setting
        rtapi_real velx_cmd = hal_get_real(s->hal.pin.velx_cmd);
        rtapi_real vely_cmd = hal_get_real(s->hal.pin.vely_cmd);
        steps_per_secx_cmd = velx_cmd * posx_scale;
        steps_per_secy_cmd = vely_cmd * posy_scale;
        // the double cast here is intentional.  (uint32_t)(-1.0) is undefined in
        // C (and in practice it gives the undesired value 0 on arm systems), but
        // (uint32_t)(int32-t)(-1.0) is defined and gives the desired value on all
        // systems.
	// Note 256 factor is from velocity scaling in xy2mod hardware (velocity added only every 256 clocks)
        if (velx_cmd != s->prev_velx_cmd) {
            lvelx_reg = (uint32_t)(int32_t)(steps_per_secx_cmd * (256*2147483648.0 / (double)hm2->xy2mod.clock_frequency));
            hm2->xy2mod.velx_reg[i] = lvelx_reg;
    //      HM2_PRINT("VELX register set to:0x%08X\n",lvelx_reg);
            hm2->llio->write(hm2->llio, hm2->xy2mod.velx_addr + istride, &hm2->xy2mod.velx_reg[i], sizeof(rtapi_u32));
            s->prev_velx_cmd = velx_cmd;
        }

        if (vely_cmd != s->prev_vely_cmd) {
            lvely_reg = (uint32_t)(int32_t)(steps_per_secy_cmd * (256*2147483648.0 / (double)hm2->xy2mod.clock_frequency));
            hm2->xy2mod.vely_reg[i] = lvely_reg;
    //      HM2_PRINT("VELY register set to:0x%08X\n",lvely_reg);
            hm2->llio->write(hm2->llio, hm2->xy2mod.vely_addr + istride, &hm2->xy2mod.vely_reg[i], sizeof(rtapi_u32));
            s->prev_vely_cmd = vely_cmd;
        }


        // acceleration setting no idea if anywhere near reality
        rtapi_real accx_cmd = hal_get_real(s->hal.pin.accx_cmd);
        rtapi_real accy_cmd = hal_get_real(s->hal.pin.accy_cmd);
        steps_per_sec2x_cmd = accx_cmd * posx_scale;
        steps_per_sec2y_cmd = accy_cmd * posy_scale;
        // the double cast here is intentional.  (uint32_t)(-1.0) is undefined in
        // C (and in practice it gives the undesired value 0 on arm systems), but
        // (uint32_t)(int32-t)(-1.0) is defined and gives the desired value on all
        // systems.
        if (accx_cmd != s->prev_accx_cmd) {
        laccx_reg = (uint32_t)(int32_t)(steps_per_sec2x_cmd * (double)(4096.0*4294967296.0*32768.0) / ((double)(hm2->xy2mod.clock_frequency*(double)hm2->xy2mod.clock_frequency*256)));
        hm2->xy2mod.accx_reg[i] = laccx_reg;
        //    HM2_PRINT("ACCX register set to:0x%08X\n",laccx_reg);
        hm2->llio->write(hm2->llio, hm2->xy2mod.accx_addr + istride, &hm2->xy2mod.accx_reg[i], sizeof(rtapi_u32));
        s->prev_accx_cmd = accx_cmd;
        }

        if (accy_cmd != s->prev_accy_cmd) {
        laccy_reg = (uint32_t)(int32_t)(steps_per_sec2y_cmd * (double)(4096.0*4294967296.0*32768.0) / ((double)(hm2->xy2mod.clock_frequency*(double)hm2->xy2mod.clock_frequency*256)));
        hm2->xy2mod.accy_reg[i] = laccy_reg;
        //    HM2_PRINT("ACCY register set to:0x%08X\n",laccy_reg);
        hm2->llio->write(hm2->llio, hm2->xy2mod.accy_addr + istride, &hm2->xy2mod.accy_reg[i], sizeof(rtapi_u32));
        s->prev_accy_cmd = accy_cmd;
        }
	hal_set_ui32(s->hal.pin.controlx, hal_get_ui32(s->hal.pin.controlx) & 7);
	hal_set_ui32(s->hal.pin.controly, hal_get_ui32(s->hal.pin.controly) & 7);
        
	hm2->xy2mod.mode_reg[i] = 
        (hal_get_ui32(s->hal.pin.controlx) << 0)  |
        (hal_get_ui32(s->hal.pin.controly) << 3)  |
        (hal_get_bool(s->hal.pin.mode18bitx)<< 10) |
        (hal_get_bool(s->hal.pin.mode18bity)<< 11) |
        (hal_get_bool(s->hal.pin.commandmodex)<< 12) |
        (hal_get_bool(s->hal.pin.commandmodey)<< 13);

        hm2->llio->write(hm2->llio,hm2->xy2mod.mode_addr + istride, &hm2->xy2mod.mode_reg[i], sizeof(rtapi_u32));

	hal_set_ui32(s->hal.pin.commandx, hal_get_ui32(s->hal.pin.commandx) & 0xFFFF);
	hal_set_ui32(s->hal.pin.commandy, hal_get_ui32(s->hal.pin.commandy) & 0xFFFF);

	hm2->xy2mod.command_reg[i] = 
            (hal_get_ui32(s->hal.pin.commandx) << 0)  |
            (hal_get_ui32(s->hal.pin.commandy) << 16);

        hm2->llio->write(hm2->llio,hm2->xy2mod.command_addr + istride, &hm2->xy2mod.command_reg[i], sizeof(rtapi_u32));

    } else {
// if enable is false reset all registers
// may eventually want to spilt out resetting the overflow flags

// reset position registers
        hal_set_real(s->hal.pin.posx_cmd, 0);
   	s->prev_posx_cmd = 0;
        hm2->xy2mod.posx_reg[i] =0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.posx_addr + istride, &hm2->xy2mod.posx_reg[i], sizeof(rtapi_u32));

        hal_set_real(s->hal.pin.posy_cmd, 0);
   	s->prev_posy_cmd = 0;
        hm2->xy2mod.posy_reg[i] =0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.posy_addr + istride, &hm2->xy2mod.posy_reg[i], sizeof(rtapi_u32));
 

// reset velocity registers
        hal_set_real(s->hal.pin.velx_cmd, 0);
        s->prev_velx_cmd = 0;
        hm2->xy2mod.velx_reg[i] = 0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.velx_addr + istride, &hm2->xy2mod.velx_reg[i], sizeof(rtapi_u32));

        hal_set_real(s->hal.pin.vely_cmd, 0);
        s->prev_vely_cmd = 0;
        hm2->xy2mod.vely_reg[i] = 0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.vely_addr + istride, &hm2->xy2mod.vely_reg[i], sizeof(rtapi_u32));

// reset acceleration registers
        hal_set_real(s->hal.pin.accx_cmd, 0);
        s->prev_accx_cmd = 0;
        hm2->xy2mod.accx_reg[i] = 0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.accx_addr + istride, &hm2->xy2mod.accx_reg[i], sizeof(rtapi_u32));

        hal_set_real(s->hal.pin.accy_cmd, 0);
        s->prev_accy_cmd = 0;
        hm2->xy2mod.accy_reg[i] = 0;
        hm2->llio->write(hm2->llio, hm2->xy2mod.accy_addr + istride, &hm2->xy2mod.accy_reg[i], sizeof(rtapi_u32));

//reset control bits and overflow flags

        hal_set_ui32(s->hal.pin.controlx, 1);  // default to 16 bit mode
        hal_set_ui32(s->hal.pin.controly, 1);
        hm2->xy2mod.mode_reg[i] = 0x000003C9; // reset X and Y overflow bits when disabled
        hm2->llio->write(hm2->llio, hm2->xy2mod.mode_addr + istride, &hm2->xy2mod.mode_reg[i], sizeof(rtapi_u32));
    }   
    
}

static void hm2_xy2mod_set_dpll_rtimer(hostmot2_t *hm2) {
    rtapi_u32 data = 0;
    rtapi_s32 dpll_rtimer_num = hal_get_si32(hm2->xy2mod.hal->pin.dpll_rtimer_num);

    if ((dpll_rtimer_num < -1) || (dpll_rtimer_num > 4)) {
        dpll_rtimer_num = hal_set_si32(hm2->xy2mod.hal->pin.dpll_rtimer_num, 0);
    }
    if (dpll_rtimer_num > -1) {
        data = (dpll_rtimer_num << 12) | (1 << 15);
    }
    hm2->llio->write(hm2->llio, hm2->xy2mod.dpll_rtimer_num_addr, &data, sizeof(rtapi_u32));
    hm2->xy2mod.written_dpll_rtimer_num = dpll_rtimer_num;
}

static void hm2_xy2mod_set_dpll_wtimer(hostmot2_t *hm2) {
    rtapi_u32 data = 0;
    rtapi_s32 dpll_wtimer_num = hal_get_si32(hm2->xy2mod.hal->pin.dpll_wtimer_num);

    if ((dpll_wtimer_num < -1) || (dpll_wtimer_num > 4)) {
        dpll_wtimer_num = hal_set_si32(hm2->xy2mod.hal->pin.dpll_wtimer_num, 0);
    }
    if (dpll_wtimer_num > -1) {
        data = (dpll_wtimer_num << 12) | (1 << 15);
    }
    hm2->llio->write(hm2->llio, hm2->xy2mod.dpll_wtimer_num_addr, &data, sizeof(rtapi_u32));
    hm2->xy2mod.written_dpll_wtimer_num = dpll_wtimer_num;
}


void hm2_xy2mod_write(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->xy2mod.num_instances; i ++) {
         hm2_xy2mod_instance_write(hm2, i);
    }
    if (hm2->xy2mod.num_instances > 0 && hm2->dpll_module_present) {
        if (hal_get_si32(hm2->xy2mod.hal->pin.dpll_rtimer_num) != hm2->xy2mod.written_dpll_rtimer_num) {
            hm2_xy2mod_set_dpll_rtimer(hm2);
        }
        if (hal_get_si32(hm2->xy2mod.hal->pin.dpll_wtimer_num) != hm2->xy2mod.written_dpll_wtimer_num) {
            hm2_xy2mod_set_dpll_wtimer(hm2);
        }
    }
}







static void hm2_xy2mod_force_write_dpll_timer(hostmot2_t *hm2) {
    if (hm2->xy2mod.num_instances > 0 && hm2->dpll_module_present) {
        hm2_xy2mod_set_dpll_rtimer(hm2);
        hm2_xy2mod_set_dpll_wtimer(hm2);
    }
}



void hm2_xy2mod_force_write(hostmot2_t *hm2) {
    if (hm2->xy2mod.num_instances == 0) return;
    hm2_xy2mod_force_write_dpll_timer(hm2);
}




void hm2_xy2mod_tram_init(hostmot2_t *hm2) {
    for (int i = 0; i < hm2->xy2mod.num_instances; i ++) {
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.posx_cmd, 0);
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.posy_cmd, 0);
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.velx_cmd, 0);
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.vely_cmd, 0);
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.accx_cmd, 0);
        hal_set_real(hm2->xy2mod.instance[i].hal.pin.accy_cmd, 0);
    }
}




void hm2_xy2mod_allocate_pins(hostmot2_t *hm2) {
    HM2_PRINT("allocate pins entry");
    int i;

    for (i = 0; i < hm2->num_pins; i ++) {
        if (
            (hm2->pin[i].sec_tag != HM2_GTAG_XY2MOD)
            || (hm2->pin[i].sec_unit >= hm2->xy2mod.num_instances)
        ) {
            continue;
        }

        hm2_set_pin_source(hm2, i, HM2_PIN_SOURCE_IS_SECONDARY);
        if (hm2->pin[i].sec_pin & 0x80){
            hm2_set_pin_direction_at_start(hm2, i, HM2_PIN_DIR_IS_OUTPUT);
        }
    }
    HM2_PRINT("allocate pins exit");
}




int hm2_xy2mod_parse_md(hostmot2_t *hm2, int md_index) {

    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //

    if (hm2_md_is_consistent(hm2, md_index, 0, 11, 4, 0x01FF)) {
        // this one is ok, as far as we know
    } else {
        HM2_ERR("unknown xy2mod MD:\n");
        HM2_ERR("    Version = %d, expected 0-2\n", md->version);
        HM2_ERR("    NumRegisters = %d, expected 11\n", md->num_registers);
        HM2_ERR("    InstanceStride = 0x%08X, expected 4\n", md->instance_stride);
        HM2_ERR("    MultipleRegisters = 0x%08X, expected 0x000001FF\n", md->multiple_registers);
        return -EINVAL;
    }

    if (hm2->xy2mod.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_xy2mods > md->instances) {
        HM2_ERR(
            "config.num_xy2mods=%d, but only %d are available, not loading driver\n",
            hm2->config.num_xy2mods,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_xy2mods == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_xy2mods == -1) {
        hm2->xy2mod.num_instances = md->instances;
    } else {
        hm2->xy2mod.num_instances = hm2->config.num_xy2mods;
    }


    // allocate the module-global HAL shared memory
    hm2->xy2mod.hal = hal_malloc(sizeof(*hm2->xy2mod.hal));
    if (hm2->xy2mod.hal == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->xy2mod.instance = hal_malloc(hm2->xy2mod.num_instances * sizeof(*hm2->xy2mod.instance));
    if (hm2->xy2mod.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->xy2mod.clock_frequency = md->clock_freq;
    hm2->xy2mod.version = md->version;

    hm2->xy2mod.accx_addr = md->base_address + (0 * md->register_stride);
    hm2->xy2mod.accy_addr = md->base_address + (1 * md->register_stride);
    hm2->xy2mod.velx_addr = md->base_address + (2 * md->register_stride);
    hm2->xy2mod.vely_addr = md->base_address + (3 * md->register_stride);
    hm2->xy2mod.posx_addr = md->base_address + (4 * md->register_stride);
    hm2->xy2mod.posy_addr = md->base_address + (5 * md->register_stride);
    hm2->xy2mod.mode_addr = md->base_address + (6 * md->register_stride);
    hm2->xy2mod.command_addr = md->base_address + (7 * md->register_stride);
    hm2->xy2mod.status_addr = md->base_address + (8 * md->register_stride);
    hm2->xy2mod.dpll_rtimer_num_addr = md->base_address + (9 * md->register_stride);
    hm2->xy2mod.dpll_wtimer_num_addr = md->base_address + (10 * md->register_stride);



    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.posx_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.posx_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod X Position register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.posy_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.posy_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod Y Position register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.velx_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.velx_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod X Velocity register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.vely_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.vely_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod Y Velocity register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.accx_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.accx_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod X Acceleration register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.accy_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.accy_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod Y Acceleration register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.mode_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.mode_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod mode register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.status_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.status_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod status register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->xy2mod.command_addr, (hm2->xy2mod.num_instances * sizeof(rtapi_u32)), &hm2->xy2mod.command_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for xy2mod command register (%d)\n", r);
        goto fail0;
    }

    hm2->xy2mod.command_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->xy2mod.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->xy2mod.command_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    // export to HAL

    {
        if (hm2->dpll_module_present) {
            r = hal_pin_new_si32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.hal->pin.dpll_rtimer_num),
                                 -1, "%s.xy2mod.read-timer-number", hm2->llio->name);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.read-timer-number', aborting\n", r, hm2->llio->name);
                return -EINVAL;
            }

            r = hal_pin_new_si32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.hal->pin.dpll_wtimer_num),
                                 -1, "%s.xy2mod.write-timer-number", hm2->llio->name);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.write-timer-number', aborting\n", r, hm2->llio->name);
                return -EINVAL;
            }
        }

        for (int i = 0; i < hm2->xy2mod.num_instances; i ++) {
            // pins
            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.posx_cmd),
                                 0.0, "%s.xy2mod.%02d.posx-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posx-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.posy_cmd),
                                 0.0, "%s.xy2mod.%02d.posy-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posy-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.velx_cmd),
                                 0.0, "%s.xy2mod.%02d.velx-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.velx-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.vely_cmd),
                                 0.0, "%s.xy2mod.%02d.vely-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.vely-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.accx_cmd),
                                 0.0, "%s.xy2mod.%02d.accx-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.accx-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.accy_cmd),
                                 0.0, "%s.xy2mod.%02d.accy-cmd", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.accy-cmd', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.velx_fb),
                                 0.0, "%s.xy2mod.%02d.velx-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.velx-fb', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.vely_fb),
                                 0.0, "%s.xy2mod.%02d.vely-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.vely-fb', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.posx_fb),
                                 0.0, "%s.xy2mod.%02d.posx-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posx-fb', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.posy_fb),
                                 0.0, "%s.xy2mod.%02d.posy-fb", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posy-fb', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }
 
            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.posx_scale),
                                 1.0, "%s.xy2mod.%02d.posx-scale", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posx-scale', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.posy_scale),
                                 1.0, "%s.xy2mod.%02d.posy-scale", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posy-scale', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.enable),
                                 0, "%s.xy2mod.%02d.enable", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.enable', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }
 
            r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.controlx),
                                 1, "%s.xy2mod.%02d.controlx", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.controlx', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

 
            r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.controly),
                                 1, "%s.xy2mod.%02d.controly", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.controly', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.commandx),
                                 0, "%s.xy2mod.%02d.commandx", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.commandx', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

 
            r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.commandy),
                                 0, "%s.xy2mod.%02d.commandy", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.commandy', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }


            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.mode18bitx),
                                 0, "%s.xy2mod.%02d.18bitmodex", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.18bitmodex', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.mode18bity),
                                 0, "%s.xy2mod.%02d.18bitmodey", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.18bitmodey', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.commandmodex),
                                 0, "%s.xy2mod.%02d.commandmodex", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.commandmodex', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->xy2mod.instance[i].hal.pin.commandmodey),
                                 0, "%s.xy2mod.%02d.commandmodey", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.commandmodey', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.posx_overflow),
                                 0, "%s.xy2mod.%02d.posx-overflow", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posx-overflow', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.posy_overflow),
                                 0, "%s.xy2mod.%02d.posy-overflow", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.posy-overflow', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.velx_overflow),
                                 0, "%s.xy2mod.%02d.velx-overflow", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.velx-overflow', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }

            r = hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.vely_overflow),
                                 0, "%s.xy2mod.%02d.vely-overflow", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.vely-overflow', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }
 
            r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &(hm2->xy2mod.instance[i].hal.pin.status),
                                 0, "%s.xy2mod.%02d.status", hm2->llio->name, i);
            if (r < 0) {
                HM2_ERR("error %d adding pin '%s.xy2mod.%02d.status', aborting\n", r, hm2->llio->name, i);
                goto fail0;
            }
        }
    }


    return hm2->xy2mod.num_instances;

fail0:
    hm2->xy2mod.num_instances = 0;
    return r;
}

void hm2_xy2mod_cleanup(hostmot2_t *hm2) {
    if (hm2->xy2mod.num_instances <= 0) {
        return;
//  possible cleanup code  
    }
}

void hm2_xy2mod_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->xy2mod.num_instances <= 0) return;
    HM2_PRINT("xy2mod: %d\n", hm2->xy2mod.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->xy2mod.clock_frequency, hm2_hz_to_mhz(hm2->xy2mod.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->xy2mod.version);
    HM2_PRINT("    accx_addr: 0x%04X\n", hm2->xy2mod.accx_addr);
    HM2_PRINT("    accy_addr: 0x%04X\n", hm2->xy2mod.accy_addr);
    HM2_PRINT("    velx_addr: 0x%04X\n", hm2->xy2mod.velx_addr);
    HM2_PRINT("    vely_addr: 0x%04X\n", hm2->xy2mod.vely_addr);
    HM2_PRINT("    posx_addr: 0x%04X\n", hm2->xy2mod.posx_addr);
    HM2_PRINT("    posy_addr: 0x%04X\n", hm2->xy2mod.posy_addr);
    HM2_PRINT("    mode_addr: 0x%04X\n", hm2->xy2mod.mode_addr);
    HM2_PRINT("    command_addr: 0x%04X\n", hm2->xy2mod.command_addr);
    HM2_PRINT("    status_addr: 0x%04X\n", hm2->xy2mod.status_addr);
    for (i = 0; i < hm2->xy2mod.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        enable = %d\n", hal_get_bool(hm2->xy2mod.instance[i].hal.pin.enable));
        HM2_PRINT("        hw:\n");
        HM2_PRINT("            accx = 0x%08X\n", hm2->xy2mod.accx_reg[i]);
        HM2_PRINT("            accy = 0x%08X\n", hm2->xy2mod.accy_reg[i]);
        HM2_PRINT("            velx = 0x%08X\n", hm2->xy2mod.velx_reg[i]);
        HM2_PRINT("            vely = 0x%08X\n", hm2->xy2mod.vely_reg[i]);
        HM2_PRINT("            posx = 0x%08X\n", hm2->xy2mod.posx_reg[i]);
        HM2_PRINT("            posy = 0x%08X\n", hm2->xy2mod.posy_reg[i]);
        HM2_PRINT("            mode = 0x%08X\n", hm2->xy2mod.mode_reg[i]);
        HM2_PRINT("            mode = 0x%08X\n", hm2->xy2mod.command_reg[i]);
        HM2_PRINT("            mode = 0x%08X\n", hm2->xy2mod.status_reg[i]);
    }
}

