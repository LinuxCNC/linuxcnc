
//
//    Copyright (C) 2011 Andy Pugh
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

// local functions
int hm2_sserial_auto_read_configs(hostmot2_t *hm2, hm2_sserial_tram_t *tram, hal_sserial_auto_t *chan);
int hm2_sserial_auto_make_pins(hostmot2_t *hm2, hm2_sserial_tram_t *tram, hal_sserial_auto_t *chan);
int setbits(hm2_sserial_tram_t *tram, u64 *val, int start, int len);
int getbits(hm2_sserial_tram_t *tram, u64 *val, int start, int len);
//

int hm2_auto_create(hostmot2_t *hm2, hm2_module_descriptor_t *md) {
    int i,c;
    int n;
    int buff;
    char name[5] = {'\0'};
    
    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {
        
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        
        if (inst->num_auto == 0) continue;
        
        inst->hal_auto =
        (hal_sserial_auto_t *)hal_malloc(inst->num_auto * sizeof(hal_sserial_auto_t));
        if (inst->hal_auto == NULL) {
            HM2_ERR("out of memory!\n");
            return -ENOMEM;
        }
        
        inst->tram_auto =
        (hm2_sserial_tram_t *)kmalloc(inst->num_auto * sizeof(hm2_sserial_tram_t),
                                      GFP_KERNEL);
        if (inst->tram_auto == NULL) {
            HM2_ERR("out of memory!\n");
            return -ENOMEM;
        }
        n = 0;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            if (inst->tag_auto & (1 << c)) {
                hm2_sserial_tram_t *tram = &inst->tram_auto[n];
                hal_sserial_auto_t *chan = &inst->hal_auto[n];
                n++;
                chan->num_confs = 0;
                chan->num_modes = 0;
                tram->reg_command_addr = inst->command_reg_addr;
                tram->reg_data_addr= inst->data_reg_addr;
                tram->index = c;
                tram->reg_cs_addr = md->base_address + 2 * md->register_stride
                + i * md->instance_stride + c * sizeof(u32);
                tram->reg_0_addr = md->base_address + 3 * md->register_stride
                + i * md->instance_stride + c * sizeof(u32);
                tram->reg_1_addr = md->base_address + 4 * md->register_stride
                + i * md->instance_stride + c * sizeof(u32);
                tram->reg_2_addr = md->base_address + 5 * md->register_stride
                + i * md->instance_stride + c * sizeof(u32);
                
                // Get the board ID and name before it is over-written by DoIts
                hm2->llio->read(hm2->llio, tram->reg_0_addr, 
                                &buff, sizeof(u32));
                chan->hm2_serialnumber = buff;
                hm2->llio->read(hm2->llio, tram->reg_1_addr, name, sizeof(u32));
                rtapi_snprintf(tram->name, 9, "%2s.%d.%d",name, i, c);
                tram->name[1] |= 0x20; ///lower case
                
                if (hm2_sserial_auto_read_configs(hm2, tram, chan) < 0) {
                    return -EINVAL;
                }
                
                if ( hm2_sserial_auto_make_pins(hm2, tram, chan) < 0) {
                    return -EINVAL;
                }
            }
        }
    }
    return 0;           
}


void hm2_auto_prepare_tram_write(hostmot2_t *hm2){
    int i, c, n, p, b; 
    int r;
    int bitcount;
    u64 buff;
    float val;
    
    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        if (*inst->state != 0x01) continue ; // Only work on running instances
        n = 0;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            if (inst->tag_auto & (1 << c)) {
                hm2_sserial_tram_t *tram = &inst->tram_auto[n];
                hal_sserial_auto_t *chan = &inst->hal_auto[n];
                n++;
                bitcount = 0;
                if (tram->reg_0_write) *tram->reg_0_write = 0;
                if (tram->reg_1_write) *tram->reg_1_write = 0;
                if (tram->reg_2_write) *tram->reg_2_write = 0;
                for (p=0 ; p < chan->num_confs ; p++){
                    hm2_sserial_data_t *conf = &chan->conf[p];
                    hm2_sserial_pins_t *pin = &chan->pins[p];
                    if (conf->DataDir & 0xC0){
                        switch (conf->DataType){
                            case 0x01: // bits
                                buff = 0;
                                for (b = 0 ; b < conf->DataLength ; b++){
                                    buff |= ((u64)(*pin->bit_pins[b] != 0) << b)
                                          ^ ((u64)(pin->invert[b] != 0) << b);
                                }
                                break;
                            case 0x02: // unsigned
                                val = *pin->float_pin;
                                if (val > pin->maxlim) val = pin->maxlim;
                                if (val < pin->minlim) val = pin->minlim;
                                buff = (u64)((val / pin->fullscale) 
                                             * ((1LL << conf->DataLength) - 1));
                                break;
                            case 0x03: // signed
                                //this only works if DataLength <= 32
                                val = *pin->float_pin;
                                if (val > pin->maxlim) val = pin->maxlim;
                                if (val < pin->minlim) val = pin->minlim;
                                buff = (((s32)(val / pin->fullscale * 2147483647))
                                              >> (32 - conf->DataLength))
                                              & ((1 << conf->DataLength) - 1);
                                break;
                            case 0x06: // byte stream
                                buff = *pin->u32_pin & ((1 << conf->DataLength) - 1);
                                break;
                            case 0x07: // Boolean
                                buff = 0;
                                if (*pin->boolean ^ *pin->invert){
                                    buff = ((1 << conf->DataLength) - 1);
                                }
                                break;
                            case 0x08: //Counter
                                // Would we ever write to a counter? 
                                // Assume not for the time being
                                break;
                            default:
                                HM2_ERR("Unsupported datatype %i\n",
                                        conf->DataType);
                                
                        }
                        r = setbits(tram, &buff, bitcount, conf->DataLength);
                        bitcount += conf->DataLength;
                    }
                }
            }
        }
    }
}

void hm2_auto_process_tram_read(hostmot2_t *hm2){
    int i, c, n, p, b; 
    int r;
    int bitcount;
    u64 buff;
    s32 buff32;
    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        if (*inst->state != 0x01) continue ; // Only work on running instances
        n = 0;
        for (c = 0 ; c < inst->num_channels ; c++ ) {
            if (inst->tag_auto & (1 << c)) {
                hm2_sserial_tram_t *tram = &inst->tram_auto[n];
                hal_sserial_auto_t *chan = &inst->hal_auto[n];
                n++;
                bitcount = 0;
                buff = 0;
                chan->status = *tram->reg_cs_read;
                for (p=0 ; p < chan->num_confs ; p++){
                    hm2_sserial_data_t *conf = &chan->conf[p];
                    hm2_sserial_pins_t *pin = &chan->pins[p];
                    if (! (conf->DataDir & 0x80)){
                        r = getbits(tram, &buff, bitcount, conf->DataLength);
                        switch (conf->DataType){
                            case 0x01: // bits
                                for (b = 0 ; b < conf->DataLength ; b++){
                                    *pin->bit_pins[b] = ((buff & (1LL << b)) != 0);
                                    *pin->bit_pins_not[b] = ! *pin->bit_pins[b];
                                }
                                break;
                            case 0x02: // unsigned
                                *pin->float_pin = (buff * conf->ParmMax)
                                               / ((1 << conf->DataLength) - 1);
                                break;
                            case 0x03: // signed. assumes nothing > 32 bits. 
                                buff32 = (buff & 0xFFFFFFFFL) << (32 - conf->DataLength);
                                *pin->float_pin = (buff32 / 2147483647.0 ) 
                                * conf->ParmMax;
                                break;
                            case 0x06: // stream
                                *pin->u32_pin = buff & ((1 << conf->DataLength) - 1);
                                break;
                            case 0x07:
                                *pin->boolean = (buff != 0);
                                *pin->boolean_not = (buff == 0);
                                break;
                            case 0x08: //Counter
                                // sign-extend buff into buff32 
                                buff32 = 1L << (conf->DataLength - 1);
                                buff32 = (buff ^ buff32) - buff32;
                                if ((buff32 - pin->oldval) > (1 << (conf->DataLength - 2))){
                                    *pin->s32_pin -= (1 << conf->DataLength);
                                } else if ((pin->oldval - buff32) > (1 << (conf->DataLength - 2))){
                                    *pin->s32_pin += (1 << conf->DataLength);
                                }
                                *pin->s32_pin += (buff32 - pin->oldval);
                                pin->oldval = buff32;
                                break;
                            default:
                                HM2_ERR("Unsupported datatype %i\n",
                                        conf->DataType);
                        }
                        bitcount += conf->DataLength;
                    }
                }
            }
        }
    }
}

int hm2_sserial_auto_read_configs(hostmot2_t *hm2, hm2_sserial_tram_t *tram, hal_sserial_auto_t *chan){
    
    int ptoc, addr, buff;
    unsigned char rectype;
    
    hm2->llio->read(hm2->llio, tram->reg_2_addr, &buff, sizeof(u32)); 
    ptoc=(buff & 0xffff); 
    
    //Iterate through the config blocks 
    while ((addr = 0xFFFF & hm2_sserial_get_param(hm2, tram, ptoc))){
        ptoc += 2;
        
        if (chan->num_confs > 100){// This is probably a bug
            HM2_ERR("Unfeasible number of pins found in sserial "
                    "device. Aborting.\n");
            return -EINVAL;
        }
        
        hm2_sserial_get_bytes(hm2, tram, &rectype, addr, 1);
        
        if (rectype == 0xA0) {
            chan->num_confs++;
            chan->conf = (hm2_sserial_data_t *)krealloc(chan->conf, 
                          chan->num_confs * sizeof(hm2_sserial_data_t),
                          GFP_KERNEL);
            addr = hm2_sserial_get_bytes(hm2, tram, &chan->conf[chan->num_confs-1], addr, 14);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, tram, 
                                         &(chan->conf[chan->num_confs-1].UnitString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, tram, 
                                         &(chan->conf[chan->num_confs-1].NameString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
 
            if (chan->conf[chan->num_confs-1].ParmMin 
                == chan->conf[chan->num_confs-1].ParmMax){
                chan->conf[chan->num_confs-1].ParmMin = 0;
                chan->conf[chan->num_confs-1].ParmMax = 1;
            }
            

            
            hm2_sserial_auto_print_conf(hm2, &chan->conf[chan->num_confs-1]);
            
        } else if (rectype == 0xB0 ) {
            chan->num_modes++;
            chan->modes = (hm2_sserial_mode_t *)krealloc(chan->modes, 
                           chan->num_modes * sizeof(hm2_sserial_mode_t),
                           GFP_KERNEL);
            addr = hm2_sserial_get_bytes(hm2, tram, &chan->modes[chan->num_modes-1], addr, 4);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, tram, 
                                         &(chan->modes[chan->num_modes-1].NameString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
            hm2_sserial_auto_print_modes(hm2, &chan->modes[chan->num_modes-1]);
        }        
    }
    return chan->num_confs;
}

int hm2_sserial_auto_make_pins(hostmot2_t *hm2, hm2_sserial_tram_t *tram, 
                               hal_sserial_auto_t *chan){
    int i, j, r;
    char name[HAL_NAME_LEN + 1];
    int read_tally, write_tally;
    int data_dir;
    chan->pins = (hm2_sserial_pins_t*)hal_malloc(chan->num_confs 
                                                 * sizeof(hm2_sserial_pins_t));
    rtapi_snprintf(name, sizeof(name), "%s.%s.serial-number",
                   hm2->llio->name,
                   tram->name);
    r = hal_param_u32_new(name, HAL_RO, &(chan->hm2_serialnumber),
                           hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding parameter %s. aborting\n", name);
        goto fail1;
    }
    rtapi_snprintf(name, sizeof(name), "%s.%s.status",
                   hm2->llio->name,
                   tram->name);
    r = hal_param_u32_new(name, HAL_RO, &(chan->status),
                          hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding parameter %s. aborting\n", name);
        goto fail1;
    }
    
    read_tally = write_tally = 0;
    for (i = 0 ; i < chan->num_confs ; i++ ){

        if ( ! (chan->conf[i].DataDir & 0xC0)){
            data_dir = HAL_OUT;
            read_tally += chan->conf[i].DataLength; 
        }
        else if (chan->conf[i].DataDir & 0x40){
            data_dir = HAL_IO;
            read_tally += chan->conf[i].DataLength; 
            write_tally += chan->conf[i].DataLength; 
        }
        else if (chan->conf[i].DataDir & 0x80){
            data_dir = HAL_IN;
            write_tally += chan->conf[i].DataLength;
        }
        else {
            HM2_ERR("Invalid Pin direction specifier. Aborting");
            return -EINVAL;
        }
                 
        switch (chan->conf[i].DataType){
            case 0x00: // Pad, do nothing
                break;
            case 0x01: // bits
                chan->pins[i].bit_pins = (hal_bit_t**)
                    hal_malloc(chan->conf[i].DataLength * sizeof(hal_bit_t*));
                chan->pins[i].bit_pins_not = (hal_bit_t**)
                    hal_malloc(chan->conf[i].DataLength * sizeof(hal_bit_t*));
                chan->pins[i].invert = (hal_bit_t*)
                hal_malloc(chan->conf[i].DataLength * sizeof(hal_bit_t));
                for (j = 0; j < chan->conf[i].DataLength ; j++){

                    rtapi_snprintf(name, sizeof(name), "%s.%s.%s-%02d",
                                   hm2->llio->name,
                                   tram->name, 
                                   chan->conf[i].NameString,
                                   j);
                    r = hal_pin_bit_new(name,
                                        data_dir,
                                        &(chan->pins[i].bit_pins[j]),
                                        hm2->llio->comp_id);
                    if (r < 0) {
                        HM2_ERR("error adding pin '%s', aborting\n", name);
                        return r;
                    }
                    if (data_dir != HAL_IN) {
                        rtapi_snprintf(name, sizeof(name), "%s.%s.%s-%02d-not",
                                       hm2->llio->name,
                                       tram->name, 
                                       chan->conf[i].NameString,
                                       j);
                        r = hal_pin_bit_new(name,
                                            data_dir,
                                            &(chan->pins[i].bit_pins_not[j]),
                                            hm2->llio->comp_id);
                        if (r < 0) {
                            HM2_ERR("error adding pin '%s', aborting\n", name);
                            return r;
                        }
                    }
                    if (data_dir != HAL_OUT){
                        rtapi_snprintf(name, sizeof(name), "%s.%s.%s-%02d-invert",
                                       hm2->llio->name,
                                       tram->name, 
                                       chan->conf[i].NameString,
                                       j);
                        r = hal_param_bit_new(name,
                                              HAL_RW,
                                              &(chan->pins[i].invert[j]),
                                              hm2->llio->comp_id);
                        if (r < 0) {
                            HM2_ERR("error adding pin '%s', aborting\n", name);
                            return r;
                        }
                    }
                }
                break;
            case 0x02: // unsigned
            case 0x03: // signed
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_pin_float_new(name,
                                    data_dir,
                                    &(chan->pins[i].float_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                if (data_dir == HAL_OUT) {break;}
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s-maxlim",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_param_float_new(name,
                                      HAL_RW,
                                      &(chan->pins[i].maxlim),
                                      hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].maxlim = chan->conf[i].ParmMax;
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s-minlim",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_param_float_new(name,
                                      HAL_RW,
                                      &(chan->pins[i].minlim),
                                      hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].minlim = chan->conf[i].ParmMin;
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s-scalemax",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_param_float_new(name,
                                      HAL_RW,
                                      &(chan->pins[i].fullscale),
                                      hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].fullscale = chan->conf[i].ParmMax;
                break;
            case 0x04: 
            case 0x05:
                HM2_ERR("Non-Volatile data type found in PTOC. This should "
                        "never happen. Aborting");
                return r;
            case 0x06: // stream
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_pin_u32_new(name,
                                    data_dir,
                                    &(chan->pins[i].u32_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                break;
            case 0x07: // boolean
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_pin_bit_new(name,
                                    data_dir,
                                    &(chan->pins[i].boolean),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                if (data_dir != HAL_IN) {
                    rtapi_snprintf(name, sizeof(name), "%s.%s.%s-not",
                                   hm2->llio->name,
                                   tram->name, 
                                   chan->conf[i].NameString);
                    r = hal_pin_bit_new(name,
                                        data_dir,
                                        &(chan->pins[i].boolean_not),
                                        hm2->llio->comp_id);
                    if (r < 0) {
                        HM2_ERR("error adding pin '%s', aborting\n", name);
                        return r;
                    }  
                }
                if (data_dir != HAL_OUT) {
                    chan->pins[i].invert = hal_malloc(sizeof(hal_bit_t));
                    rtapi_snprintf(name, sizeof(name), "%s.%s.%s-invert",
                                   hm2->llio->name,
                                   tram->name, 
                                   chan->conf[i].NameString);
                    r = hal_param_bit_new(name,
                                        HAL_RW,
                                        chan->pins[i].invert,
                                        hm2->llio->comp_id);
                    if (r < 0) {
                        HM2_ERR("error adding pin '%s', aborting\n", name);
                        return r;
                    }  
                }
                break;
            case 0x08: // Encoder Counts
                rtapi_snprintf(name, sizeof(name), "%s.%s.%s",
                               hm2->llio->name,
                               tram->name, 
                               chan->conf[i].NameString);
                r = hal_pin_s32_new(name,
                                    data_dir,
                                    &(chan->pins[i].s32_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }                
                break;
            default:
                HM2_ERR("Unhandled sserial data type (%i) Name %s Units %s\n",
                        chan->conf[i].DataType, 
                        chan->conf[i].NameString,
                        chan->conf[i].UnitString);
        }
    }
    // Register the TRAM READ
    
    HM2_DBG("%s read-bits = %i, write-bits = %i\n", tram->name, read_tally, write_tally);
    
    r = hm2_register_tram_read_region(hm2, tram->reg_cs_addr, sizeof(u32), 
                                      &tram->reg_cs_read);
    if (r < 0) { HM2_ERR("error registering tram read region for sserial CS"
                         "register (%d)\n", r);
        goto fail1;
    }
    if (read_tally > 0){
        r = hm2_register_tram_read_region(hm2, tram->reg_0_addr, sizeof(u32),
                                          &tram->reg_0_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 0 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_0_read = NULL;
    }
    
    if (read_tally > 32){
        r = hm2_register_tram_read_region(hm2, tram->reg_1_addr, sizeof(u32),
                                          &tram->reg_1_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 1 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_1_read = NULL;
    }
    
    if (read_tally > 64){
        r = hm2_register_tram_read_region(hm2, tram->reg_2_addr, sizeof(u32),
                                          &tram->reg_2_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 2 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_2_read = NULL;
    }
    
    // Register the TRAM WRITE
    if (write_tally > 0){
        r = hm2_register_tram_write_region(hm2, tram->reg_0_addr, sizeof(u32),
                                           &(tram->reg_0_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 0 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_0_write = NULL;
    }
    
    if (write_tally > 32){
        r = hm2_register_tram_write_region(hm2, tram->reg_1_addr, sizeof(u32),
                                           &(tram->reg_1_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 1 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_1_write = NULL;
    }
    
    if (write_tally > 64){
        r = hm2_register_tram_write_region(hm2, tram->reg_2_addr, sizeof(u32),
                                           &(tram->reg_2_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 2 register (%d)\n", r);
            goto fail1;
        }
    } else {
        tram->reg_2_write = NULL;
    }
    
    return 0;
    
fail1:
    return -EINVAL;
}

int getbits(hm2_sserial_tram_t *tram, u64 *val, int start, int len){
    long long user0 = (tram->reg_0_read == NULL)? 0 : *tram->reg_0_read;
    long long user1 = (tram->reg_1_read == NULL)? 0 : *tram->reg_1_read;
    long long user2 = (tram->reg_2_read == NULL)? 0 : *tram->reg_2_read;
    long long mask = (1LL << len) - 1;
    
    if (start + len <= 32){
        *val = (user0 >> start) & mask;
    } else if (start + len <= 64){
        if (start >= 32){
            *val = (user1 >> (start - 32)) & mask;
        } else { 
            *val = (((user1 << 32) | user0) >> start ) & mask;
        }
    } else {
        if (start >= 64){
            *val = (user2 >> (start - 64)) & mask;
        } else if (start >= 32) {
            *val = (((user2 << 32) | user1) >> (start - 32)) & mask;
        } else {
            *val = ((user2 << (64 - start)) | (user1 << (32 - start)) 
            | (user0 >> start)) & mask;
        }
    }
    return 0;
}
                                              
            
int setbits(hm2_sserial_tram_t *tram, u64 *val, int start, int len){
    // Assumes that all registers are zeroed elsewhere as required
    long long mask0, mask1, mask2;
 
    if (start + len <= 32){
        mask0 = (1LL << len) -1;
        *tram->reg_0_write |= (*val  & mask0) << start;
    } else if (start + len <= 64){
        if (start >= 32){
            mask1 = (1LL << len) -1;
            *tram->reg_1_write |= (*val & mask1) << (start - 32);
        } else { 
            mask0 = (1LL << (32 - start)) - 1;
            mask1 = ((1LL << (start + len - 32)) - 1) << (32 - start);
            *tram->reg_0_write |= (*val & mask0) << start;
            *tram->reg_1_write |= (*val & mask1) >> (32 - start);
        }
    } else {
        if (start >= 64){
            mask2 = (1LL << len) - 1;
            *tram->reg_2_write |= (*val  & mask2) << (start - 64);
        } else if (start >= 32) {
            mask1 = (1LL << (64 - start)) - 1;
            mask2 = ((1LL << (start + len - 64)) - 1) << (64 - start);
            *tram->reg_1_write |= (*val & mask1) << (start - 32);
            *tram->reg_2_write |= (*val & mask2) >> (64 - start);
        } else {
            mask0 = (1LL << (32 - start)) -1;
            mask1 = ((1LL << 32) - 1) << (32 - start);
            mask2 = ((1LL << (start + len - 64)) -1 ) << (64 - start);
            *tram->reg_0_write |= (*val & mask0) << start;
            *tram->reg_1_write = (*val & mask1) >> (32 - start);
            *tram->reg_2_write |= (*val & mask2) >> (64 - start);
        }
    }
    return 0;
}

void hm2_sserial_auto_setmode(hostmot2_t *hm2, hm2_sserial_instance_t *inst){
    u32 buff=0x00;
    u32 addr;
    int c;
    int n = 0;
    int i = inst->module_index;
    HM2_DBG("Num Auto = %i\n", inst->num_auto);
    for (c = 0 ; c < inst->num_auto ; c++){
        n = inst->tram_auto[c].index;
        if (hm2->config.sserial_modes[i][n] != 'x') {
            // CS addr - write card mode
            addr = inst->tram_auto[c].reg_cs_addr;
            buff = (hm2->config.sserial_modes[i][n] - '0') << 24;
            hm2->llio->write(hm2->llio, addr, &buff, sizeof(u32));
            HM2_DBG("Normal Start: Writing %08x to %04x\n", buff, addr);
        }
    }
}

int hm2_sserial_auto_check(hostmot2_t *hm2, hm2_sserial_instance_t *inst){
    int c;
    int err_flag = 0;
    for (c = 0 ; c < inst->num_auto ; c++){
        hm2_sserial_tram_t *tram=&inst->tram_auto[c];
        if (hm2_sserial_check_errors(hm2, tram) < 0) {err_flag = -EINVAL;}
    }
    return err_flag;
}

    
void hm2_sserial_auto_print_conf(hostmot2_t *hm2, hm2_sserial_data_t *conf){
    HM2_DBG("RecordType = %x\n", conf->RecordType);
    HM2_DBG("DataLength = %i\n", conf->DataLength);
    HM2_DBG("DataType = %i\n", conf->DataType);
    HM2_DBG("DataDir = %i\n", conf->DataDir);
    HM2_DBG("ParmMax %0i.%02i\n", (int)conf->ParmMax, 
            (int)((conf->ParmMax - (int)conf->ParmMax) * 100.0));
    HM2_DBG("ParmMin %0i.%02i\n",(int)conf->ParmMin,
            (int)((conf->ParmMin - (int)conf->ParmMin) * 100.0));
    HM2_DBG("SizeOf ParmMin %i\n", sizeof(conf->ParmMax));
    HM2_DBG("ParmAddr = %x\n", conf->ParmAddr); 
    HM2_DBG("UnitString = %s.\n", conf->UnitString);
    HM2_DBG("NameString = %s.\n\n", conf->NameString);
}
 
void hm2_sserial_auto_print_modes(hostmot2_t *hm2, hm2_sserial_mode_t *modes){
    HM2_DBG("RecordType = %x\n", modes->RecordType);
    HM2_DBG("ModeIndex = %i\n", modes->ModeIndex);
    HM2_DBG("ModeType = %i\n", modes->ModeType);
    HM2_DBG("Unused = %i\n", modes->Unused);
    HM2_DBG("NameString = %s.\n\n", modes->NameString);
}
    
void hm2_sserial_auto_cleanup(hostmot2_t *hm2){
    
    int i,j;
    for (i = 1 ; i < hm2->sserial.num_instances; i++){
        if (hm2->sserial.instance[i].tram_auto){
            for (j = 0 ; i < hm2->sserial.instance[i].num_auto; j++){
                if (hm2->sserial.instance[i].hal_auto[j].conf){
                    kfree(hm2->sserial.instance[i].hal_auto[j].conf);
                }
                if (hm2->sserial.instance[i].hal_auto[j].modes){
                    kfree(hm2->sserial.instance[i].hal_auto[j].modes);
                }
            }
            kfree(hm2->sserial.instance[i].tram_auto);
        }
    }
}
    
    
