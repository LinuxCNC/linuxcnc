/*
  License GPL Version 2
*/

/* switchkins_setup.c: the part of an in-tree switchkins module that names
*  its switchkinsSetup().  Kept apart from switchkins.c so that the core
*  names nothing a module defines and can be loaded as switchkins_core.
*
*  switchkinsRunSetup() is what rtapi_app_main() calls before
*  switchkinsInit().  kinsDescribe() is the description a copy of the
*  module loaded outside RT answers with.
*/
#include <rtapi.h>

#include <switchkins.h>

int switchkinsRunSetup(kparms* kp, const char* sparm)
{
    return switchkinsRunSetupWith(switchkinsSetup, kp, sparm);
} // switchkinsRunSetup()

int kinsDescribe(const char *coordinates, const char *sparm,
                 kins_module_info *info)
{
    return switchkinsDescribeWith(switchkinsSetup, coordinates, sparm, info);
} // kinsDescribe()

EXPORT_SYMBOL(switchkinsRunSetup);
EXPORT_SYMBOL(kinsDescribe);
