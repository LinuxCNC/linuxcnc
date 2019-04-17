#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <python_plugin.hh>
#include <rs274ngc_interp.hh>
#include <stdio.h>
#include <saicanon.hh>

int _task = 1; // Dummy this out, not used in unit test
InterpBase *pinterp;

// KLUDGE fix missing symbol the ugly way
struct _inittab builtin_modules[] = {
    { nullptr, nullptr }
};

int main (int argc, char * argv[]) {
    // KLUDGE just to satisfy saicanon dependencies, not used in tests
    _outfile = fopen("test_interp_canon.log", "w");
    PythonPlugin::instantiate(builtin_modules);
    return Catch::Session().run( argc, argv );
}
