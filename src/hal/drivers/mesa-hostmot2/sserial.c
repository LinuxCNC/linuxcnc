//
//   Copyright (C) 2010 Andy Pugh
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//


#include "config_module.h"
#include RTAPI_INC_SLAB_H
#include RTAPI_INC_STRING_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "rtapi_math64.h"

#include "hal.h"

#include "hostmot2.h"
#include "bitfile.h"

//utility function delarations
int hm2_sserial_stopstart(hostmot2_t *hm2, hm2_module_descriptor_t *md, 
                          hm2_sserial_instance_t *inst, u32 start_mode);
int getbits(hm2_sserial_remote_t *chan, u64 *val, int start, int len);
int setbits(hm2_sserial_remote_t *chan, u64 *val, int start, int len);
int hm2_sserial_get_bytes(hostmot2_t *hm2, hm2_sserial_remote_t *chan, void *buffer, int addr, int size);
int hm2_sserial_read_globals(hostmot2_t *hm2,hm2_sserial_remote_t *chan);
int hm2_sserial_create_params(hostmot2_t *hm2, hm2_sserial_remote_t *chan);
int getlocal32(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr);
int getlocal8(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr);
int check_set_baudrate(hostmot2_t *hm2, hm2_sserial_instance_t *inst);
int setlocal32(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr, int val);

// Main functions

int hm2_sserial_parse_md(hostmot2_t *hm2, int md_index){
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int i, c;
    int pin = -1;
    int port_pin, port;
    u32 ddr_reg, src_reg, buff;
    int r = -EINVAL;
    int count = 0;
    int chan_counts[] = {0,0,0,0,0,0,0,0};
    
    hm2->sserial.version = md->version;

    //
    // some standard sanity checks
    //

    if (hm2_md_is_consistent(hm2, md_index, 0, 5, 0x40, 0x001F)) {
        HM2_ERR("The bitfile contains Smart Serial modules for a firmware "
                "revision < rev22. This Driver now requires rev22 or newer "
                "firmwares\n");
        return -EINVAL;
    }
    
    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 6, 0x40, 0x003C)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->sserial.num_instances != 0) {
        HM2_ERR(
                "found duplicate Module Descriptor for %s (inconsistent firmwar"
                "e), not loading driver\n",
                hm2_get_general_function_name(md->gtag)
                );
        return -EINVAL;
    }
    if (hm2->config.num_sserials > md->instances) {
        HM2_ERR(
                "num_sserials references %d instances, but only %d are available"
                ", not loading driver\n",
                hm2->config.num_sserials,
                md->instances
                );
        return -EINVAL;
    }

    if (hm2->config.num_sserials == 0) {
        return 0;
    }

    //
    // looks good, start initializing
    //

    if (hm2->config.num_sserials == -1) {
        hm2->sserial.num_instances = md->instances;
    } else {
        hm2->sserial.num_instances = hm2->config.num_sserials;
    }
    //num_instances may be revised down depending on detected hardware
    HM2_DBG("sserial_num_instances = %i\n", hm2->sserial.num_instances);

    // allocate the per-instance HAL shared memory
    hm2->sserial.instance = (hm2_sserial_instance_t *)
    hal_malloc(hm2->sserial.num_instances * sizeof(hm2_sserial_instance_t));
    if (hm2->sserial.instance == NULL) {
        HM2_ERR("hm2_sserial_parse_md: hm2_sserial_instance: out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }
    // We can't create the pins until we know what is on each channel, and
    // can't communicate until the pin directions are set up.

    // Temporarily enable the pins that are not masked by sserial_mode

    for (port  = 0; port < hm2->ioport.num_instances; port ++) {
        ddr_reg = 0;
        src_reg = 0;
        for (port_pin = 0 ; port_pin < hm2->idrom.port_width; port_pin ++){
            pin++;
            if (hm2->pin[pin].sec_tag == HM2_GTAG_SMARTSERIAL) {
                // look for highest-indexed pin to determine number of channels
                if ((hm2->pin[pin].sec_pin & 0x0F) > chan_counts[hm2->pin[pin].sec_unit]) {
                    chan_counts[hm2->pin[pin].sec_unit] = (hm2->pin[pin].sec_pin & 0x0F);
                }
                // check if the channel is enabled
                HM2_DBG("sec unit = %i, sec pin = %i\n", hm2->pin[pin].sec_unit, hm2->pin[pin].sec_pin & 0x0F);
                if (hm2->config.sserial_modes[hm2->pin[pin].sec_unit]
                                        [(hm2->pin[pin].sec_pin & 0x0F) - 1] != 'x') {
                    src_reg |= (1 << port_pin);
                    if (hm2->pin[pin].sec_pin & 0x80){ ddr_reg |= (1 << port_pin); }
                }
            }
        }

        hm2->llio->write(hm2->llio, hm2->ioport.ddr_addr + 4 * port,
                         &ddr_reg, sizeof(u32));
        hm2->llio->write(hm2->llio, hm2->ioport.alt_source_addr + 4 * port,
                         &src_reg, sizeof(u32));

    }

    // Now iterate through the sserial instances, seeing what is on the enabled pins.
    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {

        hm2_sserial_instance_t *inst = &hm2->sserial.instance[count];
        inst->index = i;
        inst->num_channels = chan_counts[i];
        inst->command_reg_addr = md->base_address + i * md->instance_stride;
        inst->data_reg_addr = md->base_address + i * md->instance_stride + md->register_stride;
        
        buff=0x4000; //Reset
        HM2WRITE(inst->command_reg_addr, buff);
        buff=0x0001; //Clear
        HM2WRITE(inst->command_reg_addr, buff);
        if (hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF,1007) < 0){
            r = -EINVAL;
            goto fail0;
        }
        do {
            buff=getlocal8(hm2, inst, SSLBPMINORREVISIONLOC);
        } while (buff == 0xAA);
        HM2_PRINT("Smart Serial Firmware Version %i\n",buff);
        hm2->sserial.version = buff;
        
        r = check_set_baudrate(hm2, inst) < 0;
        if (r < 0) {goto fail0;}
        
        //start up in setup mode
        r = hm2_sserial_stopstart(hm2, md, inst, 0xF00) < 0;
        if(r < 0) {goto fail0;}
        
        inst->num_remotes = 0;
        
        for (c = 0 ; c < inst->num_channels ; c++) {
            u32 addr0, addr1, addr2;
            u32 user0, user1, user2;
            
            addr0 = md->base_address + 3 * md->register_stride
                                    + i * md->instance_stride + c * sizeof(u32);
            HM2READ(addr0, user0);
            HM2_DBG("Inst %i Chan %i User0 = %x\n", i, c, user0);

            addr1 = md->base_address + 4 * md->register_stride
                                    + i * md->instance_stride + c * sizeof(u32);
            HM2READ(addr1, user1);
            HM2_DBG("Inst %i Chan %i User1 = %x\n", i, c, user1);
            
            addr2 = md->base_address + 5 * md->register_stride
            + i * md->instance_stride + c * sizeof(u32);
            HM2READ(addr2, user2);
            HM2_DBG("Inst %i Chan %i User2 = %x\n", i, c, user2);
            
            if (hm2->sserial.baudrate == 115200
                && hm2->config.sserial_modes[i][c] != 'x') { //setup mode
                rtapi_print("Setup mode\n");
                if ((user1 & 0xFF00) == 0x4900){ //XiXXboard
                    
                    rtapi_print("found a %4s\n", (char*)&user1);
                    
                    inst->num_remotes += 1;
                    inst->tag |= 1<<c;
                }
                else { // Look for 8i20s with the CRC check off
                    int lbpstride = getlocal8(hm2, inst, SSLBPCHANNELSTRIDELOC);
                    int crc_addr = getlocal8(hm2, inst, SSLBPCHANNELSTARTLOC) 
                    + (c * lbpstride) + 30;
                    
                    rtapi_print("Looking for 8i20s, crc_addr = %i\n", crc_addr);
                    
                    if (getlocal8(hm2, inst, SSLBPMINORREVISIONLOC) < 37 ){
                        HM2_PRINT("Unable to check for 8i20s with firmware < 37 "
                                  "If you are not trying to reflash an 8i20 then"
                                  " ignore this message.\n");
                    }
                    else if (setlocal32(hm2, inst, crc_addr, 0xFF) < 0) {
                        HM2_ERR("Unable to disable CRC to check for old 8i20s");
                    }
                    else if ( (r = hm2_sserial_stopstart(hm2, md, inst, 0xF00)) < 0) {
                        goto fail0;
                    }
                    else {
                        HM2READ(addr1, user1);
                        
                        rtapi_print("found a %4s\n", (char*)&user1);
                        
                        if ((user1 & 0xFF00) == 0x4900){ //XiXXboard
                            inst->num_remotes += 1;
                            inst->tag |= 1<<c;
                        }
                    }
                }
            }
            else if ((user2 & 0x0000ffff) // Parameter discovery
                     || user1 == HM2_SSERIAL_TYPE_7I64 //predate discovery
                     || user1 == HM2_SSERIAL_TYPE_8I20 ){
                inst->num_remotes += 1;
                inst->tag |= 1<<c;
            } 
            
            // nothing connected, or masked by config or wrong baudrate or.....
            // make the pins into GPIO. 
            else if (user1 == 0 
                     || (inst->tag & 1<<c) == 0
                     || hm2->config.sserial_modes[i][c] == 'x'){ 
                for (pin = 0 ; pin < hm2->num_pins ; pin++){
                    if (hm2->pin[pin].sec_tag == HM2_GTAG_SMARTSERIAL
                        && (hm2->pin[pin].sec_pin & 0x0F) - 1  == c
                        && hm2->pin[pin].sec_unit == i){
                        hm2->pin[pin].sec_tag = 0;
                    }
                }
            }
            else if (hm2->config.sserial_modes[i][c] != 'x'){
                HM2_ERR("Unsupported Device (%4s) found on sserial %d "
                        "channel %d\n", (char*)&user1, i, c);
            }
        }
        if (inst->num_remotes > 0){
            if ((r = hm2_sserial_setup_channel(hm2, inst, count)) < 0 ) {
                HM2_ERR("Smart Serial setup failure on instance %i\n", 
                        inst->device_id);
                goto fail0;}
            if ((r = hm2_sserial_setup_remotes(hm2, inst, md)) < 0 ) {
                HM2_ERR("Remote setup failure on instance %i\n", 
                        inst->device_id);
                goto fail0;}
            if ((r = hm2_sserial_stopstart(hm2, md, inst, 0x900)) < 0 ){
                HM2_ERR("Failed to restart device %i on instance\n", 
                        inst->device_id);
                goto fail0;}
            if ((r = hm2_sserial_check_errors(hm2, inst)) < 0) {
                //goto fail0; // Ignore it for the moment. 
            }
            //only increment the instance index if this one is populated
            count++ ;
        }
    }
    
    hm2->sserial.num_instances = count; // because of the extra increment

    // Stop the sserial ports.
    buff=0x800; //Stop All
    for (i = 0 ; i < hm2->sserial.num_instances ; i++) {
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        hm2->llio->write(hm2->llio, inst->command_reg_addr, &buff, sizeof(u32));
    }
    // Return the physical ports to default
    ddr_reg = 0;
    src_reg = 0;
    for (port  = 0; port < hm2->ioport.num_instances; port ++) {
        hm2->llio->write(hm2->llio, hm2->ioport.ddr_addr + 4 * port,
                         &ddr_reg, sizeof(u32));
        hm2->llio->write(hm2->llio, hm2->ioport.alt_source_addr + 4 * port,
                         &src_reg, sizeof(u32));
    }
    return hm2->sserial.num_instances;

fail0:
    hm2_sserial_cleanup(hm2);
    hm2->sserial.num_instances = 0;
    return r;
}

int hm2_sserial_setup_channel(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int index){
    int r;
    
    r = hal_pin_bit_newf(HAL_IN, &(inst->run),
                         hm2->llio->comp_id, 
                         "%s.sserial.port-%1d.run",
                         hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding pin %s.sserial.%1d.run. aborting\n",
                hm2->llio->name, index);
        return -EINVAL;
    }
    r = hal_pin_u32_newf(HAL_OUT, &(inst->state),
                         hm2->llio->comp_id, 
                         "%s.sserial.port-%1d.port_state",
                         hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding pin %s.sserial.%1d.port_state. aborting\n",
                hm2->llio->name, index);
        return -EINVAL;
    }
    r = hal_pin_u32_newf(HAL_OUT, &(inst->fault_count),
                         hm2->llio->comp_id, 
                         "%s.sserial.port-%1d.fault-count",
                         hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding pin %s.sserial.%1d.fault-count. aborting\n",
                hm2->llio->name, index);
        return -EINVAL;
    }
    r = hal_param_u32_newf(HAL_RW, &(inst->fault_inc),
                           hm2->llio->comp_id, 
                           "%s.sserial.port-%1d.fault-inc",
                           hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding parameter %s.sserial.port-%1d.fault-inc"
                " aborting\n",hm2->llio->name, index);
        return -EINVAL;
    }            
    
    r = hal_param_u32_newf(HAL_RW, &(inst->fault_dec),
                           hm2->llio->comp_id, 
                           "%s.sserial.port-%1d.fault-dec",
                           hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding parameter %s.sserial.port-%1d.fault-dec"
                " aborting\n",hm2->llio->name, index);
        return -EINVAL;
    }
    
    r = hal_param_u32_newf(HAL_RW, &(inst->fault_lim),
                           hm2->llio->comp_id, 
                           "%s.sserial.port-%1d.fault-lim",
                           hm2->llio->name, index);
    if (r < 0) {
        HM2_ERR("error adding parameter %s.sserial.port-%1d.fault-lim"
                " aborting\n",hm2->llio->name, index);
        return -EINVAL;
    }
    //parameter defaults;
    inst->fault_dec = 1;
    inst->fault_inc = 10;
    inst->fault_lim = 200;
    
    // setup read-back in all modes
    
    r = hm2_register_tram_read_region(hm2, inst->command_reg_addr,
                                      sizeof(u32),
                                      &inst->command_reg_read);
    if (r < 0) {
        HM2_ERR("error registering tram write region for sserial"
                "command register (%d)\n", index);
        return -EINVAL;
    }
    
    r = hm2_register_tram_read_region(hm2, inst->data_reg_addr,
                                      sizeof(u32),
                                      &inst->data_reg_read);
    if (r < 0) {
        HM2_ERR("error registering tram write region for sserial "
                "command register (%d)\n", index);
        return -EINVAL;
        
    }
    // Nothing happens without a "Do It" command
    r = hm2_register_tram_write_region(hm2, inst->command_reg_addr,
                                       sizeof(u32),
                                       &inst->command_reg_write);
    if (r < 0) {
        HM2_ERR("error registering tram write region for sserial "
                "command register (%d)\n", index);
        return -EINVAL;
        
    }
    return 0;
}                                    

int hm2_sserial_setup_remotes(hostmot2_t *hm2, 
                              hm2_sserial_instance_t *inst, 
                              hm2_module_descriptor_t *md) {
    int c, r;
    int buff;
    
    inst->remotes =
    (hm2_sserial_remote_t *)kzalloc(inst->num_remotes*sizeof(hm2_sserial_remote_t), 
                                    GFP_KERNEL);
    if (inst->remotes == NULL) {
        HM2_ERR("out of memory!\n");
        return -ENOMEM;
    }
    r = -1;
    for (c = 0 ; c < inst->num_channels ; c++ ) {
        if (inst->tag & (1 << c)) {
            hm2_sserial_remote_t *chan = &inst->remotes[++r];
            chan->num_confs = 0;
            chan->num_modes = 0;
            chan->command_reg_addr = inst->command_reg_addr;
            chan->myinst = inst->index;
            chan->data_reg_addr= inst->data_reg_addr;
            chan->index = c;
            HM2_DBG("Instance %i, channel %i / %i\n", inst->index, c, r);
            chan->reg_cs_addr = md->base_address + 2 * md->register_stride
            + inst->index * md->instance_stride + c * sizeof(u32);
            HM2_DBG("reg_cs_addr = %x\n", chan->reg_cs_addr);
            chan->reg_0_addr = md->base_address + 3 * md->register_stride
            + inst->index * md->instance_stride + c * sizeof(u32);
            HM2_DBG("reg_0_addr = %x\n", chan->reg_0_addr);
            chan->reg_1_addr = md->base_address + 4 * md->register_stride
            + inst->index * md->instance_stride + c * sizeof(u32);
            HM2_DBG("reg_1_addr = %x\n", chan->reg_1_addr);
            chan->reg_2_addr = md->base_address + 5 * md->register_stride
            + inst->index * md->instance_stride + c * sizeof(u32);
            HM2_DBG("reg_2_addr = %x\n", chan->reg_2_addr);
            
            // Get the board ID and name before it is over-written by DoIts
            hm2->llio->read(hm2->llio, chan->reg_0_addr, 
                            &buff, sizeof(u32));
            chan->serialnumber = buff;
            HM2_DBG("BoardSerial %08x\n", chan->serialnumber);
            hm2->llio->read(hm2->llio, chan->reg_1_addr, chan->raw_name, sizeof(u32));
            chan->raw_name[1] |= 0x20; ///lower case
            if (hm2->use_serial_numbers){
                rtapi_snprintf(chan->name, sizeof(chan->name),
                               "hm2_%2s.%04x",
                               chan->raw_name,
                               (chan->serialnumber & 0xffff));
            } else {
                rtapi_snprintf(chan->name, sizeof(chan->name),
                               "%s.%2s.%d.%d",
                               hm2->llio->name, 
                               chan->raw_name,
                               inst->index,
                               c);
            }
            
            HM2_DBG("BoardName %s\n", chan->name);
            
            
            if (hm2_sserial_read_globals(hm2, chan) < 0) {
                HM2_ERR("Failed to read/setup the globals on %s\n", 
                        chan->name);
                return -EINVAL;
            }
            
            if (hm2_sserial_read_configs(hm2, chan) < 0) {
                HM2_ERR("Failed to read/setup the config data on %s\n", 
                        chan->name);
                return -EINVAL;
            } 
            
            if ( hm2_sserial_create_pins(hm2, chan) < 0) {
                HM2_ERR("Failed to create the pins on %s\n", 
                        chan->name);
                return -EINVAL;
            }

            if ( hm2_sserial_register_tram(hm2, chan) < 0) {
                HM2_ERR("Failed to register TRAM for %s\n",
                        chan->name);
                return -EINVAL;
            }
        }
    }
    return 0;           
}
void config_8i20(hostmot2_t *hm2, hm2_sserial_remote_t *chan){
    u32 buff;
    chan->num_modes=0;
    chan->num_confs = sizeof(hm2_8i20_params) / sizeof(hm2_sserial_data_t);
    chan->confs = kzalloc(sizeof(hm2_8i20_params),GFP_KERNEL);
    memcpy(chan->confs, hm2_8i20_params, sizeof(hm2_8i20_params));
    
    //8i20 has reprogrammable current scaling:
    buff = 0;
    hm2_sserial_get_bytes(hm2, chan, &buff, 0x8E8, 2);
    chan->confs[1].ParmMax = buff * 0.01;
    chan->confs[1].ParmMin = buff * -0.01;
    chan->globals = kzalloc(sizeof(hm2_8i20_globals), GFP_KERNEL);
    memcpy(chan->globals, hm2_8i20_globals, sizeof(hm2_8i20_globals));
    chan->num_globals = sizeof(hm2_8i20_globals) / sizeof(hm2_sserial_data_t);
}

void config_7i64(hostmot2_t *hm2, hm2_sserial_remote_t *chan){
    chan->num_modes=0;
    chan->num_confs = sizeof(hm2_7i64_params) / sizeof(hm2_sserial_data_t);
    chan->confs = kzalloc(sizeof(hm2_7i64_params), GFP_KERNEL);
    memcpy(chan->confs, hm2_7i64_params, sizeof(hm2_7i64_params));
}

int hm2_sserial_read_configs(hostmot2_t *hm2,  hm2_sserial_remote_t *chan){
    
    int ptoc, addr, buff, c, m;
    unsigned char rectype;
   
    hm2->llio->read(hm2->llio, chan->reg_2_addr, &buff, sizeof(u32)); 
    ptoc=(buff & 0xffff); 
    
    if (ptoc == 0) {return chan->num_confs;} // Old 8i20 or 7i64
    
    c = m = 0;
    chan->num_confs = 0;
    do {
        addr = 0;
        ptoc = hm2_sserial_get_bytes(hm2, chan, &addr, ptoc, 2);
        if (((addr &= 0xFFFF) <= 0) || (ptoc < 0)) break;
        if (hm2_sserial_get_bytes(hm2, chan, &rectype, addr, 1) < 0) {
            return -EINVAL;
        }
        
        if (rectype == LBP_DATA) {
            chan->num_confs++;
            c = chan->num_confs - 1;
            chan->confs = (hm2_sserial_data_t *)
                            krealloc(chan->confs, 
                                    chan->num_confs * sizeof(hm2_sserial_data_t),
                                    GFP_KERNEL);
            addr = hm2_sserial_get_bytes(hm2, chan, &chan->confs[c], addr, 14);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, chan, 
                                         &(chan->confs[c].UnitString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, chan, 
                                         &(chan->confs[c].NameString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
            
            if (chan->confs[c].ParmMin == chan->confs[c].ParmMax){
                chan->confs[c].ParmMin = 0;
                chan->confs[c].ParmMax = 1;
            }
        } else if (rectype == LBP_MODE ) {
            chan->num_modes++;
            m = chan->num_modes - 1;
            chan->modes = (hm2_sserial_mode_t *)
                            krealloc(chan->modes, 
                                     chan->num_modes * sizeof(hm2_sserial_mode_t),
                                     GFP_KERNEL);
            addr = hm2_sserial_get_bytes(hm2, chan, &chan->modes[m], addr, 4);
            if (addr < 0){ return -EINVAL;}
            addr = hm2_sserial_get_bytes(hm2, chan, 
                                         &(chan->modes[m].NameString), 
                                         addr, -1);
            if (addr < 0){ return -EINVAL;}
        }
    } while (addr > 0);
        
    return chan->num_confs;
}

int hm2_sserial_read_globals(hostmot2_t *hm2, hm2_sserial_remote_t *chan){
    
    int gtoc, addr, buff;
    
    unsigned char rectype;
    hm2_sserial_data_t data;
    
    chan->num_globals = 0;
    hm2->llio->read(hm2->llio, chan->reg_2_addr, &buff, sizeof(u32)); 
    gtoc=(buff & 0xffff0000) >> 16; 
    if (gtoc == 0){
        if (hm2->sserial.baudrate == 115200) {
            HM2_PRINT("Setup mode, creating no pins for smart-serial channel %s\n",
                      chan->name);
            chan->num_confs = 0;
            chan->num_globals = 0;
            return 0;
        }
        else if (strstr(chan->name, "8i20")){
            config_8i20(hm2, chan);
        }
        else if (strstr(chan->name, "7i64")){
            config_7i64(hm2, chan);
        }
        else { HM2_ERR("No GTOC in sserial read globals\n"); return -1;}
    }
    else
    {
        do {
            addr = 0;
            gtoc = hm2_sserial_get_bytes(hm2, chan, &addr, gtoc, 2);
            if (((addr &= 0xFFFF) <= 0) || (gtoc < 0)) break;
            if (hm2_sserial_get_bytes(hm2, chan, &rectype, addr, 1) < 0) {
                return -EINVAL;
            }
            if (rectype == LBP_DATA) {
                addr = hm2_sserial_get_bytes(hm2, chan, &data, addr, 14);
                if (addr < 0){ return -EINVAL;}
                addr = hm2_sserial_get_bytes(hm2, chan, &(data.UnitString), addr, -1);
                if (addr < 0){ return -EINVAL;}
                addr = hm2_sserial_get_bytes(hm2, chan, &(data.NameString), addr, -1);
                if (addr < 0){ return -EINVAL;}
                
                // only keep the nonvol types, and swrevision
                if (data.DataType == 0x04
                    || data.DataType == 0x05
                    || 0 == strcmp(data.NameString, "swrevision")){
                    chan->num_globals++;
                    chan->globals = (hm2_sserial_data_t *)
                             krealloc(chan->globals, 
                             chan->num_globals * sizeof(hm2_sserial_data_t),
                             GFP_KERNEL);
                    
                    chan->globals[chan->num_globals - 1] = data; 
                }
            }
            else if (rectype == LBP_MODE){
                char * type;
                hm2_sserial_mode_t mode;
                addr = hm2_sserial_get_bytes(hm2, chan, &mode, addr, 4);
                addr = hm2_sserial_get_bytes(hm2, chan, &mode.NameString, addr, -1);
                type = (mode.ModeType == 0x01)? "Software" : "Hardware";
                rtapi_print("Board %s %s Mode %i = %s\n", 
                            chan->name, 
                            type,
                            mode.ModeIndex, 
                            mode.NameString);
            }
        } while (addr > 0);
    }
    
    if ( hm2_sserial_create_params(hm2, chan) < 0) {
        HM2_ERR("Failed to create parameters for device %s\n", chan->name);
        return -EINVAL;
    }
    
    return 0;
}

int hm2_sserial_read_nvram_word(hostmot2_t *hm2, 
                                hm2_sserial_remote_t *chan, 
                                int addr,
                                int length,
                                void *data){
    u32 buff;
//    return 0; //// This needs to disappear eventually, obviously
    buff = 0xEC000000;
    hm2->llio->write(hm2->llio, chan->reg_cs_addr, &buff, sizeof(u32));
    buff = 0x01;
    hm2->llio->write(hm2->llio, chan->reg_0_addr, &buff, sizeof(u32));
    buff = 0x1000 | (1 << chan->index);
    hm2->llio->write(hm2->llio, chan->command_reg_addr, &buff, sizeof(u32));
    if (0 > hm2_sserial_waitfor(hm2, chan->command_reg_addr, 0xFFFFFFFF, 1012)){
        HM2_ERR("Timeout in sserial_read_nvram_word(2)\n");
        goto fail0;
    }
    switch (length){
        case 1:
            buff = 0x44000000 + addr; break;
        case 2:
            buff = 0x45000000 + addr; break;
        case 4:
            buff = 0x46000000 + addr; break;
        default:
            HM2_ERR("Unsupported global variable bitlength");
            return -EINVAL;
    }
    hm2->llio->write(hm2->llio, chan->reg_cs_addr, &buff, sizeof(u32));
    buff = 0x1000 | (1 << chan->index);
    hm2->llio->write(hm2->llio, chan->command_reg_addr, &buff, sizeof(u32));
    if (0 > hm2_sserial_waitfor(hm2, chan->command_reg_addr, 0xFFFFFFFF, 1013)){
        HM2_ERR("Timeout in sserial_read_nvram_word(4)\n");
        goto fail0;
    }
    hm2->llio->read(hm2->llio, chan->reg_0_addr, data, sizeof(u32));
    
fail0: // attempt to set back to normal access
    buff = 0xEC000000;
    hm2->llio->write(hm2->llio, chan->reg_cs_addr, &buff, sizeof(u32));
    buff = 0x00;
    hm2->llio->write(hm2->llio, chan->reg_0_addr, &buff, sizeof(u32));
    buff = 0x1000 | (1 << chan->index);
    hm2->llio->write(hm2->llio, chan->command_reg_addr, &buff, sizeof(u32));
    if (0 > hm2_sserial_waitfor(hm2, chan->command_reg_addr, 0xFFFFFFFF, 1014)){
        HM2_ERR("Timeout in sserial_read_nvram_word(6)\n");
        return -EINVAL;
    }
    return 0;
}    
    

int hm2_sserial_create_params(hostmot2_t *hm2, hm2_sserial_remote_t *chan){
    int i, r;
    hm2_sserial_data_t global;
    
    chan->params = hal_malloc(chan->num_globals * sizeof(hm2_sserial_params_t));
    
    for (i = 0 ; i < chan->num_globals ; i++){
        global = chan->globals[i];
        
        r = 0;
        switch (global.DataType) {
            case 0x02:
                if ( ! strcmp(global.NameString, "swrevision") 
                    || ! strcmp(global.NameString, "unitnumber")){
                        r = hal_param_u32_newf(HAL_RO, 
                                               &(chan->params[i].u32_param), 
                                               hm2->llio->comp_id,
                                               "%s.%s", 
                                               chan->name, 
                                               global.NameString);
                        if (r < 0) {HM2_ERR("Out of memory\n") ; return -ENOMEM;}
                        r = hm2_sserial_get_bytes(hm2, 
                                                  chan,
                                                  (void*)&(chan->params[i].u32_param),
                                                  global.ParmAddr,
                                                  global.DataLength/8);
                        if (r < 0) {HM2_ERR("SSerial Parameter read error\n") ; return -EINVAL;}
                        if ((strcmp(global.NameString, "swrevision") == 0) && (chan->params[i].u32_param < 14)) {
                            HM2_ERR("Warning: sserial remote device %s channel %d has old firmware that should be updated\n", chan->raw_name, chan->index);
                        }
                }
                break;
            case 0x04:
                r = hal_param_u32_newf(HAL_RO, 
                                       &(chan->params[i].u32_param), 
                                       hm2->llio->comp_id,
                                       "%s.%s", 
                                       chan->name, 
                                       global.NameString);
                if (r < 0) {HM2_ERR("Out of memory\n") ; return -ENOMEM;}
                r = hm2_sserial_read_nvram_word(hm2, 
                                                chan,
                                                global.ParmAddr,
                                                global.DataLength/8,
                                                (void*)&(chan->params[i].u32_param));
                if (r < 0) {HM2_ERR("SSerial Parameter read error\n") ; return -EINVAL;}
                break;
            case 0x05:
                r = hal_param_s32_newf(HAL_RO, 
                                       &(chan->params[i].s32_param), 
                                       hm2->llio->comp_id,
                                       "%s.%s", 
                                       chan->name, 
                                       chan->globals[i].NameString);
                if (r < 0) {HM2_ERR("Out of memory\n") ; return -ENOMEM;}
                r = hm2_sserial_read_nvram_word(hm2, 
                                                chan,
                                                global.ParmAddr,
                                                global.DataLength/8,
                                                (void*)&(chan->params[i].s32_param));
                if (r < 0) {HM2_ERR("SSerial Parameter read error\n") ; return -EINVAL;}
                break;
                
        }
    }
    return 0;
}
    
    
int hm2_sserial_create_pins(hostmot2_t *hm2, hm2_sserial_remote_t *chan){
    int i, j;
    int r = 0;
    char name[HAL_NAME_LEN + 1];
    int data_dir;
    chan->pins = (hm2_sserial_pins_t*)hal_malloc(chan->num_confs 
                                                 * sizeof(hm2_sserial_pins_t));
    
    chan->num_read_bits = 0 ; chan->num_write_bits = 0;

    for (i = 0 ; i < chan->num_confs ; i++ ){

        if (chan->confs[i].DataDir == LBP_IN){
            data_dir = HAL_OUT;
            chan->num_read_bits += chan->confs[i].DataLength;
        }
        else if (chan->confs[i].DataDir == LBP_IO){
            data_dir = HAL_IO;
            chan->num_read_bits += chan->confs[i].DataLength;
            chan->num_write_bits += chan->confs[i].DataLength;
        }
        else if (chan->confs[i].DataDir == LBP_OUT){
            data_dir = HAL_IN;
            chan->num_write_bits += chan->confs[i].DataLength;
        }
        else {
            HM2_ERR("Invalid Pin direction (%x). Aborting\n",
                    chan->confs[i].DataDir);
            return -EINVAL;
        }
        
        if (strcmp(chan->confs[i].UnitString, "gray") == 0){
            chan->pins[i].graycode = 1;
        } else {
            chan->pins[i].graycode = 0;
        }


        switch (chan->confs[i].DataType){
            case LBP_PAD:
                break;
            case LBP_BITS:
                chan->pins[i].bit_pins = (hal_bit_t**)
                hal_malloc(chan->confs[i].DataLength * sizeof(hal_bit_t*));
                chan->pins[i].bit_pins_not = (hal_bit_t**)
                hal_malloc(chan->confs[i].DataLength * sizeof(hal_bit_t*));
                chan->pins[i].invert = (hal_bit_t*)
                hal_malloc(chan->confs[i].DataLength * sizeof(hal_bit_t));
                for (j = 0; j < chan->confs[i].DataLength ; j++){
                    
                    rtapi_snprintf(name, sizeof(name), "%s.%s-%02d",
                                   chan->name, 
                                   chan->confs[i].NameString,
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
                        rtapi_snprintf(name, sizeof(name), "%s.%s-%02d-not",
                                       chan->name, 
                                       chan->confs[i].NameString,
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
                        rtapi_snprintf(name, sizeof(name), "%s.%s-%02d-invert",
                                       chan->name, 
                                       chan->confs[i].NameString,
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
            case LBP_UNSIGNED:
            case LBP_SIGNED:
                rtapi_snprintf(name, sizeof(name), "%s.%s",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_pin_float_new(name,
                                      data_dir,
                                      &(chan->pins[i].float_pin),
                                      hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                rtapi_snprintf(name, sizeof(name), "%s.%s-scalemax",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_param_float_new(name,
                                        HAL_RW,
                                        &(chan->pins[i].fullscale),
                                        hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].fullscale = chan->confs[i].ParmMax;
                if (data_dir == HAL_OUT) {break;}
                rtapi_snprintf(name, sizeof(name), "%s.%s-maxlim",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_param_float_new(name,
                                        HAL_RW,
                                        &(chan->pins[i].maxlim),
                                        hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].maxlim = chan->confs[i].ParmMax;
                rtapi_snprintf(name, sizeof(name), "%s.%s-minlim",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_param_float_new(name,
                                        HAL_RW,
                                        &(chan->pins[i].minlim),
                                        hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                chan->pins[i].minlim = chan->confs[i].ParmMin;
                break;
            case LBP_NONVOL_UNSIGNED:
            case LBP_NONVOL_SIGNED:
                HM2_ERR("Non-Volatile data type found in PTOC. This should "
                        "never happen. Aborting");
                return r;
            case LBP_STREAM:
                rtapi_snprintf(name, sizeof(name), "%s.%s",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_pin_u32_new(name,
                                    data_dir,
                                    &(chan->pins[i].u32_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                break;
            case LBP_BOOLEAN:
                rtapi_snprintf(name, sizeof(name), "%s.%s",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_pin_bit_new(name,
                                    data_dir,
                                    &(chan->pins[i].boolean),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return r;
                }
                if (data_dir != HAL_IN) {
                    rtapi_snprintf(name, sizeof(name), "%s.%s-not",
                                   chan->name, 
                                   chan->confs[i].NameString);
                    r = hal_pin_bit_new(name,
                                        data_dir,
                                        &(chan->pins[i].boolean2),
                                        hm2->llio->comp_id);
                    if (r < 0) {
                        HM2_ERR("error adding pin '%s', aborting\n", name);
                        return r;
                    }  
                }
                if (data_dir != HAL_OUT) {
                    chan->pins[i].invert = hal_malloc(sizeof(hal_bit_t));
                    rtapi_snprintf(name, sizeof(name), "%s.%s-invert",
                                   chan->name, 
                                   chan->confs[i].NameString);
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
            case LBP_ENCODER:
            case LBP_ENCODER_H:

                rtapi_snprintf(name, sizeof(name), "%s.%s.count",
                               chan->name, 
                               chan->confs[i].NameString);
                r = hal_pin_s32_new(name,
                                    HAL_OUT,
                                    &(chan->pins[i].s32_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }
                rtapi_snprintf(name, sizeof(name), "%s.%s.rawcounts",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_pin_s32_new(name,
                                    HAL_OUT,
                                    &(chan->pins[i].s32_pin2),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }
                rtapi_snprintf(name, sizeof(name), "%s.%s.position",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_pin_float_new(name,
                                    HAL_OUT,
                                    &(chan->pins[i].float_pin),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }
                rtapi_snprintf(name, sizeof(name), "%s.%s.index-enable",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_pin_bit_new(name,
                                    HAL_IO,
                                    &(chan->pins[i].boolean),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }

                rtapi_snprintf(name, sizeof(name), "%s.%s.reset",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_pin_bit_new(name,
                                    HAL_IO,
                                    &(chan->pins[i].boolean2),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }
                rtapi_snprintf(name, sizeof(name), "%s.%s.scale",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_param_float_new(name,
                                    HAL_RW,
                                    &(chan->pins[i].fullscale),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }

                rtapi_snprintf(name, sizeof(name), "%s.%s.counts-per-rev",
                               chan->name,
                               chan->confs[i].NameString);
                r = hal_param_u32_new(name,
                                    HAL_RW,
                                    &(chan->pins[i].u32_param),
                                    hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding pin '%s', aborting\n", name);
                    return -EINVAL;
                }
                chan->pins[i].fullscale = chan->confs[i].ParmMax;
                chan->pins[i].u32_param = 256;
                break;
            case LBP_ENCODER_L:
                //No pins for encoder L
                break;
            default:
                HM2_ERR("Unhandled sserial data type (%i) Name %s Units %s\n",
                        chan->confs[i].DataType, 
                        chan->confs[i].NameString,
                        chan->confs[i].UnitString);
        }
    }
    return 0;
}

int hm2_sserial_register_tram(hostmot2_t *hm2, hm2_sserial_remote_t *chan){

    int r = 0;

    HM2_DBG("%s read-bits = %i, write-bits = %i\n", chan->name,
            chan->num_read_bits, chan->num_write_bits);

    r = hm2_register_tram_read_region(hm2, chan->reg_cs_addr, sizeof(u32), 
                                      &chan->reg_cs_read);
    if (r < 0) { HM2_ERR("error registering tram read region for sserial CS"
                         "register (%d)\n", r);
        goto fail1;
    }
    if (chan->num_read_bits > 0){
        r = hm2_register_tram_read_region(hm2, chan->reg_0_addr, sizeof(u32),
                                          &chan->reg_0_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 0 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_0_read = NULL;
    }
    
    if (chan->num_read_bits > 32){
        r = hm2_register_tram_read_region(hm2, chan->reg_1_addr, sizeof(u32),
                                          &chan->reg_1_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 1 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_1_read = NULL;
    }
    
    if (chan->num_read_bits > 64){
        r = hm2_register_tram_read_region(hm2, chan->reg_2_addr, sizeof(u32),
                                          &chan->reg_2_read);
        if (r < 0) { HM2_ERR("error registering tram read region for sserial "
                             "interface 2 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_2_read = NULL;
    }
    
    // Register the TRAM WRITE
    if (chan->num_write_bits > 0){
        r = hm2_register_tram_write_region(hm2, chan->reg_0_addr, sizeof(u32),
                                           &(chan->reg_0_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 0 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_0_write = NULL;
    }
    
    if (chan->num_write_bits > 32){
        r = hm2_register_tram_write_region(hm2, chan->reg_1_addr, sizeof(u32),
                                           &(chan->reg_1_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 1 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_1_write = NULL;
    }
    
    if (chan->num_write_bits > 64){
        r = hm2_register_tram_write_region(hm2, chan->reg_2_addr, sizeof(u32),
                                           &(chan->reg_2_write));
        if (r < 0) {HM2_ERR("error registering tram write region for sserial"
                            "interface 2 register (%d)\n", r);
            goto fail1;
        }
    } else {
        chan->reg_2_write = NULL;
    }
    
    return 0;
    
fail1:
    return -EINVAL;
}


void hm2_sserial_prepare_tram_write(hostmot2_t *hm2, long period){
    // This function contains a state machine to handle starting and stopping
    // The ports as well as setting up the pin data

    static int doit_err_count, comm_err_flag; // to avoid repeating error messages
    int b, f, i, p, r; 
    int bitcount;
    u64 buff;
    float val;
    
    if (hm2->sserial.num_instances <= 0) return;
    
    for (i = 0 ; i < hm2->sserial.num_instances ; i++ ) {
        // a state-machine to start and stop the ports, and to
        // supply Do-It commands when required.
        
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        
        switch (*inst->state){
            case 0: // Idle
                if (! *inst->run){ return; }
                *inst->state = 0x11;
                inst->timer = 0;
                
                //set the modes for the cards
                hm2_sserial_setmode(hm2, inst);
                
                *inst->command_reg_write = 0x900 | inst->tag;
                HM2_DBG("Tag-All = %x\n", inst->tag);
                *inst->fault_count = 0;
                doit_err_count = 0;
                comm_err_flag = 0;
                break;
            case 0x01: // normal running
                if (!*inst->run){
                     *inst->state = 0x02;
                    break;
                }
                
                if (*inst->fault_count > inst->fault_lim) {
                    // If there have been a large percentage of misses, for quite
                    // a long time, it's time to take it seriously. 
                    HM2_ERR("Smart Serial Comms Error: "
                            "There have been more than %i errors in %i "
                            "thread executions at least %i times. "
                            "See other error messages for details.\n",
                            inst->fault_dec, 
                            inst->fault_inc,
                            inst->fault_lim);
                    HM2_ERR("***Smart Serial Port %i will be stopped***\n",i); 
                    *inst->state = 0x20;
                    *inst->run = 0;
                    *inst->command_reg_write = 0x800; // stop command
                    break;
                }
                if (*inst->command_reg_read) {
                    if (doit_err_count < 6){ doit_err_count++; }
                    if (doit_err_count == 4 ){ // ignore 4 errors at startup
                        HM2_ERR("Smart Serial port %i: DoIt not cleared from previous "
                                "servo thread. Servo thread rate probably too fast. "
                                "This message will not be repeated, but the " 
                                "%s.sserial.%1d.fault-count pin will indicate "
                                "if this is happening frequently.\n",
                                i, hm2->llio->name, i);
                    }
                    *inst->fault_count += inst->fault_inc;
                    *inst->command_reg_write = 0x80000000; // set bit31 for ignored cmd
                    break; // give the register chance to clear
                }
                if (hm2_sserial_check_errors(hm2, inst) != 0) {
                    if (*inst->data_reg_read & 0xff) { // indicates a failed transfer
                        f = (*inst->data_reg_read & (comm_err_flag ^ 0xFF));
                        if (f != 0 && f != 0xFF) {
                            HM2_ERR("Smart Serial Error: port %i channel %i. " 
                                "You may see this error if the FPGA card "
                                """read"" thread is not running. "
                                "This error message will not repeat.\n",
                                i, ffs(f) - 1);
                        }
                    }
                    *inst->fault_count += inst->fault_inc;
                }
                
                if (*inst->fault_count > inst->fault_dec) {
                    *inst->fault_count -= inst->fault_dec;
                }
                else
                {
                    *inst->fault_count = 0;
                }
                
                // All seems well, handle the pins. 
                for (r = 0 ; r < inst->num_remotes ; r++ ) {
                    hm2_sserial_remote_t *chan = &inst->remotes[r];
                    bitcount = 0;
                    if (chan->reg_0_write) *chan->reg_0_write = 0;
                    if (chan->reg_1_write) *chan->reg_1_write = 0;
                    if (chan->reg_2_write) *chan->reg_2_write = 0;
                    for (p = 0 ; p < chan->num_confs ; p++){
                        hm2_sserial_data_t *conf = &chan->confs[p];
                        hm2_sserial_pins_t *pin = &chan->pins[p];
                        if (conf->DataDir & 0xC0){
                            switch (conf->DataType){
                                case LBP_PAD:
                                    // do nothing
                                    break;
                                case LBP_BITS:
                                    buff = 0;
                                    for (b = 0 ; b < conf->DataLength ; b++){
                                        buff |= ((u64)(*pin->bit_pins[b] != 0) << b)
                                        ^ ((u64)(pin->invert[b] != 0) << b);
                                    }
                                    break;
                                case LBP_UNSIGNED:
                                    val = *pin->float_pin;
                                    if (val > pin->maxlim) val = pin->maxlim;
                                    if (val < pin->minlim) val = pin->minlim;
				    /* convert to u32 before u64 to
				     avoid needing __fixunsdfdi() in
				     libgcc.a from some gccs on 32-bit
				     arches
				    */
                                    buff = (u64)(u32)
					((val / (float)pin->fullscale)
					 * (~0ull >> (64 - conf->DataLength)));
                                    break;
                                case LBP_SIGNED:
                                    //this only works if DataLength <= 32
                                    val = *pin->float_pin;
                                    if (val > pin->maxlim) val = pin->maxlim;
                                    if (val < pin->minlim) val = pin->minlim;
                                    buff = (((s32)(val / pin->fullscale * 2147483647))
                                            >> (32 - conf->DataLength))
                                    & (~0ull >> (64 - conf->DataLength));
                                    break;
                                case LBP_STREAM:
                                    buff = *pin->u32_pin & (~0ull >> (64 - conf->DataLength));
                                    break;
                                case LBP_BOOLEAN:
                                    buff = 0;
                                    if (*pin->boolean ^ *pin->invert){
                                        buff = (~0ull >> (64 - conf->DataLength));
                                    }
                                    break;
                                case LBP_ENCODER:
                                     // Would we ever write to a counter? 
                                    // Assume not for the time being
                                    break;
                                default:
                                    HM2_ERR("Unsupported output datatype %i (name ""%s"")\n",
                                            conf->DataType, conf->NameString);
                                    
                            }
                            bitcount = setbits(chan, &buff, bitcount, conf->DataLength);
                        }
                    }
                }
                
                *inst->command_reg_write = 0x1000 | inst->tag;
                break;
 
            case 0x02: // run to stop transition
                *inst->state = 0x10;
                inst->timer = 0;
                *inst->command_reg_write = 0x800;
                break;
            case 0x10:// wait for normal stop
            case 0x11:// wait for normal start
                inst->timer += period;
                if (*inst->command_reg_read != 0) {
                    if (inst->timer < 2100000000) {
                        break;
                    }
                    HM2_ERR("sserial_write:"
                            "Timeout waiting for CMD to clear\n");
                    *inst->fault_count += inst->fault_inc;
                    // carry on, nothing much we can do about it
                }
                *inst->state &= 0x0F;
                *inst->command_reg_write = 0x80000000; // mask pointless writes
                break;
            case 0x20:// Do-nothing state for serious errors. require run pin to cycle
                *inst->command_reg_write = 0x80000000; // set bit31 for ignored cmd
                if ( ! *inst->run){*inst->state = 0x02;}
                break;
            default: // Should never happen
                HM2_ERR("Unhandled run/stop configuration in \n"
                        "hm2_sserial_write (%x)\n",
                        *inst->state);
                *inst->state = 0;
        }
    }
}

int hm2_sserial_read_pins(hm2_sserial_remote_t *chan){
    static int h_flag = 0, l_flag = 0;//these are the "memory" for 2-part 
    static int bitshift = 1;               //Fanuc encoders where the full turns
    static u64 buff_store;             //and part turns are not contiguous
    int b, p, r;
    int bitcount = 0;
    u64 buff;
    s32 buff32;
    s64 buff64;
    chan->status = *chan->reg_cs_read;
    for (p=0 ; p < chan->num_confs ; p++){
        hm2_sserial_data_t *conf = &chan->confs[p];
        hm2_sserial_pins_t *pin = &chan->pins[p];
        if (! (conf->DataDir & 0x80)){
            r = getbits(chan, &buff, bitcount, conf->DataLength);
            if(r < 0) return r;
            
            switch (conf->DataType){
            case LBP_PAD:
                // do nothing
                break;
            case LBP_BITS:
                for (b = 0 ; b < conf->DataLength ; b++){
                    *pin->bit_pins[b] = ((buff & (1LL << b)) != 0);
                    *pin->bit_pins_not[b] = ! *pin->bit_pins[b];
                }
                break;
            case LBP_UNSIGNED:
                
                if (pin->graycode){
                    u64 mask;
                    for(mask = buff >> 1 ; mask != 0 ; mask = mask >> 1){
                        buff ^= mask;
                    }
                }
                
                *pin->float_pin = (buff * pin->fullscale)
                / ((1 << conf->DataLength) - 1);
                break;
            case LBP_SIGNED:
                buff32 = (buff & 0xFFFFFFFFL) << (32 - conf->DataLength);
                *pin->float_pin = (buff32 / 2147483647.0 )
                                    * pin->fullscale;
                break;
            case LBP_STREAM:
                *pin->u32_pin = buff & (~0ull >> (64 - conf->DataLength));
                break;
            case LBP_BOOLEAN:
                *pin->boolean = (buff != 0);
                *pin->boolean2 = (buff == 0);
                break;
            case LBP_ENCODER_H:
            case LBP_ENCODER_L:
                if (conf->DataType == LBP_ENCODER_H){
                    h_flag = conf->DataLength;
                    buff_store |= (buff << bitshift);
                } else {
                    l_flag = conf->DataLength;
                    bitshift = conf->DataLength;
                    buff_store |= buff;
                }
                if ( ! (h_flag && l_flag)){
                    break;
                }
                buff = buff_store;
                /* no break */
            case LBP_ENCODER:
            {
                int bitlength;
                s32 rem1, rem2;
                s64 previous;
                u32 ppr = pin->u32_param;
                
                if (conf->DataType == LBP_ENCODER){
                    bitlength = conf->DataLength;
                } else {
                    bitlength = h_flag + l_flag;
                    h_flag = 0; l_flag = 0;
                    buff_store = 0;
                }
                
                
                if (pin->graycode){
                    u64 mask;
                    for(mask = buff >> 1 ; mask != 0 ; mask = mask >> 1){
                        buff ^= mask;
                    }
                }

                // sign-extend buff into buff64
                buff64 = (1U << (bitlength - 1));
                buff64 = (buff ^ buff64) - buff64;
                previous = pin->accum;

                if ((buff64 - pin->oldval) > (1 << (bitlength - 2))){
                    pin->accum -= (1 << bitlength);
                } else if ((pin->oldval - buff64) > (1 << (bitlength - 2))){
                    pin->accum += (1 << bitlength);
                }
                pin->accum += (buff64 - pin->oldval);

                //reset
                if (*pin->boolean2){pin->offset = pin->accum;}

                //index-enable
                if (*pin->boolean && ppr > 0){ // index-enable set
                    rtapi_div_s64_rem(previous, ppr, &rem1);
                    rtapi_div_s64_rem(pin->accum, ppr, &rem2);
                    if (abs(rem1 - rem2) > ppr / 2
                            || (rem1 >= 0 && rem2 < 0)
                            || (rem1 < 0 && rem2 >= 0)){
                        if (pin->accum > previous){
                            if (pin->accum > 0){
                                pin->offset = pin->accum - rem2;
                            } else if (pin->accum < 0){
                                pin->offset = pin->accum - rem2;
                            } else {
                                pin->offset = 0;
                            }
                        } else {
                            if (pin->accum > 0){
                                pin->offset = pin->accum - rem2 + ppr;
                            } else if (pin->accum < 0){
                                pin->offset = pin->accum - rem2;
                            } else {
                                pin->offset = 0;
                            }
                        }
                        *pin->boolean = 0;
                    }
                }
                pin->oldval = buff64;
                *pin->s32_pin = pin->accum - pin->offset;
                *pin->s32_pin2 = pin->accum;
                *pin->float_pin = (double)(pin->accum - pin->offset) / pin->fullscale ;
                break;
            }
            default:
                HM2_ERR_NO_LL("Unsupported input datatype %i (name ""%s"")\n",
                        conf->DataType, conf->NameString);
            }
            bitcount += conf->DataLength;
        }
    }
    return 0;
}

void hm2_sserial_process_tram_read(hostmot2_t *hm2, long period){
    int i, c;
    for (i = 0 ; i < hm2->sserial.num_instances ; i++){
        hm2_sserial_instance_t *inst = &hm2->sserial.instance[i];
        if (*inst->state != 0x01) continue ; // Only work on running instances
        for (c = 0 ; c < inst->num_remotes ; c++ ) {
            hm2_sserial_remote_t *chan = &inst->remotes[c];
            hm2_sserial_read_pins(chan);
        }
    }
}

void hm2_sserial_print_module(hostmot2_t *hm2) {
    int i,r,c,g,m;
    HM2_PRINT("SSerial: %d\n", hm2->sserial.num_instances);
    HM2_PRINT("  version %d\n", hm2->sserial.version);
    if (hm2->sserial.num_instances <= 0) return;
    for (i = 0; i < hm2->sserial.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        Command Addr 0x%04x\n", hm2->sserial.instance[i].command_reg_addr);
        HM2_PRINT("        Data Addr    0x%04x\n", hm2->sserial.instance[i].data_reg_addr);
        for (r = 0; r < hm2->sserial.instance[i].num_remotes; r++) {
            HM2_PRINT("        port %i device %s\n", r, hm2->sserial.instance[i].remotes[r].name);
            HM2_PRINT("             Parameters:\n");
            for (c = 0 ; c < hm2->sserial.instance[i].remotes[r].num_confs; c++){
                hm2_sserial_data_t conf = hm2->sserial.instance[i].remotes[r].confs[c];
                HM2_PRINT("                   RecordType = 0x%02x\n", conf.RecordType);
                HM2_PRINT("                   DataLength = 0x%02x\n", conf.DataLength);
                HM2_PRINT("                   DataType = 0x%02x\n", conf.DataType);
                HM2_PRINT("                   DataDir = 0x%02x\n", conf.DataDir);
                HM2_PRINT("                   ParmMax %0i.%02i\n", (int)conf.ParmMax, 
                        (int)((conf.ParmMax - (int)conf.ParmMax) * 100.0));
                HM2_PRINT("                   ParmMin %0i.%02i\n",(int)conf.ParmMin,
                        (int)((conf.ParmMin - (int)conf.ParmMin) * 100.0));
                HM2_PRINT("                   SizeOf ParmMin 0x%02zx\n", sizeof(conf.ParmMax));
                HM2_PRINT("                   ParmAddr = 0x%04x\n", conf.ParmAddr); 
                HM2_PRINT("                   UnitString = %s\n", conf.UnitString);
                HM2_PRINT("                   NameString = %s\n\n", conf.NameString);
            }
            HM2_PRINT("             Globals:\n");
            for (g = 0 ; g < hm2->sserial.instance[i].remotes[r].num_globals; g++){
                hm2_sserial_data_t conf = hm2->sserial.instance[i].remotes[r].globals[g];
                HM2_PRINT("                   RecordType = 0x%02x\n", conf.RecordType);
                HM2_PRINT("                   DataLength = 0x%02x\n", conf.DataLength);
                HM2_PRINT("                   DataType = 0x%02x\n", conf.DataType);
                HM2_PRINT("                   DataDir = 0x%02x\n", conf.DataDir);
                HM2_PRINT("                   ParmMax %0i.%02i\n", (int)conf.ParmMax, 
                        (int)((conf.ParmMax - (int)conf.ParmMax) * 100.0));
                HM2_PRINT("                   ParmMin %0i.%02i\n",(int)conf.ParmMin,
                        (int)((conf.ParmMin - (int)conf.ParmMin) * 100.0));
                HM2_PRINT("                   SizeOf ParmMin %zi\n", sizeof(conf.ParmMax));
                HM2_PRINT("                   ParmAddr = 0x%04x\n", conf.ParmAddr); 
                HM2_PRINT("                   UnitString = %s\n", conf.UnitString);
                HM2_PRINT("                   NameString = %s\n\n", conf.NameString);
            }
            HM2_PRINT("             Modes:\n");         
            for (m = 0; m < hm2->sserial.instance[i].remotes[r].num_modes; m++){
                hm2_sserial_mode_t mode = hm2->sserial.instance[i].remotes[r].modes[m];
                HM2_PRINT("               RecordType = 0x%02x\n", mode.RecordType);
                HM2_PRINT("               ModeIndex = 0x%02x\n", mode.ModeIndex);
                HM2_PRINT("               ModeType = 0x%02x\n", mode.ModeType);
                HM2_PRINT("               Unused = %i\n", mode.Unused);
                HM2_PRINT("               NameString = %s\n\n", mode.NameString);
            }
        }
    }
    HM2_PRINT("\n");
}

int hm2_sserial_get_bytes(hostmot2_t *hm2, hm2_sserial_remote_t *chan, void *buffer, int addr, int size ){
    // Gets the bytes one at a time. This could be done more efficiently. 
    char *ptr;
    u32 data;
    int string = size;
    // -1 in size means "find null" for strings. -2 means don't lcase
    
    ptr = (char*)buffer;
    while(0 != size){
        data = 0x4C000000 | addr++;
        hm2->llio->write(hm2->llio, chan->reg_cs_addr, &data, sizeof(u32));
        
        if (0 > hm2_sserial_waitfor(hm2, chan->reg_cs_addr, 0x0000FF00, 24)){
            HM2_ERR("Timeout trying to read config data in sserial_get_bytes\n");
            return -EINVAL;
        }
        data = 0x1000 | (1 << chan->index);
        hm2->llio->write(hm2->llio, chan->command_reg_addr, &data, sizeof(u32));
        
        if (0 > hm2_sserial_waitfor(hm2, chan->command_reg_addr, 0xFFFFFFFF, 25)){
            HM2_ERR("Timeout during do-it in sserial_get_bytes\n");
            return -EINVAL;
        }
        
        hm2->llio->read(hm2->llio, chan->reg_0_addr, &data, sizeof(u32));
        data &= 0x000000FF;
        size--;
        if (size < 0) { // string data
            if (data == 0 || size < (-HM2_SSERIAL_MAX_STRING_LENGTH)){
                size = 0; 
            } else if (string > -2 && data >= 'A' && data <= 'Z') {
                data |= 0x20; // lower case
            }
        } 
        
        *(ptr++) = (unsigned char)data;
    }
    return addr;
}

int getbits(hm2_sserial_remote_t *chan, u64 *val, int start, int len){
    long long user0 = (chan->reg_0_read == NULL)? 0 : *chan->reg_0_read;
    long long user1 = (chan->reg_1_read == NULL)? 0 : *chan->reg_1_read;
    long long user2 = (chan->reg_2_read == NULL)? 0 : *chan->reg_2_read;
    long long mask = (~0ull >> (64 - len));
    
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


int setbits(hm2_sserial_remote_t *chan, u64 *val, int start, int len){
    // Assumes that all registers are zeroed elsewhere as required
    long long mask0, mask1, mask2;
    int end = start + len;
    
    if (end <= 32){
        mask0 = (~0ull >> (64 - len));
        *chan->reg_0_write |= (*val  & mask0) << start;
    } else if (end <= 64){
        if (start >= 32){
            mask1 = (~0ull >> (64 - len));
            *chan->reg_1_write |= (*val & mask1) << (start - 32);
        } else { 
            mask0 = (~0ull >> (32 + start));
            mask1 = (~0ull >> (96 - (end))) << (32 - start);
            *chan->reg_0_write |= (*val & mask0) << start;
            *chan->reg_1_write |= (*val & mask1) >> (32 - start);
        }
    } else {
        if (start >= 64){
            mask2 = (~0ull >> (64 - len));
            *chan->reg_2_write |= (*val  & mask2) << (start - 64);
        } else if (start >= 32) {
            mask1 = (~0ull >> start);
            mask2 = (~0ull >> (128 - (end))) << (64 - start);
            *chan->reg_1_write |= (*val & mask1) << (start - 32);
            *chan->reg_2_write |= (*val & mask2) >> (64 - start);
        } else {
            mask0 = (~0ull >> (32 + start));
            mask1 = (0xFFFFFFFFull << (32 - start));
            mask2 = (~0ull >> (128 - (end))) << (64 - start);
            *chan->reg_0_write |= (*val & mask0) << start;
            *chan->reg_1_write = (*val & mask1) >> (32 - start);
            *chan->reg_2_write |= (*val & mask2) >> (64 - start);
        }
    }
    return end;
}

int hm2_sserial_stopstart(hostmot2_t *hm2, hm2_module_descriptor_t *md, 
                          hm2_sserial_instance_t *inst, u32 start_mode){
    u32 buff, addr;
    int i = inst->index;
    int c;
    
    buff=0x800; //Stop All
    hm2->llio->write(hm2->llio, inst->command_reg_addr, &buff, sizeof(u32));
    if (hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF,51) < 0){
        return -EINVAL;
    }
    
    for (c = 0 ; c < inst->num_channels ; c++){
        if (hm2->config.sserial_modes[i][c] != 'x'){
            start_mode |= 1 << c;
            HM2_DBG("Start-mode = %x\n", start_mode);
            // CS addr - write card mode
            addr = md->base_address + 2 * md->register_stride
            + i * md->instance_stride + c * sizeof(u32);
            buff = (hm2->config.sserial_modes[i][c] - '0') << 24;
            hm2->llio->write(hm2->llio, addr, &buff, sizeof(u32));
        }
    }
    hm2->llio->write(hm2->llio, inst->command_reg_addr, &start_mode, sizeof(u32));
    if (hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF, 8000) < 0){
        return -EINVAL;
    }
    return 0;
}

void hm2_sserial_setmode(hostmot2_t *hm2, hm2_sserial_instance_t *inst){
    u32 buff=0x00;
    u32 addr;
    int c;
    int n = 0;
    int i = inst->index;
    HM2_DBG("Num Auto = %i\n", inst->num_remotes);
    for (c = 0 ; c < inst->num_remotes ; c++){
        n = inst->remotes[c].index;
        if (hm2->config.sserial_modes[i][n] != 'x') {
            // CS addr - write card mode
            addr = inst->remotes[c].reg_cs_addr;
            buff = (hm2->config.sserial_modes[i][n] - '0') << 24;
            hm2->llio->write(hm2->llio, addr, &buff, sizeof(u32));
            HM2_DBG("Normal Start: Writing %08x to %04x\n", buff, addr);
        }
    }
}


int hm2_sserial_check_errors(hostmot2_t *hm2, hm2_sserial_instance_t *inst){
    u32 buff;
    int i,r;
    int err_flag = 0;
    u32 err_mask = 0xFF00E1FF;
    const char *err_list[32] = {"CRC error", "Invalid cookie", "Overrun",
        "Timeout", "Extra character", "Serial Break Error", "Remote Fault", 
        "Too many errors", 
        
        "Remote fault", "unused", "unused", "unused", "unused", 
        "Communication error", "No Remote ID", "Communication Not Ready",
        
        "unused","unused","unused","unused","unused","unused","unused","unused",
        
        "Watchdog Fault", "No Enable", "Over Temperature", "Over Current", 
        "Over Voltage", "Under Voltage", "Illegal Remote Mode", "LBPCOM Fault"};
    
    for (r = 0 ; r < inst->num_remotes ; r++){
        hm2_sserial_remote_t *chan=&inst->remotes[r];
        buff = chan->status;
        buff &= err_mask;
        for (i = 31 ; i > 0 ; i--){
            if (buff & (1 << i)) {
                HM2_ERR("Smart serial card %s error = (%i) %s\n", 
                        chan->name, i, err_list[i]);
                err_flag = -EINVAL;
            }
        }
    }
    return err_flag;
}

int hm2_sserial_waitfor(hostmot2_t *hm2, u32 addr, u32 mask, int ms){
    u64 t1, t2;
    u32 d;
    t1 = rtapi_get_time();
    do { // wait for addr to clear
        rtapi_delay(50000);
        hm2->llio->read(hm2->llio, addr, &d, sizeof(u32));
        t2 = rtapi_get_time();
        if ((u32)(t2 - t1) > 1000000L * ms) {
            HM2_ERR("hm2_sserial_waitfor: Timeout (%dmS) waiting for addr %x &"
                    "mask %x val %x\n", ms, addr, mask, d & mask);
            addr += 0x100;
            hm2->llio->read(hm2->llio, addr, &d, sizeof(u32));
            HM2_ERR("DATA addr %x after timeout: %x\n", addr, d);
            return -1;
        }
    }while (d & mask);
    return 0;
}

int getlocal8(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr){
    u32 val = 0;
    u32 buff;
    buff = READ_LOCAL_CMD | addr;
    HM2WRITE(inst->command_reg_addr, buff);
    hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF, 22);
    HM2READ(inst->data_reg_addr, buff);
    val = (val << 8) | buff;
    return val;
}

int getlocal32(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr){
    u32 val = 0;
    int bytes = 4;
    u32 buff;
    for (;bytes--;){
        buff = READ_LOCAL_CMD | (addr + bytes);
        HM2WRITE(inst->command_reg_addr, buff);
        hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF, 22);
        HM2READ(inst->data_reg_addr, buff);
        val = (val << 8) | buff;
    }    
    return val;
}

int setlocal32(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int addr, int val){
    int bytes = 0;
    u32 buff;
    for (;bytes < 4; bytes++){
        
        if (hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF, 22) < 0) {
            HM2_ERR("Command register not ready\n");
            return -1;
        }
        
        buff = val & 0xff;
        val >>= 8;
        HM2WRITE(inst->data_reg_addr, buff);
        buff = WRITE_LOCAL_CMD | (addr + bytes);
        HM2WRITE(inst->command_reg_addr, buff);
        
        if (hm2_sserial_waitfor(hm2, inst->command_reg_addr, 0xFFFFFFFF, 22) < 0) {
            HM2_ERR("Write failure attempting to set baud rate\n");
            return -1;
        }
    }    
    return 0;
}

int check_set_baudrate(hostmot2_t *hm2, hm2_sserial_instance_t *inst){
    u32 baudrate;
    int baudaddr;
    int lbpstride; 
    u32 buff;
    int c;
    
    if (hm2->sserial.baudrate < 0){ return 0;}
    if (hm2->sserial.version < 34) {
    HM2_ERR("Setting baudrate is not supported in the current firmware version\n"
    "Version must be > v33 and you have version %i.", hm2->sserial.version);
    return -EINVAL;
    }
    lbpstride = getlocal8(hm2, inst, SSLBPCHANNELSTRIDELOC);
    HM2_PRINT("num_channels = %i\n", inst->num_channels);
    for (c = 0; c < inst->num_channels; c++){
        baudaddr = getlocal8(hm2, inst, SSLBPCHANNELSTARTLOC) + (c * lbpstride) + 42;
        baudrate = getlocal32(hm2, inst, baudaddr);
        HM2_PRINT("Chan %i baudrate = %i\n", c, baudrate);
        if (baudrate != hm2->sserial.baudrate) {
            if (setlocal32(hm2, inst, baudaddr, hm2->sserial.baudrate) < 0) {
                HM2_ERR("Problem setting new baudrate, power-off reset may be needed to"
                        " recover from this.\n");
                return -EINVAL;
            }
            baudrate = getlocal32(hm2, inst, baudaddr);
            HM2_PRINT("Chan %i. Baudrate set to %i\n", c, baudrate);
        }
    }
    buff = 0x800; HM2WRITE(inst->command_reg_addr, buff); // stop all
    
    return 0;
}


void hm2_sserial_force_write(hostmot2_t *hm2){
    int i;
    u32 buff;
    for(i = 0; i < hm2->sserial.num_instances; i++){
        buff = 0x800;
        hm2->llio->write(hm2->llio, hm2->sserial.instance[i].command_reg_addr, &buff, sizeof(u32));
        *hm2->sserial.instance[i].run = 0;
        *hm2->sserial.instance[i].state = 0;
        hm2_sserial_waitfor(hm2, hm2->sserial.instance[i].command_reg_addr, 0xFFFFFFFF, 26);
        *hm2->sserial.instance[i].run = 1;
        *hm2->sserial.instance[i].command_reg_write = 0x80000000;
    }
}

void hm2_sserial_cleanup(hostmot2_t *hm2){
    int i,r;
    u32 buff;
    for (i = 1 ; i < hm2->sserial.num_instances; i++){
        //Shut down the sserial devices rather than leave that to the watchdog. 
        buff = 0x800;
        hm2->llio->write(hm2->llio,
                         hm2->sserial.instance[i].command_reg_addr,
                         &buff,
                         sizeof(u32));
        if (hm2->sserial.instance[i].remotes != NULL){
            if (hm2->sserial.instance[i].remotes){
                for (r = 0 ; r < hm2->sserial.instance[i].num_remotes; r++){
                    if (hm2->sserial.instance[i].remotes[r].num_confs > 0){
                        kfree(hm2->sserial.instance[i].remotes[r].confs);
                    };
                    if (hm2->sserial.instance[i].remotes[r].num_modes > 0){
                        kfree(hm2->sserial.instance[i].remotes[r].modes);
                    }
                }
                kfree(hm2->sserial.instance[i].remotes);
            }
        }
        
    }
}


