
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




int debug_pin_descriptors = 0;
RTAPI_MP_INT(debug_pin_descriptors, "Developer/debug use only!  Enable debug logging of the HostMot2\nPin Descriptors.");




// FIXME: the static automatic string makes this function non-reentrant
static const char* hm2_get_pin_secondary_name(hm2_pin_t *pin) {
    static char unknown[100];
    int sec_pin = pin->sec_pin & 0x3F;  // turn off the "pin is an output" bit and "applies to all instances" bit

    switch (pin->sec_tag) {

        case HM2_GTAG_ENCODER:
            switch (sec_pin) {
                case 1: return "A";
                case 2: return "B";
                case 3: return "Index";
                case 4: return "IndexMask";
                case 0x45:
                case 5: return "Probe";
            }
            break;

        case HM2_GTAG_PWMGEN:
            // FIXME: these depend on the pwmgen mode
            switch (sec_pin) {
                case 1: return "Out0 (PWM or Up)";
                case 2: return "Out1 (Dir or Down)";
                case 3: return "Not-Enable";
            }
            break;

        case HM2_GTAG_STEPGEN:
            // FIXME: these depend on the stepgen mode
            switch (sec_pin) {
                case 1: return "Step";
                case 2: return "Direction";
                case 3: return "(unused)";
                case 4: return "(unused)";
                case 5: return "(unused)";
                case 6: return "(unused)";
            }
            break;

    }

    rtapi_snprintf(unknown, sizeof(unknown), "unknown-pin-%d", sec_pin & 0x7F);
    return unknown;
}




static void hm2_print_pin_descriptors(hostmot2_t *hm2) {
    int i;

    HM2_PRINT("%d HM2 Pin Descriptors:\n", hm2->num_pins);

    for (i = 0; i < hm2->num_pins; i ++) {
        HM2_PRINT("    pin %d:\n", i);
        HM2_PRINT(
            "        Primary Tag: 0x%02X (%s)\n",
            hm2->pin[i].primary_tag,
            hm2_get_general_function_name(hm2->pin[i].primary_tag)
        );
        if (hm2->pin[i].sec_tag != 0) {
            HM2_PRINT(
                "        Secondary Tag: 0x%02X (%s)\n",
                hm2->pin[i].sec_tag,
                hm2_get_general_function_name(hm2->pin[i].sec_tag)
            );
            HM2_PRINT("        Secondary Unit: 0x%02X\n", hm2->pin[i].sec_unit);
            HM2_PRINT(
                "        Secondary Pin: 0x%02X (%s, %s)\n",
                hm2->pin[i].sec_pin,
                hm2_get_pin_secondary_name(&hm2->pin[i]),
                ((hm2->pin[i].sec_pin & 0x80) ? "Output" : "Input")
            );
        }
    }
}




int hm2_read_pin_descriptors(hostmot2_t *hm2) {
    int i;
    int addr;

    hm2->num_pins = hm2->idrom.io_width;
    hm2->pin = kmalloc(sizeof(hm2_pin_t) * hm2->num_pins, GFP_KERNEL);
    if (hm2->pin == NULL) {
        HM2_ERR("out of memory!\n");
        return -ENOMEM;
    }

    addr = hm2->idrom_offset + hm2->idrom.offset_to_pin_desc;
    for (i = 0; i < hm2->num_pins; i ++) {
        u32 d;

        if (!hm2->llio->read(hm2->llio, addr, &d, sizeof(u32))) {
            HM2_ERR("error reading Pin Descriptor %d (at 0x%04x)\n", i, addr); 
            return -EIO;
        }

        hm2->pin[i].sec_pin     = (d >>  0) & 0x000000FF;
        hm2->pin[i].sec_tag     = (d >>  8) & 0x000000FF;
        hm2->pin[i].sec_unit    = (d >> 16) & 0x000000FF;
        hm2->pin[i].primary_tag = (d >> 24) & 0x000000FF;

        if (hm2->pin[i].primary_tag == 0) {
            // oops, found the Zero sentinel before the promised number of pins
            HM2_ERR(
                "pin %d primary tag is 0 (end-of-list sentinel), expected %d!\n",
                i,
                hm2->num_pins
            );
            return -EINVAL;
        }

        if (hm2->pin[i].primary_tag != HM2_GTAG_IOPORT) {
            HM2_ERR(
                "pin %d primary tag is %d (%s), not IOPort!\n",
                i,
                hm2->pin[i].primary_tag,
                hm2_get_general_function_name(hm2->pin[i].primary_tag)
            );
            return -EINVAL;
        }

        hm2->pin[i].gtag = hm2->pin[i].primary_tag;

        addr += 4;
    }

    if (debug_pin_descriptors) {
        hm2_print_pin_descriptors(hm2);
    }

    return 0;
}




void hm2_set_pin_source(hostmot2_t *hm2, int pin_number, int source) {
    int ioport_number;
    int bit_number;

    ioport_number = pin_number / 24;
    bit_number = pin_number % 24;

    if ((pin_number < 0) || (ioport_number > hm2->ioport.num_instances)) {
        HM2_ERR("hm2_set_pin_source: invalid pin number %d\n", pin_number);
        return;
    }

    if (source == HM2_PIN_SOURCE_IS_PRIMARY) {
        hm2->ioport.alt_source_reg[ioport_number] &= ~(1 << bit_number);
        hm2->pin[pin_number].gtag = hm2->pin[pin_number].primary_tag;
    } else if (source == HM2_PIN_SOURCE_IS_SECONDARY) {
        hm2->ioport.alt_source_reg[ioport_number] |= (1 << bit_number);
        hm2->pin[pin_number].gtag = hm2->pin[pin_number].sec_tag;
    } else {
        HM2_ERR("hm2_set_pin_source: invalid pin source 0x%08X\n", source);
        return;
    }
}




void hm2_set_pin_direction(hostmot2_t *hm2, int pin_number, int direction) {
    int ioport_number;
    int bit_number;

    ioport_number = pin_number / 24;
    bit_number = pin_number % 24;

    if ((pin_number < 0) || (ioport_number > hm2->ioport.num_instances)) {
        HM2_ERR("hm2_set_pin_direction: invalid pin number %d\n", pin_number);
        return;
    }

    if ((direction != HM2_PIN_DIR_IS_INPUT) && (direction != HM2_PIN_DIR_IS_OUTPUT)) {
        HM2_ERR("hm2_set_pin_direction: invalid pin direction 0x%08X\n", direction);
        return;
    }

    hm2->pin[pin_number].direction = direction;
}




void hm2_print_pin_usage(hostmot2_t *hm2) {
    int i;

    HM2_PRINT("%d I/O Pins used:\n", hm2->num_pins);

    for (i = 0; i < hm2->num_pins; i ++) {
        int port = i / hm2->idrom.port_width;
        int port_pin = ((i % 24) * 2) + 1;

        if (hm2->pin[i].gtag == hm2->pin[i].sec_tag) {
            if(hm2->pin[i].sec_unit & 0x80)
                HM2_PRINT(
                    "    IO Pin %03d (%s-%02d): %s (all), pin %s (%s)\n",
                    i,
                    hm2->llio->ioport_connector_name[port],
                    port_pin,
                    hm2_get_general_function_name(hm2->pin[i].gtag),
                    hm2_get_pin_secondary_name(&hm2->pin[i]),
                    ((hm2->pin[i].sec_pin & 0x80) ? "Output" : "Input")
                );
            else
                HM2_PRINT(
                    "    IO Pin %03d (%s-%02d): %s #%d, pin %s (%s)\n",
                    i,
                    hm2->llio->ioport_connector_name[port],
                    port_pin,
                    hm2_get_general_function_name(hm2->pin[i].gtag),
                    hm2->pin[i].sec_unit,
                    hm2_get_pin_secondary_name(&hm2->pin[i]),
                    ((hm2->pin[i].sec_pin & 0x80) ? "Output" : "Input")
                );
        } else {
            HM2_PRINT(
                "    IO Pin %03d (%s-%02d): %s\n",
                i,
                hm2->llio->ioport_connector_name[port],
                port_pin,
                hm2_get_general_function_name(hm2->pin[i].gtag)
            );
        }
    }
}




// all pins whose secondary_tag == gtag and whose
// secondary_unit < num_instances get their source set to secondary and
// their pin direction updated to match
static void hm2_pins_allocate_all(hostmot2_t *hm2, int gtag, int num_instances) {
    int i;

    for (i = 0; i < hm2->num_pins; i ++) {
        if ((hm2->pin[i].sec_tag == gtag)
            && ((hm2->pin[i].sec_unit < num_instances)
                || ((hm2->pin[i].sec_unit & 0x80) && (num_instances > 0)))
        ) {
            hm2_set_pin_source(hm2, i, HM2_PIN_SOURCE_IS_SECONDARY);
            if (hm2->pin[i].sec_pin & 0x80){
                hm2_set_pin_direction(hm2, i, HM2_PIN_DIR_IS_OUTPUT);
            }
        }
    }
}




// sets up all the IOPort instances, return 0 on success, -errno on failure
void hm2_configure_pins(hostmot2_t *hm2) {
    int i;

    // 
    // the bits in the alt_source register of the ioport function say
    // whether *output* data comes from the primary source (ioport
    // function) (0) or the secondary source (1)
    // 
    // the bits in the data direction register say whether the pins are
    // inputs (0) or outputs (1)
    // 
    // if a pin is marked as an input in the ddr, it can be used for its
    // function (encoder, say) *and* as a digital input pin without
    // conflict
    // 
    // Each function instance that is not disabled by the relevant
    // num_<functions> modparam has all its pins marked 1 in the alt_source
    // register.  The driver uses this to to keep track of which pins are
    // "allocated" to functions and which pins are available for use as
    // gpios.
    // 

    // everything defaults to GPIO input...
    for (i = 0; i < hm2->num_pins; i ++) {
        hm2_set_pin_source(hm2, i, HM2_PIN_SOURCE_IS_PRIMARY);
        hm2_set_pin_direction(hm2, i, HM2_PIN_DIR_IS_INPUT);
    }


    //
    // ... then modules get to take what they want
    //

    // stepgen is special, it wants to think about what pins it takes
    hm2_stepgen_allocate_pins(hm2);

    // encoder and pwmgen just get all their enabled instances' pins
    hm2_pins_allocate_all(hm2, HM2_GTAG_ENCODER, hm2->encoder.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_PWMGEN,  hm2->pwmgen.num_instances);
}


