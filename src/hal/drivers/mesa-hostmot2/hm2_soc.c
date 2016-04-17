//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//    Transformed for Machinekit socfpga in 2016 by Michael Brown
//    Copyright (C) Holotronic 2016-2017
//
//    some polishings Michael Haberler 2016

//---------------------------------------------------------------------------//
//  #
//  # module assignments (from hm2reg_io_hw.tcl)
//  #
//  set_module_assignment embeddedsw.dts.group hm2-socfpga
//  set_module_assignment embeddedsw.dts.name hm2reg-io
//  set_module_assignment embeddedsw.dts.params.address_width 14
//  set_module_assignment embeddedsw.dts.params.data_width 32
//  set_module_assignment embeddedsw.dts.vendor machkt
//---------------------------------------------------------------------------//
// in device tree (sos_system.dts)
//
//  hm2reg_io_0: hm2-socfpga@0x100040000 {
//      compatible = "machkt,hm2reg-io-1.0";
//      reg = <0x00000001 0x00040000 0x00010000>;
//      clocks = <&clk_0>;
//      address_width = <14>;	/* embeddedsw.dts.params.address_width type NUMBER */
//      data_width = <32>;	/* embeddedsw.dts.params.data_width type NUMBER */
//  }; //end hm2-socfpga@0x100040000 (hm2reg_io_0)
//---------------------------------------------------------------------------//
// on commandline:
//  machinekit@mksoc:~$ ls -R /proc/device-tree | grep hm2-socfpga
//  hm2-socfpga@0x100040000
//  /proc/device-tree/sopc@0/bridge@0xc0000000/hm2-socfpga@0x100040000:
//---------------------------------------------------------------------------//
#include "config.h"

// this should be an general socfpga #define
#if !defined(TARGET_PLATFORM_SOCFPGA)
#error "This driver is for the socfpga platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_hexdump.h"
#include "rtapi_compat.h"
#include "rtapi_io.h"
#include "hal.h"
#include "hal/lib/config_module.h"
#include "hostmot2-lowlevel.h"
#include "hostmot2.h"
#include "hm2_soc.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

// from 3.10.37-ltsi drivers/fpga/fpga-mgrs/altera.c:
// returned by altera_fpga_status()
/* Register bit defines */
/* ALT_FPGAMGR_STAT register */
#define ALT_FPGAMGR_STAT_POWER_UP			0x0
#define ALT_FPGAMGR_STAT_RESET				0x1
#define ALT_FPGAMGR_STAT_CFG				0x2
#define ALT_FPGAMGR_STAT_INIT				0x3
#define ALT_FPGAMGR_STAT_USER_MODE			0x4
#define ALT_FPGAMGR_STAT_UNKNOWN			0x5
#define ALT_FPGAMGR_STAT_STATE_MASK			0x7
/* This is a flag value that doesn't really happen in this register field */
#define ALT_FPGAMGR_STAT_POWER_OFF			0xf

#define HM2REG_IO_0_SPAN 65536

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Brown & Michael Haberler");
MODULE_DESCRIPTION("Driver initially for HostMot2 on the DE0 Nano"
		   " / Atlas Cyclone V socfpga board from Terasic");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-5i25");

static char *config[HM2_SOC_MAX_BOARDS];
RTAPI_MP_ARRAY_STRING(config, HM2_SOC_MAX_BOARDS,
		      "config string for the AnyIO boards (see hostmot2(9) manpage)")
static int debug;
RTAPI_MP_INT(debug, "turn on extra debug output");

static char *uio_dev = "/dev/uio0";
RTAPI_MP_STRING(uio_dev, "UIO device to use; default /dev/uio0");

static int comp_id;
static hm2_soc_t board[HM2_SOC_MAX_BOARDS];
static int num_boards;
static int failed_errno = 0; // errno of last failed registration
static char *fpga_sysfs_status = "/sys/class/fpga/%s/status";
static char *fpga_dev = "fpga0";

static int hm2_soc_mmap(hm2_soc_t *board);

static int altera_fpga_status(const char *fmt, ...);
static char *strlwr(char *str);
static int altera_set_fpga_bridge(const char *bridge, const char *value);

// 
// these are the "low-level I/O" functions exported up
//
static int hm2_soc_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *board = this->private;
    int i;
    u32* src = (u32*) (board->base + addr);
    u32* dst = (u32*) buffer;

    /* Per Peter Wallace, all hostmot2 access should be 32 bits and 32-bit aligned */
    /* Check for any address or size values that violate this alignment */
    if ( ((addr & 0x3) != 0) || ((size & 0x03) != 0) ){
        u16* dst16 = (u16*) dst;
        u16* src16 = (u16*) src;
        /* hm2_read_idrom performs a 16-bit read, which seems to be OK, so let's allow it */
        if ( ((addr & 0x1) != 0) || (size != 2) ){
            LL_ERR( "hm2_soc_read: Unaligned Access: %08x %04x\n", addr,size);
            memcpy(dst, src, size);
            return 1;  // success
        }
        dst16[0] = src16[0];
        return 1;  // success
    }
    for (i=0; i<(size/4); i++) {
        dst[i] = src[i];
    }
    return 1;  // success
}

static int hm2_soc_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *board = this->private;
    int i;
    u32* src = (u32*) buffer;
    u32* dst = (u32*) (board->base + addr);

    /* Per Peter Wallace, all hostmot2 access should be 32 bits and 32-bit aligned */
    /* Check for any address or size values that violate this alignment */
    if ( ((addr & 0x3) != 0) || ((size & 0x03) != 0) ){
        LL_ERR( "hm2_soc_write: Unaligned Access: %08x %04x\n", addr,size);
        memcpy(dst, src, size);
        return 1;  // success
    }

    for (i=0; i<(size/4); i++) {
        dst[i] = src[i];
    }
    return 1;  // success
}

// when no firmware= was specified, hm2_register() will read/write from the
// (as of yet unmapped) fpga region. Trap this case by lazy mapping - this will
// happen only once at startup as hm2_soc_mmap() sets llio->read&write
// to hm2_soc_read & hm2_soc_write.
static int hm2_soc_unmapped_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *board = this->private;
    int rc = hm2_soc_mmap(board);
    if (rc)
	return rc;
    return hm2_soc_read(this, addr, buffer, size);
}

static int hm2_soc_unmapped_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *board = this->private;
    int rc = hm2_soc_mmap(board);
    if (rc)
	return rc;
    return hm2_soc_write(this, addr, buffer, size);
}

static int hm2_soc_verify_firmware(hm2_lowlevel_io_t *this,  const struct firmware *fw) {
    // if you dont trust what is being passed in, inspect *fw here
    LL_DBG("soc_verify_firmware size=%zu at %p fd=%d",
	   fw->size, fw->data, fw->fd);
    return 0;
}

static int hm2_soc_reset(hm2_lowlevel_io_t *this) {
    // currently not needed
    LL_DBG( "soc_reset");
    return 0;
}

static int hm2_soc_program_fpga(hm2_lowlevel_io_t *this,
				const bitfile_t *bitfile,
				const struct firmware *fw) {

    int rc;
    hm2_soc_t *board =  this->private;

    LL_DBG("soc_program_fpga");
    board->firmware_given = 1;

    char devname[100];
    rtapi_snprintf(devname, sizeof(devname),"/dev/%s", board->fpga_dev);

    if (board->fpga_state  < 0) {
	LL_ERR("%s: %s - invalid FPGA state :%d",
	       __FUNCTION__, devname, board->fpga_state);
	return board->fpga_state;
    }
    char *gpio_module = "gpio_altera";
    int gpio_altera_was_loaded = is_module_loaded(gpio_module);

    // unload gpio_altera during FPGA loading
    if (gpio_altera_was_loaded) {
	rc = run_shell("/sbin/modprobe -r %s", gpio_module);
	if (rc) {
	    LL_ERR("run_shell(modprobe -r %s) failed: %s", gpio_module, strerror(errno));
	    return -errno;
	}
	// paranoia
	if (is_module_loaded(gpio_module)) {
	    LL_ERR("module %s failed to unload", gpio_module);
	    return -EBUSY;
	}
    }

    int fd = open(devname, O_WRONLY);
    if (fd < 0) {
	LL_ERR("open(%s) failed: %s", devname, strerror(errno));
	return -EIO;
    }

    // disable bridges
    altera_set_fpga_bridge("hps2fpga", "0\n");
    altera_set_fpga_bridge("fpga2hps", "0\n");
    altera_set_fpga_bridge("lwhps2fpga", "0\n");

    // load FPGA
    if (write(fd, fw->data, fw->size) != fw->size)  {
	LL_ERR("write(%s, %zu) failed: %s",
	       devname, fw->size, strerror(errno));
	close(fd);
	return -EIO;
    }
    close(fd);

    // re-enable bridge(s)
    // altera_set_fpga_bridge("hps2fpga", "1\n"); // currently unused
    // altera_set_fpga_bridge("fpga2hps", "1\n"); // currently unused
    altera_set_fpga_bridge("lwhps2fpga", "1\n");

    // load gpio_altera it again if it was present
    if (gpio_altera_was_loaded) {
	rc = run_shell("/sbin/modprobe %s", gpio_module);
	if (rc) {
	    LL_ERR("run_shell(modprobe %s) failed: %s",
		   gpio_module, strerror(errno));
	    return -errno;
	}
	// paranoia
	if (!is_module_loaded(gpio_module)) {
	    LL_ERR("module %s failed to load", gpio_module);
	    return -ENOENT;
	}
    }
    board->fpga_state = altera_fpga_status(fpga_sysfs_status, board->fpga_dev);
    if (board->fpga_state != ALT_FPGAMGR_STAT_USER_MODE) {
	LL_ERR("FPGA %s not in user mode post programming: state=%d",
	       devname, board->fpga_state);
        return -ENOENT;
    }
    rc = hm2_soc_mmap(this->private);
    if (rc) {
	LL_ERR("soc_mmap_fail");
	return -EINVAL;
    }
    return 0;
}

static int hm2_soc_mmap(hm2_soc_t *board) {

    volatile void *virtual_base;
    hm2_lowlevel_io_t *this = &board->llio;

    // we can mmap this device safely only if programmed so cop out
    if (board->fpga_state != ALT_FPGAMGR_STAT_USER_MODE) {
	LL_ERR("/dev/%s: invalid fpga state %d, unsafe to mmap %s",
	       board->fpga_dev, board->fpga_state, board->uio_dev);
	if (!board->firmware_given)
	    LL_ERR("the FPGA was not initialized and no"
		   " firmware was specified during 'loadrt %s'",HM2_LLIO_NAME);
	return -EIO;
    }

    board->uio_fd = open(board->uio_dev , ( O_RDWR | O_SYNC ));
    if (board->uio_fd < 0) {
        LL_ERR("Could not open %s: %s",  board->uio_dev, strerror(errno));
        return -errno;
    }
    virtual_base = mmap( NULL, HM2REG_IO_0_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED,
			 board->uio_fd, 0);

    if (virtual_base == MAP_FAILED) {
	LL_ERR( "mmap failed: %s", strerror(errno));
	close(board->uio_fd);
	board->uio_fd = -1;
	return -EINVAL;
    }
    if (debug)
	rtapi_print_hex_dump(RTAPI_MSG_INFO, RTAPI_DUMP_PREFIX_OFFSET,
			     16, 1, (const void *)virtual_base, 4096, 1,
			     "hm2 regs at %p:", virtual_base);

    // this duplicates stuff already happening in hm2_register - no harm
    u32 reg = *((u32 *)(virtual_base + HM2_ADDR_IOCOOKIE));
    if (reg != HM2_IOCOOKIE) {
	LL_ERR("invalid cookie, got 0x%08X, expected 0x%08X\n", reg, HM2_IOCOOKIE);
	LL_ERR( "FPGA failed to initialize, or unexpected firmware?\n");
	close(board->uio_fd);
	board->uio_fd = -1;
	return -EINVAL;
    }

    reg = *((u32 *)(virtual_base + HM2_ADDR_IDROM_OFFSET));
    hm2_idrom_t *idrom = (void *)(virtual_base + reg);

    LL_DBG("hm2 cookie check OK, board name='%8.8s'", idrom->board_name);

    void *configname = (void *)virtual_base + HM2_ADDR_CONFIGNAME;
    if (strncmp(configname, HM2_CONFIGNAME, HM2_CONFIGNAME_LENGTH) != 0) {
	LL_ERR("%s signature not found at %p", HM2_CONFIGNAME, configname);
	close(board->uio_fd);
	board->uio_fd = -1;
	return -EINVAL;
    }

    // use BoardNameHigh as board name - see http://freeby.mesanet.com/regmap
    rtapi_snprintf(this->name, sizeof(this->name), "hm2_%4.4s.%d",
		   idrom->board_name + 4, num_boards);
    strlwr(this->name);
    board->base = (void *)virtual_base;
    // now it's safe to read/write
    this->read = hm2_soc_read;
    this->write = hm2_soc_write;
    return 0;
}

static int hm2_soc_register(hm2_soc_t *brd,
			    const char *uiodev,
			    const char *fpgadev) {
    memset(brd, 0,  sizeof(hm2_soc_t));

    int state = altera_fpga_status(fpga_sysfs_status, fpgadev);
    if (state < 0) {
	return state;
    }

    brd->fpga_state = state;
    brd->fpga_dev = fpgadev;
    brd->uio_dev = uio_dev;

    brd->llio.comp_id = comp_id;
    brd->llio.num_ioport_connectors = 2;
    brd->llio.pins_per_connector = 17;
    brd->llio.ioport_connector_name[0] = "P3";
    brd->llio.ioport_connector_name[1] = "P2";
    brd->llio.fpga_part_number = 0; // "6slx9tqg144";
    brd->llio.num_leds = 2;

    // keeps hm2_register happy - will be overwritten
    // once the FPGA regs are accessible
    strcpy( brd->llio.name, "foo");

    brd->llio.threadsafe = 1;

    // not safe to read yet as fpga memory not yet mapped
    // so trap this on first downcall
    brd->llio.read = hm2_soc_unmapped_read;
    brd->llio.write = hm2_soc_unmapped_write;
    brd->llio.reset = hm2_soc_reset;

    // hm2_register will downcall on those if firmware= given
    brd->llio.program_fpga = hm2_soc_program_fpga;
    brd->llio.verify_firmware = hm2_soc_verify_firmware;

    brd->llio.private = brd;
    hm2_lowlevel_io_t *this =  &brd->llio;
    int r = hm2_register( this, config[0]);
    if (r != 0) {
        THIS_ERR("hm2_soc_board fails HM2 registration\n");
	close(brd->uio_fd);
	board->uio_fd = -1;
        return -EIO;
    }
    LL_PRINT("initialized AnyIO hm2_soc_board \n");
    num_boards++;
    return 0;
}


static int hm2_soc_munmap(hm2_soc_t *board) {
    if (board->base != NULL)
	munmap((void *) board->base, HM2REG_IO_0_SPAN);
    if (board->uio_fd > -1) {
	close(board->uio_fd);
	board->uio_fd = -1;
    }
    return(1);
}

int rtapi_app_main(void) {
    int r = 0;

    LL_PRINT("loading Mesa AnyIO HostMot2 socfpga driver version " HM2_SOCFPGA_VERSION "\n");

    if (!(r = is_module_loaded("hm2reg_ui"))) {
        LL_ERR("hm2reg_uio not loaded!");
        return r;
    }

    comp_id = hal_init(HM2_LLIO_NAME);
    if (comp_id < 0) return comp_id;

    r = hm2_soc_register(&board[0], uio_dev, fpga_dev);
    if (r != 0) {
        LL_ERR("error registering UIO driver\n");
        hal_exit(comp_id);
        return r;
    }

    if (failed_errno) {
	// at least one card registration failed
	hal_exit(comp_id);
	r = hm2_soc_munmap(&board[0]);
	return r;
    }

    if (num_boards == 0) {
	// no cards were detected
	LL_PRINT("error - no supported cards detected\n");
	hal_exit(comp_id);
	r = hm2_soc_munmap(&board[0]);
	return r;
    }
    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void) {

    hm2_unregister(&board->llio);
    hm2_soc_munmap(&board[0]);
    LL_PRINT("UIO driver unloaded\n");
    hal_exit(comp_id);
}

static int altera_set_fpga_bridge(const char *bridge, const char *value) {
    char fname[100];
    rtapi_snprintf(fname, sizeof(fname),"/sys/class/fpga-bridge/%s/enable", bridge);
    int rc = procfs_cmd(fname, value);
    if (rc < 0)
	LL_ERR( "write '%s' to '%s' failed: %s",
			value, fname, strerror(errno));
    return rc;
}

static char *strlwr(char *str)
{
  unsigned char *p = (unsigned char *)str;
  while (*p) {
     *p = tolower(*p);
      p++;
  }
  return str;
}

struct altera_fpga_state {
    int state;
    char *name;
};

static struct altera_fpga_state altera_fpga_states[] = {
    { ALT_FPGAMGR_STAT_POWER_UP, "power up phase" },
    { ALT_FPGAMGR_STAT_RESET, "reset phase" },
    { ALT_FPGAMGR_STAT_CFG, "configuration phase" },
    { ALT_FPGAMGR_STAT_INIT, "initialization phase" },
    { ALT_FPGAMGR_STAT_USER_MODE, "user mode" },
    { ALT_FPGAMGR_STAT_UNKNOWN, "undetermined" },
    { ALT_FPGAMGR_STAT_POWER_OFF, "powered off" },
    { -1, NULL }
};

static int altera_fpga_status(const char *fmt, ...)
{
    va_list ap;
    char sysfsname[100];

    va_start(ap, fmt);
    rtapi_vsnprintf(sysfsname, sizeof(sysfsname), fmt, ap);
    va_end(ap);

    //    rtapi_snprintf(sysfsname, sizeof(sysfsname), fpga_sysfs_status, fpga_dev);
    int fd = open(sysfsname, O_RDONLY);
    if (fd < 0) {
	LL_ERR( "Failed to open sysfs entry '%s': %s\n",
		sysfsname, strerror(errno));
	return -errno;
    }
    char status[100];
    memset(status, 0, sizeof(status));
    int len = read(fd, status, sizeof(status));
    if (len < 0)  {
	LL_ERR( "Failed to read sysfs entry '%s': %s\n",
		sysfsname, strerror(errno));
	close(fd);
	return -errno;
    }
    close(fd);
    struct altera_fpga_state *fs = altera_fpga_states;
    while (fs->state > -1) {
	if (strncmp(status, fs->name, strlen(fs->name)) == 0) {
	    LL_DBG( "FPGA /dev/%s status: %s", fpga_dev, status);
	    return fs->state;
	}
	fs++;
    }
    LL_ERR( "FPGA /dev/%s: unknown status: %s", fpga_dev, status);
    return -EINVAL;
}
