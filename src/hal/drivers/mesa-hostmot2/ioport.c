
//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
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

#include <linux/slab.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




int hm2_ioport_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int i, r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent(hm2, md_index, 0, 5, 4, 0x001F)) {
        ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->ioport.num_instances != 0) {
        ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }


    // 
    // special sanity check for io_ports
    // 

    if (hm2->idrom.io_ports != md->instances) {
        ERR(
            "IDROM IOPorts is %d but MD IOPort NumInstances is %d, inconsistent firmware, aborting driver load\n",
            hm2->idrom.io_ports,
            md->instances
        );
        return -EINVAL;
    }

    hm2->ioport.num_instances = md->instances;


    hm2->ioport.clock_frequency = md->clock_freq;
    hm2->ioport.version = md->version;

    hm2->ioport.data_addr = md->base_address + (0 * md->register_stride);
    hm2->ioport.ddr_addr = md->base_address + (1 * md->register_stride);
    hm2->ioport.alt_source_addr = md->base_address + (2 * md->register_stride);
    hm2->ioport.open_drain_addr = md->base_address + (3 * md->register_stride);
    hm2->ioport.output_invert_addr = md->base_address + (4 * md->register_stride);

    r = hm2_register_tram_read_region(hm2, hm2->ioport.data_addr, (hm2->ioport.num_instances * sizeof(u32)), &hm2->ioport.data_read_reg);
    if (r < 0) {
        ERR("error registering tram read region for IOPort Data register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->ioport.data_addr, (hm2->ioport.num_instances * sizeof(u32)), &hm2->ioport.data_write_reg);
    if (r < 0) {
        ERR("error registering tram write region for IOPort Data register (%d)\n", r);
        goto fail0;
    }

    hm2->ioport.ddr_reg = (u32 *)kmalloc(hm2->ioport.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->ioport.ddr_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    // this one's not a real register
    hm2->ioport.written_ddr = (u32 *)kmalloc(hm2->ioport.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->ioport.written_ddr == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }

    hm2->ioport.alt_source_reg = (u32 *)kmalloc(hm2->ioport.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->ioport.alt_source_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail2;
    }

    hm2->ioport.open_drain_reg = (u32 *)kmalloc(hm2->ioport.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->ioport.open_drain_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail3;
    }

    hm2->ioport.output_invert_reg = (u32 *)kmalloc(hm2->ioport.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->ioport.output_invert_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail4;
    }


    //
    // initialize to all gpios, all inputs, not open drain, not output-inverted
    //

    for (i = 0; i < hm2->ioport.num_instances; i ++) {
        hm2->ioport.ddr_reg[i] = 0;           // all are inputs
        hm2->ioport.written_ddr[i] = 0;       // we're starting out in sync
        hm2->ioport.alt_source_reg[i] = 0;    // they're all gpios
        hm2->ioport.open_drain_reg[i] = 0;    // none are open drain
        hm2->ioport.output_invert_reg[i] = 0; // none are output-inverted
    }

    // we can't export this one to HAL yet, because some pins may be allocated to other modules

    return hm2->ioport.num_instances;


fail4:
    kfree(hm2->ioport.open_drain_reg);

fail3:
    kfree(hm2->ioport.alt_source_reg);

fail2:
    kfree(hm2->ioport.written_ddr);

fail1:
    kfree(hm2->ioport.ddr_reg);

fail0:
    hm2->ioport.num_instances = 0;
    return r;
}




void hm2_ioport_cleanup(hostmot2_t *hm2) {
    if (hm2->ioport.num_instances <= 0) return;
    if (hm2->ioport.ddr_reg != NULL) kfree(hm2->ioport.ddr_reg);
    if (hm2->ioport.written_ddr != NULL) kfree(hm2->ioport.written_ddr);
    if (hm2->ioport.alt_source_reg != NULL) kfree(hm2->ioport.alt_source_reg);
    if (hm2->ioport.open_drain_reg != NULL) kfree(hm2->ioport.open_drain_reg);
    if (hm2->ioport.output_invert_reg != NULL) kfree(hm2->ioport.output_invert_reg);
}




int hm2_ioport_gpio_export_hal(hostmot2_t *hm2) {
    int r;
    int i;

    for (i = 0; i < hm2->num_pins; i ++) {
        int port = i / hm2->idrom.port_width;

        if (hm2->pin[i].gtag != HM2_GTAG_IOPORT) {
            continue;
        }

        hm2->pin[i].instance = (hm2_gpio_instance_t *)hal_malloc(sizeof(hm2_gpio_instance_t));
        if (hm2->pin[i].instance == NULL) {
            ERR("out of memory!\n");
            return -ENOMEM;
        }

        // pins
        r = hal_pin_bit_newf(
            HAL_OUT,
            &(hm2->pin[i].instance->hal.pin.in),
            hm2->llio->comp_id,
            "%s.gpio.%s.%03d.in",
            hm2->llio->name,
            hm2->llio->ioport_connector_name[port],
            i
        );
        if (r != HAL_SUCCESS) {
            ERR("error %d adding gpio pin, aborting\n", r);
            return -EINVAL;
        }

        r = hal_pin_bit_newf(
            HAL_OUT,
            &(hm2->pin[i].instance->hal.pin.in_not),
            hm2->llio->comp_id,
            "%s.gpio.%s.%03d.in_not",
            hm2->llio->name,
            hm2->llio->ioport_connector_name[port],
            i
        );
        if (r != HAL_SUCCESS) {
            ERR("error %d adding gpio pin, aborting\n", r);
            return -EINVAL;
        }

        r = hal_pin_bit_newf(
            HAL_IN,
            &(hm2->pin[i].instance->hal.pin.out),
            hm2->llio->comp_id,
            "%s.gpio.%s.%03d.out",
            hm2->llio->name,
            hm2->llio->ioport_connector_name[port],
            i
        );
        if (r != HAL_SUCCESS) {
            ERR("error %d adding gpio pin, aborting\n", r);
            return -EINVAL;
        }

        // parameters
        r = hal_param_bit_newf(
            HAL_RW,
            &(hm2->pin[i].instance->hal.param.is_output),
            hm2->llio->comp_id,
            "%s.gpio.%s.%03d.is_output",
            hm2->llio->name,
            hm2->llio->ioport_connector_name[port],
            i
        );
        if (r != HAL_SUCCESS) {
            ERR("error %d adding gpio param, aborting\n", r);
            return -EINVAL;
        }

        r = hal_param_bit_newf(
            HAL_RW,
            &(hm2->pin[i].instance->hal.param.invert_output),
            hm2->llio->comp_id,
            "%s.gpio.%s.%03d.invert_output",
            hm2->llio->name,
            hm2->llio->ioport_connector_name[port],
            i
        );
        if (r != HAL_SUCCESS) {
            ERR("error %d adding gpio param, aborting\n", r);
            return -EINVAL;
        }


        // initialize
        hm2_set_pin_direction(hm2, i, HM2_PIN_DIR_IS_INPUT);
        *(hm2->pin[i].instance->hal.pin.out) = 0;
        hm2->pin[i].instance->hal.param.is_output = 0;
        hm2->pin[i].instance->hal.param.invert_output = 0;
    }

    return 0;
}




void hm2_ioport_print_module(int msg_level, hostmot2_t *hm2) {
    int i;
    PRINT(msg_level, "IO Ports: %d\n", hm2->ioport.num_instances);
    if (hm2->ioport.num_instances <= 0) return;
    PRINT(msg_level, "    clock_frequency: %d Hz (%s MHz)\n", hm2->ioport.clock_frequency, hm2_hz_to_mhz(hm2->ioport.clock_frequency));
    PRINT(msg_level, "    version: %d\n", hm2->ioport.version);
    PRINT(msg_level, "    data_addr: 0x%04X\n", hm2->ioport.data_addr);
    PRINT(msg_level, "    ddr_addr: 0x%04X\n", hm2->ioport.ddr_addr);
    PRINT(msg_level, "    alt_source_addr: 0x%04X\n", hm2->ioport.alt_source_addr);
    PRINT(msg_level, "    open_drain_addr: 0x%04X\n", hm2->ioport.open_drain_addr);
    PRINT(msg_level, "    output_invert_addr: 0x%04X\n", hm2->ioport.output_invert_addr);
    for (i = 0; i < hm2->ioport.num_instances; i ++) {
        PRINT(msg_level, "    instance %d:\n", i);
        PRINT(msg_level, "        data_read = 0x%06X\n", hm2->ioport.data_read_reg[i]);
        PRINT(msg_level, "        data_write = 0x%06X\n", hm2->ioport.data_write_reg[i]);
        PRINT(msg_level, "        ddr = 0x%06X\n", hm2->ioport.ddr_reg[i]);
        PRINT(msg_level, "        alt_source = 0x%06X\n", hm2->ioport.alt_source_reg[i]);
        PRINT(msg_level, "        open_drain = 0x%06X\n", hm2->ioport.open_drain_reg[i]);
        PRINT(msg_level, "        output_invert = 0x%06X\n", hm2->ioport.output_invert_reg[i]);
    }
}




static void hm2_ioport_force_write_ddr(hostmot2_t *hm2) {
    int size = hm2->ioport.num_instances * sizeof(u32);
    hm2->llio->write(hm2->llio, hm2->ioport.ddr_addr, hm2->ioport.ddr_reg, size);
    memcpy(hm2->ioport.written_ddr, hm2->ioport.ddr_reg, size);
}


void hm2_ioport_force_write(hostmot2_t *hm2) {
    int size = hm2->ioport.num_instances * sizeof(u32);

    hm2_ioport_force_write_ddr(hm2);
    hm2->llio->write(hm2->llio, hm2->ioport.alt_source_addr,    hm2->ioport.alt_source_reg,    size);
    hm2->llio->write(hm2->llio, hm2->ioport.open_drain_addr,    hm2->ioport.open_drain_reg,    size);
    hm2->llio->write(hm2->llio, hm2->ioport.output_invert_addr, hm2->ioport.output_invert_reg, size);
}


void hm2_ioport_write(hostmot2_t *hm2) {
    int port;
    int port_pin;

    int io_pin;

    int need_update_ddr = 0;


    //
    // update ioport variables
    //

    for (port = 0; port < hm2->ioport.num_instances; port ++) {
        for (port_pin = 0; port_pin < hm2->idrom.port_width; port_pin ++) {
            io_pin = (port * hm2->idrom.port_width) + port_pin;
            if (hm2->pin[io_pin].gtag != HM2_GTAG_IOPORT) continue;

            if (hm2->pin[io_pin].instance->hal.param.is_output) {
                hm2->ioport.ddr_reg[port] |= (1 << port_pin);  // set the bit in the ddr register
            } else {
                hm2->ioport.ddr_reg[port] &= ~(1 << port_pin);  // clear the bit in the ddr register
            }
        }
        if (hm2->ioport.written_ddr[port] != hm2->ioport.ddr_reg[port]) {
            need_update_ddr = 1;
        }
    }

    if (need_update_ddr) {
        hm2_ioport_force_write_ddr(hm2);
    }
}




//
// initialize the tram write registers
//

void hm2_ioport_gpio_tram_write_init(hostmot2_t *hm2) {
    int port;
    for (port = 0; port < hm2->ioport.num_instances; port ++) {
        hm2->ioport.data_write_reg[port] = 0;
    }
}




//
// the ioport.data_read buffer has been updated by a TRAM read from the values in the ioport data register
// this function sets the HAL pins based on the values in the data_read register buffer
//

void hm2_ioport_gpio_process_tram_read(hostmot2_t *hm2) {
    int port;
    int port_pin;

    // 
    // parse it out to the HAL pins
    //

    for (port = 0; port < hm2->ioport.num_instances; port ++) {
        for (port_pin = 0; port_pin < hm2->idrom.port_width; port_pin ++) {
            int io_pin = (port * hm2->idrom.port_width) + port_pin;
            hal_bit_t bit;

            if (hm2->pin[io_pin].gtag != HM2_GTAG_IOPORT) continue;
            if (hm2->pin[io_pin].instance->hal.param.is_output) continue;

            bit = (hm2->ioport.data_read_reg[port] >> port_pin) & 0x1;
            *hm2->pin[io_pin].instance->hal.pin.in = bit;
            *hm2->pin[io_pin].instance->hal.pin.in_not = !bit;
        }
    }
}




//
// this function sets the data_write register TRAM buffer from the values of the HAL pins
// the data_write buffer will get written to the TRAM and thus to the ioport data register by the caller
// 

void hm2_ioport_gpio_prepare_tram_write(hostmot2_t *hm2) {
    int port;
    int port_pin;

    //
    // copy HAL pins to HM2 pins
    //

    for (port = 0; port < hm2->ioport.num_instances; port ++) {
        for (port_pin = 0; port_pin < hm2->idrom.port_width; port_pin ++) {
            int io_pin = (port * hm2->idrom.port_width) + port_pin;
            hal_bit_t out;

            if (hm2->pin[io_pin].gtag != HM2_GTAG_IOPORT) continue;
            if (!hm2->pin[io_pin].instance->hal.param.is_output) continue;

            out = *(hm2->pin[io_pin].instance->hal.pin.out) ^ hm2->pin[io_pin].instance->hal.param.invert_output;
            hm2->ioport.data_write_reg[port] &= ~(1 << port_pin);   // zero the bit
            hm2->ioport.data_write_reg[port] |= (out << port_pin);  // and set it as appropriate
        }
    }
}

