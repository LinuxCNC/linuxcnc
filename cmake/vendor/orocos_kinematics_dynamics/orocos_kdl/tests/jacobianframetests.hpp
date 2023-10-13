#ifndef PVFramesTests_H
#define PVFramesTests_H

#include <kdl/jacobianexpr.hpp>
#include <kdl/jacobianframe.hpp>
#include <kdl/frames.hpp>
#include <kdl/frames_io.hpp>

#include "jacobiantests.hpp"
#include "jacobiandoubletests.hpp"

namespace KDL {

void checkDiffs();

void checkFrameOps();
void checkFrameVelOps();
} 
#endif




