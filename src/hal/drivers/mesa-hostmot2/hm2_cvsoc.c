
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
//#include "rtapi_math.h"
#include "rtapi_string.h"
//#include "rtapi_pci.h"

#include "hal.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


//#include "hal/drivers/mesa-hostmot2/bitfile.h"
#include "hal/drivers/mesa-hostmot2/hostmot2-lowlevel.h"
#include "hm2_cvsoc.h"

//#include "../Include/mkhm2soc/hps_0.h"
#define HM2REG_IO_0_SPAN 65536


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Brown");
MODULE_DESCRIPTION("Driver initially for HostMot2 on the DE0 Nano / Atlas Cyclone V socfpga board from Terasic");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-5i25");  // FIXME


static char *config[HM2_SOC_MAX_BOARDS];
RTAPI_MP_ARRAY_STRING(config, HM2_SOC_MAX_BOARDS, "config string for the AnyIO boards (see hostmot2(9) manpage)")

static int comp_id;
static int  uio_fd;


// FIXME: should probably have a linked list of boards instead of an array
static hm2_cvsoc_t hm2_cvsoc_board[HM2_SOC_MAX_BOARDS];
static int num_boards = 0;
//static int num_5i20 = 0;
//static int num_5i21 = 0;
//static int num_5i22 = 0;
//static int num_5i23 = 0;
//static int num_5i24 = 0;
static int num_5i25 = 0;
static int num_6i25 = 0;
//static int num_4i65 = 0;
//static int num_4i68 = 0;
//static int num_4i69 = 0;
//static int num_3x20 = 0;
static int failed_errno=0; // errno of last failed registration


//---------------------------------------------------------------//



/* // probe string for uio driver
static const struct of_device_id uio_of_genirq_match[] = {
    { .compatible = "machkt,hm2reg-io-1.0", },
    { }
};
*/

// this struct contains the hm2 interface ip core info provided in the device-tree

static struct dts_device_id hm2_cvsoc_tbl[] = {
        
    // 5i25
    {
        .address_width = 14,        //0x0000000E
        .clocks = 2,                //0x00000002 number of clocks ?
        .compatible = {
            .vendor = "machkt",       //6D 61 63 68 6B 74 2C
            .name = "hm2reg-io",      //68 6D 32 72 65 67 2D 69 6F 2D 31 2E 30 00
        }, // (machkt,hm2reg-io-1.0.)
        .data_width = 32,           // 0x00000020
        .name = "hm2-socfpga",        //68 6D 32 2D 73 6F 63 66 70 67 61 00 (hm2-socfpga.)
        .reg = {0x00000001, 0x00040000, 0x00010000},// ?, address offset from bridge, address span (= max address +1)
    },
    {0,},
};

//MODULE_DEVICE_TABLE(soc, hm2_cvsoc_tbl);


// 
// these are the "low-level I/O" functions exported up
//

static int hm2_cvsoc_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_cvsoc_t *board = this->private;
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
            rtapi_print_msg(RTAPI_MSG_ERR, "hm2_cvsoc_read: Unaligned Access: %08x %04x\n", addr,size);
            memcpy(dst, src, size);
            return 1;  // success
        }
        dst16[0] = src16[0];
        return 1;  // success
    }

//    rtapi_print_msg(RTAPI_MSG_ERR, "pci_read : %08x.%04x", addr,size);
    for (i=0; i<(size/4); i++) {
        dst[i] = src[i];
//        rtapi_print_msg(RTAPI_MSG_ERR, " %08x", dst[i]);
    }
//    rtapi_print_msg(RTAPI_MSG_ERR, "\n");
    return 1;  // success
}

static int hm2_cvsoc_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    hm2_cvsoc_t *board = this->private;
    int i;
    u32* src = (u32*) buffer;
    u32* dst = (u32*) (board->base + addr);

    /* Per Peter Wallace, all hostmot2 access should be 32 bits and 32-bit aligned */
    /* Check for any address or size values that violate this alignment */
    if ( ((addr & 0x3) != 0) || ((size & 0x03) != 0) ){
        rtapi_print_msg(RTAPI_MSG_ERR, "hm2_cvsoc_write: Unaligned Access: %08x %04x\n", addr,size);
        memcpy(dst, src, size);
        return 1;  // success
    }

//    rtapi_print_msg(RTAPI_MSG_ERR, "pci_write: %08x.%04x", addr,size);
    for (i=0; i<(size/4); i++) {
//        rtapi_print_msg(RTAPI_MSG_ERR, " %08x", src[i]);
        dst[i] = src[i];
    }
//    rtapi_print_msg(RTAPI_MSG_ERR, "\n");
    return 1;  // success
}


/*
static int hm2_cvsoc_program_fpga(hm2_lowlevel_io_t *this, const bitfile_t *bitfile) {

//    disable bridges:  
    echo 0 > hps2fpga, lwhps2fpga, fpga2hps
    cat bitfile > /dev/fpga
    //    enable bridges:  
    echo 1 > hps2fpga, lwhps2fpga, fpga2hps

    return 0;


fail:
    ???
    return -EIO;
}
*/


/*
static int hm2_cvsoc_reset(hm2_lowlevel_io_t *this) {
    return 0;
}
*/
// 
// misc internal functions
//

/*
static int hm2_cvsoc_probe(struct cvsoc_dev *dev, const struct dts_device_id *id) {
    int r;
    hm2_cvsoc_t *board;
    hm2_lowlevel_io_t *this;

// could there be more than 1 hm2 interface needed on a soc ?
//    if (num_boards >= HM2_SOC_MAX_BOARDS) {
//        LL_PRINT("skipping hm2 soc interface at %s, this driver can only handle %d\n", dts_name(dev), HM2_SOC_MAX_BOARDS);
//        return -EINVAL;
//    }

    board = &hm2_cvsoc_board[num_boards];
    this = &board->llio;
    memset(this, 0, sizeof(hm2_lowlevel_io_t));

//    switch (dev->name) {
    switch (dts_device_id->name) {
            
        case hm2-socfpga: {
            if (dts_device_id->compatible->name == hm2reg-io) {
//                LL_PRINT("discovered hm2reg-io at %s\n", dts_name(dev));
                LL_PRINT("discovered hm2reg-io at entity (machkt,hm2reg-io-1.0.)\n");                
//                rtapi_snprintf(board->llio.name, sizeof(board->llio.name), "hm2_5i25.%d", num_5i25);
                num_5i25 ++;
            }
//            else {
//                LL_PRINT("discovered 6i25 at %s\n", dts_name(dev));
//                rtapi_snprintf(board->llio.name, sizeof(board->llio.name), "hm2_6i25.%d", num_6i25);
//                num_6i25 ++;
//            }
            board->llio.num_ioport_connectors = 2;
            board->llio.pins_per_connector = 17;
            board->llio.ioport_connector_name[0] = "P3";
            board->llio.ioport_connector_name[1] = "P2";
            board->llio.fpga_part_number = "6slx9tqg144";
            board->llio.num_leds = 2;
            break;
        }

        default: {
//            LL_ERR("unknown subsystem device id 0x%04x\n", dev->name);
            LL_ERR("unknown dts subsystem \n");
            return failed_errno = -ENODEV;
        }
    }


    switch (dev->name) {
//        case HM2_PCI_DEV_MESA5I25:
        case hm2-socfpga: {
              // mksocfpga_io_hm2 is 64K mem (32 bit)
//            board->len = pci_resource_len(dev, 0);
            board->base = pci_ioremap_bar(dev, 0);
            if (board->base == NULL) {
                THIS_ERR("could not map in FPGA address space\n");
                r = -ENODEV;
                goto fail0;
            }
            break;
        }

        default: {
            THIS_ERR("unknown DTS Device ID 0x%04x\n", dev->group);
            r = -ENODEV;
            goto fail0;
        }
    }


    board->dev = dev;

//    pci_set_drvdata(dev, board);
    cvsoc_set_drvdata(dev, board);
    
    board->llio.comp_id = comp_id;
    board->llio.private = board;

    board->llio.threadsafe = 1;

    board->llio.read = hm2_cvsoc_read;
    board->llio.write = hm2_cvsoc_write;

    r = hm2_register(&board->llio, config[num_boards]);
    if (r != 0) {
        THIS_ERR("board fails HM2 registration\n");
        goto fail1;
    }

//    THIS_PRINT("initialized AnyIO board at %s\n", dts_name(dev));
    THIS_PRINT("initialized AnyIO HM2 core at %s\n", cvsoc_name(dev));
    
    num_boards ++;
    return 0;


fail1:
//    pci_set_drvdata(dev, NULL);
    cvsoc_set_drvdata(dev, NULL);
    iounmap(board->base);
    board->base = NULL;

fail0:
//    cvsoc_disable_device(dev);
    return failed_errno = r;
}
*/
/*
static void hm2_cvsoc_remove(struct cvsoc_dev *dev) {
    int i;

    for (i = 0; i < num_boards; i++) {
        hm2_cvsoc_t *board = &hm2_cvsoc_board[i];
        hm2_lowlevel_io_t *this = &board->llio;

        if (board->dev == dev) {
            THIS_PRINT("dropping AnyIO board at %s\n", dts_name(dev));

            hm2_unregister(&board->llio);

            // Unmap board memory
            if (board->base != NULL) {
                iounmap(board->base);
                board->base = NULL;
            }

            pci_disable_device(dev);
            pci_set_drvdata(dev, NULL);
            board->dev = NULL;
        }
    }
}
*/
/*
static struct uio_driver hm2_cvsoc_driver = {
	.name = HM2_LLIO_NAME,
	.id_table = hm2_cvsoc_tbl,
	.probe = hm2_cvsoc_probe,
	.remove = hm2_cvsoc_remove,
};
*/

//static int hm2_cvsoc_mmap(const struct dts_device_id *id) {
//static int hm2_cvsoc_mmap(struct cvsoc_dev *dev, const struct dts_device_id *id) {
static int hm2_cvsoc_mmap(void) {

    hm2_cvsoc_t *me;
    hm2_lowlevel_io_t *this;
    int r = 0;

    /* Open the resource node */
    uio_fd = open ( "/dev/uio0", ( O_RDWR | O_SYNC ) );
    if (uio_fd < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Could not open UIO resource for: hm2_mksocfpga . (%s)\n", strerror(errno));
        return 0;
    }
    // get virtual addr that maps to physical
    virtual_base = mmap( NULL, HM2REG_IO_0_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, uio_fd, 0);
    return (1);
    

    me = &hm2_cvsoc_board[0];
    this = &me->llio;
     rtapi_snprintf(me->llio.name, sizeof(me->llio.name), "hm2_5i25.%d", num_5i25);
    
    me->llio.num_ioport_connectors = 2;
    me->llio.pins_per_connector = 17;
    me->llio.ioport_connector_name[0] = "P3";
    me->llio.ioport_connector_name[1] = "P2";
    me->llio.fpga_part_number = "6slx9tqg144";
    me->llio.num_leds = 2;
    
    me->llio.comp_id = comp_id;
    me->llio.private = me;

    me->llio.threadsafe = 1;

    me->llio.read = hm2_cvsoc_read;
    me->llio.write = hm2_cvsoc_write;

    r = hm2_register(&hm2_cvsoc_board->llio, config[num_boards]);

    if (r != 0) {
        THIS_ERR("hm2_cvsoc_board fails HM2 registration\n");
        return -EIO;
    }

    THIS_PRINT("initialized AnyIO hm2_cvsoc_board \n");

    num_boards ++;
    return 0;

//    close ( fd );

}

static int hm2_cvsoc_munmap(void) {
  if (virtual_base)
    munmap((void *) virtual_base, HM2REG_IO_0_SPAN);
  if (uio_fd > -1)
      close (uio_fd);
//  hal_exit(comp_id);
  return(1);

}
int rtapi_app_main(void) {
//    hm2_cvsoc_t *me;
//    hm2_lowlevel_io_t *this;
    int r = 0;

    LL_PRINT("loading Mesa AnyIO HostMot2 socfpgs driver version " HM2_SOCFPGA_VERSION "\n");

    comp_id = hal_init(HM2_LLIO_NAME);
    if (comp_id < 0) return comp_id;

//    me = &hm2_cvsoc_board[0];
//    this = &me->llio;

//    r = uio_register_driver(&hm2_cvsoc_driver);
    r = hm2_cvsoc_mmap();
    
    if (r != 0) {
        LL_ERR("error registering UIO driver\n");
        hal_exit(comp_id);
        return r;
    }

    if(failed_errno) {
	// at least one card registration failed
	hal_exit(comp_id);
//	uio_unregister_driver(&hm2_cvsoc_driver);
//	hm2_cvsoc_munmap(&hm2_cvsoc_driver);
	r = hm2_cvsoc_munmap();
//	return failed_errno;
	return r;
    }

    if(num_boards == 0) {
	// no cards were detected
    LL_PRINT("error no supported cards detected\n");
	hal_exit(comp_id);
//	pci_unregister_driver(&hm2_cvsoc_driver);
	r = hm2_cvsoc_munmap();
   
//	return -ENODEV;
	return r;
    }

//    me->llio.program_fpga = hm2_test_program_fpga;
//    me->llio.reset = hm2_test_reset;


    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void) {
//    uio_unregister_driver(&hm2_cvsoc_driver);
    hm2_cvsoc_munmap();
    LL_PRINT("UIO driver unloaded\n");
    hal_exit(comp_id);
}

