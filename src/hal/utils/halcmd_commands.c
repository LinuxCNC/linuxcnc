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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */
#include "halcmd_commands.h"
#include <rtapi_mutex.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <fnmatch.h>


static int unloadrt_comp(char *mod_name);
static void print_comp_info(char **patterns);
static void print_pin_info(int type, char **patterns);
static void print_pin_aliases(char **patterns);
static void print_param_aliases(char **patterns);
static void print_sig_info(int type, char **patterns);
static void print_script_sig_info(int type, char **patterns);
static void print_param_info(int type, char **patterns);
static void print_funct_info(char **patterns);
static void print_thread_info(char **patterns);
static void print_comp_names(char **patterns);
static void print_pin_names(char **patterns);
static void print_sig_names(char **patterns);
static void print_param_names(char **patterns);
static void print_funct_names(char **patterns);
static void print_thread_names(char **patterns);
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
static void save_unconnected_input_pin_values(FILE *dst);
static void save_threads(FILE *dst);
static void print_help_commands(void);

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
       don't want to create a sig, which after that won't be useful */
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
        halcmd_error("addf failed\n");
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

#if 0  /* newinst deferred to version 2.2 */
int do_newinst_cmd(char *comp_name, char *inst_name) {
    hal_comp_t *comp = halpr_find_comp_by_name(comp_name);

    if(!comp) {
        halcmd_error( "No such component: %s\n", comp_name);
        return -ENOENT;
    }
    if(!comp->make) {
        halcmd_error( "%s does not support 'newinst'\n", comp_name);
        return -ENOSYS;
    }
    if ( *inst_name == '\0' ) {
        halcmd_error( "Must supply name for new instance\n");
        return -EINVAL;
    }	

#if defined(RTAPI_USPACE)
    {
        char *argv[MAX_TOK];
        int m = 0, result;
        argv[m++] = EMC2_BIN_DIR "/rtapi_app";
        argv[m++] = "newinst";
        argv[m++] = comp_name;
        argv[m++] = inst_name;
        argv[m++] = 0;
        result = hal_systemv(argv);
        if(result != 0) {
            halcmd_error( "newinst failed: %d\n", result);
            return -EINVAL;
        }
    }
#else
    {
    FILE *f;
    f = fopen("/proc/rtapi/hal/newinst", "w");
    if(!f) {
        halcmd_error( "cannot open proc entry: %s\n",
                strerror(errno));
        return -EINVAL;
    }

    rtapi_mutex_get(&(hal_data->mutex));

    while(hal_data->pending_constructor) {
        struct timespec ts = {0, 100 * 1000 * 1000}; // 100ms
        rtapi_mutex_give(&(hal_data->mutex));
        nanosleep(&ts, NULL);
        rtapi_mutex_get(&(hal_data->mutex));
    }
    strncpy(hal_data->constructor_prefix, inst_name, HAL_NAME_LEN);
    hal_data->constructor_prefix[HAL_NAME_LEN]=0;
    hal_data->pending_constructor = comp->make;
    rtapi_mutex_give(&(hal_data->mutex));

    if(fputc(' ', f) == EOF) {
        halcmd_error( "cannot write to proc entry: %s\n",
                strerror(errno));
        fclose(f);
        rtapi_mutex_get(&(hal_data->mutex));
        hal_data->pending_constructor = 0;
        rtapi_mutex_give(&(hal_data->mutex));
        return -EINVAL;
    }
    if(fclose(f) != 0) {
        halcmd_error(
                "cannot close proc entry: %s\n",
                strerror(errno));
        rtapi_mutex_get(&(hal_data->mutex));
        hal_data->pending_constructor = 0;
        rtapi_mutex_give(&(hal_data->mutex));
        return -EINVAL;
    }

    while(hal_data->pending_constructor) {
        struct timespec ts = {0, 100 * 1000 * 1000}; // 100ms
        nanosleep(&ts, NULL);
    }
    }
#endif
    rtapi_mutex_get(&hal_data->mutex);
    {
    hal_comp_t *inst = halpr_alloc_comp_struct();
    if (inst == 0) {
        /* couldn't allocate structure */
        rtapi_mutex_give(&(hal_data->mutex));
        halcmd_error(
            "insufficient memory for instance '%s'\n", inst_name);
        return -ENOMEM;
    }
    inst->comp_id = comp->comp_id | 0x10000;
    inst->mem_id = -1;
    inst->type = 2;
    inst->pid = 0;
    inst->ready = 1;
    inst->shmem_base = 0;
    rtapi_snprintf(inst->name, sizeof(inst->name), "%s", inst_name);
    /* insert new structure at head of list */
    inst->next_ptr = hal_data->comp_list_ptr;
    hal_data->comp_list_ptr = SHMOFF(inst);

    rtapi_mutex_give(&(hal_data->mutex));
    }
    return 0;
}
#endif /* newinst deferred */

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
            /* found it */
            type = pin->type;
            if(pin->dir == HAL_OUT) {
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
    halcmd_error("pin or parameter '%s' not found\n", name);
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
    halcmd_error("pin or parameter '%s' not found\n", name);
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

int do_show_cmd(char *type, char **patterns)
{

    if (rtapi_get_msg_level() == RTAPI_MSG_NONE) {
	/* must be -Q, don't print anything */
	return 0;
    }
    if (!type || *type == '\0') {
	/* print everything */
	print_comp_info(NULL);
	print_pin_info(-1, NULL);
	print_pin_aliases(NULL);
	print_sig_info(-1, NULL);
	print_param_info(-1, NULL);
	print_param_aliases(NULL);
	print_funct_info(NULL);
	print_thread_info(NULL);
    } else if (strcmp(type, "all") == 0) {
	/* print everything, using the pattern */
	print_comp_info(patterns);
	print_pin_info(-1, patterns);
	print_pin_aliases(patterns);
	print_sig_info(-1, patterns);
	print_param_info(-1, patterns);
	print_param_aliases(patterns);
	print_funct_info(patterns);
	print_thread_info(patterns);
    } else if (strcmp(type, "comp") == 0) {
	print_comp_info(patterns);
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

int do_loadrt_cmd(char *mod_name, char *args[])
{
    char arg_string[MAX_CMD_LEN+1];
    int m=0, n=0, retval;
    hal_comp_t *comp;
    char *argv[MAX_TOK+3];
    char *cp1;
#if defined(RTAPI_USPACE)
    argv[m++] = "-Wn";
    argv[m++] = mod_name;
    argv[m++] = EMC2_BIN_DIR "/rtapi_app";
    argv[m++] = "load";
    argv[m++] = mod_name;
    /* loop thru remaining arguments */
    while ( args[n] && args[n][0] != '\0' ) {
        argv[m++] = args[n++];
    }
    argv[m++] = NULL;
    retval = do_loadusr_cmd(argv);
#else
    static char *rtmod_dir = EMC2_RTLIB_DIR;
    struct stat stat_buf;
    char mod_path[MAX_CMD_LEN+1];

    if (hal_get_lock()&HAL_LOCK_LOAD) {
	halcmd_error("HAL is locked, loading of modules is not permitted\n");
	return -EPERM;
    }
    if ( (strlen(rtmod_dir)+strlen(mod_name)+5) > MAX_CMD_LEN ) {
	halcmd_error("Module path too long\n");
	return -1;
    }

    /* make full module name '<path>/<name>.o' */
    {
        int r;
        r = snprintf(mod_path, sizeof(mod_path), "%s/%s%s", rtmod_dir, mod_name, MODULE_EXT);
        if (r < 0) {
            halcmd_error("error making module path for %s/%s%s\n", rtmod_dir, mod_name, MODULE_EXT);
            return -1;
        } else if (r >= sizeof(mod_path)) {
            // truncation!
            halcmd_error("module path too long (max %lu) for %s/%s%s\n", (unsigned long)sizeof(mod_path)-1, rtmod_dir, mod_name, MODULE_EXT);
            return -1;
        }
    }

    /* is there a file with that name? */
    if ( stat(mod_path, &stat_buf) != 0 ) {
        /* can't find it */
        halcmd_error("Can't find module '%s' in %s\n", mod_name, rtmod_dir);
        return -1;
    }
    
    argv[0] = EMC2_BIN_DIR "/linuxcnc_module_helper";
    argv[1] = "insert";
    argv[2] = mod_path;
    /* loop thru remaining arguments */
    n = 0;
    m = 3;
    while ( args[n] && args[n][0] != '\0' ) {
        argv[m++] = args[n++];
    }
    /* add a NULL to terminate the argv array */
    argv[m] = NULL;

    retval = hal_systemv(argv);
#endif

    if ( retval != 0 ) {
	halcmd_error("insmod for %s failed, returned %d\n"
#if !defined(RTAPI_USPACE)
            "See the output of 'dmesg' for more information.\n"
#endif
        , mod_name, retval );
	return -1;
    }
    /* make the args that were passed to the module into a single string */
    n = 0;
    arg_string[0] = '\0';
    while ( args[n] && args[n][0] != '\0' ) {
	strncat(arg_string, args[n++], MAX_CMD_LEN);
	strncat(arg_string, " ", MAX_CMD_LEN);
    }
    /* allocate HAL shmem for the string */
    cp1 = hal_malloc(strlen(arg_string)+1);
    if ( cp1 == NULL ) {
	halcmd_error("failed to allocate memory for module args\n");
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
	halcmd_error("module '%s' not loaded\n", mod_name);
	return -EINVAL;
    }
    /* link args to comp struct */
    comp->insmod_args = SHMOFF(cp1);
    rtapi_mutex_give(&(hal_data->mutex));
    /* print success message */
    halcmd_info("Realtime module '%s' loaded\n", mod_name);
    return 0;
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
	if ( comp->type == 0 && comp->pid != ourpid) {
	    /* found a userspace component besides us */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		/* we want to unload this component, send it SIGTERM */
                kill(abs(comp->pid), SIGTERM);
	    }
	}
	next = comp->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    return 0;
}


int do_unloadrt_cmd(char *mod_name)
{
    int next, retval, retval1, n, all;
    hal_comp_t *comp;
    char comps[64][HAL_NAME_LEN+1];

    /* check for "all" */
    if ( strcmp(mod_name, "all" ) == 0 ) {
	all = 1;
    } else {
	all = 0;
    }
    /* build a list of component(s) to unload */
    n = 0;
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
	    /* found a realtime component */
	    if ( all || ( strcmp(mod_name, comp->name) == 0 )) {
		/* we want to unload this component, remember its name */
		if ( n < 63 ) {
		    strncpy(comps[n], comp->name, HAL_NAME_LEN );
		    comps[n][HAL_NAME_LEN] = '\0';
		    n++;
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
	halcmd_error("component '%s' is not loaded\n", mod_name);
	return -1;
    }
    /* we now have a list of components, unload them */
    n = 0;
    retval1 = 0;
    while ( comps[n][0] != '\0' ) {
        // special case: initial prefix means it is not a real comp
        if (strstr(comps[n],HAL_PSEUDO_COMP_PREFIX) == comps[n] ) {
           n++;
           continue;
        }
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
    if (retval1 < 0) {
	halcmd_error("unloadrt failed\n");
    }
    return retval1;
}

static int unloadrt_comp(char *mod_name)
{
    int retval;
    char *argv[4];

#if defined(RTAPI_USPACE)
    argv[0] = EMC2_BIN_DIR "/rtapi_app";
    argv[1] = "unload";
#else
    argv[0] = EMC2_BIN_DIR "/linuxcnc_module_helper";
    argv[1] = "remove";
#endif
    argv[2] = mod_name;
    /* add a NULL to terminate the argv array */
    argv[3] = NULL;

    retval = hal_systemv(argv);

    if ( retval != 0 ) {
	halcmd_error("rmmod failed, returned %d\n", retval);
	return -1;
    }
    /* print success message */
    halcmd_info("Realtime module '%s' unloaded\n",
	mod_name);
    return 0;
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
        if(type == 1) return do_unloadrt_cmd(mod_name);
        else return do_unloadusr_cmd(mod_name);
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

static void reset_getopt_state() {
/*

It turns out that it is not portable to reset the state of getopt, so that
a different argv list can be parsed.

https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=192834

(though that thread ends with the bug being closed as fixed in lenny, it
is not fixed or has regressed by debian jessie)
*/

#ifdef __GNU_LIBRARY__
    optind = 0;
#else
    optind = 1;
#endif

#ifdef HAVE_OPTRESET
    optreset = 1;
#endif
}

int do_loadusr_cmd(char *args[])
{
    int wait_flag, wait_comp_flag, ignore_flag;
    char *prog_name, *new_comp_name=NULL;
    char *argv[MAX_TOK+1];
    int n, m, retval, status;
    pid_t pid;

    int argc = 0;
    while(args[argc] && *args[argc]) argc++;
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
    reset_getopt_state();
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
    hal_ready(comp_id);
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
	        if (WIFEXITED(status) && WEXITSTATUS(status)) {
	            halcmd_error("waitpid failed %s %s\n",prog_name,new_comp_name);
	            ready = 0;
	            break;
	        }
	    }
	    /* check for program becoming ready */
            rtapi_mutex_get(&(hal_data->mutex));
            comp = halpr_find_comp_by_name(new_comp_name);
            if(comp && comp->ready) {
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


int do_waitusr_cmd(char *comp_name)
{
    hal_comp_t *comp;
    int exited;

    if (*comp_name == '\0') {
	halcmd_error("component name missing\n");
	return -EINVAL;
    }
    rtapi_mutex_get(&(hal_data->mutex));
    comp = halpr_find_comp_by_name(comp_name);
    if (comp == NULL) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_info("component '%s' not found or already exited\n", comp_name);
	return 0;
    }
    if (comp->type != 0) {
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_error("'%s' is not a userspace component\n", comp_name);
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


static void print_comp_info(char **patterns)
{
    int next;
    hal_comp_t *comp;

    if (scriptmode == 0) {
	halcmd_output("Loaded HAL Components:\n");
	halcmd_output("ID      Type  %-*s PID   State\n", HAL_NAME_LEN, "Name");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( match(patterns, comp->name) ) {
            if(comp->type == 2) {
                hal_comp_t *comp1 = halpr_find_comp_by_id(comp->comp_id & 0xffff);
                halcmd_output("    INST %s %s",
                        comp1 ? comp1->name : "(unknown)", 
                        comp->name);
            } else {
                halcmd_output(" %5d  %-4s  %-*s",
                    comp->comp_id, (comp->type ? "RT" : "User"),
                    HAL_NAME_LEN, comp->name);
                if(comp->type == 0) {
                        halcmd_output(" %5d %s", comp->pid, comp->ready > 0 ?
                                "ready" : "initializing");
                } else {
                        halcmd_output(" %5s %s", "", comp->ready > 0 ?
                                "ready" : "initializing");
                }
            }
            halcmd_output("\n");
	}
	next = comp->next_ptr;
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
	halcmd_output("Owner   Type  Dir         Value  Name\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->pin_list_ptr;
    while (next != 0) {
	pin = SHMPTR(next);
	if ( tmatch(type, pin->type) && match(patterns, pin->name) ) {
	    comp = SHMPTR(pin->owner_ptr);
	    if (pin->signal != 0) {
		sig = SHMPTR(pin->signal);
		dptr = SHMPTR(sig->data_ptr);
	    } else {
		sig = 0;
		dptr = &(pin->dummysig);
	    }
	    if (scriptmode == 0) {
		halcmd_output(" %5d  %5s %-3s  %9s  %s",
		    comp->comp_id,
		    data_type((int) pin->type),
		    pin_data_dir((int) pin->dir),
		    data_value((int) pin->type, dptr),
		    pin->name);
	    } else {
		halcmd_output("%s %s %s %s %s",
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
	halcmd_output("Owner   Type  Dir         Value  Name\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->param_list_ptr;
    while (next != 0) {
	param = SHMPTR(next);
	if ( tmatch(type, param->type), match(patterns, param->name) ) {
	    comp = SHMPTR(param->owner_ptr);
	    if (scriptmode == 0) {
		halcmd_output(" %5d  %5s %-3s  %9s  %s\n",
		    comp->comp_id, data_type((int) param->type),
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

static void print_funct_info(char **patterns)
{
    int next;
    hal_funct_t *fptr;
    hal_comp_t *comp;

    if (scriptmode == 0) {
	halcmd_output("Exported Functions:\n");
	halcmd_output("Owner   CodeAddr  Arg       FP   Users  Name\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->funct_list_ptr;
    while (next != 0) {
	fptr = SHMPTR(next);
	if ( match(patterns, fptr->name) ) {
	    comp = SHMPTR(fptr->owner_ptr);
	    if (scriptmode == 0) {
		halcmd_output(" %05d  %08lx  %08lx  %-3s  %5d   %s\n",
		    comp->comp_id,
		    (long)fptr->funct,
		    (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
		    fptr->users, fptr->name);
	    } else {
		halcmd_output("%s %08lx %08lx %s %3d %s\n",
		    comp->name,
		    (long)fptr->funct,
		    (long)fptr->arg, (fptr->uses_fp ? "YES" : "NO"),
		    fptr->users, fptr->name);
	    } 
	}
	next = fptr->next_ptr;
    }
    rtapi_mutex_give(&(hal_data->mutex));
    halcmd_output("\n");
}

static void print_thread_info(char **patterns)
{
    int next_thread, n;
    hal_thread_t *tptr;
    hal_list_t *list_root, *list_entry;
    hal_funct_entry_t *fentry;
    hal_funct_t *funct;

    if (scriptmode == 0) {
	halcmd_output("Realtime Threads:\n");
	halcmd_output("     Period  FP     Name               (     Time, Max-Time )\n");
    }
    rtapi_mutex_get(&(hal_data->mutex));
    next_thread = hal_data->thread_list_ptr;
    while (next_thread != 0) {
	tptr = SHMPTR(next_thread);
	if ( match(patterns, tptr->name) ) {
		/* note that the scriptmode format string has no \n */
            char name[HAL_NAME_LEN+1];
            hal_pin_t* pin;
            hal_sig_t *sig;
            void *dptr;
  
            snprintf(name, sizeof(name), "%s.time",tptr->name);
            pin = halpr_find_pin_by_name(name);
            if (pin) {
                if (pin->signal != 0) {
                    sig = SHMPTR(pin->signal);
                    dptr = SHMPTR(sig->data_ptr);
                } else {
                    sig = 0;
                    dptr = &(pin->dummysig);
                }

                halcmd_output(((scriptmode == 0) ? "%11ld  %-3s  %20s ( %8ld, %8ld )\n"
                                                 : "%ld %s %s %8ld %ld"),
                              tptr->period,
                              (tptr->uses_fp ? "YES" : "NO"),
                              tptr->name,
                              (long)*(long*)dptr,
                              (long)tptr->maxtime);
            } else {
                rtapi_print_msg(RTAPI_MSG_ERR,
                     "unexpected: cannot find time pin for %s thread",tptr->name);
            }


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

static void print_mem_status()
{
    int active, recycled, next;
    hal_pin_t *pin;
    hal_param_t *param;

    halcmd_output("HAL memory status\n");
    halcmd_output("  used/total shared memory:   %ld/%d\n", (long)(HAL_SIZE - hal_data->shmem_avail), HAL_SIZE);
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
    if (   (strcmp(type, "all")  == 0)
	|| (strcmp(type, "allu") == 0) ) {
	/* save everything */
	save_comps(dst);
	save_aliases(dst);
	save_signals(dst, 1);
	save_nets(dst, 3);
	save_params(dst);
	if (strcmp(type,"allu") == 0) {
	    save_unconnected_input_pin_values(dst);
	}
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
    } else if (strcmp(type, "unconnectedinpins") == 0) {
	save_unconnected_input_pin_values(dst);
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

static void save_comps(FILE *dst)
{
    int next;
    hal_comp_t *comp;

    fprintf(dst, "# components\n");
    rtapi_mutex_get(&(hal_data->mutex));

    int ncomps = 0;
    next = hal_data->comp_list_ptr;
    while (next != 0) {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
            ncomps ++;
        }
	next = comp->next_ptr;
    }

    hal_comp_t *comps[ncomps], **compptr = comps;
    next = hal_data->comp_list_ptr;
    while(next != 0)  {
	comp = SHMPTR(next);
	if ( comp->type == 1 ) {
            *compptr++ = SHMPTR(next);
        }
	next = comp->next_ptr;
    }

    int i;
    for(i=ncomps; i--;)
    {
        comp = comps[i];
        /* only print realtime components */
        if ( comp->insmod_args == 0 ) {
            fprintf(dst, "#loadrt %s  (not loaded by loadrt, no args saved)\n", comp->name);
        } else {
            fprintf(dst, "loadrt %s %s\n", comp->name,
                (char *)SHMPTR(comp->insmod_args));
        }
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

static void save_unconnected_input_pin_values(FILE *dst)
{
    hal_pin_t *pin;
    void *dptr;
    int next;
    fprintf(dst, "# unconnected pin values\n");
    for(next = hal_data->pin_list_ptr; next; next=pin->next_ptr)
    {
        pin = SHMPTR(next);
        if (   (pin->signal == 0)
            && ( (pin->dir == HAL_IN) || (pin->dir == HAL_IO) )
           ) {
            dptr = &(pin->dummysig);
            fprintf(dst, "setp %s %s\n",
                   pin->name, data_value((int) pin->type, dptr));
        }
    }
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
	printf("  -Wn NAME  wait for component named NAME to become ready\n");
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
	printf("  Type can be 'comp', 'alias', 'sig[u]', 'signal', 'link[a]'\n");
        printf("  'net[a]', 'netl', 'netla', netal', 'param', 'parameter,\n");
	printf("  'unconnectedinpins', 'all, 'allu', or 'thread'.\n");
        printf("  (A final 'a' character (like 'neta' means show arrows for pin direction.)\n");
        printf("  If 'type' is 'allu'), does the equivalent of:\n");
	printf("  'comp', 'alias', 'sigu', 'netla', 'param', 'unconnectedinpins' and 'thread'.\n\n");
        printf("  If 'type' is omitted (or type is 'all'), does the equivalent of:\n");
	printf("  'comp', 'alias', 'sigu', 'netla', 'param', and 'thread'.\n\n");
        printf("  See the man page ($man halcmd) for save option details\n");
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
