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

#ifndef __HOSTMOT2_UART_H
#define __HOSTMOT2_UART_H

#include <rtapi.h>

RTAPI_BEGIN_DECLS

int hm2_uart_setup(char *name, int bitrate, rtapi_s32 tx_mode, rtapi_s32 rx_mode);
int hm2_uart_send(char *name, unsigned char data[], int count);
int hm2_uart_read(char *name, unsigned char data[]);

int hm2_pktuart_setup(char *name, int bitrate, rtapi_s32 tx_mode, rtapi_s32 rx_mode, int txclear, int rxclear); // deprecated after v2
int hm2_pktuart_setup_rx(char *name, unsigned int bitrate, unsigned int filter_hz, unsigned int parity, int frame_delay, bool rx_enable, bool rx_mask);
int hm2_pktuart_setup_tx(char *name, unsigned int bitrate, unsigned int parity, int frame_delay, bool drive_enable, bool drive_auto, int enable_delay);
void hm2_pktuart_reset(char *name);
int hm2_pktuart_send(char *name, const unsigned char data[], rtapi_u8 *num_frames, const rtapi_u16 frame_sizes[]);
int hm2_pktuart_read(char *name, unsigned char data[],  rtapi_u8 *num_frames, rtapi_u16 *max_frame_length, rtapi_u16 frame_sizes[]);
int hm2_pktuart_queue_get_frame_sizes(char *name, rtapi_u32 fsizes[]);
int hm2_pktuart_queue_read_data(char *name, rtapi_u32 *data, int bytes);
int hm2_pktuart_get_clock(char *name);
int hm2_pktuart_get_version(char *name);
rtapi_u32 hm2_pktuart_get_rx_status(char *name);
rtapi_u32 hm2_pktuart_get_tx_status(char *name);

int hm2_bspi_setup_chan(char *name, int chan, int cs, int bits, double mhz,
                        int delay, int cpol, int cpha, int noclear, int noecho,
                        int samplelate);
int hm2_bspi_set_read_function(char *name, int (*func)(void *subdata), void *subdata);
int hm2_bspi_set_write_function(char *name, int (*func)(void *subdata), void *subdata);
int hm2_bspi_write_chan(char* name, int chan, rtapi_u32 val);
int hm2_allocate_bspi_tram(char* name);
int hm2_tram_add_bspi_frame(char *name, int chan, rtapi_u32 **wbuff, rtapi_u32 **rbuff);
int hm2_bspi_clear_fifo(char * name);

RTAPI_END_DECLS

#endif
