// this component provides support for other RT components
// it exports protobuf message definitions in nanopb format
// as automatically generated in protobuf/Submakefile
// from protobuf/proto/*.proto definitions

#include "config.h"		/* GIT_VERSION */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL API */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Nanopb protobuf message definitions");
MODULE_LICENSE("GPL");

#include <protobuf/pb-linuxcnc.h>
#include <protobuf/nanopb/pb_decode.h>
#include <protobuf/nanopb/pb_encode.h>

#include <protobuf/generated/types.npb.h>
#include <protobuf/generated/value.npb.h>
#include <protobuf/generated/object.npb.h>
#include <protobuf/generated/message.npb.h>
#include <protobuf/generated/rtapi_message.npb.h>
#include <protobuf/generated/test.npb.h>

static int comp_id;
static const char *name = "pbmsgs";

int rtapi_app_main(void)
{
    if ((comp_id = hal_init(name)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_init() failed: %d\n",
			name, comp_id);
	return -1;
    }
    hal_ready(comp_id);
    rtapi_print_msg(RTAPI_MSG_DBG, "%s " VERSION " loaded\n", name);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

#define PB_MESSAGE(symbol) EXPORT_SYMBOL(symbol ## _fields)

// FIXME many lacking - autogenerate

PB_MESSAGE(Container);
PB_MESSAGE(Command);
PB_MESSAGE(Response);
PB_MESSAGE(Value);
PB_MESSAGE(Object);
PB_MESSAGE(RTAPI_Message);
PB_MESSAGE(Test1);
PB_MESSAGE(Test2);
PB_MESSAGE(Test3);
