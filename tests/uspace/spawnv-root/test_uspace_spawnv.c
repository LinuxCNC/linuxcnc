#include <rtapi.h>
#include <rtapi_app.h>
#include <errno.h>
#include <stdio.h>

int comp;

int rtapi_app_main() {
    uid_t ruid_before, euid_before, suid_before;
    uid_t ruid_after, euid_after, suid_after;
    pid_t pid;
    char * const argv[] = { "true", NULL };

    comp = hal_init("test_uspace_spawnv");
    if(comp < 0) { return comp; }

    int e = 0, r = getresuid(&ruid_before, &euid_before, &suid_before);
    if(r < 0) { e=errno; perror("getresuid before"); goto out; }
    printf("getresuid() before: %d %d %d\n", 
        (int) ruid_before, (int) euid_before, (int) suid_before);
    rtapi_spawn_as_root(&pid, "true", NULL, NULL, argv, NULL);

    r = getresuid(&ruid_after, &euid_after, &suid_after);
    if(r < 0) { e=errno; perror("getresuid after"); goto out; }
    printf("getresuid() after: %d %d %d\n", 
        (int) ruid_after, (int) euid_after, (int) suid_after);

    if(ruid_before != ruid_after) {
        printf("ruid lost by rtapi_spawn_as_root %d -> %d\n", (int)ruid_before, (int)ruid_after);
        e = EINVAL;
    }
    if(euid_before != euid_after) {
        printf("euid lost by rtapi_spawn_as_root %d -> %d\n", (int)euid_before, (int)euid_after);
        e = EINVAL;
    }
    if(suid_before != suid_after) {
        printf("suid lost by rtapi_spawn_as_root %d -> %d\n", (int)suid_before, (int)suid_after);
        e = EINVAL;
    }
out:
    if(e == 0) {
    printf("ready\n");
        hal_ready(comp);
    } else {
    printf("fail\n");
        hal_exit(comp);
    }
    printf("exiting\n");
    return e;
}

void rtapi_app_exit() {
    hal_exit(comp);
}


