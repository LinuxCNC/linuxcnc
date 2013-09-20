

//
//  Driver for the Mesa SSI Encoder module.
//  It is expected that it will be expanded to cover BISS and Fanuc absolute
//  encoders in the future.
//


#include <linux/slab.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "rtapi_math64.h"
#include "hal.h"
#include "hal/drivers/mesa-hostmot2/hostmot2.h"


static void hm2_absenc_trigger(void *void_hm2, long period){
    hostmot2_t *hm2 = void_hm2;
    u32 buff = 0xFFFFFFFF;
    hm2->llio->write(hm2->llio,
            hm2->absenc.ssi_global_start_addr,
            &buff,
            sizeof(u32));
}

int hm2_absenc_register_tram(hostmot2_t *hm2){
    int i;
    
    for (i = 0; i < hm2->absenc.num_chans; i++){
        int r = 0;
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        
        if (chan->reg_cs_addr){
            r = hm2_register_tram_read_region(hm2, chan->reg_cs_addr,
                    sizeof(u32),
                    &chan->reg_cs_read);
        }
        if (chan->reg_0_addr){
            r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                    sizeof(u32),
                    &chan->reg_0_read);
        }
        if (chan->reg_1_addr){
            r += hm2_register_tram_read_region(hm2, chan->reg_1_addr,
                    sizeof(u32),
                    &chan->reg_1_read);
        }
        if (chan->reg_2_addr){
            r += hm2_register_tram_read_region(hm2, chan->reg_2_addr,
                    sizeof(u32),
                    &chan->reg_2_read);
        }

        if (r < 0) {
            HM2_ERR("error registering tram read region for SSI encoder\n");
            return -EINVAL;
        }
    }
    return 0;
}

int hm2_absenc_setup_ssi(hostmot2_t *hm2, hm2_sserial_remote_t *chan, 
                         hm2_module_descriptor_t *md){
    
    char name[HM2_SSERIAL_MAX_STRING_LENGTH+1] = "";
    static bool funct_flag = false;

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
    chan->data_written = 0;
    if (hm2->absenc.ssi_global_start_addr == 0){
        hm2->absenc.ssi_global_start_addr = md->base_address 
                + (3 * md->register_stride);
    }

    rtapi_snprintf(name, sizeof(name), "%s.ssi.%02d.frequency-khz",
            hm2->llio->name, chan->index);
    if (hal_param_float_new(name, HAL_RW,
            &(chan->params->float_param ),
            hm2->llio->comp_id)){
        HM2_ERR("error adding param '%s', aborting\n", name);
        return -EINVAL;
    }

    // export the absenc to HAL
    //Unlike most other Hostmot2 functions, this one needs to have a private
    // "trigger" function directly callable from HAL.
    if (! funct_flag){
        rtapi_snprintf(name, sizeof(name),
                "%s.trigger-ssi-encoders", hm2->llio->name);
        hal_export_funct(name, hm2_absenc_trigger,
                hm2, 0, 0,hm2->llio->comp_id);
        funct_flag = true;
    }
    return 0;
}

int hm2_absenc_parse_format(hm2_sserial_remote_t *chan,  hm2_absenc_format_t *def){
    char* format = def->string;
    char name[HM2_SSERIAL_MAX_STRING_LENGTH+1] = "";
    
    while(*format){
        if (*format == '%' && *name){
            int q = simple_strtol(++format, &format, 0);
            if (q == 0){
                HM2_ERR_NO_LL("Invalid field length specification, you may "
                        "not get the pins you expected\n");
            }
            else if (strchr("bBuUsSeEfFpPgG", *format)){
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
                strcpy(conf->UnitString, "none");

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
                case 'g':
                case 'G':
                    strcpy(conf->UnitString, "gray");
                    /* no break */
                case 'e':
                case 'E':
                    conf->DataType = LBP_ENCODER;
                    conf->ParmMax = 1;
                    conf->ParmMin = 0;
                    break;
                }
            }
            else
            {
                HM2_ERR_NO_LL("Unknown format specifer %s\n", format);
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
    hm2->absenc.clock_frequency = md->clock_freq;

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
                    //if (hm2_absenc_setup_biss(hm2, chan, md)) goto fail1;
                    break;
                case HM2_GTAG_FABS:
                    rtapi_snprintf(chan->name, sizeof(chan->name),
                            "%s.fabs.%02d", hm2->llio->name, index);
                    //if (hm2_absenc_setup_fabs(hm2, chan, md)) goto fail1;
                    break;
                default:
                    HM2_ERR("Unsupported GTAG passed to hm2_absenc driver\n");
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

    // FABS is last in the list, so now we can set up the TRAM
    // with no danger of stuff moving due to krealloc
    if (md->gtag == HM2_GTAG_FABS){
        if (hm2_absenc_register_tram(hm2)) goto fail1;
    }

    return hm2->absenc.num_chans;

    fail1:
    hm2_absenc_cleanup(hm2);

    hm2->absenc.num_chans = 0;
    return -EINVAL;
}

void hm2_absenc_process_tram_read(hostmot2_t *hm2, long period) {
    int i;

    static int err_count = 0;
    if (hm2->absenc.num_chans <= 0) return;

    // process each absenc instance independently
    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        switch (chan->myinst){
        case HM2_GTAG_SSI:
            // Check that the data is all in.
            if ((*chan->reg_cs_read & 0x80000000) && err_count < 10){
                err_count++;
                HM2_ERR("Data transmission not complete before encoder position \n"
                        "read You may need to place the absenc_trigger function \n"
                        "earlier in the thread (warning %i of 10)\n", err_count);
            }
            else
            {
                hm2_sserial_read_pins(chan);
            }
            break;
        case HM2_GTAG_BISS:
            break;
        case HM2_GTAG_FABS:
            break;
        default:
            HM2_ERR("hm2_absenc_read called with unsupported type (%i)\n", 
                    chan->myinst);
        }
    }
}

void hm2_absenc_write(hostmot2_t *hm2){

    int i;
    u32 buff;
    if (hm2->absenc.num_chans <= 0) return;

    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        switch (chan->myinst){
        case HM2_GTAG_SSI:
            buff = ((u32)(0x10000 * (chan->params->float_param * 1000
                    / hm2->absenc.clock_frequency))) << 16
                    | 0x100 //enable global read
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
            break;
        case HM2_GTAG_FABS:
            break;
        default:
            HM2_ERR("hm2_absenc_write called with unsupported type (%i)\n", 
                    chan->myinst);
        }
    }
}

void hm2_absenc_cleanup(hostmot2_t *hm2) {
    int i;

    if (hm2->absenc.chans != NULL) {
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
