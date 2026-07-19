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

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <rtapi.h>
#include <rtapi_string.h>

#include "hostmot2-lowlevel.h"
#include "hm2_eth_net_posix.h"

#define SEND_TIMEOUT_US 10
#define RECV_TIMEOUT_US 10

/// ethernet io functions

int hm2_posix_init_board(hm2_eth_t *board, const char *board_ip) {
    int ret;
    LL_PRINT("%s: INFO: init board (POSIX)\n", board_ip);
    board->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (board->sockfd < 0) {
        LL_PRINT("ERROR: can't open socket: %s\n", strerror(errno));
        return -errno;
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
    char *ifptr = fetch_ifname(board->sockfd, board->ifname, sizeof(board->ifname));
    if(!ifptr) {
        LL_PRINT("failed to retrieve interface name for board\n");
        return 0;
    }

    if(!use_firewall()) {
        LL_PRINT(\
"WARNING: Unable to restrict other access to the hm2-eth device.\n"
"This means that other software using the same network interface can violate\n"
"realtime guarantees.  See hm2_eth(9) for more information.\n");
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RECV_TIMEOUT_US;
    ret = setsockopt(board->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set receive timeout socket option: %s\n", strerror(errno));
        return -errno;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = SEND_TIMEOUT_US;
    ret = setsockopt(board->sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set send timeout socket option: %s\n", strerror(errno));
        return -errno;
    }

    memset(&board->req, 0, sizeof(board->req));
    struct sockaddr_in *sin;

    sin = (struct sockaddr_in *) &board->req.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(board_ip);

    board->req.arp_ha.sa_family = AF_LOCAL;
    board->req.arp_flags = ATF_PERM | ATF_COM;
    ret = fetch_hwaddr( board, (void*)&board->req.arp_ha.sa_data );
    if(ret < 0) {
        LL_PRINT("ERROR: Could not retrieve hardware address (MAC) of %s: %s\n", board_ip, strerror(-ret));
        return ret;
    }

    // Pinning the ARP entry needs CAP_NET_ADMIN; rootless without setcap
    // fails with EPERM.  Best-effort, not fatal: fall back to dynamic ARP
    // so the board still loads.  Clear ATF_PERM so the SIOCDARP teardown
    // in close_board() does not try to remove an entry we never set.
    ret = ioctl(board->sockfd, SIOCSARP, &board->req);
    if(ret < 0) {
        LL_PRINT("WARNING: ioctl SIOCSARP failed: %s; continuing with "
                 "dynamic ARP.  Install file capabilities (sudo make "
                 "setcap) or run setuid to pin the board's ARP entry and "
                 "avoid occasional transmit latency.\n", strerror(errno));
        board->req.arp_flags &= ~ATF_PERM;
    }

    // install_firewall_board() is a no-op when no firewall backend is
    // available (rootless install without CAP_NET_ADMIN, or
    // firewall=none), so it is safe to call unconditionally.
    ret = install_firewall_board(board->sockfd);
    if(ret < 0) return ret;

    board->write_packet_ptr = board->write_packet;
    board->read_packet_ptr = board->read_packet;

    return 0;
}

int hm2_posix_init_board_realtime(hm2_eth_t *board){
    (void)board;
    return 0; //Nothing todo
}

int hm2_posix_close_board(hm2_eth_t *board) {
    int ret;
    board->llio.reset(&board->llio);

    clear_firewall();

    if(board->req.arp_flags & ATF_PERM) {
        ret = ioctl(board->sockfd, SIOCDARP, &board->req);
        if(ret < 0) perror("ioctl SIOCDARP");
    }
    ret = shutdown(board->sockfd, SHUT_RDWR);
    if (ret == -1)
        LL_PRINT("ERROR: can't shutdown socket: %s\n", strerror(errno));
    
    ret = close(board->sockfd);
    if (ret == -1)
        LL_PRINT("ERROR: can't close socket: %s\n", strerror(errno));
    
    return ret < 0 ? -errno : 0;
}

int hm2_posix_eth_socket_send(hm2_eth_t *board, const void *buffer, int len, int flags) {
    return send(board->sockfd, buffer, len, flags);
}

int hm2_posix_eth_socket_recv(hm2_eth_t *board, void *buffer, int len, int flags) {
    return recv(board->sockfd, buffer, len, flags);
}
