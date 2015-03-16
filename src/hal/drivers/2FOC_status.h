/********************************************************************
* Description:  2FOC_status.h
* This file is part of the ZedBoard CAN communication driver for 
* the 2FOC board. This file provides the data structure of the
* status/fault CAN packet provided by the 2FOC controller. 
*
* \author Claudio Lorini (claudio.lorini@iit.it)
* License: GPL Version 2
* Copyright (c) 2015.
*
********************************************************************/
/**
 \brief Image of the status/fault of the 2FOC
 \par Revision history:
 \date 12.03.2015 ported to machinekit
*/

#ifndef __FOC_STATUS_H
#define __FOC_STATUS_H

//
// Flags for various Errors of the System
//
typedef union uSysError
{
    struct
    {
        // an invalid value of HES has been detected
        unsigned DHESInvalidValue:1;
        // an invalid sequence of HES activation has been detected
        unsigned DHESInvalidSequence:1;
        // ADC calibration failure
        unsigned ADCCalFailure:1;
        // was can. not used anymore. Can is now below
        unsigned Reserved:1;                    //4
        // Overvoltage
        unsigned OverVoltageFailure:1;
        // Undervoltage
        unsigned UnderVoltageFailure:1;
        // OverCurrent
        unsigned OverCurrentFailure:1;
        // I2T protection
        unsigned I2TFailure:1;                  //8
        // External Fault
        unsigned ExternalFaultAsserted:1;           

        // EMUROM Fault
        unsigned EMUROMFault:1;
        // EMUROM CRC Fault
        unsigned EMUROMCRCFault:1;

        // Rotor alignment procedure failed
        unsigned RotorAlignmentFault:1;         //12

        // SPI reading has been interrupted before finishing by foc loop
        unsigned FirmwareSPITimingError:1;		

        unsigned AS5045CSumError:1;               
        unsigned AS5045CalcError:1;				 

        // This is true when the FOC loop tried to delay the PWM update (see below)
        // but the wait for the PWM counter timed out (PWM counter freezed?)
        // This should never happen, and that may indicate
        // a firmware BUG or abnormal firmware behaviour due to
        // unpredictable and exotic reasons (malfunctions, voodoo
        // magic, hardware bugs, gravity inversions, alien invasions, 
        // end of the world).
        // In any case please consider that this is certainly NOT due
        // to the firmware developer, but more likely it's electronic 
        // eng. full responsibility :-)
        unsigned FirmwarePWMFatalError:1;         //16

        // an RTR flag has been seen over the wire. This is not OK for LorCan
        unsigned CAN_RTRFlagActive:1;
        // A CAN fifo full event has happened. This should never happen because
        // this FW on the 2FOC should be able to handle CAN with full load.
        // This might happen in certain blocking operation are requested like
        // save to eeprom parameters TODO: verify it.
        unsigned CAN_BufferOverRun:1;
   
        // useless to send over can when can is dead :)
        // unsigned CAN_BusOff:1; 

        // can has been in TX passive mode at least one time.
        unsigned CAN_TXWasPasv:1;
        // can IS is TX passive mode
        unsigned CAN_TXIsPasv:1;                //20
        // can has been in RX passive 
        unsigned CAN_RXWasPasv:1;
        // can IS in RX passive mode
        unsigned CAN_RXIsPasv:1;
        // can IS in bus warn   
        unsigned CAN_IsWarn:1;
        // can has been in bus warn at least one time
        unsigned CAN_WasWarn:1;
        // at least one DLC error has been seen
        unsigned CAN_DLCError:1;	
        // the MCU silicon revision in unsupported
        unsigned SiliconRevisionFault:1;
        // the position has run out upper position limit
        unsigned PositionLimitUpper:1;          //27
        // the position has run out lower position limit
        unsigned PositionLimitLower:1;
        // no new setpoint has been received for a long time..
        unsigned SetpointWatchdogTimeout:1;
        // the encoder read wrong data (valid when there is
        // also an AUX encoder connected to the board)
        unsigned EncoderFault:1;
        // the speed exceed the upper limit
        unsigned SpeedLimitUpper:1;
        // the speed exceed the lower limit
        unsigned SpeedLimitLower:1;

    }; //Flags; 
    // Permits to access the whole structure data in byte/word/array fashon 
    hal_u32_t     dW[2];
    unsigned int  W[2];
    unsigned char b[4];
}  __attribute__((__packed__)) tSysError;

#endif
