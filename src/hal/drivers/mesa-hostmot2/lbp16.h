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

#ifndef __LBP16_H
#define __LBP16_H

#define LBP16_SENDRECV_DEBUG 0

#define LBP16_UDP_PORT 27181
#define LBP16_HW_IP "192.168.1.121"

#define LBP16_MEM_SPACE_COUNT 8

#define LBP16_CMD_SIZE  2
#define LBP16_ADDR_SIZE 2
#define LBP16_CMDADDR_PACKET_SIZE (LBP16_CMD_SIZE + LBP16_ADDR_SIZE)
#define LBP16_CMDONLY_PACKET_SIZE (LBP16_CMD_SIZE)
#define LBP16_MAX_PACKET_DATA_SIZE 0x7F

#define FLASH_ADDR_REG      0x0000
#define FLASH_DATA_REG      0x0004
#define FLASH_ID_REG        0x0008
#define FLASH_SEC_ERASE_REG 0x000C

#define ETH_EEPROM_IP_REG   0x0020

#define COMM_CTRL_WRITE_ENA_REG 0x001A

#define LBP16_ADDR_AUTO_INC     0x0080
#define LBP16_ARGS_8BIT         0x0000
#define LBP16_ARGS_16BIT        0x0100
#define LBP16_ARGS_32BIT        0x0200
#define LBP16_ARGS_64BIT        0x0300
#define LBP16_SPACE_HM2         0x0000
#define LBP16_SPACE_ETH_CHIP    0x0400
#define LBP16_SPACE_ETH_EEPROM  0x0800
#define LBP16_SPACE_FPGA_FLASH  0x0C00
#define LBP16_SPACE_TIMER       0x1000
#define LBP16_SPACE_COMM_CTRL   0x1800
#define LBP16_SPACE_BOARD_INFO  0x1C00
#define LBP16_SPACE_ACC         0x0000
#define LBP16_INFO_ACC          0x2000
#define LBP16_READ              0x0000
#define LBP16_ADDR              0x4000
#define LBP16_NO_ADDR           0x0000
#define LBP16_WRITE             0x8000

#define CMD_READ_AREA_INFO_16 (LBP16_READ | LBP16_ADDR | LBP16_INFO_ACC | LBP16_ARGS_16BIT)
#define CMD_READ_ADDR_16 (LBP16_READ | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ARGS_16BIT)
#define CMD_READ_ADDR_32 (LBP16_READ | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ARGS_32BIT)
#define CMD_WRITE_ADDR_16 (LBP16_WRITE | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ARGS_16BIT)
#define CMD_WRITE_ADDR_16_INCR (LBP16_WRITE | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ADDR_AUTO_INC | LBP16_ARGS_16BIT)
#define CMD_WRITE_ADDR_32 (LBP16_WRITE | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ARGS_32BIT)
#define CMD_WRITE_ADDR_32_INCR (LBP16_WRITE | LBP16_ADDR | LBP16_SPACE_ACC | LBP16_ADDR_AUTO_INC | LBP16_ARGS_32BIT)

#define CMD_READ_AREA_INFO_ADDR16(space, size)        (CMD_READ_AREA_INFO_16 | space | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_AREA_INFO_ADDR16_INCR(space, size)   (CMD_READ_AREA_INFO_16 | LBP16_ADDR_AUTO_INC | space | ((size) & LBP16_MAX_PACKET_DATA_SIZE))

#define CMD_READ_HOSTMOT2_ADDR32(size)        (CMD_READ_ADDR_32 | LBP16_SPACE_HM2 | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_HOSTMOT2_ADDR32_INCR(size)   (CMD_READ_ADDR_32 | LBP16_SPACE_HM2 | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_ETH_CHIP_ADDR16(size)        (CMD_READ_ADDR_16 | LBP16_SPACE_ETH_CHIP | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_ETH_CHIP_ADDR16_INCR(size)   (CMD_READ_ADDR_16 | LBP16_SPACE_ETH_CHIP | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_ETH_EEPROM_ADDR16(size)      (CMD_READ_ADDR_16 | LBP16_SPACE_ETH_EEPROM | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_ETH_EEPROM_ADDR16_INCR(size) (CMD_READ_ADDR_16 | LBP16_SPACE_ETH_EEPROM | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_FPGA_FLASH_ADDR32(size)      (CMD_READ_ADDR_32 | LBP16_SPACE_FPGA_FLASH | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_TIMER_ADDR16(size)           (CMD_READ_ADDR_16 | LBP16_SPACE_TIMER | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_TIMER_ADDR16_INCR(size)      (CMD_READ_ADDR_16 | LBP16_SPACE_TIMER | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_COMM_CTRL_ADDR16(size)       (CMD_READ_ADDR_16 | LBP16_SPACE_COMM_CTRL | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_COMM_CTRL_ADDR16_INCR(size)  (CMD_READ_ADDR_16 | LBP16_SPACE_COMM_CTRL | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_BOARD_INFO_ADDR16(size)      (CMD_READ_ADDR_16 | LBP16_SPACE_BOARD_INFO | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_READ_BOARD_INFO_ADDR16_INCR(size) (CMD_READ_ADDR_16 | LBP16_SPACE_BOARD_INFO | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))

#define CMD_WRITE_HOSTMOT2_ADDR32(size)       (CMD_WRITE_ADDR_32 | LBP16_SPACE_HM2 | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_WRITE_HOSTMOT2_ADDR32_INCR(size)  (CMD_WRITE_ADDR_32 | LBP16_SPACE_HM2 | LBP16_ADDR_AUTO_INC | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_WRITE_FPGA_FLASH_ADDR32(size)     (CMD_WRITE_ADDR_32 | LBP16_SPACE_FPGA_FLASH | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_WRITE_COMM_CTRL_ADDR16(size)      (CMD_WRITE_ADDR_16 | LBP16_SPACE_COMM_CTRL | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_WRITE_ETH_EEPROM_ADDR16(size)     (CMD_WRITE_ADDR_16 | LBP16_SPACE_ETH_EEPROM | ((size) & LBP16_MAX_PACKET_DATA_SIZE))
#define CMD_WRITE_ETH_EEPROM_ADDR16_INCR(size) (CMD_WRITE_ADDR_16_INCR | LBP16_SPACE_ETH_EEPROM | ((size) & LBP16_MAX_PACKET_DATA_SIZE))

// common packets
#define CMD_READ_HM2_COOKIE  (CMD_READ_HOSTMOT2_ADDR32(1))
#define CMD_READ_FLASH_IDROM (CMD_READ_FPGA_FLASH_ADDR32(1))

#define LO_BYTE(x) ((x) & 0xFF)
#define HI_BYTE(x) (((x) & 0xFF00) >> 8)

typedef struct {
    u8 cmd_hi;
    u8 cmd_lo;
} lbp16_cmd;

typedef struct {
    u8 cmd_hi;
    u8 cmd_lo;
    u8 addr_hi;
    u8 addr_lo;
} lbp16_cmd_addr;

typedef struct {
    u8 cmd_hi;
    u8 cmd_lo;
    u8 addr_hi;
    u8 addr_lo;
    u8 data_hi;
    u8 data_lo;
} lbp16_cmd_addr_data16;

typedef struct {
    u8 cmd_hi;
    u8 cmd_lo;
    u8 addr_hi;
    u8 addr_lo;
    u8 data1;
    u8 data2;
    u8 data3;
    u8 data4;
} lbp16_cmd_addr_data32;

typedef struct {
    u8 cmd_hi;
    u8 cmd_lo;
    u8 addr_hi;
    u8 addr_lo;
    u8 page[256];
} lbp16_write_flash_page_packet;

typedef struct {
    lbp16_cmd_addr_data16 write_ena_pck;
    lbp16_cmd_addr_data32 fl_erase_pck;
} lbp16_erase_flash_sector_packets;

typedef struct {
    lbp16_cmd_addr_data16 write_ena_pck;
    lbp16_write_flash_page_packet fl_write_page_pck;
} lbp16_write_flash_page_packets;

typedef struct {
    lbp16_cmd_addr_data16 write_ena_pck;
    lbp16_cmd_addr_data32 eth_write_ip_pck;
} lbp16_write_ip_addr_packets;

#define LBP16_INIT_PACKET4(packet, cmd, addr) do { \
    packet.cmd_hi = LO_BYTE(cmd); \
    packet.cmd_lo = HI_BYTE(cmd); \
    packet.addr_hi = LO_BYTE(addr); \
    packet.addr_lo = HI_BYTE(addr); \
    } while (0);

#define LBP16_INIT_PACKET4_PTR(packet, cmd, addr) do { \
    packet->cmd_hi = LO_BYTE(cmd); \
    packet->cmd_lo = HI_BYTE(cmd); \
    packet->addr_hi = LO_BYTE(addr); \
    packet->addr_lo = HI_BYTE(addr); \
    } while (0);

#define LBP16_INIT_PACKET6(packet, cmd, addr, data) do { \
    packet.cmd_hi = LO_BYTE(cmd); \
    packet.cmd_lo = HI_BYTE(cmd); \
    packet.addr_hi = LO_BYTE(addr); \
    packet.addr_lo = HI_BYTE(addr); \
    packet.data_hi = LO_BYTE(data); \
    packet.data_lo = HI_BYTE(data); \
    } while (0);

#define LBP16_INIT_PACKET8(packet, cmd, addr, data) do { \
    packet.cmd_hi = LO_BYTE(cmd); \
    packet.cmd_lo = HI_BYTE(cmd); \
    packet.addr_hi = LO_BYTE(addr); \
    packet.addr_lo = HI_BYTE(addr); \
    packet.data1 = LO_BYTE(data); \
    packet.data2 = HI_BYTE(data); \
    packet.data3 = LO_BYTE((data >> 16) & 0xFFFF); \
    packet.data4 = HI_BYTE((data >> 16) & 0xFFFF); \
    } while (0);

typedef struct {
    u16 cookie;
    u16 size;
    u16 range;
    u16 addr;
    u8  name[8];
} lbp_mem_info_area;

typedef struct {
    u16 reserved1;
    u16 mac_addr_lo;
    u16 mac_addr_mid;
    u16 mac_addr_hi;
    u16 reserved2;
    u16 reserved3;
    u16 reserved4;
    u16 reserved5;
    u8 name[16];
    u16 ip_addr_lo;
    u16 ip_addr_hi;
    u16 reserved6;
    u16 reserved7;
    u16 led_debug;
    u16 reserved8;
    u16 reserved9;
    u16 reserved10;
} lbp_eth_eeprom_area;

typedef struct {
    u16 uSTimeStampReg;
    u16 WaituSReg;
    u16 HM2Timeout;
    u16 WaitForHM2RefTime;
    u16 WaitForHM2Timer1;
    u16 WaitForHM2Timer2;
    u16 WaitForHM2Timer3;
    u16 WaitForHM2Timer4;
} lbp_timers_area;

typedef struct {
    u16 ErrorReg;
    u16 LBPParseErrors;
    u16 LBPMemErrors;
    u16 LBPWriteErrors;
    u16 RXPacketCount;
    u16 RXUDPCount;
    u16 RXBadCount;
    u16 TXPacketCount;
    u16 TXUDPCount;
    u16 TXBadCount;
    u16 led_mode;
    u16 DebugLEDPtr;
    u16 Scratch;
} lbp_status_area;

typedef struct {
    u8 name[16];
    u16 LBP16_version;
    u16 firmware_version;
    u16 jumpers;
} lbp_info_area;

int lbp16_send_packet(void *packet, int size);
int lbp16_recv_packet(void *buffer, int size);
void lbp16_socket_nonblocking();
void lbp16_socket_blocking();
void lbp16_socket_set_dest_ip(char *addr_name);
char *lbp16_socket_get_src_ip();
int lbp16_read(u16 cmd, u32 addr, void *buffer, int size);
int lbp16_hm2_read(u32 addr, void *buffer, int size);
int lbp16_hm2_write(u32 addr, void *buffer, int size);
void lbp16_print_info();
void lbp16_init();
void lbp16_release();

#endif
