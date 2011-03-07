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

int hm2_other_create(hostmot2_t *hm2, hm2_module_descriptor_t *md) {
    int i,c,r;
    int n = -1;
    u32 buff, addr;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {

        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        inst->hal_other =
        (hal_other_t *)hal_malloc(inst->num_other * sizeof(hal_other_t));
        if (inst->hal_other == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail0;
        }


        inst->tram_other =
        (hm2_sserial_tram_t *)kmalloc(inst->num_other * sizeof(hm2_sserial_tram_t),
                                      GFP_KERNEL);
        if (inst->tram_other == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail1;
        }

        n = -1;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            hal_other_t *hal;
            hm2_sserial_tram_t *tram;

            if (inst->tag_other & (1 << c)) {

                n++;

                hal = &hm2->sserial.instance[i].hal_other[n];
                tram = &hm2->sserial.instance[i].tram_other[n];

                tram->tag = (1 << c);
                tram->reg_command_addr = inst->command_reg_addr;
                tram->reg_data_addr= inst->data_reg_addr;

                // Get the card ID
                addr = md->base_address + 2 * md->register_stride
                + inst->module_index * md->instance_stride
                + c * sizeof(u32);
                hm2->llio->read(hm2->llio, addr, &buff, sizeof(u32));
                hal->card_type = buff >> 24;

                r = hal_pin_u32_newf(HAL_IN, &(hal->pin.hm2_reg_0_write),
                                       hm2->llio->comp_id,"%s.BoardID-%X.%1d.%1d.reg-0-write",
                                       hm2->llio->name,
                                       hal->card_type,
                                       inst->module_index,
                                       c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.BoardID-%X.%1d.%1d.reg-0-write. aborting\n",
                            hm2->llio->name,
                            hal->card_type,
                            inst->module_index,
                            c);
                    goto fail1;
                }

                r = hal_pin_u32_newf(HAL_IN, &(hal->pin.hm2_reg_1_write),
                                     hm2->llio->comp_id,"%s.BoardID-%X.%1d.%1d.reg-1-write",
                                     hm2->llio->name,
                                     hal->card_type,
                                     inst->module_index,
                                     c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.BoardID-%X.%1d.%1d.reg-1-write. aborting\n",
                            hm2->llio->name,
                            hal->card_type,
                            inst->module_index,
                            c);
                    goto fail1;
                }
                r = hal_pin_u32_newf(HAL_OUT, &(hal->pin.hm2_reg_0_read),
                                     hm2->llio->comp_id,"%s.BoardID-%X.%1d.%1d.reg-0-read",
                                     hm2->llio->name,
                                     hal->card_type,
                                     inst->module_index,
                                     c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.BoardID-%X.%1d.%1d.reg-0-read. aborting\n",
                            hm2->llio->name,
                            hal->card_type,
                            inst->module_index,
                            c);
                    goto fail1;
                }

                r = hal_pin_u32_newf(HAL_OUT, &(hal->pin.hm2_reg_1_read),
                                     hm2->llio->comp_id,"%s.BoardID-%X.%1d.%1d.reg-1-read",
                                     hm2->llio->name,
                                     hal->card_type,
                                     inst->module_index,
                                     c);
                if (r < 0) {
                    HM2_ERR("error adding pin %s.BoardID-%X.%1d.%1d.reg-1-read. aborting\n",
                            hm2->llio->name,
                            hal->card_type,
                            inst->module_index,
                            c);
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
                r = hm2_register_tram_write_region(hm2,
                                                   tram->reg_1_addr,
                                                   sizeof(u32),
                                                   &(tram->reg_1_write));
                if (r < 0) {
                    HM2_ERR("error registering tram write region for sserial"
                            "interface 1 register (%d)\n", r);
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
            if (hm2->sserial.instance[i].tram_other != NULL){
                kfree(hm2->sserial.instance[i].tram_other);
            }
        }
    }
fail0:
    return r;

}

void hm2_other_prepare_tram_write(hostmot2_t *hm2){
    int i;
    int c;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_other ; c++){
            hal_other_t *hal = &inst->hal_other[c];
            hm2_sserial_tram_t *tram = &inst->tram_other[c];
            *tram->reg_0_write = *hal->pin.hm2_reg_0_write;
            *tram->reg_1_write = *hal->pin.hm2_reg_1_write;
        }
    }
}


void hm2_other_process_tram_read(hostmot2_t *hm2){
    int i;
    int c;

    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];

        if (*inst->state != 0x01) continue ; // Only work on running instances

        for (c = 0 ; c < hm2->sserial.instance[i].num_other ; c++){
            hal_other_t *hal = &inst->hal_other[c];
            hm2_sserial_tram_t *tram = &inst->tram_other[c];
            *hal->pin.hm2_reg_0_read = *tram->reg_0_read;
            *hal->pin.hm2_reg_1_read = *tram->reg_1_read;
        }
    }
}
