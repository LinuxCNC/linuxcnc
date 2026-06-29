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

#ifndef __INCLUDE_HM2_ETH_NET_POSIX_H
#define __INCLUDE_HM2_ETH_NET_POSIX_H

#include "hm2_eth.h"

int hm2_posix_init_board(hm2_eth_t *board, const char *board_ip);
int hm2_posix_init_board_realtime(hm2_eth_t *board);
int hm2_posix_close_board(hm2_eth_t *board);
int hm2_posix_eth_socket_send(hm2_eth_t *board, const void *buffer, int len, int flags);
int hm2_posix_eth_socket_recv(hm2_eth_t *board, void *buffer, int len, int flags);

#define UDP_PORT 27181
#define SEND_TIMEOUT_US 10
#define RECV_TIMEOUT_US 10

#endif
