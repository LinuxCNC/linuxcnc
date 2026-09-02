/*    This is a component of LinuxCNC
 *    Copyright 2013,2014 Michael Geszkiewicz <micges@wp.pl>,
 *    Jeff Epler <jepler@unpythonic.net>
 *
 *    Xenomai Port:
 *    Copyright 2026 Hannes Diethelm <hannes.diethelm@gmail.com>
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

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

#include <rtapi.h>
#include <rtapi_string.h>

#include "hostmot2-lowlevel.h"
#include "hm2_eth_net_xenomai.h"

#define SEND_TIMEOUT_US 10
#define RECV_TIMEOUT_US 10

/// ethernet io functions

int hm2_xenomai_init_board(hm2_eth_t *board, const char *board_ip) {
    int ret;
    LL_PRINT("%s: INFO: init board (XENOMAI)\n", board_ip);
    board->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (board->sockfd < 0) {
        LL_PRINT("ERROR: can't open socket: %s\n", strerror(errno));
        return -errno;
    }

    //Check if socket is rtnet: F_GETFD will
    //fail due to this is only supported on normal
    //sockets. Nicer alternatives are welcome but
    //it looks to be the only option.
    //See: kernel/cobalt/rtdm/fd.c
    if( fcntl(board->sockfd, F_GETFD) >= 0 ){
        LL_PRINT("ERROR: Socket is not realtime\n"
        "    Read hm2_eth man page how to enable realtime\n"
        "    ethernet for Xenomai\n");
        return -1;
    }

    board->server_addr.sin_family = AF_INET;
    board->server_addr.sin_port = htons(LBP16_UDP_PORT);
    board->server_addr.sin_addr.s_addr = inet_addr(board_ip);

    board->local_addr.sin_family      = AF_INET;
    board->local_addr.sin_addr.s_addr = INADDR_ANY;

    ret = connect(board->sockfd, (struct sockaddr *) &board->server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        LL_PRINT("ERROR: can't connect: %s\n", strerror(errno));
        return -errno;
    }

    strncpy(board->ip, board_ip, sizeof(board->ip)-1);

    memset(&board->req, 0, sizeof(board->req));
    struct sockaddr_in *sin;

    sin = (struct sockaddr_in *) &board->req.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(board_ip);

    board->req.arp_ha.sa_family = AF_LOCAL;
    board->req.arp_flags = ATF_PERM | ATF_COM;
    ret = hm2_eth_fetch_hwaddr( board, (void*)&board->req.arp_ha.sa_data );
    if(ret < 0) {
        LL_PRINT("ERROR: Could not retrieve hardware address (MAC) of %s: %s\n", board_ip, strerror(-ret));
        return ret;
    }

    board->write_packet_ptr = board->write_packet;
    board->read_packet_ptr = board->read_packet;
    board->needs_firewall = false;
    
    return 0;
}

int hm2_xenomai_close_board(hm2_eth_t *board) {
    int ret;
    board->llio.reset(&board->llio);

    ret = close(board->sockfd);
    if (ret == -1)
        LL_PRINT("ERROR: can't close socket: %s\n", strerror(errno));

    return ret < 0 ? -errno : 0;
}

int hm2_xenomai_eth_socket_send(hm2_eth_t *board, const void *buffer, int len) {
    return send(board->sockfd, buffer, len, 0);
}

int hm2_xenomai_eth_socket_recv(hm2_eth_t *board, void *buffer, int len, int recv_timeout_ns) {
    fd_set rfds;
    struct timeval tv;
    int ret;

    //ppoll is not suported by xenomai, use select
    FD_ZERO(&rfds);
    FD_SET(board->sockfd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = recv_timeout_ns;
    ret = select(board->sockfd+1, &rfds, NULL, NULL, &tv);

    if (ret < 0) {
        LL_PRINT("ERROR: select() failed: %m\n");
    } else if(ret) {
        ret = recv(board->sockfd, buffer, len, 0);
    } else {
        errno = EAGAIN;
        ret = -1;
    }

    return ret;
}
