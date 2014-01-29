
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "config_module.h"
#include RTAPI_INC_SLAB_H
#include RTAPI_INC_CTYPE_H
#include RTAPI_INC_STRING_H

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "hal.h"

#include "hostmot2-lowlevel.h"
#include "hostmot2.h"
#include "hm2_eth.h"
#include "lbp16.h"

//#include "/usr/rtnet/include/rtnet.h"
//#include <native/task.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Geszkiewicz");
MODULE_DESCRIPTION("Driver for HostMot2 on the 7i80 Anything I/O board from Mesa Electronics");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-7i80");

static char *board_ip;
RTAPI_MP_STRING(board_ip, "ip address of ethernet board(s)");

static char *board_mac;
RTAPI_MP_STRING(board_mac, "mac address of ethernet board(s)");

static char *config[MAX_ETH_BOARDS];
RTAPI_MP_ARRAY_STRING(config, MAX_ETH_BOARDS, "config string for the AnyIO boards (see hostmot2(9) manpage)")

int debug = 0;
RTAPI_MP_INT(debug, "Developer/debug use only!  Enable debug logging.");

static hm2_eth_t boards[MAX_ETH_BOARDS];
static int boards_count = 0;

int probe_fail = 0;
int comm_active = 0;

static int comp_id;

//RT_TASK rt_probe_task;

#define UDP_PORT 27181
#define RCV_TIMEOUT 200000

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

static int init_net(void) {
    int ret;

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LL_PRINT("ERROR: can't open socket: %s\n", strerror(errno));
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LBP16_UDP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(board_ip);

    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;

    ret = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        LL_PRINT("ERROR: can't connect: %s\n", strerror(errno));
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;

    ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set socket option: %s\n", strerror(errno));
        return -1;
    }
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    if (ret < 0) {
        LL_PRINT("ERROR: can't set socket option: %s\n", strerror(errno));
        return -1;
    }

    struct arpreq req;
    memset(&req, 0, sizeof(req));
    struct sockaddr_in *sin;
    u32 v[6];
    int i;
    u8 *ptr = (u8 *) &req.arp_ha.sa_data;

    sin = (struct sockaddr_in *) &req.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(board_ip);
    sscanf(board_mac, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    for (i = 0; i < 6; i++)
        *ptr++ = v[i];
    req.arp_flags = ATF_PERM | ATF_COM;

    ret = ioctl(sockfd, SIOCSARP, &req);

    //setsockopt (sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    //setsockopt (sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));

    return 0;
}

static int close_net(void) {
    int ret = shutdown(sockfd, SHUT_RDWR);
    if (ret < 0)
        LL_PRINT("ERROR: can't close socket: %s\n", strerror(errno));

    return ret;
}

/*
static int init_rtnet(void) {
    int ret;
    int64_t timeout = RCV_TIMEOUT;

    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    // Set address information structures
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;

    server_addr.sin_family      = AF_INET;
    inet_aton(board_ip, &server_addr.sin_addr);
    server_addr.sin_port        = htons(UDP_PORT);

   // Create new socket. 
    sockfd = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Error opening socket: %d\n", sockfd);
        rt_dev_close(sockfd);
        return sockfd;
    }
    ret = rt_dev_ioctl(sockfd, RTNET_RTIOC_TIMEOUT, &timeout);
    if (ret < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Setting socket option failed with error %d", ret);
        return ret;
    }

    ret = rt_dev_bind(sockfd, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Binding to socket %d failed!\n", 10000);
        return ret;
    }

    ret = rt_dev_connect(sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Connect to socket failed with error %d\n", ret);
        return ret;
    }
    
    return 0;
}        

static int close_rtnet(void) {
    int ret;

    ret = rt_dev_close(sockfd);
    rtapi_print("Close RTNET %d\n", ret);
    return 0;
}
*/

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
    long long t0, t1, t2;

    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    read_cnt++;

    LBP16_INIT_PACKET4(read_packet, CMD_READ_HOSTMOT2_ADDR32_INCR(size/4), addr & 0xFFFF);

    t0 = rtapi_get_time();
    send = eth_socket_send(sockfd, (void*) &read_packet, sizeof(read_packet), 0);
    LL_PRINT_IF(debug, "read(%d) : PACKET SENT [CMD:%02X%02X | ADDR: %02X%02X | SIZE: %d]\n", read_cnt, read_packet.cmd_hi, read_packet.cmd_lo, 
      read_packet.addr_lo, read_packet.addr_hi, size);
    t1 = rtapi_get_time();
    do {
        rtapi_delay(10000);
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
        int send, recv, i;
        u8 tmp_buffer[queue_buff_size];
    
        read_cnt++;
        send = eth_socket_send(sockfd, (void*) &queue_packets, sizeof(lbp16_cmd_addr)*queue_reads_count, 0);
        recv = eth_socket_recv(sockfd, (void*) &tmp_buffer, queue_buff_size, 0);
        
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
    LL_PRINT_IF(debug, "write(%d): PACKET SENT [CMD:%02X%02X | ADDR: %02X%02X | SIZE: %d]\n", write_cnt, packet.wr_packet.cmd_hi, packet.wr_packet.cmd_lo, 
      packet.wr_packet.addr_lo, packet.wr_packet.addr_hi, size);

    return 1;  // success
}

static int hm2_eth_enqueue_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    if (comm_active == 0) return 1;
    if (size == 0) return 1;
    if (size == -1) {
        int send;
    write_cnt++;

        //lbp16_cmd_addr *packet = (lbp16_cmd_addr *) write_packet_ptr;

        //LBP16_INIT_PACKET4_PTR(packet, CMD_READ_COMM_CTRL_ADDR16(1), 0);
        send = eth_socket_send(sockfd, (void*) &write_packet, write_packet_size, 0);
        //LL_PRINT_IF(debug, "write(): PACKET SENDED [SIZE: %d]\n", write_packet_size);
        write_packet_ptr = &write_packet;
        write_packet_size = 0;
    } else {
        lbp16_cmd_addr *packet = (lbp16_cmd_addr *) write_packet_ptr;

        LBP16_INIT_PACKET4_PTR(packet, CMD_WRITE_HOSTMOT2_ADDR32_INCR(size/4), addr);
        //LL_PRINT_IF(debug, "hm2_eth_enqueue_write(): PACKET QUEUED [CMD:%02X%02X | ADDR: %02X%02X | SIZE: %d]\n",
        //  packet->cmd_hi, packet->cmd_lo, packet->addr_hi, packet->addr_lo, size);
        write_packet_ptr += sizeof(packet);
        memcpy(write_packet_ptr, buffer, size);
        write_packet_ptr += size;
        write_packet_size += (sizeof(packet) + size);
        //LL_PRINT_IF(debug, "hm2_eth_enqueue_write(): [SIZE: %d]\n", size);
    }
    return 1;
}

static void hm2_eth_probe() {
    int ret, send, recv;
    char board_name[16] = {0, };
    char llio_name[16] = {0, };
    hm2_lowlevel_io_t *this;
    hm2_eth_t *board;

    LBP16_INIT_PACKET4(read_packet, CMD_READ_BOARD_INFO_ADDR16_INCR(16/2), 0);
    send = eth_socket_send(sockfd, (void*) &read_packet, sizeof(read_packet), 0);
    recv = eth_socket_recv(sockfd, (void*) &board_name, 16, 0);

    board = &boards[boards_count];
    this = &board->llio;
    
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
    } else {
        probe_fail = 1;
        LL_PRINT("No ethernet board found\n");
        return;
    }

    LL_PRINT("discovered %.*s\n", 16, board_name);

    rtapi_snprintf(board->llio.name, sizeof(board->llio.name), "hm2_%.*s.%d", strlen(llio_name), llio_name, boards_count);
   
    board->llio.comp_id = comp_id;
    board->llio.private = board;

    board->llio.read = hm2_eth_read;
    board->llio.write = hm2_eth_write;
    board->llio.queue_read = hm2_eth_enqueue_read;
    board->llio.queue_write = hm2_eth_enqueue_write;

    ret = hm2_register(&board->llio, config[boards_count]);
    if (ret != 0) {
        rtapi_print("board fails HM2 registration\n");
        return;
    }
    rtapi_print("board %s registred succesfully\n", board_name);
    boards_count++;

    int val = fcntl(sockfd, F_GETFL);
    val = val | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, val);
}

int rtapi_app_main(void) {
    int ret;

    LL_PRINT("loading Mesa AnyIO HostMot2 ethernet driver version " HM2_ETH_VERSION "\n");
    
    ret = hal_init(HM2_LLIO_NAME);
    if (ret < 0)
        goto error0;
    comp_id = ret;

    ret = init_net();
    if (ret < 0) {
        rtapi_print("RTNET layer not ready\n");
        goto error1;
    }

    comm_active = 1;

    //rest of init must be done in rt context
    // start rt task, run hm2_eth_probe() in it, and wait for finish here
//    ret = rt_task_create(&rt_probe_task, "probe", 0, 10, T_JOINABLE);
//    rt_task_start(&rt_probe_task, hm2_eth_probe, NULL);
//    rt_task_join(&rt_probe_task);

    hm2_eth_probe();

    if (probe_fail == 1)
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
