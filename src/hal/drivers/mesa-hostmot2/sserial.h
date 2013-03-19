//
//    Copyright (C) 2012 Andy Pugh
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

#ifndef __SSERIAL_H
#define __SSERIAL_H

#define HM2_SSERIAL_TYPE_8I20               0x30324938  // '8i20' as 4 ascii
#define HM2_SSERIAL_TYPE_7I64               0x34364937  // More to be added later.
#define HM2_SSERIAL_MAX_STRING_LENGTH       48

//Commands etc
#define LBPNONVOL_flag      0xCC000000
#define LBPWRITE            0x20000000
#define LBPNONVOLCLEAR      0x0
#define LBPNONVOLEEPROM     0x01             
#define LBPNONVOLFLASH      0x02

#define READ_LOCAL_CMD      0x2000
#define WRITE_LOCAL_CMD     0xA000

#define READ_REM_BYTE_CMD     0x44000000
#define READ_REM_WORD_CMD     0x45000000
#define READ_REM_LONG_CMD     0x46000000
#define READ_REM_DOUBLE_CMD   0x47000000
#define READ_COOKIE_CMD       0xDF000000
#define WRITE_REM_BYTE_CMD    0x64000000
#define WRITE_REM_WORD_CMD    0x65000000
#define WRITE_REM_LONG_CMD    0x66000000
#define WRITE_REM_DOUBLE_CMD  0x67000000
#define FLASHERASE_CMD        0xFE
#define FLASHWRITE_CMD        0xFD

#define SSLBPMAJORREVISIONLOC   2
#define SSLBPMINORREVISIONLOC   3
#define SSLBPCHANNELSTARTLOC    4
#define SSLBPCHANNELSTRIDELOC   5
#define LBPFLASHOFFSETLOC       0x8000
#define LBPFLASHERASESIZELOC    0x8004
#define LBPFLASHWRITESIZELOC    0x8005
#define LBPFLASHCOMMITLOC       0x8007

#define LBPCOOKIE               0x5A

#define HM2WRITE(a,b)  hm2->llio->write(hm2->llio, a, &b, sizeof(u32))
#define HM2READ(a,b)  hm2->llio->read(hm2->llio, a, &b, sizeof(u32)) 

typedef struct {
    unsigned char RecordType;
    unsigned char DataLength;
    unsigned char DataType;
    unsigned char DataDir;
    float ParmMin;
    float ParmMax;
    short ParmAddr;
    char UnitString[HM2_SSERIAL_MAX_STRING_LENGTH+1];
    char NameString[HM2_SSERIAL_MAX_STRING_LENGTH+1];
}hm2_sserial_data_t;

static const hm2_sserial_data_t hm2_8i20_params[] = {
    {0xA0,0x10,0x02,0x80,0.0,1.0,0,"degrees","angle"},
    {0xA0,0x10,0x03,0x80,-10.0,10.0,0,"amps","current"},
    {0xA0,0x10,0x02,0x00,0,65535,0,"Deg C","card-temp"},
    {0xA0,0x10,0x02,0x00,0,655.35,0,"volts","bus-voltage"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.current-lim"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.brake-on"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.brake-old"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.bus-underv"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.wd-reset"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.sw-reset"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.ext-reset"},
    {0xA0,0x01,0x00,0x00,0,0,0,"pad","pad"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.no-enable"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","status.pid-on"},
    {0xA0,0x06,0x00,0x00,0,0,0,"pad","pad"},
    
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.watchdog"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.no-enable"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.overtemp"},
    {0xA0,0x01,0x00,0x00,0,0,0,"pad","pad"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.overcurrent"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.U-current"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.V-current"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.W-current"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.bus-underv"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.bus-high"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.bus-overv"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.module"},
    {0xA0,0x01,0x00,0x00,0,0,0,"pad","pad"},
    {0xA0,0x01,0x00,0x00,0,0,0,"pad","pad"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.overrun"},
    {0xA0,0x01,0x07,0x00,0,0,0,"none","fault.framingr"}
};
static const hm2_sserial_data_t hm2_8i20_globals[] = { 
    {0xA0,0x10,0x02,0x00,0,0,2164,"none","swrevision"},
    {0xA0,0x20,0x02,0x00,0,0,2344,"none","unitnumber"},
    {0xA0,0x10,0x04,0x00,0,32.768,2,"amps","nvmaxcurrent"},
    {0xA0,0x10,0x04,0x00,0,0,36,"volts","nvbusundervmax"},
    {0xA0,0x10,0x04,0x00,0,0,34,"volts","nvbusundervmin"},
    {0xA0,0x10,0x04,0x00,0,0,38,"volts","nvbusoverv"},
    {0xA0,0x10,0x04,0x00,0,0,40,"volts","nvbrakeonv"},
    {0xA0,0x10,0x04,0x00,0,0,42,"volts","nvbrakeoffv"},
    {0xA0,0x10,0x04,0x00,0,0,6,"bps","nvrembaudrate"},
    {0xA0,0x10,0x04,0x00,0,0,10,"none","nvkqp"},
    {0xA0,0x10,0x04,0x00,0,0,14,"none","nvkqihi"},
    {0xA0,0x10,0x04,0x00,0,0,12,"none","nvkqilo"},
    {0xA0,0x10,0x04,0x00,0,0,16,"none","nvkqil"},
    {0xA0,0x10,0x04,0x00,0,0,18,"none","nvkdp"},
    {0xA0,0x10,0x04,0x00,0,0,22,"none","nvkdihi"},
    {0xA0,0x10,0x04,0x00,0,0,20,"none","nvkdilo"},
    {0xA0,0x10,0x04,0x00,0,0,24,"none","nvkdil"}
};

static const hm2_sserial_data_t hm2_7i64_params[] = {
    {0xA0,0x18,0x01,0x80,0,0,0,"none","output"},
    {0xA0,0x18,0x01,0x00,0,0,0,"none","input"},
    {0xA0,0x08,0x00,0x00,0,0,0,"pad","pad"},
    {0xA0,0x10,0x02,0x00,0,3.3,0,"volts","analog0"},
    {0xA0,0x10,0x02,0x00,0,3.3,0,"volts","analog1"}
};

typedef struct {
    unsigned char RecordType;
    unsigned char ModeIndex;
    unsigned char ModeType;
    unsigned char Unused;
    char NameString[HM2_SSERIAL_MAX_STRING_LENGTH+1];
}hm2_sserial_mode_t;

typedef struct {
    hal_u32_t *u32_pin;
    hal_s32_t *s32_pin;
    hal_float_t *float_pin;
    hal_bit_t **bit_pins;
    hal_bit_t **bit_pins_not;
    hal_bit_t *invert;
    hal_bit_t *boolean;
    hal_bit_t *boolean_not;
    hal_float_t maxlim;
    hal_float_t minlim;
    hal_float_t fullscale;
    s32 oldval;
}hm2_sserial_pins_t;

typedef struct {
    hal_u32_t u32_param;
    hal_s32_t s32_param;
}hm2_sserial_params_t;

typedef struct {
    int num_confs;
    int num_modes;
    int num_globals;
    int num_pins;
    hm2_sserial_mode_t *modes;
    hm2_sserial_data_t *confs;
    hm2_sserial_data_t *globals;
    hm2_sserial_pins_t *pins;
    hm2_sserial_params_t *params;
    hal_u32_t serialnumber;
    hal_u32_t status;

    u32 *reg_cs_read;
    u32 *reg_cs_write;
    u32 *reg_0_read;
    u32 *reg_0_write;
    u32 *reg_1_read;
    u32 *reg_1_write;
    u32 *reg_2_read;
    u32 *reg_2_write;
    u32 reg_cs_addr;
    u32 reg_0_addr;
    u32 reg_1_addr;
    u32 reg_2_addr;
    int index;
    u32 command_reg_addr; // a duplicate so that a single channel can be passed
    u32 data_reg_addr;
    int myinst;
    char name[21];
    
}hm2_sserial_remote_t;

typedef struct {
    int device_id;
    int use_serial_numbers;
    int num_remotes;
    int num_channels;
    int tag;
    hm2_sserial_remote_t *remotes;
    int index;
    u32 command_reg_addr;
    u32 *command_reg_read;
    u32 *command_reg_write;
    u32 data_reg_addr;
    u32 *data_reg_read;
    u32 *data_reg_write;
    hal_u32_t *fault_count;
    hal_u32_t fault_inc;
    hal_u32_t fault_dec;
    hal_u32_t fault_lim;
    
    hal_bit_t *run;
    hal_u32_t *state;
    u32 timer;
} hm2_sserial_instance_t;

typedef struct {
    u8 version;
    int baudrate;
    int num_instances; // number of active instances
    hm2_sserial_instance_t *instance ;
} hm2_sserial_t;

#endif
