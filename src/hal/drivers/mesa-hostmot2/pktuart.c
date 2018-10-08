//
//    Copyright (C) 2011 Andy Pugh, 2016 Boris Skegin
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERinstTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include "config_module.h"
#include RTAPI_INC_SLAB_H
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hostmot2.h"


#define MaxTrFrames     (16) // Send counts are written to 16 deep FIFO, burst mode

// Tx mode register errors
#define  TxSCFIFOError  (214)  // Tx Send Count FIFO Error


// Rx mode register errors
#define RxRCFIFOError   (114)  // RCFIFO Error, Bit  4  
#define RxOverrunError  (111)  // Overrun error (no stop bit when expected) (sticky), Bit  1
#define RxStartbitError (110)  // False Start bit error (sticky), Bit  0

// Rx count register errors
#define RxPacketOverrrunError (1115)    // Bit 15         Overrun error in this packet
#define RxPacketStartbitError (1114)    // Bit 14         False Start bit error in this packet

// the next two error conditions
// are very unprobable, but we consider them nevertheless
#define RxPacketSizeZero      (1120)    // the length of the received packet is 0
#define RxArraySizeError      (1140)    // sizeof(data array)= num_frames*max_frame_length is too small for all the data in the buffer

int hm2_pktuart_parse_md(hostmot2_t *hm2, int md_index) 
{
    // All this function actually does is allocate memory
    // and give the uart modules names. 
    
    
    // 
    // some standard sanity checks
    //
    
    int i, r = -EINVAL;
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    static int last_gtag = -1;
    
    //The PktUART declares a TX and RX module separately
    
    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 4, 4, 0x000F)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }
    
    if (hm2->pktuart.num_instances > 1 && last_gtag == md->gtag) {
        HM2_ERR(
                "found duplicate Module Descriptor for %s (inconsistent "
                "firmware), not loading driver %i %i\n",
                hm2_get_general_function_name(md->gtag), md->gtag, last_gtag
                );
        return -EINVAL;
    }
    last_gtag = md->gtag;
    
    if (hm2->config.num_pktuarts > md->instances) {
        HM2_ERR(
                "config defines %d pktuarts, but only %d are available, "
                "not loading driver\n",
                hm2->config.num_pktuarts,
                md->instances
                );
        return -EINVAL;
    }
    
    if (hm2->config.num_pktuarts == 0) {
        return 0;
    }

    // 
    // looks good, start, or continue, initializing
    // 

    if (hm2->pktuart.num_instances == 0){
        if (hm2->config.num_pktuarts == -1) {
            hm2->pktuart.num_instances = md->instances;
        } else {
            hm2->pktuart.num_instances = hm2->config.num_pktuarts;
        }
        
        hm2->pktuart.instance = (hm2_pktuart_instance_t *)hal_malloc(hm2->pktuart.num_instances 
                                                               * sizeof(hm2_pktuart_instance_t));
        if (hm2->pktuart.instance == NULL) {
            HM2_ERR("out of memory!\n");
            r = -ENOMEM;
            goto fail0;
        }
    }
    
    for (i = 0 ; i < hm2->pktuart.num_instances ; i++){
        hm2_pktuart_instance_t *inst = &hm2->pktuart.instance[i];
        // For the time being we assume that all PktUARTS come on pairs
        if (inst->clock_freq == 0){
            inst->clock_freq = md->clock_freq;
            r = rtapi_snprintf(inst->name, sizeof(inst->name), "%s.pktuart.%01d", hm2->llio->name, i);
            HM2_PRINT("created PktUART Interface function %s.\n", inst->name);
        }
        if (md->gtag == HM2_GTAG_PKTUART_TX){
            inst->tx_addr = md->base_address + i * md->instance_stride;
            inst->tx_fifo_count_addr = (md->base_address 
                                        + md->register_stride 
                                        + i * md->instance_stride);
            inst->tx_bitrate_addr = (md->base_address 
                                     + 2 * md->register_stride
                                     + i * md->instance_stride);
            inst->tx_mode_addr = (md->base_address 
                                  + 3 * md->register_stride
                                  +i * md->instance_stride);  
        }
        else if (md->gtag == HM2_GTAG_PKTUART_RX){
            inst->rx_addr = md->base_address + i * md->instance_stride;
            inst->rx_fifo_count_addr = (md->base_address 
                                        + md->register_stride 
                                        + i * md->instance_stride);
            inst->rx_bitrate_addr = (md->base_address 
                                     + 2 * md->register_stride
                                     + i * md->instance_stride);
            inst->rx_mode_addr = (md->base_address 
                                  + 3 * md->register_stride 
                                  +i * md->instance_stride);    
        }
        else{
            HM2_ERR("Something very wierd happened");
            goto fail0;
        }       
        
    }

    return hm2->pktuart.num_instances;
fail0:
    return r;
}

EXPORT_SYMBOL_GPL(hm2_pktuart_setup);
// use -1 for tx_mode and rx_mode to leave the mode unchanged
// use 1 for txclear or rxclear to issue a clear command for Tx or Rx registers
int hm2_pktuart_setup(char *name, int bitrate, s32 tx_mode, s32 rx_mode, int txclear, int rxclear){
    hostmot2_t *hm2;
    hm2_pktuart_instance_t *inst = 0;
    u32 buff;
    int i,r;
    
    i = hm2_get_pktuart(&hm2, name);
    if (i < 0){
        HM2_ERR_NO_LL("Can not find PktUART instance %s.\n", name);
        return -EINVAL;
    }
    inst = &hm2->pktuart.instance[i];

    buff = (u32)((bitrate * 1048576.0)/inst->clock_freq); //20 bits in this version
    r = 0;
    if (buff != inst->bitrate){
        inst->bitrate = buff;
        r += hm2->llio->write(hm2->llio, inst->rx_bitrate_addr, &buff, sizeof(u32));
        r += hm2->llio->write(hm2->llio, inst->tx_bitrate_addr, &buff, sizeof(u32));
    }

    /* http://freeby.mesanet.com/regmap
     The PktUARTx/PktUARTr mode register has a special data command that clears the PktUARTx/PktUARTr
     Clearing aborts any sends/receives in process, clears the data FIFO and 
     clears the send count FIFO. To issue a clear command, you write 0x80010000
     to the PktUARTx/PktUARTr mode register.
    */
    buff = 0x80010000;
    if (txclear==1)
        r += hm2->llio->write(hm2->llio, inst->tx_mode_addr, &buff, sizeof(u32)); // clear sends, data FIFO and count register
    if (rxclear==1)
        r += hm2->llio->write(hm2->llio, inst->rx_mode_addr, &buff, sizeof(u32)); // clear receives, data FIFO and count register

    /*  http://freeby.mesanet.com/regmap
      The PktUARTxMode register is used for setting and checking the
      PktUARTx's operation mode, timing and status:
      Bit  21          FrameBuffer Has Data
      Bits 20..16      Frames to send
      Bits 15..8       InterFrame delay in bit times
      Bit  7           Transmit Logic active, not an error
      Bit  6           Drive Enable bit (enables external RS-422/485 Driver when set)
      Bit  5           Drive enable Auto (Automatic external drive enable)
      Bit  4           unused
      Bits 3..0        Drive enable delay (delay from asserting drive enable 
                       to start of data transmit). In CLock Low periods
    */
    if (tx_mode >= 0) {
        buff = ((u32)tx_mode) & 0xffff;
        r += hm2->llio->write(hm2->llio, inst->tx_mode_addr, &buff, sizeof(u32));
    }

    /* http://freeby.mesanet.com/regmap
      The PktUARTrMode register is used for setting and checking the PktUARTr's 
      operation mode, timing, and status
      Bit  31..30      Unused
      Bit  29..22      Filter Register
      Bit  21          FrameBuffer has data 
      Bits 20..16      Frames received
      Bits 15..8       InterFrame delay in bit times
      Bit  7           Receive Logic active, not an error
      Bit  6           RXMask
      Bit  5           Unused
      Bit  4           RCFIFO Error
      Bit  3           RXEnable (must be set to receive packets) 
      Bit  2           RXMask Enable (enables input data masking when transmitting)
      Bit  1           Overrun error (no stop bit when expected) (sticky)
      Bit  0           False Start bit error (sticky)
    */
    if (rx_mode >= 0) {
        buff = ((u32)rx_mode) & 0x3fc0ffff; // 0011 1111 1100 0000 1111 1111 1111 1111
        // the expert user is allowed to pass his own FilterReg value,
        // otherwise it will be calculated as floor( 0.5*bittime*ClockLow -1 )
        if ( (buff >> 22) & (0xff == 0x0)) {
            u32 filter_reg = rtapi_floor(0.5*inst->clock_freq/inst->bitrate - 1.0) ;
            if (filter_reg > 255)
                filter_reg = 255;
            buff = buff | (filter_reg << 22) ;
        }
        r += hm2->llio->write(hm2->llio, inst->rx_mode_addr, &buff, sizeof(u32));
    }

    if (r < 0) {
        HM2_ERR("PktUART: hm2->llio->write failure %s\n", name);
        return -1;
    }

    return 0;
}


EXPORT_SYMBOL_GPL(hm2_pktuart_send);
int hm2_pktuart_send(char *name,  unsigned char data[], u8 *num_frames, u16 frame_sizes[])
{
    hostmot2_t *hm2;
    u32 buff;
    int r, c;
    int inst;
    
    inst = hm2_get_pktuart(&hm2, name);
    if (inst < 0){
        HM2_ERR_NO_LL("Can not find PktUART instance %s.\n", name);
        return -EINVAL;
    }
    if (hm2->pktuart.instance[inst].bitrate == 0){
        HM2_ERR("%s has not been configured.\n", name);
        return -EINVAL;
    }

    c = 0;
    u16 count = 0;
    /* 
       we work with nframes as a local copy of num_frames,
       so that we can return the num_frames sent out
       in case of SCFIFO error.
     */
    u8 nframes = *num_frames; 

    /* http://freeby.mesanet.com/regmap
       Send counts are written to 16 deep FIFO allowing up to 16 packets to be 
       sent in a burst (subject to data FIFO depth limits).
    */
    // Test if num_frames <= MaxTrFrames
    if ((*num_frames) > MaxTrFrames){
        nframes = MaxTrFrames;
    } else{
        nframes = *num_frames;
    }
    
    *num_frames = 0;

    u8 i;
    for (i = 0; i < nframes; i++){
        count = count + frame_sizes[i];
        while (c < count - 3){
               buff = (data[c] + 
                      (data[c+1] << 8) +
                      (data[c+2] << 16) +
                      (data[c+3] << 24));
               r = hm2->llio->write(hm2->llio, hm2->pktuart.instance[inst].tx_addr,
                             &buff, sizeof(u32));
               if (r < 0) {
                   HM2_ERR("%s send: hm2->llio->write failure\n", name);
                   return -1;
                  }
              c = c + 4;
    }


    // Now write the last bytes with bytes number < 4
    switch(count - c){
             case 0: 
                  break;
             case 1:
                  buff = data[c];  
                  r = hm2->llio->write(hm2->llio, hm2->pktuart.instance[inst].tx_addr,
                                 &buff, sizeof(u32));
                  if (r < 0){
                     HM2_ERR("%s send: hm2->llio->write failure\n", name);
                     return -1;
                  }
                  break;
             case 2:
                 buff = (data[c] + 
                        (data[c+1] << 8));
                 r = hm2->llio->write(hm2->llio, hm2->pktuart.instance[inst].tx_addr,
                                 &buff, sizeof(u32));
                 if (r < 0){
                     HM2_ERR("%s send: hm2->llio->write failure\n", name);
                     return -1;
                 }
                 break;
             case 3:
                 buff = (data[c] + 
                       (data[c+1] << 8) +
                       (data[c+2] << 16));
                 r = hm2->llio->write(hm2->llio, hm2->pktuart.instance[inst].tx_addr,
                                 &buff, sizeof(u32));
                 if (r < 0){
                     HM2_ERR("%s send: hm2->llio->write failure\n", name);
                     return -1;
                 }
                 break;
          default:
               HM2_ERR("%s send error in buffer parsing: count = %i, i = %i\n", name, count, c);
               return -1;
    } // end switch 

    // Write the number of bytes to be sent to PktUARTx sendcount register
    buff = (u32) frame_sizes[i];
    r = hm2->llio->write(hm2->llio, hm2->pktuart.instance[inst].tx_fifo_count_addr,
                                 &buff, sizeof(u32));
    // Check for Send Count FIFO error
    r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].tx_mode_addr,
                                 &buff, sizeof(u32));
    if ((buff >> 4) & 0x01) {
        HM2_ERR_NO_LL("%s: SCFFIFO error\n", name);
        return -TxSCFIFOError;
    }

    if (r < 0){
        HM2_ERR("%s send: hm2->llio->write failure\n", name);
        return -1;
    }

    (*num_frames)++;
    c = count;
    } // for loop

    return count;
}

EXPORT_SYMBOL_GPL(hm2_pktuart_read);
int hm2_pktuart_read(char *name, unsigned char data[], u8 *num_frames, u16 *max_frame_length, u16 frame_sizes[])
{
    hostmot2_t *hm2;
    int r, c;
    int bytes_total = 0; // total amount of bytes read
    u16 countp; // packets count
    u16 countb; // bytes count for the oldest packet received
    int inst;
    u32 buff;
    u16 data_size=(*num_frames)*(*max_frame_length);
    
    inst = hm2_get_pktuart(&hm2, name);
    
    if (inst < 0){ 
        HM2_ERR_NO_LL("Can not find PktUART instance %s.\n", name);
        *num_frames=0;  
        return -EINVAL;
    }
    if (hm2->pktuart.instance[inst].bitrate == 0 ) {
        HM2_ERR("%s has not been configured.\n", name);
        *num_frames=0;  
        return -EINVAL;
    }


    // First poll the mode register for a non zero frames recieved count 
    // (mode register bits 20..16)
    r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_mode_addr,
                        &buff, sizeof(u32));
    if (r < 0) {
        HM2_ERR("%s read: hm2->llio->write failure\n", name);
                return -1; // make the error message more detailed
    }
    countp = (buff >> 16)  & 0x1f; 
    // We expect to read at least 1 frame. 
    // If there is no complete frame yet in the buffer,
    // we'll deal with this by checking error bits.
    *num_frames = 0;

    // Bit 7 set does not really indicate any error condition,
    // but very probably means that the cycle time of the thread,
    // which you attach this function to, is not appropriate.       
    if ((buff >> 7) & 0x1){
        HM2_INFO("%s: Rx Logic active\n", name);
    }

    // Now check the error bits
    if ((buff >> 1) & 0x1){
        HM2_ERR_NO_LL("%s: Overrun error, no stop bit\n", name); 
        return -RxOverrunError;
    }
    if (buff & 0x1){
        HM2_ERR_NO_LL("%s: False Start bit error\n", name);     
        return -RxStartbitError;
    }

    // RCFIFO Error will get sticky if it is a consequence of either Overrun or False Start bit error?
    if ((buff >> 4) & 0x1){
        HM2_ERR_NO_LL("%s: RCFIFO Error\n", name); 
        return -RxRCFIFOError;
    }

    if (countp==0){ 
        HM2_ERR_NO_LL("%s: no new frames \n", name);            
        return 0;       // return zero bytes
    }


    int i=0;
    while ( i < countp ) {
          buff=0;
       /* The receive count register is a FIFO that contains the byte counts 
          of recieved packets. Since it is a FIFO it must only be read once after it 
          has be determined that there are packets available to read. */
          r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_fifo_count_addr,
                        &buff, sizeof(u32));
                    
          countb = buff & 0x3ff; // PktUARTr  receive count register Bits 9..0 : bytes in receive packet

          if ((buff >> 14) & 0x1) {
              HM2_ERR_NO_LL("%s has False Start bit error in this packet.\n", name);
              return -RxPacketStartbitError;
          }

          if ((buff >> 15) & 0x1) {
              HM2_ERR_NO_LL("%s has Overrun error in this packet\n", name);
              return -RxPacketOverrrunError;
          }

           // a packet is completely received, but its byte count is zero
           // is very unprobable, however we intercept this error too
          if (countb==0) {
              HM2_ERR_NO_LL("%s: packet %d has %d bytes.\n", name, countp+1, countb);
              return -RxPacketSizeZero; 
          }

          if (( bytes_total+countb)> data_size) {
               HM2_ERR_NO_LL("%s: bytes avalaible %d are more than data array size %d\n", name, bytes_total+countb, data_size);
               return -RxArraySizeError ;
          }

          (*num_frames)++; // increment num_frames to be returned at the end
          c = 0;
          buff = 0;
          frame_sizes[i]=countb;

          while (c < countb - 3){
                r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr,
                            &buff, sizeof(u32));

                if (r < 0) {
                   HM2_ERR("%s read: hm2->llio->read failure\n", name);
                   return r;
                }
                 
                 data[bytes_total+c] = (buff & 0x000000FF); // i*frame_sizes[i]
                 data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
                 data[bytes_total+c+2] = (buff & 0x00FF0000) >> 16;
                 data[bytes_total+c+3] = (buff & 0xFF000000) >> 24;
                 c = c + 4;
                 
            }

          switch(countb - c){
                 case 0: 
                      break;
                 case 1:
                      r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr,
                                &buff, sizeof(u32));
                      data[bytes_total+c]   = (buff & 0x000000FF);
                      break;
                 case 2:
                      r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr,
                                &buff, sizeof(u32));
                      data[bytes_total+c]   = (buff & 0x000000FF);
                      data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
                      break;
                 case 3:
                      r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr,
                                &buff, sizeof(u32));
                      data[bytes_total+c]   = (buff & 0x000000FF);
                      data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
                      data[bytes_total+c+2] = (buff & 0x00FF0000) >> 16;
                      break;
                default:
                     HM2_ERR_NO_LL("PktUART READ: Error in buffer parsing.\n");
                     return -EINVAL;
          }
        if (r < 0) {
            HM2_ERR("%s read: hm2->llio->write failure\n", name);
            return -1;
        }

       bytes_total = bytes_total + countb;   
       i++; // one frame/datagram read
    }// frame loop


    return bytes_total;
}

void hm2_pktuart_print_module(hostmot2_t *hm2){
    int i;
    HM2_PRINT("PktUART: %d\n", hm2->pktuart.num_instances);
    if (hm2->pktuart.num_instances <= 0) return;
    HM2_PRINT("    version: %d\n", hm2->pktuart.version);
    HM2_PRINT("    channel configurations\n");
    for (i = 0; i < hm2->pktuart.num_instances; i ++) {
        HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", 
                  hm2->pktuart.instance[i].clock_freq, 
                  hm2_hz_to_mhz(hm2->pktuart.instance[i].clock_freq));
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("    HAL name = %s\n", hm2->pktuart.instance[i].name);
    }
}

// The following standard Hostmot2 functions are not currently used by pktuart. 

void hm2_pktuart_cleanup(hostmot2_t *hm2)
{
}

void hm2_pktuart_write(hostmot2_t *hm2)
{
}

