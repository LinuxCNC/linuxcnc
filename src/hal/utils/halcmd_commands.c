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
#include "rtapi_hexdump.h"

#include <../include/machinetalk/protobuf/types.npb.h>

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

static int unloadrt_comp(const char *mod_name);
static void print_comp_info(char **patterns);
static void print_pin_exists(int type, char **patterns);
static void print_inst_info(char **patterns);
static void print_vtable_info(char **patterns);
static void print_pin_info(int type, char **patterns);
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
static void print_mem_status();
static const char *data_type(int type);
static const char *data_type2(int type);
static const char *pin_data_dir(int dir);
static const char *param_data_dir(int dir);
static const char *data_arrow1(int dir);
static const char *data_arrow2(int dir);
static char *data_value(int type, void *valptr);
static char *data_value2(const int type, const void *valptr);
static void save_comps(FILE *dst);
//static void save_aliases(FILE *dst);
static void save_signals(FILE *dst, int only_unlinked);
static void save_links(FILE *dst, int arrows);
static void save_nets(FILE *dst, int arrows);
static void save_params(FILE *dst);
static void save_threads(FILE *dst);
static void print_help_commands(void);
static int print_name(hal_object_ptr o, foreach_args_t *args);
static int print_objects(char **patterns);
static int print_mutexes(char **patterns);
static int print_heap(char **patterns);

static int inst_count(const int use_halmutex, hal_comp_t *comp);

static int tmatch(int req_type, int type) {
    return req_type == -1 || type == req_type;
}

static int match(char **patterns, const char *value) {
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
    const hal_pin_t *first_pin, *second_pin;
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
    halcmd_info("Realtime threads %sstarted\n", retval ? "already" :"");
    return 0;
}

int do_stop_cmd(void) {
    int retval = hal_stop_threads();
    halcmd_info("Realtime threads %sstopped\n", retval ? "already" : "");
    return 0;
}

int do_echo_cmd(void) {
    printf("Echo on\n");
    return 0;
}
int do_unecho_cmd(void) {
    printf("Echo off\n");
    return 0;
}
int do_addf_cmd(char *func, char *thread, char **opt)
{
    int i, position = -1;
    int retval;
    int rmb = 0, wmb = 0;
    char *cp, *s;

    for (i = 0; ((s = opt[i]) != NULL) && strlen(s); i++) {
	if  (!strcasecmp(s,"rmb")) {
	    rmb = 1;
	}  else if  (!strcasecmp(s,"wmb")) {
	    wmb = 1;
	} else {
	    position = strtol(s, &cp, 0);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		/* invalid chars in string */
		halcmd_error("string '%s' invalid for thread position\n", s);
		retval = -EINVAL;
	    }
	}
    }
    retval = hal_add_funct_to_thread(func, thread, position, rmb, wmb);
    if(retval == 0) {
        halcmd_info("Function '%s' added to thread '%s', rmb=%d wmb=%d\n",
                    func, thread, rmb, wmb);
    } else {
        halcmd_error("addf failed: %s\n", hal_lasterror());
    }
    return retval;
}
#if 0
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
#endif
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

int find_modifier(hal_object_ptr o, foreach_args_t *args)
{
    if ((signal_of(o.pin) == args->user_ptr1) &&
	(pin_dir(o.pin) == args->user_arg1)) {

	// pass back pin name
	args->user_ptr2 = (void *) ho_name(o.pin);
	// terminate visit on first match
	return 1;
    }
    return 0;
}

static int preflight_net_cmd(char *signal, hal_sig_t *sig, char *pins[])
{
    int i,
	type = -1,
	writers = 0,
	bidirs = 0,
	pincnt = 0;
    char *writer_name = NULL,
	*bidir_name = NULL;

    /* if signal already exists, use its info */
    if (sig) {
	type = sig->type;
	writers = sig->writers;
	bidirs = sig->bidirs;
    }

    if (writers || bidirs) {

	foreach_args_t writerargs =  {
	    .type = HAL_PIN,
	    .user_arg1 = HAL_OUT,
	    .user_ptr1 = sig,
	};
	if (halg_foreach(0, &writerargs, find_modifier))
	    writer_name = writerargs.user_ptr2;

	foreach_args_t bidirargs =  {
	    .type = HAL_PIN,
	    .user_arg1 = HAL_IO,
	    .user_ptr1 = sig,
	};
	if (halg_foreach(0, &bidirargs, find_modifier))
	    bidir_name = writer_name = bidirargs.user_ptr2;

#if 0
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
#endif
    }

    for(i=0; pins[i] && *pins[i]; i++) {
        const hal_pin_t *pin;
        pin = halpr_find_pin_by_name(pins[i]);
        if(!pin) {
            halcmd_error("Pin '%s' does not exist\n",
                    pins[i]);
            return -ENOENT;
        }
        if(signal_of(pin) == sig) {
	     /* Already on this signal */
	    pincnt++;
	    continue;
	} else if (pin_is_linked(pin)) {
            hal_sig_t *osig = signal_of(pin);
            halcmd_error("Pin '%s' was already linked to signal '%s'\n",
			 ho_name(pin), ho_name(osig));
            return -EINVAL;
	}
	if (type == -1) {
	    /* no pre-existing type, use this pin's type */
	    type = pin_type(pin);
	}
        if(type !=  pin_type(pin)) {
            halcmd_error(
                "Signal '%s' of type '%s' cannot add pin '%s' of type '%s'\n",
                signal, data_type2(type), ho_name(pin), data_type2(pin->type));
            return -EINVAL;
        }
        if(pin_dir(pin) == HAL_OUT) {
            if(writers || bidirs) {
            dir_error:
                halcmd_error(
                    "Signal '%s' can not add %s pin '%s', "
                    "it already has %s pin '%s'\n",
                        signal, pin_data_dir(pin->dir), ho_name(pin),
                        bidir_name ? pin_data_dir(HAL_IO):pin_data_dir(HAL_OUT),
                        bidir_name ? bidir_name : writer_name);
                return -EINVAL;
            }
            writer_name = (char *) ho_name(pin);
            writers++;
        }
	if(pin_dir(pin) == HAL_IO) {
            if(writers) {
                goto dir_error;
            }
            bidir_name = (char *) ho_name(pin);
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
	const hal_pin_t *pin = halpr_find_pin_by_name(signal);
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
        const hal_pin_t *pin = halpr_find_pin_by_name(pins[0]);
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
    } else if (strcasecmp(type, "u64") == 0) {
	retval = hal_signal_new(name, HAL_U64);
    } else if (strcasecmp(type, "s64") == 0) {
	retval = hal_signal_new(name, HAL_S64);
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
    unsigned long long ullval;
    long long llval;
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
    case HAL_S64:
	llval = strtoll(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    halcmd_error("value '%s' invalid for S64\n", value);
	    retval = -EINVAL;
	} else {
	    *((hal_s64_t *) (d_ptr)) = llval;
	}
	break;
    case HAL_U64:
	ullval = strtoull(value, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    /* invalid chars in string */
	    halcmd_error("value '%s' invalid for U64\n", value);
	    retval = -EINVAL;
	} else {
	    *((hal_u64_t *) (d_ptr)) = ullval;
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
    const hal_pin_t *pin;
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
	    comp =  halpr_find_owning_comp(ho_owner_id(pin));
            /* found it */
            type = pin->type;
            if ((pin->dir == HAL_OUT) && (comp->state != COMP_UNBOUND)) {
                rtapi_mutex_give(&(hal_data->mutex));
                halcmd_error("pin '%s' is not writable\n", name);
                return -EINVAL;
            }
            if(pin_is_linked(pin)) {
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
    const hal_pin_t *pin;
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
    // hal_sig_t *sig;
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
    if (pin) {
        /* found it */
        /* type = pin->type; */
        /* if (pin_is_linked(pin)) { */
        /*     sig = signal_of(pin); */
        /*     d_ptr = SHMPTR(sig->data_ptr); */
        /* } else { */
        /*     sig = 0; */
        /*     d_ptr = &(pin->dummysig); */
        /* } */
        halcmd_output("%s\n", data_value2((int) pin_type(pin), pin_value(pin)));
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
    d_ptr = sig_value(sig);
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
    d_ptr = sig_value(sig);
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
    if(strcmp(typestr, "s64") == 0) return HAL_S64;
    if(strcmp(typestr, "u64") == 0) return HAL_U64;
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
	print_sig_info(-1, NULL);
	print_param_info(-1, NULL);
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
	print_sig_info(-1, patterns);
	print_param_info(-1, patterns);
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
    } else if (strcmp(type, "pexists") == 0) {
	int type = get_type(&patterns);
	print_pin_exists(type, patterns);
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
    } else if (strcmp(type, "objects") == 0) {
	print_objects(patterns);
    } else if (strcmp(type, "mutex") == 0) {
	print_mutexes(patterns);
    } else if (strcmp(type, "heap") == 0) {
	print_heap(patterns);
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


bool module_loaded(const int use_halmutex, char *mod_name)
{
    CHECK_HALDATA();
    CHECK_STR(mod_name);
    {
	WITH_HAL_MUTEX_IF(use_halmutex);
        hal_comp_t *comp = halpr_find_comp_by_name(mod_name);
        return (comp != NULL);
    }
}


bool inst_name_exists(const int use_halmutex, char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);
    {
	WITH_HAL_MUTEX_IF(use_halmutex);

	hal_inst_t *ins  = halpr_find_inst_by_name(name);
	return (ins != NULL);
    }
}

int loadrt(const int use_halmutex, char *mod_path, char *args[])
{
    char *cp1;
    int n, retval;
    char arg_string[MAX_CMD_LEN+1];

    // Get the basename of args->name
    char *mod_name = mod_path; // default case:  module loaded by rpath
    for (int i=0; mod_path[i] != 0; i++)
      if (mod_path[i] == '/')
        mod_name = mod_path + i + 1;

    retval = rtapi_loadrt(rtapi_instance, mod_path, (const char **)args);
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
    cp1 = halg_malloc(use_halmutex, strlen(arg_string)+1);
    if ( cp1 == NULL ) {
	halcmd_error("failed to allocate memory for module args\n");
	return -1;
    }
    // copy string to shmem
    strcpy (cp1, arg_string);
    {
	WITH_HAL_MUTEX_IF(use_halmutex);

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
    bool instantiable = false;
    char *cp1, *cp2;
    char *argv[] = { NULL};
    // MAX_ARGS defined in halcmd_commands.h - currently 20
    char *list[MAX_ARGS] = {"\0",};
    int list_index = 0;

    if (hal_get_lock() & HAL_LOCK_LOAD) {
	halcmd_error("HAL is locked, loading of modules is not permitted\n");
	return -EPERM;
    }


    retval = rtapi_get_tags(mod_name);
    if(retval == -1) {
	halcmd_error("Error in module tags search");
	return retval;
    }  else {
	if((retval & HC_INSTANTIABLE) == HC_INSTANTIABLE )
	    instantiable = true;
	// extra test for other tags below
    }

    // if not instantiable and not called from do_newinst_cmd(),
    // just loadrt the comp
    if (!(instantiable && instantiate)) {
	// legacy components
        return loadrt(1, mod_name, args);
    }

    // from here on: only instantiable comps to be considered
    //
    // if we come here we were called from do_newinst_cmd()
    if (!(args[0] != NULL && strlen(args[0]))) {

	// no args case: treat as count=1
	// if no args just create a single instance
	// with default number 0

	// if the module isnt loaded yet, do so now:
	// XXX - autoload setting? I guess this is assumed
	// to be on
	if (!module_loaded(1, mod_name)) {
	    if((retval = (loadrt(0, mod_name, argv))) )
		return retval;
	}
	// determine instance name:
        // find unused instance name
        w = 0;
        sprintf(buff, "%s.%d", mod_name, w);
        while(inst_name_exists(1, buff))
    	    sprintf(buff, "%s.%d", mod_name, ++w);

	// now instantiate with this name
	retval = do_newinst_cmd(mod_name, buff, argv);
	if ( retval != 0 ) {
	    halcmd_error("rc=%d  %s\n", retval, rtapi_rpcerror());
	}
	return retval;
	// end of scoped lock
    }

    // args were given.
    assert(args[0] != NULL && strlen(args[0]));

    strcpy(arg_string, args[0]);
    // handle count=N
    if (strncmp(arg_string, "count=", 6) == 0) {
	strcpy(arg_section, &arg_string[6]);
	n = strtol(arg_section, &cp1, 10);
	if (n > 0) {
	    // check if already loaded, if not load it
	    if (!module_loaded(1, mod_name)) {
		if((retval = (loadrt(1, mod_name, argv))) )
		    return retval;
	    }
	    for(int y = 0, v = 0; y < n; y++ , v++) {
		// find unused instance name
		sprintf(buff, "%s.%d", mod_name, v);
		while(inst_name_exists(1, buff))
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
    else if (strncmp(arg_string, "names=", 6) == 0) {
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
	    if (!module_loaded(1, mod_name)) {
		if ((retval = (loadrt(1, mod_name, argv)))) {
		    for(p = 0; p < list_index; p++)
			free(list[p]);
		    return retval;
		}
	    }
	    for (w = 0; w < list_index; w++) {
		if (inst_name_exists(1, list[w])) {
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
		     "NB. Use of personality or cfg is deprecated\n\n",
		     args[x]);
	return -1;
    }
    return 0;
}


int do_loadrt_cmd(char *mod_name, char *args[])
{
    return loadrt_cmd(true, mod_name, args);
}


int do_delsig_cmd(char *sig_name)
{
    foreach_args_t args =  {
	.type = HAL_SIGNAL,
	.name = ( strcmp(sig_name, "all" ) != 0 ) ? NULL : sig_name,
    };
    // NB: the iterator holds the lock, the callback
    // uses halg_delete_<type> calls
    int retval = halg_foreach(1, &args, unlocked_delete_halobject);
    if (retval < 0) {
	halcmd_error("delsig %s failed: %s\n", sig_name, hal_lasterror());
	return retval;
    }
    return 0;
}

static int unload_usr_cb(hal_object_ptr o, foreach_args_t *args)
{
    hal_comp_t *comp = o.comp;

    if ((comp->type == TYPE_REMOTE)
	&& comp->pid == 0) {
	// found a disowned remote component
	// need to cleanup with hal_exit
	// NB: no point in acquiring the HAL mutex
	// since it is held in the calling iterator
	halg_exit(0, ho_id(comp));
	return 0;
    }
    // an owned remote component, or a user component
    // owned by somebody other than us receives a signal
    if (((comp->type == TYPE_REMOTE) && (comp->pid != 0)) ||
	((comp->type == TYPE_USER) && comp->pid != args->user_arg1)) {

	// found a userspace or remote component and it is not us
	// send SIGTERM to unload this component
	// this will also exit haltalk if unloadusr of a remote
	// comp which is being served by haltalk
	kill(abs(comp->pid), SIGTERM);
    }
    return 0;
}

int do_unloadusr_cmd(char *name)
{
    foreach_args_t args =  {
	.type = HAL_COMPONENT,
	.name = strcmp(name, "all") ?  name : NULL,
	.user_arg1 = getpid(),
    };
    // NB: the iterator acquires the lock, so the callback
    // uses halg_<type> calls with the lock param set to 0
    halg_foreach(1, &args, unload_usr_cb);
    return 0;
}

static int unload_rt_cb(hal_object_ptr o, foreach_args_t *args)
{
    // HAL mutex held in caller.
    hal_comp_t *comp = o.comp;

    // skip user, remote comps,
    // and.. the HAL library which is a comp, too

    if (comp->type != TYPE_RT)
	return 0;

    // on first pass, skip comps which export vtables
    if (args->user_arg1 &&
	halg_count_exported_vtables(0, ho_id(comp)))
	return 0; // but continue iterating

    // dont directly work with the name from hal_comp_t
    // as halg_free_object() will zero the name,
    // making print error messages return a zero-length
    // string in unloadrt_comp()
    char *name = strdup(ho_name(comp));
    rtapi_mutex_give(&(hal_data->mutex));
    int retval = unloadrt_comp(name);
    rtapi_mutex_get(&(hal_data->mutex));
    free(name);

    args->user_arg2 = retval; // pass it back
    // check for fatal error
    if ( retval < -1 )
	return retval;
    return 0; // continue
}


int do_unloadrt_cmd(char *name)
{
    int ret;

    foreach_args_t args =  {
	.type = HAL_COMPONENT,
	.name = (strcmp(name, "all") == 0) ? NULL : name,
	.user_arg1 = 1, // signifiy 'skip if vtable exported'
    };
    ret = halg_foreach(1, &args, unload_rt_cb);
    if (ret < 0)
	goto FATAL;

    args.user_arg1 = 0; // now unload those which exported vtables
    ret = halg_foreach(1, &args, unload_rt_cb);
    if (ret < 0)
	goto FATAL;
    return 0;

 FATAL:
    halcmd_error("unloadrt failed rc=%d\n", args.user_arg2);
    return args.user_arg2;
}


#if 0
int XXXXdo_unloadrt_cmd(char *mod_name)
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
		// add to 'unload last' list if any found
		foreach_args_t args =  {
		    .type = HAL_VTABLE,
		    .user_arg1 = comp->comp_id,
		    .user_arg2 = 0, // returned count of exported vtables
		};
		halg_foreach(false, &args, _count_exported_vtables);
		if (args.user_arg2) {
		    // this comp exports (a) vtable(s)
		    zlist_append(vtables, comp->name);
		    goto NEXTCOMP;
		} else
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

    if (all)
	do_delthread_cmd("all");

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
#endif

int do_shutdown_cmd(void)
{
    int retval = rtapi_shutdown(rtapi_instance);
    return retval;
}

int do_sweep_cmd(char *flags)
{
    bool log = (flags && strlen(flags));
    int hflags, gflags;
    if (log) {
	hflags = rtapi_heap_setflags(&hal_data->heap,
				    RTAPIHEAP_TRACE_MALLOC|
				    RTAPIHEAP_TRACE_FREE);
	gflags = rtapi_heap_setflags(&global_data->heap,
				     RTAPIHEAP_TRACE_MALLOC|
				     RTAPIHEAP_TRACE_FREE);
    }
    int retval = hal_sweep();
    if (retval)
	halcmd_output("%d objects freed\n", retval);
    if (log) {
	rtapi_heap_setflags(&hal_data->heap, hflags);
	rtapi_heap_setflags(&global_data->heap, gflags);
    }
    return 0;
}

int do_ping_cmd(void)
{
    int retval = rtapi_ping(rtapi_instance);
    return retval;
}

static int unloadrt_comp(const char *mod_name)
{
    int retval;

    retval = rtapi_unloadrt(rtapi_instance, mod_name);
    if (retval < 0) {
	halcmd_error("error unloading realtime module '%s': rc=%d %s\n",
		     mod_name, retval,rtapi_rpcerror());
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

static int inst_count(const int use_halmutex, hal_comp_t *comp)
{
    foreach_args_t args =  {
	.type = HAL_INST,
	.owner_id = ho_id(comp),
    };
    return halg_foreach(use_halmutex,  &args, yield_count);
}

static int print_comp_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_comp_t *comp = o.comp;
    bool has_ctor = (comp->ctor != NULL) ;
    bool has_dtor = (comp->dtor != NULL) ;
    bool is_hallib = (comp->type == TYPE_HALLIB);

    if (match(args->user_ptr1, ho_name(comp))) {

        halcmd_output(" %5d  %-4s %c%c%c%c  %4d %-*s",
                      ho_id(comp),
                      type_name(comp),
                      has_ctor ? 'c': ' ',
                      has_dtor ? 'd': ' ',
                      is_hallib ? 'i': ' ',
                      ' ',
                      inst_count(0, comp),
                      HAL_NAME_LEN,
                      ho_name(comp));

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
    return 0;
}

static void print_comp_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("HAL Components:\n");
	halcmd_output("    ID  Type Flags Inst %-*s PID   State\n", HAL_NAME_LEN, "Name");
    }
    foreach_args_t args =  {
	.type = HAL_COMPONENT,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_comp_entry);
    halcmd_output("\n");
}


static int print_inst_line(hal_object_ptr o, foreach_args_t *args)
{
    hal_inst_t *inst = o.inst;
    if ( match(args->user_ptr1, ho_name(inst))) {
	hal_comp_t *comp = halpr_find_comp_by_id(ho_owner_id(inst));

	halcmd_output("%5d %5d %5d  %-*s %-*s",
		      ho_id(inst),
		      ho_id(comp),
		      inst->inst_size,
		      40, // HAL_NAME_LEN,
		      ho_name(inst),
		      20, // HAL_NAME_LEN,
		      ho_name(comp));
	halcmd_output("\n");
    }
    return 0; // continue
}

static void print_inst_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Instances:\n");
	halcmd_output(" Inst  Comp  Size  Name                                              Owner\n" );
    }
    foreach_args_t args =  {
	.type = HAL_INST,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_inst_line);
    halcmd_output("\n");
}

static int print_vtable_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_vtable_t *vt = o.vtable;
    if ( match(args->user_ptr1, ho_name(vt)) ) {
	halcmd_output(" %5d  %-40.40s  %-5d   %-5d",
		      ho_id(vt),
		      ho_name(vt),
		      vt->version,
		      ho_refcnt(vt));
	if (vt->context == 0)
	    halcmd_output("   RT   ");
	else
	    halcmd_output("   %-5d", vt->context);

	hal_comp_t *comp = halpr_find_comp_by_id(ho_owner_id(vt));
	if (comp) {
	    halcmd_output("   %-5d %-40.40s", ho_id(comp),  ho_name(comp));
	} else {
	    halcmd_output("   * not owned by a component *");
	}
	halcmd_output("\n");
    }
    return 0;
}

static void print_vtable_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Exported vtables:\n");
	halcmd_output("ID      Name                                           Version Refcnt  Context Owner\n");
    }

    foreach_args_t args =  {
	.type = HAL_VTABLE,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_vtable_entry);
    halcmd_output("\n");
}

static int print_pin_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_pin_t *pin = o.pin;
    hal_comp_t *comp;
    hal_sig_t *sig;
    void *dptr;

    if ( tmatch(args->user_arg1, pin->type) &&
	 match(args->user_ptr1, ho_name(pin)) ) {
	comp = halpr_find_owning_comp(ho_owner_id(pin));

	sig = signal_of(pin);
	dptr = pin_value(pin);

	if (scriptmode == 0) {

	    halcmd_output(" %5d  ", ho_id(comp));
	    if ( ho_id(comp) == ho_owner_id(pin))
		halcmd_output("     ");
	    else
		halcmd_output("%5d", ho_owner_id(pin));

	    if (pin->type == HAL_FLOAT) {
		halcmd_output(" %5s %-3s  %9s  %-40.40s\t%f\t%s%s%s%s",
			      data_type((int) pin->type),
			      pin_data_dir((int) pin->dir),
			      data_value((int) pin->type, dptr),
			      ho_name(pin),
			      hal_data->epsilon[pin->eps_index],
			      ho_rmb(pin) ? "r" : "-",
			      ho_wmb(pin) ? "w" : "-",
			      ho_legacy(pin) ? "l" : "-",
			      pin->flags & PIN_DO_NOT_TRACK ? "n" : "-");
	    } else {
		halcmd_output(" %5s %-3s  %9s  %-40.40s\t\t%s%s%s%s",
			      data_type((int) pin->type),
			      pin_data_dir((int) pin->dir),
			      data_value((int) pin->type, dptr),
			      ho_name(pin),
			      ho_rmb(pin) ? "r" : "-",
			      ho_wmb(pin) ? "w" : "-",
			      ho_legacy(pin) ? "l" : "-",
			      pin->flags & PIN_DO_NOT_TRACK ? "n" : "-");
	    }
	} else {
	    halcmd_output("%s %s %s %s %-40.40s",
			  ho_name(comp),
			  data_type((int) pin->type),
			  pin_data_dir((int) pin->dir),
			  data_value2((int) pin->type, dptr),
			  ho_name(pin));
	}
	if (sig == 0) {
	    halcmd_output("\n");
	} else {
	    halcmd_output("\t      %s %s\n", data_arrow1((int) pin->dir), ho_name(sig));
	}
#ifdef DEBUG
	halcmd_output("%s %d:%d sig=%p dptr=%p *dptr=%p\n",
		      pin->name, pin->signal_inst,pin->signal,
		      sig, dptr, *((void **)dptr));
#endif
    }

    return 0;
}

static void print_pin_info(int type, char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Component Pins:\n");
	halcmd_output("  Comp   Inst Type  Dir         Value  Name                                            Epsilon Flags  linked to:\n");
    }
    foreach_args_t args =  {
	.type = HAL_PIN,
	.user_arg1 = type,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_pin_entry);
    halcmd_output("\n");
}


static int pin_match(hal_object_ptr o, foreach_args_t *args)
{
hal_pin_t *pin = o.pin;

    if ( tmatch(args->user_arg1, pin->type) && match(args->user_ptr1, ho_name(pin)) ) {
	args->user_arg2 = 999;
	return 1; // stop iterating
    }

    return 0; // continue iterating
}



// This function is a temporary measure to keep the xhc-hb04 pendant working.
// xhc-hb04.tcl uses commandline halcmd outputs against tests which are fixed
// in expectation of a particular return. (ie from linuxcnc circa 2014)
// Changes to halcmd print routines broke xhc-hb04.
// This will tell the tcl code if a pin exists, removing the need for the
// previous convoluted parsing of the stdout output.
// 06032017 updated for halg_foreach() use

static void print_pin_exists(int type, char **patterns)
{

    foreach_args_t args =  {
	.type = HAL_PIN,
	.user_arg1 = type,
	.user_ptr1 = patterns,
	.user_arg2 = 0
    };

    halg_foreach(true, &args, pin_match);
    if(args.user_arg2 == 999)
	halcmd_output("Exists\n");
    else
	halcmd_output("Imaginary\n");

}

static int linked_pin_callback(hal_pin_t *pin, hal_sig_t *sig, void *user)
{
    halcmd_output("                                 %s %s\n",
		  data_arrow2((int) pin->dir),
		  ho_name(pin));
    return 0; // continue iterating
}

static int print_signal_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_sig_t *sig = o.sig;
    if ( match(args->user_ptr1, ho_name(sig)) ) {
	void *dptr = sig_value(sig);
	halcmd_output("%s  %s  %s%s    %-20s \n",
		      data_type((int) sig->type),
		      data_value((int) sig->type, dptr),
		      ho_rmb(sig) ? "r" : "-",
		      ho_wmb(sig) ? "w" : "-",
		      ho_name(sig));

	// look for pin(s) linked to this signal
	halg_foreach_pin_by_signal(false, sig, linked_pin_callback, NULL);
    }
    return 0; // continue iterating
}

static void print_sig_info(int type, char **patterns)
{

    if (scriptmode != 0) {
    	print_script_sig_info(type, patterns);
	return;
    }
    halcmd_output("Signals:\n");
    halcmd_output("Type          Value  flags Name                   linked to:\n");

    foreach_args_t args =  {
	.type = HAL_SIGNAL,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_signal_entry);
    halcmd_output("\n");
}


static void print_script_sig_info(int type, char **patterns)
{
#if 0
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
		    data_arrow2((int) pin->dir), ho_name(pin));
		pin = halpr_find_pin_by_sig(sig, pin);
	    }
	    halcmd_output("\n");
	}
	next = sig->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
#endif
}

static int print_param_line(hal_object_ptr o, foreach_args_t *args)
{
    hal_param_t *param = o.param;

    if ( match(args->user_ptr1, ho_name(param))) {
	int param_owner_id = ho_owner_id(param);
	hal_comp_t *comp = halpr_find_owning_comp(param_owner_id);

	if (scriptmode == 0) {
	    halcmd_output(" %5d  ", ho_id(comp));
	    if (ho_id(comp) == param_owner_id)
		halcmd_output("     ");
	    else
		halcmd_output("%5d", param_owner_id);

	    halcmd_output("  %5s %-3s  %9s  %s\n",
			  data_type((int) param->type),
			  param_data_dir((int) param->dir),
			  data_value((int) param->type, SHMPTR(param->data_ptr)),
			  ho_name(param));
	} else {
	    halcmd_output("%s %s %s %s %s\n",
			  ho_name(comp), data_type((int) param->type),
			  param_data_dir((int) param->dir),
			  data_value2((int) param->type, SHMPTR(param->data_ptr)),
			  ho_name(param));
	}
    }
    return 0; // continue

}

static void print_param_info(int type, char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Parameters:\n");
	halcmd_output(" Comp    Inst Type   Dir         Value  Name\n");
    }

    foreach_args_t args =  {
	.type = HAL_PARAM,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_param_line);
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

static int print_funct_line(hal_object_ptr o, foreach_args_t *args)
{
    hal_funct_t *fptr = o.funct;

    if ( match(args->user_ptr1, ho_name(fptr))) {

	hal_comp_t *comp =  halpr_find_owning_comp(ho_owner_id(fptr));
	if (!comp) {
	    halcmd_error("BUG: no owning comp for owner_id %d\n",
			 ho_owner_id(fptr));
	    return 0;
	}
	if (scriptmode == 0) {
	    halcmd_output(" %5d  ", ho_id(comp));
	    if (ho_id(comp) == ho_owner_id(fptr))
		halcmd_output("     ");
	    else
		halcmd_output("%5d", ho_owner_id(fptr));
	    halcmd_output(" %12.12lx  %12.12lx  %-3s  %5d %-7s %s\n",
			  (long)fptr->funct.l,
			  (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
			  fptr->users,
			  ftype(fptr->type),
			  ho_name(fptr));
	} else {
	    halcmd_output("%s %12.12lx %12.12lx %s %3d %s\n",
			  ho_name(comp),
			  (long)fptr->funct.l,
			  (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
			  fptr->users, ho_name(fptr));
	}
    }
    return 0; // continue
}

static void print_funct_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Exported Functions:\n");
	halcmd_output("  Comp   Inst CodeAddr      Arg           FP   Users Type    Name\n");
    }
    foreach_args_t args =  {
	.type = HAL_FUNCT,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_funct_line);
    halcmd_output("\n");
}

static int print_objects(char **patterns)
{
    WITH_HAL_MUTEX();
    halhdr_t *hh, *tmp;
    int count = 0;
    dlist_for_each_entry_safe(hh, tmp, OBJECTLIST, list) {
	if (!hh_is_valid(hh)) {
	    count++;
	    continue;
	}
	char buffer[200];
	hh_snprintf(buffer, sizeof(buffer), hh);
	halcmd_output("%s\n", buffer);
    }
    if (count) 	halcmd_output("%d objects marked for deletion\n", count);
    return 0;
}

#include "rtapi_global.h"
#include "rtapi/shmdrv/shmdrv.h"
static int print_mutexes(char **patterns)
{
    extern global_data_t *global_data;
    extern hal_data_t *hal_data;
    if (MMAP_OK(global_data))
	halcmd_output("global_data->mutex: %ld\n", global_data->mutex);

    if (MMAP_OK(hal_data)) {
	printf("hal_data->mutex: %ld\n", hal_data->mutex);
	printf("hal_data->heap.mutex: %ld\n", hal_data->heap.mutex);
    }
    return 0;
}
static int print_heap(char **patterns)
{
    extern hal_data_t *hal_data;

    if (MMAP_OK(hal_data)) {
	struct rtapi_heap_stat hs;
	rtapi_heap_status(&hal_data->heap, &hs);
	halcmd_output("total_avail=%zu fragments=%zu largest=%zu\n",
		      hs.total_avail, hs.fragments, hs.largest);
    }
    return 0;
}

static void print_thread_stats(hal_thread_t *tptr)
{
    int flavor = global_data->rtapi_thread_flavor;
    rtapi_threadstatus_t *ts =
	&global_data->thread_status[tptr->task_id];

    halcmd_output("\nLowlevel thread statistics for '%s':\n\n",
		  ho_name(tptr));

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

static int print_thread_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_thread_t *tptr = o.thread;

    char **patterns = args->user_ptr1;
    int named = patterns && patterns[0] && strlen(patterns[0]);

    if (match(patterns, ho_name(tptr))) {
	// note that the scriptmode format string has no \n
	// TODO FIXME add thread runtime and max runtime to this print
	    char flags[100];
	    snprintf(flags, sizeof(flags),"%s%s",
		     tptr->flags & TF_NONRT ? "posix ":"",
		     tptr->flags & TF_NOWAIT ? "nowait":"");
	halcmd_output(((scriptmode == 0) ?
		       "%11ld  %-3s %-2d   %-40s  %8u, %8u %3ld%% %3ld%%  +/-%5.2f%% %s\n" :
		       "%ld %s %d %s %u %u %3ld%% %3ld%% %.2f"),
		      tptr->period,
		      (tptr->uses_fp ? "YES" : "NO"),
		      tptr->cpu_id,
		      ho_name(tptr),
		      get_s32_pin(tptr->runtime),
		      get_s32_pin(tptr->maxtime),
		      get_s32_pin(tptr->runtime)*100/tptr->period,
		      get_s32_pin(tptr->maxtime)*100/tptr->period,
// https://en.wikipedia.org/wiki/68%E2%80%9395%E2%80%9399.7_rule
		      sqrt(tptr->m2/ (tptr->cycles -1))*100.0*2.0/tptr->period,
		      flags);

	hal_list_t *list_root = &(tptr->funct_list);
	hal_list_t *list_entry = dlist_next(list_root);
	int n = 1;
	while (list_entry != list_root) {
	    /* print the function info */
	    hal_funct_entry_t *fentry = (hal_funct_entry_t *) list_entry;
	    hal_funct_t *funct = SHMPTR(fentry->funct_ptr);
	    /* scriptmode only uses one line per thread, which contains:
	       thread period, FP flag, name, then all functs separated by spaces  */
	    if (scriptmode == 0) {
		halcmd_output("                   %2d %s\n", n,
			      ho_name(funct));
	    } else {
		halcmd_output(" %s", ho_name(funct));
	    }
	    n++;
	    list_entry = dlist_next(list_entry);
	}
	if (scriptmode != 0) {
	    halcmd_output("\n");
	} else {
	    // if a thread name was given, print the flavor specific stats
	    if (named)
		print_thread_stats(tptr);
	}
    }
    return 0;
}

static void print_thread_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Realtime Threads (flavor: %s, currently %s) :\n",
		      current_flavor->name,
		      (hal_data->threads_running > 0) ? "running" : "stopped");
	halcmd_output("     Period  FP CPU   Name                                          "
		      "Time  Max-Time util  max  jitter-95%%     flags\n");
    }
    foreach_args_t args =  {
	.type = HAL_THREAD,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_thread_entry);
    halcmd_output("\n");
}

static void print_comp_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_COMPONENT,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static void print_pin_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_PIN,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static void print_sig_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_SIGNAL,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static int print_name(hal_object_ptr o, foreach_args_t *args)
{
    if ( match(args->user_ptr1, hh_get_name(o.hdr)))
	halcmd_output("%s ", hh_get_name(o.hdr));
    return 0; // continue
}

static void print_param_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_PARAM,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}


static void print_funct_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_FUNCT,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static void print_thread_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_THREAD,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
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


int yield_ostats(hal_object_ptr o, foreach_args_t *args)
{
    if (ho_legacy(o.pin)) args->user_arg1++;   // legacy count
    args->user_arg2 += rtapi_allocsize(&hal_data->heap, o.any); // descriptor sizes

    switch (ho_object_type(o.pin)) {
    case HAL_PIN:
    case HAL_SIGNAL:
    case HAL_PARAM:
	args->user_arg3 += sizeof(hal_data_u); // descriptor case
	break;
    case HAL_INST:
	args->user_arg3 += o.inst->inst_size;
	break;
    case HAL_RING:
	if (o.ring->ring_shmkey) // shm seg
	    args->user_arg4 += o.ring->total_size;
	else // HAL mem
	    args->user_arg3 += o.ring->total_size;
	break;
    }
    return 0; // continue visiting
}
static int count_objects(const char *tag, const int type)
{
    foreach_args_t args = {
	.type = type,
	.user_arg1 = 0, // # of legacy objects
	.user_arg2 = 0, // descriptor rtapi_allocsize (heap usage)
	.user_arg3 = 0, // pins, params, signals - 'RT memory' total
	.user_arg4 = 0, // shm segments total size
    };
    int n = halg_foreach(true, &args, yield_ostats);
    halcmd_output("%s:\t%6d", tag, n);
    halcmd_output("\tlegacy: %6d descriptor mem: %6d RT mem: %6d shmtotal: %6d\n",
		  args.user_arg1,
		  args.user_arg2,
		  args.user_arg3,
		  args.user_arg4);
    return n;
}

static void print_mem_status()
{
    size_t unused = hal_data->shmem_top - hal_data->shmem_bot;

    halcmd_output("HAL memory status\n");
    halcmd_output("HAL shm segment size:  %d unused: %zu Usage=%zu%%\n",
		  global_data->hal_size, unused,
		  100*(global_data->hal_size-unused)/global_data->hal_size);
    struct rtapi_heap_stat hs = {};
    rtapi_heap_status(&hal_data->heap, &hs);

    halcmd_output("  heap: arena size=%zu totail_avail=%zu"
		  " fragments=%zu largest=%zu\n",
		  hs.arena_size, hs.total_avail, hs.fragments, hs.largest);
    if (hs.allocated)
	halcmd_output("  heap: requested=%zu allocated=%zu freed=%zu waste=%zu%%\n",
		      hs.requested, hs.allocated, hs.freed,
		      (hs.allocated - hs.requested)*100/hs.allocated);

    halcmd_output("  hal_malloc():   %zu, mostly by comps\n",
		  hal_data->hal_malloced);
    halcmd_output("  RT objects: %zu  non-hal_malloc thereof: %zu\n",
		  (size_t)(global_data->hal_size - hal_data->shmem_top),
		  (size_t)(global_data->hal_size - hal_data->shmem_top -
			   hal_data->hal_malloced));
    halcmd_output("  strings on heap: alloc=%zu freed=%zu balance=%zu\n",
		  hal_data->str_alloc,
		  hal_data->str_freed,
		  hal_data->str_alloc - hal_data->str_freed);
    halcmd_output("  unused:   %ld\n",
		  (long)( hal_data->shmem_top - hal_data->shmem_bot));

    halcmd_output("HAL objects\n");

    count_objects("components", HAL_COMPONENT);
    count_objects("pins\t", HAL_PIN);
    count_objects("params\t", HAL_PARAM);
    count_objects("signals\t", HAL_SIGNAL);
    count_objects("threads\t", HAL_THREAD);
    count_objects("groups\t", HAL_GROUP);
    count_objects("members\t", HAL_MEMBER);
    count_objects("functs\t", HAL_FUNCT);
    count_objects("rings\t", HAL_RING);
    count_objects("plugs\t", HAL_PLUG);
    count_objects("instances", HAL_INST);
    halcmd_output("(some figures do not fully add up as some usage is unaccounted for)\n");
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
    case HAL_S64:
	type_str = "s64  ";
	break;
    case HAL_U64:
	type_str = "u64  ";
	break;
    default:
	/* Shouldn't get here, but just in case... */
	type_str = "undef";
    }
    return type_str;
}

static const char *data_type2(int type)
{
    return hals_type(type);
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
    case HAL_S64:
	snprintf(buf, 14, "  %lld", (long long)*((hal_s64_t *) valptr));
	value_str = buf;
	break;
    case HAL_U64:
	snprintf(buf, 14, "  %llu", (unsigned long long)*((hal_u64_t *) valptr));
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
static char *data_value2(const int type, const void *valptr)
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
	//	save_aliases(dst);
        save_signals(dst, 1);
        save_nets(dst, 3);
	save_params(dst);
	save_threads(dst);
    } else if (strcmp(type, "comp") == 0) {
	save_comps(dst);
    /* } else if (strcmp(type, "alias") == 0) { */
    /* 	save_aliases(dst); */
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
    return halg_group_new(1, group, arg1, arg2);
}

int do_delg_cmd(char *group)
{
    return halg_group_delete(1, group);
}


int do_newm_cmd(char *group, char *member, char **opt)
{
    int arg1 = MEMBER_MONITOR_CHANGE, retval;
    char *cp;
    int eps_index = 0;
    hal_sig_t *sig;
    hal_group_t *grp;

    WITH_HAL_MUTEX();
    sig = halpr_find_sig_by_name(member);
    grp = halpr_find_group_by_name(member);

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
    retval = halg_member_new(0,group, member, arg1, eps_index);
    if (retval)
	halcmd_error("'newm %s %s' failed\n", group, member);
    return retval;
}

int do_delm_cmd(char *group, char *member)
{
    return halg_member_delete(1, group, member);
}

static void print_group_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_GROUP,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static int print_member_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_sig_t *sig = SHMPTR(o.member->sig_ptr);
    void *dptr = sig_value(sig);

    halcmd_output("\t%-14.14s  %-6.6s %16.16s 0x%8.8x %f ",
		  ho_name(sig),
		  data_type((int) sig->type),
		  data_value((int) sig->type, dptr),
		  o.member->userarg1,
		  hal_data->epsilon[o.member->eps_index]);
    halcmd_output("\n");
    return 0;
}

static int print_group_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_group_t *gptr = o.group;
    char **patterns = args->user_ptr1;
    if ( match(patterns, ho_name(gptr)) ) {
	halcmd_output("Group name      Arg1       Arg2       Refs\n");

	halcmd_output("%-15.15s 0x%8.8x 0x%8.8x %d \n",
		      ho_name(gptr), gptr->userarg1, gptr->userarg2,
		      ho_refcnt(gptr));
	if (scriptmode == 0) {
	    halcmd_output("\n\tMember          Type              Value Arg1       Epsilon  Groupref:\n");
	}
	foreach_args_t args =  {
	    .type = HAL_MEMBER,
	    .owner_id = ho_id(gptr)
	};
	halg_foreach(false, &args, print_member_entry);
	halcmd_output("\n");
    }
    return 0;
}

static void print_group_info(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_GROUP,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_group_entry);
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
    foreach_args_t args =  {
	.type = HAL_RING,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
    halcmd_output("\n");
}

static int print_plug_entry(hal_object_ptr o, foreach_args_t *args)
{

    if (o.plug->ring_id == args->user_arg1) {
	halcmd_output("                                             %s %s id=%d owner=%d\n",
		      o.plug->role == PLUG_WRITER ? "<==" : "==>",
		      ho_name(o.plug),
		      ho_id(o.plug),
		      ho_owner_id(o.plug));
    }
    return 0;
}

static int print_ring_entry(hal_object_ptr o, foreach_args_t *args)
{
    hal_ring_t *rptr = o.ring;
    ringheader_t *rh;
    ringbuffer_t ringbuffer;
    int retval;

    if ( match(args->user_ptr1, ho_name(rptr))) {
	unsigned flags;
	if ((retval = halg_ring_attachf(0, &ringbuffer, &flags, ho_name(rptr)))) {
	    halcmd_error("%s: hal_ring_attachf(%d) failed ",
			 ho_name(rptr), rptr->ring_id);
	    goto done;
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
	halcmd_output("%-5d %-40.40s %-10u %-6.6s %d/%d %d/%d %-3d",
		      ho_id(rptr),
		      ho_name(rptr),
		      rh->size,
		      rtype,
		      rh->reader,rh->reader_instance,
		      rh->writer,rh->writer_instance,
		      rh->refcount-1);
	if (rh->use_rmutex)
	    halcmd_output(" rmutex");
	if (rh->use_wmutex )
	    halcmd_output(" wmutex");
	halcmd_output(rh->alloc_halmem ? " halmem" : " shmseg");
	if (rh->type == RINGTYPE_STREAM)
	    halcmd_output(" free:%u ",
			  stream_write_space(rh));
	else
	    halcmd_output(" recmax:%u ",
			  record_write_space(rh));
	if (ring_scratchpad_size(&ringbuffer))
	    halcmd_output(" scratchpad:%u ", ring_scratchpad_size(&ringbuffer));
	halcmd_output("\n");
	if ((retval = halg_ring_detach(0,  &ringbuffer)) < 0) {
	    halcmd_error("%s: rtapi_ring_detach(%d) failed ",
			 ho_name(rptr), rptr->ring_id);
	}
	foreach_args_t args =  {
	    .type = HAL_PLUG,
	    .user_arg1 = ho_id(rptr),
	};
	halg_foreach(false, &args, print_plug_entry);
    }
 done:
    return 0;
}

static void print_ring_info(char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Rings:\n");
	halcmd_output("ID    Name                                     Size       Type   Rdr Wrt Ref Flags \n");
    }
    foreach_args_t args =  {
	.type = HAL_RING,
	.user_ptr1 = patterns
    };
    halg_foreach(true, &args, print_ring_entry);
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
    int retval = 0;
    char *cp;


#define SCRATCHPAD "scratchpad="
#define ENCODINGS "encodings="
#define PAIRED "paired="
#define ZMQTYPE "zmq="
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
	}  else if  (!strcasecmp(s,"halmem")) {
	    mode |=  ALLOC_HALMEM;
	}  else if  (!strcasecmp(s,"record")) {
	    // default
	}  else if  (!strcasecmp(s,"stream")) {
	    mode |=  RINGTYPE_STREAM;
	} else if  (!strcasecmp(s,"multi")) {
	    mode |=  RINGTYPE_MULTIPART;
	} else if (!strncasecmp(s, SCRATCHPAD, strlen(SCRATCHPAD))) {
	    spsize = strtol(strchr(s,'=') + 1, &cp, 0);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		/* invalid chars in string */
		halcmd_error("string '%s' invalid for scratchpad size\n", s);
		return(-EINVAL);
	    }
	    if ((spsize < 0) || (spsize > MAX_SPSIZE)) {
		halcmd_error("scratchpad size out of bounds (0..%d)\n", MAX_SPSIZE);
		return(-EINVAL);
	    }

	} else {
	    halcmd_error("newring: invalid option '%s' (use one or several of: record stream multi"
			 " rtapi hal rmutex wmutex scratchpad=<size>)\n",s);
	    return -EINVAL;
	}
    }
    // this will happen under hal_data->mutex locked
    if (halg_ring_newf(1, size, spsize, mode, ring) == NULL) {
	halcmd_error("newring: failed to create new ring %s: %s\n",
		     ring, hal_lasterror());
	retval =  _halerrno;
    }
    return retval;
}

int do_delring_cmd(char *ring)
{
    return halg_ring_deletef(1,ring);
}

typedef int (*ring_attached_t)(const char *name, ringbuffer_t *rb, void *arg);
static int with_ring_attached(const char *ring, ring_attached_t func, void *arg)
{
    ringbuffer_t ringbuffer;
    unsigned flags;
    int retval;
    WITH_HAL_MUTEX();

    if (halg_ring_attachf(0, NULL, NULL, ring) < 0) {
	halcmd_error("no such ring '%s'\n", ring);
	return -EINVAL;
    }
    if ((retval = halg_ring_attachf(0, &ringbuffer, &flags, ring)) < 0) {
	halcmd_error("hal_ring_attachf(%s) failed\n", ring);
	return -EINVAL;
    }
    int result = func(ring, &ringbuffer, arg);

    if ((retval = halg_ring_detach(0, &ringbuffer)) < 0) {
	halcmd_error("hal_ring_detach(%s) failed\n",ring);
	return -EINVAL;
    }
    return result;
}

static void hdprinter(int level, const char *fmt, ...)
{
    char buf[BUFFERLEN + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFFERLEN, fmt, ap);
    va_end(ap);
    halcmd_output("%s", buf);
}

static int ringdump(const char *name, ringbuffer_t *rb, void *arg)
{
    ringheader_t *rh = rb->header;
    ringsize_t size;
    size_t nr = 0, nb = 0, tfc = 0, fc = 0;
    const void *data;
    ringiter_t ri;
    int result;

    switch (rh->type) {
    default:
	halcmd_output("%s: %s ring\n", name, rh->type == RINGTYPE_RECORD? "record" : "multi");

	if ((result = record_iter_init(rb, &ri)) != 0)
	    return result;

	while (1) {
	    size = 0;
	    while ((result = record_iter_read(&ri, &data, &size)) == EINVAL)
		record_iter_init(rb, &ri); // renew

	    switch (result) {
	    case EAGAIN:   // done
		halcmd_output("records:%zu", nr);
		if (rh->type == RINGTYPE_MULTIPART)
		    halcmd_output(" frames:%zu", tfc);
		halcmd_output(" used:%zu", nb);
		halcmd_output("\n");
		return 0;

	    default:
		// printf("data=%p size=%d\n", data, size);
		if (rh->type == RINGTYPE_RECORD) {
		    rtapi_print_hex_dump(RTAPI_MSG_ALL, RTAPI_DUMP_PREFIX_OFFSET,
					 16, 1, data, size, true,
					 hdprinter, "%6u ", size);
		    halcmd_output("\n");
		} else {
		    // multiframe
		    msgbuffer_t mrb;
		    msgbuffer_init(&mrb, rb);
		    // just iterate the record we already pulled above
		    mrb._read = data;
		    mrb.read_size = size;
		    ringvec_t rv;
		    mflag_t *mp = (mflag_t *) &rv.rv_flags;
		    fc = 0;
		    while (frame_readv(&mrb, &rv) == 0) {
			halcmd_output("record %zu/%zu  msgid=%d format=%d %s %s\n",
				      nr, fc,
				      mp->f.msgid, mp->f.format,
				      mp->f.more ? "more":"",
				      mp->f.eor ? "eor":"");
			rtapi_print_hex_dump(RTAPI_MSG_ALL, RTAPI_DUMP_PREFIX_OFFSET,
					     16, 1, rv.rv_base, rv.rv_len, true,
					     hdprinter, "%6u ", rv.rv_len);
			frame_shift(&mrb);
			fc++;
		    }

		    halcmd_output("\n");
		}
		if ((record_iter_shift(&ri)) == EINVAL)
		    continue;
		nr++;
		nb += size;
		tfc += fc;
	    }
	}
	break;

    case RINGTYPE_STREAM:
	size = stream_read_space(rh);
	halcmd_output("%s: stream ring, %u bytes unread\n", name, size);
	if (size) {
	    void *data = malloc(size);
	    if (!data) return -ENOMEM;
	    size = stream_peek(rb, data, size);
	    rtapi_print_hex_dump(RTAPI_MSG_ALL, RTAPI_DUMP_PREFIX_OFFSET,
				 16, 1, data, size, true,
				 hdprinter, NULL);

	    free(data);
	}
	break;
    }
    return 0;
}

int do_ringdump_cmd(char *ring)
{
    return with_ring_attached(ring, ringdump, NULL);
}

//convert hexstring to len bytes of data
//returns 0 on success, -1 on error
//data is a buffer of at least len bytes
//hexstring is upper or lower case hexadecimal, NOT prepended with "0x"
//http://stackoverflow.com/questions/3408706/hexadecimal-string-to-byte-array-in-c
int hex2data(char *data, const char *hexstring, unsigned int len)
{
    const char *pos = hexstring;
    char *endptr;
    size_t count = 0;

    if ((hexstring[0] == '\0') || (strlen((char *)hexstring) % 2)) {
        //hexstring contains no data
        //or hexstring has an odd length
        return -1;
    }

    for(count = 0; count < len; count++) {
        char buf[5] = {'0', 'x', pos[0], pos[1], 0};
        data[count] = strtol(buf, &endptr, 0);
        pos += 2 * sizeof(char);

        if (endptr[0] != '\0') {
            //non-hexadecimal character encountered
            return -1;
        }
    }
    return 0;
}

static int ringwrite(const char *name, ringbuffer_t *rb, void *arg)
{
    char **tokens = arg;
    ringheader_t *rh = rb->header;
    size_t size,wsize;
    int i, retval;
    msgbuffer_t mrb;
    bool have_flag = false;

    if (rh->type == RINGTYPE_MULTIPART)
	msgbuffer_init(&mrb, rb);

    for(i = 0; tokens[i] && *tokens[i]; i++) {
	char *s = tokens[i];
	unsigned flags = 0;
	char buf[1024];
	char *data, *sep;
	if ((sep = strchr(s,':')) != NULL) {
	    *sep = '\0';
	    char *cp = s;
	    flags = strtoul(s, &cp, 0);
	    if ((*cp != '\0') && (!isspace(*cp))) {
		halcmd_error("value '%s' invalid for flag (integer required)\n", s);
		return -EINVAL;
	    }
	    s = sep + 1;
	    have_flag = true;
	}
	if (strncasecmp(s, "0x",2) == 0) {
	    int count = strlen(s+2);
	    if (count & 1) {
		halcmd_error("%s: '%s' - odd number of hex nibbles: %d\n",
			     name, s, count);
		return -EINVAL;
	    }
	    count /= 2;
	    int n = hex2data(buf, s + 2, count);
	    if (n < 0) {
		halcmd_error("%s: '%s' - invalid hex string\n", name, s);
		return -EINVAL;
	    }
	    data = buf;
	    wsize = count;
	} else {
	    data = s;
	    wsize = strlen(s);
	}
	//printf("flag=%u data='%s' wsize=%zu\n",flags, data, wsize);

	switch (rh->type) {
	case RINGTYPE_MULTIPART:
	    retval = frame_write(&mrb, data, wsize, flags);
	    switch (retval) {
	    case EAGAIN:
		halcmd_error("%s: insufficient space for %zu bytes\n",name, wsize);
		break;
	    case ERANGE:
		halcmd_error("%s: write size %zu exceeds ringbuffer size \n",name, wsize);
		break;
	    default: ; // success
	    }
	    break;
	case RINGTYPE_RECORD:
	    if (have_flag) {
		halcmd_error("flag %d has no meaning for record ring '%s'\n",
			     flags, name);
		break;
	    }
	    retval = record_write(rb, data, wsize);
	    switch (retval) {
	    case EAGAIN:
		halcmd_error("%s: insufficient space for %zu bytes\n",name, wsize);
		break;
	    case ERANGE:
		halcmd_error("%s: write size %zu exceeds ringbuffer size \n",name, wsize);
		break;
	    default: ; // success
	    }
	    break;
	case RINGTYPE_STREAM:
	    if (have_flag) {
		halcmd_error("flag %d has no meaning for stream ring '%s'\n",
			     flags, name);
		break;
	    }
	    size = stream_write(rb, data, wsize);
	    if (size < wsize) {
		halcmd_error("%s: '%s' - space only for %zu out of %zu bytes\n",
			     name, data, size, wsize);
	    }
	}
    }
    switch (rh->type) {
    case RINGTYPE_MULTIPART:
	msg_write_flush(&mrb);
	break;
    case RINGTYPE_RECORD:
	break;
    case RINGTYPE_STREAM:;
    }
    return 0;
}

int do_ringwrite_cmd(char *ring,char *tokens[])
{
    return with_ring_attached(ring, ringwrite, tokens);
}

static int ringflush(const char *name, ringbuffer_t *rb, void *arg)
{
    ringheader_t *rh = rb->header;
    int result;
    size_t n;
    switch (rh->type) {
    case RINGTYPE_RECORD:
	result = record_flush_reader(rb);
	halcmd_output("%s: %d records flushed\n", name, result);
	break;
    case RINGTYPE_MULTIPART:
	result = record_flush_reader(rb);
	halcmd_output("%s: %d multiframes flushed\n", name, result);
	break;
    case RINGTYPE_STREAM:
	n = stream_flush(rb);
	halcmd_output("%s: %zu bytes flushed\n", name, n);
	break;
    }
    return 0;
}

int do_ringflush_cmd(char *ring)
{
    return with_ring_attached(ring, ringflush, NULL);
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
    int retval;

    WITH_HAL_MUTEX();
    hal_comp_t *comp = halpr_find_comp_by_name(comp_name);

    if (comp == NULL) {
	halcmd_error( "No such component: %s\n", comp_name);
	return -ENOENT;
    }
    if(comp->type != TYPE_REMOTE) {
	halcmd_error( "%s is not a remote component\n", comp_name);
	return -ENOSYS;
    }
    retval = halg_ready(0, ho_id(comp));
    if (retval < 0) {
	halcmd_error("ready: cant hal_ready component '%s':  %s\n",
		     comp_name, strerror(-retval));
	return -EINVAL;
    }
    return 0;
}

int do_newpin_cmd(char *comp_name, char *pin_name, char *type_name, char *args[])
{
    int retval;
    hal_type_t type = HAL_BIT; // shut up warning
    hal_pin_dir_t dir = HAL_IN;
    hal_comp_t *comp;
    hal_pin_t *pin;
    char *s,*cp;
    int flags = 0;
    int eps_index = 0;
    int i;
    void *p;

    {
	WITH_HAL_MUTEX();
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
	} else if (strcasecmp(type_name, "u64") == 0) {
	    type = HAL_U64;
	} else if (strcasecmp(type_name, "s64") == 0) {
	    type = HAL_S64;
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
#ifdef RCOMP_V1_PINS
	// hal_malloc wants the mutex but we're holding it so use halg_malloc
	if ((p = halg_malloc(0, sizeof(void *))) == NULL) {
	    halcmd_error("cant allocate memory for pin '%s'\n", pin_name);
	    return -EINVAL;
	}
	memset(p, 0, sizeof(void *));
#else
	p = NULL;
#endif
	// same here - use unlocked pin_new
	pin  = halg_pin_newf(0, type, dir, p, ho_id(comp), "%s", pin_name);
	if (pin == NULL) {
	    halcmd_error("cant create pin '%s':  %s\n",
			 pin_name, strerror(-retval));
	    return -EINVAL;
	}
	if (pin) {
	    pin->eps_index = eps_index;
	    pin->flags = flags;
	}
    } // unlocks HAL mutex
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
	halcmd_error("function call %s returned %d: %s\n", func, retval, rtapi_rpcerror());
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

cstatus_t classify_comp(const int use_halmutex, const char *comp)
{
    CHECK_HALDATA();
    CHECK_STR(comp);
    {
	WITH_HAL_MUTEX_IF(use_halmutex);
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
    cstatus_t status = classify_comp(1, comp);
    char *argv[] = { NULL};

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
    }

    retval = rtapi_newinst(rtapi_instance, comp, inst, (const char **)args);
    if (retval) {
	halcmd_error("rc=%d: %s\n", retval, rtapi_rpcerror());
	return retval;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////


int do_delinst_cmd(char *inst)
{
    {
	WITH_HAL_MUTEX();

	hal_inst_t *hi = halpr_find_inst_by_name(inst);
	if (hi == NULL) {
	    halcmd_error("no such instance: '%s'\n", inst);
	    return -1;
	}
    }
    int retval = rtapi_delinst(rtapi_instance, inst);
    if ( retval != 0 ) {
	halcmd_error("rc=%d: %s\n", retval, rtapi_rpcerror());
	return retval;
    }
    return 0;
}

static void print_inst_names(char **patterns)
{
    foreach_args_t args =  {
	.type = HAL_INST,
	.user_ptr1 = patterns
    };
    halg_foreach(1, &args, print_name);
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

static int save_comp_line(hal_object_ptr o, foreach_args_t *args)
{
FILE *dst = (FILE*) args->user_ptr2;

    hal_comp_t *comp = o.comp;
    if ( match(args->user_ptr1, ho_name(comp)) ) {
		if ( comp->type == TYPE_RT ) {
	    // FIXME XXX MAH - save halcmd defined remote comps!!
	    // only print realtime components
	    if ( comp->insmod_args == 0 )
			fprintf(dst, "#loadrt %s  (not loaded by loadrt, no args saved)\n", ho_name(comp));
	    else
			fprintf(dst, "loadrt %s %s\n", ho_name(comp),(char *)SHMPTR(comp->insmod_args));
		}
	}
    return 0; // continue iterating
}

static void save_comps(FILE *dst)
{
    fprintf(dst, "# components\n");

    foreach_args_t args =  {
    .type = HAL_COMPONENT,
    .user_ptr1 = NULL, // all of them
    .user_ptr2 = dst,
    };
    halg_foreach(true, &args, save_comp_line);

    fprintf(dst, "\n");
}

static int yield_objects(hal_object_ptr o, foreach_args_t *args)
{
    int retval = halg_object_setbarriers(0, o, args->user_arg1, args->user_arg2);
    return retval < 0 ? retval : 1;  // continue on no error, else propagate return code
}

static int change_barrier(char *object, int read_barrier, int write_barrier)
{
    foreach_args_t args =  {
	.name = (char *)object,
	.user_arg1 = read_barrier,  // -1 to leave as is /0/1
	.user_arg2 = write_barrier, // -1 to leave as is /0/1
    };
    return halg_foreach(1, &args, yield_objects);
}

int do_setrmb_cmd(char *object)
{
    int retval = change_barrier(object, 1, -1);
    if (retval < 0) {
    	halcmd_error("setrmb:  %s\n",  hal_lasterror());
	return retval;
    };
    return 0;
}

int do_setwmb_cmd(char *object)
{
    int retval =  change_barrier(object, -1, 1);
    if (retval < 0) {
    	halcmd_error("setwmb:  %s\n",  hal_lasterror());
	return retval;
    };
    return 0;
}

int do_clear_rmb_cmd(char *object)
{
    int retval =  change_barrier(object, 0, -1);
    if (retval < 0) {
    	halcmd_error("clearrmb:  %s\n",  hal_lasterror());
	return retval;
    };
    return 0;
}

int do_clear_wmb_cmd(char *object)
{
    int retval =  change_barrier(object, -1, 0);
    if (retval < 0) {
    	halcmd_error("clearwmb:  %s\n",  hal_lasterror());
	return retval;
    };
    return 0;
}

int do_handshake_cmd(char *signal)
{
    halcmd_error("NIY\n");
    return -ENOSYS;
}


////////////////////////////////////////////////////////////////////////

static int save_sig_line(hal_object_ptr o, foreach_args_t *args)
{
    FILE *dst = (FILE*) args->user_ptr1;
    int only_unlinked = args->user_arg1;
    hal_sig_t *sig = o.sig;

    if (!(only_unlinked && (sig->readers || sig->writers)) )
	fprintf(dst, "newsig %s %s\n", ho_name(sig), data_type((int) sig->type));

    return 0; // continue iterating
}

static void save_signals(FILE *dst, int only_unlinked)
{
    fprintf(dst, "# signals\n");

    foreach_args_t args =  {
	.type = HAL_SIGNAL,
	.user_ptr1 = dst,
	.user_arg1 = only_unlinked
    };
    halg_foreach(true, &args, save_sig_line);
    fprintf(dst, "\n");
}

static void save_links(FILE *dst, int arrow)
{
    halcmd_error("the link and linka commands are deprecated, use 'save net' instead\n");
}


static int fill_pin_array(hal_pin_t *pin, hal_sig_t *sig, void *user)
{
    // pin and sig are guaranteed to be non-NULL, and linked to each other
    FILE *dst = (FILE *) user;

    switch (pin->dir) {
    case HAL_IN:
	fprintf(dst, "net %s => %s\n", ho_name(sig), ho_name(pin) );
	break;
    case HAL_OUT:
	fprintf(dst, "net %s <= %s\n", ho_name(sig), ho_name(pin) );
	break;
    case HAL_IO:
	fprintf(dst, "net %s <=> %s\n", ho_name(sig), ho_name(pin) );
	break;
    default: ;
    }
    return 0; // continue iterating
}

static int save_net_line(hal_object_ptr o, foreach_args_t *args)
{
    halg_foreach_pin_by_signal(false, o.sig, fill_pin_array, args->user_ptr1);
    return 0;
}

static void save_nets(FILE *dst, int arrow)
{
    fprintf(dst, "# nets\n");

    foreach_args_t args =  {
	.type = HAL_SIGNAL,
	.user_ptr1 = dst,
	.user_arg1 = arrow // let's ignore this configurable arrow nonsense ;)
    };
    halg_foreach(true, &args, save_net_line);
    fprintf(dst, "\n");
}

////////////////////////////////////////////////////////////////////////

static int save_param_line(hal_object_ptr o, foreach_args_t *args)
{
    FILE *dst = (FILE*) args->user_ptr1;
    hal_param_t *param = o.param;

    if (param->dir != HAL_RO){
        //param is writable, save its value
        fprintf(dst, "setp %s %s\n", ho_name(param),
		data_value((int) param->type, SHMPTR(param->data_ptr)));
    }
    return 0; // continue
}

static void save_params(FILE *dst)
{
    fprintf(dst, "# parameter values\n");
    foreach_args_t args =  {
	.type = HAL_PARAM,
	.user_ptr1 = dst
    };
    halg_foreach(true, &args, save_param_line);
    fprintf(dst, "\n");

}

static int save_thread_line(hal_object_ptr o, foreach_args_t *args)
{
    FILE *dst = (FILE*) args->user_ptr1;

    hal_thread_t *tptr = o.thread;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    list_root = &(tptr->funct_list);
    list_entry = dlist_next(list_root);
    while (list_entry != list_root) {
	/* print the function info */
	fentry = (hal_funct_entry_t *) list_entry;
	funct = SHMPTR(fentry->funct_ptr);
	fprintf(dst, "addf %s %s\n", ho_name(funct), ho_name(tptr));
	list_entry = dlist_next(list_entry);
    }
    return 0; // continue
}

static void save_threads(FILE *dst)
{
    fprintf(dst, "# realtime thread/function links\n");

    foreach_args_t args =  {
	.type = HAL_THREAD,
	.user_ptr1 = dst
    };
    halg_foreach(true, &args, save_thread_line);
    fprintf(dst, "\n");
}

////////////////////////////////////////////////////////////////////////

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
int do_newthread_cmd(char *name, char *args[])
{
    int i, retval;
    bool use_fp = false;
    int cpu = -1;
    char *s;
    int per = 1000000;
    int flags = 0;

    for (i = 0; ((s = args[i]) != NULL) && strlen(s); i++) {
	if (sscanf(s, "cpu=%d", &cpu) == 1)
	    continue;
	if (strcmp(s, "fp") == 0) {
	    use_fp = true;
	    continue;
	}
	if (strcmp(s, "nofp") == 0) {
	    use_fp = false;
	    continue;
	}
	if (strcmp(s, "posix") == 0) {
	    flags |= TF_NONRT;
	    continue;
	}
	if (strcmp(s, "nowait") == 0) {
	    flags |= TF_NOWAIT;
	    continue;
	}
	char *cp = s;
	per = strtol(s, &cp, 0);
	if ((*cp != '\0') && (!isspace(*cp))) {
	    halcmd_error("value '%s' invalid for period\n", s);
	    retval = -EINVAL;
	}
    }
    if ((per < 10000) && !(flags & TF_NOWAIT))
	halcmd_warning("a period < 10uS is unlikely to work\n");

    if ((flags & (TF_NOWAIT|TF_NONRT)) == TF_NOWAIT){
	halcmd_info("specifying 'nowait' without 'posix' makes it easy to lock up RT\n");
    }

    retval = rtapi_newthread(rtapi_instance, name, per, cpu, (int)use_fp, flags);
    if (retval)
	halcmd_error("rc=%d: %s\n",retval,rtapi_rpcerror());

    return retval;
}


// delete an RT thread
int do_delthread_cmd(char *name)
{
    int retval =  rtapi_delthread(rtapi_instance, name);
    if (retval)
	halcmd_error("rc=%d: %s\n",retval, rtapi_rpcerror());
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
	printf("  is 'bit', 'float', 'u32', 's32', 'u64', or 's64'.\n");
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

