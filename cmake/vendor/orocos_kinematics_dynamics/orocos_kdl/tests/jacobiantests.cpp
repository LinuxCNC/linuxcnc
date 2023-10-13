#include "jacobiantests.hpp"
#include "jacobiandoubletests.hpp"
#include "jacobianframetests.hpp"


int main(int argc,char** argv) {
    KDL::checkDoubleOps();
    KDL::checkFrameOps();
	KDL::checkFrameVelOps();
}




