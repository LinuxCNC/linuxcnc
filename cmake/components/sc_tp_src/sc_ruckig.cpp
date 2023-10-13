#include "sc_ruckig.h"

sc_ruckig::sc_ruckig()
{

}

V sc_ruckig::set_a_jm(T maxacc, T jerkmax){
    myIn.max_acceleration[0]=maxacc;
    myIn.max_jerk[0]=jerkmax;

    //! Phase, ///< Phase synchronize the DoFs when possible, else fallback to "Time" strategy
    //! Time, ///< Always synchronize the DoFs to reach the target at the same time (Default)
    //! TimeIfNecessary, ///< Synchronize only when necessary (e.g. for non-zero target velocity or acceleration)
    //! None, ///< Calculate every DoF independently
    myIn.synchronization=ruckig::Synchronization::None;
    //! Continuous, ///< Every trajectory duration is allowed (Default), in lcnc this is called the split cycle.
    //! Discrete, ///< The trajectory duration must be a multiple of the control cycle
    myIn.duration_discretization=ruckig::DurationDiscretization::Continuous;

    myIn.enabled[0]=1;
}

V sc_ruckig::set_interval(T value){
    myInterval=value;
}

T sc_ruckig::get_a(){
    return myIn.max_acceleration[0];
}

T sc_ruckig::get_jm(){
    return myIn.max_jerk[0];
}

V sc_ruckig::set_target(T tarpos, T tarvel, T taracc){

    myIn.target_position[0]=tarpos-myMemPos;
    myIn.target_velocity[0]=tarvel;
    myIn.target_acceleration[0]=taracc;
}

V sc_ruckig::set_current(T curpos, T curvel, T curacc){
    myIn.current_position[0]=curpos;
    myIn.current_velocity[0]=curvel;
    myIn.current_acceleration[0]=curacc;
}

V sc_ruckig::set_vm(T maxvel){
    myMaxVelocity=maxvel;
}

T sc_ruckig::get_vm(){
    return myIn.max_velocity[0];
}

V sc_ruckig::set_mem_pos(T value){
    myMemPos=value;
}

V sc_ruckig::set_fo(T value){
    myFeedOveride=std::max(value,0.01);
}

//! If adaptive feed<0, you have to set a previous target position along with it.
V sc_ruckig::set_af(T value){
    value=abs(value);
    myAdaptiveFeed=std::max(value,0.01);
}

V sc_ruckig::set_all_and_run(T a, T jm, T interval, T vm, T fo, T af,
                             T curpos, T curvel, T curacc,
                             T tarpos, T tarvel, T taracc){

    set_a_jm(a,jm);
    set_interval(interval);
    set_vm(vm);
    set_fo(fo);
    set_af(af);
    set_current(curpos,curvel,curacc);
    set_target(tarpos,tarvel,taracc);
    run();
}

V sc_ruckig::run(T tarpos, T tarvel, T taracc){
    set_target(tarpos,tarvel,taracc);
    run();
}

V sc_ruckig::print_result(ruckig::Result r){

    if(r==ruckig::Result::Error){
        std::cout<<"ruckig error."<<std::endl;
    }
    if(r==ruckig::Result::ErrorExecutionTimeCalculation){
        std::cout<<"ruckig ErrorExecutionTimeCalculation."<<std::endl;
    }
    if(r==ruckig::Result::ErrorInvalidInput){
        std::cout<<"ruckig ErrorInvalidInput."<<std::endl;
    }
    if(r==ruckig::Result::ErrorPositionalLimits){
        std::cout<<"ruckig ErrorPositionalLimits."<<std::endl;
    }
    if(r==ruckig::Result::ErrorSynchronizationCalculation){
        std::cout<<"ruckig ErrorSynchronizationCalculation."<<std::endl;
    }
    if(r==ruckig::Result::ErrorTrajectoryDuration){
        std::cout<<"ruckig ErrorTrajectoryDuration."<<std::endl;
    }
    if(r==ruckig::Result::Finished){
        std::cout<<"ruckig Finished."<<std::endl;
    }
    if(r==ruckig::Result::Working){
        std::cout<<"ruckig Working."<<std::endl;
    }
}

V sc_ruckig::run(){

    myState=sc_ruckig_run;

    //! enum: position, velocity
    myIn.control_interface=ruckig::ControlInterface::Position;

    myIn.max_velocity[0]=(myMaxVelocity*myFeedOveride)*myAdaptiveFeed;

    set_current(0,myVel[0],myAcc[0]);

    ruckig::Ruckig<1> myOtg {myInterval}; //! Normally 0.001
    myResult=myOtg.update(myIn, myOut);
    //! print_result(myResult);

    myDuration=myOut.trajectory.get_duration();

    myAtTime=0; //! Reset.
    myOldPos=0;

    /*
    //! Check for position overshoot when ve is set too high.
    std::array<double, 1> vel, acc, pos;
    myOut.trajectory.at_time(myDuration,pos, vel, acc);
    if(pos[0]>myIn.target_position[0]){
        std::cout<<"position overshoot."<<std::endl;
        std::cout<<"pos:"<<pos[0]<<" vel:"<<vel[0]<<" acc:"<<acc[0]<<std::endl;
    }*/
}

V sc_ruckig::stop(){

    std::cout<<"ruckig stop request"<<std::endl;

    myState=sc_ruckig_stop;

    //! enum: position, velocity
    myIn.control_interface=ruckig::ControlInterface::Velocity;

    myIn.target_acceleration[0]=0;
    myIn.target_velocity[0]=0;

    set_current(0,myVel[0],myAcc[0]);

    ruckig::Ruckig<1> myOtg {myInterval}; //! Normally 0.001
    myResult=myOtg.update(myIn, myOut);
    myDuration=myOut.trajectory.get_duration();

    myAtTime=0; //! Reset.
    myOldPos=0;
}

V sc_ruckig::pause(){
    stop();
}

V sc_ruckig::pause_resume(){
    run();
}

//! Finished returns 1.
V sc_ruckig::update(T &incpos, T &mempos, T &newpos, T &newvel, T &newacc){

    myOut.trajectory.at_time(std::min(myAtTime,myDuration),myPos, myVel, myAcc);
    myAtTime+=0.001;

    newpos=myPos[0];
    incpos=myPos[0]-myOldPos;
    newvel=myVel[0];
    newacc=myAcc[0];

    myOldPos=myPos[0];

    myMemPos+=incpos;
    mempos=myMemPos;

    //! Finished.
    if(myAtTime>myDuration){

        //! If in pause, stay in pause state.
        if(myState==sc_ruckig_wait){
            return;
        }

        //! If stop complete, set wait state.
        if(myState==sc_ruckig_stop){
            myState=sc_ruckig_wait;
            return;
        }

        //! If run complete, set finished state.
        if(myState==sc_ruckig_run){
            myState=sc_ruckig_finished;
            return;
        }
    }
}

sc_ruckig_state sc_ruckig::get_state(){
    return myState;
}

V sc_ruckig::set_wait_state(){
    myState=sc_ruckig_wait;
}


//! Create intance.
extern "C" sc_ruckig* ruckig_init_ptr(){
    return new sc_ruckig();
}

extern "C" V ruckig_set_a_jm(sc_ruckig *ptr, T maxacc, T jerkmax){
    ptr->set_a_jm(maxacc,jerkmax);
}

extern "C" V ruckig_set_interval(sc_ruckig *ptr, T value){
    ptr->set_interval(value);
}

extern "C" V ruckig_set_vm(sc_ruckig *ptr, T value){
    ptr->set_vm(value);
}

extern "C" V ruckig_set_fo(sc_ruckig *ptr, T value){
    ptr->set_fo(value);
}

extern "C" V ruckig_set_af(sc_ruckig *ptr, T value, T tarpos_begin, T tarpos_end){
    ptr->set_af(value);
    if(value<0){
        ptr->set_target(tarpos_begin,0,0);
    } else {
        ptr->set_target(tarpos_end,0,0);
    }
}

extern "C" V ruckig_set_all_and_run(sc_ruckig *ptr, T interval,
                                    T a, T jm,
                                    T curpos, T curvel, T curacc,
                                    T tarpos, T tarvel, T taracc,
                                    T vm, T fo, T af){

    ptr->set_all_and_run(a,jm,interval,vm,fo,af,curpos,curvel,curacc,tarpos,tarvel,taracc);
}

extern "C" V ruckig_run(sc_ruckig *ptr){
    ptr->run();
}

extern "C" V ruckig_stop(sc_ruckig *ptr){
    ptr->stop();
}

extern "C" V ruckig_pause(sc_ruckig *ptr){
    ptr->stop();
}

extern "C" V ruckig_pause_resume(sc_ruckig *ptr){
    ptr->pause_resume();
}

extern "C" V ruckig_set_mempos(sc_ruckig *ptr, T value){
    ptr->set_mem_pos(value);
}

extern "C" V ruckig_update(sc_ruckig *ptr, T *mempos, T *newvel, T *newacc){

    //! Vars for converting c++ by reference to c pointer style.
    T incpos_=0, mempos_=0, newpos_=0, newvel_=0, newacc_=0;

    ptr->update(incpos_,mempos_,newpos_,newvel_,newacc_);

    *mempos=mempos_;
    *newvel=newvel_;
    *newacc=newacc_;
}

extern "C" T ruckig_get_vm(sc_ruckig *ptr){
    return ptr->get_vm();
}

extern "C" T ruckig_get_a(sc_ruckig *ptr){
    return ptr->get_a();
}

extern "C" T ruckig_get_jm(sc_ruckig *ptr){
    return ptr->get_jm();
}

extern "C" enum sc_ruckig_state ruckig_get_state(sc_ruckig *ptr){
    return ptr->get_state();
}

extern "C" B ruckig_state_run(sc_ruckig *ptr){
    if(ptr->get_state()==sc_ruckig_run){
        return 1;
    }
    return 0;
}

extern "C" B ruckig_state_pause(sc_ruckig *ptr){
    if(ptr->get_state()==sc_ruckig_stop){
        return 1;
    }
    return 0;
}

extern "C" B ruckig_state_stop(sc_ruckig *ptr){
    if(ptr->get_state()==sc_ruckig_stop){
        return 1;
    }
    return 0;
}

extern "C" B ruckig_state_pause_resume(sc_ruckig *ptr){
    if(ptr->get_state()==sc_ruckig_run){
        return 1;
    }
    return 0;
}

extern "C" B ruckig_state_finished(sc_ruckig *ptr){
    if(ptr->get_state()==sc_ruckig_finished){
        return 1;
    }
    return 0;
}

extern "C" V ruckig_set_wait_state(sc_ruckig *ptr){
    ptr->set_wait_state();
}

























