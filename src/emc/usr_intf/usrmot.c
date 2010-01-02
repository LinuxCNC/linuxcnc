/********************************************************************
* Description: usrmot.c
*   Linux user-level process for communicating with RT motion controller
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>		/* isspace() */
#include <unistd.h>		/* STDIN_FILENO */
#include <fcntl.h>		/* F_GETFL, O_NONBLOCK */
#include <signal.h>		/* signal(), SIGINT */
#include "_timer.h"		/* esleep() */
#include "motion.h"
#include "motion_debug.h"
#include "motion_struct.h"
#include "usrmotintf.h"		/* usrmotInit(), etc */
#include "posemath.h"
#include "emcmotcfg.h"		/* EMCMOT_ERROR_LEN,NUM */
#include "emcmotglb.h"		/* SHMEM_KEY */

/* max numbers allowed */
#define MAX_NUMBERS 8

/* microseconds to sleep between calls to getinput() */
#define SLEEP_SECS 0.100

/* takes a string, and returns 0 if all whitespace, 1 otherwise */
static int anyprintable(const char *string)
{
    int cnt = 0;
    char c;

    while (0 != (c = string[cnt++]))
	if (!isspace(c))
	    return 1;
    return 0;
}

static char *skipWhite(char *s)
{
    while (isspace(*s)) {
	s++;
    }
    return s;
}

static char *skipNonwhite(char *s)
{
    while (!isspace(*s)) {
	s++;
    }
    return s;
}

static int scanNumbers(char *string, double *numbers, int max)
{
    char *ptr = string;
    int count = 0;

    while (count < max) {
	if (1 != sscanf(ptr, "%lf", &numbers[count])) {
	    return count;
	}
	count++;
	ptr = skipNonwhite(ptr);
	ptr = skipWhite(ptr);
    }

    return count;
}

/*
  emcmotGetArgs() looks for -ini <inifile>, and sets the global
  EMCMOT_INIFILE (not to be confused with the EMC-level EMC_INIFILE).
*/

int emcmotGetArgs(int argc, char *argv[])
{
    int t;

    /* process command line args, indexing argv[] from [1] */
    for (t = 1; t < argc; t++) {
	if (!strcmp(argv[t], "-ini")) {
	    if (t == argc - 1) {
		return -1;
	    } else {
		strcpy(EMCMOT_INIFILE, argv[t + 1]);
		t++;		/* step over following arg */
	    }
	}
	/* else not recognized-- ignore */
    }

    return 0;
}

/*
  getinput() returns the number of chars read, -1 if no chars were available,
  or 0 if EOF. It doesn't block, so you can call this repeatedly and when
  it returns non-zero you have that many chars, not including the added NULL.
  */
int getinput(char *buffer, int maxchars)
{
    int flags;
    int nchars;
    int index = 0;

    /* save the flags */
    flags = fcntl(STDIN_FILENO, F_GETFL);

    /* make terminal non-blocking */
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    /* read the outstanding chars, one by one, until newline or no more */
    while (1 == (nchars = read(STDIN_FILENO, &buffer[index], 1))) {
	if (buffer[index++] == '\n') {
	    buffer[index] = 0;	/* null terminate */
	    break;
	}
    }

    /* restore the terminal */
    fcntl(STDIN_FILENO, F_SETFL, flags);

    if (nchars == -1) {
	return -1;		/* nothing new */
    }

    if (nchars == 0) {
	return 0;		/* end of file */
    }

    return index;
}

static void usrmotQuit(int sig)
{
    printf("Received %i SIGINT - Detaching from motion\n", sig);
    usrmotExit();
    exit(0);
}

/*
   syntax:  usrmot
*/
int main(int argc, char *argv[])
{
    emcmot_command_t emcmotCommand;
    emcmot_status_t emcmotStatus;
    emcmot_config_t emcmotConfig;
    emcmot_debug_t emcmotDebug;
    char input[LINELEN];
    char cmd[LINELEN];
    char errorString[EMCMOT_ERROR_LEN];
    int valid;
    int done;
    int type=0;
    int nchars;
    int printPrompt;
    int disablePrompt = 0;	/* flag for disabling prompt printing via > */
    double numbers[MAX_NUMBERS];	/* space for number input data */
    int num;			/* how many there are */
    int numNumbers = 0;		/* established number of input numbers */
    char filename[LINELEN];
    int linenum;
    FILE *fp;			/* ini file ptr */
    int lastPrint = 0;		/* flag for which status subset to print */
    int statconfigdebug = 0;	/* 0 for stat, 1 for debug, 2 for config */
    int motionId = 0;		/* motion id sent down with moves */
    int joint;			/* joint selected for command */
    int errCode;		/* returned from usrmotWrite,Read... */
    char compfile[LINELEN];	/* name of the compensation file */

    /* print the sizes first, so that even if emcmot isn't up and running we
       can use usrmot to simply print the size and fail */
    printf("sizeof(emcmot_command_t) = %lu\n", (unsigned long)sizeof(emcmot_command_t));
    printf("sizeof(emcmot_status_t) = %lu\n", (unsigned long)sizeof(emcmot_status_t));
    printf("sizeof(emcmot_config_t) = %lu\n", (unsigned long)sizeof(emcmot_config_t));
    printf("sizeof(emcmot_debug_t) = %lu\n", (unsigned long)sizeof(emcmot_debug_t));
    printf("sizeof(emcmot_error_t) = %lu\n", (unsigned long)sizeof(emcmot_error_t));
    printf("sizeof(emcmot_comp_t) = %lu\n", (unsigned long)sizeof(emcmot_comp_t));
    printf("sizeof(emcmot_joint_t) = %lu\n", (unsigned long)sizeof(emcmot_joint_t));
    printf("sizeof(emcmot_struct_t) = %lu\n", (unsigned long)sizeof(emcmot_struct_t));
    printf("sizeof(TC_STRUCT) = %lu\n", (unsigned long)sizeof(TC_STRUCT));

    /* process command line args */
    emcmotGetArgs(argc, argv);

    /* read comm parameters */
    if (-1 == usrmotIniLoad(EMCMOT_INIFILE)) {
	fprintf(stderr, "can't load ini file %s\n", EMCMOT_INIFILE);
	exit(1);
    }

    /* init comm */
    if (-1 == usrmotInit("usrmot")) {
	fprintf(stderr, "can't initialize comm interface\n");
	exit(1);
    }

    /* Now that we have connected to shared memory via rtapi register a
       SIGINT handler */
    signal(SIGINT, usrmotQuit);

    emcmotCommand.pos.a = 0.0;
    emcmotCommand.pos.b = 0.0;
    emcmotCommand.pos.c = 0.0;

    /* loop on input */
    done = 0;
    printPrompt = 1;
    while (!feof(stdin) && !done) {
	/* read errors */
	while (0 == usrmotReadEmcmotError(errorString)) {
	    printf("error: %s\n", errorString);
	}

	/* check if we need to print a prompt */
	if (printPrompt) {
	    if (!disablePrompt) {
		printf("motion> ");
		fflush(stdout);
	    }
	    printPrompt = 0;
	}

	/* get the next input line, if any */
	nchars = getinput(input, LINELEN);
	if (nchars > 0) {
	    printPrompt = 1;
	} else if (nchars == -1) {
	    /* no new input-- cycle again */
	    esleep(SLEEP_SECS);
	    continue;		/* the while(!feof(stdin)) loop */
	} else {
	    /* nchars == 0, EOF */
	    break;
	}

	/* got input-- check for a number first */
	num = scanNumbers(input, numbers, MAX_NUMBERS);
	if (num > 0) {
	    if (numNumbers == 0) {
		numNumbers = num;
	    } else if (numNumbers != num) {
		fprintf(stderr, "need %d numbers\n", numNumbers);
		/* ignore 'em */
		continue;
	    }

	    /* write out command */

	    emcmotCommand.command = EMCMOT_SET_LINE;
	    emcmotCommand.pos.tran.x = numbers[0];
	    emcmotCommand.pos.tran.y = numbers[1];
	    emcmotCommand.pos.tran.z = numbers[2];
	    emcmotCommand.id = motionId++;

	    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		fprintf(stderr, "Can't send a command to RT-task\n");
	    }
	} else {
	    /* read first arg in */
	    cmd[0] = 0;
	    sscanf(input, "%s", cmd);

	    /* match it in the big if loop */
	    if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
		printf("? or help\tprint help\n");
		printf(">\ttoggle prompt on or off\n");
		printf(";\tcomment follows\n");
		printf("quit\tquit\n");
		printf
		    ("show {flags} {limits} {scales} {times}\tshow status\n");
		printf("show debug <screen>\tshow debug\n");
		printf("show config <screen>\tshow config\n");
		printf("free\tset mode to free\n");
		printf("teleop\tset mode to teleop\n");
		printf("coord\tset mode to coordinated\n");
		printf("pause\tpause motion controller\n");
		printf("resume\tresume motion controller\n");
		printf("a\tabort motion controller\n");
		printf("scale <0..1>\tscale velocity\n");
		printf
		    ("enable | disable\tenable or disable motion controller\n");
		printf("jog <joint> + | -\tjog joint pos or neg\n");
		printf("jog <joint> + | -\tjog joint pos or neg\n");
		printf("id <num>   \tset base id for subsequent moves\n");
		printf("<x> <y> <z>\tput motion on queue\n");
		printf("load <file>\tput motions from file on queue\n");
		printf
		    ("cw <x> <y> <z> <cx> <cy> <cz> <turn>\tput CW circle on queue\n");
		printf
		    ("ccw <x> <y> <z> <cx> <cy> <cz> <turn>\tput CCW circle on queue\n");
		printf("set v <vel> | a <acc>\tset params\n");
		printf("limit <axis> <min> <max>\tset position limits\n");
		printf("ferror <axis> <value>\tset max following error\n");
		printf("live <axis 0..n-1>\tenable amp n\n");
		printf("kill <axis 0..n-1>\tkill amp n\n");
		printf("activate <axis 0..n-1>\tactivate axis n\n");
		printf("deactivate <axis 0..n-1>\tdeactivate axis n\n");
		printf("home <axis 0..n-1>\thome axis\n");
		printf("nolim             \toverride hardware limits\n");
		printf("wd on | off\tenable or disable watchdog toggle\n");
		printf
		    ("probe <x> <y> <z>\tMove toward x,y,z, if probe is tripped on the way the probe position will be updated and motion stopped.\n");
		printf
		    ("probeclear\tClear the probeTripped status flag.\n");
	    } else if (!strcmp(cmd, ">")) {
		disablePrompt = !disablePrompt;
	    } else if (!strcmp(cmd, ";")) {
		/* comment */
	    } else if (!strcmp(cmd, "quit")) {
		done = 1;
	    } else if (!strcmp(cmd, "show")) {
		if (1 == sscanf(input, "%*s %s", cmd)) {
		    statconfigdebug = 0;
		    if (!strcmp(cmd, "pids")) {
			lastPrint = 1;
			statconfigdebug = 2;	/* config */
		    } else if (!strcmp(cmd, "flags")) {
			lastPrint = 2;
		    } else if (!strcmp(cmd, "limits")) {
			lastPrint = 3;
			statconfigdebug = 2;	/* config */
		    } else if (!strcmp(cmd, "scales")) {
			lastPrint = 4;
		    } else if (!strcmp(cmd, "times")) {
			lastPrint = 5;
			statconfigdebug = 1;	/* debug */
		    } else if (!strcmp(cmd, "stat")) {
			statconfigdebug = 0;
			lastPrint =
			    strtol(strstr(input, "stat") + 4, 0, 0);
		    } else if (!strcmp(cmd, "debug")) {
			statconfigdebug = 1;
			lastPrint =
			    strtol(strstr(input, "debug") + 5, 0, 0);
		    } else if (!strcmp(cmd, "config")) {
			statconfigdebug = 2;
			lastPrint =
			    strtol(strstr(input, "config") + 6, 0, 0);
		    } else {
			/* invalid parameter */
			printf
			    ("syntax: show {pids} {flags} {limits} {scales} {times}\n");
			continue;	/* to while loop on stdin */
		    }
		} else {
		    lastPrint = 0;
		    statconfigdebug = 0;
		}

		/* print status */
		switch (statconfigdebug) {
		case 0:
		    if (0 == (errCode =
			      usrmotReadEmcmotStatus(&emcmotStatus))) {
			usrmotPrintEmcmotStatus(emcmotStatus, lastPrint);
		    } else {
			fprintf(stderr, "can't read status: %s\n",
				errCode ==
				EMCMOT_COMM_ERROR_CONNECT ?
				"EMCMOT_COMM_ERROR_CONNECT" : errCode ==
				EMCMOT_COMM_ERROR_TIMEOUT ?
				"EMCMOT_COMM_ERROR_TIMEOUT" : errCode ==
				EMCMOT_COMM_ERROR_COMMAND ?
				"EMCMOT_COMM_ERROR_COMMAND" : errCode ==
				EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				"EMCMOT_COMM_SPLIT_READ_TIMEOUT" : "?");
		    }
		    break;

		case 1:
		    if (0 ==
			(errCode = usrmotReadEmcmotDebug(&emcmotDebug))) {
			usrmotPrintEmcmotDebug(emcmotDebug, lastPrint);
		    } else {
			fprintf(stderr, "can't read debug: %s\n",
				errCode ==
				EMCMOT_COMM_ERROR_CONNECT ?
				"EMCMOT_COMM_ERROR_CONNECT" : errCode ==
				EMCMOT_COMM_ERROR_TIMEOUT ?
				"EMCMOT_COMM_ERROR_TIMEOUT" : errCode ==
				EMCMOT_COMM_ERROR_COMMAND ?
				"EMCMOT_COMM_ERROR_COMMAND" : errCode ==
				EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				"EMCMOT_COMM_SPLIT_READ_TIMEOUT" : "?");
		    }
		    break;

		case 2:
		    if (0 == (errCode =
			      usrmotReadEmcmotConfig(&emcmotConfig))) {
			usrmotPrintEmcmotConfig(emcmotConfig, lastPrint);
		    } else {
			fprintf(stderr, "can't read config: %s\n",
				errCode ==
				EMCMOT_COMM_ERROR_CONNECT ?
				"EMCMOT_COMM_ERROR_CONNECT" : errCode ==
				EMCMOT_COMM_ERROR_TIMEOUT ?
				"EMCMOT_COMM_ERROR_TIMEOUT" : errCode ==
				EMCMOT_COMM_ERROR_COMMAND ?
				"EMCMOT_COMM_ERROR_COMMAND" : errCode ==
				EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				"EMCMOT_COMM_SPLIT_READ_TIMEOUT" : "?");
		    }
		    break;
		}
	    } else if (!strcmp(cmd, "pause")) {
		emcmotCommand.command = EMCMOT_PAUSE;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "resume")) {
		emcmotCommand.command = EMCMOT_RESUME;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "a")) {
		/* set the joint field, if provided. If not provided, it will
		   default to the last one used. In coord mode it's not used
		   at all. */
		valid = 0;
		if (1 == sscanf(input, "%*s %d", &joint)) {
		    if (joint < 0 || joint >= EMCMOT_MAX_JOINTS) {
			fprintf(stderr, "bad joint %d to abort\n", joint);
		    } else {
			emcmotCommand.joint = joint;
			valid = 1;
		    }
		} else {
		    /* joint not provided, so leave last one in
		       emcmotCommand.axs alone */
		    valid = 1;
		}

		if (valid) {
		    emcmotCommand.command = EMCMOT_ABORT;

		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "scale")) {
		if (1 == sscanf(input, "%*s %lf", &emcmotCommand.scale)) {
		    emcmotCommand.command = EMCMOT_FEED_SCALE;
		    if (emcmotCommand.scale < 0.0) {
			emcmotCommand.scale = 0.0;	/* clamp it */
		    }
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    /* invalid parameter */
		    printf("syntax: scale <0..1>\n");
		}
	    } else if (!strcmp(cmd, "enable")) {
		emcmotCommand.command = EMCMOT_ENABLE;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "disable")) {
		emcmotCommand.command = EMCMOT_DISABLE;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "free")) {
		emcmotCommand.command = EMCMOT_FREE;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "teleop")) {
		emcmotCommand.command = EMCMOT_TELEOP;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "coord")) {
		emcmotCommand.command = EMCMOT_COORD;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    } else if (!strcmp(cmd, "jog")) {
		if (2 ==
		    sscanf(input, "%*s %d %s", &emcmotCommand.joint, cmd)) {
		    emcmotCommand.command = EMCMOT_JOG_CONT;
		    if (cmd[0] == '+') {
			emcmotCommand.vel = emcmotStatus.vel;
		    } else if (cmd[0] == '-') {
			emcmotCommand.vel = -emcmotStatus.vel;
		    } else {
			fprintf(stderr, "syntax: jog <joint> + | -\n");
			break;
		    }

		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    fprintf(stderr, "syntax: jog <joint> + | -\n");
		}
	    } else if (!strcmp(cmd, "id")) {
		int newId;
		if (1 == sscanf(input, "%*s %d", &newId)) {
		    motionId = newId;
		    printf("setting id to %d\n", motionId);
		} else {
		    fprintf(stderr, "syntax: id <num>\n");
		}
	    } else if (!strcmp(cmd, "load")) {
		if (1 == sscanf(input, "%*s %s", filename)) {
		    if (NULL != (fp = fopen(filename, "r"))) {
			linenum = 0;
			while (!feof(fp)) {
			    if (NULL != fgets(input, LINELEN, fp)) {
				linenum++;
				if (3 == sscanf(input, "%lf %lf %lf",
						&emcmotCommand.pos.tran.x,
						&emcmotCommand.pos.tran.y,
						&emcmotCommand.pos.tran.
						z)) {
				    printf("sending %f %f %f\n",
					   emcmotCommand.pos.tran.x,
					   emcmotCommand.pos.tran.y,
					   emcmotCommand.pos.tran.z);
				    emcmotCommand.command =
					EMCMOT_SET_LINE;
				    emcmotCommand.id = motionId++;
				    if (usrmotWriteEmcmotCommand
					(&emcmotCommand) == -1) {
					fprintf(stderr,
						"Can't send a command to RT-task\n");
				    }
				} else {
				    /* input error */
				    if (anyprintable(input)) {
					fprintf(stderr,
						"bad input on line %d of file %s\n",
						linenum, filename);
					fclose(fp);
					break;	/* out of while (! feof(fp)) */
				    }
				}
			    } else {
				/* end of input */
				fclose(fp);
				break;	/* out of while (! feof(fp)) */
			    }
			}	/* end while (! feof(fp)) */
		    } /* end if file open success */
		    else {
			fprintf(stderr, "can't open %s\n", filename);
		    }
		} /* end if correct arg to "load" */
		else {
		    fprintf(stderr, "syntax: load <filename>\n");
		}
	    } /* end match on "load" */
	    else if (!strcmp(cmd, "cw") || !strcmp(cmd, "ccw")) {
		if (7 == sscanf(input, "%*s %lf %lf %lf %lf %lf %lf %d",
				&emcmotCommand.pos.tran.x,
				&emcmotCommand.pos.tran.y,
				&emcmotCommand.pos.tran.z,
				&emcmotCommand.center.x,
				&emcmotCommand.center.y,
				&emcmotCommand.center.z,
				&emcmotCommand.turn)) {
		    emcmotCommand.command = EMCMOT_SET_CIRCLE;
		    emcmotCommand.normal.x = 0.0;
		    emcmotCommand.normal.y = 0.0;
		    if (!strcmp(cmd, "cw")) {
			emcmotCommand.normal.z = -1.0;
		    } else {
			emcmotCommand.normal.z = 1.0;
		    }
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    fprintf(stderr,
			    "syntax: cw <x> <y> <z> <cx> <cy> <cz> <turn>\n");
		}
	    } else if (!strcmp(cmd, "set")) {
		sscanf(input, "%*s %s", cmd);
/*! \todo FIXME - obsolete commands */
		if (!strcmp(cmd, "t")) {
		    fprintf(stderr, "'set t' command is obsolete\n");
/*! \todo Another #if 0 */
#if 0
		    if (1 != sscanf(input, "%*s %*s %lf",
				    &emcmotCommand.cycleTime)) {
			/* invalid parameter */
			fprintf(stderr, "bad value for cycle time\n");
		    } else {
			emcmotCommand.command = EMCMOT_SET_TRAJ_CYCLE_TIME;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    }
#endif
		} else if (!strcmp(cmd, "s")) {
		    fprintf(stderr, "'set t' command is obsolete\n");
/*! \todo Another #if 0 */
#if 0
		    if (1 != sscanf(input, "%*s %*s %lf",
				    &emcmotCommand.cycleTime)) {
			/* invalid parameter */
			fprintf(stderr,
				"bad value for interpolation rate\n");
		    } else {
			emcmotCommand.command =
			    EMCMOT_SET_SERVO_CYCLE_TIME;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    }
#endif
		} else if (!strcmp(cmd, "v")) {
		    if (1 !=
			sscanf(input, "%*s %*s %lf", &emcmotCommand.vel)) {
			/* invalid parameter */
			fprintf(stderr, "bad value for velocity\n");
		    } else {
			emcmotCommand.command = EMCMOT_SET_VEL;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    }
		} else if (!strcmp(cmd, "a")) {
		    if (1 !=
			sscanf(input, "%*s %*s %lf", &emcmotCommand.acc)) {
			/* invalid parameter */
			fprintf(stderr, "bad value for acceleration\n");
		    } else {
			emcmotCommand.command = EMCMOT_SET_ACC;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    }
		} else {
		    /* invalid parameter */
/*! \todo FIXME	    printf
			("syntax: set t <traj t> | s <servo t> | v <vel> | a <acc>\n");
*/
		    printf("syntax: set v <vel> | a <acc>\n");
		}
/*! \todo Another #if 0 */
#if 0
	    } else if (!strcmp(cmd, "oscale")) {
		if (3 != sscanf(input, "%*s %d %lf %lf",
				&emcmotCommand.joint,
				&emcmotCommand.scale,
				&emcmotCommand.offset)) {
		    printf("syntax: oscale <joint 0..n-1> <a> <b>\n");
		} else {
		    emcmotCommand.command = EMCMOT_SET_OUTPUT_SCALE;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "iscale")) {
		if (3 != sscanf(input, "%*s %d %lf %lf",
				&emcmotCommand.joint,
				&emcmotCommand.scale,
				&emcmotCommand.offset)) {
		    printf("syntax: iscale <joint 0..n-1> <a> <b>\n");
		} else {
		    emcmotCommand.command = EMCMOT_SET_INPUT_SCALE;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "pol")) {
		if (3 != sscanf(input, "%*s %d %s %d",
				&emcmotCommand.joint, cmd,
				&emcmotCommand.level)) {
		    printf
			("syntax: pol <joint 0..n-1> <enable nhl phl homedir homesw fault> <0 1>\n");
		} else {
		    valid = 1;

		    if (!strcmp(cmd, "enable")) {
			emcmotCommand.jointFlag = EMCMOT_JOINT_ENABLE_BIT;
		    } else if (!strcmp(cmd, "nhl")) {
			emcmotCommand.jointFlag =
			    EMCMOT_JOINT_MIN_HARD_LIMIT_BIT;
		    } else if (!strcmp(cmd, "phl")) {
			emcmotCommand.jointFlag =
			    EMCMOT_JOINT_MAX_HARD_LIMIT_BIT;
		    } else if (!strcmp(cmd, "homedir")) {
			emcmotCommand.jointFlag = EMCMOT_JOINT_HOMING_BIT;
		    } else if (!strcmp(cmd, "homesw")) {
			emcmotCommand.jointFlag =
			    EMCMOT_JOINT_HOME_SWITCH_BIT;
		    } else if (!strcmp(cmd, "fault")) {
			emcmotCommand.jointFlag = EMCMOT_JOINT_FAULT_BIT;
		    } else {
			valid = 0;
		    }

		    if (valid) {
			emcmotCommand.command = EMCMOT_SET_POLARITY;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    } else {
			printf
			    ("syntax: pol <joint 0..n-1> <enable nhl phl homedir homesw fault> <0 1>\n");
		    }
		}
	    } else if (!strcmp(cmd, "pid")) {
		if (1 != sscanf(input, "%*s %d", &emcmotCommand.joint) ||
		    emcmotCommand.joint < 0 ||
		    emcmotCommand.joint >= EMCMOT_MAX_JOINTS ||
		    1 != sscanf(input, "%*s %*s %s", filename)) {
		    printf("syntax: pid <n> <ini file>\n");
		} else {
		    /* load params into pid struct from inifile */
		    if (0 != pidIniLoad(&emcmotCommand.pid, filename)) {
			fprintf(stderr,
				"error loading pid params from %s\n",
				filename);
		    } else {
			emcmotCommand.command = EMCMOT_SET_PID;
			if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			    fprintf(stderr,
				    "Can't send a command to RT-task\n");
			}
		    }
		}
#endif
	    } else if (!strcmp(cmd, "limit")) {
		if (3 != sscanf(input, "%*s %d %lf %lf",
				&emcmotCommand.joint,
				&emcmotCommand.minLimit,
				&emcmotCommand.maxLimit)) {
		    printf("syntax: limit <joint> <min> <max>\n");
		} else {
		    emcmotCommand.command = EMCMOT_SET_JOINT_POSITION_LIMITS;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
/*! \todo Another #if 0 */
#if 0				/* obsolete command */
	    } else if (!strcmp(cmd, "clamp")) {
		if (3 != sscanf(input, "%*s %d %lf %lf",
				&emcmotCommand.joint,
				&emcmotCommand.minLimit,
				&emcmotCommand.maxLimit)) {
		    printf("syntax: clamp <joint> <min> <max>\n");
		} else {
		    emcmotCommand.command = EMCMOT_SET_OUTPUT_LIMITS;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
#endif
	    } else if (!strcmp(cmd, "ferror")) {
		if (2 != sscanf(input, "%*s %d %lf",
				&emcmotCommand.joint,
				&emcmotCommand.maxFerror)) {
		    printf("syntax: ferror <joint> <ferror>\n");
		} else {
		    emcmotCommand.command = EMCMOT_SET_JOINT_MAX_FERROR;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "live")) {
		if (1 != sscanf(input, "%*s %d", &emcmotCommand.joint) ||
		    emcmotCommand.joint < 0 ||
		    emcmotCommand.joint >= EMCMOT_MAX_JOINTS) {
		    printf("syntax: live <n>\n");
		} else {
		    emcmotCommand.command = EMCMOT_JOINT_ENABLE_AMPLIFIER;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "kill")) {
		if (1 != sscanf(input, "%*s %d", &emcmotCommand.joint) ||
		    emcmotCommand.joint < 0 ||
		    emcmotCommand.joint >= EMCMOT_MAX_JOINTS) {
		    printf("syntax: kill <n>\n");
		} else {
		    emcmotCommand.command = EMCMOT_JOINT_DISABLE_AMPLIFIER;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "activate")) {
		if (1 != sscanf(input, "%*s %d", &emcmotCommand.joint) ||
		    emcmotCommand.joint < 0 ||
		    emcmotCommand.joint >= EMCMOT_MAX_JOINTS) {
		    printf("syntax: activate <n>\n");
		} else {
		    emcmotCommand.command = EMCMOT_JOINT_ACTIVATE;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
	    } else if (!strcmp(cmd, "deactivate")) {
		if (1 != sscanf(input, "%*s %d", &emcmotCommand.joint) ||
		    emcmotCommand.joint < 0 ||
		    emcmotCommand.joint >= EMCMOT_MAX_JOINTS) {
		    printf("syntax: deactivate <n>\n");
		} else {
		    emcmotCommand.command = EMCMOT_JOINT_DEACTIVATE;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		}
/*! \todo Another #if 0 */
#if 0
	    } else if (!strcmp(cmd, "dac")) {
		if (2 == sscanf(input, "%*s %d %lf",
				&emcmotCommand.joint,
				&emcmotCommand.dacOut)) {
		    emcmotCommand.command = EMCMOT_DAC_OUT;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    printf("syntax: dac <num> <-10.0 .. 10.0>\n");
		}
#endif
	    } else if (!strcmp(cmd, "home")) {
		if (1 == sscanf(input, "%*s %d", &emcmotCommand.joint)) {
		    emcmotCommand.command = EMCMOT_JOINT_HOME;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    printf("syntax: home <joint>\n");
		}
	    } else if (!strcmp(cmd, "nolim")) {
		emcmotCommand.command = EMCMOT_OVERRIDE_LIMITS;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
	    }
#ifdef ENABLE_PROBING
	    else if (!strcmp(cmd, "probeclear")) {
		emcmotCommand.command = EMCMOT_CLEAR_PROBE_FLAGS;
		if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
		    fprintf(stderr, "Can't send a command to RT-task\n");
		}
/*! \todo Another #if 0 */
#if 0
	    } else if (!strcmp(cmd, "probeindex")) {
		if (1 ==
		    sscanf(input, "%*s %d", &emcmotCommand.probeIndex)) {
		    emcmotCommand.command = EMCMOT_SET_PROBE_INDEX;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    printf("syntax: probeindex <index>\n");
		}
	    } else if (!strcmp(cmd, "probepolarity")) {
		if (1 == sscanf(input, "%*s %d", &emcmotCommand.level)) {
		    emcmotCommand.command = EMCMOT_SET_PROBE_POLARITY;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    printf("syntax: probepolarity <polarity>\n");
		}
#endif
	    } else if (!strcmp(cmd, "probe")) {
		if (3 == sscanf(input, "%*s %lf %lf %lf",
				&emcmotCommand.pos.tran.x,
				&emcmotCommand.pos.tran.y,
				&emcmotCommand.pos.tran.z)) {
		    emcmotCommand.command = EMCMOT_PROBE;
		    emcmotCommand.id = motionId++;
		    if (usrmotWriteEmcmotCommand(&emcmotCommand) == -1) {
			fprintf(stderr,
				"Can't send a command to RT-task\n");
		    }
		} else {
		    printf("syntax: probe <x> <y> <z>\n");
		}
	    }
#endif				/* ENABLE_PROBING */
	    else if (!strcmp(cmd, "comp")) {
		if (1 != sscanf(input, "%*s %d", &joint)) {
		    fprintf(stderr, "syntax: comp <joint> {<file> <type>}\n");
		} else {
		    /* try a string for the compfile, else it's blank which
		       means print */
		    if (1 == sscanf(input, "%*s %*d %s %d", compfile, &type)) {
			if (0 != usrmotLoadComp(joint, compfile, type)) {
			    fprintf(stderr, "Can't load comp file %s\n",
				    compfile);
			}
		    } else {
			if (0 != usrmotPrintComp(joint)) {
			    fprintf(stderr, "Can't print comp table\n");
			}
		    }
		}
	    } else {
		if (anyprintable(input)) {
		    printf("huh? : %s", input);	/* input will have newline */
		} else {
		    /* blank line was typed */
		    /* print status */
		    switch (statconfigdebug) {
		    case 0:
/*! \todo Another #if 0 */
#if 0
			if (0 == (errCode =
				  usrmotReadEmcmotStatus(&emcmotStatus))) {
			    usrmotPrintEmcmotStatus(emcmotStatus,
						    lastPrint);
			} else {
			    fprintf(stderr, "can't read status: %s\n",
				    errCode ==
				    EMCMOT_COMM_ERROR_CONNECT ?
				    "EMCMOT_COMM_ERROR_CONNECT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_TIMEOUT ?
				    "EMCMOT_COMM_ERROR_TIMEOUT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_COMMAND ?
				    "EMCMOT_COMM_ERROR_COMMAND" : errCode
				    ==
				    EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				    "EMCMOT_COMM_SPLIT_READ_TIMEOUT" :
				    "?");
			}
#endif
			break;

		    case 1:
			if (0 == (errCode =
				  usrmotReadEmcmotDebug(&emcmotDebug))) {
			    usrmotPrintEmcmotDebug(emcmotDebug, lastPrint);
			} else {
			    fprintf(stderr, "can't read debug: %s\n",
				    errCode ==
				    EMCMOT_COMM_ERROR_CONNECT ?
				    "EMCMOT_COMM_ERROR_CONNECT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_TIMEOUT ?
				    "EMCMOT_COMM_ERROR_TIMEOUT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_COMMAND ?
				    "EMCMOT_COMM_ERROR_COMMAND" : errCode
				    ==
				    EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				    "EMCMOT_COMM_SPLIT_READ_TIMEOUT" :
				    "?");
			}
			break;

		    case 2:
			if (0 == (errCode =
				  usrmotReadEmcmotConfig(&emcmotConfig))) {
			    usrmotPrintEmcmotConfig(emcmotConfig,
						    lastPrint);
			} else {
			    fprintf(stderr, "can't read config: %s\n",
				    errCode ==
				    EMCMOT_COMM_ERROR_CONNECT ?
				    "EMCMOT_COMM_ERROR_CONNECT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_TIMEOUT ?
				    "EMCMOT_COMM_ERROR_TIMEOUT" : errCode
				    ==
				    EMCMOT_COMM_ERROR_COMMAND ?
				    "EMCMOT_COMM_ERROR_COMMAND" : errCode
				    ==
				    EMCMOT_COMM_SPLIT_READ_TIMEOUT ?
				    "EMCMOT_COMM_SPLIT_READ_TIMEOUT" :
				    "?");
			}
			break;
		    default:
			break;
		    }		/* end of switch */
		}		/* end of else blank line */
	    }			/* end of big-if input matching */

	}
	/* end of non-number input processing */
    }				/* end of while stdin */

    usrmotExit();
    exit(0);
}
