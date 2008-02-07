/* Classic Ladder Project */
/* Copyright (C) 2001-2007 Marc Le Douarain */
/* http://www.multimania.com/mavati/classicladder */
/* http://www.sourceforge.net/projects/classicladder */
/* February 2001 */
/* ------------------------------ */
/* The main for Linux Application */
/* ------------------------------ */
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

// This is an adaption of classicladder V7.124 For EMC
// most of this file is very different from the original
// it uses RTAPI realtime code and HAL code for memory allocation and input/output.
// this adaptation was started Jan 2008 by Chris Morley  

#ifdef MODULE
#include <linux/string.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "hal.h"
#include "classicladder.h"
#include "global.h"
#include "files_project.h"
#include "calc.h"
#include "vars_access.h"
#include "manager.h"
#include "calc_sequential.h"
#include "files_sequential.h"
#include "config.h"
// #include "hardware.h"
#include "socket_server.h"
#include "socket_modbus_master.h"

#if !defined( MODULE )
#include "classicladder_gtk.h"
#include "manager_gtk.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#ifdef __WIN32__
#include <windows.h>
#else
#include <pthread.h>
#endif
#ifdef __XENO__
#include <sys/mman.h>
#endif
#endif

#ifdef GTK_INTERFACE
#include <gtk/gtk.h>
#endif

#ifdef MAT_CONNECTION
#include "../../lib/plc.h"
#endif

int cl_remote;
int nogui = 0;
int ModbusServerPort = 9502; // Standard "502" requires root privileges...
int CyclicThreadRunning = 0;


void ClassicLadderEndOfAppli( void )
{
	CyclicThreadRunning = 0;
	CloseSocketModbusMaster( );
	CloseSocketServer( );
	hal_exit(compId); // added for emc
printf("end of appli --gonna free\n");
	ClassicLadder_FreeAll(TRUE);

}

void HandlerSignalInterrupt( int signal_id )
{
	printf("End of application asked\n");
	ClassicLadderEndOfAppli( );
	exit( 0 );
}


void display_help (void)
{
	printf("Usage: classicladder [OPTIONS] [PATH]\n"
			"Start classicladder PLC with an optional project path e.g. myplc\n"
			"\n"
			"           --help     	        display this help and exit\n"
			"           --version  	        output version information and exit\n"
			"           --nogui             do not create the GUI\n"
			"-p port    --modbus_port=port   port to use for modbus server\n"
			"-c file    --config=file        read PLC configuration from file\n"
			);
	hal_exit(compId); // add for emc
	exit(0);
}



void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;)
	{
		int option_index = 0;
		static const char *short_options = "c:";
		static const struct option long_options[] = {
			{"nogui", no_argument, 0, 'n'},
			{"config", required_argument, 0, 'c'},
			{"modbus_port", required_argument, 0, 'p'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				    long_options, &option_index);
		if (c == EOF)
			break;

		switch (c)
		{
			
			case 'n':
				nogui = 1;
				break;
			case 'c':
	
				read_config (optarg);
	
				break;
			case 'p':
				ModbusServerPort = atoi( optarg );
				break;
			case '?':
				error = 1;
				break;
		}
	}

	if (error)
		display_help ();

	if ((argc - optind) != 0)
		VerifyDirectorySelected (argv[optind]);
}
//for emc: do_exit
static void do_exit(int unused) {
		hal_exit(compId);
		printf("error intializing classicladder user module\n");
		exit(0);
}
void DoPauseMilliSecs( int Time )
{
#ifdef __WIN32__
	Sleep( Time );
#else
	struct timespec time;
	time.tv_sec = 0;
	time.tv_nsec = Time*1000000;
	nanosleep( &time, NULL );
	//usleep( Time*1000 );
#endif
}
void StopRunIfRunning( void )
{
	if (InfosGene->LadderState==STATE_RUN)
	{
		printf("Stopping...");
		InfosGene->LadderStoppedToRunBack = TRUE;
		InfosGene->LadderState = STATE_STOP;
		while( InfosGene->UnderCalculationPleaseWait==TRUE )
		{
			DoPauseMilliSecs( 100 );
		}
		printf("done.\n");
	}
}
void RunBackIfStopped( void )
{
	if ( InfosGene->LadderStoppedToRunBack )
	{
		printf("Start running!\n");
		InfosGene->LadderState = STATE_RUN;
		InfosGene->LadderStoppedToRunBack = FALSE;
	}
}
// after processing options and intiallising HAL, MODBUS and registering shared memory
// the main function is divided into  NOGUI true or NOGUI FALSE
// The difference between them is mostly about checking if a program has already been loaded (only when GUI is to be shown)
// if rungs are used then a program is already in memory so we dont re initallise memory we just start the GUI

int main( int   argc, char *argv[] )
{
	int used=0, NumRung;

	compId=hal_init("classicladder"); //emc
	if (compId<0) return -1; //emc
	signal(SIGTERM,do_exit); //emc
	
	
	InitModbusMasterBeforeReadConf( );

	process_options (argc, argv);


	if (ClassicLadder_AllocAll())
	{

		InitSocketServer( 0/*UseUdpMode*/, ModbusServerPort/*PortNbr*/ );
		InitSocketModbusMaster( );
				
			char ProjectLoadedOk=TRUE;
		if (nogui==TRUE) {
			 rtapi_print("***No ladder GUI*** realtime is running till HAL closes***\n");
			ClassicLadder_InitAllDatas( );
			ProjectLoadedOk = LoadProjectFiles( CurrentProjectFileName );
			
			InfosGene->LadderState = STATE_RUN;
			ClassicLadder_FreeAll(TRUE);
			hal_ready(compId);
			hal_exit(compId);	
			return 0; }

		else {		
				for(NumRung=0;NumRung<NBR_RUNGS;NumRung++) 
		 		{ if(RungArray[NumRung].Used) used++; }
				printf("\nINFO___used= %d\n",used);
				if(used==0){	
						ClassicLadder_InitAllDatas( );
						ProjectLoadedOk = LoadProjectFiles( CurrentProjectFileName );
					   }

				InitGtkWindows( argc, argv );
				
				UpdateAllGtkWindows();				
				MessageInStatusBar( ProjectLoadedOk?"Project loaded and running":"Project failed to load...");
				if (!ProjectLoadedOk){ClassicLadder_InitAllDatas( );}				
				printf("INFO___halready next\n");				

				InfosGene->LadderState = STATE_RUN;
				hal_ready(compId);
				gtk_main();
				 rtapi_print("Ladder GUI closed realtime runs till HAL closes\n");
				ClassicLadder_FreeAll(TRUE);
				hal_exit(compId);
				return 0;	}		
	}
	 rtapi_print("Ladder memory allocation error\n");
	ClassicLadder_FreeAll(TRUE);
	hal_exit(compId);		
	return 0;
}
