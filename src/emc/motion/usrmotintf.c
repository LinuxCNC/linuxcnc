/*
  usrmotintf.c

  Defs for interface functions (init, exit, read, write) for user
  processes which communicate with the real-time motion controller
  in emcmot.c

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) ident[] =
    "$Id$";

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>		/* memcpy() */
#include <stdlib.h>		/* sizeof() */
#include <float.h>		/* DBL_MIN */
#include "_timer.h"		/* rcslib esleep() */
#include "_shm.h"		/* rcslib shm_t, rcs_shm_open(), ... */
#include "rcs_print.hh"		/* set_rcs_print_destination(), ... */
#include "emcmot.h"		/* EMCMOT_STATUS,CMD */
#include "emcmotcfg.h"		/* EMCMOT_ERROR_NUM,LEN */
#include "emcmotglb.h"		/* SHMEM_BASE_ADDRESS, SHMEM_KEY */
#include "usrmotintf.h"		/* these decls */
#include "emcmotlog.h"		/* EMCMOT_LOG */
#include "inifile.h"		/* iniFind() */

#define READ_TIMEOUT_SEC 0	/* seconds for timeout */
#define READ_TIMEOUT_USEC 100000	/* microseconds for timeout */

#include "rtapi.h"

#ifndef MAP_FAILED		/* kernel 2.0.29 left this out */
#define MAP_FAILED ((void *) -1)
#endif

static int inited = 0;		/* flag if inited */
static int usingShmem = 0;
static shm_t *shmem = NULL;

static EMCMOT_COMMAND *emcmotCommand = 0;
static EMCMOT_STATUS *emcmotStatus = 0;
static EMCMOT_CONFIG *emcmotConfig = 0;
static EMCMOT_DEBUG *emcmotDebug = 0;
static EMCMOT_ERROR *emcmotError = 0;
static EMCMOT_IO *emcmotIo = 0;	/* added struct JE 8/21/2001 */
static EMCMOT_LOG *emcmotLog = 0;
static EMCMOT_COMP *emcmotComp[EMCMOT_MAX_AXIS] = { 0 };
static EMCMOT_STRUCT *emcmotStruct = 0;
EMCMOT_STRUCT *emcmotshmem = NULL;	// Shared memory base address.

#ifndef USE_RCS_SHM_GET_ADDR
#define rcs_shm_get_addr(x) ((x)->addr)
#endif

/* usrmotIniLoad() loads params (SHMEM_KEY, SHMEM_BASE_ADDRESS,
   COMM_TIMEOUT, COMM_WAIT) from named ini file */
int usrmotIniLoad(const char *filename)
{
    FILE *fp;
    const char *inistring;
    int saveInt;
    double saveDouble;

    /* open it */
    if (NULL == (fp = fopen(filename, "r"))) {
	rcs_print("can't find emcmot ini file %s\n", filename);
	return -1;
    }

    saveInt = SHMEM_KEY;
    if (NULL != (inistring = iniFind(fp, "SHMEM_KEY", "EMCMOT"))) {
	if (1 == sscanf(inistring, "%i", &SHMEM_KEY)) {
	    /* found it */
	} else {
	    /* found, but invalid */
	    SHMEM_KEY = saveInt;
	    rcs_print
		("invalid [EMCMOT] SHMEM_KEY in %s (%s); using default %d\n",
		filename, inistring, SHMEM_KEY);
	}
    } else {
	/* not found, using default */
	rcs_print("[EMCMOT] SHMEM_KEY not found in %s; using default %d\n",
	    filename, SHMEM_KEY);
    }

    saveInt = SHMEM_BASE_ADDRESS;
    if (NULL != (inistring = iniFind(fp, "SHMEM_BASE_ADDRESS", "EMCMOT"))) {
	if (1 == sscanf(inistring, "%lu", &SHMEM_BASE_ADDRESS)) {
	    /* found it */
	} else {
	    /* found, but invalid */
	    SHMEM_BASE_ADDRESS = saveInt;
	    rcs_print
		("invalid [EMCMOT] SHMEM_BASE_ADDRESS in %s (%s); using default %d\n",
		filename, inistring, SHMEM_BASE_ADDRESS);
	}
    } else {
	/* not found, using default */
	rcs_print
	    ("[EMCMOT] SHMEM_BASE_ADDRESS not found in %s; using default %d\n",
	    filename, SHMEM_BASE_ADDRESS);

    }

    saveDouble = EMCMOT_COMM_TIMEOUT;
    if (NULL != (inistring = iniFind(fp, "COMM_TIMEOUT", "EMCMOT"))) {
	if (1 == sscanf(inistring, "%lf", &EMCMOT_COMM_TIMEOUT)) {
	    /* found it */
	} else {
	    /* found, but invalid */
	    EMCMOT_COMM_TIMEOUT = saveDouble;
	    rcs_print
		("invalid [EMCMOT] COMM_TIMEOUT in %s (%s); using default %f\n",
		filename, inistring, EMCMOT_COMM_TIMEOUT);
	}
    } else {
	/* not found, using default */
	rcs_print("[EMCMOT] COMM_TIMEOUT not found in %s; using default %f\n",
	    filename, EMCMOT_COMM_TIMEOUT);
    }

    saveDouble = EMCMOT_COMM_WAIT;
    if (NULL != (inistring = iniFind(fp, "COMM_WAIT", "EMCMOT"))) {
	if (1 == sscanf(inistring, "%lf", &EMCMOT_COMM_WAIT)) {
	    /* found it */
	} else {
	    /* found, but invalid */
	    EMCMOT_COMM_WAIT = saveDouble;
	    rcs_print
		("invalid [EMCMOT] COMM_WAIT in %s (%s); using default %f\n",
		filename, inistring, EMCMOT_COMM_WAIT);
	}
    } else {
	/* not found, using default */
	rcs_print("[EMCMOT] COMM_WAIT not found in %s; using default %f\n",
	    filename, EMCMOT_COMM_WAIT);
    }

    fclose(fp);

    return 0;
}

int emcmot_comm_timeout_count = 0;

/* writes command from c */
int usrmotWriteEmcmotCommand(EMCMOT_COMMAND * c)
{
    EMCMOT_STATUS s;
    static int commandNum = 0;
    static unsigned char headCount = 0;
    double end;

    c->head = ++headCount;
    c->tail = c->head;
    c->commandNum = ++commandNum;

    if (usingShmem) {
	/* check for shmem still around */
	if (0 == emcmotCommand) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}
	/* end of if */
	*emcmotCommand = *c;
    } /* end of if */
    else {
	/* check for mapped mem still around */
	if (0 == emcmotCommand) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}
	/* end of if */
	*emcmotCommand = *c;
    }				/* end of else */

    /* poll for receipt of command */

    /* set timeout for comm failure, now + timeout */
    end = etime() + EMCMOT_COMM_TIMEOUT;

    /* now check to see if it got it */
    while (etime() < end) {
	/* update status */
	if (0 == usrmotReadEmcmotStatus(&s) && s.commandNumEcho == commandNum) {
	    /* now check emcmot status flag */
	    if (s.commandStatus == EMCMOT_COMMAND_OK) {
		return EMCMOT_COMM_OK;
	    } /* end of if */
	    else {
		return EMCMOT_COMM_ERROR_COMMAND;
	    }			/* end of else */
	}
	/* end of if */
	/* rcs_print("s.commandNumEcho = %d,
	   commandNum=%d\n",s.commandNumEcho, commandNum); */
    }				/* end of while loop */

    emcmot_comm_timeout_count++;
    /* rcs_print("emcmot_comm_timeout_count=%d\n",
       emcmot_comm_timeout_count); */
    return EMCMOT_COMM_ERROR_TIMEOUT;
}

int emcmot_status_split_count = 0;
int emcmot_config_split_count = 0;
int emcmot_debug_split_count = 0;

/* copies status to s */
int usrmotReadEmcmotStatus(EMCMOT_STATUS * s)
{
    if (usingShmem) {
	/* check for shmem still around */
	if (0 == emcmotStatus) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotStatus, sizeof(EMCMOT_STATUS));
    } else {
	/* check for shmem still around */
	if (0 == emcmotStatus) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotStatus, sizeof(EMCMOT_STATUS));
    }

    /* got it, now check head-tail matches */
#ifndef IGNORE_SPLIT_READS
    if (s->head != s->tail) {
#if 0
	emcmot_status_split_count++;
	if (emcmot_status_split_count > 2
	    && emcmot_status_split_count % 100 == 0) {
	    rcs_print("emcmot_status_split_count = %d\n",
		emcmot_status_split_count);
	}
#endif
	return EMCMOT_COMM_SPLIT_READ_TIMEOUT;
    }
#endif

    emcmot_status_split_count = 0;
    return EMCMOT_COMM_OK;
}

/* copies config to s */
int usrmotReadEmcmotConfig(EMCMOT_CONFIG * s)
{
    if (usingShmem) {
	/* check for shmem still around */
	if (0 == emcmotConfig) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotConfig, sizeof(EMCMOT_CONFIG));
    } else {
	/* check for shmem still around */
	if (0 == emcmotConfig) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotConfig, sizeof(EMCMOT_CONFIG));
    }

    /* got it, now check head-tail matches */
#ifndef IGNORE_SPLIT_READS
    if (s->head != s->tail) {
#if 0
	emcmot_config_split_count++;
	if (emcmot_config_split_count > 2
	    && emcmot_config_split_count % 100 == 0) {
	    rcs_print("emcmot_config_split_count = %d\n",
		emcmot_config_split_count);
	}
#endif
	return EMCMOT_COMM_SPLIT_READ_TIMEOUT;
    }
#endif

    emcmot_config_split_count = 0;
    return EMCMOT_COMM_OK;
}

/* copies debug to s */
int usrmotReadEmcmotDebug(EMCMOT_DEBUG * s)
{
    if (usingShmem) {
	/* check for shmem still around */
	if (0 == emcmotDebug) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotDebug, sizeof(EMCMOT_DEBUG));
    } else {
	/* check for shmem still around */
	if (0 == emcmotDebug) {
	    return EMCMOT_COMM_ERROR_CONNECT;
	}

	memcpy(s, emcmotDebug, sizeof(EMCMOT_DEBUG));
    }

    /* got it, now check head-tail matches */
#ifndef IGNORE_SPLIT_READS
    if (s->head != s->tail) {
#if 0
	emcmot_debug_split_count++;
	if (emcmot_debug_split_count > 2
	    && emcmot_debug_split_count % 100 == 0) {
	    rcs_print("emcmot_debug_split_count = %d\n",
		emcmot_debug_split_count);
	}
#endif
	return EMCMOT_COMM_SPLIT_READ_TIMEOUT;
    }
#endif

    emcmot_debug_split_count = 0;
    return EMCMOT_COMM_OK;
}

/* copies error to s */
int usrmotReadEmcmotError(char *e)
{
    /* check to see if ptr still around */
    if (emcmotError == 0) {
	return -1;
    }

    /* returns 0 if something, -1 if not */
    return emcmotErrorGet(emcmotError, e);
}

/*
 htostr()

 converts short int to 0-1 style string, in s. Assumes a short is 2 bytes.
*/
static int htostr(char *s, unsigned short h)
{
    int t;

    for (t = 15; t >= 0; t--) {
	s[t] = h % 2 ? '1' : '0';
	h >>= 1;
    }
    s[16] = 0;			/* null terminate */

    return 0;
}

void printEmcPose(EmcPose * pose)
{
    printf("x=%f\ty=%f\tz=%f\ta=%f\tb=%f\tc=%f",
	pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c);
}

void printTPstruct(TP_STRUCT * tp)
{
    printf("queueSize=%d\n", tp->queueSize);
    printf("cycleTime=%f\n", tp->cycleTime);
    printf("vMax=%f\n", tp->vMax);
    printf("vScale=%f\n", tp->vScale);
    printf("vRestore=%f\n", tp->vRestore);
    printf("aMax=%f\n", tp->aMax);
    printf("vLimit=%f\n", tp->vLimit);
    printf("wMax=%f\n", tp->wMax);
    printf("wDotMax=%f\n", tp->wDotMax);
    printf("nextId=%d\n", tp->nextId);
    printf("execId=%d\n", tp->execId);
    printf("termCond=%d\n", tp->termCond);
    printf("currentPos :");
    printEmcPose(&tp->currentPos);
    printf("\n");
    printf("goalPos :");
    printEmcPose(&tp->goalPos);
    printf("\n");
    printf("done=%d\n", tp->done);
    printf("depth=%d\n", tp->depth);
    printf("activeDepth=%d\n", tp->activeDepth);
    printf("aborting=%d\n", tp->aborting);
    printf("pausing=%d\n", tp->pausing);
}

void usrmotPrintEmcmotDebug(EMCMOT_DEBUG d, int which)
{
    int t;

    printf("running time: \t%f\n", d.running_time);
    switch (which) {
    case 0:
	printf("split:        \t%d\n", d.split);
	printf("teleop desiredVel: \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    d.teleop_data.desiredVel.tran.x,
	    d.teleop_data.desiredVel.tran.y,
	    d.teleop_data.desiredVel.tran.z,
	    d.teleop_data.desiredVel.a,
	    d.teleop_data.desiredVel.b, d.teleop_data.desiredVel.c);
	printf("teleop currentVel: \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    d.teleop_data.currentVel.tran.x,
	    d.teleop_data.currentVel.tran.y,
	    d.teleop_data.currentVel.tran.z,
	    d.teleop_data.currentVel.a,
	    d.teleop_data.currentVel.b, d.teleop_data.currentVel.c);
	printf("teleop desiredAccell: \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    d.teleop_data.desiredAccell.tran.x,
	    d.teleop_data.desiredAccell.tran.y,
	    d.teleop_data.desiredAccell.tran.z,
	    d.teleop_data.desiredAccell.a,
	    d.teleop_data.desiredAccell.b, d.teleop_data.desiredAccell.c);
	printf("teleop currentAccell: \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    d.teleop_data.currentAccell.tran.x,
	    d.teleop_data.currentAccell.tran.y,
	    d.teleop_data.currentAccell.tran.z,
	    d.teleop_data.currentAccell.a,
	    d.teleop_data.currentAccell.b, d.teleop_data.currentAccell.c);
	break;
	printf("\nferror:        ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.ferrorCurrent[t]);
	}
	printf("\n");

	printf("\nferror High:        ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.ferrorHighMark[t]);
	}
	printf("\n");
	break;

    case 5:
	printf("traj  m/m/a:\t%f\t%f\t%f\n", d.tMin, d.tMax, d.tAvg);
	printf("tMmxavg : sum=%f, in=%d, size=%d, index=%d\n",
	    d.tMmxavg.sum, d.tMmxavg.in, d.tMmxavg.size, d.tMmxavg.index);
#if 0
	if (d.tMmxavg.in > 0 && d.tMmxavg.size > 0) {
	    printf("tMmxavg : nums \n\t ");
	    for (t = 0; t < d.tMmxavg.in && t < d.tMmxavg.size; t++) {
		printf("%3.3e \t", d.tMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif
	printf("\n");
	printf("servo m/m/a:\t%f\t%f\t%f\n", d.sMin, d.sMax, d.sAvg);
	printf("sMmxavg : sum=%f, in=%d, size=%d, index=%d\n",
	    d.sMmxavg.sum, d.sMmxavg.in, d.sMmxavg.size, d.sMmxavg.index);

#if 0
	if (d.sMmxavg.in > 0 && d.sMmxavg.size > 0) {
	    printf("sMmxavg : nums \n\t ");
	    for (t = 0; t < d.sMmxavg.in && t < d.sMmxavg.size; t++) {
		printf("%3.3e \t", d.sMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif
	printf("\n");
	printf("(off) m/m/a:\t%f\t%f\t%f\n", d.nMin, d.nMax, d.nAvg);
	printf("nMmxavg : sum=%f, in=%d, size=%d, index=%d\n",
	    d.nMmxavg.sum, d.nMmxavg.in, d.nMmxavg.size, d.nMmxavg.index);
#if 0
	if (d.nMmxavg.in > 0 && d.nMmxavg.size > 0) {
	    printf("nMmxavg : nums \n\t ");
	    for (t = 0; t < d.nMmxavg.in && t < d.nMmxavg.size; t++) {
		printf("%3.3e \t", d.nMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif
	printf("\n");

	printf("(cycle to cycle  time) m/m/a:\t%f\t%f\t%f\n", d.yMin, d.yMax,
	    d.yAvg);
	printf("yMmxavg : sum=%f, in=%d, size=%d, index=%d\n", d.yMmxavg.sum,
	    d.yMmxavg.in, d.yMmxavg.size, d.yMmxavg.index);
#if 0
	if (d.yMmxavg.in > 0 && d.yMmxavg.size > 0) {
	    printf("nMmxavg : nums \n\t ");
	    for (t = 0; t < d.yMmxavg.in && t < d.yMmxavg.size; t++) {
		printf("%3.3e \t", d.yMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif

	printf("\n");

	printf("(frequency compute  time) m/m/a:\t%f\t%f\t%f\n", d.fMin,
	    d.fMax, d.fAvg);
	printf("fMmxavg : sum=%f, in=%d, size=%d, index=%d\n", d.fMmxavg.sum,
	    d.fMmxavg.in, d.fMmxavg.size, d.fMmxavg.index);
#if 0
	if (d.fMmxavg.in > 0 && d.fMmxavg.size > 0) {
	    printf("nMmxavg : nums \n\t ");
	    for (t = 0; t < d.fMmxavg.in && t < d.fMmxavg.size; t++) {
		printf("%3.3e \t", d.fMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif

	printf("\n");

	printf("(frequecy cycle to cycle  time) m/m/a:\t%f\t%f\t%f\n",
	    d.fyMin, d.fyMax, d.fyAvg);
	printf("fyMmxavg : sum=%f, in=%d, size=%d, index=%d\n",
	    d.fyMmxavg.sum, d.fyMmxavg.in, d.fyMmxavg.size, d.fyMmxavg.index);
#if 0
	if (d.fyMmxavg.in > 0 && d.fyMmxavg.size > 0) {
	    printf("nMmxavg : nums \n\t ");
	    for (t = 0; t < d.fyMmxavg.in && t < d.fyMmxavg.size; t++) {
		printf("%3.3e \t", d.fyMmxavgSpace[t]);
		if (t % 8 == 0 && t > 0) {
		    printf("\n\t ");
		}
	    }
	}
#endif

	printf("\n");
	break;

    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
	printf("jointPos[%d]: %f\n", which - 6, d.jointPos[(which - 6)]);
	printf("coarseJointPos[%d]: %f\n",
	    which - 6, d.coarseJointPos[(which - 6)]);
	printf("jointVel[%d]: %f\n", which - 6, d.jointVel[(which - 6)]);
	printf("rawInput[%d]: %f\n", which - 6, d.rawInput[(which - 6)]);
	printf("rawOutput[%d]: %f\n", which - 6, d.rawOutput[(which - 6)]);
	printf("bcompincr[%d]: %f\n", which - 6, d.bcompincr[(which - 6)]);
	printf("freeAxis[%d]:\n", which - 6);
	printTPstruct(&d.freeAxis[which - 6]);
	break;

    case 12:
	printf("\noldInput:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.oldInput[t]);
	}
	printf("\nrawInput:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.rawInput[t]);
	}
	printf("\ninverseInputScale:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.inverseInputScale[t]);
	}
	printf("\nbcompincr:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", d.bcompincr[t]);
	}
	printf("\n");

    default:
	break;
    }

}

void usrmotPrintEmcmotConfig(EMCMOT_CONFIG c, int which)
{
    int t;
    char m[32];

    switch (which) {
    case 0:
	printf("debug level   \t%d\n", c.debug);
	printf("traj time:    \t%f\n", c.trajCycleTime);
	printf("servo time:   \t%f\n", c.servoCycleTime);
	printf("interp rate:  \t%d\n", c.interpolationRate);
	printf("v limit:      \t%f\n", c.limitVel);
	printf("axis vlimit:  \t");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%f ", c.axisLimitVel[t]);
	}
	printf("\n");
	printf("probe index: %d\n", c.probeIndex);
	printf("probe polarity: %d\n", c.probePolarity);
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    htostr(m, c.axisPolarity[t]);
	    printf("%s ", m);
	}
	printf("\n");
	break;

    case 1:
	printf
	    ("pid:\tP\tI\tD\tFF0\tFF1\tFF2\tBCKLSH\tBIAS\tMAXI\tDEADBAND\tCYCLE TIME\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf
		("\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%f\t%f\n",
		c.pid[t].p, c.pid[t].i, c.pid[t].d, c.pid[t].ff0,
		c.pid[t].ff1, c.pid[t].ff2, c.pid[t].backlash, c.pid[t].bias,
		c.pid[t].maxError, c.pid[t].deadband, c.pid[t].cycleTime);
	}
	printf("\n");
	break;

    case 3:
	printf("pos limits:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.maxLimit[t]);
	}

	printf("\nneg limits:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.minLimit[t]);
	}

	printf("\nmax output:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.maxOutput[t]);
	}

	printf("\nmin output:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.minOutput[t]);
	}

	printf("\nmax ferror:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.maxFerror[t]);
	}
	printf("\n");

	printf("\nmin ferror:   ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.minFerror[t]);
	}
	printf("\n");

	printf("\nhome offsets:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", c.homeOffset[t]);
	}
	printf("\n");

	break;

    default:
	break;
    }

}

/* status printing function */
void usrmotPrintEmcmotStatus(EMCMOT_STATUS s, int which)
{
    int t;
    char m[32];

    switch (which) {
    case 0:
	printf("mode:         \t%s\n",
	    s.motionFlag & EMCMOT_MOTION_TELEOP_BIT ? "teleop" :
	    (s.motionFlag & EMCMOT_MOTION_COORD_BIT ? "coord" : "free")
	    );
	printf("cmd:          \t%d\n", s.commandEcho);
	printf("cmd num:      \t%d\n", s.commandNumEcho);
	printf("heartbeat:    \t%u\n", s.heartbeat);
	printf("compute time: \t%f\n", s.computeTime);
	printf("axes enabled: \t");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d", s.axisFlag[t] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0);
	}
	printf("\n");
	printf("cmd pos:      \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    s.pos.tran.x, s.pos.tran.y, s.pos.tran.z,
	    s.pos.a, s.pos.b, s.pos.c);
	printf("act pos:      \t%f\t%f\t%f\t%f\t%f\t%f\n",
	    s.actualPos.tran.x, s.actualPos.tran.y, s.actualPos.tran.z,
	    s.actualPos.a, s.actualPos.b, s.actualPos.c);
	printf("axis pos:     ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.axisPos[t]);
	}
	printf("\n");
	printf("velocity:     \t%f\n", s.vel);
	printf("accel:        \t%f\n", s.acc);
	printf("id:           \t%d\n", s.id);
	printf("depth:        \t%d\n", s.depth);
	printf("active depth: \t%d\n", s.activeDepth);
	printf("inpos:        \t%d\n",
	    s.motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0);
	printf("vscales:      \tQ: %.2f", s.qVscale);
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%d: %.2f", t, s.axVscale[t]);
	}
	printf("\n");
	printf("logging:      \t%s and %s, size %d, skipping %d, type %d\n",
	    s.logOpen == 0 ? "closed" : "open",
	    s.logStarted == 0 ? "stopped" : "started",
	    s.logSize, s.logSkip, s.logType);
	printf("homing:       \t");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d", s.axisFlag[0] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0);
	}
	printf("\n");
	printf("enabled:     \t%s\n",
	    s.motionFlag & EMCMOT_MOTION_ENABLE_BIT ? "ENABLED" : "DISABLED");
#ifdef ENABLE_PROBING
	printf("probe value: %d\n", s.probeval);
	printf("probe Tripped: %d\n", s.probeTripped);
	printf("probing: %d\n", s.probing);
	printf("probed pos:      \t%f\t%f\t%f\n",
	    s.probedPos.tran.x, s.probedPos.tran.y, s.probedPos.tran.z);
#endif
	break;

    case 2:
	htostr(m, s.motionFlag);
	printf("motion:   %s\n", m);
	printf("axes:     ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    htostr(m, s.axisFlag[t]);
	    printf("%s ", m);
	}
	printf("\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_ENABLE_BIT) != 0));
	}
	printf("enable\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_ACTIVE_BIT) != 0));
	}
	printf("active\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_INPOS_BIT) != 0));
	}
	printf("inpos\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_ERROR_BIT) != 0));
	}
	printf("error\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t",
		((s.axisFlag[t] & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT) != 0));
	}
	printf("max_soft_limit\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t",
		((s.axisFlag[t] & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT) != 0));
	}
	printf("min_soft_limit\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t",
		((s.axisFlag[t] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT) != 0));
	}
	printf("max_hard_limit\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t",
		((s.axisFlag[t] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT) != 0));
	}
	printf("min_hard_limit\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t",
		((s.axisFlag[t] & EMCMOT_AXIS_HOME_SWITCH_BIT) != 0));
	}
	printf("home_switch\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_HOMING_BIT) != 0));
	}
	printf("homing\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_HOMED_BIT) != 0));
	}
	printf("homed\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_FERROR_BIT) != 0));
	}
	printf("ferror\n");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("%d\t", ((s.axisFlag[t] & EMCMOT_AXIS_FAULT_BIT) != 0));
	}
	printf("fault\n");
	printf("\npolarity: ");
	printf("limit override: %d\n", s.overrideLimits);
	break;

    case 4:
	printf("output scales: ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.outputScale[t]);
	}

	printf("\noutput offsets:");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.outputOffset[t]);
	}

	printf("\nscaled outputs:");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.output[t]);
	}

	printf("\ninput scales:  ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.inputScale[t]);
	}

	printf("\ninput offsets: ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.inputOffset[t]);
	}

	printf("\nscaled inputs: ");
	for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	    printf("\t%f", s.input[t]);
	}
	printf("\n");
	break;

    default:
	break;
    }
}

static int module_id;
static int shmem_id;

int usrmotInit(void)
{
    int axis;

    RCS_PRINT_DESTINATION_TYPE dest;
    usingShmem = 0;

    /* try default OS shared memory first, inhibiting error messages if OS
       shared memory isn't the only kind available. */
    dest = get_rcs_print_destination();
    set_rcs_print_destination(RCS_PRINT_TO_NULL);

    shmem = rcs_shm_open(SHMEM_KEY, sizeof(EMCMOT_STRUCT), 0);
    if (NULL != shmem && NULL != rcs_shm_get_addr(shmem)) {
	/* map shmem area into local address space */
	emcmotStruct = (EMCMOT_STRUCT *) rcs_shm_get_addr(shmem);
	usingShmem = 1;
    }
    set_rcs_print_destination(dest);

    if (!usingShmem) {
	module_id = rtapi_init("usrmotintf");
	shmem_id =
	    rtapi_shmem_new(SHMEM_KEY, module_id, sizeof(EMCMOT_STRUCT));

	rtapi_shmem_getptr(shmem_id, (void **) &emcmotStruct);
	if (emcmotStruct == NULL) {
	    fprintf(stderr,
		"rtapi shmem alloc(%d (0x%X), %d (0x%X) ) failed\n",
		SHMEM_KEY, SHMEM_KEY, sizeof(EMCMOT_STRUCT),
		sizeof(EMCMOT_STRUCT));
	    return -1;
	}

    }

    /* got it */
    emcmotCommand = &(emcmotStruct->command);
    emcmotStatus = &(emcmotStruct->status);
    emcmotDebug = &(emcmotStruct->debug);
    emcmotConfig = &(emcmotStruct->config);
    emcmotError = &(emcmotStruct->error);
    emcmotIo = &(emcmotStruct->io);
    emcmotLog = &(emcmotStruct->log);
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis] = &(emcmotStruct->comp[axis]);
    }
    emcmotshmem = emcmotStruct;

    inited = 1;

    return 0;
}

int usrmotExit(void)
{
    int axis;

    if (usingShmem) {
	if (NULL != shmem) {
	    rcs_shm_close(shmem);
	    shmem = NULL;
	}
    } else {
	if (NULL != emcmotStruct) {
	    rtapi_shmem_delete(shmem_id, module_id);
	    rtapi_exit(module_id);
	}
    }

    emcmotStruct = 0;
    emcmotCommand = 0;
    emcmotStatus = 0;
    emcmotError = 0;
    emcmotIo = 0;
    emcmotLog = 0;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis] = 0;
    }
    emcmotshmem = 0;

    inited = 0;
    return 0;
}

/* added routines to control passing auxilliary I/O through to reat time section */
/* set up aux I/O byte count */
int usrmotSetIOWriteCount(unsigned short int count)
{
    if (0 == emcmotIo) {	/* pointer not initialized */
	return -1;
    }
    emcmotIo->NumWrite = count;
    return 0;
}

int usrmotSetIOReadCount(unsigned short int count)
{
    if (0 == emcmotIo) {	/* pointer not initialized */
	return -1;
    }
    emcmotIo->NumRead = count;
    return 0;
}

int usrmotWriteIO(int index, unsigned char val)
{
    if (0 == emcmotIo) {	/* pointer not initialized */
	return -1;
    }
    if (index < 0 || index >= EMCMOT_IO_SIZE) {	/* index out of range */
	return -1;
    }
    emcmotIo->OutBytes[index] = val;
    return 0;
}

unsigned char usrmotReadIO(int index)
{
    if (0 == emcmotIo) {	/* pointer not initialized */
	return -1;
    }
    if (index < 0 || index >= EMCMOT_IO_SIZE) {	/* index out of range */
	return -1;
    }
    return emcmotIo->InBytes[index];
}

/* reads the log fifo and dumps the contents to a text file */
int usrmotDumpLog(const char *filename, int include_header)
{
    FILE *fp;
    EMCMOT_LOG_STRUCT ls;
    int axis;
    int first_point = 1;
    double start_time = 0;
    int start_time_set = 0;

    /* open output log file */
    if (NULL == (fp = fopen(filename, "w"))) {
	fprintf(stderr, "can't open dump file %s\n", filename);
	return -1;
    }

    /* get the data */
    while (emcmotLog->howmany > 0) {
	if (-1 == emcmotLogGet(emcmotLog, &ls)) {
	    break;
	}

	if (include_header && first_point) {
	    first_point = 0;
	    fprintf(fp, "#!/bin/sh\n");
	    fprintf(fp,
		"exec cat $0 | head -n 16 | tail -n 12 | sed s#THIS_PLOT_FILE#$0#g |  gnuplot -persist\n");
	    fprintf(fp, "exit\n");
	    fprintf(fp, "\n");

	    fprintf(fp,
		"# This file contains both the gnuplot plotting instructions\n");
	    fprintf(fp, "# and the data logged, by EMC.\n");
	    fprintf(fp,
		"# For this to work, the data must begin after 2 blank lines,\n");
	    fprintf(fp,
		"# and the line \"exit\" must appear before the data.\n");
	    fprintf(fp, "# (lines beginning with \"#\" are ignored.)\n");
	    fprintf(fp, "print \"Running THIS_PLOT_FILE . . .\\n\"\n");
	    switch (ls.type) {

	    case EMCMOT_LOG_TYPE_AXIS_POS:
		fprintf(fp,
		    "print \"Plotting axis position values . . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"input\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"output\"\n");
		break;

	    case EMCMOT_LOG_TYPE_ALL_OUTPOS:
		fprintf(fp,
		    "print \"Plotting all output values . . .\\n\"\n");
		fprintf(fp, "set multiplot; set size 1,0.33; ");

		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    fprintf(fp,
			"set origin 0,%f; plot \"THIS_PLOT_FILE\" index 1 using 1:%d title \"output[%d]\"; ",
			axis * 0.33, axis + 2, axis);
		}
		fprintf(fp, "set nomultiplot;\n");
		break;

	    case EMCMOT_LOG_TYPE_ALL_INPOS:
		fprintf(fp, "print \"Plotting all input values . . .\\n\"\n");
		fprintf(fp, "set multiplot; set size 1,0.33; ");

		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    fprintf(fp,
			"set origin 0,%f; plot \"THIS_PLOT_FILE\" index 1 using 1:%d title \"input[%d]\"; ",
			axis * 0.33, axis + 2, axis);
		}
		fprintf(fp, "set nomultiplot;\n");
		break;

	    case EMCMOT_LOG_TYPE_CMD:
		fprintf(fp,
		    "print \"Plotting command/commandNum values . . .\\n\"\n");
		fprintf(fp,
		    "set multiplot; set size 1,0.5; plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"command\"; set origin 0,0.5; plot  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"cmdNumber\"; set nomultiplot;\n");
		break;

	    case EMCMOT_LOG_TYPE_AXIS_VEL:
		fprintf(fp,
		    "print \"Plotting axis velocity values . . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"cmdVel\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"actualVel\"\n");
		break;

	    case EMCMOT_LOG_TYPE_ALL_FERROR:
		fprintf(fp,
		    "print \"Plotting all following errors. . .\\n\"\n");
		fprintf(fp, "set multiplot; set size 1,0.33; ");

		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    fprintf(fp,
			"set origin 0,%f; plot \"THIS_PLOT_FILE\" index 1 using 1:%d title \"ferror[%d]\"; ",
			axis * 0.33, axis + 2, axis);
		}
		fprintf(fp, "set nomultiplot;\n");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_POS:
		fprintf(fp,
		    "print \"Plotting all traj positions. . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"X\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"Y\",  \"THIS_PLOT_FILE\" index 1 using 1:4 title \"Z\"\n");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_VEL:
		fprintf(fp,
		    "print \"Plotting all traj velocities . . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"X Velocity\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"Y Velocity\",  \"THIS_PLOT_FILE\" index 1 using 1:4 title \"Z Velocity\",  \"THIS_PLOT_FILE\" index 1 using 1:5 title \"Velocity Magnitude\"\n");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_ACC:
		fprintf(fp,
		    "print \"Plotting all traj accellerations . . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"X Accelleration\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"Y Accelleration\",  \"THIS_PLOT_FILE\" index 1 using 1:4 title \"Z Accelleration\",  \"THIS_PLOT_FILE\" index 1 using 1:5 title \"Accelleration Magnitude\"\n");
		break;

	    case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		fprintf(fp,
		    "print \"Plotting EMC position and voltage data . . .\\n\"\n");
		fprintf(fp,
		    "set multiplot; set size 1,0.5; plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"pos\"; set origin 0,0.5; plot  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"volt\"; set nomultiplot;\n");
		break;

	    default:
		fprintf(fp,
		    "print \"Plotting unknown data values . . .\\n\"\n");
		fprintf(fp,
		    "plot \"THIS_PLOT_FILE\" index 1 using 1:2 title \"data1\",  \"THIS_PLOT_FILE\" index 1 using 1:3 title \"data2\"\n");
		break;

	    }
	    fprintf(fp, "exit\n");
	    fprintf(fp, "\n");
	    fprintf(fp, "\n");
	    switch (ls.type) {
	    case EMCMOT_LOG_TYPE_AXIS_POS:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\n", "time", "input",
		    "output");
		break;

	    case EMCMOT_LOG_TYPE_ALL_INPOS:
		fprintf(fp, "#%17.17s\t", "time");
		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    char input_name[20];;
		    sprintf(input_name, "input[%d]", axis);
		    fprintf(fp, "%17.17s\t", input_name);
		}
		break;

	    case EMCMOT_LOG_TYPE_ALL_OUTPOS:
		fprintf(fp, "#%17.17s\t", "time");
		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    char input_name[20];;
		    sprintf(input_name, "output[%d]", axis);
		    fprintf(fp, "%17.17s\t", input_name);
		}
		fprintf(fp, "\n");
		break;

	    case EMCMOT_LOG_TYPE_CMD:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\n", "time", "command",
		    "cmdNumber");
		break;

	    case EMCMOT_LOG_TYPE_AXIS_VEL:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\n", "time", "cmdVel",
		    "actVel");
		break;

	    case EMCMOT_LOG_TYPE_ALL_FERROR:
		fprintf(fp, "#%17.17s\t", "time");
		for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		    char input_name[20];;
		    sprintf(input_name, "ferror[%d]", axis);
		    fprintf(fp, "%17.17s\t", input_name);
		}
		fprintf(fp, "\n");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_POS:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\t%17.17s\n", "time",
		    "X", "Y", "Z");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_VEL:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\t%17.17s\t%17.17s\n",
		    "time", "Xvel", "Yvel", "Zvel", "VelMag");
		break;

	    case EMCMOT_LOG_TYPE_TRAJ_ACC:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\t%17.17s\t%17.17s\n",
		    "time", "Xacc", "Yacc", "Zacc", "AccMag");
		break;

	    case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		fprintf(fp, "#%17.17s\t%17.17s\t%17.17s\n", "time", "pos",
		    "volt");
		break;

	    default:
		fprintf(fp, "#\n");
		break;
	    }
	}

	switch (ls.type) {
	case EMCMOT_LOG_TYPE_AXIS_POS:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.axisPos.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\n",
		ls.item.axisPos.time - start_time,
		ls.item.axisPos.input, ls.item.axisPos.output);
	    break;

	case EMCMOT_LOG_TYPE_ALL_INPOS:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.allInpos.time;
	    }
	    fprintf(fp, "%.10e", ls.item.allInpos.time);
	    for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		fprintf(fp, "\t%.10e", ls.item.allInpos.input[axis]);
	    }
	    fprintf(fp, "\n");
	    break;

	case EMCMOT_LOG_TYPE_ALL_OUTPOS:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.allOutpos.time;
	    }
	    fprintf(fp, "%.10e", ls.item.allOutpos.time);
	    for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		fprintf(fp, "\t%.10e", ls.item.allOutpos.output[axis]);
	    }
	    fprintf(fp, "\n");
	    break;

	case EMCMOT_LOG_TYPE_CMD:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.cmd.time;
	    }
	    fprintf(fp, "%.10e\t%d\t%d\n",
		ls.item.cmd.time - start_time,
		ls.item.cmd.command, ls.item.cmd.commandNum);
	    break;

	case EMCMOT_LOG_TYPE_AXIS_VEL:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.axisVel.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\n",
		ls.item.axisVel.time - start_time,
		ls.item.axisVel.cmdVel, ls.item.axisVel.actVel);
	    break;

	case EMCMOT_LOG_TYPE_ALL_FERROR:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.allFerror.time;
	    }
	    fprintf(fp, "%.10e", ls.item.allFerror.time);
	    for (axis = 0; axis < EMCMOT_LOG_NUM_AXES; axis++) {
		fprintf(fp, "\t%.10e", ls.item.allFerror.ferror[axis]);
	    }
	    fprintf(fp, "\n");
	    break;

	case EMCMOT_LOG_TYPE_TRAJ_POS:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.trajPos.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\t%.10e\n",
		ls.item.trajPos.time - start_time,
		ls.item.trajPos.pos.x,
		ls.item.trajPos.pos.y, ls.item.trajPos.pos.z);
	    break;

	case EMCMOT_LOG_TYPE_TRAJ_VEL:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.trajVel.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\t%.10e\t%.10e\n",
		ls.item.trajVel.time - start_time,
		ls.item.trajVel.vel.x,
		ls.item.trajVel.vel.y,
		ls.item.trajVel.vel.z, ls.item.trajVel.mag);
	    break;

	case EMCMOT_LOG_TYPE_TRAJ_ACC:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.trajAcc.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\t%.10e\t%.10e\n",
		ls.item.trajAcc.time - start_time,
		ls.item.trajAcc.acc.x,
		ls.item.trajAcc.acc.y,
		ls.item.trajAcc.acc.z, ls.item.trajAcc.mag);
	    break;

	case EMCMOT_LOG_TYPE_POS_VOLTAGE:
	    if (!start_time_set) {
		start_time_set = 1;
		start_time = ls.item.posVoltage.time;
	    }
	    fprintf(fp, "%.10e\t%.10e\t%.10e\n",
		ls.item.posVoltage.time - start_time,
		ls.item.posVoltage.pos, ls.item.posVoltage.voltage);
	    break;

	default:
	    break;
	}
    }

    /* close output file */
    fclose(fp);

    chmod(filename, 00775);
    return 0;
}

int usrmotLoadComp(int axis, const char *file)
{
    FILE *fp;
#define BUFFERLEN 256
    char buffer[BUFFERLEN];
    double nom, fwd, rev;
    int index = 0;
    int total = 0;

    /* check axis range */
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	fprintf(stderr, "axis out of range for compensation\n");
	return -1;
    }

    /* first check if comp pointer is valid */
    if (emcmotComp[axis] == 0) {
	fprintf(stderr, "compensation data structure not present\n");
	return -1;
    }

    /* open input comp file */
    if (NULL == (fp = fopen(file, "r"))) {
	fprintf(stderr, "can't open compensation file %s\n", file);
	return -1;
    }

    while (!feof(fp)) {
	if (NULL == fgets(buffer, BUFFERLEN, fp)) {
	    break;
	}
	/* 
	   expecting nominal-forward-reverse triplets, e.g., 0.000000
	   0.000000 -0.001279 0.100000 0.098742 0.051632 0.200000 0.171529
	   0.194216 */
	if (3 != sscanf(buffer, "%lf %lf %lf", &nom, &fwd, &rev)) {
	    break;
	}
	if (index >= EMCMOT_COMP_SIZE) {
	    break;
	}
	emcmotComp[axis]->nominal[index] = nom;
	emcmotComp[axis]->forward[index] = fwd;
	emcmotComp[axis]->reverse[index] = rev;
	index++;
	total++;
    }
    fclose(fp);

    if (total > 1) {
	emcmotComp[axis]->avgint = (emcmotComp[axis]->nominal[total - 1] -
	    emcmotComp[axis]->nominal[0]) / (total - 1);
    }

    /* ->total is the flag to emcmot that the comp table is valid, so only
       set this to be >1 if the data is really valid: total > 1 and avgint >
       0 */
    if (total > 1 && emcmotComp[axis]->avgint > DBL_MIN) {
	emcmotComp[axis]->total = total;
    } else {
	fprintf(stderr, "compensation file %s has too few distinct points\n",
	    file);
	return -1;
    }

    /* leave alter alone */

    return 0;
}

int usrmotAlter(int axis, double alter)
{
    /* check axis range */
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	fprintf(stderr, "axis out of range for alter\n");
	return -1;
    }

    /* first check if comp pointer is valid */
    if (emcmotComp[axis] == 0) {
	fprintf(stderr, "compensation data structure not present\n");
	return -1;
    }

    /* set alter value */
    emcmotComp[axis]->alter = alter;

    return 0;
}

int usrmotQueryAlter(int axis, double *alter)
{
    /* check axis range */
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	fprintf(stderr, "axis out of range for alter query\n");
	return -1;
    }

    /* first check if comp pointer is valid */
    if (emcmotComp[axis] == 0) {
	fprintf(stderr, "compensation data structure not present\n");
	return -1;
    }

    /* set alter value */
    *alter = emcmotComp[axis]->alter;

    return 0;
}

int usrmotPrintComp(int axis)
{
    int t;

    /* check axis range */
    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	fprintf(stderr, "axis out of range for compensation\n");
	return -1;
    }

    /* first check if comp pointer is valid */
    if (emcmotComp[axis] == 0) {
	fprintf(stderr, "compensation data structure not present\n");
	return -1;
    }

    printf("total:  %d\n", emcmotComp[axis]->total);
    printf("avgint: %f\n", emcmotComp[axis]->avgint);
    printf("alter:  %f\n", emcmotComp[axis]->alter);
    for (t = 0; t < emcmotComp[axis]->total; t++) {
	printf("%f\t%f\t%f\n",
	    emcmotComp[axis]->nominal[t],
	    emcmotComp[axis]->forward[t], emcmotComp[axis]->reverse[t]);
    }

    return 0;
}
