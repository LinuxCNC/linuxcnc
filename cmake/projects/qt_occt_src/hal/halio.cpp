#include "halio.h"
#include <vector>

//! Make conversion's easy:
#define toRadians M_PI/180.0
#define toDegrees (180.0/M_PI)

void halio::close(){
    system("/opt/hal-core-2.0/bin/./halcmd stop");
    system("/opt/hal-core-2.0/bin/./halcmd unloadrt hal_app");
}

int halio::init(){
    
    system("/opt/hal-core-2.0/bin/./halcmd stop");
    
    comp_id = hal_init("hal_app");

    //! Parameter pins
    velmax = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.velmax",HAL_RW,&(velmax->Pin),comp_id);
    accmax = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.accmax",HAL_RW,&(accmax->Pin),comp_id);
    cart_stepsize = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.cart_stepsize",HAL_RW,&(cart_stepsize->Pin),comp_id);
    euler_stepsize = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.euler_stepsize",HAL_RW,&(euler_stepsize->Pin),comp_id);
    euler_maxdegsec = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.euler_maxdegsec",HAL_RW,&(euler_maxdegsec->Pin),comp_id);
    joint_stepsize = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.joint_stepsize",HAL_RW,&(joint_stepsize->Pin),comp_id);
    joint_maxdegsec = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.joint_maxdegsec",HAL_RW,&(joint_maxdegsec->Pin),comp_id);
    tooldir_stepsize = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.tooldir_stepsize",HAL_RW,&(tooldir_stepsize->Pin),comp_id);

    //! S32 pins
    streamermeat = (s32_data_t*)hal_malloc(sizeof(s32_data_t));
    hal_pin_s32_new("hal_app.streamermeat",HAL_IN,&(streamermeat->Pin),comp_id);

    //! If forward kinematics, send value.
    j0_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j0_cmd",HAL_OUT,&(j0_cmd->Pin),comp_id);
    j1_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j1_cmd",HAL_OUT,&(j1_cmd->Pin),comp_id);
    j2_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j2_cmd",HAL_OUT,&(j2_cmd->Pin),comp_id);
    j3_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j3_cmd",HAL_OUT,&(j3_cmd->Pin),comp_id);
    j4_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j4_cmd",HAL_OUT,&(j4_cmd->Pin),comp_id);
    j5_cmd=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j5_cmd",HAL_OUT,&(j5_cmd->Pin),comp_id);

    //! If inverse kinmatics, send value.
    cart_x_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_x_cmd",HAL_OUT,&(cart_x_cmd->Pin),comp_id);
    cart_y_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_y_cmd",HAL_OUT,&(cart_y_cmd->Pin),comp_id);
    cart_z_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_z_cmd",HAL_OUT,&(cart_z_cmd->Pin),comp_id);

    euler_x_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_x_cmd",HAL_OUT,&(euler_x_cmd->Pin),comp_id);
    euler_y_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_y_cmd",HAL_OUT,&(euler_y_cmd->Pin),comp_id);
    euler_z_cmd = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_z_cmd",HAL_OUT,&(euler_z_cmd->Pin),comp_id);

    //! Float params feeback.
    j0=(float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j0",HAL_IN,&(j0->Pin),comp_id);
    j1 = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j1",HAL_IN,&(j1->Pin),comp_id);
    j2 = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j2",HAL_IN,&(j2->Pin),comp_id);
    j3 = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j3",HAL_IN,&(j3->Pin),comp_id);
    j4 = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j4",HAL_IN,&(j4->Pin),comp_id);
    j5 = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.j5",HAL_IN,&(j5->Pin),comp_id);

    cart_x = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_x",HAL_IN,&(cart_x->Pin),comp_id);
    cart_y = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_y",HAL_IN,&(cart_y->Pin),comp_id);
    cart_z = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.cart_z",HAL_IN,&(cart_z->Pin),comp_id);

    euler_x = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_x",HAL_IN,&(euler_x->Pin),comp_id);
    euler_y = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_y",HAL_IN,&(euler_y->Pin),comp_id);
    euler_z = (float_data_t*)hal_malloc(sizeof(float_data_t));
    hal_pin_float_new("hal_app.euler_z",HAL_IN,&(euler_z->Pin),comp_id);

    //! Parameters
    //! Joint 0.
    j0_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j0_x",HAL_RW,&(j0_x->Pin),comp_id);
    j0_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j0_y",HAL_RW,&(j0_y->Pin),comp_id);
    j0_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j0_z",HAL_RW,&(j0_z->Pin),comp_id);
    //! Joint 1.
    j1_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j1_x",HAL_RW,&(j1_x->Pin),comp_id);
    j1_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j1_y",HAL_RW,&(j1_y->Pin),comp_id);
    j1_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j1_z",HAL_RW,&(j1_z->Pin),comp_id);
    j2_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j2_x",HAL_RW,&(j2_x->Pin),comp_id);
    j2_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j2_y",HAL_RW,&(j2_y->Pin),comp_id);
    j2_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j2_z",HAL_RW,&(j2_z->Pin),comp_id);
    j3_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j3_x",HAL_RW,&(j3_x->Pin),comp_id);
    j3_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j3_y",HAL_RW,&(j3_y->Pin),comp_id);
    j3_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j3_z",HAL_RW,&(j3_z->Pin),comp_id);
    j4_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j4_x",HAL_RW,&(j4_x->Pin),comp_id);
    j4_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j4_y",HAL_RW,&(j4_y->Pin),comp_id);
    j4_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j4_z",HAL_RW,&(j4_z->Pin),comp_id);
    j5_x = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j5_x",HAL_RW,&(j5_x->Pin),comp_id);
    j5_y = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j5_y",HAL_RW,&(j5_y->Pin),comp_id);
    j5_z = (param_data_t*)hal_malloc(sizeof(param_data_t));
    hal_param_float_new("hal_app.j5_z",HAL_RW,&(j5_z->Pin),comp_id);

    //! Bit pins.
    tool0 = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    hal_pin_bit_new("hal_app.tool_0",HAL_OUT,&(tool0->Pin),comp_id);
    tool1 = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    hal_pin_bit_new("hal_app.tool_1",HAL_OUT,&(tool1->Pin),comp_id);
    tool2 = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    hal_pin_bit_new("hal_app.tool_2",HAL_OUT,&(tool2->Pin),comp_id);

    mode_fk = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    hal_pin_bit_new("hal_app.mode_fk",HAL_OUT,&(mode_fk->Pin),comp_id);

    int error = hal_ready(comp_id);
    if(error==0){
        std::cout << "hal_app component ok, now loading config.hal file." << std::endl;

        //! Load config with hal file
        system("/opt/hal-core-2.0/bin/./halcmd -f /opt/hal-core-2.0/src/hal/components/opencascade/config.hal");

        return 1; //ok go on
    } else {
        std::cout << "Hal component error, performing [halclean] now." << std::endl;
        system("/opt/hal-core-2.0/bin/./halcmd stop");
        system("/opt/hal-core-2.0/bin/./halcmd unloadrt hal_app");
        system("/opt/hal-core-2.0/scripts/./halclean");
        std::cout << "Restart application required." << std::endl;
        return 0; //not good, show error output in terminal.
    }

    // Normally we don't get here.
    return 1;
}





































