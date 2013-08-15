#include "python_plugin.hh"


extern "C" void initemctask();
extern "C" void initinterpreter();
extern "C" void initemccanon();
struct _inittab builtin_modules[] = {
    { (char *) "interpreter", initinterpreter },
    { (char *) "emccanon", initemccanon },
    { (char *) "emctask", initemctask },
    // any others...
    { NULL, NULL }
};
