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


pthread_t thread_cyclic_refresh;
#endif
int CyclicThreadRunning = 0;

void HandlerSignalInterrupt( int signal_id )
{
	printf("End of application asked\n");
// HAL_SUPPORT
	hal_exit(compId);
	ClassicLadderFreeAll();
	exit( 0 );
}


void display_help (void)
{	printf("\nClassicLadder v" RELEASE_VER_STRING "\n" RELEASE_DATE_STRING "\n"
	       "Copyright (C) 2001-2004 Marc Le Douarain\nmavati@club-internet.fr\n"
	       "Adapted to EMC\n"
			"\n"
	       "ClassicLadder comes with NO WARRANTY\n"
	       "to the extent permitted by law.\n"
	       "\n"
	       "You may redistribute copies of ClassicLadder\n"
	       "under the terms of the GNU Lesser General Public Licence.\n"
	       "See the file `lesserGPL.txt' for more information.\n");	
	
	printf("\nUsage: classicladder [OPTIONS] [PATH]\n"
	       "eg: loadusr -w classicladder  ladtest.clp\n"
	       "eg: loadusr -w classicladder  --nogui ladtest.clp\n"
	       "\n"
	        "   --nogui   do not create a GUI, only load a configuration\n"
	                    );
	hal_exit(compId);
	exit(0);
}



void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;) {
		  int option_index = 0;
		  static const char *short_options = "c:";
		  static const struct option long_options[] = {
			{"nogui", no_argument, 0, 'n'},
                        {"debug",no_argument,0,'d'},
			{0, 0, 0, 0},		};

		  int c = getopt_long(argc, argv, short_options,
				    long_options, &option_index);
		  if (c == EOF) {break;}

		  switch (c) {
                                case 'n':
                                         nogui = 1;
                                         break;
                                case 'd':
                                rtapi_set_msg_level(RTAPI_MSG_ALL);
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

// HAL_SUPPORT
static void do_exit(int unused) {
	hal_exit(compId);
	printf("Error initializing classicladder user module");
	_exit(0);
}


int main( int   argc, char *argv[] )
{
        int used=0, NumRung;
        static int old_level ;
        old_level = rtapi_get_msg_level();
        //HAL_SUPPORT
        compId = hal_init("classicladder");
	if(compId < 0) return -1;
	signal(SIGTERM, do_exit);
		
        if (ClassicLadderAllocAll())
	{
             process_options (argc, argv);
             if ( nogui )
                {
                 rtapi_print_msg(RTAPI_MSG_DBG, "INFO CLASSICLADDER--- No ladder GUI requested\n");        
                 InitAllLadderDatas( TRUE );
                 InitTempDir( );
                 LoadProjectFiles( InfosGene->LadderDirectory );
                 // start the ladder program		    		
                 InfosGene->LadderState = STATE_RUN;
                 // Let HAL know we are ready now
                 hal_ready(compId);       
                 rtapi_print_msg(RTAPI_MSG_INFO, "Ladder project loaded\n");
                 CleanTmpDirectory( TmpDirectory, TRUE/*DestroyDir*/ );
                 hal_exit(compId);			
                 return 0;
                }else{	
                      //check for used rungs meaning previous program loaded
                      rtapi_print_msg(RTAPI_MSG_INFO, "checking for used rungs\n");
                      for(NumRung=0;NumRung<NBR_RUNGS;NumRung++)    {   if(RungArray[NumRung].Used) used++;   }
                      rtapi_print_msg(RTAPI_MSG_INFO, "Used rungs: %d\n", used);
                      if((used==0) || ( (argc - optind) != 0) )   {   InitAllLadderDatas( TRUE );    }
#ifdef GTK_INTERFACE
                      InitGtkWindows( argc, argv );
                      rtapi_print_msg(RTAPI_MSG_INFO, "Init GTK");
#endif
                      InitTempDir( );
                      rtapi_print_msg(RTAPI_MSG_INFO, "Init tmp dir=%s\n", TmpDirectory);
                      // if no rungs used (meaning no program previously loaded)
                      // then Load All LadderDatas
                      if((used==0) || ( (argc - optind) != 0) )  {   LoadProjectFiles( InfosGene->LadderDirectory );   }
#ifdef GTK_INTERFACE
                      UpdateGtkAfterLoading( TRUE/*cCreateTimer*/ );
                      UpdateWindowTitleWithProjectName();
#endif
                      // start running ladder program
                      InfosGene->LadderState = STATE_RUN;
                      //let HAL know we are ready...
                      hal_ready(compId);
#ifdef GTK_INTERFACE
                      rtapi_print_msg(RTAPI_MSG_INFO, "Loading ladder GUI\n");
                      gtk_main();
                      printf("Ladder GUI closed. Realtime module still runs till HAL closes\n");	
                     }
        }
         ClassicLadderFreeAll(); 
         CleanTmpDirectory( TmpDirectory, TRUE );
         rtapi_set_msg_level(old_level);
         hal_exit(compId);
         return 0;
}
#endif
