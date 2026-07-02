/* Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2003 John Kasunich
 *                     <jmkasunich AT users DOT sourceforge DOT net>
 * Copyright (C) 2026 B.Stultiens
 *
 *  Other contributors:
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
#include <rtapi.h>		// RTAPI realtime OS API
#include <hal.h>		// HAL public API decls
#include "halcmd_commands.h"
#include <rtapi_string.h>	// rtapi_strlcpy

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
#include <string>
#include <vector>
#include <fmt/format.h>

#include "setps_util.h"

static int unloadrt_comp(const char *mod_name);
static void print_comp_info(const char **patterns);
static void print_pin_info(int type, const char **patterns);
static void print_pin_aliases(const char **patterns);
static void print_param_aliases(const char **patterns);
static void print_sig_info(int type, const char **patterns);
static void print_script_sig_info(int type, const char **patterns);
static void print_param_info(int type, const char **patterns);
static void print_funct_info(const char **patterns);
static void print_thread_info(const char **patterns);
static void print_comp_names(const char **patterns);
static void print_pin_names(const char **patterns);
static void print_sig_names(const char **patterns);
static void print_param_names(const char **patterns);
static void print_funct_names(const char **patterns);
static void print_thread_names(const char **patterns);
static void print_lock_status();
static void print_mem_status();
static const char *data_type(hal_type_t type);
static const char *data_type2(hal_type_t type);
static const char *pin_data_dir(hal_pdir_t dir);
static const char *param_data_dir(hal_pdir_t dir);
static const char *data_arrow1(hal_pdir_t dir);
static const char *data_arrow2(hal_pdir_t dir);
static std::string querydata_refstr_20(hal_type_t type, hal_refs_u u);
static std::string querydata_refstr(hal_type_t type, hal_refs_u u);
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

static int match(const char **patterns, const char *value) {
    int i;
    if(!patterns || !patterns[0] || !patterns[0][0]) return 1;
    for(i=0; patterns[i] && *patterns[i]; i++) {
	const char *pattern = patterns[i];
	if(strncmp(pattern, value, strlen(pattern)) == 0) return 1;
	if (fnmatch(pattern, value, 0) == 0) return 1;
    }
    return 0;
}

int do_lock_cmd(const char *command)
{
	int retval = 0;

	/* if command is blank or "all", want to lock everything */
	if ((command == NULL) || (strcmp(command, "all") == 0)) {
		retval = hal_set_lock(HAL_LOCK_ALL);
	} else if (strcmp(command, "none") == 0) {
		retval = hal_set_lock(HAL_LOCK_NONE);
	} else if (strcmp(command, "tune") == 0) {
		retval = hal_set_lock(HAL_LOCK_TUNE);
	}

    if (retval == 0) {
	/* print success message */
	halcmd_info("Locking completed");
    } else {
	halcmd_error("Locking failed\n");
    }
    return retval;
}

int do_unlock_cmd(const char *command)
{
	int retval = 0;

	/* if command is blank or "all", want to unlock everything */
	if ((command == NULL) || (strcmp(command, "all") == 0)) {
		retval = hal_set_lock(HAL_LOCK_NONE);
	} else if (strcmp(command, "none") == 0) {
		retval = hal_set_lock(HAL_LOCK_NONE);
	} else if (strcmp(command, "tune") == 0) {
		retval = hal_set_lock(hal_get_lock() & ~HAL_LOCK_TUNE);
	}

    if (retval == 0) {
	/* print success message */
	halcmd_info("Unlocking completed");
    } else {
	halcmd_error("Unlocking failed\n");
    }
    return retval;
}

int do_linkpp_cmd(const char *pname1, const char *pname2)
{
    static int dep_msg_printed = 0;
    if ( dep_msg_printed == 0 ) {
        halcmd_warning("linkpp command is deprecated, use 'net'\n");
        dep_msg_printed = 1;
    }

    hal_query_t p1 = {};
    p1.name = pname1;
    int rv = hal_getref_p(&p1);
    if(rv) {
        // First pin not found
        halcmd_error("pin '%s' not found\n", pname1);
        return -EINVAL;
    }

    hal_query_t p2 = {};
    p2.name = pname2;
    rv = hal_getref_p(&p2);
    if(rv) {
        // Second pin not found
        halcmd_error("pin '%s' not found\n", pname2);
        return -EINVAL;
    }

    if(p1.pp.type != p2.pp.type) {
        // Disjoint types
        halcmd_error("pins '%s' and '%s' not of the same type\n", pname1, pname2);
        return -EINVAL;
    }

    if(0 != (rv = hal_signal_new(pname1, p1.pp.type))) {
        halcmd_error("linkpp_cmd: failed to create signal '%s' with type %d\n", pname1, (int)p1.pp.type);
        return rv;
    }

    if(0 != (rv = hal_link(pname1, pname1))) {
        halcmd_error("linkpp_cmd: failed to link pin '%s' to signal '%s'\n", pname1, pname1);
        return rv;
    }

    if(0 != (rv = hal_link(pname2, pname1))) {
        halcmd_error("linkpp_cmd: failed to link pin '%s' to signal '%s'\n", pname2, pname1);
        return rv;
    }

    return 0;
}

int do_linkps_cmd(const char *pin, const char *sig)
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

int do_linksp_cmd(const char *sig, const char *pin) {
    return do_linkps_cmd(pin, sig);
}


int do_unlinkp_cmd(const char *pin)
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

int do_set_debug_cmd(const char *level){
    int retval = -EINVAL;
#if defined(RTAPI_USPACE)
    int m = 0;
    const char *argv[4];
    argv[m++] = EMC2_BIN_DIR "/rtapi_app";
    argv[m++] = "debug";
    argv[m++] = level;
    argv[m++] = NULL;
    retval = hal_systemv(argv);
#else
    halcmd_error("debug: not implemented for anything else than uspace\n");
#endif
    return retval;
}

int do_source_cmd(const char *hal_filename) {
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
        if(readresult == NULL) {
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
int do_addf_cmd(const char *func, const char *thread, const char **opt) {
    const char *position_str = opt ? opt[0] : NULL;
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

int do_initf_cmd(const char *func, const char *thread, const char **opt) {
    /* usage: initf <funct> <thread> [position]
       position has the same meaning as in addf: +N from start of the init
       list (+1 = run first), -N from end (-1 = run last, default), 0 illegal.
       The function runs once in realtime context in a dedicated cycle before
       the cyclic funct list; next cyclic cycle wakes one period later. */
    const char *position_str = opt ? opt[0] : NULL;
    int position = -1;
    int retval;

    if(position_str && *position_str) position = atoi(position_str);

    retval = hal_init_funct_to_thread(func, thread, position);
    if(retval == 0) {
        halcmd_info("Init function '%s' registered on thread '%s'\n",
                    func, thread);
    } else if(retval == -EALREADY) {
        halcmd_error("initf: thread '%s' init cycle already executed; "
                     "'%s' was NOT registered\n", thread, func);
    } else {
        halcmd_error("initf failed\n");
    }
    return retval;
}

int do_alias_cmd(const char *pinparam, const char *name, const char *alias) {
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

int do_unalias_cmd(const char *pinparam, const char *name) {
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
int do_delf_cmd(const char *func, const char *thread) {
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

struct net_cmd_data_t {
    const char *writer_name;
    const char *bidir_name;
};

static int preflight_net_cmd_cb(hal_query_t *q, void *arg)
{
    const char *signame = (const char *)arg;
    net_cmd_data_t *ncd = (net_cmd_data_t *)q->callerdata.vpval;

    if(q->pp.signal && !strcmp(q->pp.signal, signame)) {
        if(q->pp.dir == HAL_OUT)
            ncd->writer_name = q->name;
        if(q->pp.dir == HAL_IO)
            ncd->bidir_name = ncd->writer_name = q->name;
    }
    return 0;
}

static int preflight_net_cmd(const char *signal, const hal_query_t *qsig, const char *pins[], hal_type_t *typep)
{
    hal_type_t type = qsig->sig.type;
    int writers = qsig->sig.writers;
    int bidirs  = qsig->sig.bidirs;
    int pincnt = 0;
    net_cmd_data_t ncd = { NULL, NULL};

    if(writers || bidirs) 
    {
        hal_query_t qp = {};
        qp.qtype = HAL_QTYPE_PIN;
        qp.callerdata.vpval = (void *)&ncd;
        hal_list_p(&qp, preflight_net_cmd_cb, (void *)signal);
    }

    for(int i = 0; pins[i] && *pins[i]; i++) {
        hal_query_t qp = {};
        qp.qtype = HAL_QTYPE_PIN;
        qp.name = pins[i];
        int rv = hal_getref_p(&qp);
        if(rv) {
            // In theory, other errors may occur...
            halcmd_error("Pin '%s' does not exist\n", pins[i]);
            return -ENOENT;
        }

        if(qp.pp.signal) {
            if(strcmp(qp.pp.signal, signal)) {
                halcmd_error("Pin '%s' was already linked to signal '%s'\n", qp.name, qp.pp.signal);
                return -EINVAL;
            } else {
                // Already linked
                pincnt++;
                continue;
            }
        }

	if(0 == type) {
            // no pre-existing type, use this pin's type
            type = qp.pp.type;
	}
        if(type != qp.pp.type) {
            halcmd_error("Signal '%s' of type '%s' cannot add pin '%s' of type '%s'\n",
                signal, data_type2(type), qp.name, data_type2(qp.pp.type));
            return -EINVAL;
        }

        if(HAL_OUT == qp.pp.dir) {
            if(writers || bidirs) {
dir_error:
                halcmd_error( "Signal '%s' can not add %s pin '%s', it already has %s pin '%s'\n",
                    signal, pin_data_dir(qp.pp.dir), qp.name,
                    ncd.bidir_name ? pin_data_dir(HAL_IO) : pin_data_dir(HAL_OUT),
                    ncd.bidir_name ? ncd.bidir_name : ncd.writer_name);
                return -EINVAL;
            }
            ncd.writer_name = qp.name;
            writers++;
        }

        if(HAL_IO == qp.pp.dir) {
            if(writers) {
                goto dir_error;
            }
            ncd.bidir_name = qp.name;
            bidirs++;
        }
        pincnt++;
    }
    *typep = type;
    if(pincnt)
        return 0;
    halcmd_error("'net' requires at least one pin, none given\n");
    return -EINVAL;
}

int do_net_cmd(const char *signal, const char *pins[])
{
    int rv;
    hal_query_t qs = {};
    qs.name = signal;
    int havesig = hal_getref_s(&qs);
    if(0 != havesig && -ENOENT != havesig) {
        halcmd_error("net_cmd: failed signal search error=%d\n", havesig);
        return havesig;
    }

    hal_type_t type = HAL_TYPE_UNINITIALIZED;
    if(0 != (rv = preflight_net_cmd(signal, &qs, pins, &type))) {
        return rv;
    }

    hal_query_t qp = {};
    qp.name = signal;
    if(0 == (rv = hal_getref_p(&qp))) {
        halcmd_error("Signal name '%s' must not be the same as a pin.  "
                     "Did you omit the signal name?\n", signal);
        return -ENOENT;
    }

    if(-ENOENT == havesig) {
        // Signal does not yet exist
        if(0 == type) {
            halcmd_error("Failed to determine type for signal '%s'\n", signal);
            return -EINVAL;
        }
        if(0 != (rv = hal_signal_new(signal, type))) {
            halcmd_error("Failed to create signal '%s'\n", signal);
            return rv;
        }
    }

    for(int i = 0; pins[i] && *pins[i]; i++) {
        if(0 != (rv = do_linkps_cmd(pins[i], signal))) {
            return rv;
        }
    }
    return 0;
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

    halpr_mutex_acquire();

    while(hal_data->pending_constructor) {
        struct timespec ts = {0, 100 * 1000 * 1000}; // 100ms
        halpr_mutex_release();
        nanosleep(&ts, NULL);
        halpr_mutex_acquire();
    }
    rtapi_strlcpy(hal_data->constructor_prefix, inst_name, HAL_NAME_LEN);
    hal_data->constructor_prefix[HAL_NAME_LEN]=0;
    hal_data->pending_constructor = comp->make;
    halpr_mutex_release();

    if(fputc(' ', f) == EOF) {
        halcmd_error( "cannot write to proc entry: %s\n",
                strerror(errno));
        fclose(f);
        halpr_mutex_acquire();
        hal_data->pending_constructor = 0;
        halpr_mutex_release();
        return -EINVAL;
    }
    if(fclose(f) != 0) {
        halcmd_error(
                "cannot close proc entry: %s\n",
                strerror(errno));
        halpr_mutex_acquire();
        hal_data->pending_constructor = 0;
        halpr_mutex_release();
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
        halpr_mutex_release();
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

    halpr_mutex_release();
    }
    return 0;
}
#endif /* newinst deferred */

static hal_type_t typestr_to_haltype(const char *type, bool anycase)
{
    static const struct {
        const char *name;
        hal_type_t type;
    } htypes[] = {
        { "bool",  HAL_BOOL },
        { "real",  HAL_REAL },
        { "sint",  HAL_SINT },
        { "uint",  HAL_UINT },
        { "port",  HAL_PORT },
        { "bit",   HAL_BOOL },
        { "float", HAL_REAL },
        { "u32",   HAL_U32 },
        { "s32",   HAL_S32 },
        { "u64",   HAL_UINT },
        { "s64",   HAL_SINT },
        { nullptr, HAL_TYPE_UNSPECIFIED }
    };

    int (*cmpfunc)(const char *, const char *) = anycase ? strcasecmp : strcmp;
    for(int i = 0; htypes[i].name; i++) {
        if(!cmpfunc(type, htypes[i].name)) {
            return htypes[i].type;
        }
    }
    return HAL_TYPE_UNINITIALIZED;
}

int do_newsig_cmd(const char *name, const char *type)
{
    hal_type_t htype = typestr_to_haltype(type, true);
    if(HAL_TYPE_UNINITIALIZED == htype) {
        halcmd_error("Unknown signal type '%s'\n", type);
        return -EINVAL;
    }

    int rv = hal_signal_new(name, htype);
    if(rv < 0)
        halcmd_error("newsig failed\n");
    return rv;
}

int do_setp_cmd(const char *name, const char *value)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_set_p(&q, setps_common_cb, (void *)value);
    if(rv) {
        const char *tag;
        switch(q.qtype) {
        case HAL_QTYPE_PARAM: tag = "parameter"; break;
        case HAL_QTYPE_PIN:   tag = "pin"; break;
        default:             tag = "parameter or pin"; break;
        }
        halcmd_error("%s '%s': %s\n", tag, name, hal_strerror(rv));
    }
    return rv;
}

int do_print_cmd(const char *value)
{
    halcmd_error( "HALCMD MSG: %s\n",value );
    return 0;
}

int do_ptype_cmd(const char *name)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_getref_p(&q);
    if(!rv) {
        halcmd_output("%s\n", data_type2(q.pp.type));
    } else {
        const char *tag;
        switch(q.qtype) {
        case HAL_QTYPE_PARAM: tag = "parameter"; break;
        case HAL_QTYPE_PIN:   tag = "pin"; break;
        default:             tag = "parameter or pin"; break;
        }
        halcmd_error("%s '%s': %s\n", tag, name, hal_strerror(rv));
    }
    return rv;
}

static std::string querydata_valuestr(hal_type_t type, const hal_query_value_u *v)
{
    switch(type) {
    case HAL_BOOL:
        return v->b ? "TRUE" : "FALSE";
    case HAL_REAL:
        return fmt::format("{:.7g}", v->r);
    case HAL_S32:
    case HAL_SINT:
    case HAL_PORT:
        return fmt::format("{}", v->s);
    case HAL_U32:
    case HAL_UINT:
        return fmt::format("{}", v->u);
    default:
	/* Shouldn't get here, but just in case... */
	return "unknown_type";
    }
}

int do_getp_cmd(const char *name)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_get_p(&q, NULL, NULL);
    if(!rv) {
        halcmd_output("%s\n", querydata_valuestr(q.pp.type, &q.pp.value).c_str());
    } else {
        const char *tag;
        switch(q.qtype) {
        case HAL_QTYPE_PARAM: tag = "parameter"; break;
        case HAL_QTYPE_PIN:   tag = "pin"; break;
        default:             tag = "parameter or pin"; break;
        }
        halcmd_error("%s '%s': %s\n", tag, name, hal_strerror(rv));
    }
    return rv;
}

int do_sets_cmd(const char *name, const char *value)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_set_s(&q, setps_common_cb, (void *)value);
    if(rv) {
        halcmd_error("signal '%s': %s\n", name, hal_strerror(rv));
    }
    return rv;
}

int do_stype_cmd(const char *name)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_getref_s(&q);
    if(!rv) {
        halcmd_output("%s\n", data_type2(q.sig.type));
    } else {
        halcmd_error("signal '%s': %s\n", name, hal_strerror(rv));
    }
    return rv;
}

int do_gets_cmd(const char *name)
{
    hal_query_t q = {};
    q.name = name;
    int rv = hal_get_s(&q, NULL, NULL);
    if(!rv) {
        halcmd_output("%s\n", querydata_valuestr(q.sig.type, &q.sig.value).c_str());
    } else {
        halcmd_error("signal '%s': %s\n", name, hal_strerror(rv));
    }
    return rv;
}

static int get_type(const char ***patterns) {
    const char *typestr = NULL;
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
    hal_type_t htype = typestr_to_haltype(typestr, false);
    if(htype != HAL_TYPE_UNINITIALIZED)
        return htype;
    if(strcmp(typestr, "signed") == 0) return HAL_S32;
    if(strcmp(typestr, "unsigned") == 0) return HAL_U32;
    return -1;
}

int do_show_cmd(const char *type, const char **patterns)
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

int do_list_cmd(const char *type, const char **patterns)
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

int do_status_cmd(const char *type)
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

int do_loadrt_cmd(const char *mod_name, const char *args[])
{
    int m=0, n=0, retval;
    const char *argv[MAX_TOK+3];
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
    static const char *rtmod_dir = EMC2_RTLIB_DIR;
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
        } else if (r >= (int)sizeof(mod_path)) {
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
    std::string arg_string;
    for(int i = 0; args[i] && *args[i]; i++) {
        if(i > 0)
            arg_string += ' ';
        arg_string += args[i];
    }
    /* allocate HAL shmem for the string */
    char *halcpy = (char *)hal_malloc(arg_string.size() + 1);
    if (NULL == halcpy) {
        halcmd_error("failed to allocate memory for module args\n");
        return -1;
    }
    /* copy string to shmem */
    strcpy(halcpy, arg_string.c_str());
    hal_comp_insmod_args(mod_name, halcpy);

    /* print success message */
    halcmd_info("Realtime module '%s' loaded\n", mod_name);
    return 0;
}

static int delsig_cmd_cb(hal_query_t *q, void *arg)
{
    std::vector<std::string> *sigs = reinterpret_cast<std::vector<std::string> *>(arg);
    sigs->push_back(q->name);
    return 0;
}

int do_delsig_cmd(const char *signame)
{
    if(strcmp(signame, "all")) {
        // Only a single signal to delete
        int rv = hal_signal_delete(signame);
	// FIXME: This should keep quit on success and complain on failure.
	// No news is good news mantra.
        if(!rv) {
            // print success message
            halcmd_info("Signal '%s' deleted'\n", signame);
        }
        return rv;
    }

    // Build a list of signals to delete
    std::vector<std::string> sigs;
    hal_query_t q = {};
    hal_list_s(&q, delsig_cmd_cb, (void *)&sigs);

    if(0 == sigs.size()) {
        halcmd_error("no signals found to be deleted\n");
        return -1;
    }

    for(const auto &sig : sigs) {
        int rv = hal_signal_delete(sig.c_str());
        if(rv) {
            halcmd_error("signal '%s' could not be deleted error=%d\n", sig.c_str(), rv);
            return rv;
        }
	// FIXME: This should keep quit on success and complain on failure.
	// No news is good news mantra.
        halcmd_info("Signal '%s' deleted'\n", sig.c_str());
    }
    return 0;
}

static int do_unloadusr_cmd_cb(hal_query_t *q, void *)
{
    if(HAL_COMP_TYPE_USER == q->comp.type && q->comp.pid != q->callerdata.sival)
        kill(abs(q->comp.pid), SIGTERM);
    return 0;
}

int do_unloadusr_cmd(const char *mod_name)
{
    hal_query_t q = {};
    if(!strcmp(mod_name, "all")) {
        q.callerdata.sival = (int)getpid();
        hal_list_comp(&q, do_unloadusr_cmd_cb, NULL);
    } else {
        int rv = hal_comp_by_name(mod_name, &q);
        if(!rv) {
            if(HAL_COMP_TYPE_USER == q.comp.type && q.comp.pid != (int)getpid())
                kill(abs(q.comp.pid), SIGTERM);
        }
    }
    return 0;
}

static int unloadrt_cmd_cb(hal_query_t *q, void *arg)
{
    std::vector<std::string> *comps = reinterpret_cast<std::vector<std::string> *>(arg);
    if(HAL_COMP_TYPE_REALTIME == q->comp.type) {
        comps->push_back(q->name);
    }
    return 0;
}

int do_unloadrt_cmd(const char *mod_name)
{
    if(strcmp(mod_name, "all")) {
        int rv = hal_comp_by_name(mod_name, NULL);
        if(rv) {
            if(-ENOENT == rv) {
                halcmd_error("component '%s' is not loaded\n", mod_name);
            } else {
                halcmd_error("hal_comp_by_name on '%s' returned error=%d\n", mod_name, rv);
            }
            return rv;
        }
        // One specific module and not 'all'
	if(0 != (rv = unloadrt_comp(mod_name))) {
            halcmd_error("unloadrt failed on '%s'\n", mod_name);
        }
        return rv;
    }

    // Need to unload all modules
    // Build a list of loaded modules
    std::vector<std::string> comps;
    hal_query_t q = {};
    hal_list_comp(&q, unloadrt_cmd_cb, (void *)&comps);

    /* Unload newest first so dependent modules release their references
       before the ones they depend on are removed. This matters for
       kernel modules (RTAI) where rmmod refuses to unload an in-use
       module. Uspace dlclose has no such check, which is why a wrong
       direction here only ever surfaces on RTAI.

       Iterating forward (i = 0 .. n-1) is "newest first" because the
       array was built by walking comp_list_ptr from its head, and that
       list is ordered newest-at-head. If you are tempted to "reverse"
       this loop to match insmod order: you are doing the wrong thing,
       this IS already reverse-insmod order. See PR #3443 for an
       example of that exact mistake. */
    int retval = 0;
    for(const auto &comp : comps) {
        // special case: pseudo prefix means it is not a real comp
        if(0 == comp.find(HAL_PSEUDO_COMP_PREFIX)) {
            continue;
        }
        int rv = unloadrt_comp(comp.c_str());
        if(rv < -1)
            return rv;  // FIXME: This cannot happen because unloadrt_comp() only returns {0, -1}
        if(rv)
            retval = rv;
    }
    return retval;
}

static int unloadrt_comp(const char *mod_name)
{
    int retval;
    const char *argv[4];

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

int do_unload_cmd(const char *mod_name) {
    if(strcmp(mod_name, "all") == 0) {
        int res = do_unloadusr_cmd(mod_name);
        if(res) return res;
        return do_unloadrt_cmd(mod_name);
    } else {
        hal_query_t q = {};
        int rv = hal_comp_by_name(mod_name, &q);
        if(0 != rv) {
            if(-ENOENT == rv)
                halcmd_error("component '%s' is not loaded\n", mod_name);
            else
                halcmd_error("do_unload_cmd: search component '%s' returned error=%d\n", mod_name, rv);
            return -1;
        }
        if(q.comp.type == HAL_COMP_TYPE_REALTIME)
            return do_unloadrt_cmd(mod_name);
        else
            return do_unloadusr_cmd(mod_name);
    }
}

static const char *guess_comp_name(const char *prog_name)
{
    static char name[HAL_NAME_LEN+1];
    const char *last_slash = strrchr(prog_name, '/');
    const char *st = last_slash ? last_slash + 1 : prog_name;
    const char *last_dot = strrchr(st, '.');
    const char *en = last_dot ? last_dot : prog_name + strlen(prog_name);
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

#include <set>

static int get_all_comp_names_cb(hal_query_t *q, void *arg)
{
    std::set<std::string> *comps = reinterpret_cast<std::set<std::string> *>(arg);
    comps->insert(q->name);
    return 0;
}

static std::set<std::string> get_all_comp_names() {
    std::set<std::string> result;
    hal_query_t q = {};
    hal_list_comp(&q, get_all_comp_names_cb, (void *)&result);
    return result;
}

static void warn_newly_loaded_comps(std::set<std::string> &names, const char *newname) {
    auto new_names = get_all_comp_names();
    for(const auto &name : new_names) {
        if(name == newname) continue;
        if(names.find(name) == names.end()) {
            fprintf(stderr, "\nWhile waiting for '%s', component '%s' loaded.\nDid you specify the correct name via 'loadusr -Wn'?", newname, name.c_str());
        }
    }
    std::swap(new_names, names);
}

int do_loadusr_cmd(const char *args[])
{
    int wait_flag, wait_comp_flag, ignore_flag;
    const char *prog_name, *new_comp_name=NULL;
    const char *argv[MAX_TOK+1];
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
	int c = getopt(argc, (char * const *)args, "+wWin:");
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
    if (prog_name == NULL) { return -EINVAL; }
    if(!new_comp_name) {
	new_comp_name = guess_comp_name(prog_name);
    }

    std::set<std::string> comp_names_pre = get_all_comp_names();

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
            hal_query_t q = {};
            int rv = hal_comp_by_name(new_comp_name, &q);
            if(!rv && q.comp.ready) {
                ready = 1;
            }
	    /* pacify the user */
            count++;
            if(count == 200) {
                fprintf(stderr, "Waiting for component '%s' to become ready.",
                        new_comp_name);
                warn_newly_loaded_comps(comp_names_pre, new_comp_name);
                fflush(stderr);
            } else if(count > 200 && count % 10 == 0) {
                fprintf(stderr, ".");
                warn_newly_loaded_comps(comp_names_pre, new_comp_name);
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


int do_waitusr_cmd(const char *comp_name)
{
    if (!comp_name || *comp_name == '\0') {
        halcmd_error("component name missing\n");
        return -EINVAL;
    }

    hal_query_t q = {};
    int rv = hal_comp_by_name(comp_name, &q);
    if (rv) {
        halcmd_info("component '%s' not found or already exited\n", comp_name);
        return 0;
    }
    if (q.comp.type != HAL_COMP_TYPE_USER) {
        halcmd_error("'%s' is not a userspace component\n", comp_name);
        return -EINVAL;
    }
    /* let the user know what is going on */
    halcmd_info("Waiting for component '%s'\n", comp_name);
    while(1) {
        /* sleep for 200mS */
        struct timespec ts = {0, 200 * 1000 * 1000};
        nanosleep(&ts, NULL);
        /* check for component still around */
        if(0 != hal_comp_by_name(comp_name, NULL)) {
            break;
        }
    }
    halcmd_info("Component '%s' finished\n", comp_name);
    return 0;
}

static int print_comp_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!match(patterns, q->name))
        return 0;

    switch(q->comp.type) {
    case HAL_COMP_TYPE_USER:
    case HAL_COMP_TYPE_REALTIME:
        halcmd_output(" %5d  %-4s  %-*s",
            q->comp.comp_id,
            q->comp.type == HAL_COMP_TYPE_REALTIME ? "RT" : "User",
            HAL_NAME_LEN, q->name);
        if(q->comp.type == HAL_COMP_TYPE_USER) {
            halcmd_output(" %5d %s", q->comp.pid, q->comp.ready ?  "ready" : "initializing");
        } else {
            halcmd_output(" %5s %s", "", q->comp.ready ?  "ready" : "initializing");
        }
        break;
    case HAL_COMP_TYPE_OTHER: {
        hal_query_t qc = {};
        // Need to reacquire the component name
        int rv = hal_comp_by_id(q->comp.comp_id, &qc);
        halcmd_output("    INST %s %s", !rv ? qc.name : "(unknown)", q->name);
        } break;
    default:
        halcmd_output(" INVALID COMP TYPE %d for '%s'", (int)q->comp.type, q->name);
        break;
    }
    halcmd_output("\n");
    return 0;
}

static void print_comp_info(const char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Loaded HAL Components:\n");
	halcmd_output("ID      Type  %-*s PID   State\n", HAL_NAME_LEN, "Name");
    }
    hal_query_t q = {};
    hal_list_comp(&q, print_comp_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_pin_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!tmatch(q->callerdata.sival, q->pp.type) || !match(patterns, q->name))
        return 0;

    if (scriptmode == 0) {
        halcmd_output(" %5d  %5s %-3s  %9s  %s",
            q->pp.comp_id,
            data_type(q->pp.type),
            pin_data_dir(q->pp.dir),
            querydata_refstr_20(q->pp.type, q->pp.ref).c_str(),
            q->name);
    } else {
        halcmd_output("%s %s %s %s %s",
            q->pp.comp,
            data_type(q->pp.type),
            pin_data_dir(q->pp.dir),
            querydata_refstr(q->pp.type, q->pp.ref).c_str(),
            q->name);
    } 
    if (q->pp.signal == NULL) {
	halcmd_output("\n");
    } else {
	halcmd_output(" %s %s\n", data_arrow1(q->pp.dir), q->pp.signal);
    }
    return 0;
}

static void print_pin_info(int type, const char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Component Pins:\n");
	halcmd_output("Owner   Type  Dir                 Value  Name\n");
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    q.callerdata.sival = type;
    hal_list_p(&q, print_pin_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_pin_aliases_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(q->pp.alias) {
        /* name is an alias */
        if(!match(patterns, q->name) && !match(patterns, q->pp.alias))
            return 0;
        if(scriptmode == 0) {
            halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, q->name, q->pp.alias);
        } else {
            halcmd_output(" %s  %s\n", q->name, q->pp.alias);
        }
    }
    return 0;
}

static void print_pin_aliases(const char **patterns)
{
    if (scriptmode == 0) {
	halcmd_output("Pin Aliases:\n");
	halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    hal_list_p(&q, print_pin_aliases_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_sig_pin_info_cb(hal_query_t *q, void *)
{
    halcmd_output("%32s %s %s\n", "", data_arrow2(q->pp.dir), q->name);
    return 0;
}

static int print_sig_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!tmatch(q->callerdata.sival, q->sig.type) || !match(patterns, q->name))
        return 0;

    halcmd_output("%s  %s  %s\n",
        data_type(q->sig.type),
        querydata_refstr_20(q->sig.type, q->sig.ref).c_str(),
        q->name);

    // List all pins connected to the signal
    hal_query_t qs = {};
    qs.name = q->name;
    hal_list_p_s(&qs, print_sig_pin_info_cb, NULL);
    return 0;
}

static void print_sig_info(int type, const char **patterns)
{
    if (scriptmode != 0) {
    	print_script_sig_info(type, patterns);
	return;
    }
    halcmd_output("Signals:\n");
    halcmd_output("Type                  Value  Name     (linked to)\n");

    hal_query_t q = {};
    q.callerdata.sival = type;
    hal_list_s(&q, print_sig_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_script_sig_pin_info_cb(hal_query_t *q, void *)
{
    halcmd_output(" %s %s", data_arrow2(q->pp.dir), q->name);
    return 0;
}

static int print_script_sig_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!tmatch(q->callerdata.sival, q->sig.type) || !match(patterns, q->name))
        return 0;

    halcmd_output("%s  %s  %s", data_type(q->sig.type), querydata_refstr(q->sig.type, q->sig.ref).c_str(), q->name);

    // List all pins connected to the signal
    hal_query_t qs = {};
    qs.name = q->name;
    hal_list_p_s(&qs, print_script_sig_pin_info_cb, NULL);
    halcmd_output("\n");
    return 0;
}

static void print_script_sig_info(int type, const char **patterns)
{
    if(!scriptmode)
    	return;
    hal_query_t q = {};
    q.callerdata.sival = type;
    hal_list_s(&q, print_script_sig_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_param_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!tmatch(q->callerdata.sival, q->pp.type) || !match(patterns, q->name))
        return 0;

    if(!scriptmode) {
        halcmd_output(" %5d  %5s %-3s  %9s  %s\n",
            q->pp.comp_id,
            data_type(q->pp.type),
            param_data_dir(q->pp.dir),
            querydata_refstr_20(q->pp.type, q->pp.ref).c_str(),
            q->name);
    } else {
        halcmd_output("%s %s %s %s %s\n",
            q->pp.comp,
            data_type(q->pp.type),
            param_data_dir(q->pp.dir),
            querydata_refstr(q->pp.type, q->pp.ref).c_str(),
            q->name);
    }
    return 0;
}

static void print_param_info(int type, const char **patterns)
{
    if(!scriptmode) {
	halcmd_output("Parameters:\n");
	halcmd_output("Owner   Type  Dir                 Value  Name\n");
    }
    hal_query_t q = {};
    q.callerdata.sival = type;
    q.qtype = HAL_QTYPE_PARAM;
    hal_list_p(&q, print_param_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_param_aliases_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(NULL == q->pp.alias)
        return 0;
    if(!match(patterns, q->name) && !match(patterns, q->pp.alias))
        return 0;

    // name is an alias
    if(!scriptmode) {
        halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, q->name, q->pp.alias);
    } else {
        halcmd_output(" %s  %s\n", q->name, q->pp.alias);
    }
    return 0;
}

static void print_param_aliases(const char **patterns)
{
    if(!scriptmode) {
        halcmd_output("Parameter Aliases:\n");
        halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;  // Only parameters
    hal_list_p(&q, print_param_aliases_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_funct_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!match(patterns, q->name))
        return 0;
    if(!scriptmode) {
        halcmd_output(" %05d  %08llx  %08llx  %-3s  %5d   %s\n",
            q->funct.comp_id,
            (long long)q->funct.funct,
            (long long)q->funct.arg,
            "YES",  // Always uses FP
            q->funct.users,
            q->name);
    } else {
        halcmd_output("%s %08llx %08llx %s %3d %s\n",
            q->funct.comp,
            (long long)q->funct.funct,
            (long long)q->funct.arg,
            "YES",  // Always uses FP
            q->funct.users,
            q->name);
    }
    return 0;
}

static void print_funct_info(const char **patterns)
{
    if(!scriptmode) {
        halcmd_output("Exported Functions:\n");
        halcmd_output("Owner   CodeAddr      Arg           FP   Users   Name\n");
    }
    hal_query_t q = {};
    hal_list_funct(&q, print_funct_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_thread_info_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!match(patterns, q->name))
        return 0;

    // The first callback is on the pure thread data
    if(HAL_QTYPE_THREAD == q->qtype) {
        char tname[HAL_NAME_LEN+1];
        char mname[HAL_NAME_LEN+1];
        size_t ret = snprintf(tname, sizeof(tname), "%s.time", q->name);
        if (ret >= sizeof(tname)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "unexpected: time pin name too long for buffer %s\n", q->name);
            return 0;
        }
        ret = snprintf(mname, sizeof(mname), "%s.tmax", q->name);
        if (ret >= sizeof(mname)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "unexpected: tmax pin name too long for buffer %s\n", q->name);
            return 0;
        }
        hal_query_t qt = {};
        qt.name = tname;
        hal_query_t qm = {};
        qm.name = mname;
        int rvt = hal_getref_p(&qt);
        int rvm = hal_getref_p(&qm);
        if(!rvt && !rvm) {
            // note that the scriptmode format string has no \n
            halcmd_output((!scriptmode ? "%11ld  %-3s  %20s ( %8ld, %8ld )\n" : "%ld %s %s %8ld %ld"),
                q->thread.period,
                "YES",  // Always uses FP
                q->name,
                (long)hal_get_sint(qt.pp.ref.s),
                (long)hal_get_sint(qm.pp.ref.s));
        } else {
            rtapi_print_msg(RTAPI_MSG_ERR, "unexpected: cannot find time/tmax pin for %s thread\n", q->name);
        }
    }

    // Any attached function has a different connection ID
    if(HAL_QTYPE_THREAD_FUNCT == q->qtype) {
        if(!scriptmode) {
            halcmd_output("                 %2d %s\n", q->thread.functidx + 1, q->thread.funct);
        } else {
            // scriptmode only uses one line per thread, which contains: 
            // thread period, FP flag, name, then all functs separated by spaces
            halcmd_output(" %s", q->thread.funct);
        }
    }

    if(scriptmode) {
        halcmd_output("\n");
    }
    return 0;
}

static void print_thread_info(const char **patterns)
{
    if(!scriptmode) {
        halcmd_output("Realtime Threads:\n");
        halcmd_output("     Period  FP     Name               (     Time, Max-Time )\n");
    }
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT; // Callback on both threads and functions attached
    hal_list_thread(&q, print_thread_info_cb, (void *)patterns);
    halcmd_output("\n");
}

static int print_any_names_cb(hal_query_t *q, void *arg)
{
    const char **patterns = (const char **)arg;
    if(!match(patterns, q->name))
        return 0;
    halcmd_output("%s ", q->name);
    return 0;
}

static void print_comp_names(const char **patterns)
{
    hal_query_t q = {};
    hal_list_comp(&q, print_any_names_cb, (void *)patterns);
    halcmd_output("\n");
}

static void print_pin_names(const char **patterns)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN; // Pins only
    hal_list_p(&q, print_any_names_cb, (void *)patterns);
    halcmd_output("\n");
}

static void print_sig_names(const char **patterns)
{
    hal_query_t q = {};
    hal_list_s(&q, print_any_names_cb, (void *)patterns);
    halcmd_output("\n");
}

static void print_param_names(const char **patterns)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM; // Parameters only
    hal_list_p(&q, print_any_names_cb, (void *)patterns);
    halcmd_output("\n");
}

static void print_funct_names(const char **patterns)
{
    hal_query_t q = {};
    hal_list_funct(&q, print_any_names_cb, (void *)patterns);
    halcmd_output("\n");
}

static void print_thread_names(const char **patterns)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD;  // Only thread names
    hal_list_thread(&q, print_any_names_cb, (void *)patterns);
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

static void print_mem_status()
{
    hal_statistics_t s = {};
    int rv = hal_statistics(&s);
    if(0 != rv) {
        halcmd_error("print_mem_status: failed to get HAL statistics, error=%d\n", rv);
        return;
    }

    halcmd_output("HAL memory status\n");
    halcmd_output("  used/total shared memory:   %ld/%ld\n", (long)(s.mem_total - s.mem_free), (long)s.mem_total);
    halcmd_output("  active/recycled components: %d/%d\n", s.ncomps,   s.ncomps_free);
    halcmd_output("  active/recycled pins:       %d/%d\n", s.npins,    s.npins_free);
    halcmd_output("  active/recycled parameters: %d/%d\n", s.nparams,  s.nparams_free);
    halcmd_output("  active/recycled aliases:    %d/%d\n", s.naliases, s.naliases_free);
    halcmd_output("  active/recycled signals:    %d/%d\n", s.nsignals, s.nsignals_free);
    halcmd_output("  active/recycled functions:  %d/%d\n", s.nfuncts,  s.nfuncts_free);
    halcmd_output("  active/recycled threads:    %d/%d\n", s.nthreads, s.nthreads_free);
}

/* Switch function for pin/sig/param type for the print_*_list functions */
static const char *data_type(hal_type_t type)
{
    switch (type) {
    case HAL_BOOL: return "bit  ";
    case HAL_REAL: return "float";
    case HAL_S32:  return "s32  ";
    case HAL_U32:  return "u32  ";
    case HAL_SINT: return "s64  ";
    case HAL_UINT: return "u64  ";
    case HAL_PORT: return "port ";
    default: return "undef"; /* Shouldn't get here, but just in case... */
    }
}

static const char *data_type2(hal_type_t type)
{
    switch (type) {
    case HAL_BOOL: return "bit";
    case HAL_REAL: return "float";
    case HAL_S32:  return "s32";
    case HAL_U32:  return "u32";
    case HAL_SINT: return "s64";
    case HAL_UINT: return "u64";
    case HAL_PORT: return "port";
    default: return "undef"; /* Shouldn't get here, but just in case... */
    }
}

/* Switch function for pin direction for the print_*_list functions  */
static const char *pin_data_dir(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "IN";
    case HAL_OUT: return "OUT";
    case HAL_IO:  return "I/O";
    default: return "???"; /* Shouldn't get here, but just in case... */
    }
}

/* Switch function for param direction for the print_*_list functions  */
static const char *param_data_dir(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_RO: return "RO";
    case HAL_RW: return "RW";
    default: return "??"; /* Shouldn't get here, but just in case... */
    }
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow1(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "<==";
    case HAL_OUT: return "==>";
    case HAL_IO:  return "<=>";
    default: return "???"; /* Shouldn't get here, but just in case... */
    }
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow2(hal_pdir_t dir)
{
    switch (dir) {
    case HAL_IN:  return "==>";
    case HAL_OUT: return "<==";
    case HAL_IO:  return "<=>";
    default: return "???"; /* Shouldn't get here, but just in case... */
    }
}

/* Switch function to return var value for the print_*_info functions
   (scriptmode = 0) as well as save_params() and save_unconnected_input_pin_values().
   The value is printed in a 20 character wide field. */
static std::string querydata_refstr_20(hal_type_t type, hal_refs_u u)
{
    switch(type) {
    case HAL_BOOL:
        return fmt::format("{:>20s}", hal_get_bool(u.b) ? "TRUE" : "FALSE");
    case HAL_REAL:
        return fmt::format("{:20.7g}", hal_get_real(u.r));
    case HAL_S32:
        return fmt::format("{:20d}", (long)hal_get_si32(u.s));
    case HAL_SINT:
    case HAL_PORT:
        return fmt::format("{:20d}", (long long)hal_get_sint(u.s));
    case HAL_U32:
        return fmt::format("          0x{:08X}", (unsigned long)hal_get_ui32(u.u));
    case HAL_UINT:
        return fmt::format("  0x{:016X}", (unsigned long long)hal_get_uint(u.u));
    default:
	/* Shouldn't get here, but just in case... */
	return "        undef       ";
    }
}

/* Switch function to return var value in string form for the
   print_*_info functions (scriptmode = 1) and getp and gets command.
   The value is printed as a packed string (no whitespaces). */
static std::string querydata_refstr(hal_type_t type, hal_refs_u u)
{
    switch(type) {
    case HAL_BOOL:
        return hal_get_bool(u.b) ? "TRUE" : "FALSE";
    case HAL_REAL:
        return fmt::format("{:.7g}", hal_get_real(u.r));
    case HAL_S32:
        return fmt::format("{}", hal_get_si32(u.s));
    case HAL_SINT:
    case HAL_PORT:
        return fmt::format("{}", hal_get_sint(u.s));
    case HAL_U32:
        return fmt::format("{}", hal_get_ui32(u.u));
    case HAL_UINT:
        return fmt::format("{}", hal_get_uint(u.u));
    default:
	/* Shouldn't get here, but just in case... */
	return "unknown_type";
    }
}


int do_save_cmd(const char *type, const char *filename)
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
    if (type == NULL || *type == '\0') {
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

struct _comp_save_data_t {
    const char *name;
    const char *insmod;
};

static int save_comp_cb(hal_query_t *q, void *arg)
{
    std::vector<_comp_save_data_t> *v = (std::vector<_comp_save_data_t> *)arg;
    if(HAL_COMP_TYPE_REALTIME == q->comp.type)
        v->push_back({q->name, q->comp.insmod});
    return 0;
}

static void save_comps(FILE *dst)
{
    hal_query_t q = {};
    std::vector<_comp_save_data_t> compdata;
    hal_list_comp(&q, save_comp_cb, (void *)&compdata);

    fprintf(dst, "# components\n");
    if(0 == compdata.size()) {
        // No components found, bail
        return;
    }

    for(auto i = compdata.rbegin(); i != compdata.rend(); ++i) {
        if(NULL == i->insmod) {
            fprintf(dst, "#loadrt %s  (not loaded by loadrt, no args saved)\n", i->name);
        } else {
            fprintf(dst, "loadrt %s %s\n", i->name, i->insmod);
        }
    }
}

static int save_aliases_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    if(q->pp.alias) {
        const char *s = q->qtype == HAL_QTYPE_PIN ? "pin" : "param";
        fprintf(fp, "alias %s %s %s\n", s, q->pp.alias, q->name);
    }
    return 0;
}

static void save_aliases(FILE *dst)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    fprintf(dst, "# pin aliases\n");
    hal_list_p(&q, save_aliases_cb, (void *)dst);

    q = {};
    q.qtype = HAL_QTYPE_PARAM;
    fprintf(dst, "# param aliases\n");
    hal_list_p(&q, save_aliases_cb, (void *)dst);
}

static int save_signals_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    // FIXME: What about the bidirs?
    if(!(q->callerdata.sival && (q->sig.writers || q->sig.readers)))
        fprintf(fp, "newsig %s %s\n", q->name, data_type(q->sig.type));
    return 0;
}

static void save_signals(FILE *dst, int only_unlinked)
{
    hal_query_t q = {};
    q.callerdata.sival = only_unlinked;
    fprintf(dst, "# signals\n");
    hal_list_s(&q, save_signals_cb, (void *)dst);
}

static int save_links_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    if(q->pp.signal) {
        const char *arrow_str = q->callerdata.sival ? data_arrow1(q->pp.dir) : "";
        fprintf(fp, "linkps %s %s %s\n", q->name, arrow_str, q->pp.signal);
    }
    return 0;
}

static void save_links(FILE *dst, int arrow)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    q.callerdata.sival = arrow;
    fprintf(dst, "# links\n");
    hal_list_p(&q, save_links_cb, (void *)dst);
}

struct save_nets_state_t {
    int state;
    int first;
};

static int save_nets_3_outpins_cb(hal_query_t *q, void *arg)
{
    if(HAL_OUT == q->pp.dir) {
        fprintf((FILE *)arg, " %s", q->name);
        reinterpret_cast<save_nets_state_t *>(q->callerdata.vpval)->state = 1;
    }
    return 0;
}

static int save_nets_3_iopins_cb(hal_query_t *q, void *arg)
{
    if(HAL_IO == q->pp.dir) {
        FILE *fp = (FILE *)arg;
        save_nets_state_t *st = reinterpret_cast<save_nets_state_t *>(q->callerdata.vpval);
        fprintf(fp, " ");
        if(st->state) {
            fprintf(fp, "=> ");
            st->state = 0;
        } else if(!st->first) {
            fprintf(fp, "<=> ");
        }
        fprintf(fp, "%s", q->name);
        st->first = 0;
    }
    return 0;
}

static int save_nets_3_inpins_cb(hal_query_t *q, void *arg)
{
    if(HAL_IN == q->pp.dir) {
        FILE *fp = (FILE *)arg;
        save_nets_state_t *st = reinterpret_cast<save_nets_state_t *>(q->callerdata.vpval);
        fprintf(fp, " ");
        if(st->state) {
            fprintf(fp, "=> ");
            st->state = 0;
        }
        fprintf(fp, "%s", q->name);
    }
    return 0;
}

static int save_nets_3_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    save_nets_state_t st = {.state = 0, .first = 1};
    hal_query_t qp;

    // If there are no pins connected to this signal, do nothing
    if(!q->sig.writers && !q->sig.readers && !q->sig.bidirs)
        return 0;

    fprintf(fp, "net %s", q->name);

    /* Step 1: Output pin, if any */
    qp = {};
    qp.name = q->name;
    qp.callerdata.vpval = &st; // Counts pins
    hal_list_p_s(&qp, save_nets_3_outpins_cb, arg);
    
    /* Step 2: I/O pins, if any */
    qp = {};
    qp.name = q->name;
    qp.callerdata.vpval = &st;
    hal_list_p_s(&qp, save_nets_3_iopins_cb, arg);

    if(!st.first)
        st.state = 1;

    /* Step 3: Input pins, if any */
    qp = {};
    qp.name = q->name;
    qp.callerdata.vpval = &st;
    hal_list_p_s(&qp, save_nets_3_inpins_cb, arg);

    fprintf(fp, "\n");
    return 0;
}

static void save_nets_3(FILE *dst)
{
    hal_query_t q = {};
    hal_list_s(&q, save_nets_3_cb, (void *)dst);
}

static int save_nets_2_pins_cb(hal_query_t *q, void *arg)
{
    fprintf((FILE *)arg, " %s", q->name);
    return 0;
}

static int save_nets_2_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;

    // If there are no pins connected to this signal, do nothing
    if(!q->sig.writers && !q->sig.readers && !q->sig.bidirs)
        return 0;

    fprintf(fp, "net %s", q->name);
    hal_query_t qp = {};
    qp.name = q->name;
    hal_list_p_s(&qp, save_nets_2_pins_cb, arg);
    fprintf(fp, "\n");
    return 0;
}

static void save_nets_2(FILE *dst)
{
    hal_query_t q = {};
    hal_list_s(&q, save_nets_2_cb, (void *)dst);
}

static int save_nets_01_pins_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    const char *arrow_str = q->callerdata.sival ? data_arrow2(q->pp.dir) : "";
    fprintf(fp, "linksp %s %s %s\n", q->pp.signal, arrow_str, q->name);
    return 0;
}

static int save_nets_01_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    fprintf(fp, "newsig %s %s\n", q->name, data_type(q->sig.type));
    hal_query_t qp = {};
    qp.callerdata.sival = q->callerdata.sival; // Propagate arrow type
    qp.name = q->name;
    return hal_list_p_s(&qp, save_nets_01_pins_cb, arg);
}

static void save_nets_01(FILE *dst, int arrow)
{
    hal_query_t q = {};
    q.callerdata.sival = arrow;
    hal_list_s(&q, save_nets_01_cb, (void *)dst);
}

static void save_nets(FILE *dst, int arrow)
{
    fprintf(dst, "# nets\n");
    switch(arrow) {
    case 3: save_nets_3(dst); break;
    case 2: save_nets_2(dst); break;
    case 1:
    case 0: save_nets_01(dst, arrow); break;
    default: break;
    }
}

static int save_params_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    if(HAL_RO != q->pp.dir) {
        // Writable parameter, save its value
        fprintf(fp, "setp %s %s\n", q->name, querydata_refstr_20(q->pp.type, q->pp.ref).c_str());
    }
    return 0;
}

static void save_params(FILE *dst)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PARAM;
    fprintf(dst, "# parameter values\n");
    hal_list_p(&q, save_params_cb, (void *)dst);
}

static int save_threads_cb(hal_query_t *q, void *arg)
{
    if(HAL_QTYPE_THREAD_FUNCT == q->qtype) {
        const char *f = q->thread.is_init ? "initf" : "addf";
        fprintf((FILE *)arg, "%s %s %s\n", f, q->thread.funct, q->name);
    }
    return 0;
}

static void save_threads(FILE *dst)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_THREAD_FUNCT;
    fprintf(dst, "# realtime thread/function links\n");
    hal_list_thread(&q, save_threads_cb, (void *)dst);
}

static int save_unconnected_input_pin_values_cb(hal_query_t *q, void *arg)
{
    FILE *fp = (FILE *)arg;
    if(!q->pp.signal && (HAL_IN == q->pp.dir || HAL_IO == q->pp.dir)) {
        fprintf(fp, "setp %s %s\n", q->name, querydata_refstr(q->pp.type, q->pp.ref).c_str());
    }
    return 0;
}

static void save_unconnected_input_pin_values(FILE *dst)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    fprintf(dst, "# unconnected pin values\n");
    hal_list_p(&q, save_unconnected_input_pin_values_cb, (void *)dst);
}

int do_setexact_cmd()
{
    int retval = hal_enforce_exact_base_period();
    if(0 != retval) {
        halcmd_error(
            "HAL_LIB: Cannot run 'setexact'"
            " after a thread has been created\n");
        retval = -EINVAL;
    } else {
        halcmd_warning(
            "HAL_LIB: HAL will pretend that the exact"
            " base period requested is possible.\n"
            "This mode is not suitable for running real hardware.\n");
    }
    return retval;
}

int do_help_cmd(const char *command)
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
	printf("  'modname', naming it 'instname'.\n");
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
	printf("  is 'bit', 'float', 'port', 'u32', or 's32'.\n");
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
	printf("  Sets a non-port signal 'signame' to 'value' (if signal has no writers).\n");
    printf("  If 'signame' is a port signal (that has not previously been allocated),\n");
    printf("  then 'value' is the new maximum size in bytes of that port.\n");
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
    printf("  If signal 'signame' is a port, returns the buffer size of that port.\n");
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
    } else if (strcmp(command, "debug")==0){
    printf("debug [level]\n");
    printf("   set the messaging level for the realtime API (calls rtapi_set_msg_level)\n");
    printf("   levels are \n");
    printf("   0 = None\n");
	printf("   1 = Errors only (default)\n");
	printf("   2 = Warnings and above\n");
	printf("   3 = Info and above\n");
	printf("   4 = Debug and above\n");
	printf("   5 = All messages\n");
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
    } else if (strcmp(command, "print") == 0) {
        printf("print [message]\n");
        printf("This will print to the terminal the filename, line number and\n");
        printf("an (optional) message. If the message has spaces, put it inside quotes.\n");
        printf("Useful for debugging ie. confirming a HAL file is being run.\n");
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
    printf("  unloadrt            Unload realtime module \n");
    printf("  unloadusr           Unload userspace component\n");
    printf("  lock, unlock        Lock/unlock HAL behaviour\n");
    printf("  linkpp              Link pin to pin (OBSOLETE)\n");
    printf("  linkps              Link pin to signal (OBSOLETE)\n");
    printf("  linksp              Link signal to pin (OBSOLETE)\n");
    printf("  net                 Link a number of pins to a signal\n");
    printf("  unlinkp             Unlink pin\n");
    printf("  newsig              Create a signal (OBSOLETE)\n");
    printf("  delsig              Delete a signal\n");
    printf("  getp, gets          Get the value of a pin, parameter or signal\n");
    printf("  ptype, stype        Get the type of a pin, parameter or signal\n");
    printf("  setp, sets          Set the value of a pin, parameter or signal\n");
    printf("  addf, delf          Add/remove function to/from a thread\n");
    printf("  show                Display info about HAL objects\n");
    printf("  list                Display names of HAL objects\n");
    printf("  source              Execute commands from another .hal file\n");
    printf("  status              Display status information\n");
    printf("  debug               Set the rtapi message level\n");
    printf("  save                Print config as commands\n");
    printf("  start, stop         Start/stop realtime threads\n");
    printf("  alias, unalias      Add or remove pin or parameter name aliases\n");
    printf("  echo, unecho        Echo commands from stdin to stderr\n");
    printf("  print               print filename, line number and optional message to terminal\n");
    printf("  quit, exit          Exit from halcmd\n");
}
