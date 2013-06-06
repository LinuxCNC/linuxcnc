
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

#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




int hm2_raw_setup(hostmot2_t *hm2) {
    int r;
    char name[HAL_NAME_LEN + 1];


    if (hm2->config.enable_raw == 0) {
        return 0;
    }


    hm2->raw = (hm2_raw_t *)hal_malloc(sizeof(hm2_raw_t));
    if (hm2->raw == NULL) {
        HM2_ERR("out of memory!\n");
        hm2->config.enable_raw = 0;
        return -ENOMEM;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.read_address", hm2->llio->name);
    r = hal_pin_u32_new(name, HAL_IN, &(hm2->raw->hal.pin.read_address), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.read_data", hm2->llio->name);
    r = hal_pin_u32_new(name, HAL_OUT, &(hm2->raw->hal.pin.read_data), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.write_address", hm2->llio->name);
    r = hal_pin_u32_new(name, HAL_IN, &(hm2->raw->hal.pin.write_address), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.write_data", hm2->llio->name);
    r = hal_pin_u32_new(name, HAL_IN, &(hm2->raw->hal.pin.write_data), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.write_strobe", hm2->llio->name);
    r = hal_pin_bit_new(name, HAL_IN, &(hm2->raw->hal.pin.write_strobe), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    rtapi_snprintf(name, sizeof(name), "%s.raw.dump_state", hm2->llio->name);
    r = hal_pin_bit_new(name, HAL_IO, &(hm2->raw->hal.pin.dump_state), hm2->llio->comp_id);
    if (r < 0) {
        HM2_ERR("error adding pin '%s', aborting\n", name);
        return -EINVAL;
    }

    // init hal objects
    *(hm2->raw->hal.pin.read_address)  = 0;
    *(hm2->raw->hal.pin.read_data) = 0;

    *(hm2->raw->hal.pin.write_address) = 0;
    *(hm2->raw->hal.pin.write_data) = 0;
    *(hm2->raw->hal.pin.write_strobe) = 0;

    *(hm2->raw->hal.pin.dump_state) = 0;

    return 0;
}




void hm2_raw_read(hostmot2_t *hm2) {
    if (hm2->config.enable_raw == 0) return;

    hm2->llio->read(
        hm2->llio,
        *hm2->raw->hal.pin.read_address & 0xffff,
        (void *)hm2->raw->hal.pin.read_data,
        sizeof(u32)
    );

    if (*hm2->raw->hal.pin.dump_state != 0) {
        hm2_print_modules(hm2);
        *hm2->raw->hal.pin.dump_state = 0;
    }
}




void hm2_raw_write(hostmot2_t *hm2) {
    if (hm2->config.enable_raw == 0) return;
    if (*hm2->raw->hal.pin.write_strobe == 0) return;

    hm2->llio->write(
        hm2->llio,
        *hm2->raw->hal.pin.write_address & 0xffff,
        (void *)hm2->raw->hal.pin.write_data,
        sizeof(u32)
    );

    *hm2->raw->hal.pin.write_strobe = 0;
}

