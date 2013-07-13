/*
This module_helper program, installed setuid, allows the user to add
and remove modules necessary to run LinuxCNC.  It looks for modules in
the RTLIB_DIR directory, and if not found there, looks in RTDIR (if
defined; needed by RTAI only).

Without a scheme like this, we have to rely on sudo to start AND EXIT
our program, and that may require the user to enter a password.
Prompting for a password to exit a program is bad.  If the user
cancels at that phase of the run, it's also bad since we leave
realtime modules inserted and he'll probably end up being forced to
reboot.

In summary, I don't like this any more than you do, but I can't
think of a better way.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>                // PATH_MAX

#include "rtapi.h"

void error(int argc, char **argv) {
    int i;
    char *prog = argv[0];

    /* drop root privs permanently */
    setuid(getuid());

    fprintf(stderr, "%s: Invalid usage with args:", argv[0]);
    for(i=1; i<argc; i++) {
        fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr,
	    "\nUsage: %s [ insert | remove ] module [param1=value1 ...]\n"
	    "\tExample:  %s insert rtapi\n",
	    prog, prog);
    exit(1);
}

#include <sys/types.h>
#include <dirent.h>

int main(int argc, char **argv) {
    char *mod;
    int i;
    int inserting = 0;
    char **exec_argv;
    char modpath[PATH_MAX];

    if(geteuid() != 0) {
        fprintf(stderr, "module_helper is not setuid root\n");
        return 1;
    }
    /* drop root privs temporarily */
    seteuid(getuid());

    if(argc < 3) error(argc, argv);
    if(strcmp(argv[1], "insert") && strcmp(argv[1], "remove")) error(argc, argv);
    exec_argv = malloc(argc * sizeof(char *));

    if(!strcmp(argv[1], "insert"))
        inserting = 1;

    mod = argv[2];

    if(inserting) {
	// Construct the module path
	if (module_path(modpath, mod) < 0) {
	    // Failed to find module path
	    fprintf(stderr, "%s: Unable to locate module '%s'\n", argv[0],
		    mod);
	    exit(1);
	}

        exec_argv[0] = "/sbin/insmod";
        exec_argv[1] = modpath;

        for(i=3; i<argc; i++) {
            exec_argv[i-1] = argv[i];
        }
        exec_argv[argc-1] = NULL;
    } else {
        exec_argv[0] = "/sbin/rmmod";
        exec_argv[1] = mod;
        exec_argv[2] = NULL;
    }

    /* reinstate root privs */
    seteuid(0);
    execve(exec_argv[0], exec_argv, NULL);

    perror("execv failed");
    return 1;
}

/* vim:sts=4:et:sw=4
 */
