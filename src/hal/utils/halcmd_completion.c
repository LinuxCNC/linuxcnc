/* Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2003 John Kasunich
 *                     <jmkasunich AT users DOT sourceforge DOT net>
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

#include "halcmd_completion.h"
#include "config.h"
#include <rtapi.h>		/* RTAPI realtime OS API */
#include <hal.h>		/* HAL public API decls */
#include <rtapi_mutex.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static int argno;

static const char *command_table[] = {
    "loadrt", "loadusr", "unload", "lock", "unlock",
    "linkps", "linksp", "linkpp", "unlinkp",
    "net", "newsig", "delsig", "getp", "gets", "setp", "sets", "ptype", "stype",
    "addf", "delf", "show", "list", "status", "save", "source",
    "start", "stop", "quit", "exit", "help", "alias", "unalias", 
    NULL,
};

static const char *nonRT_command_table[] = {
    "-h",
    NULL,
};

static const char *alias_table[] = {
    "param", "pin",
    NULL,
};

static const char *show_table[] = {
    "all", "alias", "comp", "pin", "sig", "param", "funct", "thread",
    NULL,
};

static const char *save_table[] = {
    "all", "alias", "comp", "sig", "link", "linka", "net", "neta", "param", "thread",
    NULL,
};

static const char *list_table[] = {
    "comp", "alias", "pin", "sig", "param", "funct", "thread",
    NULL
};

static const char *status_table[] = {
    "alias", "lock", "mem", "all",
    NULL
};

static const char *pintype_table[] = {
    "bit", "float", "u32", "s32", 
    NULL
};

static const char *lock_table[] = { "none", "tune", "all", NULL };
static const char *unlock_table[] = { "tune", "all", NULL };

static const char **string_table = NULL;

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

typedef struct {
    char name[HAL_NAME_LEN+1]; // HAL name strings
    int val;
    union {
        struct { // Pins,params and signals
            hal_pdir_t dir;
            hal_type_t type;
            const char *sig;   // Pin is attached to a signal
            const char *alias; // Pin/param has an alias
            bool drive;        // Signal has writers
        };
        struct { // Components
            hal_comp_type_t ctype;
            bool ready;
        };
    };
} gendata_entry_t;
 
typedef struct {
    size_t na;
    size_t n;
    gendata_entry_t *data; // Array of fixed HAL name strings
} gendata_list_t;

static inline void gendata_clr(gendata_list_t *g)
{
    g->n = 0;
}

static gendata_entry_t *gendata_add(gendata_list_t *g, const char *s, int v)
{
    if(!g->data) {
        g->n = 0;
        g->na = 64;
        g->data = (gendata_entry_t *)calloc(g->na, sizeof(*g->data));
        if(!g->data) {
            g->na = 0;
            return NULL;
        }
    } else if(g->n >= g->na) {
        g->na *= 2;
        g->data = reallocarray(g->data, g->na, sizeof(*g->data));
        if(!g->data) {
            g->na = 0;
            g->n = 0;
            return NULL;
        }
        memset(&g->data[g->n], 0, (g->na - g->n) * sizeof(*g->data));
    }
    strcpy(g->data[g->n].name, s); // This string should always fit
    g->data[g->n].val = v;
    g->n++;
    return &g->data[g->n-1];
}

static int match_pintype_cb(hal_query_t *q, void *arg)
{
    const char *name = (const char *)arg;
    if (q->callerdata.uival == strlen(q->name) && !strncmp(name, q->name, q->callerdata.uival)) {
        match_type = q->pp.type;
        match_direction = q->pp.dir;
        return 1;  // Break the loop
    }
    return 0;
}

static void check_match_type_pin(const char *name)
{
    hal_query_t q = {};
    q.qtype = HAL_QTYPE_PIN;
    q.callerdata.uival = strcspn(name, " \t");
    hal_list_p(&q, match_pintype_cb, (void *)name);
}

static int match_sigtype_cb(hal_query_t *q, void *arg)
{
    const char *name = (const char *)arg;
    if (q->callerdata.uival == strlen(q->name) && !strncmp(name, q->name, q->callerdata.uival)) {
        match_type = q->sig.type;
        match_writers = q->sig.writers > 0;
        return 1;  // Break the loop
    }
    return 0;
}

static void check_match_type_signal(const char *name)
{
    hal_query_t q = {};
    q.callerdata.uival = strcspn(name, " \t");
    hal_list_s(&q, match_sigtype_cb, (void *)name);
}

static int genthread_cb(hal_query_t *q, void *arg)
{
    gendata_add((gendata_list_t *)arg, q->name, 0);
    return 0;
}

static char *thread_generator(const char *text, int state) { 
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {}; // By default, we only get thread names
        hal_list_thread(&q, genthread_cb, &gdl);
        idx = 0;
        len = strlen(text);
    }
    for(; idx < gdl.n; idx++) {
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static int genpin_cb(hal_query_t *q, void *arg)
{
    gendata_list_t *gdl = (gendata_list_t *)arg;
    gendata_entry_t *gde = gendata_add(gdl, q->name, 0);
    if(!gde)
        return 1;  // Allocation error, just drop out
    gde->dir   = q->pp.dir;
    gde->type  = q->pp.type;
    gde->sig   = q->pp.signal;
    gde->alias = q->pp.alias;
    return 0;
}

static void build_pinparam_list(gendata_list_t *gdlp, size_t *idx, int *len, const char *text, hal_qtype_t ctype)
{
    gendata_clr(gdlp);
    hal_query_t q = {};
    q.qtype = ctype;
    hal_list_p(&q, genpin_cb, gdlp);
    *idx = 0;
    *len = strlen(text);
}

static char *parameter_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    static int aliased;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_PARAM);
        aliased = 0;
    }

    const char *name;
    for(; idx < gdl.n; idx++) {
        if(!aliased) {
            if(gdl.data[idx].alias) {
                name = gdl.data[idx].alias;
                aliased = 1;
            } else {
                name = gdl.data[idx].name;
            }
        } else {
            name = gdl.data[idx].name;
            aliased = 0;
        }

        if(!writer_match(gdl.data[idx].dir, match_writers)) {
            idx -= aliased;
            continue;
        }
        if(!direction_match(gdl.data[idx].dir, match_direction)) {
            idx -= aliased;
            continue;
        }
	if(!strncmp(text, name, len)) {
            idx += 1 - aliased;
            return strdup(name);
        }
    }
    return NULL;
}

static int genfunct_cb(hal_query_t *q, void *arg)
{
    gendata_add((gendata_list_t *)arg, q->name, q->callerdata.sival);
    return 0;
}

static char *funct_generator_common(const char *text, int state, int inuse) { 
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {};
        q.callerdata.sival = inuse;
        hal_list_funct(&q, genfunct_cb, &gdl);
        idx = 0;
        len = strlen(text);
    }
    for(; idx < gdl.n; idx++) {
	if(gdl.data[idx].val == inuse && !strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static char *funct_generator(const char *text, int state) {
    return funct_generator_common(text, state, 0);
}

static char *attached_funct_generator(const char *text, int state) {
    return funct_generator_common(text, state, 1);
}

static int gensignal_cb(hal_query_t *q, void *arg)
{
    gendata_list_t *gdl = (gendata_list_t *)arg;
    gendata_entry_t *gde = gendata_add(gdl, q->name, 0);
    if(!gde)
        return 1;  // Allocation error, just drop out
    gde->type = q->sig.type;
    gde->drive = q->sig.writers > 0;
    return 0;
}

static char *signal_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {};
        hal_list_s(&q, gensignal_cb, &gdl);
        idx = 0;
        len = strlen(text);
    }
    for(; idx < gdl.n; idx++) {
        if(match_type != HAL_TYPE_UNSPECIFIED && match_type != gdl.data[idx].type)
            continue; 
        if(!writer_match(match_direction, gdl.data[idx].drive))
            continue;
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static char *getp_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_ANY);
    }
    for(; idx < gdl.n; idx++) {
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static char *setp_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_ANY);
    }
    for(; idx < gdl.n; idx++) {
        if(0 == (gdl.data[idx].dir & (HAL_WO | HAL_OUT)) || NULL != gdl.data[idx].sig)
            continue; // Not writable or has a signal attached
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}


static int gencomp_cb(hal_query_t *q, void *arg)
{
    gendata_list_t *gdl = (gendata_list_t *)arg;
    gendata_entry_t *gde = gendata_add(gdl, q->name, 0);
    if(!gde)
        return 1;  // Allocation error, just drop out
    gde->ctype = q->comp.type;
    gde->ready = q->comp.ready;
    return 0;
}

static char *usrcomp_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {}; // By default, we only get thread names
        hal_list_comp(&q, gencomp_cb, &gdl);
        idx = 0;
        len = strlen(text);
        if(!strncmp(text, "all", len))
            return strdup("all");
    }

    for(; idx < gdl.n; idx++) {
        if(gdl.data[idx].ctype != HAL_COMP_TYPE_USER)
            continue;
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    rl_attempted_completion_over = 1;
    return NULL;
}



static char *comp_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {}; // By default, we only get thread names
        hal_list_comp(&q, gencomp_cb, &gdl);
        idx = 0;
        len = strlen(text);
        if(!strncmp(text, "all", len))
            return strdup("all");
    }

    for(; idx < gdl.n; idx++) {
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    rl_attempted_completion_over = 1;
    return NULL;
}


static char *rtcomp_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        gendata_clr(&gdl);
        hal_query_t q = {}; // By default, we only get thread names
        hal_list_comp(&q, gencomp_cb, &gdl);
        idx = 0;
        len = strlen(text);
        if(!strncmp(text, "all", len))
            return strdup("all");
    }

    for(; idx < gdl.n; idx++) {
        if(gdl.data[idx].ctype != HAL_COMP_TYPE_REALTIME)
            continue;
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    rl_attempted_completion_over = 1;
    return NULL;
}

static char *parameter_alias_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_PARAM);
    }

    for(; idx < gdl.n; idx++) {
        if(!gdl.data[idx].alias)
            continue;
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static char *pin_alias_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_PIN);
    }

    for(; idx < gdl.n; idx++) {
        if(!gdl.data[idx].alias)
            continue;
	if(!strncmp(text, gdl.data[idx].name, len)) {
            return strdup(gdl.data[idx++].name);
        }
    }
    return NULL;
}

static char *pin_generator(const char *text, int state)
{
    static gendata_list_t gdl = {};
    static int len;
    static size_t idx;
    static int aliased;
    if(!state) { // Initial state, fill generator data
        build_pinparam_list(&gdl, &idx, &len, text, HAL_QTYPE_PIN);
        aliased = 0;
    }

    const char *name;
    for(; idx < gdl.n; idx++) {
        if(!aliased) {
            if(gdl.data[idx].alias) {
                name = gdl.data[idx].alias;
                aliased = 1;
            } else {
                name = gdl.data[idx].name;
            }
        } else {
            name = gdl.data[idx].name;
            aliased = 0;
        }

        if(!writer_match(gdl.data[idx].dir, match_writers)) {
            idx -= aliased;
            continue;
        }
        if(!direction_match(gdl.data[idx].dir, match_direction)) {
            idx -= aliased;
            continue;
        }
        if(match_type != HAL_TYPE_UNSPECIFIED && match_type != gdl.data[idx].type) {
            idx -= aliased;
            continue;
        }
	if(!strncmp(text, name, len)) {
            idx += 1 - aliased;
            return strdup(name);
        }
    }
    rl_attempted_completion_over = 1;
    return NULL;
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
    if (d != NULL) {
        closedir(d);
    }
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
        if(!result)
            return NULL;
        result[strlen(result) - strlen(MODULE_EXT)] = 0;
        return result;
    }
    if (d != NULL) {
        closedir(d);
    }
    return NULL;
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
    (void)end;
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

    if(startswith(buffer, "delsig ") && argno == 1) {
        result = func(text, signal_generator);
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
    } else if (startswith(buffer, "alias ")) {
        if (argno == 1) {
            result = completion_matches_table(text, alias_table, func);
        } else if (argno == 2) {
            n = nextword(buffer);
            if (startswith(n, "pin")) {
                result = func(text, pin_generator);
            } else if (startswith(n, "param")) {
                result = func(text, parameter_generator);
            }
        }
    } else if(startswith(buffer, "unalias ")) {
        if (argno == 1) {
            result = completion_matches_table(text, alias_table, func);
        } else if (argno == 2) {
            n = nextword(buffer);
            if (startswith(n, "pin")) {
                result = func(text, pin_alias_generator);
            } else if (startswith(n, "param")) {
                result = func(text, parameter_alias_generator);
            }
        }
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
    } else if(startswith(buffer, "unlock ") && argno == 1) {
        result = completion_matches_table(text, unlock_table, func);
    } else if(startswith(buffer, "addf ") && argno == 1) {
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
    } else if(startswith(buffer, "waitusr ") && argno == 1) {
        result = func(text, usrcomp_generator);
    } else if(startswith(buffer, "unloadrt ") && argno == 1) {
        result = func(text, rtcomp_generator);
    } else if(startswith(buffer, "unload ") && argno == 1) {
        result = func(text, comp_generator);
    } else if(startswith(buffer, "source ") && argno == 1) {
        // leaves rl_attempted_completion_over = 0 to complete from filesystem
        return NULL;
    } else if(startswith(buffer, "loadusr ") && argno < 3) {
        // leaves rl_attempted_completion_over = 0 to complete from filesystem
        return func(text, loadusr_generator);
    } else if(startswith(buffer, "loadrt ") && argno == 1) {
        result = func(text, loadrt_generator);
    }

    rl_attempted_completion_over = 1;
    return result;
}

static char **rlcompleter(const char *text, int start, int end) {
    return halcmd_completer(text, start, end, rl_completion_matches, rl_line_buffer);
}

void halcmd_init_readline() {
    rl_readline_name = "halcmd";
    rl_attempted_completion_function = rlcompleter;
}

