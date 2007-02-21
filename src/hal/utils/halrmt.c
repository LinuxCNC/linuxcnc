/** This file, 'halrmt.c', is a HAL component that provides a simple
    telnet interface to the EMC2 HAL.  It is a user space component.
*/

/** Copyright (C) 2006 Eric H. Johnson 
    Derived from work by John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>

    Other contributers:
                       Alex Joni
                       <alex_joni AT users DOT sourceforge DOT net>
 */

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#ifndef ULAPI
#error This is a user mode component only!
#endif

#include "config.h"

#define _REENTRANT

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <pthread.h>

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */
/* non-EMC related uses of halrmt may want to avoid libnml dependency */
#ifndef NO_INI
#include "inifile.hh"		/* iniFind() from libnml */
#endif

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/* These functions are used internally by this file.  The code is at
   the end of the file.  */

#define MAX_TOK 20
#define MAX_CMD_LEN 1024
#define MAX_EXPECTED_SIGS 999

static int release_HAL_mutex(void);
static int do_help_cmd(char *command);
static int unloadrt_comp(char *mod_name);
static char *data_type(int type);
static char *pin_data_dir(int dir);
static char *param_data_dir(int dir);
static char *data_arrow1(int dir);
static char *data_arrow2(int dir);
static char *data_value(int type, void *valptr);
static char *data_value2(int type, void *valptr);
static void save_comps(FILE *dst);
static void save_signals(FILE *dst);
static void save_links(FILE *dst, int arrows);
static void save_nets(FILE *dst, int arrows);
static void save_params(FILE *dst);
static void save_threads(FILE *dst);
static void print_help_general(int showR);

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

int comp_id = -1;	/* -1 means hal_init() not called yet */
int hal_flag = 0;	/* used to indicate that halrmt might have the
			   hal mutex, so the sig handler can't just
			   exit, instead it must set 'done' */
int done = 0;		/* used to break out of processing loop */
int linenumber=0;	/* used to print linenumber on errors */
int scriptmode = 0;	/* used to make output "script friendly" (suppress headers) */
int prompt_mode = 0;	/* when getting input from stdin, print a prompt */
char comp_name[HAL_NAME_LEN];	/* name for this instance of halrmt */

typedef struct {  
  int cliSock;
  char hostName[80];
  char version[8];
  int linked;
  int echo;
  int verbose;
  int enabled;
  int commMode;
  int commProt;
  char inBuf[256];
  char outBuf[4096];
  char progName[256];} connectionRecType;


int halSocket = 5006;
char errorStr[256];

int server_sockfd, client_sockfd;
socklen_t server_len, client_len;
struct sockaddr_in server_address;
struct sockaddr_in client_address;
int useSockets = 1;
int tokenIdx;
char *delims = " \n\r\0";
int connCount = -1;
int enabledConn = -1;

typedef enum {
  cmdHello, cmdSet, cmdGet, cmdQuit, cmdShutdown, cmdHelp, cmdUnknown} commandTokenType;
  
typedef enum {
  hcEcho, hcVerbose, hcEnable, hcConfig, hcCommMode, hcCommProt,
  hcComps, hcPins, hcPinVals, hcSigs, hcSigVals, hcParams, hcParamVals, hcFuncts, hcThreads,
  hcComp, hcPin, hcPinVal, hcSig, hcSigVal, hcParam, hcParamVal, hcFunct, hcThread,
  hcLoadRt, hcUnloadRt, hcLoadUsr, hcLinkps, hcLinksp, hcLinkpp, hcUnlinkp,
  hcLock, hcUnlock, hcNewSig, hcDelSig, hcSetP, hcSetS, hcAddF, hcDelF,
  hcSave, hcStart, hcStop, hcUnknown
  } halCommandType;
  
typedef enum {
  rtNoError, rtHandledNoError, rtStandardError, rtCustomError, rtCustomHandledError
  } cmdResponseType;  

char *commands[] = {"HELLO", "SET", "GET", "QUIT", "SHUTDOWN", "HELP", ""};
char *halCommands[] = {
  "ECHO", "VERBOSE", "ENABLE", "CONFIG", "COMM_MODE", "COMM_PROT",
  "COMPS", "PINS", "PINVALS", "SIGNALS", "SIGVALS", "PARAMS", "PARAMVALS", "FUNCTS", "THREADS",
  "COMP", "PIN", "PINVAL", "SIGNAL", "SIGVAL", "PARAM", "PARAMVAL", "FUNCT", "THREAD",
  "LOADRT", "UNLOADRT", "LOADUSR", "LINKPS", "LINKSP", "LINKPP", "UNLINKP",
  "LOCK", "UNLOCK", "NEWSIG", "DELSIG", "SETP", "SETS", "ADDF", "DELF",
  "SAVE", "START", "STOP", ""};

#ifndef NO_INI
    FILE *inifile = NULL;
#endif


/* signal handler */
static void quit(int sig)
{
    if ( hal_flag ) {
	/* this process might have the hal mutex, so just set the
	   'done' flag and return, exit after mutex work finishes */
	done = 1;
    } else {
	/* don't have to worry about the mutex, but if we just
	   return, we might return into the fgets() and wait 
	   all day instead of exiting.  So we exit from here. */
	if ( comp_id > 0 ) {
	    hal_exit(comp_id);
	}
	_exit(1);
    }
}

static void strupr(char *s)
{  
  int i;
  
  for (i = 0; i < (int)strlen(s); i++)
    if (s[i] > 96 && s[i] <= 'z')
      s[i] -= 32;
}

static int initSockets()
{
  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(halSocket);
  server_len = sizeof(server_address);
  bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  listen(server_sockfd, 5);
  signal(SIGCHLD, SIG_IGN);
  return 0;
}

static int sockWrite(connectionRecType *context)
{
   strcat(context->outBuf, "\r\n");
   write(context->cliSock, context->outBuf, strlen(context->outBuf));
   return 0;
}

static void sockWriteError(char *nakStr, connectionRecType *context)
{
  if (context->verbose == 1)
    sprintf(context->outBuf, "%s %s", nakStr, errorStr);
  else
    sprintf(context->outBuf, "%s", nakStr);
  sockWrite(context);
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

/* release_HAL_mutex() unconditionally releases the hal_mutex
   very useful after a program segfaults while holding the mutex
*/
static int release_HAL_mutex(void)
{
    int comp_id, mem_id, retval;
    void *mem;
    hal_data_t *hal_data;

    /* do RTAPI init */
    comp_id = rtapi_init("hal_unlocker");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ERROR: rtapi init failed\n");
        return HAL_FAIL;
    }
    /* get HAL shared memory block from RTAPI */
    mem_id = rtapi_shmem_new(HAL_KEY, comp_id, HAL_SIZE);
    if (mem_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "ERROR: could not open shared memory\n");
        rtapi_exit(comp_id);
        return HAL_FAIL;
    }
    /* get address of shared memory area */
    retval = rtapi_shmem_getptr(mem_id, &mem);
    if (retval != RTAPI_SUCCESS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "ERROR: could not access shared memory\n");
        rtapi_exit(comp_id);
        return HAL_FAIL;
    }
    /* set up internal pointers to shared mem and data structure */
    hal_data = (hal_data_t *) mem;
    /* release mutex  */
    rtapi_mutex_give(&(hal_data->mutex));
    /* release RTAPI resources */
    rtapi_shmem_delete(mem_id, comp_id);
    rtapi_exit(comp_id);
    /* done */
    return HAL_SUCCESS;

}

static int doLock(char *command, connectionRecType *context)
{
    int retval=0;
    char *nakStr = "SET LOCK NAK";

    /* if command is blank, want to lock everything */
    if (*command == '\0')
      retval = hal_set_lock(HAL_LOCK_ALL);
    else 
      if (strcmp(command, "none") == 0)
        retval = hal_set_lock(HAL_LOCK_NONE);
      else 
        if (strcmp(command, "tune") == 0)
          retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);
        else 
	  if (strcmp(command, "all") == 0)
            retval = hal_set_lock(HAL_LOCK_ALL);

    if (retval != 0) {
      sprintf(errorStr, "HAL:%d: Locking failed", linenumber);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doUnlock(char *command, connectionRecType *context)
{
    int retval=0;
    char *nakStr = "SET UNLOCK NAK";

    /* if command is blank, want to lock everything */
    if (*command == '\0')
      retval = hal_set_lock(HAL_LOCK_NONE);
    else 
      if (strcmp(command, "all") == 0)
        retval = hal_set_lock(HAL_LOCK_NONE);
      else 
        if (strcmp(command, "tune") == 0)
          retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);

    if (retval != 0) {
      sprintf(errorStr, "HAL:%d: Unlocking failed", linenumber);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doLinkpp(char *first_pin_name, char *second_pin_name, connectionRecType *context)
{
    int retval;
    hal_pin_t *first_pin, *second_pin;
    char *nakStr = "SET LINKPP NAK";

    rtapi_mutex_get(&(hal_data->mutex));
    /* check if the pins are there */
    first_pin = halpr_find_pin_by_name(first_pin_name);
    second_pin = halpr_find_pin_by_name(second_pin_name);
    if (first_pin == 0) {
	/* first pin not found*/
      rtapi_mutex_give(&(hal_data->mutex));
      sprintf(errorStr, "HAL:%d: ERROR: pin '%s' not found\n", linenumber, first_pin_name);
      sockWriteError(nakStr, context);
      return HAL_INVAL; 
      } 
    else 
      if (second_pin == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
        sprintf(errorStr, "HAL:%d: ERROR: pin '%s' not found", linenumber, second_pin_name);
        sockWriteError(nakStr, context);
	return HAL_INVAL; 
        }
    
    /* give the mutex, as the other functions use their own mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    
    /* check that both pins have the same type, 
       don't want ot create a sig, which after that won't be usefull */
    if (first_pin->type != second_pin->type) {
      sprintf(errorStr, "HAL:%d: ERROR: pins '%s' and '%s' not of the same type", 
	  linenumber, first_pin_name, second_pin_name);
      sockWriteError(nakStr, context);
      return HAL_INVAL; 
    }
	
    /* now create the signal */
    retval = hal_signal_new(first_pin_name, first_pin->type);

    if (retval == HAL_SUCCESS) {
      /* if it worked, link the pins to it */
      retval = hal_link(first_pin_name, first_pin_name);

      if ( retval == HAL_SUCCESS )
      /* if that worked, link the second pin to the new signal */
        retval = hal_link(second_pin_name, first_pin_name);
      }
    if (retval != HAL_SUCCESS) {
      sprintf(errorStr, "HAL:%d: linkpp failed", linenumber);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doLink(char *pin, char *sig, connectionRecType *context)
{
    int retval;
    char *nakStr = "SET LINKPS NAK";

    /* if sig is blank, want to unlink pin */
    if (*sig == '\0') {
	/* tell hal_link() to unlink the pin */
	sig = NULL;
    }
    /* make the link */
    retval = hal_link(pin, sig);
    if (retval != 0) {
      sprintf(errorStr, "HAL:%d: link failed", linenumber);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doNewsig(char *name, char *type, connectionRecType *context)
{
    int retval;
    char *nakStr = "SET NEWSIG NAK";

    if (strcasecmp(type, "bit") == 0)
      retval = hal_signal_new(name, HAL_BIT);
    else 
      if (strcasecmp(type, "float") == 0) 
        retval = hal_signal_new(name, HAL_FLOAT);
      else 
        if (strcasecmp(type, "u16") == 0)
          retval = hal_signal_new(name, HAL_U32);
        else 
          if (strcasecmp(type, "s32") == 0)
            retval = hal_signal_new(name, HAL_S32);
          else {
            sprintf(errorStr, "HAL:%d: Unknown signal type '%s'", 
              linenumber, type);
            sockWriteError(nakStr, context);
            retval = HAL_INVAL;
            }
    if (retval != HAL_SUCCESS) {
      sprintf(errorStr, "HAL:%d: newsig failed", linenumber);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int set_common(hal_type_t type, void *d_ptr, char *value, connectionRecType *context) {
    // This function assumes that the mutex is held
    int retval = 0;
    float fval;
    long lval;
    unsigned long ulval;
    char *cp = value;

    switch (type) {
    case HAL_BIT:
	if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0)) {
	    *(hal_bit_t *) (d_ptr) = 1;
	} else if ((strcmp("0", value) == 0)
	    || (strcasecmp("FALSE", value)) == 0) {
	    *(hal_bit_t *) (d_ptr) = 0;
	} else {
	    retval = HAL_INVAL;
	}
	break;
    case HAL_FLOAT:
	fval = strtod ( value, &cp );
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid character(s) in string */
	    retval = HAL_INVAL;
	} else {
	    *((hal_float_t *) (d_ptr)) = fval;
	}
	break;
    case HAL_S32:
	lval = strtol(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    retval = HAL_INVAL;
	} else {
	    *((hal_s32_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U32:
	ulval = strtoul(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    retval = HAL_INVAL;
	} else {
	    *((hal_u32_t *) (d_ptr)) = ulval;
	}
	break;
    default:
	/* Shouldn't get here, but just in case... */
	retval = HAL_INVAL;
    }
    return retval;
}

static int doSetp(char *name, char *value, connectionRecType *context)
{
    char *nakStr = "SET SETP NAK";
    int retval;
    hal_param_t *param;
    hal_pin_t *pin;
    hal_type_t type;
    void *d_ptr;

    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == 0) {
        pin = halpr_find_pin_by_name(name);
        if(pin == 0) {
            rtapi_mutex_give(&(hal_data->mutex));
            sprintf(errorStr,
                "HAL:%d: ERROR: parameter or pin '%s' not found\n", linenumber, name);
            sockWriteError(nakStr, context);
            return HAL_INVAL;
        } else {
            /* found it */
            type = pin->type;
            if(pin->dir == HAL_OUT) {
                rtapi_mutex_give(&(hal_data->mutex));
                sprintf(errorStr,
                    "HAL:%d: ERROR: pin '%s' is not writable\n", linenumber, name);
                sockWriteError(nakStr, context);
                return HAL_INVAL;
            }
            if(pin->signal != 0) {
                rtapi_mutex_give(&(hal_data->mutex));
                sprintf(errorStr,
                    "HAL:%d: ERROR: pin '%s' is connected to a signal\n", linenumber, name);
                sockWriteError(nakStr, context);
                return HAL_INVAL;
            }
            // d_ptr = (void*)SHMPTR(pin->dummysig);
            d_ptr = (void*)&pin->dummysig;
        }
    } else {
        /* found it */
        type = param->type;
        /* is it read only? */
        if (param->dir == HAL_RO) {
            rtapi_mutex_give(&(hal_data->mutex));
            rtapi_print_msg(RTAPI_MSG_ERR,
                "HAL:%d: ERROR: param '%s' is not writable\n", linenumber, name);
            return HAL_INVAL;
        }
        d_ptr = SHMPTR(param->data_ptr);
    }

    retval = set_common(type, d_ptr, value, context);

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0) {
	sprintf(errorStr, "HAL:%d: setp failed\n", linenumber);
        sockWriteError(nakStr, context);
    }
    return retval;
}

static int doSets(char *name, char *value, connectionRecType *context)
{
    char *nakStr = "SET SETS NAK";
    int retval;
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;

    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	sprintf(errorStr,
	    "HAL:%d: ERROR: signal '%s' not found\n", linenumber, name);
        sockWriteError(nakStr, context);
	return HAL_INVAL;
    }
    /* found it - does it have a writer? */
    if (sig->writers > 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	sprintf(errorStr,
	    "HAL:%d: ERROR: signal '%s' already has writer(s)\n", linenumber, name);
        sockWriteError(nakStr, context);
	return HAL_INVAL;
    }
    /* no writer, so we can safely set it */
    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    retval = set_common(type, d_ptr, value, context);
    rtapi_mutex_give(&(hal_data->mutex));
    if (retval != 0) {
      sprintf(errorStr, "HAL:%d: sets failed\n", linenumber);
      sockWriteError(nakStr, context);
    }
    return retval;
}

static int doAddf(char *name, char *thread, char *parm, connectionRecType *context)
{
    int retval;
    char *nakStr = "SET ADDF NAK";
    
    if (parm[0] == '\0')
	    /* no - add function at end of thread */
      retval = hal_add_funct_to_thread(name, thread, -1);
    else
      retval = hal_add_funct_to_thread(name, thread, atoi(parm));
    if (retval != 0) {
      /* print fail message */
      sprintf(errorStr, "HAL:%d: Unable to add function '%s' to thread '%s'", linenumber, name, thread);
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doDelf(char *name, char *thread, connectionRecType *context)
{
    int retval;
    char *nakStr = "SET DELF NAK";
    
    retval = hal_del_funct_from_thread(name, thread);
    if (retval != 0) {
      /* print success message */
      sprintf(errorStr, "Failed to remove function '%s' from thread '%s'", name, thread);
      sockWriteError(nakStr, context);
    }
    return retval;
}

static int doStart(connectionRecType *context)
{
    int retval;
    char *nakStr = "SET START NAK";

    retval = hal_start_threads();
    if (retval != 0) {
	    /* print success message */
      sprintf(errorStr, "Failed to start realtime threads");
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doStop(connectionRecType *context)
{
    int retval;
    char *nakStr = "SET STOP NAK";

    retval = hal_stop_threads();
    if (retval != 0) {
	    /* print success message */
      sprintf(errorStr, "Unable to stop realtime threads");
      sockWriteError(nakStr, context);
      }
    return retval;
}

static int doLoadRt(char *mod_name, char *args[], connectionRecType *context)
{
    /* note: these are static so that the various searches can
       be skipped for subsequent commands */
    static char *rtmod_dir = EMC2_BIN_DIR "/rtapi_app";
    struct stat stat_buf;
    char mod_path[MAX_CMD_LEN+1];
    char *cp1;
    char *argv[MAX_TOK+1];
    char arg_string[MAX_CMD_LEN+1];
    int n, m, retval, status;
    hal_comp_t *comp;
    pid_t pid;
    char *nakStr = "SET LOADRT NAK";

    if (hal_get_lock()&HAL_LOCK_LOAD) {
      sprintf(errorStr,  "HAL:%d: ERROR: HAL is locked, loading of modules is not permitted", 
	linenumber);
      sockWriteError(nakStr, context);
      return HAL_PERM;
      }
    if ((strlen(rtmod_dir)+strlen(mod_name)+5) > MAX_CMD_LEN) {
      sprintf(errorStr, "HAL:%d: ERROR: Module path too long", linenumber);
      sockWriteError(nakStr, context);
      return -1;
      }
    /* make full module name '<path>/<name>.o' */
    strcpy (mod_path, rtmod_dir);
    strcat (mod_path, "/");
    strcat (mod_path, mod_name);
    strcat (mod_path, MODULE_EXT);
    /* is there a file with that name? */
    if (stat(mod_path, &stat_buf) != 0 ) {
        /* can't find it */
      sprintf(errorStr, "HAL:%d: ERROR: Can't find module '%s' in %s", 
	linenumber, mod_name, rtmod_dir);
      sockWriteError(nakStr, context);
      return -1;
      }
    /* now we need to fork, and then exec insmod.... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if (pid < 0) {
      sprintf(errorStr, "HAL:%d: ERROR: loadrt fork() failed", linenumber);
      sockWriteError(nakStr, context);
	/* reconnect to the HAL shmem area */
      comp_id = hal_init(comp_name);
      if (comp_id < 0) {
        sprintf(errorStr, "halrmt: hal_init() failed after fork: %d", comp_id);
        sockWriteError(nakStr, context);
        exit(-1);
	}
      hal_ready(comp_id);
      return -1;
      }
    if (pid == 0) {
	/* this is the child process - prepare to exec() insmod */
      argv[0] = EMC2_BIN_DIR "/emc_module_helper";
      argv[1] = "insert";
      argv[2] = mod_path;
      /* loop thru remaining arguments */
      n = 0;
      m = 3;
      while (args[n][0] != '\0') {
        argv[m++] = args[n++];
        }
      /* add a NULL to terminate the argv array */
      argv[m] = NULL;
      /* print debugging info if "very verbose" (-V) */
      rtapi_print_msg(RTAPI_MSG_DBG, "%s %s %s ", argv[0], argv[1], argv[2] );
      n = 3;
      while (argv[n] != NULL) {
        rtapi_print_msg(RTAPI_MSG_DBG, "%s ", argv[n++]);
        }
      rtapi_print_msg(RTAPI_MSG_DBG, "\n");
        /* call execv() to invoke insmod */
      execv(argv[0], argv);
      /* should never get here */
      sprintf(errorStr, "HAL:%d: ERROR: execv(%s) failed", linenumber, argv[0] );
      sockWriteError(nakStr, context);
      exit(1);
      }
    /* this is the parent process, wait for child to end */
    retval = waitpid (pid, &status, 0);
    /* reconnect to the HAL shmem area */
    comp_id = hal_init(comp_name);
    if (comp_id < 0) {
      sprintf(errorStr, "halcmd: hal_init() failed after loadrt: %d", comp_id);
      sockWriteError(nakStr, context);
      exit(-1);
      }
    hal_ready(comp_id);
    /* check result of waitpid() */
    if (retval < 0) {
      sprintf(errorStr, "HAL:%d: ERROR: waitpid(%d) failed", linenumber, pid);
      sockWriteError(nakStr, context);
      return -1;
      }
    if (WIFEXITED(status) == 0) {
      sprintf(errorStr, "HAL:%d: ERROR: child did not exit normally", linenumber);
      sockWriteError(nakStr, context);
      return -1;
      }
    retval = WEXITSTATUS(status);
    if (retval != 0) {
      sprintf(errorStr, "HAL:%d: ERROR: insmod failed, returned %d", linenumber, retval );
      sockWriteError(nakStr, context);
      return -1;
      }
    /* make the args that were passed to the module into a single string */
    n = 0;
    arg_string[0] = '\0';
    while ( args[n][0] != '\0' ) {
      strncat(arg_string, args[n++], MAX_CMD_LEN);
      strncat(arg_string, " ", MAX_CMD_LEN);
      }
    /* allocate HAL shmem for the string */
    cp1 = hal_malloc(strlen(arg_string) + 1);
    if (cp1 == NULL) {
      sprintf(errorStr, "HAL:%d: ERROR: failed to allocate memory for module args", linenumber);
      sockWriteError(nakStr, context);
      return -1;
      }
    /* copy string to shmem */
    strcpy (cp1, arg_string);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search component list for the newly loaded component */
    comp = halpr_find_comp_by_name(mod_name);
    if (comp == 0) {
      rtapi_mutex_give(&(hal_data->mutex));
      sprintf(errorStr, "HAL:%d: ERROR: module '%s' not loaded", linenumber, mod_name);
      sockWriteError(nakStr, context);
      return HAL_INVAL;
      }
    /* link args to comp struct */
    comp->insmod_args = SHMOFF(cp1);
    rtapi_mutex_give(&(hal_data->mutex));
    /* print success message */
    return 0;
}

static int doDelsig(char *mod_name, connectionRecType *context)
{
    int next, retval, retval1, n;
    hal_sig_t *sig;
    char sigs[MAX_EXPECTED_SIGS][HAL_NAME_LEN+1];
    char *nakStr = "SET DELSIG NAK";

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) != 0 ) {
      retval = hal_signal_delete(mod_name);
      return retval;
      }
    else {
      /* build a list of signal(s) to delete */
      n = 0;
      rtapi_mutex_get(&(hal_data->mutex));

      next = hal_data->sig_list_ptr;
      while (next != 0) {
        sig = SHMPTR(next);
        /* we want to unload this signal, remember it's name */
        if (n < ( MAX_EXPECTED_SIGS - 1))
          strncpy(sigs[n++], sig->name, HAL_NAME_LEN );
        next = sig->next_ptr;
	}
      rtapi_mutex_give(&(hal_data->mutex));
      sigs[n][0] = '\0';

      if ((sigs[0][0] == '\0')) {
        /* desired signals not found */
        sprintf(errorStr, "HAL:%d: ERROR: no signals found to be deleted", linenumber);
        sockWriteError(nakStr, context);
        return -1;
        }
      /* we now have a list of components, unload them */
      n = 0;
      retval1 = 0;
      while (sigs[n][0] != '\0') {
        retval = hal_signal_delete(sigs[n]);
        /* check for fatal error */
	if (retval < -1)
          return retval;
        /* check for other error */
        if (retval != 0)
          retval1 = retval;
        n++;
	}
      }
    return retval1;
}

static int doUnloadRt(char *mod_name)
{
    int next, retval, retval1, n, all;
    hal_comp_t *comp;
    char comps[64][HAL_NAME_LEN+1];

    /* check for "all" */
    if (strcmp(mod_name, "all" ) == 0)
      all = 1;
    else
      all = 0;
    /* build a list of component(s) to unload */
    n = 0;
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
	    /* found a realtime component */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		/* we want to unload this component, remember it's name */
		if ( n < 63 ) {
		    strncpy(comps[n++], comp->name, HAL_NAME_LEN );
		}
	    }
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    /* mark end of list */
    comps[n][0] = '\0';
    if ( !all && ( comps[0][0] == '\0' )) {
	/* desired component not found */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: component '%s' is not loaded\n", linenumber, mod_name);
	return -1;
    }
    /* we now have a list of components, unload them */
    n = 0;
    retval1 = 0;
    while ( comps[n][0] != '\0' ) {
	retval = unloadrt_comp(comps[n++]);
	/* check for fatal error */
	if ( retval < -1 ) {
	    return retval;
	}
	/* check for other error */
	if ( retval != 0 ) {
	    retval1 = retval;
	}
    }
    if (retval1 != HAL_SUCCESS) {
	rtapi_print_msg(RTAPI_MSG_ERR, "HAL:%d: ERROR: unloadrt failed\n", linenumber);
    }
    return retval1;
}

static int unloadrt_comp(char *mod_name)
{
    int retval, status;
    char *argv[3];
    pid_t pid;

    /* now we need to fork, and then exec rmmod.... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if ( pid < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: unloadrt fork() failed\n", linenumber);
	/* reconnect to the HAL shmem area */
	comp_id = hal_init(comp_name);
	if (comp_id < 0) {
	    fprintf(stderr, "halrmt: hal_init() failed after fork: %d\n",
                    comp_id);
	    exit(-1);
	}
        hal_ready(comp_id);
	return -1;
    }
    if ( pid == 0 ) {
	/* this is the child process - prepare to exec() rmmod */
	argv[0] = EMC2_BIN_DIR "/emc_module_helper";
	argv[1] = "remove";
	argv[2] = mod_name;
	/* add a NULL to terminate the argv array */
	argv[3] = NULL;
	/* print debugging info if "very verbose" (-V) */
	rtapi_print_msg(RTAPI_MSG_DBG, "%s %s %s\n", argv[0], argv[1], argv[2] );
	/* call execv() to invoke rmmod */
	execv(argv[0], argv);
	/* should never get here */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: execv(%s) failed\n", linenumber, argv[0] );
	exit(1);
    }
    /* this is the parent process, wait for child to end */
    retval = waitpid ( pid, &status, 0 );
    /* reconnect to the HAL shmem area */
    comp_id = hal_init(comp_name);
    if (comp_id < 0) {
	fprintf(stderr, "halrmt: hal_init() failed after unloadrt: %d\n",
                comp_id );
	exit(-1);
    }
    hal_ready(comp_id);
    /* check result of waitpid() */
    if ( retval < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: waitpid(%d) failed\n", linenumber, pid);
	return -1;
    }
    if ( WIFEXITED(status) == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: child did not exit normally\n", linenumber);
	return -1;
    }
    retval = WEXITSTATUS(status);
    if ( retval != 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: rmmod failed, returned %d\n", linenumber, retval);
	return -1;
    }
    /* print success message */
    rtapi_print_msg(RTAPI_MSG_INFO, "Realtime module '%s' unloaded\n",
	mod_name);
    return 0;
}

static int doLoadUsr(char *args[])
{
    int wait_flag, wait_comp_flag, name_flag, ignore_flag;
    char *prog_name, *new_comp_name;
    char prog_path[MAX_CMD_LEN+1];
    char *cp1, *cp2, *envpath;
    struct stat stat_buf;
    char *argv[MAX_TOK+1];
    int n, m, retval, status;
    pid_t pid;

    if (hal_get_lock()&HAL_LOCK_LOAD) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: HAL is locked, loading of programs is not permitted\n", linenumber);
	return HAL_PERM;
    }
    /* check for options (-w, -i, and/or -r) */
    wait_flag = 0;
    wait_comp_flag = 0;
    name_flag = 0;
    ignore_flag = 0;
    prog_name = NULL;
    while ( **args == '-' ) {
	/* this argument contains option(s) */
	cp1 = *args;
	cp1++;
	while ( *cp1 != '\0' ) {
	    if ( *cp1 == 'w' ) {
		wait_flag = 1;
            } else if ( *cp1 == 'W' ) {
                wait_comp_flag = 1;
	    } else if ( *cp1 == 'i' ) {
		ignore_flag = 1;
	    } else if ( *cp1 == 'n' ) {
		name_flag = 1;
	    } else {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL:%d: ERROR: unknown loadusr option '-%c'\n", linenumber, *cp1);
		return HAL_INVAL;
	    }
	    cp1++;
	}
	/* move to next arg */
	args++;
    }
    /* get program and component name */
    if(name_flag) {
        new_comp_name = *args++;
    prog_name = *args++;
    } else {
        new_comp_name = prog_name = *args++;
    }
    /* need to find path to a program matching "prog_name" */
    prog_path[0] = '\0';
    if ( prog_path[0] == '\0' ) {
	/* try the name by itself */
	strncpy (prog_path, prog_name, MAX_CMD_LEN);
	rtapi_print_msg(RTAPI_MSG_DBG, "Trying '%s'\n", prog_path);
	if ( stat(prog_path, &stat_buf) != 0 ) {
	    /* no luck, clear prog_path to indicate failure */
	    prog_path[0] = '\0';
	}
    }
    if ( prog_path[0] == '\0' ) {
	/* no luck yet, try the emc2/bin directory where
	   the halrmt executable is located */
	n = readlink("/proc/self/exe", prog_path, MAX_CMD_LEN-10);
	if ( n > 0 ) {
	    prog_path[n] = '\0';
	    /* have path to executabie, find last '/' */
	    cp2 = "";
	    cp1 = prog_path;
	    while ( *cp1 != '\0' ) {
		if ( *cp1 == '/' ) {
		    cp2 = cp1;
		}
		cp1++;
	    }
	    if ( *cp2 == '/' ) {
		/* chop "halrmt" from end of path */
		*(++cp2) = '\0';
		/* append the program name */
		strncat(prog_path, prog_name, MAX_CMD_LEN-strlen(prog_path));
		/* and try it */
		rtapi_print_msg(RTAPI_MSG_DBG, "Trying '%s'\n", prog_path);
		if ( stat(prog_path, &stat_buf) != 0 ) {
		    /* no luck, clear prog_path to indicate failure */
		    prog_path[0] = '\0';
		}
	    }
	}
    }
   if ( prog_path[0] == '\0' ) {
	/* no luck yet, try the user's PATH */
	envpath = getenv("PATH");
	if ( envpath != NULL ) {
	    while ( *envpath != '\0' ) {
		/* copy a single directory from the PATH env variable */
		n = 0;
		while ( (*envpath != ':') && (*envpath != '\0') && (n < MAX_CMD_LEN)) {
		    prog_path[n++] = *envpath++;
		}
		/* append '/' and program name */
		if ( n < MAX_CMD_LEN ) {
		    prog_path[n++] = '/';
		}
		cp1 = prog_name;
		while ((*cp1 != '\0') && ( n < MAX_CMD_LEN)) {
		    prog_path[n++] = *cp1++;
		}
		prog_path[n] = '\0';
		rtapi_print_msg(RTAPI_MSG_DBG, "Trying '%s'\n", prog_path);
		if ( stat(prog_path, &stat_buf) != 0 ) {
		    /* no luck, clear prog_path to indicate failure */
		    prog_path[0] = '\0';
		    /* and get ready to try the next directory */
		    if ( *envpath == ':' ) {
		        envpath++;
		    }
		} else {
		    /* success, break out of loop */
		    break;
		}
	    } 
	}
    }
    if ( prog_path[0] == '\0' ) {
	/* still can't find a program to run */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: Can't find program '%s'\n", linenumber, prog_name);
	return -1;
    }

    /* now we need to fork, and then exec the program.... */
    /* disconnect from the HAL shmem area before forking */
    hal_exit(comp_id);
    comp_id = 0;
    /* now the fork() */
    pid = fork();
    if ( pid < 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: loadusr fork() failed\n", linenumber);
	/* reconnect to the HAL shmem area */
	comp_id = hal_init(comp_name);
	if (comp_id < 0) {
	    fprintf(stderr, "halrmt: hal_init() failed after fork: %d\n",
                    comp_id);
	    exit(-1);
	}
        hal_ready(comp_id);
	return -1;
    }
    if ( pid == 0 ) {
	/* this is the child process - prepare to exec() the program */
	argv[0] = prog_name;
	/* loop thru remaining arguments */
	n = 0;
	m = 1;
	while ( args[n][0] != '\0' ) {
	    argv[m++] = args[n++];
	}
	/* add a NULL to terminate the argv array */
	argv[m] = NULL;
	/* print debugging info if "very verbose" (-V) */
	rtapi_print_msg(RTAPI_MSG_DBG, "%s ", argv[0] );
	n = 1;
	while ( argv[n] != NULL ) {
	    rtapi_print_msg(RTAPI_MSG_DBG, "%s ", argv[n++] );
	}
	rtapi_print_msg(RTAPI_MSG_DBG, "\n" );
	/* call execv() to invoke the program */
	execv(prog_path, argv);
	/* should never get here */
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL:%d: ERROR: execv(%s) failed: %s\n", linenumber, prog_name,
            strerror(errno));
	exit(1);
    }
    /* this is the parent process, reconnect to the HAL shmem area */
    comp_id = hal_init(comp_name);
    if (comp_id < 0) {
	fprintf(stderr, "halrmt: hal_init() failed after loadusr: %d\n",
                comp_id);
	exit(-1);
    }
    hal_ready(comp_id);
    if ( wait_comp_flag ) {
        int ready = 0, count=0;
        int next;
        while(!ready) {
            struct timespec ts = {0, 10 * 1000 * 1000}; // 10ms
            nanosleep(&ts, NULL);
            retval = waitpid( pid, &status, WNOHANG );
            if(retval != 0) goto wait_common;

            rtapi_mutex_get(&(hal_data->mutex));
            next = hal_data->comp_list_ptr;
            while(next) {
                hal_comp_t *comp = SHMPTR(next);
                next = comp->next_ptr;
                if(strcmp(comp->name, new_comp_name) == 0 && comp->ready) {
                    ready = 1;
                    break;
                }
            }
            rtapi_mutex_give(&(hal_data->mutex));

            count++;
            if(count == 100) {
                fprintf(stderr, "Waiting for component '%s' to become ready.",
                        new_comp_name);
                fflush(stderr);
            } else if(count > 100 && count % 10 == 0) {
                fprintf(stderr, ".");
                fflush(stderr);
            }
        }
        if (count >= 100) {
	    fprintf(stderr, "\n");
	}
	rtapi_print_msg(RTAPI_MSG_INFO, "Component '%s' ready\n", new_comp_name);
    }
    if ( wait_flag ) {
	/* wait for child process to complete */
	retval = waitpid ( pid, &status, 0 );
	/* check result of waitpid() */
wait_common:
	if ( retval < 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL:%d: ERROR: waitpid(%d) failed\n", linenumber, pid);
	    return -1;
	}
	if ( WIFEXITED(status) == 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"HAL:%d: ERROR: program '%s' did not exit normally\n", linenumber, prog_name );
	    return -1;
	}
	if ( ignore_flag == 0 ) {
	    retval = WEXITSTATUS(status);
	    if ( retval != 0 ) {
		rtapi_print_msg(RTAPI_MSG_ERR,
		    "HAL:%d: ERROR: program '%s' failed, returned %d\n", linenumber, prog_name, retval );
		return -1;
	    }
	}
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO, "Program '%s' finished\n", prog_name);
    } else {
	/* print success message */
	rtapi_print_msg(RTAPI_MSG_INFO, "Program '%s' started\n", prog_name);
    }
    return 0;
}

static void getCompInfo(char *pattern, connectionRecType *context)
{
    int next, len;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->comp_list_ptr;
    while (next != 0) {
      comp = SHMPTR(next);
      if (strncmp(pattern, comp->name, len) == 0) {
        sprintf(context->outBuf, "COMP %s %02d %s", comp->name, comp->comp_id, (comp->type ? "RT  " : "User"));
	sockWrite(context);
	}
      next = comp->next_ptr;
      }
    rtapi_mutex_give(&(hal_data->mutex));
}


static void getPinInfo(char *pattern, int valuesOnly, connectionRecType *context)
{
    int next, len;
    hal_pin_t *pin;
    hal_comp_t *comp;
    hal_sig_t *sig;
    void *dptr;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->pin_list_ptr;
    while (next != 0) {
      pin = SHMPTR(next);
      if (strncmp(pattern, pin->name, len) == 0) {
        comp = SHMPTR(pin->owner_ptr);
        if (pin->signal != 0) {
	  sig = SHMPTR(pin->signal);
	  dptr = SHMPTR(sig->data_ptr);
	  } 
        else {
	  sig = 0;
	  dptr = &(pin->dummysig);
	  }
	if (valuesOnly == 0)  
          sprintf(context->outBuf, "PIN %s %s %02d %s %s",
	    pin->name,
	    data_value2((int) pin->type, dptr),
	    comp->comp_id,
	    data_type((int) pin->type),
	    pin_data_dir((int) pin->dir));
	else
	  sprintf(context->outBuf, "PINVAL %s %s",
	    pin->name,
	    data_value2((int) pin->type, dptr));
	sockWrite(context);
	} 
      next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}


static void getSigInfo(char *pattern, int valuesOnly, connectionRecType *context)
{
    int next, len;
    hal_sig_t *sig;
    void *dptr;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->sig_list_ptr;
    while (next != 0) {
      sig = SHMPTR(next);
      if (strncmp(pattern, sig->name, len) == 0) {
        dptr = SHMPTR(sig->data_ptr);
        if (valuesOnly == 0)
          sprintf(context->outBuf, "SIGNAL %s  %s  %s", 
            sig->name,
	    data_value((int) sig->type, dptr),
	    data_type((int) sig->type));
	else
	  sprintf(context->outBuf, "SIGNALVAL %s %s", sig->name, data_value((int) sig->type, dptr));
	sockWrite(context);
        }
      next = sig->next_ptr;
      }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void getParamInfo(char *pattern, int valuesOnly, connectionRecType *context)
{
    int next, len;
    hal_param_t *param;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->param_list_ptr;
    while (next != 0) {
      param = SHMPTR(next);
      if ( strncmp(pattern, param->name, len) == 0 ) {
        comp = SHMPTR(param->owner_ptr);
	if (valuesOnly == 0)
          sprintf(context->outBuf, "PARAM %s %s %02d %s %s",
	    param->name,
            data_value((int) param->type, SHMPTR(param->data_ptr)),
            comp->comp_id, 
	    data_type((int) param->type),
            param_data_dir((int) param->dir));
	else
	  sprintf(context->outBuf, "PARAMVAL %s %s",
	    param->name,
	    data_value((int) param->type, SHMPTR(param->data_ptr)));
	sockWrite(context);
	}
      next = param->next_ptr;
      }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void getFunctInfo(char *pattern, connectionRecType *context)
{
    int next, len;
    hal_funct_t *fptr;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next = hal_data->funct_list_ptr;
    while (next != 0) {
      fptr = SHMPTR(next);
      if (strncmp(pattern, fptr->name, len) == 0) {
        comp = SHMPTR(fptr->owner_ptr);
	sprintf(context->outBuf, "FUNCT %s %02d %08X %08X %s %3d", 
	  fptr->name, 
	  comp->comp_id, 
	  (unsigned int)fptr->funct,
	  (unsigned int)fptr->arg,
	  (fptr->uses_fp ? "YES" : "NO"),
	  fptr->users);
	sockWrite(context);
	}
      next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void getThreadInfo(char *pattern, connectionRecType *context)
{
    int next_thread, len, n;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    rtapi_mutex_get(&(hal_data->mutex));
    len = strlen(pattern);
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
      tptr = SHMPTR(next_thread);
      if (strncmp(pattern, tptr->name, len) == 0) {
        sprintf(context->outBuf, "THREAD %s %11d %s %d %d",
	  tptr->name, 
	  (unsigned int)tptr->period, 
	  (tptr->uses_fp ? "YES" : "NO "),  
	  (unsigned int)tptr->runtime, 
	  (unsigned int)tptr->maxtime);
	sockWrite(context);
        list_root = &(tptr->funct_list);
        list_entry = list_next(list_root);
        n = 1;
        while (list_entry != list_root) {
          /* print the function info */
          fentry = (hal_funct_entry_t *) list_entry;
          funct = SHMPTR(fentry->funct_ptr);
          /* scriptmode only uses one line per thread, which contains: 
             thread period, FP flag, name, then all functs separated by spaces  */
          sprintf(context->outBuf, "SUBTHREAD %s %2d", funct->name, n);
	  sockWrite(context);
          n++;
          list_entry = list_next(list_entry);
	  }
	}
      next_thread = tptr->next_ptr;
      }
    rtapi_mutex_give(&(hal_data->mutex));
}


/* Switch function for pin/sig/param type for the print_*_list functions */
static char *data_type(int type)
{
    char *type_str;

    switch (type) {
    case HAL_BIT:
	type_str = "bit  ";
	break;
    case HAL_FLOAT:
	type_str = "float";
	break;
    case HAL_S32:
	type_str = "s32  ";
	break;
    case HAL_U32:
	type_str = "u32  ";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	type_str = "undef";
    }
    return type_str;
}

/* Switch function for pin direction for the print_*_list functions  */
static char *pin_data_dir(int dir)
{
    char *pin_dir;

    switch (dir) {
    case HAL_IN:
	pin_dir = "IN ";
	break;
    case HAL_OUT:
	pin_dir = "OUT";
	break;
    case HAL_IO:
	pin_dir = "I/O";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	pin_dir = "???";
    }
    return pin_dir;
}

/* Switch function for param direction for the print_*_list functions  */
static char *param_data_dir(int dir)
{
    char *param_dir;

    switch (dir) {
    case HAL_RO:
	param_dir = "RO";
	break;
    case HAL_RW:
	param_dir = "RW";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	param_dir = "??";
    }
    return param_dir;
}

/* Switch function for arrow direction for the print_*_list functions  */
static char *data_arrow1(int dir)
{
    char *arrow;

    switch (dir) {
    case HAL_IN:
	arrow = "<==";
	break;
    case HAL_OUT:
	arrow = "==>";
	break;
    case HAL_IO:
	arrow = "<=>";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	arrow = "???";
    }
    return arrow;
}

/* Switch function for arrow direction for the print_*_list functions  */
static char *data_arrow2(int dir)
{
    char *arrow;

    switch (dir) {
    case HAL_IN:
	arrow = "==>";
	break;
    case HAL_OUT:
	arrow = "<==";
	break;
    case HAL_IO:
	arrow = "<=>";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	arrow = "???";
    }
    return arrow;
}

/* Switch function to return var value for the print_*_list functions  */
/* the value is printed in a fixed width field */
static char *data_value(int type, void *valptr)
{
    char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "       FALSE";
	else
	    value_str = "        TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%12.7g", (double)*((hal_float_t *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, "  %10ld", (long)*((hal_u32_t *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "    %08lX", (unsigned long)*((hal_u32_t *) valptr));
	value_str = buf;
	break;
    default:
	/* Shouldn't get here, but just in case... */
	value_str = "   undef    ";
    }
    return value_str;
}

/* Switch function to return var value in string form  */
/* the value is printed as a packed string (no whitespace */
static char *data_value2(int type, void *valptr)
{
    char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "FALSE";
	else
	    value_str = "TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%.7g", (double)*((hal_float_t *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, "%ld", (long)*((hal_s32_t *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "%ld", (unsigned long)*((hal_u32_t *) valptr));
	value_str = buf;
	break;
    default:
	/* Shouldn't get here, but just in case... */
	value_str = "unknown_type";
    }
    return value_str;
}

static int doSave(char *type, char *filename, connectionRecType *context)
{
    FILE *dst;
    char *nakStr = "SET SAVE NAK";

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
      }
    if (*filename == '\0' )
      dst = stdout;
    else {
      dst = fopen(filename, "w" );
      if (dst == NULL) {
	sprintf(errorStr, "HAL:%d: Can't open 'save' destination '%s'", linenumber, filename);
	sockWriteError(nakStr, context);
	return -1;
	}
      }
    if (*type == '\0')
      type = "all";
    if (strcmp(type, "all" ) == 0) {
	/* save everything */
	save_comps(dst);
	save_signals(dst);
	save_links(dst, 0);
	save_params(dst);
	save_threads(dst);
      } 
    else 
      if (strcmp(type, "comp") == 0)
	save_comps(dst);
      else 
        if (strcmp(type, "sig") == 0)
	  save_signals(dst);
        else 
	  if (strcmp(type, "link") == 0)
	    save_links(dst, 0);
          else 
	    if (strcmp(type, "linka") == 0)
	      save_links(dst, 1);
            else 
	      if (strcmp(type, "net") == 0)
	        save_nets(dst, 0);
              else 
	        if (strcmp(type, "neta") == 0)
	          save_nets(dst, 1);
                else 
		  if (strcmp(type, "param") == 0)
	            save_params(dst);
                  else 
		    if (strcmp(type, "thread") == 0)
	              save_threads(dst);
                    else {
	              sprintf(errorStr, "HAL:%d: Unknown 'save' type '%s'", linenumber, type);
		      sockWriteError(nakStr, context);
	              return -1;
                      }
    if (*filename != '\0' )
      fclose(dst);
    return 0;
}

static void save_comps(FILE *dst)
{
    int next;
    hal_comp_t *comp;

    fprintf(dst, "# components\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
	    /* only print realtime components */
	    if ( comp->insmod_args == 0 ) {
		fprintf(dst, "#loadrt %s  (not loaded by loadrt, no args saved)\n", comp->name);
	    } else {
		fprintf(dst, "loadrt %s %s\n", comp->name,
		    (char *)SHMPTR(comp->insmod_args));
	    }
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_signals(FILE *dst)
{
    int next;
    hal_sig_t *sig;

    fprintf(dst, "# signals\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	fprintf(dst, "newsig %s %s\n", sig->name, data_type((int) sig->type));
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_links(FILE *dst, int arrow)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    char *arrow_str;

    fprintf(dst, "# links\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if (pin->signal != 0) {
	    sig = SHMPTR(pin->signal);
	    if (arrow != 0) {
		arrow_str = data_arrow1((int) pin->dir);
	    } else {
		arrow_str = "\0";
	    }
	    fprintf(dst, "linkps %s %s %s\n", pin->name, arrow_str, sig->name);
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_nets(FILE *dst, int arrow)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    char *arrow_str;

    fprintf(dst, "# nets\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	fprintf(dst, "newsig %s %s\n", sig->name, data_type((int) sig->type));
	pin = halpr_find_pin_by_sig(sig, 0);
	while (pin != 0) {
	    if (arrow != 0) {
		arrow_str = data_arrow2((int) pin->dir);
	    } else {
		arrow_str = "\0";
	    }
	    fprintf(dst, "linksp %s %s %s\n", sig->name, arrow_str, pin->name);
	    pin = halpr_find_pin_by_sig(sig, pin);
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_params(FILE *dst)
{
    int next;
    hal_param_t *param;

    fprintf(dst, "# parameter values\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if (param->dir != HAL_RO) {
	    /* param is writable, save it's value */
	    fprintf(dst, "setp %s %s\n", param->name,
		data_value((int) param->type, SHMPTR(param->data_ptr)));
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_threads(FILE *dst)
{
    int next_thread;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    fprintf(dst, "# realtime thread/function links\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	list_root = &(tptr->funct_list);
	list_entry = list_next(list_root);
	while (list_entry != list_root) {
	    /* print the function info */
	    fentry = (hal_funct_entry_t *) list_entry;
	    funct = SHMPTR(fentry->funct_ptr);
	    fprintf(dst, "addf %s %s\n", funct->name, tptr->name);
	    list_entry = list_next(list_entry);
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static int do_help_cmd(char *command)
{

    if (strcmp(command, "help") == 0) {
	printf("If you need help to use 'help', then I can't help you.\n");
    } else if (strcmp(command, "loadrt") == 0) {
	printf("loadrt modname [modarg(s)]\n");
	printf("  Loads realtime HAL module 'modname', passing 'modargs'\n");
	printf("  to the module.\n");
    } else if (strcmp(command, "unloadrt") == 0) {
	printf("unloadrt modname\n");
	printf("  Unloads realtime HAL module 'modname'.  If 'modname'\n");
	printf("  is 'all', unloads all realtime modules.\n");
    } else if (strcmp(command, "loadusr") == 0) {
	printf("loadusr [options] progname [progarg(s)]\n");
	printf("  Starts user space program 'progname', passing\n");
	printf("  'progargs' to it.  Options are:\n");
	printf("  -w  wait for program to finish\n");
	printf("  -i  ignore program return value (use with -w)\n");
    } else if ((strcmp(command, "linksp") == 0) || (strcmp(command,"linkps") == 0)) {
	printf("linkps pinname [arrow] signame\n");
	printf("linksp signame [arrow] pinname\n");
	printf("  Links pin 'pinname' to signal 'signame'.  Both forms do\n");
	printf("  the same thing.  Use whichever makes sense.  The optional\n");
	printf("  'arrow' can be '==>', '<==', or '<=>' and is ignored.  It\n");
	printf("  can be used in files to show the direction of data flow,\n");
	printf("  but don't use arrows on the command line.\n");
    } else if (strcmp(command, "linkpp") == 0) {
	printf("linkpp firstpin secondpin\n");
	printf("  Creates a signal with the name of the first pin,\n");	printf("  then links both pins to the signal. \n");
    }else if (strcmp(command, "unlinkp") == 0) {
	printf("unlinkp pinname\n");
	printf("  Unlinks pin 'pinname' if it is linked to any signal.\n");
    } else if (strcmp(command, "lock") == 0) {
	printf("lock [all|tune|none]\n");
	printf("  Locks HAL to some degree.\n");
	printf("  none - no locking done.\n");
	printf("  tune - some tuning is possible (setp & such).\n");
	printf("  all  - HAL completely locked.\n");
    } else if (strcmp(command, "unlock") == 0) {
	printf("unlock [all|tune]\n");
	printf("  Unlocks HAL to some degree.\n");
	printf("  tune - some tuning is possible (setp & such).\n");
	printf("  all  - HAL completely unlocked.\n");
    } else if (strcmp(command, "newsig") == 0) {
	printf("newsig signame type\n");
	printf("  Creates a new signal called 'signame'.  Type is 'bit',\n");
	printf("  'float', 'u8', 's8', 'u16', 's16', 'u32', or 's32'.\n");
    } else if (strcmp(command, "delsig") == 0) {
	printf("delsig signame\n");
	printf("  Deletes signal 'signame'.  If 'signame is 'all',\n");
	printf("  deletes all signals\n");
    } else if (strcmp(command, "setp") == 0) {
	printf("setp paramname value\n");
	printf("paramname = value\n");
	printf("  Sets parameter 'paramname' to 'value' (if writable).\n");
	printf("  'setp' and '=' work the same, don't use '=' on the\n");
	printf("  command line.  'value' may be a constant such as 1.234\n");
	printf("  or TRUE, or a reference to an environment variable,\n");
#ifdef NO_INI
	printf("  using the syntax '$name'./n");
#else
	printf("  using the syntax '$name'.  If option -i was given,\n");
	printf("  'value' may also be a reference to an ini file entry\n");
	printf("  using the syntax '[section]name'.\n");
#endif
    } else if (strcmp(command, "sets") == 0) {
	printf("sets signame value\n");
	printf("  Sets signal 'signame' to 'value' (if sig has no writers).\n");
    } else if (strcmp(command, "getp") == 0) {
	printf("getp paramname\n");
	printf("  Gets the value of parameter 'paramname'.\n");
    } else if (strcmp(command, "gets") == 0) {
	printf("gets signame\n");
	printf("  Gets the value of signal 'signame'.\n");
    } else if (strcmp(command, "addf") == 0) {
	printf("addf functname threadname [position]\n");
	printf("  Adds function 'functname' to thread 'threadname'.  If\n");
	printf("  'position' is specified, adds the function to that spot\n");
	printf("  in the thread, otherwise adds it to the end.  Negative\n");
	printf("  'position' means position with respect to the end of the\n");
	printf("  thread.  For example '1' is start of thread, '-1' is the\n");
	printf("  end of the thread, '-3' is third from the end.\n");
    } else if (strcmp(command, "delf") == 0) {
	printf("delf functname threadname\n");
	printf("  Removes function 'functname' from thread 'threadname'.\n");
    } else if (strcmp(command, "show") == 0) {
	printf("show [type] [pattern]\n");
	printf("  Prints info about HAL items of the specified type.\n");
	printf("  'type' is 'comp', 'pin', 'sig', 'param', 'funct',\n");
	printf("  'thread', or 'all'.  If 'type' is omitted, it assumes\n");
	printf("  'all' with no pattern.  If 'pattern' is specified\n");
	printf("  it prints only those items whose names match the\n");
	printf("  pattern (no fancy regular expressions, just a simple\n");
	printf("  match: 'foo' matches 'foo', 'foobar' and 'foot' but\n");
	printf("  not 'fo' or 'frobz' or 'ffoo').\n");
    } else if (strcmp(command, "list") == 0) {
	printf("list type [pattern]\n");
	printf("  Prints the names of HAL items of the specified type.\n");
	printf("  'type' is 'comp', 'pin', 'sig', 'param', 'funct', or\n");
	printf("  'thread'.  If 'pattern' is specified it prints only\n");
	printf("  those names that match the pattern (no fancy regular\n");
	printf("  expressions, just a simple match: 'foo' matches 'foo',\n");
	printf("  'foobar' and 'foot' but not 'fo' or 'frobz' or 'ffoo').\n");
	printf("  Names are printed on a single line, space separated.\n");
    } else if (strcmp(command, "status") == 0) {
	printf("status [type]\n");
	printf("  Prints status info about HAL.\n");
	printf("  'type' is 'lock', 'mem', or 'all'. \n");
	printf("  If 'type' is omitted, it assumes\n");
	printf("  'all'.\n");
    } else if (strcmp(command, "save") == 0) {
	printf("save [type] [filename]\n");
	printf("  Prints HAL state to 'filename' (or stdout), as a series\n");
	printf("  of HAL commands.  State can later be restored by using\n");
	printf("  \"halrmt -f filename\".\n");
	printf("  Type can be 'comp', 'sig', 'link[a]', 'net[a]', 'param',\n");
	printf("  or 'thread'.  ('linka' and 'neta' show arrows for pin\n");
	printf("  direction.)  If 'type' is omitted or 'all', does the\n");
	printf("  equivalent of 'comp', 'sig', 'link', 'param', and 'thread'.\n");
    } else if (strcmp(command, "start") == 0) {
	printf("start\n");
	printf("  Starts all realtime threads.\n");
    } else if (strcmp(command, "stop") == 0) {
	printf("stop\n");
	printf("  Stops all realtime threads.\n");
    } else if (strcmp(command, "quit") == 0) {
	printf("quit\n");
	printf("  Stop processing input and terminate halrmt (when\n");
	printf("  reading from a file or stdin).\n");
    } else if (strcmp(command, "exit") == 0) {
	printf("exit\n");
	printf("  Stop processing input and terminate halrmt (when\n");
	printf("  reading from a file or stdin).\n");
    } else {
	printf("No help for unknown command '%s'\n", command);
    }
    return 0;
}

static void print_help_general(int showR)
{
    printf("\nUsage:   halrmt [options] [cmd [args]]\n\n");
    printf("options:\n\n");
    printf("  -f [filename]  Read commands from 'filename', not command\n");
    printf("                 line.  If no filename, read from stdin.\n");
#ifndef NO_INI
    printf("  -i filename    Open .ini file 'filename', allow commands\n");
    printf("                 to get their values from ini file.\n");
#endif
    printf("  -k             Keep going after failed command.  Default\n");
    printf("                 is to exit if any command fails. (Useful with -f)\n");
    printf("  -q             Quiet - print errors only (default).\n");
    printf("  -Q             Very quiet - print nothing.\n");
    if (showR != 0) {
    printf("  -R             Release mutex (for crash recovery only).\n");
    }
    printf("  -s             Script friendly - don't print headers on output.\n");
    printf("  -v             Verbose - print result of every command.\n");
    printf("  -V             Very verbose - print lots of junk.\n");
    printf("  -h             Help - print this help screen and exit.\n\n");
    printf("commands:\n\n");
    printf("  loadrt, unloadrt, loadusr, lock, unlock, linkps, linksp, linkpp,\n");
    printf("  unlinkp, newsig, delsig, setp, getp, sets, gets, addf, delf, show,\n");
    printf("  list, save, status, start, stop, quit, exit\n");
    printf("  help           Lists all commands with short descriptions\n");
    printf("  help command   Prints detailed help for 'command'\n\n");
}

static halCommandType lookupHalCommand(char *s)
{
  halCommandType i = hcEcho;
  int temp;
  
  while (i < hcUnknown) {
    if (strcmp(halCommands[i], s) == 0) return i;
//    (int)i += 1;
      temp = i;
      temp++;
      i = (halCommandType) temp;
    }
  return i;
}
 
 static int commandHello(connectionRecType *context)
{
  char *pch;
  char *password = "EMC";
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  if (strcmp(pch, password) != 0) return -1;
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  strcpy(context->hostName, pch);  
  pch = strtok(NULL, delims);
  if (pch == NULL) return -1;
  context->linked = 1;    
  strcpy(context->version, pch);
  printf("Connected to %s\n\r", context->hostName);
  return 0;
}

static cmdResponseType getEcho(char *s, connectionRecType *context)
{
  char *pEchoStr = "ECHO %s";
  
  if (context->echo == 1) sprintf(context->outBuf, pEchoStr, "ON");
  else sprintf(context->outBuf, pEchoStr, "OFF");
  return rtNoError;
}

static cmdResponseType getVerbose(char *s, connectionRecType *context)
{
  char *pVerboseStr = "VERBOSE %s";
  
  if (context->verbose == 1) sprintf(context->outBuf, pVerboseStr, "ON");
  else sprintf(context->outBuf, pVerboseStr, "OFF");
  return rtNoError;
}

static cmdResponseType getEnable(char *s, connectionRecType *context)
{
  char *pEnableStr = "ENABLE %s";
  
  if (context->cliSock == enabledConn) sprintf(context->outBuf, pEnableStr, "ON");
  else sprintf(context->outBuf, pEnableStr, "OFF");
  return rtNoError;
}

static cmdResponseType getConfig(char *s, connectionRecType *context)
{
  char *pConfigStr = "CONFIG";

  strcpy(context->outBuf, pConfigStr);
  return rtNoError;
}

static cmdResponseType getCommMode(char *s, connectionRecType *context)
{
  char *pCommModeStr = "COMM_MODE %s";
  
  switch (context->commMode) {
    case 0: sprintf(context->outBuf, pCommModeStr, "ASCII"); break;
    case 1: sprintf(context->outBuf, pCommModeStr, "BINARY"); break;
    }
  return rtNoError;
}

static cmdResponseType getCommProt(char *s, connectionRecType *context)
{
  char *pCommProtStr = "COMM_PROT %s";
  
  sprintf(context->outBuf, pCommProtStr, context->version);
  return rtNoError;
}

static cmdResponseType getComps(char *s, connectionRecType *context)
{
  getCompInfo("", context);
  return rtHandledNoError;
}

static cmdResponseType getPins(char *s, connectionRecType *context)
{
  getPinInfo("", 0, context);
  return rtHandledNoError;
}

static cmdResponseType getPinVals(char *s, connectionRecType *context)

{
  getPinInfo("", 1, context);
  return rtHandledNoError;
}

static cmdResponseType getSignals(char *s, connectionRecType *context)
{
  getSigInfo("", 0, context);
  return rtHandledNoError;
}

static cmdResponseType getSignalVals(char *s, connectionRecType *context)
{
  getSigInfo("", 1, context);
  return rtHandledNoError;
}

static cmdResponseType getParams(char *s, connectionRecType *context)
{
  getParamInfo("", 0, context);
  return rtHandledNoError;
}

static cmdResponseType getParamVals(char *s, connectionRecType *context)
{
  getParamInfo("", 1, context);
  return rtHandledNoError;
}

static cmdResponseType getFuncts(char *s, connectionRecType *context)
{
  getFunctInfo("", context);
  return rtHandledNoError;
}

static cmdResponseType getThreads(char *s, connectionRecType *context)
{
  getThreadInfo("", context);
  return rtHandledNoError;
}

static cmdResponseType getComp(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getCompInfo(s, context);
  return rtHandledNoError;
}

static cmdResponseType getPin(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getPinInfo(s, 0, context);
  return rtHandledNoError;
}

static cmdResponseType getPinVal(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getPinInfo(s, 1, context);
  return rtHandledNoError;
}

static cmdResponseType getSignal(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getSigInfo(s, 0, context);
  return rtHandledNoError;
}

static cmdResponseType getSignalVal(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getSigInfo(s, 1, context);
  return rtHandledNoError;
}

static cmdResponseType getParam(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getParamInfo(s, 0, context);
  return rtHandledNoError;
}

static cmdResponseType getParamVal(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getParamInfo(s, 1, context);
  return rtHandledNoError;
}

static cmdResponseType getFunct(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getFunctInfo(s, context);
  return rtHandledNoError;
}

static cmdResponseType getThread(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  getThreadInfo(s, context);
  return rtHandledNoError;
}

int commandGet(connectionRecType *context)
{
  static char *setNakStr = "GET NAK\r\n";
  static char *setCmdNakStr = "GET %s NAK\r\n";
  halCommandType cmd;
  char *pch;
  cmdResponseType ret = rtNoError;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) {
    write(context->cliSock, setNakStr, strlen(setNakStr));
    return 0;
    }
  strupr(pch);
  cmd = lookupHalCommand(pch);
  switch (cmd) {
    case hcEcho: ret = getEcho(pch, context); break;
    case hcVerbose: ret = getVerbose(pch, context); break;
    case hcEnable: ret = getEnable(pch, context); break;
    case hcConfig: ret = getConfig(pch, context); break;
    case hcCommMode: ret = getCommMode(pch, context); break;
    case hcCommProt: ret = getCommProt(pch, context); break;
    case hcComps: ret = getComps(pch, context); break;
    case hcPins: ret = getPins(pch, context); break;
    case hcPinVals: ret = getPinVals(pch, context); break;
    case hcSigs: ret = getSignals(pch, context); break;
    case hcSigVals: ret = getSignalVals(pch, context); break;
    case hcParams: ret = getParams(pch, context); break;
    case hcParamVals: ret = getParamVals(pch, context); break;
    case hcFuncts: ret = getFuncts(pch, context); break;
    case hcThreads: ret = getThreads(pch, context); break;
    case hcComp: ret = getComp(strtok(NULL, delims), context); break;
    case hcPin: ret = getPin(strtok(NULL, delims), context); break;
    case hcPinVal: ret = getPinVal(strtok(NULL, delims), context); break;
    case hcSig: ret = getSignal(strtok(NULL, delims), context); break;
    case hcSigVal: ret = getSignalVal(strtok(NULL, delims), context); break;
    case hcParam: ret = getParam(strtok(NULL, delims), context); break;
    case hcParamVal: ret = getParamVal(strtok(NULL, delims), context); break;
    case hcFunct: ret = getFunct(strtok(NULL, delims), context); break;
    case hcThread: ret = getThread(strtok(NULL, delims), context); break;
    case hcLoadRt: ;
    case hcUnloadRt: ; 
    case hcLoadUsr: ; 
    case hcLinkps: ;
    case hcLinksp: ; 
    case hcLinkpp: ;
    case hcUnlinkp: ;
    case hcLock: ;
    case hcUnlock: ;
    case hcNewSig: ;
    case hcDelSig: ;
    case hcSetP: ;
    case hcSetS: ;
    case hcAddF: ;
    case hcDelF: ;
    case hcSave: ;
    case hcStart: ;
    case hcStop: ;
    case hcUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError: // Standard ok response, just write value in buffer
      sockWrite(context);
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError: // Standard error response
      sprintf(context->outBuf, setCmdNakStr, pch); 
      sockWrite(context);
      break;
    case rtCustomError: // Custom error response entered in buffer
      sockWrite(context);
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

static int checkOnOff(char *s)
{
  static char *onStr = "ON";
  static char *offStr = "OFF";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, onStr) == 0) return 0;
  if (strcmp(s, offStr) == 0) return 1;
  return -1;
}

static int checkBinaryASCII(char *s)
{
  static char *binaryStr = "BINARY";
  static char *ASCIIStr = "ASCII";
  
  if (s == NULL) return -1;
  strupr(s);
  if (strcmp(s, ASCIIStr) == 0) return 0;
  if (strcmp(s, binaryStr) == 0) return 1;
  return -1;
}

static cmdResponseType setEcho(char *s, connectionRecType *context)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: context->echo = 1; break;
     case 1: context->echo = 0;
     }
   return rtNoError;
}

static cmdResponseType setVerbose(char *s, connectionRecType *context)
{
   
   switch (checkOnOff(s)) {
     case -1: return rtStandardError;
     case 0: context->verbose = 1; break;
     case 1: context->verbose = 0;
     }
   return rtNoError;
}

static cmdResponseType setEnable(char *s, connectionRecType *context)
{
   char *enablePWD = "EMCTOO";
  
   switch (checkOnOff(s)) {
     case -1: 
       if (strcmp(s, enablePWD) == 0) {
//         enable = true;
	 enabledConn = context->cliSock;
//	 printf("Enabled Context = %d This context = %d\n", enabledConn, connId);
         return rtNoError;
	 }
       else return rtStandardError;
     case 1: 
//       enable = false;
       enabledConn = -1;
     }
   return rtNoError;
}

static cmdResponseType setConfig(char *s, connectionRecType *context)
{
  return rtNoError;
}

static cmdResponseType setCommMode(char *s, connectionRecType *context)
{
  int ret;
  
  ret = checkBinaryASCII(s);
  if (ret == -1) return rtStandardError;
  context->commMode = ret;
  return rtNoError;
}

static cmdResponseType setCommProt(char *s, connectionRecType *context)
{
  char *pVersion;
  
  pVersion = strtok(NULL, delims);
  if (pVersion == NULL) return rtStandardError;
  strcpy(context->version, pVersion);
  return rtNoError;
}

static cmdResponseType setLoadRt(char *s, connectionRecType *context)
{
  char *pch;
  
  pch = strtok(NULL, delims);
  if (doLoadRt(pch, &s, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setUnloadRt(char *s, connectionRecType *context)
{
  if (doUnloadRt(s) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setLoadUsr(char *s, connectionRecType *context)
{
  if (doLoadUsr(&s) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setLinkps(char *p, char *s, connectionRecType *context)
{
  if ((p == NULL) || (s == NULL)) return rtStandardError;
  if (doLink(p, s, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setLinksp(char *p, char *s, connectionRecType *context)
{
  if ((p == NULL) || (s == NULL)) return rtStandardError;
  if (doLink(s, p, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setLinkpp(char *p1, char *p2, connectionRecType *context)
{
  if ((p1 == NULL) || (p2 == NULL)) return rtStandardError;
  if (doLinkpp(p1, p2, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setUnlink(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  if (doLink(s, "\0", context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setLock(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  if (doLock(s, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setUnlock(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  if (doUnlock(s, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setNewsig(char *s, char *t, connectionRecType *context)
{
  if ((s == NULL) || (t == NULL)) return rtStandardError;
  if (doNewsig(s, t, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setDelsig(char *s, connectionRecType *context)
{
  if (s == NULL) return rtStandardError;
  if (doDelsig(s, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setSetp(char *s, char *p, connectionRecType *context)
{
  if ((s == NULL) || (p == NULL)) return rtStandardError;
  if (doSetp(s, p, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setSets(char *s, char *p, connectionRecType *context)
{
  if ((s == NULL) || (p == NULL)) return rtStandardError;
  if (doSets(s, p, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setAddf(char *s, char *t, char *p, connectionRecType *context)
{
  if ((s == NULL) || (t == NULL)) return rtStandardError;
  if (doAddf(s, t, p, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setDelf(char *s, char *t, connectionRecType *context)
{
  if ((s == NULL) || (t == NULL)) return rtStandardError;
  if (doDelf(s, t, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setSave(char *type, char *fileName, connectionRecType *context)
{
  if (doSave(type, fileName, context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setStart(connectionRecType *context)
{
  if (doStart(context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

static cmdResponseType setStop(connectionRecType *context)
{
  if (doStop(context) == 0)
    return rtNoError;
  else
    return rtCustomHandledError;
}

#define MAX_TOKENS 5

int commandSet(connectionRecType *context)
{
  static char *setNakStr = "SET NAK\n\r";
  static char *setCmdNakStr = "SET %s NAK\n\r";
  static char *ackStr = "SET %s ACK\n\r";
  halCommandType cmd;
  char *tokens[MAX_TOKENS];
  int i;
  char *pch;
  char *pcmd;
  cmdResponseType ret = rtNoError;
  
  pcmd = strtok(NULL, delims);
  if (pcmd == NULL) {
    write(context->cliSock, setNakStr, strlen(setNakStr));
    return 0;
    }
  strupr(pcmd);
  cmd = lookupHalCommand(pcmd);
  if ((cmd >= hcCommProt) && (context->cliSock != enabledConn)) {
    sprintf(context->outBuf, setCmdNakStr, pcmd);
    write(context->cliSock, context->outBuf, strlen(context->outBuf));
    return 0;
    }
  pch = strtok(NULL, delims);
  i = 0;
  while (pch != NULL) {
    tokens[i] = pch;
    i++;
    pch = strtok(NULL, delims);
    }
  switch (cmd) {
    case hcEcho: ret = setEcho(tokens[0], context); break;
    case hcVerbose: ret = setVerbose(tokens[0], context); break;
    case hcEnable: ret = setEnable(tokens[0], context); break;
    case hcConfig: ret = setConfig(tokens[0], context); break;
    case hcCommMode: ret = setCommMode(tokens[0], context); break;
    case hcCommProt: ret = setCommProt(tokens[0], context); break;
    case hcComps: break;
    case hcPins: break;
    case hcPinVals: break;
    case hcSigs: break;
    case hcSigVals: break;
    case hcParams: break;
    case hcParamVals: break;
    case hcFuncts: break;
    case hcThreads: break;
    case hcComp: break;
    case hcPin: break;
    case hcPinVal: break;
    case hcSig: break;
    case hcSigVal: break;
    case hcParam: break;
    case hcParamVal: break;
    case hcFunct: break;
    case hcThread: break;
    case hcLoadRt: ret = setLoadRt(tokens[0], context); break;
    case hcUnloadRt: ret = setUnloadRt(tokens[0], context); break;
    case hcLoadUsr: ret = setLoadUsr(tokens[0], context); break;
    case hcLinkps: setLinkps(tokens[0], tokens[1], context); break;
    case hcLinksp: setLinksp(tokens[0], tokens[1], context); break; 
    case hcLinkpp: setLinkpp(tokens[0], tokens[1], context); break;
    case hcUnlinkp: setUnlink(tokens[0], context); break;
    case hcLock: setLock(tokens[0], context); break;
    case hcUnlock: setUnlock(tokens[0], context); break;;
    case hcNewSig: setNewsig(tokens[0], tokens[1], context); break;
    case hcDelSig: setDelsig(tokens[0], context); break;
    case hcSetP: setSetp(tokens[0], tokens[1], context); break;
    case hcSetS: setSets(tokens[0], tokens[1], context); break;
    case hcAddF: setAddf(tokens[0], tokens[1], tokens[2], context); break;
    case hcDelF: setDelf(tokens[0], tokens[1], context); break;
    case hcSave: setSave(tokens[0], tokens[1], context); break;
    case hcStart: setStart(context); break;
    case hcStop: setStop(context); break;
    case hcUnknown: ret = rtStandardError;
    }
  switch (ret) {
    case rtNoError:  
      if (context->verbose) {
        sprintf(context->outBuf, ackStr, pcmd);
        write(context->cliSock, context->outBuf, strlen(context->outBuf));
        }
      break;
    case rtHandledNoError: // Custom ok response already handled, take no action
      break; 
    case rtStandardError:
      sprintf(context->outBuf, setCmdNakStr, pcmd);
      write(context->cliSock, context->outBuf, strlen(context->outBuf));
      break;
    case rtCustomError: // Custom error response entered in buffer
      write(context->cliSock, context->outBuf, strlen(context->outBuf));
      break;
    case rtCustomHandledError: ;// Custom error respose handled, take no action
    }
  return 0;
}

int commandQuit(connectionRecType *context)
{
  printf("Closing connection with %s\n", context->hostName);
  return -1;
}

int commandShutdown(connectionRecType *context)
{
  if (context->cliSock == enabledConn) {
    printf("Shutting down\n");
    hal_flag = 1;
    hal_exit(comp_id);
    exit(0);
    return -1;
    }
  else
    return 0;
}

static int helpGeneral(connectionRecType *context)
{
  sprintf(context->outBuf, "Available commands:\n\r");
  strcat(context->outBuf, "  Hello <password> <client name> <protocol version>\n\r");
  strcat(context->outBuf, "  Get <emc command>\n\r");
  strcat(context->outBuf, "  Set <emc command>\n\r");
  strcat(context->outBuf, "  Quit\n\r");
  strcat(context->outBuf, "  Shutdown\n\r");
  strcat(context->outBuf, "  Help <command>\n\r");
  sockWrite(context);
  return 0;
}

static int helpHello(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  Hello <Password> <Client Name> <Protocol Version>\n\rWhere:\n\r");
  strcat(context->outBuf, "  Password is the connection password to allow communications with the CNC server.\n\r");
  strcat(context->outBuf, "  Client Name is the name of client trying to connect, typically the network name of the client.\n\r");
  strcat(context->outBuf, "  Protocol Version is the version of the protocol with which the client wishes to use.\n\r\n\r");
  strcat(context->outBuf, "  With valid password, server responds with:\n\r");
  strcat(context->outBuf, "  Hello Ack <Server Name> <Protocol Version>\n\rWhere:\n\r");
  strcat(context->outBuf, "  Ack is acknowledging the connection has been made.\n\r");
  strcat(context->outBuf, "  Server Name is the name of the EMC Server to which the client has connected.\n\r");
  strcat(context->outBuf, "  Protocol Version is the cleint requested version or latest version support by server if");
  strcat(context->outBuf, "  the client requests a version later than that supported by the server.\n\r\n\r");
  strcat(context->outBuf, "  With invalid password, the server responds with:\n\r");
  strcat(context->outBuf, "  Hello Nak\n\r");
  sockWrite(context);
  return 0;
}

static int helpGet(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\rGet <emc command>\n\r");
  strcat(context->outBuf, "  Get commands require that a hello has been successfully negotiated.\n\r");
  strcat(context->outBuf, "  Emc command may be one of:\n\r");
  strcat(context->outBuf, "    Comm_mode\n\r");
  strcat(context->outBuf, "    Comm_prot\n\r");
  strcat(context->outBuf, "    Comp <comp name\n\r");
  strcat(context->outBuf, "    Comps\n\r");
  strcat(context->outBuf, "    Echo\n\r");
  strcat(context->outBuf, "    Enable\n\r");
  strcat(context->outBuf, "    Funct <funct name>\n\r");
  strcat(context->outBuf, "    Functs\n\r");
  strcat(context->outBuf, "    Param <param name>\n\r");
  strcat(context->outBuf, "    Params\n\r");
  strcat(context->outBuf, "    ParamVal <param name>\n\r");
  strcat(context->outBuf, "    ParamVals\n\r");
  strcat(context->outBuf, "    Pin <pin name>\n\r");
  strcat(context->outBuf, "    PinVal <pin name>\n\r");
  strcat(context->outBuf, "    Pins\n\r");
  strcat(context->outBuf, "    PinVals\n\r");
  strcat(context->outBuf, "    Signal <signal name>\n\r");
  strcat(context->outBuf, "    Signals\n\r");
  strcat(context->outBuf, "    SigVal <signal name>\n\r");
  strcat(context->outBuf, "    SigVals\n\r");
  strcat(context->outBuf, "    Thread <thread name>\n\r");
  strcat(context->outBuf, "    Threads\n\r");
  strcat(context->outBuf, "    Verbose\n\r");
//  strcat(outBuf, "CONFIG\n\r");
  sockWrite(context);
  return 0;
}

static int helpSet(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r  Set <emc command>\n\r");
  strcat(context->outBuf, "  Set commands require that a hello has been successfully negotiated,\n\r");
  strcat(context->outBuf, "  in most instances requires that control be enabled by the connection.\n\r");
  strcat(context->outBuf, "  The set commands not requiring control enabled are:\n\r");
  strcat(context->outBuf, "    Comm_mode <mode>\n\r");
  strcat(context->outBuf, "    Comm_prot <protocol>\n\r");
  strcat(context->outBuf, "    Echo <On | Off>\n\r");
  strcat(context->outBuf, "    Enable <Pwd | Off>\n\r");
  strcat(context->outBuf, "    Verbose <On | Off>\n\r\n\r");
  strcat(context->outBuf, "  The set commands requiring control enabled are:\n\r");
  strcat(context->outBuf, "    Addf <function name>\n\r");
  strcat(context->outBuf, "    Delf <function name>\n\r");
  strcat(context->outBuf, "    DelSig <signal name>\n\r");
  strcat(context->outBuf, "    Linkpp <pin name> <pin name>\n\r");
  strcat(context->outBuf, "    Linkps <pin name> <signal name>\n\r");
  strcat(context->outBuf, "    Linksp <signal name> <pin name>\n\r");
  strcat(context->outBuf, "    Loadrt\n\r");
  strcat(context->outBuf, "    Loadusr <name> [<param 1> .. <param n>]\n\r");
  strcat(context->outBuf, "    Lock <command>\n\r");
  strcat(context->outBuf, "    NewSig <signal name>\n\r");
  strcat(context->outBuf, "    Save [<hal type> [<file name>]]\n\r");
  strcat(context->outBuf, "    Setp <pin name>\n\r");
  strcat(context->outBuf, "    Sets <signal name>\n\r");
  strcat(context->outBuf, "    Start\n\r");
  strcat(context->outBuf, "    Stop\n\r");
  strcat(context->outBuf, "    Unlink <pin name>\n\r");
  strcat(context->outBuf, "    Unloadrt\n\r");
  strcat(context->outBuf, "    Unlock <command>\n\r");
  
  sockWrite(context);
  return 0;
}

static int helpQuit(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  The quit command has the server initiate a disconnect from the client,\n\r");
  strcat(context->outBuf, "  the command has no parameters and no requirements to have negotiated\n\r");
  strcat(context->outBuf, "  a hello, or be in control.");
  sockWrite(context);
  return 0;
}

static int helpShutdown(connectionRecType *context)
{
  sprintf(context->outBuf, "Usage:\n\r");
  strcat(context->outBuf, "  The shutdown command terminates the connection with all clients,\n\r");
  strcat(context->outBuf, "  and initiates a shutdown of EMC. The command has no parameters, and\n\r");
  strcat(context->outBuf, "  can only be issued by the connection having control.\n\r");
  sockWrite(context);
  return 0;
}

static int helpHelp(connectionRecType *context)
{
  sprintf(context->outBuf, "If you need help on help, it is time to look into another line of work.\n\r");
  sockWrite(context);
  return 0;
}

int commandHelp(connectionRecType *context)
{
  char *pch;
  
  pch = strtok(NULL, delims);
  if (pch == NULL) return (helpGeneral(context));
  strupr(pch);
  printf("Command = %s", pch);
  if (strcmp(pch, "HELLO") == 0) return (helpHello(context));
  if (strcmp(pch, "GET") == 0) return (helpGet(context));
  if (strcmp(pch, "SET") == 0) return (helpSet(context));
  if (strcmp(pch, "QUIT") == 0) return (helpQuit(context));
  if (strcmp(pch, "SHUTDOWN") == 0) return (helpShutdown(context));
  if (strcmp(pch, "HELP") == 0) return (helpHelp(context));
  sprintf(context->outBuf, "%s is not a valid command.", pch);
  sockWrite(context);
  return 0;
}

commandTokenType lookupToken(char *s)
{
  commandTokenType i = cmdHello;
  int temp;
  
  while (i < cmdUnknown) {
    if (strcmp(commands[i], s) == 0) return i;
//    (int)i += 1;
    temp = i;
    temp++;
    i = (commandTokenType) temp;
    }
  return i;
}
 
int parseCommand(connectionRecType *context)
{
  int ret = 0;
  char *pch;
  static char *helloNakStr = "HELLO NAK\r\n";
  static char *helloAckStr = "HELLO ACK EMCNETSVR 1.0\r\n";
  static char *setNakStr = "SET NAK\r\n";
    
  pch = strtok(context->inBuf, delims);
  if (pch != NULL) {
    strupr(pch);
    switch (lookupToken(pch)) {
      case cmdHello: 
        if (commandHello(context) == -1)
          write(context->cliSock, helloNakStr, strlen(helloNakStr));
        else write(context->cliSock, helloAckStr, strlen(helloAckStr));
        break;
      case cmdGet: 
        ret = commandGet(context);
        break;
      case cmdSet:
        if (context->linked == 0)
	  write(context->cliSock, setNakStr, strlen(setNakStr)); 
        else ret = commandSet(context);
        break;
      case cmdQuit: 
        ret = commandQuit(context);
        break;
      case cmdShutdown:
        ret = commandShutdown(context);
	break;
      case cmdHelp:
        ret = commandHelp(context);
	break;
      case cmdUnknown: ret = -2;
      }
    }
  return ret;
}  

void *readClient(void *arg)
{
  char str[1600];
  char buf[1600];
  unsigned int i, j;
  int len;
  connectionRecType *context;
  
  
//  res = 1;
  context = (connectionRecType *) malloc(sizeof(connectionRecType));
  context->cliSock = client_sockfd;
  context->linked = 0;
  context->echo = 1;
  context->verbose = 0;
  strcpy(context->version, "1.0");
  strcpy(context->hostName, "Default");
  connCount++;
  context->commMode = 0;
  context->commProt = 0;
  context->inBuf[0] = 0;
  buf[0] = 0;
  
  while (1) {
    len = read(context->cliSock, &str, 1600);
    str[len] = 0;
    strcat(buf, str);
    if (!memchr(str, 0x0d, strlen(str))) continue;
    if ((context->echo == 1) && (context->linked) == 1)
      write(context->cliSock, &buf, strlen(buf));
    i = 0;
    j = 0;
    while (i <= strlen(buf)) {
      if ((buf[i] != '\n') && (buf[i] != '\r')) {
        context->inBuf[j] = buf[i];
	j++;
	}
      else
        if (j > 0)
          {
  	    context->inBuf[j] = 0;
            if (parseCommand(context) == -1)
              {
               close(context->cliSock);
               free(context);
	       pthread_exit((void *)0);
              }
	    j = 0;
	}
        i++;	
      }
    buf[0] = 0;
    } 
  return 0;
}
  
/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/ 
 
 /* main() is responsible for parsing command line options, and then
   parsing either a single command from the command line or a series
   of commands from a file or standard input.  It breaks the command[s]
   into tokens, and passes them to parse_cmd() which does the actual
   work for each command.
*/

int sockMain()
{
    pthread_t thrd;
    int res;
    
    while (1) {
      
      client_len = sizeof(client_address);
      client_sockfd = accept(server_sockfd,
        (struct sockaddr *)&client_address, &client_len);
      if (client_sockfd < 0) exit(0);
      res = pthread_create(&thrd, NULL, readClient, (void *)NULL);
      if (res != 0)
        close(client_sockfd);
     }
    return 0;
}

int main(int argc, char **argv)
{
    int n, fd;
    int keep_going, retval, errorcount;
    char *cp1, *filename = NULL;
    FILE *srcfile = NULL;

    /* set default level of output - 'quiet' */
    rtapi_set_msg_level(RTAPI_MSG_ERR);
    /* set default for other options */
    keep_going = 0;
    /* start parsing the command line, options first */
    n = 1;
    while ((n < argc) && (argv[n][0] == '-')) {
	cp1 = argv[n++];
	/* loop to parse grouped options */
	while (*(++cp1) != '\0') {
	    switch (*cp1) {
	    case 'R':
		/* force an unlock of the HAL mutex - to be used after a segfault in a hal program */
		if (release_HAL_mutex() != HAL_SUCCESS) {
			printf("halrmt: Release Mutex failed!\n");
			return 1;
		}
		return 0;
		break;
	    case 'h':
		/* -h = help */
                if (argc > n) {       /* there are more arguments, n has been incremented already */
                    do_help_cmd(argv[n]);
                } else
		    print_help_general(1);
		return 0;
		break;
	    case 'k':
		/* -k = keep going */
		keep_going = 1;
		break;
	    case 'q':
		/* -q = quiet (default) */
		rtapi_set_msg_level(RTAPI_MSG_ERR);
		break;
	    case 'Q':
		/* -Q = very quiet */
		rtapi_set_msg_level(RTAPI_MSG_NONE);
		break;
	    case 's':
		/* script friendly mode */
		scriptmode = 1;
		break;
	    case 'v':
		/* -v = verbose */
		rtapi_set_msg_level(RTAPI_MSG_INFO);
		break;
	    case 'V':
		/* -V = very verbose */
		rtapi_set_msg_level(RTAPI_MSG_ALL);
		break;
	    case 'f':
		/* -f = read from file (or stdin) */
		if (srcfile == NULL) {
		    /* it's the first -f (ignore repeats) */
		    if ((n < argc) && (argv[n][0] != '-')) {
			/* there is a following arg, and it's not an option */
			filename = argv[n++];
			srcfile = fopen(filename, "r");
			if (srcfile == NULL) {
			    fprintf(stderr,
				"Could not open command file '%s'\n",
				filename);
			    exit(-1);
			}
			/* make sure file is closed on exec() */
			fd = fileno(srcfile);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
		    } else {
			/* no filename followed -f option, use stdin */
			srcfile = stdin;
			prompt_mode = 1;
		    }
		}
		break;
#ifndef NO_INI
	    case 'i':
		/* -i = allow reading 'setp' values from an ini file */
		if (inifile == NULL) {
		    /* it's the first -i (ignore repeats) */
		    if ((n < argc) && (argv[n][0] != '-')) {
			/* there is a following arg, and it's not an option */
			filename = argv[n++];
			inifile = fopen(filename, "r");
			if (inifile == NULL) {
			    fprintf(stderr,
				"Could not open ini file '%s'\n",
				filename);
			    exit(-1);
			}
			/* make sure file is closed on exec() */
			fd = fileno(inifile);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
		    } else {
			/* no filename followed -i option, error */
			fprintf(stderr,
			    "No missing ini filename for -i option\n");
			exit(-1);
		    }
		}
		break;
#endif /* NO_INI */
	    default:
		/* unknown option */
		printf("Unknown option '-%c'\n", *cp1);
		break;
	    }
	}
    }
    /* don't print prompt in script mode (this may change later) */
//    if (scriptmode != 0) {
//        prompt_mode = 0;
//    }
    /* register signal handlers - if the process is killed
       we need to call hal_exit() to free the shared memory */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, SIG_IGN);
    /* at this point all options are parsed, connect to HAL */
    /* create a unique module name, to allow for multiple halrmt's */
    snprintf(comp_name, HAL_NAME_LEN-1, "halrmt%d", getpid());
    /* tell the signal handler that we might have the mutex */
    hal_flag = 1;
    /* connect to the HAL */
    comp_id = hal_init(comp_name);
    /* done with mutex */
    hal_flag = 0;
    /* check result */
    if (comp_id < 0) {
	fprintf(stderr, "halrmt: hal_init() failed: %d\n", comp_id );
	fprintf(stderr, "NOTE: 'rtapi' kernel module must be loaded\n" );
	return 1;
    }
    hal_ready(comp_id);
    retval = 0;
    errorcount = 0;
    initSockets();
    /* HAL init is OK, let's process the command(s) */
    /* tell the signal handler we might have the mutex */
    sockMain();
    hal_flag = 1;
    hal_exit(comp_id);
    if ( errorcount > 0 ) {
	return 1;
    } else {
	return 0;
    }
}

