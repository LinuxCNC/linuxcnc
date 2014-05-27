#include <stdio.h>
#include <emc/motion/motion.h>
#include <emc/motion/motion_debug.h>

#include <machinetalk/generated/test.npb.h>
#include <machinetalk/generated/value.npb.h>
#include <machinetalk/generated/object.npb.h>
#include <machinetalk/generated/message.npb.h>
#include <machinetalk/generated/motcmds.npb.h>


int main()
{
    printf("emcmot_joint_t = %zu\n", sizeof(emcmot_joint_t));
    printf("emcmot_joint_status_t = %zu\n", sizeof(emcmot_joint_status_t));
    printf("emcmot_command_t = %zu\n", sizeof(emcmot_command_t));
    printf("spindle_status = %zu\n", sizeof(spindle_status));
    printf("emcmot_status_t = %zu\n", sizeof(emcmot_status_t));
    printf("emcmot_config_t = %zu\n", sizeof(emcmot_config_t));
    printf("emcmot_debug_t = %zu\n", sizeof(emcmot_debug_t));

    printf("npb Container = %zu\n", sizeof(pb_Container));
    printf("npb Test1 = %zu\n", sizeof(pb_Test1));
    printf("npb Value = %zu\n", sizeof(pb_Value));

    printf("npb MotionCommand = %zu\n", sizeof(pb_MotionCommand));
    printf("npb MotionStatus = %zu\n", sizeof(pb_MotionStatus));
    pb_Container c[1];
    //  printf("npb Task cmds = %zu\n", c->has_traj_set_g5x-c->has_tpexecute);
    printf("npb Canon cmds = %zu\n", (void *)&c[1]- (void *)&(c->has_traj_set_g5x));


    return 0;
}
