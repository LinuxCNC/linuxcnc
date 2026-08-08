
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

#include <rtapi.h>
#include <rtapi_string.h>
#include <rtapi_math.h>

#include <hal.h>

#include "hostmot2.h"




int hm2_raw_setup(hostmot2_t *hm2) {
    int r;

    if (hm2->config.enable_raw == 0) {
        return 0;
    }


    hm2->raw = hal_malloc(sizeof(*hm2->raw));
    if (hm2->raw == NULL) {
        HM2_ERR("out of memory!\n");
        hm2->config.enable_raw = 0;
        return -ENOMEM;
    }

    r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->raw->hal.pin.read_address),
                         0, "%s.raw.read_address", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.read_address', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &(hm2->raw->hal.pin.read_data),
                         0, "%s.raw.read_data", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.read_data', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->raw->hal.pin.write_address),
                         0, "%s.raw.write_address", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.write_address', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    r = hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &(hm2->raw->hal.pin.write_data),
                         0, "%s.raw.write_data", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.write_data', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &(hm2->raw->hal.pin.write_strobe),
                         0, "%s.raw.write_strobe", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.write_strobe', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    r = hal_pin_new_bool(hm2->llio->comp_id, HAL_IO, &(hm2->raw->hal.pin.dump_state),
                         0, "%s.raw.dump_state", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error %d adding pin '%s.raw.dump_state', aborting\n", r, hm2->llio->name);
        return -EINVAL;
    }

    return 0;
}




void hm2_raw_queue_read(hostmot2_t *hm2) {
    if (hm2->config.enable_raw == 0) return;

    hm2->llio->queue_read(
        hm2->llio,
        hal_get_ui32(hm2->raw->hal.pin.read_address) & 0xffff,
        (void *)hm2->raw->hal.pin.read_data,
        sizeof(rtapi_u32)
    );

    if (hal_get_bool(hm2->raw->hal.pin.dump_state)) {
        hm2_print_modules(hm2);
        hal_set_bool(hm2->raw->hal.pin.dump_state, 0);
    }
}




void hm2_raw_write(hostmot2_t *hm2) {
    if (hm2->config.enable_raw == 0) return;
    if (!hal_get_bool(hm2->raw->hal.pin.write_strobe)) return;

    hm2->llio->write(
        hm2->llio,
        hal_get_ui32(hm2->raw->hal.pin.write_address) & 0xffff,
        (void *)hm2->raw->hal.pin.write_data,
        sizeof(rtapi_u32)
    );

    hal_set_bool(hm2->raw->hal.pin.write_strobe, 0);
}

