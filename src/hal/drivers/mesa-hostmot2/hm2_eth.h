/*    This is a component of LinuxCNC
 *    Copyright 2013,2014 Michael Geszkiewicz <micges@wp.pl>,
 *    Jeff Epler <jepler@unpythonic.net>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __INCLUDE_HM2_ETH_H
#define __INCLUDE_HM2_ETH_H

#include "lbp16.h"

#define MAX_ETH_BOARDS 4

#define HM2_ETH_VERSION "0.2"
#define HM2_LLIO_NAME "hm2_eth"

#define MAX_ETH_READS 64

typedef struct {
    void *buffer;
    int size;
    int from;
} hm2_read_queue_entry_t;

typedef struct {
    hm2_lowlevel_io_t llio;

    int sockfd;
    struct sockaddr_in local_addr;
    struct sockaddr_in server_addr;

    rtapi_u8 read_packet[1400];
    rtapi_u8 *read_packet_ptr;
    hm2_read_queue_entry_t queue_reads[MAX_ETH_READS];
    int queue_reads_count;
    int queue_buff_size;

    rtapi_u8 write_packet[1400];
    rtapi_u8 *write_packet_ptr;
    int write_packet_size;
    uint32_t read_cnt, write_cnt;
    // these two fields must be kept together, they're read by a single
    // read-request
    uint32_t confirm_read_cnt, confirm_write_cnt;

    int comm_error_counter;
    uint16_t old_rxudpcount, rxudpcount;
    struct arpreq req;

    struct {
        hal_s32_t read_timeout;
        hal_s32_t packet_error_limit;
        hal_s32_t packet_error_increment;
        hal_s32_t packet_error_decrement;
        hal_bit_t *packet_error;
        hal_s32_t *packet_error_level;
        hal_bit_t *packet_error_exceeded;
    } *hal;
} hm2_eth_t;

#endif
