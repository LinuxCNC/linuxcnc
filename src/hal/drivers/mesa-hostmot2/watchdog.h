//    Copyright (C) 2012 Michael Geszkiewicz
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

#ifndef __WATCHDOG_H
#define __WATCHDOG_H

typedef struct {
    struct {

        struct {
            hal_bit_t *has_bit;
        } pin;

        struct {
            hal_u32_t timeout_ns;
        } param;

    } hal;

    u32 written_timeout_ns;

    int enable;  // gets set to 0 at load time, gets set to 1 at first pet_watchdog
    int written_enable;

    // This is a flag to help warn the user if they specify a too-short
    // timeout.  The flag gets set to 0 whenever the user changes the
    // timeout.  The pet_watchdog() funtion checks the requested timeout
    // against the reported period, if if it's dangeriously short it warns
    // about it once, and sets this flag to remind it not to warn again
    // (until the user changes the timeout again).
    int warned_about_short_timeout;
} hm2_watchdog_instance_t;


typedef struct {
    int num_instances;
    hm2_watchdog_instance_t *instance;

    u32 clock_frequency;
    u8 version;

    u32 timer_addr;
    u32 *timer_reg;

    u32 status_addr;
    u32 *status_reg;

    u32 reset_addr;
    u32 *reset_reg;
} hm2_watchdog_t;

#endif
