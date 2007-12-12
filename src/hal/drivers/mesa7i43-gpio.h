//    Copyright (C) 2007 Sebastian Kuzminsky
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

#ifdef SIM
#include <sys/io.h>
#include <errno.h>
#else
#include <asm/io.h>
#endif


#define M7I43_GPIO_VERSION "0.4"

static void m7i43_nanosleep(unsigned long int nanoseconds);


// 
// EPP stuff
// 

// 
// 50,000 ns works reliably
// 20,000 ns works reliably
//  5,000 ns does not work reliably
// 

#define M7I43_EPP_DELAY (20 * 1000)

#define M7I43_EPP_STATUS_OFFSET   (1)
#define M7I43_EPP_CONTROL_OFFSET  (2)
#define M7I43_EPP_ADDRESS_OFFSET  (3)
#define M7I43_EPP_DATA_OFFSET     (4)

#define M7I43_ECP_CONFIG_A_HIGH_OFFSET  (0)
#define M7I43_ECP_CONFIG_B_HIGH_OFFSET  (1)
#define M7I43_ECP_CONTROL_HIGH_OFFSET   (2)


