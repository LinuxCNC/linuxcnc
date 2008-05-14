
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


#include <linux/pci.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "hal.h"

#include "hostmot2-lowlevel.h"
#include "hm2_5i20.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sebastian Kuzminsky");
MODULE_DESCRIPTION("Driver for HostMot2 on the 5i20 Mesa Anything I/O board");
MODULE_SUPPORTED_DEVICE("Mesa-AnythingIO-5i20");


// #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
//     #error "Kernel version not supported, 2.6.22 and later only" 
// #endif


static char *config[HM2_5I20_MAX_BOARDS];
static int num_config_strings = HM2_5I20_MAX_BOARDS;
module_param_array(config, charp, &num_config_strings, S_IRUGO);
MODULE_PARM_DESC(config, "config string for 5i20 boards (see hostmot2(9) manpage)");


static int comp_id;


// FIXME: should probably have a linked list of boards instead of an array
static hm2_5i20_t hm2_5i20_board[HM2_5I20_MAX_BOARDS];
static int num_boards = 0;


static struct pci_device_id hm2_5i20_pci_tbl[] = {
    {
        .vendor = 0x10b5,
        .device = 0x9030,
        .subvendor = 0x10b5,
        .subdevice = 0x3131,
    },

    {0,},
};

MODULE_DEVICE_TABLE(pci, hm2_5i20_pci_tbl);


static int hm2_5i20_read(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size) {
    hm2_5i20_t *this = self->private;
    memcpy(buffer, (this->base + addr), size);
    return 1;  // success
}

static int hm2_5i20_write(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size) {
    hm2_5i20_t *this = self->private;
    memcpy((this->base + addr), buffer, size);
    return 1;  // success
}


static int hm2_5i20_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    int r;
    hm2_5i20_t *board;
    hm2_lowlevel_io_t *this;


    if (num_boards >= HM2_5I20_MAX_BOARDS) {
        LL_WARN("skipping 5i20 at %s, this driver can only handle %d\n", pci_name(dev), HM2_5I20_MAX_BOARDS);
        return -EINVAL;
    }

    if (pci_enable_device(dev)) {
        LL_WARN("skipping 5i20 at %s, failed to enable PCI device\n", pci_name(dev));
        return -ENODEV;
    }

    board = &hm2_5i20_board[num_boards];

    //
    // region 5 is 64K mem (32 bit)
    //

    board->len = pci_resource_len(dev, 5);
    board->base = ioremap_nocache(pci_resource_start(dev, 5), board->len);
    if (board->base == NULL) {
        LL_WARN("skipping 5i20 at %s, could not map in FPGA address space\n", pci_name(dev));
        pci_disable_device(dev);
        return -ENODEV;
    }


    board->dev = dev;

    pci_set_drvdata(dev, board);

    snprintf(board->llio.name, HAL_NAME_LEN, "%s.%d", HM2_LLIO_NAME, num_boards);
    board->llio.comp_id = comp_id;
    board->llio.read = hm2_5i20_read;
    board->llio.write = hm2_5i20_write;
    board->llio.private = board;
    board->llio.num_ioport_connectors = 3;
    board->llio.ioport_connector_name[0] = "P2";
    board->llio.ioport_connector_name[1] = "P3";
    board->llio.ioport_connector_name[2] = "P4";
    this = &board->llio;

    r = hm2_register(&board->llio, config[num_boards]);
    if (r != 0) {
        LL_WARN("skipping 5i20 at %s, board fails HM2 registration\n", pci_name(dev));
        pci_disable_device(dev);
        board->dev = NULL;
        pci_set_drvdata(dev, NULL);
        return r;
    }

    THIS_INFO("found 5i20 board at %s with HostMot2 firmware\n", pci_name(dev));

    num_boards ++;
    return 0;
}


static void hm2_5i20_remove(struct pci_dev *dev) {
    int i;

    for (i = 0; i < num_boards; i++) {
        hm2_5i20_t *board = &hm2_5i20_board[i];
        hm2_lowlevel_io_t *this = &board->llio;

        if (board->dev == dev) {
            THIS_INFO("dropping 5i20 at %s\n", pci_name(dev));

            hm2_unregister(&(board->llio));

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


static struct pci_driver hm2_5i20_driver = {
	.name = HM2_LLIO_NAME,
	.id_table = hm2_5i20_pci_tbl,
	.probe = hm2_5i20_probe,
	.remove = hm2_5i20_remove,
};


int rtapi_app_main(void) {
    int r = 0;

    LL_INFO("loading HostMot2 Mesa 5i20 driver version " HM2_5I20_VERSION "\n");

    comp_id = hal_init(HM2_LLIO_NAME);
    if (comp_id < 0) return comp_id;

    r = pci_register_driver(&hm2_5i20_driver);
    if (r != 0) {
        LL_ERR("error registering PCI driver\n");
        hal_exit(comp_id);
        return -EINVAL;
    }

    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void) {
    pci_unregister_driver(&hm2_5i20_driver);
    LL_INFO("unloaded driver\n");
    hal_exit(comp_id);
}

