/************************************************************************
 * Demo of SMP_SAFE instantiated component in C++ class
 * 
 * ArcEye 2015  <arceyeATmgwareDOTcoDOTuk>
************************************************************************/ 

#define _CORRECT_ISO_CPP_STRING_H_PROTO

// Std C / C++
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>   /* Standard types */

#include <rtapi.h>
#include "rtapi_app.h"
#include "rtapi_string.h"
#include <hal.h>
#include <hal_priv.h>
#include <hal_accessor.h>
#include "hal_internal.h"

// Can't rely on bool working otherwise
#undef TRUE
#define TRUE (1)
#undef FALSE
#define FALSE (0)
#undef true
#define true (1)
#undef false
#define false (0)

using namespace std;

#ifdef MODULE_INFO
MODULE_INFO(machinekit, "component:constant:Use a parameter to set the value of a pin");
MODULE_INFO(machinekit, "pin:out:float:None:out::None");
MODULE_INFO(machinekit, "pin:value:float:None:io::None");
MODULE_INFO(machinekit, "funct:_:1:");
MODULE_INFO(machinekit, "license:GPL");
MODULE_LICENSE("GPL");
#endif // MODULE_INFO
RTAPI_TAG(HAL,HC_INSTANTIABLE);

struct inst_data
    {
    float_pin_ptr out;
    float_pin_ptr value;
    int local_argc;
    const char **local_argv;
    };

int comp_id;

class ConstRt
{
public:
    ConstRt();
    ~ConstRt();
    
    // these members must be static to work as and in callbacks from 
    // the rtapi C functions and that is the root of all the problems
    static int export_halobjs(struct inst_data *ip, int owner_id, const char *name, const int argc, const char **argv);
    static int instantiate(const char *name, const int argc, const char**argv);
    static int constant_(void *arg, const hal_funct_args_t *fa);
    
    struct inst_data *inst;
};


ConstRt::ConstRt()
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, NULL, "const-rtv2");
    if (comp_id < 0)
        {
        hal_print_msg(RTAPI_MSG_DBG,"Component creation ABORTED");
        exit(-1);
        }

    hal_ready(comp_id);
}

ConstRt::~ConstRt()
{
    hal_exit(comp_id);
}

int ConstRt::instantiate(const char *name, const int argc, const char**argv)
{
struct inst_data *ip;
int r;

    int inst_id = hal_inst_create(name, comp_id, sizeof(struct inst_data), (void **)&ip);

    if (inst_id < 0)
        return -1;

    hal_print_msg(RTAPI_MSG_DBG,"%s inst=%s argc=%d",__FUNCTION__, name, argc);

    r = export_halobjs(ip, inst_id, name, argc, argv);
    if(r == 0)
        hal_print_msg(RTAPI_MSG_DBG,"%s - instance %s creation SUCCESSFUL",__FUNCTION__, name);
    else
        hal_print_msg(RTAPI_MSG_DBG,"%s - instance %s creation ABORTED",__FUNCTION__, name);
    return r;
}

int ConstRt::export_halobjs(struct inst_data *ip, int owner_id, const char *name, const int argc, const char **argv)
{
    char buf[HAL_NAME_LEN + 1];
    int r = 0;

    ip->out = halx_pin_float_newf(HAL_OUT, owner_id, "%s.out", name);
    if (float_pin_null(ip->out))
            return _halerrno;


    ip->value = halx_pin_float_newf(HAL_IO, owner_id,"%s.value", name);
    if (float_pin_null(ip->value))
            return _halerrno;

    
    ip->local_argv = argv;
    ip->local_argc = argc;
    
    // exporting an extended thread function:
    hal_export_xfunct_args_t __xf;

        __xf.type = FS_XTHREADFUNC;
        __xf.funct.x = constant_;
        __xf.arg = ip;
        __xf.uses_fp = 1;
        __xf.reentrant = 0;
        __xf.owner_id = owner_id;

    rtapi_snprintf(buf, sizeof(buf),"%s.funct", name);
    r = hal_export_xfunctf(&__xf, buf, name);
    if(r != 0)
        return r;
    return 0;
}

// ~~~~~~~~~~~~~~~~~  The component function  ~~~~~~~~~~~~~~~~~~~~~~~~~~

int ConstRt::constant_(void *arg, const hal_funct_args_t *fa)
{
long period __attribute__((unused)) = fa_period(fa);
struct inst_data *ip  = (inst_data *) arg;

    set_float_pin(ip->out, get_float_pin(ip->value));

    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cannot get away from these two - the entry and exit point
// needed for all rt components
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


ConstRt *cr;

extern "C" int rtapi_app_main(void)
{
    cr = new ConstRt();

    return 0;

}


extern "C" void rtapi_app_exit(void)
{
    // this will call the destructor of ConstRt and thence
    // hal_exit(comp_id);
    delete(cr);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
