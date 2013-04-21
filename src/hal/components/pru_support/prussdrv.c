/*
 * prussdrv.c
 *
 * User space driver for PRUSS
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

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */

#include <prussdrv.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define PRUSS_UIO_PRAM_PATH_LEN 128
#define PRUSS_UIO_PARAM_VAL_LEN 20
#define HEXA_DECIMAL_BASE 16

static char *modname = "prussdrv"; // for error messages

tprussdrv prussdrv;

int __pruss_detect_hw_version(unsigned int *pruss_io)
{

    if (pruss_io[(AM18XX_INTC_PHYS_BASE - AM18XX_DATARAM0_PHYS_BASE) >> 2]
        == AM18XX_PRUSS_INTC_REV)
        return PRUSS_V1;
    else {
        if (pruss_io
            [(AM33XX_INTC_PHYS_BASE - AM33XX_DATARAM0_PHYS_BASE) >> 2] ==
            AM33XX_PRUSS_INTC_REV)
            return PRUSS_V2;
        else
            return -1;
    }
}

void __prussintc_set_cmr(unsigned int *pruintc_io, unsigned short sysevt,
                         unsigned short channel)
{
    pruintc_io[(PRU_INTC_CMR1_REG + (sysevt & ~(0x3))) >> 2] |=
        ((channel & 0xF) << ((sysevt & 0x3) << 3));

}


void __prussintc_set_hmr(unsigned int *pruintc_io, unsigned short channel,
                         unsigned short host)
{
    pruintc_io[(PRU_INTC_HMR1_REG + (channel & ~(0x3))) >> 2] =
        pruintc_io[(PRU_INTC_HMR1_REG +
                    (channel & ~(0x3))) >> 2] | (((host) & 0xF) <<
                                                 (((channel) & 0x3) << 3));

}

int __prussdrv_memmap_init(void)
{
    int i, fd;
    char hexstring[PRUSS_UIO_PARAM_VAL_LEN];

    if (prussdrv.mmap_fd == 0) {
        for (i = 0; i < NUM_PRU_HOSTIRQS; i++) {
            if (prussdrv.fd[i])
                break;
        }
        if (i == NUM_PRU_HOSTIRQS)
            return -1;
        else
            prussdrv.mmap_fd = prussdrv.fd[i];
    }
    fd = open(PRUSS_UIO_DRV_PRUSS_BASE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.pruss_phys_base =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;
    fd = open(PRUSS_UIO_DRV_PRUSS_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.pruss_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;

    prussdrv.base[0].dataram_base =
        mmap(0, prussdrv.pruss_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_PRUSS);
    prussdrv.version =
        __pruss_detect_hw_version(prussdrv.base[0].dataram_base);

    switch (prussdrv.version) {

    case PRUSS_V1:
	rtapi_print_msg(RTAPI_MSG_INFO, "%s: AM18XX detected\n", modname);

	prussdrv.intc_phy_base = AM18XX_INTC_PHYS_BASE;

	prussdrv.base[0].dataram_phy_base = AM18XX_DATARAM0_PHYS_BASE;
	prussdrv.base[1].dataram_phy_base = AM18XX_DATARAM1_PHYS_BASE;

	prussdrv.base[0].control_phy_base = AM18XX_PRU0CONTROL_PHYS_BASE;
	prussdrv.base[1].control_phy_base = AM18XX_PRU1CONTROL_PHYS_BASE;

	prussdrv.base[0].debug_phy_base = AM18XX_PRU0DEBUG_PHYS_BASE;
	prussdrv.base[1].debug_phy_base = AM18XX_PRU1DEBUG_PHYS_BASE;

	prussdrv.base[0].iram_phy_base = AM18XX_PRU0IRAM_PHYS_BASE;
	prussdrv.base[1].iram_phy_base = AM18XX_PRU1IRAM_PHYS_BASE;
        break;

    case PRUSS_V2:

	rtapi_print_msg(RTAPI_MSG_INFO, "%s: AM33XX detected\n", modname);

	prussdrv.intc_phy_base = AM33XX_INTC_PHYS_BASE;
	prussdrv.pruss_sharedram_phy_base = AM33XX_PRUSS_SHAREDRAM_BASE;
	prussdrv.pruss_cfg_phy_base = AM33XX_PRUSS_CFG_BASE;
	prussdrv.pruss_uart_phy_base = AM33XX_PRUSS_UART_BASE;
	prussdrv.pruss_iep_phy_base = AM33XX_PRUSS_IEP_BASE;
	prussdrv.pruss_ecap_phy_base = AM33XX_PRUSS_ECAP_BASE;
	prussdrv.pruss_miirt_phy_base = AM33XX_PRUSS_MIIRT_BASE;
	prussdrv.pruss_mdio_phy_base = AM33XX_PRUSS_MDIO_BASE;

	prussdrv.base[0].dataram_phy_base = AM33XX_DATARAM0_PHYS_BASE;
	prussdrv.base[1].dataram_phy_base = AM33XX_DATARAM1_PHYS_BASE;

	prussdrv.base[0].control_phy_base = AM33XX_PRU0CONTROL_PHYS_BASE;
	prussdrv.base[1].control_phy_base = AM33XX_PRU1CONTROL_PHYS_BASE;

	prussdrv.base[0].debug_phy_base = AM33XX_PRU0DEBUG_PHYS_BASE;
	prussdrv.base[1].debug_phy_base = AM33XX_PRU1DEBUG_PHYS_BASE;

	prussdrv.base[0].iram_phy_base = AM33XX_PRU0IRAM_PHYS_BASE;
	prussdrv.base[1].iram_phy_base = AM33XX_PRU1IRAM_PHYS_BASE;
        break;

    default:
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: __prussdrv_memmap_init: invalid pruss driver version %d\n",
			modname, prussdrv.version);
    }

    prussdrv.base[1].dataram_base =
        prussdrv.base[0].dataram_base + prussdrv.base[1].dataram_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.intc_base =
        prussdrv.base[0].dataram_base + prussdrv.intc_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[0].control_base =
        prussdrv.base[0].dataram_base + prussdrv.base[0].control_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[0].debug_base =
        prussdrv.base[0].dataram_base + prussdrv.base[0].debug_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[1].control_base =
        prussdrv.base[0].dataram_base + prussdrv.base[1].control_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[1].debug_base =
        prussdrv.base[0].dataram_base + prussdrv.base[1].debug_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[0].iram_base =
        prussdrv.base[0].dataram_base + prussdrv.base[0].iram_phy_base -
        prussdrv.base[0].dataram_phy_base;
    prussdrv.base[1].iram_base =
        prussdrv.base[0].dataram_base + prussdrv.base[1].iram_phy_base -
        prussdrv.base[0].dataram_phy_base;

    if (prussdrv.version == PRUSS_V2) {
        prussdrv.pruss_sharedram_base =
            prussdrv.base[0].dataram_base +
            prussdrv.pruss_sharedram_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_cfg_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_cfg_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_uart_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_uart_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_iep_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_iep_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_ecap_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_ecap_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_miirt_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_miirt_phy_base -
            prussdrv.base[0].dataram_phy_base;
        prussdrv.pruss_mdio_base =
            prussdrv.base[0].dataram_base + prussdrv.pruss_mdio_phy_base -
            prussdrv.base[0].dataram_phy_base;
    }

#ifndef DISABLE_L3RAM_SUPPORT
    fd = open(PRUSS_UIO_DRV_L3RAM_BASE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.l3ram_phys_base =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;

    fd = open(PRUSS_UIO_DRV_L3RAM_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.l3ram_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;

    prussdrv.l3ram_base =
        mmap(0, prussdrv.l3ram_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_L3RAM);
#endif

    fd = open(PRUSS_UIO_DRV_EXTRAM_BASE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.extram_phys_base =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;

    fd = open(PRUSS_UIO_DRV_EXTRAM_SIZE, O_RDONLY);
    if (fd >= 0) {
        read(fd, hexstring, PRUSS_UIO_PARAM_VAL_LEN);
        prussdrv.extram_map_size =
            strtoul(hexstring, NULL, HEXA_DECIMAL_BASE);
        close(fd);
    } else
        return -1;

    prussdrv.extram_base =
        mmap(0, prussdrv.extram_map_size, PROT_READ | PROT_WRITE,
             MAP_SHARED, prussdrv.mmap_fd, PRUSS_UIO_MAP_OFFSET_EXTRAM);

    return 0;

}

int prussdrv_init(void)
{
    memset(&prussdrv, 0, sizeof(prussdrv));
    return 0;
}

int prussdrv_open(unsigned int pru_evtout_num)
{
    char name[PRUSS_UIO_PRAM_PATH_LEN];
    if (!prussdrv.fd[pru_evtout_num]) {
        sprintf(name, "/dev/uio%d", pru_evtout_num);
        prussdrv.fd[pru_evtout_num] = open(name, O_RDWR | O_SYNC);
        return __prussdrv_memmap_init();
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_open(%d) failed\n",
			modname, pru_evtout_num);
        return -1;
    }
}

int prussdrv_open_fd(int fd, unsigned int pru_evtout_num)
{
    struct stat st;

    if (fstat(fd, &st)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_open_fd: fstat(%d) failed: %d - %s\n",
			modname, fd, errno,strerror(errno));
        return -1;
    }
    if (!S_ISCHR(st.st_mode)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_open_fd: fd %d: not a char device\n",
			modname, fd);
        return -1;
    }
    if (!prussdrv.fd[pru_evtout_num]) {
	prussdrv.fd[pru_evtout_num] = fd;
        return __prussdrv_memmap_init();
    } else {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_open_fd(%d,%d): fd already open\n",
			modname, fd, pru_evtout_num);
        return -1;
    }
}

preg prussdrv_pru_ctrl(unsigned int prunum)
{
    return *((preg_ptr) prussdrv.base[PN(prunum)].control_base);
}

preg prussdrv_pru_ctrl_clearbits(unsigned int prunum, preg bits)
{
    preg_ptr pructrl = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    preg previous = *pructrl;
    *pructrl &= ~bits;
    return previous;
}

preg prussdrv_pru_ctrl_setbits(unsigned int prunum, preg bits)
{
    preg_ptr pructrl = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    preg previous = *pructrl;
    *pructrl |= bits;
    return previous;
}

int prussdrv_pru_running(unsigned int prunum)
{
    // preg_ptr prucontrolregs = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    // return *(prucontrolregs+1) &  (1 << 15); //mah: pretty sure this was wrong
    return prussdrv_pru_ctrl(prunum) &  (1 << 15);
}

int prussdrv_pru_reset(unsigned int prunum)
{
    preg_ptr pructrl = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    preg previous = *pructrl;
    *pructrl = 0;
    return previous;
}

preg prussdrv_pru_enable(unsigned int prunum)
{
    preg_ptr pructrl = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    preg previous = *pructrl;
    *pructrl = 2;
    return previous;
}

preg prussdrv_pru_disable(unsigned int prunum)
{
    preg_ptr pructrl = (preg_ptr) prussdrv.base[PN(prunum)].control_base;
    preg previous = *pructrl;
    *pructrl = 1;
    return previous;
}

int prussdrv_pru_write_memory(unsigned int pru_ram_id,
                              unsigned int wordoffset,
                              unsigned int *memarea,
                              unsigned int bytelength)
{
    unsigned int *pruramarea, i, wordlength;
    switch (pru_ram_id) {
    case PRUSS0_PRU0_IRAM:
        pruramarea = (unsigned int *) prussdrv.base[0].iram_base;
        break;
    case PRUSS0_PRU1_IRAM:
        pruramarea = (unsigned int *) prussdrv.base[1].iram_base;
        break;
    case PRUSS0_PRU0_DATARAM:
        pruramarea = (unsigned int *) prussdrv.base[0].dataram_base;
        break;
    case PRUSS0_PRU1_DATARAM:
        pruramarea = (unsigned int *) prussdrv.base[1].dataram_base;
        break;
    case PRUSS0_SHARED_DATARAM:
        if (prussdrv.version != PRUSS_V2)
            return -1;
        pruramarea = (unsigned int *) prussdrv.pruss_sharedram_base;
        break;
    default:
        return -1;
    }

    wordlength = (bytelength + 3) >> 2; //Adjust length as multiple of 4 bytes
    for (i = 0; i < wordlength; i++) {
        *(pruramarea + i + wordoffset) = *(memarea + i);
    }
    return wordlength;
}


int prussdrv_pruintc_init(tpruss_intc_initdata * prussintc_init_data)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    unsigned int i, mask1, mask2;

    pruintc_io[PRU_INTC_SIPR1_REG >> 2] = 0xFFFFFFFF;
    pruintc_io[PRU_INTC_SIPR2_REG >> 2] = 0xFFFFFFFF;

    for (i = 0; i < (NUM_PRU_SYS_EVTS + 3) >> 2; i++)
        pruintc_io[(PRU_INTC_CMR1_REG >> 2) + i] = 0;
    for (i = 0;
         ((prussintc_init_data->sysevt_to_channel_map[i].sysevt != -1)
          && (prussintc_init_data->sysevt_to_channel_map[i].channel !=
              -1)); i++) {
        __prussintc_set_cmr(pruintc_io,
                            prussintc_init_data->sysevt_to_channel_map[i].
                            sysevt,
                            prussintc_init_data->sysevt_to_channel_map[i].
                            channel);
    }
    for (i = 0; i < (NUM_PRU_HOSTS + 3) >> 2; i++)
        pruintc_io[(PRU_INTC_HMR1_REG >> 2) + i] = 0;
    for (i = 0;
         ((prussintc_init_data->channel_to_host_map[i].channel != -1)
          && (prussintc_init_data->channel_to_host_map[i].host != -1));
         i++) {

        __prussintc_set_hmr(pruintc_io,
                            prussintc_init_data->channel_to_host_map[i].
                            channel,
                            prussintc_init_data->channel_to_host_map[i].
                            host);
    }

    pruintc_io[PRU_INTC_SITR1_REG >> 2] = 0x0;
    pruintc_io[PRU_INTC_SITR2_REG >> 2] = 0x0;


    mask1 = mask2 = 0;
    for (i = 0; prussintc_init_data->sysevts_enabled[i] != 255; i++) {
        if (prussintc_init_data->sysevts_enabled[i] < 32) {
            mask1 =
                mask1 + (1 << (prussintc_init_data->sysevts_enabled[i]));
        } else if (prussintc_init_data->sysevts_enabled[i] < 64) {
            mask2 =
                mask2 +
                (1 << (prussintc_init_data->sysevts_enabled[i] - 32));
        } else {
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_pruintc_init(): Error: SYS_EVT%d out of range\n",
			    modname, prussintc_init_data->sysevts_enabled[i]);
            return -1;
        }
    }
    pruintc_io[PRU_INTC_ESR1_REG >> 2] = mask1;
    pruintc_io[PRU_INTC_SECR1_REG >> 2] = mask1;
    pruintc_io[PRU_INTC_ESR2_REG >> 2] = mask2;
    pruintc_io[PRU_INTC_SECR2_REG >> 2] = mask2;

    for (i = 0; i < MAX_HOSTS_SUPPORTED; i++)
        if (prussintc_init_data->host_enable_bitmask & (1 << i)) {
            pruintc_io[PRU_INTC_HIEISR_REG >> 2] = i;
        }

    pruintc_io[PRU_INTC_GER_REG >> 2] = 0x1;

    return 0;
}


int prussdrv_pru_send_event(unsigned int eventnum)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    if (eventnum < 32)
        pruintc_io[PRU_INTC_SRSR1_REG >> 2] = 1 << eventnum;
    else
        pruintc_io[PRU_INTC_SRSR2_REG >> 2] = 1 << (eventnum - 32);
    return 0;
}

int prussdrv_pru_wait_event(unsigned int pru_evtout_num, int *event_count)
{
    int retval;
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    retval = read(prussdrv.fd[pru_evtout_num], event_count, sizeof(int));
    if (retval < 0)
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_pru_wait_event: read returned %d - %s\n",
			modname, retval, strerror(errno));
    pruintc_io[PRU_INTC_HIEISR_REG >> 2] = pru_evtout_num+2;
    return retval;
}

int prussdrv_pru_clear_event(unsigned int eventnum)
{
    unsigned int *pruintc_io = (unsigned int *) prussdrv.intc_base;
    if (eventnum < 32)
        pruintc_io[PRU_INTC_SECR1_REG >> 2] = 1 << eventnum;
    else
        pruintc_io[PRU_INTC_SECR2_REG >> 2] = 1 << (eventnum - 32);
    return 0;
}

int prussdrv_pru_send_wait_clear_event(unsigned int send_eventnum,
                                       unsigned int pru_evtout_num,
                                       unsigned int ack_eventnum)
{
    int event_count;
    prussdrv_pru_send_event(send_eventnum);
    prussdrv_pru_wait_event(pru_evtout_num, &event_count);
    prussdrv_pru_clear_event(ack_eventnum);
    return 0;
}


int prussdrv_map_l3mem(void **address)
{
    *address = prussdrv.l3ram_base;
    return 0;
}



int prussdrv_map_extmem(void **address)
{
    *address = prussdrv.extram_base;
    return 0;
}


int prussdrv_map_prumem(unsigned int pru_ram_id, void **address)
{
    switch (pru_ram_id) {
    case PRUSS0_PRU0_DATARAM:
        *address = prussdrv.base[0].dataram_base;
        break;
    case PRUSS0_PRU1_DATARAM:
        *address = prussdrv.base[1].dataram_base;
        break;
    case PRUSS0_SHARED_DATARAM:
        if (prussdrv.version != PRUSS_V2)
            return -1;
        *address = prussdrv.pruss_sharedram_base;
        break;
    default:
        *address = 0;
        return -1;
    }
    return 0;
}

int prussdrv_map_peripheral_io(unsigned int per_id, void **address)
{
    if (prussdrv.version != PRUSS_V2)
        return -1;

    switch (per_id) {
    case PRUSS0_CFG:
        *address = prussdrv.pruss_cfg_base;
        break;
    case PRUSS0_UART:
        *address = prussdrv.pruss_uart_base;
        break;
    case PRUSS0_IEP:
        *address = prussdrv.pruss_iep_base;
        break;
    case PRUSS0_ECAP:
        *address = prussdrv.pruss_ecap_base;
        break;
    case PRUSS0_MII_RT:
        *address = prussdrv.pruss_miirt_base;
        break;
    case PRUSS0_MDIO:
        *address = prussdrv.pruss_mdio_base;
        break;
    default:
        *address = 0;
        return -1;
    }
    return 0;
}

unsigned int prussdrv_get_phys_addr(void *address)
{
    unsigned int retaddr = 0;
    if ((address >= prussdrv.base[0].dataram_base)
        && (address <
            prussdrv.base[0].dataram_base + prussdrv.pruss_map_size)) {
        retaddr =
            ((unsigned int) (address - prussdrv.base[0].dataram_base) +
             prussdrv.base[0].dataram_phy_base);
    } else if ((address >= prussdrv.l3ram_base)
               && (address <
                   prussdrv.l3ram_base + prussdrv.l3ram_map_size)) {
        retaddr =
            ((unsigned int) (address - prussdrv.l3ram_base) +
             prussdrv.l3ram_phys_base);
    } else if ((address >= prussdrv.extram_base)
               && (address <
                   prussdrv.extram_base + prussdrv.extram_map_size)) {
        retaddr =
            ((unsigned int) (address - prussdrv.extram_base) +
             prussdrv.extram_phys_base);
    }
    return retaddr;

}

void *prussdrv_get_virt_addr(unsigned int phyaddr)
{
    void *address = 0;
    if ((phyaddr >= prussdrv.base[0].dataram_phy_base)
        && (phyaddr <
            prussdrv.base[0].dataram_phy_base + prussdrv.pruss_map_size)) {
        address =
            (void *) ((unsigned int) prussdrv.base[0].dataram_base +
                      (phyaddr - prussdrv.base[0].dataram_phy_base));
    } else if ((phyaddr >= prussdrv.l3ram_phys_base)
               && (phyaddr <
                   prussdrv.l3ram_phys_base + prussdrv.l3ram_map_size)) {
        address =
            (void *) ((unsigned int) prussdrv.l3ram_base +
                      (phyaddr - prussdrv.l3ram_phys_base));
    } else if ((phyaddr >= prussdrv.extram_phys_base)
               && (phyaddr <
                   prussdrv.extram_phys_base + prussdrv.extram_map_size)) {
        address =
            (void *) ((unsigned int) prussdrv.extram_base +
                      (phyaddr - prussdrv.extram_phys_base));
    }
    return address;

}


int prussdrv_exit()
{
    int i;
    munmap(prussdrv.base[0].dataram_base, prussdrv.pruss_map_size);
    munmap(prussdrv.l3ram_base, prussdrv.l3ram_map_size);
    munmap(prussdrv.extram_base, prussdrv.extram_map_size);
    for (i = 0; i < NUM_PRU_HOSTIRQS; i++) {
        if (prussdrv.fd[i])
            close(prussdrv.fd[i]);
        if (prussdrv.irq_thread[i])
            pthread_join(prussdrv.irq_thread[i], NULL);
    }
    return 0;
}

int prussdrv_exec_program(int prunum, char *filename, int disabled)
{
    FILE *fPtr;
    unsigned char fileDataArray[PRUSS_MAX_IRAM_SIZE];
    int fileSize = 0, got;
    unsigned int pru_ram_id;

    if (prunum == 0)
        pru_ram_id = PRUSS0_PRU0_IRAM;
    else if (prunum == 1)
        pru_ram_id = PRUSS0_PRU1_IRAM;
    else
        return -1;

    // Open an File from the hard drive
    fPtr = fopen(filename, "rb");
    if (fPtr == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_exec_program(%d,%s): cant open file\n",
			modname, prunum, filename);
        return -1;
    }
    // Read file size
    fseek(fPtr, 0, SEEK_END);
    fileSize = ftell(fPtr);

    if (fileSize == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_exec_program(%d,%s): file size is zero\n",
			modname, prunum, filename);
        fclose(fPtr);
        return -1;
    }

    fseek(fPtr, 0, SEEK_SET);
    got = fread((unsigned char *) fileDataArray, 1, fileSize, fPtr);

    if (fileSize != got)  {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: prussdrv_exec_program(%d,%s): file size mismatch - %d/%d\n",
			modname, prunum, filename, fileSize, got);
	fclose(fPtr);
	return -1;
    }
    fclose(fPtr);

    // Make sure PRU sub system is first disabled/reset
    prussdrv_pru_disable(prunum);
    prussdrv_pru_write_memory(pru_ram_id, 0,
                              (unsigned int *) fileDataArray, fileSize);
    if (!disabled)
	prussdrv_pru_enable(prunum);

    return 0;
}

int prussdrv_exec_code(int prunum, const unsigned int *code, int codelen, int disabled)
{
    unsigned int pru_ram_id;

    if (prunum == 0)
        pru_ram_id = PRUSS0_PRU0_IRAM;
    else if (prunum == 1)
        pru_ram_id = PRUSS0_PRU1_IRAM;
    else
        return -1;

    // Make sure PRU sub system is first disabled/reset
    prussdrv_pru_disable(prunum);
    prussdrv_pru_write_memory(pru_ram_id, 0, (unsigned int *) code, codelen);

    if (!disabled)
	prussdrv_pru_enable(prunum);

    return 0;
}

int prussdrv_start_irqthread(unsigned int pru_evtout_num, int priority,
                             prussdrv_function_handler irqhandler, void *arg)
{
    pthread_attr_t pthread_attr;
    struct sched_param sched_param;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);
    if (priority != 0) {
        pthread_attr_setinheritsched(&pthread_attr,
                                     PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&pthread_attr, SCHED_FIFO);
        sched_param.sched_priority = priority;
        pthread_attr_setschedparam(&pthread_attr, &sched_param);
    }

    pthread_create(&prussdrv.irq_thread[pru_evtout_num], &pthread_attr,
                   irqhandler, arg);

    pthread_attr_destroy(&pthread_attr);

    return 0;

}
