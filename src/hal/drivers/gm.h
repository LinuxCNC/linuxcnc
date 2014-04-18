#ifndef GM_H_
#define GM_H_

#include "hal.h"


#define PLX_VENDOR_ID		0x10B5 //PLX Vendor ID
#define GM_DEVICE_ID		0x6ACC //GM 6 Axis Control Card,   www.pcidatabase.com

#define MAX_GM_DEVICES		3

#define RS485MODUL_ID_8INPUT 0x1
#define RS485MODUL_ID_8OUTPUT 0x2
#define RS485MODUL_ID_DACADC 0x3
#define RS485MODUL_ID_TEACHPAD 0x04

#define IDmask_card 	0xF0000000
#define cardVersion1 	0x10000000
#define IDmask_can 	0x000F0000
#define canVersion1 	0x00010000
#define IDmask_rs485 	0x0000F000
#define rs485Version1 	0x00001000
#define IDmask_dac 	0x00000F00
#define dacVersion1 	0x00000100
#define dacVersion2 	0x00000200
#define IDmask_stepgen 	0x000000F0
#define stepgenVersion1 0x00000010
#define IDmask_encoder 	0x0000000F
#define encoderVersion1 0x00000001
#define notPresented	0x00000000

typedef struct {
	hal_u32_t			serialModulesDataOut[16][8]; // 0000 0000
	hal_u32_t			serialModulesDataIn[16][8];  // 1000 0000
	
	hal_u32_t			moduleId[8];		//addr 0 	0000 000 
	
	hal_u32_t			card_status_reg;	//addr 1	0001 000        //  ... Estop_2 | Estop_1 | Pwr_fault | Bus_err | Wdt_err  //Card status read resets wdt
	hal_u32_t			cardID;				//	0001 001
	hal_u32_t			card_control_reg; 		//	0001 010	//  Wdt_period(16 bit)[us] | ... | EstopEn_2 | EstopEn_1 | power_enable | card_enable
	hal_u32_t			reserved_0;			//	0001 011
	hal_u32_t			gpio;				//	0001 100
	hal_u32_t			gpioDir;			//	0001 101
	hal_u32_t			StepGen_status;			//	0001 110
	hal_u32_t			PCI_clk_counter;		//	0001 111
	
	hal_u32_t			ENC_control_reg;	//addr 2	0010 000
	hal_u32_t			CAN_status_reg;
	hal_u32_t			CAN_control_reg;
	hal_u32_t			DAC_0; //DAC AXIS 1-0
	hal_u32_t			DAC_1; //DAC AXIS 3-2
	hal_u32_t			DAC_2; //DAC AXIS 5-4
	hal_u32_t			reserved_1[2];
	
	hal_u32_t			CAN_RX_buffer[4];	//addr 3     	0011 000
	hal_u32_t			CAN_TX_buffer[4];	
	hal_u32_t			reserved_2[8];		//addr 4       	0100 000
		
	hal_u32_t			reserved_3[8];		//addr 5       	0101 000
		
	hal_u32_t			reserved_4[8];		//addr 6       	0110 000
	
	hal_u32_t			reserved_5[8];		//addr 7       	0111 000
	
	hal_s32_t			ENC_counter[6];		//addr 8	1000 000
	hal_u32_t			reserved_6[2];
	hal_s32_t			ENC_period[6];		//addr 9	1001 000
	hal_u32_t			reserved_7[2];
	hal_s32_t			ENC_index_latch[6]; 	//addr 10	1010 000
	hal_u32_t			reserved_8[2];
	hal_s32_t			reserved_9[8]; 		//addr 11	1011 000

	hal_s32_t			StepGen_steprate[6];	//addr 12	1100 000
	hal_u32_t			reserved_10[2];
	hal_u32_t			StepGen_fb[6];		//addr 13	1101 000
	hal_u32_t			reserved_11[2];			
	hal_u32_t			StepGen_time_params[6];	//addr 14	1110 000
	hal_u32_t			reserved_12[2];		
	hal_u32_t			reserved_16[8];		//addr 15	1111 000

} volatile card;



#endif
