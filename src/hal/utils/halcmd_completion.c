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

#include "halcmd_completion.h"
#include "config.h"
#include "linux/limits.h"
#include "stdlib.h"
#include "rtapi.h"      /* RTAPI realtime OS API */
#include "hal.h"        /* HAL public API decls */
#include "hal_priv.h"   /* private HAL decls */
#include "hal_ring.h"           /* ringbuffer declarations */
#include "hal_group.h"          /* halgroups declarations */

static int argno;

static const char *command_table[] = {
    "loadrt", "loadusr", "unload", "lock", "unlock",
    "linkps", "linksp", "linkpp", "unlinkp",
    "net", "newsig", "delsig", "getp", "gets", "setp", "sets", "sete", "ptype", "stype",
    "addf", "call", "delf", "show", "list", "status", "save", "source","sweep",
    "start", "stop", "quit", "exit", "help",
    "newg"," delg", "newm", "delm",
    "newring","delring","ringdump","ringwrite","ringflush",
    "newcomp","newpin","ready","waitbound", "waitunbound", "waitexists",
    "log","shutdown","ping","newthread","delthread",
    "sleep","vtable","autoload","newinst", "delinst",
    NULL,
};

static const char *nonRT_command_table[] = {
    "-h",
    NULL,
};

static const char *show_table[] = {
    "all", "comp", "pin", "sig", "param", "funct", "thread", "group", "member",
    "ring", "eps","vtable","inst",
    NULL,
};

static const char *save_table[] = {
    "all",  "comp", "sig", "link", "linka", "net", "neta", "param", "thread",
    "group", "member", "ring",
    NULL,
};

static const char *list_table[] = {
    "comp", "pin", "sig", "param", "funct", "thread", "group", "member",
    "ring","inst",
    NULL
};

static const char *status_table[] = {
    "lock", "mem", "all",
    NULL
};

static const char *pintype_table[] = {
    "bit", "float", "u32", "s32", 
    NULL
};

static const char *log_table[] = {
    "rt", "user",
    NULL
};

static const char *lock_table[] = { "none", "tune", "all", NULL };
static const char *unlock_table[] = { "tune", "all", NULL };

static const char **string_table = NULL;

foreach_args_t cargs;

static inline void zero_foreach_args(foreach_args_t *a) 
    {
    memset((void *)a, 0, sizeof(foreach_args_t));
    }

static char *table_generator(const char *text, int state) {
    static int len;
    static int list_index = 0;
    const char *name;

    if(state == 0) {
        list_index = 0;
        len = strlen(text);
    }

    while((name = string_table[list_index]) != NULL) {
        list_index ++;
        if(strncmp (name, text, len) == 0) return strdup(name);
    }
    return NULL;
}

static char **completion_matches_table(const char *text, const char **table, hal_completer_func func) {
    string_table = table;
    return func(text, table_generator);
}

static hal_type_t match_type = -1;
static int match_writers = -1;
static hal_pin_dir_t match_direction = HAL_DIR_UNSPECIFIED;

static int direction_match(hal_pin_dir_t dir1, hal_pin_dir_t dir2) {
    if(dir1 == HAL_DIR_UNSPECIFIED || dir2 == -1) return 1;
    return (dir1 | dir2) == HAL_IO;
}

static int writer_match(hal_pin_dir_t dir, int writers) {
    if(writers == -1 || dir == -1) return 1;
    if(dir & HAL_IN || writers == 0) return 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////

static int yield_pinmatch(hal_object_ptr o, foreach_args_t *args)
{
int sz = strcspn(args->user_ptr1, " \t");

    if ( sz == strlen(hh_get_name(&o.pin->hdr)) && 
         strncmp(args->user_ptr1, hh_get_name(&o.pin->hdr), sz) == 0 ) 
        {
        match_type = o.pin->type;
        match_direction = o.pin->dir;
        return 1;
        }

     return 0;
}

static void check_match_type_pin(const char *name) 
{
    zero_foreach_args(&cargs);
    cargs.type = HAL_PIN;
    cargs.user_ptr1 = (char *) name;
   
    halg_yield(0, &cargs, yield_pinmatch);
}

////////////////////////////////////////////////////////////////////////

static int yield_signalmatch(hal_object_ptr o, foreach_args_t *args)
{
int sz = strcspn(args->user_ptr1, " \t");

    if ( sz == strlen(hh_get_name(&o.sig->hdr)) && 
         strncmp(args->user_ptr1, hh_get_name(&o.sig->hdr), sz) == 0 ) 
        {
        match_type = o.sig->type;
        match_writers = o.sig->writers;
        return 1;
        }

     return 0;
}

static void check_match_type_signal(const char *name) {

    zero_foreach_args(&cargs);
    cargs.type = HAL_SIGNAL;
    cargs.user_ptr1 = (char *) name;
   
    halg_yield(0, &cargs, yield_signalmatch);

}
////////////////////////////////////////////////////////////////////////

static int yield_threadstrname(hal_object_ptr o, foreach_args_t *args)
{
size_t len = strlen(args->user_ptr1);

    if(strncmp(args->user_ptr1, hh_get_name(&o.thread->hdr), len) == 0 )
        {
        args->result = strdup(hh_get_name(&o.thread->hdr));
        return 1;
        }
     return 0;
}


static char *thread_generator(const char *text, int state) {
    if (!state) 
        {
        zero_foreach_args(&cargs);
        cargs.type = HAL_THREAD;
        cargs.user_ptr1 = (char *) text;
        }
    halg_yield(0, &cargs, yield_threadstrname);
    return cargs.result;

}

////////////////////////////////////////////////////////////////////////

static int yield_paramstrname(hal_object_ptr o, foreach_args_t *args)
{
size_t len = strlen(args->user_ptr1);
   
    if( (strncmp(args->user_ptr1, hh_get_name(&o.param->hdr), len) == 0 )) 
        {
        args->result = strdup(hh_get_name(&o.param->hdr));
        return 1;
        }

    return 0;
}


static char *parameter_generator(const char *text, int state) 
{ 
// we don't have aliases any more, so just read param names

    if (!state) 
        {
        zero_foreach_args(&cargs);
        cargs.type = HAL_PARAM;
        cargs.user_ptr1 = (char *) text;
        }
    halg_yield(0, &cargs, yield_paramstrname);
    return cargs.result;

}

////////////////////////////////////////////////////////////////////////

static int yield_functstrname(hal_object_ptr o, foreach_args_t *args)
{
size_t len = strlen(args->user_ptr1);
   
    if( (strncmp(args->user_ptr1, hh_get_name(&o.funct->hdr), len) == 0 ) &&
        ( args->user_arg1 == o.funct->users) )
        {
        args->result = strdup(hh_get_name(&o.funct->hdr));
        return 1;
        }

    return 0;
}

static char *funct_generator_common(const char *text, int state, int inuse) 
{ 
    if (!state) 
        {
        zero_foreach_args(&cargs);
        cargs.type = HAL_FUNCT;
        cargs.user_ptr1 = (char *) text;
        cargs.user_arg1 = inuse;
        }
    halg_yield(0, &cargs, yield_functstrname);

    return cargs.result;

}

static char *funct_generator(const char *text, int state) {
    return funct_generator_common(text, state, 0);
}

static char *attached_funct_generator(const char *text, int state) {
    return funct_generator_common(text, state, 1);
}

////////////////////////////////////////////////////////////////////////

static int yield_strname(hal_object_ptr o, foreach_args_t *args)
{
    size_t len = strlen(args->user_ptr1);
    if ((len == 0) ||
    !strncmp(hh_get_name(o.hdr),    // prefix match
         args->user_ptr1,
         len)) {
    args->result = strdup(hh_get_name(o.hdr));
    return 1;  // terminate visit on first match
    }
    return 0;
}
static char *object_generator(const char *text, int state, int type)
{
    if (!state) {
    zero_foreach_args(&cargs);
    cargs.type = type;
    cargs.user_ptr1 = (char *) text;
    }
    halg_yield(0, &cargs, yield_strname);
    return cargs.result;
}

static char *signal_generator(const char *text, int state)
{
    return object_generator(text, state, HAL_SIGNAL);
}


static int yield_getpp(hal_object_ptr o, foreach_args_t *args)
{
    size_t len = strlen(args->user_ptr1);
    if (len && strncmp(hh_get_name(o.hdr),    // prefix match
               args->user_ptr1,
               len))
    return 0;

    switch (hh_get_object_type(o.hdr)) {
    case HAL_PARAM:
    case HAL_PIN:
    args->result = strdup(hh_get_name(o.hdr));
    return 1;
    break;
    default: ;
    }
    return 0;
}

static char *getp_generator(const char *text, int state)
{
    if (!state) {
    zero_foreach_args(&cargs);
    cargs.user_ptr1 = (char *) text;
    }
    halg_yield(0, &cargs, yield_getpp);
    return cargs.result;
}

static int yield_setpp(hal_object_ptr o, foreach_args_t *args)
{
    size_t len = strlen(args->user_ptr1);
    if (len && strncmp(hh_get_name(o.hdr),    // prefix match
		       args->user_ptr1,
		       len))
	return 0;

    switch (hh_get_object_type(o.hdr)) {
    case HAL_PARAM:
	if (o.param->dir != HAL_RO) {
	    args->result = strdup(hh_get_name(o.hdr));
	    return 1;
	}
	break;
    case HAL_PIN:
	if (o.pin->dir != HAL_OUT && !pin_is_linked(o.pin)) {
	    args->result = strdup(hh_get_name(o.hdr));
	    return 1;
	}
	break;
    default: ;
    }
    return 0;
}

static char *setp_generator(const char *text, int state)
{
    if (!state) {
	zero_foreach_args(&cargs);
	cargs.user_ptr1 = (char *) text;
    }
    halg_yield(0, &cargs, yield_setpp);
    return cargs.result;
}



static char *comp_generator(const char *text, int state) {
    return object_generator(text, state, HAL_COMPONENT);
}

////////////////////////////////////////////////////////////////////////

#define T_RT 0
#define T_USER 1
#define T_INST 2
    
static int yield_typecompstrname(hal_object_ptr o, foreach_args_t *args)
{
size_t len;

    switch(args->user_arg1)
        {
        case T_USER:
            if(o.comp->type == TYPE_USER)
                {
                len = strlen(args->user_ptr1);
                if(strncmp(args->user_ptr1, hh_get_name(&o.comp->hdr), len) == 0 )
                    {
                    args->result = strdup(hh_get_name(&o.comp->hdr));
                    return 1;
                    }
                }
            break;
        case T_INST:
            if (is_instantiable(o.comp))
                {
                len = strlen(args->user_ptr1);
                if(strncmp(args->user_ptr1, hh_get_name(&o.comp->hdr), len) == 0 )
                    {
                    args->result = strdup(hh_get_name(&o.comp->hdr));
                    return 1;
                    }
                }
            break;
        case T_RT:
            if(o.comp->type == TYPE_RT)
                {
                len = strlen(args->user_ptr1);
                if(strncmp(args->user_ptr1, hh_get_name(&o.comp->hdr), len) == 0 )
                    {
                    args->result = strdup(hh_get_name(&o.comp->hdr));
                    return 1;
                    }
                }
            break;
        default:  // out of range error
            return -1;
        }
        
    return 0;
}    
        


static char *typecomp_generator(const char *text, int state, int type) {
    
    if (!state) 
        {
        zero_foreach_args(&cargs);
        cargs.type = HAL_COMPONENT;
        cargs.user_ptr1 = (char *) text;
        cargs.user_arg1 =  type;
        }
    halg_yield(0, &cargs, yield_typecompstrname);
    return cargs.result;

}

static char *usrcomp_generator(const char *text, int state) 
{
    return typecomp_generator(text, state, T_USER);
}

static char *icomp_generator(const char *text, int state) 
{
    return typecomp_generator(text, state, T_INST);
}

static char *rtcomp_generator(const char *text, int state) 
{
    return typecomp_generator(text, state, T_RT);
}
////////////////////////////////////////////////////////////////////////

static int yield_pinstrname(hal_object_ptr o, foreach_args_t *args)
{
    if ( !writer_match( o.pin->dir, match_writers ) )
        return 0;
    if ( !direction_match( o.pin->dir, match_direction ) )
        return 0;
    if ( match_type != HAL_TYPE_UNSPECIFIED && match_type != o.pin->type )  
        return 0;

    size_t len = strlen(args->user_ptr1);
    if ((len == 0) ||
    !strncmp(hh_get_name(o.hdr),    // prefix match
         args->user_ptr1, len)) {
    args->result = strdup(hh_get_name(o.hdr));
    return 1;
    }
    return 0;
}

static char *pin_generator(const char *text, int state)
{
    if (!state) {
    zero_foreach_args(&cargs);
    cargs.type = HAL_PIN;
    cargs.user_ptr1 = (char *) text;
    }
    halg_yield(0, &cargs, yield_pinstrname);
    return cargs.result;
}

#include <dirent.h>

static int startswith(const char *string, const char *stem) {
    return strncmp(string, stem, strlen(stem)) == 0;
}

const char *loadusr_table[] = {"-W", "-Wn", "-w", "-iw", NULL};

static char *loadusr_generator(const char *text, int state) {
    static int len;
    static DIR *d;
    struct dirent *ent;
    static int doing_table;
    char bindir[PATH_MAX];

    if (get_rtapi_config(bindir,"BIN_DIR",PATH_MAX) != 0)
    return NULL;

    if(!state) {
    if(argno == 1) doing_table = 1;
        string_table = loadusr_table;
        len = strlen(text);
        d = opendir(bindir);
    }

    if(doing_table) {
        char *result = table_generator(text, state);
        if(result) return result;
        doing_table = 0;
    }

    while(d && (ent = readdir(d))) {
        char *result;
        if(!startswith(ent->d_name, "hal")) continue;
        if(startswith(ent->d_name, "halcmd")) continue;
        if(strncmp(text, ent->d_name, len) != 0) continue;
        result = strdup(ent->d_name);
        return result;
    }
    if (d != NULL) {
        closedir(d);
    }
    return NULL;
}

extern flavor_ptr current_flavor; // reference to current flavor descriptor

static char *loadrt_generator(const char *text, int state) {
    static int len;
    static DIR *d;
    struct dirent *ent;
    char rtlibdir[PATH_MAX];

    if (get_rtapi_config(rtlibdir,"RTLIB_DIR",PATH_MAX) != 0)
    return NULL;

    strcat(rtlibdir,"/");
    strcat(rtlibdir, current_flavor->name);
    strcat(rtlibdir,"/");

    if(!state) {
        len = strlen(text);
        d = opendir(rtlibdir);
    }

    while(d && (ent = readdir(d))) {
        char *result;
        if(!strstr(ent->d_name, default_flavor()->mod_ext)) continue;
        if(startswith(ent->d_name, "rtapi.")) continue;
        if(startswith(ent->d_name, "hal_lib.")) continue;
        if(strncmp(text, ent->d_name, len) != 0) continue;
        result = strdup(ent->d_name);
        result[strlen(result) - \
           strlen(default_flavor()->mod_ext)] = 0;
        return result;
    }
    if (d != NULL) {
        closedir(d);
    }
    return NULL;
}

static char *group_generator(const char *text, int state) {
     return object_generator(text, state, HAL_GROUP);
}

static char *ring_generator(const char *text, int state) {
    return object_generator(text, state, HAL_RING);
}

static char *inst_generator(const char *text, int state){
    return object_generator(text, state, HAL_INST);
}

static inline int isskip(int ch) {
    return isspace(ch) || ch == '=' || ch == '<' || ch == '>';
}

static char *nextword(char *s) {
    s = strchr(s, ' ');
    if(!s) return NULL;
    return s+strspn(s, " \t=<>");
}

char **halcmd_completer(const char *text, int start, int end, hal_completer_func func, char *buffer) {
    int i;
    char **result = NULL, *n;

    while (isskip(*text)) text++;   // skip initial whitespace
    while (isskip(*buffer)) {
        buffer++;
        start--;
    }
    if (start<0) start=0;
    if(start == 0) {
        if (comp_id >= 0) {
            return completion_matches_table(text, command_table, func);
        } else {
            return completion_matches_table(text, nonRT_command_table, func);
        }
    }

    for(i=0, argno=0; i<start; i++) {
        if(isskip(buffer[i])) {
            argno++;
            while(i<start && isskip(buffer[i])) i++;
        }
    }

    match_type = -1;
    match_writers = -1;
    match_direction = -1;

    rtapi_mutex_get(&(hal_data->mutex));

    if(startswith(buffer, "delsig ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "delg ") && argno == 1) {
        result = func(text, group_generator);
    } else if(startswith(buffer, "delinst ") && argno == 1) {
        result = func(text, inst_generator);
    } else if(startswith(buffer, "delm ") && argno == 1) {
        result = func(text, group_generator);
    } else if(startswith(buffer, "delt ") && argno == 1) {
        result = func(text, thread_generator);

    } else if(startswith(buffer, "newr ") && argno > 2) {
    result = func(text, ring_generator);
    } else if(startswith(buffer, "delr ") && argno == 1) {
        result = func(text, ring_generator);
    } else if(startswith(buffer, "ringdump ") && argno == 1) {
    result = func(text, ring_generator);
   } else if(startswith(buffer, "ringflush ") && argno == 1) {
    result = func(text, ring_generator);
   } else if(startswith(buffer, "ringwrite ") && argno == 1) {
    result = func(text, ring_generator);
    } else if(startswith(buffer, "linkps ") && argno == 1) {
        result = func(text, pin_generator);
    } else if(startswith(buffer, "linkps ") && argno == 2) {
        check_match_type_pin(buffer + 7);
        result = func(text, signal_generator);
    } else if(startswith(buffer, "net ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "net ") && argno == 2) {
        check_match_type_signal(nextword(buffer));
        result = func(text, pin_generator);


    } else if(startswith(buffer, "net ") && argno > 2) {
        check_match_type_signal(nextword(buffer));
        if(match_type == HAL_TYPE_UNSPECIFIED) {
            check_match_type_pin(nextword(nextword(buffer)));
            if(match_direction == HAL_IN) match_direction = -1;
        }
        result = func(text, pin_generator);
    } else if(startswith(buffer, "linksp ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "linksp ") && argno == 2) {
        check_match_type_signal(buffer + 7);
        result = func(text, pin_generator);
    } else if(startswith(buffer, "linkpp ") && argno == 1) {
        result = func(text, pin_generator);
    } else if(startswith(buffer, "linkpp ") && argno == 2) {
        check_match_type_pin(buffer + 7);
        result = func(text, pin_generator);
    } else if(startswith(buffer, "unlinkp ") && argno == 1) {
        result = func(text, pin_generator);
    } else if(startswith(buffer, "setp ") && argno == 1) {
        result = func(text, setp_generator);
    } else if(startswith(buffer, "sets ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "ptype ") && argno == 1) {
        result = func(text, getp_generator);
    } else if(startswith(buffer, "getp ") && argno == 1) {
        result = func(text, getp_generator);
    } else if(startswith(buffer, "stype ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "gets ") && argno == 1) {
        result = func(text, signal_generator);
    } else if(startswith(buffer, "list ")) {
        if (argno == 1) {
            result = completion_matches_table(text, list_table, func);
        } else if (argno==2) {
            n = nextword(buffer);
            if (startswith(n, "pin")) {
                result = func(text, pin_generator);
            } else if (startswith(n, "sig")) {
                result = func(text, signal_generator);
            } else if (startswith(n, "param")) {
                result = func(text, parameter_generator);
            } else if (startswith(n, "funct")) {
                result = func(text, funct_generator);
            } else if (startswith(n, "thread")) {
                result = func(text, thread_generator);
        } else if (startswith(n, "group")) {
                result = func(text, group_generator);
        } else if (startswith(n, "inst")) {
                result = func(text, inst_generator);
        } else if (startswith(n, "ring")) {
                result = func(text, ring_generator);
            }
        }
    } else if(startswith(buffer, "show ")) {
        if (argno == 1) {
            result = completion_matches_table(text, show_table, func);
        } else if (argno==2) {
            n = nextword(buffer);
            if (startswith(n, "pin")) {
                result = func(text, pin_generator);
            } else if (startswith(n, "sig")) {
                result = func(text, signal_generator);
            } else if (startswith(n, "param")) {
                result = func(text, parameter_generator);
            } else if (startswith(n, "funct")) {
                result = func(text, funct_generator);
            } else if (startswith(n, "thread")) {
                result = func(text, thread_generator);
            } else if (startswith(n, "group")) {
                result = func(text, group_generator);
        } else if (startswith(n, "ring")) {
                result = func(text, ring_generator);
        } else if (startswith(n, "inst")) {
                result = func(text, inst_generator);
        } else if (startswith(n, "comp")) {
                result = func(text, comp_generator);
            }
        }
    } else if(startswith(buffer, "save ") && argno == 1) {
        result = completion_matches_table(text, save_table, func);
    } else if(startswith(buffer, "status ") && argno == 1) {
        result = completion_matches_table(text, status_table, func);
    } else if(startswith(buffer, "newsig ") && argno == 2) {
        result = completion_matches_table(text, pintype_table, func);
    } else if(startswith(buffer, "lock ") && argno == 1) {
        result = completion_matches_table(text, lock_table, func);
    } else if(startswith(buffer, "log ") && argno == 1) {
        result = completion_matches_table(text, log_table, func);
    } else if(startswith(buffer, "unlock ") && argno == 1) {
        result = completion_matches_table(text, unlock_table, func);
    } else if(startswith(buffer, "addf ") && argno == 1) {
        result = func(text, funct_generator);
    } else if(startswith(buffer, "call ") && argno == 1) {
        result = func(text, funct_generator);
    } else if(startswith(buffer, "addf ") && argno == 2) {
        result = func(text, thread_generator);
    } else if(startswith(buffer, "delf ") && argno == 1) {
        result = func(text, attached_funct_generator);
    } else if(startswith(buffer, "delf ") && argno == 2) {
        result = func(text, thread_generator);
    } else if(startswith(buffer, "help ") && argno == 1) {
        result = completion_matches_table(text, command_table, func);
    } else if(startswith(buffer, "unloadusr ") && argno == 1) {
        result = func(text, usrcomp_generator);
    } else if(startswith(buffer, "newinst ") && argno == 1) {
        result = func(text, icomp_generator);
    } else if(startswith(buffer, "waitusr ") && argno == 1) {
        result = func(text, usrcomp_generator);
    } else if(startswith(buffer, "unloadrt ") && argno == 1) {
        result = func(text, rtcomp_generator);
    } else if(startswith(buffer, "unload ") && argno == 1) {
        result = func(text, comp_generator);
    } else if(startswith(buffer, "source ") && argno == 1) {
        rtapi_mutex_give(&(hal_data->mutex));
        // leaves rl_attempted_completion_over = 0 to complete from filesystem
        return 0;
    } else if(startswith(buffer, "loadusr ") && argno < 3) {
        rtapi_mutex_give(&(hal_data->mutex));
        // leaves rl_attempted_completion_over = 0 to complete from filesystem
        return func(text, loadusr_generator);
    } else if(startswith(buffer, "loadrt ") && argno == 1) {
        result = func(text, loadrt_generator);
    } else if(startswith(buffer, "delg ") && argno == 1) {
        result = func(text, group_generator);
    } else if(startswith(buffer, "delm ") && argno == 1) {
        result = func(text, group_generator);
    } else if(startswith(buffer, "newm ") && argno == 1) {
        result = func(text, group_generator);
    } else if(startswith(buffer, "newm ") && argno == 2) {
        result = func(text, signal_generator); // FIXME should be signal_and_pin_generator
    }

    rtapi_mutex_give(&(hal_data->mutex));

    rl_attempted_completion_over = 1;
    return result;
}

static char **rlcompleter(const char *text, int start, int end) {
    return halcmd_completer(text, start, end, rl_completion_matches, rl_line_buffer);
}

void halcmd_save_history()
{
    char path[PATH_MAX];
    snprintf(path,sizeof(path),"%s/%s", getenv("HOME"), HALCMD_HISTORY);
    if ((write_history (path)))
        perror(path);
    history_truncate_file (path, MAX_HISTORY);
}

void halcmd_init_readline()
{
    char path[PATH_MAX];
    snprintf(path,sizeof(path),"%s/%s", getenv("HOME"), HALCMD_HISTORY);
    if ((read_history (path))){
    // perror(path);
    } else {
    // printf("%d history lines read from %s\n", history_length, path);
    }
    rl_readline_name = "halcmd";
    rl_attempted_completion_function = rlcompleter;
}

