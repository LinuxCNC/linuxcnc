
// this component provides support for other RT components
// it exports protobuf message descriptors in nanopb format
// as automatically generated in machinetalk/Submakefile
// from machinetalk/proto/*.proto definitions

// for any message <messagename>, the descriptor is accessible
// as symbol 'pb_<messagename>_fields'.

// to make a new message descriptor available:
// edit the Submakefile to link in the new descriptor
//   (stanza pbmsgs-objs := ..)
// add an PB_DESCRIPTOR(..) at the end of this file

#include "config.h"		/* GIT_VERSION */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL API */
#include "pbmsgs.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Nanopb protobuf message descriptors");
MODULE_LICENSE("GPL");

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
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s git=" GIT_VERSION " nanopb=" VERSION "\n", name);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

#define PB_DESCRIPTOR(symbol) EXPORT_SYMBOL(pb_ ## symbol ## _fields)

PB_DESCRIPTOR(Container);
PB_DESCRIPTOR(Function);
PB_DESCRIPTOR(Instance);
PB_DESCRIPTOR(Pin);
PB_DESCRIPTOR(Param);
PB_DESCRIPTOR(Signal);
PB_DESCRIPTOR(Group);
PB_DESCRIPTOR(Ring);
PB_DESCRIPTOR(Component);
PB_DESCRIPTOR(Application);
PB_DESCRIPTOR(File);
PB_DESCRIPTOR(Preview);


/* PB_DESCRIPTOR(); */

PB_DESCRIPTOR(MotionCommand);
PB_DESCRIPTOR(MotionStatus);
//PB_DESCRIPTOR(Pm_Cartesian);
//PB_DESCRIPTOR(Emc_Pose);
//PB_DESCRIPTOR(LegacyEmcPose);
PB_DESCRIPTOR(RTAPI_Message);
PB_DESCRIPTOR(RTAPICommand);
PB_DESCRIPTOR(LogMessage);
/* PB_DESCRIPTOR(Test1); */
/* PB_DESCRIPTOR(Test2); */
/* PB_DESCRIPTOR(Test3); */

// this likely supersedes the above exports, as it
// contains a superset of pb_<message>_fields
msginfo_t msginfo[] = {
    CANON_MESSAGES	   \
    CONFIG_MESSAGES	   \
    EMCCLASS_MESSAGES	   \
    LOG_MESSAGES	   \
    MESSAGE_MESSAGES	   \
    MOTCMDS_MESSAGES	   \
    OBJECT_MESSAGES	   \
    PREVIEW_MESSAGES	   \
    RTAPICOMMAND_MESSAGES  \
    STATUS_MESSAGES	   \
    TASK_MESSAGES	   \
    TEST_MESSAGES	   \
    TYPES_MESSAGES	   \
    VALUE_MESSAGES	   \
    PB_MSGINFO_DELIMITER
};

EXPORT_SYMBOL(msginfo);

