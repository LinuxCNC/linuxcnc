component userspace_count_names;
option userspace;
option userinit;

pin out signed out = 42;
license "GPL";
;;
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

void exit_handler(int signo) {
    ;
}

void userinit(int argc, char **argv) {
    int i;
    printf("count=%d\n", count);
    for (i = 0; i < 16; i ++) {
        if (names[i] != NULL) {
            printf("names[%d]=%s\n", i, names[i]);
        }
    }
    printf("argc=%d\n", argc);
    for (i = 0; i <= argc; i ++) {
        printf("argv[%d]=%s\n", i, argv[i]);
    }
    signal(SIGTERM, exit_handler);
    fflush(NULL);
}

void user_mainloop(void) {
    pause();
}

