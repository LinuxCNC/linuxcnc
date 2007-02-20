/* Classic Ladder Project */
/* Copyright (C) 2001-2006 Marc Le Douarain */
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
#include "files.h"
#include "calc.h"
#include "vars_access.h"
#include "manager.h"
#include "calc_sequential.h"
#include "files_sequential.h"

#if !defined( MODULE )
#include "classicladder_gtk.h"
#include "manager_gtk.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
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
int nogui=0;
int ModbusServerPort = 9502; // Standard "502" requires root privileges...

#if !defined( MODULE ) && !defined( RTAPI )

#ifdef __WIN32__
HANDLE ThreadHandleCyclicRefresh = NULL;
DWORD ThreadIdCyclicRefresh;
#else
pthread_t thread_cyclic_refresh;
#endif
int CyclicThreadRunning = 0;

void HandlerSignalInterrupt( int signal_id )
{
	printf("End of application asked\n");
	InfosGene->LadderState = STATE_LOADING;
#ifdef HAL_SUPPORT
	hal_exit(compId);
#else
	CloseSocketModbusMaster( );
	CloseSocketServer( );
#endif
	ClassicLadderFreeAll();
	exit( 0 );
}


void display_help (void)
{
	printf("Usage: classicladder [OPTIONS] [PATH]\n"
	       "Start classicladder PLC with an optional project path e.g. myplc\n"
	       "\n"
	       "           --help     	        display this help and exit\n"
	       "           --version  	        output version information and exit\n"
	       "           --nogui		do not create a GUI, only load a configuration\n"
	       "-p port    --modbus_port=port   port to use for modbus server\n"
#ifdef DYNAMIC_PLCSIZE
	       "-c file    --config=file        read PLC configuration from file\n"
#endif
              );
	exit(0);
}

void display_version (void)
{
	printf("ClassicLadder v" RELEASE_VER_STRING "\n" RELEASE_DATE_STRING "\n"
	       "\n"
               "Copyright (C) 2001-2004 Marc Le Douarain\nmavati@club-internet.fr\n"
	       "\n"
	       "ClassicLadder comes with NO WARRANTY\n"
	       "to the extent permitted by law.\n"
	       "\n"
	       "You may redistribute copies of ClassicLadder\n"
	       "under the terms of the GNU Lesser General Public Licence.\n"
	       "See the file `lesserGPL.txt' for more information.\n");
	exit(0);
}

void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "c:";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"version", no_argument, 0, 0},
			{"nogui", no_argument, 0, 'n'},
#ifdef DYNAMIC_PLCSIZE
			{"config", required_argument, 0, 'c'},
#endif
			{"modbus_port", required_argument, 0, 'p'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				    long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
		case 0:
			switch (option_index) {
			case 0:
				display_help();
				break;
			case 1:
				display_version();
				break;
			}
			break;
		case 'c':
#ifndef RT_SUPPORT
			read_config (optarg);
#else
			printf("Config file is used by the RT module in RTLinux version !!!\n");
#endif
			break;
                case 'n':
                        nogui = 1;
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

#ifndef HAL_SUPPORT
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

void CyclicCalcRefresh( void )
{
#ifdef __XENO__
	struct timespec start, period, now;
	unsigned long overruns;
	int err = 0;
	pthread_set_name_np( pthread_self(), __FUNCTION__ );
	clock_gettime( CLOCK_REALTIME, &start );
	start.tv_sec += 1;	/* Start in one second from now. */
	period.tv_sec = 0;
	period.tv_nsec = TIME_REFRESH_RUNG_MS*1000000;
	if ( pthread_make_periodic_np(pthread_self(), &start, &period)!=0 )
	{
		printf("Xenomai make_periodic failed: err %d\n", err);
		exit(EXIT_FAILURE);
	}
#endif
	while( CyclicThreadRunning )
	{
#ifdef __XENO__
		err = pthread_wait_np(&overruns);
		if (err || overruns) {
			printf( "Xenomai wait_period failed: err %d, overruns: %lu\n", err, overruns);
//			exit(EXIT_FAILURE);
		}
#else
		DoPauseMilliSecs( TIME_REFRESH_RUNG_MS );
#endif

		if (InfosGene->LadderState==STATE_RUN)
		{
#ifdef __XENO__
			clock_gettime( CLOCK_REALTIME, &start );
#endif
			ReadPhysicalInputs();
			RefreshAllRungs();
			WritePhysicalOutputs();

#ifdef __XENO__
			clock_gettime( CLOCK_REALTIME, &now );
			if ( now.tv_sec==start.tv_sec )
				InfosGene->DurationOfLastScan = now.tv_nsec-start.tv_nsec;
#endif
		}
	}
#ifndef __WIN32__
	pthread_exit(NULL);
#endif
}

int InitCyclicThread( void )
{
	int InitOk = 0;
	int Error;
	CyclicThreadRunning = 1;
#ifdef __WIN32__
	ThreadHandleCyclicRefresh = CreateThread( NULL/*no security attributes*/, 16*1024L/*default stack size*/,                                                   
			(LPTHREAD_START_ROUTINE)CyclicCalcRefresh/* thread function*/, 
			NULL/*argument to thread function*/,                
			THREAD_QUERY_INFORMATION/*use default creation flags*/,                           
			&ThreadIdCyclicRefresh/* returns the thread identifier*/);                
	if ( ThreadHandleCyclicRefresh==NULL )
#else
	pthread_attr_t ThreadAttributes;
	pthread_attr_init(&ThreadAttributes);
	pthread_attr_setdetachstate(&ThreadAttributes, PTHREAD_CREATE_DETACHED /*PTHREAD_CREATE_JOINABLE*/);
#ifdef __XENO__
	pthread_attr_setschedpolicy(&ThreadAttributes, SCHED_FIFO);
	struct sched_param paramA = { .sched_priority = 98 };
	pthread_attr_setschedparam(&ThreadAttributes, &paramA);
#endif
	pthread_attr_setstacksize(&ThreadAttributes, 32*1024);
	Error = pthread_create( &thread_cyclic_refresh, &ThreadAttributes, (void *(*)(void *))CyclicCalcRefresh, (void *)NULL );
	if (Error==-1)
#endif
		printf("Failed to create cyclic calc thread...\n");
	else
		InitOk = 1;
	return InitOk;
}
#endif

#ifdef HAL_SUPPORT
static void do_exit(int unused) {
	hal_exit(compId);
	_exit(0);
}
#endif

int main( int   argc, char *argv[] )
{
#ifdef __XENO__
	mlockall(MCL_CURRENT | MCL_FUTURE);
#endif

#if defined(HAL_SUPPORT) && defined(ULAPI)
	compId = hal_init("classicladder");
	if(compId < 0) return -1;
	signal(SIGTERM, do_exit);
	hal_ready(compId);
#endif

#ifdef USE_MODBUS
	InitModbusMasterBeforeReadConf( );
#endif
	process_options (argc, argv);
	if (ClassicLadderAllocAll())
	{
		if ( nogui ) {
                    InitAllLadderDatas( TRUE );
                    InitTempDir( );
                    LoadProjectFiles( LadderDirectory );
		    InfosGene->LadderState = STATE_RUN;
		    ClassicLadderFreeAll();
                    hal_exit(compId);
                    return 0;
                }
#if !defined(HAL_SUPPORT)
		InitAllLadderDatas( TRUE );
		InitSocketServer( 0/*UseUdpMode*/, ModbusServerPort/*PortNbr*/ );
		InitSocketModbusMaster( );
#endif

// cyclic thread for rt is in the kernel (RTLinux/RTAI)
#if #defined(RT_SUPPORT) && !defined(HAL_SUPPORT)
		if ( InitCyclicThread( ) )
#endif
                {
                    int used=0, NumRung;
                    for(NumRung=0;NumRung<NBR_RUNGS;NumRung++) {
                        if(RungArray[NumRung].Used) used++;
                    }
                    printf("Used rungs: %d\n", used);
#ifdef GTK_INTERFACE
			InitGtkWindows( argc, argv );
#endif
	
                    
	// added here in v0.7.5
			InitTempDir( );
printf("Init tmp dir=%s\n", TmpDirectory);
	
	////        LoadAllLadderDatas(LadderDirectory);
			if(!used)
                            LoadProjectFiles( LadderDirectory );
#ifdef GTK_INTERFACE
			UpdateGtkAfterLoading( TRUE/*cCreateTimer*/ );
#endif
#ifndef RT_SUPPORT
			OpenHardware( 0 );
			ConfigHardware( );
#endif
	
			InfosGene->LadderState = STATE_RUN;
	
#ifdef GTK_INTERFACE
	//ProblemWithPrint		gdk_threads_enter( );
			gtk_main();
	//ProblemWithPrint		gdk_threads_leave( );
#else
			signal( SIGINT /*SIGTERM*/, HandlerSignalInterrupt );
			printf("Running...\n");
			for( ;; )
				sleep( 1 );
#endif
			CyclicThreadRunning = 0;
#ifdef __WIN32__
			if ( ThreadHandleCyclicRefresh )
				TerminateThread( ThreadHandleCyclicRefresh, 0);
#endif	
		}
	}
	else
	{
		ClassicLadderFreeAll();
	}

	hal_exit(compId);

	return 0;
}
#endif
