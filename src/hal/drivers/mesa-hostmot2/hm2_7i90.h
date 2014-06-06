
//
//    Copyright (C) 2013-2014 Sebastian Kuzminsky and Kim Kirwan
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

#define HM2_7I90_VERSION "0.3"

#define HM2_LLIO_NAME "hm2_7i90"

#include "hal/drivers/epp.h"




#define HM2_7I90_ADDR_AUTOINCREMENT (0x8000)

#define HM2_7I90_MAX_BOARDS (4)




//
// The Mesa 7i90 struct
//

typedef struct {
    epp_t epp_port;
    hm2_lowlevel_io_t llio;
} hm2_7i90_t;

