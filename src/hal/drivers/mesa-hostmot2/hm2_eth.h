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

typedef struct hm2_eth_t hm2_eth_t;

struct hm2_eth_t {
    hm2_lowlevel_io_t llio;

    int sockfd;
    struct sockaddr_in local_addr;
    struct sockaddr_in server_addr;

    char ip[64];
    char ifname[64];

    //RT network specific functions
    int (*init_board)(hm2_eth_t *board, const char *board_ip);
    int (*close_board)(hm2_eth_t *board);
    int (*eth_socket_send)(hm2_eth_t *board, const void *buffer, int len);
    int (*eth_socket_recv)(hm2_eth_t *board, void *buffer, int len, int recv_timeout_ns);

    rtapi_u8 read_packet[1400];
    rtapi_u8 *read_packet_ptr;
    hm2_read_queue_entry_t queue_reads[MAX_ETH_READS];
    int queue_reads_count;
    int queue_buff_size;

    rtapi_u8 write_packet[1400];
    rtapi_u8 *write_packet_ptr;
    uint32_t read_cnt, write_cnt;
    struct {
        // These two fields must be kept together. They are read by a single
        // queued read-request and retrieve the above read_cnt and write_cnt
        // added by hm2_eth_send_queued_reads() and hm2_eth_send_queued_writes().
        // The values of the two should match or we know that the send/recv
        // packets are out of sync.
        uint32_t read_cnt;
        uint32_t write_cnt;
    } confirm_rw_cnt;
    // Set when a queued write has set write_cnt in the board
    int has_written_cnt;

    int comm_error_counter;
    uint16_t old_rxudpcount, rxudpcount;
    struct arpreq req;

    struct {
        hal_sint_t read_timeout;
        hal_sint_t packet_error_limit;
        hal_sint_t packet_error_increment;
        hal_sint_t packet_error_decrement;
        hal_bool_t packet_error;
        hal_uint_t packet_error_total;
        hal_sint_t packet_error_level;
        hal_bool_t packet_error_exceeded;
    } *hal;
};

bool use_firewall();
int install_firewall_board(int sockfd);
int install_firewall_perinterface(const char *ifbuf);
void clear_firewall();
char* fetch_ifname(int sockfd, char *buf, size_t n);
int fetch_hwaddr(hm2_eth_t *board, unsigned char buf[6]);

#endif
