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
#include <linux/slab.h>
#include "hal/drivers/mesa-hostmot2/hostmot2.h"


int hm2_7i64_create(hostmot2_t *hm2, hm2_module_descriptor_t *md) {
    int i,c,r = 0,p;
    int n = -1;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {

        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        inst->hal_7i64 =
        (hal_7i64_t *)hal_malloc(inst->num_7i64 * sizeof(hal_7i64_t));
        if (inst->hal_7i64 == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail0;
        }

        inst->tram_7i64 =
        (hm2_sserial_tram_t *)kmalloc(inst->num_7i64 * sizeof(hm2_sserial_tram_t),
                                      GFP_KERNEL);
        if (inst->tram_7i64 == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail1;
        }

        n = -1;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            hal_7i64_t *hal;
            hm2_sserial_tram_t *tram;

            if (inst->tag_7i64 & (1 << c)) {

                n++;

                hal = &hm2->sserial.instance[i].hal_7i64[n];
                tram = &hm2->sserial.instance[i].tram_7i64[n];

                tram->tag = (1 << c);
                tram->reg_command_addr = inst->command_reg_addr;
                tram->reg_data_addr= inst->data_reg_addr;

                for (p = 0; p < 24 ; p++){

                    r = hal_pin_bit_newf(HAL_OUT, &(hal->pin.digital_in[p]),
                                         hm2->llio->comp_id,
                                         "%s.7i64.%1d.%1d.digin.%02d.in",
                                         hm2->llio->name, inst->module_index, 
                                         c, p);
                    if (r < 0) {
                        HM2_ERR("error adding pin %s.7i64.%1d.%1d.digin.%02d.in"
                                ", aborting\n",
                                hm2->llio->name, inst->module_index, c, p);
                        goto fail1;
                    }
                    r = hal_pin_bit_newf(HAL_OUT, &(hal->pin.digital_in_not[p]),
                                         hm2->llio->comp_id,
                                         "%s.7i64.%1d.%1d.digin.%02d.in-not",
                                         hm2->llio->name, inst->module_index, 
                                         c, p);
                    if (r < 0) {
                        HM2_ERR("error adding pin %s.7i64.%1d.%1d.digin.%02d."
                                "in-not, aborting\n",
                                hm2->llio->name, inst->module_index, c, p);
                        goto fail1;
                    }
                    r = hal_pin_bit_newf(HAL_IN, &(hal->pin.digital_out[p]),
                                         hm2->llio->comp_id,
                                         "%s.7i64.%1d.%1d.digout.%02d",
                                         hm2->llio->name, inst->module_index, 
                                         c, p);
                    if (r < 0) {
                        HM2_ERR("error adding pin %s.7i64.%1d.%1d.digout.%02d, "
                                "aborting\n",
                                hm2->llio->name, inst->module_index, c, p);
                        goto fail1;
                    }

                    r = hal_param_bit_newf(HAL_RW, &(hal->param.invert[p]),
                                         hm2->llio->comp_id,
                                         "%s.7i64.%1d.%1d.digout.%02d.invert",
                                         hm2->llio->name, inst->module_index, 
                                         c, p);
                    if (r < 0) {
                        HM2_ERR("error adding param %s.7i64.%1d.%1d.digout.%02d"
                                ".invert, aborting\n",
                                hm2->llio->name, inst->module_index, c, p);
                        goto fail1;
                    }
                }
                r = hal_pin_float_newf(HAL_OUT, &(hal->pin.analogue_in[0]),
                                     hm2->llio->comp_id,
                                     "%s.7i64.%1d.%1d.adcin.00.in",
                                     hm2->llio->name, inst->module_index, c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.7i64.%1d.%1d.adcin.00.in, "
                            "aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }
                r = hal_pin_float_newf(HAL_OUT, &(hal->pin.analogue_in[1]),
                                       hm2->llio->comp_id,
                                       "%s.7i64.%1d.%1d.adcin.01.in",
                                       hm2->llio->name, inst->module_index,c );
                if (r < 0) {
                    HM2_ERR("error adding pin %s.7i64.%1d.%1d.adcin.01.in, "
                            "aborting\n",
                            hm2->llio->name, inst->module_index, c);
                    goto fail1;
                }



                // Register the TRAM values and store the register addresses for
                // other functions to use


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
    return 0;

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

void hm2_7i64_prepare_tram_write(hostmot2_t *hm2){
    int i, c, p;
    u32 r = 0;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_7i64 ; c++){
            hal_7i64_t *hal = &inst->hal_7i64[c];
            hm2_sserial_tram_t *tram = &inst->tram_7i64[c];
            r = 0;
            for (p = 0 ; p < 24 ; p++){
                r |= ((*hal->pin.digital_out[p] ^ hal->param.invert[p]) << p);
            }
            *tram->reg_0_write = r;
        }
    }
}

void hm2_7i64_process_tram_read(hostmot2_t  * hm2){
    int i, c, p;
    u32 r = 0;
    const double u16toV = 0.000050354772259;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_7i64 ; c++){
            hal_7i64_t *hal = &inst->hal_7i64[c];
            hm2_sserial_tram_t *tram = &inst->tram_7i64[c];
            r = *tram->reg_0_read;
            for (p = 0 ; p < 24 ; p++){
                if (r & (1 << p)){
                    *hal->pin.digital_in[p] = 1;
                    *hal->pin.digital_in_not[p] = 0;}
                else{
                    *hal->pin.digital_in[p] = 0;
                    *hal->pin.digital_in_not[p] = 1;}
            }
            r = *tram->reg_1_read;
            *hal->pin.analogue_in[0] = (r & 0xFFFF) * u16toV;
            *hal->pin.analogue_in[1] = ((r & 0xFFFF0000) >> 16) * u16toV;
        }
    }
}
