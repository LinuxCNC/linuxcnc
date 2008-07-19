
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

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include "rtapi.h"
#include "hal.h"


#define LL_PRINT(level, fmt, args...)   rtapi_print_msg(level, HM2_LLIO_NAME ": " fmt, ## args);
#define LL_DEBUG(enable, fmt, args...)  if (enable) { rtapi_print_msg(RTAPI_MSG_DBG, HM2_LLIO_NAME ": " fmt, ## args); }

#define LL_ERR(fmt, args...)            rtapi_print_msg(RTAPI_MSG_ERR,  HM2_LLIO_NAME ": " fmt, ## args);
#define LL_WARN(fmt, args...)           rtapi_print_msg(RTAPI_MSG_WARN, HM2_LLIO_NAME ": " fmt, ## args);
#define LL_INFO(fmt, args...)           rtapi_print_msg(RTAPI_MSG_INFO, HM2_LLIO_NAME ": " fmt, ## args);
#define LL_DBG(fmt, args...)            rtapi_print_msg(RTAPI_MSG_DBG,  HM2_LLIO_NAME ": " fmt, ## args);

#define THIS_ERR(fmt, args...)            rtapi_print_msg(RTAPI_MSG_ERR,  "%s: " fmt, this->name, ## args);
#define THIS_WARN(fmt, args...)           rtapi_print_msg(RTAPI_MSG_WARN, "%s: " fmt, this->name, ## args);
#define THIS_INFO(fmt, args...)           rtapi_print_msg(RTAPI_MSG_INFO, "%s: " fmt, this->name, ## args);
#define THIS_DBG(fmt, args...)            rtapi_print_msg(RTAPI_MSG_DBG,  "%s: " fmt, this->name, ## args);




// 
// this struct holds an abstract "low-level I/O" driver
//

typedef struct hm2_lowlevel_io_struct hm2_lowlevel_io_t;

// FIXME: this is really a lowlevel io *instance*, or maybe a "board"
struct hm2_lowlevel_io_struct {
    char name[HAL_NAME_LEN];
    int comp_id;

    // on success these two return TRUE (not zero)
    // on failure they return FALSE (0) and set *self->io_error (below) to TRUE
    int (*read)(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size);
    int (*write)(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size);

    // 
    // This is a HAL parameter allocated and added to HAL by hostmot2.
    // 
    // * The llio driver sets it whenever it detects an I/O error.
    // 
    // * The hostmot2 driver checks it and stops calling the llio driver if
    //   it's true.
    // 
    // * Users can clear it (by poking the HAL parameter), which makes the
    //   hostmot2 driver call into llio to reset the hardware and start
    //   driving it again.
    // 
    hal_bit_t *io_error;

    // this gets set to TRUE when the llio driver detects an io_error, and
    // by the hm2 watchdog (if present) when it detects a watchdog bite
    int needs_reset;

    // the names of the io port connectors on this board
    int num_ioport_connectors;
    const char *ioport_connector_name[8];  // no more than 8 connectors yet...

    void *private;  // for the low-level driver to hang their struct on
};




// 
// HostMot2 functions for the low-level I/O drivers to call
//

int hm2_register(hm2_lowlevel_io_t *llio, char *config);
void hm2_unregister(hm2_lowlevel_io_t *llio);


