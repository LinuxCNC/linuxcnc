#include "cpp_interface.h"

Cpp_interface::Cpp_interface()
{
    // This is performed every ms
}

result Cpp_interface::dofs(result input){

    //! Setup ruckig config.
    ruckig::Ruckig<1> otg {input.period};
    ruckig::InputParameter<1> in;
    ruckig::OutputParameter<1> out;
    std::array<double, 1> vel, acc, pos;

    int i=0;

    in.max_velocity[i]=abs(input.maxvel);
    in.max_acceleration[i]=input.maxacc;
    in.max_jerk[i]=input.maxjerk;
    in.current_position[i]=input.curpos;
    in.current_velocity[i]=input.curvel;
    in.current_acceleration[i]=input.curacc;
    in.target_velocity[i]=0;
    in.target_acceleration[i]=0;
    in.target_position[i]=input.tarpos;

    if(input.interfacetype==int(ruckig::ControlInterface::Position)){
        in.control_interface=ruckig::ControlInterface::Position;
    }
    if(input.interfacetype==int(ruckig::ControlInterface::Velocity)){
         in.control_interface=ruckig::ControlInterface::Velocity;
    }

    in.synchronization=ruckig::Synchronization::Time;

    in.enabled[i]=input.enable;

    auto result_x = otg.update(in,out);
    result_x = ruckig::Result::Working;

    // One ms forward.
    out.trajectory.at_time(0.001,pos, vel, acc);

    input.curpos=pos[i];
    input.curvel=vel[i];
    input.curacc=acc[i];

    return input;
}

extern "C" result wrapper_get_pos(result input){
    result r=Cpp_interface().dofs(input);
    return r;
}





















