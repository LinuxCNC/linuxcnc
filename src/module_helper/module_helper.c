//    Copyright 2006-2010, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

/*
This module_helper program will be installed setuid and allows
the user to add and remove a whitelist of modules necessary to
run EMC.  Without a scheme like this, we have to rely on sudo
to start AND EXIT our program, and that may require the user to
enter a password.  Prompting for a password to exit a program
is bad.  If the user cancels at that phase of the run, it's also
bad since we leave realtime modules inserted and he'll probably
end up being forced to reboot.

In summary, I don't like this any more than you do, but I can't
think of a better way.
*/

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "config.h"

/* module name, between last / and ., must be one of these */
/* if one module name is a prefix of the other (e.g., "rtai" is a prefix to
 * "rtai_math") then put the shorter name last.
 */
char *module_whitelist[] = {
    "rtai_math", "rtai_sem", "rtai_fifos", "rtai_up", "rtai_lxrt",
    "rtai_hal", "rtai_sched", "rtai_smi", "rtai", "rt_mem_mgr", "adeos",

    NULL
};

/* module path must start with this. */

char *path_whitelist[] = {
    "/lib/modules", RTDIR, NULL,

    NULL
};

/* module extension must be one of these */

char *ext_whitelist[] = {
    ".ko", NULL
};

void error(int argc, char **argv) {
    int i;
    int res;
    char *prog = argv[0];

    /* drop root privs permanently */
    res = setuid(getuid());
    if(res != 0)
    {
        perror("setuid");
        exit(1);
    }

    fprintf(stderr, "%s: Invalid usage with args:", argv[0]);
    for(i=1; i<argc; i++) {
        fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr, "\n\nUsage: %s insert /path/to/module.ext [param1=value1 ...]\n", prog);
    fprintf(stderr, "\nwhere module is one of:\n");
    for(i=0; module_whitelist[i]; i++) {
        fprintf(stderr, "\t%s\n", module_whitelist[i]);
    } 
    fprintf(stderr, "\nthe path starts with one of:\n");
    for(i=0; path_whitelist[i]; i++) {
        fprintf(stderr, "\t%s\n", path_whitelist[i]);
    }
    fprintf(stderr, "\nand the extension is one of:\n");
    for(i=0; ext_whitelist[i]; i++) {
        fprintf(stderr, "\t%s\n", ext_whitelist[i]);
    }
    fprintf(stderr, "\nor the module is in the directory %s\n",
            EMC2_RTLIB_DIR);
    fprintf(stderr, "\nOR\n\n%s remove module\n\nwhere module is one of"
                    " the modules listed above.\n\n", prog);
    exit(1);
}

/* Check that the next characters in target match one of the items
 * in table.  Failure is indicated by NULL, success is indicated by returning
 * a pointer to just after the end of the string matched from the whitelist.
 * If target was NULL, then the result is NULL.
 *
 * This means that it is easy to check for what comes after the whitelisted
 * characters (e.g., module extension) even when the table entries have
 * different lengths.
 */
char *check_whitelist(char *target, char *table[]) {
    int i;
    if(!target) return 0;
    for(i=0; table[i]; i++) {
        int sz = strlen(table[i]);
        if(!strncmp(target, table[i], sz)) return target + sz;
    }
    return 0;
}

/* Check that the given module is in the whitelist.  It's in the whitelist
 * if it's inside the EMC2_RTLIB_DIR, or if it's in a directory
 * whose prefix is path_whitelist and the module name is in the module_whitelist
 *
 * Paths without slashes and paths with ".." (even /a..b/) are never allowed.
 *
 * Paths must always start with a slash, but this is not explicitly tested.
 * It's tested by strncmp() or check_whitelist() implicitly.
 */
void check_whitelist_module_path(char *mod, int argc, char **argv) {
    char *ext, *end;
    char *last_slash = strrchr(mod, '/');

    if(!last_slash || strstr(mod, "..")) error(argc, argv);

    if(strncmp(mod, EMC2_RTLIB_DIR, strlen(EMC2_RTLIB_DIR)) == 0)
        return;

    ext = check_whitelist(last_slash + 1, module_whitelist);
    if(!ext) error(argc, argv);

    end = check_whitelist(ext, ext_whitelist);
    if(!end || *end) error(argc, argv);

    if(!check_whitelist(mod, path_whitelist)) error(argc, argv);
}

#include <sys/types.h>
#include <dirent.h>

/* Check that a module (without path or extension) is in the whitelist.
 * It's in the whitelist if it's in the module_whitelist, or if it exists
 * in the EMC2_RTLIB_DIR with the extension MODULE_EXT.
 */
void check_whitelist_module(char *mod, int argc, char **argv) {
    char *end;
    DIR *d = opendir(EMC2_RTLIB_DIR);

    if(d) {
        char buf[NAME_MAX + 1];
        snprintf(buf, NAME_MAX, "%s%s", mod, MODULE_EXT);

        while(1) {
            struct dirent *ent = readdir(d);
            if(!ent) break;
            if(strcmp(ent->d_name, buf) == 0) {
                closedir(d);
                return;
            }
        }
        closedir(d);
    }

    end = check_whitelist(mod, module_whitelist);

    /* Check that end is not NULL (whitelist succeeded) and that it is the end of the string */
    if(!end || *end) error(argc, argv);


}

int main(int argc, char **argv) {
    char *mod;
    int i;
    int inserting = 0;
    int res;
    struct utsname u;
    char buf[4096];
    char **exec_argv;

    if(geteuid() != 0) {
        fprintf(stderr, "module_helper is not setuid root\n");
        return 1;
    }
    /* drop root privs temporarily */
    res = seteuid(getuid());
    if(res != 0)
    {
        perror("seteuid");
        return 1;
    }

    res = uname(&u);
    if(res != 0)
    {
        perror("uname");
        return 1;
    }

    res = snprintf(buf, sizeof(buf), "/usr/realtime-%s/modules", u.release);
    if(res < 0 || res >= sizeof(buf))
    {
        perror("snprintf");
        return 1;
    }
    path_whitelist[2] = buf;

    if(argc < 3) error(argc, argv);
    if(strcmp(argv[1], "insert") && strcmp(argv[1], "remove")) error(argc, argv);
    exec_argv = malloc(argc * sizeof(char *));

    if(!strcmp(argv[1], "insert"))
        inserting = 1;

    mod = argv[2];

    if(inserting) {
        check_whitelist_module_path(mod, argc, argv);

        exec_argv[0] = "/sbin/insmod";
        exec_argv[1] = mod;

        for(i=3; i<argc; i++) {
            exec_argv[i-1] = argv[i];
        }
        exec_argv[argc-1] = NULL;
    } else {
        check_whitelist_module(mod, argc, argv);
        exec_argv[0] = "/sbin/rmmod";
        exec_argv[1] = mod;
        exec_argv[2] = NULL;
    }

    /* reinstate root privs */
    res = seteuid(0);
    if(res != 0)
    {
        perror("seteuid");
        return 1;
    }

    execve(exec_argv[0], exec_argv, NULL);

    perror("execv failed");
    return 1;
}

/* vim:sts=4:et:sw=4
 */
