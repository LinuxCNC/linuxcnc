/* Classic Ladder Project */
/* Copyright (C) 2001-2005 Marc Le Douarain */
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
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
extern int ModbusSerialUseRtsToSend;
extern int ModbusDebugLevel;
extern int ModbusSerialDataBits;   // Number of data bits (7, 6, 7, 8)
extern int ModbusSerialStopBits;   // Number of stop bits (1 or 2)
extern int ModbusSerialParity;      // Parity (00 = NONE, 01 = Odd, 02 = Even, 03 = Mark, 04 = Space)

int SerialSpeed[ ] = { 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 0 };
int SerialCorres[ ] = { B300, B600, B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, 0 };

char PortIsOpened = 0;
int fd;
long DATABITS;
long STOPBITS;
long PARITYON;
long PARITY;

struct termios oldtio;
struct termios newtio;

char SerialOpen( char * SerialPortName, int Speed )
{
	/* if port already opened => close it before */
	if ( PortIsOpened )
		SerialClose( );

	/* open the device to be non-blocking (read will return immediatly) */
	fd = open( SerialPortName, O_RDWR | O_NOCTTY | O_NDELAY/*don't wait DTR*/ );
        //fd = open( SerialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd >=0)
	{
		int BaudRate = -1;
		int ScanBaudRate = 0;
		fcntl(fd, F_SETFL, O_RDWR | O_NOCTTY ); /* perform blocking reads */
		for(ScanBaudRate = 0; SerialSpeed[ScanBaudRate]; ScanBaudRate++)
		{
			if ( SerialSpeed[ ScanBaudRate ]==Speed )
			{
				BaudRate = SerialCorres[ ScanBaudRate ];
				break;
			}
		}
switch (ModbusSerialDataBits)
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
      switch (ModbusSerialStopBits)
      {
         case 1:
         default:
            STOPBITS = 0;
            break;
         case 2:
            STOPBITS = CSTOPB;
            break;
      }  //end of switch stop bits
      switch (ModbusSerialParity)
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
		if ( BaudRate!=-1 )
		{        
			tcgetattr(fd,&oldtio); /* save current port settings */
			/* set new port settings */
			bzero(&newtio, sizeof(newtio));
			//newtio.c_cflag = BaudRate | /*CRTSCTS |*/ CS8 | CLOCAL | CREAD;
                        newtio.c_cflag = BaudRate | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
			//newtio.c_cflag |= PARENB
			newtio.c_iflag = IGNPAR    | IGNBRK; // | ICRNL;
			newtio.c_oflag = 0;
			newtio.c_lflag = 0;
			newtio.c_cc[VMIN]=0; //1;
			newtio.c_cc[VTIME]=0;
			tcsetattr(fd,TCSANOW,&newtio);
			PortIsOpened = 1;
		}
		else
		{
			printf("Speed value not found for serial\n");
		}
	}
	else
	{
		printf( "Failed to open serial port %s...\n", SerialPortName );
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
		if ( ModbusDebugLevel>=3 )
			printf( "Set RTS=%d\n", State );
		ioctl(fd, TIOCMSET, &status);
	}
}

void SerialSend( char *Buff, int BuffLength )
{
	if ( PortIsOpened )
	{
		if ( ModbusSerialUseRtsToSend )
		{
			SerialSetRTS( 1 );
		}
		if ( ModbusDebugLevel>=2 )
			printf("Serial writing...\n");
		while(BuffLength > 0) {
			int r = write(fd,Buff,BuffLength);
			if(r < 0) {
				if ( ModbusDebugLevel>=1 )
					printf("SerialSend failed: %s!\n", strerror(errno));
				break;
			}
			Buff += r;
			BuffLength -= r;
		}
		if ( ModbusDebugLevel>=2 )
			printf("Writing done!\n");
		// wait until everything has been transmitted
		tcdrain( fd );
		if ( ModbusSerialUseRtsToSend )
		{
			SerialSetRTS( 0 );
		}
	}
}

void SerialSetResponseSize( int Size, int TimeOutResp )
{
	if ( PortIsOpened )
	{
	        newtio.c_cc[VMIN] = Size; //Nbr chars we should receive;
		newtio.c_cc[VTIME] = 1;
//		tcflush(fd, TCIFLUSH);
		if ( ModbusDebugLevel>=2 )
			printf("Serial config...\n");
		tcsetattr(fd,TCSANOW,&newtio);
	}
}

int SerialReceive( char * Buff, int MaxBuffLength, int TimeOutResp )
{
	int NbrCarsReceived = 0;
	if ( PortIsOpened )
	{

// the select is used it no char at all is received (else read() block...)
int recep_descrip;
fd_set myset;
struct timeval tv;
FD_ZERO( &myset);
// add descrip to survey and set time-out wanted !
FD_SET( fd, &myset );
tv.tv_sec = TimeOutResp / 1000; //seconds
tv.tv_usec = (TimeOutResp % 1000) * 1000; //micro-seconds
if ( ModbusDebugLevel>=3 )
	printf("select() for serial reading...\n");
recep_descrip = select( 16, &myset, NULL, NULL, &tv );
if ( recep_descrip>0 )
{
		if ( ModbusDebugLevel>=2 )
			printf("Serial reading...\n");
			NbrCarsReceived = read(fd,Buff,MaxBuffLength);
		if ( ModbusDebugLevel>=2 )
			printf("%d chars found\n", NbrCarsReceived);
}
	}
	return NbrCarsReceived;
}

void SerialFlush( void )
{
	if ( PortIsOpened )
	{
		if ( ModbusDebugLevel>=2 )
			printf("Serial flush all!\n");
		tcflush( fd, TCIOFLUSH );
		usleep( 250*1000 );
		tcflush( fd, TCIOFLUSH );
	}
}

