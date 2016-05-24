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
//
//    Rewritten for overlay based FPGA manager interface
//    Copyright (C) 2016 Devin Hughes, JD Squared

/* a matching device tree overlay:

/dts-v1/ /plugin/;

/ {
	fragment@0 {
		target-path = "/soc/base_fpga_region";
		#address-cells = <1>;
		#size-cells = <1>;
		__overlay__ {
			#address-cells = <1>;
			#size-cells = <1>;

			ranges = <0x00040000 0xff240000 0x00010000>;
			firmware-name = "socfpga/DE0_NANO.rbf";

			hm2reg_io_0: hm2-socfpg0@0x40000 {
				compatible = "hm2reg_io,generic-uio,ui_pdrv";
				reg = <0x40000 0x10000>;
				interrupt-parent = <0x2>;
				interrupts = <0 43 1>;
				clocks = <&osc1>;
				address_width = <14>;
				data_width = <32>;
			};
		};
	};
};
# /usr/src/linux-headers-4.1.17-ltsi-rt17-gab2edea/scripts/dtc/dtc -I dts -O dtb -o hm2reg_uio.dtbo hm2reg_uio-irq.dts
see configs/hm2-soc-stepper/irqtest.hal for a usage example
 */
//---------------------------------------------------------------------------//

#include "config.h"

// this should be an general socfpga #define
/* #if !defined(TARGET_PLATFORM_SOCFPGA) */
/* #error "This driver is for the socfpga platform only" */
/* #endif */

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
#include "hal_priv.h"
#include "hal/lib/config_module.h"
#include "hostmot2-lowlevel.h"
#include "hostmot2.h"
#include "hm2_soc_ol.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

/* Wrap up dt state */
#define DTOV_STAT_UNAPPLIED	0x0
#define DTOV_STAT_APPLIED 	0x1

#define HM2REG_IO_0_SPAN 65536


#define MAXUIOIDS  100
#define MAXNAMELEN 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Brown & Michael Haberler & Devin Hughes");
MODULE_DESCRIPTION("Low level driver for HostMot2 on SoC Based Devices");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-5i25");

static char *config[HM2_SOC_MAX_BOARDS];
RTAPI_MP_ARRAY_STRING(config, HM2_SOC_MAX_BOARDS,
		      "config string for the AnyIO boards (see hostmot2(9) manpage)")
static int debug;
RTAPI_MP_INT(debug, "turn on extra debug output");

static char *name = "hm2-socfpg0";
RTAPI_MP_STRING(name, "logical device name, default hm2-socfpg0hm2-socfpg0");

static char *descriptor = NULL;
RTAPI_MP_STRING(descriptor, ".bin file with encoded fwid protobuf descriptor message");

static int no_init_llio;
RTAPI_MP_INT(no_init_llio, "debugging - if 1, do not set any llio fields (like num_leds");

static long timer1;
RTAPI_MP_INT(timer1, "rate for hm2 Timer1 IRQ, 0: IRQ disabled");


static int comp_id;
static hm2_soc_t board[HM2_SOC_MAX_BOARDS];
static int num_boards;
static int failed_errno = 0; // errno of last failed registration

/* Pick a configfs folder that is unlikely to conflict with other overlays */
static char *configfs_dir = "/sys/kernel/config/device-tree/overlays/hm2_soc_ol";
static char *configfs_path = "/sys/kernel/config/device-tree/overlays/hm2_soc_ol/path";
static char *configfs_status = "/sys/kernel/config/device-tree/overlays/hm2_soc_ol/status";
static int zero = 0;

static int hm2_soc_mmap(hm2_soc_t *brd);
static int waitirq(void *arg, const hal_funct_args_t *fa); // HAL thread funct
static int fpga_status();
static char *strlwr(char *str);
static int locate_uio_device(hm2_soc_t *brd, const char *name);

//
// these are the "low-level I/O" functions exported up
//
static int hm2_soc_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *brd = this->private;
    int i;
    u32* src = (u32*) (brd->base + addr);
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
    hm2_soc_t *brd = this->private;
    int i;
    u32* src = (u32*) buffer;
    u32* dst = (u32*) (brd->base + addr);

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
    hm2_soc_t *brd = this->private;
    int rc = hm2_soc_mmap(brd);
    if (rc)
	return rc;
    return hm2_soc_read(this, addr, buffer, size);
}

static int hm2_soc_unmapped_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *brd = this->private;
    int rc = hm2_soc_mmap(brd);
    if (rc)
	return rc;
    return hm2_soc_write(this, addr, buffer, size);
}

static int hm2_soc_verify_firmware(hm2_lowlevel_io_t *this,  const struct firmware *fw) {
    // Overlay based drivers need the *name* of the overlay, not the contents.
    if(this->firmware == NULL) {
	LL_ERR("NULL firmware name, unable to program fpga");
        return -EINVAL;
    }
    return 0;
}

static int hm2_soc_reset(hm2_lowlevel_io_t *this) {
    // currently not needed
    LL_DBG( "soc_reset");
    return 0;
}

/* Updated to program fpga by applying overlay */
static int hm2_soc_program_fpga(hm2_lowlevel_io_t *this,
				const bitfile_t *bitfile,
				const struct firmware *fw) {

    int rc;
    hm2_soc_t *brd =  this->private;

    LL_DBG("soc_program_fpga");

    // To program with an overlay, simply create a folder in the
    // configfs device-tree/overlays/ folder. The kernel autogenerates
    // three files in the directory. Write the name of a valid overlay
    // on the firmware path to the "path" node and it will do all of the
    // magic.

    DIR* dir = opendir(configfs_dir);
    if(dir) {
        // old config left behind? - Directory always appears empty
        // so okay to call rmdir()
        if(rmdir(configfs_dir) < 0) {
            LL_ERR("rmdir(%s) failed: %s", configfs_dir, strerror(errno));
            return -EIO;
        }
    }

    // umask will clip permissions to match parent
    if(mkdir(configfs_dir, 0777) < 0) {
        LL_ERR("mkdir(%s) failed: %s", configfs_dir, strerror(errno));
        return -EIO;
    }

    int fd;

    // Spin to give configfs time to make the files
    int retries = 10;
    while(retries > 0) {
        fd = open(configfs_path, O_WRONLY);
        if (fd != -1)
            break;
        retries--;
        usleep(200000);
    }

    if (fd < 0) {
        LL_ERR("open(%s) failed: %s", configfs_path, strerror(errno));
        return -EIO;
    }

    size_t size = strlen(this->firmware);

    // load FPGA by writing the name of the overlay to the path node
    if (write(fd, this->firmware, size) != size)  {
        LL_ERR("write(%s, %zu) failed: %s",
            this->firmware, size, strerror(errno));
        close(fd);
        return -EIO;
    }
    close(fd);

    // Spin to give fpga time to program
    retries = 12;
    while(retries > 0) {
        brd->fpga_state = fpga_status();
        if (brd->fpga_state == DTOV_STAT_APPLIED)
            break;
        retries--;
        usleep(250000);
    }

    if (brd->fpga_state != DTOV_STAT_APPLIED) {
        LL_ERR("DTOverlay status is not applied post programming: state=%d",
            brd->fpga_state);
        return -ENOENT;
    }

    rc = hm2_soc_mmap(this->private);
    if (rc) {
        LL_ERR("soc_mmap_fail");
        return -EINVAL;
    }
    return 0;
}

static int hm2_soc_mmap(hm2_soc_t *brd) {

    volatile void *virtual_base;
    hm2_lowlevel_io_t *this = &brd->llio;

    // we can mmap this device safely only if programmed so cop out
    if (brd->fpga_state != DTOV_STAT_APPLIED) {
        LL_ERR("invalid fpga state %d, unsafe to mmap %s",
            brd->fpga_state, brd->uio_dev);
        return -EIO;
    }

    // Fix race from overlay framework
    // spin to give uio device time to appear with proper permissions
    int retries = 10;
    int ret;
    while(retries > 0) {
	ret = locate_uio_device(brd, name);
	if (ret == 0)
	    break;
        retries--;
        usleep(200000);
    }
    if (ret || (brd->uio_dev == NULL)) {
	LL_ERR("failed to map %s to /dev/uioX\n", name);
	return ret;
    }
    brd->uio_fd = open(brd->uio_dev , ( O_RDWR | O_SYNC ));
    if (brd->uio_fd < 0) {
        LL_ERR("Could not open %s: %s",  brd->uio_dev, strerror(errno));
        return -errno;
    }
    virtual_base = mmap( NULL, HM2REG_IO_0_SPAN, ( PROT_READ | PROT_WRITE ),
                         MAP_SHARED, brd->uio_fd, 0);

    if (virtual_base == MAP_FAILED) {
        LL_ERR( "mmap failed: %s", strerror(errno));
            close(brd->uio_fd);
        brd->uio_fd = -1;
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
        close(brd->uio_fd);
        brd->uio_fd = -1;
        return -EINVAL;
    }

    reg = *((u32 *)(virtual_base + HM2_ADDR_IDROM_OFFSET));
    hm2_idrom_t *idrom = (void *)(virtual_base + reg);

    LL_DBG("hm2 cookie check OK, board name='%8.8s'", idrom->board_name);

    void *configname = (void *)virtual_base + HM2_ADDR_CONFIGNAME;
    if (strncmp(configname, HM2_CONFIGNAME, HM2_CONFIGNAME_LENGTH) != 0) {
        LL_ERR("%s signature not found at %p", HM2_CONFIGNAME, configname);
        close(brd->uio_fd);
        brd->uio_fd = -1;
        return -EINVAL;
    }

    // use BoardNameHigh as board name - see http://freeby.mesanet.com/regmap
    rtapi_snprintf(this->name, sizeof(this->name), "hm2_%4.4s.%d",
                   idrom->board_name + 4, num_boards);
    strlwr(this->name);
    brd->base = (void *)virtual_base;
    // now it's safe to read/write
    this->read = hm2_soc_read;
    this->write = hm2_soc_write;
    return 0;
}

static int hm2_soc_register(hm2_soc_t *brd,
			    const char *name,
			    void *fwid,
			    size_t fwid_len)
{

    brd->name = name;

    // no directory to check state yet, so it's unknown
    brd->fpga_state = -1;

    brd->llio.comp_id = comp_id;

    if (no_init_llio) {
	// defer to initialization in hm2_register()
	// via fwid message
	brd->llio.num_ioport_connectors = 0;
	brd->llio.pins_per_connector = 0;
	brd->llio.ioport_connector_name[0] = NULL;
	brd->llio.fpga_part_number = 0;
	brd->llio.num_leds = 0;
   } else {
	brd->llio.num_ioport_connectors = 4;
	brd->llio.pins_per_connector = 17;
	brd->llio.ioport_connector_name[0] = "GPIO0.P3";
	brd->llio.ioport_connector_name[1] = "GPIO0.P2";
	brd->llio.ioport_connector_name[2] = "GPIO1.P3";
	brd->llio.ioport_connector_name[3] = "GPIO1.P2";
	brd->llio.fpga_part_number = 0; // "6slx9tqg144";
	brd->llio.num_leds = 4;
    }

    // keeps hm2_register happy - will be overwritten
    // once the FPGA regs are accessible
    strcpy( brd->llio.name, "foo");

    brd->llio.threadsafe = 1;

    // not yet safe to read as fpga memory not yet mapped
    // so trap first downcall to read or write,
    // and mmap() there
    brd->llio.read = hm2_soc_unmapped_read;
    brd->llio.write = hm2_soc_unmapped_write;

    brd->llio.reset = hm2_soc_reset;

    // hm2_register will downcall on those if firmware= given
    brd->llio.program_fpga = hm2_soc_program_fpga;
    brd->llio.verify_firmware = hm2_soc_verify_firmware;

    // if descriptor=<filename> is given,
    // read it (see rtapi_app_main) and pass it up to hm2_register()
    // to override the fwid message in the FPGA
    brd->llio.fwid_msg = fwid;
    brd->llio.fwid_len = fwid_len;

    brd->llio.private = brd;
    hm2_lowlevel_io_t *this =  &brd->llio;

    int r = hm2_register(this, config[0]);
    if (r != 0) {
        THIS_ERR("hm2_soc_ol_board fails HM2 registration\n");
        close(brd->uio_fd);
        brd->uio_fd = -1;
        return r;
    }

    LL_PRINT("initialized AnyIO hm2_soc_ol_board %s on %s\n", brd->name, brd->uio_dev);
    num_boards++;
    return 0;
}


static int hm2_soc_munmap(hm2_soc_t *brd) {
    if (brd->base != NULL)
        munmap((void *) brd->base, HM2REG_IO_0_SPAN);
    if (brd->uio_fd > -1) {
        close(brd->uio_fd);
        brd->uio_fd = -1;
    }
    return(1);
}

int rtapi_app_main(void) {
    int r = 0;
    size_t nread = 0;
    void *blob = NULL;

    LL_PRINT("loading Mesa AnyIO HostMot2 socfpga overlay driver version " HM2_SOCFPGA_VERSION "\n");

    // read a custom fwid message if given
    if (descriptor) {
	struct stat st;
	if (stat(descriptor, &st)) {
	    LL_ERR("stat(%s) failed: %s\n", descriptor,
		   strerror(errno));
	    return -EINVAL;
	}
	blob = malloc(st.st_size);
	if (blob == 0) {
	    LL_ERR("malloc(%lu) failed: %s\n", st.st_size,
		   strerror(errno));
	    return -ENOMEM;
	}
	int fd = open(descriptor, O_RDONLY);
	if (fd < 0) {
	    LL_ERR("open(%s) failed: %s\n", descriptor,
		   strerror(errno));
	    free(blob);
	    return -EINVAL;
	}
	nread = read(fd, blob, st.st_size);
	if (nread != st.st_size) {
	    LL_ERR("reading '%s': expected %zu got %u - %s\n",
		   descriptor,
		   (unsigned) st.st_size, nread, strerror(errno));
	    return -EINVAL;
	}
	close(fd);
	LL_DBG("custom descriptor '%s' size %zu", descriptor, nread);
    }

    comp_id = hal_init(HM2_LLIO_NAME);
    if (comp_id < 0) return comp_id;

    hm2_soc_t *brd = &board[0];
    memset(brd, 0,  sizeof(hm2_soc_t));


    r = hm2_soc_register(brd, name, blob, nread);
    if (r != 0) {
        LL_ERR("error registering UIO driver\n");
        hal_exit(comp_id);
        return r;
    }

    if (failed_errno) {
        // at least one card registration failed
        hal_exit(comp_id);
        r = hm2_soc_munmap(brd);
        return r;
    }

    if (num_boards == 0) {
        // no cards were detected
        LL_PRINT("error - no supported cards detected\n");
        hal_exit(comp_id);
        r = hm2_soc_munmap(brd);
        return r;
    }

    // enable time IRQ's
    if (timer1) {

	brd->pins = hal_malloc(sizeof(hm2_soc_pins_t));
	if (!brd->pins)
	    return -1;

	if (hal_pin_u32_newf(HAL_OUT, &brd->pins->irq_count, comp_id, "%s.irq.count", brd->llio.name) ||
	    hal_pin_u32_newf(HAL_OUT, &brd->pins->irq_missed, comp_id, "%s.irq.missed", brd->llio.name) ||
	    hal_pin_u32_newf(HAL_OUT, &brd->pins->write_errors, comp_id, "%s.errors.write", brd->llio.name) ||
	    hal_pin_u32_newf(HAL_OUT, &brd->pins->read_errors, comp_id, "%s.errors.read", brd->llio.name))
	    return -1;

	hal_export_xfunct_args_t xfunct_args = {
	    .type = FS_XTHREADFUNC,
	    .funct.x = waitirq,
	    .arg = brd,
	    .uses_fp = 0,
	    .reentrant = 0,
	    .owner_id = comp_id
	};
	if ((r = hal_export_xfunctf(&xfunct_args, "waitirq.%d", 0)) != 0) {
	    LL_PRINT("hal_export waitirq failed - %d\n", r);
	    hal_exit(comp_id);
	    r = hm2_soc_munmap(brd);
	    return r;
	}

	// enable timer 1 interrupt
	int val = 2;
	hm2_soc_write(&brd->llio, HM2_IRQ_STATUS_REG , (void *)&val, sizeof(val));

    } else {
	LL_PRINT("timer1 argument 0 - waitirq function not exported\n");
    }
    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void)
{
    hm2_soc_t *brd = &board[0];

    hm2_unregister(&brd->llio);
    hm2_soc_munmap(brd);
    // Clean up the overlay
    rmdir(configfs_dir);
    LL_PRINT("UIO driver unloaded\n");
    hal_exit(comp_id);
}


static char *strlwr(char *str) {
  unsigned char *p = (unsigned char *)str;
  while (*p) {
     *p = tolower(*p);
      p++;
  }
  return str;
}

struct dt_ol_state {
    int state;
    char *name;
};

static struct dt_ol_state dt_ol_states[] = {
    { DTOV_STAT_UNAPPLIED, "unapplied" },
    { DTOV_STAT_APPLIED, "applied" },
    { -1, NULL }
};

static int fpga_status() {
    // The device tree status node only has two choices. If the state is applied
    // then the fpga was configured correctly, and any modules are probed.
    int fd = open(configfs_status, O_RDONLY);
    if (fd < 0) {
        LL_ERR( "Failed to open sysfs entry '%s': %s\n",
                configfs_status, strerror(errno));
        return -errno;
    }

    char status[50];
    memset(status, 0, sizeof(status));
    int len = read(fd, status, sizeof(status));
    if (len < 0)  {
        LL_ERR( "Failed to read sysfs entry '%s': %s\n",
                configfs_status, strerror(errno));
        close(fd);
        return -errno;
    }
    close(fd);
    struct dt_ol_state *fs = dt_ol_states;
    while (fs->state > -1) {
        if (strncmp(status, fs->name, strlen(fs->name)) == 0) {
            LL_DBG( "FPGA overlay status: %s", status);
            return fs->state;
        }
        fs++;
    }

    // state wasn't recognized from array
    LL_ERR( "FPGA overlay unknown status: %s", status);
    return -EINVAL;
}

// fills in brd->uio_dev based on name
static int locate_uio_device(hm2_soc_t *brd, const char *name)
{
    char buf[MAXNAMELEN];
    int uio_id;

    for (uio_id = 0; uio_id < MAXUIOIDS; uio_id++) {
	if (rtapi_fs_read(buf, MAXNAMELEN, "/sys/class/uio/uio%d/name", uio_id) < 0)
	    continue;
	if (strncmp(name, buf, strlen(name)) == 0)
	    break;
    }
    if (uio_id >= MAXUIOIDS)
	return -1;

    rtapi_snprintf(buf, sizeof(buf), "/dev/uio%d", uio_id);
    brd->uio_dev = strdup(buf);
    return 0;
}


static int waitirq(void *arg, const hal_funct_args_t *fa)
{
    hm2_soc_t *brd = arg;
    uint32_t info;
    ssize_t nb;

    info = 1; /* unmask */

    nb = write(brd->uio_fd, &info, sizeof(info));
    if (nb < sizeof(info)) {
	*(brd->pins->write_errors) += 1;
    }

    info = 0;
    // wait for IRQ
    nb = read(brd->uio_fd, &info, sizeof(info));
    if (nb != sizeof(info)) {
	*(brd->pins->read_errors) += 1;
    }
    *(brd->pins->irq_count) += 1;
    *(brd->pins->irq_missed) = info - *(brd->pins->irq_count);

    // clear pending IRQ
    hm2_soc_write(&brd->llio, HM2_CLEAR_IRQ_REG, &zero, sizeof(zero));

    return 0;
}
