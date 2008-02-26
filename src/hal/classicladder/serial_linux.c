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

#include "serial_common.h"
extern int ModbusSerialUseRtsToSend;
extern int ModbusDebugLevel;


int SerialSpeed[ ] = { 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 0 };
int SerialCorres[ ] = { B300, B600, B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, 0 };

char PortIsOpened = 0;
int fd;
struct termios oldtio;
struct termios newtio;

char SerialOpen( char * SerialPortName, int Speed )
{
	/* if port already opened => close it before */
	if ( PortIsOpened )
		SerialClose( );

	/* open the device to be non-blocking (read will return immediatly) */
	fd = open( SerialPortName, O_RDWR | O_NOCTTY | O_NDELAY/*don't wait DTR*/ );
	if (fd >=0)
	{
		int BaudRate = -1;
		int ScanBaudRate = 0;
		while( SerialSpeed[ ScanBaudRate ]!=Speed && SerialSpeed[ ScanBaudRate ]>=0 )
		{
			ScanBaudRate++;
			if ( SerialSpeed[ ScanBaudRate ]==Speed )
			{
				BaudRate = SerialCorres[ ScanBaudRate ];
			}
		}
		if ( BaudRate!=-1 )
		{        
			tcgetattr(fd,&oldtio); /* save current port settings */
			/* set new port settings */
			bzero(&newtio, sizeof(newtio));
			newtio.c_cflag = BaudRate | /*CRTSCTS |*/ CS8 | CLOCAL | CREAD;
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
	if ( PortIsOpened )
	{
		int status;
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
		write(fd,Buff,BuffLength);
		if ( ModbusDebugLevel>=2 )
			printf("Writing done!\n");
		if ( ModbusSerialUseRtsToSend )
		{
			// wait until everything has been transmitted
			tcdrain( fd );
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
		if ( ModbusDebugLevel>=2 )
			printf("Serial config...\n");
		tcsetattr(fd,TCSANOW,&newtio);
	}
}

int SerialReceive( char * Buff, int MaxBuffLength )//, int TimeOutResp )
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
tv.tv_sec = 0; //seconds
tv.tv_usec = newtio.c_cc[VTIME]*100 *1000; //micro-seconds
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

