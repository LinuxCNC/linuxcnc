
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

#include <rtapi_slab.h>

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




int debug_pin_descriptors = 0;
RTAPI_MP_INT(debug_pin_descriptors, "Developer/debug use only!  Enable debug logging of the HostMot2\nPin Descriptors.");




// FIXME: the static automatic string makes this function non-reentrant
static const char* hm2_get_pin_secondary_name(hm2_pin_t *pin) {
    static char unknown[100];
    int sec_pin = pin->sec_pin & 0x7F;  // turn off the "pin is an output" bit
//FIXME: some pins use the same sec_tag but different meanings depending on
//direction.
    switch (pin->sec_tag) {

        case HM2_GTAG_MUXED_ENCODER:
            switch (sec_pin) {
                case 1: return "Muxed A";
                case 2: return "Muxed B";
                case 3: return "Muxed Index";
                case 4: return "Muxed IndexMask";
            }
            break;

        case HM2_GTAG_MUXED_ENCODER_SEL:
            switch (sec_pin) {
                case 1: return "Mux Select 0";
                case 2: return "Mux Select 1";
            }
            break;

        case HM2_GTAG_ENCODER:
            switch (sec_pin) {
                case 1: return "A";
                case 2: return "B";
                case 3: return "Index";
                case 4: return "IndexMask";
                case 5: return "Probe";
            }
            break;
        case HM2_GTAG_SSI:
            switch (sec_pin) {
                case 1: return "Clock";
                case 2: return "ClockEnable";
                case 3: return "Data";
            }
            break;

        case HM2_GTAG_SPI: // Not supported yet
                switch (sec_pin) {
                    case 1: return "Frame";
                    case 2: return "Out";
                    case 3: return "Clock";
                    case 4: return "In";
                }
                break;

        case HM2_GTAG_RESOLVER:
            switch (sec_pin) {
                case 1: return "NC";
                case 2: return "REFPDM+";
                case 3: return "REFPDM-";
                case 4: return "AMUX0";
                case 5: return "AMUX1";
                case 6: return "AMUX2";
                case 7: return "SPICS";
                case 8: return "SPICLK";
                case 9: return "SPIDO0";
                case 10: return "SPIDO1";
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

        case HM2_GTAG_TPPWM:
            switch (sec_pin) {
                case 1: return "PWM A";
                case 2: return "PWM B";
                case 3: return "PWM C";
                case 4: return "PWM /A";
                case 5: return "PWM /B";
                case 6: return "PWM /C";
                case 7: return "Enable";
                case 8: return "Fault";
            }
            break;

        case HM2_GTAG_STEPGEN:
            // FIXME: these depend on the stepgen mode
            switch (sec_pin) {
                case 1: return "Step";
                case 2: return "Direction";
                case 3: return "Table2Pin";
                case 4: return "Table3Pin";
                case 5: return "Table4Pin";
                case 6: return "Table5Pin";
                case 7: return "Table6Pin";
                case 8: return "Table7Pin";
            }
            break;

        case HM2_GTAG_SMARTSERIAL:
            if (pin->sec_pin & 0x80){ // Output pin codes
                switch (sec_pin) {
                    case 0x1: return "TxData0";
                    case 0x2: return "TxData1";
                    case 0x3: return "TxData2";
                    case 0x4: return "TxData3";
                    case 0x5: return "TxData4";
                    case 0x6: return "TxData5";
                    case 0x7: return "TxData6";
                    case 0x8: return "TxData7";
                    case 0x11: return "TxEn0  ";
                    case 0x12: return "TxEn1  ";
                    case 0x13: return "TxEn2  ";
                    case 0x14: return "TxEn3  ";
                    case 0x15: return "TxEn4  ";
                    case 0x16: return "TxEn5  ";
                    case 0x17: return "TxEn6  ";
                    case 0x18: return "TxEn7  ";
                }
                break;
            }else{ // INput Pin Codes
                switch (sec_pin) {
                    case 0x1: return "RxData0";
                    case 0x2: return "RxData1";
                    case 0x3: return "RxData2";
                    case 0x4: return "RxData3";
                    case 0x5: return "RxData4";
                    case 0x6: return "RxData5";
                    case 0x7: return "RxData6";
                    case 0x8: return "RxData7";
                }
                break;
            }
        case HM2_GTAG_BSPI:
            switch (sec_pin) {
                case 0x1: return "/Frame";
                case 0x2: return "Serial Out";
                case 0x3: return "Clock";
                case 0x4: return "Serial In";
                case 0x5: return "CS0";
                case 0x6: return "CS1";
                case 0x7: return "CS2";
                case 0x8: return "CS3";
                case 0x9: return "CS4";
                case 0xA: return "CS5";
                case 0xB: return "CS6";
                case 0xC: return "CS7";
        }
        break;

        case HM2_GTAG_DBSPI: // Not Supported Currently
            switch (sec_pin) {
                case 0x2: return "Serial Out";
                case 0x3: return "Clock";
                case 0x4: return "Serial In";
                case 0x5: return "CS0";
                case 0x6: return "CS1";
                case 0x7: return "CS2";
                case 0x8: return "CS3";
                case 0x9: return "CS4";
                case 0xA: return "CS5";
                case 0xB: return "CS6";
                case 0xC: return "CS7";
            }
            break;

        case HM2_GTAG_UART_RX:
            switch (sec_pin) {
                case 0x1: return "RX Data";
            }
            break;
        case HM2_GTAG_UART_TX:    
            switch (sec_pin) {
                case 0x1: return "TX Data";
                case 0x2: return "Drv Enable";
            }
            break;

        case HM2_GTAG_PKTUART_RX:
            switch (sec_pin) {
                case 0x1: return "RX Data";
            }
            break;
        case HM2_GTAG_PKTUART_TX:    
            switch (sec_pin) {
                case 0x1: return "TX Data";
                case 0x2: return "Drv Enable";
            }
            break;

        case HM2_GTAG_DPLL: // Not Supported Currently
            switch (sec_pin) {
                case 0x1: return "SynchIn";
                case 0x2: return "MSBOut";
                case 0x3: return "Fout";
                case 0x4: return "PostOut";
                case 0x5: return "SynchTog";
            }
            break;

        case HM2_GTAG_WAVEGEN: // Not Supported Currently
            switch (sec_pin) {
                case 0x1: return "PDMAOut";
                case 0x2: return "PDMBOut";
                case 0x3: return "Trigger0";
                case 0x4: return "Trigger1";
                case 0x5: return "Trigger2";
                case 0x6: return "Trigger3";
            }
            break;

        case HM2_GTAG_DAQFIFO: // Not Supported Currently
            switch (sec_pin) {
                case 0x41: return "Strobe";
                default:
                    sprintf(unknown, "Data%02x",sec_pin - 1);
                    return unknown;
            }
            break;

        case HM2_GTAG_BINOSC: // Not Supported Currently
             sprintf(unknown, "Out%02x",sec_pin -1);
             return unknown;
             break;

        case HM2_GTAG_BISS: // Not Supported Currently
            switch (sec_pin) {
                case 0x1: return "Clck";
                case 0x2: return "Data";
            }
            break;

        case HM2_GTAG_FABS: // Not Supported Currently
            switch (sec_pin) {
                case 0x1: return "Rq";
                case 0x2: return "RqEn";
                case 0x3: return "Data";
                case 0x4: return "TestClk";
            }
            break;
            
        case HM2_GTAG_HM2DPLL:
            switch (sec_pin) {
                case 0x1: return "Sync In Pin";
                case 0x2: return "Ref Out Pin";
                case 0x3: return "Timer 1 Pin";
                case 0x4: return "Timer 2 Pin";
                case 0x5: return "Timer 3 Pin";
                case 0x6: return "Timer 4 Pin";
            }
            break;

        case HM2_GTAG_TWIDDLER: // Not Supported Currently
             if (sec_pin < 0x20){
                 sprintf(unknown, "In%02x", sec_pin - 1);
             } else if (sec_pin > 0xC0){
                 sprintf(unknown, "IO%02x", sec_pin - 1);
             } else {
                 sprintf(unknown, "Out%02x", sec_pin - 1);
             }
             return unknown;
             break;

        case HM2_GTAG_SSR:
            if ((sec_pin >= 1) && (sec_pin <= 31)) {
                sprintf(unknown, "Out-%02d",sec_pin - 1);
                return unknown;
            } else if (sec_pin == 32) {
                sprintf(unknown, "AC Ref (internal)");
                return unknown;
            }
            break;
    }

    rtapi_snprintf(unknown, sizeof(unknown), "unknown-pin-%d", sec_pin & 0x7F);
    return unknown;
}

const char* hm2_get_pin_secondary_hal_name(const hm2_pin_t *pin) {
    int sec_pin = pin->sec_pin & 0x7F;  // turn off the "pin is an output" bit
//FIXME: some pins use the same sec_tag but different meanings depending on
//direction.
    switch (pin->sec_tag) {

        case HM2_GTAG_MUXED_ENCODER:
            switch (sec_pin) {
                case 1: return "phase-A";
                case 2: return "phase-B";
                case 3: return "phase-Z";
                case 4: return "phase-Z-mask";
            }
            break;

        case HM2_GTAG_MUXED_ENCODER_SEL:
            switch (sec_pin) {
                case 1: return "sel0";
                case 2: return "sel1";
            }
            break;

        case HM2_GTAG_ENCODER:
            switch (sec_pin) {
                case 1: return "phase-A";
                case 2: return "phase-B";
                case 3: return "phase-Z";
                case 4: return "phase-Z-mask";
                case 5: return "probe-latch";
            }
            break;
        case HM2_GTAG_SSI:
            switch (sec_pin) {
                case 1: return "clock";
                case 2: return "clock-enable";
                case 3: return "data";
            }
            break;

        case HM2_GTAG_SPI: // Not supported yet
                switch (sec_pin) {
                    case 1: return "frame";
                    case 2: return "out";
                    case 3: return "clock";
                    case 4: return "in";
                }
                break;

        case HM2_GTAG_RESOLVER:
            switch (sec_pin) {
                case 1: return "nc";
                case 2: return "refpdm-plus";
                case 3: return "refpdm-minus";
                case 4: return "amux0";
                case 5: return "amux1";
                case 6: return "amux2";
                case 7: return "spics";
                case 8: return "spiclk";
                case 9: return "spido0";
                case 10: return "spido1";
            }
            break;

        case HM2_GTAG_PWMGEN:
            // FIXME: these depend on the pwmgen mode
            switch (sec_pin) {
                case 1: return "out0";
                case 2: return "out1";
                case 3: return "enable";
            }
            break;

        case HM2_GTAG_TPPWM:
            switch (sec_pin) {
                case 1: return "phase-A";
                case 2: return "phase-B";
                case 3: return "phase-C";
                case 4: return "phase-A-inverted";
                case 5: return "phase-B-inverted";
                case 6: return "phase-C-inverted";
                case 7: return "enable";
                case 8: return "fault";
            }
            break;

        case HM2_GTAG_STEPGEN:
            // FIXME: these depend on the stepgen mode
            switch (sec_pin) {
                case 1: return "step";
                case 2: return "direction";
                case 3: return "table2";
                case 4: return "table3";
                case 5: return "table4";
                case 6: return "table5";
                case 7: return "table6";
                case 8: return "table7";
            }
            break;

        case HM2_GTAG_SMARTSERIAL:
            if (pin->sec_pin & 0x80){ // Output pin codes
                switch (sec_pin) {
                    case 0x1: return "tx0";
                    case 0x2: return "tx1";
                    case 0x3: return "tx2";
                    case 0x4: return "tx3";
                    case 0x5: return "tx4";
                    case 0x6: return "tx5";
                    case 0x7: return "tx6";
                    case 0x8: return "tx7";
                    case 0x11: return "txen0";
                    case 0x12: return "txen1";
                    case 0x13: return "txen2";
                    case 0x14: return "txen3";
                    case 0x15: return "txen4";
                    case 0x16: return "txen5";
                    case 0x17: return "txen6";
                    case 0x18: return "txen7";
                }
                break;
            }else{ // INput Pin Codes
                switch (sec_pin) {
                    case 0x1: return "rx0";
                    case 0x2: return "rx1";
                    case 0x3: return "rx2";
                    case 0x4: return "rx3";
                    case 0x5: return "rx4";
                    case 0x6: return "rx5";
                    case 0x7: return "rx6";
                    case 0x8: return "rx7";
                }
                break;
            }
        case HM2_GTAG_BSPI:
            switch (sec_pin) {
                case 0x1: return "nframe";
                case 0x2: return "out";
                case 0x3: return "clk";
                case 0x4: return "in";
                case 0x5: return "cs0";
                case 0x6: return "cs1";
                case 0x7: return "cs2";
                case 0x8: return "cs3";
                case 0x9: return "cs4";
                case 0xA: return "cs5";
                case 0xB: return "cs6";
                case 0xC: return "cs7";
        }
        break;

        case HM2_GTAG_DBSPI: // Not Supported Currently
            switch (sec_pin) {
                case 0x2: return "out";
                case 0x3: return "clock";
                case 0x4: return "in";
                case 0x5: return "cs0";
                case 0x6: return "cs1";
                case 0x7: return "cs2";
                case 0x8: return "cs3";
                case 0x9: return "cs4";
                case 0xA: return "cs5";
                case 0xB: return "cs6";
                case 0xC: return "cs7";
            }
            break;

        case HM2_GTAG_UART_RX:
            switch (sec_pin) {
                case 0x1: return "rx";
            }
            break;
        case HM2_GTAG_UART_TX:
            switch (sec_pin) {
                case 0x1: return "tx";
                case 0x2: return "tx-drive-enable";
            }
            break;

        case HM2_GTAG_PKTUART_RX:
            switch (sec_pin) {
                case 0x1: return "rx";
            }
            break;
        case HM2_GTAG_PKTUART_TX:
            switch (sec_pin) {
                case 0x1: return "tx";
                case 0x2: return "tx-drive-enable";
            }
            break;

    }
    return NULL;
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

    const rtapi_u8 DB25[] = {1,14,2,15,3,16,4,17,5,6,7,8,9,10,11,12,13};
    
    hm2->num_pins = hm2->idrom.io_width;
    hm2->pin = rtapi_kmalloc(sizeof(hm2_pin_t) * hm2->num_pins, RTAPI_GFP_KERNEL);
    if (hm2->pin == NULL) {
        HM2_ERR("out of memory!\n");
        return -ENOMEM;
    }

    addr = hm2->idrom_offset + hm2->idrom.offset_to_pin_desc;
    for (i = 0; i < hm2->num_pins; i ++) {
        hm2_pin_t *pin = &(hm2->pin[i]);
        rtapi_u32 d;

        if (!hm2->llio->read(hm2->llio, addr, &d, sizeof(rtapi_u32))) {
            HM2_ERR("error reading Pin Descriptor %d (at 0x%04x)\n", i, addr); 
            return -EIO;
        }

        pin->sec_pin     = (d >>  0) & 0x000000FF;
        pin->sec_tag     = (d >>  8) & 0x000000FF;
        pin->sec_unit    = (d >> 16) & 0x000000FF;
        pin->primary_tag = (d >> 24) & 0x000000FF;

        if (pin->primary_tag == 0) {
            // oops, found the Zero sentinel before the promised number of pins
            HM2_ERR(
                "pin %d primary tag is 0 (end-of-list sentinel), expected %d pins!\n",
                i,
                hm2->num_pins
            );
            return -EINVAL;
        }

        if (pin->primary_tag != HM2_GTAG_IOPORT) {
            HM2_ERR(
                "pin %d primary tag is %d (%s), not IOPort!\n",
                i,
                pin->primary_tag,
                hm2_get_general_function_name(pin->primary_tag)
            );
            return -EINVAL;
        }

        pin->gtag = pin->primary_tag;
        
        pin->port_num = i / hm2->idrom.port_width;
        
        if ((pin->port_num < 0 ) 
            || (pin->port_num >= hm2->llio->num_ioport_connectors)){
            HM2_ERR("hm2_read_pin_descriptors: Calculated port number (%d) is "
                    "invalid\n", pin->port_pin );
            return -EINVAL;
        }
        
        pin->bit_num = i % hm2->idrom.port_width;
        
        if ((pin->bit_num < 0 ) || (pin->bit_num > 31)){
            HM2_ERR("hm2_read_pin_descriptors: Calculated bit number (%d) is "
                    "invalid\n", pin->bit_num );
            return -EINVAL;
        }
        switch (hm2->idrom.port_width) {
            case 24:   /* standard 50 pin 24 I/O cards, just the odd pins */
                pin->port_pin = ((i % 24) * 2) + 1;
                break;
            case 17:    /* 25 pin 17 I/O parallel port type cards funny DB25 order */
                pin->port_pin = DB25[i % 17];
                break;
            case 32:      /* 5I21 punt on this for now */
                pin->port_pin = i + 1;
                break;
            default:
                HM2_ERR("%s: invalid port width %d\n", __FUNCTION__, hm2->idrom.port_width);
        }
        
        addr += 4;
    }

    if (debug_pin_descriptors) {
        hm2_print_pin_descriptors(hm2);
    }

    return 0;
}




void hm2_set_pin_source(hostmot2_t *hm2, int pin_number, int source) {
    
    if ((pin_number < 0) 
        || (pin_number >= hm2->num_pins)
        || (hm2->ioport.num_instances <= 0)) {
        HM2_ERR("hm2_set_pin_source: invalid pin number %d\n", pin_number);
        return;
    }
    {
        hm2_pin_t *pin = &(hm2->pin[pin_number]);
        if (source == HM2_PIN_SOURCE_IS_PRIMARY) {
            hm2->ioport.alt_source_reg[pin->port_num] &= ~(1 << pin->bit_num);
            pin->gtag = pin->primary_tag;
        } else if (source == HM2_PIN_SOURCE_IS_SECONDARY) {
            hm2->ioport.alt_source_reg[pin->port_num] |= (1 << pin->bit_num);
            pin->gtag = pin->sec_tag;
        } else {
            HM2_ERR("hm2_set_pin_source: invalid pin source 0x%08X\n", source);
            return;
        }
    }
}




void hm2_set_pin_direction(hostmot2_t *hm2, int pin_number, int direction) {

    if ((pin_number < 0) 
        || (pin_number >= hm2->num_pins)
        || (hm2->ioport.num_instances <= 0)) {
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
        hm2_pin_t *pin = &(hm2->pin[i]);
        char connector_pin_name[100];

        if (hm2->llio->io_connector_pin_names == NULL) {
            snprintf(connector_pin_name, sizeof(connector_pin_name), "%s-%02d", hm2->llio->ioport_connector_name[pin->port_num], pin->port_pin);
        } else {
            if (hm2->llio->io_connector_pin_names[i] == NULL) {
                continue;
            }
            snprintf(connector_pin_name, sizeof(connector_pin_name), "%s", hm2->llio->io_connector_pin_names[i]);
        }

        if (pin->gtag == pin->sec_tag) {
            if(pin->sec_unit & 0x80)
                HM2_PRINT(
                    "    IO Pin %03d (%s): %s (all), pin %s (%s)\n",
                    i,
                    connector_pin_name,
                    hm2_get_general_function_name(pin->gtag),
                    hm2_get_pin_secondary_name(&hm2->pin[i]),
                    ((pin->sec_pin & 0x80) ? "Output" : "Input")
                );
            else
                HM2_PRINT(
                    "    IO Pin %03d (%s): %s #%d, pin %s (%s)\n",
                    i,
                    connector_pin_name,
                    hm2_get_general_function_name(pin->gtag),
                    pin->sec_unit,
                    hm2_get_pin_secondary_name(pin),
                    ((pin->sec_pin & 0x80) ? "Output" : "Input")
                );
        } else {
            HM2_PRINT(
                "    IO Pin %03d (%s): %s\n",
                i,
                connector_pin_name,
                hm2_get_general_function_name(pin->gtag)
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
    // Abs encoders are all packed together, not necessarily contiguously
    hm2_pins_allocate_all(hm2, HM2_GTAG_SSI, MAX_ABSENCS);
    hm2_pins_allocate_all(hm2, HM2_GTAG_BISS, MAX_ABSENCS);
    hm2_pins_allocate_all(hm2, HM2_GTAG_FABS, MAX_ABSENCS);
    hm2_pins_allocate_all(hm2, HM2_GTAG_RESOLVER, hm2->resolver.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_PWMGEN,  hm2->pwmgen.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_TPPWM,  hm2->tp_pwmgen.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_BSPI,  hm2->bspi.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_UART_RX,  hm2->uart.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_UART_TX ,  hm2->uart.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_PKTUART_RX,  hm2->pktuart.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_PKTUART_TX ,  hm2->pktuart.num_instances);
    // smart-serial might also not be contiguous
    hm2_pins_allocate_all(hm2, HM2_GTAG_SMARTSERIAL,  HM2_SSERIAL_MAX_PORTS);
    // muxed encoder gets the sel pins
    hm2_pins_allocate_all(hm2, HM2_GTAG_MUXED_ENCODER_SEL, hm2->encoder.num_instances);
    // and about half as many I/Os as you'd expect
    hm2_pins_allocate_all(hm2, HM2_GTAG_MUXED_ENCODER, (hm2->encoder.num_instances+1)/2);
    hm2_pins_allocate_all(hm2, HM2_GTAG_HM2DPLL, hm2->dpll.num_instances);
    hm2_pins_allocate_all(hm2, HM2_GTAG_SSR, hm2->ssr.num_instances);
}

const char *hm2_get_general_function_hal_name(int gtag) {
    switch(gtag) {
        case HM2_GTAG_ENCODER:  return "encoder";
        case HM2_GTAG_SSI:      return "ssi";
        case HM2_GTAG_BISS:     return "biss";
        case HM2_GTAG_FABS:     return "fanuc";
        case HM2_GTAG_RESOLVER: return "resolver";
        case HM2_GTAG_STEPGEN:  return "stepgen";
        case HM2_GTAG_PWMGEN:   return "pwmgen";
        case HM2_GTAG_TPPWM:    return "3pwmgen";
        case HM2_GTAG_MUXED_ENCODER: return "encoder";
        case HM2_GTAG_MUXED_ENCODER_SEL: return "encoder";

        // XXX these don't seem to have consistent names of the expected form
        case HM2_GTAG_SMARTSERIAL: return "sserial";
        case HM2_GTAG_BSPI:     return "bspi";
        case HM2_GTAG_UART_RX:  return "uart";
        case HM2_GTAG_UART_TX:  return "uart";
        case HM2_GTAG_PKTUART_RX:  return "uart";
        case HM2_GTAG_PKTUART_TX:  return "uart";

        default: return NULL;
    }
}
