#ifndef GM_H_
#define GM_H_

#include <hal.h>


#define PLX_VENDOR_ID           0x10B5 //PLX Vendor ID
#define GM_DEVICE_ID            0x6ACC //GM 6 Axis Control Card,   www.pcidatabase.com
#define GM_SUBDEVICE_ID_1         0x3131
#define GM_SUBDEVICE_ID_2         0x6ACC

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
	volatile rtapi_u32			serialModulesDataOut[16][8]; // 0000 0000
	volatile rtapi_u32			serialModulesDataIn[16][8];  // 1000 0000

	volatile rtapi_u32			moduleId[8];		//addr 0 	0000 000

	volatile rtapi_u32			card_status_reg;	//addr 1	0001 000        //  ... Estop_2 | Estop_1 | Pwr_fault | Bus_err | Wdt_err  //Card status read resets wdt
	volatile rtapi_u32			cardID;				//	0001 001
	volatile rtapi_u32			card_control_reg; 		//	0001 010	//  Wdt_period(16 bit)[us] | ... | EstopEn_2 | EstopEn_1 | power_enable | card_enable
	volatile rtapi_u32			reserved_0;			//	0001 011
	volatile rtapi_u32			gpio;				//	0001 100
	volatile rtapi_u32			gpioDir;			//	0001 101
	volatile rtapi_u32			StepGen_status;			//	0001 110
	volatile rtapi_u32			PCI_clk_counter;		//	0001 111

	volatile rtapi_u32			ENC_control_reg;	//addr 2	0010 000
	volatile rtapi_u32			CAN_status_reg;
	volatile rtapi_u32			CAN_control_reg;
	volatile rtapi_u32			DAC_0; //DAC AXIS 1-0
	volatile rtapi_u32			DAC_1; //DAC AXIS 3-2
	volatile rtapi_u32			DAC_2; //DAC AXIS 5-4
	volatile rtapi_u32			reserved_1[2];

	volatile rtapi_u32			CAN_RX_buffer[4];	//addr 3     	0011 000
	volatile rtapi_u32			CAN_TX_buffer[4];
	volatile rtapi_u32			reserved_2[8];		//addr 4       	0100 000

	volatile rtapi_u32			reserved_3[8];		//addr 5       	0101 000

	volatile rtapi_u32			reserved_4[8];		//addr 6       	0110 000

	volatile rtapi_u32			reserved_5[8];		//addr 7       	0111 000

	volatile rtapi_s32			ENC_counter[6];		//addr 8	1000 000
	volatile rtapi_u32			reserved_6[2];
	volatile rtapi_s32			ENC_period[6];		//addr 9	1001 000
	volatile rtapi_u32			reserved_7[2];
	volatile rtapi_s32			ENC_index_latch[6]; 	//addr 10	1010 000
	volatile rtapi_u32			reserved_8[2];
	volatile rtapi_s32			reserved_9[8]; 		//addr 11	1011 000

	volatile rtapi_s32			StepGen_steprate[6];	//addr 12	1100 000
	volatile rtapi_u32			reserved_10[2];
	volatile rtapi_u32			StepGen_fb[6];		//addr 13	1101 000
	volatile rtapi_u32			reserved_11[2];
	volatile rtapi_u32			StepGen_time_params[6];	//addr 14	1110 000
	volatile rtapi_u32			reserved_12[2];
	volatile rtapi_u32			reserved_16[8];		//addr 15	1111 000

} volatile card;



#endif

