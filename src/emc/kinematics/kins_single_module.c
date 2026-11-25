/********************************************************************
* Description: kins_single_module.c
*   kinsDescribe() for an in-tree module with one kinematics type: the
*   description the module names kins_module, for a copy of the module
*   loaded outside RT.  Kept apart from kins_single.c so that file can be
*   loaded as kins_single_core and serve a module built out of tree.
*
* License: GPL Version 2
********************************************************************/

#include <rtapi.h>

#include <kins_rt.h>

int kinsDescribe(const char *coordinates, const char *sparm,
                 kins_module_info *info)
{
    (void)coordinates;
    (void)sparm;
    return kinsSingleDescribe(&kins_module, info);
}

EXPORT_SYMBOL(kinsDescribe);
