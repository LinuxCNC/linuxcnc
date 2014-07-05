//    Copyright (C) 2009 Jeff Epler
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

#ifndef HAL_PARPORT_COMMON_H
#define HAL_PARPORT_COMMON_H

#include <rtapi_parport.h>

#define hal_parport_t rtapi_parport_t
#define hal_parport_get(comp_id, port, base, base_hi, modes) \
     rtapi_parport_get(hal_comp_name(comp_id), port, base, base_hi, modes)
#define hal_parport_release rtapi_parport_release

#endif
