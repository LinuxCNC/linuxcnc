

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


int hm2_absenc_parse_md(hostmot2_t *hm2, int md_index,
        char all_formats[][MAX_ABSENC_LEN]) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    static int last_index = -1;
    static int last_format = 0;
    static int funct_flag = 0;
    int i, r = 0,  f = 0;
    int index;

    rtapi_print("\nlast_index = %i\n", last_index);

    //
    // some standard sanity checks
    //
    if ( ! hm2_md_is_consistent_or_complain(hm2, md_index, 0, 4, 4, 0x03)) {
        HM2_ERR("inconsistent absenc Module Descriptor!\n");
        return -EINVAL;
    }
    /* This module is intended to handle more than just SSI, so re-entering is OK
    */
    for(i = 0; i < MAX_ABSENCS; i++){
        if (all_formats[i][0]){
            if ( i >= md->instances) {
                HM2_ERR( "config.num_absenc=%d, but only %d are available, not"
                        " loading driver\n",
                        hm2->config.num_absencs, md->instances );
                return -EINVAL;
            }
            hm2->absenc.num_chans = i + 1;
        }
    }
    if (hm2->absenc.num_chans == 0) {
        return 0;
    }
    
    //
    // looks good, start initializing
    //

    hm2->absenc.clock_frequency = md->clock_freq;
    index = last_index;

    for (f = 0; f < MAX_ABSENCS; f++){
        char name[HM2_SSERIAL_MAX_STRING_LENGTH+1] = "";
        char* format = all_formats[f];
        hm2_sserial_remote_t *chan;
        int bitcount = 0;
        if (*format){
            index++;
            hm2->absenc.chans = (hm2_sserial_remote_t *)krealloc(hm2->absenc.chans,
                    (index + 1) * sizeof(hm2_sserial_remote_t),
                    GFP_KERNEL);
            chan = &hm2->absenc.chans[index];
            chan->confs = NULL ; chan->globals = NULL; chan->modes = NULL;
            chan->num_confs = 0;
            while(*format){
                if (*format == '%' && *name){
                    int q = simple_strtol(++format, &format, 0);
                    bitcount += q;
                    if (q == 0){
                        HM2_ERR("Invalid field length specification, you may "
                                "not get the pins you expected\n");
                    }
                    else if (strchr("bBuUsSeEfFpPgG", *format)){
                        hm2_sserial_data_t *conf;
                        chan->num_confs++;
                        chan->confs = (hm2_sserial_data_t *)
                                        krealloc(chan->confs,
                                                chan->num_confs * sizeof(hm2_sserial_data_t),
                                                GFP_KERNEL);

                        conf = &(chan->confs[chan->num_confs - 1]);
                        if (conf == NULL){ HM2_ERR("Oh dash and blast it\n") ; return -1;}
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
                        HM2_ERR("Unknown format specifer %s\n", format);
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

            switch (md->gtag){
            case HM2_GTAG_SSI:
                rtapi_snprintf(chan->name, sizeof(chan->name),
                               "%s.ssi.%02d", hm2->llio->name, f);
                if ( hm2_sserial_create_pins(hm2, chan)) goto fail1;
                chan->params = hal_malloc(sizeof(hm2_sserial_params_t));
                hm2->absenc.clock_frequency = md->clock_freq;
                hm2->absenc.ssi_version = md->version;

                chan->reg_0_addr = md->base_address + (0 * md->register_stride);
                chan->reg_1_addr = md->base_address + (1 * md->register_stride);
                chan->reg_cs_addr = md->base_address + (2 * md->register_stride);
                chan->command_reg_addr = 0; // Just used as a handy 32-bit buffer
                if (hm2->absenc.ssi_global_start_addr == 0){
                    hm2->absenc.ssi_global_start_addr = md->base_address + (3 * md->register_stride);
                }

                r = hm2_register_tram_read_region(hm2, chan->reg_cs_addr,
                        sizeof(u32),
                        &chan->reg_cs_read); // To check "busy"
                r += hm2_register_tram_read_region(hm2, chan->reg_0_addr,
                        sizeof(u32),
                        &chan->reg_0_read);
                r += hm2_register_tram_read_region(hm2, chan->reg_1_addr,
                        sizeof(u32),
                        &chan->reg_1_read);
                if (r < 0) {
                    HM2_ERR("error registering tram read region for SSI encoder\n");
                    goto fail1;
                }

                rtapi_snprintf(name, sizeof(name), "%s.ssi.%02d.frequency-khz",
                        hm2->llio->name, f);
                r = hal_param_float_new(name, HAL_RW,
                        &(chan->params->float_param ),
                        hm2->llio->comp_id);
                if (r < 0) {
                    HM2_ERR("error adding param '%s', aborting\n", name);
                    goto fail1;
                }

                // export the absenc to HAL
                //Unlike most other Hostmot2 functions, this one needs to have a private
                // "trigger" function directly callable from HAL.
                if (! (funct_flag & 0x01)){
                    rtapi_snprintf(name, sizeof(name),
                            "%s.trigger-ssi-encoders", hm2->llio->name);
                    hal_export_funct(name, hm2_absenc_trigger,
                            hm2, 0, 0,hm2->llio->comp_id);
                    funct_flag |= 0x01;
                }

                break;
            default:
                HM2_ERR("We don't handle BISS or Fanuc yet, sorry\n");
            }
        }
    }
    //Store the indexes for re-entry to handle other encoder styles
    //On balance I think we need one set of formats per encoder style
    //just because of how the mds are sent through.

    last_index = index;
    last_format = f;

    return hm2->absenc.num_chans;

    fail1:
    hm2_absenc_cleanup(hm2);

    hm2->absenc.num_chans = 0;
    return r;
}


void hm2_absenc_process_tram_read(hostmot2_t *hm2, long period) {
    int i;

    static int err_count = 0;
    if (hm2->absenc.num_chans <= 0) return;

    // process each absenc instance independently
    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
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
    }
}

void hm2_absenc_write(hostmot2_t *hm2){

    int i;
    u32 buff;
    if (hm2->absenc.num_chans <= 0) return;
    
    for (i = 0; i < hm2->absenc.num_chans; i ++) {
        for (i = 0; i < hm2->absenc.num_chans; i ++) {
            hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
            buff = ((u32)(0x10000 * (chan->params->float_param * 1000
                    / hm2->absenc.clock_frequency))) << 16
                            | 0x100 //enable global read
                            | chan->num_read_bits;
            //This is NOT the command reg address, it is a re-purposed u32
            if (buff != chan->command_reg_addr ){
                hm2->llio->write(hm2->llio,
                           chan->reg_cs_addr,
                           &buff,
                           sizeof(u32));
                chan->command_reg_addr = buff;
            }
        }
    }
}

void hm2_absenc_cleanup(hostmot2_t *hm2) {
    int i;
//    rtapi_print("FIXME: !! Exiting without cleanup! \n");
//    return;
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
    HM2_PRINT("absenc: %d\n", hm2->absenc.num_chans);
    if (hm2->absenc.num_chans <= 0) return;
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n",
            hm2->absenc.clock_frequency,
            hm2_hz_to_mhz(hm2->absenc.clock_frequency));
    HM2_PRINT("    ssi-version: %d\n", hm2->absenc.ssi_version);
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
