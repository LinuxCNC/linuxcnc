
//
//    Copyright (C) 2016 Boris Skegin
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

#ifndef __PKTUART_ERRNO_H
#define __PKTUART_ERRNO_H

// Tx mode register errors
#define  HM2_PKTUART_TxSCFIFOError  (214)  // Tx Send Count FIFO Error


// Rx mode register errors
#define HM2_PKTUART_RxRCFIFOError   (114)  // RCFIFO Error, Bit  4  
#define HM2_PKTUART_RxOverrunError  (111)  // Overrun error (no stop bit when expected) (sticky), Bit  1
#define HM2_PKTUART_RxStartbitError (110)  // False Start bit error (sticky), Bit  0

// Rx count register errors
#define HM2_PKTUART_RxPacketOverrrunError (1115)    // Bit 15         Overrun error in this packet
#define HM2_PKTUART_RxPacketStartbitError (1114)    // Bit 14         False Start bit error in this packet

// the next two error conditions
// are very unprobable, but we consider them nevertheless
#define HM2_PKTUART_RxPacketSizeZero      (1120)    // the length of the received packet is 0
#define HM2_PKTUART_RxArraySizeError      (1140)    // sizeof(data array)= num_frames*max_frame_length is too small for all the data in the buffer

#endif