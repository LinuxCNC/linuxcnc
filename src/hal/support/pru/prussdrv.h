/*
 * prussdrv.h
 *
 * Describes PRUSS userspace driver for Industrial Communications
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
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
 * Copyright (c) Texas Instruments Inc 2010-11
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
*/

#ifndef _PRUSSDRV_H
#define _PRUSSDRV_H

#include <sys/types.h>
#include <pthread.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define NUM_PRU_HOSTIRQS        8
#define NUM_PRU_HOSTS          10
#define NUM_PRU_CHANNELS       10
#define NUM_PRU_SYS_EVTS       64

#define PRUSS0_PRU0_DATARAM     0
#define PRUSS0_PRU1_DATARAM     1
#define PRUSS0_PRU0_IRAM        2
#define PRUSS0_PRU1_IRAM        3

//Available in AM33xx series - begin
#define PRUSS0_SHARED_DATARAM   4
#define	PRUSS0_CFG              5
#define	PRUSS0_UART             6
#define	PRUSS0_IEP              7
#define	PRUSS0_ECAP             8
#define	PRUSS0_MII_RT           9
#define	PRUSS0_MDIO            10
//Available in AM33xx series - end

#define PRU_EVTOUT_0            0
#define PRU_EVTOUT_1            1
#define PRU_EVTOUT_2            2
#define PRU_EVTOUT_3            3
#define PRU_EVTOUT_4            4
#define PRU_EVTOUT_5            5
#define PRU_EVTOUT_6            6
#define PRU_EVTOUT_7            7

#define PN(p) ((p) & 1) // every non-zero PRU number considered PRU1

    typedef volatile unsigned int preg, *preg_ptr;

    typedef void *(*prussdrv_function_handler) (void *);

    typedef struct __sysevt_to_channel_map {
        short sysevt;
        short channel;
    } tsysevt_to_channel_map;

    typedef struct __channel_to_host_map {
        short channel;
        short host;
    } tchannel_to_host_map;

    typedef struct __pruss_intc_initdata {
        //Enabled SYSEVTs - Range:0..63
        //{-1} indicates end of list
        char sysevts_enabled[NUM_PRU_SYS_EVTS];

        //SysEvt to Channel map. SYSEVTs - Range:0..63 Channels -Range: 0..9
        //{-1, -1} indicates end of list
        tsysevt_to_channel_map sysevt_to_channel_map[NUM_PRU_SYS_EVTS];

        //Channel to Host map.Channels -Range: 0..9  HOSTs - Range:0..9
        //{-1, -1} indicates end of list
        tchannel_to_host_map channel_to_host_map[NUM_PRU_CHANNELS];

        //10-bit mask - Enable Host0-Host9 {Host0/1:PRU0/1, Host2..9 : PRUEVT_OUT0..7)
        unsigned int host_enable_bitmask;
    } tpruss_intc_initdata;

    int prussdrv_init(void);


    int prussdrv_open(unsigned int pru_evtout_num);

    // same thing, but /dev/uio%evtout already opened
    int prussdrv_open_fd(int fd, unsigned int pru_evtout_num);


    preg prussdrv_pru_ctrl(unsigned int prunum);
    preg prussdrv_pru_ctrl_clearbits(unsigned int prunum, preg bits);
    preg prussdrv_pru_ctrl_setbits(unsigned int prunum, preg bits);

    int prussdrv_pru_reset(unsigned int prunum);

    int prussdrv_pru_running(unsigned int prunum);

    preg prussdrv_pru_disable(unsigned int prunum);

    preg prussdrv_pru_enable(unsigned int prunum);

    int prussdrv_pru_write_memory(unsigned int pru_ram_id,
                                  unsigned int wordoffset,
                                  unsigned int *memarea,
                                  unsigned int bytelength);

    int prussdrv_pruintc_init(tpruss_intc_initdata * prussintc_init_data);

    int prussdrv_map_l3mem(void **address);

    int prussdrv_map_extmem(void **address);

    int prussdrv_map_prumem(unsigned int pru_ram_id, void **address);

    int prussdrv_map_peripheral_io(unsigned int per_id, void **address);

    unsigned int prussdrv_get_phys_addr(void *address);

    void *prussdrv_get_virt_addr(unsigned int phyaddr);

    int prussdrv_pru_wait_event(unsigned int pru_evtout_num, int *event_count);

    int prussdrv_pru_send_event(unsigned int eventnum);

    int prussdrv_pru_clear_event(unsigned int eventnum);

    int prussdrv_pru_send_wait_clear_event(unsigned int send_eventnum,
                                           unsigned int pru_evtout_num,
                                           unsigned int ack_eventnum);

    int prussdrv_exit(void);

    int prussdrv_exec_program(int prunum, char *filename, int disabled);

    int prussdrv_exec_code(int prunum, const unsigned int *code, int codelen, int disabled);

    int prussdrv_start_irqthread(unsigned int pru_evtout_num, int priority,
                                 prussdrv_function_handler irqhandler, void *arg);

#include "__prussdrv.h"

    extern tprussdrv prussdrv;


#if defined (__cplusplus)
}
#endif
#endif
