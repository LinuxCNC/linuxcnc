//
//    Copyright (C) 2015 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_RTAPI_KMOD_H_
#define _LCEC_RTAPI_KMOD_H_

#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/math64.h>

#define lcec_zalloc(size) kzalloc(size, GFP_KERNEL)
#define lcec_free(ptr) kfree(ptr)

#define lcec_gettimeofday(x) do_gettimeofday(x) 

#define LCEC_MS_TO_TICKS(x) (HZ * x / 1000)
#define lcec_get_ticks() ((long) jiffies)

#define lcec_schedule() schedule()

static inline long long lcec_mod_64(long long val, unsigned long div) {
  s32 rem;
  div_s64_rem(val, div, &rem);
  return rem;
}

#endif

