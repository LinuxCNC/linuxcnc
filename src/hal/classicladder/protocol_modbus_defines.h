//    Copyright 2008-2010, various authors
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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef _PROTOCOL_MODBUS_DEFINES_H
#define _PROTOCOL_MODBUS_DEFINES_H

/* To set/reset single coil with function 5 */
#define MODBUS_BIT_OFF 0x0000
#define MODBUS_BIT_ON 0xFF00

#define LGT_MODBUS_IP_HEADER 7

/* Function codes list */
#define MODBUS_FC_READ_COILS 1
#define MODBUS_FC_READ_INPUTS 2
#define MODBUS_FC_READ_HOLD_REGS 3
#define MODBUS_FC_READ_INPUT_REGS 4
#define MODBUS_FC_FORCE_COIL 5
#define MODBUS_FC_WRITE_HOLD_REG 6
#define MODBUS_FC_FORCE_COILS 15
#define MODBUS_FC_WRITE_HOLD_REGS 16
#define MODBUS_FC_READ_STATUS 7
#define MODBUS_FC_DIAGNOSTICS 8
#define MODBUS_FC_EXCEPTION_BIT 0x80

/* Exceptions list */
#define MODBUS_ILLEGAL_FUNCTION 0x01 
#define MODBUS_ILLEGAL_DATA_ADDRESS 0x02 
#define MODBUS_ILLEGAL_DATA_VALUE 0x03 
#define MODBUS_SLAVE_DEVICE_FAILURE 0x04 
#define MODBUS_SLAVE_DEVICE_BUSY 0x06 
#define MODBUS_NEGATIVE_ACKNOWLEDGE 0x07 
#define MODBUS_MEMORY_PARITY_ERROR 0x08 

#endif

