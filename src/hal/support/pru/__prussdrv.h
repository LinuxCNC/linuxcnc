/*
 * __prussdrv.h
 *
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*
 * ============================================================================
 * Copyright (c) Texas Instruments Inc 2010-12
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/unistd.h>

#define DISABLE_L3RAM_SUPPORT

#define	PAGE_SIZE                     4096

#define PRUSS_V1                    1
#define PRUSS_V2                    2

#define AM33XX_PRUSS_INTC_REV         0x4E82A900
#define AM18XX_PRUSS_INTC_REV         0x4E825900

#define PRUSS_MAX_IRAM_SIZE                  8192

#define AM33XX_PRUSS_IRAM_SIZE               8192
#define AM33XX_PRUSS_MMAP_SIZE               0x40000
#define AM33XX_DATARAM0_PHYS_BASE            0x4a300000
#define AM33XX_DATARAM1_PHYS_BASE            0x4a302000
#define AM33XX_INTC_PHYS_BASE                0x4a320000
#define AM33XX_PRU0CONTROL_PHYS_BASE         0x4a322000
#define AM33XX_PRU0DEBUG_PHYS_BASE           0x4a322400
#define AM33XX_PRU1CONTROL_PHYS_BASE         0x4a324000
#define AM33XX_PRU1DEBUG_PHYS_BASE           0x4a324400
#define AM33XX_PRU0IRAM_PHYS_BASE            0x4a334000
#define AM33XX_PRU1IRAM_PHYS_BASE            0x4a338000
#define AM33XX_PRUSS_SHAREDRAM_BASE          0x4a310000
#define	AM33XX_PRUSS_CFG_BASE                0x4a326000
#define	AM33XX_PRUSS_UART_BASE               0x4a328000
#define	AM33XX_PRUSS_IEP_BASE                0x4a32e000
#define	AM33XX_PRUSS_ECAP_BASE               0x4a330000
#define	AM33XX_PRUSS_MIIRT_BASE              0x4a332000
#define	AM33XX_PRUSS_MDIO_BASE               0x4a332400

#define AM18XX_PRUSS_IRAM_SIZE               4096
#define AM18XX_PRUSS_MMAP_SIZE               0x7C00
#define AM18XX_DATARAM0_PHYS_BASE            0x01C30000
#define AM18XX_DATARAM1_PHYS_BASE            0x01C32000
#define AM18XX_INTC_PHYS_BASE                0x01C34000
#define AM18XX_PRU0CONTROL_PHYS_BASE         0x01C37000
#define AM18XX_PRU0DEBUG_PHYS_BASE           0x01C37400
#define AM18XX_PRU1CONTROL_PHYS_BASE         0x01C37800
#define AM18XX_PRU1DEBUG_PHYS_BASE           0x01C37C00
#define AM18XX_PRU0IRAM_PHYS_BASE            0x01C38000
#define AM18XX_PRU1IRAM_PHYS_BASE            0x01C3C000

//PRUSS INTC register offsets
#define PRU_INTC_REVID_REG   0x000
#define PRU_INTC_CR_REG      0x004
#define PRU_INTC_HCR_REG     0x00C
#define PRU_INTC_GER_REG     0x010
#define PRU_INTC_GNLR_REG    0x01C
#define PRU_INTC_SISR_REG    0x020
#define PRU_INTC_SICR_REG    0x024
#define PRU_INTC_EISR_REG    0x028
#define PRU_INTC_EICR_REG    0x02C
#define PRU_INTC_HIEISR_REG  0x034
#define PRU_INTC_HIDISR_REG  0x038
#define PRU_INTC_GPIR_REG    0x080

#define PRU_INTC_SRSR1_REG   0x200
#define PRU_INTC_SRSR2_REG   0x204

#define PRU_INTC_SECR1_REG   0x280
#define PRU_INTC_SECR2_REG   0x284

#define PRU_INTC_ESR1_REG    0x300
#define PRU_INTC_ESR2_REG    0x304

#define PRU_INTC_ECR1_REG    0x380
#define PRU_INTC_ECR2_REG    0x384

#define PRU_INTC_CMR1_REG    0x400
#define PRU_INTC_CMR2_REG    0x404
#define PRU_INTC_CMR3_REG    0x408
#define PRU_INTC_CMR4_REG    0x40C
#define PRU_INTC_CMR5_REG    0x410
#define PRU_INTC_CMR6_REG    0x414
#define PRU_INTC_CMR7_REG    0x418
#define PRU_INTC_CMR8_REG    0x41C
#define PRU_INTC_CMR9_REG    0x420
#define PRU_INTC_CMR10_REG   0x424
#define PRU_INTC_CMR11_REG   0x428
#define PRU_INTC_CMR12_REG   0x42C
#define PRU_INTC_CMR13_REG   0x430
#define PRU_INTC_CMR14_REG   0x434
#define PRU_INTC_CMR15_REG   0x438
#define PRU_INTC_CMR16_REG   0x43C

#define PRU_INTC_HMR1_REG    0x800
#define PRU_INTC_HMR2_REG    0x804
#define PRU_INTC_HMR3_REG    0x808

#define PRU_INTC_SIPR1_REG   0xD00
#define PRU_INTC_SIPR2_REG   0xD04

#define PRU_INTC_SITR1_REG   0xD80
#define PRU_INTC_SITR2_REG   0xD84

#define PRU_INTC_HIER_REG    0x1500


#define MAX_HOSTS_SUPPORTED	10

//UIO driver expects user space to map PRUSS_UIO_MAP_OFFSET_XXX to
//access corresponding memory regions - region offset is N*PAGE_SIZE

#define PRUSS_UIO_MAP_OFFSET_PRUSS 0*PAGE_SIZE
#define PRUSS_UIO_DRV_PRUSS_BASE "/sys/class/uio/uio0/maps/map0/addr"
#define PRUSS_UIO_DRV_PRUSS_SIZE "/sys/class/uio/uio0/maps/map0/size"

#ifndef DISABLE_L3RAM_SUPPORT

#define PRUSS_UIO_MAP_OFFSET_L3RAM 1*PAGE_SIZE
#define PRUSS_UIO_DRV_L3RAM_BASE "/sys/class/uio/uio0/maps/map1/addr"
#define PRUSS_UIO_DRV_L3RAM_SIZE "/sys/class/uio/uio0/maps/map1/size"

#define PRUSS_UIO_MAP_OFFSET_EXTRAM 2*PAGE_SIZE
#define PRUSS_UIO_DRV_EXTRAM_BASE "/sys/class/uio/uio0/maps/map2/addr"
#define PRUSS_UIO_DRV_EXTRAM_SIZE "/sys/class/uio/uio0/maps/map2/size"

#else

#define PRUSS_UIO_MAP_OFFSET_EXTRAM 1*PAGE_SIZE
#define PRUSS_UIO_DRV_EXTRAM_BASE "/sys/class/uio/uio0/maps/map1/addr"
#define PRUSS_UIO_DRV_EXTRAM_SIZE "/sys/class/uio/uio0/maps/map1/size"


#endif


typedef struct pru_base {
    void *dataram_base;
    void *control_base;
    void *debug_base;
    void *iram_base;
    unsigned int dataram_phy_base;
    unsigned int control_phy_base;
    unsigned int debug_phy_base;
    unsigned int iram_phy_base;
} pru_base, *pru_base_ptr;

typedef struct __prussdrv {
    int version;
    int fd[NUM_PRU_HOSTIRQS];
    pthread_t irq_thread[NUM_PRU_HOSTIRQS];
    int mmap_fd;

    pru_base base[2];

    void *intc_base;
    void *l3ram_base;
    void *extram_base;
    void *pruss_sharedram_base;
    void *pruss_cfg_base;
    void *pruss_uart_base;
    void *pruss_iep_base;
    void *pruss_ecap_base;
    void *pruss_miirt_base;
    void *pruss_mdio_base;

    unsigned int intc_phy_base;
    unsigned int l3ram_phy_base;
    unsigned int extram_phy_base;
    unsigned int pruss_sharedram_phy_base;
    unsigned int pruss_cfg_phy_base;
    unsigned int pruss_uart_phy_base;
    unsigned int pruss_iep_phy_base;
    unsigned int pruss_ecap_phy_base;
    unsigned int pruss_miirt_phy_base;
    unsigned int pruss_mdio_phy_base;
    unsigned int pruss_phys_base;
    unsigned int pruss_map_size;
    unsigned int l3ram_phys_base;
    unsigned int l3ram_map_size;
    unsigned int extram_phys_base;
    unsigned int extram_map_size;
} tprussdrv;


extern int __pruss_detect_hw_version(unsigned int *pruss_io);
extern void __prussintc_set_cmr(unsigned int *pruintc_io, unsigned short sysevt,
				unsigned short channel);
extern void __prussintc_set_hmr(unsigned int *pruintc_io, unsigned short channel,
				unsigned short host);
