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
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "config_module.h"
#include RTAPI_INC_SLAB_H
#include RTAPI_INC_CTYPE_H

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "hal.h"

#include "hostmot2-lowlevel.h"
#include "hostmot2.h"
#include "hm2_eth.h"
#include "lbp16.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Geszkiewicz");
MODULE_DESCRIPTION("Driver for HostMot2 on the 7i80 Anything I/O board from Mesa Electronics");
#ifdef MODULE_SUPPORTED_DEVICE
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-7i80");
#endif

static char *board_ip;
RTAPI_MP_STRING(board_ip, "ip address of ethernet board(s)");

static char *config[MAX_ETH_BOARDS];
RTAPI_MP_ARRAY_STRING(config, MAX_ETH_BOARDS, "config string for the AnyIO boards (see hostmot2(9) manpage)")

int debug = 0;
RTAPI_MP_INT(debug, "Developer/debug use only!  Enable debug logging.");

static hm2_eth_t boards[MAX_ETH_BOARDS];
static int boards_count = 0;

int comm_active = 0;

static int comp_id;

#define UDP_PORT 27181
#define SEND_TIMEOUT_US 10
#define RECV_TIMEOUT_US 10
#define READ_PCK_DELAY_NS 10000

static int sockfd = -1;
static struct sockaddr_in local_addr;
static struct sockaddr_in server_addr;

static lbp16_cmd_addr read_packet;

read_queue_entry_t queue_reads[MAX_ETH_READS];
lbp16_cmd_addr queue_packets[MAX_ETH_READS];
int queue_reads_count = 0;
int queue_buff_size = 0;

static u8 write_packet[1400];
void *write_packet_ptr = &write_packet;
int write_packet_size = 0;
int read_cnt = 0;
int write_cnt = 0;

/// ethernet io functions

struct arpreq req;
static int eth_socket_send(int sockfd, const void *buffer, int len, int flags);
static int eth_socket_recv(int sockfd, void *buffer, int len, int flags);

#define IPTABLES "/sbin/iptables"
#define CHAIN "hm2-eth-rules-output"

static int shell(char *command) {
    char *const argv[] = {"sh", "-c", command, NULL};
    pid_t pid;
    int res = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
    if(res < 0) perror("posix_spawn");
    int status;
    waitpid(pid, &status, 0);
    if(WIFEXITED(status)) return WEXITSTATUS(status);
    else if(WIFSTOPPED(status)) return WTERMSIG(status)+128;
    else return status;
}

// Assume we can use iptables if this chain exists
static bool _use_iptables() {
    if(geteuid() != 0) return 0;
    int result =
        shell(IPTABLES" -n -L "CHAIN" > /dev/null 2>&1");
    return result == EXIT_SUCCESS;
}

static int iptables_state = -1;
static bool use_iptables() {
    if(iptables_state == -1) iptables_state = _use_iptables();
    return iptables_state;
}

static void clear_iptables() {
    shell(IPTABLES" -F "CHAIN" > /dev/null 2>&1");
}

static char* inet_ntoa_buf(struct in_addr in, char *buf, size_t n) {
    const char *addr = inet_ntoa(in);
    snprintf(buf, n, "%s", addr);
    return buf;
}

static char* fetch_ifname(struct sockaddr_in srcaddr, char *buf, size_t n) {
    struct ifaddrs *ifa, *it;

    if(getifaddrs(&ifa) < 0) return NULL;

    for(it=ifa; it; it=it->ifa_next) {
        struct sockaddr_in *ifaddr = (struct sockaddr_in*)it->ifa_addr;
        if(ifaddr->sin_family != srcaddr.sin_family) continue;
        if(ifaddr->sin_addr.s_addr != srcaddr.sin_addr.s_addr) continue;
        snprintf(buf, n, "%s", it->ifa_name);
        freeifaddrs(ifa);
        return buf;
    }

    freeifaddrs(ifa);
    return NULL;
}

static char *vseprintf(char *buf, char *ebuf, const char *fmt, va_list ap) {
    int result = vsnprintf(buf, ebuf-buf, fmt, ap);
    if(result < 0) return ebuf;
    else if(buf + result > ebuf) return ebuf;
    else return buf + result;
}

static char *seprintf(char *buf, char *ebuf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *result = vseprintf(buf, ebuf, fmt, ap);
    va_end(ap);
    return result;
}

static int install_iptables_rule(const char *fmt, ...) {
    char commandbuf[1024], *ptr = commandbuf,
        *ebuf = commandbuf + sizeof(commandbuf);
    ptr = seprintf(ptr, ebuf, IPTABLES" -A "CHAIN" ");
    va_list ap;
    va_start(ap, fmt);
    ptr = vseprintf(ptr, ebuf, fmt, ap);
    va_end(ap);

    if(ptr == ebuf)
    {
        LL_PRINT("ERROR: commandbuf too small\n");
        return -ENOSPC;
    }

    int res = shell(commandbuf);
    if(res == EXIT_SUCCESS) return 0;

    LL_PRINT("ERROR: Failed to execute '%s'\n", commandbuf);
    return -EINVAL;
}

static int install_iptables(int sockfd) {
    struct sockaddr_in srcaddr, dstaddr;
    char srchost[16], dsthost[16]; // enough for 255.255.255.255\0
    char ifbuf[64]; // more than enough for eth0

    socklen_t addrlen = sizeof(srcaddr);
    int res = getsockname(sockfd, &srcaddr, &addrlen);
    if(res < 0) return -errno;

    addrlen = sizeof(dstaddr);
    res = getpeername(sockfd, &dstaddr, &addrlen);
    if(res < 0) return -errno;

    res = install_iptables_rule(
        "-p udp -m udp -d %s --dport %d -s %s --sport %d -j ACCEPT",
        inet_ntoa_buf(dstaddr.sin_addr, dsthost, sizeof(dsthost)),
        ntohs(dstaddr.sin_port),
        inet_ntoa_buf(srcaddr.sin_addr, srchost, sizeof(srchost)),
        ntohs(srcaddr.sin_port));
    if(res < 0) return res;

    // without this rule, 'ping' spews a lot of messages like
    //    From 192.168.1.1 icmp_seq=5 Packet filtered
    // many times for each ping packet sent.  With this rule,
    // ping prints 'ping: sendmsg: Operation not permitted' once
    // per second.
    res = install_iptables_rule(
        "-o %s -p icmp -j DROP",
        fetch_ifname(srcaddr, ifbuf, sizeof(ifbuf)));
    if(res < 0) return res;

    res = install_iptables_rule(
        "-o %s -j REJECT --reject-with icmp-admin-prohibited",
        ifbuf);
    if(res < 0) return res;

    return 0;
}

static int fetch_hwaddr(int sockfd, unsigned char buf[6]) {
    lbp16_cmd_addr packet;
    unsigned char response[6];
    LBP16_INIT_PACKET4(packet, 0x4983, 0x0002);
    int res = eth_socket_send(sockfd, &packet, sizeof(packet), 0);
    if(res < 0) return -errno;

    int i=0;
    do {
        res = eth_socket_recv(sockfd, &response, sizeof(response), 0);
    } while(++i < 10 && res < 0 && errno == EAGAIN);
    if(res < 0) return -errno;

    // eeprom order is backwards from arp AF_LOCAL order
    for(i=0; i<6; i++) buf[i] = response[5-i];

    LL_PRINT("Hardware address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

    return 0;
}

static int init_net(void) {
    int ret;

    sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sockfd < 0) {
        LL_PRINT("ERROR: can't open socket: %s\n", strerror(errno));
        return -errno;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LBP16_UDP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(board_ip);

    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;

    ret = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        LL_PRINT("ERROR: can't connect: %s\n", strerror(errno));
        return -errno;
    }

    if(use_iptables()) {
        LL_PRINT("Using iptables for exclusive access to network interface\n")
        // firewall has to be open in order to successfully arp the board
        clear_iptables();
    } else {
        LL_PRINT(\
"WARNING: Unable to restrict other access to the hm2-eth device.\n"
"This means that other software using the same network interface can violate\n"
"realtime guarantees.  See hm2_eth(9) for more information.\n");
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RECV_TIMEOUT_US;

    ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set socket option: %s\n", strerror(errno));
        return -errno;
    }

    timeout.tv_usec = SEND_TIMEOUT_US;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set socket option: %s\n", strerror(errno));
        return -errno;
    }

    memset(&req, 0, sizeof(req));
    struct sockaddr_in *sin;

    sin = (struct sockaddr_in *) &req.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(board_ip);

    req.arp_ha.sa_family = AF_LOCAL;
    req.arp_flags = ATF_PERM | ATF_COM;
    ret = fetch_hwaddr( sockfd, (void*)&req.arp_ha.sa_data );
    if(ret < 0) {
        LL_PRINT("ERROR: Could not retrieve mac address\n");
        return ret;
    }

    ret = ioctl(sockfd, SIOCSARP, &req);
    if(ret < 0) {
        perror("ioctl SIOCSARP");
        req.arp_flags &= ~ATF_PERM;
        return -errno;
    }

    if(use_iptables())
    {
        ret = install_iptables(sockfd);
        if(ret < 0) return ret;
    }

    return 0;
}

static int close_net(void) {
    if(use_iptables()) clear_iptables();

    if(req.arp_flags & ATF_PERM) {
        int ret = ioctl(sockfd, SIOCDARP, &req);
        if(ret < 0) perror("ioctl SIOCDARP");
    }
    int ret = shutdown(sockfd, SHUT_RDWR);
    if (ret < 0)
        LL_PRINT("ERROR: can't close socket: %s\n", strerror(errno));

    return ret < 0 ? -errno : 0;
}

static int eth_socket_send(int sockfd, const void *buffer, int len, int flags) {
    return send(sockfd, buffer, len, flags);
}

static int eth_socket_recv(int sockfd, void *buffer, int len, int flags) {
    return recv(sockfd, buffer, len, flags);
}

/// hm2_eth io functions

static int hm2_eth_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int send, recv, i = 0;
    u8 tmp_buffer[size + 4];
    long long t1, t2;

    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    read_cnt++;

    LBP16_INIT_PACKET4(read_packet, CMD_READ_HOSTMOT2_ADDR32_INCR(size/4), addr & 0xFFFF);

    send = eth_socket_send(sockfd, (void*) &read_packet, sizeof(read_packet), 0);
    if(send < 0)
        LL_PRINT("ERROR: sending packet: %s\n", strerror(errno));
    LL_PRINT_IF(debug, "read(%d) : PACKET SENT [CMD:%02X%02X | ADDR: %02X%02X | SIZE: %d]\n", read_cnt, read_packet.cmd_hi, read_packet.cmd_lo,
      read_packet.addr_lo, read_packet.addr_hi, size);
    t1 = rtapi_get_time();
    do {
        rtapi_delay(READ_PCK_DELAY_NS);
        recv = eth_socket_recv(sockfd, (void*) &tmp_buffer, size, 0);
        t2 = rtapi_get_time();
        i++;
    } while ((recv < 0) && ((t2 - t1) < 200*1000*1000));

    if (recv == 4) {
        LL_PRINT_IF(debug, "read(%d) : PACKET RECV [DATA: %08X | SIZE: %d | TRIES: %d | TIME: %llu]\n", read_cnt, *tmp_buffer, recv, i, t2 - t1);
    } else {
        LL_PRINT_IF(debug, "read(%d) : PACKET RECV [SIZE: %d | TRIES: %d | TIME: %llu]\n", read_cnt, recv, i, t2 - t1);
    }
    if (recv < 0)
        return 0;
    memcpy(buffer, tmp_buffer, size);
    return 1;  // success
}

static int hm2_eth_enqueue_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    if (size == -1) {
        int send, recv, i = 0;
        long long t1, t2;
        u8 tmp_buffer[queue_buff_size];

        read_cnt++;
        send = eth_socket_send(sockfd, (void*) &queue_packets, sizeof(lbp16_cmd_addr)*queue_reads_count, 0);
        if(send < 0)
            LL_PRINT("ERROR: sending packet: %s\n", strerror(errno));
        t1 = rtapi_get_time();
        do {
            rtapi_delay(READ_PCK_DELAY_NS);
            recv = eth_socket_recv(sockfd, (void*) &tmp_buffer, queue_buff_size, 0);
            t2 = rtapi_get_time();
            i++;
        } while ((recv < 0) && ((t2 - t1) < 200*1000*1000));
        LL_PRINT_IF(debug, "enqueue_read(%d) : PACKET RECV [SIZE: %d | TRIES: %d | TIME: %llu]\n", read_cnt, recv, i, t2 - t1);

        for (i = 0; i < queue_reads_count; i++) {
            memcpy(queue_reads[i].buffer, &tmp_buffer[queue_reads[i].from], queue_reads[i].size);
        }

        queue_reads_count = 0;
        queue_buff_size = 0;
    } else {
        LBP16_INIT_PACKET4(queue_packets[queue_reads_count], CMD_READ_HOSTMOT2_ADDR32_INCR(size/4), addr);
        queue_reads[queue_reads_count].buffer = buffer;
        queue_reads[queue_reads_count].size = size;
        queue_reads[queue_reads_count].from = queue_buff_size;
        queue_reads_count++;
        queue_buff_size += size;
    }
    return 1;
}

static int hm2_eth_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int send;
    static struct {
        lbp16_cmd_addr wr_packet;
        u8 tmp_buffer[127*8];
    } packet;

    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    write_cnt++;

    memcpy(packet.tmp_buffer, buffer, size);
    LBP16_INIT_PACKET4(packet.wr_packet, CMD_WRITE_HOSTMOT2_ADDR32_INCR(size/4), addr & 0xFFFF);

    send = eth_socket_send(sockfd, (void*) &packet, sizeof(lbp16_cmd_addr) + size, 0);
    if(send < 0)
        LL_PRINT("ERROR: sending packet: %s\n", strerror(errno));
    LL_PRINT_IF(debug, "write(%d): PACKET SENT [CMD:%02X%02X | ADDR: %02X%02X | SIZE: %d]\n", write_cnt, packet.wr_packet.cmd_hi, packet.wr_packet.cmd_lo,
      packet.wr_packet.addr_lo, packet.wr_packet.addr_hi, size);

    return 1;  // success
}

static int hm2_eth_enqueue_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    if (size == -1) {
        int send;
        long long t0, t1;

        write_cnt++;
        t0 = rtapi_get_time();
        send = eth_socket_send(sockfd, (void*) &write_packet, write_packet_size, 0);
        if(send < 0)
            LL_PRINT("ERROR: sending packet: %s\n", strerror(errno));
        t1 = rtapi_get_time();
        LL_PRINT_IF(debug, "enqueue_write(%d) : PACKET SEND [SIZE: %d | TIME: %llu]\n", write_cnt, send, t1 - t0);
        write_packet_ptr = &write_packet;
        write_packet_size = 0;
    } else {
        lbp16_cmd_addr *packet = (lbp16_cmd_addr *) write_packet_ptr;

        LBP16_INIT_PACKET4_PTR(packet, CMD_WRITE_HOSTMOT2_ADDR32_INCR(size/4), addr);
        write_packet_ptr += sizeof(*packet);
        memcpy(write_packet_ptr, buffer, size);
        write_packet_ptr += size;
        write_packet_size += (sizeof(*packet) + size);
    }
    return 1;
}

static int hm2_eth_probe() {
    int ret, send, recv;
    char board_name[16] = {0, };
    char llio_name[16] = {0, };
    hm2_eth_t *board;

    LBP16_INIT_PACKET4(read_packet, CMD_READ_BOARD_INFO_ADDR16_INCR(16/2), 0);
    send = eth_socket_send(sockfd, (void*) &read_packet, sizeof(read_packet), 0);
    if(send < 0)
        LL_PRINT("ERROR: sending packet: %s\n", strerror(errno));
    recv = eth_socket_recv(sockfd, (void*) &board_name, 16, 0);
    if(recv < 0)
        LL_PRINT("ERROR: receiving packet: %s\n", strerror(errno));

    board = &boards[boards_count];

    if (strncmp(board_name, "7I80DB-16", 9) == 0) {
        strncpy(llio_name, board_name, 4);
        llio_name[1] = tolower(llio_name[1]);

        board->llio.num_ioport_connectors = 4;
        board->llio.pins_per_connector = 17;
        board->llio.ioport_connector_name[0] = "J2";
        board->llio.ioport_connector_name[1] = "J3";
        board->llio.ioport_connector_name[2] = "J4";
        board->llio.ioport_connector_name[3] = "J5";
        board->llio.fpga_part_number = "XC6SLX16";
        board->llio.num_leds = 4;
    } else if (strncmp(board_name, "7I80DB-25", 9) == 0) {
        strncpy(llio_name, board_name, 4);
        llio_name[1] = tolower(llio_name[1]);

        board->llio.num_ioport_connectors = 4;
        board->llio.pins_per_connector = 17;
        board->llio.ioport_connector_name[0] = "J2";
        board->llio.ioport_connector_name[1] = "J3";
        board->llio.ioport_connector_name[2] = "J4";
        board->llio.ioport_connector_name[3] = "J5";
        board->llio.fpga_part_number = "XC6SLX25";
        board->llio.num_leds = 4;
    } else if (strncmp(board_name, "7I80HD-16", 9) == 0) {
        strncpy(llio_name, board_name, 4);
        llio_name[1] = tolower(llio_name[1]);

        board->llio.num_ioport_connectors = 3;
        board->llio.pins_per_connector = 24;
        board->llio.ioport_connector_name[0] = "P1";
        board->llio.ioport_connector_name[1] = "P2";
        board->llio.ioport_connector_name[2] = "P3";
        board->llio.fpga_part_number = "XC6SLX16";
        board->llio.num_leds = 4;
    } else if (strncmp(board_name, "7I80HD-25", 9) == 0) {
        strncpy(llio_name, board_name, 4);
        llio_name[1] = tolower(llio_name[1]);

        board->llio.num_ioport_connectors = 3;
        board->llio.pins_per_connector = 24;
        board->llio.ioport_connector_name[0] = "P1";
        board->llio.ioport_connector_name[1] = "P2";
        board->llio.ioport_connector_name[2] = "P3";
        board->llio.fpga_part_number = "XC6SLX25";
        board->llio.num_leds = 4;
    } else if (strncmp(board_name, "7I76E-16", 8) == 0) {
        strncpy(llio_name, board_name, 5);
        llio_name[1] = tolower(llio_name[1]);
        llio_name[4] = tolower(llio_name[4]);

        board->llio.num_ioport_connectors = 3;
        board->llio.pins_per_connector = 17;
        board->llio.ioport_connector_name[0] = "P1";
        board->llio.ioport_connector_name[1] = "P2";
        board->llio.ioport_connector_name[2] = "P3";
        board->llio.fpga_part_number = "XC6SLX16";
        board->llio.num_leds = 4;
    } else if (strncmp(board_name, "7I92", 4) == 0) {
        strncpy(llio_name, board_name, 4);
        llio_name[1] = tolower(llio_name[1]);

        board->llio.num_ioport_connectors = 2;
        board->llio.pins_per_connector = 17;
        board->llio.ioport_connector_name[0] = "P2";
        board->llio.ioport_connector_name[1] = "P1";
        board->llio.fpga_part_number = "XC6SLX9";
        board->llio.num_leds = 4;
    } else {
        LL_PRINT("No ethernet board found\n");
        return -ENODEV;
    }

    LL_PRINT("discovered %.*s\n", 16, board_name);

    rtapi_snprintf(board->llio.name, sizeof(board->llio.name), "hm2_%.*s.%d", (int)strlen(llio_name), llio_name, boards_count);

    board->llio.comp_id = comp_id;
    board->llio.private = board;

    board->llio.read = hm2_eth_read;
    board->llio.write = hm2_eth_write;
    board->llio.queue_read = hm2_eth_enqueue_read;
    board->llio.queue_write = hm2_eth_enqueue_write;

    ret = hm2_register(&board->llio, config[boards_count]);
    if (ret != 0) {
        rtapi_print("board fails HM2 registration\n");
        return ret;
    }
    boards_count++;

    int val = fcntl(sockfd, F_GETFL);
    val = val | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, val);

    return 0;
}

int rtapi_app_main(void) {
    int ret;

    LL_PRINT("loading Mesa AnyIO HostMot2 ethernet driver version " HM2_ETH_VERSION "\n");

    ret = hal_init(HM2_LLIO_NAME);
    if (ret < 0)
        goto error0;
    comp_id = ret;

    ret = init_net();
    if (ret < 0)
        goto error1;

    comm_active = 1;

    ret = hm2_eth_probe();

    if (ret < 0)
        goto error1;

    hal_ready(comp_id);

    return 0;

error1:
    close_net();
error0:
    hal_exit(comp_id);
    return ret;
}

void rtapi_app_exit(void) {
    comm_active = 0;
    close_net();
    hal_exit(comp_id);
    LL_PRINT("HostMot2 ethernet driver unloaded\n");
}
