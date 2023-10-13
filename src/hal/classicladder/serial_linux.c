/* Classic Ladder Project */
/* Copyright (C) 2001-2010 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* August 2005 */
/* ------------------------------------ */
/* Serial low-level functions for Linux */
/* ------------------------------------ */

/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h> 
#include <sys/ioctl.h>
#include <errno.h>

#include "serial_common.h"
#include "protocol_modbus_master.h"
extern StrModbusConfig ModbusConfig;
void DoPauseMilliSecs( int MilliSecsTime );

int SerialSpeed[ ] = { 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 0 };
int SerialBaudCorres[ ] = { B300, B600, B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, 0 };

char PortIsOpened = 0;
int fd;
struct termios oldtio;
struct termios newtio;

char SerialOpen( )
{
	/* if port already opened => close it before */
	if ( PortIsOpened )
		SerialClose( );

	/* open the device to be non-blocking (read will return immediatly) */
	fd = open( ModbusConfig.ModbusSerialPortNameUsed, O_RDWR | O_NOCTTY | O_NDELAY/*don't wait DTR*/ );
	if (fd >=0)
	{
		int BaudRate = -1;
		int ScanBaudRate = 0;
		fcntl(fd, F_SETFL, O_RDWR | O_NOCTTY ); /* perform blocking reads */
		while( BaudRate==-1 && SerialSpeed[ ScanBaudRate ]>=0 )
		{
			if ( SerialSpeed[ ScanBaudRate ]==ModbusConfig.ModbusSerialSpeed )
				BaudRate = SerialBaudCorres[ ScanBaudRate ];
			else
				ScanBaudRate++;
			
		}
		if ( BaudRate!=-1 )
		{        
			long DATABITS;
			long STOPBITS;
			long PARITYON;
			long PARITY;

printf("extra emc serial data=%d, parity=%d, stop=%d\n", ModbusConfig.ModbusSerialDataBits, ModbusConfig.ModbusSerialParity, ModbusConfig.ModbusSerialStopBits );
			// Nice EMC addons for more serial parameters !
			switch (ModbusConfig.ModbusSerialDataBits)
			{
				case 8:
				default:
					DATABITS = CS8;
					break;
				case 7:
					DATABITS = CS7;
					break;
				case 6:
					DATABITS = CS6;
					break;
				case 5:
					DATABITS = CS5;
					break;
			}  //end of switch data_bits
			switch (ModbusConfig.ModbusSerialStopBits)
			{
				case 1:
				default:
					STOPBITS = 0;
					break;
				case 2:
					STOPBITS = CSTOPB;
					break;
			}  //end of switch stop bits
			switch (ModbusConfig.ModbusSerialParity)
			{
				case 0:
				default:                       //none
					PARITYON = 0;
					PARITY = 0;
					break;
				case 1:                        //odd
					PARITYON = PARENB;
					PARITY = PARODD;
					break;
				case 2:                        //even
					PARITYON = PARENB;
					PARITY = 0;
					break;
			}  //end of switch parity

			tcgetattr(fd,&oldtio); /* save current port settings */
			/* set new port settings */
			bzero(&newtio, sizeof(newtio));
			//newtio.c_cflag = BaudRate | /*CRTSCTS |*/ CS8 | CLOCAL | CREAD;
			//newtio.c_cflag |= PARENB
			newtio.c_cflag = BaudRate | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
			newtio.c_iflag = IGNPAR    | IGNBRK; // | ICRNL;
			newtio.c_oflag = 0;
			newtio.c_lflag = 0;
			newtio.c_cc[VMIN]=0; //1;
			newtio.c_cc[VTIME]=0;
			tcsetattr(fd,TCSANOW,&newtio);
tcflush( fd, TCIFLUSH ); //discard possible datas not read
			PortIsOpened = 1;
		}
		else
		{
			printf(_("Serial speed value %d not found for serial\n"), ModbusConfig.ModbusSerialSpeed );
		}
	}
	else
	{
		printf( _("Failed to open serial port %s...\n"), ModbusConfig.ModbusSerialPortNameUsed );
	}
	return PortIsOpened;
}

void SerialClose( )
{
	if ( PortIsOpened )
	{
		PortIsOpened = 0;
		/* restore old port settings */
		tcsetattr(fd,TCSANOW,&oldtio);
		close(fd);
	}
}

char SerialPortIsOpened( void )
{
	return PortIsOpened;
}

void SerialSetRTS( int State )
{
     int status;
	if ( PortIsOpened )
	{
		ioctl(fd, TIOCMGET, &status);
		if ( State )
			status |= TIOCM_RTS;
		else
			status &= ~TIOCM_RTS;
		if ( ModbusConfig.ModbusDebugLevel>=3 )
			printf( _("Set RTS=%d\n"), State );
		ioctl(fd, TIOCMSET, &status);
	}
}

void SerialSend( char *Buff, int BuffLength )
{
	if ( PortIsOpened )
	{
		if ( ModbusConfig.ModbusSerialUseRtsToSend )
		{
			SerialSetRTS( 1 );
//premiers essais avec mon module AVR...
//////DoPauseMilliSecs( 30 );
		}
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("Serial writing...\n"));
		write(fd,Buff,BuffLength);
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("Writing done!\n"));
		if ( ModbusConfig.ModbusSerialUseRtsToSend )
		{
			// wait until everything has been transmitted
			tcdrain( fd );
//premiers essais avec mon module AVR...
DoPauseMilliSecs( 10 );
			SerialSetRTS( 0 );
		}
	}
}

void SerialSetResponseSize( int Size, int TimeOutResp )
{
	if ( PortIsOpened )
	{
	        newtio.c_cc[VMIN] = Size; //Nbr chars we should receive;
		newtio.c_cc[VTIME] = TimeOutResp/100; // TimeOut in 0.1s
//		tcflush(fd, TCIFLUSH);
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("Serial config...\n"));
		tcsetattr(fd,TCSANOW,&newtio);
	}
}

int SerialReceive( char * Buff, int MaxBuffLength )//, int TimeOutResp )
{
	int NbrCarsReceived = 0;
	if ( PortIsOpened )
	{

// the select is used if no char at all is received (else read() block...)
int recep_descrip;
fd_set myset;
struct timeval tv;
FD_ZERO( &myset);
// add descrip to survey and set time-out wanted !
FD_SET( fd, &myset );
tv.tv_sec = 0; //seconds
tv.tv_usec = newtio.c_cc[VTIME]*100 *1000; //micro-seconds
if ( ModbusConfig.ModbusDebugLevel>=3 )
	printf(_("select() for serial reading...\n"));
recep_descrip = select( 16, &myset, NULL, NULL, &tv );
if ( recep_descrip>0 )
{
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("Serial reading...\n"));
		NbrCarsReceived = read(fd,Buff,MaxBuffLength);
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("%d chars found\n"), NbrCarsReceived);
}
	}
	return NbrCarsReceived;
}

void SerialPurge( void )
{
	if ( PortIsOpened )
	{
		if ( ModbusConfig.ModbusDebugLevel>=2 )
			printf(_("Serial flush all!\n"));
		tcflush( fd, TCIOFLUSH );
		usleep( 250*1000 );
		tcflush( fd, TCIOFLUSH );
	}
}

