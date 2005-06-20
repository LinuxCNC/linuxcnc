/* Classic Ladder Project */
/* Copyright (C) 2001-2004 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* December 2004 */
/* -------------------------------------- */
/* Socket server used for modbus protocol */
/* -------------------------------------- */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "classicladder.h"
#include "global.h"
#include "protocol_modbus_slave.h"
#include "socket_server.h"


#define SOCK_FD unsigned int
#define SOCK_INVALID -1
pthread_t thread_socket_server;

#define BUF_SIZE 512
SOCK_FD server_s;             // Server socket descriptor
struct sockaddr_in server_addr;          // Server Internet address

int SocketOpened = 0;
int SocketRunning = 0;


/* TODO: Add support for Modbus/UDP. TCP sucks for a such serial protocol ! ;-) */
void InitSocketServer( int UseUdpMode, int PortNbr )
{

	int Error;
	// to not receive "Broken pipe" exception 
	signal(SIGPIPE, SIG_IGN);

printf("INIT SOCKET!!!\n");	
	// Create a socket
	server_s = socket(AF_INET, SOCK_STREAM, 0);
	if ( server_s==SOCK_INVALID )
	{
		printf("Failed to open socket server...\n");
	}
	else
	{
		SocketOpened = 1;
		// Fill-in my socket's address information and bind the socket
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons( PortNbr );
		server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
		Error = bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if ( Error==SOCK_INVALID )
		{
			printf("Failed to bind socket server...(error=%s)\n", strerror(errno));
			CloseSocketServer( );
		}
		else
		{
			// Listen for connections and then accept
			Error = listen(server_s, 1);
			if ( Error==SOCK_INVALID )
			{
				printf("Failed to bind socket server...(error=%s)\n", strerror(errno));
				CloseSocketServer( );
			}
			else
			{
				SocketRunning = 1;
		
				Error = pthread_create( &thread_socket_server, NULL, (void *(*)(void *))SocketServerTcpMainLoop, (void *)NULL );
				if (Error==-1)
				{
					printf("Failed to create thread socket server...\n");
					CloseSocketServer( );
				}
				else
				{
					printf("Server socket init ok (modbus - port %d)!\n", PortNbr);
				}
			}
		}
	
	}
}

#define LGT_IP_HEADER 7
int AskAndAnswer( unsigned char * Ask, int LgtAsk, unsigned char * Answer )
{
	int LgtModbusReply = 0;
	int LgtHeaderReply = 0;
	int i;
	printf("-> ");
	for (i=0; i<LgtAsk; i++)
	{
		if ( i==LGT_IP_HEADER )
			printf("- ");
		printf ("%02X ", Ask[i]);
	}
	printf("\n");

	LgtModbusReply = ModbusRequestToRespond( &Ask[ LGT_IP_HEADER ], LgtAsk, &Answer[ LGT_IP_HEADER ] );
	// Send response to the client
	if ( LgtModbusReply>0 )
	{
		// add IP specific header
		// invocation identifier
		Answer[ LgtHeaderReply++ ] = Ask[ 0 ];
		Answer[ LgtHeaderReply++ ] = Ask[ 1 ];
		// protocol identifier
		Answer[ LgtHeaderReply++ ] = 0;
		Answer[ LgtHeaderReply++ ] = 0;
		// length					
		Answer[ LgtHeaderReply++ ] = (unsigned char)(LgtModbusReply>>8);
		Answer[ LgtHeaderReply++ ] = (unsigned char)LgtModbusReply;
		// unit identifier					
		Answer[ LgtHeaderReply++ ] = 1;
printf("MODBUS RESPONSE LGT=%d+%d !\n", LgtHeaderReply, LgtModbusReply );
		printf("<- ");
		for (i=0; i<LgtHeaderReply+LgtModbusReply; i++)
		{
			if ( i==LGT_IP_HEADER )
				printf("- ");
			printf ("%02X ", Answer[i]);
		}
		printf("\n");
	}
	return LgtHeaderReply+LgtModbusReply;
}

/* Main loop for tcp mode connections */
void SocketServerTcpMainLoop( void )
{
	SOCK_FD client_s;             // Client socket descriptor
	struct sockaddr_in client_addr;          // Client Internet address
	int addr_len;             // Internet address length
	unsigned char in_buf[BUF_SIZE];     // Input buffer for resquest
	unsigned char out_buf[BUF_SIZE];    // Output buffer for response

	int retcode;
	while( SocketRunning )
	{
printf("SOCKET WAITING...\n");
		addr_len = sizeof(client_addr);
		client_s = accept(server_s, (struct sockaddr *)&client_addr, &addr_len);
		if ( client_s!=-1 )
		{
printf("SOCKET CLIENT ACCEPTED...\n");
			do
			{
				// Request received from the client
				//  - The return code from recv() is the number of bytes received
				retcode = recv(client_s, in_buf, BUF_SIZE, 0);
printf("SOCKED RECEIVED=%d !\n", retcode);
				if ( retcode==-1 )
				{
					printf("Failed to recv socket server...(error=%s)\n", strerror(errno));
				}
				else
				{
					if ( retcode>0 )
					{
						int LgtReply = AskAndAnswer( in_buf, retcode, out_buf ); 
						if ( LgtReply>0 )
							send(client_s, out_buf, LgtReply, 0);
					}
				}

			}
			while( retcode>0 );
printf("CLOSE SOCK CLIENT.\n");
			close(client_s);
		}
	}
	pthread_exit(NULL);
}

void CloseSocketServer( void )
{
	SocketRunning = 0;
	// close socket server
	if ( SocketOpened )
	{
		close(server_s);
		SocketOpened = 0;
		printf("Server socket closed!\n");
	}
}



