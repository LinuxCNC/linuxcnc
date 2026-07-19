/*    This is a component of LinuxCNC
 *    Copyright 2013,2014 Michael Geszkiewicz <micges@wp.pl>,
 *    Jeff Epler <jepler@unpythonic.net>
 *
 *    EVL Port:
 *    Copyright 2024 Hannes Diethelm <hannes.diethelm@gmail.com>
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
#include <sys/fsuid.h>

#include <evl/thread.h>
#include <evl/clock.h>
#include <evl/net/net.h>

#include <rtapi.h>
#include <rtapi_string.h>

#include "hostmot2-lowlevel.h"
#include "hm2_eth_net_evl.h"

#define SEND_TIMEOUT_US 10
#define RECV_TIMEOUT_US 10

/// ethernet io functions

static int oob_enable_port(hm2_eth_t *board);
static int oob_disable_port(hm2_eth_t *board);

int hm2_evl_init_board(hm2_eth_t *board, const char *board_ip) {
    int ret;
    LL_PRINT("%s: INFO: init board (Xenomai EVL)\n", board_ip);
    board->is_evl_oob_active = false;
    board->sockfd = socket(PF_INET, SOCK_DGRAM | SOCK_OOB, IPPROTO_IP);
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

    if (setsockopt(board->sockfd, SOL_SOCKET, SO_BINDTODEVICE, board->ifname, strlen(board->ifname))) {
        LL_PRINT("ERROR: can't SO_BINDTODEVICE socket: %s\n", strerror(errno));
    }

    if (!use_firewall()) {
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
    if (ret < 0) {
        LL_PRINT("ERROR: Could not retrieve hardware address (MAC) of %s: %s\n", board_ip, strerror(-ret));
        return ret;
    }

    // install_firewall_board() is a no-op when no firewall backend is
    // available (rootless install without CAP_NET_ADMIN, or
    // firewall=none), so it is safe to call unconditionally.
    ret = install_firewall_board(board->sockfd);
    if (ret < 0) return ret;

    board->write_packet_ptr = board->write_packet;
    board->read_packet_ptr = board->read_packet;

    return 0;
}

int hm2_evl_init_board_realtime(hm2_eth_t *board) {
    static bool error_shown = false;
    if (board->is_evl_oob_active ) {
        if (!error_shown) {
            LL_PRINT("ERROR: realtime-init called while realtime is allready initialized\n"
                     "    You might be you using \"addf hm2_eth.realtime-init servo-thread\"\n"
                     "    instead of \"initf hm2_eth.realtime-init servo-thread\"\n");
            error_shown = true;
        }
        return 0; //No failure in this case
    }
    return oob_enable_port(board);
}

static int oob_enable_port(hm2_eth_t *board) {
    //long long t1, t2;
    int devfd, ret;
    
    LL_PRINT("%s: INFO: enable OOB for board on if %s\n", board->ip, board->ifname);

    //t1 = rtapi_get_time();
    size_t poolsz = 0, bufsz = 0; /* Use defaults. */

    //Need root fsuid for this
    uid_t previous = setfsuid(0);

    devfd = evl_net_open_dev(board->ifname);
    if (devfd < 0) {
        LL_PRINT("ERROR: cannot open out-of-band port %s\n", board->ifname);
        return -1;
    }

    ret = evl_net_enable_port(devfd, poolsz, bufsz);
    if (ret) {
        LL_PRINT("ERROR: cannot enable out-of-band port on %s\n", board->ifname);
        return -1;
    }

    close(devfd);    /* We don't need the fildes of the oob port. */
    
    ret = evl_net_solicit(board->sockfd, (struct sockaddr*)&board->server_addr, EVL_NEIGH_PERMANENT);
    if (ret) {
        LL_PRINT("solicit did not respond\n");
        return -1;
    }

    setfsuid(previous);

    /*t2 = rtapi_get_time();
    LL_PRINT("Enable dur = %lli\n", t2-t1);*/

    board->is_evl_oob_active=true;

    return 0;
}

static int oob_disable_port(hm2_eth_t *board) {
    (void)board;
    int ret, devfd;

    LL_PRINT("%s: INFO: disable OOB for board on if %s\n", board->ip, board->ifname);

    //Need root fsuid for this
    uid_t previous = setfsuid(0);

    devfd = evl_net_open_dev(board->ifname);
    if (devfd < 0) {
        LL_PRINT("ERROR: cannot open out-of-band port %s\n", board->ifname);
        return -1;
    }

    ret = evl_net_disable_port(devfd);
    if (ret < 0) {
        LL_PRINT("ERROR: cannot disable out-of-band port on %s\n", board->ifname);
        return -1;
    }

    close(devfd);

    setfsuid(previous);

    board->is_evl_oob_active=false;

    return 0;
}

int hm2_evl_close_board(hm2_eth_t *board) {
    int ret;
    oob_disable_port(board);

    board->llio.reset(&board->llio);

    clear_firewall();

    ret = close(board->sockfd);
    if (ret == -1)
        LL_PRINT("ERROR: can't close socket: %s\n", strerror(errno));
    
    return ret < 0 ? -errno : 0;
}

/*
static void print_addr(char* desc, struct sockaddr_in *addr){
    char ip_str[INET_ADDRSTRLEN+1];
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, sizeof(ip_str));
    LL_PRINT("--------%s--------\n", desc);
    LL_PRINT("IP-Address: %s\n", ip_str);
    LL_PRINT("Port:       %d\n", ntohs(addr->sin_port));
    LL_PRINT("Family:     %d\n", ntohs(addr->sin_family));
}
*/

static int check_evl(hm2_eth_t *board) {
    static bool error_shown = false;
    if (!board->is_evl_oob_active && evl_get_self() >= 0) {
        if (!error_shown) {
            LL_PRINT("ERROR: hm2_eth evl mode: OOB not active in realtime thread\n"
                     "    Please add: \"initf hm2_eth.realtime-init servo-thread\"\n"
                     "    to your hal file\n");
            error_shown = true;
        }
        return -1;
    } else {
        return 0;
    }
}

int hm2_evl_eth_socket_send(hm2_eth_t *board, const void *buffer, int len, int flags) {
    ssize_t ret = 0;
    ret = check_evl(board);
    if (ret < 0) {
        return ret;
    }
    if (board->is_evl_oob_active) {
        struct iovec iov;
        struct oob_msghdr msghdr;
        struct timespec ts_timeout;

        evl_read_clock(EVL_CLOCK_MONOTONIC, &ts_timeout);
        ts_timeout.tv_nsec += 1000*SEND_TIMEOUT_US;
        while (ts_timeout.tv_nsec >= 1000000000) {
            ts_timeout.tv_nsec -= 1000000000;
            ts_timeout.tv_sec ++;
        }

        /* OUTPUT */
        iov.iov_base = (void *)buffer;
        iov.iov_len = len;
        msghdr.msg_iov = &iov;
        msghdr.msg_iovlen = 1;
        msghdr.msg_control = NULL;
        msghdr.msg_controllen = 0;
        msghdr.msg_name = &board->server_addr;
        msghdr.msg_namelen = sizeof(board->server_addr);
        /*msghdr.msg_name = NULL;
        msghdr.msg_namelen = 0;*/
        msghdr.msg_flags = 0;
        ret = oob_sendmsg(board->sockfd, &msghdr, &ts_timeout, flags);
        if (ret == -1) {
            LL_PRINT("ERROR: oob_sendmsg %m %i %li\n", errno, ret);
        }
    } else {
        ret = send(board->sockfd, buffer, len, flags);
    }
    return ret;
}

int hm2_evl_eth_socket_recv(hm2_eth_t *board, void *buffer, int len, int flags) {
    ssize_t ret = 0;
    ret = check_evl(board);
    if (ret < 0) {
        return ret;
    }
    if (board->is_evl_oob_active) {
        struct oob_msghdr msghdr;
        struct iovec iov;
        struct timespec ts_timeout;

        evl_read_clock(EVL_CLOCK_MONOTONIC, &ts_timeout);
        ts_timeout.tv_nsec += 1000*RECV_TIMEOUT_US;
        while (ts_timeout.tv_nsec >= 1000000000) {
            ts_timeout.tv_nsec -= 1000000000;
            ts_timeout.tv_sec ++;
        }

        /* INPUT */
        iov.iov_base = buffer;
        iov.iov_len = len;
        msghdr.msg_iov = &iov;
        msghdr.msg_iovlen = 1;
        msghdr.msg_control = NULL;
        msghdr.msg_controllen = 0;
        msghdr.msg_name = &board->local_addr;
        msghdr.msg_namelen = sizeof(board->local_addr);
        /*msghdr.msg_name = NULL;
        msghdr.msg_namelen = 0;*/
        msghdr.msg_flags = 0;
        ret = oob_recvmsg(board->sockfd, &msghdr, &ts_timeout, flags);
    } else {
        ret = recv(board->sockfd, buffer, len, flags);
    }
    return ret;
}
