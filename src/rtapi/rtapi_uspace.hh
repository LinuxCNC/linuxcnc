#ifndef RTAPI_USPACE_HH
#define RTAPI_USPACE_HH
#include <sys/fsuid.h>
#include <unistd.h>

struct WithRoot
{
    WithRoot() { if(!level) setfsuid(geteuid()); level++; }
    ~WithRoot() { --level; if(!level) setfsuid(getuid()); }
    static int level;
};

#define WITH_ROOT WithRoot root
#endif
