/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#ifdef GTK_INTERFACE
#include <gtk/gtk.h>
#endif

#include "rtapi.h"
#include "classicladder.h"
#include "global.h"
#include "arrays.h"
#include "files.h"
#include "socket_server.h"
#ifdef GTK_INTERFACE
#include "classicladder_gtk.h"
#endif


int cl_remote;
int ModbusServerPort = 9502; // Standard "502" requires root privileges...
int noGui = FALSE;
int loadProject = FALSE;

char LadderDirectory[400] = "";


void display_help (void)
{
	printf("Usage: classicladder [OPTIONS] [PATH]\n"
	       "Start classicladder PLC with an optional project path to load\n"
	       "\n"
	       "           --help     	        display this help and exit\n"
	       "           --version  	        output version information and exit\n"
	       "           --nogui  	        don't start the GUI\n"
	       "-p port    --modbus_port=port   port to use for modbus sever\n");
	exit(-1);
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
			{"nogui", no_argument, 0, 0},
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
			case 2:
				noGui = TRUE;
				break;
			}
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

	if ((argc - optind) != 0){
		if(VerifyDirectorySelected (LadderDirectory, argv[optind]))
		    loadProject = TRUE;
	}
}


int main( int   argc, char *argv[] )
{
    int modId;

    process_options(argc, argv);

    if((modId = rtapi_init("classicladder")) < 0){
	printf("rtapi_init() failed\n");
	exit(-1);
    }

    if (ClassicLadderAllocAll(modId))
    {

        if(InfosGene->LadderState == STATE_LOADING){
	    InitAllLadderDatas();
	}

        if(!noGui){
	    InitSocketServer( 0/*UseUdpMode*/, ModbusServerPort/*PortNbr*/ );
#ifdef GTK_INTERFACE
	    InitGtkWindows( argc, argv );
#endif
	}

        if(loadProject){
	    InfosGene->LadderState = STATE_LOADING;
	    LoadProjectFiles( LadderDirectory );
	    InfosGene->LadderState = STATE_RUN;
	}

#ifdef GTK_INTERFACE
        if(!noGui){
	    UpdateGtkAfterLoading( TRUE/*cCreateTimer*/ );
	    gtk_main();
	    CloseSocketServer( );
	}
#endif

    }

    ClassicLadderFreeAll(modId);
    rtapi_exit(modId);

    return 0;
}
