/* Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2003 John Kasunich
 *                     <jmkasunich AT users DOT sourceforge DOT net>
 *
 *  Other contributers:
 *                     Martin Kuhnle
 *                     <mkuhnle AT users DOT sourceforge DOT net>
 *                     Alex Joni
 *                     <alex_joni AT users DOT sourceforge DOT net>
 *                     Benn Lipkowitz
 *                     <fenn AT users DOT sourceforge DOT net>
 *                     Stephen Wille Padnos
 *                     <swpadnos AT users DOT sourceforge DOT net>
 *                     Mick Grant - instantiated component loading
 *                     <arceye AT mgware DOT co DOT uk>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General
 *  Public License as published by the Free Software Foundation.
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 *  THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 *  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 *  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 *  harming persons must have provisions for completely removing power
 *  from all motors, etc, before persons enter any danger area.  All
 *  machinery must be designed to comply with local and national safety
 *  codes, and the authors of this software can not, and do not, take
 *  any responsibility for such compliance.
 *
 *  This code was written as part of the EMC HAL project.  For more
 *  information, go to www.linuxcnc.org.
 */

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_compat.h"
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"	/* private HAL decls */
#include "hal_ring.h"	        /* ringbuffer declarations */
#include "hal_group.h"	        /* group/member declarations */
#include "hal_rcomp.h"	        /* remote component declarations */
#include "halcmd_commands.h"
#include "halcmd_rtapiapp.h"

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
#include <fnmatch.h>
#include <limits.h>			/* PATH_MAX */
#include "rtapi_math.h"
#include <math.h> /* floorl */
#include <czmq.h>

const char *logpath = "/var/log/linuxcnc.log";

static int unloadrt_comp(char *mod_name);
static void print_comp_info(char **patterns);
static void print_inst_info(char **patterns);
static void print_vtable_info(char **patterns);
static void print_pin_info(int type, char **patterns);
static void print_pin_aliases(char **patterns);
static void print_param_aliases(char **patterns);
static void print_sig_info(int type, char **patterns);
static void print_script_sig_info(int type, char **patterns);
static void print_param_info(int type, char **patterns);
static void print_funct_info(char **patterns);
static void print_thread_info(char **patterns);
static void print_group_info(char **patterns);
static void print_ring_info(char **patterns);
static void print_comp_names(char **patterns);
static void print_pin_names(char **patterns);
static void print_sig_names(char **patterns);
static void print_param_names(char **patterns);
static void print_funct_names(char **patterns);
static void print_thread_names(char **patterns);
static void print_group_names(char **patterns);
static void print_ring_names(char **patterns);
static void print_inst_names(char **patterns);
static void print_eps_info(char **patterns);

static void print_lock_status();
static int count_list(int list_root);
static void print_mem_status();
static const char *data_type(int type);
static const char *data_type2(int type);
static const char *pin_data_dir(int dir);
static const char *param_data_dir(int dir);
static const char *data_arrow1(int dir);
static const char *data_arrow2(int dir);
static char *data_value(int type, void *valptr);
static char *data_value2(int type, void *valptr);
static void save_comps(FILE *dst);
static void save_aliases(FILE *dst);
static void save_signals(FILE *dst, int only_unlinked);
static void save_links(FILE *dst, int arrows);
static void save_nets(FILE *dst, int arrows);
static void save_params(FILE *dst);
static void save_threads(FILE *dst);
static void print_help_commands(void);

static int inst_count(hal_comp_t *comp);

static int tmatch(int req_type, int type) {
    return req_type == -1 || type == req_type;
}

static int match(char **patterns, char *value) {
    int i;
    if(!patterns || !patterns[0] || !patterns[0][0]) return 1;
    for(i=0; patterns[i] && *patterns[i]; i++) {
	char *pattern = patterns[i];
	if(strncmp(pattern, value, strlen(pattern)) == 0) return 1;
	if (fnmatch(pattern, value, 0) == 0) return 1;
    }
    return 0;
}

int do_lock_cmd(char *command)
{
    int retval=0;

    /* if command is blank or "all", want to lock everything */
    if ((command == NULL) || (strcmp(command, "all") == 0)) {
	retval = hal_set_lock(HAL_LOCK_ALL);
    } else if (strcmp(command, "none") == 0) {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "tune") == 0) {
	retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);
    } else if (strcmp(command, "all") == 0) {
	retval = hal_set_lock(HAL_LOCK_ALL);
    }

    if (retval == 0) {
	/* print success message */
	halcmd_info("Locking completed");
    } else {
	halcmd_error("Locking failed\n");
    }
    return retval;
}

int do_unlock_cmd(char *command)
{
    int retval=0;

    /* if command is blank or "all", want to unlock everything */
    if ((command == NULL) || (strcmp(command, "all") == 0)) {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "all") == 0) {
	retval = hal_set_lock(HAL_LOCK_NONE);
    } else if (strcmp(command, "tune") == 0) {
	retval = hal_set_lock(HAL_LOCK_LOAD & HAL_LOCK_CONFIG);
    }

    if (retval == 0) {
	/* print success message */
	halcmd_info("Unlocking completed");
    } else {
	halcmd_error("Unlocking failed\n");
    }
    return retval;
}

int do_linkpp_cmd(char *first_pin_name, char *second_pin_name)
{
    int retval;
    hal_pin_t *first_pin, *second_pin;
    static int dep_msg_printed = 0;

    if ( dep_msg_printed == 0 ) {
	halcmd_warning("linkpp command is deprecated, use 'net'\n");
	dep_msg_printed = 1;
    }
    rtapi_mutex_get(&(hal_data->mutex));
    /* check if the pins are there */
    first_pin = halpr_find_pin_by_name(first_pin_name);
    second_pin = halpr_find_pin_by_name(second_pin_name);
    if (first_pin == 0) {
	/* first pin not found*/
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("pin '%s' not found\n", first_pin_name);
	return -EINVAL;
    } else if (second_pin == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("pin '%s' not found\n", second_pin_name);
	return -EINVAL;
    }

    /* give the mutex, as the other functions use their own mutex */
    rtapi_mutex_give(&(hal_data->mutex));

    /* check that both pins have the same type,
       don't want to create a sig, which after that won't be usefull */
    if (first_pin->type != second_pin->type) {
	halcmd_error("pins '%s' and '%s' not of the same type\n",
                first_pin_name, second_pin_name);
	return -EINVAL;
    }

    /* now create the signal */
    retval = hal_signal_new(first_pin_name, first_pin->type);

    if (retval == 0) {
	/* if it worked, link the pins to it */
	retval = hal_link(first_pin_name, first_pin_name);

	if ( retval == 0 ) {
	/* if that worked, link the second pin to the new signal */
	    retval = hal_link(second_pin_name, first_pin_name);
	}
    }
    if (retval < 0) {
	halcmd_error("linkpp failed\n");
    }
    return retval;
}

int do_linkps_cmd(char *pin, char *sig)
{
    int retval;

    retval = hal_link(pin, sig);
    if (retval == 0) {
	/* print success message */
        halcmd_info("Pin '%s' linked to signal '%s'\n", pin, sig);
    } else {
        halcmd_error("link failed\n");
    }
    return retval;
}

int do_linksp_cmd(char *sig, char *pin) {
    return do_linkps_cmd(pin, sig);
}


int do_unlinkp_cmd(char *pin)
{
    int retval;

    retval = hal_unlink(pin);
    if (retval == 0) {
	/* print success message */
	halcmd_info("Pin '%s' unlinked\n", pin);
    } else {
        halcmd_error("unlink failed\n");
    }
    return retval;
}

int do_source_cmd(char *hal_filename) {
    FILE *f = fopen(hal_filename, "r");
    char buf[MAX_CMD_LEN+1];
    int fd;
    int result = 0;
    int lineno_save = halcmd_get_linenumber();
    int linenumber = 1;
    char *filename_save = strdup(halcmd_get_filename());

    if(!f) {
        fprintf(stderr, "Could not open hal file '%s': %s\n",
                hal_filename, strerror(errno));
	free(filename_save);
        return -EINVAL;
    }
    fd = fileno(f);
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    halcmd_set_filename(hal_filename);

    while(1) {
        char *readresult = fgets(buf, MAX_CMD_LEN, f);
        halcmd_set_linenumber(linenumber++);
        if(readresult == 0) {
            if(feof(f)) break;
            halcmd_error("Error reading file: %s\n", strerror(errno));
            result = -EINVAL;
            break;
        }
        result = halcmd_parse_line(buf);
        if(result != 0) break;
    }

    halcmd_set_linenumber(lineno_save);
    halcmd_set_filename(filename_save);
    free(filename_save);
    fclose(f);
    return result;
}

int do_start_cmd(void) {
    int retval = hal_start_threads();
    if (retval == 0) {
        /* print success message */
        halcmd_info("Realtime threads started\n");
    }
    return retval;
}

int do_stop_cmd(void) {
    int retval = hal_stop_threads();
    if (retval == 0) {
        /* print success message */
        halcmd_info("Realtime threads stopped\n");
    }
    return retval;
}

int do_echo_cmd(void) {
    printf("Echo on\n");
    return 0;
}
int do_unecho_cmd(void) {
    printf("Echo off\n");
    return 0;
}
int do_addf_cmd(char *func, char *thread, char **opt) {
    char *position_str = opt ? opt[0] : NULL;
    int position = -1;
    int retval;

    if(position_str && *position_str) position = atoi(position_str);

    retval = hal_add_funct_to_thread(func, thread, position);
    if(retval == 0) {
        halcmd_info("Function '%s' added to thread '%s'\n",
                    func, thread);
    } else {
        halcmd_error("addf failed: %s\n", hal_lasterror());
    }
    return retval;
}

int do_alias_cmd(char *pinparam, char *name, char *alias) {
    int retval;

    if ( strcmp (pinparam, "pin" ) == 0 ) {
	retval = hal_pin_alias(name, alias);
    } else if ( strcmp (pinparam, "param" ) == 0 ) {
	retval = hal_param_alias(name, alias);
    } else {
	retval = -EINVAL;
    }
    if(retval == 0) {
        halcmd_info("%s '%s' aliased to '%s'\n",
                    pinparam, name, alias);
    } else {
        halcmd_error("alias failed\n");
    }
    return retval;
}

int do_unalias_cmd(char *pinparam, char *name) {
    int retval;
    if (strcmp(pinparam, "pin") == 0) {
        retval = hal_pin_alias(name, NULL);
    } else if ( strcmp (pinparam, "param" ) == 0 ) {
      retval = hal_param_alias(name, NULL);
    } else {
        return -EINVAL;
    };
    if(retval == 0) {
        halcmd_info("%s '%s' unaliased\n",
                    pinparam, name);
    } else {
        halcmd_error("unalias failed\n");
    }
    return retval;
}
int do_delf_cmd(char *func, char *thread) {
    int retval;

    retval = hal_del_funct_from_thread(func, thread);
    if(retval == 0) {
        halcmd_info("Function '%s' removed from thread '%s'\n",
                    func, thread);
    } else {
        halcmd_error("delf failed\n");
    }

    return retval;
}

static int preflight_net_cmd(char *signal, hal_sig_t *sig, char *pins[]) {
    int i, type=-1, writers=0, bidirs=0, pincnt=0;
    char *writer_name=0, *bidir_name=0;
    /* if signal already exists, use its info */
    if (sig) {
	type = sig->type;
	writers = sig->writers;
	bidirs = sig->bidirs;
    }

    if(writers || bidirs)
    {
        hal_pin_t *pin;
        int next;
        for(next = hal_data->pin_list_ptr; next; next=pin->next_ptr)
        {
            pin = SHMPTR(next);
            if(SHMPTR(pin->signal) == sig && pin->dir == HAL_OUT)
                writer_name = pin->name;
            if(SHMPTR(pin->signal) == sig && pin->dir == HAL_IO)
                bidir_name = writer_name = pin->name;
        }
    }

    for(i=0; pins[i] && *pins[i]; i++) {
        hal_pin_t *pin = 0;
        pin = halpr_find_pin_by_name(pins[i]);
        if(!pin) {
            halcmd_error("Pin '%s' does not exist\n",
                    pins[i]);
            return -ENOENT;
        }
        if(SHMPTR(pin->signal) == sig) {
	     /* Already on this signal */
	    pincnt++;
	    continue;
	} else if(pin->signal != 0) {
            hal_sig_t *osig = SHMPTR(pin->signal);
            halcmd_error("Pin '%s' was already linked to signal '%s'\n",
                    pin->name, osig->name);
            return -EINVAL;
	}
	if (type == -1) {
	    /* no pre-existing type, use this pin's type */
	    type = pin->type;
	}
        if(type != pin->type) {
            halcmd_error(
                "Signal '%s' of type '%s' cannot add pin '%s' of type '%s'\n",
                signal, data_type2(type), pin->name, data_type2(pin->type));
            return -EINVAL;
        }
        if(pin->dir == HAL_OUT) {
            if(writers || bidirs) {
            dir_error:
                halcmd_error(
                    "Signal '%s' can not add %s pin '%s', "
                    "it already has %s pin '%s'\n",
                        signal, pin_data_dir(pin->dir), pin->name,
                        bidir_name ? pin_data_dir(HAL_IO):pin_data_dir(HAL_OUT),
                        bidir_name ? bidir_name : writer_name);
                return -EINVAL;
            }
            writer_name = pin->name;
            writers++;
        }
	if(pin->dir == HAL_IO) {
            if(writers) {
                goto dir_error;
            }
            bidir_name = pin->name;
            bidirs++;
        }
        pincnt++;
    }
    if(pincnt)
        return 0;
    halcmd_error("'net' requires at least one pin, none given\n");
    return -EINVAL;
}

int do_net_cmd(char *signal, char *pins[]) {
    hal_sig_t *sig;
    int i, retval;

    rtapi_mutex_get(&(hal_data->mutex));
    /* see if signal already exists */
    sig = halpr_find_sig_by_name(signal);

    /* verify that everything matches up (pin types, etc) */
    retval = preflight_net_cmd(signal, sig, pins);
    if(retval < 0) {
        rtapi_mutex_give(&(hal_data->mutex));
        return retval;
    }

    {
	hal_pin_t *pin = halpr_find_pin_by_name(signal);
	if(pin) {
	    halcmd_error(
                    "Signal name '%s' must not be the same as a pin.  "
                    "Did you omit the signal name?\n",
		signal);
	    rtapi_mutex_give(&(hal_data->mutex));
	    return -ENOENT;
	}
    }
    if(!sig) {
        /* Create the signal with the type of the first pin */
        hal_pin_t *pin = halpr_find_pin_by_name(pins[0]);
        rtapi_mutex_give(&(hal_data->mutex));
        if(!pin) {
            return -ENOENT;
        }
        retval = hal_signal_new(signal, pin->type);
    } else {
	/* signal already exists */
        rtapi_mutex_give(&(hal_data->mutex));
    }
    /* add pins to signal */
    for(i=0; retval == 0 && pins[i] && *pins[i]; i++) {
        retval = do_linkps_cmd(pins[i], signal);
    }

    return retval;
}

int do_newsig_cmd(char *name, char *type)
{
    int retval;

    if (strcasecmp(type, "bit") == 0) {
	retval = hal_signal_new(name, HAL_BIT);
    } else if (strcasecmp(type, "float") == 0) {
	retval = hal_signal_new(name, HAL_FLOAT);
    } else if (strcasecmp(type, "u32") == 0) {
	retval = hal_signal_new(name, HAL_U32);
    } else if (strcasecmp(type, "s32") == 0) {
	retval = hal_signal_new(name, HAL_S32);
    } else {
	halcmd_error("Unknown signal type '%s'\n", type);
	retval = -EINVAL;
    }
    if (retval < 0) {
	halcmd_error("newsig failed\n");
    }
    return retval;
}

static int set_common(hal_type_t type, void *d_ptr, char *value) {
    // This function assumes that the mutex is held
    int retval = 0;
    double fval;
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
	    halcmd_error("value '%s' invalid for bit\n", value);
	    retval = -EINVAL;
	}
	break;
    case HAL_FLOAT:
	fval = strtod ( value, &cp );
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid character(s) in string */
	    halcmd_error("value '%s' invalid for float\n", value);
	    retval = -EINVAL;
	} else {
	    *((hal_float_t *) (d_ptr)) = fval;
	}
	break;
    case HAL_S32:
	lval = strtol(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    halcmd_error("value '%s' invalid for S32\n", value);
	    retval = -EINVAL;
	} else {
	    *((hal_s32_t *) (d_ptr)) = lval;
	}
	break;
    case HAL_U32:
	ulval = strtoul(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    halcmd_error("value '%s' invalid for U32\n", value);
	    retval = -EINVAL;
	} else {
	    *((hal_u32_t *) (d_ptr)) = ulval;
	}
	break;
    default:
	/* Shouldn't get here, but just in case... */
	halcmd_error("bad type %d\n", type);
	retval = -EINVAL;
    }
    return retval;
}

int do_setp_cmd(char *name, char *value)
{
    int retval;
    hal_param_t *param;
    hal_pin_t *pin;
    hal_type_t type;
    void *d_ptr;
    hal_comp_t *comp; // owning component

    halcmd_info("setting parameter '%s' to '%s'\n", name, value);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param == 0) {
        pin = halpr_find_pin_by_name(name);
        if(pin == 0) {
            rtapi_mutex_give(&(hal_data->mutex));
            halcmd_error("parameter or pin '%s' not found\n", name);
            return -EINVAL;
        } else {
	    comp =  halpr_find_owning_comp(pin->owner_id);
            /* found it */
            type = pin->type;
            if ((pin->dir == HAL_OUT) && (comp->state != COMP_UNBOUND)) {
                rtapi_mutex_give(&(hal_data->mutex));
                halcmd_error("pin '%s' is not writable\n", name);
                return -EINVAL;
            }
            if(pin->signal != 0) {
                rtapi_mutex_give(&(hal_data->mutex));
                halcmd_error("pin '%s' is connected to a signal\n", name);
                return -EINVAL;
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
            halcmd_error("param '%s' is not writable\n", name);
            return -EINVAL;
        }
        d_ptr = SHMPTR(param->data_ptr);
    }

    retval = set_common(type, d_ptr, value);

    rtapi_mutex_give(&(hal_data->mutex));
    if (retval == 0) {
	/* print success message */
        if(param) {
            halcmd_info("Parameter '%s' set to %s\n", name, value);
        } else {
            halcmd_info("Pin '%s' set to %s\n", name, value);
	}
    } else {
	halcmd_error("setp failed\n");
    }
    return retval;

}

int do_sete_cmd(char *pos, char *value)
{
    char *cp = pos;

    unsigned index = strtoul(pos, &cp, 0);
    if ((*cp != '\0') && (!isspace(*cp))) {
	/* invalid chars in string */
	halcmd_error("value '%s' invalid for index\n", value);
	return -EINVAL;
    }
    if (index > MAX_EPSILON-1) {
	halcmd_error("index %u out of range (0..%d)\n", index, MAX_EPSILON-1);
	return -EINVAL;
    }

    double epsilon = strtod ( value, &cp );
    if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid character(s) in string */
	halcmd_error("value '%s' invalid for float\n", value);
	return -EINVAL;
    }
    halcmd_info("setting epsilon[%u] = %f\n", index, epsilon);

    rtapi_mutex_get(&(hal_data->mutex));
    hal_data->epsilon[index] = epsilon;
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}


int do_ptype_cmd(char *name)
{
    hal_param_t *param;
    hal_pin_t *pin;
    hal_type_t type;

    rtapi_print_msg(RTAPI_MSG_DBG, "getting parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param) {
        /* found it */
        type = param->type;
        halcmd_output("%s\n", data_type2(type));
        rtapi_mutex_give(&(hal_data->mutex));
        return 0;
    }

    /* not found, search pin list for name */
    pin = halpr_find_pin_by_name(name);
    if(pin) {
        /* found it */
        type = pin->type;
        halcmd_output("%s\n", data_type2(type));
        rtapi_mutex_give(&(hal_data->mutex));
        return 0;
    }

    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_error("parameter '%s' not found\n", name);
    return -EINVAL;
}


int do_getp_cmd(char *name)
{
    hal_param_t *param;
    hal_pin_t *pin;
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "getting parameter '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search param list for name */
    param = halpr_find_param_by_name(name);
    if (param) {
        /* found it */
        type = param->type;
        d_ptr = SHMPTR(param->data_ptr);
        halcmd_output("%s\n", data_value2((int) type, d_ptr));
        rtapi_mutex_give(&(hal_data->mutex));
        return 0;
    }

    /* not found, search pin list for name */
    pin = halpr_find_pin_by_name(name);
    if(pin) {
        /* found it */
        type = pin->type;
        if (pin->signal != 0) {
            sig = SHMPTR(pin->signal);
            d_ptr = SHMPTR(sig->data_ptr);
        } else {
            sig = 0;
            d_ptr = &(pin->dummysig);
        }
        halcmd_output("%s\n", data_value2((int) type, d_ptr));
        rtapi_mutex_give(&(hal_data->mutex));
        return 0;
    }

    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_error("parameter '%s' not found\n", name);
    return -EINVAL;
}

int do_sets_cmd(char *name, char *value)
{
    int retval;
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "setting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("signal '%s' not found\n", name);
	return -EINVAL;
    }
    /* found it - does it have a writer? */
    if (sig->writers > 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("signal '%s' already has writer(s)\n", name);
	return -EINVAL;
    }
    /* no writer, so we can safely set it */
    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    retval = set_common(type, d_ptr, value);
    rtapi_mutex_give(&(hal_data->mutex));
    if (retval == 0) {
	/* print success message */
	halcmd_info("Signal '%s' set to %s\n", name, value);
    } else {
	halcmd_error("sets failed\n");
    }
    return retval;

}

int do_stype_cmd(char *name)
{
    hal_sig_t *sig;
    hal_type_t type;

    rtapi_print_msg(RTAPI_MSG_DBG, "getting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("signal '%s' not found\n", name);
	return -EINVAL;
    }
    /* found it */
    type = sig->type;
    halcmd_output("%s\n", data_type2(type));
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}

int do_gets_cmd(char *name)
{
    hal_sig_t *sig;
    hal_type_t type;
    void *d_ptr;

    rtapi_print_msg(RTAPI_MSG_DBG, "getting signal '%s'\n", name);
    /* get mutex before accessing shared data */
    rtapi_mutex_get(&(hal_data->mutex));
    /* search signal list for name */
    sig = halpr_find_sig_by_name(name);
    if (sig == 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("signal '%s' not found\n", name);
	return -EINVAL;
    }
    /* found it */
    type = sig->type;
    d_ptr = SHMPTR(sig->data_ptr);
    halcmd_output("%s\n", data_value2((int) type, d_ptr));
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}

static int get_type(char ***patterns) {
    char *typestr = 0;
    if(!(*patterns)) return -1;
    if(!(*patterns)[0]) return -1;
    if((*patterns)[0][0] != '-' || (*patterns)[0][1] != 't') return -1;
    if((*patterns)[0][2]) {
	typestr = &(*patterns)[0][2];
	*patterns += 1;
    } else if((*patterns)[1][0]) {
	typestr = (*patterns)[1];
	*patterns += 2;
    }
    if(!typestr) return -1;
    if(strcmp(typestr, "float") == 0) return HAL_FLOAT;
    if(strcmp(typestr, "bit") == 0) return HAL_BIT;
    if(strcmp(typestr, "s32") == 0) return HAL_S32;
    if(strcmp(typestr, "u32") == 0) return HAL_U32;
    if(strcmp(typestr, "signed") == 0) return HAL_S32;
    if(strcmp(typestr, "unsigned") == 0) return HAL_U32;
    return -1;
}

int do_log_cmd(char *type, char *level)
{
    char *lp = level;
    int ivalue;

    if (type == NULL) {
	halcmd_output("RTAPI message level:  RT:%d User:%d\n",
		  global_data->rt_msg_level, global_data->user_msg_level);
	return 0;
    }
    if (level == NULL)  {
	if (strcasecmp(type, "rt") == 0) {
	    halcmd_output("%d\n", global_data->rt_msg_level);
	} else if (strcasecmp(type, "user") == 0) {
	    halcmd_output("%d\n", global_data->user_msg_level);
	} else {
	    halcmd_error("log: invalid loglevel type '%s' - expected 'rt' or 'user'\n", type);
	    return -EINVAL;
	}
	return 0;
    }
    ivalue = strtol(level, &lp, 0);
    if ((*lp != '\0') && (!isspace(*lp))) {
	/* invalid chars in string */
	halcmd_error("value '%s' invalid for interger\n", level);
	return -EINVAL;
    } else {
	if (strcasecmp(type, "rt") == 0) {
	    global_data->rt_msg_level = ivalue;
	} else if (strcasecmp(type, "user") == 0) {
	    global_data->user_msg_level = ivalue;
	} else {
	    halcmd_error("log: invalid loglevel type '%s' - expected 'rt' or 'user'\n", type);
	    return -EINVAL;
	}
	return 0;
    }
}

int do_show_cmd(char *type, char **patterns)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (!type || *type == '\0') {
	/* print everything */
	print_comp_info(NULL);
	print_inst_info(NULL);
	print_pin_info(-1, NULL);
	print_pin_aliases(NULL);
	print_sig_info(-1, NULL);
	print_param_info(-1, NULL);
	print_param_aliases(NULL);
	print_funct_info(NULL);
	print_thread_info(NULL);
	print_group_info(NULL);
	print_ring_info(NULL);
	print_vtable_info(NULL);
	print_eps_info(NULL);
    } else if (strcmp(type, "all") == 0) {
	/* print everything, using the pattern */
	print_comp_info(patterns);
	print_inst_info(patterns);
	print_pin_info(-1, patterns);
	print_pin_aliases(patterns);
	print_sig_info(-1, patterns);
	print_param_info(-1, patterns);
	print_param_aliases(patterns);
	print_funct_info(patterns);
	print_thread_info(patterns);
	print_group_info(patterns);
	print_ring_info(patterns);
	print_vtable_info(patterns);
	print_eps_info(patterns);
    } else if (strcmp(type, "comp") == 0) {
	print_comp_info(patterns);
    } else if (strcmp(type, "inst") == 0) {
	print_inst_info(patterns);
    } else if (strcmp(type, "vtable") == 0) {
	print_vtable_info(patterns);
    } else if (strcmp(type, "pin") == 0) {
	int type = get_type(&patterns);
	print_pin_info(type, patterns);
    } else if (strcmp(type, "sig") == 0) {
	int type = get_type(&patterns);
	print_sig_info(type, patterns);
    } else if (strcmp(type, "signal") == 0) {
	int type = get_type(&patterns);
	print_sig_info(type, patterns);
    } else if (strcmp(type, "param") == 0) {
	int type = get_type(&patterns);
	print_param_info(type, patterns);
    } else if (strcmp(type, "parameter") == 0) {
	int type = get_type(&patterns);
	print_param_info(type, patterns);
    } else if (strcmp(type, "funct") == 0) {
	print_funct_info(patterns);
    } else if (strcmp(type, "function") == 0) {
	print_funct_info(patterns);
    } else if (strcmp(type, "thread") == 0) {
	print_thread_info(patterns);
    } else if (strcmp(type, "group") == 0) {
	print_group_info(patterns);
    } else if (strcmp(type, "ring") == 0) {
	print_ring_info(patterns);
    } else if (strcmp(type, "eps") == 0) {
	print_eps_info(patterns);
    } else if (strcmp(type, "alias") == 0) {
	print_pin_aliases(patterns);
	print_param_aliases(patterns);
    } else {
	halcmd_error("Unknown 'show' type '%s'\n", type);
	return -1;
    }
    return 0;
}

int do_list_cmd(char *type, char **patterns)
{
    if ( !type) {
	halcmd_error("'list' requires type'\n");
	return -1;
    }
    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (strcmp(type, "comp") == 0) {
	print_comp_names(patterns);
    } else if (strcmp(type, "pin") == 0) {
	print_pin_names(patterns);
    } else if (strcmp(type, "sig") == 0) {
	print_sig_names(patterns);
    } else if (strcmp(type, "signal") == 0) {
	print_sig_names(patterns);
    } else if (strcmp(type, "param") == 0) {
	print_param_names(patterns);
    } else if (strcmp(type, "parameter") == 0) {
	print_param_names(patterns);
    } else if (strcmp(type, "funct") == 0) {
	print_funct_names(patterns);
    } else if (strcmp(type, "function") == 0) {
	print_funct_names(patterns);
    } else if (strcmp(type, "thread") == 0) {
	print_thread_names(patterns);
    } else if (strcmp(type, "group") == 0) {
	print_group_names(patterns);
    } else if (strcmp(type, "ring") == 0) {
	print_ring_names(patterns);
    } else if (strcmp(type, "inst") == 0) {
	print_inst_names(patterns);
    } else {
	halcmd_error("Unknown 'list' type '%s'\n", type);
	return -1;
    }
    return 0;
}

int do_status_cmd(char *type)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if ((type == NULL) || (strcmp(type, "all") == 0)) {
	/* print everything */
	/* add other status functions here if/when they are defined */
	print_lock_status();
	print_mem_status();
    } else if (strcmp(type, "lock") == 0) {
	print_lock_status();
    } else if (strcmp(type, "mem") == 0) {
	print_mem_status();
    } else {
	halcmd_error("Unknown 'status' type '%s'\n", type);
	return -1;
    }
    return 0;
}

// can this get any uglier?
int yesno(const char *s)
{
    if (!s)
	return -1;
    if ((strcmp("1", s) == 0) ||
	(strcasecmp("true", s) == 0) ||
	(strcasecmp("yes", s) == 0))
	return 1;
    if ((strcmp("0", s) == 0) ||
	(strcasecmp("false", s) == 0) ||
	(strcasecmp("no", s) == 0))
	return 0;

    return -1;
}


extern int autoload;

int do_autoload_cmd(char *what)
{
    if (!what) {
	halcmd_output("component autoload on 'newinst' is %s\n",
		      autoload ? "ON":"OFF");
	return 0;
    }
    int val = yesno(what);
    if (val < 0) {
	    halcmd_error("value '%s' invalid for autoload (1 or 0)\n", what);
	   return -EINVAL;
    }
    autoload = val;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// helper functions to check if base module is loaded and what instances exist


bool module_loaded(char *mod_name)
{
    CHECK_HALDATA();
    CHECK_STR(mod_name);
    {
	WITH_HAL_MUTEX();
        hal_comp_t *comp = halpr_find_comp_by_name(mod_name);
        return (comp != NULL);
    }
}


bool inst_name_exists(char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);
    {
	WITH_HAL_MUTEX();

	hal_inst_t *ins  = halpr_find_inst_by_name(name);
	return (ins != NULL);
    }
}

int loadrt(char *mod_name, char *args[])
{
    char *cp1;
    int n, retval;
    char arg_string[MAX_CMD_LEN+1];

    retval = rtapi_loadrt(rtapi_instance, mod_name, (const char **)args);
    if ( retval != 0 ) {
	halcmd_error("insmod failed, returned %d:\n%s\n"
		     "See %s for more information.\n",
		     retval, rtapi_rpcerror(), logpath);
	return -1;
    }

    // make the args that were passed to the module into a single string
    n = 0;
    arg_string[0] = '\0';
    while ( args[n] && args[n][0] != '\0' ) {
	strncat(arg_string, args[n++], MAX_CMD_LEN);
	strncat(arg_string, " ", MAX_CMD_LEN);
    }
    // allocate HAL shmem for the string
    cp1 = hal_malloc(strlen(arg_string)+1);
    if ( cp1 == NULL ) {
	halcmd_error("failed to allocate memory for module args\n");
	return -1;
    }
    // copy string to shmem
    strcpy (cp1, arg_string);
    {
	WITH_HAL_MUTEX();

	// search component list for the newly loaded component
	hal_comp_t *comp = halpr_find_comp_by_name(mod_name);
	if (comp == NULL) {
	    halcmd_error("module '%s' not loaded\n", mod_name);
	    return -EINVAL;
        }
	// link args to comp struct
	comp->insmod_args = SHMOFF(cp1);
    }
    // print success message
    halcmd_info("Realtime module '%s' loaded\n", mod_name);
    return 0;
}

static int loadrt_cmd(const bool instantiate, // true if called from do_newinst
		      char *mod_name,
		      char *args[])
{
    char arg_string[MAX_CMD_LEN+1];
    char arg_section[MAX_CMD_LEN+1];
    char buff[MAX_CMD_LEN+1];
    int  n = 0, x = 0, w = 0, p = 0, retval;
    bool instantiable = false, singleton = false;
    char *cp1, *cp2;
    char *argv[] = { NULL};
    // MAX_ARGS defined in halcmd_commands.h - currently 20
    char *list[MAX_ARGS] = {"\0",};
    int list_index = 0;

    if (hal_get_lock() & HAL_LOCK_LOAD) {
	halcmd_error("HAL is locked, loading of modules is not permitted\n");
	return -EPERM;
    }

    // determine module properties (loaded or not)
    retval = rtapi_get_tags(mod_name);
    if(retval == -1) {
	halcmd_error("Error in module tags search");
	return retval;
    }  else {
	if((retval & HC_INSTANTIABLE) == HC_INSTANTIABLE )
	    instantiable = true;
	// extra test for other tags below
	if((retval & HC_SINGLETON) == HC_SINGLETON)
	    singleton = true;
    }

    // if not instantiable and not called from do_newinst_cmd(),
    // just loadrt the comp
    if (!(instantiable && instantiate)) {
	// legacy components
        return loadrt(mod_name, args);
    }

    // from here on: only instantiable comps to be considered
    // a singleton might be instantiable too (once only)
    //
    // if we come here we were called from do_newinst_cmd()
    if (!(args[0] != NULL && strlen(args[0]))) {
	// no args case: treat as count=1
	// if no args just create a single instance
	// with default number 0, unless singleton.

	// if the module isnt loaded yet, do so now:
	// XXX - autoload setting? I guess this is assumed
	// to be on
	if (!module_loaded(mod_name)) {
	    if((retval = (loadrt(mod_name, argv))) )
		return retval;
	}
	// determine instance name:
	if (singleton) {
	    // a singleton instantiable comp will have a single instance
	    // with the same name as the component.
	    sprintf(buff, "%s", mod_name);
	    hal_comp_t *existing_comp = halpr_find_comp_by_name(mod_name);
	    if (inst_name_exists(buff) || inst_count(existing_comp)) {
		halcmd_error("\nError singleton component '%s' already exists\n", buff);
		return -1;
	    }
	} else {
	    // find unused instance name
	    w = 0;
	    sprintf(buff, "%s.%d", mod_name, w);
	    while(inst_name_exists(buff))
		sprintf(buff, "%s.%d", mod_name, ++w);
	}
	// now instantiate with this name
	retval = do_newinst_cmd(mod_name, buff, argv);
	if ( retval != 0 ) {
	    halcmd_error("rc=%d\n%s", retval, rtapi_rpcerror());
	}
	return retval;
    }

    // args were given.
    assert(args[0] != NULL && strlen(args[0]));

    strcpy(arg_string, args[0]);
    // handle count=N
    if ((strncmp(arg_string, "count=", 6) == 0) && !singleton) {
	strcpy(arg_section, &arg_string[6]);
	n = strtol(arg_section, &cp1, 10);
	if (n > 0) {
	    // check if already loaded, if not load it
	    if (!module_loaded(mod_name)) {
		if((retval = (loadrt(mod_name, argv))) )
		    return retval;
	    }
	    for(int y = 0, v = 0; y < n; y++ , v++) {
		// find unused instance name
		sprintf(buff, "%s.%d", mod_name, v);
		while(inst_name_exists(buff))
		    sprintf(buff, "%s.%d", mod_name, ++v);
		// and instantiate
		retval = do_newinst_cmd(mod_name, buff, argv);
		if ( retval != 0 )
		    return retval;
	    }
	} else {
	    halcmd_error("%s: count=%d parameter invalid\n",
			 mod_name, n);
	    return -1;
	}
    } // count=N
    // handle names="..."
    else if ((strncmp(arg_string, "names=", 6) == 0) && !singleton) {
	strcpy(arg_section, &arg_string[6]);
	cp1 = strtok(arg_section, ",");
	list_index = 0;
	while( cp1 != NULL ) {
	    cp2 = (char *) malloc(strlen(cp1) + 1);
	    strcpy(cp2, cp1);
	    list[list_index++] = cp2;
	    cp1 = strtok(NULL, ",");
	}
	if (list_index) {
	    if (!module_loaded(mod_name)) {
		if ((retval = (loadrt(mod_name, argv)))) {
		    for(p = 0; p < list_index; p++)
			free(list[p]);
		    return retval;
		}
	    }
	    for (w = 0; w < list_index; w++) {
		if (inst_name_exists(list[w])) {
		    halcmd_error("\nA named instance '%s' already exists\n", list[w]);
		    for( p = 0; p < list_index; p++)
			free(list[p]);
		    return -1;
		}
		retval = do_newinst_cmd(mod_name, list[w], argv);
		if ( retval != 0 ) {
		    for( p = 0; p < list_index; p++)
			free(list[p]);
		    return retval;
		}
	    }
	    for(p = 0; p < list_index; p++)
		free(list[p]);
	}
    } else {
	// invalid parameter
	halcmd_error("\nInvalid argument '%s' to instantiated component\n"
		     "NB. Use of personality or cfg is deprecated\n"
		     "Singleton components cannot have multiple instances\n\n",
		     args[x]);
	return -1;
    }
    return 0;
}


int do_loadrt_cmd(char *mod_name, char *args[])
{
    return loadrt_cmd(true, mod_name, args);
}


int do_delsig_cmd(char *mod_name)
{
    int next, retval, retval1, n;
    hal_sig_t *sig;
    char sigs[MAX_EXPECTED_SIGS][HAL_NAME_LEN+1];

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) != 0 ) {
	retval = hal_signal_delete(mod_name);
	if (retval == 0) {
	    /* print success message */
	    halcmd_info("Signal '%s' deleted'\n", mod_name);
	}
	return retval;
    } else {
	/* build a list of signal(s) to delete */
	n = 0;
	rtapi_mutex_get(&(hal_data->mutex));

	next = hal_data->sig_list_ptr;
	while (next != 0) {
	    sig = SHMPTR(next);
	    /* we want to unload this signal, remember its name */
	    if ( n < ( MAX_EXPECTED_SIGS - 1 ) ) {
	        strncpy(sigs[n], sig->name, HAL_NAME_LEN );
		sigs[n][HAL_NAME_LEN] = '\0';
		n++;
	    }
	    next = sig->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	sigs[n][0] = '\0';

	if ( sigs[0][0] == '\0' ) {
	    /* desired signals not found */
	    halcmd_error("no signals found to be deleted\n");
	    return -1;
	}
	/* we now have a list of components, unload them */
	n = 0;
	retval1 = 0;
	while ( sigs[n][0] != '\0' ) {
	    retval = hal_signal_delete(sigs[n]);
	/* check for fatal error */
	    if ( retval < -1 ) {
		return retval;
	    }
	    /* check for other error */
	    if ( retval != 0 ) {
		retval1 = retval;
	    }
	    if (retval == 0) {
		/* print success message */
		halcmd_info("Signal '%s' deleted'\n",
		sigs[n]);
	    }
	    n++;
	}
    }
    return retval1;
}

int do_unloadusr_cmd(char *mod_name)
{
    int next, all;
    hal_comp_t *comp;
    pid_t ourpid = getpid();

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) == 0 ) {
	all = 1;
    } else {
	all = 0;
    }
    /* build a list of component(s) to unload */
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	// do this here because hal_exit() wipes
	// the comp->next_ptr field
	next = comp->next_ptr;

	if ((comp->type == TYPE_REMOTE)
	    && comp->pid == 0) {
	    /* found a disowned remote component */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		// we want to unload this component,so hal_exit() it
		// need to temporarily release the mutex
		// because hal_exit() grabs it too
		rtapi_mutex_give(&(hal_data->mutex));
		hal_exit(comp->comp_id);
		rtapi_mutex_get(&(hal_data->mutex));
	    }
	}
	// an owned remote component, or a user component
	// owned by somebody other than us receives a signal
	if (((comp->type == TYPE_REMOTE) && (comp->pid != 0)) ||
	    ((comp->type == TYPE_USER) && comp->pid != ourpid)) {

	    /* found a userspace or remote component besides us */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		/* we want to unload this component, send it SIGTERM */
                kill(abs(comp->pid), SIGTERM);
	    }
	}
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}


int do_unloadrt_cmd(char *mod_name)
{
    int next, retval, retval1, nc, nvt, all;
    hal_comp_t *comp;

    zlist_t *components = zlist_new ();     //  http://api.zeromq.org/czmq3-0:zlist
    zlist_t *vtables = zlist_new ();

    zlist_autofree (components); // normal rtcomps
    zlist_autofree (vtables);    // vtables still referenced

    all = strcmp(mod_name, "all" ) == 0;

    /* build a list of component(s) to unload */
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == TYPE_RT ) {
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		// see if a HAL vtable is exported by this comp, and
		// add to 'unload last' list
		hal_vtable_t *c;
		int next = hal_data->vtable_list_ptr;
		while (next != 0) {
		    c = (hal_vtable_t *) SHMPTR(next);
		    if (comp->comp_id == c->comp_id) {
			zlist_append(vtables, comp->name);
			goto NEXTCOMP;
		    }
		    next = c->next_ptr;
		}
		zlist_append(components, comp->name);
	    }
	}
	NEXTCOMP:
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    nc = zlist_size(components);
    nvt = zlist_size(vtables);

    if (!all && ((nc + nvt) == 0)) {
	halcmd_error("component '%s' is not loaded\n", mod_name);
	retval1 = -1;
	goto EXIT;
    }
    // concat vtables to end of component list
    char *name;
    while ((name = zlist_pop(vtables)) != NULL)
	zlist_append(components, name);

    /* we now have a list of components to do in-order, unload them */
    retval1 = 0;
    while ((name = zlist_pop(components)) != NULL) {
	retval = unloadrt_comp(name);
	/* check for fatal error */
	if ( retval < -1 ) {
	    retval1 = retval;
	    goto EXIT;
	}
	/* check for other error */
	if ( retval != 0 ) {
	    retval1 = retval;
	}
    }
    if (retval1 < 0) {
	halcmd_error("unloadrt failed\n");
    }
 EXIT:
    zlist_destroy (&components);
    zlist_destroy (&vtables);
    return retval1;
}

int do_shutdown_cmd(void)
{
    int retval = rtapi_shutdown(rtapi_instance);
    return retval;
}

int do_ping_cmd(void)
{
    int retval = rtapi_ping(rtapi_instance);
    return retval;
}

static int unloadrt_comp(char *mod_name)
{
    int retval;

    retval = rtapi_unloadrt(rtapi_instance, mod_name);
    if (retval < 0) {
	halcmd_error("error unloading realtime module '%s': rc=%d\n",mod_name, retval);
	halcmd_error("%s\n",rtapi_rpcerror());
    } else {
	halcmd_info("Realtime module '%s' unloaded\n",
		    mod_name);
    }
    return retval;
}

int do_unload_cmd(char *mod_name) {
    if(strcmp(mod_name, "all") == 0) {
        int res = do_unloadusr_cmd(mod_name);
        if(res) return res;
        return do_unloadrt_cmd(mod_name);
    } else {
        hal_comp_t *comp;
        int type = -1;
        rtapi_mutex_get(&(hal_data->mutex));
        comp = halpr_find_comp_by_name(mod_name);
        if(comp) type = comp->type;
        rtapi_mutex_give(&(hal_data->mutex));
        if(type == -1) {
            halcmd_error("component '%s' is not loaded\n",
                mod_name);
            return -1;
        }
	switch (type) {
	case TYPE_RT:
	    return do_unloadrt_cmd(mod_name);
	case TYPE_USER:
	case TYPE_REMOTE:
	    return do_unloadusr_cmd(mod_name);
	case TYPE_HALLIB:
            halcmd_error("the hal_lib component should not be unloaded\n");
            return -1;
	default:
	    return -1;
	}
    }
}

static char *guess_comp_name(char *prog_name)
{
    static char name[HAL_NAME_LEN+1];
    char *last_slash = strrchr(prog_name, '/');
    char *st = last_slash ? last_slash + 1 : prog_name;
    char *last_dot = strrchr(st, '.');
    char *en = last_dot ? last_dot : prog_name + strlen(prog_name);
    size_t len = en-st;

    snprintf(name, sizeof(name), "%.*s", (int)len, st);
    return name;
}

#if 0

loadusr [flags] unix-command
(load Userspace component) Executes the given unix-command, usually to load a userspace component. [flags] may be one or more of:

-W to wait for the component to become ready. The component is assumed to have the same name as the first argument of the command.
-Wn name to wait for the component, which will have the given name.
-w to wait for the program to exit
-i to ignore the program return value (with -w)

#endif

int do_loadusr_cmd(char *args[])
{
    int wait_flag, wait_comp_flag, ignore_flag;
    char *prog_name, *new_comp_name=NULL;
    char *argv[MAX_TOK+1];
    int n, m, retval, status;
    pid_t pid;

    int argc = 0;
    while(args[argc] && *args[argc]) {
	argc++;
    }
    args--; argc++;

    if (hal_get_lock()&HAL_LOCK_LOAD) {
	halcmd_error("HAL is locked, loading of programs is not permitted\n");
	return -EPERM;
    }
    wait_flag = 0;
    wait_comp_flag = 0;
    ignore_flag = 0;
    prog_name = NULL;

    /* check for options (-w, -i, and/or -r) */
    optind = 0;
    while (1) {
	int c = getopt(argc, args, "+wWin:");
	if(c == -1) break;

	switch(c) {
	    case 'w':
		wait_flag = 1; break;
	    case 'W':
		wait_comp_flag = 1; break;
	    case 'i':
		ignore_flag = 1; break;
	    case 'n':
		new_comp_name = optarg; break;
	    default:
		return -EINVAL;
		break;
	}
    }
    /* get program and component name */
    args += optind;
    prog_name = *args++;
    if (prog_name == 0) { return -EINVAL; }
    if(!new_comp_name) {
	new_comp_name = guess_comp_name(prog_name);
    }
    /* prepare to exec() the program */
    argv[0] = prog_name;
    /* loop thru remaining arguments */
    n = 0;
    m = 1;
    while ( args[n] && args[n][0] != '\0' ) {
        argv[m++] = args[n++];
    }
    /* add a NULL to terminate the argv array */
    argv[m] = NULL;
    /* start the child process */
    pid = hal_systemv_nowait(argv);
    /* make sure we reconnected to the HAL */
    if (comp_id < 0) {
	fprintf(stderr, "halcmd: hal_init() failed after fork: %d\n",
	    comp_id );
	exit(-1);
    }
    if ( wait_comp_flag ) {
        int ready = 0, count=0, exited=0;
        hal_comp_t *comp = NULL;
	retval = 0;
        while(!ready && !exited) {
	    /* sleep for 10mS */
            struct timespec ts = {0, 10 * 1000 * 1000};
            nanosleep(&ts, NULL);
	    /* check for program ending */
	    retval = waitpid( pid, &status, WNOHANG );
	    if ( retval != 0 ) {
		exited = 1;
	    }
	    /* check for program becoming ready */
            rtapi_mutex_get(&(hal_data->mutex));
            comp = halpr_find_comp_by_name(new_comp_name);
            if(comp && (comp->state > COMP_INITIALIZING)) {
                ready = 1;
            }
            rtapi_mutex_give(&(hal_data->mutex));
	    /* pacify the user */
            count++;
            if(count == 200) {
                fprintf(stderr, "Waiting for component '%s' to become ready.",
                        new_comp_name);
                fflush(stderr);
            } else if(count > 200 && count % 10 == 0) {
                fprintf(stderr, ".");
                fflush(stderr);
            }
        }
        if (count >= 100) {
	    /* terminate pacifier */
	    fprintf(stderr, "\n");
	}
	/* did it work? */
	if (ready) {
	    halcmd_info("Component '%s' ready\n", new_comp_name);
	} else {
	    if ( retval < 0 ) {
		halcmd_error("\nwaitpid(%d) failed\n", pid);
	    } else {
		halcmd_error("%s exited without becoming ready\n", prog_name);
	    }
	    return -1;
	}
    }
    if ( wait_flag ) {
	/* wait for child process to complete */
	retval = waitpid ( pid, &status, 0 );
	/* check result of waitpid() */
	if ( retval < 0 ) {
	    halcmd_error("waitpid(%d) failed\n", pid);
	    return -1;
	}
	if ( WIFEXITED(status) == 0 ) {
	    halcmd_error("program '%s' did not exit normally\n", prog_name );
	    return -1;
	}
	if ( ignore_flag == 0 ) {
	    retval = WEXITSTATUS(status);
	    if ( retval != 0 ) {
		halcmd_error("program '%s' failed, returned %d\n", prog_name, retval );
		return -1;
	    }
	}
	/* print success message */
	halcmd_info("Program '%s' finished\n", prog_name);
    } else {
	/* print success message */
	halcmd_info("Program '%s' started\n", prog_name);
    }
    return 0;
}


int do_waitusr_cmd(char *arg1, char *arg2)
{
    hal_comp_t *comp;
    int exited;
    char *comp_name, *flag = NULL;
    int ignore = 0;

    if (arg2 == NULL) {
	comp_name = arg1;
    } else {
	comp_name = arg2;
	flag = arg1;
    }
    if (flag) {
	if (!strcmp(flag, "-i"))
	    ignore = 1;
	else {
	    halcmd_error("invalid flag for waitusr: '%s'\n", flag);
	return -EINVAL;
	}
    }
    if ((comp_name == NULL) || (*comp_name == '\0')) {
	halcmd_error("component name missing\n");
	return -EINVAL;
    }
    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);
    if (comp == NULL) {
	rtapi_mutex_give(&(hal_data->mutex));
	if (ignore)
	    return 0;
	halcmd_error("component '%s' not found\n", comp_name);
	return -EINVAL;
    }
    if ((comp->type != TYPE_USER) && (comp->type != TYPE_REMOTE)){
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("'%s' is not a userspace or remote component\n", comp_name);
	return -EINVAL;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    /* let the user know what is going on */
    halcmd_info("Waiting for component '%s'\n", comp_name);
    exited = 0;
    while(!exited) {
	/* sleep for 200mS */
	struct timespec ts = {0, 200 * 1000 * 1000};
	nanosleep(&ts, NULL);
	/* check for component still around */
	rtapi_mutex_get(&(hal_data->mutex));
	comp = halpr_find_comp_by_name(comp_name);
	if(comp == NULL) {
		exited = 1;
	}
	rtapi_mutex_give(&(hal_data->mutex));
    }
    halcmd_info("Component '%s' finished\n", comp_name);
    return 0;
}

static const char *type_name(hal_comp_t *comp){
    switch (comp->type) {
    case TYPE_RT:
	return "RT";
    case TYPE_USER:
	return "User";
    case TYPE_REMOTE:
	return "Rem";
    case TYPE_HALLIB:
	if (comp->pid) return "uHAL";
	return "rHAL";
    default:
	return "***error***";
    }
}

static const char *state_name(int state)
{
    switch (state) {
    case COMP_INITIALIZING:
	return "initializing";
    case COMP_UNBOUND:
	return "unbound";
    case COMP_BOUND:
	return "bound";
    case COMP_READY:
	return "ready";
    default:
	return "**error**";
    }
}

static int inst_count(hal_comp_t *comp)
{
    int n = 0;
    hal_inst_t *start = NULL, *inst;

    while ((inst = halpr_find_inst_by_owning_comp(comp->comp_id, start)) != NULL) {
	start = inst;
	n++;
    }
    return n;
}

static void print_comp_info(char **patterns)
{
    int next;
    hal_comp_t *comp;

    if (scriptmode == 0) {
	halcmd_output("Loaded HAL Components:\n");
	halcmd_output("    ID  Type Flags Inst %-*s PID   State\n", HAL_NAME_LEN, "Name");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	bool has_ctor = (comp->ctor != NULL) ;
	bool has_dtor = (comp->dtor != NULL) ;
	bool is_hallib = (comp->type == TYPE_HALLIB) ;

	if ( match(patterns, comp->name) ) {

	    halcmd_output(" %5d  %-4s %c%c%c%c  %4d %-*s",
			  comp->comp_id,
			  type_name(comp),
			  has_ctor ? 'c': ' ',
			  has_dtor ? 'd': ' ',
			  is_hallib ? 'i': ' ',
			  ' ',
			  inst_count(comp),
			  HAL_NAME_LEN,
			  comp->name);

	    switch (comp->type) {
	    case TYPE_USER:
	    case TYPE_HALLIB:

		halcmd_output(" %-5d %s", comp->pid,
			      state_name(comp->state));
		break;

	    case TYPE_RT:
		halcmd_output(" RT    %s",
			      state_name(comp->state));
		break;

		/* halcmd_output(" HAL   %s", */
		/* 	      state_name(comp->state)); */
		/* break; */

	    case TYPE_REMOTE:
		halcmd_output(" %-5d %s", comp->pid,
			      state_name(comp->state));
		time_t now = time(NULL);
		if (comp->last_update) {

		    halcmd_output(", update:-%ld",-(comp->last_update-now));
		} else
		    halcmd_output(", update:never");

		if (comp->last_bound) {

		    halcmd_output(", bound:%lds",comp->last_bound-now);
		} else
		    halcmd_output(", bound:never");
		if (comp->last_unbound) {
		    time_t now = time(NULL);

		    halcmd_output(", unbound:%lds", comp->last_unbound-now);
		} else
		    halcmd_output(", unbound:never");
		break;
	    default:
		halcmd_output(" %-5s %s", "", state_name(comp->state));
	    }
	    halcmd_output(", u1:%d u2:%d", comp->userarg1, comp->userarg2);
	    halcmd_output("\n");
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_inst_info(char **patterns)
{
    int next;
    hal_comp_t *comp;
    hal_inst_t *inst;

    if (scriptmode == 0) {
	halcmd_output("Instances:\n");
	halcmd_output(" Inst  Comp  Size  %-*s Owner\n", 25, "Name");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->inst_list_ptr;

    while (next != 0) {
	inst = SHMPTR(next);
	comp = halpr_find_comp_by_id(inst->comp_id);

	if ( match(patterns, inst->name) ) {

	    halcmd_output("%5d %5d %5d  %-*s %-*s",
			  inst->inst_id,
			  comp->comp_id,
			  inst->inst_size,
			  25, // HAL_NAME_LEN,
			  inst->name,
			  20, // HAL_NAME_LEN,
			  comp->name);
	    halcmd_output("\n");
	}
	next = inst->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_vtable_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Exported vtables:\n");
	halcmd_output("ID      Name                  Version Refcnt  Context Owner\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    int next = hal_data->vtable_list_ptr;
    while (next != 0) {
	hal_vtable_t *vt = SHMPTR(next);
	if ( match(patterns, vt->name) ) {
	    halcmd_output(" %5d  %-20.20s  %-5d   %-5d",
			  vt->handle, vt->name, vt->version, vt->refcount);
	    if (vt->context == 0)
		halcmd_output("   RT   ");
	    else
		halcmd_output("   %-5d", vt->context);
	    hal_comp_t *comp = halpr_find_comp_by_id(vt->comp_id);
	    if (comp) {
                halcmd_output("   %-5d %-30.30s", comp->comp_id,  comp->name);
	    } else {
                halcmd_output("   * not owned by a component *");
	    }
	    halcmd_output("\n");
	}
	next = vt->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}


static void print_pin_info(int type, char **patterns)
{
    int next;
    hal_pin_t *pin;
    hal_comp_t *comp;
    hal_sig_t *sig;
    void *dptr;

    if (scriptmode == 0) {
	halcmd_output("Component Pins:\n");
	halcmd_output("  Comp   Inst Type  Dir         Value  Name                             Epsilon         Flags\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( tmatch(type, pin->type) && match(patterns, pin->name) ) {
	    comp = halpr_find_owning_comp(pin->owner_id);
	    if (pin->signal != 0) {
		sig = SHMPTR(pin->signal);
		dptr = SHMPTR(sig->data_ptr);
	    } else {
		sig = 0;
		dptr = &(pin->dummysig);
	    }
	    if (scriptmode == 0) {

		halcmd_output(" %5d  ", comp->comp_id);
		if (comp->comp_id == pin->owner_id)
		    halcmd_output("     ");
		else
		    halcmd_output("%5d", pin->owner_id);

		if (pin->type == HAL_FLOAT) {
		    halcmd_output(" %5s %-3s  %9s  %-30.30s\t%f\t%d",
				  data_type((int) pin->type),
				  pin_data_dir((int) pin->dir),
				  data_value((int) pin->type, dptr),
				  pin->name,
				  hal_data->epsilon[pin->eps_index],
				  pin->flags);
		} else {
		    halcmd_output(" %5s %-3s  %9s  %-30.30s\t\t\t%d",
				  data_type((int) pin->type),
				  pin_data_dir((int) pin->dir),
				  data_value((int) pin->type, dptr),
				  pin->name,
				  pin->flags);
		}
	    } else {
		halcmd_output("%s %s %s %s %-30.30s",
			      comp->name,
			      data_type((int) pin->type),
			      pin_data_dir((int) pin->dir),
			      data_value2((int) pin->type, dptr),
			      pin->name);
	    }
	    if (sig == 0) {
		halcmd_output("\n");
	    } else {
		halcmd_output(" %s %s\n", data_arrow1((int) pin->dir), sig->name);
	    }
#ifdef DEBUG
	    halcmd_output("%s %d:%d sig=%p dptr=%p *dptr=%p\n",
			  pin->name, pin->signal_inst,pin->signal,
			  sig, dptr, *((void **)dptr));
#endif
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_pin_aliases(char **patterns)
{
    int next;
    hal_oldname_t *oldname;
    hal_pin_t *pin;

    if (scriptmode == 0) {
	halcmd_output("Pin Aliases:\n");
	halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( pin->oldname != 0 ) {
	    /* name is an alias */
	    oldname = SHMPTR(pin->oldname);
	    if ( match(patterns, pin->name) || match(patterns, oldname->name) ) {
		if (scriptmode == 0) {
		    halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, pin->name, oldname->name);
		} else {
		    halcmd_output(" %s  %s\n", pin->name, oldname->name);
		}
	    }
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}
static void print_sig_info(int type, char **patterns)
{
    int next;
    hal_sig_t *sig;
    void *dptr;
    hal_pin_t *pin;

    if (scriptmode != 0) {
    	print_script_sig_info(type, patterns);
	return;
    }
    halcmd_output("Signals:\n");
    halcmd_output("Type          Value  Name     (linked to)\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if ( tmatch(type, sig->type) && match(patterns, sig->name) ) {
	    dptr = SHMPTR(sig->data_ptr);
	    halcmd_output("%s  %s  %s\n", data_type((int) sig->type),
		data_value((int) sig->type, dptr), sig->name);
	    /* look for pin(s) linked to this signal */
	    pin = halpr_find_pin_by_sig(sig, 0);
	    while (pin != 0) {
		halcmd_output("                         %s %s\n",
		    data_arrow2((int) pin->dir), pin->name);
		pin = halpr_find_pin_by_sig(sig, pin);
	    }
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}


static void print_script_sig_info(int type, char **patterns)
{
    int next;
    hal_sig_t *sig;
    void *dptr;
    hal_pin_t *pin;

    if (scriptmode == 0) {
    	return;
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if ( tmatch(type, sig->type) && match(patterns, sig->name) ) {
	    dptr = SHMPTR(sig->data_ptr);
	    halcmd_output("%s  %s  %s", data_type((int) sig->type),
		data_value2((int) sig->type, dptr), sig->name);
	    /* look for pin(s) linked to this signal */
	    pin = halpr_find_pin_by_sig(sig, 0);
	    while (pin != 0) {
		halcmd_output(" %s %s",
		    data_arrow2((int) pin->dir), pin->name);
		pin = halpr_find_pin_by_sig(sig, pin);
	    }
	    halcmd_output("\n");
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_param_info(int type, char **patterns)
{
    int next;
    hal_param_t *param;
    hal_comp_t *comp;

    if (scriptmode == 0) {
	halcmd_output("Parameters:\n");
	halcmd_output(" Comp    Inst Type   Dir         Value  Name\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( tmatch(type, param->type), match(patterns, param->name) ) {
	    comp =  halpr_find_owning_comp(param->owner_id);
	    if (scriptmode == 0) {


		halcmd_output(" %5d  ", comp->comp_id);
		if (comp->comp_id == param->owner_id)
		    halcmd_output("     ");
		else
		    halcmd_output("%5d", param->owner_id);



		halcmd_output("  %5s %-3s  %9s  %s\n",
			      data_type((int) param->type),
			      param_data_dir((int) param->dir),
			      data_value((int) param->type, SHMPTR(param->data_ptr)),
			      param->name);
	    } else {
		halcmd_output("%s %s %s %s %s\n",
			      comp->name, data_type((int) param->type),
			      param_data_dir((int) param->dir),
			      data_value2((int) param->type, SHMPTR(param->data_ptr)),
			      param->name);
	    }
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_param_aliases(char **patterns)
{
    int next;
    hal_oldname_t *oldname;
    hal_param_t *param;

    if (scriptmode == 0) {
	halcmd_output("Parameter Aliases:\n");
	halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( param->oldname != 0 ) {
	    /* name is an alias */
	    oldname = SHMPTR(param->oldname);
	    if ( match(patterns, param->name) || match(patterns, oldname->name) ) {
		if (scriptmode == 0) {
		    halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, param->name, oldname->name);
		} else {
		    halcmd_output(" %s  %s\n", param->name, oldname->name);
		}
	    }
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static const char *ftype(int ft)
{
    switch (ft) {
    case FS_LEGACY_THREADFUNC: return "thread";
    case FS_XTHREADFUNC: return "xthread";
    case FS_USERLAND: return "user";
    default: return "*invalid*";
    }
}

static void print_funct_info(char **patterns)
{
    int next;
    hal_funct_t *fptr;
    hal_comp_t *comp;

    if (scriptmode == 0) {
	halcmd_output("Exported Functions:\n");
	halcmd_output("  Comp   Inst CodeAddr  Arg       FP   Users Type    Name\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	fptr = SHMPTR(next);
	if ( match(patterns, fptr->name) ) {
	    comp =  halpr_find_owning_comp(fptr->owner_id);
	    if (scriptmode == 0) {

		halcmd_output(" %5d  ", comp->comp_id);
		if (comp->comp_id == fptr->owner_id)
		    halcmd_output("     ");
		else
		    halcmd_output("%5d", fptr->owner_id);
		halcmd_output(" %08lx  %08lx  %-3s  %5d %-7s %s\n",

			      (long)fptr->funct.l,
			      (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
			      fptr->users,
			      ftype(fptr->type),
			      fptr->name);
	    } else {
		halcmd_output("%s %08lx %08lx %s %3d %s\n",
		    comp->name,
		    (long)fptr->funct.l,
		    (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
		    fptr->users, fptr->name);
	    }
	}
	next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_thread_stats(hal_thread_t *tptr)
{
    int flavor = global_data->rtapi_thread_flavor;
    rtapi_threadstatus_t *ts =
	&global_data->thread_status[tptr->task_id];

    halcmd_output("\nLowlevel thread statistics for '%s':\n\n",
		  tptr->name);

    // generic statistics counters
    halcmd_output("    updates=%d\t", ts->num_updates);
    if (ts->num_updates) {
	halcmd_output("api_err=%d\t", ts->api_errors);
	halcmd_output("other_err=%d\n", ts->api_errors);
    }

    // flavor-specific statistics counters
    switch (flavor) {
    case RTAPI_XENOMAI_ID: // xenomai-user
    case RTAPI_XENOMAI_KERNEL_ID:

	halcmd_output("    wait_errors=%d\t",
		      ts->flavor.xeno.wait_errors);
	halcmd_output("overruns=%d\t",
		      ts->flavor.xeno.total_overruns);
	halcmd_output("modeswitches=%d\t",
		      ts->flavor.xeno.modeswitches);
	halcmd_output("contextswitches=%d\n",
		      ts->flavor.xeno.ctxswitches);
	halcmd_output("    pagefaults=%d\t",
		      ts->flavor.xeno.pagefaults);
	halcmd_output("exectime=%llduS\t",
		      ts->flavor.xeno.exectime/1000);
	halcmd_output("status=0x%x\n",
		      ts->flavor.xeno.status);
	break;

    case RTAPI_POSIX_ID:
    case RTAPI_RT_PREEMPT_ID:
	halcmd_output("    wait_errors=%d\t",
		      ts->flavor.rtpreempt.wait_errors);
	halcmd_output("usercpu=%lduS\t",
		      ts->flavor.rtpreempt.utime_sec * 1000000 +
		      ts->flavor.rtpreempt.utime_usec);
	halcmd_output("syscpu=%lduS\t",
		      ts->flavor.rtpreempt.stime_sec * 1000000 +
		      ts->flavor.rtpreempt.stime_usec);
	halcmd_output("nsigs=%ld\n",
		      ts->flavor.rtpreempt.ru_nsignals);
	halcmd_output("    ivcsw=%ld\t",
		      ts->flavor.rtpreempt.ru_nivcsw -
		      ts->flavor.rtpreempt.startup_ru_nivcsw);
	halcmd_output("    minflt=%ld\t",
		      ts->flavor.rtpreempt.ru_minflt -
		      ts->flavor.rtpreempt.startup_ru_minflt);
	halcmd_output("    majflt=%ld\n",
		      ts->flavor.rtpreempt.ru_majflt -
		      ts->flavor.rtpreempt.startup_ru_majflt);
	break;

    default:
	halcmd_error("halcmd: thread flavor %d stats not implemented\n",
		     flavor);
    }
    halcmd_output("\n");
}

static void print_thread_info(char **patterns)
{
    int next_thread, n;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;
    int named = patterns && patterns[0] && strlen(patterns[0]);

    if (scriptmode == 0) {
	halcmd_output("Realtime Threads (flavor: %s) :\n",  current_flavor->name);
	halcmd_output("     Period  FP     Name               (     Time, Max-Time ) flags\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	if ( match(patterns, tptr->name) ) {
		/* note that the scriptmode format string has no \n */
		// TODO FIXME add thread runtime and max runtime to this print
	    char flags[100];
	    snprintf(flags, sizeof(flags),"%s%s",
		     tptr->flags & TF_NONRT ? "posix ":"",
		     tptr->flags & TF_NOWAIT ? "nowait":"");

	    halcmd_output(((scriptmode == 0) ? "%11ld  %-3s  %20s ( %8ld, %8ld ) %s\n" : "%ld %s %s %ld %ld %s"),
		tptr->period, (tptr->uses_fp ? "YES" : "NO"), tptr->name,
			  (long)tptr->runtime,
			  (long)tptr->maxtime,  flags);

	    list_root = &(tptr->funct_list);
	    list_entry = list_next(list_root);
	    n = 1;
	    while (list_entry != list_root) {
		/* print the function info */
		fentry = (hal_funct_entry_t *) list_entry;
		funct = SHMPTR(fentry->funct_ptr);
		/* scriptmode only uses one line per thread, which contains:
		   thread period, FP flag, name, then all functs separated by spaces  */
		if (scriptmode == 0) {
		    halcmd_output("                 %2d %s\n", n, funct->name);
		} else {
		    halcmd_output(" %s", funct->name);
		}
		n++;
		list_entry = list_next(list_entry);
	    }
	    if (scriptmode != 0) {
		halcmd_output("\n");
	    } else {
		// if a thread name was given, print the flavor specific stats
		if (named)
		    print_thread_stats(tptr);
	    }
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_comp_names(char **patterns)
{
    int next;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( match(patterns, comp->name) ) {
	    halcmd_output("%s ", comp->name);
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_pin_names(char **patterns)
{
    int next;
    hal_pin_t *pin;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( match(patterns, pin->name) ) {
	    halcmd_output("%s ", pin->name);
	}
	next = pin->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_sig_names(char **patterns)
{
    int next;
    hal_sig_t *sig;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->sig_list_ptr;
    while (next != 0) {
	sig = SHMPTR(next);
	if ( match(patterns, sig->name) ) {
	    halcmd_output("%s ", sig->name);
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_param_names(char **patterns)
{
    int next;
    hal_param_t *param;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( match(patterns, param->name) ) {
	    halcmd_output("%s ", param->name);
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_funct_names(char **patterns)
{
    int next;
    hal_funct_t *fptr;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	fptr = SHMPTR(next);
	if ( match(patterns, fptr->name) ) {
	    halcmd_output("%s ", fptr->name);
	}
	next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_thread_names(char **patterns)
{
    int next_thread;
    hal_thread_t *tptr;

    rtapi_mutex_get(&(hal_data->mutex));
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	if ( match(patterns, tptr->name) ) {
	    halcmd_output("%s ", tptr->name);
	}
	next_thread = tptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_lock_status()
{
    int lock;

    lock = hal_get_lock();

    halcmd_output("HAL locking status:\n");
    halcmd_output("  current lock value %d (%02x)\n", lock, lock);

    if (lock == HAL_LOCK_NONE)
	halcmd_output("  HAL_LOCK_NONE - nothing is locked\n");
    if (lock & HAL_LOCK_LOAD)
	halcmd_output("  HAL_LOCK_LOAD    - loading of new components is locked\n");
    if (lock & HAL_LOCK_CONFIG)
	halcmd_output("  HAL_LOCK_CONFIG  - link and addf is locked\n");
    if (lock & HAL_LOCK_PARAMS)
	halcmd_output("  HAL_LOCK_PARAMS  - setting params is locked\n");
    if (lock & HAL_LOCK_RUN)
	halcmd_output("  HAL_LOCK_RUN     - running/stopping HAL is locked\n");
}

static int count_list(int list_root)
{
    int n, next;

    rtapi_mutex_get(&(hal_data->mutex));
    next = list_root;
    n = 0;
    while (next != 0) {
	n++;
	next = *((int *) SHMPTR(next));
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return n;
}

static int count_members()
{
    int n, nextg, nextm;
    hal_group_t *group;
    hal_member_t *member;
    rtapi_mutex_get(&(hal_data->mutex));
    nextg = hal_data->group_list_ptr;
    n = 0;
    while (nextg != 0) {
	group = SHMPTR(nextg);
	nextm = group->member_ptr;
	while (nextm != 0) {
	    member = SHMPTR(nextm);
	    n++;
	    nextm = member->next_ptr;
	}
	nextg = group->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return n;
}

static void print_mem_status()
{
    int active, recycled, next;
    hal_pin_t *pin;
    hal_param_t *param;

    halcmd_output("HAL memory status\n");
    halcmd_output("  used/total shared memory:   %ld/%d\n",
		  (long)(global_data->hal_size - hal_data->shmem_avail),
		  global_data->hal_size);
    // count components
    active = count_list(hal_data->comp_list_ptr);
    recycled = count_list(hal_data->comp_free_ptr);
    halcmd_output("  active/recycled components: %d/%d\n", active, recycled);
    // count pins
    active = count_list(hal_data->pin_list_ptr);
    recycled = count_list(hal_data->pin_free_ptr);
    halcmd_output("  active/recycled pins:       %d/%d\n", active, recycled);
    // count parameters
    active = count_list(hal_data->param_list_ptr);
    recycled = count_list(hal_data->param_free_ptr);
    halcmd_output("  active/recycled parameters: %d/%d\n", active, recycled);
    // count aliases
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    active = 0;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( pin->oldname != 0 ) active++;
	next = pin->next_ptr;
    }
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( param->oldname != 0 ) active++;
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    recycled = count_list(hal_data->oldname_free_ptr);
    halcmd_output("  active/recycled aliases:    %d/%d\n", active, recycled);
    // count signals
    active = count_list(hal_data->sig_list_ptr);
    recycled = count_list(hal_data->sig_free_ptr);
    halcmd_output("  active/recycled signals:    %d/%d\n", active, recycled);
    // count functions
    active = count_list(hal_data->funct_list_ptr);
    recycled = count_list(hal_data->funct_free_ptr);
    halcmd_output("  active/recycled functions:  %d/%d\n", active, recycled);
    // count threads
    active = count_list(hal_data->thread_list_ptr);
    recycled = count_list(hal_data->thread_free_ptr);
    halcmd_output("  active/recycled threads:    %d/%d\n", active, recycled);
    // count groups
    active = count_list(hal_data->group_list_ptr);
    recycled = count_list(hal_data->group_free_ptr);
    halcmd_output("  active/recycled groups:     %d/%d\n", active, recycled);
    // count members
    active = count_members();
    recycled = count_list(hal_data->member_free_ptr);
    halcmd_output("  active/recycled member:     %d/%d\n", active, recycled);

    // count rings
    active = count_list(hal_data->ring_list_ptr);
    recycled = count_list(hal_data->ring_free_ptr);
    halcmd_output("  active/deleted rings:       %d/%d\n", active, recycled);
    halcmd_output("RTAPI message level:  RT:%d User:%d\n",
		  global_data->rt_msg_level, global_data->user_msg_level);
}

/* Switch function for pin/sig/param type for the print_*_list functions */
static const char *data_type(int type)
{
    const char *type_str;

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

static const char *data_type2(int type)
{
    const char *type_str;

    switch (type) {
    case HAL_BIT:
	type_str = "bit";
	break;
    case HAL_FLOAT:
	type_str = "float";
	break;
    case HAL_S32:
	type_str = "s32";
	break;
    case HAL_U32:
	type_str = "u32";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	type_str = "undef";
    }
    return type_str;
}

/* Switch function for pin direction for the print_*_list functions  */
static const char *pin_data_dir(int dir)
{
    const char *pin_dir;

    switch (dir) {
    case HAL_IN:
	pin_dir = "IN";
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
static const char *param_data_dir(int dir)
{
    const char *param_dir;

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
static const char *data_arrow1(int dir)
{
    const char *arrow;

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
static const char *data_arrow2(int dir)
{
    const char *arrow;

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
/* the value is printed in a 12 character wide field */
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
	snprintf(buf, 14, "  %10ld", (long)*((hal_s32_t *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "  0x%08lX", (unsigned long)*((hal_u32_t *) valptr));
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

int do_save_cmd(char *type, char *filename)
{
    FILE *dst;

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (filename == NULL || *filename == '\0' ) {
	dst = stdout;
    } else {
	dst = fopen(filename, "w" );
	if ( dst == NULL ) {
	    halcmd_error("Can't open 'save' destination '%s'\n", filename);
	return -1;
	}
    }
    if (type == 0 || *type == '\0') {
	type = "all";
    }
    if (strcmp(type, "all" ) == 0) {
	/* save everything */
	save_comps(dst);
	save_aliases(dst);
        save_signals(dst, 1);
        save_nets(dst, 3);
	save_params(dst);
	save_threads(dst);
    } else if (strcmp(type, "comp") == 0) {
	save_comps(dst);
    } else if (strcmp(type, "alias") == 0) {
	save_aliases(dst);
    } else if (strcmp(type, "sig") == 0) {
	save_signals(dst, 0);
    } else if (strcmp(type, "signal") == 0) {
	save_signals(dst, 0);
    } else if (strcmp(type, "sigu") == 0) {
	save_signals(dst, 1);
    } else if (strcmp(type, "link") == 0) {
	save_links(dst, 0);
    } else if (strcmp(type, "linka") == 0) {
	save_links(dst, 1);
    } else if (strcmp(type, "net") == 0) {
	save_nets(dst, 0);
    } else if (strcmp(type, "neta") == 0) {
	save_nets(dst, 1);
    } else if (strcmp(type, "netl") == 0) {
	save_nets(dst, 2);
    } else if (strcmp(type, "netla") == 0 || strcmp(type, "netal") == 0) {
	save_nets(dst, 3);
    } else if (strcmp(type, "param") == 0) {
	save_params(dst);
    } else if (strcmp(type, "parameter") == 0) {
	save_params(dst);
    } else if (strcmp(type, "thread") == 0) {
	save_threads(dst);
    } else {
	halcmd_error("Unknown 'save' type '%s'\n", type);
        if (dst != stdout) fclose(dst);
	return -1;
    }
    if (dst != stdout) {
	fclose(dst);
    }
    return 0;
}

int do_newg_cmd(char *group,char **opt)
{
    int arg1 = 0;
    // default to report only changed members
    int arg2 = (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS);

    char *cp;
    int optind = 0;
    while (opt[optind] && strlen(opt[optind])) {
	char *current  = opt[optind++];
	char *saveptr;
	char *s1 = NULL, *s2 = NULL;
	s1 = strtok_r(current, "=", &saveptr);
	if (s1) {
	    s2 = strtok_r(NULL, "=", &saveptr);
	    if (s1 && strlen(s1)) {

		if (s2 && strlen(s2)) {
		    // args of the form key=value
		    if (!strcmp(s1, "timer")) {
			cp = s2;
			arg1 = 	strtol(s2, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("value '%s' invalid for timer=<int> (integer required)\n", s2);
			    return -EINVAL;
			}
		    } else {
			halcmd_error("unrecognized parameter '%s'\n", current);
			return -EINVAL;
		    }
		} else {
		    // keyword-only arguments
		    if (!strcmp(s1, "onchange")) {
			arg2 |= GROUP_REPORT_ON_CHANGE;
		    } else if (!strcmp(s1, "always")) {
			arg2 &= ~GROUP_REPORT_ON_CHANGE;
		    } else if (!strcmp(s1, "monitorall")) {
			arg2 |= GROUP_MONITOR_ALL_MEMBERS;
		    } else if (!strcmp(s1, "reportchanged")) {
			arg2 |= GROUP_REPORT_CHANGED_MEMBERS;
		    } else if (!strcmp(s1, "reportall")) {
			arg2 &= ~GROUP_REPORT_CHANGED_MEMBERS;
		    } else {
			// try to convert from integer
			arg2 = 	strtol(s1, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("not a keyword and integer value '%s' invalid\n",
					 s1);
			    return -EINVAL;
			}
		    }
		}
	    }
	}
    }
    return hal_group_new(group, arg1, arg2);
}

int do_delg_cmd(char *group)
{
    return hal_group_delete(group);
}


int do_newm_cmd(char *group, char *member, char **opt)
{
    int arg1 = MEMBER_MONITOR_CHANGE, retval;
    char *cp;
    int eps_index = 0;
    hal_sig_t *sig;
    hal_group_t *grp;

    rtapi_mutex_get(&(hal_data->mutex));
    sig = halpr_find_sig_by_name(member);
    grp = halpr_find_group_by_name(member);
    rtapi_mutex_give(&(hal_data->mutex));

    if ((sig == NULL) && (grp == NULL)) {
	halcmd_error("member '%s':  no group or signal by that name\n", member);
		return -EINVAL;
    }

    int optind = 0;

    while (opt[optind] && strlen(opt[optind])) {
	char *current  = opt[optind++];
	char *saveptr;
	char *s1 = NULL, *s2 = NULL;
	s1 = strtok_r(current, "=", &saveptr);
	if (s1) {
	    s2 = strtok_r(NULL, "=", &saveptr);
	    if (s1 && strlen(s1)) {

		if (s2 && strlen(s2)) {
		    // args of the form key=value
		    if (!strcmp(s1, "epsilon")) {
			cp = s2;
			eps_index = strtoul(s2, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("value '%s' invalid for epsilon=<int> (integer value required)\n", s2);
			    return -EINVAL;
			}
			if (sig && (sig->type != HAL_FLOAT)) {
			    halcmd_error("epsilon=<int> only makes sense for float signals\n");
			    return -EINVAL;
			}

		    } else {
			halcmd_error("unrecognized parameter '%s'\n", current);
			return -EINVAL;
		    }
		} else {
		    // keyword-only arguments
		    if (!strcmp(s1, "nomonitor")) {
			arg1 &= ~MEMBER_MONITOR_CHANGE;
		    } else {
			// try to convert from integer
			arg1 = 	strtol(s1, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("not a keyword and integer value '%s' invalid\n",
					 s1);
			    return -EINVAL;
			}
		    }
		}
	    }
	}
    }
    retval = hal_member_new(group, member, arg1, eps_index);
    if (retval)
	halcmd_error("'newm %s %s' failed\n", group, member);
    return retval;
}

int do_delm_cmd(char *group, char *member)
{
    return hal_member_delete(group, member);
}

static void print_group_names(char **patterns)
{
    int next_group;
    hal_group_t *gptr;

    rtapi_mutex_get(&(hal_data->mutex));
    next_group = hal_data->group_list_ptr;
    while (next_group != 0) {
	gptr = SHMPTR(next_group);
	if ( match(patterns, gptr->name) ) {
	    halcmd_output("%s ", gptr->name);
	}
	next_group = gptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static int print_member_cb(int level, hal_group_t **groups, hal_member_t *member,
				  void *cb_data)
{
    hal_sig_t *sig = SHMPTR(member->sig_member_ptr);
    void *dptr = SHMPTR(sig->data_ptr);

    halcmd_output("\t%-14.14s  %-6.6s %16.16s 0x%8.8x %f ",
		  sig->name,
		  data_type((int) sig->type),
		  data_value((int) sig->type, dptr),
		  member->userarg1,
		  hal_data->epsilon[member->eps_index]);

    // print stack of nested group references
    while (level) {
	halcmd_output("%s ", groups[level]->name);
	level--;
    }
    halcmd_output("\n");
    return 0;
}


static void print_group_info(char **patterns)
{
    int next_group;
    hal_group_t *gptr;

    rtapi_mutex_get(&(hal_data->mutex));
    next_group = hal_data->group_list_ptr;
    while (next_group != 0) {
	gptr = SHMPTR(next_group);
	if ( match(patterns, gptr->name) ) {
	    halcmd_output("Group name      Arg1       Arg2       Refs\n");

	    halcmd_output("%-15.15s 0x%8.8x 0x%8.8x %d \n",
			  gptr->name, gptr->userarg1, gptr->userarg2,
			  gptr->refcount);
	    if (gptr->member_ptr) {

		if (scriptmode == 0) {
		    halcmd_output("\n\tMember          Type              Value Arg1       Epsilon  Groupref:\n");
		}
		halpr_foreach_member(gptr->name, print_member_cb, NULL,
				     RESOLVE_NESTED_GROUPS);
	    }
	    halcmd_output("\n");

	}
	next_group = gptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}


static void print_eps_info(char **patterns)
{
    rtapi_mutex_get(&(hal_data->mutex));
    int i;

    halcmd_output("Epsilon\tValue\n");
    for (i = 0; i < MAX_EPSILON; i++)
	halcmd_output("%-d\t%f\n", i, hal_data->epsilon[i]);
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

// ring support code

static void print_ring_names(char **patterns)
{
    int next_ring __attribute__((cleanup(halpr_autorelease_mutex)));
    hal_ring_t *rptr;

    rtapi_mutex_get(&(hal_data->mutex));
    next_ring = hal_data->ring_list_ptr;
    while (next_ring != 0) {
	rptr = SHMPTR(next_ring);
	if ( match(patterns, rptr->name) ) {
	    halcmd_output("%s ", rptr->name);
	}
	next_ring = rptr->next_ptr;
    }
    halcmd_output("\n");
}

#ifdef RINGDEBUG
void dump_rings(const char *where, int attach, int detach)
{
    int next,retval;
    hal_ring_t *rptr;
    ringbuffer_t ringbuffer;

    printf("place: %s attach=%d detach=%d\n", where, attach, detach);
    next =  hal_data->ring_list_ptr;
    while (next) {
	rptr = SHMPTR(next);
	printf("name=%s next=%d ring_id=%d owner=%d\n",
	       rptr->name, rptr->next_ptr, rptr->ring_id, rptr->owner);
	if (attach) {
	    if ((retval = rtapi_ring_attach(rptr->ring_id, &ringbuffer, comp_id))) {
		halcmd_error("%s: rtapi_ring_attach(%d) failed ",
			     rptr->name, rptr->ring_id);
	    }
	}
	if (detach) {

	    if ((retval = rtapi_ring_detach(rptr->ring_id, comp_id))) {
		halcmd_error("%s: rtapi_ring_detach(%d) failed ",
			     rptr->name, rptr->ring_id);
	    }
	}
	next = rptr->next_ptr;
    }
}
#endif



static void print_ring_info(char **patterns)
{
    int next_ring, retval;
    hal_ring_t *rptr;
    ringheader_t *rh;
    ringbuffer_t ringbuffer;

    if (scriptmode == 0) {
	halcmd_output("Rings:\n");
	halcmd_output("Name           Size       Type   Rdr Wrt Ref Flags \n");
    }

    //    rtapi_mutex_get(&(hal_data->mutex));
    next_ring = hal_data->ring_list_ptr;
    while (next_ring != 0) {
	rptr = SHMPTR(next_ring);
	if ( match(patterns, rptr->name) ) {
	    unsigned flags;
	    if ((retval = hal_ring_attach(rptr->name, &ringbuffer, &flags))) {
		halcmd_error("%s: hal_ring_attach(%d) failed ",
			     rptr->name, rptr->ring_id);
		goto failed;
	    }
	    rh = ringbuffer.header;
/* Name           Size       Type   Rdr Wrt Ref Flags  */
/* ring_0         16392      record 0   0   2   recmax:16376  */
	    char *rtype = "unknown";
	    switch (rh->type) {
	    case RINGTYPE_RECORD:    rtype = "record"; break;
	    case RINGTYPE_MULTIPART: rtype = "multi"; break;
	    case RINGTYPE_STREAM:    rtype = "stream"; break;
	    }
	    halcmd_output("%-14.14s %-10zu %-6.6s %d/%d %d/%d %-3d",
			  rptr->name,
			  rh->size,
			  rtype,
			  rh->reader,rh->reader_instance,
			  rh->writer,rh->writer_instance,
			  rh->refcount-1);
	    if (rh->use_rmutex)
		halcmd_output(" rmutex");
	    if (rh->use_wmutex )
		halcmd_output(" wmutex");
	    if (rh->type == RINGTYPE_STREAM)
		halcmd_output(" free:%zu ",
			      stream_write_space(rh));
	    else
		halcmd_output(" recmax:%zu ",
			      record_write_space(rh));
	    if (ring_scratchpad_size(&ringbuffer))
		halcmd_output(" scratchpad:%zu ", ring_scratchpad_size(&ringbuffer));
	    halcmd_output("\n");
	    if ((retval = hal_ring_detach(rptr->name,  &ringbuffer)) < 0) {
		halcmd_error("%s: rtapi_ring_detach(%d) failed ",
			     rptr->name, rptr->ring_id);
		goto failed;
	    }
	}
	next_ring = rptr->next_ptr;
    }
 failed:
    halcmd_output("\n");
}

int do_newring_cmd(char *ring, char *ring_size, char **opt)
{
    int size = -1;
    int spsize = 0;
    char *r = ring_size;
    size_t rmax = 50000000;  // XXX: make MAX_RINGSIZE
    char *s;
    unsigned long mode = 0; // defaults
    int i = 0;
    int retval;
    char *cp;

#define SCRATCHPAD "scratchpad="
#define MAX_SPSIZE (1024*1024)

    size = strtol(ring_size, &r, 0);
    if ((*r != '\0') && (!isspace(*r))) {
	halcmd_error("value '%s' invalid for ring size (integer required)\n", ring_size);
	return -EINVAL;
    }
    if (size > rmax) {
	halcmd_error("ring size %d: too large (max=%zu)\n", size,rmax);
	return -EINVAL;
    }
    for (i = 0; ((s = opt[i]) != NULL) && strlen(s); i++) {
	if  (!strcasecmp(s,"rmutex")) {
	    mode |=  USE_RMUTEX;
	}  else if  (!strcasecmp(s,"wmutex")) {
	    mode |=  USE_WMUTEX;
	}  else if  (!strcasecmp(s,"record")) {
	    // default
	}  else if  (!strcasecmp(s,"stream")) {
	    mode |=  RINGTYPE_STREAM;
	} else if (!strncasecmp(s, SCRATCHPAD, strlen(SCRATCHPAD))) {
	    spsize = strtol(strchr(s,'=') + 1, &cp, 0);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		/* invalid chars in string */
		halcmd_error("string '%s' invalid for scratchpad size\n", s);
		retval = -EINVAL;
	    }
	    if ((spsize < 0) || (spsize > MAX_SPSIZE)) {
		halcmd_error("scratchpad size out of bounds (0..%d)\n", MAX_SPSIZE);
		retval = -EINVAL;
	    }
	} else {
	    halcmd_error("newring: invalid option '%s' (use one or several of: record stream"
			 " rtapi hal rmutex wmutex scratchpad=<size>)\n",s);
	    return -EINVAL;
	}
    }
    // this will happen under hal_data->mutex locked
    if ((retval = hal_ring_new(ring, size, spsize, mode))) {
	halcmd_error("newring: failed to create new ring %s: %s\n",
		     ring, strerror(-retval));
	return -EINVAL;
    }
    return 0;
}

int do_delring_cmd(char *ring)
{
    halcmd_output("delring NIY: ring='%s'\n", ring);
    // return halpr_group_delete(group);
    return 0;
}

int do_ringdump_cmd(char *ring)
{
    halcmd_output("ringdump NIY: ring='%s'\n", ring);
    return 0;
}
int do_ringwrite_cmd(char *ring,char *content)
{
    halcmd_output("ringwrite NIY: ring='%s'\n", ring);
    return 0;
}

int do_ringread_cmd(char *ring, char *tokens[])
{
    halcmd_output("ringread NIY: ring='%s'\n", ring);
    return 0;
}
// ----- end ring support

// --- remote comp support

int do_newcomp_cmd(char *comp, char *opt[])
{
    //   char *s , *cp;

    int type = TYPE_REMOTE;
    int arg1 = 0;
    int arg2 = 0;
    char *cp;
    int optind = 0;

    while (opt[optind] && strlen(opt[optind])) {
	char *current  = opt[optind++];
	char *saveptr;
	char *s1 = NULL, *s2 = NULL;
	s1 = strtok_r(current, "=", &saveptr);
	if (s1) {
	    s2 = strtok_r(NULL, "=", &saveptr);
	    if (s1 && strlen(s1)) {

		if (s2 && strlen(s2)) {
		    // args of the form key=value
		    if (!strcmp(s1, "timer")) {
			cp = s2;
			arg1 = 	strtol(s2, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("value '%s' invalid for timer=<int> (integer required)\n", s2);
			    return -EINVAL;
			}
		    } else {
			halcmd_error("unrecognized parameter '%s'\n", current);
			return -EINVAL;
		    }
		} else {
		    // handle keyword-only arguments
		    if (!strcmp(s1, "acceptdefaults")) {
			arg2 |=RCOMP_ACCEPT_VALUES_ON_BIND;
#if 0 // handle more keywords like so:
		    } else if (!strcmp(s1, "thatflag")) {
			arg2 |= RCOMP_THATFLAG;
#endif
		    } else {
			// try to convert from integer
			arg2 = 	strtol(s1, &cp, 0);
			if ((*cp != '\0') && (!isspace(*cp))) {
			    halcmd_error("not a keyword and integer value '%s' invalid\n",
					 s1);
			    return -EINVAL;
			}
		    }
		}
	    }
	}
    }
    int comp_id = hal_xinit(type, arg1, arg2, NULL, NULL, comp);

    if (comp_id < 1) {
	halcmd_error("newcomp: cant create component '%s' type %d: %s\n",
		     comp, type, strerror(-comp_id));
	return -EINVAL;
    }
    return 0;
}


int do_ready_cmd(char *comp_name, char *tokens[])
{
    int retval, comp_id;
    hal_comp_t *comp;

    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);

    if(!comp) {
        halcmd_error( "No such component: %s\n", comp_name);
	rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }
    if(comp->type != TYPE_REMOTE) {
        halcmd_error( "%s is not a remote component\n", comp_name);
	rtapi_mutex_give(&(hal_data->mutex));
        return -ENOSYS;
    }
    comp_id = comp->comp_id;
    rtapi_mutex_give(&(hal_data->mutex));

    retval = hal_ready(comp_id);
    if (retval < 0) {
	halcmd_error("ready: cant hal_ready component '%s':  %s\n",
		     comp_name, strerror(-comp_id));
	return -EINVAL;
    }
    return 0;
}

int do_newpin_cmd(char *comp_name, char *pin_name, char *type_name, char *args[])
{
    int retval;
    hal_type_t type = HAL_BIT; // shut up warnin
    hal_pin_dir_t dir = HAL_IN;
    hal_comp_t *comp __attribute__((cleanup(halpr_autorelease_mutex)));
    hal_pin_t *pin;
    char *s,*cp;
    int flags = 0;
    int eps_index = 0;
    int i;
    void *p;

    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);
    pin = halpr_find_pin_by_name(pin_name);

    if (!comp) {
        halcmd_error("No such component: %s\n", comp_name);
        return -ENOENT;
    }
    if (comp->type != TYPE_REMOTE) {
        halcmd_error( "%s is not a remote component\n", comp_name);
        return -ENOSYS;
    }
    if (pin) {
        halcmd_error("pin '%s' already exists\n", pin_name);
        return -ENOENT;
    }
    if (strcasecmp(type_name, "bit") == 0) {
	type =  HAL_BIT;
    } else if (strcasecmp(type_name, "float") == 0) {
	type = HAL_FLOAT;
    } else if (strcasecmp(type_name, "u32") == 0) {
	type = HAL_U32;
    } else if (strcasecmp(type_name, "s32") == 0) {
	type = HAL_S32;
    } else {
	halcmd_error("Unknown pin type '%s'\n", type_name);
	retval = -EINVAL;
    }
#define EPSILON "eps="
#define FLAGS "flags="

    for (i = 0; ((s = args[i]) != NULL) && strlen(s); i++) {
	if (!strcasecmp(s,"in")) {
	    dir = HAL_IN;
	} else if  (!strcasecmp(s,"out")) {
	    dir = HAL_OUT;
	}  else if  (!strcasecmp(s,"inout") || !strcasecmp(s,"io")) {
	    dir = HAL_IO;
	} else if (!strncasecmp(s, EPSILON, strlen(EPSILON))) {
	    if (type != HAL_FLOAT) {
		halcmd_error("%s make no sense for non-float pins\n", s);
		return -EINVAL;
	    }
	    eps_index = strtoul(strchr(s,'=') + 1, &cp, 0);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		halcmd_error("value '%s' invalid for epsilon (int required)\n", cp);
		return -EINVAL;
	    }
	} else if (!strncasecmp(s, FLAGS, strlen(FLAGS))) {
	    flags = strtoul(strchr(s,'=') + 1, &cp, 10);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		halcmd_error("value '%s' invalid for flags (int required)\n", cp);
		return -EINVAL;
	    }
	} else {
	    halcmd_error("newpin: invalid option '%s' "
			 "(use one or several of: in out io eps=<flaot> flags=<int>)",s);
	    return -EINVAL;
	}
    }
    // hal_malloc wants the mutex too
    rtapi_mutex_give(&(hal_data->mutex));
    if ((p = hal_malloc(sizeof(void *))) == NULL) {
	// grab mutex - autorelease will free it
	rtapi_mutex_get(&(hal_data->mutex));
	halcmd_error("cant allocate memory for pin '%s'\n", pin_name);
	return -EINVAL;
    }
    memset(p, 0, sizeof(void *));

    // mutex not held here - hal_pin_new wants it
    retval = hal_pin_new(pin_name, type, dir, p, comp->comp_id);
    if (retval < 0) {
	// re-grab
	rtapi_mutex_get(&(hal_data->mutex));
	halcmd_error("cant create pin '%s':  %s\n",
		     pin_name, strerror(-retval));
	return -EINVAL;
    }
    rtapi_mutex_get(&(hal_data->mutex));

    pin = halpr_find_pin_by_name(pin_name);
    if (pin) {
	pin->eps_index = eps_index;
	pin->flags = flags;
    } else {
	halcmd_error("FATAL: cant find new pin '%s':  %s\n",
		     pin_name, strerror(-retval));
	return -EINVAL;
    }
    return 0;
}

static int do_wait_remote(char *comp_name, int state)

{
    hal_comp_t *comp;
    int done = 0;

    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);

    if(!comp) {
        halcmd_error( "No such component: %s\n", comp_name);
	rtapi_mutex_give(&(hal_data->mutex));
        return -ENOENT;
    }
    if(comp->type != TYPE_REMOTE) {
        halcmd_error( "%s is not a remote component\n", comp_name);
	rtapi_mutex_give(&(hal_data->mutex));
        return -ENOSYS;
    }
    if (comp->state == state) {
	rtapi_mutex_give(&(hal_data->mutex));
        return 0;
    }
    halcmd_info("Waiting for component '%s' to %sbind\n",
		comp_name,
		state == COMP_BOUND ? "" : "un");

    rtapi_mutex_give(&(hal_data->mutex));
    do {
	/* sleep for 200mS */
	struct timespec ts = {0, 200 * 1000 * 1000};
	nanosleep(&ts, NULL);
	rtapi_mutex_get(&(hal_data->mutex));
	if (comp->state == state) {
	    done = 1;
	}
	rtapi_mutex_give(&(hal_data->mutex));
    } while (!done);
    halcmd_info("Component '%s' %sbound\n", comp_name,
		state == COMP_BOUND ? "" : "un");
    return 0;
}

int do_waitexists_cmd(char *comp_name)
{
    hal_comp_t *comp;
    int done = 0;

    halcmd_info("Waiting for component '%s' to be created\n", comp_name);
    do {
	struct timespec ts = {0, 200 * 1000 * 1000};
	nanosleep(&ts, NULL);
	rtapi_mutex_get(&(hal_data->mutex));
	comp = halpr_find_comp_by_name(comp_name);
	if (comp != NULL) {
	    done = 1;
	}
	rtapi_mutex_give(&(hal_data->mutex));
    } while (!done);
    halcmd_info("Component '%s' now exists\n", comp_name);
    return 0;
}

int do_waitbound_cmd(char *comp_name, char *tokens[])
{
    return do_wait_remote(comp_name,  COMP_BOUND);
}

int do_waitunbound_cmd(char *comp_name, char *tokens[])
{
    return do_wait_remote(comp_name,  COMP_UNBOUND);
}
// --- end remote comp support

int do_callfunc_cmd(char *func, char *args[])
{
    int retval = rtapi_callfunc(rtapi_instance, func, (const char **)args);
    if ( retval < 0 ) {
	halcmd_error("function call %s returned %d\n%s", func, retval, rtapi_rpcerror());
	return retval;
    }
    halcmd_info("function '%s' returned %d\n", func, retval);
    return 0;
}

typedef enum {
    CS_NOT_LOADED,
    CS_NOT_RT,
    CS_RTLOADED_NOT_INSTANTIABLE,
    CS_RTLOADED_AND_INSTANTIABLE
} cstatus_t;

cstatus_t classify_comp(const char *comp)
{
    CHECK_HALDATA();
    CHECK_STR(comp);
    {
	WITH_HAL_MUTEX();
	hal_comp_t *c = halpr_find_comp_by_name(comp);
	if (c == NULL)
	    return CS_NOT_LOADED;
	if (c->type != TYPE_RT)
	    return CS_NOT_RT;
	if (c->ctor == NULL)
	    return CS_RTLOADED_NOT_INSTANTIABLE;
    }
    return CS_RTLOADED_AND_INSTANTIABLE;
}

int do_newinst_cmd(char *comp, char *inst, char *args[])
{
    int retval;
    cstatus_t status = classify_comp(comp);
    char *argv[] = { NULL};
    bool singleton = false;

    switch (status) {
    case CS_NOT_LOADED:
	if (autoload) {
	    retval = loadrt_cmd(false, comp, argv);
	    if (retval)
		return retval;
	    return do_newinst_cmd(comp, inst,  args);
	    break;
	}
	halcmd_error("component '%s' not loaded\n", comp);
	break;

    case CS_NOT_RT:
	halcmd_error("'%s' not an RT component\n", comp);
	return -EINVAL;
	break;

    case  CS_RTLOADED_NOT_INSTANTIABLE:
	halcmd_error("legacy component '%s' loaded, but not instantiable\n", comp);
	return -EINVAL;
	break;

    case CS_RTLOADED_AND_INSTANTIABLE:
	// we're good
	break;
    }

    if (hal_get_lock() & HAL_LOCK_LOAD) {
        halcmd_error("HAL is locked, loading of modules is not permitted\n");
        return -EPERM;
    }

    retval = rtapi_get_tags(comp);
    if (retval == -1) {
        halcmd_error("Error in module tags search");
        return retval;
    } else {
        if ((retval & HC_SINGLETON) == HC_SINGLETON)
            singleton = true;
    }

    //  If a singleton is created via a direct loadrt, it will have the same name
    //  as the component.  If created by newinst, it could have any name.
    //  Try to prevent more than one singleton being created using newinst afterwards.
    if (singleton) {
	WITH_HAL_MUTEX();
        hal_comp_t *existing_comp = halpr_find_comp_by_name(comp);
        if (inst_name_exists(comp) || inst_count(existing_comp)) {
	    halcmd_error("Singleton components cannot have multiple instances\n\n");
	    return -1;
	}
    }

    retval = rtapi_newinst(rtapi_instance, comp, inst, (const char **)args);
    if (retval) {
	halcmd_error("rc=%d\n%s", retval, rtapi_rpcerror());
	return retval;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////


int do_delinst_cmd(char *inst)
{
    {
	hal_inst_t *hi  __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));
	hi = halpr_find_inst_by_name(inst);

	if (hi == NULL) {
	    halcmd_error("no such instance: '%s'\n", inst);
	    return -1;
	}
    }
    int retval = rtapi_delinst(rtapi_instance, inst);
    if ( retval != 0 ) {
	halcmd_error("rc=%d\n%s", retval, rtapi_rpcerror());
	return retval;
    }
    return 0;
}

static void print_inst_names(char **patterns)
{
    hal_inst_t *start  __attribute__((cleanup(halpr_autorelease_mutex))) = NULL, *inst;
    rtapi_mutex_get(&(hal_data->mutex));

    while ((inst = halpr_find_inst_by_owning_comp(-1, start)) != NULL) {
	if ( match(patterns, inst->name) ) {
	    halcmd_output("%s ", inst->name);
	}
	start = inst;
    }
    halcmd_output("\n");
}

int do_sleep_cmd(char *naptime)
{
    char *cp = naptime;
    double duration = strtod ( naptime, &cp );
    if ((*cp != '\0') && (!isspace(*cp))) {
	/* invalid character(s) in string */
	halcmd_error("value '%s' invalid for sleep time\n", naptime);
	return -EINVAL;
    }
    if (duration < 0) {
	halcmd_error("sleep time must be > 0: '%s' \n", naptime);
	return -EINVAL;
    }
    halcmd_info("sleeping for %f seconds\n", duration);
    struct timespec ts;
    ts.tv_sec = floorl(duration);
    ts.tv_nsec = (duration - ts.tv_sec) *  1000 * 1000;
    nanosleep(&ts, NULL);
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
	if ( comp->type == TYPE_RT ) {

	    // FIXME XXX MAH - save halcmd defined remote comps!!
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
#if 0  /* newinst deferred to version 2.2 */
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 2 ) {
            hal_comp_t *comp1 = halpr_find_comp_by_id(comp->comp_id & 0xffff);
            fprintf(dst, "newinst %s %s\n", comp1->name, comp->name);
        }
	next = comp->next_ptr;
    }
#endif
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_aliases(FILE *dst)
{
    int next;
    hal_pin_t *pin;
    hal_param_t *param;
    hal_oldname_t *oldname;

    fprintf(dst, "# pin aliases\n");
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( pin->oldname != 0 ) {
	    /* name is an alias */
	    oldname = SHMPTR(pin->oldname);
	    fprintf(dst, "alias pin %s %s\n", oldname->name, pin->name);
	}
	next = pin->next_ptr;
    }
    fprintf(dst, "# param aliases\n");
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( param->oldname != 0 ) {
	    /* name is an alias */
	    oldname = SHMPTR(param->oldname);
	    fprintf(dst, "alias param %s %s\n", oldname->name, param->name);
	}
	next = param->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_signals(FILE *dst, int only_unlinked)
{
    int next;
    hal_sig_t *sig;

    fprintf(dst, "# signals\n");
    rtapi_mutex_get(&(hal_data->mutex));

    for( next = hal_data->sig_list_ptr; next; next = sig->next_ptr) {
	sig = SHMPTR(next);
        if(only_unlinked && (sig->readers || sig->writers)) continue;
	fprintf(dst, "newsig %s %s\n", sig->name, data_type((int) sig->type));
    }
    rtapi_mutex_give(&(hal_data->mutex));
}

static void save_links(FILE *dst, int arrow)
{
    int next;
    hal_pin_t *pin;
    hal_sig_t *sig;
    const char *arrow_str;

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
    const char *arrow_str;

    fprintf(dst, "# nets\n");
    rtapi_mutex_get(&(hal_data->mutex));

    for (next = hal_data->sig_list_ptr; next != 0; next = sig->next_ptr) {
	sig = SHMPTR(next);
        if(arrow == 3) {
            int state = 0, first = 1;

            /* If there are no pins connected to this signal, do nothing */
            pin = halpr_find_pin_by_sig(sig, 0);
            if(!pin) continue;

            fprintf(dst, "net %s", sig->name);

            /* Step 1: Output pin, if any */

            for(pin = halpr_find_pin_by_sig(sig, 0); pin;
                    pin = halpr_find_pin_by_sig(sig, pin)) {
                if(pin->dir != HAL_OUT) continue;
                fprintf(dst, " %s", pin->name);
                state = 1;
            }

            /* Step 2: I/O pins, if any */
            for(pin = halpr_find_pin_by_sig(sig, 0); pin;
                    pin = halpr_find_pin_by_sig(sig, pin)) {
                if(pin->dir != HAL_IO) continue;
                fprintf(dst, " ");
                if(state) { fprintf(dst, "=> "); state = 0; }
                else if(!first) { fprintf(dst, "<=> "); }
                fprintf(dst, "%s", pin->name);
                first = 0;
            }
            if(!first) state = 1;

            /* Step 3: Input pins, if any */
            for(pin = halpr_find_pin_by_sig(sig, 0); pin;
                    pin = halpr_find_pin_by_sig(sig, pin)) {
                if(pin->dir != HAL_IN) continue;
                fprintf(dst, " ");
                if(state) { fprintf(dst, "=> "); state = 0; }
                fprintf(dst, "%s", pin->name);
            }

            fprintf(dst, "\n");
        } else if(arrow == 2) {
            /* If there are no pins connected to this signal, do nothing */
            pin = halpr_find_pin_by_sig(sig, 0);
            if(!pin) continue;

            fprintf(dst, "net %s", sig->name);
            pin = halpr_find_pin_by_sig(sig, 0);
            while (pin != 0) {
                fprintf(dst, " %s", pin->name);
                pin = halpr_find_pin_by_sig(sig, pin);
            }
            fprintf(dst, "\n");
        } else {
            fprintf(dst, "newsig %s %s\n",
                    sig->name, data_type((int) sig->type));
            pin = halpr_find_pin_by_sig(sig, 0);
            while (pin != 0) {
                if (arrow != 0) {
                    arrow_str = data_arrow2((int) pin->dir);
                } else {
                    arrow_str = "\0";
                }
                fprintf(dst, "linksp %s %s %s\n",
                        sig->name, arrow_str, pin->name);
                pin = halpr_find_pin_by_sig(sig, pin);
            }
        }
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
	    /* param is writable, save its value */
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

int do_setexact_cmd() {
    int retval = 0;
    rtapi_mutex_get(&(hal_data->mutex));
    if(hal_data->base_period) {
        halcmd_error(
            "HAL_LIB: Cannot run 'setexact'"
            " after a thread has been created\n");
        retval = -EINVAL;
    } else {
        halcmd_warning(
            "HAL_LIB: HAL will pretend that the exact"
            " base period requested is possible.\n"
            "This mode is not suitable for running real hardware.\n");
        hal_data->exact_base_period = 1;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return retval;
}

// create a new named RT thread
int do_newthread_cmd(char *name, char *period, char *args[])
{
    int i, retval;
    bool use_fp = false;
    int cpu = -1;
    const char *s;
    int per = atoi(period);
    int flags = 0;

    for (i = 0; ((s = args[i]) != NULL) && strlen(s); i++) {
	if (sscanf(s, "cpu=%d", &cpu) == 1)
	    continue;
	if (strcmp(s, "fp") == 0)
	    use_fp = true;
	if (strcmp(s, "nofp") == 0)
	    use_fp = false;
	if (strcmp(s, "posix") == 0)
	    flags |= TF_NONRT;
	if (strcmp(s, "nowait") == 0)
	    flags |= TF_NOWAIT;
    }

    if ((per < 10000) && !(flags & TF_NOWAIT))
	halcmd_warning("a period < 10uS is unlikely to work\n");
    if ((flags & (TF_NOWAIT|TF_NONRT)) == TF_NOWAIT){
	halcmd_error("specifying 'nowait' without 'posix' will likely lock up RT\n");
	return -EINVAL;
    }

    retval = rtapi_newthread(rtapi_instance, name, per, cpu, (int)use_fp, flags);
    if (retval)
	halcmd_error("%s\n",rtapi_rpcerror());

    return retval;
}


// delete an RT thread
int do_delthread_cmd(char *name)
{
    int retval =  rtapi_delthread(rtapi_instance, name);
    if (retval)
	halcmd_error("%s\n",rtapi_rpcerror());
    return retval;
}

int do_help_cmd(char *command)
{
    if (!command) {
        print_help_commands();
    } else if (strcmp(command, "help") == 0) {
	printf("If you need help to use 'help', then I can't help you.\n");
    } else if (strcmp(command, "loadrt") == 0) {
	printf("loadrt modname [modarg(s)]\n");
	printf("  Loads realtime HAL module 'modname', passing 'modargs'\n");
	printf("  to the module.\n");
#if 0  /* newinst deferred to version 2.2 */
    } else if (strcmp(command, "newinst") == 0) {
	printf("newinst modname instname\n");
	printf("  Creates another instance of previously loaded module\n" );
	printf("  'modname', nameing it 'instname'.\n");
#endif
    } else if (strcmp(command, "unload") == 0) {
	printf("unload compname\n");
	printf("  Unloads HAL module 'compname', whether user space or realtime.\n");
        printf("  If 'compname' is 'all', unloads all components.\n");
    } else if (strcmp(command, "waitusr") == 0) {
	printf("waitusr compname\n");
	printf("  Waits for user space HAL module 'compname' to exit.\n");
    } else if (strcmp(command, "unloadusr") == 0) {
	printf("unloadusr compname\n");
	printf("  Unloads user space HAL module 'compname'.  If 'compname'\n");
	printf("  is 'all', unloads all userspace components.\n");
    } else if (strcmp(command, "unloadrt") == 0) {
	printf("unloadrt modname\n");
	printf("  Unloads realtime HAL module 'modname'.  If 'modname'\n");
	printf("  is 'all', unloads all realtime modules.\n");
    } else if (strcmp(command, "loadusr") == 0) {
	printf("loadusr [options] progname [progarg(s)]\n");
	printf("  Starts user space program 'progname', passing\n");
	printf("  'progargs' to it.  Options are:\n");
	printf("  -W  wait for HAL component to become ready\n");
	printf("  -Wn name to wait for the component, which will have the given name.\n");
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
    } else if(strcmp(command, "net") == 0) {
        printf("net signame pinname ...\n");
        printf("Creates 'signame' with the type of 'pinname' if it does not yet exist\n");
        printf("And then links signame to each pinname specified.\n");
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
	printf("  Creates a new signal called 'signame'.  Type\n");
	printf("  is 'bit', 'float', 'u32', or 's32'.\n");
    } else if (strcmp(command, "delsig") == 0) {
	printf("delsig signame\n");
	printf("  Deletes signal 'signame'.  If 'signame is 'all',\n");
	printf("  deletes all signals\n");
    } else if (strcmp(command, "setp") == 0) {
	printf("setp paramname value\n");
	printf("setp pinname value\n");
	printf("paramname = value\n");
	printf("  Sets parameter 'paramname' to 'value' (if writable).\n");
	printf("  Sets pin 'pinname' to 'value' (if an unconnected input).\n");
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
	printf("  Sets signal 'signame' to 'value' (if signal has no writers).\n");
    } else if (strcmp(command, "getp") == 0) {
	printf("getp paramname\n");
	printf("getp pinname\n");
	printf("  Gets the value of parameter 'paramname' or pin 'pinname'.\n");
    } else if (strcmp(command, "ptype") == 0) {
	printf("ptype paramname\n");
	printf("ptype pinname\n");
	printf("  Gets the type of parameter 'paramname' or pin 'pinname'.\n");
    } else if (strcmp(command, "gets") == 0) {
	printf("gets signame\n");
	printf("  Gets the value of signal 'signame'.\n");
    } else if (strcmp(command, "stype") == 0) {
	printf("stype signame\n");
	printf("  Gets the type of signal 'signame'\n");
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
	printf("  pattern, which may be a 'shell glob'.\n");
    } else if (strcmp(command, "list") == 0) {
	printf("list type [pattern]\n");
	printf("  Prints the names of HAL items of the specified type.\n");
	printf("  'type' is 'comp', 'pin', 'sig', 'param', 'funct', or\n");
	printf("  'thread'.  If 'pattern' is specified it prints only\n");
	printf("  those names that match the pattern, which may be a\n");
	printf("  'shell glob'.\n");
	printf("  For 'sig', 'pin' and 'param', the first pattern may be\n");
	printf("  -tdatatype where datatype is the data type (e.g., 'float')\n");
	printf("  in this case, the listed pins, signals, or parameters\n");
	printf("  are restricted to the given data type\n");
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
	printf("  \"halcmd -f filename\".\n");
	printf("  Type can be 'comp', 'sig', 'link[a]', 'net[a]', 'netl', 'param',\n");
	printf("  or 'thread'.  ('linka' and 'neta' show arrows for pin\n");
	printf("  direction.)  If 'type' is omitted or 'all', does the\n");
	printf("  equivalent of 'comp', 'netl', 'param', and 'thread'.\n");
    } else if (strcmp(command, "start") == 0) {
	printf("start\n");
	printf("  Starts all realtime threads.\n");
    } else if (strcmp(command, "stop") == 0) {
	printf("stop\n");
	printf("  Stops all realtime threads.\n");
    } else if (strcmp(command, "quit") == 0) {
	printf("quit\n");
	printf("  Stop processing input and terminate halcmd (when\n");
	printf("  reading from a file or stdin).\n");
    } else if (strcmp(command, "exit") == 0) {
	printf("exit\n");
	printf("  Stop processing input and terminate halcmd (when\n");
	printf("  reading from a file or stdin).\n");
    } else if (strcmp(command, "alias") == 0) {
        printf("alias type name alias\n");
        printf("  Assigns \"alias\" as a second name for the pin or parameter\n");
        printf("  \"name\".  For most operations, an alias provides a second\n");
        printf("  name that can be used to refer to a pin or parameter, both the\n");
        printf("  original name and the alias will work.\n");
        printf("  \"type\" must be pin or param\n");
        printf("  \"name\" must be an existing name or alias of the specified type.\n");
    } else if (strcmp(command, "unalias") == 0) {
        printf("unalias type name\n");
        printf("  Removes any alias from the pin or parameter \"name\".\n");
        printf("  \"type\" must be pin or param\n");
        printf("  \"name\" must be an existing name or alias of the specified type.\n");
    } else if (strcmp(command, "echo") == 0) {
        printf("echo\n");
        printf("echo the commands from stdin to stderr\n");
        printf("Useful for debugging scripted commands from a running program\n");
    } else if (strcmp(command, "unecho") == 0) {
        printf("unecho\n");
        printf("Turn off echo of commands from stdin to stdout\n");

    } else {
	printf("No help for unknown command '%s'\n", command);
    }
    return 0;
}


static void print_help_commands(void)
{
    printf("Use 'help <command>' for more details about each command\n");
    printf("Available commands:\n");
    printf("  loadrt              Load realtime module(s)\n");
    printf("  loadusr             Start user space program\n");
    printf("  waitusr             Waits for userspace component to exit\n");
    printf("  unload              Unload realtime module or terminate userspace component\n");
    printf("  lock, unlock        Lock/unlock HAL behaviour\n");
    printf("  linkps              Link pin to signal\n");
    printf("  linksp              Link signal to pin\n");
    printf("  net                 Link a number of pins to a signal\n");
    printf("  unlinkp             Unlink pin\n");
    printf("  newsig, delsig      Create/delete a signal\n");
    printf("  getp, gets          Get the value of a pin, parameter or signal\n");
    printf("  ptype, stype        Get the type of a pin, parameter or signal\n");
    printf("  setp, sets          Set the value of a pin, parameter or signal\n");
    printf("  addf, delf          Add/remove function to/from a thread\n");
    printf("  show                Display info about HAL objects\n");
    printf("  list                Display names of HAL objects\n");
    printf("  source              Execute commands from another .hal file\n");
    printf("  status              Display status information\n");
    printf("  save                Print config as commands\n");
    printf("  start, stop         Start/stop realtime threads\n");
    printf("  alias, unalias      Add or remove pin or parameter name aliases\n");
    printf("  echo, unecho        Echo commands from stdin to stderr\n");
    printf("  quit, exit          Exit from halcmd\n");
}

