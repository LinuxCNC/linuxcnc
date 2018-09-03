#include <stdio.h>
#include <emc/motion/motion.h>
#include <emc/motion/motion_debug.h>

#include <machinetalk/protobuf/test.npb.h>
#include <machinetalk/protobuf/value.npb.h>
#include <machinetalk/protobuf/object.npb.h>
#include <machinetalk/protobuf/message.npb.h>
#include <machinetalk/protobuf/motcmds.npb.h>


int main()
{
    printf("emcmot_joint_t = %zu\n", sizeof(emcmot_joint_t));
    printf("emcmot_joint_status_t = %zu\n", sizeof(emcmot_joint_status_t));
    printf("emcmot_command_t = %zu\n", sizeof(emcmot_command_t));
    printf("spindle_status = %zu\n", sizeof(spindle_status));
    printf("emcmot_status_t = %zu\n", sizeof(emcmot_status_t));
    printf("emcmot_config_t = %zu\n", sizeof(emcmot_config_t));
    printf("emcmot_debug_t = %zu\n", sizeof(emcmot_debug_t));

    printf("npb Container = %zu\n", sizeof(machinetalk_Container));
    printf("npb Test1 = %zu\n", sizeof(machinetalk_Test1));
    printf("npb Value = %zu\n", sizeof(machinetalk_Value));

    printf("npb MotionCommand = %zu\n", sizeof(machinetalk_MotionCommand));
    printf("npb MotionStatus = %zu\n", sizeof(machinetalk_MotionStatus));
    machinetalk_Container c[1];
    //  printf("npb Task cmds = %zu\n", c->has_traj_set_g5x-c->has_tpexecute);
    printf("npb Canon cmds = %zu\n", (void *)&c[1]- (void *)&(c->has_traj_set_g5x));


    return 0;
}
