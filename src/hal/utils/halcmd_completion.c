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
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "../hal_priv.h"	/* private HAL decls */

static int argno;

static char *command_table[] = {
    "loadrt", "loadusr", "unload", "lock", "unlock",
    "linkps", "linksp", "linkpp", "unlinkp",
    "net", "newsig", "delsig", "getp", "gets", "setp", "sets",
    "addf", "delf", "show", "list", "status", "save",
    "start", "stop", "quit", "exit", "help", 
    NULL,
};

static char *show_table[] = {
    "all", "comp", "pin", "sig", "param", "funct", "thread",
    NULL,
};

static char *save_table[] = {
    "all", "comp", "sig", "link", "linka", "net", "neta", "param", "thread",
    NULL,
};

static char *list_table[] = {
    "comp", "pin", "sig", "param", "funct", "thread",
    NULL
};

static char *status_table[] = {
    "lock", "mem", "all",
    NULL
};

static char *pintype_table[] = {
    "bit", "float", "u32", "s32", 
    NULL
};

static char *lock_table[] = { "none", "tune", "all", NULL };
static char *unlock_table[] = { "tune", "all", NULL };

static char **string_table = NULL;

static char *table_generator(const char *text, int state) {
    static int len;
    static int list_index = 0;
    char *name;

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

static char **completion_matches_table(const char *text, char **table) {
    string_table = table;
    return rl_completion_matches(text, table_generator);
}

static hal_type_t match_type = -1;
static int match_writers = -1;
static hal_pin_dir_t match_direction = -1;

static int direction_match(hal_pin_dir_t dir1, hal_pin_dir_t dir2) {
    if(dir1 == -1 || dir2 == -1) return 1;
    return (dir1 | dir2) == HAL_IO;
}

static int writer_match(hal_pin_dir_t dir, int writers) {
    if(writers == -1 || dir == -1) return 1;
    if(dir & HAL_IN || writers == 0) return 1;
    return 0;
}

static void check_match_type_pin(const char *name) {
    int next = hal_data->pin_list_ptr;
    int sz = strcspn(name, " \t");

    while(next) {
        hal_pin_t *pin = SHMPTR(next);
        next = pin->next_ptr;
	if ( sz == strlen(pin->name) && strncmp(name, pin->name, sz) == 0 ) {
            match_type = pin->type;
            match_direction = pin->dir;
            return;
        }
    }
}

static void check_match_type_signal(const char *name) {
    int next = hal_data->sig_list_ptr;
    int sz = strcspn(name, " \t");

    while(next) {
        hal_sig_t *sig = SHMPTR(next);
        next = sig->next_ptr;
	if ( sz == strlen(sig->name) && strncmp(name, sig->name, sz) == 0 ) {
            match_type = sig->type;
            match_writers = sig->writers;
            return;
        }
    }
}

static char *thread_generator(const char *text, int state) { 
    static int len;
    static int next;
    if(!state) {
        next = hal_data->thread_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_thread_t *thread = SHMPTR(next);
        next = thread->next_ptr;
	if ( strncmp(text, thread->name, len) == 0 )
            return strdup(thread->name);
    }
    return NULL;
}

static char *parameter_generator(const char *text, int state) { 
    static int len;
    static int next;
    if(!state) {
        next = hal_data->param_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_param_t *param = SHMPTR(next);
        next = param->next_ptr;
	if ( strncmp(text, param->name, len) == 0 )
            return strdup(param->name);
    }
    return NULL;
}

static char *funct_generator(const char *text, int state) { 
    static int len;
    static int next;
    if(!state) {
        next = hal_data->funct_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_funct_t *funct = SHMPTR(next);
        next = funct->next_ptr;
	if ( strncmp(text, funct->name, len) == 0 )
            return strdup(funct->name);
    }
    return NULL;
}

static char *signal_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->sig_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_sig_t *sig = SHMPTR(next);
        next = sig->next_ptr;
        if ( match_type != -1 && match_type != sig->type ) continue; 
        if ( !writer_match( match_direction, sig->writers ) ) continue;
	if ( strncmp(text, sig->name, len) == 0 )
            return strdup(sig->name);
    }
    return NULL;
}

static char *getp_generator(const char *text, int state) {
    static int len;
    static int next;
    static int what;
    if(!state) {
        what = 0;
        next = hal_data->param_list_ptr;
        len = strlen(text);
    }

    if(what == 0) {
        while(next) {
            hal_param_t *param = SHMPTR(next);
            next = param->next_ptr;
            if ( strncmp(text, param->name, len) == 0 )
                return strdup(param->name);
        }
        what = 1;
        next = hal_data->pin_list_ptr;
    }
    while(next) {
        hal_pin_t *pin = SHMPTR(next);
        next = pin->next_ptr;
        if ( strncmp(text, pin->name, len) == 0 )
            return strdup(pin->name);
    }

    return NULL;
}

static char *setp_generator(const char *text, int state) {
    static int len;
    static int next;
    static int what;
    if(!state) {
        what = 0;
        next = hal_data->param_list_ptr;
        len = strlen(text);
    }

    if(what == 0) {
        while(next) {
            hal_param_t *param = SHMPTR(next);
            next = param->next_ptr;
            if ( param->dir != HAL_RO && strncmp(text, param->name, len) == 0 )
                return strdup(param->name);
        }
        what = 1;
        next = hal_data->pin_list_ptr;
    }
    while(next) {
        hal_pin_t *pin = SHMPTR(next);
        next = pin->next_ptr;
        if ( pin->dir != HAL_OUT && pin->signal == 0 && 
                 strncmp(text, pin->name, len) == 0 )
            return strdup(pin->name);
    }

    return NULL;
}


static char *param_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->param_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_param_t *param = SHMPTR(next);
        next = param->next_ptr;
	if ( strncmp(text, param->name, len) == 0 )
            return strdup(param->name);
    }
    return NULL;
}

static char *usrcomp_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->comp_list_ptr;
        len = strlen(text);
        if(strncmp(text, "all", len) == 0)
            return strdup("all");
    }

    while(next) {
        hal_comp_t *comp = SHMPTR(next);
        next = comp->next_ptr;
        if(comp->type) continue;
	if(strncmp(text, comp->name, len) == 0)
            return strdup(comp->name);
    }
    rl_attempted_completion_over = 1;
    return NULL;
}



static char *comp_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->comp_list_ptr;
        len = strlen(text);
        if(strncmp(text, "all", len) == 0)
            return strdup("all");
    }

    while(next) {
        hal_comp_t *comp = SHMPTR(next);
        next = comp->next_ptr;
	if ( strncmp(text, comp->name, len) == 0 )
            return strdup(comp->name);
    }
    rl_attempted_completion_over = 1;
    return NULL;
}


static char *rtcomp_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->comp_list_ptr;
        len = strlen(text);
        if(strncmp(text, "all", len) == 0)
            return strdup("all");
    }

    while(next) {
        hal_comp_t *comp = SHMPTR(next);
        next = comp->next_ptr;
        if(!comp->type) continue;
	if ( strncmp(text, comp->name, len) == 0 )
            return strdup(comp->name);
    }
    rl_attempted_completion_over = 1;
    return NULL;
}


static char *pin_generator(const char *text, int state) {
    static int len;
    static int next;
    if(!state) {
        next = hal_data->pin_list_ptr;
        len = strlen(text);
    }

    while(next) {
        hal_pin_t *pin = SHMPTR(next);
        next = pin->next_ptr;
        if ( !writer_match( pin->dir, match_writers ) ) continue;
        if ( !direction_match( pin->dir, match_direction ) ) continue;
        if ( match_type != -1 && match_type != pin->type ) continue; 
	if ( strncmp(text, pin->name, len) == 0 )
            return strdup(pin->name);
    }
    rl_attempted_completion_over = 1;
    return NULL;
}

#include <dirent.h>

static int startswith(const char *string, const char *stem) {
    return strncmp(string, stem, strlen(stem)) == 0;
}

char *loadusr_table[] = {"-W", "-Wn", "-w", "-iw", NULL};

static char *loadusr_generator(const char *text, int state) {
    static int len;
    static DIR *d;
    struct dirent *ent;
    static int doing_table;

    if(!state) {
	if(argno == 1) doing_table = 1;
        string_table = loadusr_table;
        len = strlen(text);
        d = opendir(EMC2_BIN_DIR);
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
    closedir(d);
    return NULL;
}


static char *loadrt_generator(const char *text, int state) {
    static int len;
    static DIR *d;
    struct dirent *ent;

    if(!state) {
        len = strlen(text);
        d = opendir(EMC2_RTLIB_DIR);
    }

    while(d && (ent = readdir(d))) {
        char *result;
        if(!strstr(ent->d_name, MODULE_EXT)) continue;
        if(startswith(ent->d_name, "rtapi.")) continue;
        if(strncmp(text, ent->d_name, len) != 0) continue;
        result = strdup(ent->d_name);
        result[strlen(result) - strlen(MODULE_EXT)] = 0;
        return result;
    }
    closedir(d);
    return NULL;
}

static inline int isskip(int ch) {
    return isspace(ch) || ch == '=' || ch == '<' || ch == '>';
}

char *nextword(char *s) {
    s = strchr(s, ' ');
    if(!s) return NULL;
    return s+1;
}

char **completer(const char *text, int start, int end) {
    int i;
    char **result = NULL;

    if(start == 0)
        return completion_matches_table(text, command_table);

    for(i=0, argno=0; i<start; i++) {
        if(isskip(rl_line_buffer[i])) {
            argno++;
            while(i<start && isskip(rl_line_buffer[i])) i++;
        }
    }

    match_type = -1;
    match_writers = -1;
    match_direction = -1;

    rtapi_mutex_get(&(hal_data->mutex));

    if(startswith(rl_line_buffer, "delsig ") && argno == 1) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "linkps ") && argno == 1) {
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "linkps ") && argno == 2) {
        check_match_type_pin(rl_line_buffer + 7);
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "net ") && argno == 1) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "net ") && argno == 2) {
        check_match_type_signal(nextword(rl_line_buffer));
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "net ") && argno > 2) {
        check_match_type_signal(nextword(rl_line_buffer));
        if(match_type == -1) {
            check_match_type_pin(nextword(nextword(rl_line_buffer)));
            if(match_direction == HAL_IN) match_direction = -1;
        }
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "linksp ") && argno == 1) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "linksp ") && argno == 2) {
        check_match_type_signal(rl_line_buffer + 7);
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "linkpp ") && argno == 1) {
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "linkpp ") && argno == 2) {
        check_match_type_pin(rl_line_buffer + 7);
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "unlinkp ") && argno == 1) {
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "setp ") && argno == 1) {
        result = rl_completion_matches(text, setp_generator);
    } else if(startswith(rl_line_buffer, "sets ") && argno == 1) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "getp ") && argno == 1) {
        result = rl_completion_matches(text, getp_generator);
    } else if(startswith(rl_line_buffer, "gets ") && argno == 1) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "show ") && argno == 1) {
        result = completion_matches_table(text, show_table);
    } else if(startswith(rl_line_buffer, "list pin") && argno == 2) {
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "list sig") && argno == 2) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "list param") && argno == 2) {
        result = rl_completion_matches(text, parameter_generator);
    } else if(startswith(rl_line_buffer, "list funct") && argno == 2) {
        result = rl_completion_matches(text, funct_generator);
    } else if(startswith(rl_line_buffer, "list thread") && argno == 2) {
        result = rl_completion_matches(text, thread_generator);
    } else if(startswith(rl_line_buffer, "show pin") && argno == 2) {
        result = rl_completion_matches(text, pin_generator);
    } else if(startswith(rl_line_buffer, "show sig") && argno == 2) {
        result = rl_completion_matches(text, signal_generator);
    } else if(startswith(rl_line_buffer, "show param") && argno == 2) {
        result = rl_completion_matches(text, parameter_generator);
    } else if(startswith(rl_line_buffer, "show funct") && argno == 2) {
        result = rl_completion_matches(text, funct_generator);
    } else if(startswith(rl_line_buffer, "show thread") && argno == 2) {
        result = rl_completion_matches(text, thread_generator);
    } else if(startswith(rl_line_buffer, "save ") && argno == 1) {
        result = completion_matches_table(text, save_table);
    } else if(startswith(rl_line_buffer, "list ") && argno == 1) {
        result = completion_matches_table(text, list_table);
    } else if(startswith(rl_line_buffer, "status ") && argno == 1) {
        result = completion_matches_table(text, status_table);
    } else if(startswith(rl_line_buffer, "newsig ") && argno == 2) {
        result = completion_matches_table(text, pintype_table);
    } else if(startswith(rl_line_buffer, "lock ") && argno == 1) {
        result = completion_matches_table(text, lock_table);
    } else if(startswith(rl_line_buffer, "unlock ") && argno == 1) {
        result = completion_matches_table(text, unlock_table);
    } else if(startswith(rl_line_buffer, "addf ") && argno == 1) {
        result = rl_completion_matches(text, funct_generator);
    } else if(startswith(rl_line_buffer, "addf ") && argno == 2) {
        result = rl_completion_matches(text, thread_generator);
    } else if(startswith(rl_line_buffer, "delf ") && argno == 1) {
        result = rl_completion_matches(text, funct_generator);
    } else if(startswith(rl_line_buffer, "delf ") && argno == 2) {
        result = rl_completion_matches(text, thread_generator);
    } else if(startswith(rl_line_buffer, "help ") && argno == 1) {
        result = completion_matches_table(text, command_table);
    } else if(startswith(rl_line_buffer, "unloadusr ") && argno == 1) {
        result = rl_completion_matches(text, usrcomp_generator);
    } else if(startswith(rl_line_buffer, "waitusr ") && argno == 1) {
        result = rl_completion_matches(text, usrcomp_generator);
    } else if(startswith(rl_line_buffer, "unloadrt ") && argno == 1) {
        result = rl_completion_matches(text, rtcomp_generator);
    } else if(startswith(rl_line_buffer, "unload ") && argno == 1) {
        result = rl_completion_matches(text, comp_generator);
    } else if(startswith(rl_line_buffer, "loadusr ") && argno < 3) {
        rtapi_mutex_give(&(hal_data->mutex));
        // leaves rl_attempted_completion_over = 0 to complete from filesystem
        return rl_completion_matches(text, loadusr_generator);
    } else if(startswith(rl_line_buffer, "loadrt ") && argno == 1) {
        result = rl_completion_matches(text, loadrt_generator);
    }

    rtapi_mutex_give(&(hal_data->mutex));

    rl_attempted_completion_over = 1;
    return result;
}

void halcmd_init_readline() {
    rl_readline_name = "halcmd";
    rl_attempted_completion_function = completer;
}

