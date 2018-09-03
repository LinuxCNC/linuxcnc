//
//    Copyright (C) 2015-2016  Michael Brown, Devin Hughes, Michael Haberler
//
//    loosely based on hm2_pci.c, Copyright (C) 2007-2008 Sebastian Kuzminsky
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
//    Copyright (C) 2016 Michael Haberler
//
//    Rewritten for overlay based FPGA manager interface
//    Copyright (C) 2016 Devin Hughes, JD Squared

/* a matching device tree overlay - Altera Terasic DE0-Nano:

/dts-v1/;/plugin/;

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

			hm2reg_io_0: hm2-socfpga0@0x40000 {
				compatible = "generic-uio,ui_pdrv";
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

Xilinx Zyn, MYIR Z-Turn:

/dts-v1/; /plugin/;

/ {
  fragment@0 {
    target = <&base_fpga_region>;
    #address-cells = <1>;
    #size-cells = <1>;
    __overlay__ {
      #address-cells = <1>;
      #size-cells = <1>;

          firmware-name = "zynq/zturn_ztio.bit.bin";

          hm2reg_io_0: hm2-socfpga0@0x43C00000 {
                compatible = "generic-uio,ui_pdrv";
                reg = < 0x43C00000 0x00010000 >;
                interrupt-parent = <&intc>;
                interrupts = <0 29 1>;
          };
    };
  };
};

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

// on a time-critical path. Unlikely to happen - never seen so far.
#define WARN_ON_UNALIGNED_ACCESS 1

#define MAXUIOIDS  100
#define MAXNAMELEN 256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Brown & Michael Haberler & Devin Hughes");
// Instantiation param changes by Mick Grant <arceyeATmgwareDOTcoDOTuk> 01022017
MODULE_DESCRIPTION("Low level driver for HostMot2 on SoC Based Devices");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-5i25");

// module-level params - strings should be passed through argc/argv mechanism
static int debug = 0;
RTAPI_IP_INT(debug, "turn on extra debug output");

static int no_init_llio = 0;
RTAPI_IP_INT(no_init_llio, "debugging - if 1, do not set any llio fields (like num_leds");

static int num = 0;
RTAPI_IP_INT(num, "hm2 instance number, used for <boardname>.<num>.<pinname>");

static int already_programmed = 0;
RTAPI_IP_INT(already_programmed, "if 1 - fpga is already programmed and defined in current device tree");

static int comp_id = -1;

/* derive configfs folder from instance name which is unique */
static char *configfs_dir = "/sys/kernel/config/device-tree/overlays/%s";
static char *configfs_path = "/sys/kernel/config/device-tree/overlays/%s/path";
static char *configfs_status = "/sys/kernel/config/device-tree/overlays/%s/status";

static int hm2_soc_mmap(hm2_soc_t *brd);
static int fpga_status(const char *name);
static char *strlwr(char *str);
static int locate_uio_device(hm2_soc_t *brd, const char *name);

//
// these are the "low-level I/O" functions exported up
//
static int hm2_soc_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *brd = this->private;

#ifdef WARN_ON_UNALIGNED_ACCESS
    /* hm2_read_idrom performs a 16-bit read, which seems to be OK, so let's allow it */
    if ( ((addr & 0x1) != 0) || (((size & 0x03) != 0)  && (size != 2))) {
	LL_ERR( "hm2_soc_read: Unaligned Access: %08x %04x\n", addr,size);
    }
#endif
    memcpy(buffer, brd->base + addr, size);
    return 1;  // success
}

static int hm2_soc_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_soc_t *brd = this->private;

#ifdef WARN_ON_UNALIGNED_ACCESS
    // Per Peter Wallace, all hostmot2 access should be 32 bits and 32-bit aligned
    // Check for any address or size values that violate this alignment
    if ((addr & 0x3) || (size & 0x3)) {
	LL_ERR( "hm2_soc_write: Unaligned Access: %08x %04x\n", addr, size);
    }
#endif
    memcpy(brd->base + addr, buffer, size);
    return 1;
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
    char buf[MAXNAMELEN];

    LL_DBG("soc_program_fpga");

    // To program with an overlay, simply create a folder in the
    // configfs device-tree/overlays/ folder. The kernel autogenerates
    // three files in the directory. Write the name of a valid overlay
    // on the firmware path to the "path" node and it will do all of the
    // magic.

    rtapi_snprintf(buf, sizeof(buf), configfs_dir, brd->name);

    DIR* dir = opendir(buf);
    if(dir) {
        // old config left behind? - Directory always appears empty
        // so okay to call rmdir()
        if(rmdir(buf) < 0) {
            LL_ERR("rmdir(%s) failed: %s", buf, strerror(errno));
            return -EIO;
        }
    }

    // umask will clip permissions to match parent
    if(mkdir(buf, 0777) < 0) {
        LL_ERR("mkdir(%s) failed: %s", buf, strerror(errno));
        return -EIO;
    }

    int fd;
    rtapi_snprintf(buf, sizeof(buf), configfs_path, brd->name);

    // Spin to give configfs time to make the files
    int retries = 10;
    while(retries > 0) {
        fd = open(buf, O_WRONLY);
        if (fd != -1)
            break;
        retries--;
        usleep(200000);
    }

    if (fd < 0) {
        LL_ERR("open(%s) failed: %s", buf, strerror(errno));
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
        brd->fpga_state = fpga_status(brd->name);
        if (brd->fpga_state == DTOV_STAT_APPLIED)
            break;
        retries--;
        usleep(250000);
    }

    if (brd->fpga_state != DTOV_STAT_APPLIED) {
        LL_ERR("DTOverlay status is not applied post programming: name=%s state=%d",
	       brd->name, brd->fpga_state);
        return -ENOENT;
    }

    rc = hm2_soc_mmap(this->private);
    if (rc) {
        LL_ERR("soc_mmap_fail %s", brd->name);
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
	ret = locate_uio_device(brd, brd->name);
	if (ret == 0)
	    break;
        retries--;
        usleep(200000);
    }
    if (ret || (brd->uio_dev == NULL)) {
	LL_ERR("failed to map %s to /dev/uioX\n", brd->name);
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

    if (brd->debug)
        rtapi_print_hex_dump(RTAPI_MSG_INFO, RTAPI_DUMP_PREFIX_OFFSET,
			     16, 1, (const void *)virtual_base, 4096, 1, NULL,
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
                   idrom->board_name + 4, brd->num);
    strlwr(this->name);
    brd->base = (void *)virtual_base;
    // now it's safe to read/write
    this->read = hm2_soc_read;
    this->write = hm2_soc_write;

    // handle irq setup request
    if (this->host_wants_irq != 0)
        this->irq_fd = brd->uio_fd;

    return 0;
}


static int hm2_soc_register(hm2_soc_t *brd, void *fwid, size_t fwid_len, int inst_id)
{
    // no directory to check state yet, so it's unknown
    brd->fpga_state = -1;

    // pins are owned by instance so they go away on delinst
    brd->llio.comp_id = inst_id;

    if (brd->no_init_llio) {
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

    if (!brd->already_programmed) {
	// hm2_register will downcall on those if firmware= given
	brd->llio.program_fpga = hm2_soc_program_fpga;
	brd->llio.verify_firmware = hm2_soc_verify_firmware;
    } else {
	// we need this branch for soc boards with pre-programmed fpga
	// so, we assume that fpga programmed from u-boot or later and
	// active device tree have generic-uio compatible hm2-socfpga0 node
	LL_DBG("mapping pre-programmed hm2_soc_ol_board %s\n", brd->name);
	// set fpga state flag to "programmed"
	brd->fpga_state = DTOV_STAT_APPLIED;
	// try to map hm2 using available /sys/class/uio*
	int r = hm2_soc_mmap(brd);
	if (r) {
    	    LL_ERR("preloaded_soc_mmap_fail %s, err=%d", brd->name, r);
    	    return -EINVAL;
	}
    }

    // if descriptor=<filename> is given,
    // read it (see rtapi_app_main) and pass it up to hm2_register()
    // to override the fwid message in the FPGA
    brd->llio.fwid_msg = fwid;
    brd->llio.fwid_len = fwid_len;

    brd->llio.private = brd;
    hm2_lowlevel_io_t *this =  &brd->llio;

    int r = hm2_register(this, (char *)brd->config);
    if (r != 0) {
        THIS_ERR("hm2_soc_ol_board fails HM2 registration\n");
        close(brd->uio_fd);
        brd->uio_fd = -1;
        return r;
    }

    LL_PRINT("initialized AnyIO hm2_soc_ol_board %s on %s\n", brd->name, brd->uio_dev);
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


static int instantiate(const int argc, const char**argv)
{
    int r = 0, x;
    size_t nread = 0;
    void *blob = NULL;

    LL_PRINT("loading Mesa AnyIO HostMot2 socfpga overlay driver version " HM2_SOCFPGA_VERSION "\n");
    
    if(debug){
        for(x = 0; x < argc; x++){
            LL_DBG("argv[%d] = %s\n", x, argv[x]);
        }
    }

    // argv[0]: component name
    const char *name = argv[1]; // instance name
    hm2_soc_t *brd;

    // allocate a named instance, and HAL memory for the instance data
    int inst_id = hal_inst_create(name, comp_id, sizeof(hm2_soc_t), (void **)&brd);
    if (inst_id < 0)
        return -1;
    
    /********************************************************************************
    // Initialise and fill inst array values first
    //
    // Then zero or void instparam values or they will be passed to next instance
    // If they are boolean, just zeroing to the default is sufficient
    //
    // If they are a value within a value range, you should void it to -1, 
    // so that the instantiation code can insert the default value if it finds -1
    //
    // Alternatively it could flag an error if it means at least one previous 
    // instance exists and this value cannot be valid ( see num )
    **********************************************************************************/
    
    brd->name = name;
    brd->config = NULL;
    brd->descriptor = NULL;
    brd->argc = 0;
    brd->argv = NULL;
    // remove module and instance names
    if(argc > 2){
        brd->argc = argc - 2;
        brd->argv = &argv[2];
	for(x = 0; x < brd->argc; x++){
            if(strncmp(brd->argv[x], "config=", 7) == 0){
                brd->config = halg_strdup(1, &brd->argv[x][7]);
                continue;
            }
            if(strncmp(brd->argv[x], "descriptor=", 11) == 0)
                brd->descriptor = halg_strdup(1, &brd->argv[x][11]);
        }
    }
    if(!brd->argc || brd->config == NULL){
        LL_ERR("Error: no config string passed.\n");
        LL_ERR("Use newinst hm2_soc_ol hm2-socfpga0 <params> -- config=\"xxxxxxxxxxxxx\"\n");
        return -1;
    }
    brd->no_init_llio = no_init_llio;
    brd->debug = debug;
    brd->already_programmed = already_programmed;
    if(num == -1){
        LL_ERR("num set to -1 by previous instance.  Set a valid board number");
        return -1;
    }
    else 
        brd->num = num;
    // void parameters
    no_init_llio = 0;
    num = -1;
    debug = 0;
    
    // read a custom fwid message if given
    if (brd->descriptor != NULL) {
        struct stat st;
        if (stat(brd->descriptor, &st)) {
            LL_ERR("stat(%s) failed: %s\n", brd->descriptor,
                strerror(errno));
            return -EINVAL;
        }
        blob = malloc(st.st_size);
        if (blob == 0) {
            LL_ERR("malloc(%lu) failed: %s\n", st.st_size,
                strerror(errno));
            return -ENOMEM;
        }
        int fd = open(brd->descriptor, O_RDONLY);
        if (fd < 0) {
            LL_ERR("open(%s) failed: %s\n", brd->descriptor,
                strerror(errno));
            free(blob);
            return -EINVAL;
        }
        nread = read(fd, blob, st.st_size);
        if (nread != st.st_size) {
            LL_ERR("reading '%s': expected %zu got %u - %s\n", brd->descriptor,
                (unsigned) st.st_size, nread, strerror(errno));
            return -EINVAL;
        }
        close(fd);
        LL_DBG("custom descriptor '%s' size %zu", brd->descriptor, nread);
    }

    r = hm2_soc_register(brd, blob, nread, inst_id);
    if (blob)
        free(blob);
    if (r != 0) {
        LL_ERR("error registering UIO driver: %i\n",r);
        return -1;
    }
    return 0;
}

static int delete(const char *name, void *inst, const int inst_size)
{
    hm2_soc_t *brd = (hm2_soc_t *)inst;
    char buf[MAXNAMELEN];

    // explicitly free locally allocated strings in case realtime
    // continues to run after shutdown of this driver
    if(brd->config != NULL)
	halg_free_single_str(brd->config);
    if(brd->descriptor != NULL)
	halg_free_single_str(brd->descriptor);

    hm2_unregister(&brd->llio);
    int r = hm2_soc_munmap(brd);
    rtapi_snprintf(buf, sizeof(buf), configfs_dir, name);
    rmdir(buf);
    return r;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

int rtapi_app_main(void) {

    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, delete, HM2_LLIO_NAME);
    if (comp_id < 0)
	return comp_id;
    hal_ready(comp_id);
    return 0;
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

static int fpga_status(const char *name) {

    char buf[MAXNAMELEN];

    // The device tree status node only has two choices. If the state is applied
    // then the fpga was configured correctly, and any modules are probed.

    rtapi_snprintf(buf, sizeof(buf), configfs_status, name);

    int fd = open(buf, O_RDONLY);
    if (fd < 0) {
        LL_ERR( "Failed to open sysfs entry '%s': %s\n",
                buf, strerror(errno));
        return -errno;
    }

    char status[50];
    memset(status, 0, sizeof(status));
    int len = read(fd, status, sizeof(status));
    if (len < 0)  {
        LL_ERR( "Failed to read sysfs entry '%s': %s\n",
                buf, strerror(errno));
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
    LL_ERR( "FPGA overlay unknown status: %s %s", buf, status);
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
