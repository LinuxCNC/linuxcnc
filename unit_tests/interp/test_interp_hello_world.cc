#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rs274ngc_interp.hh>
#include <python_plugin.hh>

extern Interp *pinterp;

void foo_init()
{
    printf("foo init\n");
}
void bar_init()
{
    printf("bar init\n");
}
struct _inittab builtin_modules[] = {
    { (char *) "foo", foo_init },
    { (char *) "bar", bar_init },
    // any others...
    { NULL, NULL }
};

TEST_CASE("Create a dummy interp and execute a simple command")
{
    Interp dummy;
    pinterp = &dummy;
    PythonPlugin::instantiate(builtin_modules);
    dummy.init();
    const char *set_mm = "G21\n";
    dummy.execute(set_mm);
}
