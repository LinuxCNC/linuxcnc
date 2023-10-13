/*    This is a component of LinuxCNC
 *    Copyright 2014
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

#include <ctype.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <endian.h>

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_bool.h>
#include <rtapi_gfp.h>
#include <hal.h>

#include "hostmot2-lowlevel.h"
#include "hostmot2.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Epler");
MODULE_DESCRIPTION("Driver for HostMot2 devices connected via SPI");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-7i90");

#define MAX_BOARDS (8)

// Maximum size of a transaction in 32-bit words.  spidev is by default limited
// to 1 physical page, typically 4096 bytes.  Typically, actual transactions
// are smaller than this; for example, an SV4_ST8 configuration with everything
// turned on uses just 28 words.
#define MAX_TRX (1024)

static int spidev_rate[MAX_BOARDS] = { [0 ... MAX_BOARDS-1] = 24000 };
RTAPI_MP_ARRAY_INT(spidev_rate, MAX_BOARDS, "SPI clock rate in kHz");

static char *spidev_path[MAX_BOARDS] = { "/dev/spidev1.0" };
RTAPI_MP_ARRAY_STRING(spidev_path, MAX_BOARDS, "path to spi device");

static char *config[MAX_BOARDS];
RTAPI_MP_ARRAY_STRING(config, MAX_BOARDS, "config string for the AnyIO boards (see hostmot2(9) manpage)")

typedef struct {
    hm2_lowlevel_io_t llio;
    int fd;
    struct spi_ioc_transfer settings;

    uint32_t trxbuf[MAX_TRX];
    uint32_t *scatter[MAX_TRX];
    int nbuf;
} hm2_spi_t;

static hm2_spi_t boards[MAX_BOARDS];
static int nboards;
static int comp_id;

static char *hm2_7c80_pin_names[] = {
	"TB07-02/TB07-03",	/* Step/Dir/Misc 5V out */
	"TB07-04/TB07-05",
	"TB08-02/TB08-03",
	"TB08-04/TB08-05",
	"TB09-02/TB09-03",
	"TB09-04/TB09-05",
	"TB10-02/TB10-03",
	"TB10-04/TB10-05",
	"TB11-02/TB11-03",
	"TB11-04/TB11-05",
	"TB12-02/TB12-03",
	"TB12-04/TB12-05",
	"TB03-03/TB04-04",	/* RS-422/RS-485 interface */
	"TB03-05/TB04-06",
	"TB03-05/TB03-06",
	"TB04-01/TB04-02",	/* Encoder */
	"TB04-04/TB04-05",
	"TB04-07/TB04-08",
	"TB05-02",		/* Spindle */
	"TB05-02",
	"TB05-05/TB05-06",
	"TB05-07/TB05-08",
	"Internal InMux0",	/* InMux */
	"Internal InMux1",
	"Internal InMux2",
	"Internal InMux3",
	"Internal InMux4",

	"Internal InMuxData",
	"TB13-01/TB13-02",	/* SSR */
	"TB13-03/TB13-04",
	"TB13-05/TB13-06",
	"TB13-07/TB13-08",
	"TB14-01/TB14-02",
	"TB14-03/TB14-04",
	"TB14-05/TB14-06",
	"TB14-07/TB14-08",
	"Internal SSR",
	"P1-01/DB25-01",   /* P1 parallel expansion */
	"P1-02/DB25-14",
	"P1-03/DB25-02",
	"P1-04/DB25-15",
	"P1-05/DB25-03",
	"P1-06/DB25-16",
	"P1-07/DB25-04",
	"P1-08/DB25-17",
	"P1-09/DB25-05",
	"P1-11/DB25-06",
	"P1-13/DB25-07",
	"P1-15/DB25-08",
	"P1-17/DB25-09",
	"P1-19/DB25-10",
	"P1-21/DB25-11",
	"P1-23/DB25-12",
	"P1-25/DB25-13",
};

static char *hm2_7c81_pin_names[] = {
	"P1-01/DB25-01",
	"P1-02/DB25-14",
	"P1-03/DB25-02",
	"P1-04/DB25-15",
	"P1-05/DB25-03",
	"P1-06/DB25-16",
	"P1-07/DB25-04",
	"P1-08/DB25-17",
	"P1-09/DB25-05",
	"P1-11/DB25-06",
	"P1-13/DB25-07",
	"P1-15/DB25-08",
	"P1-17/DB25-09",
	"P1-19/DB25-10",
	"P1-21/DB25-11",
	"P1-23/DB25-12",
	"P1-25/DB25-13",
	"J5-TX0",
	"J6-TX1",

	"P2-01/DB25-01",
	"P2-02/DB25-14",
	"P2-03/DB25-02",
	"P2-04/DB25-15",
	"P2-05/DB25-03",
	"P2-06/DB25-16",
	"P2-07/DB25-04",
	"P2-08/DB25-17",
	"P2-09/DB25-05",
	"P2-11/DB25-06",
	"P2-13/DB25-07",
	"P2-15/DB25-08",
	"P2-17/DB25-09",
	"P2-19/DB25-10",
	"P2-21/DB25-11",
	"P2-23/DB25-12",
	"P2-25/DB25-13",
	"J5-TXEN0",
	"J6-TXEN1",

	"P7-01/DB25-01",
	"P7-02/DB25-14",
	"P7-03/DB25-02",
	"P7-04/DB25-15",
	"P7-05/DB25-03",
	"P7-06/DB25-16",
	"P7-07/DB25-04",
	"P7-08/DB25-17",
	"P7-09/DB25-05",
	"P7-11/DB25-06",
	"P7-13/DB25-07",
	"P7-15/DB25-08",
	"P7-17/DB25-09",
	"P7-19/DB25-10",
	"P7-21/DB25-11",
	"P7-23/DB25-12",
	"P7-25/DB25-13",
	"P5-RX0",
	"P6-RX1"
};

 
static uint32_t read_command(uint16_t addr, unsigned nelem) {
    bool increment = true;
    return (addr << 16) | 0xA000 | (increment ? 0x800 : 0) | (nelem << 4);
}

static uint32_t write_command(uint16_t addr, unsigned nelem) {
    bool increment = true;
    return (addr << 16) | 0xB000 | (increment ? 0x800 : 0) | (nelem << 4);
}

static int do_pending(hm2_spi_t *this) {
    if(this->nbuf == 0) return 0;

    struct spi_ioc_transfer t;
    t = this->settings;
    t.tx_buf = t.rx_buf = (uint64_t)(uintptr_t)this->trxbuf;
    t.len = 4 * this->nbuf;

    if(this->settings.bits_per_word == 8) {
	int i;
	for(i=0; i<this->nbuf; i++)
	   this->trxbuf[i] = htobe32(this->trxbuf[i]);
    }
    int r = ioctl(this->fd, SPI_IOC_MESSAGE(1), &t);
    if(r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "hm2_spi: SPI_IOC_MESSAGE: %s\n", strerror(errno));
        this->nbuf = 0;
        return -errno;
    }

    if(this->settings.bits_per_word == 8) {
	int i;
	for(i=0; i<this->nbuf; i++)
	   this->trxbuf[i] = be32toh(this->trxbuf[i]);
    }

    // because linux manages SPI chip select behind our backs, we can't know
    // whether to expect the "AAAAAAAA" cookie to appear in the first
    // word of the response.  that's too bad, because checking this
    // would give a minimum level of detection of gross problems such as
    // a disconnected cable.
    uint32_t *buf = this->trxbuf;
    uint32_t **scatter = this->scatter;
    int i=0;
    for(i=0; i<this->nbuf; i++) {
        uint32_t *target = scatter[i];
        if(target) *target = buf[i];
    }

    this->nbuf = 0;
    return 1;
}

// Add a word to the transaction.  The "send_data" word is transmitted, and the
// response to that word is stored at "recv_addr" if it is not NULL.  Typically
// one argument is 0/NULL and the other argument has a value.
#define PUT(send_data, recv_addr) do { \
    this->trxbuf[this->nbuf] = send_data; \
    this->scatter[this->nbuf++] = recv_addr; \
} while(0)

static int send_queued_writes(hm2_lowlevel_io_t *llio) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    return do_pending(this) >= 0;
}

static int queue_write(hm2_lowlevel_io_t *llio, rtapi_u32 addr, const void *buffer, int size) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    if(size == 0) return 0;
    if(size % 4) return -EINVAL;

    int wsize = size / 4;
    if(wsize + this->nbuf + 1 > MAX_TRX) {
        int r = do_pending(this);
        if(r < 0) return r;
    }

    PUT(write_command(addr, wsize), 0);

    const uint32_t *wbuffer = buffer;
    int i=0;
    for(i=0; i<wsize; i++)
        PUT(wbuffer[i], 0);

    return 1;
}

static int send_queued_reads(hm2_lowlevel_io_t *llio) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    return do_pending(this) >= 0;
}

static int queue_read(hm2_lowlevel_io_t *llio, rtapi_u32 addr, void *buffer, int size) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    if(size == 0) return 0;
    if(size % 4) return -EINVAL;

    int wsize = size / 4;
    if(wsize + this->nbuf + 1 > MAX_TRX) {
        int r = do_pending(this);
        if(r < 0) return r;
    }

    uint32_t *wbuffer = buffer;
    int i=0;
    PUT(read_command(addr, wsize), 0);
    for(i=0; i<wsize; i++)
        PUT(0, &wbuffer[i]);

    return 1;
}

static int do_write(hm2_lowlevel_io_t *llio, rtapi_u32 addr, const void *buffer, int size) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    int r = queue_write(llio, addr, buffer, size);
    if(r < 0) return r;
    return do_pending(this) >= 0;
}

static int do_read(hm2_lowlevel_io_t *llio, rtapi_u32 addr, void *buffer, int size) {
    hm2_spi_t *this = (hm2_spi_t*) llio;
    int r = queue_read(llio, addr, buffer, size);
    if(r < 0) return r;
    return do_pending(this);
}

static int spidev_set_lsb_first(int fd, uint8_t lsb_first) {
    return ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb_first);
}

static int spidev_set_mode(int fd, uint8_t mode) {
    return ioctl(fd, SPI_IOC_WR_MODE, &mode);
}

static int spidev_set_max_speed_hz(int fd, uint32_t speed_hz) {
    return ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);
}

static int spidev_set_bits_per_word(int fd, uint8_t bits) {
    return ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
}

static int spidev_get_bits_per_word(int fd) {
    uint8_t bits;
    int r;
    r = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if(r < 0) return -1;
    return bits;
}

static int spidev_open_and_configure(char *dev, int rate) {
    int fd = open(dev, O_RDWR);

    if(fd < 0) return -errno;

    int r = spidev_set_lsb_first(fd, false);
    if(r < 0) goto fail_errno;

    r = spidev_set_mode(fd, 0);
    if(r < 0) goto fail_errno;

    r = spidev_set_bits_per_word(fd, 32);
    if(r < 0) r = spidev_set_bits_per_word(fd, 8);
    if(r < 0) goto fail_errno;

    r = spidev_set_max_speed_hz(fd, rate);
    if(r < 0) goto fail_errno;

    return fd;

fail_errno:
    r = -errno;
    close(fd);
    return r;
}

static int check_cookie(hm2_spi_t *board) {
    uint32_t cookie[4];
    uint32_t xcookie[4] = {0x55aacafe, 0x54534f48, 0x32544f4d, 0x00000400};
    int r = do_read(&board->llio, 0x100, cookie, 16);
    if(r < 0) return -errno;

    if(memcmp(cookie, xcookie, sizeof(cookie))) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Invalid cookie\n");
        rtapi_print_msg(RTAPI_MSG_ERR, "Read: %08x %08x %08x %08x\n",
            cookie[0], cookie[1], cookie[2], cookie[3]);
        return -ENODEV;
    }
    return 0;
}

static int read_ident(hm2_spi_t *board, char *ident) {
    return do_read(&board->llio, 0x40c, ident, 8);
}

static int probe(char *dev, int rate) {
    printf("probe %d\n", rate);
    if(nboards >= MAX_BOARDS) return -ENOSPC;

    hm2_spi_t *board = &boards[nboards];
    board->fd = spidev_open_and_configure(dev, rate);
    if(board->fd < 0) return board->fd;

    board->settings.speed_hz = rate;
    board->settings.bits_per_word = spidev_get_bits_per_word(board->fd);

    int r = check_cookie(board);
    if(r < 0) goto fail;

    char ident[8];
    r = read_ident(board, ident);
    if(r < 0) goto fail;

    char *base;

    if(!memcmp(ident, "MESA7I43", 8)) {
        base = "hm2_7i43spi";
        board->llio.num_ioport_connectors = 2;
        board->llio.pins_per_connector = 24;
        board->llio.ioport_connector_name[0] = "P4";
        board->llio.ioport_connector_name[1] = "P3";
        board->llio.num_leds = 8;
        board->llio.fpga_part_number = "3s400tq144";
    } else if(!memcmp(ident, "MESA7I90", 8)) {
        base = "hm2_7i90";
        board->llio.num_ioport_connectors = 3;
        board->llio.pins_per_connector = 24;
        board->llio.ioport_connector_name[0] = "P1";
        board->llio.ioport_connector_name[1] = "P2";
        board->llio.ioport_connector_name[2] = "P3";
        board->llio.num_leds = 2;
        board->llio.fpga_part_number = "xc6slx9tq144";
    } else if(!memcmp(ident, "MESA7C80", 8)){
            base = "hm2_7c80";
            board->llio.num_ioport_connectors = 2;
            board->llio.pins_per_connector = 27;
            board->llio.ioport_connector_name[0] = "Embedded I/O";
            board->llio.ioport_connector_name[1] = "Embedded I/O + P1 expansion";
            board->llio.io_connector_pin_names = hm2_7c80_pin_names;
            board->llio.num_leds = 4;
            board->llio.fpga_part_number = "xc6slx9tq144";
    } else if(!memcmp(ident, "MESA7C81", 8)){
            base = "hm2_7c81";
            board->llio.num_ioport_connectors = 3;
            board->llio.pins_per_connector = 19;
            board->llio.ioport_connector_name[0] = "P1";
            board->llio.ioport_connector_name[1] = "P2";
            board->llio.ioport_connector_name[2] = "P7";
            board->llio.io_connector_pin_names = hm2_7c81_pin_names;
            board->llio.num_leds = 4;
            board->llio.fpga_part_number = "xc6slx9tq144";
    } else {
        // peter's been busy
        int i=0;
        for(i=0; i<sizeof(ident); i++)
            if(!isprint(ident[i])) ident[i] = '?';
        rtapi_print_msg(RTAPI_MSG_ERR, "Unknown board: %.8s\n", ident);
        goto fail;
    }

    rtapi_snprintf(board->llio.name, sizeof(board->llio.name),
        "%s.%d", base, nboards);
    board->llio.comp_id = comp_id;
    board->llio.private = &board;
    board->llio.read = do_read;
    board->llio.write = do_write;
    board->llio.queue_read = queue_read;
    board->llio.send_queued_reads = send_queued_reads;
    board->llio.queue_write = queue_write;
    board->llio.send_queued_writes = send_queued_writes;

    r = hm2_register(&board->llio, config[nboards]);
    if(r < 0) goto fail;

    nboards++;
    return 0;
fail:
    close(board->fd);
    return r;
}

int rtapi_app_main() {
    int ret;
    int i=0;
    comp_id = ret = hal_init("hm2_spi");
    if(ret < 0) goto fail;

    for(i=0; i<MAX_BOARDS && spidev_path[i]; i++) {
        ret = probe(spidev_path[i], 1000 * spidev_rate[i]);
        if(ret < 0) goto fail;
    }

    hal_ready(comp_id);
    return 0;

fail:
    for(i=0; i<MAX_BOARDS && boards[i].fd; i++)
        close(boards[i].fd);
    return ret;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}
