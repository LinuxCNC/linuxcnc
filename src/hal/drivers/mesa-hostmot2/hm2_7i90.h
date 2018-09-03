
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

#define HM2_7I90_VERSION "0.3"

#define HM2_LLIO_NAME "hm2_7i90"

#include "hal_parport.h"



//
// EPP stuff
//

#define HM2_7I90_EPP_STATUS_OFFSET   (1)
#define HM2_7I90_EPP_CONTROL_OFFSET  (2)
#define HM2_7I90_EPP_ADDRESS_OFFSET  (3)
#define HM2_7I90_EPP_DATA_OFFSET     (4)

#define HM2_7I90_ECP_CONFIG_A_HIGH_OFFSET  (0)
#define HM2_7I90_ECP_CONFIG_B_HIGH_OFFSET  (1)
#define HM2_7I90_ECP_CONTROL_HIGH_OFFSET   (2)


#define HM2_7I90_ADDR_AUTOINCREMENT (0x8000)

#define HM2_7I90_MAX_BOARDS 4




//
// The Mesa 7i90 struct
//

typedef struct {
    hal_parport_t port;
    int epp_wide;

    hm2_lowlevel_io_t llio;
} hm2_7i90_t;

