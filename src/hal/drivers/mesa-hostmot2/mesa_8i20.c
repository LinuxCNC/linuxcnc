
//
//    Copyright (C) 2010 Andy Pugh
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

#include "rtapi_math.h"
#include "hal/drivers/mesa-hostmot2/hostmot2.h"

/****************/

int hm2_8i20_create(hostmot2_t *hm2, hm2_module_descriptor_t *md) {
    int i,c,r;
    int n = -1;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {

        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        inst->hal_8i20 =
            (hal_8i20_t *)hal_malloc(inst->num_8i20 * sizeof(hal_8i20_t));
        if (inst->hal_8i20 == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail0;
        }


        inst->tram_8i20 =
        (hm2_sserial_tram_t *)kmalloc(inst->num_8i20 * sizeof(hm2_sserial_tram_t),
                                      GFP_KERNEL);
        if (inst->tram_8i20 == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail1;
        }

        n = -1;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            hal_8i20_t *hal;
            hm2_sserial_tram_t *tram;

            if (inst->tag_8i20 & (1 << c)) {

                n++;

                hal = &hm2->sserial.instance[i].hal_8i20[n];
                tram = &hm2->sserial.instance[i].tram_8i20[n];

                tram->tag = (1 << c);
                tram->reg_command_addr = inst->command_reg_addr;
                tram->reg_data_addr= inst->data_reg_addr;

                r = hal_pin_float_newf(HAL_IN, &(hal->pin.hm2_phase_angle),
                                    hm2->llio->comp_id,"%s.8i20.%1d.%1d.angle",
                                    hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.angle. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_float_newf(HAL_IN, &(hal->pin.hm2_current),
                                    hm2->llio->comp_id, "%s.8i20.%1d.%1d.current",
                                    hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.current. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_float_newf(HAL_OUT, &(hal->pin.hm2_bus_voltage),
                                    hm2->llio->comp_id, "%s.8i20.%1d.%1d.voltage",
                                    hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.voltage. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_float_newf(HAL_OUT, &(hal->pin.hm2_card_temp),
                                    hm2->llio->comp_id, "%s.8i20.%1d.%1d.temp",
                                    hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.temp. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_u32_newf(HAL_OUT, &(hal->pin.hm2_fault),
                                     hm2->llio->comp_id, "%s.8i20.%1d.%1d.fault",
                                     hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.fault. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_u32_newf(HAL_OUT, &(hal->pin.hm2_status),
                                     hm2->llio->comp_id, "%s.8i20.%1d.%1d.status",
                                     hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.status. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_u32_newf(HAL_OUT, &(hal->pin.hm2_comms),
                                     hm2->llio->comp_id, "%s.8i20.%1d.%1d.comms",
                                     hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.comms. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_bit_newf(HAL_IN, &(hal->pin.enable),
                                     hm2->llio->comp_id, "%s.8i20.%1d.%1d.amp_enable",
                                     hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.8i20.%1d.%1d.amp_enable. aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }


                // Create Parameters (normal mode)

                r = hal_param_float_newf(HAL_RW, &(hal->param.hm2_max_current),
                                         hm2->llio->comp_id, "%s.8i20.%1d.%1d.max_current",
                                         hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding parameter %s.8i20.%1d.%1d.max_current."
                            "aborting\n", hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_param_u32_newf(HAL_RW, &(hal->param.hm2_serialnumber),
                                       hm2->llio->comp_id, "%s.8i20.%1d.%1d.serial_number",
                                       hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding parameter %s.8i20.%1d.%1d.serial_"
                            "number. aborting\n",hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }


                // Register the TRAM values and store the register addresses for
                // other functions to use
                // Use tram_read in all modes, but only tram_write in normal mode.


                tram->reg_cs_addr = md->base_address + 2 * md->register_stride
                                    + inst->module_index * md->instance_stride
                                    + c * sizeof(u32);
                r = hm2_register_tram_read_region(hm2,
                                                  tram->reg_cs_addr,
                                                  sizeof(u32),
                                                  &tram->reg_cs_read);
                if (r < 0) {
                    HM2_ERR("error registering tram read region for sserial CS"
                            "register (%d)\n", r);
                    goto fail1;
                }

                tram->reg_0_addr = md->base_address + 3 * md->register_stride
                                    + inst->module_index * md->instance_stride
                                    + c * sizeof(u32);
                r = hm2_register_tram_read_region(hm2, tram->reg_0_addr,
                                                  sizeof(u32),
                                                  &tram->reg_0_read);
                if (r < 0) {
                    HM2_ERR("error registering tram read region for sserial "
                            "interface 0 register (%d)\n", r);
                    goto fail1;
                }

                tram->reg_1_addr = md->base_address + 4 * md->register_stride
                                   + inst->module_index * md->instance_stride
                                   + c * sizeof(u32);
                r = hm2_register_tram_read_region(hm2,
                                                  tram->reg_1_addr,
                                                  sizeof(u32),
                                                  &tram->reg_1_read);
                if (r < 0) {
                    HM2_ERR("error registering tram read region for sserial "
                            "interface 1 register (%d)\n", r);
                    goto fail1;
                }

                r = hm2_register_tram_write_region(hm2,
                                                   tram->reg_0_addr,
                                                   sizeof(u32),
                                                   &(tram->reg_0_write));
                if (r < 0) {
                    HM2_ERR("error registering tram write region for sserial"
                            "interface 0 register (%d)\n", r);
                    goto fail1;
                }
            }
        }
    }

    return n;
fail1:
    {
        int i;
        for (i = 1 ; i < hm2->sserial.num_instances; i++){
            if (hm2->sserial.instance[i].tram_8i20 != NULL){
                kfree(hm2->sserial.instance[i].tram_8i20);
            }
            if (hm2->sserial.instance[i].tram_7i64 != NULL){
                kfree(hm2->sserial.instance[i].tram_7i64);
            }
        }
    }
fail0:
    return r;

}

void hm2_8i20_prepare_tram_write(hostmot2_t *hm2){
    int i;
    int c;
    double angle_lim, norm_current;
    const float i_const = 32767;
    const float a_const = 182.0444479;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_8i20 ; c++){
            hal_8i20_t *hal = &inst->hal_8i20[c];
            hm2_sserial_tram_t *tram = &inst->tram_8i20[c];

            if (hal->param.hm2_max_current > hal->param.hm2_nv_max_current) {
                HM2_ERR("8i20 %X EEPROM Max Curent is %f.\n You asked for %f."
                        "Resetting to EEPROM max.",
                        hal->param.hm2_serialnumber,
                        hal->param.hm2_nv_max_current,
                        hal->param.hm2_max_current);
                hal->param.hm2_max_current = hal->param.hm2_nv_max_current;
            }
            if (hal->param.hm2_max_current < 0){
                HM2_ERR("8i20 minimum setting for max_current parameter is zero."
                        "Resetting to zero");
                hal->param.hm2_max_current = 0;
            }


            angle_lim = *hal->pin.hm2_phase_angle;
            if ( 0.0 > (angle_lim = 360.0 * (angle_lim - floor(angle_lim/360.0)))){
                angle_lim = 360.0 - angle_lim;
            }

            if (*hal->pin.hm2_current > 1.0) {*hal->pin.hm2_current = 1.0;}
            else if (*hal->pin.hm2_current < -1.0){*hal->pin.hm2_current = -1;}

            norm_current = *hal->pin.hm2_current
                           * hal->param.hm2_max_current
                           / hal->param.hm2_nv_max_current
                           * i_const;
            if (*hal->pin.enable) {
                *tram->reg_0_write = ((int)(norm_current) << 16)
                                    | ((u32)(angle_lim * a_const) & 0x0000FFFF);
            }
            else
            {
                *tram->reg_0_write = 0x00000000;
            }

        }
    }
}


void hm2_8i20_process_tram_read(hostmot2_t *hm2){
    int i;
    int c;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
         hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_8i20 ; c++){
            hal_8i20_t *hal = &inst->hal_8i20[c];
            hm2_sserial_tram_t *tram = &inst->tram_8i20[c];

            *hal->pin.hm2_comms = *tram->reg_cs_read;

            *hal->pin.hm2_card_temp=(hal_float_t)(*tram->reg_0_read & 0xFFFF);

            *hal->pin.hm2_bus_voltage=
                (hal_float_t)((*tram->reg_0_read & 0xFFFF0000) >> 16) * 0.01;

            *hal->pin.hm2_fault = (*tram->reg_1_read & 0xFFFF0000) >> 16;

            *hal->pin.hm2_status= *tram->reg_1_read & 0x0000FFFF;

        }
    }
}


int hm2_8i20_params(hostmot2_t *hm2){

    int i, c, buff;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (hm2->sserial.instance[i].num_8i20 > 0) {
            for (c = 0 ; c < hm2->sserial.instance[i].num_8i20 ; c++) {
                hal_8i20_t *hal = &inst->hal_8i20[c];
                hm2_sserial_tram_t *tram = &inst->tram_8i20[c];

                hm2->llio->read(hm2->llio, tram->reg_0_addr, &buff, sizeof(u32));
                hal->param.hm2_serialnumber = buff;
                buff = hm2_sserial_get_param(hm2, tram, 0x8E8);
                hal->param.hm2_nv_max_current = buff * 0.01;
                hal->param.hm2_max_current = buff * 0.01;
            }
        }
    }
    return 0;
}
