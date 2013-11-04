

//
//  Driver for the Mesa SSI Encoder module.
//  It is expected that it will be expanded to cover BISS and Fanuc absolute
//  encoders in the future.
//



#include "config_module.h"
#include RTAPI_INC_SLAB_H
#include "rtapi/rtapi_compat.h" //simple_strtol

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "rtapi_math64.h"
#include "hal.h"
#include "hal/drivers/mesa-hostmot2/hostmot2.h"

static bool funct_flag = false;

static void hm2_absenc_trigger(void *void_hm2, long period){
    hostmot2_t *hm2 = void_hm2;
    u32 buff = 0xFFFFFFFF;
    if (hm2->absenc.ssi_global_start_addr){
    hm2->llio->write(hm2->llio,
            hm2->absenc.ssi_global_start_addr,
            &buff,
            sizeof(u32));
    }
    if (hm2->absenc.fabs_global_start_addr){
    hm2->llio->write(hm2->llio,
            hm2->absenc.fabs_global_start_addr,
            &buff,
            sizeof(u32));
    }
    if (hm2->absenc.biss_global_start_addr){
    hm2->llio->write(hm2->llio,
            hm2->absenc.biss_global_start_addr,
            &buff,
            sizeof(u32));
    }

}

int hm2_absenc_register_tram(hostmot2_t *hm2){
    int i;
    
    // This function is called from the main hostmot2 driver when all the 
    // encoder types have had a chance to be set up.
    
    // If we don't read the "busy" flags first in the sequence then they will
    // always show "busy"
    
    if (hm2->absenc.ssi_global_start_addr){
        if (hm2_register_tram_read_region(hm2, hm2->absenc.ssi_global_start_addr,
                sizeof(u32),
                &(hm2->absenc.ssi_busy_flags)) < 0){
            HM2_ERR("error registering tram read region for SSI flags\n");
            return -EINVAL;
        }
    }
    if (hm2->absenc.biss_global_start_addr){
        if (hm2_register_tram_read_region(hm2, hm2->absenc.biss_global_start_addr,
                sizeof(u32),
                &(hm2->absenc.biss_busy_flags)) < 0){
            HM2_ERR("error registering tram read region for BiSS flags\n");
            return -EINVAL;
        }
    }
    if (hm2->absenc.fabs_global_start_addr){
        if (hm2_register_tram_read_region(hm2, hm2->absenc.fabs_global_start_addr,
                sizeof(u32),
                &(hm2->absenc.fabs_busy_flags)) < 0){
            HM2_ERR("error registering tram read region for BiSS flags\n");
            return -EINVAL;
        }
    }
    
    for (i = 0; i < hm2->absenc.num_chans; i++){
        int r = 0;
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        
        r = hm2_register_tram_read_region(hm2, chan->reg_cs_addr,
                  sizeof(u32),
                  &chan->reg_cs_read);
        
        switch (chan->myinst){
        case HM2_GTAG_FABS:
            r += hm2_register_tram_read_region(hm2, chan->reg_2_addr,
                    sizeof(u32),
                    &chan->reg_2_read);
                    /* no break */
        case HM2_GTAG_SSI:
            r += hm2_register_tram_read_region(hm2, chan->reg_1_addr,
                    sizeof(u32),
                    &chan->reg_1_read);
            r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                    sizeof(u32),
                    &chan->reg_0_read);
            break;
        case HM2_GTAG_BISS:
            //BiSS is different, it reads mutliple times from the same address
            r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                    sizeof(u32),
                    &chan->reg_0_read);
            if (chan->num_read_bits > 32){
                r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                        sizeof(u32),
                        &chan->reg_1_read);
            }
            if (chan->num_read_bits > 64){
                r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                        sizeof(u32),
                        &chan->reg_2_read);
            }
            if (chan->num_read_bits > 96){
                HM2_ERR("The driver is currently limited to 96 total bits and"
                        "no more than 32 in a single field. If you have hit "
                        "this limit then please raise a feature request\n");
                return -EINVAL;
            }
            break;
        }
        if (r < 0) {
            HM2_ERR("error registering tram read region for Absolute encoder\n");
            return -EINVAL;
        }
    }
    
    // If there is no dpll to link to, then we export the trigger function.
    
    if (hm2->config.num_dplls == 0){
        char name[HM2_SSERIAL_MAX_STRING_LENGTH+1] = "";
        rtapi_snprintf(name, sizeof(name),
                "%s.trigger-encoders", hm2->llio->name);
        hal_export_funct(name, hm2_absenc_trigger,
                hm2, 0, 0,hm2->llio->comp_id);
        funct_flag = true;
    }

    return 0;
}

int hm2_absenc_setup_ssi(hostmot2_t *hm2, hm2_sserial_remote_t *chan, 
                         hm2_module_descriptor_t *md){

    if ( hm2_sserial_create_pins(hm2, chan)) return -EINVAL;
    
    chan->params = hal_malloc(sizeof(hm2_sserial_params_t));
    hm2->absenc.clock_frequency = md->clock_freq;
    hm2->absenc.ssi_version = md->version;

    chan->reg_0_addr = md->base_address 
            + (0 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_1_addr = md->base_address 
            + (1 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_cs_addr = md->base_address 
            + (2 * md->register_stride)
            + chan->index * md->instance_stride;
    hm2->absenc.ssi_global_start_addr = md->base_address 
            + (3 * md->register_stride);
    chan->data_written = 0;

    
    chan->params->float_param = 500;
    chan->params->timer_num = 0;
    return 0;
}

int hm2_absenc_setup_biss(hostmot2_t *hm2, hm2_sserial_remote_t *chan, 
                         hm2_module_descriptor_t *md){
    
    if ( hm2_sserial_create_pins(hm2, chan)) return -EINVAL;
    
    chan->params = hal_malloc(sizeof(hm2_sserial_params_t));
    hm2->absenc.clock_frequency = md->clock_freq;
    hm2->absenc.biss_version = md->version;
    
    chan->reg_0_addr = md->base_address 
            + (0 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_cs_addr = md->base_address 
            + (1 * md->register_stride)
            + chan->index * md->instance_stride;
    hm2->absenc.biss_global_start_addr = md->base_address 
            + (2 * md->register_stride);
    chan->data_written = 0;
    
    chan->params->float_param = 500;
    chan->params->timer_num = 0;
    return 0;
}

int hm2_absenc_setup_fabs(hostmot2_t *hm2, hm2_sserial_remote_t *chan, 
                         hm2_module_descriptor_t *md){
    
    if ( hm2_sserial_create_pins(hm2, chan)) return -EINVAL;
    
    chan->params = hal_malloc(sizeof(hm2_sserial_params_t));
    hm2->absenc.clock_frequency = md->clock_freq;
    hm2->absenc.fanuc_version = md->version;

    chan->reg_0_addr = md->base_address 
            + (0 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_1_addr = md->base_address 
            + (1 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_2_addr = md->base_address 
            + (2 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->reg_cs_addr = md->base_address 
            + (3 * md->register_stride)
            + chan->index * md->instance_stride;
    chan->data_reg_addr = md->base_address 
            + (4 * md->register_stride)
            + chan->index * md->instance_stride;
    hm2->absenc.fabs_global_start_addr = md->base_address 
            + (5 * md->register_stride);
    chan->data_written = 0;

    if (hal_param_u32_newf(HAL_RW, &(chan->params->u32_param),
            hm2->llio->comp_id,"%s.filter",
            chan->name)){
        HM2_ERR("error adding param fanuc param 2, aborting\n");
        return -EINVAL;
    }
    chan->params->float_param = 1024.0;
    chan->params->u32_param = 0xF;
    chan->params->timer_num = 0;

    return 0;
}


int hm2_absenc_parse_format(hm2_sserial_remote_t *chan,  hm2_absenc_format_t *def){
    char* AA64 = "%5pbatt_fail%1b%2ppos_invalid%1b%9plow%16l%2pencoder%16h%2pcomm%10u%7pcrc%5u";
    char* format = def->string;
    char name[HM2_SSERIAL_MAX_STRING_LENGTH+1] = "";
    
    if (chan->myinst == HM2_GTAG_FABS && strncmp(format, "AA64",4) == 0){
        format = AA64;
    }
    
    while(*format){
        if (*format == '%'){
            int q = simple_strtol(++format, &format, 0);
            if (q == 0){
                HM2_ERR_NO_LL("Invalid field length specification, you may "
                        "not get the pins you expected\n");
            }
            else if (strchr("bBuUsSeEfFpPgGhHlL", *format)){
                hm2_sserial_data_t *conf;
                chan->num_confs++;
                chan->confs = (hm2_sserial_data_t *)krealloc(chan->confs,
                        chan->num_confs * sizeof(hm2_sserial_data_t),
                        GFP_KERNEL);

                conf = &(chan->confs[chan->num_confs - 1]);
                conf->DataDir = LBP_IN;
                conf->DataLength = q;
                strcpy(conf->NameString, name);
                conf->RecordType = 0xA0;
                conf->ParmAddr = 0;
                if (*format=='g' || *format=='G'){
                    strcpy(conf->UnitString, "gray");
                    format++;
                } else {
                    strcpy(conf->UnitString, "none");
                }
                switch(*format){
                case 'b':
                case 'B':
                    conf->DataType = LBP_BOOLEAN;
                    conf->ParmMax = 0;
                    conf->ParmMin = 0;
                    break;
                case 'u':
                case 'U':
                    conf->DataType = LBP_UNSIGNED;
                    conf->ParmMax = 1;
                    conf->ParmMin = 0;
                    break;
                case 's':
                case 'S':
                    conf->DataType = LBP_SIGNED;
                    conf->ParmMax = 1;
                    conf->ParmMin = -1;
                    break;
                case 'f':
                case 'F':
                    conf->DataType = LBP_BITS;
                    conf->ParmMax = 0;
                    conf->ParmMin = 0;
                    break;
                case 'p':
                case 'P':
                    conf->DataType = LBP_PAD;
                    conf->ParmMax = 0;
                    conf->ParmMin = 0;
                    break;
                case 'e':
                case 'E':
                    conf->DataType = LBP_ENCODER;
                    conf->ParmMax = 1;
                    conf->ParmMin = 0;
                    break;
                case 'h':
                case 'H':
                    conf->DataType = LBP_ENCODER_H;
                    conf->ParmMax = 1;
                    conf->ParmMin = 0;
                    break;
                case 'l':
                case 'L':
                    conf->DataType = LBP_ENCODER_L;
                    conf->ParmMax = 1;
                    conf->ParmMin = 0;
                    break;
                default:
                    HM2_ERR_NO_LL("The \"g\" format modifier must be paired "
                            "with one of the other data types\n");
                    return -EINVAL;
                }
                
            }
            else
            {
                HM2_ERR_NO_LL("Unknown format specifer %s\n", format);
                return -EINVAL;
            }
            //Start a new name
            strcpy(name, "");
            //move to the next string
            format++;
        }
        else
        {
            strncat(name, format++, 1);
        }
    }
    return 0;
}

int hm2_absenc_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    hm2_absenc_format_t *def = 0;
    struct list_head *ptr;
    int index;

    //
    // some standard sanity checks
    //
    switch (md->gtag){
    case HM2_GTAG_SSI:
        if ( ! hm2_md_is_consistent_or_complain(hm2, md_index, 0, 4, 4, 0x03)) {
            HM2_ERR("inconsistent absenc Module Descriptor!\n");
            return -EINVAL;
        }
        break;
    case HM2_GTAG_BISS:
        if ( ! hm2_md_is_consistent_or_complain(hm2, md_index, 0, 3, 4, 0x03)) {
            HM2_ERR("inconsistent absenc Module Descriptor!\n");
            return -EINVAL;
        }
        break;
    case HM2_GTAG_FABS:
        if ( ! hm2_md_is_consistent_or_complain(hm2, md_index, 0, 3, 4, 0x1F)) {
            HM2_ERR("inconsistent absenc Module Descriptor!\n");
            return -EINVAL;
        }
        break;
    }

    //
    // looks good (so far), start initializing
    //

    if (hm2->absenc.num_chans) { // first time though
        hm2->absenc.clock_frequency = md->clock_freq;
        hm2->absenc.ssi_busy_flags = kmalloc(sizeof(u32), GFP_KERNEL);
        *hm2->absenc.ssi_busy_flags = 0;
        hm2->absenc.biss_busy_flags = kmalloc(sizeof(u32), GFP_KERNEL);
        *hm2->absenc.biss_busy_flags = 0;
        hm2->absenc.fabs_busy_flags = kmalloc(sizeof(u32), GFP_KERNEL);
        *hm2->absenc.fabs_busy_flags = 0;
    }
    
    for (index = 0 ; index < md->instances ; index ++){
        bool has_format = false;
        hm2_sserial_remote_t *chan;
        list_for_each(ptr, &hm2->config.absenc_formats){
            def = list_entry(ptr, hm2_absenc_format_t, list);
       
            if (def->index > md->instances && def->gtag == md->gtag){
                HM2_ERR("You have defined a configuration string for %s number"
                        " %i but only %i %s available, exiting.\n",
                        hm2_get_general_function_name(md->gtag), def->index,
                        md->instances, md->instances > 1 ? "are" : "is");
                goto fail1;
            }
            if (index == def->index && md->gtag == def->gtag){
                has_format = true;
                hm2->absenc.num_chans += 1;
                hm2->absenc.chans = krealloc(hm2->absenc.chans,
                        hm2->absenc.num_chans * sizeof(hm2_sserial_remote_t),
                        GFP_KERNEL);
                chan = &hm2->absenc.chans[hm2->absenc.num_chans - 1];
                memset(chan, 0, sizeof(hm2_sserial_remote_t));
                chan->index = index;
                chan->myinst = md->gtag;
                
                if (hm2_absenc_parse_format(chan, def) ) goto fail1;

                switch (md->gtag){
                case HM2_GTAG_SSI:
                    rtapi_snprintf(chan->name, sizeof(chan->name),
                            "%s.ssi.%02d", hm2->llio->name, index);
                    if (hm2_absenc_setup_ssi(hm2, chan, md)) goto fail1;
                    break;
                case HM2_GTAG_BISS:
                    rtapi_snprintf(chan->name, sizeof(chan->name),
                            "%s.biss.%02d", hm2->llio->name, index);
                    if (hm2_absenc_setup_biss(hm2, chan, md)) goto fail1;
                    break;
                case HM2_GTAG_FABS:
                    rtapi_snprintf(chan->name, sizeof(chan->name),
                            "%s.fanuc.%02d", hm2->llio->name, index);
                    if (hm2_absenc_setup_fabs(hm2, chan, md)) goto fail1;
                    break;
                default:
                    HM2_ERR("Unsupported GTAG passed to hm2_absenc driver\n");
                }
                
                // Set up the common pins
                if (hal_pin_bit_newf(HAL_OUT, &(chan->params->error),
                        hm2->llio->comp_id,"%s.data-invalid",
                        chan->name)){
                    HM2_ERR("error adding %s over-run pin, aborting\n", 
                            chan->name);
                    return -EINVAL;
                }
                // And Params
                if (hal_param_float_newf(HAL_RW, &(chan->params->float_param),
                        hm2->llio->comp_id,"%s.frequency-khz",
                        chan->name)){
                    HM2_ERR("error adding frequency param for %s, aborting\n",
                            chan->name);
                    return -EINVAL;
                }
                if (hal_param_u32_newf(HAL_RW, &(chan->params->timer_num),
                        hm2->llio->comp_id,"%s.timer-number",
                        chan->name)){
                    HM2_ERR("error adding %s timer number param, aborting\n", 
                            chan->name);
                    return -EINVAL;
                }
            }
        }
        // pins.c can't handle non-contiguous instances, so set unused pins to 
        // GPIO here
        if (has_format == false){
            int pin;
            for (pin = 0 ; pin < hm2->num_pins ; pin++){
                if (hm2->pin[pin].sec_tag == md->gtag
                        && hm2->pin[pin].sec_unit == index){
                    hm2->pin[pin].sec_tag = 0;
                }
            }
        }
    }

    return hm2->absenc.num_chans;

    fail1:
    hm2_absenc_cleanup(hm2);
    hm2->absenc.num_chans = 0;
    return -EINVAL;
}

void hm2_absenc_process_tram_read(hostmot2_t *hm2, long period) {
    int i;
    static int err_count[MAX_ABSENCS];
    static int err_tag[MAX_ABSENCS];
    if (hm2->absenc.num_chans <= 0) return;

    // process each absenc instance independently

    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        int err_flag = 0;

        // This actually populates all the pins. 
        hm2_sserial_read_pins(chan);

        if ((chan->myinst == HM2_GTAG_FABS) 
                &&(*chan->reg_2_read & 0x80000000)){

            err_flag = 1;
            
            if(err_count[i] > 5000 && err_tag[i] == 0){
                HM2_ERR("Fanuc encoder channel %s cable fault\n"
                        "this warning will not repeat\n", chan->name);
                err_tag[i] = 1;
            }    
        } 
        if (((chan->myinst == HM2_GTAG_SSI)
                && (*hm2->absenc.ssi_busy_flags & (1 << chan->index)))
                || ((chan->myinst == HM2_GTAG_BISS)
                && (*hm2->absenc.biss_busy_flags & (1 << chan->index)))
                || ((chan->myinst == HM2_GTAG_FABS)
                && (*hm2->absenc.fabs_busy_flags & (1 << chan->index)))){
            
            err_flag = 1;
            
            if (err_count[i] > 5000 && err_tag[i] == 0){
                HM2_ERR("Data transmission not complete on channel %s read."
                        " You  may need to change the timing of %s. This "
                        "warning  will not repeat\n",  chan->name,
                        (chan->params->timer_num == 0) ? 
                                "the trigger function" : "the hm2dpll timer");
                err_tag[i] = 1;
            }
        } 
        
        // a bit of hysteresis (50 cycles) on the error bit and a long wait
        // before reporting a fault (5000 cycles)
        if (err_flag){
            if (err_count[i] < 5001) {
                ++err_count[i];
            } else {
                *chan->params->error = 1;
            }
                
        } else {
            if (err_count[i] > 4950){
                --err_count[i];
            } else {
                *chan->params->error = 0;
            }
        }
    }
}


void hm2_absenc_write(hostmot2_t *hm2){

    int i;
    u32 buff, buff2, buff3;
    if (hm2->absenc.num_chans <= 0) return;

    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        switch (chan->myinst){
        case HM2_GTAG_SSI:
            if (chan->params->timer_num > 4) chan->params->timer_num = 4;
            buff = ((u32)(0x10000 * (chan->params->float_param * 1000
                    / hm2->absenc.clock_frequency))) << 16
                    | chan->params->timer_num << 12
                    | (chan->params->timer_num == 0) << 8
                    | (chan->params->timer_num != 0) << 9
                    | chan->num_read_bits;
            if (buff != chan->data_written){
                hm2->llio->write(hm2->llio,
                        chan->reg_cs_addr,
                        &buff,
                        sizeof(u32));
                chan->data_written = buff;
            }
            break;
        case HM2_GTAG_BISS:
            if (chan->params->timer_num > 4) chan->params->timer_num = 4;
            buff = ((u32)(0x10000 * (chan->params->float_param * 1000
                    / hm2->absenc.clock_frequency))) << 16
                    | chan->params->timer_num << 12
                    | (chan->params->timer_num == 0) << 10
                    | (chan->params->timer_num != 0) << 11
                    | chan->num_read_bits;
            if (buff != chan->data_written){
                hm2->llio->write(hm2->llio,
                        chan->reg_cs_addr,
                        &buff,
                        sizeof(u32));
                chan->data_written = buff;
            }
            break;
        case HM2_GTAG_FABS:
            if (chan->params->timer_num > 4) chan->params->timer_num = 4;
            if (chan->params->u32_param > 15) chan->params->u32_param = 15;
            buff3 = chan->num_read_bits << 24
                    | (u32)(8.0e-6 * hm2->absenc.clock_frequency) << 14;
            buff2 = chan->params->u32_param << 28
                    | ((u32)(0x100000 * (chan->params->float_param * 1000
                    / hm2->absenc.clock_frequency)));
            buff =  chan->params->timer_num << 12
                    | (chan->params->timer_num == 0) << 8
                    | (chan->params->timer_num != 0) << 9
                    | (buff3 != chan->data3_written || buff2 != chan->data2_written) << 7;
            if (buff != chan->data_written){
                // if necessary this will set the write flag, then next time through
                // it will get cleared. 
                hm2->llio->write(hm2->llio,
                        chan->reg_cs_addr,
                        &buff,
                        sizeof(u32));
                chan->data_written = buff;
            }
            if (buff2 != chan->data2_written){
                hm2->llio->write(hm2->llio,
                        chan->data_reg_addr,
                        &buff,
                        sizeof(u32));
                chan->data2_written = buff2;
            }
            if (buff3 != chan->data3_written){
                hm2->llio->write(hm2->llio,
                        chan->reg_2_addr,
                        &buff,
                        sizeof(u32));
                chan->data3_written = buff3;
            }
            break;
        default:
            HM2_ERR("hm2_absenc_write called with unsupported type (%i)\n", 
                    chan->myinst);
        }
    }
}

void hm2_absenc_cleanup(hostmot2_t *hm2) {
    int i;
    if (hm2->absenc.num_chans > 0) {
        for (i = 0 ; i < hm2->absenc.num_chans ; i++){
            if (hm2->absenc.chans[i].confs != NULL){
                kfree(hm2->absenc.chans[i].confs);
            }
        }
        kfree(hm2->absenc.chans);
    }
}


void hm2_absenc_print_module(hostmot2_t *hm2) {
    int i;
    HM2_PRINT("Absolute Encoder (Generic): %d\n", hm2->absenc.num_chans);
    if (hm2->absenc.num_chans <= 0) return;
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n",
            hm2->absenc.clock_frequency,
            hm2_hz_to_mhz(hm2->absenc.clock_frequency));
    HM2_PRINT("    ssi-version: %d\n", hm2->absenc.ssi_version);
    HM2_PRINT("    ssi global-start: 0x%04X\n", hm2->absenc.ssi_global_start_addr);
    HM2_PRINT("    biss-version: %d\n", hm2->absenc.biss_version);
    HM2_PRINT("    fanuc-version: %d\n", hm2->absenc.fanuc_version);
    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        hw:\n");
        HM2_PRINT("    command_addr: 0x%04X\n", hm2->absenc.chans[i].reg_cs_addr);
        HM2_PRINT("    data 0 addr: 0x%04X\n", hm2->absenc.chans[i].reg_0_addr);
        HM2_PRINT("    data 1 addr: 0x%04X\n", hm2->absenc.chans[i].reg_1_addr);
        HM2_PRINT("    data 2 addr: 0x%04X\n", hm2->absenc.chans[i].reg_2_addr);
    }
}
