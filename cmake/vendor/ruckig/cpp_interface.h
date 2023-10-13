#ifndef CPP_INTERFACE_H
#define CPP_INTERFACE_H

#include <iostream>
#include <vector>
#include <fstream>

//! https://github.com/pantor/ruckig
#include "ruckig.hpp"

/*
int i=1;
ruckig::Ruckig<ruckig::DynamicDOFs> otg {static_cast<size_t>(i), 0.001};
ruckig::InputParameter<ruckig::DynamicDOFs> in {static_cast<size_t>(i)};
ruckig::OutputParameter<ruckig::DynamicDOFs> out {static_cast<size_t>(i)};
std::array<double, 3> pos, vel, acc;
*/

enum interface {
    position,
    velocity
};

enum synchronization {
    Phase, ///< Phase synchronize the DoFs when possible, else fallback to "Time" strategy
    Time, ///< Always synchronize the DoFs to reach the target at the same time (Default)
    TimeIfNecessary, ///< Synchronize only when necessary (e.g. for non-zero target velocity or acceleration)
    None, ///< Calculate every DoF independently
};

enum durationdiscretization {
    Continuous, ///< Every trajectory duration is allowed (Default)
    Discrete, ///< The trajectory duration must be a multiple of the control cycle
};

struct result {
    double period;
    double curvel,curacc,curpos,tarpos;
    double maxvel,maxacc,maxjerk;
    bool enable;
    enum interface interfacetype;
    enum synchronization synchronizationtype;
    enum durationdiscretization durationdiscretizationtype;
};

class Cpp_interface
{
public:

public:
    Cpp_interface();
    result dofs(result input);
private:

};

#endif
