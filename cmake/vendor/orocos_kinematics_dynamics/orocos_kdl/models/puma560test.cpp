#include <chain.hpp>
#include "models.hpp"
#include <frames_io.hpp>
#include <kinfam_io.hpp>

#include <chainfksolverpos_recursive.hpp>
#include <chainidsolver_recursive_newton_euler.hpp>

using namespace KDL;

int main(int argc , char** argv){
    
    Chain p560=Puma560();
    //Chain p560;
//    p560.addSegment(Segment(Joint(Joint::RotX),Frame::Identity(),RigidBodyInertia(1.0,Vector(0.0,1.0,.0),RotationalInertia(1.0,2.0,3.0))));
//    p560.addSegment(Segment(Joint(Joint::RotY),Frame(Rotation::Identity(),Vector(0,2,0)),RigidBodyInertia(1.0,Vector(1.0,0.0,.0),RotationalInertia(1.0,2.0,3,4,5,6))));
//    p560.addSegment(Segment(Joint(Joint::RotZ),Frame(Rotation::Identity(),Vector(2,0,0)),RigidBodyInertia(1.0,Vector(0.0,0.0,1),RotationalInertia(1.0,2.0,3,4,5,6))));
    
    JntArray q(p560.getNrOfJoints());
    JntArray qdot(p560.getNrOfJoints());
    JntArray qdotdot(p560.getNrOfJoints());
    JntArray tau(p560.getNrOfJoints());
    Wrenches f(p560.getNrOfSegments());

    for(unsigned int i=0;i<p560.getNrOfJoints();i++){
      q(i)=0.0;
      qdot(i)=0.0;
      qdotdot(i)=0.0;
      
      //if(i<2)
      //{
	std::cout << "give q(" << i+1 << ")\n" << std::endl;
	std::cin >> q(i);
	std::cout << "give qdot(" << i+1 << ")\n" << std::endl;
	std::cin >> qdot(i);
	std::cout << "give qdotdot(" << i << ")\n" << std::endl;
	std::cin >> qdotdot(i);
      //}
        
    }
    
    ChainFkSolverPos_recursive fksolver(p560);
    Frame T;
    ChainIdSolver_RNE idsolver(p560,Vector(0.0,0.0,-9.81));
    
    //#include <time.h>
    //time_t before,after;
    //time(&before);
    //unsigned int k=0;
    //for(k=0;k<1e7;k++)
        fksolver.JntToCart(q,T);
    //time(&after);
    //std::cout<<"elapsed time for FK: "<<difftime(after,before)<<" seconds for "<<k<<" iterations"<<std::endl;
    //std::cout<<"time per iteration for FK: "<<difftime(after,before)/k<<" seconds."<<std::endl;
    //time(&before);
    //for(k=0;k<1e7;k++)
        idsolver.CartToJnt(q,qdot,qdotdot,f,tau);
        //time(&after);
        //std::cout<<"elapsed time for ID: "<<difftime(after,before)<<" seconds for "<<k<<" iterations"<<std::endl;
        //std::cout<<"time per iteration for ID: "<<difftime(after,before)/k<<" seconds."<<std::endl;

    std::cout<<T<<std::endl;
    std::cout<<"tau: "<<tau<<std::endl;


}
    
