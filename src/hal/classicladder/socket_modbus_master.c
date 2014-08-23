/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* August 2005 */
/* ------------------------------------------- */
/* Socket for modbus master (Distributed I/O)  */
/* + making call to low-level serial functions */
/* if this is the mode used                    */
/* ------------------------------------------- */

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
// Chris Morley July 08 (EMC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#endif
#include <errno.h>
#include <time.h>

#include "classicladder.h"
#include "global.h"

#include "socket_modbus_master.h"
#include "protocol_modbus_master.h"
#include "serial_common.h"


#ifdef __WIN32__
#define SOCK_FD SOCKET
#define SOCK_INVALID SOCKET_ERROR
HANDLE ThreadHandleClient = NULL;
DWORD ThreadIdClient;
#else
#define SOCK_FD unsigned int
#define SOCK_INVALID -1
pthread_t thread_socket_client;
#endif

#define BUF_SIZE 512

// will work, if all reqs are different IP address... (perhaps too much and not necessary...)
#define NBR_CLIENTS_MAX NBR_MODBUS_MASTER_REQ

int ClientSocketRunning = 0;

int ClientSocketOpened[ NBR_CLIENTS_MAX ]; // -1 if not opened, else req nbr (to retrieve directly IP address)
SOCK_FD ClientSockDescrip[ NBR_CLIENTS_MAX ]; 
SOCK_FD client_s;             // Client socket descriptor

int CptErrors = 0;
int NbrFrames = 0;

void InitSocketModbusMaster( )
{

	int Error;
	int ScanClientSock;

	// WSAStartup for Windows already done for socket server...
	
	for( ScanClientSock=0; ScanClientSock<NBR_CLIENTS_MAX; ScanClientSock++ )
	{
		ClientSocketOpened[ ScanClientSock ] = -1;
		ClientSockDescrip[ ScanClientSock ] = SOCK_INVALID;
	}

	ClientSocketRunning = 1;
#ifdef __WIN32__
	ThreadHandleClient = CreateThread( NULL/*no security attributes*/, 16*1024L/*default stack size*/,                                                   
			(LPTHREAD_START_ROUTINE)SocketModbusMasterLoop/* thread function*/, 
			NULL/*argument to thread function*/,                
			THREAD_QUERY_INFORMATION/*use default creation flags*/,                           
			&ThreadIdClient/* returns the thread identifier*/);                
	if ( ThreadHandleClient==NULL )
#else
	Error = pthread_create( &thread_socket_client, NULL, (void *(*)(void *))SocketModbusMasterLoop, (void *)NULL );
	if (Error)
#endif
	{
		printf("ERROR CLASSICLADDER-   Failed to create thread I/O modbus master...\n");
		CloseSocketModbusMaster( );
	}
	else
	{
		Error = 0;
		if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
		{
			if ( !SerialOpen( ModbusSerialPortNameUsed, ModbusSerialSpeed ) )
				Error = -1;
                        printf("INFO CLASSICLADDER---I/O modbus master Data bits %i Stop bits %i Parity %i\n",ModbusSerialDataBits,ModbusSerialStopBits,ModbusSerialParity);
		}
		if ( Error!=-1 )
		printf("INFO CLASSICLADDER---I/O modbus master (%s) init ok !\n", ModbusSerialPortNameUsed[ 0 ]!='\0'?"Serial":"Ethernet");
	}
}

/* if not already connected => connect to slave... */
char VerifyTcpConnection( char * SlaveAdr )
{
	char AlreadyConnected = FALSE;
	char FreeOneFound = FALSE;
	// verify all connections, to see if one is opened with the same IP address...
	int ScanClientSock = 0;
	do
	{
		if ( ClientSocketOpened[ ScanClientSock ]!=-1 )
		{
			if ( strcmp( SlaveAdr, ModbusMasterReq[ ClientSocketOpened[ ScanClientSock ] ].SlaveAdr )==0 )
			{
				AlreadyConnected = TRUE;
			}
		}
		if ( !AlreadyConnected )
			ScanClientSock++;
	}
	while( !AlreadyConnected && ScanClientSock<NBR_CLIENTS_MAX );
	
	// if not connected => find a free
	if ( !AlreadyConnected )
	{
		ScanClientSock = 0;
		do
		{
			if ( ClientSocketOpened[ ScanClientSock ]==-1 )
				FreeOneFound = TRUE;
			else
				ScanClientSock++;
		}
		while( !FreeOneFound && ScanClientSock<NBR_CLIENTS_MAX );
		
		// connect to slave now !
		if ( FreeOneFound )
		{
			struct sockaddr_in io_module_addr;          // Server Internet address

			if( ModbusDebugLevel>=2 )
				printf("Init socket for I/O module (%d)...\n", ScanClientSock);	
			client_s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if ( client_s==SOCK_INVALID )
			{
				printf("Failed to open I/O socket master...\n");
			}
			else
			{
				int NumPort = 502; // default modbus port
				char * PosiSep;
				memset(&io_module_addr, 0, sizeof(io_module_addr));     /* Zero out structure */
				io_module_addr.sin_family = AF_INET;             /* Internet address family */
				
				// verify if port given in string ?
				PosiSep = strchr( SlaveAdr, ':' );
				if ( PosiSep==NULL )
				{
					io_module_addr.sin_addr.s_addr = inet_addr( SlaveAdr );   /* Server IP address */
				}
				else
				{
					static char Address[ 50 ];
					strcpy( Address, SlaveAdr );
					Address[ PosiSep-SlaveAdr ] = '\0';
					NumPort = atoi( PosiSep+1 );
					io_module_addr.sin_addr.s_addr = inet_addr( Address );   /* Server IP address */
				}
				io_module_addr.sin_port = htons( NumPort ); /* Server port */

				if( ModbusDebugLevel>=2 )
					printf("Connecting I/O module...\n");
				/* Establish the connection with the I/O module */
				if (connect(client_s, (struct sockaddr *) &io_module_addr, sizeof(io_module_addr)) >= 0)
				{
					int ScanModbusTableForIpAddr = 0;
					int OffsetIpAdrFound = -1;
					do
					{
						if ( strcmp( SlaveAdr, ModbusMasterReq[ ScanModbusTableForIpAddr ].SlaveAdr )==0 )
							OffsetIpAdrFound = ScanModbusTableForIpAddr;
						else
							ScanModbusTableForIpAddr++;
					}
					while( OffsetIpAdrFound==-1 && ScanModbusTableForIpAddr<NBR_MODBUS_MASTER_REQ );
					if ( OffsetIpAdrFound!=-1 )
					{
						AlreadyConnected = TRUE;
						ClientSocketOpened[ ScanClientSock ] = OffsetIpAdrFound;
						ClientSockDescrip[ ScanClientSock ] = client_s;
					}
					else
					{
						printf("Not able to retrieve IP address in modbus table, huhh?!!!\n");
					}
				}
				else
				{
					printf("Failed  to connect to I/O module\n"); 
				}
			}
		}
		else
		{
			printf("Too much IP connections under run...\n");
		}
	}
	else
	{
		client_s = ClientSockDescrip[ ScanClientSock ];
	}
	return AlreadyConnected;
}

int SendSocketModbusMaster( char * SlaveAdr, int NumPort, char * Frame, int LgtFrame )
{
	int Status = -1;
	int LgtSend;
	if ( VerifyTcpConnection( SlaveAdr ) )
	{
		if( ModbusDebugLevel>=2 )
			printf("INFO CLASSICLADDER-   Sending frame to I/O module...\n");
		/* Send the modbus frame */
		LgtSend = send(client_s, Frame, LgtFrame, 0);
		if ( LgtSend==LgtFrame )
			Status = 0;
		else
		printf("ERROR CLASSICLADDER-  FAILED TO SEND ON SOCKET !!!(LgtSend=%d)\n",LgtSend);
	}
	return Status;
}

int WaitRespSocketModbusMaster( char * Buff, int BuffSize, int TimeOutResponseMilliSecs )
{
		int ResponseSize = 0;	
		int recep_descrip;
		fd_set myset;
		struct timeval tv;

		FD_ZERO( &myset);
		// add descrip to survey and set time-out wanted !
		FD_SET( client_s, &myset );
		tv.tv_sec = 0; //seconds
		tv.tv_usec = TimeOutResponseMilliSecs*1000; //micro-seconds
		recep_descrip = select( 16, &myset, NULL, NULL, &tv );
		if ( recep_descrip>0 )
		{
		int bytesRcvd;
		if( ModbusDebugLevel>=2 )   {printf("INFO CLASSICLADDER-   waiting for slave response...\n");}
		if ((bytesRcvd = recv(client_s, Buff, BuffSize, 0)) > 0)    {ResponseSize = bytesRcvd;}
		}
	
	return ResponseSize;
}


void CloseSocketModbusMaster( void )
{
	int ScanClientSock;
	ClientSocketRunning = 0;
#ifdef __WIN32__
	if ( ThreadHandleClient )
		TerminateThread( ThreadHandleClient, 0);
#endif	
	// close sockets
	for( ScanClientSock=0; ScanClientSock<NBR_CLIENTS_MAX; ScanClientSock++ )
	{
		if ( ClientSocketOpened[ ScanClientSock ]!=-1 )
		{
#ifdef __WIN32__
			closesocket( ClientSockDescrip[ ScanClientSock ] );
#else
			close( ClientSockDescrip[ ScanClientSock ] );
#endif
		}
		ClientSocketOpened[ ScanClientSock ] = -1;
		ClientSockDescrip[ ScanClientSock ] = SOCK_INVALID;
	}
	if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
		SerialClose( );
	printf("INFO CLASSICLADDER---I/O modbus master closed!\n");
}

void SocketModbusMasterLoop( void )
{
	char AdrIP[ 30 ];
	int SizeQuestionToAsk;
	static char QuestionFrame[ 800 ];
	int ResponseSize;
	static char ResponseFrame[ 800 ];
        int SendResultStatus = 0;

#ifdef __XENO__
	pthread_set_name_np(pthread_self(), __FUNCTION__);
#endif

	while( ClientSocketRunning )
	{
// TODO: added for XENO... not found why required for now...
// (task suspended otherwise with the watchdog, seen with dmesg!)
// removing this next line for EMC- we don't use XENO

//		DoPauseMilliSecs( 10 );

		if (InfosGene->LadderState!=STATE_RUN)
		{
			DoPauseMilliSecs( ModbusTimeInterFrame );
		}
		else
		{
			SizeQuestionToAsk = ModbusMasterAsk( (unsigned char*)AdrIP, (unsigned char*)QuestionFrame );
			if ( SizeQuestionToAsk>0 )
			{
				if ( ModbusSerialPortNameUsed[ 0 ]=='\0' )
				{
					SendResultStatus = SendSocketModbusMaster( AdrIP, 502, QuestionFrame, SizeQuestionToAsk );
				}
				else
				{
					// before sending queston, set size of frame that will be to receive after! 
					SerialSetResponseSize( 1/*adr*/+GetModbusResponseLenghtToReceive()+2/*crc*/, ModbusTimeOutReceipt );
					SerialSend( QuestionFrame, SizeQuestionToAsk );
				}
                                if ( SendResultStatus==0 )
				{
				    if ( ModbusTimeAfterTransmit>0 )
				    {
				    	// usefull for USB-RS485 dongle...
				    	if( ModbusDebugLevel>=3 )
						printf("INFO CLASSICLADDER- after transmit delay now...%i milliseconds\n",ModbusTimeAfterTransmit);
					DoPauseMilliSecs( ModbusTimeAfterTransmit );
			    	    }
				
				    if ( ModbusSerialPortNameUsed[ 0 ]=='\0' )
					ResponseSize = WaitRespSocketModbusMaster( ResponseFrame, 800, ModbusTimeOutReceipt );
				    else
					ResponseSize = SerialReceive( ResponseFrame, 800, ModbusTimeOutReceipt );
				    NbrFrames++;
				    if ( ResponseSize==0 )
					printf("ERROR CLASSICLADDER-  MODBUS NO RESPONSE (Errs=%d/%d) !?\n", ++CptErrors, NbrFrames);
				    if ( !TreatModbusMasterResponse( (unsigned char *)ResponseFrame, ResponseSize ) )
				    {
					// trouble? => flush all (perhaps we can receive now responses for old asks
					// and are shifted between ask/resp...)
					if ( ModbusSerialPortNameUsed[ 0 ]!='\0' )
						SerialFlush( );
				    }
                                }
				DoPauseMilliSecs( ModbusTimeInterFrame );
			}
			else
			{
//				sleep( 1 );
				DoPauseMilliSecs( 1000 );
			}
		}
	
	}
#ifndef __WIN32__
	pthread_exit(NULL);
#endif

}


