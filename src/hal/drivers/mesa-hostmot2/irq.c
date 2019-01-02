//
//    Copyright (C) 2016 Devin Hughes, JD Squared

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

// A driver for the Hostmot2 HM2_DPLL IRQ


#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"
#include "hal_priv.h"
#include "hal/drivers/mesa-hostmot2/hostmot2.h"
#include <unistd.h>

static int zero = 0;

static int hm2_waitirq(void *void_hm2, const hal_funct_args_t *fa);

int hm2_irq_setup(hostmot2_t *hm2, long period) {
    int r;

    // Fake the module count - if it gets promoted this will be populated
    // by the module descriptor parse function
    hm2->irq.num_instances = 1;
    hm2->irq.dpll_timer_num = 0; // Use the dpll reference timer

    // export the irq hal pins
    hm2->irq.pins = hal_malloc(sizeof(hm2_irq_pins_t));

    r = hal_pin_u32_newf(HAL_IN, &(hm2->irq.pins->desired_rate_nsec),
            hm2->llio->comp_id, "%s.irq.desired-rate-nsec", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->irq.pins->current_rate_nsec),
            hm2->llio->comp_id, "%s.irq.current-rate-nsec", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->irq.pins->count),
            hm2->llio->comp_id, "%s.irq.count", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->irq.pins->missed),
            hm2->llio->comp_id, "%s.irq.missed", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->irq.pins->write_errors),
            hm2->llio->comp_id, "%s.irq.write-errors", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->irq.pins->read_errors),
            hm2->llio->comp_id, "%s.irq.read-errors", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error adding hm2_irq pins, Aborting\n");
        goto fail0;
    }

    *(hm2->irq.pins->desired_rate_nsec) = (u32)period;
    *(hm2->irq.pins->current_rate_nsec) = 0;

    // export the waitirq blocking function
    hal_export_xfunct_args_t xfunct_args = {
	    .type = FS_XTHREADFUNC,
	    .funct.x = hm2_waitirq,
	    .arg = hm2,
	    .uses_fp = 1,
	    .reentrant = 0,
	    .owner_id = hm2->llio->comp_id
    };

    if ((r = hal_export_xfunctf(&xfunct_args,
				"%s.waitirq",
				hm2->llio->name)) != 0) {
        HM2_ERR("hal_export waitirq failed - %d\n", r);
        return r;
    }

    // Issue the write to set the registers properly first pass
    hm2_irq_write(hm2);
    return 1;

 fail0:
    return r;
}

void hm2_irq_write(hostmot2_t *hm2) {
    hm2_irq_pins_t *pins;
    u32 buff;

    if(hm2->irq.num_instances == 0) return;

    pins = hm2->irq.pins;

    if(*(pins->current_rate_nsec) != *(pins->desired_rate_nsec)) {
        // mask and clear interrupt
        buff = 0;
        hm2->llio->write(hm2->llio, HM2_IRQ_STATUS_REG, &buff, sizeof(u32));

        // Set up the dpll with the new period
        hm2_dpll_write(hm2, *(pins->desired_rate_nsec));

        // unmask interrupt, and select timer
        buff = (hm2->irq.dpll_timer_num << 2) | HM2_IRQ_MASK;
        hm2->llio->write(hm2->llio, HM2_IRQ_STATUS_REG, &buff, sizeof(u32));
        *(pins->current_rate_nsec) = *(pins->desired_rate_nsec);
    }
}

static int hm2_waitirq(void *void_hm2, const hal_funct_args_t *fa) {
    hostmot2_t *hm2 = void_hm2;

    u32 info;
    ssize_t nb;

    info = 1; /* unmask */

    nb = write(hm2->llio->irq_fd, &info, sizeof(info));
    if (nb < sizeof(info)) {
	    *(hm2->irq.pins->write_errors) += 1;
    }

    info = 0;
    // wait for IRQ
    nb = read(hm2->llio->irq_fd, &info, sizeof(info));
    if (nb != sizeof(info)) {
	    *(hm2->irq.pins->read_errors) += 1;
    }
    *(hm2->irq.pins->count) += 1;
    *(hm2->irq.pins->missed) = info - *(hm2->irq.pins->count);

    // clear pending IRQ
    hm2->llio->write(hm2->llio, HM2_CLEAR_IRQ_REG, &zero, sizeof(zero));

    return 0;
}
